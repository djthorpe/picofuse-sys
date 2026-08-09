#include <picofuse/dev/framebuffer.h>
#include <picofuse/sys.h>

#if defined(SYSTEM_NAME_LINUX)
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(SYSTEM_NAME_LINUX)

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_framebuffer_t {
  int fd;
  uint8_t *data;
  size_t size;
  uint32_t bpp;
  pix_frame_t frame;
  uint32_t r_offset;
  uint32_t r_length;
  uint32_t g_offset;
  uint32_t g_length;
  uint32_t b_offset;
  uint32_t b_length;
  bool locked;
  bool vsync_warned;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _dev_framebuffer_ready(const dev_framebuffer_t *fb) {
  return fb != NULL && fb->data != NULL && fb->data != MAP_FAILED;
}

static uint32_t _dev_framebuffer_pack_component(uint8_t value8,
                                                uint32_t length) {
  if (length == 0u) {
    return 0u;
  }

  if (length >= 8u) {
    return (uint32_t)value8 << (length - 8u);
  }

  return (uint32_t)(value8 >> (8u - length));
}

static bool
_dev_framebuffer_resolve_format(const struct fb_var_screeninfo *vinfo,
                                pix_format_t *fmt_out) {
  uint32_t bpp = vinfo->bits_per_pixel;
  uint32_t r_len = vinfo->red.length;
  uint32_t g_len = vinfo->green.length;
  uint32_t b_len = vinfo->blue.length;

  if (bpp == 32u && r_len == 8u && g_len == 8u && b_len == 8u) {
    *fmt_out = PIX_FMT_RGBA32;
    return true;
  }

  if (bpp == 24u && r_len == 8u && g_len == 8u && b_len == 8u) {
    *fmt_out = PIX_FMT_RGB888;
    return true;
  }

  if (bpp == 16u && r_len == 5u && g_len == 6u && b_len == 5u) {
    *fmt_out = PIX_FMT_RGB565;
    return true;
  }

  return false;
}

static uint32_t _dev_framebuffer_pack(const dev_framebuffer_t *fb,
                                      pix_color_t color) {
  uint8_t r = (uint8_t)((color >> 24) & 0xFFu);
  uint8_t g = (uint8_t)((color >> 16) & 0xFFu);
  uint8_t b = (uint8_t)((color >> 8) & 0xFFu);

  uint32_t rv = _dev_framebuffer_pack_component(r, fb->r_length)
                << fb->r_offset;
  uint32_t gv = _dev_framebuffer_pack_component(g, fb->g_length)
                << fb->g_offset;
  uint32_t bv = _dev_framebuffer_pack_component(b, fb->b_length)
                << fb->b_offset;

  return rv | gv | bv;
}

static void _dev_framebuffer_write_pixel(const dev_framebuffer_t *fb,
                                         uint16_t x, uint16_t y,
                                         uint32_t pixel) {
  uint32_t bytes_per_pixel = fb->bpp / 8u;
  uint8_t *px = fb->data + ((size_t)y * fb->frame.stride) +
               ((size_t)x * bytes_per_pixel);

  if (bytes_per_pixel == 2u) {
    *(uint16_t *)px = (uint16_t)pixel;
  } else if (bytes_per_pixel == 3u) {
    px[0] = (uint8_t)(pixel & 0xFFu);
    px[1] = (uint8_t)((pixel >> 8) & 0xFFu);
    px[2] = (uint8_t)((pixel >> 16) & 0xFFu);
  } else {
    *(uint32_t *)px = pixel;
  }
}

static bool _dev_framebuffer_do_lock(dev_framebuffer_t *fb) {
  if (!_dev_framebuffer_ready(fb)) {
    return false;
  }

  if (fb->locked) {
    sys_debugf("[framebuffer] lock called while already locked");
  }

  uint32_t crtc = 0u;
  if (ioctl(fb->fd, FBIO_WAITFORVSYNC, &crtc) != 0 && !fb->vsync_warned) {
    sys_debugf("[framebuffer] vsync wait unsupported, writes may tear");
    fb->vsync_warned = true;
  }

  fb->locked = true;
  return true;
}

static void _dev_framebuffer_do_unlock(dev_framebuffer_t *fb) {
  if (!_dev_framebuffer_ready(fb)) {
    return;
  }

  if (!fb->locked) {
    sys_debugf("[framebuffer] unlock called while not locked");
  }

  fb->locked = false;
}

static void _dev_framebuffer_do_clear(const dev_framebuffer_t *fb,
                                      pix_color_t color) {
  if (!_dev_framebuffer_ready(fb)) {
    return;
  }

  if (!fb->locked) {
    sys_debugf("[framebuffer] clear called without lock (writes may tear)");
  }

  uint32_t pixel = _dev_framebuffer_pack(fb, color);

  for (uint16_t y = 0u; y < fb->frame.size.h; y++) {
    for (uint16_t x = 0u; x < fb->frame.size.w; x++) {
      _dev_framebuffer_write_pixel(fb, x, y, pixel);
    }
  }
}

static void _dev_framebuffer_do_set(const dev_framebuffer_t *fb,
                                    pix_color_t color, pix_point_t origin,
                                    pix_size_t size) {
  if (!_dev_framebuffer_ready(fb)) {
    return;
  }

  if (origin.x < 0 || origin.y < 0) {
    return;
  }

  if (!fb->locked) {
    sys_debugf("[framebuffer] set called without lock (writes may tear)");
  }

  // A size where both dimensions are 0 or 1 addresses a single pixel.
  uint16_t w = size.w;
  uint16_t h = size.h;
  if (w <= 1u && h <= 1u) {
    w = 1u;
    h = 1u;
  }

  uint32_t x0 = (uint32_t)origin.x;
  uint32_t y0 = (uint32_t)origin.y;
  if (x0 >= fb->frame.size.w || y0 >= fb->frame.size.h) {
    return;
  }

  uint32_t x1 = x0 + w;
  uint32_t y1 = y0 + h;
  if (x1 > fb->frame.size.w) {
    x1 = fb->frame.size.w;
  }
  if (y1 > fb->frame.size.h) {
    y1 = fb->frame.size.h;
  }

  uint32_t pixel = _dev_framebuffer_pack(fb, color);

  for (uint32_t y = y0; y < y1; y++) {
    for (uint32_t x = x0; x < x1; x++) {
      _dev_framebuffer_write_pixel(fb, (uint16_t)x, (uint16_t)y, pixel);
    }
  }
}

static void _dev_framebuffer_do_copy(const dev_framebuffer_t *fb,
                                     const pix_frame_t *src,
                                     pix_point_t origin, pix_size_t size) {
  (void)fb;
  (void)src;
  (void)origin;
  (void)size;
  // pix_frame_t no longer exposes a raw data pointer, so there is currently
  // no generic way to read source pixels here; see the caller for context.
  sys_debugf("[framebuffer] copy is not yet implemented");
}

static bool _dev_framebuffer_frame_lock(pix_frame_t *frame) {
  return _dev_framebuffer_do_lock((dev_framebuffer_t *)frame->ctx);
}

static void _dev_framebuffer_frame_unlock(pix_frame_t *frame) {
  _dev_framebuffer_do_unlock((dev_framebuffer_t *)frame->ctx);
}

static void _dev_framebuffer_frame_clear(pix_frame_t *frame, pix_color_t color,
                                         pix_op_t op) {
  (void)op;
  _dev_framebuffer_do_clear((const dev_framebuffer_t *)frame->ctx, color);
}

static void _dev_framebuffer_frame_set(pix_frame_t *frame, pix_color_t color,
                                       pix_point_t origin, pix_size_t size,
                                       pix_op_t op) {
  (void)op;
  _dev_framebuffer_do_set((const dev_framebuffer_t *)frame->ctx, color, origin,
                          size);
}

static void _dev_framebuffer_frame_copy(pix_frame_t *frame,
                                        const pix_frame_t *src,
                                        pix_point_t origin, pix_size_t size,
                                        pix_op_t op) {
  (void)op;
  _dev_framebuffer_do_copy((const dev_framebuffer_t *)frame->ctx, src, origin,
                           size);
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_framebuffer_t *dev_framebuffer_init(const char *device,
                                        pix_frame_t **frame) {
  if (frame != NULL) {
    *frame = NULL;
  }

  if (device == NULL || device[0] == '\0') {
    return NULL;
  }

  int fd = open(device, O_RDWR);
  if (fd < 0) {
    sys_debugf("[framebuffer] open %s failed", device);
    return NULL;
  }

  struct fb_fix_screeninfo finfo;
  struct fb_var_screeninfo vinfo;
  if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0 ||
      ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
    sys_debugf("[framebuffer] ioctl query failed on %s", device);
    close(fd);
    return NULL;
  }

  if (vinfo.red.msb_right || vinfo.green.msb_right || vinfo.blue.msb_right) {
    sys_debugf("[framebuffer] unsupported msb-right layout on %s", device);
    close(fd);
    return NULL;
  }

  pix_format_t fmt;
  if (!_dev_framebuffer_resolve_format(&vinfo, &fmt)) {
    sys_debugf("[framebuffer] unsupported pixel layout on %s "
               "(bpp=%u r=%u g=%u b=%u)",
               device, (unsigned int)vinfo.bits_per_pixel,
               (unsigned int)vinfo.red.length, (unsigned int)vinfo.green.length,
               (unsigned int)vinfo.blue.length);
    close(fd);
    return NULL;
  }

  uint32_t stride = finfo.line_length;
  if (stride == 0u) {
    stride = (vinfo.xres * vinfo.bits_per_pixel) / 8u;
  }

  size_t size = (size_t)stride * (size_t)vinfo.yres;
  if (size == 0u) {
    sys_debugf("[framebuffer] zero-sized screen on %s", device);
    close(fd);
    return NULL;
  }

  void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    sys_debugf("[framebuffer] mmap failed on %s", device);
    close(fd);
    return NULL;
  }

  dev_framebuffer_t *fb = sys_calloc(1u, sizeof(*fb));
  if (fb == NULL) {
    munmap(data, size);
    close(fd);
    return NULL;
  } else {
    fb->fd = fd;
    fb->data = (uint8_t *)data;
    fb->size = size;
    fb->bpp = vinfo.bits_per_pixel;
    fb->r_offset = vinfo.red.offset;
    fb->r_length = vinfo.red.length;
    fb->g_offset = vinfo.green.offset;
    fb->g_length = vinfo.green.length;
    fb->b_offset = vinfo.blue.offset;
    fb->b_length = vinfo.blue.length;

    fb->frame.size.w = (uint16_t)vinfo.xres;
    fb->frame.size.h = (uint16_t)vinfo.yres;
    fb->frame.stride = stride;
    fb->frame.fmt = fmt;
    fb->frame.ctx = fb;
    fb->frame.lock = _dev_framebuffer_frame_lock;
    fb->frame.unlock = _dev_framebuffer_frame_unlock;
    fb->frame.clear = _dev_framebuffer_frame_clear;
    fb->frame.set = _dev_framebuffer_frame_set;
    fb->frame.copy = _dev_framebuffer_frame_copy;
  }

  sys_debugf("[framebuffer] init %s %ux%u bpp=%u fmt=%d stride=%u", device,
             (unsigned int)fb->frame.size.w, (unsigned int)fb->frame.size.h,
             (unsigned int)fb->bpp, (int)fb->frame.fmt,
             (unsigned int)fb->frame.stride);

  if (frame != NULL) {
    *frame = &fb->frame;
  }

  return fb;
}

void dev_framebuffer_deinit(dev_framebuffer_t *fb) {
  if (fb == NULL) {
    return;
  }

  if (fb->data != NULL && fb->data != MAP_FAILED) {
    munmap(fb->data, fb->size);
  }

  if (fb->fd >= 0) {
    close(fb->fd);
  }

  sys_free(fb);
}

#else // !SYSTEM_NAME_LINUX

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_framebuffer_t {
  bool _unused;
};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_framebuffer_t *dev_framebuffer_init(const char *device,
                                        pix_frame_t **frame) {
  (void)device;
  if (frame != NULL) {
    *frame = NULL;
  }
  sys_debugf("[framebuffer] unsupported on this platform");
  return NULL;
}

void dev_framebuffer_deinit(dev_framebuffer_t *fb) { (void)fb; }

#endif

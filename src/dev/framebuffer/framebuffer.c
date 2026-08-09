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
  uint32_t stride;
  uint32_t bpp;
  pix_size_t size_px;
  pix_format_t fmt;
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

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_framebuffer_t *dev_framebuffer_init(const char *device) {
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
    fb->stride = stride;
    fb->bpp = vinfo.bits_per_pixel;
    fb->size_px.w = (uint16_t)vinfo.xres;
    fb->size_px.h = (uint16_t)vinfo.yres;
    fb->fmt = fmt;
    fb->r_offset = vinfo.red.offset;
    fb->r_length = vinfo.red.length;
    fb->g_offset = vinfo.green.offset;
    fb->g_length = vinfo.green.length;
    fb->b_offset = vinfo.blue.offset;
    fb->b_length = vinfo.blue.length;
  }

  sys_debugf("[framebuffer] init %s %ux%u bpp=%u fmt=%d stride=%u", device,
             (unsigned int)fb->size_px.w, (unsigned int)fb->size_px.h,
             (unsigned int)fb->bpp, (int)fb->fmt, (unsigned int)fb->stride);

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

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

pix_size_t dev_framebuffer_info(const dev_framebuffer_t *fb,
                                pix_format_t *format) {
  if (!_dev_framebuffer_ready(fb)) {
    pix_size_t empty = {0};
    return empty;
  }

  if (format != NULL) {
    *format = fb->fmt;
  }

  return fb->size_px;
}

///////////////////////////////////////////////////////////////////////////////
// LOCKING

pix_frame_t dev_framebuffer_lock(dev_framebuffer_t *fb) {
  if (!_dev_framebuffer_ready(fb)) {
    pix_frame_t empty = {0};
    return empty;
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

  pix_frame_t frame = {
      .data = fb->data,
      .size = fb->size_px,
      .stride = fb->stride,
      .fmt = fb->fmt,
  };
  return frame;
}

void dev_framebuffer_unlock(dev_framebuffer_t *fb) {
  if (!_dev_framebuffer_ready(fb)) {
    return;
  }

  if (!fb->locked) {
    sys_debugf("[framebuffer] unlock called while not locked");
  }

  fb->locked = false;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

void dev_framebuffer_clear(const dev_framebuffer_t *fb,
                           const pix_color_t color) {
  if (!_dev_framebuffer_ready(fb)) {
    return;
  }

  if (!fb->locked) {
    sys_debugf("[framebuffer] clear called without lock (writes may tear)");
  }

  uint32_t pixel = _dev_framebuffer_pack(fb, color);
  uint32_t bytes_per_pixel = fb->bpp / 8u;

  for (uint16_t y = 0u; y < fb->size_px.h; y++) {
    uint8_t *row = fb->data + ((size_t)y * fb->stride);

    for (uint16_t x = 0u; x < fb->size_px.w; x++) {
      uint8_t *px = row + ((size_t)x * bytes_per_pixel);
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
  }
}

#else // !SYSTEM_NAME_LINUX

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_framebuffer_t {
  bool _unused;
};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_framebuffer_t *dev_framebuffer_init(const char *device) {
  (void)device;
  sys_debugf("[framebuffer] unsupported on this platform");
  return NULL;
}

void dev_framebuffer_deinit(dev_framebuffer_t *fb) { (void)fb; }

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

pix_size_t dev_framebuffer_info(const dev_framebuffer_t *fb,
                                pix_format_t *format) {
  (void)fb;
  (void)format;

  pix_size_t empty = {0};
  return empty;
}

///////////////////////////////////////////////////////////////////////////////
// LOCKING

pix_frame_t dev_framebuffer_lock(dev_framebuffer_t *fb) {
  (void)fb;
  pix_frame_t empty = {0};
  return empty;
}

void dev_framebuffer_unlock(dev_framebuffer_t *fb) { (void)fb; }

///////////////////////////////////////////////////////////////////////////////
// METHODS

void dev_framebuffer_clear(const dev_framebuffer_t *fb,
                           const pix_color_t color) {
  (void)fb;
  (void)color;
}

#endif

#include <picofuse/dev/framebuffer.h>
#include <picofuse/sys.h>

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

/**
 * @file
 * @brief Framebuffer example application.
 */

#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

const char *device = "/dev/fb0";

int main(int argc, char **argv) {
  sys_init();
  hw_init();

  dev_framebuffer_t *fb = dev_framebuffer_init(device);
  if (fb == NULL) {
    sys_debugf("[framebuffer] failed to init %s", device);
  } else {
    pix_size_t size = dev_framebuffer_info(fb, NULL);
    sys_debugf("[framebuffer] %s %ux%u", device, (unsigned int)size.w,
               (unsigned int)size.h);

    dev_framebuffer_clear(fb, 0xFF0000FFu); // Red
    sys_sleep_ms(1000u);

    dev_framebuffer_clear(fb, 0xFF00FF00u); // Green
    sys_sleep_ms(1000u);

    dev_framebuffer_clear(fb, 0xFFFF0000u); // Blue
    sys_sleep_ms(1000u);

    dev_framebuffer_clear(fb, 0xFFFFFFFFu); // White
    sys_sleep_ms(1000u);

    dev_framebuffer_clear(fb, 0xFF000000u); // Black
    sys_sleep_ms(1000u);

    dev_framebuffer_deinit(fb);
  }

  hw_exit();
  sys_exit();
  return 0;
}

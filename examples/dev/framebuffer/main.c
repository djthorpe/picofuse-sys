/**
 * @file
 * @brief Framebuffer example application.
 */

#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

const char *device = "/dev/fb0";

static void show_color(dev_framebuffer_t *fb, pix_color_t color) {
  dev_framebuffer_lock(fb);
  dev_framebuffer_clear(fb, color);
  dev_framebuffer_unlock(fb);
  sys_sleep_ms(1000u);
}

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

    show_color(fb, 0xFF0000FFu); // Red
    show_color(fb, 0x00FF00FFu); // Green
    show_color(fb, 0x0000FFFFu); // Blue
    show_color(fb, 0xFFFFFFFFu); // White
    show_color(fb, 0x000000FFu); // Black

    dev_framebuffer_deinit(fb);
  }

  hw_exit();
  sys_exit();
  return 0;
}

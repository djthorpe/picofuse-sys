/**
 * @file
 * @brief Framebuffer example application.
 */

#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/pix.h>
#include <picofuse/sys.h>

const char *device = "/dev/fb0";

static void show_color(pix_frame_t *frame, pix_color_t color) {
  frame->lock(frame);
  frame->clear(frame, color, PIX_SET);
  frame->unlock(frame);
  sys_sleep_ms(1000u);
}

int main(int argc, char **argv) {
  sys_init();
  hw_init();

  pix_frame_t *frame = NULL;
  dev_framebuffer_t *fb = dev_framebuffer_init(device, &frame);
  if (fb == NULL) {
    sys_debugf("framebuffer", "failed to init %s", device);
  } else {
    sys_debugf("framebuffer", "%s %ux%u", device,
               (unsigned int)frame->size.w, (unsigned int)frame->size.h);

    show_color(frame, PIX_COLOR_RED);   // Red
    show_color(frame, PIX_COLOR_GREEN); // Green
    show_color(frame, PIX_COLOR_BLUE);  // Blue
    show_color(frame, PIX_COLOR_WHITE); // White
    show_color(frame, PIX_COLOR_BLACK); // Black

    dev_framebuffer_deinit(fb);
  }

  hw_exit();
  sys_exit();
  return 0;
}

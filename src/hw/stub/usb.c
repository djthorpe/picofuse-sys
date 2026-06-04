#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_usb_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_usb_t *hw_usb_init(hw_usb_callback_t callback, void *userdata) {
  sys_debugf("usb_init: callback=%p userdata=%p unsupported on this platform",
             callback, userdata);
  (void)callback;
  (void)userdata;
  return NULL;
}

void hw_usb_deinit(hw_usb_t *usb) {
  sys_debugf("usb_deinit: usb=%p unsupported on this platform", usb);
  (void)usb;
}

bool hw_usb_valid(const hw_usb_t *usb) {
  (void)usb;
  return false;
}

void _hw_usb_poll(void) {}
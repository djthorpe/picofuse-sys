#include <picofuse/hw.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_usb_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_usb_t *hw_usb_init(hw_usb_callback_t callback, void *userdata) {
  (void)callback;
  (void)userdata;
  return NULL;
}

void hw_usb_deinit(hw_usb_t *usb) { (void)usb; }

bool hw_usb_valid(const hw_usb_t *usb) {
  (void)usb;
  return false;
}
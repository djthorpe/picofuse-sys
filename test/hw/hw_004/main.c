#include <test.h>

static void usb_callback(hw_usb_t *usb, hw_usb_event_t event,
                         const hw_usb_device_t *device, void *userdata) {
  (void)userdata;

  // Basic callback contract checks.
  if (usb == NULL || device == NULL) {
    return;
  }

  if (event != hw_usb_event_attached && event != hw_usb_event_detached) {
    return;
  }
}

bool test_main(void) {
  hw_usb_deinit(NULL);

  TestAssert(!hw_usb_valid(NULL), "NULL USB handle should be invalid");
  TestAssert(hw_usb_init(NULL, NULL) == NULL,
             "USB init should reject NULL callback");

  hw_usb_t *usb = hw_usb_init(usb_callback, NULL);

  if (usb != NULL) {
    TestAssert(hw_usb_valid(usb),
               "USB handle should be valid after successful init");
    hw_usb_deinit(usb);
    TestAssert(!hw_usb_valid(usb), "USB handle should be invalid after deinit");
  }

  return true;
}

TestMain(test_main)

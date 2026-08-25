#include "private.h"

#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static const char *_hid_usb_name = "usb";
static hid_device_t *_hid_usb_device = NULL;
static hw_usb_t *_hid_usb_handle = NULL;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hid_usb_deinit(hid_device_t *device, void *userdata);

static const hid_device_callbacks_t _hid_usb_callbacks = {
    .deinit = _hid_usb_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _hid_usb_callback(hw_usb_t *usb, hw_usb_event_t event,
                              const hw_usb_device_t *device,
                              void *userdata) {
  (void)usb;
  (void)userdata;

  if (device == NULL) {
    // Enumeration-complete marker (see hw_usb_init()'s doc).
    sys_debugf("hid", "usb enumeration complete");
    return;
  }

  sys_debugf("hid", "usb %s: vid=%04x pid=%04x class=%u product=%s",
             event == hw_usb_event_attached ? "attached" : "detached",
             (unsigned int)device->vid, (unsigned int)device->pid,
             (unsigned int)device->device_class, device->product);
}

static bool _hid_usb_deinit(hid_device_t *device, void *userdata) {
  (void)device;
  (void)userdata;

  hw_usb_deinit(_hid_usb_handle);
  _hid_usb_handle = NULL;
  _hid_usb_device = NULL;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_usb(hid_t *instance) {
  if (instance == NULL || _hid_usb_device != NULL) {
    return NULL;
  }

  hw_usb_t *handle = hw_usb_init(_hid_usb_callback, NULL);
  if (handle == NULL) {
    sys_debugf("hid", "usb register failed: hw_usb_init");
    return NULL;
  }

  hid_device_t *device = hid_register(instance, _hid_usb_name, 0u,
                                      hid_type_usb, hid_class_unknown, 0u,
                                      NULL, _hid_usb_callbacks);
  if (device == NULL) {
    sys_debugf("hid", "usb register failed: hid_register");
    hw_usb_deinit(handle);
    return NULL;
  }

  _hid_usb_handle = handle;
  _hid_usb_device = device;
  return device;
}

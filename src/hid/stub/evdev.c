#include "../any/private.h"
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_evdev(hid_t *instance, const char *path,
                                 bool exclusive, void *userdata) {
  (void)instance;
  (void)path;
  (void)exclusive;
  (void)userdata;
  sys_debugf("[hid] evdev unsupported on this platform");
  return NULL;
}

size_t hid_evdev_list(hid_device_list_callback_t callback, void *userdata) {
  (void)callback;
  (void)userdata;
  return 0u;
}

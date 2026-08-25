#include "private.h"

#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static const char *_hid_wifi_name = "wifi";
static hid_device_t *_hid_wifi_device = NULL;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hid_wifi_deinit(hid_device_t *device, void *userdata);

static const hid_device_callbacks_t _hid_wifi_callbacks = {
    .deinit = _hid_wifi_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _hid_wifi_callback(hw_wifi_t *wifi, hw_wifi_event_t event,
                               const hw_wifi_network_t *network,
                               void *userdata) {
  (void)wifi;
  (void)userdata;
  (void)hid_event_queue_wifi(_hid_wifi_device, event, network);
}

static bool _hid_wifi_deinit(hid_device_t *device, void *userdata) {
  (void)device;

  hw_wifi_deinit((hw_wifi_t *)userdata);
  _hid_wifi_device = NULL;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_wifi(hid_t *instance, const char *country_code) {
  if (instance == NULL || _hid_wifi_device != NULL) {
    return NULL;
  }

  hw_wifi_t *handle = hw_wifi_init_client(country_code, _hid_wifi_callback,
                                          NULL);
  if (handle == NULL) {
    sys_debugf("hid", "wifi register failed: hw_wifi_init_client");
    return NULL;
  }

  // Store the raw hw_wifi_t* as userdata (not an opaque state struct), so
  // hid_device_userdata() hands it straight back — this registers an
  // observer only; callers drive hw_wifi_scan()/connect()/disconnect()
  // themselves via that handle. Mirrors hid_type_gpio, whose userdata is
  // likewise its raw hw_gpio_t* (see hid_deregister()'s fallback branch
  // in device.c).
  hid_device_t *device = hid_register(instance, _hid_wifi_name, 0u,
                                      hid_type_wifi, hid_class_unknown, 0u,
                                      handle, _hid_wifi_callbacks);
  if (device == NULL) {
    sys_debugf("hid", "wifi register failed: hid_register");
    hw_wifi_deinit(handle);
    return NULL;
  }

  _hid_wifi_device = device;
  return device;
}

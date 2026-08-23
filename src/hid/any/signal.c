#include "private.h"

#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static const char *_hid_signal_name = "signal";
static hid_device_t *_hid_signal_device = NULL;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hid_signal_deinit(hid_device_t *device, void *userdata);

static const hid_device_callbacks_t _hid_signal_callbacks = {
    .deinit = _hid_signal_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _hid_signal_callback(sys_env_signal_t signal) {
  if (_hid_signal_device != NULL && signal != SYS_ENV_SIGNAL_NONE) {
    (void)hid_event_queue_signal(_hid_signal_device, signal);
  }
}

static bool _hid_signal_deinit(hid_device_t *device, void *userdata) {
  (void)device;
  (void)userdata;
  _hid_signal_device = NULL;
  return sys_env_signalhandler(SYS_ENV_SIGNAL_NONE, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_signal(hid_t *instance) {
  if (instance == NULL || _hid_signal_device != NULL) {
    return NULL;
  }
  if (!sys_env_signalhandler(SYS_ENV_SIGNAL_NONE, _hid_signal_callback)) {
    return NULL;
  }

  hid_device_t *device =
      hid_register(instance, _hid_signal_name, 0u, hid_type_signal,
                   hid_class_unknown, 0u, NULL, _hid_signal_callbacks);
  if (device == NULL) {
    (void)sys_env_signalhandler(SYS_ENV_SIGNAL_NONE, NULL);
    return NULL;
  } else {
    _hid_signal_device = device;
  }

  return device;
}

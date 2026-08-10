#include "private.h"

#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static bool _hid_timer_init(hid_device_t *device, void *userdata);
static bool _hid_timer_deinit(hid_device_t *device, void *userdata);

static const char *_hid_timer_name = "timer";

static const hid_device_callbacks_t _hid_timer_callbacks = {
    .init = _hid_timer_init,
    .read = NULL,
    .deinit = _hid_timer_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

static bool _hid_timer_init(hid_device_t *device, void *userdata) {
  sys_timer_t *timer = (sys_timer_t *)userdata;
  (void)device;
  if (timer == NULL) {
    return false;
  }
  return sys_timer_start(timer);
}

static bool _hid_timer_deinit(hid_device_t *device, void *userdata) {
  sys_timer_t *timer = (sys_timer_t *)userdata;

  if (timer == NULL) {
    return true;
  }

  if (device != NULL) {
    device->userdata = NULL;
  }

  sys_timer_deinit(timer);
  return true;
}

static void _hid_timer_callback(sys_timer_t *timer) {
  if (!sys_timer_valid(timer)) {
    return;
  }

  hid_t *instance;
  hid_device_t *device;
  if (!_hid_find_device_by_timer(timer, &instance, &device) || device == NULL ||
      instance == NULL) {
    return;
  }

  if (!sys_event_queue_valid(instance->queue)) {
    return;
  }

  hid_event_t *event = _hid_event_pool_retain(instance);
  if (event == NULL) {
    return;
  } else {
    event->device = device;
    event->type = hid_event_type_timer;
    event->data.timer.userdata = sys_timer_get_userdata(timer);
  }

  if (!sys_event_queue_try_push(instance->queue, (sys_event_t)event)) {
    hid_event_free(event);
  }

  if (!device->timer_repeating) {
    sys_timer_deinit(timer);
    device->userdata = NULL;
    device->timer_remove_after_event = true;
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_timer(hid_t *instance, uint32_t id,
                                 uint32_t interval_ms, bool repeating,
                                 void *userdata) {
  if (instance == NULL || interval_ms == 0u) {
    return NULL;
  }

  if (!_hid_event_pool_init(instance)) {
    return NULL;
  }

  sys_timer_t *timer;
  timer = sys_timer_init(interval_ms, userdata, _hid_timer_callback);
  if (timer == NULL) {
    return NULL;
  }

  hid_device_t *device;
  device = hid_register(instance, _hid_timer_name, id, hid_type_timer, 0u,
                        timer, _hid_timer_callbacks);
  if (device == NULL) {
    sys_timer_deinit(timer);
    return NULL;
  } else {
    device->timer_repeating = repeating;
    device->timer_remove_after_event = false;
  }

  return device;
}

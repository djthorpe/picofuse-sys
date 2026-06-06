#include <picofuse/hid.h>
#include <picofuse/sys.h>

#include "private.h"

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Queue a keycode-based HID event on the owning instance queue.
 */
bool hid_event_queue_keycode(hid_device_t *device, hid_state_t state,
                             uint16_t keycode) {
  hid_t *instance;
  hid_state_t next_state;
  hid_state_t translated_state;
  hid_event_t *event = (hid_event_t *)sys_calloc(1u, sizeof(hid_event_t));
  if (device == NULL) {
    return false;
  }

  instance = _hid_device_instance(device);
  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    return false;
  }

  if (event == NULL) {
    return false;
  }

  translated_state = hid_keycode_to_state(keycode);
  next_state = device->state;

  if ((state & hid_state_on) != 0u) {
    if (translated_state != hid_state_none) {
      next_state |= translated_state;
    }
    next_state |= hid_state_on;
    next_state &= ~hid_state_off;
  }

  if ((state & hid_state_off) != 0u) {
    if (translated_state != hid_state_none) {
      next_state &= ~translated_state;
    }
    next_state |= hid_state_off;
    next_state &= ~hid_state_on;
  }

  device->state = next_state;

  event->device = device;
  event->type = hid_event_type_keycode;
  event->data.keycode.state = next_state;
  event->data.keycode.keycode = keycode;

  if (!sys_event_queue_try_push(instance->queue, (sys_event_t)event)) {
    hid_event_free(event);
    return false;
  }

  return true;
}

/**
 * @brief Queue a float metric HID event on a HID instance queue.
 */
bool hid_event_queue_metric_float(hid_device_t *device, const char *name,
                                  const char *unit, float value) {
  hid_t *instance;
  hid_event_t *event = (hid_event_t *)sys_calloc(1u, sizeof(hid_event_t));

  if (device == NULL || name == NULL || unit == NULL || event == NULL) {
    hid_event_free(event);
    return false;
  }

  instance = _hid_device_instance(device);
  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    hid_event_free(event);
    return false;
  }

  event->device = device;
  event->type = hid_event_type_metric;
  event->data.metric.name = name;
  event->data.metric.unit = unit;
  event->data.metric.value = value;

  if (!sys_event_queue_try_push(instance->queue, (sys_event_t)event)) {
    hid_event_free(event);
    return false;
  }

  return true;
}

/**
 * @brief Free a HID event object allocated by hid_event_alloc.
 */
void hid_event_free(hid_event_t *event) {
  if (event != NULL && event->type == hid_event_type_timer &&
      event->device != NULL && event->device->timer_remove_after_event) {
    hid_t *instance = _hid_device_instance(event->device);

    if (instance != NULL) {
      event->device->timer_remove_after_event = false;
      (void)hid_deregister(instance, event->device);
    }
  }

  sys_free(event);
}

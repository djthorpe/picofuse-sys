#include <picofuse/hid.h>
#include <picofuse/sys.h>

#include "private.h"

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

hid_event_t *_hid_event_pool_retain(hid_t *instance) {
  size_t i;
  size_t capacity;

  if (instance == NULL || instance->events == NULL ||
      !sys_event_queue_valid(instance->queue)) {
    return NULL;
  }

  capacity = sys_event_queue_capacity(instance->queue);
  if (capacity == 0u) {
    return NULL;
  }

  for (i = 0u; i < capacity; ++i) {
    hid_event_type_t expected = hid_event_type_none;
    if (__atomic_compare_exchange_n(&instance->events[i].type, &expected,
                                    hid_event_type_keycode, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
      instance->events[i].device = NULL;
      sys_memset(&instance->events[i].data, 0,
                 sizeof(instance->events[i].data));
      return &instance->events[i];
    }
  }

  return NULL;
}

static void _hid_event_pool_release(hid_event_t *event) {
  size_t capacity;

  if (event == NULL || event->device == NULL) {
    return;
  }

  hid_t *instance = event->device->instance;
  if (instance == NULL || instance->events == NULL ||
      !sys_event_queue_valid(instance->queue)) {
    return;
  }

  capacity = sys_event_queue_capacity(instance->queue);
  if (capacity == 0u) {
    return;
  }

  if (event < instance->events || event >= (instance->events + capacity)) {
    return;
  }

  size_t slot = (size_t)(event - instance->events);
  instance->events[slot].device = NULL;
  __atomic_store_n(&instance->events[slot].type, hid_event_type_none,
                   __ATOMIC_RELEASE);
}

bool _hid_event_pool_init(hid_t *instance) {
  size_t capacity;

  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    return false;
  }

  if (instance->events != NULL) {
    return true;
  }

  capacity = sys_event_queue_capacity(instance->queue);
  if (capacity == 0u) {
    return false;
  }

  instance->events = (hid_event_t *)sys_calloc(capacity, sizeof(hid_event_t));
  if (instance->events == NULL) {
    return false;
  }

  sys_memset(instance->events, 0, capacity * sizeof(hid_event_t));
  return true;
}

void _hid_event_pool_deinit(hid_t *instance) {
  if (instance == NULL) {
    return;
  }

  if (instance->events != NULL) {
    sys_free(instance->events);
    instance->events = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Queue a keycode-based HID event on the owning instance queue.
 */
bool hid_event_queue_keycode(hid_device_t *device, hid_state_t state,
                             uint16_t keycode) {
  if (device == NULL) {
    return false;
  }

  hid_t *instance = device->instance;
  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    return false;
  }
  if (!_hid_event_pool_init(instance)) {
    return false;
  }
  hid_event_t *event = _hid_event_pool_retain(instance);
  if (event == NULL) {
    return false;
  }

  hid_state_t translated_state = hid_keycode_to_state(keycode);
  hid_state_t next_state = device->state;
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

  // Transient one-shot annotations (e.g. auto-repeat, click counts) describe
  // this event only, so they are reported here without being folded into
  // device->state, where they would otherwise incorrectly linger and show
  // up on unrelated later events.
  hid_state_t transient_state =
      state & (hid_state_repeat | hid_state_click | hid_state_double_click |
               hid_state_triple_click | hid_state_long_click);

  event->device = device;
  event->type = hid_event_type_keycode;
  event->data.keycode.state = next_state | transient_state;
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
  hid_event_t *event;

  if (device == NULL || name == NULL || unit == NULL) {
    return false;
  }

  instance = device->instance;
  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    return false;
  }

  if (!_hid_event_pool_init(instance)) {
    return false;
  }

  event = _hid_event_pool_retain(instance);
  if (event == NULL) {
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
 * @brief Queue a touch-based HID event on the owning instance queue.
 */
bool hid_event_queue_touch(hid_device_t *device, hid_state_t state,
                           pix_point_t point, uint8_t slot) {
  hid_t *instance;
  hid_event_t *event;

  if (device == NULL) {
    return false;
  }

  instance = device->instance;
  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    return false;
  }

  if (!_hid_event_pool_init(instance)) {
    return false;
  }

  event = _hid_event_pool_retain(instance);
  if (event == NULL) {
    return false;
  }

  event->device = device;
  event->type = hid_event_type_touch;
  event->data.touch.state = state;
  event->data.touch.point = point;
  event->data.touch.slot = slot;

  if (!sys_event_queue_try_push(instance->queue, (sys_event_t)event)) {
    hid_event_free(event);
    return false;
  }

  return true;
}

/**
 * @brief Queue a signal HID event on the owning instance queue.
 */
bool hid_event_queue_signal(hid_device_t *device, sys_env_signal_t signal) {
  hid_t *instance;
  hid_event_t *event;

  if (device == NULL || signal == SYS_ENV_SIGNAL_NONE) {
    return false;
  }

  instance = device->instance;
  if (instance == NULL || !sys_event_queue_valid(instance->queue)) {
    return false;
  }

  if (!_hid_event_pool_init(instance)) {
    return false;
  }

  event = _hid_event_pool_retain(instance);
  if (event == NULL) {
    return false;
  }

  event->device = device;
  event->type = hid_event_type_signal;
  event->data.signal.signal = signal;

  if (!sys_event_queue_try_push(instance->queue, (sys_event_t)event)) {
    hid_event_free(event);
    return false;
  }

  return true;
}

/**
 * @brief Free a HID event object allocated by event queue helpers.
 */
void hid_event_free(hid_event_t *event) {
  if (event != NULL && event->type == hid_event_type_timer &&
      event->device != NULL && event->device->timer_remove_after_event) {
    hid_t *instance = event->device->instance;

    if (instance != NULL && sys_event_queue_valid(instance->queue)) {
      event->device->timer_remove_after_event = false;
      (void)hid_deregister(instance, event->device);
    }
  }

  _hid_event_pool_release(event);
}

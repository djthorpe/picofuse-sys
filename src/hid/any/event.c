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
  event->state = next_state;
  event->keycode = keycode;
  event->point.x = 0;
  event->point.y = 0;
  event->slot = 0u;

  if (!sys_event_queue_try_push(instance->queue, (sys_event_t)event)) {
    hid_event_free(event);
    return false;
  }

  return true;
}

/**
 * @brief Free a HID event object allocated by hid_event_alloc.
 */
void hid_event_free(hid_event_t *event) { sys_free(event); }

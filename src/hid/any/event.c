#include <picofuse/hid.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Allocate a GPIO-derived HID event object.
 */
hid_event_t *hid_event_alloc(hid_device_t *device, hid_state_t state,
                             uint16_t keycode) {
  hid_event_t *event = (hid_event_t *)sys_calloc(1u, sizeof(hid_event_t));
  if (event == NULL) {
    return NULL;
  }

  event->device = device;
  event->state = state;
  event->keycode = keycode;
  event->point.x = 0;
  event->point.y = 0;
  event->slot = 0u;
  return event;
}

/**
 * @brief Free a HID event object allocated by hid_event_alloc.
 */
void hid_event_free(hid_event_t *event) { sys_free(event); }

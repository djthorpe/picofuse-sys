#include <picofuse/hid.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Allocate a GPIO-derived HID event object.
 */
hid_event_t *hid_alloc_gpio_event(void) {
  return (hid_event_t *)sys_calloc(1u, sizeof(hid_event_t));
}

/**
 * @brief Free a HID event object allocated by hid_alloc_gpio_event.
 */
void hid_event_free(hid_event_t *event) { sys_free(event); }

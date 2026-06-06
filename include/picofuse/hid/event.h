/**
 * @file event.h
 * @brief HID event type definitions.
 */
#pragma once

#include "device.h"
#include "keycode.h"
#include <picofuse/pix/types.h>
#include <stdbool.h>

/**
 * @brief Represents a single HID input event.
 */
typedef struct {
  hid_state_t state;    ///< HID state flags for this event.
  hid_device_t *device; ///< HID child-device associated with this event.
  uint16_t keycode;     ///< HID keycode associated with the event.
  pix_point_t point;    ///< Touch/event coordinates in pixels.
  uint8_t slot;         ///< Touch slot index for multi-touch tracking.
} hid_event_t;

/**
 * @brief Allocate a HID event for GPIO-derived input.
 * @return Newly allocated HID event, or NULL on allocation failure.
 */
hid_event_t *hid_alloc_gpio_event(void);

/**
 * @brief Free a HID event allocated by @ref hid_alloc_gpio_event.
 * @param event Event pointer to release.
 */
void hid_event_free(hid_event_t *event);

/**
 * @file event.h
 * @brief HID event type definitions.
 */
#pragma once

#include "keycode.h"
#include <picofuse/pix/types.h>

/**
 * @brief Represents a single HID input event.
 */
typedef struct {
  uint16_t keycode;  ///< HID keycode associated with the event.
  hid_state_t state; ///< HID state flags for this event.
  pix_point_t point; ///< Touch/event coordinates in pixels.
  uint8_t slot;      ///< Touch slot index for multi-touch tracking.
} hid_event_t;

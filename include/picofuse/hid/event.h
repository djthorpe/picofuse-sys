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
 * @brief Queue a keycode-based HID event to the owning HID instance queue.
 * @param device HID device associated with the event.
 * @param state Input transition flags to apply (for example
 * hid_state_on/hid_state_off).
 * @param keycode HID keycode associated with the event.
 * @retval true Event queued successfully.
 * @retval false Queueing failed.
 *
 * The helper applies @p state to the device-local HID state, then queues an
 * event containing the resulting state snapshot.
 */
bool hid_event_queue_keycode(hid_device_t *device, hid_state_t state,
                             uint16_t keycode);

/**
 * @brief Free a HID event allocated internally by HID queue helpers.
 * @param event Event pointer to release.
 */
void hid_event_free(hid_event_t *event);

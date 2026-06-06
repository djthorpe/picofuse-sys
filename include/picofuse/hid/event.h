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
 * @brief HID event payload selector.
 */
typedef enum {
  hid_event_type_keycode = 1,
  hid_event_type_touch = 2,
  hid_event_type_metric = 3,
} hid_event_type_t;

/**
 * @brief Keycode-oriented HID event payload.
 */
typedef struct {
  hid_state_t state; ///< HID state flags for this event.
  uint16_t keycode;  ///< HID keycode associated with the event.
} hid_keycode_t;

/**
 * @brief Touch-oriented HID event payload.
 */
typedef struct {
  hid_state_t state; ///< HID state flags for this touch event.
  pix_point_t point; ///< Touch/event coordinates in pixels.
  uint8_t slot;      ///< Touch slot index for multi-touch tracking.
} hid_touch_t;

/**
 * @brief Metric-oriented HID event payload.
 */
typedef struct {
  const char *name; ///< Metric name.
  const char *unit; ///< Metric unit string.
  float value;      ///< Metric value.
} hid_metric_t;

/**
 * @brief Represents a single HID input event.
 */
typedef struct {
  hid_device_t *device;  ///< HID child-device associated with this event.
  hid_event_type_t type; ///< Event payload tag.
  union {
    hid_keycode_t keycode;
    hid_touch_t touch;
    hid_metric_t metric;
  } data;
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
 * @brief Queue a float metric HID event to a HID instance queue.
 * @param device HID device associated with the destination queue.
 * @param name Metric name string.
 * @param unit Metric unit string.
 * @param value Metric value.
 * @retval true Event queued successfully.
 * @retval false Queueing failed.
 */
bool hid_event_queue_metric_float(hid_device_t *device, const char *name,
                                  const char *unit, float value);

/**
 * @brief Free a HID event allocated internally by HID queue helpers.
 * @param event Event pointer to release.
 */
void hid_event_free(hid_event_t *event);

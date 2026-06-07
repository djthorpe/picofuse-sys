/**
 * @file infrared.h
 * @brief Infrared (IR) receiver interface
 * @defgroup Infrared Infrared
 * @ingroup Hardware
 *
 * Infrared receiver interface for capturing and processing IR remote control
 * signals. This module provides functions to initialize an IR receiver,
 * configure callbacks for signal events, and process MARK/SPACE/TIMEOUT timing
 * data. The implementation of specific protocols is included in the driver
 * library.
 *
 * The callback is called with either MARK (high pulse), SPACE (low pulse),
 * or TIMEOUT (overly long pulse) events. Events above 50ms are marked as
 * TIMEOUT values, in which case detection of specific IR codes should be
 * reset.
 *
 * @example examples/hw/infrared/main.c
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef HW_IR_CAPACITY
#define HW_IR_CAPACITY 2u
#endif

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Infrared receiver handle.
 * @ingroup Infrared
 * @headerfile infrared.h picofuse/hw.h
 */
typedef struct hw_infrared_rx_t hw_infrared_rx_t;

/**
 * @brief Infrared event types.
 * @ingroup Infrared
 * @headerfile infrared.h picofuse/hw.h
 */
typedef enum hw_infrared_event_t {
  HW_INFRARED_EVENT_TIMEOUT = (1 << 0),
  HW_INFRARED_EVENT_MARK = (1 << 1),
  HW_INFRARED_EVENT_SPACE = (1 << 2)
} hw_infrared_event_t;

/**
 * @brief Infrared receiver callback.
 * @ingroup Infrared
 * @headerfile infrared.h picofuse/hw.h
 */
typedef void (*hw_infrared_rx_callback_t)(hw_infrared_event_t event,
                                          uint32_t duration_us,
                                          void *user_data);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize an Infrared receiver.
 * @ingroup Infrared
 * @param bank The GPIO bank number for the Infrared receiver.
 * @param gpio The GPIO pin number for the Infrared receiver.
 * @param callback The callback function to be called on IR events.
 * @param user_data Pointer to user data to be passed to the callback.
 * @return A pointer to the initialized Infrared receiver, or `NULL` when
 *         unsupported or when the receiver pool is exhausted.
 */
hw_infrared_rx_t *hw_infrared_rx_init(uint8_t bank, uint8_t gpio,
                                      hw_infrared_rx_callback_t callback,
                                      void *user_data);

/**
 * @brief Initialize an Infrared receiver for a specific device.
 * @ingroup Infrared
 * @param device Device path or identifier for the Infrared receiver.
 * @param callback The callback function to be called on IR events.
 * @param user_data Pointer to user data to be passed to the callback.
 * @return A pointer to the initialized Infrared receiver, or `NULL` when
 *         unsupported or when the receiver pool is exhausted.
 */
hw_infrared_rx_t *hw_infrared_rx_init_device(const char *device,
                                             hw_infrared_rx_callback_t callback,
                                             void *user_data);

/**
 * @brief Deinitialize and release an Infrared receiver.
 * @ingroup Infrared
 * @param rx Pointer to the Infrared receiver structure to deinitialize.
 */
void hw_infrared_rx_deinit(hw_infrared_rx_t *rx);

#ifdef __cplusplus
}
#endif
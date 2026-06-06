/**
 * @file ft6236.h
 * @brief FocalTech FT6236 capacitive touch controller interface.
 * @defgroup FT6236 FT6236
 * @ingroup Device
 *
 * This module provides a device-level API for FT6236-compatible capacitive
 * touch controllers over I2C.
 */
#pragma once

#include <picofuse/hid/event.h>
#include <picofuse/hw.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque FT6236 handle.
 * @ingroup FT6236
 */
typedef struct dev_ft6236_t dev_ft6236_t;

/**
 * @brief Maximum number of simultaneous touch contacts reported by FT6236.
 * @ingroup FT6236
 */
#define DEV_FT6236_MAX_POINTS 2u

/**
 * @brief Optional FT6236 initialization options.
 * @ingroup FT6236
 */
typedef struct {
  uint8_t i2c_address; ///< 7-bit I2C address.
  bool irq_active_low; ///< True when the interrupt pin is active low.
} dev_ft6236_config_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Fill an FT6236 config struct with safe defaults.
 * @ingroup FT6236
 * @param config Config structure to initialize.
 */
void dev_ft6236_default_config(dev_ft6236_config_t *config);

/**
 * @brief Initialize an FT6236-compatible touch controller over I2C.
 * @ingroup FT6236
 * @param i2c I2C interface to use.
 * @param int_pin Optional interrupt GPIO handle. Pass `NULL` to always poll.
 * @param config Optional pointer to initialization options. Pass `NULL` to use
 * default values.
 * @return FT6236 handle or `NULL` on failure.
 */
dev_ft6236_t *dev_ft6236_init(hw_i2c_t *i2c, hw_gpio_t *int_pin,
                              const dev_ft6236_config_t *config);

/**
 * @brief Deinitialize an FT6236 controller.
 * @ingroup FT6236
 * @param ft6236 FT6236 handle.
 *
 * Passing `NULL` is safe and is a no-op.
 */
void dev_ft6236_deinit(dev_ft6236_t *ft6236);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Check whether an FT6236 handle is valid.
 * @ingroup FT6236
 * @param ft6236 FT6236 handle.
 * @retval true The handle is valid.
 * @retval false The handle is invalid.
 */
bool dev_ft6236_valid(const dev_ft6236_t *ft6236);

/**
 * @brief Report whether the FT6236 interrupt line is currently asserted.
 * @ingroup FT6236
 * @param ft6236 FT6236 handle.
 * @retval true The controller is signalling a pending update.
 * @retval false No pending update is signalled, or no interrupt pin exists.
 */
bool dev_ft6236_irq_active(const dev_ft6236_t *ft6236);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Poll the controller and parse the latest touch frame.
 * @ingroup FT6236
 * @param ft6236 FT6236 handle.
 * @param events Array receiving parsed touch events, indexed by touch slot.
 * Must have room for @ref DEV_FT6236_MAX_POINTS entries.
 * @param out_touch_count Optional pointer receiving number of active touch
 * points.
 * Pass `NULL` to ignore.
 * @retval true Read succeeded.
 * @retval false Read failed.
 *
 * When an interrupt pin is configured, this function skips the I2C transaction
 * when no touch is pending and the previous frame was already idle.
 */
bool dev_ft6236_poll(dev_ft6236_t *ft6236,
                     hid_event_t events[DEV_FT6236_MAX_POINTS],
                     uint8_t *out_touch_count);

/** @} */
/**
 * @file tca9555.h
 * @brief TI TCA9555 16-bit I/O expander interface.
 * @defgroup TCA9555 TCA9555
 * @ingroup Device
 *
 * This module provides a device-level API for the TCA9555 I2C I/O expander.
 */
#pragma once

#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque TCA9555 handle.
 * @ingroup TCA9555
 */
typedef struct dev_tca9555_t dev_tca9555_t;

/**
 * @brief Supported TCA9555 7-bit I2C addresses.
 * @ingroup TCA9555
 */
typedef enum {
  DEV_TCA9555_I2C_ADDR_ANY = 0x00, ///< Auto-detect across 0x20..0x27
  DEV_TCA9555_I2C_ADDR_20 = 0x20,  ///< A2=A1=A0=0
  DEV_TCA9555_I2C_ADDR_21 = 0x21,  ///< A0=1
  DEV_TCA9555_I2C_ADDR_22 = 0x22,  ///< A1=1
  DEV_TCA9555_I2C_ADDR_23 = 0x23,  ///< A1=A0=1
  DEV_TCA9555_I2C_ADDR_24 = 0x24,  ///< A2=1
  DEV_TCA9555_I2C_ADDR_25 = 0x25,  ///< A2=A0=1
  DEV_TCA9555_I2C_ADDR_26 = 0x26,  ///< A2=A1=1
  DEV_TCA9555_I2C_ADDR_27 = 0x27   ///< A2=A1=A0=1
} dev_tca9555_i2c_addr_t;

/**
 * @brief Callback invoked when polled TCA9555 input state changes.
 * @ingroup TCA9555
 * @param tca9555 TCA9555 handle.
 * @param value New packed pin values (bit 0=P0.0 ... bit 15=P1.7).
 * @param userdata Opaque pointer supplied at initialization.
 */
typedef void (*dev_tca9555_callback_t)(dev_tca9555_t *tca9555, uint16_t value,
                                       void *userdata);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize a TCA9555 over I2C.
 * @ingroup TCA9555
 * @param i2c I2C interface to use.
 * @param i2c_addr 7-bit TCA9555 I2C address, or
 * @ref DEV_TCA9555_I2C_ADDR_ANY to auto-detect.
 * @param output_mask Pin mask for output-capable pins.
 * @param callback Optional callback for input-state changes. Pass NULL to
 * disable timer polling callbacks.
 * @param callback_userdata Optional user data pointer passed to @p callback.
 *
 * When @p i2c_addr is @ref DEV_TCA9555_I2C_ADDR_ANY, initialization probes
 * addresses 0x20 through 0x27 and uses the first responding device.
 *
 * Bits set in @p output_mask are configured as outputs. Bits cleared are
 * configured as inputs. Output pins are initialized low and polarity
 * inversion is disabled.
 *
 * When @p callback is non-NULL, a periodic timer is started internally to
 * poll input registers and invoke the callback whenever the input state
 * changes.
 * @return TCA9555 handle or NULL on failure.
 */
dev_tca9555_t *dev_tca9555_init_i2c(hw_i2c_t *i2c,
                                    dev_tca9555_i2c_addr_t i2c_addr,
                                    uint16_t output_mask,
                                    dev_tca9555_callback_t callback,
                                    void *callback_userdata);

/**
 * @brief Deinitialize a TCA9555 device.
 * @ingroup TCA9555
 * @param tca9555 TCA9555 handle.
 */
void dev_tca9555_deinit(dev_tca9555_t *tca9555);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Get the configured 7-bit I2C address.
 * @ingroup TCA9555
 * @param tca9555 TCA9555 handle.
 * @return I2C address, or 0 when handle is invalid.
 */
uint8_t dev_tca9555_i2c_addr(const dev_tca9555_t *tca9555);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Read all TCA9555 pin values.
 * @ingroup TCA9555
 * @param tca9555 TCA9555 handle.
 * @param value Receives packed pin values (bit 0=P0.0 ... bit 15=P1.7).
 * @retval true Read succeeded.
 * @retval false Read failed.
 */
bool dev_tca9555_read(dev_tca9555_t *tca9555, uint16_t *value);

/**
 * @brief Write output values.
 * @ingroup TCA9555
 * @param tca9555 TCA9555 handle.
 * @param value Packed output values (bit 0=P0.0 ... bit 15=P1.7).
 *
 * Only pins enabled by the initialization output mask are affected.
 * @retval true Write succeeded.
 * @retval false Write failed.
 */
bool dev_tca9555_write(dev_tca9555_t *tca9555, uint16_t value);

/**
 * @brief Register a Pimoroni pad HID device.
 * @ingroup TCA9555
 * @param hid HID instance that owns the registration.
 * @param i2c I2C interface used to initialize the TCA9555.
 * @param i2c_addr 7-bit TCA9555 I2C address, or
 * @ref DEV_TCA9555_I2C_ADDR_ANY to auto-detect.
 * @param output_mask Pin mask for output-capable pins.
 *
 * This helper initializes and registers a TCA9555 as a HID device of
 * type @ref hid_type_other. On HID deregistration, the TCA9555 device is
 * deinitialized automatically. The I2C handle remains caller-owned.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *dev_pimoroni_pad_register(hid_t *hid, hw_i2c_t *i2c,
                                        dev_tca9555_i2c_addr_t i2c_addr,
                                        uint16_t output_mask);

/** @} */

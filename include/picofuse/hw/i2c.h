/**
 * @file i2c.h
 * @brief I2C (Inter-Integrated Circuit) interface
 * @defgroup I2C I2C
 * @ingroup Hardware
 *
 * Inter-Integrated Circuit (I2C) interface for hardware platforms.
 * This module provides functions to initialize I2C peripherals and perform
 * bidirectional transfers with I2C devices in master mode.
 */
#pragma once
#include "gpio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque I2C handle.
 * @ingroup I2C
 * @headerfile i2c.h hw/hw.h
 */
typedef struct hw_i2c_t hw_i2c_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize an I2C interface using the platform default adapter and
 * pins.
 * @ingroup I2C
 * @param baud_rate Desired I2C baud rate in Hz.
 * @return I2C handle or NULL on failure.
 */
hw_i2c_t *hw_i2c_init_default(uint32_t baud_rate);

/**
 * @brief Initialize an I2C interface with a specific adapter and pins.
 * @ingroup I2C
 * @param index I2C adapter index to use.
 * @param sda_pin GPIO handle for SDA.
 * @param scl_pin GPIO handle for SCL.
 * @param baud_rate Desired I2C baud rate in Hz.
 * @return I2C handle or NULL on failure.
 */
hw_i2c_t *hw_i2c_init(uint8_t index, const hw_gpio_t *sda_pin,
                      const hw_gpio_t *scl_pin, uint32_t baud_rate);

/**
 * @brief Initialize an I2C interface from a platform-specific device path.
 * @ingroup I2C
 * @param device Device identifier such as `/dev/i2c-1`.
 * @param baud_rate Desired I2C baud rate in Hz.
 * @return I2C handle or NULL on failure.
 *
 * This entry point is intended for platforms where I2C buses are exposed as
 * named devices rather than a fixed, enumerable set of adapters.
 */
hw_i2c_t *hw_i2c_init_device(const char *device, uint32_t baud_rate);

/**
 * @brief Deinitialize an I2C interface.
 * @ingroup I2C
 * @param i2c I2C handle.
 *
 * Safe to call on an invalid or already deinitialized handle; in that case it
 * is a no-op. After deinitialization, `hw_i2c_valid()` returns false.
 */
void hw_i2c_deinit(hw_i2c_t *i2c);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Get the total number of available I2C adapters.
 * @ingroup I2C
 * @return Number of I2C adapters available on the current platform.
 *
 * On platforms that open I2C buses by device path, this may return `0` even
 * when I2C is supported. In that case, use `hw_i2c_init_device()` instead of
 * enumerating adapters by index.
 */
uint8_t hw_i2c_count(void);

/**
 * @brief Check whether an I2C handle is valid.
 * @ingroup I2C
 * @param i2c I2C handle.
 * @retval true The I2C handle is valid.
 * @retval false The I2C handle is invalid.
 */
bool hw_i2c_valid(const hw_i2c_t *i2c);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Detect whether an I2C device is present at the given address.
 * @ingroup I2C
 * @param i2c I2C handle.
 * @param addr 7-bit slave address.
 * @retval true The device responded.
 * @retval false No device responded or the request was invalid.
 */
bool hw_i2c_detect(hw_i2c_t *i2c, uint8_t addr);

/**
 * @brief Perform an I2C transfer operation.
 * @ingroup I2C
 * @param i2c I2C handle.
 * @param addr 7-bit slave address.
 * @param data Buffer used for transmitted and received data.
 * @param tx Number of bytes to transmit from `data`.
 * @param rx Number of bytes to receive into `data + tx`.
 * @param timeout_ms Timeout in milliseconds for the operation. Set to `0` to
 * request an immediate or best-effort transfer with no blocking beyond what
 * the platform backend requires.
 * @return Number of bytes transferred, or `0` on failure.
 *
 * This method supports write-only (`tx > 0, rx == 0`), read-only
 * (`tx == 0, rx > 0`), and write-then-read (`tx > 0, rx > 0`) transfers.
 *
 * For combined write-then-read transfers, the backend should keep control of
 * the bus between phases and issue a repeated START rather than a STOP between
 * the write and read portions when the platform supports it.
 *
 * When `rx > 0`, received bytes are written to `((uint8_t *)data) + tx`, so
 * the caller must provide a buffer large enough to hold `tx + rx` bytes.
 */
size_t hw_i2c_xfr(hw_i2c_t *i2c, uint8_t addr, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms);

/**
 * @brief Read bytes from a register on an I2C device.
 * @ingroup I2C
 * @param i2c I2C handle.
 * @param addr 7-bit slave address.
 * @param reg Register address to read from.
 * @param data Buffer to receive the bytes.
 * @param len Number of bytes to read.
 * @param timeout_ms Timeout in milliseconds for the operation.
 * @return Number of bytes read, or `0` on failure.
 *
 * This is a convenience wrapper around `hw_i2c_xfr()` for the common
 * register-read pattern.
 */
size_t hw_i2c_read(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, void *data,
                   size_t len, uint32_t timeout_ms);

/**
 * @brief Write bytes to a register on an I2C device.
 * @ingroup I2C
 * @param i2c I2C handle.
 * @param addr 7-bit slave address.
 * @param reg Register address to write to.
 * @param data Buffer containing bytes to write.
 * @param len Number of bytes to write.
 * @param timeout_ms Timeout in milliseconds for the operation.
 * @return Number of bytes written, or `0` on failure.
 *
 * This is a convenience wrapper around `hw_i2c_xfr()` for the common
 * register-write pattern.
 */
size_t hw_i2c_write(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, const void *data,
                    size_t len, uint32_t timeout_ms);

/** @} */
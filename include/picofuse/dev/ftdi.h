/**
 * @file ftdi.h
 * @brief FTDI FT232H (and compatible MPSSE-capable) USB bridge interface.
 * @defgroup Ftdi Ftdi
 * @ingroup Device
 *
 * This module provides a device-level API for FTDI USB bridge chips with an
 * MPSSE engine (FT232H, FT2232H, FT4232H), exposing their single shared
 * engine as SPI, I2C, or plain GPIO. Only one of those modes is active on a
 * given handle at a time; re-initializing into a different mode reconfigures
 * the underlying engine.
 */
#pragma once
#include <picofuse/hw/i2c.h>
#include <picofuse/hw/spi.h>
#include <picofuse/hw/usb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque FTDI device handle.
 * @ingroup Ftdi
 */
typedef struct dev_ftdi_t dev_ftdi_t;

/**
 * @brief Opaque cursor for @ref dev_ftdi_next.
 * @ingroup Ftdi
 */
typedef struct dev_ftdi_iterator_t dev_ftdi_iterator_t;

///////////////////////////////////////////////////////////////////////////////
// ENUMERATION

/** @name Enumeration
 * @{ */

/**
 * @brief Enumerate attached, supported FTDI devices.
 * @ingroup Ftdi
 * @param iterator In/out cursor. Point a `dev_ftdi_iterator_t *` variable
 * initialized to NULL at the first call, e.g.:
 * @code
 * dev_ftdi_iterator_t *it = NULL;
 * const hw_usb_device_t *device;
 * while ((device = dev_ftdi_next(&it)) != NULL) { ... }
 * @endcode
 * @return Descriptor of the next attached device, or NULL once enumeration
 * is complete or on error.
 *
 * On the first call, `*iterator` must be NULL; this function then
 * allocates cursor state and stores it there. Successive calls return the
 * next device until none remain, at which point NULL is returned,
 * `*iterator` is reset to NULL, and the cursor state is freed
 * automatically. The enumeration is a single snapshot taken on the first
 * call, so it stays consistent even if devices are attached or detached
 * while iterating.
 *
 * The iterator must be called repeatedly until it returns NULL to avoid
 * leaking the cursor state.
 *
 * The returned pointer is valid until the next call to @ref dev_ftdi_next
 * with the same @p iterator. Pass @ref hw_usb_device_t.serial to
 * @ref dev_ftdi_init to open the corresponding device.
 */
const hw_usb_device_t *dev_ftdi_next(dev_ftdi_iterator_t **iterator);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Open an attached FTDI device.
 * @ingroup Ftdi
 * @param serial Serial number string as reported by the device (see
 * @ref dev_ftdi_next), or NULL to open the first attached, supported
 * device.
 * @return FTDI handle, or NULL on failure (including no matching device).
 */
dev_ftdi_t *dev_ftdi_init(const char *serial);

/**
 * @brief Close an FTDI device.
 * @ingroup Ftdi
 * @param ftdi FTDI handle.
 *
 * Safe to call on an invalid or already deinitialized handle; in that case
 * it is a no-op.
 */
void dev_ftdi_deinit(dev_ftdi_t *ftdi);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// SPI

/** @name SPI
 * @{ */

/**
 * @brief Configure the device's MPSSE engine for SPI.
 * @ingroup Ftdi
 * @param ftdi FTDI handle.
 * @param cs_pin Pin number to drive as chip-select (see @ref
 * dev_ftdi_gpio_init for bit layout). Must not be one of the pins the MPSSE
 * engine uses for SPI clock/MOSI/MISO.
 * @param baud_rate Desired SPI clock rate in Hz.
 * @param config Optional pointer to extended SPI configuration. Pass `NULL`
 * to use default mode and frame size. `config->cs_active_low` controls the
 * idle/asserted level driven on @p cs_pin.
 * @retval true The engine is now configured for SPI.
 * @retval false Configuration failed; any previously active mode
 * (SPI/I2C/GPIO) is left unspecified and should be reconfigured before use.
 *
 * Reconfigures the shared MPSSE engine; any I2C or GPIO mode previously
 * active on @p ftdi is replaced. Unlike @ref hw_spi_init, which is given an
 * already-initialized @ref hw_gpio_t for chip-select, this takes a bare pin
 * number because GPIO and SPI cannot be independently active engine modes
 * on this chip: @ref dev_ftdi_spi_xfr toggles @p cs_pin itself, internally,
 * around each transfer, using MPSSE's ability to bit-bang auxiliary pins
 * while clocking data.
 */
bool dev_ftdi_spi_init(dev_ftdi_t *ftdi, uint8_t cs_pin, uint32_t baud_rate,
                       const hw_spi_config_t *config);

/**
 * @brief Perform an SPI transfer operation.
 * @ingroup Ftdi
 * @param ftdi FTDI handle, previously configured with @ref dev_ftdi_spi_init.
 * @param data Buffer used for transmitted and received bytes.
 * @param tx Number of bytes to transmit from `data`.
 * @param rx Number of bytes to receive into `data + tx`.
 * @param timeout_ms Timeout in milliseconds for the operation. Set to `0` to
 * use the backend's default transfer path.
 * @return Number of bytes transferred, or `0` on failure.
 *
 * This method supports write-only (`tx > 0, rx == 0`), read-only
 * (`tx == 0, rx > 0`), and write-then-read (`tx > 0, rx > 0`) transfers.
 * The chip-select pin configured in @ref dev_ftdi_spi_init is asserted
 * before the transfer and released afterward; the caller does not manage
 * it directly.
 */
size_t dev_ftdi_spi_xfr(dev_ftdi_t *ftdi, void *data, size_t tx, size_t rx,
                        uint32_t timeout_ms);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// I2C

/** @name I2C
 * @{ */

/**
 * @brief Configure the device's MPSSE engine for I2C.
 * @ingroup Ftdi
 * @param ftdi FTDI handle.
 * @param baud_rate Desired I2C baud rate in Hz.
 * @retval true The engine is now configured for I2C.
 * @retval false Configuration failed; any previously active mode
 * (SPI/I2C/GPIO) is left unspecified and should be reconfigured before use.
 *
 * Reconfigures the shared MPSSE engine; any SPI or GPIO mode previously
 * active on @p ftdi is replaced.
 */
bool dev_ftdi_i2c_init(dev_ftdi_t *ftdi, uint32_t baud_rate);

/**
 * @brief Perform an I2C transfer operation.
 * @ingroup Ftdi
 * @param ftdi FTDI handle, previously configured with @ref dev_ftdi_i2c_init.
 * @param addr 7-bit slave address.
 * @param data Buffer used for transmitted and received data.
 * @param tx Number of bytes to transmit from `data`.
 * @param rx Number of bytes to receive into `data + tx`.
 * @param timeout_ms Timeout in milliseconds for the operation. Set to `0` to
 * use the backend's default transfer path.
 * @return Number of bytes transferred. Successful write-only transfers
 * return `tx`, successful read-only transfers return `rx`, and successful
 * combined write-then-read transfers return `tx + rx`. Returns `0` on
 * failure.
 *
 * This method supports write-only (`tx > 0, rx == 0`), read-only
 * (`tx == 0, rx > 0`), and write-then-read (`tx > 0, rx > 0`) transfers.
 *
 * For combined write-then-read transfers, a repeated START is issued
 * between the write and read portions rather than a STOP, matching
 * @ref hw_i2c_xfr.
 *
 * When `rx > 0`, received bytes are written to `((uint8_t *)data) + tx`, so
 * the caller must provide a buffer large enough to hold `tx + rx` bytes.
 */
size_t dev_ftdi_i2c_xfr(dev_ftdi_t *ftdi, uint8_t addr, void *data, size_t tx,
                        size_t rx, uint32_t timeout_ms);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// GPIO

/** @name GPIO
 * @{ */

/**
 * @brief Configure the device's MPSSE engine for plain GPIO.
 * @ingroup Ftdi
 * @param ftdi FTDI handle.
 * @param output_mask Pin mask for output-capable pins (bit 0=ADBUS0 ...
 * bit 7=ADBUS7, bit 8=ACBUS0 ... bit 15=ACBUS7, where present on the chip).
 * Bits set are configured as outputs; bits cleared are configured as
 * inputs.
 * @retval true The engine is now configured for GPIO.
 * @retval false Configuration failed; any previously active mode
 * (SPI/I2C/GPIO) is left unspecified and should be reconfigured before use.
 *
 * Reconfigures the shared MPSSE engine; any SPI or I2C mode previously
 * active on @p ftdi is replaced.
 */
bool dev_ftdi_gpio_init(dev_ftdi_t *ftdi, uint16_t output_mask);

/**
 * @brief Read all pin values.
 * @ingroup Ftdi
 * @param ftdi FTDI handle, previously configured with @ref dev_ftdi_gpio_init.
 * @param value Receives packed pin values (see @ref dev_ftdi_gpio_init for
 * bit layout).
 * @retval true Read succeeded.
 * @retval false Read failed.
 */
bool dev_ftdi_gpio_read(dev_ftdi_t *ftdi, uint16_t *value);

/**
 * @brief Write output pin values.
 * @ingroup Ftdi
 * @param ftdi FTDI handle, previously configured with @ref dev_ftdi_gpio_init.
 * @param value Packed output values (see @ref dev_ftdi_gpio_init for bit
 * layout). Only pins enabled by the initialization output mask are
 * affected.
 * @retval true Write succeeded.
 * @retval false Write failed.
 */
bool dev_ftdi_gpio_write(dev_ftdi_t *ftdi, uint16_t value);

/** @} */

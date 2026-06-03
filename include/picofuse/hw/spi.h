/**
 * @file spi.h
 * @brief SPI (Serial Peripheral Interface) interface
 * @defgroup SPI SPI
 * @ingroup Hardware
 *
 * Serial Peripheral Interface (SPI) interface for hardware platforms.
 * This module provides functions to initialize SPI peripherals and perform
 * bidirectional transfers with SPI devices in master mode.
 */
#pragma once
#include "gpio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque SPI handle.
 * @ingroup SPI
 * @headerfile spi.h hw/hw.h
 */
typedef struct hw_spi_t hw_spi_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize an SPI interface using the platform default adapter and
 * pins.
 * @ingroup SPI
 * @param cs_active_low True when chip-select is active low.
 * @param baud_rate Desired SPI clock rate in Hz.
 * @return SPI handle or NULL on failure.
 * @note On platforms where SPI buses are selected by device path instead of
 * index (for example Linux), this function may be unsupported and return
 * `NULL` by design. Use `hw_spi_init_device()` on those platforms.
 */
hw_spi_t *hw_spi_init_default(bool cs_active_low, uint32_t baud_rate);

/**
 * @brief Initialize an SPI interface with a specific adapter and pins.
 * @ingroup SPI
 * @param index SPI adapter index to use.
 * @param sck_pin GPIO handle for SCK.
 * @param tx_pin GPIO handle for MOSI.
 * @param rx_pin GPIO handle for MISO.
 * @param cs_pin Optional GPIO handle for CS. Pass NULL to leave CS unmanaged.
 * @param cs_active_low True when chip-select is active low.
 * @param baud_rate Desired SPI clock rate in Hz.
 * @return SPI handle or NULL on failure.
 */
hw_spi_t *hw_spi_init(uint8_t index, hw_gpio_t *sck_pin, hw_gpio_t *tx_pin,
                      hw_gpio_t *rx_pin, hw_gpio_t *cs_pin, bool cs_active_low,
                      uint32_t baud_rate);

/**
 * @brief Initialize an SPI interface from a platform-specific device path.
 * @ingroup SPI
 * @param device Device identifier such as `/dev/spidev0.0`.
 * @param cs_active_low True when chip-select is active low.
 * @param baud_rate Desired SPI clock rate in Hz.
 * @return SPI handle or NULL on failure.
 *
 * This entry point is intended for platforms where SPI buses are exposed as
 * named devices rather than a fixed, enumerable set of adapters.
 */
hw_spi_t *hw_spi_init_device(const char *device, bool cs_active_low,
                             uint32_t baud_rate);

/**
 * @brief Deinitialize an SPI interface.
 * @ingroup SPI
 * @param spi SPI handle.
 *
 * Safe to call on an invalid or already deinitialized handle; in that case it
 * is a no-op. After deinitialization, `hw_spi_valid()` returns false.
 */
void hw_spi_deinit(hw_spi_t *spi);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Get the total number of available SPI adapters.
 * @ingroup SPI
 * @return Number of SPI adapters available on the current platform.
 *
 * On platforms that open SPI buses by device path, this may return `0` even
 * when SPI is supported. In that case, use `hw_spi_init_device()` instead of
 * enumerating adapters by index.
 */
uint8_t hw_spi_count(void);

/**
 * @brief Check whether an SPI handle is valid.
 * @ingroup SPI
 * @param spi SPI handle.
 * @retval true The SPI handle is valid.
 * @retval false The SPI handle is invalid.
 */
bool hw_spi_valid(const hw_spi_t *spi);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Perform an SPI transfer operation.
 * @ingroup SPI
 * @param spi SPI handle.
 * @param data Buffer used for transmitted and received bytes.
 * @param tx Number of bytes to transmit from `data`.
 * @param rx Number of bytes to receive into `data + tx`.
 * @param timeout_ms Timeout in milliseconds for the operation. Set to `0` to
 * use the backend's default transfer path.
 * @return Number of bytes transferred, or `0` on failure.
 *
 * This method supports write-only (`tx > 0, rx == 0`), read-only
 * (`tx == 0, rx > 0`), and write-then-read (`tx > 0, rx > 0`) transfers.
 */
size_t hw_spi_xfr(hw_spi_t *spi, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms);

/**
 * @brief Read bytes from a register on an SPI device.
 * @ingroup SPI
 * @param spi SPI handle.
 * @param reg Register address to read from.
 * @param data Buffer to receive the bytes.
 * @param len Number of bytes to read.
 * @param timeout_ms Timeout in milliseconds for the operation. Set to `0` to
 * use the backend's default transfer path.
 * @return Number of bytes read, or `0` on failure.
 */
size_t hw_spi_read(hw_spi_t *spi, uint8_t reg, void *data, size_t len,
                   uint32_t timeout_ms);

/**
 * @brief Write bytes to a register on an SPI device.
 * @ingroup SPI
 * @param spi SPI handle.
 * @param reg Register address to write to.
 * @param data Buffer containing bytes to write.
 * @param len Number of bytes to write.
 * @param timeout_ms Timeout in milliseconds for the operation. Set to `0` to
 * use the backend's default transfer path.
 * @return Number of bytes written, or `0` on failure.
 */
size_t hw_spi_write(hw_spi_t *spi, uint8_t reg, const void *data, size_t len,
                    uint32_t timeout_ms);

/** @} */
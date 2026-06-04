/**
 * @file bme280.h
 * @brief Bosch BME280 environmental sensor interface.
 * @defgroup BME280 BME280
 * @ingroup Device
 *
 * This module provides a device-level wrapper for the Bosch BME280 sensor
 * over either I2C or SPI.
 */
#pragma once

#include <picofuse/hw.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque BME280 handle.
 * @ingroup BME280
 */
typedef struct dev_bme280_t dev_bme280_t;

/**
 * @brief BME280 oversampling settings.
 * @ingroup BME280
 */
typedef enum {
  DRIVER_BME280_OVERSAMPLING_NONE = 0, ///< No oversampling (output set to 0)
  DRIVER_BME280_OVERSAMPLING_1X = 1,   ///< Oversampling x1
  DRIVER_BME280_OVERSAMPLING_2X = 2,   ///< Oversampling x2
  DRIVER_BME280_OVERSAMPLING_4X = 3,   ///< Oversampling x4
  DRIVER_BME280_OVERSAMPLING_8X = 4,   ///< Oversampling x8
  DRIVER_BME280_OVERSAMPLING_16X = 5   ///< Oversampling x16
} driver_bme280_oversampling_t;

/**
 * @brief BME280 IIR filter coefficients.
 * @ingroup BME280
 */
typedef enum {
  DRIVER_BME280_FILTER_OFF = 0, ///< Filter off
  DRIVER_BME280_FILTER_2 = 1,   ///< Filter coefficient 2
  DRIVER_BME280_FILTER_4 = 2,   ///< Filter coefficient 4
  DRIVER_BME280_FILTER_8 = 3,   ///< Filter coefficient 8
  DRIVER_BME280_FILTER_16 = 4   ///< Filter coefficient 16
} driver_bme280_filter_t;

/**
 * @brief BME280 initialization/configuration options.
 * @ingroup BME280
 *
 * Pass `NULL` to the init functions to use the backend defaults.
 */
typedef struct {
  driver_bme280_oversampling_t temperature_oversampling;
  driver_bme280_oversampling_t pressure_oversampling;
  driver_bme280_oversampling_t humidity_oversampling;
  driver_bme280_filter_t filter;
  float temperature_offset_c; ///< Temperature offset in degrees Celsius.
} dev_bme280_config_t;

/**
 * @brief BME280 measurement values.
 * @ingroup BME280
 */
typedef struct {
  float temperature_c; ///< Temperature in degrees Celsius.
  float pressure_pa;   ///< Pressure in pascals.
  float humidity_pct;  ///< Relative humidity in percent.
} dev_bme280_data_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize a BME280 sensor over I2C.
 * @ingroup BME280
 * @param i2c I2C interface to use.
 * @param config Optional pointer to extended BME280 configuration. Pass
 * `NULL` to use default values.
 * @return BME280 handle or NULL on failure.
 */
dev_bme280_t *dev_bme280_init_i2c(hw_i2c_t *i2c,
                                  const dev_bme280_config_t *config);

/**
 * @brief Initialize a BME280 sensor over SPI.
 * @ingroup BME280
 * @param spi SPI interface to use.
 * @param cs_pin Optional chip-select GPIO handle. Pass NULL if the backend
 * manages chip-select externally.
 * @param config Optional pointer to extended BME280 configuration. Pass
 * `NULL` to use default values.
 * @return BME280 handle or NULL on failure.
 */
dev_bme280_t *dev_bme280_init_spi(hw_spi_t *spi, hw_gpio_t *cs_pin,
                                  const dev_bme280_config_t *config);

/**
 * @brief Deinitialize a BME280 sensor.
 * @ingroup BME280
 * @param bme280 BME280 handle.
 *
 * Safe to call on an invalid or already deinitialized handle; in that case it
 * is a no-op.
 */
void dev_bme280_deinit(dev_bme280_t *bme280);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Check whether a BME280 handle is valid.
 * @ingroup BME280
 * @param bme280 BME280 handle.
 * @retval true The handle is valid.
 * @retval false The handle is invalid.
 */
bool dev_bme280_valid(const dev_bme280_t *bme280);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Read all sensor data (temperature, pressure, humidity).
 * @ingroup BME280
 *
 * Triggers a forced measurement (single shot), waits for completion, and
 * reads all three sensor values in a single transaction. The sensor returns
 * to sleep mode after the measurement.
 *
 * @param bme280 Pointer to driver structure.
 * @param data Pointer to structure to receive measurement data.
 * @return true if successful, false otherwise.
 */
bool dev_bme280_read_data(dev_bme280_t *bme280, dev_bme280_data_t *data);

/**
 * @brief Calculate altitude from a pressure reading.
 * @ingroup BME280
 * @param data Measurement data returned by @ref dev_bme280_read_data.
 * @param sea_level_pressure Sea-level pressure in pascals. Pass 0 to use the
 * default of 101325 Pa.
 * @return Estimated altitude in meters.
 */
float dev_bme280_calculate_altitude(const dev_bme280_data_t *data,
                                    float sea_level_pressure);

/**
 * @brief Calculate sea-level pressure from a pressure reading and altitude.
 * @ingroup BME280
 * @param data Measurement data returned by @ref dev_bme280_read_data.
 * @param altitude Altitude in meters.
 * @return Estimated sea-level pressure in pascals.
 */
float dev_bme280_calculate_sea_level_pressure(const dev_bme280_data_t *data,
                                              float altitude);

/** @} */

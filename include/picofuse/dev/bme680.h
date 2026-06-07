/**
 * @file bme680.h
 * @brief Bosch BME680 environmental sensor interface.
 * @defgroup BME680 BME680
 * @ingroup Device
 */
#pragma once

#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque BME680 handle.
 * @ingroup BME680
 */
typedef struct dev_bme680_t dev_bme680_t;

typedef enum {
  DEV_BME680_OVERSAMPLING_SKIP = 0,
  DEV_BME680_OVERSAMPLING_1X = 1,
  DEV_BME680_OVERSAMPLING_2X = 2,
  DEV_BME680_OVERSAMPLING_4X = 3,
  DEV_BME680_OVERSAMPLING_8X = 4,
  DEV_BME680_OVERSAMPLING_16X = 5,
} dev_bme680_oversampling_t;

typedef enum {
  DEV_BME680_IIR_FILTER_OFF = 0,
  DEV_BME680_IIR_FILTER_1 = 1,
  DEV_BME680_IIR_FILTER_3 = 2,
  DEV_BME680_IIR_FILTER_7 = 3,
  DEV_BME680_IIR_FILTER_15 = 4,
  DEV_BME680_IIR_FILTER_31 = 5,
  DEV_BME680_IIR_FILTER_63 = 6,
  DEV_BME680_IIR_FILTER_127 = 7,
} dev_bme680_iir_filter_t;

typedef enum {
  DEV_BME680_HEATER_TEMP_200C = 200,
  DEV_BME680_HEATER_TEMP_250C = 250,
  DEV_BME680_HEATER_TEMP_300C = 300,
  DEV_BME680_HEATER_TEMP_320C = 320,
  DEV_BME680_HEATER_TEMP_350C = 350,
  DEV_BME680_HEATER_TEMP_400C = 400,
} dev_bme680_heater_temp_t;

typedef enum {
  DEV_BME680_HEATER_DUR_50MS = 50,
  DEV_BME680_HEATER_DUR_100MS = 100,
  DEV_BME680_HEATER_DUR_120MS = 120,
  DEV_BME680_HEATER_DUR_150MS = 150,
  DEV_BME680_HEATER_DUR_200MS = 200,
} dev_bme680_heater_duration_t;

typedef enum {
  DEV_BME680_AMBIENT_TEMP_0C = 0,
  DEV_BME680_AMBIENT_TEMP_10C = 10,
  DEV_BME680_AMBIENT_TEMP_20C = 20,
  DEV_BME680_AMBIENT_TEMP_25C = 25,
  DEV_BME680_AMBIENT_TEMP_30C = 30,
  DEV_BME680_AMBIENT_TEMP_35C = 35,
  DEV_BME680_AMBIENT_TEMP_40C = 40,
} dev_bme680_ambient_temp_t;

typedef enum {
  DEV_BME680_GAS_DISABLED = 0,
  DEV_BME680_GAS_ENABLED = 1,
} dev_bme680_gas_mode_t;

/**
 * @brief BME680 measurement values.
 * @ingroup BME680
 */
typedef struct {
  float temperature_c;       ///< Temperature in degrees Celsius.
  float pressure_pa;         ///< Pressure in pascals.
  float humidity_pct;        ///< Relative humidity in percent.
  float gas_resistance_ohms; ///< Gas resistance in ohms.
} dev_bme680_data_t;

/**
 * @brief BME680 runtime configuration.
 * @ingroup BME680
 *
 * Oversampling/filter fields use strongly typed enums.
 */
typedef struct {
  dev_bme680_oversampling_t os_temp;
  dev_bme680_oversampling_t os_press;
  dev_bme680_oversampling_t os_hum;
  dev_bme680_iir_filter_t iir_filter;
  dev_bme680_heater_temp_t heater_temp_c;
  dev_bme680_heater_duration_t heater_duration_ms;
  dev_bme680_ambient_temp_t ambient_temp_c;
  dev_bme680_gas_mode_t gas_mode;
} dev_bme680_config_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Fill a BME680 config struct with safe defaults.
 * @ingroup BME680
 * @param config Config structure to initialize.
 */
void dev_bme680_default_config(dev_bme680_config_t *config);

/**
 * @brief Initialize a BME680 sensor over I2C.
 * @ingroup BME680
 * @param i2c I2C interface to use.
 * @param config Optional configuration. Pass NULL for defaults.
 * @return BME680 handle or NULL on failure.
 */
dev_bme680_t *dev_bme680_init_i2c(hw_i2c_t *i2c,
                                  const dev_bme680_config_t *config);

/**
 * @brief Initialize a BME680 sensor over SPI.
 * @ingroup BME680
 * @param spi SPI interface to use.
 * @param cs_pin Optional chip-select GPIO handle. Pass NULL if the backend
 * manages chip-select externally.
 * @param config Optional configuration. Pass NULL for defaults.
 * @return BME680 handle or NULL on failure.
 */
dev_bme680_t *dev_bme680_init_spi(hw_spi_t *spi, hw_gpio_t *cs_pin,
                                  const dev_bme680_config_t *config);

/**
 * @brief Deinitialize a BME680 sensor.
 * @ingroup BME680
 * @param bme680 BME680 handle.
 */
void dev_bme680_deinit(dev_bme680_t *bme680);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Get the detected chip ID.
 * @ingroup BME680
 * @param bme680 BME680 handle.
 * @return Chip ID, or 0 when handle is invalid.
 */
uint8_t dev_bme680_chip_id(const dev_bme680_t *bme680);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Read sensor data from BME680.
 * @ingroup BME680
 * @param bme680 BME680 handle.
 * @param data Structure to receive sensor values.
 * @retval true Read succeeded.
 * @retval false Read failed.
 */
bool dev_bme680_read_data(dev_bme680_t *bme680, dev_bme680_data_t *data);

/**
 * @brief Register a BME680 over I2C as a polling HID metric source.
 * @ingroup BME680
 * @param hid HID instance that owns the registration.
 * @param i2c I2C interface used to initialize the BME680.
 * @param config Optional configuration. Pass NULL for defaults.
 * @param polling_interval_ms Polling interval in milliseconds.
 * @details Passing 0 uses the default interval of 30000 ms.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *dev_bme680_hid_register_i2c(hid_t *hid, hw_i2c_t *i2c,
                                          const dev_bme680_config_t *config,
                                          uint32_t polling_interval_ms);

/**
 * @brief Register a BME680 over SPI as a polling HID metric source.
 * @ingroup BME680
 * @param hid HID instance that owns the registration.
 * @param spi SPI interface used to initialize the BME680.
 * @param cs_pin Optional chip-select GPIO handle.
 * @param config Optional configuration. Pass NULL for defaults.
 * @param polling_interval_ms Polling interval in milliseconds.
 * @details Passing 0 uses the default interval of 30000 ms.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *dev_bme680_hid_register_spi(hid_t *hid, hw_spi_t *spi,
                                          hw_gpio_t *cs_pin,
                                          const dev_bme680_config_t *config,
                                          uint32_t polling_interval_ms);

/** @} */

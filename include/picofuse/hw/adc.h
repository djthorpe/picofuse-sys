/**
 * @file adc.h
 * @brief ADC (Analog-to-Digital Converter) interface
 * @defgroup ADC ADC
 * @ingroup Hardware
 *
 * Analog-to-Digital Converter (ADC) interface for hardware platforms.
 * This module provides functions to initialize and read from ADC peripherals.
 */
#pragma once
#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque ADC handle.
 * @ingroup ADC
 * @headerfile adc.h hw/hw.h
 */
typedef struct hw_adc_t hw_adc_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Get the total number of available ADC channels.
 * @ingroup ADC
 * @return Number of ADC channels available on the current platform.
 *
 * The returned count includes both GPIO-mappable ADC channels and any
 * internal ADC channels exposed by the backend. For example, on Pico
 * platforms this includes the internal temperature sensor channel.
 */
uint8_t hw_adc_count(void);

/**
 * @brief Initialize an ADC handle for a specific GPIO pin.
 * @ingroup ADC
 * @param gpio GPIO handle configured for ADC-capable pin access.
 * @return ADC handle or NULL on failure.
 */
hw_adc_t *hw_adc_init_pin(hw_gpio_t *gpio);

/**
 * @brief Initialize an ADC handle for the internal temperature sensor channel.
 * @ingroup ADC
 * @return ADC handle or NULL on failure.
 *
 * The internal temperature sensor channel is not associated with a GPIO pin.
 */
hw_adc_t *hw_adc_init_temperature(void);

/**
 * @brief Initialize an ADC handle for the VSYS voltage channel.
 * @ingroup ADC
 * @return ADC handle or NULL on failure.
 *
 * The VSYS voltage channel uses the ADC to measure the system supply voltage.
 * Some platforms may not support this feature, in which case NULL is returned.
 */
hw_adc_t *hw_adc_init_vsys(void);

/**
 * @brief Finalize and release an ADC handle.
 * @ingroup ADC
 * @param adc ADC handle.
 */
void hw_adc_deinit(hw_adc_t *adc);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Get the ADC channel number for a GPIO pin.
 * @ingroup ADC
 * @param gpio GPIO handle.
 * @return Channel number, or 0xFF if the GPIO is not ADC-capable.
 */
uint8_t hw_adc_gpio_channel(const hw_gpio_t *gpio);

/**
 * @brief Get the GPIO pin number for an ADC channel.
 * @ingroup ADC
 * @param channel ADC channel number.
 * @return GPIO pin number, or 0xFF if the channel has no GPIO mapping.
 *
 * Channels that are valid ADC inputs but are not backed by a GPIO pin, such
 * as internal temperature-sensor channels, return 0xFF here.
 */
uint8_t hw_adc_gpio_pin(uint8_t channel);

/**
 * @brief Check if an ADC handle is valid and usable.
 * @ingroup ADC
 * @param adc ADC handle.
 * @retval true The ADC handle is valid.
 * @retval false The ADC handle is invalid.
 */
bool hw_adc_valid(const hw_adc_t *adc);

/**
 * @brief Read the current value from an ADC channel as a 12-bit value.
 * @ingroup ADC
 * @param adc ADC handle.
 * @param num_samples Number of conversions to average. 0 or 1 takes a
 * single, immediate reading; higher values sample the ADC FIFO that many
 * times and return the mean, which reduces noise at the cost of latency.
 * Backends may clamp this to an implementation-defined maximum to bound
 * how long the call can block.
 * @return Raw value in the 0-4095 range.
 */
uint16_t hw_adc_read_12(hw_adc_t *adc, uint16_t num_samples);

/**
 * @brief Read the current value from an ADC channel as a 16-bit value.
 * @ingroup ADC
 * @param adc ADC handle.
 * @param num_samples Number of conversions to average. See hw_adc_read_12().
 * @return Raw value in the 0-65535 range.
 */
uint16_t hw_adc_read_16(hw_adc_t *adc, uint16_t num_samples);

/**
 * @brief Read the current value from an ADC channel as a voltage.
 * @ingroup ADC
 * @param adc ADC handle.
 * @param num_samples Number of conversions to average. See hw_adc_read_12().
 * @return Voltage value in volts.
 */
float hw_adc_read_voltage(hw_adc_t *adc, uint16_t num_samples);

/**
 * @brief Read the current value from an ADC channel as a temperature.
 * @ingroup ADC
 * @param adc ADC handle configured for temperature sensing.
 * @param num_samples Number of conversions to average. See hw_adc_read_12().
 * @return Temperature value in degrees Celsius.
 */
float hw_adc_read_temperature(hw_adc_t *adc, uint16_t num_samples);

/** @} */
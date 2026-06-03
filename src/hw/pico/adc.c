#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(PICO_RP2040) || defined(PICO_RP2050A) || defined(PICO_RP2350A)
// Package with ADC-capable GPIOs starting at 26 (RP2040 / RP2050A / RP2350A)
#define ADC_CHANNEL_OFFSET 26
#elif defined(PICO_RP2350B)
// RP2350B package exposes additional GPIOs; ADC GPIOs start at 40
#define ADC_CHANNEL_OFFSET 40
#elif defined(PIMORONI_PICO_LIPO2_RP2350)
#define ADC_CHANNEL_OFFSET 40
#elif defined(PICO_RP2350)
// RP2350 generic fallback; many boards map ADC pins like package A.
#define ADC_CHANNEL_OFFSET 26
#endif

#ifndef ADC_CHANNEL_OFFSET
#define ADC_CHANNEL_OFFSET 26
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_adc_t {
  uint8_t channel;
  hw_gpio_t *gpio;
  bool temperature;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static hw_adc_t _hw_adc_channels[NUM_ADC_CHANNELS] = {0};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_adc_count(void) { return NUM_ADC_CHANNELS; }

hw_adc_t *hw_adc_init_pin(hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }

  uint8_t channel = hw_adc_gpio_channel(gpio);
  if (channel == UINT8_MAX) {
    return NULL;
  }

  // Last channel is reserved for the internal temperature sensor.
  hw_gpio_set_mode(gpio, HW_GPIO_ADC);

  hw_adc_t *adc = &_hw_adc_channels[channel];
  adc->channel = channel;
  adc->gpio = gpio;
  adc->temperature = false;
  adc->init = true;
  return adc;
}

hw_adc_t *hw_adc_init_temperature(void) {
  adc_set_temp_sensor_enabled(true);

  hw_adc_t *adc = &_hw_adc_channels[ADC_TEMPERATURE_CHANNEL_NUM];
  adc->channel = ADC_TEMPERATURE_CHANNEL_NUM;
  adc->gpio = NULL;
  adc->temperature = true;
  adc->init = true;
  return adc;
}

void hw_adc_deinit(hw_adc_t *adc) {
  if (!hw_adc_valid(adc)) {
    return;
  }

  if (adc->channel == ADC_TEMPERATURE_CHANNEL_NUM) {
    adc_set_temp_sensor_enabled(false);
  }

  adc->channel = 0;
  adc->gpio = NULL;
  adc->temperature = false;
  adc->init = false;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

uint8_t hw_adc_gpio_channel(const hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return UINT8_MAX;
  }

  uint8_t pin = hw_gpio_get_pin_num(gpio);

  // Check if the GPIO pin is within the ADC channel range.
  if (pin >= ADC_CHANNEL_OFFSET &&
      pin < (uint8_t)(ADC_CHANNEL_OFFSET + NUM_ADC_CHANNELS - 1)) {
    return (uint8_t)(pin - ADC_CHANNEL_OFFSET);
  }

  return UINT8_MAX;
}

uint8_t hw_adc_gpio_pin(uint8_t channel) {
  if (channel >= (uint8_t)(NUM_ADC_CHANNELS - 1)) {
    return UINT8_MAX;
  }

  return (uint8_t)(ADC_CHANNEL_OFFSET + channel);
}

bool hw_adc_valid(const hw_adc_t *adc) {
  return adc != NULL && adc->init && adc->channel < hw_adc_count();
}

uint16_t hw_adc_read_12(hw_adc_t *adc) {
  sys_assert(adc);
  sys_assert(hw_adc_valid(adc));

  if (adc->temperature) {
    adc_set_temp_sensor_enabled(true);
  }

  adc_run(false);
  adc_select_input(adc->channel);
  return adc_read();
}

uint16_t hw_adc_read_16(hw_adc_t *adc) {
  uint16_t raw12 = hw_adc_read_12(adc);
  return (uint16_t)((raw12 << 4) | (raw12 >> 8));
}

#define ADC_VREF (3.3f)

float hw_adc_read_voltage(hw_adc_t *adc) {
  sys_assert(adc);
  uint16_t raw12 = hw_adc_read_12(adc);
  const float conversion_factor = ADC_VREF / (float)(1 << 12);

#ifdef PICO_VSYS_PIN
  if (adc->gpio != NULL && hw_gpio_valid(adc->gpio) &&
      hw_gpio_get_pin_num(adc->gpio) == PICO_VSYS_PIN) {
    return (float)raw12 * 3.0f * conversion_factor;
  }
#endif

  return (float)raw12 * conversion_factor;
}

float hw_adc_read_temperature(hw_adc_t *adc) {
  sys_assert(adc);
  sys_assert(hw_adc_valid(adc));
  sys_assert(adc->channel == ADC_TEMPERATURE_CHANNEL_NUM);

  float voltage = hw_adc_read_voltage(adc);
  return 27.0f - ((voltage - 0.706f) / 0.001721f);
}
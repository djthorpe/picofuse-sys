#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef PICO_CYW43_SUPPORTED
#include "cyw43.h"
#include <pico/cyw43_arch.h>
#endif

#if defined(PICO_RP2040) || defined(PICO_RP2350A)
// Package with ADC-capable GPIOs starting at 26 (RP2040 / RP2350A)
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

#ifndef ADC_VREF
#define ADC_VREF (3.3f)
#endif

// Upper bound on hw_adc_read_12() sample averaging. At the default ~500kSPS
// free-running rate this caps a single call to roughly 2ms of blocking time.
#define ADC_MAX_SAMPLES 1024u

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_adc_t {
  uint8_t channel;
  hw_gpio_t *gpio;
  bool temperature;
  bool vsys;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static hw_adc_t _hw_adc_channels[NUM_ADC_CHANNELS] = {0};

///////////////////////////////////////////////////////////////////////////////
// FPRIVATE METHODS

/**
 * @brief Get the GPIO pin number for the VSYS voltage channel
 * @return GPIO pin number, or 0xFF if the channel has no GPIO mapping.
 */
static inline uint8_t _hw_adc_vsys_pin(void) {
#if defined(PICO_VSYS_PIN)
  return PICO_VSYS_PIN;
#elif defined(PICOLIPO_BAT_SENSE_PIN)
  return PICOLIPO_BAT_SENSE_PIN;
#elif defined(PIMORONI_PICO_LIPO2_RP2350)
  return 43; // PIMORONI_PICO_LIPO2_RP2350 uses GPIO 43 for VSYS
#else
  return UINT8_MAX; // Invalid GPIO pin
#endif
}

/**
 * @brief Get the CYW43 VBUS GPIO pin used to wake the wifi module before
 * measuring VSYS.
 * @return GPIO pin number, or 0xFF if VSYS measurement does not require
 * waking the wifi module on this platform.
 */
static inline uint8_t _hw_adc_wifi_vbus_pin(void) {
#if defined(PICO_CYW43_SUPPORTED) && defined(CYW43_USES_VSYS_PIN) &&           \
    defined(CYW43_WL_GPIO_VBUS_PIN)
  return CYW43_WL_GPIO_VBUS_PIN;
#else
  return UINT8_MAX;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_adc_count(void) { return NUM_ADC_CHANNELS; }

hw_adc_t *hw_adc_init_pin(hw_gpio_t *gpio) {
  sys_debugf("hw", "adc_init_pin: gpio=%p", gpio);
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
  adc->vsys = false;
  return adc;
}

hw_adc_t *hw_adc_init_temperature(void) {
  sys_debugf("hw", "adc_init_temperature");
  adc_set_temp_sensor_enabled(true);

  hw_adc_t *adc = &_hw_adc_channels[ADC_TEMPERATURE_CHANNEL_NUM];
  adc->channel = ADC_TEMPERATURE_CHANNEL_NUM;
  adc->gpio = NULL;
  adc->temperature = true;
  adc->vsys = false;
  return adc;
}

hw_adc_t *hw_adc_init_vsys(void) {
  uint8_t vsys_pin = _hw_adc_vsys_pin();
  if (vsys_pin == UINT8_MAX) {
    sys_debugf("hw",
        "adc_init_vsys: VSYS channel not supported on this platform");
    return NULL;
  }

  // Initialize the ADC for VSYS
  hw_adc_t *adc = hw_adc_init_pin(hw_gpio_init(0u, vsys_pin, HW_GPIO_ADC));
  if (!hw_adc_valid(adc)) {
    sys_debugf("hw", "adc_init_vsys: failed to initialize GPIO %d", vsys_pin);
    return NULL;
  }
  adc->vsys = true;
  sys_debugf("hw", "adc_init_vsys: gpio=%d wifi_vbus_pin=%d", vsys_pin,
             _hw_adc_wifi_vbus_pin());

  return adc;
}

void hw_adc_deinit(hw_adc_t *adc) {
  sys_debugf("hw", "adc_deinit: adc=%p", adc);
  if (!hw_adc_valid(adc)) {
    return;
  }

  if (adc->channel == ADC_TEMPERATURE_CHANNEL_NUM) {
    adc_set_temp_sensor_enabled(false);
  }

  adc->channel = UINT8_MAX;
  adc->gpio = NULL;
  adc->temperature = false;
  adc->vsys = false;
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
  // The internal temperature channel has no backing GPIO by design (see
  // hw_adc_init_temperature()), so it is valid without one; every other
  // channel must have one.
  return adc != NULL && adc->channel < hw_adc_count() &&
         (adc->gpio != NULL || adc->temperature);
}

/**
 * @brief Sample the ADC and return the averaged raw 12-bit reading at full
 * precision (not rounded to an integer count). num_samples > 1 averaging
 * only reduces noise in the mean if that fractional precision survives to
 * the caller; hw_adc_read_12() rounds it away for its integer-count
 * contract, so callers that need the benefit of averaging beyond 1 LSB
 * (see hw_adc_read_voltage()) use this directly instead of routing through
 * hw_adc_read_12().
 */
static float _hw_adc_read_raw(hw_adc_t *adc, uint16_t num_samples) {
  sys_assert(adc);
  sys_assert(hw_adc_valid(adc));

  if (adc->temperature) {
    adc_set_temp_sensor_enabled(true);
  }

#ifdef PICO_CYW43_SUPPORTED
  uint8_t wifi_vbus_pin = adc->vsys ? _hw_adc_wifi_vbus_pin() : UINT8_MAX;
  if (wifi_vbus_pin != UINT8_MAX) {
    cyw43_thread_enter();
    // Make sure cyw43 is awake so VSYS can be measured.
    cyw43_arch_gpio_get(wifi_vbus_pin);
  }
#endif

  adc_select_input(adc->channel);

  // adc_fifo_setup()'s enable bit is sticky across calls (never disabled
  // between reads), so if the FIFO still held any entries from a prior
  // conversion on a *different* channel — e.g. adc_run(false) not fully
  // stopping free-running before the previous adc_fifo_drain() checked
  // is_empty() — they'd otherwise still be sitting here. Discarding two
  // "settling" samples below then assumes those are fresh conversions on
  // the newly-selected channel; if they're actually stale leftovers from
  // the previous channel instead, the settling discard doesn't do its job
  // and the average gets contaminated by a different channel's readings.
  // Drain unconditionally first so every read starts from a clean FIFO.
  adc_fifo_drain();

  float result;
  if (num_samples <= 1) {
    adc_run(false);
    result = (float)adc_read();
  } else {
    if (num_samples > ADC_MAX_SAMPLES) {
      num_samples = ADC_MAX_SAMPLES;
    }

    // Free-run the ADC and average samples pulled from the FIFO.
    adc_fifo_setup(true, false, 0, false, false);
    adc_run(true);

    // Discard the first couple of conversions to let the ADC settle.
    (void)adc_fifo_get_blocking();
    (void)adc_fifo_get_blocking();

    uint32_t sum = 0;
    for (uint16_t i = 0; i < num_samples; i++) {
      sum += adc_fifo_get_blocking();
    }

    adc_run(false);
    adc_fifo_drain();

    result = (float)sum / (float)num_samples;
  }

#ifdef PICO_CYW43_SUPPORTED
  if (wifi_vbus_pin != UINT8_MAX) {
    cyw43_thread_exit();
  }
#endif

  return result;
}

uint16_t hw_adc_read_12(hw_adc_t *adc, uint16_t num_samples) {
  return (uint16_t)(_hw_adc_read_raw(adc, num_samples) + 0.5f);
}

uint16_t hw_adc_read_16(hw_adc_t *adc, uint16_t num_samples) {
  uint16_t raw12 = hw_adc_read_12(adc, num_samples);
  return (uint16_t)((raw12 << 4) | (raw12 >> 8));
}

float hw_adc_read_voltage(hw_adc_t *adc, uint16_t num_samples) {
  float raw12 = _hw_adc_read_raw(adc, num_samples);
  const float conversion_factor = ADC_VREF / (float)(1 << 12);

  if (adc->vsys) {
    return raw12 * 3.0f * conversion_factor;
  }

  return raw12 * conversion_factor;
}

float hw_adc_read_temperature(hw_adc_t *adc, uint16_t num_samples) {
  sys_assert(adc);
  sys_assert(hw_adc_valid(adc));
  sys_assert(adc->channel == ADC_TEMPERATURE_CHANNEL_NUM);

  float voltage = hw_adc_read_voltage(adc, num_samples);
  return 27.0f - ((voltage - 0.706f) / 0.001721f);
}
#pragma once
#include <hardware/gpio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef PICO_CYW43_SUPPORTED
#include "cyw43.h"
#include <pico/cyw43_arch.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// Internal VSYS/VBUS power-rail helpers, shared with adc.c.

/**
 * @brief Get the GPIO pin number for the VSYS voltage channel.
 * @return GPIO pin number, or 0xFF if the channel has no GPIO mapping.
 */
static inline uint8_t _hw_power_vsys_pin(void) {
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
static inline uint8_t _hw_power_wifi_vbus_pin(void) {
#if defined(PICO_CYW43_SUPPORTED) && defined(CYW43_USES_VSYS_PIN) &&           \
    defined(CYW43_WL_GPIO_VBUS_PIN)
  return CYW43_WL_GPIO_VBUS_PIN;
#else
  return UINT8_MAX;
#endif
}

/**
 * @brief Prepare the wifi module (if any) for a VSYS ADC read.
 *
 * On boards where the CYW43 wifi module owns the VSYS GPIO, wakes the module
 * and acquires the cyw43 thread lock so VSYS can be measured safely. Must be
 * paired with a matching _hw_power_vsys_post_read() call. No-op on boards
 * that don't need it.
 */
static inline void _hw_power_vsys_pre_read(void) {
#ifdef PICO_CYW43_SUPPORTED
  uint8_t wifi_vbus_pin = _hw_power_wifi_vbus_pin();
  if (wifi_vbus_pin != UINT8_MAX) {
    cyw43_thread_enter();
    // Make sure cyw43 is awake so VSYS can be measured.
    cyw43_arch_gpio_get(wifi_vbus_pin);
  }
#endif
}

/**
 * @brief Release resources acquired by _hw_power_vsys_pre_read().
 */
static inline void _hw_power_vsys_post_read(void) {
#ifdef PICO_CYW43_SUPPORTED
  if (_hw_power_wifi_vbus_pin() != UINT8_MAX) {
    cyw43_thread_exit();
  }
#endif
}

/**
 * @brief Get the scale factor to convert a raw VSYS ADC voltage reading into
 * the actual VSYS voltage (accounting for the on-board voltage divider).
 */
static inline float _hw_power_vsys_scale(void) { return 3.0f; }

/**
 * @brief Get the GPIO pin used to detect whether VBUS (USB power) is present.
 * @return GPIO pin number, or 0xFF if this platform has no VBUS detect pin.
 */
static inline uint8_t _hw_power_vbus_pin(void) {
#ifdef CYW43_WL_GPIO_VBUS_PIN
  return CYW43_WL_GPIO_VBUS_PIN;
#elif defined(PICO_VBUS_PIN)
  return PICO_VBUS_PIN;
#elif defined(PICOLIPO_VBUS_DETECT_PIN)
  return PICOLIPO_VBUS_DETECT_PIN;
#else
  return 0xFF; // Invalid GPIO pin
#endif
}

/**
 * @brief Read the current state of the VBUS (USB power present) detect pin,
 * regardless of whether it's wired to a real RP2040 GPIO or owned by the
 * CYW43 wifi module.
 * @return true if VBUS is present, false if absent or this platform has no
 * VBUS detect pin.
 */
static inline bool _hw_power_vbus_present(void) {
#if defined(PICO_CYW43_SUPPORTED) && defined(CYW43_WL_GPIO_VBUS_PIN)
  cyw43_thread_enter();
  bool present = cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
  cyw43_thread_exit();
  return present;
#else
  uint8_t pin = _hw_power_vbus_pin();
  if (pin == UINT8_MAX) {
    return false;
  }
  return gpio_get(pin);
#endif
}

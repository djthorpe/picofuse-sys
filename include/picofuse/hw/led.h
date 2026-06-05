/**
 * @file led.h
 * @brief Board LED helpers.
 * @defgroup LED LED
 * @ingroup Hardware
 */
#pragma once
#include <stdint.h>

#if defined(SYSTEM_NAME_PICO) && defined(__has_include)
#if __has_include(<pico.h>)
#include <pico.h>
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

/**
 * @brief Sentinel returned when no default board LED GPIO is available.
 * @ingroup LED
 */
#define HW_LED_GPIO_NONE 0xFFu

///////////////////////////////////////////////////////////////////////////////
// QUERY

/**
 * @brief Return the default board LED GPIO pin.
 * @ingroup LED
 * @return The default board LED GPIO pin, or @ref HW_LED_GPIO_NONE when no
 * direct GPIO LED is available.
 */
static inline uint8_t hw_led_gpio_default(void) {
#ifdef PICO_DEFAULT_LED_PIN
  return (uint8_t)PICO_DEFAULT_LED_PIN;
#else
  return HW_LED_GPIO_NONE;
#endif
}

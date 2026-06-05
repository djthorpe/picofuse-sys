/**
 * @file led.h
 * @brief Board LED helpers.
 * @defgroup LED LED
 * @ingroup Hardware
 */
#pragma once
#include <stdint.h>

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
uint8_t hw_led_gpio_default(void);

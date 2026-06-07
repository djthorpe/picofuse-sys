/**
 * @file hw.h
 * @brief Aggregates the hardware interface headers and platform lifecycle
 * hooks.
 * @defgroup Hardware Hardware Interfaces
 * @ingroup Picofuse
 * @details
 * The Hardware module exposes platform-facing interfaces for low-level
 * peripherals and board resources, such as GPIO, ADC, PWM, I2C, SPI, UART,
 * storage, USB, networking, and board LEDs.
 *
 * These APIs provide the portability layer between application logic and the
 * underlying target implementation (for example, desktop simulation backends
 * or microcontroller-specific drivers). The goal is to offer a consistent
 * programming model across supported environments.
 *
 * Runtime lifecycle is coordinated through:
 * - `hw_init()` to initialize hardware backends and board-level resources.
 * - `hw_poll()` for cooperative processing where periodic backend work is
 *   required.
 * - `hw_exit()` to release resources during shutdown.
 *
 * Typical flow:
 * 1. Call `sys_init()` and then `hw_init()` during startup.
 * 2. Acquire/configure peripheral handles through the specific `hw/*` APIs.
 * 3. Periodically call `hw_poll()` in the main loop or runloop callback.
 * 4. Deinitialize explicit handles, then call `hw_exit()` on shutdown.
 *
 * This header is a convenience aggregator for all public hardware interfaces
 * plus the platform lifecycle entry points.
 */
#pragma once
#include "hw/adc.h"
#include "hw/block.h"
#include "hw/flash.h"
#include "hw/gpio.h"
#include "hw/i2c.h"
#include "hw/led.h"
#include "hw/pwm.h"
#include "hw/spi.h"
#include "hw/uart.h"
#include "hw/usb.h"
#include "hw/wifi.h"

/**
 * @brief Initializes the hardware system on startup.
 */
void hw_init(void);

/**
 * @brief Cleans up the hardware system on shutdown.
 */
void hw_exit(void);

/**
 * @brief Occasional polling function for the hardware system.
 */
void hw_poll(void);

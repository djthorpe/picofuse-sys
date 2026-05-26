/**
 * @file hw.h
 * @brief Aggregates the hardware interface headers and platform lifecycle
 * hooks.
 * @defgroup Hardware Hardware Interfaces
 * @ingroup Picofuse
 */
#pragma once
#include "hw/gpio.h"
#include "hw/uart.h"
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

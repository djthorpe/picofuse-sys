/**
 * @file dev.h
 * @brief Aggregates device-specific component interfaces.
 * @defgroup Device Device Implementations
 * @ingroup Picofuse
 * @details
 * The Device module groups board- and peripheral-specific integrations that
 * sit above the low-level hardware (`hw`) and system (`sys`) layers.
 *
 * These components provide concrete support for real peripherals such as
 * sensors, touch controllers, displays, and I/O expanders. Each device
 * integration typically handles probing, configuration, periodic reads or
 * updates, and teardown semantics appropriate for that chip or module.
 *
 * Device APIs are designed to be composed with higher-level subsystems.
 * Common patterns include:
 * - Registering sensor/touch adapters as HID event producers.
 * - Initializing display or bus helpers for UI and board bring-up.
 * - Reusing shared bus handles (for example I2C/SPI) across multiple devices.
 *
 * Typical flow:
 * 1. Initialize base runtime/hardware (`sys_init`, `hw_init`, bus setup).
 * 2. Register or initialize one or more device integrations.
 * 3. Poll or run event loop logic so devices can produce data/events.
 * 4. Deinitialize device handles and shared buses during shutdown.
 *
 * This header is a convenience aggregator for public device interfaces.
 */
#pragma once
#include "dev/bme280.h"
#include "dev/bme680.h"
#include "dev/ft6236.h"
#include "dev/st7701.h"
#include "dev/tca9555.h"
#include "dev/uc8151.h"

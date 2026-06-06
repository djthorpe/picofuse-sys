/**
 * @file dev.h
 * @brief Aggregates device-specific component interfaces.
 * @defgroup Device Device Implementations
 * @ingroup Picofuse
 *
 * Device integrations belong here. This module is intended for component-
 * level drivers and adapters that build on top of the lower-level hardware
 * and system abstractions.
 */
#pragma once
#include "dev/bme280.h"
#include "dev/bme680.h"
#include "dev/ft6236.h"
#include "dev/st7701.h"
#include "dev/tca9555.h"
#include "dev/uc8151.h"

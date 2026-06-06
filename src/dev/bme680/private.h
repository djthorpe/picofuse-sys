#pragma once

#include <picofuse/dev.h>
#include <stdbool.h>
#include <stdint.h>

#define DEV_BME680_METRIC_CHANGED_TEMPERATURE (1u << 0)
#define DEV_BME680_METRIC_CHANGED_PRESSURE (1u << 1)
#define DEV_BME680_METRIC_CHANGED_HUMIDITY (1u << 2)
#define DEV_BME680_METRIC_CHANGED_GAS_RESISTANCE (1u << 3)

uint8_t _dev_bme680_cache_and_collect_changes(dev_bme680_t *bme680,
                                              const dev_bme680_data_t *data);

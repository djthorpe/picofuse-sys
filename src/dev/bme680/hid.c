#include "private.h"
#include <picofuse/dev.h>
#include <picofuse/hid.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static const char *_dev_bme680_metric_temperature = "temperature";
static const char *_dev_bme680_metric_pressure = "pressure";
static const char *_dev_bme680_metric_humidity = "humidity";
static const char *_dev_bme680_metric_gas_resistance = "gas_resistance";
static const char *_dev_bme680_unit_celsius = "C";
static const char *_dev_bme680_unit_pascal = "Pa";
static const char *_dev_bme680_unit_percent = "%";
static const char *_dev_bme680_unit_ohm = "ohm";

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

static bool _dev_bme680_hid_init(void *userdata);
static bool _dev_bme680_hid_read(hid_device_t *device, void *userdata);
static bool _dev_bme680_hid_deinit(void *userdata);
static hid_device_t *
_dev_bme680_hid_register_device(hid_t *hid, dev_bme680_t *device,
                                uint32_t polling_interval_ms);

/**
 * @brief Shared lifecycle/read callbacks used for BME680 HID registration.
 */
static const hid_device_callbacks_t _dev_bme680_hid_callbacks = {
    .init = _dev_bme680_hid_init,
    .read = _dev_bme680_hid_read,
    .deinit = _dev_bme680_hid_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

/**
 * @brief Validate BME680 userdata during HID registration.
 */
static bool _dev_bme680_hid_init(void *userdata) {
  dev_bme680_t *bme680 = (dev_bme680_t *)userdata;
  return bme680 != NULL;
}

/**
 * @brief Poll and publish BME680 metrics as HID events.
 */
static bool _dev_bme680_hid_read(hid_device_t *device, void *userdata) {
  dev_bme680_t *bme680 = (dev_bme680_t *)userdata;
  dev_bme680_data_t data;
  uint8_t changed;

  if (bme680 == NULL || device == NULL) {
    return false;
  }

  if (!dev_bme680_read_data(bme680, &data)) {
    return false;
  }

  changed = _dev_bme680_cache_and_collect_changes(bme680, &data);
  if (changed == 0u) {
    return false;
  }

  if ((changed & DEV_BME680_METRIC_CHANGED_TEMPERATURE) != 0u &&
      !hid_event_queue_metric_float(device, _dev_bme680_metric_temperature,
                                    _dev_bme680_unit_celsius,
                                    data.temperature_c)) {
    return false;
  }

  if ((changed & DEV_BME680_METRIC_CHANGED_PRESSURE) != 0u &&
      !hid_event_queue_metric_float(device, _dev_bme680_metric_pressure,
                                    _dev_bme680_unit_pascal,
                                    data.pressure_pa)) {
    return false;
  }

  if ((changed & DEV_BME680_METRIC_CHANGED_HUMIDITY) != 0u &&
      !hid_event_queue_metric_float(device, _dev_bme680_metric_humidity,
                                    _dev_bme680_unit_percent,
                                    data.humidity_pct)) {
    return false;
  }

  if ((changed & DEV_BME680_METRIC_CHANGED_GAS_RESISTANCE) != 0u &&
      !hid_event_queue_metric_float(device, _dev_bme680_metric_gas_resistance,
                                    _dev_bme680_unit_ohm,
                                    data.gas_resistance_ohms)) {
    return false;
  }

  return true;
}

/**
 * @brief Deinitialize BME680 device state on HID deregistration.
 */
static bool _dev_bme680_hid_deinit(void *userdata) {
  dev_bme680_t *bme680 = (dev_bme680_t *)userdata;

  if (bme680 == NULL) {
    return false;
  }

  dev_bme680_deinit(bme680);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

static hid_device_t *
_dev_bme680_hid_register_device(hid_t *hid, dev_bme680_t *device,
                                uint32_t polling_interval_ms) {
  hid_device_t *hid_device;

  if (hid == NULL || device == NULL) {
    return NULL;
  }

  hid_device = hid_register(hid, "bme680", (uint32_t)dev_bme680_chip_id(device),
                            hid_type_other, polling_interval_ms, device,
                            _dev_bme680_hid_callbacks);
  if (hid_device == NULL) {
    dev_bme680_deinit(device);
    return NULL;
  }

  return hid_device;
}

hid_device_t *dev_bme680_hid_register_i2c(hid_t *hid, hw_i2c_t *i2c,
                                          const dev_bme680_config_t *config) {
  return dev_bme680_hid_register_i2c_with_interval(hid, i2c, config, 0u);
}

hid_device_t *
dev_bme680_hid_register_i2c_with_interval(hid_t *hid, hw_i2c_t *i2c,
                                          const dev_bme680_config_t *config,
                                          uint32_t polling_interval_ms) {
  dev_bme680_t *device;

  if (hid == NULL || i2c == NULL) {
    return NULL;
  }

  device = dev_bme680_init_i2c(i2c, config);
  if (device == NULL) {
    return NULL;
  }

  return _dev_bme680_hid_register_device(hid, device, polling_interval_ms);
}

hid_device_t *dev_bme680_hid_register_spi(hid_t *hid, hw_spi_t *spi,
                                          hw_gpio_t *cs_pin,
                                          const dev_bme680_config_t *config) {
  return dev_bme680_hid_register_spi_with_interval(hid, spi, cs_pin, config,
                                                   0u);
}

hid_device_t *dev_bme680_hid_register_spi_with_interval(
    hid_t *hid, hw_spi_t *spi, hw_gpio_t *cs_pin,
    const dev_bme680_config_t *config, uint32_t polling_interval_ms) {
  dev_bme680_t *device;

  if (hid == NULL || spi == NULL) {
    return NULL;
  }

  device = dev_bme680_init_spi(spi, cs_pin, config);
  if (device == NULL) {
    return NULL;
  }

  return _dev_bme680_hid_register_device(hid, device, polling_interval_ms);
}

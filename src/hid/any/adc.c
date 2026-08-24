#include "private.h"
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Per-device state shared by both hid_register_adc() and
 * hid_register_temperature(): the ADC handle plus last-reported values for
 * change detection. gpio is NULL for the internal temperature channel,
 * which has no backing GPIO pin.
 */
typedef struct {
  hw_adc_t *adc;
  hw_gpio_t *gpio;
  float cached_voltage;
  bool cached_voltage_valid;
  uint16_t cached_raw_12;
  bool cached_raw_12_valid;
  uint16_t cached_raw_16;
  bool cached_raw_16_valid;
  float cached_temperature_c;
  bool cached_temperature_valid;
} _hid_adc_state_t;

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static const char *_hid_adc_name = "adc";
static const char *_hid_temperature_name = "temperature";
static const char *_hid_metric_voltage = "voltage";
static const char *_hid_metric_raw_12 = "raw_12";
static const char *_hid_metric_raw_16 = "raw_16";
static const char *_hid_metric_temperature = "temperature";
static const char *_hid_unit_volt = "V";
static const char *_hid_unit_raw = "";
static const char *_hid_unit_celsius = "C";
static const uint32_t _hid_adc_default_poll_interval_ms = 1000u;

static bool _hid_adc_device_init(hid_device_t *device, void *userdata);
static bool _hid_adc_device_read(hid_device_t *device, void *userdata);
static bool _hid_temperature_device_read(hid_device_t *device,
                                         void *userdata);
static bool _hid_adc_device_deinit(hid_device_t *device, void *userdata);

static const hid_device_callbacks_t _hid_adc_callbacks = {
    .init = _hid_adc_device_init,
    .read = _hid_adc_device_read,
    .deinit = _hid_adc_device_deinit,
};

static const hid_device_callbacks_t _hid_temperature_callbacks = {
    .init = _hid_adc_device_init,
    .read = _hid_temperature_device_read,
    .deinit = _hid_adc_device_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

/**
 * @brief Validate ADC state during HID registration.
 */
static bool _hid_adc_device_init(hid_device_t *device, void *userdata) {
  _hid_adc_state_t *state = (_hid_adc_state_t *)userdata;
  (void)device;
  return state != NULL && hw_adc_valid(state->adc);
}

/**
 * @brief Poll a GPIO-pin ADC channel and publish changed metrics.
 */
static bool _hid_adc_device_read(hid_device_t *device, void *userdata) {
  _hid_adc_state_t *state = (_hid_adc_state_t *)userdata;
  bool ok = true;

  if (state == NULL || device == NULL) {
    return false;
  }

  float voltage = hw_adc_read_voltage(state->adc);
  uint16_t raw_12 = hw_adc_read_12(state->adc);
  uint16_t raw_16 = hw_adc_read_16(state->adc);

  if (!state->cached_voltage_valid || state->cached_voltage != voltage) {
    state->cached_voltage = voltage;
    state->cached_voltage_valid = true;
    ok = hid_event_queue_metric_float(device, _hid_metric_voltage,
                                      _hid_unit_volt, voltage) &&
        ok;
  }

  if (!state->cached_raw_12_valid || state->cached_raw_12 != raw_12) {
    state->cached_raw_12 = raw_12;
    state->cached_raw_12_valid = true;
    ok = hid_event_queue_metric_float(device, _hid_metric_raw_12,
                                      _hid_unit_raw, (float)raw_12) &&
        ok;
  }

  if (!state->cached_raw_16_valid || state->cached_raw_16 != raw_16) {
    state->cached_raw_16 = raw_16;
    state->cached_raw_16_valid = true;
    ok = hid_event_queue_metric_float(device, _hid_metric_raw_16,
                                      _hid_unit_raw, (float)raw_16) &&
        ok;
  }

  return ok;
}

/**
 * @brief Poll the internal temperature-sensor channel and publish a changed
 * temperature metric.
 */
static bool _hid_temperature_device_read(hid_device_t *device,
                                         void *userdata) {
  _hid_adc_state_t *state = (_hid_adc_state_t *)userdata;

  if (state == NULL || device == NULL) {
    return false;
  }

  float temperature_c = hw_adc_read_temperature(state->adc);
  if (!state->cached_temperature_valid ||
      state->cached_temperature_c != temperature_c) {
    state->cached_temperature_c = temperature_c;
    state->cached_temperature_valid = true;
    return hid_event_queue_metric_float(device, _hid_metric_temperature,
                                        _hid_unit_celsius, temperature_c);
  }

  return true;
}

/**
 * @brief Release ADC/GPIO handles and state on HID deregistration.
 */
static bool _hid_adc_device_deinit(hid_device_t *device, void *userdata) {
  _hid_adc_state_t *state = (_hid_adc_state_t *)userdata;
  (void)device;

  if (state == NULL) {
    return true;
  }

  hw_adc_deinit(state->adc);
  if (state->gpio != NULL) {
    hw_gpio_deinit(state->gpio);
  }
  sys_free(state);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_adc(hid_t *instance, uint8_t channel,
                               uint32_t polling_interval_ms) {
  if (instance == NULL) {
    return NULL;
  }

  uint8_t pin = hw_adc_gpio_pin(channel);
  if (pin == 0xFFu) {
    sys_debugf("[hid] adc register failed: channel=%u has no GPIO pin",
               (unsigned int)channel);
    return NULL;
  }

  hw_gpio_t *gpio = hw_gpio_init(0u, pin, HW_GPIO_ADC);
  if (gpio == NULL) {
    sys_debugf("[hid] adc register failed: hw_gpio_init pin=%u",
               (unsigned int)pin);
    return NULL;
  }

  hw_adc_t *adc = hw_adc_init_pin(gpio);
  if (adc == NULL) {
    sys_debugf("[hid] adc register failed: hw_adc_init_pin channel=%u",
               (unsigned int)channel);
    hw_gpio_deinit(gpio);
    return NULL;
  }

  _hid_adc_state_t *state =
      (_hid_adc_state_t *)sys_calloc(1, sizeof(_hid_adc_state_t));
  if (state == NULL) {
    hw_adc_deinit(adc);
    hw_gpio_deinit(gpio);
    return NULL;
  }
  state->adc = adc;
  state->gpio = gpio;

  uint32_t effective_interval_ms = (polling_interval_ms == 0u)
                                       ? _hid_adc_default_poll_interval_ms
                                       : polling_interval_ms;

  hid_device_t *device =
      hid_register(instance, _hid_adc_name, channel, hid_type_other,
                  hid_class_sensor, effective_interval_ms, state,
                  _hid_adc_callbacks);
  if (device == NULL) {
    sys_debugf("[hid] adc register failed: hid_register channel=%u",
               (unsigned int)channel);
    hw_adc_deinit(adc);
    hw_gpio_deinit(gpio);
    sys_free(state);
    return NULL;
  }

  return device;
}

hid_device_t *hid_register_temperature(hid_t *instance,
                                       uint32_t polling_interval_ms) {
  if (instance == NULL) {
    return NULL;
  }

  hw_adc_t *adc = hw_adc_init_temperature();
  if (adc == NULL) {
    sys_debugf("[hid] temperature register failed: hw_adc_init_temperature");
    return NULL;
  }

  _hid_adc_state_t *state =
      (_hid_adc_state_t *)sys_calloc(1, sizeof(_hid_adc_state_t));
  if (state == NULL) {
    hw_adc_deinit(adc);
    return NULL;
  }
  state->adc = adc;
  state->gpio = NULL;

  uint32_t effective_interval_ms = (polling_interval_ms == 0u)
                                       ? _hid_adc_default_poll_interval_ms
                                       : polling_interval_ms;

  hid_device_t *device =
      hid_register(instance, _hid_temperature_name, 0u, hid_type_other,
                  hid_class_sensor, effective_interval_ms, state,
                  _hid_temperature_callbacks);
  if (device == NULL) {
    sys_debugf("[hid] temperature register failed: hid_register");
    hw_adc_deinit(adc);
    sys_free(state);
    return NULL;
  }

  return device;
}

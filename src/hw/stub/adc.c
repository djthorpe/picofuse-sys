#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_adc_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_adc_count(void) { return 0; }

hw_adc_t *hw_adc_init_pin(hw_gpio_t *gpio) {
  sys_debugf("hw", "adc_init_pin: gpio=%p unsupported on this platform", gpio);
  (void)gpio;
  return NULL;
}

hw_adc_t *hw_adc_init_temperature(void) {
  sys_debugf("hw", "adc_init_temperature: unsupported on this platform");
  return NULL;
}

hw_adc_t *hw_adc_init_vsys(hw_gpio_t **vbus_gpio) {
  sys_debugf("hw", "adc_init_vsys: unsupported on this platform");
  if (vbus_gpio != NULL) {
    *vbus_gpio = NULL;
  }
  return NULL;
}

void hw_adc_deinit(hw_adc_t *adc) {
  sys_debugf("hw", "adc_deinit: adc=%p unsupported on this platform", adc);
  (void)adc;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

uint8_t hw_adc_gpio_channel(const hw_gpio_t *gpio) {
  (void)gpio;
  return UINT8_MAX;
}

uint8_t hw_adc_gpio_pin(uint8_t channel) {
  (void)channel;
  return UINT8_MAX;
}

bool hw_adc_valid(const hw_adc_t *adc) {
  (void)adc;
  return false;
}

uint16_t hw_adc_read_12(hw_adc_t *adc, uint16_t num_samples) {
  (void)adc;
  (void)num_samples;
  return 0;
}

uint16_t hw_adc_read_16(hw_adc_t *adc, uint16_t num_samples) {
  (void)adc;
  (void)num_samples;
  return 0;
}

float hw_adc_read_voltage(hw_adc_t *adc, uint16_t num_samples) {
  (void)adc;
  (void)num_samples;
  return 0.0f;
}

float hw_adc_read_temperature(hw_adc_t *adc, uint16_t num_samples) {
  (void)adc;
  (void)num_samples;
  return 0.0f;
}

bool hw_adc_vbus_supported(void) { return false; }

bool hw_adc_vbus_present(void) { return false; }
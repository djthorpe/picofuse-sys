#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_adc_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_adc_count(void) { return 0; }

hw_adc_t *hw_adc_init_pin(hw_gpio_t *gpio) {
  sys_debugf("adc_init_pin: gpio=%p unsupported on this platform", gpio);
  (void)gpio;
  return NULL;
}

hw_adc_t *hw_adc_init_temperature(void) {
  sys_debugf("adc_init_temperature: unsupported on this platform");
  return NULL;
}

void hw_adc_deinit(hw_adc_t *adc) {
  sys_debugf("adc_deinit: adc=%p unsupported on this platform", adc);
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

uint16_t hw_adc_read_12(hw_adc_t *adc) {
  (void)adc;
  return 0;
}

uint16_t hw_adc_read_16(hw_adc_t *adc) {
  (void)adc;
  return 0;
}

float hw_adc_read_voltage(hw_adc_t *adc) {
  (void)adc;
  return 0.0f;
}

float hw_adc_read_temperature(hw_adc_t *adc) {
  (void)adc;
  return 0.0f;
}
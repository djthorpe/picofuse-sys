#include <picofuse/hw.h>

struct hw_led_t {};

hw_led_t *hw_led_init_gpio(hw_gpio_t *gpio) {
  (void)gpio;
  return NULL;
}

hw_led_t *hw_led_init_neopixel(hw_gpio_t *gpio, uint8_t led_count) {
  (void)gpio;
  (void)led_count;
  return NULL;
}

hw_led_t *hw_led_init_wifi(void) { return NULL; }

hw_led_t *hw_led_init_pwm(hw_pwm_t *pwm) {
  (void)pwm;
  return NULL;
}

hw_led_t *hw_led_init_default(void) { return NULL; }

void hw_led_deinit(hw_led_t *led) { (void)led; }

bool hw_led_set(hw_led_t *led, uint8_t index, bool enabled) {
  (void)led;
  (void)index;
  (void)enabled;
  return false;
}

bool hw_led_blink(hw_led_t *led, uint8_t index, uint32_t period_ms,
                  bool repeating) {
  (void)led;
  (void)index;
  (void)period_ms;
  (void)repeating;
  return false;
}

uint8_t hw_led_gpio_default(hw_led_type_t *out_type, uint8_t *out_count) {
  if (out_type != NULL) {
    *out_type = HW_LED_TYPE_NONE;
  }
  if (out_count != NULL) {
    *out_count = 0;
  }
  return HW_LED_GPIO_NONE;
}

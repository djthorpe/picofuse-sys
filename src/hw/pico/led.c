#include <pico.h>
#include <picofuse/hw.h>

#ifdef PICO_CYW43_SUPPORTED
#include "cyw43.h"
#include <pico/cyw43_arch.h>
#endif

struct hw_led_t {
  bool initialized;
  bool owns_gpio;
  uint8_t led_count;
  hw_led_type_t type;
  hw_gpio_t *gpio;
  hw_pwm_t *pwm;
};

static struct hw_led_t _hw_led_instance = {0};

hw_led_t *hw_led_init_gpio(hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }

  hw_led_deinit(&_hw_led_instance);
  _hw_led_instance.initialized = true;
  _hw_led_instance.owns_gpio = false;
  _hw_led_instance.led_count = 1;
  _hw_led_instance.type = HW_LED_TYPE_GPIO;
  _hw_led_instance.gpio = gpio;
  _hw_led_instance.pwm = NULL;
  return &_hw_led_instance;
}

hw_led_t *hw_led_init_neopixel(hw_gpio_t *gpio, uint8_t led_count) {
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }
  if (led_count == 0) {
    return NULL;
  }

  hw_led_deinit(&_hw_led_instance);
  _hw_led_instance.initialized = true;
  _hw_led_instance.owns_gpio = false;
  _hw_led_instance.led_count = led_count;
  _hw_led_instance.type = HW_LED_TYPE_NEOPIXEL;
  _hw_led_instance.gpio = gpio;
  _hw_led_instance.pwm = NULL;
  return &_hw_led_instance;
}

hw_led_t *hw_led_init_wifi(void) {
#ifdef PICO_CYW43_SUPPORTED
  if (!cyw43_is_initialized(&cyw43_state)) {
    return NULL;
  }

  hw_led_deinit(&_hw_led_instance);
  _hw_led_instance.initialized = true;
  _hw_led_instance.owns_gpio = false;
  _hw_led_instance.led_count = 1;
  _hw_led_instance.type = HW_LED_TYPE_WIFI;
  _hw_led_instance.gpio = NULL;
  _hw_led_instance.pwm = NULL;
  return &_hw_led_instance;
#else
  return NULL;
#endif
}

hw_led_t *hw_led_init_pwm(hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }

  hw_pwm_config_t config = {
      .period_ns = 1000000u,
      .duty_percent = 0.0f,
      .enabled = true,
  };
  hw_pwm_t *pwm = hw_pwm_init(gpio, NULL, NULL, &config);
  if (!hw_pwm_valid(pwm)) {
    return NULL;
  }

  hw_led_deinit(&_hw_led_instance);
  _hw_led_instance.initialized = true;
  _hw_led_instance.owns_gpio = false;
  _hw_led_instance.led_count = 1;
  _hw_led_instance.type = HW_LED_TYPE_PWM;
  _hw_led_instance.gpio = gpio;
  _hw_led_instance.pwm = pwm;
  return &_hw_led_instance;
}

hw_led_t *hw_led_init_default(void) {
  hw_led_type_t led_type = HW_LED_TYPE_NONE;
  uint8_t led_count = 0;
  uint8_t led_pin = hw_led_gpio_default(&led_type, &led_count);

  if (led_type == HW_LED_TYPE_NONE || led_pin == HW_LED_GPIO_NONE) {
    return NULL;
  }

  if (led_type == HW_LED_TYPE_WIFI) {
    return hw_led_init_wifi();
  }

  hw_gpio_mode_t mode = HW_GPIO_OUTPUT;
  if (led_type == HW_LED_TYPE_PWM) {
    mode = HW_GPIO_PWM;
  }

  hw_gpio_t *gpio = hw_gpio_init(0, led_pin, mode);
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }

  hw_led_t *led = NULL;
  switch (led_type) {
  case HW_LED_TYPE_GPIO:
    led = hw_led_init_gpio(gpio);
    break;
  case HW_LED_TYPE_NEOPIXEL:
    led = hw_led_init_neopixel(gpio, led_count > 0 ? led_count : 1);
    break;
  case HW_LED_TYPE_PWM:
    led = hw_led_init_pwm(gpio);
    break;
  default:
    hw_gpio_deinit(gpio);
    return NULL;
  }

  if (led == NULL) {
    hw_gpio_deinit(gpio);
    return NULL;
  }

  led->owns_gpio = true;
  return led;
}

void hw_led_deinit(hw_led_t *led) {
  if (led == NULL || !led->initialized) {
    return;
  }

  if (led->pwm != NULL) {
    hw_pwm_deinit(led->pwm);
  }

  if (led->owns_gpio && led->gpio != NULL && hw_gpio_valid(led->gpio)) {
    hw_gpio_deinit(led->gpio);
  }

  led->initialized = false;
  led->owns_gpio = false;
  led->led_count = 0;
  led->type = HW_LED_TYPE_NONE;
  led->gpio = NULL;
  led->pwm = NULL;
}

bool hw_led_set(hw_led_t *led, bool enabled) {
  if (led == NULL || !led->initialized) {
    return false;
  }

  switch (led->type) {
  case HW_LED_TYPE_GPIO:
    if (!hw_gpio_valid(led->gpio)) {
      return false;
    }
    hw_gpio_set(led->gpio, enabled);
    return true;
  case HW_LED_TYPE_PWM:
    if (!hw_pwm_valid(led->pwm)) {
      return false;
    }
    hw_pwm_set_enabled(led->pwm, enabled);
    hw_pwm_set_duty_percent(led->pwm, enabled ? 100.0f : 0.0f);
    return true;
  case HW_LED_TYPE_WIFI:
#ifdef PICO_CYW43_SUPPORTED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, enabled ? 1 : 0);
    return true;
#else
    return false;
#endif
  case HW_LED_TYPE_NEOPIXEL:
  case HW_LED_TYPE_NONE:
  default:
    return false;
  }
}

uint8_t hw_led_gpio_default(hw_led_type_t *out_type, uint8_t *out_count) {
#if defined(PIMORONI_PRESTO) && defined(PICO_DEFAULT_LED_PIN)
  if (out_type != NULL) {
    *out_type = HW_LED_TYPE_NEOPIXEL;
  }
  if (out_count != NULL) {
#ifdef PICO_DEFAULT_WS2812_NUM_PIXELS
    *out_count = (uint8_t)PICO_DEFAULT_WS2812_NUM_PIXELS;
#else
    *out_count = 1;
#endif
  }
  return (uint8_t)PICO_DEFAULT_LED_PIN;
#elif defined(PICO_DEFAULT_LED_PIN)
  if (out_type != NULL) {
    *out_type = HW_LED_TYPE_GPIO;
  }
  if (out_count != NULL) {
    *out_count = 1;
  }
  return (uint8_t)PICO_DEFAULT_LED_PIN;
#elif defined(CYW43_WL_GPIO_LED_PIN)
  if (out_type != NULL) {
    *out_type = HW_LED_TYPE_WIFI;
  }
  if (out_count != NULL) {
    *out_count = 1;
  }
  return (uint8_t)CYW43_WL_GPIO_LED_PIN;
#elif defined(PICO_DEFAULT_WS2812_PIN)
  if (out_type != NULL) {
    *out_type = HW_LED_TYPE_NEOPIXEL;
  }
  if (out_count != NULL) {
#ifdef PICO_DEFAULT_WS2812_NUM_PIXELS
    *out_count = (uint8_t)PICO_DEFAULT_WS2812_NUM_PIXELS;
#else
    *out_count = 1;
#endif
  }
  return (uint8_t)PICO_DEFAULT_WS2812_PIN;
#else
  if (out_type != NULL) {
    *out_type = HW_LED_TYPE_NONE;
  }
  if (out_count != NULL) {
    *out_count = 0;
  }
  return HW_LED_GPIO_NONE;
#endif
}

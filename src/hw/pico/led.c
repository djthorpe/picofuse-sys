#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <pico.h>
#include <pico/critical_section.h>
#include <pico/time.h>
#include <picofuse/hw.h>
#include <picofuse/pix.h>
#include <picofuse/sys.h>

#include "led_neopixel.pio.h"

#define HW_LED_NEOPIXEL_MAX_PIXELS 64u

#ifdef PICO_CYW43_SUPPORTED
#include "cyw43.h"
#include <pico/cyw43_arch.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_led_t {
  bool initialized;
  bool owns_gpio;
  bool blink_repeating;
  bool blink_phase_on;
  uint8_t led_count;
  uint8_t blink_index;
  uint8_t wifi_pin;
  hw_led_type_t type;
  hw_gpio_t *gpio;
  hw_pwm_t *pwm;
  sys_timer_t *blink_timer;
  PIO neopixel_pio;
  int8_t neopixel_sm;
  pix_color_t neopixel_color[HW_LED_NEOPIXEL_MAX_PIXELS];
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_led_t _hw_led_pool[HW_LED_POOL_CAPACITY] = {0};
static bool _hw_led_neopixel_program_loaded[2] = {false, false};
static uint _hw_led_neopixel_program_offset[2] = {0u, 0u};
static critical_section_t _hw_led_lock;

///////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS

void _hw_led_module_init(void) { critical_section_init(&_hw_led_lock); }

void _hw_led_module_exit(void) {}

static hw_led_t *_hw_led_alloc(void) {
  critical_section_enter_blocking(&_hw_led_lock);
  for (uint8_t i = 0; i < HW_LED_POOL_CAPACITY; i++) {
    if (!_hw_led_pool[i].initialized) {
      // Reserve this slot so concurrent allocators cannot return it.
      _hw_led_pool[i].initialized = true;
      critical_section_exit(&_hw_led_lock);
      return &_hw_led_pool[i];
    }
  }
  critical_section_exit(&_hw_led_lock);
  return NULL;
}

static uint8_t _hw_led_neopixel_pio_index(PIO pio) {
  if (pio == pio0) {
    return 0u;
  }
  return 1u;
}

static uint32_t _hw_led_neopixel_pack_color(pix_color_t color) {
  uint8_t r = (uint8_t)((color >> 24) & 0xFFu);
  uint8_t g = (uint8_t)((color >> 16) & 0xFFu);
  uint8_t b = (uint8_t)((color >> 8) & 0xFFu);
  return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

static bool _hw_led_neopixel_program_ensure(PIO pio, uint *out_offset) {
  uint8_t pio_idx = _hw_led_neopixel_pio_index(pio);
  if (_hw_led_neopixel_program_loaded[pio_idx]) {
    *out_offset = _hw_led_neopixel_program_offset[pio_idx];
    return true;
  }

  if (!pio_can_add_program(pio, &led_neopixel_program)) {
    return false;
  }

  uint offset = pio_add_program(pio, &led_neopixel_program);
  _hw_led_neopixel_program_loaded[pio_idx] = true;
  _hw_led_neopixel_program_offset[pio_idx] = offset;
  *out_offset = offset;
  return true;
}

static bool _hw_led_neopixel_pio_init(hw_led_t *led) {
  if (led == NULL || !hw_gpio_valid(led->gpio)) {
    return false;
  }

  PIO pio_candidates[2] = {pio0, pio1};
  for (uint8_t i = 0; i < 2; i++) {
    PIO pio = pio_candidates[i];
    int sm = pio_claim_unused_sm(pio, false);
    if (sm < 0) {
      continue;
    }

    uint offset = 0u;
    if (!_hw_led_neopixel_program_ensure(pio, &offset)) {
      pio_sm_unclaim(pio, (uint)sm);
      continue;
    }

    uint8_t pin = hw_gpio_get_pin_num(led->gpio);
    led_neopixel_program_init(pio, (uint)sm, offset, pin, 800000.0f);

    led->neopixel_pio = pio;
    led->neopixel_sm = (int8_t)sm;
    return true;
  }

  return false;
}

static bool _hw_led_neopixel_index_valid(const hw_led_t *led, uint8_t index) {
  return led != NULL && led->type == HW_LED_TYPE_NEOPIXEL &&
         index < led->led_count && led->led_count <= HW_LED_NEOPIXEL_MAX_PIXELS;
}

static pix_color_t _hw_led_neopixel_effective_color(const hw_led_t *led,
                                                    uint8_t index) {
  if (!_hw_led_neopixel_index_valid(led, index)) {
    return 0u;
  }

  return led->neopixel_color[index];
}

static bool _hw_led_neopixel_flush(const hw_led_t *led) {
  if (led == NULL || led->type != HW_LED_TYPE_NEOPIXEL ||
      !hw_gpio_valid(led->gpio) || led->neopixel_pio == NULL ||
      led->neopixel_sm < 0) {
    return false;
  }

  for (uint8_t i = 0; i < led->led_count; i++) {
    pix_color_t color = _hw_led_neopixel_effective_color(led, i);
    uint32_t grb = _hw_led_neopixel_pack_color(color);
    pio_sm_put_blocking(led->neopixel_pio, (uint)led->neopixel_sm, grb << 8u);
  }

  sleep_us(80u);

  return true;
}

static bool _hw_led_neopixel_set_onoff(hw_led_t *led, uint8_t index,
                                       bool enabled) {
  if (!_hw_led_neopixel_index_valid(led, index)) {
    return false;
  }

  if (enabled) {
    if (led->neopixel_color[index] == 0u) {
      led->neopixel_color[index] = 0xFFFFFFFFu;
    }
  } else {
    led->neopixel_color[index] = 0u;
  }

  return _hw_led_neopixel_flush(led);
}

static bool _hw_led_apply_state(hw_led_t *led, uint8_t index, bool enabled) {
  if (led == NULL || !led->initialized) {
    return false;
  }

  switch (led->type) {
  case HW_LED_TYPE_GPIO:
    (void)index;
    if (!hw_gpio_valid(led->gpio)) {
      return false;
    }
    hw_gpio_set(led->gpio, enabled);
    return true;
  case HW_LED_TYPE_PWM:
    (void)index;
    if (!hw_pwm_valid(led->pwm)) {
      return false;
    }
    hw_pwm_set_enabled(led->pwm, enabled);
    hw_pwm_set_duty_percent(led->pwm, enabled ? 100.0f : 0.0f);
    return true;
  case HW_LED_TYPE_WIFI:
    (void)index;
#ifdef PICO_CYW43_SUPPORTED
    if (led->wifi_pin == HW_LED_GPIO_NONE) {
      return false;
    }
    cyw43_arch_gpio_put(led->wifi_pin, enabled ? 1 : 0);
    return true;
#else
    return false;
#endif
  case HW_LED_TYPE_NEOPIXEL:
    return _hw_led_neopixel_set_onoff(led, index, enabled);
  case HW_LED_TYPE_NONE:
  default:
    return false;
  }
}

static void _hw_led_blink_stop(hw_led_t *led) {
  sys_timer_t *timer = NULL;

  critical_section_enter_blocking(&_hw_led_lock);
  if (led != NULL && led->blink_timer != NULL) {
    timer = led->blink_timer;
    led->blink_timer = NULL;
    led->blink_repeating = false;
    led->blink_phase_on = false;
  }
  critical_section_exit(&_hw_led_lock);

  if (timer != NULL) {
    sys_timer_deinit(timer);
  }
}

static void _hw_led_blink_timer_cb(sys_timer_t *timer) {
  hw_led_t *led = (hw_led_t *)sys_timer_get_userdata(timer);
  if (led == NULL) {
    sys_timer_deinit(timer);
    return;
  }

  bool should_deinit = false;

  critical_section_enter_blocking(&_hw_led_lock);
  if (!led->initialized || led->blink_timer != timer) {
    should_deinit = true;
  } else if (!led->blink_repeating) {
    (void)_hw_led_apply_state(led, led->blink_index, false);
    led->blink_timer = NULL;
    led->blink_phase_on = false;
    should_deinit = true;
  } else {
    led->blink_phase_on = !led->blink_phase_on;
    (void)_hw_led_apply_state(led, led->blink_index, led->blink_phase_on);
  }
  critical_section_exit(&_hw_led_lock);

  if (should_deinit) {
    sys_timer_deinit(timer);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_led_t *hw_led_init_gpio(hw_gpio_t *gpio) {
  sys_debugf("led_init_gpio: gpio_valid=%u", hw_gpio_valid(gpio));
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }

  hw_led_t *led = _hw_led_alloc();
  if (led == NULL) {
    return NULL;
  }

  led->initialized = true;
  led->owns_gpio = false;
  led->blink_repeating = false;
  led->blink_phase_on = false;
  led->led_count = 1;
  led->blink_index = 0;
  led->wifi_pin = HW_LED_GPIO_NONE;
  led->type = HW_LED_TYPE_GPIO;
  led->gpio = gpio;
  led->pwm = NULL;
  led->blink_timer = NULL;
  led->neopixel_pio = NULL;
  led->neopixel_sm = -1;
  return led;
}

hw_led_t *hw_led_init_neopixel(hw_gpio_t *gpio, uint8_t led_count) {
  sys_debugf("led_init_neopixel: gpio_valid=%u led_count=%u",
             hw_gpio_valid(gpio), led_count);
  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }
  if (led_count == 0 || led_count > HW_LED_NEOPIXEL_MAX_PIXELS) {
    return NULL;
  }

  hw_led_t *led = _hw_led_alloc();
  if (led == NULL) {
    return NULL;
  }

  led->initialized = true;
  led->owns_gpio = false;
  led->blink_repeating = false;
  led->blink_phase_on = false;
  led->led_count = led_count;
  led->blink_index = 0;
  led->wifi_pin = HW_LED_GPIO_NONE;
  led->type = HW_LED_TYPE_NEOPIXEL;
  led->gpio = gpio;
  led->pwm = NULL;
  led->blink_timer = NULL;
  led->neopixel_pio = NULL;
  led->neopixel_sm = -1;

  critical_section_enter_blocking(&_hw_led_lock);
  if (!_hw_led_neopixel_pio_init(led)) {
    critical_section_exit(&_hw_led_lock);
    led->initialized = false;
    return NULL;
  }

  for (uint8_t i = 0; i < led_count; i++) {
    led->neopixel_color[i] = 0u;
  }

  if (!_hw_led_neopixel_flush(led)) {
    critical_section_exit(&_hw_led_lock);
    led->initialized = false;
    return NULL;
  }
  critical_section_exit(&_hw_led_lock);

  return led;
}

hw_led_t *hw_led_init_wifi(void) {
  sys_debugf("led_init_wifi");
#ifdef PICO_CYW43_SUPPORTED
  hw_led_type_t led_type = HW_LED_TYPE_NONE;
  uint8_t led_pin = hw_led_gpio_default(&led_type, NULL);
  if (!cyw43_is_initialized(&cyw43_state)) {
    sys_debugf("led_init_wifi: cyw43 not initialized");
    return NULL;
  }
  if (led_type != HW_LED_TYPE_WIFI || led_pin == HW_LED_GPIO_NONE) {
    sys_debugf("led_init_wifi: no wifi LED pin");
    return NULL;
  }

  hw_led_t *led = _hw_led_alloc();
  if (led == NULL) {
    return NULL;
  }

  led->initialized = true;
  led->owns_gpio = false;
  led->blink_repeating = false;
  led->blink_phase_on = false;
  led->led_count = 1;
  led->blink_index = 0;
  led->wifi_pin = led_pin;
  led->type = HW_LED_TYPE_WIFI;
  led->gpio = NULL;
  led->pwm = NULL;
  led->blink_timer = NULL;
  led->neopixel_pio = NULL;
  led->neopixel_sm = -1;
  return led;
#else
  return NULL;
#endif
}

hw_led_t *hw_led_init_pwm(hw_pwm_t *pwm) {
  sys_debugf("led_init_pwm: pwm_valid=%u", hw_pwm_valid(pwm));
  if (!hw_pwm_valid(pwm)) {
    return NULL;
  }

  // Ensure PWM-backed LEDs always start in an off/disabled state.
  hw_pwm_set_duty_percent(pwm, 0.0f);
  hw_pwm_set_enabled(pwm, false);

  hw_led_t *led = _hw_led_alloc();
  if (led == NULL) {
    hw_pwm_deinit(pwm);
    return NULL;
  }

  led->initialized = true;
  led->owns_gpio = false;
  led->blink_repeating = false;
  led->blink_phase_on = false;
  led->led_count = 1;
  led->blink_index = 0;
  led->wifi_pin = HW_LED_GPIO_NONE;
  led->type = HW_LED_TYPE_PWM;
  led->gpio = NULL;
  led->pwm = pwm;
  led->blink_timer = NULL;
  led->neopixel_pio = NULL;
  led->neopixel_sm = -1;
  return led;
}

hw_led_t *hw_led_init_default(void) {
  hw_led_type_t led_type = HW_LED_TYPE_NONE;
  uint8_t led_count = 0;
  uint8_t led_pin = hw_led_gpio_default(&led_type, &led_count);

  sys_debugf("led_init_default: type=%u pin=%u count=%u", led_type, led_pin,
             led_count);

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
  case HW_LED_TYPE_PWM: {
    hw_pwm_config_t config = {
        .period_ns = 1000000u,
        .duty_percent = 0.0f,
        .enabled = false,
    };
    hw_pwm_t *pwm = hw_pwm_init(gpio, NULL, NULL, &config);
    if (!hw_pwm_valid(pwm)) {
      hw_gpio_deinit(gpio);
      return NULL;
    }
    led = hw_led_init_pwm(pwm);
    if (led == NULL) {
      hw_pwm_deinit(pwm);
    } else {
      led->gpio = gpio;
    }
  } break;
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
  sys_debugf("led_deinit: led=%p initialized=%u", (void *)led,
             led != NULL ? led->initialized : 0);
  if (led == NULL || !led->initialized) {
    return;
  }

  _hw_led_blink_stop(led);

  critical_section_enter_blocking(&_hw_led_lock);

  if (led->pwm != NULL) {
    hw_pwm_deinit(led->pwm);
  }

  if (led->type == HW_LED_TYPE_NEOPIXEL) {
    for (uint8_t i = 0; i < led->led_count && i < HW_LED_NEOPIXEL_MAX_PIXELS;
         i++) {
      led->neopixel_color[i] = 0u;
    }
    (void)_hw_led_neopixel_flush(led);

    if (led->neopixel_pio != NULL && led->neopixel_sm >= 0) {
      pio_sm_set_enabled(led->neopixel_pio, (uint)led->neopixel_sm, false);
      pio_sm_unclaim(led->neopixel_pio, (uint)led->neopixel_sm);
    }
  }

  if (led->owns_gpio && led->gpio != NULL && hw_gpio_valid(led->gpio)) {
    hw_gpio_deinit(led->gpio);
  }

  led->initialized = false;
  led->owns_gpio = false;
  led->blink_repeating = false;
  led->blink_phase_on = false;
  led->led_count = 0;
  led->blink_index = 0;
  led->wifi_pin = HW_LED_GPIO_NONE;
  led->type = HW_LED_TYPE_NONE;
  led->gpio = NULL;
  led->pwm = NULL;
  led->blink_timer = NULL;
  led->neopixel_pio = NULL;
  led->neopixel_sm = -1;
  critical_section_exit(&_hw_led_lock);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool hw_led_set(hw_led_t *led, uint8_t index, bool enabled) {
  _hw_led_blink_stop(led);
  critical_section_enter_blocking(&_hw_led_lock);
  bool ok = _hw_led_apply_state(led, index, enabled);
  critical_section_exit(&_hw_led_lock);
  return ok;
}

bool hw_led_blink(hw_led_t *led, uint8_t index, uint32_t period_ms,
                  bool repeating) {
  if (period_ms == 0u) {
    return false;
  }

  critical_section_enter_blocking(&_hw_led_lock);
  if (led == NULL || !led->initialized) {
    critical_section_exit(&_hw_led_lock);
    return false;
  }

  // Only one active blink operation is supported per LED handle.
  if (led->blink_timer != NULL) {
    critical_section_exit(&_hw_led_lock);
    return false;
  }

  if (led->type == HW_LED_TYPE_NEOPIXEL &&
      !_hw_led_neopixel_index_valid(led, index)) {
    critical_section_exit(&_hw_led_lock);
    return false;
  }
  critical_section_exit(&_hw_led_lock);

  // Allocate a timer
  sys_timer_t *timer = sys_timer_init(period_ms, led, _hw_led_blink_timer_cb);
  if (timer == NULL) {
    return false;
  }

  // Switch on the LED immediately and start blinking. If starting the timer
  // fails, revert.
  critical_section_enter_blocking(&_hw_led_lock);
  if (led == NULL || !led->initialized || led->blink_timer != NULL) {
    critical_section_exit(&_hw_led_lock);
    sys_timer_deinit(timer);
    return false;
  }

  if (!_hw_led_apply_state(led, index, true)) {
    critical_section_exit(&_hw_led_lock);
    sys_timer_deinit(timer);
    return false;
  }

  // Initialize blink state and associate timer with LED
  led->blink_timer = timer;
  led->blink_repeating = repeating;
  led->blink_phase_on = true;
  led->blink_index = index;
  critical_section_exit(&_hw_led_lock);

  // Start the timer and revert state if it fails
  if (!sys_timer_start(timer)) {
    critical_section_enter_blocking(&_hw_led_lock);
    bool own_timer = (led != NULL && led->blink_timer == timer);
    if (own_timer) {
      led->blink_timer = NULL;
      led->blink_repeating = false;
      led->blink_phase_on = false;
      (void)_hw_led_apply_state(led, index, false);
    }
    critical_section_exit(&_hw_led_lock);

    sys_timer_deinit(timer);
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

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

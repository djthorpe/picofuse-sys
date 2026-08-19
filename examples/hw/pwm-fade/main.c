/**
 * @file
 * @brief PWM LED fade example.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define PWM_PERIOD_NS 1000000u
#define FADE_STEP_PERCENT 1
#define FADE_UPDATE_TICKS 10u

#ifdef SYSTEM_NAME_PICO
typedef struct {
  int16_t duty_percent;
  int8_t step_percent;
  uint8_t ticks;
  volatile uint32_t wrap_count;
  volatile uint32_t update_count;
} fade_state_t;

static void fade_callback(hw_pwm_t *pwm, void *userdata) {
  fade_state_t *state = userdata;
  if (state == NULL) {
    return;
  }

  state->wrap_count++;
  state->ticks++;
  if (state->ticks < FADE_UPDATE_TICKS) {
    return;
  }
  state->ticks = 0;

  hw_pwm_set_duty_percent(pwm, (float)state->duty_percent);
  state->update_count++;

  state->duty_percent += state->step_percent;
  if (state->duty_percent >= 100) {
    state->duty_percent = 100;
    state->step_percent = -FADE_STEP_PERCENT;
  } else if (state->duty_percent <= 0) {
    state->duty_percent = 0;
    state->step_percent = FADE_STEP_PERCENT;
  }
}
#endif

int main(void) {
  sys_init();
  hw_init();

#ifdef SYSTEM_NAME_PICO
  hw_led_type_t led_type = HW_LED_TYPE_NONE;
  uint8_t led_pin = hw_led_gpio_default(&led_type, NULL);
  if (led_type != HW_LED_TYPE_GPIO || led_pin == HW_LED_GPIO_NONE) {
    sys_printf("PWM fade example requires a GPIO on-board LED pin\n");
    hw_exit();
    sys_exit();
    return 0;
  }

  hw_gpio_t *led = hw_gpio_init(0, led_pin, HW_GPIO_PWM);
  if (!hw_gpio_valid(led)) {
    sys_panicf("Failed to initialize PWM LED GPIO %u", led_pin);
  }

  hw_pwm_config_t config = {
      .period_ns = PWM_PERIOD_NS,
      .duty_percent = 0.0f,
      .enabled = true,
  };
  fade_state_t state = {
      .duty_percent = 0,
      .step_percent = FADE_STEP_PERCENT,
      .ticks = 0,
      .wrap_count = 0,
      .update_count = 0,
  };
  hw_pwm_t *pwm = hw_pwm_init(led, fade_callback, &state, &config);
  if (!hw_pwm_valid(pwm)) {
    sys_panicf("Failed to initialize PWM on LED GPIO %u", led_pin);
  }

  sys_debugf("pwm_fade: led_pin=%u irq_supported=%u period_ns=%u ticks=%u",
             led_pin, hw_pwm_irq_supported(), (unsigned int)PWM_PERIOD_NS,
             FADE_UPDATE_TICKS);

  for (;;) {
    sys_debugf("pwm_fade: wraps=%u updates=%u duty=%d step=%d enabled=%u",
               state.wrap_count, state.update_count, state.duty_percent,
               state.step_percent, hw_pwm_get_enabled(pwm));
    sys_sleep_ms(1000);
  }
#else
  sys_printf("PWM fade example requires a PWM-capable on-board LED pin\n");
  hw_exit();
  sys_exit();
  return 0;
#endif
}

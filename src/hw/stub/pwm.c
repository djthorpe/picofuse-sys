#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_pwm_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_pwm_t *hw_pwm_init(hw_gpio_t *gpio, hw_pwm_callback_t callback,
                      void *userdata, const hw_pwm_config_t *config) {
  sys_debugf("hw", "pwm_init: unsupported on this platform (gpio=%p callback=%p "
             "userdata=%p config=%p)",
             (void *)gpio, (void *)callback, userdata, (const void *)config);
  if (callback != NULL && !hw_pwm_irq_supported()) {
    return NULL;
  }
  (void)gpio;
  (void)callback;
  (void)userdata;
  (void)config;
  return NULL;
}

void hw_pwm_deinit(hw_pwm_t *pwm) {
  sys_debugf("hw", "pwm_deinit: unsupported on this platform (pwm=%p)", (void *)pwm);
  (void)pwm;
}

bool hw_pwm_valid(const hw_pwm_t *pwm) {
  (void)pwm;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// CONFIGURATION

bool hw_pwm_set_period_ns(hw_pwm_t *pwm, uint64_t period_ns) {
  (void)pwm;
  (void)period_ns;
  return false;
}

uint64_t hw_pwm_get_period_ns(const hw_pwm_t *pwm) {
  (void)pwm;
  return 0;
}

bool hw_pwm_set_duty_percent(hw_pwm_t *pwm, float duty_percent) {
  (void)pwm;
  (void)duty_percent;
  return false;
}

float hw_pwm_get_duty_percent(const hw_pwm_t *pwm) {
  (void)pwm;
  return 0.0f;
}

bool hw_pwm_set_config(hw_pwm_t *pwm, const hw_pwm_config_t *config) {
  (void)pwm;
  (void)config;
  return false;
}

bool hw_pwm_get_config(const hw_pwm_t *pwm, hw_pwm_config_t *out_config) {
  (void)pwm;
  (void)out_config;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// CONTROL

void hw_pwm_set_enabled(hw_pwm_t *pwm, bool enabled) {
  (void)pwm;
  (void)enabled;
}

bool hw_pwm_get_enabled(const hw_pwm_t *pwm) {
  (void)pwm;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// INTERRUPTS

bool hw_pwm_irq_supported(void) { return false; }

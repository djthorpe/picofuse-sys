#include <test.h>

// Test callback counter
static volatile int _callback_count = 0;

static void pwm_callback(hw_pwm_t *pwm, void *userdata) {
  (void)pwm;
  (void)userdata;
  _callback_count++;
}

#ifdef SYSTEM_NAME_PICO
static bool period_close(uint64_t actual, uint64_t expected) {
  uint64_t delta = actual > expected ? actual - expected : expected - actual;
  uint64_t tolerance = expected / 1000 + 1;
  return delta <= tolerance;
}
#endif

bool test_main(void) {
  // Test NULL handle behavior
  hw_pwm_deinit(NULL);
  TestAssert(!hw_pwm_valid(NULL), "NULL PWM handle should be invalid");

  hw_pwm_config_t config = {
      .period_ns = 1000000, // 1 ms period
      .duty_percent = 50.0f,
      .enabled = false,
  };

  // Test that init returns NULL with NULL GPIO
  TestAssert(hw_pwm_init(NULL, pwm_callback, (void *)42, &config) == NULL,
             "PWM init should reject NULL GPIO handle");

#ifdef SYSTEM_NAME_PICO
  // Initialize GPIO for PWM on a Pico pin
  // GPIO 2 is a common PWM pin (slice 1, channel 0)
  hw_gpio_t *pwm_gpio = hw_gpio_init(0, 2, HW_GPIO_PWM);
  TestAssert(pwm_gpio != NULL, "GPIO initialization for PWM should succeed");

  // Test PWM init with valid GPIO
  _callback_count = 0;
  hw_pwm_t *pwm = hw_pwm_init(pwm_gpio, pwm_callback, (void *)42, &config);
  TestAssert(pwm != NULL, "PWM init should succeed on valid GPIO");
  TestAssert(hw_pwm_valid(pwm), "PWM handle should be valid after init");

  // Test get config returns what we set
  hw_pwm_config_t read_config = {0};
  TestAssert(hw_pwm_get_config(pwm, &read_config),
             "PWM get_config should succeed");
  TestAssert(period_close(read_config.period_ns, config.period_ns),
             "Period should match after init, expected %u, got %u",
             (uint32_t)config.period_ns, (uint32_t)read_config.period_ns);
  TestAssert(read_config.duty_percent == config.duty_percent,
             "Duty percent should match after init, expected %.1f, got %.1f",
             config.duty_percent, read_config.duty_percent);
  TestAssert(read_config.enabled == config.enabled,
             "Enabled state should match after init, expected %d, got %d",
             config.enabled, read_config.enabled);

  // Test period getter/setter
  uint64_t new_period = 2000000; // 2 ms
  TestAssert(hw_pwm_set_period_ns(pwm, new_period),
             "PWM set_period_ns should succeed");
  uint64_t read_period = hw_pwm_get_period_ns(pwm);
  TestAssert(read_period > 0, "PWM get_period_ns should return non-zero");
  TestAssert(period_close(read_period, new_period),
             "Period should be close to set value, expected ~%u, got %u",
             (uint32_t)new_period, (uint32_t)read_period);

  // Test duty cycle getter/setter
  float new_duty = 75.0f;
  TestAssert(hw_pwm_set_duty_percent(pwm, new_duty),
             "PWM set_duty_percent should succeed");
  float read_duty = hw_pwm_get_duty_percent(pwm);
  TestAssert(read_duty >= 0.0f && read_duty <= 100.0f,
             "Duty percent should be in range [0, 100], got %.1f", read_duty);
  // Allow some tolerance due to rounding
  TestAssert(
      read_duty >= new_duty - 2.0f && read_duty <= new_duty + 2.0f,
      "Duty percent should be close to set value, expected ~%.1f, got %.1f",
      new_duty, read_duty);

  // Test enable/disable
  TestAssert(!hw_pwm_get_enabled(pwm),
             "PWM should not be enabled after init with enabled=false");
  hw_pwm_set_enabled(pwm, true);
  TestAssert(hw_pwm_get_enabled(pwm), "PWM should be enabled after set");
  hw_pwm_set_enabled(pwm, false);
  TestAssert(!hw_pwm_get_enabled(pwm), "PWM should be disabled after set");

  // Test config setter with all parameters
  hw_pwm_config_t new_config = {
      .period_ns = 500000, // 500 µs
      .duty_percent = 25.0f,
      .enabled = true,
  };
  TestAssert(hw_pwm_set_config(pwm, &new_config),
             "PWM set_config should succeed");
  hw_pwm_config_t verify_config = {0};
  TestAssert(hw_pwm_get_config(pwm, &verify_config),
             "PWM get_config should succeed after set_config");
  TestAssert(verify_config.duty_percent >= new_config.duty_percent - 2.0f &&
                 verify_config.duty_percent <= new_config.duty_percent + 2.0f,
             "Config duty percent should match, expected %.1f, got %.1f",
             new_config.duty_percent, verify_config.duty_percent);
  TestAssert(verify_config.enabled == new_config.enabled,
             "Config enabled should match after set_config");

  // Test callback support
  TestAssert(hw_pwm_irq_supported(), "PWM should report IRQ support on Pico");
  // Note: actual interrupt testing would require special hardware setup

  // Test deinit
  hw_pwm_deinit(pwm);
  TestAssert(!hw_pwm_valid(pwm), "PWM handle should be invalid after deinit");

  // GPIO should still be valid after PWM deinit
  TestAssert(hw_gpio_valid(pwm_gpio),
             "GPIO should remain valid after PWM deinit");

  hw_gpio_deinit(pwm_gpio);

  // Test re-init on same GPIO should work
  hw_gpio_t *pwm_gpio2 = hw_gpio_init(0, 2, HW_GPIO_PWM);
  TestAssert(pwm_gpio2 != NULL, "GPIO re-init should succeed");
  hw_pwm_t *pwm2 = hw_pwm_init(pwm_gpio2, NULL, NULL, NULL);
  TestAssert(pwm2 != NULL, "PWM init with default config should succeed");
  TestAssert(hw_pwm_valid(pwm2), "PWM handle should be valid");
  hw_pwm_deinit(pwm2);
  hw_gpio_deinit(pwm_gpio2);

#else
  // On non-Pico platforms, PWM init should return NULL (stub backend)
  TestAssert(!hw_pwm_irq_supported(),
             "PWM stub should report IRQ callbacks as unsupported");

  hw_gpio_t *dummy_gpio = hw_gpio_init(0, 0, HW_GPIO_PWM);
  if (dummy_gpio != NULL) {
    hw_pwm_t *stub_pwm = hw_pwm_init(dummy_gpio, pwm_callback, NULL, &config);
    TestAssert(stub_pwm == NULL,
               "PWM stub should return NULL on non-Pico platforms");
    hw_gpio_deinit(dummy_gpio);
  }
#endif

  return true;
}

TestMain(test_main)

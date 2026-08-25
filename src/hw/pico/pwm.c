#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/sync.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_pwm_t {
  uint8_t slice;
  uint8_t channel;
  bool initialized;
  hw_pwm_callback_t callback;
  void *userdata;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static hw_pwm_t _hw_pwm_pool[NUM_PWM_SLICES * 2];
static bool _hw_pwm_irq_initialized = false;
static spin_lock_t *_hw_pwm_pool_lock = NULL;
static spin_lock_t *_hw_pwm_callback_lock = NULL;

///////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS

// Forward declarations
static void _hw_pwm_callback_handler(void);
static inline void _hw_pwm_init_locks(void);
static void _hw_pwm_set_callback(hw_pwm_t *pwm, hw_pwm_callback_t callback,
                                 void *userdata);

static inline void _hw_pwm_init_locks(void) {
  if (_hw_pwm_pool_lock == NULL) {
    _hw_pwm_pool_lock = spin_lock_instance(0);
  }
  if (_hw_pwm_callback_lock == NULL) {
    _hw_pwm_callback_lock = spin_lock_instance(1);
  }
}

static inline uint8_t _hw_pwm_handle_index(uint8_t slice, uint8_t channel) {
  return slice * 2 + channel;
}

// Hardware register accessors
static inline uint16_t _hw_pwm_get_wrap(uint8_t slice) {
  return pwm_hw->slice[slice].top;
}

static inline float _hw_pwm_get_divider(uint8_t slice) {
  uint16_t div_raw = pwm_hw->slice[slice].div;
  return (float)div_raw / 16.0f; // Divider is stored as a 16.16 fixed point
}

static inline uint16_t _hw_pwm_get_divider_raw(uint8_t slice) {
  return pwm_hw->slice[slice].div;
}

static inline uint16_t _hw_pwm_get_level(uint8_t slice, uint8_t channel) {
  return channel == 0 ? pwm_hw->slice[slice].cc & 0xFFFF
                      : (pwm_hw->slice[slice].cc >> 16) & 0xFFFF;
}

static inline bool _hw_pwm_get_enabled(uint8_t slice) {
  return (pwm_hw->slice[slice].csr & PWM_CH0_CSR_EN_BITS) != 0;
}

static inline float _hw_pwm_period_ns_to_divider(uint64_t period_ns,
                                                 uint16_t *out_wrap) {
  uint32_t sys_clk = clock_get_hz(clk_sys);
  long double target_counts =
      ((long double)period_ns * (long double)sys_clk) / 1000000000.0L;
  if (target_counts < 1.0L) {
    target_counts = 1.0L;
  }

  uint16_t best_wrap = 0;
  uint16_t best_div_raw = 16;
  long double best_error = target_counts;

  for (uint16_t div_raw = 16; div_raw < 4096; div_raw++) {
    long double counts_f = target_counts * 16.0L / (long double)div_raw;
    uint32_t counts = (uint32_t)(counts_f + 0.5L);
    if (counts < 1) {
      counts = 1;
    }
    if (counts > 65536) {
      counts = 65536;
    }

    long double actual_counts =
        ((long double)counts * (long double)div_raw) / 16.0L;
    long double error = actual_counts > target_counts
                            ? actual_counts - target_counts
                            : target_counts - actual_counts;
    if (error < best_error) {
      best_error = error;
      best_wrap = (uint16_t)(counts - 1);
      best_div_raw = div_raw;
      if (error == 0.0L) {
        break;
      }
    }
  }

  *out_wrap = best_wrap;
  return (float)best_div_raw / 16.0f;
}

static inline uint64_t _hw_pwm_divider_wrap_to_period_ns(float divider,
                                                         uint16_t wrap) {
  uint32_t sys_clk = clock_get_hz(clk_sys);
  float freq = (float)sys_clk / ((float)(wrap + 1) * divider);
  return (uint64_t)(1e9f / freq + 0.5f);
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_pwm_t *hw_pwm_init(hw_gpio_t *gpio, hw_pwm_callback_t callback,
                      void *userdata, const hw_pwm_config_t *config) {
  sys_debugf("hw", "pwm_init: gpio_valid=%u callback=%u config=%u",
             hw_gpio_valid(gpio), callback != NULL, config != NULL);

  if (callback != NULL && !hw_pwm_irq_supported()) {
    return NULL;
  }

  if (!hw_gpio_valid(gpio)) {
    return NULL;
  }

  _hw_pwm_init_locks();

  uint8_t gpio_pin = hw_gpio_get_pin_num(gpio);
  uint8_t slice = pwm_gpio_to_slice_num(gpio_pin);
  uint8_t channel = pwm_gpio_to_channel(gpio_pin);
  uint8_t idx = _hw_pwm_handle_index(slice, channel);

  uint32_t save = spin_lock_blocking(_hw_pwm_pool_lock);
  if (_hw_pwm_pool[idx].initialized) {
    spin_unlock(_hw_pwm_pool_lock, save);
    return NULL; // Already in use
  }
  hw_pwm_t *pwm = &_hw_pwm_pool[idx];

  pwm->slice = slice;
  pwm->channel = channel;
  pwm->callback = NULL;
  pwm->userdata = NULL;
  pwm->initialized = true;

  uint16_t wrap = 0xFFFF;
  uint16_t level = 0;
  float divider = 1.0f;
  bool enabled = false;

  // Apply config or use defaults
  if (config) {
    divider = _hw_pwm_period_ns_to_divider(config->period_ns, &wrap);
    enabled = config->enabled;

    // Calculate level from duty_percent
    if (config->duty_percent <= 0.0f) {
      level = 0;
    } else if (config->duty_percent >= 100.0f) {
      level = wrap;
    } else {
      uint64_t level_calc =
          (uint64_t)(wrap + 1) * (uint64_t)(config->duty_percent * 10000.0f);
      level = (uint16_t)(level_calc / (100 * 10000));
    }

    // Initialize PWM with calculated config
    pwm_config pico_config = pwm_get_default_config();
    pwm_config_set_wrap(&pico_config, wrap);
    pwm_config_set_clkdiv(&pico_config, divider);
    pwm_config_set_clkdiv_mode(&pico_config, PWM_DIV_FREE_RUNNING);
    pwm_init(slice, &pico_config, enabled);
    pwm_set_chan_level(slice, channel, level);
  } else {
    // Defaults: 65535 wrap, divider 1.0, level 0, disabled
    pwm_config pico_config = pwm_get_default_config();
    pwm_config_set_wrap(&pico_config, 0xFFFF);
    pwm_config_set_clkdiv(&pico_config, 1.0f);
    pwm_config_set_clkdiv_mode(&pico_config, PWM_DIV_FREE_RUNNING);
    pwm_init(slice, &pico_config, false);
    pwm_set_chan_level(slice, channel, 0);
  }

  // Configure GPIO for PWM
  gpio_init(gpio_pin);
  gpio_set_function(gpio_pin, GPIO_FUNC_PWM);

#ifndef NDEBUG
  uint64_t period_ns = config != NULL
                           ? config->period_ns
                           : _hw_pwm_divider_wrap_to_period_ns(1.0f, 0xFFFF);
  uint16_t div_raw = _hw_pwm_get_divider_raw(slice);
  sys_debugf("hw", "pwm_init: pin=%u slice=%u channel=%u callback=%u", gpio_pin,
             slice, channel, callback != NULL);
  sys_debugf("hw",
      "pwm_init: period_ns=%u wrap=%u div=%u.%u level=%u enabled=%u",
      (uint32_t)period_ns, wrap, div_raw >> 4, div_raw & 0x0F, level, enabled);
#endif

  spin_unlock(_hw_pwm_pool_lock, save);

  _hw_pwm_set_callback(pwm, callback, userdata);

  return pwm;
}

void hw_pwm_deinit(hw_pwm_t *pwm) {
#ifndef NDEBUG
  if (pwm != NULL && hw_pwm_valid(pwm)) {
    uint16_t div_raw = _hw_pwm_get_divider_raw(pwm->slice);
    sys_debugf("hw", "pwm_deinit: slice=%u channel=%u callback=%u", pwm->slice,
               pwm->channel, pwm->callback != NULL);
    sys_debugf("hw", "pwm_deinit: wrap=%u div=%u.%u level=%u enabled=%u",
               _hw_pwm_get_wrap(pwm->slice), div_raw >> 4, div_raw & 0x0F,
               _hw_pwm_get_level(pwm->slice, pwm->channel),
               _hw_pwm_get_enabled(pwm->slice));
  } else {
    sys_debugf("hw", "pwm_deinit: invalid");
  }
#endif

  if (!pwm) {
    return;
  }

  _hw_pwm_init_locks();

  // Disable PWM output
  pwm_set_enabled(pwm->slice, false);

  // Disable IRQ and clear callback
  pwm_set_irq_enabled(pwm->slice, false);

  uint32_t save = spin_lock_blocking(_hw_pwm_callback_lock);
  pwm->callback = NULL;
  pwm->userdata = NULL;
  spin_unlock(_hw_pwm_callback_lock, save);

  uint32_t save2 = spin_lock_blocking(_hw_pwm_pool_lock);
  pwm->initialized = false;
  spin_unlock(_hw_pwm_pool_lock, save2);
}

bool hw_pwm_valid(const hw_pwm_t *pwm) {
  if (!pwm) {
    return false;
  }
  if (pwm->slice >= NUM_PWM_SLICES) {
    return false;
  }
  if (pwm->channel >= 2) {
    return false;
  }
  return pwm->initialized;
}

///////////////////////////////////////////////////////////////////////////////
// CONFIGURATION

bool hw_pwm_set_period_ns(hw_pwm_t *pwm, uint64_t period_ns) {
  if (!hw_pwm_valid(pwm)) {
    return false;
  }

  uint16_t new_wrap;
  float new_divider = _hw_pwm_period_ns_to_divider(period_ns, &new_wrap);

  // Update PWM config
  pwm_config pico_config = pwm_get_default_config();
  pwm_config_set_wrap(&pico_config, new_wrap);
  pwm_config_set_clkdiv(&pico_config, new_divider);
  pwm_config_set_clkdiv_mode(&pico_config, PWM_DIV_FREE_RUNNING);
  pwm_init(pwm->slice, &pico_config, _hw_pwm_get_enabled(pwm->slice));

  return true;
}

uint64_t hw_pwm_get_period_ns(const hw_pwm_t *pwm) {
  if (!hw_pwm_valid(pwm)) {
    return 0;
  }
  uint16_t wrap = _hw_pwm_get_wrap(pwm->slice);
  float divider = _hw_pwm_get_divider(pwm->slice);
  return _hw_pwm_divider_wrap_to_period_ns(divider, wrap);
}

bool hw_pwm_set_duty_percent(hw_pwm_t *pwm, float duty_percent) {
  if (!hw_pwm_valid(pwm)) {
    return false;
  }

  uint16_t wrap = _hw_pwm_get_wrap(pwm->slice);

  uint16_t level;
  if (duty_percent <= 0.0f) {
    level = 0;
  } else if (duty_percent >= 100.0f) {
    level = wrap;
  } else {
    uint64_t level_calc =
        (uint64_t)(wrap + 1) * (uint64_t)(duty_percent * 10000.0f);
    level = (uint16_t)(level_calc / (100 * 10000));
  }

  pwm_set_chan_level(pwm->slice, pwm->channel, level);

  return true;
}

float hw_pwm_get_duty_percent(const hw_pwm_t *pwm) {
  if (!hw_pwm_valid(pwm)) {
    return 0.0f;
  }
  uint16_t wrap = _hw_pwm_get_wrap(pwm->slice);
  uint16_t level = _hw_pwm_get_level(pwm->slice, pwm->channel);
  return (float)level / (float)(wrap + 1) * 100.0f;
}

bool hw_pwm_set_config(hw_pwm_t *pwm, const hw_pwm_config_t *config) {
  if (!config || !hw_pwm_valid(pwm)) {
    return false;
  }

  if (!hw_pwm_set_period_ns(pwm, config->period_ns)) {
    return false;
  }
  if (!hw_pwm_set_duty_percent(pwm, config->duty_percent)) {
    return false;
  }

  if (config->enabled != _hw_pwm_get_enabled(pwm->slice)) {
    hw_pwm_set_enabled(pwm, config->enabled);
  }

  return true;
}

bool hw_pwm_get_config(const hw_pwm_t *pwm, hw_pwm_config_t *out_config) {
  if (!hw_pwm_valid(pwm) || !out_config) {
    return false;
  }

  out_config->period_ns = hw_pwm_get_period_ns(pwm);
  out_config->duty_percent = hw_pwm_get_duty_percent(pwm);
  out_config->enabled = _hw_pwm_get_enabled(pwm->slice);

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// CONTROL

void hw_pwm_set_enabled(hw_pwm_t *pwm, bool enabled) {
  if (!hw_pwm_valid(pwm)) {
    return;
  }

  pwm_set_enabled(pwm->slice, enabled);
}

bool hw_pwm_get_enabled(const hw_pwm_t *pwm) {
  if (!hw_pwm_valid(pwm)) {
    return false;
  }
  return _hw_pwm_get_enabled(pwm->slice);
}

///////////////////////////////////////////////////////////////////////////////
// INTERRUPTS

bool hw_pwm_irq_supported(void) { return true; }

static void _hw_pwm_set_callback(hw_pwm_t *pwm, hw_pwm_callback_t callback,
                                 void *userdata) {
  if (!hw_pwm_valid(pwm)) {
    return;
  }

  _hw_pwm_init_locks();

  uint32_t save = spin_lock_blocking(_hw_pwm_callback_lock);

  // Set the callback and userdata
  pwm->callback = callback;
  pwm->userdata = userdata;

  // Enable or disable IRQ based on whether a callback is set
  if (callback != NULL) {
    // Enable IRQ if setting a callback
    if (!_hw_pwm_irq_initialized) {
      _hw_pwm_irq_initialized = true;
      irq_set_exclusive_handler(PWM_IRQ_WRAP, _hw_pwm_callback_handler);
      irq_set_enabled(PWM_IRQ_WRAP, true);
    }
    pwm_set_irq_enabled(pwm->slice, true);
  } else {
    // Disable IRQ if clearing the callback
    pwm_set_irq_enabled(pwm->slice, false);
  }

  spin_unlock(_hw_pwm_callback_lock, save);
}

///////////////////////////////////////////////////////////////////////////////
// INTERNAL INTERRUPT HANDLER

static void _hw_pwm_callback_handler(void) {
  uint32_t status = pwm_get_irq_status_mask();
  for (uint8_t slice = 0; slice < NUM_PWM_SLICES; slice++) {
    if (status & (1u << slice)) {
      pwm_clear_irq(slice);
      // Invoke callbacks for both channels if they exist
      for (uint8_t ch = 0; ch < 2; ch++) {
        uint8_t idx = _hw_pwm_handle_index(slice, ch);
        hw_pwm_t *pwm = &_hw_pwm_pool[idx];

        uint32_t save = spin_lock_blocking(_hw_pwm_callback_lock);
        if (pwm->initialized && pwm->callback) {
          hw_pwm_callback_t cb = pwm->callback;
          void *userdata = pwm->userdata;
          spin_unlock(_hw_pwm_callback_lock, save);
          cb(pwm, userdata);
        } else {
          spin_unlock(_hw_pwm_callback_lock, save);
        }
      }
    }
  }
}

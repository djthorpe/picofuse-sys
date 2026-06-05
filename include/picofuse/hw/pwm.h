/**
 * @file pwm.h
 * @brief PWM (Pulse Width Modulation) interface
 * @defgroup PWM PWM
 * @ingroup Hardware
 *
 * Pulse Width Modulation (PWM) interface for hardware platforms.
 * This module provides functions to initialize PWM outputs, configure period
 * and duty cycle, and control output state.
 */
#pragma once
#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque PWM handle.
 * @ingroup PWM
 * @headerfile pwm.h hw/hw.h
 */
typedef struct hw_pwm_t hw_pwm_t;

/**
 * @brief PWM configuration.
 * @ingroup PWM
 *
 * When `NULL` is passed to @ref hw_pwm_init, backend defaults are used.
 */
typedef struct {
  uint64_t period_ns; ///< Requested PWM period in nanoseconds.
  float duty_percent; ///< Requested duty cycle percentage in [0.0, 100.0].
  bool enabled;       ///< Initial output enable state.
} hw_pwm_config_t;

/**
 * @brief PWM wrap callback function pointer.
 * @ingroup PWM
 *
 * Backends that support PWM wrap interrupts can invoke this callback when a
 * PWM period completes.
 */
typedef void (*hw_pwm_callback_t)(hw_pwm_t *pwm, void *userdata);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize a PWM output on a GPIO pin.
 * @ingroup PWM
 * @param gpio GPIO handle for a PWM-capable pin.
 * @param config Optional PWM configuration. Pass `NULL` to use defaults.
 * @return PWM handle or NULL on failure.
 */
hw_pwm_t *hw_pwm_init(hw_gpio_t *gpio, const hw_pwm_config_t *config);

/**
 * @brief Initialize a PWM output from a platform-specific device path.
 * @ingroup PWM
 * @param device Device identifier such as `/sys/class/pwm/pwmchip0/pwm0`.
 * @param config Optional PWM configuration. Pass `NULL` to use defaults.
 * @return PWM handle or NULL on failure.
 *
 * This entry point is intended for platforms where PWM endpoints are exposed
 * as device paths instead of being discovered from a GPIO mapping.
 */
hw_pwm_t *hw_pwm_init_device(const char *device, const hw_pwm_config_t *config);

/**
 * @brief Deinitialize a PWM handle.
 * @ingroup PWM
 * @param pwm PWM handle.
 */
void hw_pwm_deinit(hw_pwm_t *pwm);

/**
 * @brief Check whether a PWM handle is valid.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @retval true The PWM handle is valid.
 * @retval false The PWM handle is invalid.
 */
bool hw_pwm_valid(const hw_pwm_t *pwm);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// CONFIGURATION

/** @name Configuration
 * @{ */

/**
 * @brief Set the PWM period.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param period_ns PWM period in nanoseconds.
 * @retval true Period was accepted.
 * @retval false Period is unsupported or the handle is invalid.
 */
bool hw_pwm_set_period_ns(hw_pwm_t *pwm, uint64_t period_ns);

/**
 * @brief Get the configured PWM period.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @return PWM period in nanoseconds, or 0 on invalid handle.
 */
uint64_t hw_pwm_get_period_ns(const hw_pwm_t *pwm);

/**
 * @brief Set duty cycle percentage.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param duty_percent Duty cycle percentage in [0.0, 100.0]. Values outside
 * this range are clamped by the backend.
 * @retval true Duty cycle was applied.
 * @retval false The handle is invalid or duty control is unsupported.
 */
bool hw_pwm_set_duty_percent(hw_pwm_t *pwm, float duty_percent);

/**
 * @brief Get duty cycle percentage.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @return Duty cycle in [0.0, 100.0], or 0.0 on invalid handle.
 */
float hw_pwm_get_duty_percent(const hw_pwm_t *pwm);

/**
 * @brief Apply a complete PWM configuration.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param config PWM configuration to apply.
 * @retval true Configuration was applied.
 * @retval false Configuration failed.
 */
bool hw_pwm_set_config(hw_pwm_t *pwm, const hw_pwm_config_t *config);

/**
 * @brief Read current PWM configuration.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param out_config Output destination for current configuration.
 * @retval true Configuration was written to @p out_config.
 * @retval false Handle or output pointer is invalid.
 */
bool hw_pwm_get_config(const hw_pwm_t *pwm, hw_pwm_config_t *out_config);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// CONTROL

/** @name Control
 * @{ */

/**
 * @brief Enable or disable PWM output.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param enabled `true` to enable output, `false` to disable.
 */
void hw_pwm_set_enabled(hw_pwm_t *pwm, bool enabled);

/**
 * @brief Query whether PWM output is enabled.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @retval true PWM output is enabled.
 * @retval false PWM output is disabled or handle is invalid.
 */
bool hw_pwm_get_enabled(const hw_pwm_t *pwm);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// INTERRUPTS

/** @name Interrupts
 * @{ */

/**
 * @brief Check whether PWM wrap interrupt callbacks are supported.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @retval true Wrap interrupts are supported for this handle.
 * @retval false Wrap interrupts are unsupported.
 */
bool hw_pwm_irq_supported(const hw_pwm_t *pwm);

/**
 * @brief Set a PWM wrap callback handler.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param callback Callback invoked on wrap events, or `NULL` to disable.
 * @param userdata User context pointer forwarded to @p callback.
 */
void hw_pwm_set_callback(hw_pwm_t *pwm, hw_pwm_callback_t callback,
                         void *userdata);

/**
 * @brief Enable or disable PWM wrap interrupts.
 * @ingroup PWM
 * @param pwm PWM handle.
 * @param enabled `true` to enable wrap interrupts, `false` to disable.
 */
void hw_pwm_set_irq_enabled(hw_pwm_t *pwm, bool enabled);

/** @} */

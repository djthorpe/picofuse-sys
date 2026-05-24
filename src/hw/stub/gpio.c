#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_gpio_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a GPIO pin with the specified mode.
 */
hw_gpio_t *hw_gpio_init(uint8_t bank, uint8_t pin, hw_gpio_mode_t mode) {
  (void)bank;
  (void)pin;
  (void)mode;
  return NULL;
}

/**
 * @brief Deinitialize and release a GPIO pin.
 */
void hw_gpio_deinit(hw_gpio_t *gpio) {
  (void)gpio;
  // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Validate the GPIO pin.
 */
bool hw_gpio_valid(const hw_gpio_t *gpio) {
  (void)gpio;
  return false; // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Get the logical pin number for a GPIO handle.
 */
uint8_t hw_gpio_get_pin_num(const hw_gpio_t *gpio) {
  sys_assert(hw_gpio_valid(gpio));
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Set the global GPIO interrupt callback handler.
 */
void hw_gpio_set_callback(hw_gpio_callback_t callback, void *userdata) {
  (void)callback;
  (void)userdata;
  // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Get the total number of available GPIO pins for a given bank.
 */
uint8_t hw_gpio_count(uint8_t bank) {
  (void)bank;
  // No-op stub implementation for unsupported platforms.
  return 0;
}

/**
 * @brief Get the current mode configuration of a GPIO pin.
 */
hw_gpio_mode_t hw_gpio_get_mode(const hw_gpio_t *gpio) {
  (void)gpio;
  return HW_GPIO_UNKNOWN;
}

/**
 * @brief Set the current mode configuration of a GPIO pin.
 */
void hw_gpio_set_mode(hw_gpio_t *gpio, hw_gpio_mode_t mode) {
  (void)gpio;
  (void)mode;
  // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Read the current state of a GPIO pin.
 */
bool hw_gpio_get(const hw_gpio_t *gpio) {
  (void)gpio;
  return false; // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Set the state of a GPIO pin.
 */
void hw_gpio_set(hw_gpio_t *gpio, bool value) {
  (void)gpio;
  (void)value;
  // No-op stub implementation for unsupported platforms.
}

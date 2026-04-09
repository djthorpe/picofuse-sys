#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <picofuse-sys/hw.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_gpio_t {
  uint8_t bank;
  uint8_t pin;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

struct hw_gpio_t pins[MAX_GPIO_BANKS * MAX_GPIO_PINS];

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static void _hw_gpio_callback(uint pin, uint32_t events);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a GPIO pin with the specified mode.
 */
hw_gpio_t *hw_gpio_init(uint8_t bank, uint8_t pin, hw_gpio_mode_t mode) {}

/**
 * @brief Deinitialize and release a GPIO pin.
 */
void hw_gpio_deinit(hw_gpio_t *gpio) {}

/**
 * @brief Validate the GPIO pin.
 */
bool hw_gpio_valid(const hw_gpio_t *gpio) {}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Set the global GPIO interrupt callback handler.
 * @ingroup GPIO
 *
 * @param callback Pointer to the callback function, or `NULL` to disable
 * interrupt handling.
 * @param userdata User-defined data pointer to pass to the callback.
 */
void hw_gpio_set_callback(hw_gpio_callback_t callback, void *userdata) {}

/**
 * @brief Get the total number of available GPIO pins for a given bank.
 */
uint8_t hw_gpio_count(uint8_t bank) {
  // Pico only supports bank 0
  if (bank != 0) {
    return 0;
  }

  int max = NUM_BANK0_GPIOS;
  if (max > HW_GPIO_MAX_COUNT) {
    max = HW_GPIO_MAX_COUNT;
  }
  return max;
}

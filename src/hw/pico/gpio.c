#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <picofuse-sys/hw.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_gpio_t {
  uint8_t bank;
  uint8_t pin;
  hw_gpio_mode_t mode;
  bool value;
  bool initialized;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_gpio_t pins[NUM_BANK0_GPIOS];
static hw_gpio_callback_t gpio_callback;
static void *gpio_callback_userdata;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static void _hw_gpio_callback(uint pin, uint32_t events);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a GPIO pin with the specified mode.
 */
hw_gpio_t *hw_gpio_init(uint8_t bank, uint8_t pin, hw_gpio_mode_t mode) {
  if (bank != 0 || pin >= NUM_BANK0_GPIOS) {
    return NULL;
  }

  pins[pin].bank = bank;
  pins[pin].pin = pin;
  pins[pin].mode = mode;
  pins[pin].value = false;
  pins[pin].initialized = true;

  return &pins[pin];
}

/**
 * @brief Deinitialize and release a GPIO pin.
 */
void hw_gpio_deinit(hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return;
  }

  gpio->mode = HW_GPIO_NONE;
  gpio->value = false;
  gpio->initialized = false;
}

/**
 * @brief Validate the GPIO pin.
 */
bool hw_gpio_valid(const hw_gpio_t *gpio) {
  return gpio && gpio->initialized && gpio->bank == 0 &&
         gpio->pin < NUM_BANK0_GPIOS;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Set the global GPIO interrupt callback handler.
 */
void hw_gpio_set_callback(hw_gpio_callback_t callback, void *userdata) {
  gpio_callback = callback;
  gpio_callback_userdata = userdata;
}

/**
 * @brief Get the total number of available GPIO pins for a given bank.
 */
uint8_t hw_gpio_count(uint8_t bank) {
  // Pico only supports bank 0
  if (bank != 0) {
    return 0;
  }

  if (NUM_BANK0_GPIOS > UINT8_MAX) {
    return UINT8_MAX;
  }

  return (uint8_t)NUM_BANK0_GPIOS;
}

/**
 * @brief Get the current mode configuration of a GPIO pin.
 */
hw_gpio_mode_t hw_gpio_get_mode(const hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return HW_GPIO_UNKNOWN;
  }

  return gpio->mode;
}

/**
 * @brief Set the current mode configuration of a GPIO pin.
 */
void hw_gpio_set_mode(hw_gpio_t *gpio, hw_gpio_mode_t mode) {
  if (!hw_gpio_valid(gpio)) {
    return;
  }

  gpio->mode = mode;
}

/**
 * @brief Read the current state of a GPIO pin.
 */
bool hw_gpio_get(const hw_gpio_t *gpio) {
  if (!hw_gpio_valid(gpio)) {
    return false;
  }

  return gpio->value;
}

/**
 * @brief Set the state of a GPIO pin.
 */
void hw_gpio_set(hw_gpio_t *gpio, bool value) {
  if (!hw_gpio_valid(gpio)) {
    return;
  }

  gpio->value = value;
}

static void _hw_gpio_callback(uint pin, uint32_t events) {
  hw_gpio_event_t event = 0;

  if (events & GPIO_IRQ_EDGE_RISE) {
    event |= HW_GPIO_RISING;
  }
  if (events & GPIO_IRQ_EDGE_FALL) {
    event |= HW_GPIO_FALLING;
  }

  if (gpio_callback && pin < NUM_BANK0_GPIOS && event != 0) {
    gpio_callback(0, (uint8_t)pin, event, gpio_callback_userdata);
  }
}

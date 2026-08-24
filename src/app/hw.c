#include <picofuse/hw.h>

// Weak fallbacks used when picofuse-hw is not linked into the final binary.
// The real implementation defines strong symbols of the same names, which
// the linker prefers over these whenever it is present in the same link.
//
// hw_init()/hw_exit()/hw_poll() are called directly by app.c. The
// hw_gpio_*() functions are not called by app.c at all, but picofuse-hid
// now PUBLICLY depends on picofuse-hw (its GPIO-backed HID device support,
// src/hid/any/gpio.c, calls them unconditionally), so these particular
// fallbacks are unreachable while picofuse-hid is linked; they only matter
// for a picofuse-app build with neither picofuse-hw nor picofuse-hid.

__attribute__((weak)) void hw_init(void) {}

__attribute__((weak)) void hw_exit(void) {}

__attribute__((weak)) void hw_poll(void) {}

__attribute__((weak)) hw_watchdog_t *hw_watchdog_init(void) { return NULL; }

__attribute__((weak)) void hw_watchdog_deinit(hw_watchdog_t *watchdog) {
  (void)watchdog;
}

__attribute__((weak)) void hw_watchdog_enable(hw_watchdog_t *watchdog,
                                              bool enable) {
  (void)watchdog;
  (void)enable;
}

__attribute__((weak)) hw_gpio_t *hw_gpio_init(uint8_t bank, uint8_t pin,
                                              hw_gpio_mode_t mode) {
  (void)bank;
  (void)pin;
  (void)mode;
  return NULL;
}

__attribute__((weak)) void hw_gpio_deinit(hw_gpio_t *gpio) { (void)gpio; }

__attribute__((weak)) bool hw_gpio_valid(const hw_gpio_t *gpio) {
  (void)gpio;
  return false;
}

__attribute__((weak)) void hw_gpio_set_callback(hw_gpio_callback_t callback,
                                                void *userdata) {
  (void)callback;
  (void)userdata;
}

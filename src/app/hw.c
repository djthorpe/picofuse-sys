#include <picofuse/hw.h>

// Weak fallbacks used when picofuse-hw-obj is not linked into the final
// binary. The real implementation defines strong symbols of the same
// names, which the linker prefers over these whenever it is present in
// the same link.
//
// hw_init()/hw_exit()/hw_poll() are called directly by app.c. The
// hw_gpio_*() functions are not called by app.c at all; they are pulled in
// transitively by picofuse-hid-obj's GPIO-backed HID device support
// (src/hid/any/gpio.c, src/hid/any/device.c), so HID can also be linked
// without HW.

__attribute__((weak)) void hw_init(void) {}

__attribute__((weak)) void hw_exit(void) {}

__attribute__((weak)) void hw_poll(void) {}

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

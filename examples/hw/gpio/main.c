/**
 * @file
 * @brief GPIO example.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define EXAMPLE_GPIO_PIN 23

/**
 * @brief Configure a GPIO pin as an output, drive it high, and stop the system.
 *
 * This example shows the minimum sequence for taking control of a pin with
 * @c hw_gpio_init, driving it with @c hw_gpio_set, and then using
 * @c sys_halt to leave the board in a stable state.
 */
int main(void) {
  hw_gpio_t *led = hw_gpio_init(0, EXAMPLE_GPIO_PIN, HW_GPIO_OUTPUT);
  if (led != NULL) {
    hw_gpio_set(led, true);
  }
  sys_halt();
}

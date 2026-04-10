#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define EXAMPLE_GPIO_PIN 23

int main(void) {
  hw_gpio_t *led = hw_gpio_init(0, EXAMPLE_GPIO_PIN, HW_GPIO_OUTPUT);
  if (led != NULL) {
    hw_gpio_set(led, true);
  }

  sys_halt();
}

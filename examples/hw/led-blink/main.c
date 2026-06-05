/**
 * @file
 * @brief Blink the default LED (or all default NeoPixels) every second.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define BLINK_PERIOD_MS 1000u

int main(void) {
  sys_init();
  hw_init();

  hw_led_type_t led_type = HW_LED_TYPE_NONE;
  uint8_t led_count = 0;
  uint8_t led_pin = hw_led_gpio_default(&led_type, &led_count);

  if (led_type == HW_LED_TYPE_NONE || led_pin == HW_LED_GPIO_NONE) {
    sys_printf("No default LED available on this platform\n");
    hw_exit();
    sys_exit();
    return 0;
  }

  hw_led_t *led = hw_led_init_default();
  if (led == NULL) {
    sys_printf("Failed to initialize default LED\n");
    hw_exit();
    sys_exit();
    return 1;
  }

  uint8_t count = led_count > 0 ? led_count : 1u;
  for (uint8_t i = 0; i < count; i++) {
    if (!hw_led_blink(led, i, BLINK_PERIOD_MS, true)) {
      sys_printf("Failed to start blink on LED index %u\n", i);
    }
  }

  // Sleep for 10 seconds while blinking, then clean up and exit.
  sys_sleep_ms(10000);

  hw_led_deinit(led);
  hw_exit();
  sys_exit();
}

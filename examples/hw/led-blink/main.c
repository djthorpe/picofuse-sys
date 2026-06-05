/**
 * @file
 * @brief Exercise the default LED and verify NeoPixel chain on/off behavior.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define BLINK_PERIOD_MS 1000u
#define STEP_PERIOD_MS 250u

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
  if (led_type == HW_LED_TYPE_NEOPIXEL) {
    sys_printf("NeoPixel test on pin %u with %u pixels\n", led_pin, count);

    // Step each pixel on then off so chain indexing is easy to verify.
    for (uint8_t i = 0; i < count; i++) {
      if (!hw_led_set(led, i, true)) {
        sys_printf("Failed to set LED index %u on\n", i);
      }
      sys_sleep_ms(STEP_PERIOD_MS);

      if (!hw_led_set(led, i, false)) {
        sys_printf("Failed to set LED index %u off\n", i);
      }
      sys_sleep_ms(STEP_PERIOD_MS);
    }

  } else {
    if (!hw_led_blink(led, 0, BLINK_PERIOD_MS, true)) {
      sys_printf("Failed to start blink on default LED\n");
    }
    sys_sleep_ms(10000);
  }

  hw_led_deinit(led);
  hw_exit();
  sys_exit();
}

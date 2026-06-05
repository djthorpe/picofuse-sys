#include <pico.h>
#include <picofuse/hw.h>

uint8_t hw_led_gpio_default(void) {
#ifdef PICO_DEFAULT_LED_PIN
  return (uint8_t)PICO_DEFAULT_LED_PIN;
#else
  return HW_LED_GPIO_NONE;
#endif
}

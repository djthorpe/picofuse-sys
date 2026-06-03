#include <test.h>

bool test_main(void) {
  TestAssert(hw_gpio_count(1) == 0,
             "hw_gpio_count(1) should be 0 for unsupported bank, got %u",
             hw_gpio_count(1));

  TestAssert(hw_gpio_init(1, 0, HW_GPIO_OUTPUT) == NULL,
             "hw_gpio_init should reject unsupported bank 1");

  hw_gpio_deinit(NULL);
  TestAssert(!hw_gpio_valid(NULL), "NULL GPIO handle should be invalid");

#ifdef SYSTEM_NAME_PICO
  TestAssert(hw_gpio_count(0) > 0, "hw_gpio_count(0) should expose Pico GPIOs");

  hw_gpio_t *gpio = hw_gpio_init(0, 0, HW_GPIO_OUTPUT);
  TestAssert(gpio != NULL, "hw_gpio_init should create GPIO 0 output handle");
  TestAssert(hw_gpio_valid(gpio), "GPIO 0 handle should be valid");
  TestAssert(hw_gpio_get_pin_num(gpio) == 0,
             "GPIO handle should report pin 0, got %u",
             hw_gpio_get_pin_num(gpio));
  TestAssert(hw_gpio_get_mode(gpio) == HW_GPIO_OUTPUT,
             "GPIO 0 should start in output mode, got %d",
             hw_gpio_get_mode(gpio));

  hw_gpio_set(gpio, true);
  TestAssert(hw_gpio_get(gpio), "GPIO 0 should read high after setting high");

  hw_gpio_set(gpio, false);
  TestAssert(!hw_gpio_get(gpio), "GPIO 0 should read low after setting low");

  hw_gpio_set_mode(gpio, HW_GPIO_INPUT);
  TestAssert(hw_gpio_get_mode(gpio) == HW_GPIO_INPUT,
             "GPIO 0 should switch to input mode, got %d",
             hw_gpio_get_mode(gpio));

  hw_gpio_deinit(gpio);
  TestAssert(!hw_gpio_valid(gpio),
             "GPIO handle should be invalid after deinit");
#else
  TestAssert(hw_gpio_count(0) == 0, "stub hw_gpio_count(0) should be 0, got %u",
             hw_gpio_count(0));

  hw_gpio_t *gpio = hw_gpio_init(0, 0, HW_GPIO_OUTPUT);
  TestAssert(gpio == NULL,
             "stub hw_gpio_init should return NULL for GPIO 0 output");
#endif

  return true;
}

TestMain(test_main)
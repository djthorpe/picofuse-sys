#include <test.h>

bool test_main(void) {
  hw_adc_deinit(NULL);

  TestAssert(hw_adc_gpio_channel(NULL) == UINT8_MAX,
             "NULL GPIO should not map to an ADC channel");
  TestAssert(hw_adc_gpio_pin(UINT8_MAX) == UINT8_MAX,
             "invalid ADC channel should not map to a GPIO pin");
  TestAssert(!hw_adc_valid(NULL), "NULL ADC handle should be invalid");

#ifdef SYSTEM_NAME_PICO
  uint8_t count = hw_adc_count();
  TestAssert(count > 0, "hw_adc_count should expose Pico ADC channels");
  TestAssert(hw_adc_gpio_pin(count - 1) == UINT8_MAX,
             "temperature channel should not map to a GPIO pin");

  for (uint8_t channel = 0; channel < (uint8_t)(count - 1); channel++) {
    uint8_t pin = hw_adc_gpio_pin(channel);
    TestAssert(pin != UINT8_MAX, "ADC channel %u should map to a GPIO pin",
               channel);

    hw_gpio_t *gpio = hw_gpio_init(0, pin, HW_GPIO_INPUT);
    TestAssert(gpio != NULL, "GPIO %u should initialize for ADC test", pin);
    TestAssert(hw_adc_gpio_channel(gpio) == channel,
               "GPIO %u should map back to ADC channel %u, got %u", pin,
               channel, hw_adc_gpio_channel(gpio));

    hw_adc_t *adc = hw_adc_init_pin(gpio);
    TestAssert(adc != NULL, "ADC init should succeed for GPIO %u", pin);
    TestAssert(hw_adc_valid(adc), "ADC handle should be valid for GPIO %u",
               pin);
    TestAssert(hw_gpio_get_mode(gpio) == HW_GPIO_ADC,
               "GPIO %u should switch to ADC mode, got %d", pin,
               hw_gpio_get_mode(gpio));

    hw_adc_deinit(adc);
    TestAssert(!hw_adc_valid(adc),
               "ADC handle should be invalid after deinit for channel %u",
               channel);
    hw_gpio_deinit(gpio);
  }

  hw_adc_t *temp = hw_adc_init_temperature();
  TestAssert(temp != NULL, "temperature ADC init should succeed");
  TestAssert(hw_adc_valid(temp),
             "temperature ADC handle should be valid after init");
  hw_adc_deinit(temp);
  TestAssert(!hw_adc_valid(temp),
             "temperature ADC handle should be invalid after deinit");
#else
  TestAssert(hw_adc_count() == 0, "stub hw_adc_count should be 0, got %u",
             hw_adc_count());
  TestAssert(hw_adc_gpio_pin(0) == UINT8_MAX,
             "stub ADC channel 0 should not map to a GPIO pin");
  TestAssert(hw_adc_init_pin(NULL) == NULL,
             "stub ADC init should fail for NULL GPIO");
  TestAssert(hw_adc_init_temperature() == NULL,
             "stub temperature ADC init should fail");
#endif

  return true;
}

TestMain(test_main)
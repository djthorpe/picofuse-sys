#include <test.h>

bool test_main(void) {
  hw_i2c_deinit(NULL);

  TestAssert(!hw_i2c_valid(NULL), "NULL I2C handle should be invalid");

#ifdef SYSTEM_NAME_PICO
  TestAssert(hw_i2c_count() == 2, "Pico should expose 2 I2C adapters, got %u",
             hw_i2c_count());

#if defined(PICO_DEFAULT_I2C_SDA_PIN) && defined(PICO_DEFAULT_I2C_SCL_PIN)
  hw_i2c_t *default_i2c = hw_i2c_init_default(100000);
  TestAssert(default_i2c != NULL,
             "Pico default I2C init should succeed on default pins");
  TestAssert(hw_i2c_valid(default_i2c),
             "Pico default I2C handle should be valid after init");
  hw_i2c_deinit(default_i2c);
  TestAssert(!hw_i2c_valid(default_i2c),
             "Pico default I2C handle should be invalid after deinit");
#endif

  hw_gpio_t *sda0 = hw_gpio_init(0, 0, HW_GPIO_INPUT);
  hw_gpio_t *scl0 = hw_gpio_init(0, 1, HW_GPIO_INPUT);
  hw_gpio_t *bad_scl = hw_gpio_init(0, 3, HW_GPIO_INPUT);

  TestAssert(sda0 != NULL, "GPIO 0 should initialize for I2C SDA test");
  TestAssert(scl0 != NULL, "GPIO 1 should initialize for I2C SCL test");
  TestAssert(bad_scl != NULL,
             "GPIO 3 should initialize for invalid I2C mapping test");

  TestAssert(hw_i2c_init(2, sda0, scl0, 100000) == NULL,
             "I2C init should reject out-of-range adapter index");
  TestAssert(hw_i2c_init(0, sda0, bad_scl, 100000) == NULL,
             "I2C init should reject mismatched SDA/SCL pin mapping");
  TestAssert(hw_i2c_init(0, sda0, scl0, 0) == NULL,
             "I2C init should reject zero baud rate");

  hw_i2c_t *i2c = hw_i2c_init(0, sda0, scl0, 100000);
  TestAssert(i2c != NULL, "I2C0 should initialize on GPIO 0/1");
  TestAssert(hw_i2c_valid(i2c), "I2C handle should be valid after init");
  TestAssert(hw_gpio_get_mode(sda0) == HW_GPIO_I2C,
             "SDA pin should switch to I2C mode, got %d",
             hw_gpio_get_mode(sda0));
  TestAssert(hw_gpio_get_mode(scl0) == HW_GPIO_I2C,
             "SCL pin should switch to I2C mode, got %d",
             hw_gpio_get_mode(scl0));

  TestAssert(!hw_i2c_detect(i2c, 0x00),
             "I2C detect should reject reserved address 0x00");
  TestAssert(hw_i2c_xfr(i2c, 0x00, NULL, 0, 0, 0) == 0,
             "I2C transfer should reject reserved address 0x00");
  TestAssert(hw_i2c_read(i2c, 0x00, 0x00, &i2c, 1, 0) == 0,
             "I2C read should reject reserved address 0x00");
  TestAssert(hw_i2c_write(i2c, 0x00, 0x00, &i2c, 1, 0) == 0,
             "I2C write should reject reserved address 0x00");

  hw_i2c_deinit(i2c);
  TestAssert(!hw_i2c_valid(i2c), "I2C handle should be invalid after deinit");

  hw_gpio_deinit(sda0);
  hw_gpio_deinit(scl0);
  hw_gpio_deinit(bad_scl);
#elif defined(SYSTEM_NAME_LINUX)
  TestAssert(hw_i2c_init(0, NULL, NULL, 100000) == NULL,
             "Linux indexed I2C init should be unsupported");
  TestAssert(hw_i2c_init_device(NULL, 100000) == NULL,
             "Linux device-path init should reject NULL device");
  TestAssert(hw_i2c_init_device("", 100000) == NULL,
             "Linux device-path init should reject empty device path");
  TestAssert(hw_i2c_init_device("/dev/definitely-not-an-i2c-device", 100000) ==
                 NULL,
             "Linux device-path init should fail for missing device");
  TestAssert(!hw_i2c_detect(NULL, 0x40),
             "Linux I2C detect should fail for NULL handle");
  TestAssert(hw_i2c_xfr(NULL, 0x40, NULL, 0, 0, 0) == 0,
             "Linux I2C transfer should return 0 for NULL handle");
  TestAssert(hw_i2c_read(NULL, 0x40, 0x00, NULL, 0, 0) == 0,
             "Linux I2C read should return 0 for NULL handle");
  TestAssert(hw_i2c_write(NULL, 0x40, 0x00, NULL, 0, 0) == 0,
             "Linux I2C write should return 0 for NULL handle");
#else
  TestAssert(hw_i2c_count() == 0, "stub hw_i2c_count should be 0, got %u",
             hw_i2c_count());
  TestAssert(hw_i2c_init_default(100000) == NULL,
             "stub default I2C init should fail");
  TestAssert(hw_i2c_init(0, NULL, NULL, 100000) == NULL,
             "stub indexed I2C init should fail");
  TestAssert(hw_i2c_init_device("/dev/i2c-0", 100000) == NULL,
             "stub device-path I2C init should fail");
  TestAssert(!hw_i2c_detect(NULL, 0x40),
             "stub I2C detect should fail for NULL handle");
  TestAssert(hw_i2c_xfr(NULL, 0x40, NULL, 0, 0, 0) == 0,
             "stub I2C transfer should return 0");
  TestAssert(hw_i2c_read(NULL, 0x40, 0x00, NULL, 0, 0) == 0,
             "stub I2C read should return 0");
  TestAssert(hw_i2c_write(NULL, 0x40, 0x00, NULL, 0, 0) == 0,
             "stub I2C write should return 0");
#endif

  return true;
}

TestMain(test_main)
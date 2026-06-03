#include <test.h>

bool test_main(void) {
  hw_spi_deinit(NULL);

  TestAssert(!hw_spi_valid(NULL), "NULL SPI handle should be invalid");

#ifdef SYSTEM_NAME_PICO
  TestAssert(hw_spi_count() == 2, "Pico should expose 2 SPI adapters, got %u",
             hw_spi_count());

  hw_spi_config_t default_config = {
      .cs_active_low = false,
      .mode = HW_SPI_MODE_0,
      .bits_per_word = 8,
  };

  hw_spi_t *default_spi = hw_spi_init_default(100000, &default_config);
  TestAssert(default_spi != NULL,
             "Pico default SPI init should succeed on default pins");
  TestAssert(hw_spi_valid(default_spi),
             "Pico default SPI handle should be valid after init");
  hw_spi_deinit(default_spi);
  TestAssert(!hw_spi_valid(default_spi),
             "Pico default SPI handle should be invalid after deinit");

  hw_gpio_t *sck0 = hw_gpio_init(0, PICO_DEFAULT_SPI_SCK_PIN, HW_GPIO_INPUT);
  hw_gpio_t *tx0 = hw_gpio_init(0, PICO_DEFAULT_SPI_TX_PIN, HW_GPIO_INPUT);
  hw_gpio_t *rx0 = hw_gpio_init(0, PICO_DEFAULT_SPI_RX_PIN, HW_GPIO_INPUT);
#ifdef PICO_DEFAULT_SPI_CSN_PIN
  hw_gpio_t *cs0 = hw_gpio_init(0, PICO_DEFAULT_SPI_CSN_PIN, HW_GPIO_INPUT);
#else
  hw_gpio_t *cs0 = NULL;
#endif

  TestAssert(sck0 != NULL, "SPI SCK pin should initialize for Pico test");
  TestAssert(tx0 != NULL, "SPI MOSI pin should initialize for Pico test");
  TestAssert(rx0 != NULL, "SPI MISO pin should initialize for Pico test");

  hw_spi_config_t active_low_mode3 = {
      .cs_active_low = true,
      .mode = HW_SPI_MODE_3,
      .bits_per_word = 8,
  };

  TestAssert(hw_spi_init(2, sck0, tx0, rx0, cs0, 100000, &active_low_mode3) ==
                 NULL,
             "SPI init should reject out-of-range adapter index");

  hw_spi_t *spi =
      hw_spi_init(0, sck0, tx0, rx0, cs0, 100000, &active_low_mode3);
  TestAssert(spi != NULL, "SPI0 should initialize on Pico default pins");
  TestAssert(hw_spi_valid(spi), "SPI handle should be valid after init");
  TestAssert(hw_gpio_get_mode(sck0) == HW_GPIO_SPI,
             "SCK pin should switch to SPI mode, got %d",
             hw_gpio_get_mode(sck0));
  TestAssert(hw_gpio_get_mode(tx0) == HW_GPIO_SPI,
             "MOSI pin should switch to SPI mode, got %d",
             hw_gpio_get_mode(tx0));
  TestAssert(hw_gpio_get_mode(rx0) == HW_GPIO_SPI,
             "MISO pin should switch to SPI mode, got %d",
             hw_gpio_get_mode(rx0));
  if (cs0 != NULL) {
    TestAssert(hw_gpio_get_mode(cs0) == HW_GPIO_OUTPUT,
               "CS pin should switch to output mode, got %d",
               hw_gpio_get_mode(cs0));
    TestAssert(hw_gpio_get(cs0),
               "CS pin should idle high when configured active-low");
  }

  TestAssert(hw_spi_xfr(spi, NULL, 0, 0, 0) == 0,
             "SPI transfer should return 0 for empty transfer");
  TestAssert(hw_spi_read(spi, 0x00, NULL, 0, 0) == 0,
             "SPI read should return 0 for zero-length read");
  TestAssert(hw_spi_write(spi, 0x00, NULL, 0, 0) == 0,
             "SPI write should return 0 for zero-length write");

  hw_spi_deinit(spi);
  TestAssert(!hw_spi_valid(spi), "SPI handle should be invalid after deinit");
  TestAssert(!hw_gpio_valid(sck0),
             "SCK pin should be invalid after SPI deinit");
  TestAssert(!hw_gpio_valid(tx0),
             "MOSI pin should be invalid after SPI deinit");
  TestAssert(!hw_gpio_valid(rx0),
             "MISO pin should be invalid after SPI deinit");
  if (cs0 != NULL) {
    TestAssert(!hw_gpio_valid(cs0),
               "CS pin should be invalid after SPI deinit");
  }
#elif defined(SYSTEM_NAME_LINUX)
  TestAssert(hw_spi_count() == 0, "Linux hw_spi_count should be 0, got %u",
             hw_spi_count());
  TestAssert(hw_spi_init_default(100000, NULL) == NULL,
             "Linux default SPI init should be unsupported");
  TestAssert(hw_spi_init(0, NULL, NULL, NULL, NULL, 100000, NULL) == NULL,
             "Linux indexed SPI init should be unsupported");
  TestAssert(hw_spi_init_device(NULL, 100000, NULL) == NULL,
             "Linux device-path SPI init should reject NULL device");
  TestAssert(hw_spi_init_device("", 100000, NULL) == NULL,
             "Linux device-path SPI init should reject empty device path");
  TestAssert(hw_spi_init_device("/dev/definitely-not-a-spi-device", 100000,
                                NULL) == NULL,
             "Linux device-path SPI init should fail for missing device");
  TestAssert(hw_spi_xfr(NULL, NULL, 0, 0, 0) == 0,
             "Linux SPI transfer should return 0 for NULL handle");
  TestAssert(hw_spi_read(NULL, 0x00, NULL, 0, 0) == 0,
             "Linux SPI read should return 0 for NULL handle");
  TestAssert(hw_spi_write(NULL, 0x00, NULL, 0, 0) == 0,
             "Linux SPI write should return 0 for NULL handle");
#else
  TestAssert(hw_spi_count() == 0, "stub hw_spi_count should be 0, got %u",
             hw_spi_count());
  TestAssert(hw_spi_init_default(100000, NULL) == NULL,
             "stub default SPI init should fail");
  TestAssert(hw_spi_init(0, NULL, NULL, NULL, NULL, 100000, NULL) == NULL,
             "stub indexed SPI init should fail");
  TestAssert(hw_spi_init_device("/dev/spidev0.0", 100000, NULL) == NULL,
             "stub device-path SPI init should fail");
  TestAssert(hw_spi_xfr(NULL, NULL, 0, 0, 0) == 0,
             "stub SPI transfer should return 0");
  TestAssert(hw_spi_read(NULL, 0x00, NULL, 0, 0) == 0,
             "stub SPI read should return 0");
  TestAssert(hw_spi_write(NULL, 0x00, NULL, 0, 0) == 0,
             "stub SPI write should return 0");
#endif

  return true;
}

TestMain(test_main)
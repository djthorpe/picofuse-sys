#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_spi_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_spi_t *hw_spi_init_default(uint32_t baud_rate,
                              const hw_spi_config_t *config) {
  sys_debugf("spi_init_default: unsupported on this platform (baud=%u)",
             baud_rate);
  (void)config;
  (void)baud_rate;
  return NULL;
}

hw_spi_t *hw_spi_init(uint8_t index, hw_gpio_t *sck_pin, hw_gpio_t *tx_pin,
                      hw_gpio_t *rx_pin, hw_gpio_t *cs_pin, uint32_t baud_rate,
                      const hw_spi_config_t *config) {
  sys_debugf("spi_init: unsupported on this platform (index=%u baud=%u)", index,
             baud_rate);
  (void)index;
  (void)sck_pin;
  (void)tx_pin;
  (void)rx_pin;
  (void)cs_pin;
  (void)config;
  (void)baud_rate;
  return NULL;
}

hw_spi_t *hw_spi_init_device(const char *device, uint32_t baud_rate,
                             const hw_spi_config_t *config) {
  sys_debugf(
      "spi_init_device: unsupported on this platform (device=%s baud=%u)",
      device != NULL ? device : "(null)", baud_rate);
  (void)device;
  (void)config;
  (void)baud_rate;
  return NULL;
}

void hw_spi_deinit(hw_spi_t *spi) {
  sys_debugf("spi_deinit: spi=%p unsupported on this platform", spi);
  (void)spi;
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

uint8_t hw_spi_count(void) { return 0; }

bool hw_spi_valid(const hw_spi_t *spi) {
  (void)spi;
  return false;
}

uint8_t hw_spi_get_bits_per_word(const hw_spi_t *spi) {
  (void)spi;
  return 0u;
}

bool hw_spi_set_format(hw_spi_t *spi, hw_spi_mode_t mode,
                       uint8_t bits_per_word) {
  (void)spi;
  (void)mode;
  (void)bits_per_word;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

size_t hw_spi_xfr(hw_spi_t *spi, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms) {
  (void)spi;
  (void)data;
  (void)tx;
  (void)rx;
  (void)timeout_ms;
  return 0;
}

size_t hw_spi_read(hw_spi_t *spi, uint8_t reg, void *data, size_t len,
                   uint32_t timeout_ms) {
  (void)spi;
  (void)reg;
  (void)data;
  (void)len;
  (void)timeout_ms;
  return 0;
}

size_t hw_spi_write(hw_spi_t *spi, uint8_t reg, const void *data, size_t len,
                    uint32_t timeout_ms) {
  (void)spi;
  (void)reg;
  (void)data;
  (void)len;
  (void)timeout_ms;
  return 0;
}

size_t hw_spi_write_words(hw_spi_t *spi, const uint16_t *words, size_t len,
                          uint32_t timeout_ms) {
  (void)spi;
  (void)words;
  (void)len;
  (void)timeout_ms;
  return 0;
}
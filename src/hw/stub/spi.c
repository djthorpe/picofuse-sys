#include <picofuse/hw.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_spi_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_spi_t *hw_spi_init_default(bool cs_active_low, uint32_t baud_rate) {
  (void)cs_active_low;
  (void)baud_rate;
  return NULL;
}

hw_spi_t *hw_spi_init(uint8_t index, hw_gpio_t *sck_pin, hw_gpio_t *tx_pin,
                      hw_gpio_t *rx_pin, hw_gpio_t *cs_pin, bool cs_active_low,
                      uint32_t baud_rate) {
  (void)index;
  (void)sck_pin;
  (void)tx_pin;
  (void)rx_pin;
  (void)cs_pin;
  (void)cs_active_low;
  (void)baud_rate;
  return NULL;
}

hw_spi_t *hw_spi_init_device(const char *device, bool cs_active_low,
                             uint32_t baud_rate) {
  (void)device;
  (void)cs_active_low;
  (void)baud_rate;
  return NULL;
}

void hw_spi_deinit(hw_spi_t *spi) { (void)spi; }

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

uint8_t hw_spi_count(void) { return 0; }

bool hw_spi_valid(const hw_spi_t *spi) {
  (void)spi;
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
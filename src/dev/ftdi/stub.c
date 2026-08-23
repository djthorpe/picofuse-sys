#include <picofuse/dev/ftdi.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_ftdi_t {
  bool _unused;
};

struct dev_ftdi_iterator_t {
  bool _unused;
};

///////////////////////////////////////////////////////////////////////////////
// ENUMERATION

const hw_usb_device_t *dev_ftdi_next(dev_ftdi_iterator_t **iterator) {
  if (iterator != NULL) {
    *iterator = NULL;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_ftdi_t *dev_ftdi_init(const char *serial) {
  (void)serial;
  sys_debugf("[ftdi] unsupported on this platform");
  return NULL;
}

void dev_ftdi_deinit(dev_ftdi_t *ftdi) { (void)ftdi; }

///////////////////////////////////////////////////////////////////////////////
// SPI

bool dev_ftdi_spi_init(dev_ftdi_t *ftdi, uint8_t cs_pin, uint32_t baud_rate,
                       const hw_spi_config_t *config) {
  (void)ftdi;
  (void)cs_pin;
  (void)baud_rate;
  (void)config;
  return false;
}

size_t dev_ftdi_spi_xfr(dev_ftdi_t *ftdi, void *data, size_t tx, size_t rx,
                        uint32_t timeout_ms) {
  (void)ftdi;
  (void)data;
  (void)tx;
  (void)rx;
  (void)timeout_ms;
  return 0u;
}

///////////////////////////////////////////////////////////////////////////////
// I2C

bool dev_ftdi_i2c_init(dev_ftdi_t *ftdi, uint32_t baud_rate) {
  (void)ftdi;
  (void)baud_rate;
  return false;
}

size_t dev_ftdi_i2c_xfr(dev_ftdi_t *ftdi, uint8_t addr, void *data, size_t tx,
                        size_t rx, uint32_t timeout_ms) {
  (void)ftdi;
  (void)addr;
  (void)data;
  (void)tx;
  (void)rx;
  (void)timeout_ms;
  return 0u;
}

///////////////////////////////////////////////////////////////////////////////
// GPIO

bool dev_ftdi_gpio_init(dev_ftdi_t *ftdi, uint16_t output_mask) {
  (void)ftdi;
  (void)output_mask;
  return false;
}

bool dev_ftdi_gpio_read(dev_ftdi_t *ftdi, uint16_t *value) {
  (void)ftdi;
  (void)value;
  return false;
}

bool dev_ftdi_gpio_write(dev_ftdi_t *ftdi, uint16_t value) {
  (void)ftdi;
  (void)value;
  return false;
}

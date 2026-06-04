#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_i2c_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_i2c_t *hw_i2c_init_default(uint32_t baud_rate) {
  sys_debugf("i2c_init_default: unsupported on this platform (baud=%u)",
             baud_rate);
  (void)baud_rate;
  return NULL;
}

hw_i2c_t *hw_i2c_init(uint8_t index, hw_gpio_t *sda_pin, hw_gpio_t *scl_pin,
                      uint32_t baud_rate) {
  sys_debugf("i2c_init: unsupported on this platform (index=%u baud=%u)", index,
             baud_rate);
  (void)index;
  (void)sda_pin;
  (void)scl_pin;
  (void)baud_rate;
  return NULL;
}

hw_i2c_t *hw_i2c_init_device(const char *device, uint32_t baud_rate) {
  sys_debugf(
      "i2c_init_device: unsupported on this platform (device=%s baud=%u)",
      device != NULL ? device : "(null)", baud_rate);
  (void)device;
  (void)baud_rate;
  return NULL;
}

void hw_i2c_deinit(hw_i2c_t *i2c) {
  sys_debugf("i2c_deinit: i2c=%p unsupported on this platform", i2c);
  (void)i2c;
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

uint8_t hw_i2c_count(void) { return 0; }

bool hw_i2c_valid(const hw_i2c_t *i2c) {
  (void)i2c;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool hw_i2c_detect(hw_i2c_t *i2c, uint8_t addr) {
  (void)i2c;
  (void)addr;
  return false;
}

size_t hw_i2c_xfr(hw_i2c_t *i2c, uint8_t addr, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms) {
  (void)i2c;
  (void)addr;
  (void)data;
  (void)tx;
  (void)rx;
  (void)timeout_ms;
  return 0;
}

size_t hw_i2c_read(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, void *data,
                   size_t len, uint32_t timeout_ms) {
  (void)i2c;
  (void)addr;
  (void)reg;
  (void)data;
  (void)len;
  (void)timeout_ms;
  return 0;
}

size_t hw_i2c_write(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, const void *data,
                    size_t len, uint32_t timeout_ms) {
  (void)i2c;
  (void)addr;
  (void)reg;
  (void)data;
  (void)len;
  (void)timeout_ms;
  return 0;
}
#include <hardware/i2c.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_i2c_t {
  i2c_inst_t *instance;
  hw_gpio_t *sda_pin;
  hw_gpio_t *scl_pin;
  uint32_t baud_rate;
  bool owns_pins;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_i2c_t _hw_i2c_instances[NUM_I2CS] = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _hw_i2c_valid_addr(uint8_t addr) {
  if (addr < 0x08 || addr > 0x77) {
    return false;
  }

  // Reserved address ranges 0000xxx and 1111xxx are not valid slave IDs.
  return (addr & 0x78u) != 0x00u && (addr & 0x78u) != 0x78u;
}

static bool _hw_i2c_signal_pin_matches_index(const hw_gpio_t *pin,
                                             uint8_t index,
                                             uint8_t signal_offset) {
  if (!hw_gpio_valid(pin) || index >= hw_i2c_count()) {
    return false;
  }

  uint8_t pin_num = hw_gpio_get_pin_num(pin);
  return (pin_num & 0x3u) == (uint8_t)(index * 2u + signal_offset);
}

static int _hw_i2c_write(i2c_inst_t *instance, uint8_t addr,
                         const uint8_t *data, size_t len, bool nostop,
                         uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    return i2c_write_blocking(instance, addr, data, len, nostop);
  }

  return i2c_write_timeout_us(instance, addr, data, len, nostop,
                              timeout_ms * 1000u);
}

static int _hw_i2c_read(i2c_inst_t *instance, uint8_t addr, uint8_t *data,
                        size_t len, bool nostop, uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    return i2c_read_blocking(instance, addr, data, len, nostop);
  }

  return i2c_read_timeout_us(instance, addr, data, len, nostop,
                             timeout_ms * 1000u);
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_i2c_count(void) {
  if (NUM_I2CS > UINT8_MAX) {
    return UINT8_MAX;
  }

  return (uint8_t)NUM_I2CS;
}

hw_i2c_t *hw_i2c_init_default(uint32_t baud_rate) {
#if defined(PICO_DEFAULT_I2C) && defined(PICO_DEFAULT_I2C_SDA_PIN) &&          \
    defined(PICO_DEFAULT_I2C_SCL_PIN)
  hw_gpio_t *sda_pin = hw_gpio_init(0, PICO_DEFAULT_I2C_SDA_PIN, HW_GPIO_I2C);
  if (sda_pin == NULL) {
    return NULL;
  }

  hw_gpio_t *scl_pin = hw_gpio_init(0, PICO_DEFAULT_I2C_SCL_PIN, HW_GPIO_I2C);
  if (scl_pin == NULL) {
    hw_gpio_deinit(sda_pin);
    return NULL;
  }

  hw_i2c_t *i2c = hw_i2c_init(PICO_DEFAULT_I2C, sda_pin, scl_pin, baud_rate);
  if (i2c == NULL) {
    hw_gpio_deinit(sda_pin);
    hw_gpio_deinit(scl_pin);
    return NULL;
  }

  i2c->owns_pins = true;
  return i2c;
#else
  (void)baud_rate;
  return NULL;
#endif
}

hw_i2c_t *hw_i2c_init(uint8_t index, hw_gpio_t *sda_pin, hw_gpio_t *scl_pin,
                      uint32_t baud_rate) {
  if (index >= hw_i2c_count() || !hw_gpio_valid(sda_pin) ||
      !hw_gpio_valid(scl_pin) || baud_rate == 0) {
    return NULL;
  }

  if (!_hw_i2c_signal_pin_matches_index(sda_pin, index, 0) ||
      !_hw_i2c_signal_pin_matches_index(scl_pin, index, 1)) {
    return NULL;
  }

  hw_i2c_t *i2c = &_hw_i2c_instances[index];
  if (hw_i2c_valid(i2c)) {
    hw_i2c_deinit(i2c);
  }

  hw_gpio_mode_t sda_mode = hw_gpio_get_mode(sda_pin);
  hw_gpio_mode_t scl_mode = hw_gpio_get_mode(scl_pin);

  hw_gpio_set_mode(sda_pin, HW_GPIO_I2C);
  hw_gpio_set_mode(scl_pin, HW_GPIO_I2C);

  i2c_inst_t *instance = i2c_get_instance(index);
  uint32_t actual_baud_rate = i2c_init(instance, baud_rate);
  if (actual_baud_rate == 0) {
    hw_gpio_set_mode(sda_pin, sda_mode);
    hw_gpio_set_mode(scl_pin, scl_mode);
    return NULL;
  }

  i2c->instance = instance;
  i2c->sda_pin = sda_pin;
  i2c->scl_pin = scl_pin;
  i2c->baud_rate = actual_baud_rate;
  i2c->owns_pins = false;
  i2c->init = true;
  return i2c;
}

hw_i2c_t *hw_i2c_init_device(const char *device, uint32_t baud_rate) {
  (void)device;
  (void)baud_rate;
  return NULL;
}

void hw_i2c_deinit(hw_i2c_t *i2c) {
  if (!hw_i2c_valid(i2c)) {
    return;
  }

  i2c_deinit(i2c->instance);

  if (i2c->owns_pins) {
    hw_gpio_deinit(i2c->sda_pin);
    hw_gpio_deinit(i2c->scl_pin);
  }

  sys_memset(i2c, 0, sizeof(*i2c));
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

bool hw_i2c_valid(const hw_i2c_t *i2c) {
  return i2c != NULL && i2c->init && i2c->instance != NULL &&
         i2c->baud_rate > 0;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool hw_i2c_detect(hw_i2c_t *i2c, uint8_t addr) {
  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr)) {
    return false;
  }

  uint8_t dummy = 0;
  return _hw_i2c_write(i2c->instance, addr & 0x7Fu, &dummy, 0, false, 100) >= 0;
}

size_t hw_i2c_xfr(hw_i2c_t *i2c, uint8_t addr, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms) {
  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr) ||
      ((tx > 0 || rx > 0) && data == NULL)) {
    return 0;
  }

  uint8_t *bytes = data;

  if (tx > 0) {
    int ret = _hw_i2c_write(i2c->instance, addr & 0x7Fu, bytes, tx, rx > 0,
                            timeout_ms);
    if (ret != (int)tx) {
      return 0;
    }
  }

  if (rx > 0) {
    int ret = _hw_i2c_read(i2c->instance, addr & 0x7Fu, bytes + tx, rx, false,
                           timeout_ms);
    if (ret != (int)rx) {
      return 0;
    }
  }

  return tx + rx;
}

size_t hw_i2c_read(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, void *data,
                   size_t len, uint32_t timeout_ms) {
  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr) || data == NULL ||
      len == 0) {
    return 0;
  }

  if (_hw_i2c_write(i2c->instance, addr & 0x7Fu, &reg, sizeof(reg), true,
                    timeout_ms) != (int)sizeof(reg)) {
    return 0;
  }

  return _hw_i2c_read(i2c->instance, addr & 0x7Fu, data, len, false,
                      timeout_ms) == (int)len
             ? len
             : 0;
}

size_t hw_i2c_write(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, const void *data,
                    size_t len, uint32_t timeout_ms) {
  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr) ||
      (len > 0 && data == NULL)) {
    return 0;
  }

  if (len == SIZE_MAX) {
    return 0;
  }

  // Use a stack buffer for small writes to avoid heap allocation overhead
  size_t total_len = len + 1;
  uint8_t stack_buffer[32] = {0};
  uint8_t *buffer = stack_buffer;
  bool use_heap = total_len > sizeof(stack_buffer);
  if (use_heap) {
    buffer = sys_malloc(total_len);
    if (buffer == NULL) {
      return 0;
    }
  }

  buffer[0] = reg;
  if (len > 0) {
    sys_memcpy(buffer + 1, data, len);
  }

  int ret = _hw_i2c_write(i2c->instance, addr & 0x7Fu, buffer, total_len, false,
                          timeout_ms);
  if (use_heap) {
    sys_free(buffer);
  }

  return ret == (int)total_len ? len : 0;
}
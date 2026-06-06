#include <picofuse/dev.h>
#include <picofuse/sys.h>

#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define TCA9555_REG_INPUT0 0x00u
#define TCA9555_REG_OUTPUT0 0x02u
#define TCA9555_REG_POLARITY0 0x04u
#define TCA9555_REG_CONFIG0 0x06u

#define TCA9555_I2C_ADDR_MIN 0x20u
#define TCA9555_I2C_ADDR_MAX 0x27u

#define TCA9555_POLL_INTERVAL_MS 100u

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_tca9555_t {
  hw_i2c_t *i2c;
  uint8_t i2c_addr;
  uint16_t output_mask;
  uint16_t input_mask;
  sys_timer_t *timer;
  dev_tca9555_callback_t callback;
  void *callback_userdata;
  uint16_t last_input;
  bool has_last_input;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _dev_tca9555_valid_i2c_addr(uint8_t addr) {
  return addr >= TCA9555_I2C_ADDR_MIN && addr <= TCA9555_I2C_ADDR_MAX;
}

static bool _dev_tca9555_ready(const dev_tca9555_t *tca9555) {
  return tca9555 != NULL && tca9555->init && hw_i2c_valid(tca9555->i2c) &&
         _dev_tca9555_valid_i2c_addr(tca9555->i2c_addr);
}

static bool _dev_tca9555_read_u16(dev_tca9555_t *tca9555, uint8_t reg,
                                  uint16_t *value) {
  if (!_dev_tca9555_ready(tca9555) || value == NULL) {
    return false;
  }

  uint8_t data[2] = {0};
  if (hw_i2c_read(tca9555->i2c, tca9555->i2c_addr, reg, data, sizeof(data),
                  0u) != sizeof(data)) {
    return false;
  }

  *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  return true;
}

static bool _dev_tca9555_write_u16(dev_tca9555_t *tca9555, uint8_t reg,
                                   uint16_t value) {
  if (!_dev_tca9555_ready(tca9555)) {
    return false;
  }

  uint8_t data[2] = {(uint8_t)(value & 0xFFu), (uint8_t)((value >> 8) & 0xFFu)};
  return hw_i2c_write(tca9555->i2c, tca9555->i2c_addr, reg, data, sizeof(data),
                      0u) == sizeof(data);
}

static bool _dev_tca9555_probe_and_set_addr(dev_tca9555_t *tca9555,
                                            dev_tca9555_i2c_addr_t i2c_addr) {
  if (i2c_addr != DEV_TCA9555_I2C_ADDR_ANY) {
    uint8_t addr = (uint8_t)i2c_addr;
    if (!_dev_tca9555_valid_i2c_addr(addr) ||
        !hw_i2c_detect(tca9555->i2c, addr)) {
      return false;
    }
    tca9555->i2c_addr = addr;
    return true;
  }

  for (uint8_t addr = TCA9555_I2C_ADDR_MIN; addr <= TCA9555_I2C_ADDR_MAX;
       ++addr) {
    if (hw_i2c_detect(tca9555->i2c, addr)) {
      tca9555->i2c_addr = addr;
      return true;
    }
  }

  return false;
}

static void _dev_tca9555_timer_callback(sys_timer_t *timer) {
  dev_tca9555_t *tca9555 = (dev_tca9555_t *)sys_timer_get_userdata(timer);
  if (!_dev_tca9555_ready(tca9555) || tca9555->callback == NULL) {
    return;
  }

  uint16_t value = 0u;
  if (!_dev_tca9555_read_u16(tca9555, TCA9555_REG_INPUT0, &value)) {
    return;
  }

  uint16_t input_value = (uint16_t)(value & tca9555->input_mask);

  if (!tca9555->has_last_input) {
    tca9555->last_input = input_value;
    tca9555->has_last_input = true;
    return;
  }

  if (input_value != tca9555->last_input) {
    tca9555->last_input = input_value;
    tca9555->callback(tca9555, value, tca9555->callback_userdata);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_tca9555_t *dev_tca9555_init_i2c(hw_i2c_t *i2c,
                                    dev_tca9555_i2c_addr_t i2c_addr,
                                    uint16_t output_mask,
                                    dev_tca9555_callback_t callback,
                                    void *callback_userdata) {
  if (!hw_i2c_valid(i2c)) {
    return NULL;
  }

  dev_tca9555_t *tca9555 = sys_calloc(1u, sizeof(*tca9555));
  if (tca9555 == NULL) {
    return NULL;
  }

  tca9555->i2c = i2c;
  tca9555->output_mask = output_mask;
  tca9555->input_mask = (uint16_t)~output_mask;
  tca9555->callback = callback;
  tca9555->callback_userdata = callback_userdata;
  tca9555->init = true;

  if (!_dev_tca9555_probe_and_set_addr(tca9555, i2c_addr)) {
    dev_tca9555_deinit(tca9555);
    return NULL;
  }

  // Disable polarity inversion for all pins.
  if (!_dev_tca9555_write_u16(tca9555, TCA9555_REG_POLARITY0, 0x0000u)) {
    dev_tca9555_deinit(tca9555);
    return NULL;
  }

  // Initialize output latches low.
  if (!_dev_tca9555_write_u16(tca9555, TCA9555_REG_OUTPUT0, 0x0000u)) {
    dev_tca9555_deinit(tca9555);
    return NULL;
  }

  // Config register uses 1=input, 0=output.
  uint16_t direction = (uint16_t)~output_mask;
  if (!_dev_tca9555_write_u16(tca9555, TCA9555_REG_CONFIG0, direction)) {
    dev_tca9555_deinit(tca9555);
    return NULL;
  }

  if (tca9555->callback != NULL) {
    uint16_t value = 0u;
    if (!_dev_tca9555_read_u16(tca9555, TCA9555_REG_INPUT0, &value)) {
      dev_tca9555_deinit(tca9555);
      return NULL;
    }
    tca9555->last_input = (uint16_t)(value & tca9555->input_mask);
    tca9555->has_last_input = true;

    tca9555->timer = sys_timer_init(TCA9555_POLL_INTERVAL_MS, tca9555,
                                    _dev_tca9555_timer_callback);
    if (tca9555->timer == NULL || !sys_timer_start(tca9555->timer)) {
      dev_tca9555_deinit(tca9555);
      return NULL;
    }
  }

  return tca9555;
}

void dev_tca9555_deinit(dev_tca9555_t *tca9555) {
  if (tca9555 == NULL) {
    return;
  }

  if (tca9555->timer != NULL) {
    sys_timer_deinit(tca9555->timer);
    tca9555->timer = NULL;
  }

  sys_memset(tca9555, 0, sizeof(*tca9555));
  sys_free(tca9555);
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

uint8_t dev_tca9555_i2c_addr(const dev_tca9555_t *tca9555) {
  if (!_dev_tca9555_ready(tca9555)) {
    return 0u;
  }

  return tca9555->i2c_addr;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool dev_tca9555_read(dev_tca9555_t *tca9555, uint16_t *value) {
  return _dev_tca9555_read_u16(tca9555, TCA9555_REG_INPUT0, value);
}

bool dev_tca9555_write(dev_tca9555_t *tca9555, uint16_t value) {
  if (!_dev_tca9555_ready(tca9555)) {
    return false;
  }

  uint16_t current = 0u;
  if (!_dev_tca9555_read_u16(tca9555, TCA9555_REG_OUTPUT0, &current)) {
    return false;
  }

  uint16_t merged = (uint16_t)((current & (uint16_t)~tca9555->output_mask) |
                               (value & tca9555->output_mask));
  return _dev_tca9555_write_u16(tca9555, TCA9555_REG_OUTPUT0, merged);
}

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define HW_I2C_MAX_ADAPTERS 8

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_i2c_t {
  int fd;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_i2c_t _hw_i2c_instances[HW_I2C_MAX_ADAPTERS] = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _hw_i2c_valid_addr(uint8_t addr) {
  if (addr < 0x08 || addr > 0x77) {
    return false;
  }

  // Reserved address ranges 0000xxx and 1111xxx are not valid slave IDs.
  return (addr & 0x78u) != 0x00u && (addr & 0x78u) != 0x78u;
}

static bool _hw_i2c_set_slave_addr(int fd, uint8_t addr) {
  if (fd < 0 || !_hw_i2c_valid_addr(addr)) {
    return false;
  }

  return ioctl(fd, I2C_SLAVE, addr) >= 0;
}

static hw_i2c_t *_hw_i2c_alloc_slot(void) {
  for (size_t i = 0; i < HW_I2C_MAX_ADAPTERS; i++) {
    if (!_hw_i2c_instances[i].init) {
      return &_hw_i2c_instances[i];
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_i2c_count(void) { return 0; }

hw_i2c_t *hw_i2c_init_default(uint32_t baud_rate) {
  sys_debugf("hw", "i2c_init_default: baud=%u", baud_rate);
  (void)baud_rate;
  return NULL;
}

hw_i2c_t *hw_i2c_init(uint8_t index, hw_gpio_t *sda_pin, hw_gpio_t *scl_pin,
                      uint32_t baud_rate) {
  sys_debugf("hw", "i2c_init: index=%u sda=%p scl=%p baud=%u", index, sda_pin,
             scl_pin, baud_rate);
  (void)index;
  (void)sda_pin;
  (void)scl_pin;
  (void)baud_rate;
  return NULL;
}

hw_i2c_t *hw_i2c_init_device(const char *device, uint32_t baud_rate) {
  sys_debugf("hw", "i2c_init_device: device=%s baud=%u",
             device != NULL ? device : "(null)", baud_rate);
  if (device == NULL || device[0] == '\0' || baud_rate == 0) {
    return NULL;
  }

  hw_i2c_t *i2c = _hw_i2c_alloc_slot();
  if (i2c == NULL) {
    return NULL;
  }

  int fd = open(device, O_RDWR);
  if (fd < 0) {
    return NULL;
  }

  sys_memset(i2c, 0, sizeof(*i2c));
  i2c->fd = fd;
  i2c->init = true;
  return i2c;
}

void hw_i2c_deinit(hw_i2c_t *i2c) {
  sys_debugf("hw", "i2c_deinit: i2c=%p", i2c);
  if (!hw_i2c_valid(i2c)) {
    return;
  }

  close(i2c->fd);
  sys_memset(i2c, 0, sizeof(*i2c));
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

bool hw_i2c_valid(const hw_i2c_t *i2c) {
  return i2c != NULL && i2c->init && i2c->fd >= 0;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool hw_i2c_detect(hw_i2c_t *i2c, uint8_t addr) {
  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr) ||
      !_hw_i2c_set_slave_addr(i2c->fd, addr)) {
    return false;
  }

  struct i2c_smbus_ioctl_data args = {0};
  union i2c_smbus_data data = {0};

  args.read_write = I2C_SMBUS_READ;
  args.command = 0;
  args.size = I2C_SMBUS_BYTE;
  args.data = &data;

  return ioctl(i2c->fd, I2C_SMBUS, &args) >= 0;
}

size_t hw_i2c_xfr(hw_i2c_t *i2c, uint8_t addr, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr) ||
      ((tx > 0 || rx > 0) && data == NULL)) {
    return 0;
  }

  if (tx > UINT16_MAX || rx > UINT16_MAX) {
    return 0;
  }

  if (tx == 0 && rx == 0) {
    return 0;
  }

  struct i2c_msg messages[2] = {0};
  struct i2c_rdwr_ioctl_data request = {0};
  uint8_t *bytes = data;
  uint32_t message_count = 0;

  if (tx > 0) {
    messages[message_count].addr = addr;
    messages[message_count].flags = 0;
    messages[message_count].len = (uint16_t)tx;
    messages[message_count].buf = bytes;
    message_count++;
  }

  if (rx > 0) {
    messages[message_count].addr = addr;
    messages[message_count].flags = I2C_M_RD;
    messages[message_count].len = (uint16_t)rx;
    messages[message_count].buf = bytes + tx;
    message_count++;
  }

  request.msgs = messages;
  request.nmsgs = message_count;

  int ret = ioctl(i2c->fd, I2C_RDWR, &request);
  if (ret != (int)message_count) {
    return 0;
  }

  return tx + rx;
}

size_t hw_i2c_read(hw_i2c_t *i2c, uint8_t addr, uint8_t reg, void *data,
                   size_t len, uint32_t timeout_ms) {
  if (!hw_i2c_valid(i2c) || !_hw_i2c_valid_addr(addr) || data == NULL ||
      len == 0) {
    return 0;
  }

  if (len == SIZE_MAX) {
    return 0;
  }

  uint8_t *buffer = sys_malloc(len + 1);
  if (buffer == NULL) {
    return 0;
  }

  buffer[0] = reg;
  size_t transferred = hw_i2c_xfr(i2c, addr, buffer, 1, len, timeout_ms);
  if (transferred == len + 1) {
    sys_memcpy(data, buffer + 1, len);
    sys_free(buffer);
    return len;
  }

  sys_free(buffer);
  return 0;
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

  size_t transferred = hw_i2c_xfr(i2c, addr, buffer, total_len, 0, timeout_ms);
  if (use_heap) {
    sys_free(buffer);
  }

  return transferred == total_len ? len : 0;
}
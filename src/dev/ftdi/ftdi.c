#include <ftdi.h>
#include <libusb.h>
#include <picofuse/dev/ftdi.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef enum {
  _DEV_FTDI_MODE_NONE,
  _DEV_FTDI_MODE_SPI,
  _DEV_FTDI_MODE_GPIO,
} _dev_ftdi_mode_t;

struct dev_ftdi_t {
  struct ftdi_context *ctx;
  _dev_ftdi_mode_t mode;

  // Shadow of the MPSSE "set data bits" state, so GPIO writes and CS
  // toggling only ever change the bits they own.
  uint8_t low_dir;    // ADBUS0-7 direction (1 = output)
  uint8_t high_dir;   // ACBUS0-7 direction (1 = output)
  uint8_t low_value;  // ADBUS0-7 driven values
  uint8_t high_value; // ACBUS0-7 driven values

  // SPI mode state.
  uint8_t spi_cmd_write; // MPSSE command for a write-only byte transfer
  uint8_t spi_cmd_read;  // MPSSE command for a read-only byte transfer
  uint8_t spi_cmd_xfr;   // MPSSE command for a combined write+read transfer
  uint8_t spi_cs_pin;
  bool spi_cs_active_low;
};

struct dev_ftdi_iterator_t {
  struct ftdi_context *ctx;
  struct ftdi_device_list *list;
  struct ftdi_device_list *cursor;
  hw_usb_device_t current;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _dev_ftdi_streq(const char *a, const char *b) {
  size_t len_a = sys_strlen(a);
  size_t len_b = sys_strlen(b);
  return len_a == len_b && sys_memcmp(a, b, len_a) == 0;
}

static inline uint8_t _dev_ftdi_pin_bit(uint8_t pin) {
  return (uint8_t)(1u << (pin & 0x07u));
}

static inline bool _dev_ftdi_pin_is_high_byte(uint8_t pin) {
  return pin >= 8u;
}

static void _dev_ftdi_set_pin_dir(dev_ftdi_t *ftdi, uint8_t pin, bool output) {
  uint8_t bit = _dev_ftdi_pin_bit(pin);
  if (_dev_ftdi_pin_is_high_byte(pin)) {
    ftdi->high_dir = (uint8_t)(output ? (ftdi->high_dir | bit)
                                      : (ftdi->high_dir & (uint8_t)~bit));
  } else {
    ftdi->low_dir = (uint8_t)(output ? (ftdi->low_dir | bit)
                                     : (ftdi->low_dir & (uint8_t)~bit));
  }
}

static void _dev_ftdi_set_pin_value(dev_ftdi_t *ftdi, uint8_t pin,
                                    bool level) {
  uint8_t bit = _dev_ftdi_pin_bit(pin);
  if (_dev_ftdi_pin_is_high_byte(pin)) {
    ftdi->high_value = (uint8_t)(level ? (ftdi->high_value | bit)
                                       : (ftdi->high_value & (uint8_t)~bit));
  } else {
    ftdi->low_value = (uint8_t)(level ? (ftdi->low_value | bit)
                                      : (ftdi->low_value & (uint8_t)~bit));
  }
}

// Pushes the current low_value/low_dir/high_value/high_dir shadow to the
// device. Always writes both bytes, even if only one changed, to keep the
// call sites simple.
static bool _dev_ftdi_push(dev_ftdi_t *ftdi) {
  uint8_t buf[6] = {
      SET_BITS_LOW,  ftdi->low_value,  ftdi->low_dir,
      SET_BITS_HIGH, ftdi->high_value, ftdi->high_dir,
  };
  return ftdi_write_data(ftdi->ctx, buf, (int)sizeof(buf)) ==
         (int)sizeof(buf);
}

static bool _dev_ftdi_read_bits(dev_ftdi_t *ftdi, uint16_t *value) {
  uint8_t cmd[3] = {GET_BITS_LOW, GET_BITS_HIGH, SEND_IMMEDIATE};
  if (ftdi_write_data(ftdi->ctx, cmd, (int)sizeof(cmd)) != (int)sizeof(cmd)) {
    return false;
  }

  uint8_t resp[2];
  size_t received = 0u;
  int attempts = 0;
  while (received < sizeof(resp) && attempts < 16) {
    int n = ftdi_read_data(ftdi->ctx, resp + received,
                           (int)(sizeof(resp) - received));
    if (n < 0) {
      return false;
    }
    received += (size_t)n;
    attempts++;
  }

  if (received != sizeof(resp)) {
    return false;
  }

  *value = (uint16_t)(resp[0] | ((uint16_t)resp[1] << 8));
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ENUMERATION

const hw_usb_device_t *dev_ftdi_next(dev_ftdi_iterator_t **iterator) {
  if (iterator == NULL) {
    return NULL;
  }

  dev_ftdi_iterator_t *it = *iterator;
  if (it == NULL) {
    it = (dev_ftdi_iterator_t *)sys_calloc(1u, sizeof(*it));
    if (it == NULL) {
      return NULL;
    }

    it->ctx = ftdi_new();
    if (it->ctx == NULL) {
      sys_free(it);
      return NULL;
    }

    if (ftdi_usb_find_all(it->ctx, &it->list, 0, 0) < 0) {
      ftdi_free(it->ctx);
      sys_free(it);
      return NULL;
    }

    it->cursor = it->list;
    *iterator = it;
  }

  while (it->cursor != NULL) {
    struct libusb_device *dev = it->cursor->dev;
    it->cursor = it->cursor->next;

    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      continue; // Skip devices we can't describe.
    }

    sys_memset(&it->current, 0, sizeof(it->current));
    it->current.vid = desc.idVendor;
    it->current.pid = desc.idProduct;

    // Best-effort: leave strings empty rather than dropping an
    // otherwise-attached device if descriptors can't be read.
    ftdi_usb_get_strings(it->ctx, dev, it->current.manufacturer,
                         (int)sizeof(it->current.manufacturer),
                         it->current.product,
                         (int)sizeof(it->current.product),
                         it->current.serial,
                         (int)sizeof(it->current.serial));

    return &it->current;
  }

  ftdi_list_free(&it->list);
  ftdi_free(it->ctx);
  sys_free(it);
  *iterator = NULL;
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_ftdi_t *dev_ftdi_init(const char *serial) {
  struct ftdi_context *ctx = ftdi_new();
  if (ctx == NULL) {
    return NULL;
  }

  struct ftdi_device_list *list = NULL;
  if (ftdi_usb_find_all(ctx, &list, 0, 0) <= 0) {
    ftdi_list_free(&list);
    ftdi_free(ctx);
    return NULL;
  }

  struct libusb_device *match = NULL;
  for (struct ftdi_device_list *cur = list; cur != NULL; cur = cur->next) {
    if (serial == NULL) {
      match = cur->dev;
      break;
    }

    char found_serial[HW_USB_STRING_MAX_LENGTH + 1] = {0};
    if (ftdi_usb_get_strings(ctx, cur->dev, NULL, 0, NULL, 0, found_serial,
                             (int)sizeof(found_serial)) == 0 &&
        _dev_ftdi_streq(found_serial, serial)) {
      match = cur->dev;
      break;
    }
  }

  dev_ftdi_t *ftdi = NULL;
  if (match != NULL && ftdi_usb_open_dev(ctx, match) == 0) {
    ftdi = (dev_ftdi_t *)sys_calloc(1u, sizeof(*ftdi));
    if (ftdi == NULL) {
      ftdi_usb_close(ctx);
    } else {
      ftdi->ctx = ctx;
      ftdi->mode = _DEV_FTDI_MODE_NONE;
      ftdi_set_latency_timer(ctx, 1u); // best-effort; ignore failure
    }
  }

  ftdi_list_free(&list);

  if (ftdi == NULL) {
    ftdi_free(ctx);
  }

  return ftdi;
}

void dev_ftdi_deinit(dev_ftdi_t *ftdi) {
  if (ftdi == NULL) {
    return;
  }

  if (ftdi->ctx != NULL) {
    ftdi_usb_close(ftdi->ctx);
    ftdi_free(ftdi->ctx);
  }

  sys_free(ftdi);
}

///////////////////////////////////////////////////////////////////////////////
// SPI

bool dev_ftdi_spi_init(dev_ftdi_t *ftdi, uint8_t cs_pin, uint32_t baud_rate,
                       const hw_spi_config_t *config) {
  if (ftdi == NULL || ftdi->ctx == NULL || baud_rate == 0u) {
    return false;
  }

  // ADBUS0-2 are wired to SK/DO/DI by the MPSSE engine; cs_pin must be a
  // different pin.
  if (cs_pin <= 2u) {
    return false;
  }

  hw_spi_mode_t mode = HW_SPI_MODE_0;
  bool cs_active_low = true;
  if (config != NULL) {
    mode = config->mode;
    cs_active_low = config->cs_active_low;
    if (config->bits_per_word != 0u && config->bits_per_word != 8u) {
      return false; // Only byte-oriented transfers are supported.
    }
  }

  if (ftdi_set_bitmode(ftdi->ctx, 0, BITMODE_RESET) < 0) {
    return false;
  }
  if (ftdi_set_bitmode(ftdi->ctx, 0, BITMODE_MPSSE) < 0) {
    return false;
  }

  // MPSSE's internal clock is 60MHz on FT232H; the byte-shift clock divisor
  // command divides a fixed 30MHz (60MHz/2) reference by (1 + divisor).
  uint32_t base_clock = 30000000u;
  uint32_t divisor =
      (baud_rate >= base_clock) ? 0u : (base_clock / baud_rate) - 1u;
  if (divisor > 0xFFFFu) {
    divisor = 0xFFFFu;
  }

  uint8_t clkcfg[3] = {TCK_DIVISOR, (uint8_t)(divisor & 0xFFu),
                       (uint8_t)((divisor >> 8) & 0xFFu)};
  if (ftdi_write_data(ftdi->ctx, clkcfg, (int)sizeof(clkcfg)) !=
      (int)sizeof(clkcfg)) {
    return false;
  }

  // CPHA (mode bit 0) selects which clock edge data changes/is sampled on;
  // CPOL (mode bit 1) only sets SK's idle level, applied below.
  bool cpha = (mode == HW_SPI_MODE_1 || mode == HW_SPI_MODE_3);
  bool cpol = (mode == HW_SPI_MODE_2 || mode == HW_SPI_MODE_3);

  uint8_t write_neg = cpha ? 0u : MPSSE_WRITE_NEG;
  uint8_t read_neg = cpha ? MPSSE_READ_NEG : 0u;

  ftdi->spi_cmd_write = (uint8_t)(MPSSE_DO_WRITE | write_neg);
  ftdi->spi_cmd_read = (uint8_t)(MPSSE_DO_READ | read_neg);
  ftdi->spi_cmd_xfr =
      (uint8_t)(MPSSE_DO_WRITE | MPSSE_DO_READ | write_neg | read_neg);
  ftdi->spi_cs_pin = cs_pin;
  ftdi->spi_cs_active_low = cs_active_low;

  ftdi->low_dir = 0u;
  ftdi->high_dir = 0u;
  ftdi->low_value = 0u;
  ftdi->high_value = 0u;

  _dev_ftdi_set_pin_dir(ftdi, 0u, true);  // SK
  _dev_ftdi_set_pin_dir(ftdi, 1u, true);  // DO
  _dev_ftdi_set_pin_dir(ftdi, 2u, false); // DI
  _dev_ftdi_set_pin_dir(ftdi, cs_pin, true);

  _dev_ftdi_set_pin_value(ftdi, 0u, cpol);        // SK idle level
  _dev_ftdi_set_pin_value(ftdi, cs_pin, cs_active_low); // CS idle (deasserted)

  ftdi->mode = _DEV_FTDI_MODE_SPI;

  return _dev_ftdi_push(ftdi);
}

size_t dev_ftdi_spi_xfr(dev_ftdi_t *ftdi, void *data, size_t tx, size_t rx,
                        uint32_t timeout_ms) {
  if (ftdi == NULL || ftdi->ctx == NULL || ftdi->mode != _DEV_FTDI_MODE_SPI) {
    return 0u;
  }
  if ((tx == 0u && rx == 0u) || (data == NULL)) {
    return 0u;
  }

  size_t total = tx + rx;
  // MPSSE's byte-shift length field is 16-bit and encodes (N-1).
  if (total > 65536u) {
    return 0u;
  }

  uint8_t *bytes = (uint8_t *)data;
  uint8_t cmd = (tx > 0u && rx > 0u) ? ftdi->spi_cmd_xfr
                : (tx > 0u)          ? ftdi->spi_cmd_write
                                     : ftdi->spi_cmd_read;

  // Assert chip-select.
  _dev_ftdi_set_pin_value(ftdi, ftdi->spi_cs_pin, !ftdi->spi_cs_active_low);
  bool ok = _dev_ftdi_push(ftdi);

  if (ok) {
    bool will_write = (cmd & MPSSE_DO_WRITE) != 0u;
    size_t out_len = 3u /* cmd + length */ + (will_write ? total : 0u) +
                     1u /* SEND_IMMEDIATE */;
    uint8_t *out = (uint8_t *)sys_malloc(out_len);
    if (out == NULL) {
      ok = false;
    } else {
      size_t n = 0u;
      uint16_t len_field = (uint16_t)(total - 1u);
      out[n++] = cmd;
      out[n++] = (uint8_t)(len_field & 0xFFu);
      out[n++] = (uint8_t)((len_field >> 8) & 0xFFu);
      if (will_write) {
        if (tx > 0u) {
          sys_memcpy(out + n, bytes, tx);
        }
        if (rx > 0u) {
          // Dummy clock-fill bytes for the read phase of a combined
          // transfer; their value is irrelevant, only the clock matters.
          sys_memset(out + n + tx, 0, rx);
        }
        n += total;
      }
      out[n++] = SEND_IMMEDIATE;

      ok = ftdi_write_data(ftdi->ctx, out, (int)n) == (int)n;
      sys_free(out);
    }
  }

  bool timeout_overridden = false;
  int saved_timeout = 0;
  if (ok && timeout_ms != 0u) {
    saved_timeout = ftdi->ctx->usb_read_timeout;
    ftdi->ctx->usb_read_timeout = (int)timeout_ms;
    timeout_overridden = true;
  }

  if (ok && (cmd & MPSSE_DO_READ) != 0u) {
    uint8_t *resp = (uint8_t *)sys_malloc(total);
    if (resp == NULL) {
      ok = false;
    } else {
      size_t received = 0u;
      int attempts = 0;
      while (ok && received < total && attempts < 64) {
        int n = ftdi_read_data(ftdi->ctx, resp + received,
                               (int)(total - received));
        if (n < 0) {
          ok = false;
          break;
        }
        received += (size_t)n;
        attempts++;
      }
      ok = ok && received == total;
      if (ok && rx > 0u) {
        sys_memcpy(bytes + tx, resp + (total - rx), rx);
      }
      sys_free(resp);
    }
  }

  if (timeout_overridden) {
    ftdi->ctx->usb_read_timeout = saved_timeout;
  }

  // Release chip-select regardless of transfer outcome.
  _dev_ftdi_set_pin_value(ftdi, ftdi->spi_cs_pin, ftdi->spi_cs_active_low);
  _dev_ftdi_push(ftdi);

  return ok ? total : 0u;
}

///////////////////////////////////////////////////////////////////////////////
// GPIO

bool dev_ftdi_gpio_init(dev_ftdi_t *ftdi, uint16_t output_mask) {
  if (ftdi == NULL || ftdi->ctx == NULL) {
    return false;
  }

  if (ftdi_set_bitmode(ftdi->ctx, 0, BITMODE_RESET) < 0) {
    return false;
  }
  if (ftdi_set_bitmode(ftdi->ctx, 0, BITMODE_MPSSE) < 0) {
    return false;
  }

  ftdi->low_dir = (uint8_t)(output_mask & 0xFFu);
  ftdi->high_dir = (uint8_t)((output_mask >> 8) & 0xFFu);
  ftdi->low_value = 0u;
  ftdi->high_value = 0u;
  ftdi->mode = _DEV_FTDI_MODE_GPIO;

  return _dev_ftdi_push(ftdi);
}

bool dev_ftdi_gpio_read(dev_ftdi_t *ftdi, uint16_t *value) {
  if (ftdi == NULL || ftdi->ctx == NULL || value == NULL ||
      ftdi->mode != _DEV_FTDI_MODE_GPIO) {
    return false;
  }

  return _dev_ftdi_read_bits(ftdi, value);
}

bool dev_ftdi_gpio_write(dev_ftdi_t *ftdi, uint16_t value) {
  if (ftdi == NULL || ftdi->ctx == NULL || ftdi->mode != _DEV_FTDI_MODE_GPIO) {
    return false;
  }

  uint8_t low_mask = ftdi->low_dir;
  uint8_t high_mask = ftdi->high_dir;
  ftdi->low_value = (uint8_t)((ftdi->low_value & (uint8_t)~low_mask) |
                              ((uint8_t)(value & 0xFFu) & low_mask));
  ftdi->high_value =
      (uint8_t)((ftdi->high_value & (uint8_t)~high_mask) |
               ((uint8_t)((value >> 8) & 0xFFu) & high_mask));

  return _dev_ftdi_push(ftdi);
}

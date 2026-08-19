#include <picofuse/dev/uc8151.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define UC8151_PSR 0x00u
#define UC8151_PWR 0x01u
#define UC8151_POF 0x02u
#define UC8151_PFS 0x03u
#define UC8151_PON 0x04u
#define UC8151_BTST 0x06u
#define UC8151_DTM2 0x13u
#define UC8151_DSP 0x11u
#define UC8151_DRF 0x12u
#define UC8151_PLL 0x30u
#define UC8151_TSE 0x41u
#define UC8151_CDI 0x50u
#define UC8151_TCON 0x60u
#define UC8151_PTL 0x90u
#define UC8151_PTIN 0x91u
#define UC8151_PTOU 0x92u

#define UC8151_PSR_RES_96x230 0x00u
#define UC8151_PSR_RES_96x252 0x40u
#define UC8151_PSR_RES_128x296 0x80u
#define UC8151_PSR_RES_160x296 0xC0u
#define UC8151_PSR_LUT_OTP 0x00u
#define UC8151_PSR_FORMAT_BW 0x10u
#define UC8151_PSR_SCAN_DOWN 0x00u
#define UC8151_PSR_SCAN_UP 0x08u
#define UC8151_PSR_SHIFT_LEFT 0x00u
#define UC8151_PSR_SHIFT_RIGHT 0x04u
#define UC8151_PSR_BOOSTER_ON 0x02u
#define UC8151_PSR_RESET_NONE 0x01u

#define UC8151_PWR_VDS_INTERNAL 0x02u
#define UC8151_PWR_VDG_INTERNAL 0x01u
#define UC8151_PWR_VCOM_VD 0x00u
#define UC8151_PWR_VGHL_16V 0x00u

#define UC8151_BTST_START_10MS 0x00u
#define UC8151_BTST_STRENGTH_3 0x10u
#define UC8151_BTST_OFF_6_58US 0x07u

#define UC8151_PFS_FRAMES_1 0x00u
#define UC8151_TSE_TEMP_INTERNAL 0x00u
#define UC8151_PLL_HZ_100 0x3Au
#define UC8151_PLL_HZ_200 0x39u

#define UC8151_BUSY_POLL_MS 100u
#define UC8151_BUSY_TIMEOUT_MS 5000u

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_uc8151_t {
  hw_spi_t *spi;
  hw_gpio_t *dc_pin;
  hw_gpio_t *reset_pin;
  hw_gpio_t *busy_pin;
  uint16_t width;
  uint16_t height;
  dev_uc8151_rotation_t rotation;
  dev_uc8151_update_speed_t speed;
  bool inverted;
  bool blocking;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _dev_uc8151_reset(dev_uc8151_t *uc8151);
static bool _dev_uc8151_ready(const dev_uc8151_t *uc8151);

static bool _dev_uc8151_valid_rotation(dev_uc8151_rotation_t rotation) {
  return rotation == DEV_UC8151_ROTATION_0 ||
         rotation == DEV_UC8151_ROTATION_180;
}

static bool
_dev_uc8151_valid_update_speed(dev_uc8151_update_speed_t update_speed) {
  return update_speed <= DEV_UC8151_UPDATE_SPEED_TURBO;
}

static bool _dev_uc8151_send_command(dev_uc8151_t *uc8151, uint8_t command,
                                     const uint8_t *data, size_t data_len) {
  if (!_dev_uc8151_ready(uc8151)) {
    return false;
  }

  hw_gpio_set(uc8151->dc_pin, false);
  size_t tx = hw_spi_xfr(uc8151->spi, &command, 1u, 0u, 0u);
  if (tx != 1u) {
    sys_debugf("[uc8151] cmd 0x%02X failed (tx=%u)", (unsigned int)command,
               (unsigned int)tx);
    return false;
  }

  if (data != NULL && data_len > 0u) {
    hw_gpio_set(uc8151->dc_pin, true);
    tx = hw_spi_xfr(uc8151->spi, (void *)data, data_len, 0u, 0u);
    hw_gpio_set(uc8151->dc_pin, false);
    if (tx != data_len) {
      sys_debugf("[uc8151] cmd 0x%02X data failed (%u/%u)",
                 (unsigned int)command, (unsigned int)tx,
                 (unsigned int)data_len);
    }
    return tx == data_len;
  }

  return true;
}

static bool _dev_uc8151_send_data(dev_uc8151_t *uc8151, const uint8_t *data,
                                  size_t data_len) {
  if (!_dev_uc8151_ready(uc8151) || data == NULL || data_len == 0u) {
    return false;
  }

  hw_gpio_set(uc8151->dc_pin, true);
  size_t tx = hw_spi_xfr(uc8151->spi, (void *)data, data_len, 0u, 0u);
  hw_gpio_set(uc8151->dc_pin, false);
  return tx == data_len;
}

static bool _dev_uc8151_is_busy(const dev_uc8151_t *uc8151) {
  if (!_dev_uc8151_ready(uc8151)) {
    return false;
  }

  // UC8151 BUSY is active low on common Pico Inky boards.
  return !hw_gpio_get(uc8151->busy_pin);
}

static bool _dev_uc8151_wait_ready(dev_uc8151_t *uc8151) {
  if (!_dev_uc8151_ready(uc8151)) {
    return false;
  }

  uint32_t waited_ms = 0u;
  if (_dev_uc8151_is_busy(uc8151)) {
    sys_debugf("[uc8151] wait_ready start busy=%u",
               (unsigned int)hw_gpio_get(uc8151->busy_pin));
  }

  while (_dev_uc8151_is_busy(uc8151)) {
    if (waited_ms >= UC8151_BUSY_TIMEOUT_MS) {
      sys_debugf("[uc8151] wait_ready timeout busy=%u",
                 (unsigned int)hw_gpio_get(uc8151->busy_pin));
      return false;
    }

    sys_sleep_ms(UC8151_BUSY_POLL_MS);
    waited_ms += UC8151_BUSY_POLL_MS;
  }

  if (waited_ms > 0u) {
    sys_debugf("[uc8151] wait_ready done in %u ms", (unsigned int)waited_ms);
  }

  return true;
}

static uint8_t _dev_uc8151_psr_resolution_bits(const dev_uc8151_t *uc8151) {
  if (uc8151->height == 96u && uc8151->width == 230u) {
    return UC8151_PSR_RES_96x230;
  }
  if (uc8151->height == 96u && uc8151->width == 252u) {
    return UC8151_PSR_RES_96x252;
  }
  if (uc8151->height == 128u && uc8151->width == 296u) {
    return UC8151_PSR_RES_128x296;
  }
  if (uc8151->height == 160u && uc8151->width == 296u) {
    return UC8151_PSR_RES_160x296;
  }

  return 0xFFu;
}

static bool _dev_uc8151_setup(dev_uc8151_t *uc8151) {
  uint8_t resolution_bits = _dev_uc8151_psr_resolution_bits(uc8151);
  if (resolution_bits == 0xFFu) {
    sys_debugf("[uc8151] unsupported panel size %ux%u",
               (unsigned int)uc8151->width, (unsigned int)uc8151->height);
    return false;
  }

  sys_debugf("[uc8151] setup begin %ux%u rot=%u speed=%u inv=%u",
             (unsigned int)uc8151->width, (unsigned int)uc8151->height,
             (unsigned int)uc8151->rotation, (unsigned int)uc8151->speed,
             (unsigned int)uc8151->inverted);

  _dev_uc8151_reset(uc8151);

  uint8_t psr =
      (uint8_t)(resolution_bits | UC8151_PSR_LUT_OTP | UC8151_PSR_FORMAT_BW |
                UC8151_PSR_BOOSTER_ON | UC8151_PSR_RESET_NONE);
  if (uc8151->rotation == DEV_UC8151_ROTATION_180) {
    psr |= (uint8_t)(UC8151_PSR_SHIFT_LEFT | UC8151_PSR_SCAN_UP);
  } else {
    psr |= (uint8_t)(UC8151_PSR_SHIFT_RIGHT | UC8151_PSR_SCAN_DOWN);
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_PSR, &psr, 1u)) {
    return false;
  }

  const uint8_t pwr[] = {
      (uint8_t)(UC8151_PWR_VDS_INTERNAL | UC8151_PWR_VDG_INTERNAL),
      (uint8_t)(UC8151_PWR_VCOM_VD | UC8151_PWR_VGHL_16V),
      0x2Bu,
      0x2Bu,
      0x2Bu,
  };
  if (!_dev_uc8151_send_command(uc8151, UC8151_PWR, pwr, sizeof(pwr))) {
    return false;
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_PON, NULL, 0u) ||
      !_dev_uc8151_wait_ready(uc8151)) {
    return false;
  }

  const uint8_t btst[] = {
      (uint8_t)(UC8151_BTST_START_10MS | UC8151_BTST_STRENGTH_3 |
                UC8151_BTST_OFF_6_58US),
      (uint8_t)(UC8151_BTST_START_10MS | UC8151_BTST_STRENGTH_3 |
                UC8151_BTST_OFF_6_58US),
      (uint8_t)(UC8151_BTST_START_10MS | UC8151_BTST_STRENGTH_3 |
                UC8151_BTST_OFF_6_58US),
  };
  if (!_dev_uc8151_send_command(uc8151, UC8151_BTST, btst, sizeof(btst))) {
    return false;
  }

  const uint8_t pfs = UC8151_PFS_FRAMES_1;
  if (!_dev_uc8151_send_command(uc8151, UC8151_PFS, &pfs, 1u)) {
    return false;
  }

  const uint8_t tse = UC8151_TSE_TEMP_INTERNAL;
  if (!_dev_uc8151_send_command(uc8151, UC8151_TSE, &tse, 1u)) {
    return false;
  }

  const uint8_t tcon = 0x22u;
  if (!_dev_uc8151_send_command(uc8151, UC8151_TCON, &tcon, 1u)) {
    return false;
  }

  const uint8_t cdi = uc8151->inverted ? 0x9Cu : 0x4Cu;
  if (!_dev_uc8151_send_command(uc8151, UC8151_CDI, &cdi, 1u)) {
    return false;
  }

  uint8_t pll = UC8151_PLL_HZ_100;
  if (uc8151->speed == DEV_UC8151_UPDATE_SPEED_FAST ||
      uc8151->speed == DEV_UC8151_UPDATE_SPEED_TURBO) {
    pll = UC8151_PLL_HZ_200;
  }
  if (!_dev_uc8151_send_command(uc8151, UC8151_PLL, &pll, 1u)) {
    return false;
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_POF, NULL, 0u) ||
      !_dev_uc8151_wait_ready(uc8151)) {
    return false;
  }

  sys_debugf("[uc8151] setup complete");

  return true;
}

static bool _dev_uc8151_power_off(dev_uc8151_t *uc8151) {
  if (!_dev_uc8151_ready(uc8151)) {
    return false;
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_POF, NULL, 0u)) {
    return false;
  }

  return _dev_uc8151_wait_ready(uc8151);
}

static bool _dev_uc8151_validate_frame(const pix_frame_t *frame) {
  if (frame == NULL || frame->data == NULL || frame->fmt != PIX_FMT_MONO) {
    return false;
  }

  if (frame->size.w == 0u || frame->size.h == 0u) {
    return false;
  }

  size_t min_stride = ((size_t)frame->size.h + 7u) / 8u;
  return frame->stride >= min_stride;
}

static dev_uc8151_config_t
_dev_uc8151_resolve_config(const dev_uc8151_config_t *config) {
  dev_uc8151_config_t resolved = {
      .inverted = false,
      .blocking = true,
      .speed = DEV_UC8151_UPDATE_SPEED_DEFAULT,
      .rotation = DEV_UC8151_ROTATION_0,
  };

  if (config == NULL) {
    return resolved;
  }

  resolved.inverted = config->inverted;
  resolved.blocking = config->blocking;
  if (_dev_uc8151_valid_update_speed(config->speed)) {
    resolved.speed = config->speed;
  }
  if (_dev_uc8151_valid_rotation(config->rotation)) {
    resolved.rotation = config->rotation;
  }

  return resolved;
}

static void _dev_uc8151_reset(dev_uc8151_t *uc8151) {
  if (uc8151 == NULL || !hw_gpio_valid(uc8151->reset_pin)) {
    return;
  }

  sys_debugf("[uc8151] reset pulse begin");

  // UC8151 reset is active low; pulse low then wait for internal startup.
  hw_gpio_set(uc8151->reset_pin, true);
  sys_sleep_ms(5u);
  hw_gpio_set(uc8151->reset_pin, false);
  sys_sleep_ms(10u);
  hw_gpio_set(uc8151->reset_pin, true);
  sys_sleep_ms(10u);

  sys_debugf("[uc8151] reset pulse end");
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

static bool _dev_uc8151_ready(const dev_uc8151_t *uc8151) {
  return uc8151 != NULL && uc8151->init && hw_spi_valid(uc8151->spi) &&
         hw_gpio_valid(uc8151->dc_pin) && hw_gpio_valid(uc8151->reset_pin) &&
         hw_gpio_valid(uc8151->busy_pin) && uc8151->width > 0u &&
         uc8151->height > 0u && _dev_uc8151_valid_rotation(uc8151->rotation);
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_uc8151_t *dev_uc8151_init(hw_spi_t *spi, hw_gpio_t *dc_pin,
                              hw_gpio_t *reset_pin, hw_gpio_t *busy_pin,
                              pix_size_t size,
                              const dev_uc8151_config_t *config) {
  if (!hw_spi_valid(spi) || !hw_gpio_valid(dc_pin) ||
      !hw_gpio_valid(reset_pin) || !hw_gpio_valid(busy_pin) || size.w == 0u ||
      size.h == 0u) {
    return NULL;
  }

  dev_uc8151_config_t resolved = _dev_uc8151_resolve_config(config);

  sys_debugf("[uc8151] init request width=%u height=%u rot=%u busy=%u",
             (unsigned int)size.w, (unsigned int)size.h,
             (unsigned int)resolved.rotation,
             (unsigned int)hw_gpio_get(busy_pin));

  sys_debugf("[uc8151] init resolve_config");

  sys_debugf("[uc8151] init calloc begin");
  dev_uc8151_t *uc8151 = sys_calloc(1u, sizeof(*uc8151));
  if (uc8151 == NULL) {
    sys_debugf("[uc8151] init calloc failed");
    return NULL;
  }
  sys_debugf("[uc8151] init calloc ok");

  uc8151->spi = spi;
  uc8151->dc_pin = dc_pin;
  uc8151->reset_pin = reset_pin;
  uc8151->busy_pin = busy_pin;
  uc8151->width = size.w;
  uc8151->height = size.h;
  uc8151->rotation = resolved.rotation;
  uc8151->speed = resolved.speed;
  uc8151->inverted = resolved.inverted;
  uc8151->blocking = resolved.blocking;
  uc8151->init = true;

  sys_debugf("[uc8151] init set pins start");
  sys_debugf("[uc8151] init set reset=1 begin");
  hw_gpio_set(uc8151->reset_pin, true);
  sys_debugf("[uc8151] init set reset=1 end value=%u",
             (unsigned int)hw_gpio_get(uc8151->reset_pin));

  sys_debugf("[uc8151] init set dc=0 begin");
  hw_gpio_set(uc8151->dc_pin, false);
  sys_debugf("[uc8151] init set dc=0 end value=%u",
             (unsigned int)hw_gpio_get(uc8151->dc_pin));

  sys_debugf("[uc8151] init setup begin");
  if (!_dev_uc8151_setup(uc8151)) {
    sys_debugf("[uc8151] init setup failed");
    dev_uc8151_deinit(uc8151);
    return NULL;
  }

  sys_debugf("[uc8151] init ok");

  return uc8151;
}

void dev_uc8151_reset(dev_uc8151_t *uc8151) {
  if (!_dev_uc8151_ready(uc8151)) {
    return;
  }

  _dev_uc8151_reset(uc8151);
}

void dev_uc8151_deinit(dev_uc8151_t *uc8151) {
  if (uc8151 == NULL) {
    return;
  }

  if (uc8151->init) {
    (void)_dev_uc8151_power_off(uc8151);
  }

  uc8151->init = false;
  uc8151->spi = NULL;
  uc8151->dc_pin = NULL;
  uc8151->reset_pin = NULL;
  uc8151->busy_pin = NULL;
  sys_free(uc8151);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool dev_uc8151_paint(dev_uc8151_t *uc8151, const pix_frame_t *frame) {
  if (!_dev_uc8151_ready(uc8151) || !_dev_uc8151_validate_frame(frame)) {
    return false;
  }

  if (frame->size.w != uc8151->width || frame->size.h != uc8151->height) {
    return false;
  }

  size_t bytes_per_column = ((size_t)uc8151->height + 7u) / 8u;
  if (frame->stride < bytes_per_column) {
    return false;
  }

  if (uc8151->blocking && !_dev_uc8151_wait_ready(uc8151)) {
    return false;
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_PON, NULL, 0u)) {
    return false;
  }
  if (!_dev_uc8151_send_command(uc8151, UC8151_PTOU, NULL, 0u)) {
    return false;
  }
  if (!_dev_uc8151_send_command(uc8151, UC8151_DTM2, NULL, 0u)) {
    return false;
  }

  const uint8_t *data = (const uint8_t *)frame->data;
  for (uint16_t x = 0u; x < uc8151->width; ++x) {
    const uint8_t *column = data + ((size_t)x * frame->stride);
    if (!_dev_uc8151_send_data(uc8151, column, bytes_per_column)) {
      return false;
    }
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_DSP, NULL, 0u)) {
    return false;
  }
  if (!_dev_uc8151_send_command(uc8151, UC8151_DRF, NULL, 0u)) {
    return false;
  }

  return !uc8151->blocking || _dev_uc8151_wait_ready(uc8151);
}

bool dev_uc8151_paint_rect(dev_uc8151_t *uc8151, const pix_frame_t *frame,
                           pix_point_t origin, pix_size_t region_size) {
  if (!_dev_uc8151_ready(uc8151) || !_dev_uc8151_validate_frame(frame)) {
    return false;
  }

  if (origin.x < 0 || origin.y < 0 || region_size.w == 0u ||
      region_size.h == 0u) {
    return false;
  }

  uint16_t x0 = (uint16_t)origin.x;
  uint16_t y0 = (uint16_t)origin.y;
  uint16_t x1 = (uint16_t)(x0 + region_size.w);
  uint16_t y1 = (uint16_t)(y0 + region_size.h);

  if (x1 <= x0 || y1 <= y0) {
    return false;
  }

  if (x1 > uc8151->width || y1 > uc8151->height ||
      frame->size.w != region_size.w || frame->size.h != region_size.h) {
    return false;
  }

  if ((y0 % 8u) != 0u || (region_size.h % 8u) != 0u) {
    return false;
  }

  size_t bytes_per_column = (size_t)region_size.h / 8u;
  if (frame->stride < bytes_per_column) {
    return false;
  }

  if (uc8151->blocking && !_dev_uc8151_wait_ready(uc8151)) {
    return false;
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_PON, NULL, 0u)) {
    return false;
  }
  if (!_dev_uc8151_send_command(uc8151, UC8151_PTIN, NULL, 0u)) {
    return false;
  }

  uint8_t partial_window[7] = {
      (uint8_t)y0,
      (uint8_t)(y1 - 1u),
      (uint8_t)(x0 >> 8),
      (uint8_t)(x0 & 0xFFu),
      (uint8_t)((x1 - 1u) >> 8),
      (uint8_t)((x1 - 1u) & 0xFFu),
      0x01u,
  };
  if (!_dev_uc8151_send_command(uc8151, UC8151_PTL, partial_window,
                                sizeof(partial_window))) {
    return false;
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_DTM2, NULL, 0u)) {
    return false;
  }

  const uint8_t *data = (const uint8_t *)frame->data;
  size_t bytes_to_send = (size_t)region_size.h / 8u;
  for (uint16_t dx = 0u; dx < region_size.w; ++dx) {
    const uint8_t *column = data + ((size_t)dx * frame->stride);
    if (!_dev_uc8151_send_data(uc8151, column, bytes_to_send)) {
      return false;
    }
  }

  if (!_dev_uc8151_send_command(uc8151, UC8151_DSP, NULL, 0u)) {
    return false;
  }
  if (!_dev_uc8151_send_command(uc8151, UC8151_DRF, NULL, 0u)) {
    return false;
  }

  return !uc8151->blocking || _dev_uc8151_wait_ready(uc8151);
}

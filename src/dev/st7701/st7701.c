#include <picofuse/dev/st7701.h>
#include <picofuse/sys.h>

#if defined(SYSTEM_NAME_PICO)
#include "st7701.pio.h"
#include <hardware/pio.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define ST7701_SPI_9BIT_CMD 0x000u
#define ST7701_SPI_9BIT_DATA 0x100u
#define ST7701_INIT_MAX_PAYLOAD 32u
#define ST7701_PIO_SMOKE_ROWS 8u

#define ST7701_CMD_SWRESET 0x01u
#define ST7701_CMD_SLPOUT 0x11u
#define ST7701_CMD_NORON 0x13u
#define ST7701_CMD_INVOFF 0x20u
#define ST7701_CMD_INVON 0x21u
#define ST7701_CMD_ALLPOFF 0x22u
#define ST7701_CMD_ALLPON 0x23u
#define ST7701_CMD_DISPON 0x29u
#define ST7701_CMD_MADCTL 0x36u
#define ST7701_CMD_COLMOD 0x3Au

#define ST7701_CMD_PVGAMCTRL 0xB0u
#define ST7701_CMD_NVGAMCTRL 0xB1u
#define ST7701_CMD_LNESET 0xC0u
#define ST7701_CMD_PORCTRL 0xC1u
#define ST7701_CMD_INVSET 0xC2u
#define ST7701_CMD_RGBCTRL 0xC3u
#define ST7701_CMD_COLCTRL 0xCDu

#define ST7701_CMD_CND2BKxSEL 0xFFu

#define ST7701_TIMING_EXEC_NOP 0xB042u
#define ST7701_TIMING_EXEC_IRQ4 0xD004u

#define ST7701_ENABLE_PIO_RUNTIME 0u
#define ST7701_PIO_FRAME_ROWS_PER_PAINT 16u
#define ST7701_DEBUG_LOG_EVERY_N_PAINT 32u

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_st7701_t {
  hw_spi_t *spi;
  hw_gpio_t *reset_pin;
  hw_gpio_t *backlight_pin;
  bool backlight_active_low;
  uint16_t width;
  uint16_t height;
  dev_st7701_rotation_t rotation;
  dev_st7701_pinout_t pinout;
  uint16_t *framebuffer;
  uint32_t *palette;
#if defined(SYSTEM_NAME_PICO)
  PIO pio;
  int8_t parallel_sm;
  int8_t timing_sm;
  uint16_t parallel_offset;
  uint16_t parallel_18bpp_offset;
  uint16_t timing_offset;
  uint16_t pio_next_row;
  bool pio_staged;
#endif
  uint32_t paint_calls;
  bool init;
};

typedef struct {
  uint8_t command;
  const uint8_t *data;
  uint8_t data_len;
  uint16_t delay_ms;
} dev_st7701_init_cmd_t;

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static dev_st7701_t _dev_st7701_singleton;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _dev_st7701_send_command(dev_st7701_t *st7701, uint8_t command,
                                     const uint8_t *data, size_t data_len);
static bool _dev_st7701_run_init_sequence(dev_st7701_t *st7701);
static void _dev_st7701_set_backlight(dev_st7701_t *st7701, bool enabled);
static bool _dev_st7701_stage_pio(dev_st7701_t *st7701);
static bool _dev_st7701_start_pio(dev_st7701_t *st7701);
static void _dev_st7701_unstage_pio(dev_st7701_t *st7701);

static dev_st7701_config_t
_dev_st7701_resolve_config(const dev_st7701_config_t *config) {
  dev_st7701_config_t resolved = {
      .rotation = DEV_ST7701_ROTATION_0,
      .pinout =
          {
              .lcd_spi_data_pin = 0xFFu,
              .lcd_spi_clk_pin = 0xFFu,
              .lcd_spi_cs_pin = 0xFFu,
              .lcd_dot_clk_pin = 0xFFu,
              .lcd_de_pin = 0xFFu,
              .vsync_pin = 0xFFu,
              .hsync_pin = 0xFFu,
              .r2_pin = 0xFFu,
              .r3_pin = 0xFFu,
              .r4_pin = 0xFFu,
              .r5_pin = 0xFFu,
              .r6_pin = 0xFFu,
              .r7_pin = 0xFFu,
              .g2_pin = 0xFFu,
              .g3_pin = 0xFFu,
              .g4_pin = 0xFFu,
              .g5_pin = 0xFFu,
              .g6_pin = 0xFFu,
              .g7_pin = 0xFFu,
              .b2_pin = 0xFFu,
              .b3_pin = 0xFFu,
              .b4_pin = 0xFFu,
              .b5_pin = 0xFFu,
              .b6_pin = 0xFFu,
              .b7_pin = 0xFFu,
          },
      .backlight_active_low = false,
      .framebuffer = NULL,
      .palette = NULL,
  };

  if (config != NULL) {
    resolved = *config;
  }

  return resolved;
}

static void _dev_st7701_set_backlight(dev_st7701_t *st7701, bool enabled) {
  if (!dev_st7701_valid(st7701) || st7701->backlight_pin == NULL) {
    return;
  }

  bool level = st7701->backlight_active_low ? !enabled : enabled;
  hw_gpio_set(st7701->backlight_pin, level);
}

#if defined(SYSTEM_NAME_PICO)
static bool _dev_st7701_pin_defined(uint8_t pin) { return pin != 0xFFu; }

static uint32_t _dev_st7701_make_timing_word(bool vsync_high, bool hsync_high,
                                             uint16_t delay_clocks,
                                             uint16_t exec_instr) {
  uint32_t word = 0u;
  word |= (uint32_t)(vsync_high ? 1u : 0u) << 31;
  word |= (uint32_t)(hsync_high ? 1u : 0u) << 30;
  word |= (uint32_t)delay_clocks << 16;
  word |= (uint32_t)exec_instr;
  return word;
}

static void _dev_st7701_push_timing_word(dev_st7701_t *st7701, uint32_t word) {
  pio_sm_put_blocking(st7701->pio, (uint)st7701->timing_sm, word);
}

static uint32_t _dev_st7701_rgb888_to_rgb666(uint8_t r8, uint8_t g8,
                                             uint8_t b8) {
  uint32_t r6 = (uint32_t)(r8 >> 2) & 0x3Fu;
  uint32_t g6 = (uint32_t)(g8 >> 2) & 0x3Fu;
  uint32_t b6 = (uint32_t)(b8 >> 2) & 0x3Fu;
  return (r6 << 12) | (g6 << 6) | b6;
}

static uint32_t _dev_st7701_rgb565_to_rgb666(uint16_t rgb565) {
  uint8_t r5 = (uint8_t)((rgb565 >> 11) & 0x1Fu);
  uint8_t g6 = (uint8_t)((rgb565 >> 5) & 0x3Fu);
  uint8_t b5 = (uint8_t)(rgb565 & 0x1Fu);

  uint8_t r8 = (uint8_t)((r5 << 3) | (r5 >> 2));
  uint8_t g8 = (uint8_t)((g6 << 2) | (g6 >> 4));
  uint8_t b8 = (uint8_t)((b5 << 3) | (b5 >> 2));
  return _dev_st7701_rgb888_to_rgb666(r8, g8, b8);
}

static void _dev_st7701_push_test_row_words(dev_st7701_t *st7701,
                                            uint16_t row_index) {
  if (st7701->width == 0u) {
    return;
  }

  uint16_t bar = (uint16_t)((row_index / 8u) % 6u);
  uint32_t word_a = 0x00000u;
  uint32_t word_b = 0x00000u;

  switch (bar) {
  case 0u:
    word_a = 0x3FFFFu;
    word_b = 0x3FFFFu;
    break;
  case 1u:
    word_a = 0x3F000u;
    word_b = 0x3F000u;
    break;
  case 2u:
    word_a = 0x00FC0u;
    word_b = 0x00FC0u;
    break;
  case 3u:
    word_a = 0x0003Fu;
    word_b = 0x0003Fu;
    break;
  case 4u:
    word_a = 0x3FFC0u;
    word_b = 0x3FFC0u;
    break;
  case 5u:
  default:
    word_a = 0x3F03Fu;
    word_b = 0x3F03Fu;
    break;
  }

  for (uint16_t x = 0u; x < st7701->width; x += 2u) {
    pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm, word_a);
    pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm, word_b);
  }
}

static bool _dev_st7701_run_pio_solid(dev_st7701_t *st7701, uint32_t rgb666,
                                      uint16_t rows) {
  if (!dev_st7701_valid(st7701) || !st7701->pio_staged || rows == 0u) {
    return false;
  }

  uint32_t word = rgb666 & 0x3FFFFu;
  for (uint16_t row = 0u; row < rows; row++) {
    _dev_st7701_push_timing_word(
        st7701,
        _dev_st7701_make_timing_word(true, true, 64u, ST7701_TIMING_EXEC_IRQ4));

    for (uint16_t x = 0u; x < st7701->width; x += 2u) {
      pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm, word);
      pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm, word);
    }
  }

  _dev_st7701_push_timing_word(
      st7701,
      _dev_st7701_make_timing_word(true, true, 64u, ST7701_TIMING_EXEC_NOP));
  return true;
}

static bool _dev_st7701_run_pio_framebuffer_rgb565(dev_st7701_t *st7701,
                                                   const uint16_t *pixels,
                                                   uint16_t start_row,
                                                   uint16_t rows) {
  if (!dev_st7701_valid(st7701) || !st7701->pio_staged || rows == 0u ||
      pixels == NULL) {
    return false;
  }

  if (start_row >= st7701->height) {
    return false;
  }

  uint16_t end_row = (uint16_t)(start_row + rows);
  if (end_row > st7701->height) {
    end_row = st7701->height;
  }

  for (uint16_t row = start_row; row < end_row; row++) {
    _dev_st7701_push_timing_word(
        st7701,
        _dev_st7701_make_timing_word(true, true, 64u, ST7701_TIMING_EXEC_IRQ4));

    const uint16_t *line = &pixels[(size_t)row * (size_t)st7701->width];
    for (uint16_t x = 0u; x < st7701->width; x += 2u) {
      uint32_t p0 = _dev_st7701_rgb565_to_rgb666(line[x]);
      uint32_t p1 = (x + 1u < st7701->width)
                        ? _dev_st7701_rgb565_to_rgb666(line[x + 1u])
                        : p0;
      pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm,
                          p0 & 0x3FFFFu);
      pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm,
                          p1 & 0x3FFFFu);
    }
  }

  _dev_st7701_push_timing_word(
      st7701,
      _dev_st7701_make_timing_word(true, true, 64u, ST7701_TIMING_EXEC_NOP));
  return true;
}

static bool _dev_st7701_run_pio_smoke(dev_st7701_t *st7701, uint16_t rows) {
  if (!dev_st7701_valid(st7701) || !st7701->pio_staged || rows == 0u) {
    return false;
  }

  for (uint16_t row = 0u; row < rows; row++) {
    _dev_st7701_push_timing_word(
        st7701,
        _dev_st7701_make_timing_word(true, true, 64u, ST7701_TIMING_EXEC_IRQ4));

    _dev_st7701_push_test_row_words(st7701, row);
  }

  _dev_st7701_push_timing_word(
      st7701,
      _dev_st7701_make_timing_word(true, true, 64u, ST7701_TIMING_EXEC_NOP));
  return true;
}

static bool _dev_st7701_data_pins_contiguous(const dev_st7701_t *st7701,
                                             uint8_t *base_pin_out) {
  uint8_t pins[18] = {
      st7701->pinout.r2_pin, st7701->pinout.r3_pin, st7701->pinout.r4_pin,
      st7701->pinout.r5_pin, st7701->pinout.r6_pin, st7701->pinout.r7_pin,
      st7701->pinout.g2_pin, st7701->pinout.g3_pin, st7701->pinout.g4_pin,
      st7701->pinout.g5_pin, st7701->pinout.g6_pin, st7701->pinout.g7_pin,
      st7701->pinout.b2_pin, st7701->pinout.b3_pin, st7701->pinout.b4_pin,
      st7701->pinout.b5_pin, st7701->pinout.b6_pin, st7701->pinout.b7_pin,
  };

  uint8_t min_pin = 0xFFu;
  uint8_t max_pin = 0u;

  for (size_t i = 0; i < (sizeof(pins) / sizeof(pins[0])); i++) {
    if (!_dev_st7701_pin_defined(pins[i])) {
      return false;
    }

    if (pins[i] < min_pin) {
      min_pin = pins[i];
    }
    if (pins[i] > max_pin) {
      max_pin = pins[i];
    }
  }

  if ((uint8_t)(max_pin - min_pin + 1u) != 18u) {
    return false;
  }

  if (base_pin_out != NULL) {
    *base_pin_out = min_pin;
  }

  return true;
}

static bool _dev_st7701_stage_pio(dev_st7701_t *st7701) {
  (void)st7701;
#if ST7701_ENABLE_PIO_RUNTIME
  if (!dev_st7701_valid(st7701)) {
    return false;
  }

  sys_debugf("[st7701] stage_pio begin");

  st7701->pio = pio1;

  st7701->parallel_sm = (int8_t)pio_claim_unused_sm(st7701->pio, false);
  if (st7701->parallel_sm < 0) {
    sys_debugf("[st7701] failed to claim parallel state machine");
    return false;
  }

  st7701->timing_sm = (int8_t)pio_claim_unused_sm(st7701->pio, false);
  if (st7701->timing_sm < 0) {
    pio_sm_unclaim(st7701->pio, (uint)st7701->parallel_sm);
    st7701->parallel_sm = -1;
    sys_debugf("[st7701] failed to claim timing state machine");
    return false;
  }

  st7701->parallel_offset =
      (uint16_t)pio_add_program(st7701->pio, &st7701_parallel_program);
  st7701->parallel_18bpp_offset =
      (uint16_t)pio_add_program(st7701->pio, &st7701_parallel_18bpp_program);
  st7701->timing_offset =
      (uint16_t)pio_add_program(st7701->pio, &st7701_timing_program);
  st7701->pio_staged = true;

  sys_debugf("[st7701] staged pio=%u parallel_sm=%d timing_sm=%d "
             "off(par=%u par18=%u tim=%u)",
             (unsigned int)pio_get_index(st7701->pio), (int)st7701->parallel_sm,
             (int)st7701->timing_sm, (unsigned int)st7701->parallel_offset,
             (unsigned int)st7701->parallel_18bpp_offset,
             (unsigned int)st7701->timing_offset);

  return true;
#else
  return true;
#endif
}

static bool _dev_st7701_start_pio(dev_st7701_t *st7701) {
#if ST7701_ENABLE_PIO_RUNTIME
  if (!dev_st7701_valid(st7701) || !st7701->pio_staged || st7701->pio == NULL) {
    return false;
  }

  sys_debugf("[st7701] start_pio begin w=%u h=%u", (unsigned int)st7701->width,
             (unsigned int)st7701->height);

  uint8_t rgb_base = 0u;
  if (!_dev_st7701_data_pins_contiguous(st7701, &rgb_base)) {
    sys_debugf("[st7701] rgb data pins must be defined and contiguous");
    return false;
  }

  if (!_dev_st7701_pin_defined(st7701->pinout.hsync_pin) ||
      !_dev_st7701_pin_defined(st7701->pinout.vsync_pin) ||
      !_dev_st7701_pin_defined(st7701->pinout.lcd_dot_clk_pin) ||
      !_dev_st7701_pin_defined(st7701->pinout.lcd_de_pin)) {
    sys_debugf("[st7701] missing timing/de pin mapping for pio start");
    return false;
  }

  if (st7701->pinout.vsync_pin != (uint8_t)(st7701->pinout.hsync_pin + 1u)) {
    sys_debugf("[st7701] expected contiguous hsync/vsync pins");
    return false;
  }

  pio_sm_set_enabled(st7701->pio, (uint)st7701->timing_sm, false);
  pio_sm_set_enabled(st7701->pio, (uint)st7701->parallel_sm, false);

  pio_sm_clear_fifos(st7701->pio, (uint)st7701->timing_sm);
  pio_sm_clear_fifos(st7701->pio, (uint)st7701->parallel_sm);

  for (uint8_t pin = rgb_base; pin < (uint8_t)(rgb_base + 18u); pin++) {
    pio_gpio_init(st7701->pio, pin);
  }
  pio_gpio_init(st7701->pio, st7701->pinout.lcd_de_pin);
  pio_gpio_init(st7701->pio, st7701->pinout.hsync_pin);
  pio_gpio_init(st7701->pio, st7701->pinout.vsync_pin);
  pio_gpio_init(st7701->pio, st7701->pinout.lcd_dot_clk_pin);

  pio_sm_set_consecutive_pindirs(st7701->pio, (uint)st7701->parallel_sm,
                                 rgb_base, 18u, true);
  pio_sm_set_consecutive_pindirs(st7701->pio, (uint)st7701->parallel_sm,
                                 st7701->pinout.lcd_de_pin, 1u, true);
  pio_sm_set_consecutive_pindirs(st7701->pio, (uint)st7701->timing_sm,
                                 st7701->pinout.hsync_pin, 2u, true);
  pio_sm_set_consecutive_pindirs(st7701->pio, (uint)st7701->timing_sm,
                                 st7701->pinout.lcd_dot_clk_pin, 1u, true);

  pio_sm_config timing_cfg =
      st7701_timing_program_get_default_config(st7701->timing_offset);
  sm_config_set_out_pins(&timing_cfg, st7701->pinout.hsync_pin, 2u);
  sm_config_set_sideset_pins(&timing_cfg, st7701->pinout.lcd_dot_clk_pin);
  sm_config_set_out_shift(&timing_cfg, false, true, 32u);
  sm_config_set_fifo_join(&timing_cfg, PIO_FIFO_JOIN_TX);

  pio_sm_config parallel_cfg = st7701_parallel_18bpp_program_get_default_config(
      st7701->parallel_18bpp_offset);
  sm_config_set_out_pins(&parallel_cfg, rgb_base, 18u);
  sm_config_set_sideset_pins(&parallel_cfg, st7701->pinout.lcd_de_pin);
  sm_config_set_out_shift(&parallel_cfg, false, true, 32u);
  sm_config_set_fifo_join(&parallel_cfg, PIO_FIFO_JOIN_TX);

  pio_sm_init(st7701->pio, (uint)st7701->timing_sm, st7701->timing_offset,
              &timing_cfg);
  pio_sm_init(st7701->pio, (uint)st7701->parallel_sm,
              st7701->parallel_18bpp_offset, &parallel_cfg);

  pio_sm_put_blocking(st7701->pio, (uint)st7701->parallel_sm,
                      (uint32_t)(st7701->width / 2u - 1u));
  pio_sm_exec(st7701->pio, (uint)st7701->parallel_sm,
              pio_encode_pull(false, false));
  pio_sm_exec(st7701->pio, (uint)st7701->parallel_sm,
              pio_encode_mov(pio_y, pio_osr));

  pio_sm_set_enabled(st7701->pio, (uint)st7701->timing_sm, true);
  pio_sm_set_enabled(st7701->pio, (uint)st7701->parallel_sm, true);

  if (!_dev_st7701_run_pio_smoke(st7701, ST7701_PIO_SMOKE_ROWS)) {
    sys_debugf("[st7701] failed to run pio smoke pattern");
    return false;
  }

  sys_debugf("[st7701] started pio rgb_base=%u hsync=%u vsync=%u de=%u dclk=%u",
             (unsigned int)rgb_base, (unsigned int)st7701->pinout.hsync_pin,
             (unsigned int)st7701->pinout.vsync_pin,
             (unsigned int)st7701->pinout.lcd_de_pin,
             (unsigned int)st7701->pinout.lcd_dot_clk_pin);
  return true;
#else
  (void)st7701;
  return true;
#endif
}

static void _dev_st7701_unstage_pio(dev_st7701_t *st7701) {
  if (st7701 == NULL || !st7701->pio_staged) {
    return;
  }

  pio_sm_set_enabled(st7701->pio, (uint)st7701->parallel_sm, false);
  pio_sm_set_enabled(st7701->pio, (uint)st7701->timing_sm, false);

  pio_remove_program(st7701->pio, &st7701_timing_program,
                     st7701->timing_offset);
  pio_remove_program(st7701->pio, &st7701_parallel_18bpp_program,
                     st7701->parallel_18bpp_offset);
  pio_remove_program(st7701->pio, &st7701_parallel_program,
                     st7701->parallel_offset);

  if (st7701->timing_sm >= 0) {
    pio_sm_unclaim(st7701->pio, (uint)st7701->timing_sm);
    st7701->timing_sm = -1;
  }

  if (st7701->parallel_sm >= 0) {
    pio_sm_unclaim(st7701->pio, (uint)st7701->parallel_sm);
    st7701->parallel_sm = -1;
  }

  st7701->pio_staged = false;
}
#else
static bool _dev_st7701_stage_pio(dev_st7701_t *st7701) {
  (void)st7701;
  return true;
}

static bool _dev_st7701_start_pio(dev_st7701_t *st7701) {
  (void)st7701;
  return true;
}

static void _dev_st7701_unstage_pio(dev_st7701_t *st7701) { (void)st7701; }
#endif

static bool _dev_st7701_send_command(dev_st7701_t *st7701, uint8_t command,
                                     const uint8_t *data, size_t data_len) {
  if (!dev_st7701_valid(st7701) || data_len > ST7701_INIT_MAX_PAYLOAD) {
    return false;
  }

  uint16_t frame_words[1u + ST7701_INIT_MAX_PAYLOAD] = {0};
  frame_words[0] = ST7701_SPI_9BIT_CMD | command;

  for (size_t i = 0; i < data_len; i++) {
    frame_words[1u + i] = ST7701_SPI_9BIT_DATA | data[i];
  }

  size_t words = 1u + data_len;
  size_t tx = hw_spi_write_words(st7701->spi, frame_words, words, 0u);
  if (tx != words) {
    sys_debugf("[st7701] cmd 0x%02X failed (%u/%u)", (unsigned int)command,
               (unsigned int)tx, (unsigned int)words);
    return false;
  }

  return true;
}

static bool _dev_st7701_run_init_sequence(dev_st7701_t *st7701) {
  static const uint8_t d_c2_bk0[] = {0x77u, 0x01u, 0x00u, 0x00u, 0x10u};
  static const uint8_t d_madctl[] = {0x00u};
  static const uint8_t d_lneset[] = {0x3bu, 0x00u};
  static const uint8_t d_porctrl[] = {0x0du, 0x02u};
  static const uint8_t d_invset[] = {0x31u, 0x01u};
  static const uint8_t d_colctrl[] = {0x08u};
  static const uint8_t d_pvgam[] = {0x00u, 0x11u, 0x18u, 0x0eu, 0x11u, 0x06u,
                                    0x07u, 0x08u, 0x07u, 0x22u, 0x04u, 0x12u,
                                    0x0fu, 0xaau, 0x31u, 0x18u};
  static const uint8_t d_nvgam[] = {0x00u, 0x11u, 0x19u, 0x0eu, 0x12u, 0x07u,
                                    0x08u, 0x08u, 0x08u, 0x22u, 0x04u, 0x11u,
                                    0x11u, 0xa9u, 0x32u, 0x18u};
  static const uint8_t d_rgbctrl[] = {0x80u, 0x2eu, 0x0eu};

  static const uint8_t d_c2_bk1[] = {0x77u, 0x01u, 0x00u, 0x00u, 0x11u};
  static const uint8_t d_vhrs[] = {0x60u};
  static const uint8_t d_vcoms[] = {0x32u};
  static const uint8_t d_vghss[] = {0x07u};
  static const uint8_t d_testcmd[] = {0x80u};
  static const uint8_t d_vgls[] = {0x49u};
  static const uint8_t d_pwctrl1[] = {0x85u};
  static const uint8_t d_pwctrl2[] = {0x21u};
  static const uint8_t d_pdr1[] = {0x78u};
  static const uint8_t d_pdr2[] = {0x78u};

  // Vendor panel bring-up block; panel/module specific.
  static const uint8_t d_e0[] = {0x00u, 0x1bu, 0x02u};
  static const uint8_t d_e1[] = {0x08u, 0xa0u, 0x00u, 0x00u, 0x07u, 0xa0u,
                                 0x00u, 0x00u, 0x00u, 0x44u, 0x44u};
  static const uint8_t d_e2[] = {0x11u, 0x11u, 0x44u, 0x44u, 0xedu, 0xa0u,
                                 0x00u, 0x00u, 0xecu, 0xa0u, 0x00u, 0x00u};
  static const uint8_t d_e3[] = {0x00u, 0x00u, 0x11u, 0x11u};
  static const uint8_t d_e4[] = {0x44u, 0x44u};
  static const uint8_t d_e5[] = {0x0au, 0xe9u, 0xd8u, 0xa0u, 0x0cu, 0xebu,
                                 0xd8u, 0xa0u, 0x0eu, 0xedu, 0xd8u, 0xa0u,
                                 0x10u, 0xefu, 0xd8u, 0xa0u};
  static const uint8_t d_e6[] = {0x00u, 0x00u, 0x11u, 0x11u};
  static const uint8_t d_e7[] = {0x44u, 0x44u};
  static const uint8_t d_e8[] = {0x09u, 0xe8u, 0xd8u, 0xa0u, 0x0bu, 0xeau,
                                 0xd8u, 0xa0u, 0x0du, 0xecu, 0xd8u, 0xa0u,
                                 0x0fu, 0xeeu, 0xd8u, 0xa0u};
  static const uint8_t d_eb[] = {0x02u, 0x00u, 0xe4u, 0xe4u,
                                 0x88u, 0x00u, 0x40u};
  static const uint8_t d_ec[] = {0x3cu, 0x00u};
  static const uint8_t d_ed[] = {0xabu, 0x89u, 0x76u, 0x54u, 0x02u, 0xffu,
                                 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0x20u,
                                 0x45u, 0x67u, 0x98u, 0xbau};
  static const uint8_t d_36[] = {0x00u};

  static const uint8_t d_c2_bk3[] = {0x77u, 0x01u, 0x00u, 0x00u, 0x13u};
  static const uint8_t d_bk3_e5[] = {0xe4u};

  static const uint8_t d_c2_bk0_exit[] = {0x77u, 0x01u, 0x00u, 0x00u, 0x00u};
  static const uint8_t d_colmod[] = {0x66u};

  static const dev_st7701_init_cmd_t init_sequence[] = {
      {ST7701_CMD_SWRESET, NULL, 0u, 150u},

      {ST7701_CMD_CND2BKxSEL, d_c2_bk0, (uint8_t)sizeof(d_c2_bk0), 0u},
      {ST7701_CMD_MADCTL, d_madctl, (uint8_t)sizeof(d_madctl), 0u},
      {ST7701_CMD_LNESET, d_lneset, (uint8_t)sizeof(d_lneset), 0u},
      {ST7701_CMD_PORCTRL, d_porctrl, (uint8_t)sizeof(d_porctrl), 0u},
      {ST7701_CMD_INVSET, d_invset, (uint8_t)sizeof(d_invset), 0u},
      {ST7701_CMD_COLCTRL, d_colctrl, (uint8_t)sizeof(d_colctrl), 0u},
      {ST7701_CMD_PVGAMCTRL, d_pvgam, (uint8_t)sizeof(d_pvgam), 0u},
      {ST7701_CMD_NVGAMCTRL, d_nvgam, (uint8_t)sizeof(d_nvgam), 0u},
      {ST7701_CMD_RGBCTRL, d_rgbctrl, (uint8_t)sizeof(d_rgbctrl), 0u},

      {ST7701_CMD_CND2BKxSEL, d_c2_bk1, (uint8_t)sizeof(d_c2_bk1), 0u},
      {ST7701_CMD_PVGAMCTRL, d_vhrs, (uint8_t)sizeof(d_vhrs), 0u},
      {ST7701_CMD_NVGAMCTRL, d_vcoms, (uint8_t)sizeof(d_vcoms), 0u},
      {0xB2u, d_vghss, (uint8_t)sizeof(d_vghss), 0u},
      {0xB3u, d_testcmd, (uint8_t)sizeof(d_testcmd), 0u},
      {0xB5u, d_vgls, (uint8_t)sizeof(d_vgls), 0u},
      {0xB7u, d_pwctrl1, (uint8_t)sizeof(d_pwctrl1), 0u},
      {0xB8u, d_pwctrl2, (uint8_t)sizeof(d_pwctrl2), 0u},
      {ST7701_CMD_PORCTRL, d_pdr1, (uint8_t)sizeof(d_pdr1), 0u},
      {ST7701_CMD_INVSET, d_pdr2, (uint8_t)sizeof(d_pdr2), 0u},

      {0xE0u, d_e0, (uint8_t)sizeof(d_e0), 0u},
      {0xE1u, d_e1, (uint8_t)sizeof(d_e1), 0u},
      {0xE2u, d_e2, (uint8_t)sizeof(d_e2), 0u},
      {0xE3u, d_e3, (uint8_t)sizeof(d_e3), 0u},
      {0xE4u, d_e4, (uint8_t)sizeof(d_e4), 0u},
      {0xE5u, d_e5, (uint8_t)sizeof(d_e5), 0u},
      {0xE6u, d_e6, (uint8_t)sizeof(d_e6), 0u},
      {0xE7u, d_e7, (uint8_t)sizeof(d_e7), 0u},
      {0xE8u, d_e8, (uint8_t)sizeof(d_e8), 0u},
      {0xEBu, d_eb, (uint8_t)sizeof(d_eb), 0u},
      {0xECu, d_ec, (uint8_t)sizeof(d_ec), 0u},
      {0xEDu, d_ed, (uint8_t)sizeof(d_ed), 0u},
      {ST7701_CMD_MADCTL, d_36, (uint8_t)sizeof(d_36), 0u},

      {ST7701_CMD_CND2BKxSEL, d_c2_bk3, (uint8_t)sizeof(d_c2_bk3), 0u},
      {0xE5u, d_bk3_e5, (uint8_t)sizeof(d_bk3_e5), 0u},

      {ST7701_CMD_CND2BKxSEL, d_c2_bk0_exit, (uint8_t)sizeof(d_c2_bk0_exit),
       0u},
      {ST7701_CMD_COLMOD, d_colmod, (uint8_t)sizeof(d_colmod), 0u},
      {ST7701_CMD_INVON, NULL, 0u, 1u},
      {ST7701_CMD_SLPOUT, NULL, 0u, 120u},
      {ST7701_CMD_DISPON, NULL, 0u, 50u},
  };

  for (size_t i = 0; i < (sizeof(init_sequence) / sizeof(init_sequence[0]));
       i++) {
    const dev_st7701_init_cmd_t *step = &init_sequence[i];

    if (!_dev_st7701_send_command(st7701, step->command, step->data,
                                  step->data_len)) {
      return false;
    }

    if (step->delay_ms > 0u) {
      sys_sleep_ms(step->delay_ms);
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

bool dev_st7701_configure_spi(hw_spi_t *spi) {
  return hw_spi_set_format(spi, HW_SPI_MODE_0, 9u);
}

bool dev_st7701_set_inversion(dev_st7701_t *st7701, bool enabled) {
  if (!dev_st7701_valid(st7701)) {
    return false;
  }

  return _dev_st7701_send_command(
      st7701, enabled ? ST7701_CMD_INVON : ST7701_CMD_INVOFF, NULL, 0u);
}

bool dev_st7701_set_all_pixels(dev_st7701_t *st7701, bool enabled) {
  if (!dev_st7701_valid(st7701)) {
    return false;
  }

  return _dev_st7701_send_command(
      st7701, enabled ? ST7701_CMD_ALLPON : ST7701_CMD_ALLPOFF, NULL, 0u);
}

bool dev_st7701_set_normal_mode(dev_st7701_t *st7701) {
  if (!dev_st7701_valid(st7701)) {
    return false;
  }

  return _dev_st7701_send_command(st7701, ST7701_CMD_NORON, NULL, 0u);
}

dev_st7701_t *dev_st7701_init(hw_spi_t *spi, hw_gpio_t *reset_pin,
                              hw_gpio_t *backlight_pin, pix_size_t size,
                              const dev_st7701_config_t *config) {
  if (!hw_spi_valid(spi) || !hw_gpio_valid(reset_pin) || size.w == 0u ||
      size.h == 0u) {
    return NULL;
  }

  if (backlight_pin != NULL && !hw_gpio_valid(backlight_pin)) {
    return NULL;
  }

  uint8_t bits_per_word = hw_spi_get_bits_per_word(spi);
  if (bits_per_word != 9u) {
    if (!dev_st7701_configure_spi(spi)) {
      sys_debugf("[st7701] requires 9-bit SPI framing, got %u bits",
                 (unsigned int)bits_per_word);
      return NULL;
    }
  }

  dev_st7701_t *st7701 = &_dev_st7701_singleton;
  if (st7701->init) {
    sys_debugf("[st7701] singleton already initialized");
    return NULL;
  }
  sys_memset(st7701, 0, sizeof(*st7701));
#if defined(SYSTEM_NAME_PICO)
  st7701->parallel_sm = -1;
  st7701->timing_sm = -1;
#endif

  dev_st7701_config_t resolved = _dev_st7701_resolve_config(config);

  st7701->spi = spi;
  st7701->reset_pin = reset_pin;
  st7701->backlight_pin = backlight_pin;
  st7701->backlight_active_low = resolved.backlight_active_low;
  st7701->width = size.w;
  st7701->height = size.h;
  st7701->rotation = resolved.rotation;
  st7701->pinout = resolved.pinout;
  st7701->framebuffer = resolved.framebuffer;
  st7701->palette = resolved.palette;
  st7701->init = true;

  sys_debugf("[st7701] init %ux%u rot=%u bl=%u spi(data=%u clk=%u cs=%u) "
             "timing(dotclk=%u de=%u vsync=%u hsync=%u) fb=%u pal=%u",
             (unsigned int)st7701->width, (unsigned int)st7701->height,
             (unsigned int)st7701->rotation,
             (unsigned int)(st7701->backlight_pin != NULL),
             (unsigned int)st7701->pinout.lcd_spi_data_pin,
             (unsigned int)st7701->pinout.lcd_spi_clk_pin,
             (unsigned int)st7701->pinout.lcd_spi_cs_pin,
             (unsigned int)st7701->pinout.lcd_dot_clk_pin,
             (unsigned int)st7701->pinout.lcd_de_pin,
             (unsigned int)st7701->pinout.vsync_pin,
             (unsigned int)st7701->pinout.hsync_pin,
             (unsigned int)(st7701->framebuffer != NULL),
             (unsigned int)(st7701->palette != NULL));

  sys_debugf("[st7701] backlight active_low=%u",
             (unsigned int)(st7701->backlight_active_low ? 1u : 0u));

  if (st7701->backlight_pin != NULL) {
    hw_gpio_set_mode(st7701->backlight_pin, HW_GPIO_OUTPUT);
    _dev_st7701_set_backlight(st7701, false);
  }

  dev_st7701_reset(st7701);

  if (!_dev_st7701_run_init_sequence(st7701)) {
    dev_st7701_deinit(st7701);
    return NULL;
  }

#if ST7701_ENABLE_PIO_RUNTIME
  if (!_dev_st7701_stage_pio(st7701)) {
    dev_st7701_deinit(st7701);
    return NULL;
  }

  if (!_dev_st7701_start_pio(st7701)) {
    dev_st7701_deinit(st7701);
    return NULL;
  }
#endif

  if (st7701->backlight_pin != NULL) {
    _dev_st7701_set_backlight(st7701, true);
  }

  return st7701;
}

void dev_st7701_deinit(dev_st7701_t *st7701) {
  if (!dev_st7701_valid(st7701)) {
    return;
  }

  sys_debugf("[st7701] deinit");

  _dev_st7701_unstage_pio(st7701);
  sys_memset(st7701, 0, sizeof(*st7701));
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

bool dev_st7701_valid(const dev_st7701_t *st7701) {
  return st7701 != NULL && st7701->init && hw_spi_valid(st7701->spi) &&
         hw_gpio_valid(st7701->reset_pin) && st7701->width != 0u &&
         st7701->height != 0u;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

void dev_st7701_reset(dev_st7701_t *st7701) {
  if (!dev_st7701_valid(st7701)) {
    return;
  }

  hw_gpio_set(st7701->reset_pin, false);
  sys_sleep_ms(10u);
  hw_gpio_set(st7701->reset_pin, true);
  sys_sleep_ms(120u);
}

bool dev_st7701_paint(dev_st7701_t *st7701, const pix_bitmap_t *bitmap) {
  if (!dev_st7701_valid(st7701)) {
    return false;
  }

  st7701->paint_calls++;

#if defined(SYSTEM_NAME_PICO)
  if (st7701->framebuffer != NULL) {
    uint16_t rows = ST7701_PIO_FRAME_ROWS_PER_PAINT;
    if (rows > st7701->height) {
      rows = st7701->height;
    }

    if ((st7701->paint_calls % ST7701_DEBUG_LOG_EVERY_N_PAINT) == 0u) {
      sys_debugf("[st7701] paint fb call=%u next_row=%u rows=%u",
                 (unsigned int)st7701->paint_calls,
                 (unsigned int)st7701->pio_next_row, (unsigned int)rows);
    }

    bool ok = _dev_st7701_run_pio_framebuffer_rgb565(
        st7701, st7701->framebuffer, st7701->pio_next_row, rows);
    if (!ok) {
      sys_debugf("[st7701] paint fb failed call=%u next_row=%u rows=%u",
                 (unsigned int)st7701->paint_calls,
                 (unsigned int)st7701->pio_next_row, (unsigned int)rows);
      return false;
    }

    st7701->pio_next_row = (uint16_t)(st7701->pio_next_row + rows);
    if (st7701->pio_next_row >= st7701->height) {
      st7701->pio_next_row = 0u;
    }
    return true;
  }

  if (bitmap != NULL && bitmap->data != NULL && bitmap->fmt == PIX_FMT_RGBA32) {
    uint32_t rgba = *((const uint32_t *)bitmap->data);
    uint8_t r = (uint8_t)((rgba >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((rgba >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(rgba & 0xFFu);
    uint32_t rgb666 = _dev_st7701_rgb888_to_rgb666(r, g, b);
    if ((st7701->paint_calls % ST7701_DEBUG_LOG_EVERY_N_PAINT) == 0u) {
      sys_debugf("[st7701] paint rgba call=%u rgb666=0x%05x",
                 (unsigned int)st7701->paint_calls, (unsigned int)rgb666);
    }
    return _dev_st7701_run_pio_solid(st7701, rgb666, st7701->height);
  }

  if ((st7701->paint_calls % ST7701_DEBUG_LOG_EVERY_N_PAINT) == 0u) {
    sys_debugf("[st7701] paint smoke call=%u",
               (unsigned int)st7701->paint_calls);
  }

  return _dev_st7701_run_pio_smoke(st7701, ST7701_PIO_SMOKE_ROWS);
#else
  (void)bitmap;
  return false;
#endif
}

bool dev_st7701_paint_rect(dev_st7701_t *st7701, const pix_bitmap_t *bitmap,
                           pix_point_t origin, pix_size_t region_size) {
  if (!dev_st7701_valid(st7701)) {
    return false;
  }

  (void)origin;
  (void)region_size;
  return dev_st7701_paint(st7701, bitmap);
}

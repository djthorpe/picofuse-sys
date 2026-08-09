/**
 * @file
 * @brief Presto ST7701 display bring-up example.
 */

#include <boards/presto.h>
#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define PRESTO_DISPLAY_WIDTH 480u
#define PRESTO_DISPLAY_HEIGHT 480u

// ST7701 serial-control SPI lines (from Presto schematic/pinout notes).
#define PRESTO_LCD_SPI_INDEX 1u
#define PRESTO_LCD_SPI_SCK_PIN 26u
#define PRESTO_LCD_SPI_MOSI_PIN 27u
#define PRESTO_LCD_SPI_CS_PIN 28u
#define PRESTO_LCD_SPI_MISO_DUMMY_PIN 29u
#define PRESTO_LCD_SPI_BAUD_RATE 1000000u

#define PRESTO_LCD_RESET_PIN 44u
#define PRESTO_LCD_BACKLIGHT_PIN 45u

// RGB timing/data mapping for driver diagnostics and future parallel bring-up.
#define PRESTO_LCD_DOT_CLK_PIN 22u
#define PRESTO_LCD_DE_PIN 21u
#define PRESTO_LCD_VSYNC_PIN 20u
#define PRESTO_LCD_HSYNC_PIN 19u

#define PRESTO_LCD_R2_PIN 17u
#define PRESTO_LCD_R3_PIN 16u
#define PRESTO_LCD_R4_PIN 15u
#define PRESTO_LCD_R5_PIN 14u
#define PRESTO_LCD_R6_PIN 13u
#define PRESTO_LCD_R7_PIN 12u

#define PRESTO_LCD_G2_PIN 11u
#define PRESTO_LCD_G3_PIN 10u
#define PRESTO_LCD_G4_PIN 9u
#define PRESTO_LCD_G5_PIN 8u
#define PRESTO_LCD_G6_PIN 7u
#define PRESTO_LCD_G7_PIN 6u

#define PRESTO_LCD_B2_PIN 18u
#define PRESTO_LCD_B3_PIN 5u
#define PRESTO_LCD_B4_PIN 4u
#define PRESTO_LCD_B5_PIN 3u
#define PRESTO_LCD_B6_PIN 2u
#define PRESTO_LCD_B7_PIN 1u

#define PRESTO_DISPLAY_STEP_MS 5000u
#define PRESTO_BITBANG_PROBE 1u
#define PRESTO_USE_PSRAM_FRAMEBUFFER 0u

#define PRESTO_TIMING_H_FRONT 4u
#define PRESTO_TIMING_H_PULSE 16u
#define PRESTO_TIMING_H_BACK 30u
#define PRESTO_TIMING_H_DISPLAY PRESTO_DISPLAY_WIDTH
#define PRESTO_TIMING_H_TOTAL                                                  \
  (PRESTO_TIMING_H_FRONT + PRESTO_TIMING_H_PULSE + PRESTO_TIMING_H_BACK +      \
   PRESTO_TIMING_H_DISPLAY)

#define PRESTO_TIMING_V_PULSE 8u
#define PRESTO_TIMING_V_BACK 5u
#define PRESTO_TIMING_V_DISPLAY PRESTO_DISPLAY_HEIGHT
#define PRESTO_TIMING_V_FRONT 5u
#define PRESTO_TIMING_V_TOTAL                                                  \
  (PRESTO_TIMING_V_PULSE + PRESTO_TIMING_V_BACK + PRESTO_TIMING_V_DISPLAY +    \
   PRESTO_TIMING_V_FRONT)

#define PRESTO_BITBANG_FRAMES_PER_COLOR 24u
#define PRESTO_BITBANG_COLOR_RED 0u
#define PRESTO_BITBANG_COLOR_GREEN 1u
#define PRESTO_BITBANG_COLOR_BLUE 2u
#define PRESTO_BITBANG_COLOR_WHITE 3u
#define PRESTO_BITBANG_COLOR_MODE PRESTO_BITBANG_COLOR_WHITE

#define PRESTO_SOLID_RED 0x00FF0000u
#define PRESTO_SOLID_GREEN 0x0000FF00u
#define PRESTO_SOLID_BLUE 0x000000FFu

#define PRESTO_RGB565_RED 0xF800u
#define PRESTO_RGB565_GREEN 0x07E0u
#define PRESTO_RGB565_BLUE 0x001Fu

#if defined(SYSTEM_NAME_PICO) && PRESTO_USE_PSRAM_FRAMEBUFFER
static uint16_t __attribute__((section(".psram_data"), aligned(4)))
_presto_framebuffer[PRESTO_DISPLAY_WIDTH * PRESTO_DISPLAY_HEIGHT];
#else
static uint16_t
    _presto_framebuffer[PRESTO_DISPLAY_WIDTH * PRESTO_DISPLAY_HEIGHT];
#endif

static void _presto_fill_framebuffer_rgb565(uint16_t color) {
  for (size_t i = 0u;
       i < (sizeof(_presto_framebuffer) / sizeof(_presto_framebuffer[0]));
       i++) {
    _presto_framebuffer[i] = color;
  }
}

#if PRESTO_BITBANG_PROBE
static void _presto_bitbang_set_color(hw_gpio_t *r_pins[6],
                                      hw_gpio_t *g_pins[6],
                                      hw_gpio_t *b_pins[6], bool r, bool g,
                                      bool b) {
  for (uint8_t i = 0u; i < 6u; i++) {
    hw_gpio_set(r_pins[i], r);
    hw_gpio_set(g_pins[i], g);
    hw_gpio_set(b_pins[i], b);
  }
}

static void _presto_bitbang_send_frame(hw_gpio_t *hsync, hw_gpio_t *vsync,
                                       hw_gpio_t *de, hw_gpio_t *dotclk,
                                       hw_gpio_t *r_pins[6],
                                       hw_gpio_t *g_pins[6],
                                       hw_gpio_t *b_pins[6], bool red,
                                       bool green, bool blue) {
  for (uint16_t y = 0u; y < PRESTO_TIMING_V_TOTAL; y++) {
    bool in_vsync_pulse = (y < PRESTO_TIMING_V_PULSE);
    bool in_active_rows =
        (y >= (PRESTO_TIMING_V_PULSE + PRESTO_TIMING_V_BACK)) &&
        (y < (PRESTO_TIMING_V_PULSE + PRESTO_TIMING_V_BACK +
              PRESTO_TIMING_V_DISPLAY));

    hw_gpio_set(vsync, !in_vsync_pulse);

    for (uint16_t x = 0u; x < PRESTO_TIMING_H_TOTAL; x++) {
      bool in_hsync_pulse =
          (x >= PRESTO_TIMING_H_FRONT) &&
          (x < (PRESTO_TIMING_H_FRONT + PRESTO_TIMING_H_PULSE));
      bool in_active_cols =
          (x >= (PRESTO_TIMING_H_FRONT + PRESTO_TIMING_H_PULSE +
                 PRESTO_TIMING_H_BACK)) &&
          (x < (PRESTO_TIMING_H_FRONT + PRESTO_TIMING_H_PULSE +
                PRESTO_TIMING_H_BACK + PRESTO_TIMING_H_DISPLAY));

      bool active_pixel = in_active_rows && in_active_cols;
      hw_gpio_set(hsync, !in_hsync_pulse);
      hw_gpio_set(de, active_pixel);

      if (active_pixel) {
        _presto_bitbang_set_color(r_pins, g_pins, b_pins, red, green, blue);
      } else {
        _presto_bitbang_set_color(r_pins, g_pins, b_pins, false, false, false);
      }

      hw_gpio_set(dotclk, false);
      hw_gpio_set(dotclk, true);
    }
  }
}
#endif

int main(void) {
  sys_init();
  hw_init();

  sys_printf("[presto-display] boot\n");

  hw_gpio_t *spi_sck = hw_gpio_init(0u, PRESTO_LCD_SPI_SCK_PIN, HW_GPIO_SPI);
  hw_gpio_t *spi_mosi = hw_gpio_init(0u, PRESTO_LCD_SPI_MOSI_PIN, HW_GPIO_SPI);
  hw_gpio_t *spi_miso_dummy =
      hw_gpio_init(0u, PRESTO_LCD_SPI_MISO_DUMMY_PIN, HW_GPIO_SPI);
  hw_gpio_t *spi_cs = hw_gpio_init(0u, PRESTO_LCD_SPI_CS_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *lcd_reset = hw_gpio_init(0u, PRESTO_LCD_RESET_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *lcd_backlight =
      hw_gpio_init(0u, PRESTO_LCD_BACKLIGHT_PIN, HW_GPIO_OUTPUT);

  if (!hw_gpio_valid(spi_sck) || !hw_gpio_valid(spi_mosi) ||
      !hw_gpio_valid(spi_miso_dummy) || !hw_gpio_valid(spi_cs) ||
      !hw_gpio_valid(lcd_reset) || !hw_gpio_valid(lcd_backlight)) {
    sys_printf("[presto-display] gpio init failed\n");
    hw_exit();
    sys_exit();
    return 1;
  }

  hw_spi_config_t spi_cfg = {
      .cs_active_low = true,
      .mode = HW_SPI_MODE_0,
      .bits_per_word = 8u,
  };

  hw_spi_t *spi =
      hw_spi_init(PRESTO_LCD_SPI_INDEX, spi_sck, spi_mosi, spi_miso_dummy,
                  spi_cs, PRESTO_LCD_SPI_BAUD_RATE, &spi_cfg);
  if (!hw_spi_valid(spi)) {
    sys_printf("[presto-display] spi init failed\n");
    hw_exit();
    sys_exit();
    return 1;
  }

  dev_st7701_config_t display_cfg = {
      .rotation = DEV_ST7701_ROTATION_0,
      .backlight_active_low = false,
      .pinout =
          {
              .lcd_spi_data_pin = PRESTO_LCD_SPI_MOSI_PIN,
              .lcd_spi_clk_pin = PRESTO_LCD_SPI_SCK_PIN,
              .lcd_spi_cs_pin = PRESTO_LCD_SPI_CS_PIN,
              .lcd_dot_clk_pin = PRESTO_LCD_DOT_CLK_PIN,
              .lcd_de_pin = PRESTO_LCD_DE_PIN,
              .vsync_pin = PRESTO_LCD_VSYNC_PIN,
              .hsync_pin = PRESTO_LCD_HSYNC_PIN,
              .r2_pin = PRESTO_LCD_R2_PIN,
              .r3_pin = PRESTO_LCD_R3_PIN,
              .r4_pin = PRESTO_LCD_R4_PIN,
              .r5_pin = PRESTO_LCD_R5_PIN,
              .r6_pin = PRESTO_LCD_R6_PIN,
              .r7_pin = PRESTO_LCD_R7_PIN,
              .g2_pin = PRESTO_LCD_G2_PIN,
              .g3_pin = PRESTO_LCD_G3_PIN,
              .g4_pin = PRESTO_LCD_G4_PIN,
              .g5_pin = PRESTO_LCD_G5_PIN,
              .g6_pin = PRESTO_LCD_G6_PIN,
              .g7_pin = PRESTO_LCD_G7_PIN,
              .b2_pin = PRESTO_LCD_B2_PIN,
              .b3_pin = PRESTO_LCD_B3_PIN,
              .b4_pin = PRESTO_LCD_B4_PIN,
              .b5_pin = PRESTO_LCD_B5_PIN,
              .b6_pin = PRESTO_LCD_B6_PIN,
              .b7_pin = PRESTO_LCD_B7_PIN,
          },
      .framebuffer = _presto_framebuffer,
      .palette = NULL,
  };

  dev_st7701_t *display = dev_st7701_init(
      spi, lcd_reset, lcd_backlight,
      (pix_size_t){PRESTO_DISPLAY_WIDTH, PRESTO_DISPLAY_HEIGHT}, &display_cfg);

  if (!dev_st7701_valid(display)) {
    sys_printf("[presto-display] st7701 init failed\n");
    hw_spi_deinit(spi);
    hw_exit();
    sys_exit();
    return 1;
  }

  sys_printf("[presto-display] st7701 init ok (%ux%u)\n",
             (unsigned int)PRESTO_DISPLAY_WIDTH,
             (unsigned int)PRESTO_DISPLAY_HEIGHT);
  sys_printf("[presto-display] spi bits=%u (expected 9)\n",
             (unsigned int)hw_spi_get_bits_per_word(spi));
  sys_printf("[presto-display] paint demo: red -> green -> blue\n");
  sys_printf("[presto-display] step time: %u ms\n",
             (unsigned int)PRESTO_DISPLAY_STEP_MS);
#if PRESTO_USE_PSRAM_FRAMEBUFFER
  sys_printf("[presto-display] framebuffer: PSRAM\n");
#else
  sys_printf("[presto-display] framebuffer: SRAM\n");
#endif

#if PRESTO_BITBANG_PROBE
  sys_printf("[presto-display] bitbang probe mode enabled\n");

  dev_st7701_deinit(display);

  hw_gpio_t *hsync = hw_gpio_init(0u, PRESTO_LCD_HSYNC_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *vsync = hw_gpio_init(0u, PRESTO_LCD_VSYNC_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *de = hw_gpio_init(0u, PRESTO_LCD_DE_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *dotclk = hw_gpio_init(0u, PRESTO_LCD_DOT_CLK_PIN, HW_GPIO_OUTPUT);

  hw_gpio_t *r_pins[6] = {
      hw_gpio_init(0u, PRESTO_LCD_R2_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_R3_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_R4_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_R5_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_R6_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_R7_PIN, HW_GPIO_OUTPUT),
  };
  hw_gpio_t *g_pins[6] = {
      hw_gpio_init(0u, PRESTO_LCD_G2_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_G3_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_G4_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_G5_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_G6_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_G7_PIN, HW_GPIO_OUTPUT),
  };
  hw_gpio_t *b_pins[6] = {
      hw_gpio_init(0u, PRESTO_LCD_B2_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_B3_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_B4_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_B5_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_B6_PIN, HW_GPIO_OUTPUT),
      hw_gpio_init(0u, PRESTO_LCD_B7_PIN, HW_GPIO_OUTPUT),
  };

  bool pins_ok = hw_gpio_valid(hsync) && hw_gpio_valid(vsync) &&
                 hw_gpio_valid(de) && hw_gpio_valid(dotclk);
  for (uint8_t i = 0u; i < 6u; i++) {
    pins_ok = pins_ok && hw_gpio_valid(r_pins[i]) && hw_gpio_valid(g_pins[i]) &&
              hw_gpio_valid(b_pins[i]);
  }

  if (!pins_ok) {
    sys_printf("[presto-display] bitbang gpio init failed\n");
    hw_spi_deinit(spi);
    hw_exit();
    sys_exit();
    return 1;
  }

  hw_gpio_set(vsync, true);
  hw_gpio_set(hsync, true);
  hw_gpio_set(de, true);
  hw_gpio_set(dotclk, false);

  bool bb_r = false;
  bool bb_g = false;
  bool bb_b = false;
#if PRESTO_BITBANG_COLOR_MODE == PRESTO_BITBANG_COLOR_RED
  bb_r = true;
#elif PRESTO_BITBANG_COLOR_MODE == PRESTO_BITBANG_COLOR_GREEN
  bb_g = true;
#elif PRESTO_BITBANG_COLOR_MODE == PRESTO_BITBANG_COLOR_BLUE
  bb_b = true;
#else
  bb_r = true;
  bb_g = true;
  bb_b = true;
#endif

  sys_printf("[presto-display] bitbang constant color r=%u g=%u b=%u\n",
             (unsigned int)(bb_r ? 1u : 0u), (unsigned int)(bb_g ? 1u : 0u),
             (unsigned int)(bb_b ? 1u : 0u));

  while (true) {
    for (uint32_t i = 0u; i < PRESTO_BITBANG_FRAMES_PER_COLOR; i++) {
      _presto_bitbang_send_frame(hsync, vsync, de, dotclk, r_pins, g_pins,
                                 b_pins, bb_r, bb_g, bb_b);
    }
  }
#endif

  if (!dev_st7701_set_normal_mode(display)) {
    sys_printf("[presto-display] normal mode command failed\n");
  }
  if (!dev_st7701_set_inversion(display, false)) {
    sys_printf("[presto-display] inversion off command failed\n");
  }

  static const uint32_t colors[] = {
      PRESTO_SOLID_RED,
      PRESTO_SOLID_GREEN,
      PRESTO_SOLID_BLUE,
  };
  static const char *color_names[] = {
      "red",
      "green",
      "blue",
  };

  uint32_t step = 0u;
  while (true) {
    uint32_t idx = step % (sizeof(colors) / sizeof(colors[0]));
    uint32_t color = colors[idx];
    uint16_t color565 = PRESTO_RGB565_RED;

    if (idx == 1u) {
      color565 = PRESTO_RGB565_GREEN;
    } else if (idx == 2u) {
      color565 = PRESTO_RGB565_BLUE;
    }

    _presto_fill_framebuffer_rgb565(color565);

    pix_bitmap_t bitmap = {
        .data = &color,
        .size = {1u, 1u},
        .stride = sizeof(color),
        .fmt = PIX_FMT_RGBA32,
    };

    sys_printf("[presto-display] painting %s for %u ms\n", color_names[idx],
               (unsigned int)PRESTO_DISPLAY_STEP_MS);

    if (!dev_st7701_paint(display, &bitmap)) {
      sys_printf("[presto-display] paint failed (%s)\n", color_names[idx]);
    }

    sys_sleep_ms(PRESTO_DISPLAY_STEP_MS);

    step++;
  }

  dev_st7701_deinit(display);
  hw_spi_deinit(spi);
  hw_exit();
  sys_exit();
  return 0;
}

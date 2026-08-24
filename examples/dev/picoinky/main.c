/**
 * @file
 * @brief Pico Inky GPIO callback bring-up example.
 */

#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/pix.h>
#include <picofuse/sys.h>

#define SWA_PIN 12u
#define SWB_PIN 13u
#define SWC_PIN 14u

#define SPI_MISO_PIN 16u
#define SPI_CS_PIN 17u
#define SPI_SCLK_PIN 18u
#define SPI_MOSI_PIN 19u

#define INKY_DC_PIN 20u
#define INKY_RESET_PIN 21u
#define INKY_BUSY_PIN 26u

#define SPI_INDEX 0u
#define SPI_BAUD_RATE 4000000u

#define INKY_WIDTH 296u
#define INKY_HEIGHT 128u

#define DEBUG_HEARTBEAT_MS 1000u
#define RESET_PULSE_MS 10u

static volatile uint32_t _picoinky_rise_events;
static volatile uint32_t _picoinky_fall_events;

static uint32_t _picoinky_flush_gpio_events(void) {
  uint32_t rise = _picoinky_rise_events;
  uint32_t fall = _picoinky_fall_events;

  _picoinky_rise_events = 0u;
  _picoinky_fall_events = 0u;

  const uint8_t pins[] = {SWA_PIN, SWB_PIN, SWC_PIN, INKY_BUSY_PIN};
  for (size_t i = 0u; i < sizeof(pins) / sizeof(pins[0]); ++i) {
    uint32_t mask = (1u << pins[i]);
    bool rising = (rise & mask) != 0u;
    bool falling = (fall & mask) != 0u;
    if (!rising && !falling) {
      continue;
    }

    sys_debugf("[dev/picoinky] gpio-event bank=0 pin=%u rise=%u fall=%u",
               pins[i], (unsigned int)rising, (unsigned int)falling);
  }

  return fall;
}

static bool _picoinky_paint_box(dev_uc8151_t *display, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h, bool black) {
  size_t stride = (size_t)h / 8u;
  void *buf = sys_calloc((size_t)w * stride, 1u);
  if (buf == NULL) {
    return false;
  }
  if (black) {
    sys_memset(buf, 0xFF, (size_t)w * stride);
  }
  pix_bitmap_t bitmap = {
      .data = buf,
      .size = {.w = w, .h = h},
      .stride = stride,
      .fmt = PIX_FMT_MONO,
  };
  bool ok = dev_uc8151_paint_rect(display, &bitmap, (pix_point_t){x, y},
                                  (pix_size_t){w, h});
  sys_free(buf);
  return ok;
}

static void _picoinky_gpio_event_cb(uint8_t bank, uint8_t pin,
                                    hw_gpio_event_t event, void *userdata) {
  (void)userdata;
  (void)bank;

  if (pin != SWA_PIN && pin != SWB_PIN && pin != SWC_PIN &&
      pin != INKY_BUSY_PIN) {
    return;
  }

  uint32_t mask = (1u << pin);
  if ((event & HW_GPIO_RISING) != 0u) {
    _picoinky_rise_events |= mask;
  }
  if ((event & HW_GPIO_FALLING) != 0u) {
    _picoinky_fall_events |= mask;
  }
}

int main(void) {
  sys_init();
  hw_init();
  sys_debugf("[dev/picoinky] boot");

  hw_gpio_t *swa = hw_gpio_init(0u, SWA_PIN, HW_GPIO_PULLUP);
  hw_gpio_t *swb = hw_gpio_init(0u, SWB_PIN, HW_GPIO_PULLUP);
  hw_gpio_t *swc = hw_gpio_init(0u, SWC_PIN, HW_GPIO_PULLUP);
  hw_gpio_t *spi_miso = hw_gpio_init(0u, SPI_MISO_PIN, HW_GPIO_SPI);
  hw_gpio_t *spi_cs = hw_gpio_init(0u, SPI_CS_PIN, HW_GPIO_SPI);
  hw_gpio_t *spi_sclk = hw_gpio_init(0u, SPI_SCLK_PIN, HW_GPIO_SPI);
  hw_gpio_t *spi_mosi = hw_gpio_init(0u, SPI_MOSI_PIN, HW_GPIO_SPI);
  hw_gpio_t *inky_dc = hw_gpio_init(0u, INKY_DC_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *inky_reset = hw_gpio_init(0u, INKY_RESET_PIN, HW_GPIO_OUTPUT);
  hw_gpio_t *inky_busy = hw_gpio_init(0u, INKY_BUSY_PIN, HW_GPIO_INPUT);

  if (!hw_gpio_valid(swa) || !hw_gpio_valid(swb) || !hw_gpio_valid(swc) ||
      !hw_gpio_valid(spi_miso) || !hw_gpio_valid(spi_cs) ||
      !hw_gpio_valid(spi_sclk) || !hw_gpio_valid(spi_mosi) ||
      !hw_gpio_valid(inky_dc) || !hw_gpio_valid(inky_reset) ||
      !hw_gpio_valid(inky_busy)) {
    sys_debugf("[dev/picoinky] GPIO init failed");
    hw_exit();
    sys_exit();
    return 1;
  }

  sys_debugf("[picoinky] set dc=0 begin");
  hw_gpio_set(inky_dc, false);
  sys_debugf("[picoinky] set dc=0 end read=%u",
             (unsigned int)hw_gpio_get(inky_dc));

  sys_debugf("[picoinky] set reset=1 begin");
  hw_gpio_set(inky_reset, true);
  sys_debugf("[picoinky] set reset=1 end read=%u",
             (unsigned int)hw_gpio_get(inky_reset));

  hw_spi_t *spi = hw_spi_init(SPI_INDEX, spi_sclk, spi_mosi, spi_miso, spi_cs,
                              SPI_BAUD_RATE, NULL);
  if (!hw_spi_valid(spi)) {
    sys_debugf("[picoinky] SPI init failed");
    hw_exit();
    sys_exit();
    return 1;
  }
  sys_debugf("[picoinky] spi-only mode init ok");

  sys_debugf("[picoinky] gpio callback deferred until after reset probe");

  sys_debugf("[picoinky] busy probe start raw=%u active_low_busy=%u",
             (unsigned int)hw_gpio_get(inky_busy),
             (unsigned int)!hw_gpio_get(inky_busy));
  hw_gpio_set(inky_reset, false);
  sys_sleep_ms(RESET_PULSE_MS);
  _picoinky_flush_gpio_events();
  sys_debugf("[picoinky] busy during reset raw=%u active_low_busy=%u",
             (unsigned int)hw_gpio_get(inky_busy),
             (unsigned int)!hw_gpio_get(inky_busy));
  hw_gpio_set(inky_reset, true);
  sys_sleep_ms(RESET_PULSE_MS);
  _picoinky_flush_gpio_events();
  sys_debugf("[picoinky] busy after reset raw=%u active_low_busy=%u",
             (unsigned int)hw_gpio_get(inky_busy),
             (unsigned int)!hw_gpio_get(inky_busy));

  hw_gpio_set_callback(_picoinky_gpio_event_cb, NULL);
  sys_debugf("[picoinky] gpio callback enabled (pins %u,%u,%u,%u)",
             (unsigned int)SWA_PIN, (unsigned int)SWB_PIN,
             (unsigned int)SWC_PIN, (unsigned int)INKY_BUSY_PIN);

  sys_debugf("[picoinky] uc8151 init begin");
  dev_uc8151_t *display =
      dev_uc8151_init(spi, inky_dc, inky_reset, inky_busy,
                      (pix_size_t){INKY_WIDTH, INKY_HEIGHT}, NULL);
  if (display == NULL) {
    sys_debugf("[picoinky] uc8151 init failed");
  } else {
    sys_debugf("[picoinky] uc8151 init ok");

    size_t stride = ((size_t)INKY_HEIGHT + 7u) / 8u;
    size_t data_size = (size_t)INKY_WIDTH * stride;
    void *fb = sys_malloc(data_size);
    if (fb != NULL) {
      sys_memset(fb, 0xFF, data_size);
      pix_bitmap_t bitmap = {
          .data = fb,
          .size = {.w = INKY_WIDTH, .h = INKY_HEIGHT},
          .stride = stride,
          .fmt = PIX_FMT_MONO,
      };
      sys_debugf("[picoinky] painting black");
      bool ok = dev_uc8151_paint(display, &bitmap);
      sys_debugf("[picoinky] paint %s", ok ? "ok" : "failed");

      // Partial draws on top of the black background.
      // Constraints: origin.y and region height must be multiples of 8.
      // All-zero bits = white for UC8151 DTM2.

      // Patch 1: white 40x64 block at top-left (x=8, y=0)
      uint16_t p1w = 40u, p1h = 64u;
      size_t p1_stride = p1h / 8u;
      void *p1 = sys_calloc((size_t)p1w * p1_stride, 1u);
      if (p1 != NULL) {
        pix_bitmap_t pf1 = {
            .data = p1,
            .size = {.w = p1w, .h = p1h},
            .stride = p1_stride,
            .fmt = PIX_FMT_MONO,
        };
        ok = dev_uc8151_paint_rect(display, &pf1, (pix_point_t){8, 32},
                                   (pix_size_t){p1w, p1h});
        sys_debugf("[picoinky] patch1 %s", ok ? "ok" : "failed");
        sys_free(p1);
      }

      // Patch 2: white 80x32 block centred (x=108, y=48)
      uint16_t p2w = 80u, p2h = 32u;
      size_t p2_stride = p2h / 8u;
      void *p2 = sys_calloc((size_t)p2w * p2_stride, 1u);
      if (p2 != NULL) {
        pix_bitmap_t pf2 = {
            .data = p2,
            .size = {.w = p2w, .h = p2h},
            .stride = p2_stride,
            .fmt = PIX_FMT_MONO,
        };
        ok = dev_uc8151_paint_rect(display, &pf2, (pix_point_t){108, 48},
                                   (pix_size_t){p2w, p2h});
        sys_debugf("[picoinky] patch2 %s", ok ? "ok" : "failed");
        sys_free(p2);
      }

      // Patch 3: white 40x64 block at top-right (x=248, y=0)
      uint16_t p3w = 40u, p3h = 64u;
      size_t p3_stride = p3h / 8u;
      void *p3 = sys_calloc((size_t)p3w * p3_stride, 1u);
      if (p3 != NULL) {
        pix_bitmap_t pf3 = {
            .data = p3,
            .size = {.w = p3w, .h = p3h},
            .stride = p3_stride,
            .fmt = PIX_FMT_MONO,
        };
        ok = dev_uc8151_paint_rect(display, &pf3, (pix_point_t){248, 32},
                                   (pix_size_t){p3w, p3h});
        sys_debugf("[picoinky] patch3 %s", ok ? "ok" : "failed");
        sys_free(p3);
      }

      sys_free(fb);
    } else {
      sys_debugf("[picoinky] alloc failed");
    }
  }

  sys_debugf("[picoinky] gpio-only mode busy_raw=%u active_low_busy=%u",
             (unsigned int)hw_gpio_get(inky_busy),
             (unsigned int)!hw_gpio_get(inky_busy));

  bool patch1_black = false;
  bool patch2_black = false;
  bool patch3_black = false;

  while (true) {
    sys_sleep_ms(DEBUG_HEARTBEAT_MS);
    uint32_t pressed = _picoinky_flush_gpio_events();

    if ((pressed >> SWA_PIN) & 1u) {
      patch1_black = !patch1_black;
      sys_debugf("[picoinky] SWA pressed, patch1 -> %s",
                 patch1_black ? "black" : "white");
      _picoinky_paint_box(display, 8, 32, 40, 64, patch1_black);
    }
    if ((pressed >> SWB_PIN) & 1u) {
      patch2_black = !patch2_black;
      sys_debugf("[picoinky] SWB pressed, patch2 -> %s",
                 patch2_black ? "black" : "white");
      _picoinky_paint_box(display, 108, 48, 80, 32, patch2_black);
    }
    if ((pressed >> SWC_PIN) & 1u) {
      patch3_black = !patch3_black;
      sys_debugf("[picoinky] SWC pressed, patch3 -> %s",
                 patch3_black ? "black" : "white");
      _picoinky_paint_box(display, 248, 32, 40, 64, patch3_black);
    }

    sys_debugf("[picoinky] alive busy_raw=%u active_low_busy=%u sw=%u%u%u",
               (unsigned int)hw_gpio_get(inky_busy),
               (unsigned int)!hw_gpio_get(inky_busy),
               (unsigned int)hw_gpio_get(swa), (unsigned int)hw_gpio_get(swb),
               (unsigned int)hw_gpio_get(swc));
  }

  return 0;
}

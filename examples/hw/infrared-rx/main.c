/**
 * @file
 * @brief Infrared receiver raw frame logger on GPIO27.
 */

#include "nec.h"
#include "sony.h"

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define EXAMPLE_IR_BANK 0u
#define EXAMPLE_IR_GPIO 27u

static const nec_variant_t _variants[] = {
    NEC_VARIANT_APPLETV,
    NEC_VARIANT_NEC32,
    NEC_VARIANT_NEC16,
};

#define EXAMPLE_VARIANT_COUNT (sizeof(_variants) / sizeof(_variants[0]))

static const sony_variant_t _sony_variants[] = {
    SONY_VARIANT_20,
    SONY_VARIANT_15,
    SONY_VARIANT_12,
};

#define EXAMPLE_SONY_VARIANT_COUNT                                             \
  (sizeof(_sony_variants) / sizeof(_sony_variants[0]))

typedef struct {
  nec_rx_t decoders[EXAMPLE_VARIANT_COUNT];
  sony_rx_t sony_decoders[EXAMPLE_SONY_VARIANT_COUNT];
  bool has_last;
  nec_variant_t last_variant;
  bool last_is_sony;
  sony_variant_t last_sony_variant;
  uint16_t last_sony_device;
  uint8_t last_sony_command;
  uint16_t last_address;
  uint8_t last_command;
  uint32_t last_raw;
} app_state_t;

static const char *variant_name(nec_variant_t variant) {
  switch (variant) {
  case NEC_VARIANT_APPLETV:
    return "appletv";
  case NEC_VARIANT_NEC32:
    return "nec32";
  case NEC_VARIANT_NEC16:
    return "nec16";
  default:
    return "unknown";
  }
}

static const char *sony_variant_name(sony_variant_t variant) {
  switch (variant) {
  case SONY_VARIANT_12:
    return "sony12";
  case SONY_VARIANT_15:
    return "sony15";
  case SONY_VARIANT_20:
    return "sony20";
  default:
    return "sony";
  }
}

static void infrared_callback(hw_infrared_event_t event, uint32_t duration_us,
                              void *user_data) {
  app_state_t *app = user_data;
  if (app == NULL) {
    return;
  }

  bool repeat_seen = false;
  bool raw_frame_seen = false;
  uint32_t raw_frame = 0u;
  bool frame_seen = false;
  nec_variant_t matched_variant = NEC_VARIANT_NEC32;
  uint16_t matched_address = 0u;
  uint8_t matched_command = 0u;
  uint32_t matched_raw = 0u;
  bool sony_seen = false;
  sony_variant_t matched_sony_variant = SONY_VARIANT_12;
  uint16_t matched_sony_device = 0u;
  uint8_t matched_sony_command = 0u;
  uint32_t matched_sony_raw = 0u;

  for (size_t i = 0; i < EXAMPLE_SONY_VARIANT_COUNT; i++) {
    sony_frame_t sony_frame = {0};
    sony_rx_status_t sony_status =
        sony_rx_decode(&app->sony_decoders[i], event, duration_us, &sony_frame);

    if (sony_status == SONY_RX_STATUS_FRAME && !sony_seen) {
      sony_seen = true;
      matched_sony_variant = sony_frame.variant;
      matched_sony_device = sony_frame.device;
      matched_sony_command = sony_frame.command;
      matched_sony_raw = sony_frame.raw;
    }
  }

  if (sony_seen) {
    app->has_last = true;
    app->last_is_sony = true;
    app->last_sony_variant = matched_sony_variant;
    app->last_sony_device = matched_sony_device;
    app->last_sony_command = matched_sony_command;
    app->last_raw = matched_sony_raw;

    sys_printf("[raw] 0x%08x\n", (unsigned int)matched_sony_raw);
    sys_printf("[%s] device=%u scancode=%u raw=0x%08x\n",
               sony_variant_name(matched_sony_variant),
               (unsigned int)matched_sony_device,
               (unsigned int)matched_sony_command,
               (unsigned int)matched_sony_raw);
    return;
  }

  for (size_t i = 0; i < EXAMPLE_VARIANT_COUNT; i++) {
    nec_frame_t frame = {0};
    nec_rx_status_t status =
        nec_rx_decode(&app->decoders[i], event, duration_us, &frame);

    if (status == NEC_RX_STATUS_REPEAT) {
      repeat_seen = true;
      continue;
    }

    if (status == NEC_RX_STATUS_FRAME && !raw_frame_seen) {
      raw_frame_seen = true;
      raw_frame = frame.raw;
    }

    uint16_t address = 0u;
    uint8_t command = 0u;
    if (!nec_frame_extract(&frame, _variants[i], &address, &command)) {
      continue;
    }

    if (!frame_seen) {
      frame_seen = true;
      matched_variant = _variants[i];
      matched_address = address;
      matched_command = command;
      matched_raw = frame.raw;
    }
  }

  if (frame_seen) {
    app->has_last = true;
    app->last_is_sony = false;
    app->last_variant = matched_variant;
    app->last_address = matched_address;
    app->last_command = matched_command;
    app->last_raw = matched_raw;

    sys_printf("[raw] 0x%08x\n", (unsigned int)matched_raw);
    sys_printf("[%s] addr=0x%04x cmd=0x%02x raw=0x%08x\n",
               variant_name(matched_variant), (unsigned int)matched_address,
               (unsigned int)matched_command, (unsigned int)matched_raw);
    return;
  }

  if (raw_frame_seen) {
    app->has_last = true;
    app->last_raw = raw_frame;
    sys_printf("[raw] 0x%08x\n", (unsigned int)raw_frame);
    return;
  }

  if (repeat_seen) {
    if (app->has_last) {
      if (app->last_is_sony) {
        sys_printf("[%s] device=%u scancode=%u raw=0x%08x repeat\n",
                   sony_variant_name(app->last_sony_variant),
                   (unsigned int)app->last_sony_device,
                   (unsigned int)app->last_sony_command,
                   (unsigned int)app->last_raw);
      } else if (app->last_variant == NEC_VARIANT_APPLETV ||
                 app->last_variant == NEC_VARIANT_NEC32 ||
                 app->last_variant == NEC_VARIANT_NEC16) {
        sys_printf(
            "[%s] addr=0x%04x cmd=0x%02x raw=0x%08x repeat\n",
            variant_name(app->last_variant), (unsigned int)app->last_address,
            (unsigned int)app->last_command, (unsigned int)app->last_raw);
      } else {
        sys_printf("[raw] 0x%08x repeat\n", (unsigned int)app->last_raw);
      }
    } else {
      sys_printf("[raw] repeat\n");
    }
  }
}

int main(void) {
  app_state_t app = {0};

  sys_init();
  hw_init();

  for (size_t i = 0; i < EXAMPLE_VARIANT_COUNT; i++) {
    nec_rx_init_variant(&app.decoders[i], _variants[i]);
  }

  for (size_t i = 0; i < EXAMPLE_SONY_VARIANT_COUNT; i++) {
    sony_rx_init_variant(&app.sony_decoders[i], _sony_variants[i]);
  }

  hw_infrared_rx_t *rx = hw_infrared_rx_init(EXAMPLE_IR_BANK, EXAMPLE_IR_GPIO,
                                             infrared_callback, &app);
  if (rx == NULL) {
    sys_printf("Infrared receiver backend is unavailable on this platform\n");
    hw_exit();
    sys_exit();
    return 0;
  }

  sys_printf("Infrared RX listening on GPIO%u\n",
             (unsigned int)EXAMPLE_IR_GPIO);

  for (;;) {
    hw_poll();
    sys_sleep_ms(10u);
  }

  return 0;
}

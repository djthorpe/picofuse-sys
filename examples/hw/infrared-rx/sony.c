#include "sony.h"

#define SONY_HEADER_MARK_US 2400u
#define SONY_BIT_SPACE_US 600u
#define SONY_ONE_MARK_US 1200u
#define SONY_ZERO_MARK_US 600u
#define SONY_REPEAT_PERIOD_US 45000u
#define SONY_TRAILER_MIN_US 10000u

enum {
  SONY_RX_STATE_IDLE = 0,
  SONY_RX_STATE_HEADER_SPACE = 1,
  SONY_RX_STATE_BIT_MARK = 2,
  SONY_RX_STATE_BIT_SPACE = 3,
};

static inline bool sony_match_us(uint32_t measured, uint32_t expected) {
  const uint32_t low = (expected * 65u) / 100u;
  const uint32_t high = (expected * 135u) / 100u;
  return measured >= low && measured <= high;
}

static inline void sony_rx_reset(sony_rx_t *rx) {
  rx->state = SONY_RX_STATE_IDLE;
  rx->bits = 0u;
  rx->value = 0u;
  rx->pending_bit = 0u;
}

static inline void sony_fill_frame(const sony_rx_t *rx, sony_frame_t *frame) {
  frame->variant = rx->variant;
  frame->raw = rx->value;
  frame->command = (uint8_t)(rx->value & UINT32_C(0x7F));

  if (rx->variant == SONY_VARIANT_12) {
    frame->device = (uint16_t)((rx->value >> 7) & UINT32_C(0x1F));
  } else if (rx->variant == SONY_VARIANT_15) {
    frame->device = (uint16_t)((rx->value >> 7) & UINT32_C(0xFF));
  } else {
    frame->device = (uint16_t)((rx->value >> 7) & UINT32_C(0x1FFF));
  }
}

void sony_rx_init(sony_rx_t *rx) { sony_rx_init_variant(rx, SONY_VARIANT_12); }

void sony_rx_init_variant(sony_rx_t *rx, sony_variant_t variant) {
  if (rx == NULL) {
    return;
  }

  rx->variant = variant;
  sony_rx_reset(rx);
}

sony_rx_status_t sony_rx_decode(sony_rx_t *rx, hw_infrared_event_t event,
                                uint32_t duration_us, sony_frame_t *frame_out) {
  if (rx == NULL) {
    return SONY_RX_STATUS_ERROR;
  }

  if (event == HW_INFRARED_EVENT_TIMEOUT) {
    // Accept timeout immediately after the final mark when the closing space
    // was not observed as a separate event.
    if (rx->state == SONY_RX_STATE_BIT_SPACE &&
        rx->bits + 1u == (uint8_t)rx->variant && frame_out != NULL) {
      uint32_t value = rx->value << 1;
      if (rx->pending_bit != 0u) {
        value |= UINT32_C(1);
      }

      sony_rx_t tmp = *rx;
      tmp.value = value;
      tmp.bits = (uint8_t)rx->variant;
      sony_fill_frame(&tmp, frame_out);
      sony_rx_reset(rx);
      return SONY_RX_STATUS_FRAME;
    }

    if (rx->bits == (uint8_t)rx->variant && frame_out != NULL) {
      sony_fill_frame(rx, frame_out);
      sony_rx_reset(rx);
      return SONY_RX_STATUS_FRAME;
    }

    sony_rx_reset(rx);
    return SONY_RX_STATUS_NONE;
  }

  switch (rx->state) {
  case SONY_RX_STATE_IDLE:
    if (event == HW_INFRARED_EVENT_MARK &&
        sony_match_us(duration_us, SONY_HEADER_MARK_US)) {
      rx->state = SONY_RX_STATE_HEADER_SPACE;
      rx->bits = 0u;
      rx->value = 0u;
      rx->pending_bit = 0u;
    }
    return SONY_RX_STATUS_NONE;

  case SONY_RX_STATE_HEADER_SPACE:
    if (event == HW_INFRARED_EVENT_SPACE &&
        sony_match_us(duration_us, SONY_BIT_SPACE_US)) {
      rx->state = SONY_RX_STATE_BIT_MARK;
      return SONY_RX_STATUS_NONE;
    }

    sony_rx_reset(rx);
    return SONY_RX_STATUS_ERROR;

  case SONY_RX_STATE_BIT_MARK:
    if (event != HW_INFRARED_EVENT_MARK) {
      sony_rx_reset(rx);
      return SONY_RX_STATUS_ERROR;
    }

    if (sony_match_us(duration_us, SONY_ONE_MARK_US)) {
      rx->pending_bit = 1u;
    } else if (sony_match_us(duration_us, SONY_ZERO_MARK_US)) {
      rx->pending_bit = 0u;
    } else {
      sony_rx_reset(rx);
      return SONY_RX_STATUS_ERROR;
    }

    rx->state = SONY_RX_STATE_BIT_SPACE;
    return SONY_RX_STATUS_NONE;

  case SONY_RX_STATE_BIT_SPACE:
    if (event != HW_INFRARED_EVENT_SPACE) {
      sony_rx_reset(rx);
      return SONY_RX_STATUS_ERROR;
    }

    if (rx->pending_bit != 0u) {
      rx->value |= (UINT32_C(1) << rx->bits);
    }

    rx->bits++;
    if (rx->bits > (uint8_t)rx->variant) {
      sony_rx_reset(rx);
      return SONY_RX_STATUS_ERROR;
    }

    if (rx->bits == (uint8_t)rx->variant) {
      // Do not finalize on a nominal inter-bit space because shorter Sony
      // formats are prefixes of longer formats (12 vs 15 vs 20).
      // Finalize only when we see an end-of-frame gap here, or on timeout.
      if (duration_us >= SONY_TRAILER_MIN_US) {
        if (frame_out != NULL) {
          sony_fill_frame(rx, frame_out);
        }

        sony_rx_reset(rx);
        return SONY_RX_STATUS_FRAME;
      }

      rx->state = SONY_RX_STATE_BIT_MARK;
      return SONY_RX_STATUS_NONE;
    }

    if (!sony_match_us(duration_us, SONY_BIT_SPACE_US)) {
      sony_rx_reset(rx);
      return SONY_RX_STATUS_ERROR;
    }

    rx->state = SONY_RX_STATE_BIT_MARK;
    return SONY_RX_STATUS_NONE;

  default:
    sony_rx_reset(rx);
    return SONY_RX_STATUS_ERROR;
  }
}

size_t sony_tx_encode(sony_variant_t variant, uint16_t device, uint8_t command,
                      unsigned int repeats, hw_infrared_event_t *events_out,
                      uint32_t *durations_out, size_t capacity) {
  if (events_out == NULL || durations_out == NULL) {
    return 0u;
  }

  const uint8_t bit_count = (uint8_t)variant;
  const size_t frame_events = 2u + ((size_t)bit_count * 2u);
  const size_t required = frame_events * (size_t)(repeats + 1u);
  if (capacity < required) {
    return 0u;
  }

  uint32_t value = (uint32_t)command;
  if (variant == SONY_VARIANT_12) {
    value |= ((uint32_t)device & UINT32_C(0x1F)) << 7;
  } else if (variant == SONY_VARIANT_15) {
    value |= ((uint32_t)device & UINT32_C(0xFF)) << 7;
  } else {
    value |= ((uint32_t)device & UINT32_C(0x1FFF)) << 7;
  }

  size_t out = 0u;
  for (unsigned int r = 0u; r <= repeats; r++) {
    uint32_t frame_total = 0u;

    events_out[out] = HW_INFRARED_EVENT_MARK;
    durations_out[out++] = SONY_HEADER_MARK_US;
    frame_total += SONY_HEADER_MARK_US;

    events_out[out] = HW_INFRARED_EVENT_SPACE;
    durations_out[out++] = SONY_BIT_SPACE_US;
    frame_total += SONY_BIT_SPACE_US;

    for (uint8_t bit = 0u; bit < bit_count; bit++) {
      events_out[out] = HW_INFRARED_EVENT_MARK;
      if (((value >> bit) & UINT32_C(1)) != 0u) {
        durations_out[out++] = SONY_ONE_MARK_US;
        frame_total += SONY_ONE_MARK_US;
      } else {
        durations_out[out++] = SONY_ZERO_MARK_US;
        frame_total += SONY_ZERO_MARK_US;
      }

      events_out[out] = HW_INFRARED_EVENT_SPACE;
      durations_out[out++] = SONY_BIT_SPACE_US;
      frame_total += SONY_BIT_SPACE_US;
    }

    if (r < repeats) {
      events_out[out] = HW_INFRARED_EVENT_SPACE;
      if (SONY_REPEAT_PERIOD_US > frame_total) {
        durations_out[out++] = SONY_REPEAT_PERIOD_US - frame_total;
      } else {
        durations_out[out++] = SONY_BIT_SPACE_US;
      }
    }
  }

  return out;
}

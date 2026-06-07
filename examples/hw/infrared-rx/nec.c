#include "nec.h"

#define NEC_LEADER_MARK_US 9000u
#define NEC_LEADER_SPACE_US 4500u
#define NEC_REPEAT_SPACE_US 2250u
#define NEC_BIT_MARK_US 560u
#define NEC_ZERO_SPACE_US 560u
#define NEC_ONE_SPACE_US 1690u

#define NEC_FRAME_BITS 32u
#define NEC_REPEAT_EVENT_COUNT 3u
#define NEC_FRAME_EVENT_COUNT (2u + (NEC_FRAME_BITS * 2u) + 1u)

enum {
  NEC_RX_STATE_IDLE = 0,
  NEC_RX_STATE_LEADER_SPACE = 1,
  NEC_RX_STATE_REPEAT_MARK = 2,
  NEC_RX_STATE_BIT_MARK = 3,
  NEC_RX_STATE_BIT_SPACE = 4,
};

static inline uint8_t nec_reverse_u8(uint8_t value) {
  value = (uint8_t)(((value & UINT8_C(0xF0)) >> 4) |
                    ((value & UINT8_C(0x0F)) << 4));
  value = (uint8_t)(((value & UINT8_C(0xCC)) >> 2) |
                    ((value & UINT8_C(0x33)) << 2));
  value = (uint8_t)(((value & UINT8_C(0xAA)) >> 1) |
                    ((value & UINT8_C(0x55)) << 1));
  return value;
}

static inline bool nec_match_us(uint32_t measured, uint32_t expected) {
  const uint32_t low = (expected * 70u) / 100u;
  const uint32_t high = (expected * 130u) / 100u;
  return measured >= low && measured <= high;
}

static inline void nec_rx_reset(nec_rx_t *rx) {
  rx->state = NEC_RX_STATE_IDLE;
  rx->bit_index = 0u;
  rx->data = 0u;
}

void nec_rx_init(nec_rx_t *rx) { nec_rx_init_variant(rx, NEC_VARIANT_NEC32); }

void nec_rx_init_variant(nec_rx_t *rx, nec_variant_t variant) {
  if (rx == NULL) {
    return;
  }

  rx->variant = variant;
  nec_rx_reset(rx);
}

nec_rx_status_t nec_rx_decode(nec_rx_t *rx, hw_infrared_event_t event,
                              uint32_t duration_us, nec_frame_t *frame_out) {
  if (rx == NULL) {
    return NEC_RX_STATUS_ERROR;
  }

  if (event == HW_INFRARED_EVENT_TIMEOUT) {
    nec_rx_reset(rx);
    return NEC_RX_STATUS_NONE;
  }

  switch (rx->state) {
  case NEC_RX_STATE_IDLE:
    if (event == HW_INFRARED_EVENT_MARK &&
        nec_match_us(duration_us, NEC_LEADER_MARK_US)) {
      rx->state = NEC_RX_STATE_LEADER_SPACE;
    }
    return NEC_RX_STATUS_NONE;

  case NEC_RX_STATE_LEADER_SPACE:
    if (event != HW_INFRARED_EVENT_SPACE) {
      nec_rx_reset(rx);
      return NEC_RX_STATUS_ERROR;
    }

    if (nec_match_us(duration_us, NEC_LEADER_SPACE_US)) {
      rx->state = NEC_RX_STATE_BIT_MARK;
      rx->bit_index = 0u;
      rx->data = 0u;
      return NEC_RX_STATUS_NONE;
    }

    if (nec_match_us(duration_us, NEC_REPEAT_SPACE_US)) {
      rx->state = NEC_RX_STATE_REPEAT_MARK;
      return NEC_RX_STATUS_NONE;
    }

    nec_rx_reset(rx);
    return NEC_RX_STATUS_ERROR;

  case NEC_RX_STATE_REPEAT_MARK:
    if (event == HW_INFRARED_EVENT_MARK &&
        nec_match_us(duration_us, NEC_BIT_MARK_US)) {
      nec_rx_reset(rx);
      return NEC_RX_STATUS_REPEAT;
    }

    nec_rx_reset(rx);
    return NEC_RX_STATUS_ERROR;

  case NEC_RX_STATE_BIT_MARK:
    if (event != HW_INFRARED_EVENT_MARK ||
        !nec_match_us(duration_us, NEC_BIT_MARK_US)) {
      nec_rx_reset(rx);
      return NEC_RX_STATUS_ERROR;
    }

    rx->state = NEC_RX_STATE_BIT_SPACE;
    return NEC_RX_STATUS_NONE;

  case NEC_RX_STATE_BIT_SPACE:
    if (event != HW_INFRARED_EVENT_SPACE) {
      nec_rx_reset(rx);
      return NEC_RX_STATUS_ERROR;
    }

    if (nec_match_us(duration_us, NEC_ONE_SPACE_US)) {
      rx->data |= (UINT32_C(1) << rx->bit_index);
    } else if (!nec_match_us(duration_us, NEC_ZERO_SPACE_US)) {
      nec_rx_reset(rx);
      return NEC_RX_STATUS_ERROR;
    }

    rx->bit_index++;
    if (rx->bit_index < NEC_FRAME_BITS) {
      rx->state = NEC_RX_STATE_BIT_MARK;
      return NEC_RX_STATUS_NONE;
    }

    if (frame_out != NULL) {
      frame_out->raw = rx->data;
      frame_out->address = (uint8_t)(rx->data & UINT32_C(0xFF));
      frame_out->address_inv = (uint8_t)((rx->data >> 8) & UINT32_C(0xFF));
      frame_out->command = (uint8_t)((rx->data >> 16) & UINT32_C(0xFF));
      frame_out->command_inv = (uint8_t)((rx->data >> 24) & UINT32_C(0xFF));
    }

    nec_rx_reset(rx);
    return NEC_RX_STATUS_FRAME;

  default:
    nec_rx_reset(rx);
    return NEC_RX_STATUS_ERROR;
  }
}

bool nec_frame_extract(const nec_frame_t *frame, nec_variant_t variant,
                       uint16_t *address_out, uint8_t *command_out) {
  if (frame == NULL || address_out == NULL || command_out == NULL) {
    return false;
  }

  const uint8_t command = frame->command;
  const uint8_t command_inv = frame->command_inv;

  if (variant == NEC_VARIANT_NEC32) {
    if ((uint8_t)~command != command_inv) {
      return false;
    }

    const uint8_t address = frame->address;
    const uint8_t address_inv = frame->address_inv;
    if ((uint8_t)~address != address_inv) {
      return false;
    }

    *address_out = (uint16_t)address;
    *command_out = command;
    return true;
  }

  const uint16_t address =
      (uint16_t)frame->address | ((uint16_t)frame->address_inv << 8);
  if (variant == NEC_VARIANT_APPLETV) {
    const uint16_t address_swapped =
        (uint16_t)frame->address_inv | ((uint16_t)frame->address << 8);

    const uint8_t address_lo_rev = nec_reverse_u8(frame->address);
    const uint8_t address_hi_rev = nec_reverse_u8(frame->address_inv);
    const uint16_t address_rev =
        (uint16_t)address_lo_rev | ((uint16_t)address_hi_rev << 8);
    const uint16_t address_rev_swapped =
        (uint16_t)address_hi_rev | ((uint16_t)address_lo_rev << 8);

    if (address != NEC_APPLETV_ADDRESS &&
        address_swapped != NEC_APPLETV_ADDRESS &&
        address_rev != NEC_APPLETV_ADDRESS &&
        address_rev_swapped != NEC_APPLETV_ADDRESS) {
      return false;
    }

    // Apple protocol stores the function in command[7:1] (LSB is auxiliary).
    // It does not always follow strict NEC cmd/inv pairing.
    *address_out = NEC_APPLETV_ADDRESS;
    *command_out = (uint8_t)((command >> 1) & UINT8_C(0x7F));
    return true;
  }

  if ((uint8_t)~command != command_inv) {
    return false;
  }

  *address_out = address;
  *command_out = command;
  return true;
}

size_t nec_tx_encode(nec_variant_t variant, uint16_t address, uint8_t command,
                     bool repeat, hw_infrared_event_t *events_out,
                     uint32_t *durations_out, size_t capacity) {
  const size_t required =
      repeat ? NEC_REPEAT_EVENT_COUNT : NEC_FRAME_EVENT_COUNT;
  if (events_out == NULL || durations_out == NULL || capacity < required) {
    return 0u;
  }

  size_t i = 0u;
  events_out[i] = HW_INFRARED_EVENT_MARK;
  durations_out[i++] = NEC_LEADER_MARK_US;

  if (repeat) {
    events_out[i] = HW_INFRARED_EVENT_SPACE;
    durations_out[i++] = NEC_REPEAT_SPACE_US;
    events_out[i] = HW_INFRARED_EVENT_MARK;
    durations_out[i++] = NEC_BIT_MARK_US;
    return i;
  }

  events_out[i] = HW_INFRARED_EVENT_SPACE;
  durations_out[i++] = NEC_LEADER_SPACE_US;

  uint8_t address_lo = (uint8_t)(address & UINT16_C(0xFF));
  uint8_t address_hi = (uint8_t)((address >> 8) & UINT16_C(0xFF));

  if (variant == NEC_VARIANT_NEC32) {
    address_hi = (uint8_t)~address_lo;
  }

  const uint32_t payload = (uint32_t)address_lo | ((uint32_t)address_hi << 8) |
                           ((uint32_t)command << 16) |
                           ((uint32_t)(uint8_t)~command << 24);

  for (uint8_t bit = 0u; bit < NEC_FRAME_BITS; bit++) {
    events_out[i] = HW_INFRARED_EVENT_MARK;
    durations_out[i++] = NEC_BIT_MARK_US;

    events_out[i] = HW_INFRARED_EVENT_SPACE;
    durations_out[i++] =
        ((payload >> bit) & UINT32_C(1)) ? NEC_ONE_SPACE_US : NEC_ZERO_SPACE_US;
  }

  events_out[i] = HW_INFRARED_EVENT_MARK;
  durations_out[i++] = NEC_BIT_MARK_US;
  return i;
}

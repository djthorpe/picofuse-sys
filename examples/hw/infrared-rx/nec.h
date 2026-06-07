#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <picofuse/hw.h>

typedef struct nec_frame_t {
  uint8_t address;
  uint8_t address_inv;
  uint8_t command;
  uint8_t command_inv;
  uint32_t raw;
} nec_frame_t;

typedef enum nec_rx_status_t {
  NEC_RX_STATUS_NONE = 0,
  NEC_RX_STATUS_FRAME = 1,
  NEC_RX_STATUS_REPEAT = 2,
  NEC_RX_STATUS_ERROR = 3
} nec_rx_status_t;

typedef enum nec_variant_t {
  NEC_VARIANT_APPLETV = 0,
  NEC_VARIANT_NEC32 = 1,
  NEC_VARIANT_NEC16 = 2,
} nec_variant_t;

#define NEC_APPLETV_ADDRESS UINT16_C(0x77E1)

typedef struct nec_rx_t {
  uint8_t state;
  uint8_t bit_index;
  uint32_t data;
  nec_variant_t variant;
} nec_rx_t;

void nec_rx_init(nec_rx_t *rx);
void nec_rx_init_variant(nec_rx_t *rx, nec_variant_t variant);

nec_rx_status_t nec_rx_decode(nec_rx_t *rx, hw_infrared_event_t event,
                              uint32_t duration_us, nec_frame_t *frame_out);

bool nec_frame_extract(const nec_frame_t *frame, nec_variant_t variant,
                       uint16_t *address_out, uint8_t *command_out);

size_t nec_tx_encode(nec_variant_t variant, uint16_t address, uint8_t command,
                     bool repeat, hw_infrared_event_t *events_out,
                     uint32_t *durations_out, size_t capacity);

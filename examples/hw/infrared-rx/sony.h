#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <picofuse/hw.h>

typedef enum sony_variant_t {
  SONY_VARIANT_12 = 12,
  SONY_VARIANT_15 = 15,
  SONY_VARIANT_20 = 20,
} sony_variant_t;

typedef enum sony_rx_status_t {
  SONY_RX_STATUS_NONE = 0,
  SONY_RX_STATUS_FRAME = 1,
  SONY_RX_STATUS_ERROR = 2,
} sony_rx_status_t;

typedef struct sony_frame_t {
  sony_variant_t variant;
  uint32_t raw;
  uint8_t command;
  uint16_t device;
} sony_frame_t;

typedef struct sony_rx_t {
  sony_variant_t variant;
  uint8_t state;
  uint8_t bits;
  uint32_t value;
  uint8_t pending_bit;
} sony_rx_t;

void sony_rx_init(sony_rx_t *rx);
void sony_rx_init_variant(sony_rx_t *rx, sony_variant_t variant);

sony_rx_status_t sony_rx_decode(sony_rx_t *rx, hw_infrared_event_t event,
                                uint32_t duration_us, sony_frame_t *frame_out);

size_t sony_tx_encode(sony_variant_t variant, uint16_t device, uint8_t command,
                      unsigned int repeats, hw_infrared_event_t *events_out,
                      uint32_t *durations_out, size_t capacity);

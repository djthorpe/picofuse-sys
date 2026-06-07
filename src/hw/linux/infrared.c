#include <picofuse/hw.h>

#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_infrared_rx_t {
  char device[64];
  uint8_t bank;
  uint8_t gpio;
  hw_infrared_rx_callback_t callback;
  void *user_data;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_infrared_rx_t _hw_infrared_rx_pool[HW_IR_CAPACITY] = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static hw_infrared_rx_t *_hw_infrared_rx_alloc(void) {
  for (size_t index = 0; index < HW_IR_CAPACITY; index++) {
    if (!_hw_infrared_rx_pool[index].init) {
      return &_hw_infrared_rx_pool[index];
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_infrared_rx_t *hw_infrared_rx_init_device(const char *device,
                                             hw_infrared_rx_callback_t callback,
                                             void *user_data) {
  hw_infrared_rx_t *rx = _hw_infrared_rx_alloc();
  if (rx == NULL) {
    return NULL;
  }

  if (device != NULL) {
    (void)snprintf(rx->device, sizeof(rx->device), "%s", device);
  } else {
    rx->device[0] = '\0';
  }

  rx->bank = 0u;
  rx->gpio = 0u;
  rx->callback = callback;
  rx->user_data = user_data;
  rx->init = true;
  return rx;
}

hw_infrared_rx_t *hw_infrared_rx_init(uint8_t bank, uint8_t gpio,
                                      hw_infrared_rx_callback_t callback,
                                      void *user_data) {
  hw_infrared_rx_t *rx = _hw_infrared_rx_alloc();
  if (rx == NULL) {
    return NULL;
  }

  rx->device[0] = '\0';
  rx->bank = bank;
  rx->gpio = gpio;
  rx->callback = callback;
  rx->user_data = user_data;
  rx->init = true;
  return rx;
}

void hw_infrared_rx_deinit(hw_infrared_rx_t *rx) {
  if (rx == NULL || !rx->init) {
    return;
  }

  rx->device[0] = '\0';
  rx->bank = 0u;
  rx->gpio = 0u;
  rx->callback = NULL;
  rx->user_data = NULL;
  rx->init = false;
}
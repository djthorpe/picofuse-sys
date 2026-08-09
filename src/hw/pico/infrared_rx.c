#include "infrared_rx.pio.h"
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define HW_INFRARED_RX_TIMEOUT_COUNT UINT32_C(0x7FFFFFFF)
#define HW_INFRARED_RX_TIMEOUT_US 50000u

static const float _hw_infrared_rx_clkdiv = 10.0f;

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_infrared_rx_t {
  hw_gpio_t *gpio;
  uint8_t gpio_num;
  void *user_data;
  hw_infrared_rx_callback_t callback;
  PIO pio;
  uint sm;
  uint offset;
  int irq;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_infrared_rx_t _hw_infrared_rx_pool[HW_IR_CAPACITY] = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static inline uint32_t _hw_infrared_rx_to_us(uint32_t initial,
                                             uint32_t remaining) {
  if (remaining > initial) {
    return UINT32_MAX;
  }

  float pio_clk = (float)clock_get_hz(clk_sys) / _hw_infrared_rx_clkdiv;
  return (uint32_t)(((float)(initial - remaining) * 2000000.0f) / pio_clk);
}

static inline hw_infrared_rx_t *_hw_infrared_rx_alloc(void) {
  for (size_t index = 0; index < HW_IR_CAPACITY; index++) {
    if (!_hw_infrared_rx_pool[index].init) {
      return &_hw_infrared_rx_pool[index];
    }
  }

  return NULL;
}

static inline bool
_hw_infrared_irq_has_other_active(int irq, const hw_infrared_rx_t *skip) {
  for (size_t i = 0; i < HW_IR_CAPACITY; i++) {
    const hw_infrared_rx_t *ctx = &_hw_infrared_rx_pool[i];
    if (ctx != skip && ctx->init && ctx->irq == irq && ctx->callback != NULL) {
      return true;
    }
  }

  return false;
}

static inline void _hw_infrared_rx_irq_pio_handler(void) {
  for (size_t i = 0; i < HW_IR_CAPACITY; i++) {
    hw_infrared_rx_t *ctx = &_hw_infrared_rx_pool[i];
    if (!ctx->init || ctx->callback == NULL) {
      continue;
    }

    while (!pio_sm_is_rx_fifo_empty(ctx->pio, ctx->sm)) {
      uint32_t value = pio_sm_get(ctx->pio, ctx->sm);
      uint32_t count = value & HW_INFRARED_RX_TIMEOUT_COUNT;
      if (count == HW_INFRARED_RX_TIMEOUT_COUNT) {
        ctx->callback(HW_INFRARED_EVENT_TIMEOUT, 0u, ctx->user_data);
        continue;
      }

      bool is_space = (value & UINT32_C(0x80000000)) != 0u;
      uint32_t duration_us =
          _hw_infrared_rx_to_us(HW_INFRARED_RX_TIMEOUT_COUNT, count);

      if (is_space && duration_us > HW_INFRARED_RX_TIMEOUT_US) {
        ctx->callback(HW_INFRARED_EVENT_TIMEOUT, 0u, ctx->user_data);
      } else if (is_space) {
        ctx->callback(HW_INFRARED_EVENT_SPACE, duration_us, ctx->user_data);
      } else {
        ctx->callback(HW_INFRARED_EVENT_MARK, duration_us, ctx->user_data);
      }
    }
  }
}

static inline bool _hw_infrared_rx_pio_init(hw_infrared_rx_t *rx,
                                            hw_infrared_rx_callback_t callback,
                                            void *user_data) {
  if (rx == NULL || rx->gpio == NULL) {
    return false;
  }

  const uint8_t gpio = hw_gpio_get_pin_num(rx->gpio);
  PIO pio = NULL;
  uint sm = 0u;
  uint offset = 0u;

  if (!pio_claim_free_sm_and_add_program_for_gpio_range(
          &infrared_rx_program, &pio, &sm, &offset, gpio, 1u, true)) {
    return false;
  }

  pio_sm_config c = infrared_rx_program_get_default_config(offset);
  sm_config_set_clkdiv(&c, _hw_infrared_rx_clkdiv);
  sm_config_set_in_pins(&c, gpio);
  sm_config_set_jmp_pin(&c, gpio);
  sm_config_set_in_shift(&c, false, false, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  int irq = pio_get_irq_num(pio, 0);

  pio_gpio_init(pio, gpio);
  gpio_set_dir(gpio, GPIO_IN);
  gpio_pull_up(gpio);

  pio_sm_init(pio, sm, offset, &c);

  if (!_hw_infrared_irq_has_other_active(irq, NULL)) {
    irq_add_shared_handler(irq, _hw_infrared_rx_irq_pio_handler,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(irq, true);
  }

  const uint irq_index = (uint)(irq - pio_get_irq_num(pio, 0));
  pio_set_irqn_source_enabled(
      pio, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(sm), true);

  pio_sm_set_enabled(pio, sm, true);

  rx->gpio_num = gpio;
  rx->user_data = user_data;
  rx->callback = callback;
  rx->pio = pio;
  rx->sm = sm;
  rx->offset = offset;
  rx->irq = irq;
  return true;
}

static void _hw_infrared_rx_pio_finalize(hw_infrared_rx_t *ctx) {
  if (ctx == NULL || !ctx->init) {
    return;
  }

  const uint irq_index = (uint)(ctx->irq - pio_get_irq_num(ctx->pio, 0));
  pio_set_irqn_source_enabled(
      ctx->pio, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(ctx->sm),
      false);

  pio_sm_set_enabled(ctx->pio, ctx->sm, false);

  pio_remove_program_and_unclaim_sm(&infrared_rx_program, ctx->pio, ctx->sm,
                                    ctx->offset);

  if (!_hw_infrared_irq_has_other_active(ctx->irq, ctx)) {
    irq_set_enabled(ctx->irq, false);
    irq_remove_handler(ctx->irq, _hw_infrared_rx_irq_pio_handler);
  }

  ctx->gpio = 0u;
  ctx->gpio_num = 0u;
  ctx->user_data = NULL;
  ctx->callback = NULL;
  ctx->pio = NULL;
  ctx->sm = 0u;
  ctx->offset = 0u;
  ctx->irq = -1;
  ctx->init = false;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_infrared_rx_t *hw_infrared_rx_init_device(const char *device,
                                             hw_infrared_rx_callback_t callback,
                                             void *user_data) {
  (void)device;
  (void)callback;
  (void)user_data;
  return NULL;
}

hw_infrared_rx_t *hw_infrared_rx_init(uint8_t bank, uint8_t gpio,
                                      hw_infrared_rx_callback_t callback,
                                      void *user_data) {
  if (callback == NULL || bank != 0u || gpio >= hw_gpio_count(bank)) {
    return NULL;
  }

  hw_infrared_rx_t *rx = _hw_infrared_rx_alloc();
  if (rx == NULL) {
    return NULL;
  }

  rx->gpio = hw_gpio_init(bank, gpio, HW_GPIO_INPUT);
  if (rx->gpio == NULL) {
    return NULL;
  }

  if (!_hw_infrared_rx_pio_init(rx, callback, user_data)) {
    hw_gpio_deinit(rx->gpio);
    rx->gpio = NULL;
    return NULL;
  }

  rx->init = true;
  return rx;
}

void hw_infrared_rx_deinit(hw_infrared_rx_t *rx) {
  if (rx == NULL || !rx->init) {
    return;
  }

  _hw_infrared_rx_pio_finalize(rx);

  if (rx->gpio != NULL) {
    hw_gpio_deinit(rx->gpio);
  }

  rx->gpio = NULL;
  rx->init = false;
}

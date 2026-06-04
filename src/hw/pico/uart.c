#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/uart.h>
#include <hardware/uart.h>
#include <pico/time.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_uart_t {
  uart_inst_t *instance;
  const hw_gpio_t *rx_pin;
  const hw_gpio_t *tx_pin;
  uint32_t baud_rate;
  uint32_t events;
  hw_uart_callback_t callback;
  irq_handler_t irq_handler;
  void *userdata;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_uart_t uarts[NUM_UARTS] = {0};

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hw_uart_instance_for_pins(const hw_gpio_t *rx_pin,
                                       const hw_gpio_t *tx_pin,
                                       uart_inst_t **instance);
static bool _hw_uart_signal_pin_matches_instance(const hw_gpio_t *pin,
                                                 uart_inst_t *instance,
                                                 uint8_t signal_offset);
static void _hw_uart_configure_pin(const hw_gpio_t *pin);
static void _hw_uart_set_callback(hw_uart_t *uart, hw_uart_callback_t callback,
                                  void *userdata);
static void _hw_uart0_irq_handler(void);
#if NUM_UARTS > 1
static void _hw_uart1_irq_handler(void);
#endif
static uint8_t _hw_uart_num_for_group_base(uint8_t group_base);
static hw_uart_config_t _hw_uart_default_config(void);
static uart_parity_t _hw_uart_sdk_parity(hw_uart_parity_t parity);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a UART device.
 */
hw_uart_t *hw_uart_init(const hw_gpio_t *rx_pin, const hw_gpio_t *tx_pin,
                        uint32_t baud_rate, hw_uart_callback_t callback,
                        void *userdata, const hw_uart_config_t *config) {
  sys_debugf("uart_init: rx=%p tx=%p baud=%u callback=%p userdata=%p config=%p",
             rx_pin, tx_pin, baud_rate, (void *)callback, userdata, config);
  hw_uart_config_t settings =
      config != NULL ? *config : _hw_uart_default_config();

  sys_assert(hw_gpio_valid(rx_pin));
  sys_assert(hw_gpio_valid(tx_pin));
  sys_assert(baud_rate > 0);

  // Get the Pico instance
  uart_inst_t *instance = NULL;
  if (_hw_uart_instance_for_pins(rx_pin, tx_pin, &instance) == false) {
    return NULL;
  }

  // If the configuration is provided, validate the CTS and RTS pins.
  if (settings.flow_control == HW_UART_FLOW_CONTROL_RTS ||
      settings.flow_control == HW_UART_FLOW_CONTROL_CTS_RTS) {
    sys_assert(
        _hw_uart_signal_pin_matches_instance(settings.rts_pin, instance, 3));
  }
  if (settings.flow_control == HW_UART_FLOW_CONTROL_CTS ||
      settings.flow_control == HW_UART_FLOW_CONTROL_CTS_RTS) {
    sys_assert(
        _hw_uart_signal_pin_matches_instance(settings.cts_pin, instance, 2));
  }

  // Configure the selected UART signal pins with the platform-specific mux.
  _hw_uart_configure_pin(rx_pin);
  _hw_uart_configure_pin(tx_pin);
  if (settings.flow_control == HW_UART_FLOW_CONTROL_RTS ||
      settings.flow_control == HW_UART_FLOW_CONTROL_CTS_RTS) {
    _hw_uart_configure_pin(settings.rts_pin);
  }
  if (settings.flow_control == HW_UART_FLOW_CONTROL_CTS ||
      settings.flow_control == HW_UART_FLOW_CONTROL_CTS_RTS) {
    _hw_uart_configure_pin(settings.cts_pin);
  }

  // Get the internal UART structure for the selected instance and initialize it
  hw_uart_t *uart = &uarts[uart_get_index(instance)];
  if (uart->instance != NULL) {
    hw_uart_deinit(uart);
  }

  // Initialize the UART structure
  uart->instance = instance;
  uart->rx_pin = rx_pin;
  uart->tx_pin = tx_pin;
  uart->baud_rate = baud_rate;
  uart->events = settings.events;

  // Set the IRQ handler based on the selected instance
  switch (uart_get_index(instance)) {
  case 0:
    uart->irq_handler = _hw_uart0_irq_handler;
    break;
#if NUM_UARTS > 1
  case 1:
    uart->irq_handler = _hw_uart1_irq_handler;
    break;
#endif
  default:
    sys_panicf("Invalid UART instance");
    break;
  }

  // Set baud rate, line format, and flow control settings.
  uart_init(instance, baud_rate);
  uart_set_format(instance, settings.data_bits, settings.stop_bits,
                  _hw_uart_sdk_parity(settings.parity));
  uart_set_hw_flow(instance,
                   settings.flow_control == HW_UART_FLOW_CONTROL_CTS ||
                       settings.flow_control == HW_UART_FLOW_CONTROL_CTS_RTS,
                   settings.flow_control == HW_UART_FLOW_CONTROL_RTS ||
                       settings.flow_control == HW_UART_FLOW_CONTROL_CTS_RTS);

  // Set the callback and enable the corresponding UART interrupts
  if (uart->irq_handler != NULL) {
    _hw_uart_set_callback(uart, callback, userdata);
  }

  // Return the initialized UART handle
  return uart;
}

/**
 * @brief Deinitialize a UART device.
 */
void hw_uart_deinit(hw_uart_t *uart) {
  sys_debugf("uart_deinit: uart=%p", uart);
  if (!hw_uart_valid(uart)) {
    return;
  }

  // Deinitialize the Pico UART instance and clear the UART structure
  _hw_uart_set_callback(uart, NULL, NULL);
  uart_deinit(uart->instance);
  sys_memset(uart, 0, sizeof(hw_uart_t));
}

/**
 * @brief Check whether a UART handle is valid.
 */
bool hw_uart_valid(const hw_uart_t *uart) {
  return uart && uart->instance != NULL;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Transmit bytes over a UART interface.
 */
size_t hw_uart_write(hw_uart_t *uart, const void *data, size_t size,
                     uint32_t timeout_ms) {
  if (!hw_uart_valid(uart) || data == NULL || size == 0) {
    return 0;
  }

  const uint8_t *bytes = data;
  size_t written = 0;

  if (timeout_ms == 0) {
    while (written < size && uart_is_writable(uart->instance)) {
      uart_get_hw(uart->instance)->dr = bytes[written++];
    }
    return written;
  }

  absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  while (written < size) {
    while (!uart_is_writable(uart->instance)) {
      if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
        return written;
      }
    }
    uart_get_hw(uart->instance)->dr = bytes[written++];
  }

  return written;
}

/**
 * @brief Receive bytes from a UART interface.
 */
size_t hw_uart_read(hw_uart_t *uart, void *data, size_t size,
                    uint32_t timeout_ms) {
  if (!hw_uart_valid(uart) || data == NULL || size == 0) {
    return 0;
  }

  uint8_t *bytes = data;
  size_t read = 0;

  if (timeout_ms == 0) {
    while (read < size && uart_is_readable(uart->instance)) {
      bytes[read++] = (uint8_t)uart_get_hw(uart->instance)->dr;
    }
    return read;
  }

  absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  while (read < size) {
    while (!uart_is_readable(uart->instance)) {
      if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
        return read;
      }
    }
    bytes[read++] = (uint8_t)uart_get_hw(uart->instance)->dr;
  }

  return read;
}

/**
 * @brief Wait for UART transmission to complete.
 */
bool hw_uart_flush(hw_uart_t *uart, uint32_t timeout_ms) {
  if (!hw_uart_valid(uart)) {
    return false;
  }

  if ((uart_get_hw(uart->instance)->fr & UART_UARTFR_BUSY_BITS) == 0) {
    return true;
  }

  if (timeout_ms == 0) {
    return false;
  }

  absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  while (uart_get_hw(uart->instance)->fr & UART_UARTFR_BUSY_BITS) {
    if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
      return false;
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/**
 * @brief Determine uart instance index based on RX and TX pins.
 */
static bool _hw_uart_instance_for_pins(const hw_gpio_t *rx_pin,
                                       const hw_gpio_t *tx_pin,
                                       uart_inst_t **instance) {
  if (!hw_gpio_valid(rx_pin) || !hw_gpio_valid(tx_pin)) {
    *instance = NULL;
    return false;
  }

#if PICO_RP2040 || PICO_RP2350
  uint8_t rx_num = hw_gpio_get_pin_num(rx_pin);
  uint8_t tx_num = hw_gpio_get_pin_num(tx_pin);

  if ((tx_num & 0x3u) == 0u && rx_num == (uint8_t)(tx_num + 1u)) {
    *instance = UART_INSTANCE(_hw_uart_num_for_group_base(tx_num));
    return true;
  }
#endif

  // Sad path
  *instance = NULL;
  return false;
}

/**
 * @brief Check whether a UART signal pin matches an already selected instance.
 */
static bool _hw_uart_signal_pin_matches_instance(const hw_gpio_t *pin,
                                                 uart_inst_t *instance,
                                                 uint8_t signal_offset) {
  if (!hw_gpio_valid(pin) || instance == NULL) {
    return false;
  }

#if PICO_RP2040 || PICO_RP2350
  uint8_t pin_num = hw_gpio_get_pin_num(pin);
  if ((pin_num & 0x3u) != signal_offset) {
    return false;
  }

  return uart_get_index(instance) ==
         _hw_uart_num_for_group_base((uint8_t)(pin_num & ~0x3u));
#else
  (void)pin;
  (void)instance;
  (void)signal_offset;
  return false;
#endif
}

/**
 * @brief Configure a GPIO pin for its UART function.
 * @details On this Pico backend the GPIO number determines the UART function
 * selection, so no UART instance parameter is required here.
 */
static void _hw_uart_configure_pin(const hw_gpio_t *pin) {
  uint8_t pin_num = hw_gpio_get_pin_num(pin);
  gpio_set_function(pin_num, UART_FUNCSEL_NUM(uart0, pin_num));
}

/**
 * @brief Configure or remove the callback and its backing UART interrupt.
 */
static void _hw_uart_set_callback(hw_uart_t *uart, hw_uart_callback_t callback,
                                  void *userdata) {
  if (!hw_uart_valid(uart)) {
    return;
  }

  // Remove an existing callback and disable the corresponding UART interrupts
  // if a callback is already set
  uint32_t irq_num = UART_IRQ_NUM(uart->instance);
  if (uart->callback != NULL) {
    uart_set_irqs_enabled(uart->instance, false, false);
    irq_set_enabled(irq_num, false);
    irq_remove_handler(irq_num, uart->irq_handler);
    uart_get_hw(uart->instance)->icr = UART_UARTICR_RXIC_BITS |
                                       UART_UARTICR_RTIC_BITS |
                                       UART_UARTICR_TXIC_BITS;
  }

  // If no events are enabled, skip configuring the interrupt handler
  bool rx_has_data =
      callback != NULL && (uart->events & HW_UART_EVENT_RX_HAS_DATA) != 0;
  bool tx_empty =
      callback != NULL && (uart->events & HW_UART_EVENT_TX_EMPTY) != 0;

  uart->callback = callback;
  uart->userdata = userdata;

  if (!rx_has_data && !tx_empty) {
    return;
  }

  // Enable the corresponding UART interrupts and set the global IRQ handler for
  // the selected instance
  uart_get_hw(uart->instance)->icr =
      UART_UARTICR_RXIC_BITS | UART_UARTICR_RTIC_BITS | UART_UARTICR_TXIC_BITS;
  irq_set_exclusive_handler(irq_num, uart->irq_handler);
  uart_set_irqs_enabled(uart->instance, rx_has_data, tx_empty);
  irq_set_enabled(irq_num, true);
}

/**
 * @brief Dispatch pending UART IRQ events to the registered callback.
 */
static void _hw_uart_irq_handler(hw_uart_t *uart) {
  if (!hw_uart_valid(uart) || uart->callback == NULL) {
    return;
  }

  uint32_t status = uart_get_hw(uart->instance)->mis;
  uint32_t events = 0;
  uint32_t clear_mask = 0;

  if ((status & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS)) != 0) {
    events |= HW_UART_EVENT_RX_HAS_DATA;
    clear_mask |= UART_UARTICR_RXIC_BITS | UART_UARTICR_RTIC_BITS;
  }
  if ((status & UART_UARTMIS_TXMIS_BITS) != 0) {
    events |= HW_UART_EVENT_TX_EMPTY;
    clear_mask |= UART_UARTICR_TXIC_BITS;
  }

  if (clear_mask != 0) {
    uart_get_hw(uart->instance)->icr = clear_mask;
  }
  if (events != 0) {
    uart->callback(uart, events, uart->userdata);
  }
}

/**
 * @brief IRQ wrapper for UART0.
 */
static void _hw_uart0_irq_handler(void) { _hw_uart_irq_handler(&uarts[0]); }

#if NUM_UARTS > 1
/**
 * @brief IRQ wrapper for UART1.
 */
static void _hw_uart1_irq_handler(void) { _hw_uart_irq_handler(&uarts[1]); }
#endif

/**
 * @brief Map a 4-pin UART group base to a UART instance number.
 */
static uint8_t _hw_uart_num_for_group_base(uint8_t group_base) {
  return (uint8_t)((((group_base >> 2) & 1u) ^ ((group_base >> 3) & 1u)) & 1u);
}

/**
 * @brief Get the default UART configuration.
 */
static hw_uart_config_t _hw_uart_default_config(void) {
  return (hw_uart_config_t){
      .cts_pin = NULL,
      .rts_pin = NULL,
      .data_bits = HW_UART_DATA_BITS_8,
      .stop_bits = HW_UART_STOP_BITS_1,
      .parity = HW_UART_PARITY_NONE,
      .flow_control = HW_UART_FLOW_CONTROL_NONE,
      .events = 0,
  };
}

/**
 * @brief Convert the public parity enum to the Pico SDK parity enum.
 */
static uart_parity_t _hw_uart_sdk_parity(hw_uart_parity_t parity) {
  switch (parity) {
  case HW_UART_PARITY_EVEN:
    return UART_PARITY_EVEN;
  case HW_UART_PARITY_ODD:
    return UART_PARITY_ODD;
  case HW_UART_PARITY_NONE:
  default:
    return UART_PARITY_NONE;
  }
}

#include <hardware/uart.h>
#include <picofuse/hw.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_uart_t {
  bool initialized;
  const hw_gpio_t *rx_pin;
  const hw_gpio_t *tx_pin;
  uint32_t baud_rate;
  hw_uart_callback_t callback;
  void *userdata;
  hw_uart_config_t config;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_uart_t uarts[NUM_UARTS];

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a UART device.
 */
hw_uart_t *hw_uart_init(const hw_gpio_t *rx_pin, const hw_gpio_t *tx_pin,
                        uint32_t baud_rate, hw_uart_callback_t callback,
                        void *userdata, const hw_uart_config_t *config) {
  size_t index;

  if (!hw_gpio_valid(rx_pin) || !hw_gpio_valid(tx_pin)) {
    return NULL;
  }

  for (index = 0; index < NUM_UARTS; ++index) {
    if (!uarts[index].initialized) {
      uarts[index].initialized = true;
      uarts[index].rx_pin = rx_pin;
      uarts[index].tx_pin = tx_pin;
      uarts[index].baud_rate = baud_rate;
      uarts[index].callback = callback;
      uarts[index].userdata = userdata;
      uarts[index].config.cts_pin = NULL;
      uarts[index].config.rts_pin = NULL;
      uarts[index].config.data_bits = HW_UART_DATA_BITS_8;
      uarts[index].config.stop_bits = HW_UART_STOP_BITS_1;
      uarts[index].config.parity = HW_UART_PARITY_NONE;
      uarts[index].config.flow_control = HW_UART_FLOW_CONTROL_NONE;
      uarts[index].config.events = 0;

      if (config) {
        uarts[index].config = *config;
      }

      return &uarts[index];
    }
  }

  return NULL;
}

/**
 * @brief Deinitialize a UART device.
 */
void hw_uart_deinit(hw_uart_t *uart) {
  if (!hw_uart_valid(uart)) {
    return;
  }

  uart->initialized = false;
  uart->rx_pin = NULL;
  uart->tx_pin = NULL;
  uart->baud_rate = 0;
  uart->callback = NULL;
  uart->userdata = NULL;
  uart->config.cts_pin = NULL;
  uart->config.rts_pin = NULL;
  uart->config.data_bits = HW_UART_DATA_BITS_8;
  uart->config.stop_bits = HW_UART_STOP_BITS_1;
  uart->config.parity = HW_UART_PARITY_NONE;
  uart->config.flow_control = HW_UART_FLOW_CONTROL_NONE;
  uart->config.events = 0;
}

/**
 * @brief Check whether a UART handle is valid.
 */
bool hw_uart_valid(const hw_uart_t *uart) { return uart && uart->initialized; }

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Transmit bytes over a UART interface.
 */
size_t hw_uart_write(hw_uart_t *uart, const void *data, size_t size,
                     uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_uart_valid(uart) || !data || size == 0) {
    return 0;
  }

  return 0;
}

/**
 * @brief Receive bytes from a UART interface.
 */
size_t hw_uart_read(hw_uart_t *uart, void *data, size_t size,
                    uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_uart_valid(uart) || !data || size == 0) {
    return 0;
  }

  return 0;
}

/**
 * @brief Wait for UART transmission to complete.
 */
bool hw_uart_flush(hw_uart_t *uart, uint32_t timeout_ms) {
  (void)timeout_ms;

  return hw_uart_valid(uart);
}

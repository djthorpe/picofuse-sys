#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_uart_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a UART device.
 */
hw_uart_t *hw_uart_init(const hw_gpio_t *rx_pin, const hw_gpio_t *tx_pin,
                        uint32_t baud_rate, hw_uart_callback_t callback,
                        void *userdata, const hw_uart_config_t *config) {
  (void)rx_pin;
  (void)tx_pin;
  (void)baud_rate;
  (void)callback;
  (void)userdata;
  (void)config;
  return NULL;
}

/**
 * @brief Deinitialize a UART device.
 */
void hw_uart_deinit(hw_uart_t *uart) { (void)uart; }

/**
 * @brief Check whether a UART handle is valid.
 */
bool hw_uart_valid(const hw_uart_t *uart) {
  (void)uart;
  return false; // No-op stub implementation for unsupported platforms.
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Transmit bytes over a UART interface.
 */
size_t hw_uart_write(hw_uart_t *uart, const void *data, size_t size,
                     uint32_t timeout_ms) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout_ms;
  return 0;
}

/**
 * @brief Receive bytes from a UART interface.
 */
size_t hw_uart_read(hw_uart_t *uart, void *data, size_t size,
                    uint32_t timeout_ms) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout_ms;
  return 0;
}

/**
 * @brief Wait for UART transmission to complete.
 */
bool hw_uart_flush(hw_uart_t *uart, uint32_t timeout_ms) {
  (void)uart;
  (void)timeout_ms;
  return false;
}

/**
 * @file
 * @brief UART example.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define TX_PIN 0
#define RX_PIN 1
#define BAUD_RATE 9600u
#define TIMEOUT_MS 1000u
#define LINE_SIZE 64u

static volatile size_t rx_length = 0;
static volatile size_t echo_length = 0;
static volatile bool line_ready = false;
static uint8_t rx_buffer[LINE_SIZE] = {0};
static uint8_t echo_buffer[LINE_SIZE] = {0};

/**
 * @brief Write as much as possible to UART without blocking forever.
 *
 * The function keeps calling @c hw_uart_write with a zero timeout so partial
 * progress is accepted and the example can keep servicing incoming data.
 */
static void write_best_effort(hw_uart_t *uart, const void *data, size_t size) {
  const uint8_t *bytes = data;

  while (size > 0) {
    size_t written = hw_uart_write(uart, bytes, size, 0);
    if (written == 0) {
      return;
    }

    bytes += written;
    size -= written;
  }
}

/**
 * @brief Write the full payload to UART or abort if the port stops accepting
 * data.
 *
 * This helper loops until the whole buffer is accepted, using a timeout on
 * each write so the example can report a stuck UART instead of silently
 * dropping bytes.
 */
static void write_all(hw_uart_t *uart, const void *data, size_t size) {
  const uint8_t *bytes = data;
  size_t written = 0;
  while (written < size) {
    size_t count =
        hw_uart_write(uart, bytes + written, size - written, TIMEOUT_MS);
    if (count == 0) {
      sys_panicf("UART write timed out");
    }
    written += count;
  }
}

/**
 * @brief Handle UART receive events, implement line editing, and echo lines.
 *
 * The callback drains received bytes, handles backspace and carriage return,
 * copies the completed line into a separate echo buffer, and writes immediate
 * feedback back to the terminal.
 */
static void callback(hw_uart_t *uart, uint32_t events, void *userdata) {
  (void)userdata;

  static const uint8_t newline[] = "\r\n";
  static const uint8_t backspace[] = "\b \b";

  if ((events & HW_UART_EVENT_RX_HAS_DATA) == 0) {
    return;
  }

  for (;;) {
    uint8_t byte = 0;
    if (hw_uart_read(uart, &byte, 1, 0) == 0) {
      return;
    }

    if (byte == '\n') {
      continue;
    }

    if (byte == '\b' || byte == 0x7fu) {
      if (rx_length > 0) {
        --rx_length;
        write_best_effort(uart, backspace, sizeof(backspace) - 1u);
      }
      continue;
    }

    if (byte == '\r') {
      if (!line_ready) {
        size_t length = rx_length;
        if (length >= LINE_SIZE) {
          length = LINE_SIZE - 1u;
        }

        for (size_t i = 0; i < length; ++i) {
          echo_buffer[i] = rx_buffer[i];
        }
        echo_length = length;
        line_ready = true;
      }

      rx_length = 0;
      write_best_effort(uart, newline, sizeof(newline) - 1u);
      continue;
    }

    if (rx_length < (LINE_SIZE - 1u)) {
      rx_buffer[rx_length++] = byte;
      write_best_effort(uart, &byte, 1u);
    }
  }
}

/**
 * @brief Set up a UART echo console on GP0/GP1.
 *
 * This example shows how to configure GPIO pins for UART use, initialize the
 * UART with an event callback, transmit a banner, and echo each completed line
 * back to the terminal.
 */
int main(void) {
  hw_uart_config_t config = {
      .events = HW_UART_EVENT_RX_HAS_DATA,
  };
  hw_gpio_t *tx_pin = hw_gpio_init(0, TX_PIN, HW_GPIO_UART);
  hw_gpio_t *rx_pin = hw_gpio_init(0, RX_PIN, HW_GPIO_UART);

  if (!hw_gpio_valid(tx_pin) || !hw_gpio_valid(rx_pin)) {
    sys_panicf("Failed to initialize UART GPIO pins");
  }

  hw_uart_t *uart =
      hw_uart_init(rx_pin, tx_pin, BAUD_RATE, callback, NULL, &config);
  if (!hw_uart_valid(uart)) {
    sys_panicf("Failed to initialize UART");
  }

  static const uint8_t banner[] = "UART echo ready on GP0/GP1 at 9600 baud\r\n"
                                  "Type a line and press carriage return.\r\n";
  static const uint8_t prefix[] = "ECHO: ";
  static const uint8_t newline[] = "\r\n";

  write_all(uart, banner, sizeof(banner) - 1u);
  if (!hw_uart_flush(uart, TIMEOUT_MS)) {
    sys_panicf("UART flush timed out");
  }

  for (;;) {
    if (!line_ready) {
      continue;
    }

    size_t length = echo_length;
    line_ready = false;

    write_all(uart, prefix, sizeof(prefix) - 1u);
    write_all(uart, echo_buffer, length);
    write_all(uart, newline, sizeof(newline) - 1u);
    if (!hw_uart_flush(uart, TIMEOUT_MS)) {
      sys_panicf("UART flush timed out");
    }
  }
}
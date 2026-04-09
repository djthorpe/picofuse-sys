/**
 * @file uart.h
 * @brief UART (Universal Asynchronous Receiver/Transmitter) interface
 * @defgroup UART UART
 * @ingroup Hardware
 *
 * Universal Asynchronous Receiver/Transmitter (UART) interface for hardware
 * platforms. This module provides functions to initialize UART peripherals,
 * configure baud rates, and handle interrupts.
 */
#pragma once
#include "gpio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief UART device structure.
 * @ingroup UART
 * @headerfile uart.h hw/hw.h
 */
typedef struct hw_uart_t hw_uart_t;

/**
 * @brief UART data bit configuration.
 * @ingroup UART
 */
typedef enum {
  HW_UART_DATA_BITS_5 = 5,
  HW_UART_DATA_BITS_6 = 6,
  HW_UART_DATA_BITS_7 = 7,
  HW_UART_DATA_BITS_8 = 8,
} hw_uart_data_bits_t;

/**
 * @brief UART stop bit configuration.
 * @ingroup UART
 */
typedef enum {
  HW_UART_STOP_BITS_1 = 1,
  HW_UART_STOP_BITS_2 = 2,
} hw_uart_stop_bits_t;

/**
 * @brief UART parity configuration.
 * @ingroup UART
 */
typedef enum {
  HW_UART_PARITY_NONE,
  HW_UART_PARITY_EVEN,
  HW_UART_PARITY_ODD,
} hw_uart_parity_t;

/**
 * @brief UART hardware flow control mode.
 * @ingroup UART
 */
typedef enum {
  HW_UART_FLOW_CONTROL_NONE,
  HW_UART_FLOW_CONTROL_CTS,
  HW_UART_FLOW_CONTROL_RTS,
  HW_UART_FLOW_CONTROL_CTS_RTS,
} hw_uart_flow_control_t;

/**
 * @brief UART callback event mask.
 * @ingroup UART
 */
typedef enum {
  HW_UART_EVENT_RX_HAS_DATA = 1u << 0,
  HW_UART_EVENT_TX_EMPTY = 1u << 1,
} hw_uart_event_t;

/**
 * @brief UART event callback.
 * @ingroup UART
 *
 * Callback invoked when one or more enabled UART events occur.
 *
 * @param uart The UART device that triggered the callback.
 * @param events Bitmask of UART events that caused the callback to fire.
 * @param userdata User-defined context pointer provided during initialization.
 */
typedef void (*hw_uart_callback_t)(hw_uart_t *uart, uint32_t events,
                                   void *userdata);

/**
 * @brief UART initialization configuration.
 * @ingroup UART
 *
 * Describes optional UART settings beyond the required RX pin, TX pin,
 * baud rate, callback, and user data passed directly to `hw_uart_init()`.
 *
 * When `NULL` is passed to `hw_uart_init()`, implementation defaults are used
 * for all fields in this structure.
 */
typedef struct {
  const hw_gpio_t *cts_pin;
  const hw_gpio_t *rts_pin;
  hw_uart_data_bits_t data_bits;
  hw_uart_stop_bits_t stop_bits;
  hw_uart_parity_t parity;
  hw_uart_flow_control_t flow_control;
  uint32_t events;
} hw_uart_config_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a UART device.
 * @ingroup UART
 *
 * Creates or acquires a UART instance using the required RX pin, TX pin,
 * baud rate, callback, and user data, with optional extended configuration.
 *
 * @param rx_pin The GPIO pin to use for UART receive.
 * @param tx_pin The GPIO pin to use for UART transmit.
 * @param baud_rate The UART baud rate in bits per second.
 * @param callback Optional UART event callback. Pass `NULL` to disable
 * callback-based interrupt notification.
 * @param userdata User-defined context pointer passed to `callback` when
 * invoked.
 * @param config Optional pointer to extended UART configuration. Pass `NULL`
 * to use default line format, flow control, and event settings.
 * @return A pointer to an initialized UART device, or `NULL` if
 * initialization fails.
 */
hw_uart_t *hw_uart_init(const hw_gpio_t *rx_pin, const hw_gpio_t *tx_pin,
                        uint32_t baud_rate, hw_uart_callback_t callback,
                        void *userdata, const hw_uart_config_t *config);

/**
 * @brief Deinitialize a UART device.
 * @ingroup UART
 *
 * Shuts down a UART instance and releases any resources associated with the
 * handle.
 *
 * @param uart The UART device to deinitialize.
 */
void hw_uart_deinit(hw_uart_t *uart);

/**
 * @brief Check whether a UART handle is valid.
 * @ingroup UART
 *
 * Verifies that a UART device pointer refers to a valid runtime-managed UART
 * instance.
 *
 * @param uart The UART device to validate.
 * @retval true The UART handle is valid.
 * @retval false The UART handle is invalid.
 */
bool hw_uart_valid(const hw_uart_t *uart);

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Transmit bytes over a UART interface.
 * @ingroup UART
 *
 * Writes up to `size` bytes from the provided buffer to the UART transmitter.
 * The function waits until space is available in the UART transmit path or the
 * timeout expires.
 *
 * @param uart Pointer to the UART structure representing the interface.
 * @param data Pointer to the transmit buffer.
 * @param size Number of bytes to transmit.
 * @param timeout_ms Timeout for the entire operation in milliseconds. Set to
 * `0` for a non-blocking attempt.
 * @return Number of bytes accepted for transmission. Returns zero if no bytes
 * could be written before the timeout expired.
 */
size_t hw_uart_write(hw_uart_t *uart, const void *data, size_t size,
                     uint32_t timeout_ms);

/**
 * @brief Receive bytes from a UART interface.
 * @ingroup UART
 *
 * Reads up to `size` bytes from the UART receiver into the provided buffer.
 * The function waits until data is available or the timeout expires.
 *
 * @param uart Pointer to the UART structure representing the interface.
 * @param data Pointer to the receive buffer.
 * @param size Maximum number of bytes to receive.
 * @param timeout_ms Timeout for the entire operation in milliseconds. Set to
 * `0` for a non-blocking attempt.
 * @return Number of bytes received. Returns zero if no bytes were available
 * before the timeout expired.
 */
size_t hw_uart_read(hw_uart_t *uart, void *data, size_t size,
                    uint32_t timeout_ms);

/**
 * @brief Wait for UART transmission to complete.
 * @ingroup UART
 *
 * Waits until all queued transmit data has left the UART transmit path or the
 * timeout expires.
 *
 * @param uart Pointer to the UART structure representing the interface.
 * @param timeout_ms Timeout for the operation in milliseconds. Set to `0` for
 * a non-blocking status check.
 * @retval true All pending transmit data was sent before the timeout expired.
 * @retval false Transmission was still in progress when the timeout expired.
 */
bool hw_uart_flush(hw_uart_t *uart, uint32_t timeout_ms);
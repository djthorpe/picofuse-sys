/**
 * @file mqtt.h
 * @brief MQTT (Message Queuing Telemetry Transport) interface
 * @defgroup MQTT MQTT
 * @ingroup Network
 *
 * MQTT (Message Queuing Telemetry Transport) interface for network
 * communication.
 *
 * This module provides functions to connect to an MQTT broker, publish and
 * subscribe to topics, and handle incoming messages.
 */
#pragma once
#include <picofuse/sys/mem.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief MQTT client handle.
 * @ingroup MQTT
 * @headerfile net/mqtt.h picofuse/net.h
 */
typedef struct net_mqtt_t net_mqtt_t;

/**
 * @brief Connection/event status flags for MQTT operations.
 * @ingroup MQTT
 */
typedef enum {
  net_mqtt_status_connected_t =
      (1 << 0), /**< Connected to the broker and session is active. */
  net_mqtt_status_disconnected_t =
      (1 << 1), /**< Disconnected from the broker (remote close or local
                   request). */
  net_mqtt_status_dns_error_t = (1 << 2), /**< DNS resolution failed. */
  net_mqtt_status_protocol_error_t =
      (1 << 3), /**< Protocol-level error (malformed packet, unsupported
                   feature, etc). */
  net_mqtt_status_auth_error_t =
      (1
       << 4), /**< Authentication failure (bad client ID/username/password). */
  net_mqtt_status_timeout_t =
      (1 << 5), /**< Operation timed out (connect or in-flight request exceeded
                   deadline). */
  net_mqtt_status_error_t =
      (1 << 6), /**< Generic error occurred (e.g., connection failed). */
} net_mqtt_status_t;

/**
 * @brief Callback invoked on connection state changes and errors.
 * @ingroup MQTT
 *
 * @param mqtt      The MQTT instance that generated the event.
 * @param status    One or more @ref net_mqtt_status_t flags describing the
 *                  event. Typically a single flag such as
 *                  net_mqtt_status_connected_t or
 *                  net_mqtt_status_disconnected_t.
 * @param user_data Opaque pointer supplied at initialization time. Not
 *                  interpreted by the runtime.
 */
typedef void (*net_mqtt_connect_callback_t)(net_mqtt_t *mqtt,
                                            net_mqtt_status_t status,
                                            void *user_data);

/**
 * @brief Callback invoked when a message is received on a subscribed topic.
 * @ingroup MQTT
 *
 * @param mqtt      The MQTT instance that received the message.
 * @param topic     The topic the message was published to.
 * @param data      The message payload.
 * @param size      Payload length in bytes.
 * @param user_data Opaque pointer supplied at initialization time.
 */
typedef void (*net_mqtt_message_callback_t)(net_mqtt_t *mqtt,
                                            const char *topic,
                                            const void *data, size_t size,
                                            void *user_data);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Create and initialize an MQTT handle.
 * @ingroup MQTT
 *
 * @param connect   Callback to receive connection status updates.
 * @param user_data Opaque pointer passed back to @p connect.
 * @return net_mqtt_t* An MQTT handle on success; NULL on failure.
 *
 * The returned handle is opaque and must be released with
 * @ref net_mqtt_deinit after disconnecting.
 */
net_mqtt_t *net_mqtt_init(net_mqtt_connect_callback_t connect,
                          net_mqtt_message_callback_t message, void *user_data);

/**
 * @brief Finalize the MQTT handle and free associated resources.
 * @ingroup MQTT
 *
 * @param mqtt The MQTT handle to finalize. NULL is allowed and is a no-op.
 *
 * The caller should ensure the connection is closed (see
 * @ref net_mqtt_disconnect) and that no callbacks are in flight before calling
 * this function.
 */
void net_mqtt_deinit(net_mqtt_t *mqtt);

/**
 * @brief Check whether an MQTT client is valid and connected.
 * @ingroup MQTT
 *
 * @param mqtt The MQTT handle to check.
 * @return true if the handle is initialized and currently connected to a
 *         broker, false otherwise.
 */
bool net_mqtt_valid(net_mqtt_t *mqtt);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Begin an asynchronous connection to an MQTT broker.
 * @ingroup MQTT
 *
 * @param mqtt            MQTT handle created by @ref net_mqtt_init.
 * @param hostname        Broker hostname or IP address.
 * @param port            Broker port (e.g., 1883 for MQTT, 8883 for MQTT over
 * TLS). Pass 0 to use the default port (1883).
 * @param client_id       Client identifier. May be NULL to use an
 * implementation-defined or stack-provided default if available.
 * @param user            Username for authentication. May be NULL for
 * anonymous.
 * @param password        Password for authentication. Ignored if @p user is
 * NULL.
 * @param keepalive_secs  Keep-alive interval in seconds.
 * @param will_topic      Optional Last Will topic. May be NULL to disable will.
 * @param will_message    Optional Last Will message. If provided, a non-NULL
 *                        @p will_topic is also expected.
 * @return true if the connection attempt was successfully initiated;
 *         false on immediate failure (invalid parameters or state).
 *
 * Completion and errors are reported via the connect callback provided at
 * initialization (see @ref net_mqtt_connect_callback_t) with one of the
 * @ref net_mqtt_status_t values.
 */
bool net_mqtt_connect(net_mqtt_t *mqtt, const char *hostname, uint16_t port,
                      const char *client_id, const char *user,
                      const char *password, uint16_t keepalive_secs,
                      const char *will_topic, const char *will_message);

/**
 * @brief Begin an asynchronous disconnect from the MQTT broker.
 * @ingroup MQTT
 *
 * @param mqtt MQTT handle created by @ref net_mqtt_init.
 * @return true if the disconnect was successfully initiated; false otherwise.
 *
 * When the connection is fully closed, the connect callback will be invoked
 * with @ref net_mqtt_status_disconnected_t.
 */
bool net_mqtt_disconnect(net_mqtt_t *mqtt);

/**
 * @brief Publish a message to a topic.
 * @ingroup MQTT
 *
 * @param mqtt   MQTT handle created by @ref net_mqtt_init and currently
 *               connected (see @ref net_mqtt_valid).
 * @param topic  Topic name. Must be non-NULL and non-empty.
 * @param data   Message payload. Must be non-NULL when @p size is non-zero.
 * @param size   Payload length in bytes.
 * @param qos    QoS level: 0, 1, or 2.
 * @return `true` if the message was accepted for transmission; `false` on
 *         failure (not connected, invalid arguments, or broker error).
 */
bool net_mqtt_publish(net_mqtt_t *mqtt, const char *topic, const void *data,
                      size_t size, uint8_t qos);

/**
 * @brief Publish a NUL-terminated string to a topic.
 * @ingroup MQTT
 *
 * Convenience wrapper around @ref net_mqtt_publish that derives the payload
 * length with @ref sys_strlen.
 *
 * @param mqtt   MQTT handle created by @ref net_mqtt_init and currently
 *               connected (see @ref net_mqtt_valid).
 * @param topic  Topic name. Must be non-NULL and non-empty.
 * @param str    NUL-terminated string payload. Must be non-NULL.
 * @param qos    QoS level: 0, 1, or 2.
 * @return `true` if the message was accepted for transmission; `false` on
 *         failure.
 */
static inline bool net_mqtt_publish_str(net_mqtt_t *mqtt, const char *topic,
                                        const char *str, uint8_t qos) {
  return net_mqtt_publish(mqtt, topic, str, sys_strlen(str), qos);
}

/**
 * @brief Subscribe to a topic.
 * @ingroup MQTT
 *
 * @param mqtt   MQTT handle created by @ref net_mqtt_init and currently
 *               connected (see @ref net_mqtt_valid).
 * @param topic  Topic filter to subscribe to. Must be non-NULL and non-empty.
 * @param qos    Maximum QoS level the broker should use when delivering
 *               messages for this subscription: 0, 1, or 2.
 * @return `true` if the subscription request was accepted; `false` on failure.
 */
bool net_mqtt_subscribe(net_mqtt_t *mqtt, const char *topic, uint8_t qos);

/**
 * @brief Unsubscribe from a topic.
 * @ingroup MQTT
 *
 * @param mqtt   MQTT handle created by @ref net_mqtt_init and currently
 *               connected (see @ref net_mqtt_valid).
 * @param topic  Topic filter to unsubscribe from. Must be non-NULL and
 *               non-empty.
 * @return `true` if the unsubscribe request was accepted; `false` on failure.
 */
bool net_mqtt_unsubscribe(net_mqtt_t *mqtt, const char *topic);

/** @} */

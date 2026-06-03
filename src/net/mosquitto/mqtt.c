#include "private.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;
net_mqtt_t _instance = {0};
static bool _lib_init = false;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _lib_ensure_init(void) {
  pthread_mutex_lock(&_lock);
  if (!_lib_init) {
    mosquitto_lib_init();
    _lib_init = true;
  }
  pthread_mutex_unlock(&_lock);
}

static void _on_connect(struct mosquitto *mosq, void *obj, int rc) {
  (void)mosq;
  net_mqtt_t *mqtt = (net_mqtt_t *)obj;
  if (mqtt == NULL || mqtt->connect_callback == NULL) {
    return;
  }
  net_mqtt_status_t status;
  switch (rc) {
  case 0:
    mqtt->connected = true;
    status = net_mqtt_status_connected_t;
    break;
  case 4:
  case 5:
    status = net_mqtt_status_auth_error_t;
    break;
  default:
    status = net_mqtt_status_protocol_error_t;
    break;
  }
  mqtt->connect_callback(mqtt, status, mqtt->user_data);
}

static void _on_message(struct mosquitto *mosq, void *obj,
                        const struct mosquitto_message *msg) {
  (void)mosq;
  net_mqtt_t *mqtt = (net_mqtt_t *)obj;
  if (mqtt == NULL || mqtt->message_callback == NULL) {
    return;
  }
  mqtt->message_callback(mqtt, msg->topic, msg->payload,
                         (size_t)msg->payloadlen, mqtt->user_data);
}

static void _on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
  (void)mosq;
  net_mqtt_t *mqtt = (net_mqtt_t *)obj;
  if (mqtt == NULL || mqtt->connect_callback == NULL) {
    return;
  }
  mqtt->connected = false;
  net_mqtt_status_t status =
      (rc == 0) ? net_mqtt_status_disconnected_t : net_mqtt_status_error_t;
  mqtt->connect_callback(mqtt, status, mqtt->user_data);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC


net_mqtt_t *net_mqtt_init(net_mqtt_connect_callback_t connect,
                          net_mqtt_message_callback_t message,
                          void *user_data) {
  _lib_ensure_init();

  pthread_mutex_lock(&_lock);
  if (_instance.init) {
    pthread_mutex_unlock(&_lock);
    return NULL;
  }
  _instance.connect_callback = connect;
  _instance.message_callback = message;
  _instance.user_data = user_data;
  _instance.mosq = NULL;
  _instance.connected = false;
  _instance.init = true;
  pthread_mutex_unlock(&_lock);
  return &_instance;
}

void net_mqtt_deinit(net_mqtt_t *mqtt) {
  if (mqtt == NULL || !mqtt->init) {
    return;
  }
  pthread_mutex_lock(&_lock);
  if (mqtt->mosq != NULL) {
    mosquitto_destroy(mqtt->mosq);
    mqtt->mosq = NULL;
  }
  mqtt->connected = false;
  mqtt->init = false;
  pthread_mutex_unlock(&_lock);
}

bool net_mqtt_valid(net_mqtt_t *mqtt) {
  return mqtt != NULL && mqtt->init && mqtt->connected;
}

bool net_mqtt_connect(net_mqtt_t *mqtt, const char *hostname, uint16_t port,
                      const char *client_id, const char *user,
                      const char *password, uint16_t keepalive_secs,
                      const char *will_topic, const char *will_message) {
  if (mqtt == NULL || !mqtt->init || hostname == NULL) {
    return false;
  }

  pthread_mutex_lock(&_lock);

  if (mqtt->mosq != NULL) {
    mosquitto_destroy(mqtt->mosq);
    mqtt->mosq = NULL;
  }

  struct mosquitto *mosq = mosquitto_new(client_id, true, mqtt);
  if (mosq == NULL) {
    pthread_mutex_unlock(&_lock);
    return false;
  }

  mosquitto_connect_callback_set(mosq, _on_connect);
  mosquitto_disconnect_callback_set(mosq, _on_disconnect);
  mosquitto_message_callback_set(mosq, _on_message);

  if (user != NULL) {
    if (mosquitto_username_pw_set(mosq, user, password) != MOSQ_ERR_SUCCESS) {
      mosquitto_destroy(mosq);
      pthread_mutex_unlock(&_lock);
      return false;
    }
  }

  if (will_topic != NULL && will_message != NULL) {
    if (mosquitto_will_set(mosq, will_topic, (int)strlen(will_message),
                           will_message, 0, false) != MOSQ_ERR_SUCCESS) {
      mosquitto_destroy(mosq);
      pthread_mutex_unlock(&_lock);
      return false;
    }
  }

  mqtt->mosq = mosq;
  mqtt->connected = false;
  pthread_mutex_unlock(&_lock);

  // mosquitto_connect is required for polling mode (mosquitto_loop).
  // The on_connect callback fires asynchronously via net_poll().
  int actual_port = port > 0 ? port : 1883;
  int actual_keepalive = keepalive_secs > 0 ? keepalive_secs : 60;
  return mosquitto_connect(mosq, hostname, actual_port, actual_keepalive) ==
         MOSQ_ERR_SUCCESS;
}

bool net_mqtt_publish(net_mqtt_t *mqtt, const char *topic, const void *data,
                      size_t size, uint8_t qos) {
  if (mqtt == NULL || !mqtt->init || !mqtt->connected) {
    return false;
  }
  if (topic == NULL || topic[0] == '\0') {
    return false;
  }
  if (size > 0 && data == NULL) {
    return false;
  }
  if (qos > 2 || size > INT_MAX) {
    return false;
  }
  return mosquitto_publish(mqtt->mosq, NULL, topic, (int)size, data, (int)qos,
                           false) == MOSQ_ERR_SUCCESS;
}

bool net_mqtt_subscribe(net_mqtt_t *mqtt, const char *topic, uint8_t qos) {
  if (mqtt == NULL || !mqtt->init || !mqtt->connected || topic == NULL) {
    return false;
  }
  return mosquitto_subscribe(mqtt->mosq, NULL, topic, (int)qos) ==
         MOSQ_ERR_SUCCESS;
}

bool net_mqtt_unsubscribe(net_mqtt_t *mqtt, const char *topic) {
  if (mqtt == NULL || !mqtt->init || !mqtt->connected || topic == NULL) {
    return false;
  }
  return mosquitto_unsubscribe(mqtt->mosq, NULL, topic) == MOSQ_ERR_SUCCESS;
}

bool net_mqtt_disconnect(net_mqtt_t *mqtt) {
  if (mqtt == NULL || !mqtt->init || mqtt->mosq == NULL) {
    return false;
  }
  return mosquitto_disconnect(mqtt->mosq) == MOSQ_ERR_SUCCESS;
}

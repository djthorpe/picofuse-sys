#include <picofuse/net.h>
#include <stddef.h>

struct net_mqtt_t {
  bool init;
};


net_mqtt_t *net_mqtt_init(net_mqtt_connect_callback_t connect,
                          net_mqtt_message_callback_t message,
                          void *user_data) {
  (void)connect;
  (void)message;
  (void)user_data;
  return NULL;
}

void net_mqtt_deinit(net_mqtt_t *mqtt) { (void)mqtt; }

bool net_mqtt_valid(net_mqtt_t *mqtt) {
  (void)mqtt;
  return false;
}

bool net_mqtt_connect(net_mqtt_t *mqtt, const char *hostname, uint16_t port,
                      const char *client_id, const char *user,
                      const char *password, uint16_t keepalive_secs,
                      const char *will_topic, const char *will_message) {
  (void)mqtt;
  (void)hostname;
  (void)port;
  (void)client_id;
  (void)user;
  (void)password;
  (void)keepalive_secs;
  (void)will_topic;
  (void)will_message;
  return false;
}

bool net_mqtt_disconnect(net_mqtt_t *mqtt) {
  (void)mqtt;
  return false;
}

bool net_mqtt_publish(net_mqtt_t *mqtt, const char *topic, const void *data,
                      size_t size, uint8_t qos) {
  (void)mqtt;
  (void)topic;
  (void)data;
  (void)size;
  (void)qos;
  return false;
}

bool net_mqtt_subscribe(net_mqtt_t *mqtt, const char *topic, uint8_t qos) {
  (void)mqtt;
  (void)topic;
  (void)qos;
  return false;
}

bool net_mqtt_unsubscribe(net_mqtt_t *mqtt, const char *topic) {
  (void)mqtt;
  (void)topic;
  return false;
}

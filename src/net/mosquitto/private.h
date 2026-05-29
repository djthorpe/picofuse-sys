#pragma once
#include <mosquitto.h>
#include <picofuse/net.h>
#include <pthread.h>

struct net_mqtt_t {
  struct mosquitto *mosq;
  net_mqtt_connect_callback_t connect_callback;
  net_mqtt_message_callback_t message_callback;
  void *user_data;
  bool init;
  bool connected;
};

extern pthread_mutex_t _lock;
extern net_mqtt_t _instance;

#include "private.h"

void net_poll(void) {
  struct mosquitto *mosq = NULL;
  pthread_mutex_lock(&_lock);
  if (_instance.init && _instance.mosq != NULL) {
    mosq = _instance.mosq;
  }
  pthread_mutex_unlock(&_lock);
  if (mosq != NULL) {
    mosquitto_loop(mosq, 0, 1);
  }
}

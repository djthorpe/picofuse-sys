#include <picofuse/hw.h>
#include <picofuse/net.h>
#include <picofuse/sys.h>

#ifdef PICO_CYW43_SUPPORTED
#include <lwip/apps/mqtt.h>
#include <lwip/dns.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef struct {
  uint16_t port;
  struct mqtt_connect_client_info_t client_info;
} _net_mqtt_server_t;

struct net_mqtt_t {
  mqtt_client_t *client;
  net_mqtt_connect_callback_t connect_cb;
  net_mqtt_message_callback_t message_cb;
  void *user_data;
  _net_mqtt_server_t server;
  bool init;
};

static struct net_mqtt_t _instance = {0};

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

#ifdef PICO_CYW43_SUPPORTED
static void _net_mqtt_dns_cb(const char *name, const ip_addr_t *ipaddr, void *arg);
static void _net_mqtt_do_connect(net_mqtt_t *mqtt, const ip_addr_t *ipaddr);
static void _net_mqtt_connection_cb(mqtt_client_t *client, void *arg,
                                    mqtt_connection_status_t status);
static void _net_mqtt_publish_cb(void *arg, err_t err);
static void _net_mqtt_subscribe_cb(void *arg, err_t err);
static void _net_mqtt_inpub_cb(void *arg, const char *topic, u32_t tot_len);
static void _net_mqtt_inpub_data_cb(void *arg, const u8_t *data, u16_t len,
                                    u8_t flags);
static bool _net_mqtt_server_valid(const _net_mqtt_server_t *server);
static _net_mqtt_server_t _net_mqtt_server_new(const char *hostname, uint16_t port,
                                               const char *client_id,
                                               const char *user,
                                               const char *password,
                                               uint16_t keepalive_secs,
                                               const char *will_topic,
                                               const char *will_message);
static void _net_mqtt_server_free(_net_mqtt_server_t *server);
static char *_net_mqtt_strdup(const char *s);
#endif

#define _ERR_PENDING 1

///////////////////////////////////////////////////////////////////////////////
// PUBLIC


net_mqtt_t *net_mqtt_init(net_mqtt_connect_callback_t connect_cb,
                          net_mqtt_message_callback_t message_cb,
                          void *user_data) {
  sys_assert(connect_cb != NULL);
  if (_instance.init) {
    return NULL;
  }

#ifdef PICO_CYW43_SUPPORTED
  mqtt_client_t *client = mqtt_client_new();
  if (client == NULL) {
    return NULL;
  }
  _instance.client = client;
  mqtt_set_inpub_callback(client, _net_mqtt_inpub_cb, _net_mqtt_inpub_data_cb,
                          &_instance);
#endif

  _instance.connect_cb = connect_cb;
  _instance.message_cb = message_cb;
  _instance.user_data = user_data;
  _instance.init = true;
  return &_instance;
}

void net_mqtt_deinit(net_mqtt_t *mqtt) {
  if (mqtt == NULL || !mqtt->init) {
    return;
  }
#ifdef PICO_CYW43_SUPPORTED
  mqtt_disconnect(mqtt->client);
  mqtt_client_free(mqtt->client);
  _net_mqtt_server_free(&mqtt->server);
#endif
  memset(mqtt, 0, sizeof(struct net_mqtt_t));
}

bool net_mqtt_valid(net_mqtt_t *mqtt) {
  if (mqtt == NULL || !mqtt->init) {
    return false;
  }
#ifdef PICO_CYW43_SUPPORTED
  return mqtt_client_is_connected(mqtt->client);
#else
  return false;
#endif
}

bool net_mqtt_connect(net_mqtt_t *mqtt, const char *hostname, uint16_t port,
                      const char *client_id, const char *user,
                      const char *password, uint16_t keepalive_secs,
                      const char *will_topic, const char *will_message) {
  sys_assert(mqtt != NULL && mqtt->init);
  sys_assert(hostname != NULL);

#ifndef PICO_CYW43_SUPPORTED
  (void)port;
  (void)client_id;
  (void)user;
  (void)password;
  (void)keepalive_secs;
  (void)will_topic;
  (void)will_message;
  return false;
#else
  if (mqtt_client_is_connected(mqtt->client)) {
    return false;
  }
  sys_assert(!_net_mqtt_server_valid(&mqtt->server));

  mqtt->server = _net_mqtt_server_new(hostname, port, client_id, user, password,
                             keepalive_secs, will_topic, will_message);
  if (!_net_mqtt_server_valid(&mqtt->server)) {
    return false;
  }

  ip_addr_t addr;
  err_t err = dns_gethostbyname(hostname, &addr, _net_mqtt_dns_cb, mqtt);
  if (err == ERR_OK) {
    _net_mqtt_do_connect(mqtt, &addr);
    return true;
  } else if (err == ERR_INPROGRESS) {
    return true;
  } else {
    if (mqtt->connect_cb) {
      mqtt->connect_cb(mqtt, net_mqtt_status_dns_error_t, mqtt->user_data);
    }
    _net_mqtt_server_free(&mqtt->server);
    return false;
  }
#endif
}

bool net_mqtt_disconnect(net_mqtt_t *mqtt) {
  sys_assert(mqtt != NULL && mqtt->init);
#ifndef PICO_CYW43_SUPPORTED
  return false;
#else
  if (!mqtt_client_is_connected(mqtt->client)) {
    return false;
  }
  mqtt_disconnect(mqtt->client);
  if (mqtt->connect_cb) {
    mqtt->connect_cb(mqtt, net_mqtt_status_disconnected_t, mqtt->user_data);
  }
  return true;
#endif
}

bool net_mqtt_publish(net_mqtt_t *mqtt, const char *topic, const void *data,
                      size_t size, uint8_t qos) {
  sys_assert(mqtt != NULL && mqtt->init);
  sys_assert(topic != NULL && topic[0] != '\0');
  sys_assert(size == 0 || data != NULL);
  sys_assert(qos <= 2);
#ifndef PICO_CYW43_SUPPORTED
  return false;
#else
  if (!mqtt_client_is_connected(mqtt->client)) {
    return false;
  }
  if (size > UINT16_MAX) {
    return false;
  }
  err_t err = _ERR_PENDING;
  if (mqtt_publish(mqtt->client, topic, data, (u16_t)size, qos, qos != 0,
                   _net_mqtt_publish_cb, &err) != ERR_OK) {
    return false;
  }
  while (err == _ERR_PENDING) {
    hw_poll();
    sys_sleep_ms(10);
  }
  return err == ERR_OK;
#endif
}

bool net_mqtt_subscribe(net_mqtt_t *mqtt, const char *topic, uint8_t qos) {
  sys_assert(mqtt != NULL && mqtt->init);
  sys_assert(topic != NULL && topic[0] != '\0');
  sys_assert(qos <= 2);
#ifndef PICO_CYW43_SUPPORTED
  return false;
#else
  if (!mqtt_client_is_connected(mqtt->client)) {
    return false;
  }
  return mqtt_subscribe(mqtt->client, topic, qos, _net_mqtt_subscribe_cb,
                        mqtt) == ERR_OK;
#endif
}

bool net_mqtt_unsubscribe(net_mqtt_t *mqtt, const char *topic) {
  sys_assert(mqtt != NULL && mqtt->init);
  sys_assert(topic != NULL && topic[0] != '\0');
#ifndef PICO_CYW43_SUPPORTED
  return false;
#else
  if (!mqtt_client_is_connected(mqtt->client)) {
    return false;
  }
  return mqtt_unsubscribe(mqtt->client, topic, _net_mqtt_subscribe_cb,
                          mqtt) == ERR_OK;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

#ifdef PICO_CYW43_SUPPORTED

static void _net_mqtt_dns_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
  (void)name;
  net_mqtt_t *mqtt = (net_mqtt_t *)arg;
  if (ipaddr == NULL) {
    if (mqtt->connect_cb) {
      mqtt->connect_cb(mqtt, net_mqtt_status_dns_error_t, mqtt->user_data);
    }
    _net_mqtt_server_free(&mqtt->server);
  } else {
    _net_mqtt_do_connect(mqtt, ipaddr);
  }
}

static void _net_mqtt_do_connect(net_mqtt_t *mqtt, const ip_addr_t *addr) {
  sys_assert(mqtt != NULL && addr != NULL && _net_mqtt_server_valid(&mqtt->server));
  err_t err = mqtt_client_connect(mqtt->client, addr, mqtt->server.port,
                                  _net_mqtt_connection_cb, mqtt,
                                  &mqtt->server.client_info);
  if (err != ERR_OK) {
    if (mqtt->connect_cb) {
      mqtt->connect_cb(mqtt, net_mqtt_status_error_t, mqtt->user_data);
    }
    _net_mqtt_server_free(&mqtt->server);
  }
}

static void _net_mqtt_connection_cb(mqtt_client_t *client, void *arg,
                           mqtt_connection_status_t status) {
  (void)client;
  net_mqtt_t *mqtt = (net_mqtt_t *)arg;
  sys_assert(mqtt != NULL);

  net_mqtt_status_t s;
  switch (status) {
  case MQTT_CONNECT_ACCEPTED:
    s = net_mqtt_status_connected_t;
    break;
  case MQTT_CONNECT_REFUSED_USERNAME_PASS:
  case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
    s = net_mqtt_status_auth_error_t;
    break;
  case MQTT_CONNECT_TIMEOUT:
    s = net_mqtt_status_timeout_t;
    break;
  case MQTT_CONNECT_DISCONNECTED:
    s = net_mqtt_status_disconnected_t;
    break;
  default:
    s = net_mqtt_status_protocol_error_t;
    break;
  }

  if (mqtt->connect_cb) {
    mqtt->connect_cb(mqtt, s, mqtt->user_data);
  }
  if (status != MQTT_CONNECT_ACCEPTED) {
    _net_mqtt_server_free(&mqtt->server);
  }
}

static bool _net_mqtt_server_valid(const _net_mqtt_server_t *server) {
  return server != NULL && server->port != 0;
}

static _net_mqtt_server_t _net_mqtt_server_new(const char *hostname, uint16_t port,
                              const char *client_id, const char *user,
                              const char *password, uint16_t keepalive_secs,
                              const char *will_topic,
                              const char *will_message) {
  (void)hostname;
  _net_mqtt_server_t s = {0};
  s.port = port ? port : MQTT_PORT;
  s.client_info.keep_alive = keepalive_secs;

  s.client_info.client_id = client_id
                                 ? _net_mqtt_strdup(client_id)
                                 : _net_mqtt_strdup(sys_env_serial());
  if (s.client_info.client_id == NULL) goto fail;
  if (user) {
    s.client_info.client_user = _net_mqtt_strdup(user);
    if (s.client_info.client_user == NULL) goto fail;
  }
  if (password) {
    s.client_info.client_pass = _net_mqtt_strdup(password);
    if (s.client_info.client_pass == NULL) goto fail;
  }
  if (will_topic) {
    s.client_info.will_topic = _net_mqtt_strdup(will_topic);
    if (s.client_info.will_topic == NULL) goto fail;
  }
  if (will_message) {
    s.client_info.will_msg = _net_mqtt_strdup(will_message);
    if (s.client_info.will_msg == NULL) goto fail;
  }
  return s;

fail:
  _net_mqtt_server_free(&s);
  return s;
}

static void _net_mqtt_server_free(_net_mqtt_server_t *server) {
  if (server == NULL || server->port == 0) {
    return;
  }
  sys_free((void *)server->client_info.client_id);
  sys_free((void *)server->client_info.client_user);
  sys_free((void *)server->client_info.client_pass);
  sys_free((void *)server->client_info.will_topic);
  sys_free((void *)server->client_info.will_msg);
  sys_memset(server, 0, sizeof(_net_mqtt_server_t));
}

static char *_net_mqtt_strdup(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  size_t n = sys_strlen(s) + 1;
  char *p = sys_malloc(n);
  if (p != NULL) {
    sys_memcpy(p, s, n);
  }
  return p;
}

static char _inpub_topic[256];

static void _net_mqtt_inpub_cb(void *arg, const char *topic, u32_t tot_len) {
  (void)arg;
  (void)tot_len;
  if (topic != NULL) {
    size_t n = sys_strlen(topic);
    if (n >= sizeof(_inpub_topic)) {
      n = sizeof(_inpub_topic) - 1;
    }
    sys_memcpy(_inpub_topic, topic, n);
    _inpub_topic[n] = '\0';
  }
}

static void _net_mqtt_inpub_data_cb(void *arg, const u8_t *data, u16_t len,
                                    u8_t flags) {
  net_mqtt_t *mqtt = (net_mqtt_t *)arg;
  if (mqtt == NULL || mqtt->message_cb == NULL) {
    return;
  }
  if (flags & MQTT_DATA_FLAG_LAST) {
    mqtt->message_cb(mqtt, _inpub_topic, data, (size_t)len, mqtt->user_data);
  }
}

static void _net_mqtt_publish_cb(void *arg, err_t err) {
  err_t *result = (err_t *)arg;
  sys_assert(result != NULL);
  *result = err;
}

static void _net_mqtt_subscribe_cb(void *arg, err_t err) {
  (void)arg;
  (void)err;
}

#endif

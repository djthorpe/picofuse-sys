#include <picofuse/hw.h>
#include <picofuse/sys.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wpa_ctrl.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Handle for Linux Wi-Fi state.
 */
struct hw_wifi_t {
  hw_wifi_callback_t callback;
  void *userdata;
  struct wpa_ctrl *ctrl;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_wifi_t _hw_wifi_instance = {0};
static const char _hw_wifi_ctrl_path[] = "/var/run/wpa_supplicant/wlan0";

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _hw_wifi_request(hw_wifi_t *wifi, const char *command, char *reply,
                             size_t *reply_len) {
  if (!wifi->init || wifi->ctrl == NULL || command == NULL || reply == NULL ||
      reply_len == NULL) {
    return false;
  }

  if (wpa_ctrl_request(wifi->ctrl, command, strlen(command), reply, reply_len,
                       NULL) != 0) {
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_wifi_t *hw_wifi_init_client(const char *country_code,
                               hw_wifi_callback_t callback, void *userdata) {
  (void)country_code;

  hw_wifi_deinit(&_hw_wifi_instance);

  if (callback == NULL) {
    return NULL;
  }

  struct wpa_ctrl *ctrl = wpa_ctrl_open(_hw_wifi_ctrl_path);
  if (ctrl == NULL) {
    return NULL;
  }

  _hw_wifi_instance.callback = callback;
  _hw_wifi_instance.userdata = userdata;
  _hw_wifi_instance.ctrl = ctrl;
  _hw_wifi_instance.init = true;

  return &_hw_wifi_instance;
}

bool hw_wifi_valid(hw_wifi_t *wifi) {
  return wifi != NULL && wifi->init && wifi->ctrl != NULL;
}

void hw_wifi_deinit(hw_wifi_t *wifi) {
  if (wifi == NULL) {
    return;
  }

  if (wifi->ctrl != NULL) {
    wpa_ctrl_close(wifi->ctrl);
  }

  sys_memset(wifi, 0, sizeof(*wifi));
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool hw_wifi_scan(hw_wifi_t *wifi) {
  if (!hw_wifi_valid(wifi)) {
    return false;
  }

  char reply[64] = {0};
  size_t reply_len = sizeof(reply) - 1;
  if (!_hw_wifi_request(wifi, "SCAN", reply, &reply_len)) {
    return false;
  }

  reply[reply_len] = '\0';
  if (strncmp(reply, "OK", 2) != 0) {
    return false;
  }

  // Minimal completion callback; detailed scan result parsing can be added.
  wifi->callback(wifi, hw_wifi_event_scan, NULL, wifi->userdata);
  return true;
}

bool hw_wifi_connect(hw_wifi_t *wifi, const hw_wifi_network_t *network,
                     const char *password) {
  (void)network;
  (void)password;

  if (!hw_wifi_valid(wifi)) {
    return false;
  }

  // Connection flow is intentionally deferred; this backend currently provides
  // initialization and scan plumbing only.
  return false;
}

bool hw_wifi_disconnect(hw_wifi_t *wifi) {
  if (!hw_wifi_valid(wifi)) {
    return false;
  }

  char reply[64] = {0};
  size_t reply_len = sizeof(reply) - 1;
  if (!_hw_wifi_request(wifi, "DISCONNECT", reply, &reply_len)) {
    return false;
  }

  reply[reply_len] = '\0';
  if (strncmp(reply, "OK", 2) != 0) {
    return false;
  }

  wifi->callback(wifi, hw_wifi_event_disconnected, NULL, wifi->userdata);
  return true;
}

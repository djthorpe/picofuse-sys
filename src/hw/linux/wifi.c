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
static const uint32_t _hw_wifi_scan_timeout_ms = 15000;
static const uint32_t _hw_wifi_scan_poll_ms = 100;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static uint8_t _hw_wifi_frequency_to_channel(int frequency_mhz) {
  if (frequency_mhz == 2484) {
    return 14;
  }

  if (frequency_mhz >= 2412 && frequency_mhz <= 2472) {
    return (uint8_t)((frequency_mhz - 2407) / 5);
  }

  if (frequency_mhz >= 5000 && frequency_mhz <= 5900) {
    return (uint8_t)((frequency_mhz - 5000) / 5);
  }

  return 0;
}

static bool _hw_wifi_parse_bssid(const char *bssid_text, uint8_t bssid[6]) {
  unsigned int octets[6] = {0};
  if (sscanf(bssid_text, "%2x:%2x:%2x:%2x:%2x:%2x", &octets[0], &octets[1],
             &octets[2], &octets[3], &octets[4], &octets[5]) != 6) {
    return false;
  }

  for (size_t i = 0; i < 6; i++) {
    bssid[i] = (uint8_t)octets[i];
  }

  return true;
}

static hw_wifi_auth_t _hw_wifi_parse_auth(const char *flags) {
  hw_wifi_auth_t auth = 0;

  if (flags == NULL) {
    return hw_wifi_auth_open;
  }

  if (strstr(flags, "WEP") != NULL) {
    auth |= hw_wifi_auth_wep;
  }

  if (strstr(flags, "WPA-") != NULL || strstr(flags, "WPA]") != NULL) {
    if (strstr(flags, "TKIP") != NULL) {
      auth |= hw_wifi_auth_wpa_tkip;
    }
    if (strstr(flags, "CCMP") != NULL || strstr(flags, "AES") != NULL) {
      auth |= hw_wifi_auth_wpa_aes;
    }
  }

  if (strstr(flags, "WPA2") != NULL || strstr(flags, "RSN") != NULL) {
    if (strstr(flags, "TKIP") != NULL) {
      auth |= hw_wifi_auth_wpa2_tkip;
    }
    if (strstr(flags, "CCMP") != NULL || strstr(flags, "AES") != NULL ||
        strstr(flags, "RSN") != NULL) {
      auth |= hw_wifi_auth_wpa2_aes;
    }
  }

  if (strstr(flags, "SAE") != NULL) {
    auth |= hw_wifi_auth_wpa3_sae;
  }

  if (strstr(flags, "EAP") != NULL || strstr(flags, "IEEE8021X") != NULL) {
    auth |= hw_wifi_auth_enterprise;
  }

  if (auth == 0) {
    auth = hw_wifi_auth_open;
  }

  return auth;
}

static bool _hw_wifi_is_scanning(hw_wifi_t *wifi) {
  char reply[512] = {0};
  size_t reply_len = sizeof(reply) - 1;

  if (!_hw_wifi_request(wifi, "STATUS", reply, &reply_len)) {
    return false;
  }

  reply[reply_len] = '\0';
  return strstr(reply, "wpa_state=SCANNING") != NULL;
}

static bool _hw_wifi_wait_for_scan_complete(hw_wifi_t *wifi) {
  uint64_t deadline = sys_timestamp_ms() + _hw_wifi_scan_timeout_ms;

  while (sys_timestamp_ms() < deadline) {
    if (!_hw_wifi_is_scanning(wifi)) {
      return true;
    }
    sys_sleep_ms(_hw_wifi_scan_poll_ms);
  }

  return false;
}

static bool _hw_wifi_emit_scan_results(hw_wifi_t *wifi) {
  char results[24576] = {0};
  size_t results_len = sizeof(results) - 1;

  if (!_hw_wifi_request(wifi, "SCAN_RESULTS", results, &results_len)) {
    return false;
  }

  results[results_len] = '\0';

  char *save_line = NULL;
  char *line = strtok_r(results, "\n", &save_line);
  bool header_seen = false;

  while (line != NULL) {
    if (!header_seen) {
      header_seen = true;
      line = strtok_r(NULL, "\n", &save_line);
      continue;
    }

    char *save_field = NULL;
    char *bssid = strtok_r(line, "\t", &save_field);
    char *freq = strtok_r(NULL, "\t", &save_field);
    char *signal = strtok_r(NULL, "\t", &save_field);
    char *flags = strtok_r(NULL, "\t", &save_field);
    char *ssid = strtok_r(NULL, "", &save_field);

    if (bssid != NULL && freq != NULL && signal != NULL && flags != NULL) {
      hw_wifi_network_t network = {0};

      if (_hw_wifi_parse_bssid(bssid, network.bssid)) {
        int frequency_mhz = atoi(freq);
        int rssi_dbm = atoi(signal);

        network.channel = _hw_wifi_frequency_to_channel(frequency_mhz);
        network.rssi = (int16_t)rssi_dbm;
        network.auth = _hw_wifi_parse_auth(flags);

        if (ssid != NULL) {
          size_t ssid_len = strlen(ssid);
          if (ssid_len > HW_WIFI_SSID_MAX_LENGTH) {
            ssid_len = HW_WIFI_SSID_MAX_LENGTH;
          }
          memcpy(network.ssid, ssid, ssid_len);
          network.ssid[ssid_len] = '\0';
        }

        wifi->callback(wifi, hw_wifi_event_scan, &network, wifi->userdata);
      }
    }

    line = strtok_r(NULL, "\n", &save_line);
  }

  wifi->callback(wifi, hw_wifi_event_scan, NULL, wifi->userdata);
  return true;
}

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

  if (!_hw_wifi_wait_for_scan_complete(wifi)) {
    return false;
  }

  return _hw_wifi_emit_scan_results(wifi);
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

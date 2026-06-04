#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <string.h>

#ifdef PICO_CYW43_SUPPORTED
#include "cyw43.h"
#include "cyw43_country.h"
#include <pico/cyw43_arch.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Handle for Wi-Fi state.
 */
struct hw_wifi_t {
  char country_code[3];
  hw_wifi_callback_t callback;
  void *userdata;
  sys_atomic_t flags;
  int state;
  hw_wifi_network_t network;
  uint64_t ts;
};

/**
 * @brief Flags for Wi-Fi state.
 */
typedef enum {
  hw_wifi_flag_scanning = (1 << 1), ///< Scanning
  hw_wifi_flag_joining = (1 << 2),  ///< Joining
  hw_wifi_flag_leaving = (1 << 3),  ///< Leaving
} hw_wifi_state_t;

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

#ifdef PICO_CYW43_SUPPORTED
static struct hw_wifi_t _hw_wifi_instance = {0};
static const char _hw_wifi_default_country_code[] = "XX";
static uint32_t _hw_wifi_status_interval_ms = (1000 * 60);
#endif

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

#ifdef PICO_CYW43_SUPPORTED
static inline int _hw_wifi_link_status(void) {
  int status;
  cyw43_arch_lwip_begin();
  status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  cyw43_arch_lwip_end();
  return status;
}

/**
 * @brief Return true if the link is up
 */
static inline bool _hw_wifi_up(hw_wifi_t *wifi) {
  (void)wifi;
  return _hw_wifi_link_status() == CYW43_LINK_UP;
}

/**
 * @brief Return true if busy (leaving, joining, scanning)
 */
static inline bool _hw_wifi_get_busy(hw_wifi_t *wifi, hw_wifi_state_t state) {
  return (sys_atomic_get(&wifi->flags) & state) != 0;
}

/**
 * @brief Set busy state
 */
static inline void _hw_wifi_set_busy(hw_wifi_t *wifi, hw_wifi_state_t state,
                                     bool busy) {
  if (busy) {
    sys_atomic_set_bits(&wifi->flags, state);
  } else {
    sys_atomic_clear_bits(&wifi->flags, state);
  }
}

/**
 * @brief Return country code as uint32_t from string
 */
static uint32_t _hw_wifi_country_code(const char *country_code);

/**
 * @brief Forward declare the scanning callback
 */
static int _hw_wifi_scan_callback(void *env,
                                  const cyw43_ev_scan_result_t *result);

#endif // PICO_CYW43_SUPPORTED

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_wifi_t *hw_wifi_init_client(const char *country_code,
                               hw_wifi_callback_t callback, void *userdata) {
  sys_debugf("wifi_init_client: country_code=%s callback=%p userdata=%p",
             country_code != NULL ? country_code : "(null)", callback,
             userdata);
#ifdef PICO_CYW43_SUPPORTED
  // Deinitialize if already initialized
  hw_wifi_deinit(&_hw_wifi_instance);

  // Country code check
  if (country_code == NULL) {
    country_code = _hw_wifi_default_country_code;
  }
  if (sys_strlen(country_code) != 2u) {
    return NULL;
  }
  if (cyw43_is_initialized(&cyw43_state) == false) {
    return NULL;
  }

  // Set up the structure
  sys_memcpy(_hw_wifi_instance.country_code, country_code, 2);
  _hw_wifi_instance.country_code[2] = '\0';
  _hw_wifi_instance.callback = callback;
  _hw_wifi_instance.userdata = userdata;
  sys_atomic_init(&_hw_wifi_instance.flags, 0);

  // Return the Wi-Fi handle
  return &_hw_wifi_instance;
#else
  (void)country_code;
  (void)callback;
  (void)userdata;
  return NULL; // No-op stub implementation for unsupported platforms.
#endif
}

/** @brief Stub function in Pico SDK */
hw_wifi_t *hw_wifi_init_device(const char *device, hw_wifi_callback_t callback,
                               void *user_data) {
  sys_debugf("wifi_init_device: device=%s callback=%p userdata=%p",
             device != NULL ? device : "(null)", callback, user_data);
  (void)device;
  (void)callback;
  (void)user_data;
  return NULL; // No-op stub implementation for unsupported platforms.
}

bool hw_wifi_valid(hw_wifi_t *wifi) {
  return wifi != NULL && wifi->country_code[0] != '\0';
}

void hw_wifi_deinit(hw_wifi_t *wifi) {
  sys_debugf("wifi_deinit: wifi=%p", wifi);
  if (!hw_wifi_valid(wifi)) {
    return;
  }

#ifdef PICO_CYW43_SUPPORTED
  if (cyw43_is_initialized(&cyw43_state)) {
    // Stop any in-flight connect/scan activity and disconnect from STA.
    cyw43_arch_lwip_begin();
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, false,
                      _hw_wifi_country_code(wifi->country_code));
    cyw43_arch_lwip_end();
  }

  _hw_wifi_set_busy(
      wifi, hw_wifi_flag_leaving | hw_wifi_flag_joining | hw_wifi_flag_scanning,
      false);
  wifi->state = -1;
  wifi->ts = 0;
#endif

  sys_memset(wifi, 0, sizeof(struct hw_wifi_t));
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Begin an asynchronous scan for nearby Wi‑Fi networks.
 */
bool hw_wifi_scan(hw_wifi_t *wifi) {
  bool success = false;

#ifdef PICO_CYW43_SUPPORTED
  if (hw_wifi_valid(wifi) == false) {
    return false;
  }

  // If we're already leaving, joining or scanning, don't init a new scan
  if (_hw_wifi_get_busy(wifi, hw_wifi_flag_leaving | hw_wifi_flag_joining |
                                  hw_wifi_flag_scanning)) {
    return false;
  }

  if (_hw_wifi_up(wifi) == false) {
    // Bring Wi‑Fi up in STA (client)mode
    cyw43_arch_lwip_begin();
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true,
                      _hw_wifi_country_code(wifi->country_code));
    cyw43_arch_lwip_end();
  }

  // TODO: set power management cyw43_wifi_pm

  // Pass the wifi handle as the callback environment
  cyw43_wifi_scan_options_t opts = {0};
  cyw43_arch_lwip_begin();
  int scan_result =
      cyw43_wifi_scan(&cyw43_state, &opts, wifi, _hw_wifi_scan_callback);
  cyw43_arch_lwip_end();
  if (scan_result == 0) {
    _hw_wifi_set_busy(wifi, hw_wifi_flag_scanning, true);
    wifi->state = -1;
    success = true;
  }
#else
  (void)wifi;
#endif

  return success;
}

/**
 * @brief Begin an asynchronous connection to a Wi‑Fi network.
 */
bool hw_wifi_connect(hw_wifi_t *wifi, const hw_wifi_network_t *network,
                     const char *password) {
  bool success = false;

#ifdef PICO_CYW43_SUPPORTED
  if (hw_wifi_valid(wifi) == false || network == NULL) {
    return false;
  }

  // If we're already leaving, joining or scanning, don't start a new join.
  if (_hw_wifi_get_busy(wifi, hw_wifi_flag_leaving | hw_wifi_flag_joining |
                                  hw_wifi_flag_scanning)) {
    return false;
  }

  size_t ssid_len = sys_strlen(network->ssid);
  if (ssid_len == 0 || ssid_len > HW_WIFI_SSID_MAX_LENGTH) {
    return false;
  }

  const char *key = password != NULL ? password : "";
  size_t key_len = sys_strlen(key);

  uint32_t auth = CYW43_AUTH_OPEN;
  if ((network->auth & hw_wifi_auth_wpa3_sae) != 0) {
#if defined(CYW43_AUTH_WPA3_SAE_AES_PSK)
    auth = CYW43_AUTH_WPA3_SAE_AES_PSK;
#else
    auth = CYW43_AUTH_WPA2_AES_PSK;
#endif
  } else if ((network->auth & (hw_wifi_auth_wpa2_aes | hw_wifi_auth_wpa2_tkip |
                               hw_wifi_auth_wpa_aes)) != 0) {
    auth = CYW43_AUTH_WPA2_AES_PSK;
  } else if ((network->auth & hw_wifi_auth_wpa_tkip) != 0) {
    auth = CYW43_AUTH_WPA_TKIP_PSK;
  }

  if (auth != CYW43_AUTH_OPEN && key_len == 0) {
    return false;
  }

  if (_hw_wifi_up(wifi) == false) {
    cyw43_arch_lwip_begin();
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true,
                      _hw_wifi_country_code(wifi->country_code));
    cyw43_arch_lwip_end();
  }

  // Reset prior connection state and store the requested network.
  sys_memset(&wifi->network, 0, sizeof(wifi->network));
  sys_memcpy(&wifi->network, network, sizeof(wifi->network));
  wifi->state = -1;
  wifi->ts = 0;

  cyw43_arch_lwip_begin();
  int join_result = cyw43_wifi_join(
      &cyw43_state, ssid_len, (const uint8_t *)wifi->network.ssid, key_len,
      (const uint8_t *)key, auth, NULL, 0);
  cyw43_arch_lwip_end();

  if (join_result == 0) {
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, true);
    success = true;
  } else {
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
  }
#else
  (void)wifi;
  (void)network;
  (void)password;
#endif

  return success;
}

/**
 * @brief Disconnect from a previously-connected Wi‑Fi network.
 */
bool hw_wifi_disconnect(hw_wifi_t *wifi) {
#ifdef PICO_CYW43_SUPPORTED
  if (hw_wifi_valid(wifi) == false ||
      cyw43_is_initialized(&cyw43_state) == false) {
    return false;
  }

  // If scanning or joining is in progress, abort and report not connected.
  if (_hw_wifi_get_busy(wifi, hw_wifi_flag_scanning | hw_wifi_flag_joining)) {
    cyw43_arch_lwip_begin();
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_arch_lwip_end();
    _hw_wifi_set_busy(wifi, hw_wifi_flag_scanning | hw_wifi_flag_joining,
                      false);
    wifi->state = -1;
    wifi->ts = 0;
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    return false;
  }

  int state = _hw_wifi_link_status();
  if (state != CYW43_LINK_JOIN && state != CYW43_LINK_NOIP &&
      state != CYW43_LINK_UP) {
    return false;
  }

  cyw43_arch_lwip_begin();
  int leave_result = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
  cyw43_arch_lwip_end();
  if (leave_result != 0) {
    return false;
  }

  _hw_wifi_set_busy(wifi, hw_wifi_flag_leaving, true);
  wifi->state = -1;
  wifi->ts = 0;
  return true;
#else
  (void)wifi;
  return false;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

#ifdef PICO_CYW43_SUPPORTED

/**
 * @brief Get country code as uint32_t for SDK from Wi-Fi handle
 */
static uint32_t _hw_wifi_country_code(const char *country_code) {
  if (country_code == NULL || sys_strlen(country_code) != 2u) {
    return 0;
  } else {
    return CYW43_COUNTRY(country_code[0], country_code[1], 0);
  }
}

/**
 * @brief Get current Wi-Fi channel for connected Wi-Fi
 */
static uint8_t _hw_wifi_get_channel(hw_wifi_t *wifi) {
  sys_assert(hw_wifi_valid(wifi));
  uint32_t channel = 0;
  cyw43_arch_lwip_begin();
  cyw43_ioctl(&cyw43_state, CYW43_IOCTL_GET_CHANNEL, sizeof(channel),
              (uint8_t *)&channel, CYW43_ITF_STA);
  cyw43_arch_lwip_end();
  return (uint8_t)channel;
}

/**
 * @brief Get bssid for connected Wi-Fi
 */
static void _hw_wifi_get_bssid(hw_wifi_t *wifi, uint8_t bssid[6]) {
  sys_assert(hw_wifi_valid(wifi));
  sys_assert(bssid != NULL);
  sys_memset(bssid, 0, 6);
  cyw43_arch_lwip_begin();
  cyw43_wifi_get_bssid(&cyw43_state, bssid);
  cyw43_arch_lwip_end();
}

/**
 * @brief Get signal strength for connected Wi-Fi
 */
static int16_t _hw_wifi_get_rssi(hw_wifi_t *wifi) {
  sys_assert(hw_wifi_valid(wifi));
  int32_t rssi = 0;
  cyw43_arch_lwip_begin();
  int rssi_result = cyw43_wifi_get_rssi(&cyw43_state, &rssi);
  cyw43_arch_lwip_end();
  if (rssi_result == 0) {
    return (int16_t)rssi;
  }
  return 0;
}

/**
 * @brief Callback for scan results from the CYW43 driver.
 */
static int _hw_wifi_scan_callback(void *ctx,
                                  const cyw43_ev_scan_result_t *result) {
  static hw_wifi_network_t network = {0};
  hw_wifi_t *wifi = (hw_wifi_t *)ctx;
  sys_assert(hw_wifi_valid(wifi));

  // Stop scanning if callback is NULL
  if (wifi->callback == NULL) {
    return -1;
  }

  // Driver may invoke with result == NULL to indicate completion; we ignore
  // here and rely on poll to notify completion.
  if (result == NULL) {
    return 0;
  }

  // Copy hw_wifi_network_t ssid (clamped)
  size_t ssid_len = result->ssid_len;
  if (ssid_len >= sizeof(network.ssid)) {
    ssid_len = sizeof(network.ssid) - 1;
  }
  sys_memcpy(network.ssid, result->ssid, ssid_len);
  network.ssid[ssid_len] = '\0';

  // Copy hw_wifi_network_t bssid
  sys_assert(sizeof(network.bssid) == sizeof(result->bssid));
  sys_memcpy(network.bssid, result->bssid, sizeof(network.bssid));

  // Copy hw_wifi_network_t channel and rssi
  network.channel = result->channel;
  network.rssi = (int16_t)result->rssi;

  // Map auth: compare using 8-bit value, guard WPA3 macro size
  network.auth = 0;
  uint8_t am = result->auth_mode;
  if (am == (uint8_t)CYW43_AUTH_OPEN) {
    network.auth = hw_wifi_auth_open;
  } else if (am == (uint8_t)CYW43_AUTH_WPA_TKIP_PSK) {
    network.auth = hw_wifi_auth_wpa_tkip;
  } else if (am == (uint8_t)CYW43_AUTH_WPA2_AES_PSK ||
             am == (uint8_t)CYW43_AUTH_WPA2_MIXED_PSK) {
    network.auth = hw_wifi_auth_wpa2_aes;
  }
#if defined(CYW43_AUTH_WPA3_SAE_AES_PSK)
  else if (am == (uint8_t)CYW43_AUTH_WPA3_SAE_AES_PSK) {
    network.auth = hw_wifi_auth_wpa3_sae;
  }
#elif defined(CYW43_AUTH_WPA3_SAE_PSK)
  else if (am == (uint8_t)CYW43_AUTH_WPA3_SAE_PSK) {
    network.auth = hw_wifi_auth_wpa3_sae;
  }
#endif

  // Callback
  wifi->callback(wifi, hw_wifi_event_scan, &network, wifi->userdata);

  // Continue scanning
  return 0;
}

/**
 * @brief Poll the Wi-Fi state and handle events.
 */
void _hw_wifi_poll(void) {
  // Only act on our singleton
  hw_wifi_t *wifi = &_hw_wifi_instance;
  if (!hw_wifi_valid(wifi)) {
    return;
  }

  // If the timestamp field is > 0 then report occasionally on
  // connection status
  if (wifi->ts > 0) {
    uint64_t now = sys_timestamp_ms();
    if (now - wifi->ts > _hw_wifi_status_interval_ms) {
      wifi->ts = now;
      if (_hw_wifi_up(wifi)) {
        // Update the network
        wifi->network.rssi = _hw_wifi_get_rssi(wifi);
        _hw_wifi_get_bssid(wifi, wifi->network.bssid);
        wifi->network.channel = _hw_wifi_get_channel(wifi);

        // Callback
        wifi->callback(wifi, hw_wifi_event_connected, &wifi->network,
                       wifi->userdata);
      }
    }
  }

  // If we're not joining, leaving or scanning, then quit this
  if (_hw_wifi_get_busy(wifi, hw_wifi_flag_leaving | hw_wifi_flag_joining |
                                  hw_wifi_flag_scanning) == 0) {
    return;
  }

  // If we're scanning and scan becomes inactive then end the scanning
  if (_hw_wifi_get_busy(wifi, hw_wifi_flag_scanning)) {
    bool scan_active;
    cyw43_arch_lwip_begin();
    scan_active = cyw43_wifi_scan_active(&cyw43_state);
    cyw43_arch_lwip_end();
    if (scan_active == false) {
      _hw_wifi_set_busy(wifi, hw_wifi_flag_scanning, false);
      wifi->callback(wifi, hw_wifi_event_scan, NULL,
                     wifi->userdata); // Notify scan completion
    }
    return;
  }

  // Get current link state, and act if it's changed
  int state = _hw_wifi_link_status();
  if (state == wifi->state) {
    return;
  } else {
    wifi->state = state;
  }

  // Change state
  switch (state) {
  case CYW43_LINK_DOWN:
    sys_debugf("CYW43_LINK_DOWN");
    // Ends the connection, joining or scanning attempt
    _hw_wifi_set_busy(wifi,
                      hw_wifi_flag_joining | hw_wifi_flag_leaving |
                          hw_wifi_flag_scanning,
                      false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_disconnected, NULL, wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  case CYW43_LINK_JOIN:
    sys_debugf("CYW43_LINK_JOIN");
    // Starts a connection attempt
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, true);
    wifi->callback(wifi, hw_wifi_event_joining, &wifi->network, wifi->userdata);
    break;
  case CYW43_LINK_NOIP:
    // Continues connection attempt
    sys_debugf("CYW43_LINK_NOIP");
    break;
  case CYW43_LINK_UP:
    // Ends the connection attempt successfully
    sys_debugf("CYW43_LINK_UP");
    if (_hw_wifi_get_busy(wifi, hw_wifi_flag_joining)) {
      _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
      wifi->state = -1;

      // Update the network
      wifi->network.rssi = _hw_wifi_get_rssi(wifi);
      _hw_wifi_get_bssid(wifi, wifi->network.bssid);
      wifi->network.channel = _hw_wifi_get_channel(wifi);

      // Set the timestamp to update the network values
      wifi->ts = sys_timestamp_ms();

      // Callback
      wifi->callback(wifi, hw_wifi_event_connected, &wifi->network,
                     wifi->userdata);
    }
    break;
  case CYW43_LINK_FAIL:
    // Ends the connection attempt unsuccessfully
    sys_debugf("CYW43_LINK_FAIL");
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_error, &wifi->network, wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  case CYW43_LINK_NONET:
    // Ends the connection attempt unsuccessfully
    sys_debugf("CYW43_LINK_NONET");
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_notfound, &wifi->network,
                   wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  case CYW43_LINK_BADAUTH:
    // Ends the connection attempt unsuccessfully
    sys_debugf("CYW43_LINK_BADAUTH");
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_badauth, &wifi->network, wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  default:
    // Ends the connection, joining or scanning attempt
    sys_debugf("CYW43_LINK_UNKNOWN");
    _hw_wifi_set_busy(wifi,
                      hw_wifi_flag_joining | hw_wifi_flag_leaving |
                          hw_wifi_flag_scanning,
                      false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_error, &wifi->network, wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  }
}
#endif // PICO_CYW43_SUPPORTED

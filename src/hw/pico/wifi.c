#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <string.h>

#ifdef PICO_CYW43_SUPPORTED
#include "cyw43.h"
#include "cyw43_country.h"
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

static struct hw_wifi_t _hw_wifi_instance = {0};
static const char _hw_wifi_default_country_code[] = "XX";
static uint32_t _hw_wifi_status_interval_ms = (1000 * 60);

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

#ifdef PICO_CYW43_SUPPORTED
/**
 * @brief Return true if the link is up
 */
static inline bool _hw_wifi_up(hw_wifi_t *wifi) {
  (void)wifi;
  return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
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

  // Deinitialize if already initialized
  hw_wifi_deinit(&_hw_wifi_instance);

#ifdef PICO_CYW43_SUPPORTED
  // Country code check
  if (country_code == NULL) {
    country_code = _hw_wifi_default_country_code;
  }
  if (sys_strlen(country_code) != 2u) {
    return NULL;
  }

  if (cyw43_is_initialized(&cyw43_state) == false) {
    sys_printf("cyw43_arch_init failed\n");
    return NULL;
  }

  // Set up the structure
  sys_memcpy(_hw_wifi_instance.country_code, country_code, 2);
  _hw_wifi_instance.country_code[2] = '\0';
  _hw_wifi_instance.callback = callback;
  _hw_wifi_instance.userdata = userdata;
  sys_atomic_init(&_hw_wifi_instance.flags, 0);
#endif

  // Return the Wi-Fi handle
  return &_hw_wifi_instance;
}

bool hw_wifi_valid(hw_wifi_t *wifi) {
  return wifi != NULL && wifi->country_code[0] != '\0';
}

void hw_wifi_deinit(hw_wifi_t *wifi) {
  if (!hw_wifi_valid(wifi)) {
    return;
  }

  sys_memset(wifi, 0, sizeof(struct hw_wifi_t));
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Begin an asynchronous scan for nearby Wi‑Fi networks.
 */
bool hw_wifi_scan(hw_wifi_t *wifi) {
  bool success = false;
  if (hw_wifi_valid(wifi) == false) {
    return false;
  }

#ifdef PICO_CYW43_SUPPORTED
  // If we're already leaving, joining or scanning, don't init a new scan
  if (_hw_wifi_get_busy(wifi, hw_wifi_flag_leaving | hw_wifi_flag_joining |
                                  hw_wifi_flag_scanning)) {
    return false;
  }

  if (_hw_wifi_up(wifi) == false) {
    // Bring Wi‑Fi up in STA (client)mode
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true,
                      _hw_wifi_country_code(wifi->country_code));
  }

  // TODO: set power management cyw43_wifi_pm

  // Zero-initialize scan options
  cyw43_wifi_scan_options_t opts = {0};

  // Pass the wifi handle as the callback environment
  if (cyw43_wifi_scan(&cyw43_state, &opts, wifi, _hw_wifi_scan_callback) == 0) {
    _hw_wifi_set_busy(wifi, hw_wifi_flag_scanning, true);
    wifi->state = -1;
    success = true;
  }
#endif

  return success;
}

/**
 * @brief Begin an asynchronous connection to a Wi‑Fi network.
 */
bool hw_wifi_connect(hw_wifi_t *wifi, const hw_wifi_network_t *network,
                     const char *password);

/**
 * @brief Disconnect from a previously-connected Wi‑Fi network.
 */
bool hw_wifi_disconnect(hw_wifi_t *wifi);

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
  cyw43_ioctl(&cyw43_state, CYW43_IOCTL_GET_CHANNEL, sizeof(channel),
              (uint8_t *)&channel, CYW43_ITF_STA);
  return (uint8_t)channel;
}

/**
 * @brief Get bssid for connected Wi-Fi
 */
static void _hw_wifi_get_bssid(hw_wifi_t *wifi, uint8_t bssid[6]) {
  sys_assert(hw_wifi_valid(wifi));
  sys_assert(bssid != NULL);
  sys_memset(bssid, 0, 6);
  cyw43_wifi_get_bssid(&cyw43_state, bssid);
}

/**
 * @brief Get signal strength for connected Wi-Fi
 */
static int16_t _hw_wifi_get_rssi(hw_wifi_t *wifi) {
  sys_assert(hw_wifi_valid(wifi));
  int32_t rssi = 0;
  if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0) {
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
    if (cyw43_wifi_scan_active(&cyw43_state) == false) {
      _hw_wifi_set_busy(wifi, hw_wifi_flag_scanning, false);
      wifi->callback(wifi, hw_wifi_event_scan, NULL,
                     wifi->userdata); // Notify scan completion
    }
    return;
  }

  // Get current link state, and act if it's changed
  int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  if (state == wifi->state) {
    return;
  } else {
    wifi->state = state;
  }

  // Change state
  switch (state) {
  case CYW43_LINK_DOWN:
#ifndef NDEBUG
    sys_printf("CYW43_LINK_DOWN\n");
#endif
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
#ifndef NDEBUG
    sys_printf("CYW43_LINK_JOIN\n");
#endif
    // Starts a connection attempt
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, true);
    wifi->callback(wifi, hw_wifi_event_joining, &wifi->network, wifi->userdata);
    break;
  case CYW43_LINK_NOIP:
    // Continues connection attempt
#ifndef NDEBUG
    sys_printf("CYW43_LINK_NOIP\n");
#endif
    break;
  case CYW43_LINK_UP:
    // Ends the connection attempt successfully
#ifndef NDEBUG
    sys_printf("CYW43_LINK_UP\n");
#endif
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
#ifndef NDEBUG
    sys_printf("CYW43_LINK_FAIL\n");
#endif
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_error, &wifi->network, wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  case CYW43_LINK_NONET:
    // Ends the connection attempt unsuccessfully
#ifndef NDEBUG
    sys_printf("CYW43_LINK_NONET\n");
#endif
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_notfound, &wifi->network,
                   wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  case CYW43_LINK_BADAUTH:
    // Ends the connection attempt unsuccessfully
#ifndef NDEBUG
    sys_printf("CYW43_LINK_BADAUTH\n");
#endif
    _hw_wifi_set_busy(wifi, hw_wifi_flag_joining, false);
    wifi->state = -1;
    wifi->callback(wifi, hw_wifi_event_badauth, &wifi->network, wifi->userdata);
    sys_memset(&wifi->network, 0, sizeof(wifi->network));
    break;
  default:
    // Ends the connection, joining or scanning attempt
#ifndef NDEBUG
    sys_printf("CYW43_LINK_UNKNOWN\n");
#endif
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

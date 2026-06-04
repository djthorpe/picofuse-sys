#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Handle for Wi-Fi state.
 */
struct hw_wifi_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_wifi_t *hw_wifi_init_client(const char *country_code,
                               hw_wifi_callback_t callback, void *user_data) {
  sys_debugf(
      "wifi_init_client: country_code=%s callback=%p userdata=%p unsupported "
      "on this platform",
      country_code != NULL ? country_code : "(null)", (void *)callback,
      user_data);
  (void)country_code;
  (void)callback;
  (void)user_data;
  return NULL; // No-op stub implementation for unsupported platforms.
}

hw_wifi_t *hw_wifi_init_device(const char *device, hw_wifi_callback_t callback,
                               void *user_data) {
  sys_debugf("wifi_init_device: device=%s callback=%p userdata=%p unsupported "
             "on this platform",
             device != NULL ? device : "(null)", (void *)callback, user_data);
  (void)device;
  (void)callback;
  (void)user_data;
  return NULL; // No-op stub implementation for unsupported platforms.
}

bool hw_wifi_valid(hw_wifi_t *wifi) {
  (void)wifi;
  return false;
}

void hw_wifi_deinit(hw_wifi_t *wifi) {
  sys_debugf("wifi_deinit: wifi=%p unsupported on this platform", wifi);
  (void)wifi;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Begin an asynchronous scan for nearby Wi‑Fi networks.
 */
bool hw_wifi_scan(hw_wifi_t *wifi) {
  (void)wifi;
  return false; // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Begin an asynchronous connection to a Wi‑Fi network.
 */
bool hw_wifi_connect(hw_wifi_t *wifi, const hw_wifi_network_t *network,
                     const char *password) {
  (void)wifi;
  (void)network;
  (void)password;
  return false; // No-op stub implementation for unsupported platforms.
}

/**
 * @brief Disconnect from a previously-connected Wi‑Fi network.
 */
bool hw_wifi_disconnect(hw_wifi_t *wifi) {
  (void)wifi;
  return false;
}

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#ifndef WIFI_SSID
#error "WIFI_SSID not defined"
#endif

#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD not defined"
#endif

void wifi_callback(hw_wifi_t *wifi, hw_wifi_event_t event,
                   const hw_wifi_network_t *network, void *user_data) {
  (void)user_data;
  (void)wifi;

  if (event & hw_wifi_event_scan) {
    if (network) {
      sys_printf("Scan result\n");
    }
    if (network == NULL) {
      sys_printf("End of scan\n");
    }
  }
  if (event & hw_wifi_event_joining) {
    sys_printf("Joining a network\n");
  }
  if (event & hw_wifi_event_connected) {
    sys_printf("Connected to a network\n");
  }
  if (event & hw_wifi_event_disconnected) {
    sys_printf("Disconnected from a network\n");
  }
  if (event & hw_wifi_event_badauth) {
    sys_printf("Failed to connect: Bad Auth\n");
  }
  if (event & hw_wifi_event_notfound) {
    sys_printf("Failed to connect: Network Not Found\n");
  }
  if (event & hw_wifi_event_error) {
    sys_printf("Failed to connect: Error\n");
  }

  if (network) {
    sys_printf("AP: %02X:%02X:%02X:%02X:%02X:%02X SSID='%s' CH=%u RSSI=%d "
               "AUTH=0x%02X\n",
               network->bssid[0], network->bssid[1], network->bssid[2],
               network->bssid[3], network->bssid[4], network->bssid[5],
               network->ssid, (unsigned)network->channel, (int)network->rssi,
               (unsigned)network->auth);
  }
}

int main(void) {
  sys_init();
  hw_init();

  // Initialize Wi-Fi (use default country)
  hw_wifi_t *wifi = hw_wifi_init_client(NULL, wifi_callback, NULL);
  if (hw_wifi_valid(wifi) == false) {
    sys_printf("WiFi not available\n");
    goto done;
  }

  // Start connection
  hw_wifi_network_t network = {
      .ssid = WIFI_SSID,
      .auth = hw_wifi_auth_wpa2_aes,
  };

  if (WIFI_SSID[0] == '\0') {
    sys_printf("WIFI_SSID is empty\n");
    goto done;
  }

  sys_printf("Connecting WiFi\n");
  if (hw_wifi_connect(wifi, &network, WIFI_PASSWORD) == false) {
    sys_printf("Failed to start WiFi connection\n");
  }

  // Poll for a short while to let connection happen
  for (int i = 0; i < 5000; ++i) {
    hw_poll();
    sys_sleep_ms(10);
  }

  sys_printf("Disconnecting WiFi\n");
  if (hw_wifi_disconnect(wifi) == false) {
    sys_printf("Failed to disconnect WiFi\n");
  }

  // Poll for a short while to let disconnection happen
  for (int i = 0; i < 1000; ++i) {
    hw_poll();
    sys_sleep_ms(10);
  }

done:
  hw_wifi_deinit(wifi);
  hw_exit();
  sys_exit();
  return 0;
}

#include <picofuse/hw.h>
#include <picofuse/sys.h>

void wifi_callback(hw_wifi_t *wifi, hw_wifi_event_t event,
                   const hw_wifi_network_t *network, void *user_data) {
  if (event & hw_wifi_event_scan) {
    if (network == NULL) {
      sys_printf("Scan complete\n");
    } else {
      sys_printf("Scan result: %s (RSSI: %d, channel: %d)\n", network->ssid,
                 network->rssi, network->channel);
    }
  }
  if (event & hw_wifi_event_joining) {
    if (network->ssid != NULL) {
      sys_printf("Joining network: %s\n", network->ssid);
    }
    return;
  }
  if (event & hw_wifi_event_connected) {
    sys_printf("Connected to network: %s\n", network->ssid);
    return;
  }
  if (event & hw_wifi_event_disconnected) {
    sys_printf("Disconnected from network\n");
    return;
  }
  if (event & hw_wifi_event_badauth) {
    sys_printf("Bad authentication for network: %s\n", network->ssid);
    return;
  }
  if (event & hw_wifi_event_notfound) {
    sys_printf("Network not found: %s\n", network->ssid);
    return;
  }
  if (event & hw_wifi_event_error) {
    sys_printf("Wi-Fi error occurred\n");
    return;
  }
}

int main(void) {
  sys_init();
  hw_init();

  // Initialize Wi-Fi client with default country code and no callback
  hw_wifi_t *wifi = hw_wifi_init_client(NULL, wifi_callback, NULL);
  if (hw_wifi_valid(wifi) == false) {
    sys_printf("Wi-Fi init failed\n");
    sys_halt();
  }

  // Scan for Wi-Fi networks
  if (hw_wifi_scan(wifi) == false) {
    sys_printf("Wi-Fi scan failed\n");
    sys_halt();
  }

  // Main loop will be handled by _hw_wifi_poll called from hw_poll()
  uint64_t start_time = sys_timestamp_ms();
  while (sys_timestamp_ms() - start_time < 30 * 1000) {
    hw_poll();
    sys_sleep_ms(100);
  }

  sys_printf("Shutting down\n");
  hw_exit();
  sys_exit();
}

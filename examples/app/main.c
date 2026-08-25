#include <picofuse/app.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <string.h>

#define HELLO_TIMER_ID 0x48454C4Fu
#define HELLO_TIMER_INTERVAL_MS 5000u

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

void app_init(app_t *app, void *userdata) {
  (void)userdata;
  sys_debugf("app", "app_init: called on core %u of %u", sys_thread_core(),
             sys_thread_numcores());

  hid_t *hid = app_hid(app);
  if (hid != NULL) {
    hid_register_timer(hid, HELLO_TIMER_ID, HELLO_TIMER_INTERVAL_MS, true,
                       NULL);
  }

  // Join Wi-Fi, if APP_FLAG_WIFI registered successfully and credentials
  // were supplied at build time (export WIFI_SSID/WIFI_PASSWORD before
  // running cmake). hid_register_wifi() only observes; connecting is
  // still done directly against the raw hw_wifi_t* handle from app_wifi().
  hw_wifi_t *wifi = app_wifi(app);
  if (wifi != NULL && WIFI_SSID[0] != '\0') {
    hw_wifi_network_t network = {
        .ssid = WIFI_SSID,
        .auth = hw_wifi_auth_wpa2_aes,
    };
    sys_debugf("app", "app_init: joining wifi ssid=%s", WIFI_SSID);
    if (!hw_wifi_connect(wifi, &network, WIFI_PASSWORD)) {
      sys_debugf("app", "app_init: failed to start wifi connection");
    }
  }

  // Watchdog
  hw_watchdog_t *watchdog = app_watchdog(app);
  if (watchdog == NULL) {
    sys_debugf("app", "app_init: watchdog is not enabled");
  } else if (hw_watchdog_did_reset(watchdog)) {
    sys_debugf("app", "app_init: watchdog reset the application");
  } else {
    sys_debugf("app", "app_init: watchdog enabled");
  }
}

void app_event(app_t *app, sys_event_t event, void *userdata) {
  (void)app;
  (void)userdata;

  hid_event_t *hid_event = (hid_event_t *)event;
  if (hid_event == NULL) {
    return;
  }

  switch (hid_event->type) {
  case hid_event_type_timer:
    sys_debugf("app", "app_event: hello from the timer (t=%lu ms core=%u)",
               sys_timestamp_ms(), sys_thread_core());
    break;
  case hid_event_type_keycode:
    sys_debugf("app", "app_event: %s %s (core=%u)",
               hid_keycode_to_string(hid_event->data.keycode.keycode),
               (hid_event->data.keycode.state & hid_state_on) ? "pressed"
                                                              : "released",
               sys_thread_core());
    break;
  case hid_event_type_metric:
    if (strcmp(hid_event->data.metric.name, "temp") == 0) {
      sys_debugf("app", "app_event: %s=%.1f %s (core=%u)",
                 hid_event->data.metric.name,
                 (double)hid_event->data.metric.value,
                 hid_event->data.metric.unit, sys_thread_core());
    } else {
      sys_debugf("app", "app_event: %s=%.2f %s (core=%u)",
                 hid_event->data.metric.name,
                 (double)hid_event->data.metric.value,
                 hid_event->data.metric.unit, sys_thread_core());
    }
    break;
  case hid_event_type_signal:
    sys_env_signal_t signal = hid_event->data.signal.signal;

    switch (signal) {
    case SYS_ENV_SIGNAL_TERM:
      sys_debugf("app", "app_event: received termination signal (core=%u)",
                 sys_thread_core());
      app_shutdown(0);
      break;
    case SYS_ENV_SIGNAL_INT:
      sys_debugf("app", "app_event: received interrupt signal (core=%u)",
                 sys_thread_core());
      app_shutdown(0);
      break;
    case SYS_ENV_SIGNAL_QUIT:
      sys_debugf("app", "app_event: received quit signal (core=%u)",
                 sys_thread_core());
      app_shutdown(0);
      break;
    default:
      sys_debugf("app", "app_event: received unknown signal 0x%02X (core=%u)",
                 (unsigned int)signal, sys_thread_core());
      break;
    }
    break;
  case hid_event_type_wifi: {
    hw_wifi_event_t wifi_event = hid_event->data.wifi.event;
    const hw_wifi_network_t *network = hid_event->data.wifi.network;
    const char *label = "unknown";

    if (wifi_event & hw_wifi_event_scan) {
      label = (network != NULL) ? "scan result" : "scan complete";
    } else if (wifi_event & hw_wifi_event_joining) {
      label = "joining";
    } else if (wifi_event & hw_wifi_event_connected) {
      label = "connected";
    } else if (wifi_event & hw_wifi_event_disconnected) {
      label = "disconnected";
    } else if (wifi_event & hw_wifi_event_badauth) {
      label = "bad auth";
    } else if (wifi_event & hw_wifi_event_notfound) {
      label = "not found";
    } else if (wifi_event & hw_wifi_event_error) {
      label = "error";
    }

    if (network != NULL) {
      sys_debugf("app", "app_event: wifi %s ssid=%s rssi=%d (core=%u)", label,
                 network->ssid, (int)network->rssi, sys_thread_core());
    } else {
      sys_debugf("app", "app_event: wifi %s (core=%u)", label,
                 sys_thread_core());
    }
    break;
  }
  default:
    sys_debugf("app", "app_event: received unknown event type %d (core=%u)",
               (int)hid_event->type, sys_thread_core());
    break;
  }

  hid_event_free(hid_event);
}

int main(int argc, char **argv) {
  return app_main(argc, argv,
                  APP_FLAG_SIGNAL | APP_FLAG_WATCHDOG | APP_FLAG_MULTICORE |
                      APP_FLAG_USER_BUTTON | APP_FLAG_TEMPERATURE |
                      APP_FLAG_VSYS | APP_FLAG_USB | APP_FLAG_WIFI,
                  app_init, app_event, NULL);
}

#include <picofuse/app.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define HELLO_TIMER_ID 0x48454C4Fu
#define HELLO_TIMER_INTERVAL_MS 1000u

void app_init(app_t *app, void *userdata) {
  (void)userdata;
  sys_debugf("[app] app_init: called on core %u", sys_thread_core());

  hid_t *hid = app_hid(app);
  if (hid != NULL) {
    hid_register_timer(hid, HELLO_TIMER_ID, HELLO_TIMER_INTERVAL_MS, true,
                       NULL);
  }

  hw_watchdog_t *watchdog = app_watchdog(app);
  if (watchdog == NULL) {
    sys_debugf("[app] app_init: watchdog is not enabled");
  } else if (hw_watchdog_did_reset(watchdog)) {
    sys_debugf("[app] app_init: watchdog reset the application");
  } else {
    sys_debugf("[app] app_init: watchdog enabled");
  }
}

void app_event(app_t *app, sys_event_t event, void *userdata) {
  (void)app;
  (void)userdata;

  hid_event_t *hid_event = (hid_event_t *)event;
  if (hid_event == NULL) {
    return;
  }

  if (hid_event->type == hid_event_type_timer) {
    sys_debugf("[app] app_event: hello from the timer (t=%lu ms core=%u)",
               sys_timestamp_ms(), sys_thread_core());
  }

  if (hid_event->type == hid_event_type_keycode &&
      hid_event->data.keycode.keycode == KEYCODE_BUTTON_USER) {
    sys_debugf("[app] app_event: user button %s",
               (hid_event->data.keycode.state & hid_state_on) ? "pressed"
                                                               : "released");
  }

  if (hid_event->type == hid_event_type_signal) {
    sys_env_signal_t signal = hid_event->data.signal.signal;

    switch (signal) {
    case SYS_ENV_SIGNAL_TERM:
      sys_debugf("[app] app_event: received termination signal");
      app_shutdown(0);
      break;
    case SYS_ENV_SIGNAL_INT:
      sys_debugf("[app] app_event: received interrupt signal");
      app_shutdown(0);
      break;
    case SYS_ENV_SIGNAL_QUIT:
      sys_debugf("[app] app_event: received quit signal");
      app_shutdown(0);
      break;
    default:
      sys_debugf("[app] app_event: received unknown signal 0x%02X",
                 (unsigned int)signal);
      break;
    }
  }

  hid_event_free(hid_event);
}

int main(int argc, char **argv) {
  return app_main(argc, argv,
                  APP_FLAG_MULTICORE | APP_FLAG_SIGNAL | APP_FLAG_WATCHDOG |
                      APP_FLAG_USER_BUTTON,
                  app_init, app_event, NULL);
}

#include <picofuse/app.h>
#include <picofuse/sys.h>

void app_init(app_t *app, void *userdata) {
  (void)app;
  (void)userdata;
  sys_debugf("app_init() called on core %u", sys_thread_core());
}

void app_event(app_t *app, sys_event_t event, void *userdata) {
  (void)app;
  (void)userdata;

  hid_event_t *hid_event = (hid_event_t *)event;
  if (hid_event == NULL) {
    return;
  }

  if (hid_event->type == hid_event_type_signal) {
    sys_env_signal_t signal = hid_event->data.signal.signal;
    sys_debugf("app_event() received signal 0x%08X\n", (unsigned int)signal);

    if (signal == SYS_ENV_SIGNAL_TERM || signal == SYS_ENV_SIGNAL_INT ||
        signal == SYS_ENV_SIGNAL_QUIT) {
      app_shutdown(0);
    }
  }

  hid_event_free(hid_event);
}

int main(int argc, char **argv) {
  return app_main(argc, argv, APP_FLAG_MULTICORE | APP_FLAG_SIGNAL, app_init,
                  app_event, NULL);
}

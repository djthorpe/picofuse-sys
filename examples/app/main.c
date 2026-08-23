#include <picofuse/app.h>
#include <picofuse/sys.h>

void app_init(app_t *app, void *userdata) {
  (void)app;
  (void)userdata;
  sys_debugf("app_init() called on core %u\n", sys_thread_core());
}

int main(int argc, char **argv) {
  return app_main(argc, argv, APP_FLAG_MULTICORE | APP_FLAG_SIGNAL, app_init,
                  NULL, NULL);
}

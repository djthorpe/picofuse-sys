#include <picofuse/app.h>
#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

/**
 * @def APP_QUEUE_CAPACITY
 * @brief Maximum number of events retained by an app's event queue.
 */
#ifndef APP_QUEUE_CAPACITY
#define APP_QUEUE_CAPACITY 32
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct app_t {
  sys_event_queue_t *queue;
  hid_t *hid;
  hw_watchdog_t *watchdog;
  app_flag_t flags;
  app_callback_start_t on_start;
  app_callback_event_t on_event;
  void *userdata;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

// sys_runloop is itself a process-wide singleton, so a single static
// instance mirrors that rather than adding lifetime management app_main
// does not need.
static app_t *_app = NULL;

static void _app_on_init(uint8_t worker) {
  if (worker != 0u) {
    return;
  }

  _app->queue = sys_event_queue_init(APP_QUEUE_CAPACITY);
  sys_assert(_app->queue != NULL);

  // hw_init() and hid_init() are weakly linked (see hw.c and hid.c): they
  // are harmless no-ops (hid_init() returning NULL) unless the
  // picofuse-hw / picofuse-hid libraries are also linked into this binary,
  // so a NULL app_hid() is expected, not an error, when that library is
  // absent.
  hw_init();
  _app->hid = hid_init(sys_runloop_queue());

  if (_app->hid != NULL && (_app->flags & APP_FLAG_SIGNAL)) {
    (void)hid_register_signal(_app->hid);
  }

  // hid_register_user_button() returns NULL when the board has no user
  // button, which is expected, not an error.
  if (_app->hid != NULL && (_app->flags & APP_FLAG_USER_BUTTON)) {
    (void)hid_register_user_button(_app->hid, KEYCODE_BUTTON_USER);
  }

  // hid_register_temperature() returns NULL when the platform has no
  // internal temperature sensor, which is expected, not an error.
  if (_app->hid != NULL && (_app->flags & APP_FLAG_TEMPERATURE)) {
    (void)hid_register_temperature(_app->hid, 0u);
  }

  // hw_watchdog_init() is weakly linked (see hw.c), so a NULL
  // app_watchdog() is expected, not an error, when picofuse-hw is absent
  // or the platform has no watchdog backend.
  if (_app->flags & APP_FLAG_WATCHDOG) {
    _app->watchdog = hw_watchdog_init();
    if (_app->watchdog != NULL) {
      hw_watchdog_enable(_app->watchdog, true);
    }
  }

  if (_app->on_start != NULL) {
    _app->on_start(_app, _app->userdata);
  }
}

static void _app_on_event(sys_event_t event) {
  if (_app->on_event != NULL) {
    _app->on_event(_app, event, _app->userdata);
  }
}

static void _app_poll(void) {
  hw_poll();
  if (_app->hid != NULL) {
    (void)hid_poll(_app->hid);
  }
}

static void _app_on_exit(uint8_t worker) {
  if (worker != 0u) {
    return;
  }

  // Disable the watchdog before other teardown steps run, so shutdown work
  // cannot itself be interrupted by a watchdog-triggered reset.
  if (_app->watchdog != NULL) {
    hw_watchdog_deinit(_app->watchdog);
    _app->watchdog = NULL;
  }

  hid_deinit(_app->hid);
  _app->hid = NULL;
  hw_exit();
  sys_event_queue_deinit(_app->queue);
  _app->queue = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

int app_main(int argc, char *argv[], app_flag_t flags,
             app_callback_start_t on_start, app_callback_event_t on_event,
             void *userdata) {
  (void)argc;
  (void)argv;

  app_t app = {
      .queue = NULL,
      .hid = NULL,
      .watchdog = NULL,
      .flags = flags,
      .on_start = on_start,
      .on_event = on_event,
      .userdata = userdata,
  };
  _app = &app;

  sys_init();

  // Run on all cores if APP_FLAG_MULTICORE is set, otherwise run on a
  // single core.
  uint8_t num_workers = (flags & APP_FLAG_MULTICORE) ? 0u : 1u;

  // Run the event loop until app_shutdown() is called, then exit with the
  // provided exit code.
  uint32_t exit_code = sys_runloop_run(num_workers, _app_on_init, _app_on_event,
                                       _app_poll, _app_on_exit);
  sys_exit();
  _app = NULL;
  return (int)exit_code;
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/**
 * @brief Get the HID instance initialized for this app.
 * @return HID instance, or NULL if the picofuse-hid library is not linked
 * into this binary.
 */
hid_t *app_hid(const app_t *app) { return (app != NULL) ? app->hid : NULL; }

/**
 * @brief Get the watchdog handle enabled for this app.
 * @param app Application instance.
 * @return Watchdog handle, or NULL if APP_FLAG_WATCHDOG was not passed to
 * app_main(), or the watchdog is unavailable on this platform.
 */
hw_watchdog_t *app_watchdog(const app_t *app) {
  return (app != NULL) ? app->watchdog : NULL;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

void app_shutdown(int exit_code) { sys_runloop_shutdown((uint32_t)exit_code); }

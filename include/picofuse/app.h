/**
 * @file app.h
 * @brief Application bootstrap: a single entry point wrapping the sys
 * lifecycle and run loop.
 * @defgroup App App
 * @ingroup Picofuse
 * @details
 * `app_main()` wraps the boilerplate common to most picofuse programs:
 * `sys_init()`, running the process run loop (`sys_runloop_run()`), and
 * `sys_exit()` on the way out. It also attempts `hw_init()`/`hw_poll()`/
 * `hw_exit()` and `hid_init()`/`hid_poll()`/`hid_deinit()`; these are weak
 * references (see `src/app/hw.c` and `src/app/hid.c`), so they are
 * harmless no-ops (`hid_init()` returning NULL) unless the
 * `picofuse-hw-obj` / `picofuse-hid-obj` modules happen to also be linked
 * into the binary, in which case that subsystem is fully initialized and
 * polled automatically.
 *
 * @code
 * static void on_start(app_t *app, void *userdata) {
 *   hid_t *hid = app_hid(app);
 *   if (hid != NULL) {
 *     hid_register_user_button(hid, KEYCODE_ESC);
 *   }
 * }
 *
 * static void on_event(app_t *app, sys_event_t event, void *userdata) {
 *   // Handle an event posted via sys_runloop_post(), or a hid_event_t
 *   // produced by a device registered in on_start (see hid_event_free()).
 *   if (event == my_exit_event) {
 *     app_shutdown(0);
 *   }
 * }
 *
 * int main(int argc, char *argv[]) {
 *   return app_main(argc, argv, APP_FLAG_NONE, on_start, on_event, NULL);
 * }
 * @endcode
 */
#pragma once
#include "hid.h"
#include "sys.h"

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Feature flags controlling how app_main() runs.
 * @ingroup App
 */
typedef enum {
  APP_FLAG_NONE = 0,             ///< Default behavior.
  APP_FLAG_MULTICORE = (1 << 0), ///< Run the event loop across all cores.
  APP_FLAG_SIGNAL = (1 << 1),    ///< Register environment signals (TERM,
                                 ///< INT, QUIT) as HID events, if HID is
                                 ///< available (see @ref app_hid). Has no
                                 ///< effect otherwise.
} app_flag_t;

/**
 * @brief Opaque application instance passed to app callbacks.
 * @ingroup App
 */
typedef struct app_t app_t;

/**
 * @brief Called once, on the main worker, before the run loop starts
 * dispatching events.
 * @ingroup App
 * @param app Application instance. Valid for the duration of app_main().
 * @param userdata Opaque pointer, as passed to app_main().
 *
 * Use this to complete setup that must run before events can be produced,
 * such as registering HID devices or opening buses.
 */
typedef void (*app_callback_start_t)(app_t *app, void *userdata);

/**
 * @brief Called on a worker for each event the run loop dispatches.
 * @ingroup App
 * @param app Application instance.
 * @param event Event to handle, as posted via `sys_runloop_post()` (see
 * `sys/runloop.h`).
 * @param userdata Opaque pointer, as passed to app_main().
 */
typedef void (*app_callback_event_t)(app_t *app, sys_event_t event,
                                     void *userdata);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize the sys subsystem, run the event loop, then tear it
 * down.
 * @ingroup App
 * @param argc Argument count, as passed to `main()`.
 * @param argv Argument vector, as passed to `main()`.
 * @param flags Feature flags selecting optional behavior (see
 * @ref app_flag_t).
 * @param on_start Called once, on the main worker, before the event loop
 * starts. May be NULL.
 * @param on_event Called for each event the loop dispatches. May be NULL
 * if nothing ever posts events.
 * @param userdata Opaque pointer passed through to @p on_start and
 * @p on_event.
 * @return Exit code, suitable for returning directly from `main()`.
 *
 * Calls `sys_init()`, attempts `hw_init()` and `hid_init()` (see
 * @ref app_hid), registers environment signals as HID events if
 * @ref APP_FLAG_SIGNAL is set and HID is available, then runs the event
 * loop across every available core if @ref APP_FLAG_MULTICORE is set, or
 * on the calling thread alone otherwise. Blocks until @ref app_shutdown is
 * called from within a callback (or from another thread), then tears down
 * (`hid_deinit()`, `hw_exit()`, `sys_exit()`).
 */
int app_main(int argc, char *argv[], app_flag_t flags,
            app_callback_start_t on_start, app_callback_event_t on_event,
            void *userdata);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Get the HID instance initialized for this app.
 * @ingroup App
 * @param app Application instance.
 * @return HID instance, or NULL if the `picofuse-hid-obj` module is not
 * linked into this binary (see @ref app_main).
 */
hid_t *app_hid(const app_t *app);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Request that the running app's event loop stop.
 * @ingroup App
 * @param exit_code Value app_main() returns once the loop has drained and
 * stopped.
 *
 * Safe to call from any callback or thread. Equivalent to
 * `sys_runloop_shutdown()`, exposed here so callers do not need to include
 * `sys/runloop.h` directly.
 */
void app_shutdown(int exit_code);

/** @} */

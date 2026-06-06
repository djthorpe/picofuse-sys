/**
 * @file
 * @brief HID user-button runloop example (single core, 10 second timeout).
 */

#include <picofuse/hid.h>
#include <picofuse/sys.h>

#define POLL_INTERVAL_MS 20u
#define TIMEOUT_MS 10000u

static sys_event_queue_t *_hid_queue = NULL;
static hid_t *_hid = NULL;
static hid_device_t *_user_button = NULL;
static sys_timer_t *_poll_timer = NULL;
static sys_timer_t *_timeout_timer = NULL;
static sys_atomic_t _event_count;

static uint8_t _poll_sentinel;
static uint8_t _timeout_sentinel;

#define POLL_EVENT ((sys_event_t) & _poll_sentinel)
#define TIMEOUT_EVENT ((sys_event_t) & _timeout_sentinel)

/**
 * @brief Post periodic poll events to the runloop.
 */
static void poll_timer_callback(sys_timer_t *timer) {
  (void)timer;
  (void)sys_runloop_post(POLL_EVENT);
}

/**
 * @brief Post timeout event and stop this one-shot timer.
 */
static void timeout_timer_callback(sys_timer_t *timer) {
  (void)sys_runloop_post(TIMEOUT_EVENT);
  sys_timer_deinit(timer);
  _timeout_timer = NULL;
}

/**
 * @brief Initialize HID and timers on worker 0.
 */
static void on_init(uint8_t worker_index) {
  if (worker_index != 0u) {
    return;
  }

  _hid_queue = sys_event_queue_init(32u);
  sys_assert(_hid_queue != NULL);

  _hid = hid_init(_hid_queue);
  sys_assert(_hid != NULL);

  _user_button = hid_register_user_button(_hid, KEYCODE_ENTER);
  if (_user_button != NULL) {
    sys_printf("hid user button registered\n");
  } else {
    sys_printf("hid user button not available for this board\n");
  }

  _poll_timer = sys_timer_init(POLL_INTERVAL_MS, NULL, poll_timer_callback);
  sys_assert(_poll_timer != NULL);
  sys_assert(sys_timer_start(_poll_timer));

  _timeout_timer = sys_timer_init(TIMEOUT_MS, NULL, timeout_timer_callback);
  sys_assert(_timeout_timer != NULL);
  sys_assert(sys_timer_start(_timeout_timer));
}

/**
 * @brief Handle runloop events by draining queued HID events or timing out.
 */
static void on_event(sys_event_t event) {
  if (event == TIMEOUT_EVENT) {
    sys_printf("timeout reached (%u ms), shutting down runloop\n",
               (unsigned int)TIMEOUT_MS);
    sys_runloop_shutdown(0u);
    return;
  }

  if (event != POLL_EVENT || _hid_queue == NULL) {
    return;
  }

  while (true) {
    hid_event_t *hid_event = (hid_event_t *)sys_event_queue_try_pop(_hid_queue);

    if (hid_event == NULL) {
      break;
    }

    uint32_t count = sys_atomic_inc(&_event_count);
    sys_printf("hid event %u: state=0x%08X keycode=%s\n", (unsigned int)count,
               (unsigned int)hid_event->state,
               hid_keycode_to_string(hid_event->keycode));

    hid_event_free(hid_event);
  }
}

/**
 * @brief Clean up timers, HID registrations, and queue resources.
 */
static void on_exit(uint8_t worker_index) {
  if (worker_index != 0u) {
    return;
  }

  if (_poll_timer != NULL) {
    sys_timer_deinit(_poll_timer);
    _poll_timer = NULL;
  }

  if (_timeout_timer != NULL) {
    sys_timer_deinit(_timeout_timer);
    _timeout_timer = NULL;
  }

  if (_hid != NULL && _user_button != NULL) {
    (void)hid_deregister(_hid, _user_button);
    _user_button = NULL;
  }

  if (_hid != NULL) {
    hid_deinit(_hid);
    _hid = NULL;
  }

  if (_hid_queue != NULL) {
    sys_event_queue_deinit(_hid_queue);
    _hid_queue = NULL;
  }
}

int main(void) {
  sys_init();
  sys_atomic_init(&_event_count, 0u);

  (void)sys_runloop_run(1u, on_init, on_event, on_exit);

  sys_printf("hid runloop example complete, processed %u events\n",
             (unsigned int)sys_atomic_get(&_event_count));

  sys_exit();
  return 0;
}

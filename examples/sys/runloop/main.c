/**
 * @file
 * @brief Runloop example.
 */

#include <picofuse/sys.h>

#define TIMER_INTERVAL_MS 100
#define TARGET_EVENTS 20

// Unique address used as a sentinel event value — the runloop callback
// checks for this pointer to distinguish timer ticks from other events.
static uint8_t _tick_sentinel;
#define TICK_EVENT ((sys_event_t) & _tick_sentinel)

static sys_atomic_t _event_count;
static sys_timer_t *_timer = NULL;

/**
 * @brief Post a synthetic tick event from the timer callback.
 *
 * The timer callback converts periodic timer interrupts into runloop events by
 * posting a sentinel value with @c sys_runloop_post.
 */
static void timer_callback(sys_timer_t *timer) {
  (void)timer;
  sys_runloop_post(TICK_EVENT);
}

/**
 * @brief Initialize the timer for worker zero once the runloop queue is ready.
 *
 * This hook delays timer creation until the runloop is active, ensuring that
 * posted tick events will be received instead of being dropped.
 */
static void on_init(uint8_t worker_index) {
  if (worker_index != 0) {
    return;
  }
  // Start the timer after the runloop queue is ready so posted events
  // are guaranteed to be received.
  _timer = sys_timer_init(TIMER_INTERVAL_MS, NULL, timer_callback);
  sys_assert(_timer != NULL);
  sys_assert(sys_timer_start(_timer));
}

/**
 * @brief Handle tick events, count them, and stop the runloop at the end.
 *
 * Each tick increments the atomic counter, prints progress, delays briefly to
 * show the scheduler in action, and tears down the timer when the target count
 * is reached.
 */
static void on_event(sys_event_t event) {
  if (event != TICK_EVENT) {
    return;
  }
  uint32_t count = sys_atomic_inc(&_event_count);
  if (count > TARGET_EVENTS) {
    return;
  }
  sys_printf("tick %u of %u on core %u\n", count, (uint32_t)TARGET_EVENTS,
             sys_thread_core());
  sys_sleep_ms(1 + sys_random_uint32() % 250);
  if (count == TARGET_EVENTS) {
    sys_timer_deinit(_timer);
    _timer = NULL;
    sys_runloop_shutdown(0);
  }
}

/**
 * @brief Clean up worker-zero resources after the runloop exits.
 *
 * The exit hook deinitializes the timer and clears the pointer so shutdown is
 * safe even if the timer was already stopped by the event handler.
 */
static void on_exit(uint8_t worker_index) {
  if (worker_index != 0) {
    return;
  }
  sys_timer_deinit(_timer);
  _timer = NULL;
}

/**
 * @brief Run the event loop until the timer-driven tick target is reached.
 *
 * This example shows the full runloop lifecycle: initialize shared state,
 * register init/event/exit hooks, let the loop drive timer events, and then
 * report the final tick count.
 */
int main(void) {
  sys_init();
  sys_atomic_init(&_event_count, 0);

  sys_runloop_run(0, on_init, on_event, NULL, on_exit);

  sys_printf("runloop exited after %u ticks\n", sys_atomic_get(&_event_count));

  sys_exit();
  return 0;
}

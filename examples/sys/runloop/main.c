#include <picofuse/sys.h>

#define TIMER_INTERVAL_MS 100
#define TARGET_EVENTS 20

// Unique address used as a sentinel event value — the runloop callback
// checks for this pointer to distinguish timer ticks from other events.
static uint8_t _tick_sentinel;
#define TICK_EVENT ((sys_event_t)&_tick_sentinel)

static sys_atomic_t _event_count;
static sys_timer_t *_timer = NULL;

static void timer_callback(sys_timer_t *timer) {
  (void)timer;
  sys_runloop_post(TICK_EVENT);
}

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

static void on_exit(uint8_t worker_index) {
  if (worker_index != 0) {
    return;
  }
  sys_timer_deinit(_timer);
  _timer = NULL;
}

int main(void) {
  sys_init();
  sys_atomic_init(&_event_count, 0);

  sys_runloop_run(0, on_init, on_event, on_exit);

  sys_printf("runloop exited after %u ticks\n",
             sys_atomic_get(&_event_count));

  sys_exit();
  return 0;
}

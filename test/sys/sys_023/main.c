#include <test.h>

static sys_atomic_t _fire_count;

static void counting_callback(sys_timer_t *timer) {
  (void)timer;
  sys_atomic_inc(&_fire_count);
}

static void oneshot_callback(sys_timer_t *timer) {
  sys_atomic_inc(&_fire_count);
  sys_timer_deinit(timer);
}

static bool wait_for_min_count(const sys_atomic_t *counter, uint32_t min,
                               uint32_t timeout_ms) {
  uint64_t deadline = sys_timestamp_ms() + timeout_ms;
  while (sys_atomic_get(counter) < min) {
    if (sys_timestamp_ms() >= deadline) {
      return false;
    }
    sys_sleep_ms(1);
  }
  return true;
}

bool test_main(void) {
  // Periodic timer fires multiple times
  sys_atomic_init(&_fire_count, 0);
  sys_timer_t *timer = sys_timer_init(50, NULL, counting_callback);
  TestAssert(timer != NULL, "should allocate periodic timer");
  TestAssert(sys_timer_start(timer), "should start periodic timer");
  TestAssert(wait_for_min_count(&_fire_count, 3, 500),
             "periodic timer should fire at least 3 times within 500ms");
  sys_timer_deinit(timer);
  TestAssert(!sys_timer_valid(timer),
             "periodic timer should be invalid after deinit");

  // One-shot: callback deinits the timer itself
  sys_atomic_init(&_fire_count, 0);
  timer = sys_timer_init(50, NULL, oneshot_callback);
  TestAssert(timer != NULL, "should allocate one-shot timer");
  TestAssert(sys_timer_start(timer), "should start one-shot timer");
  sys_sleep_ms(300);
  TestAssert(sys_atomic_get(&_fire_count) == 1,
             "one-shot timer should fire exactly once");
  TestAssert(!sys_timer_valid(timer),
             "one-shot timer should be invalid after firing");

  return true;
}

TestMain(test_main)

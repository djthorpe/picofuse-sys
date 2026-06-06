#include <test.h>

static sys_atomic_t _callback_entered;
static sys_atomic_t _callback_exited;

static void blocking_callback(sys_timer_t *timer) {
  (void)timer;
  sys_atomic_inc(&_callback_entered);
  sys_sleep_ms(200);
  sys_atomic_inc(&_callback_exited);
}

static bool wait_for_count(const sys_atomic_t *counter, uint32_t min,
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
  sys_atomic_init(&_callback_entered, 0);
  sys_atomic_init(&_callback_exited, 0);

  sys_timer_t *timer = sys_timer_init(10, NULL, blocking_callback);
  TestAssert(timer != NULL, "should allocate timer");
  TestAssert(sys_timer_start(timer), "should start timer");
  TestAssert(wait_for_count(&_callback_entered, 1, 500),
             "callback should start within timeout");

  uint64_t start_ms = sys_timestamp_ms();
  sys_timer_deinit(timer);
  uint64_t elapsed_ms = sys_timestamp_ms() - start_ms;

  TestAssert(elapsed_ms >= 150,
             "sys_timer_deinit should wait for in-flight callback to finish");
  TestAssert(sys_atomic_get(&_callback_exited) == 1,
             "callback should have completed before deinit returned");
  TestAssert(!sys_timer_valid(timer), "timer should be invalid after deinit");

  return true;
}

TestMain(test_main)

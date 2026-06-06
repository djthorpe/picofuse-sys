#include <test.h>

static sys_atomic_t _callback_entered;
static sys_atomic_t _callback_exited;
static sys_atomic_t _deinit_done;

static void blocking_callback(sys_timer_t *timer) {
  (void)timer;
  sys_atomic_inc(&_callback_entered);
  uint64_t deadline = sys_timestamp_ms() + 200u;
  while (sys_timestamp_ms() < deadline) {
    for (volatile uint32_t spin = 0u; spin < 1000u; ++spin) {
    }
  }
  sys_atomic_inc(&_callback_exited);
}

static bool wait_for_count(const sys_atomic_t *counter, uint32_t min,
                           uint32_t timeout_ms);

static void deinit_worker(void *arg) {
  sys_timer_t *timer = (sys_timer_t *)arg;

  if (!wait_for_count(&_callback_entered, 1, 500)) {
    sys_timer_deinit(timer);
    sys_atomic_set(&_deinit_done, 2);
    return;
  }

  sys_timer_deinit(timer);
  sys_atomic_set(&_deinit_done, 1);
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
  sys_atomic_init(&_deinit_done, 0);

  sys_timer_t *timer = sys_timer_init(10, NULL, blocking_callback);
  TestAssert(timer != NULL, "should allocate timer");
  TestAssert(sys_timer_start(timer), "should start timer");

#ifdef SYSTEM_NAME_PICO
  TestAssert(sys_thread_create_on_core(deinit_worker, timer, 1),
             "should launch deinit worker on core 1");
#else
  TestAssert(sys_thread_create(deinit_worker, timer),
             "should launch deinit worker thread");
#endif

  uint64_t start_ms = sys_timestamp_ms();
  TestAssert(wait_for_count(&_deinit_done, 1, 1000),
             "deinit worker should complete within timeout");
  uint64_t elapsed_ms = sys_timestamp_ms() - start_ms;

  TestAssert(sys_atomic_get(&_deinit_done) == 1,
             "deinit worker should succeed");
  TestAssert(elapsed_ms >= 150,
             "sys_timer_deinit should wait for in-flight callback to finish");
  TestAssert(sys_atomic_get(&_callback_exited) == 1,
             "callback should have completed before deinit returned");
  TestAssert(!sys_timer_valid(timer), "timer should be invalid after deinit");

  return true;
}

TestMain(test_main)

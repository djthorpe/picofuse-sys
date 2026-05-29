#include <test.h>

static sys_atomic_t _core0_fire_count;
static sys_atomic_t _core1_fire_count;
static sys_atomic_t _core1_done;

static void core0_callback(sys_timer_t *timer) {
  (void)timer;
  sys_atomic_inc(&_core0_fire_count);
}

static void core1_callback(sys_timer_t *timer) {
  (void)timer;
  sys_atomic_inc(&_core1_fire_count);
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

static void core1_worker(void *arg) {
  (void)arg;

  sys_timer_t *timer = sys_timer_init(50, NULL, core1_callback);
  if (timer == NULL || !sys_timer_start(timer)) {
    if (timer != NULL) {
      sys_timer_deinit(timer);
    }
    sys_atomic_set(&_core1_done, 2);
    return;
  }

  if (!wait_for_min_count(&_core1_fire_count, 3, 500)) {
    sys_timer_deinit(timer);
    sys_atomic_set(&_core1_done, 2);
    return;
  }

  sys_timer_deinit(timer);
  sys_atomic_set(&_core1_done, 1);
}

bool test_main(void) {
  sys_atomic_init(&_core0_fire_count, 0);
  sys_atomic_init(&_core1_fire_count, 0);
  sys_atomic_init(&_core1_done, 0);

  sys_timer_t *timer = sys_timer_init(50, NULL, core0_callback);
  TestAssert(timer != NULL, "core 0 should allocate a timer");
  TestAssert(sys_timer_start(timer), "core 0 should start its timer");

#ifdef SYSTEM_NAME_PICO
  TestAssert(sys_thread_create_on_core(core1_worker, NULL, 1),
             "should launch worker on core 1");
#else
  TestAssert(sys_thread_create(core1_worker, NULL),
             "should launch worker thread");
#endif

  TestAssert(wait_for_min_count(&_core0_fire_count, 3, 500),
             "core 0 timer should fire at least 3 times concurrently");
  TestAssert(wait_for_min_count(&_core1_done, 1, 1000),
             "core 1 worker should complete within timeout");
  TestAssert(sys_atomic_get(&_core1_done) == 1,
             "core 1 timer should fire at least 3 times and deinit cleanly");

  sys_timer_deinit(timer);
  return true;
}

TestMain(test_main)

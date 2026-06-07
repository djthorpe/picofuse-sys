#include <test.h>

static sys_atomic_t _worker0_inited;
static sys_atomic_t _worker1_inited;
static sys_atomic_t _event_count;
static sys_atomic_t _exit_count;

static uint8_t _shutdown_sentinel;
#define SHUTDOWN_EVENT ((sys_event_t) & _shutdown_sentinel)

static bool wait_for_flag(const sys_atomic_t *flag, uint32_t timeout_ms) {
  uint64_t deadline = sys_timestamp_ms() + timeout_ms;
  while (!sys_atomic_get(flag)) {
    if (sys_timestamp_ms() >= deadline) {
      return false;
    }
    sys_sleep_ms(1);
  }
  return true;
}

static void on_init(uint8_t worker_index) {
  if (worker_index == 0) {
    sys_atomic_set(&_worker0_inited, 1);
    // Wait for worker 1 to start before posting — ensures it is alive
    // before the shutdown sentinel can be processed and the queue drained.
    if (!wait_for_flag(&_worker1_inited, 1000)) {
      sys_runloop_shutdown(0);
      return;
    }
    for (uint32_t i = 1; i <= 5; i++) {
      sys_runloop_post((sys_event_t)(uintptr_t)i);
    }
    sys_runloop_post(SHUTDOWN_EVENT);
  } else {
    sys_atomic_set(&_worker1_inited, 1);
  }
}

static void on_event(sys_event_t event) {
  if (event == SHUTDOWN_EVENT) {
    sys_runloop_shutdown(7);
    return;
  }
  sys_atomic_inc(&_event_count);
}

static void on_exit(uint8_t worker_index) {
  (void)worker_index;
  sys_atomic_inc(&_exit_count);
}

bool test_main(void) {
  uint8_t num_workers = sys_thread_numcores() >= 2 ? 2 : 1;

  sys_atomic_init(&_worker0_inited, 0);
  sys_atomic_init(&_worker1_inited, 0);
  sys_atomic_init(&_event_count, 0);
  sys_atomic_init(&_exit_count, 0);

  uint32_t result =
      sys_runloop_run(num_workers, on_init, on_event, NULL, on_exit);

  TestAssert(result == 7, "run should return the shutdown exit value");
  TestAssert(!sys_runloop_valid(), "run loop should not be valid after run");
  TestAssert(sys_atomic_get(&_worker0_inited) == 1,
             "worker 0 init should have run");
  TestAssert(sys_atomic_get(&_event_count) == 5,
             "all 5 events should be processed across workers");
  TestAssert(sys_atomic_get(&_exit_count) == num_workers,
             "exit should be called once per worker");

  if (num_workers >= 2) {
    TestAssert(sys_atomic_get(&_worker1_inited) == 1,
               "worker 1 init should have run");
  }

  return true;
}

TestMain(test_main)

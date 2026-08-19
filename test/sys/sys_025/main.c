#include <test.h>

static sys_atomic_t _event_count;
static sys_atomic_t _init_count;
static sys_atomic_t _init_index;
static sys_atomic_t _exit_count;

static uint8_t _shutdown_sentinel;
#define SHUTDOWN_EVENT ((sys_event_t) & _shutdown_sentinel)

static void on_init(uint8_t worker_index) {
  sys_atomic_inc(&_init_count);
  sys_atomic_set(&_init_index, worker_index);
  for (uint32_t i = 1; i <= 3; i++) {
    sys_runloop_post((sys_event_t)(uintptr_t)i);
  }
  sys_runloop_post(SHUTDOWN_EVENT);
}

static void on_event(sys_event_t event) {
  if (event == SHUTDOWN_EVENT) {
    sys_runloop_shutdown(99);
    return;
  }
  sys_atomic_inc(&_event_count);
}

static void on_exit(uint8_t worker_index) {
  (void)worker_index;
  sys_atomic_inc(&_exit_count);
}

bool test_main(void) {
  sys_atomic_init(&_event_count, 0);
  sys_atomic_init(&_init_count, 0);
  sys_atomic_init(&_init_index, 255);
  sys_atomic_init(&_exit_count, 0);

  TestAssert(!sys_runloop_valid(), "run loop should not be valid before run");
  TestAssert(!sys_runloop_post((sys_event_t)(uintptr_t)1u),
             "post should fail before run");

  uint32_t result = sys_runloop_run(1, on_init, on_event, NULL, on_exit);

  TestAssert(result == 99, "run should return the shutdown exit value");
  TestAssert(!sys_runloop_valid(), "run loop should not be valid after run");
  TestAssert(sys_atomic_get(&_event_count) == 3,
             "all 3 events should be processed");
  TestAssert(sys_atomic_get(&_init_count) == 1,
             "init should be called exactly once");
  TestAssert(sys_atomic_get(&_init_index) == 0,
             "init should receive worker index 0");
  TestAssert(sys_atomic_get(&_exit_count) == 1,
             "exit should be called exactly once");

  // Run loop should be reusable after it exits
  sys_atomic_set(&_event_count, 0);
  sys_atomic_set(&_init_count, 0);
  sys_atomic_set(&_exit_count, 0);

  result = sys_runloop_run(1, on_init, on_event, NULL, on_exit);
  TestAssert(result == 99, "second run should also return correct exit value");
  TestAssert(sys_atomic_get(&_event_count) == 3,
             "second run should process all events");
  TestAssert(sys_atomic_get(&_exit_count) == 1,
             "exit should be called once on second run");

  return true;
}

TestMain(test_main)

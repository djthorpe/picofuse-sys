#include <test.h>

static void noop_callback(sys_timer_t *timer) { (void)timer; }

bool test_main(void) {
  // Invalid arguments
  TestAssert(sys_timer_init(0, NULL, noop_callback) == NULL,
             "sys_timer_init should reject zero interval");
  TestAssert(sys_timer_init(100, NULL, NULL) == NULL,
             "sys_timer_init should reject NULL callback");

  // Valid init — not yet running
  sys_timer_t *timer = sys_timer_init(100, NULL, noop_callback);
  TestAssert(timer != NULL, "sys_timer_init should return non-NULL");
  TestAssert(!sys_timer_valid(timer), "timer should not be valid before start");

  // Start
  TestAssert(sys_timer_start(timer), "sys_timer_start should succeed");
  TestAssert(sys_timer_valid(timer), "timer should be valid after start");
  TestAssert(!sys_timer_start(timer),
             "sys_timer_start should fail when already running");

  // Deinit stops and frees the slot
  sys_timer_deinit(timer);
  TestAssert(!sys_timer_valid(timer), "timer should not be valid after deinit");

  // NULL safety
  sys_timer_deinit(NULL);
  TestAssert(!sys_timer_valid(NULL), "sys_timer_valid should handle NULL");

  // Pool exhaustion
  sys_timer_t *pool[SYS_TIMER_CAPACITY];
  for (int i = 0; i < SYS_TIMER_CAPACITY; i++) {
    pool[i] = sys_timer_init(100, NULL, noop_callback);
    TestAssert(pool[i] != NULL, "should allocate up to SYS_TIMER_CAPACITY");
  }
  TestAssert(sys_timer_init(100, NULL, noop_callback) == NULL,
             "sys_timer_init should return NULL when pool is exhausted");

  // Release all and verify pool is reusable
  for (int i = 0; i < SYS_TIMER_CAPACITY; i++) {
    sys_timer_deinit(pool[i]);
  }
  timer = sys_timer_init(100, NULL, noop_callback);
  TestAssert(timer != NULL, "pool slot should be reusable after deinit");
  sys_timer_deinit(timer);

  return true;
}

TestMain(test_main)

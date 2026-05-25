#include <test.h>

typedef struct {
  sys_waitgroup_t *wg;
  sys_atomic_t ready;
  sys_atomic_t completed;
  sys_atomic_t worker_ok;
} waitgroup_test_ctx_t;

static bool launch_test_thread(sys_thread_func_t func, void *arg) {
#ifdef SYSTEM_NAME_PICO
  return sys_thread_create_on_core(func, arg, 1);
#else
  return sys_thread_create(func, arg);
#endif
}

static bool wait_for_atomic_value(const sys_atomic_t *value, uint32_t expected,
                                  uint32_t timeout_ms) {
  uint64_t deadline = sys_timestamp_ms() + timeout_ms;
  while (sys_atomic_get(value) != expected) {
    if (sys_timestamp_ms() >= deadline) {
      return false;
    }
    sys_sleep_ms(1);
  }
  return true;
}

static void waitgroup_worker(void *arg) {
  waitgroup_test_ctx_t *ctx = (waitgroup_test_ctx_t *)arg;

  sys_atomic_inc(&ctx->ready);
  sys_sleep_ms(20);

  if (sys_waitgroup_done(ctx->wg)) {
    sys_atomic_inc(&ctx->worker_ok);
  }
  sys_atomic_inc(&ctx->completed);
}

bool test_main(void) {
  waitgroup_test_ctx_t ctx;

  sys_waitgroup_t *wg = sys_waitgroup_init();
  TestAssert(wg != NULL, "sys_waitgroup_init returned NULL");
  TestAssert(!sys_waitgroup_add(wg, -1),
             "sys_waitgroup_add should reject negative deltas");
  TestAssert(!sys_waitgroup_done(wg),
             "sys_waitgroup_done should reject underflow at zero");
  sys_waitgroup_wait(wg);

  wg = sys_waitgroup_init();
  TestAssert(wg != NULL, "sys_waitgroup_init returned NULL for blocking test");
  TestAssert(sys_waitgroup_add(wg, 1),
             "sys_waitgroup_add should accept positive deltas");

  ctx.wg = wg;
  sys_atomic_init(&ctx.ready, 0);
  sys_atomic_init(&ctx.completed, 0);
  sys_atomic_init(&ctx.worker_ok, 0);

  TestAssert(launch_test_thread(waitgroup_worker, &ctx),
             "Failed to launch waitgroup worker");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "Timed out waiting for waitgroup worker to become ready");

  uint64_t wait_start = sys_timestamp_ms();
  sys_waitgroup_wait(wg);
  uint64_t wait_elapsed = sys_timestamp_ms() - wait_start;

  TestAssert(wait_elapsed >= 10,
             "sys_waitgroup_wait should block until worker completion, only "
             "waited %" PRIu64 " ms",
             wait_elapsed);
  TestAssert(wait_for_atomic_value(&ctx.completed, 1, 1000),
             "Timed out waiting for waitgroup worker to complete");
  TestAssert(sys_atomic_get(&ctx.worker_ok) == 1,
             "Expected worker to successfully call sys_waitgroup_done");

  sys_waitgroup_t *waitgroups[SYS_WAITGROUP_CAPACITY];
  for (size_t index = 0; index < SYS_WAITGROUP_CAPACITY; index++) {
    waitgroups[index] = sys_waitgroup_init();
    TestAssert(waitgroups[index] != NULL,
               "sys_waitgroup_init failed at pool slot %zu", index);
  }

  TestAssert(sys_waitgroup_init() == NULL,
             "sys_waitgroup_init should fail after %u allocations",
             SYS_WAITGROUP_CAPACITY);

  for (size_t index = 0; index < SYS_WAITGROUP_CAPACITY; index++) {
    sys_waitgroup_wait(waitgroups[index]);
  }

  wg = sys_waitgroup_init();
  TestAssert(
      wg != NULL,
      "sys_waitgroup_init should succeed again after pool slots release");
  sys_waitgroup_wait(wg);

  return true;
}

TestMain(test_main)
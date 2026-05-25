#include <test.h>

typedef struct {
  sys_atomic_t done;
  sys_atomic_t ready;
  sys_atomic_t predicate;
  sys_atomic_t wait_ok;
  sys_atomic_t timedwait_result;
  sys_cond_t *cond;
  sys_mutex_t *mutex;
} cond_test_ctx_t;

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

static void cond_wait_worker(void *arg) {
  cond_test_ctx_t *ctx = (cond_test_ctx_t *)arg;

  if (!sys_mutex_lock(ctx->mutex)) {
    sys_atomic_set(&ctx->done, 1);
    return;
  }

  sys_atomic_set(&ctx->ready, 1);
  while (sys_atomic_get(&ctx->predicate) == 0) {
    if (!sys_cond_wait(ctx->cond, ctx->mutex)) {
      sys_mutex_unlock(ctx->mutex);
      sys_atomic_set(&ctx->done, 1);
      return;
    }
  }

  sys_atomic_set(&ctx->wait_ok, 1);
  sys_mutex_unlock(ctx->mutex);
  sys_atomic_set(&ctx->done, 1);
}

static void cond_timedwait_worker(void *arg) {
  cond_test_ctx_t *ctx = (cond_test_ctx_t *)arg;

  if (!sys_mutex_lock(ctx->mutex)) {
    sys_atomic_set(&ctx->done, 1);
    return;
  }

  sys_atomic_set(&ctx->ready, 1);
  sys_atomic_set(&ctx->timedwait_result,
                 sys_cond_timedwait(ctx->cond, ctx->mutex, 20) ? 1u : 0u);
  sys_mutex_unlock(ctx->mutex);
  sys_atomic_set(&ctx->done, 1);
}

bool test_main(void) {
  cond_test_ctx_t ctx;
  sys_cond_t *cond = sys_cond_init();
  sys_mutex_t *mutex = sys_mutex_init();

  TestAssert(cond != NULL, "sys_cond_init returned NULL");
  TestAssert(mutex != NULL, "sys_mutex_init returned NULL");

  ctx.cond = cond;
  ctx.mutex = mutex;
  sys_atomic_init(&ctx.done, 0);
  sys_atomic_init(&ctx.ready, 0);
  sys_atomic_init(&ctx.predicate, 0);
  sys_atomic_init(&ctx.wait_ok, 0);
  sys_atomic_init(&ctx.timedwait_result, 1);

  TestAssert(launch_test_thread(cond_wait_worker, &ctx),
             "Failed to launch cond wait worker");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "Timed out waiting for cond waiter to become ready");

  TestAssert(sys_mutex_lock(mutex), "Main thread failed to lock cond mutex");
  sys_atomic_set(&ctx.predicate, 1);
  TestAssert(sys_cond_signal(cond), "sys_cond_signal failed");
  TestAssert(sys_mutex_unlock(mutex),
             "Main thread failed to unlock cond mutex after signal");

  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "Timed out waiting for cond waiter completion");
  TestAssert(sys_atomic_get(&ctx.wait_ok) == 1,
             "Condition wait worker did not observe signaled predicate");

  sys_cond_deinit(cond);
  cond = sys_cond_init();
  TestAssert(cond != NULL, "sys_cond_init should reuse a deinitialized slot");

  ctx.cond = cond;
  sys_atomic_set(&ctx.done, 0);
  sys_atomic_set(&ctx.ready, 0);
  sys_atomic_set(&ctx.predicate, 0);
  sys_atomic_set(&ctx.wait_ok, 0);
  sys_atomic_set(&ctx.timedwait_result, 1);

  TestAssert(launch_test_thread(cond_timedwait_worker, &ctx),
             "Failed to launch cond timedwait worker");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "Timed out waiting for cond timedwait worker readiness");
  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "Timed out waiting for cond timedwait worker completion");
  TestAssert(sys_atomic_get(&ctx.timedwait_result) == 0,
             "sys_cond_timedwait should return false on timeout");

  sys_cond_deinit(cond);
  sys_mutex_deinit(mutex);
  return true;
}

TestMain(test_main)
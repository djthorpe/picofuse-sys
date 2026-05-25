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

#ifndef SYSTEM_NAME_PICO
typedef struct {
  sys_atomic_t done;
  sys_atomic_t ready;
  sys_atomic_t wait_ok;
  sys_atomic_t *predicate;
  sys_cond_t *cond;
  sys_mutex_t *mutex;
} cond_broadcast_ctx_t;
#endif

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

static bool timedwait_in_current_thread(sys_cond_t *cond, sys_mutex_t *mutex,
                                        uint32_t timeout_ms) {
  if (!sys_mutex_lock(mutex)) {
    return false;
  }

  bool signaled = sys_cond_timedwait(cond, mutex, timeout_ms);
  bool unlocked = sys_mutex_unlock(mutex);
  return unlocked && signaled;
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

#ifndef SYSTEM_NAME_PICO
static void cond_broadcast_worker(void *arg) {
  cond_broadcast_ctx_t *ctx = (cond_broadcast_ctx_t *)arg;

  if (!sys_mutex_lock(ctx->mutex)) {
    sys_atomic_set(&ctx->done, 1);
    return;
  }

  sys_atomic_set(&ctx->ready, 1);
  while (sys_atomic_get(ctx->predicate) == 0) {
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
#endif

bool test_main(void) {
  cond_test_ctx_t ctx;
  sys_cond_t *cond = sys_cond_init();
  sys_mutex_t *mutex = sys_mutex_init();

  TestAssert(cond != NULL, "sys_cond_init returned NULL");
  TestAssert(mutex != NULL, "sys_mutex_init returned NULL");

  TestAssert(sys_cond_signal(cond),
             "sys_cond_signal should succeed with no waiters");
  TestAssert(!timedwait_in_current_thread(cond, mutex, 20),
             "signal with no waiters should not wake a future waiter");

  TestAssert(sys_cond_broadcast(cond),
             "sys_cond_broadcast should succeed with no waiters");
  TestAssert(!timedwait_in_current_thread(cond, mutex, 20),
             "broadcast with no waiters should not wake a future waiter");

  ctx.cond = cond;
  ctx.mutex = mutex;
  sys_atomic_init(&ctx.done, 0);
  sys_atomic_init(&ctx.ready, 0);
  sys_atomic_init(&ctx.predicate, 0);
  sys_atomic_init(&ctx.wait_ok, 0);
  sys_atomic_init(&ctx.timedwait_result, 1);

  TestAssert(launch_test_thread(cond_timedwait_worker, &ctx),
             "Failed to launch cond timedwait worker");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "Timed out waiting for cond timedwait worker readiness");
  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "Timed out waiting for cond timedwait worker completion");
  TestAssert(sys_atomic_get(&ctx.timedwait_result) == 0,
             "sys_cond_timedwait should return false on timeout");

  TestAssert(sys_cond_signal(cond),
             "late sys_cond_signal should succeed after timeout");
  TestAssert(!timedwait_in_current_thread(cond, mutex, 20),
             "late signal after a timeout should not leave a stored wakeup");

#ifndef SYSTEM_NAME_PICO
  sys_atomic_t predicate;
  cond_broadcast_ctx_t first;
  cond_broadcast_ctx_t second;

  sys_atomic_init(&predicate, 0);

  first.cond = cond;
  first.mutex = mutex;
  first.predicate = &predicate;
  sys_atomic_init(&first.done, 0);
  sys_atomic_init(&first.ready, 0);
  sys_atomic_init(&first.wait_ok, 0);

  second.cond = cond;
  second.mutex = mutex;
  second.predicate = &predicate;
  sys_atomic_init(&second.done, 0);
  sys_atomic_init(&second.ready, 0);
  sys_atomic_init(&second.wait_ok, 0);

  TestAssert(launch_test_thread(cond_broadcast_worker, &first),
             "Failed to launch first broadcast waiter");
  TestAssert(launch_test_thread(cond_broadcast_worker, &second),
             "Failed to launch second broadcast waiter");
  TestAssert(wait_for_atomic_value(&first.ready, 1, 1000),
             "Timed out waiting for first broadcast waiter readiness");
  TestAssert(wait_for_atomic_value(&second.ready, 1, 1000),
             "Timed out waiting for second broadcast waiter readiness");

  TestAssert(sys_mutex_lock(mutex),
             "Main thread failed to lock cond mutex before broadcast");
  sys_atomic_set(&predicate, 1);
  TestAssert(sys_cond_broadcast(cond), "sys_cond_broadcast failed");
  TestAssert(sys_mutex_unlock(mutex),
             "Main thread failed to unlock cond mutex after broadcast");

  TestAssert(wait_for_atomic_value(&first.done, 1, 1000),
             "Timed out waiting for first broadcast waiter completion");
  TestAssert(wait_for_atomic_value(&second.done, 1, 1000),
             "Timed out waiting for second broadcast waiter completion");
  TestAssert(sys_atomic_get(&first.wait_ok) == 1,
             "First waiter was not released by broadcast");
  TestAssert(sys_atomic_get(&second.wait_ok) == 1,
             "Second waiter was not released by broadcast");
#endif

  sys_cond_deinit(cond);
  sys_mutex_deinit(mutex);
  return true;
}

TestMain(test_main)
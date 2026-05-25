#include <test.h>

typedef struct {
  sys_mutex_t *mutex;
  sys_atomic_t done;
  sys_atomic_t worker_error;
  uint32_t *counter;
  uint32_t iterations;
} mutex_thread_ctx_t;

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

static void mutex_worker(void *arg) {
  mutex_thread_ctx_t *ctx = (mutex_thread_ctx_t *)arg;
  for (uint32_t index = 0; index < ctx->iterations; index++) {
    if (!sys_mutex_lock(ctx->mutex)) {
      sys_atomic_set(&ctx->worker_error, index * 2 + 1);
      sys_atomic_set(&ctx->done, 1);
      return;
    }

    (*ctx->counter)++;

    if (!sys_mutex_unlock(ctx->mutex)) {
      sys_atomic_set(&ctx->worker_error, index * 2 + 2);
      sys_atomic_set(&ctx->done, 1);
      return;
    }
  }

  sys_atomic_set(&ctx->done, 1);
}

bool test_main(void) {
  const uint32_t iterations = 1000;
  uint32_t counter = 0;
  sys_mutex_t *mutex = sys_mutex_init();
  mutex_thread_ctx_t ctx;

  TestAssert(mutex != NULL, "sys_mutex_init returned NULL");

  ctx.mutex = mutex;
  ctx.counter = &counter;
  ctx.iterations = iterations;
  sys_atomic_init(&ctx.done, 0);
  sys_atomic_init(&ctx.worker_error, 0);

  TestAssert(launch_test_thread(mutex_worker, &ctx),
             "Failed to launch mutex worker thread");

  for (uint32_t index = 0; index < iterations; index++) {
    TestAssert(sys_mutex_lock(mutex),
               "Main thread failed to lock shared mutex at iteration %u",
               index);
    counter++;
    TestAssert(sys_mutex_unlock(mutex),
               "Main thread failed to unlock shared mutex at iteration %u",
               index);
  }

  TestAssert(wait_for_atomic_value(&ctx.done, 1, 2000),
             "Timed out waiting for mutex worker completion");
  TestAssert(sys_atomic_get(&ctx.worker_error) == 0,
             "Worker mutex operation failed with code %u",
             sys_atomic_get(&ctx.worker_error));
  TestAssert(counter == iterations * 2,
             "Shared counter should be %u after both threads finish, got %u",
             iterations * 2, counter);

  sys_mutex_deinit(mutex);
  return true;
}

TestMain(test_main)
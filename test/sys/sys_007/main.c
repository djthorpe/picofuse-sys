#include <test.h>

typedef struct {
  sys_atomic_t done;
  sys_atomic_t release;
  sys_atomic_t started;
  sys_atomic_t worker_core;
} thread_test_ctx_t;

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

static void thread_worker(void *arg) {
  thread_test_ctx_t *ctx = (thread_test_ctx_t *)arg;
  sys_atomic_set(&ctx->started, 1);
  sys_atomic_set(&ctx->worker_core, sys_thread_core());

#ifdef SYSTEM_NAME_PICO
  while (sys_atomic_get(&ctx->release) == 0) {
    sys_sleep_ms(1);
  }
#endif

  sys_atomic_set(&ctx->done, 1);
}

bool test_main(void) {
  thread_test_ctx_t ctx;
  uint8_t num_cores = sys_thread_numcores();

  sys_atomic_init(&ctx.done, 0);
  sys_atomic_init(&ctx.release, 0);
  sys_atomic_init(&ctx.started, 0);
  sys_atomic_init(&ctx.worker_core, UINT32_MAX);

#ifdef SYSTEM_NAME_PICO
  TestAssert(sys_thread_create_on_core(thread_worker, &ctx, 1),
             "Failed to launch worker on Pico core 1");
  TestAssert(wait_for_atomic_value(&ctx.started, 1, 1000),
             "Timed out waiting for Pico worker thread to start");
  TestAssert(!sys_thread_create_on_core(thread_worker, &ctx, 1),
             "Second Pico worker launch should fail while core 1 is busy");
  sys_atomic_set(&ctx.release, 1);
#else
  TestAssert(sys_thread_create(thread_worker, &ctx),
             "Failed to launch worker thread");
#endif

  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "Timed out waiting for worker thread completion");

  uint32_t worker_core = sys_atomic_get(&ctx.worker_core);
  TestAssert(
      worker_core < num_cores,
      "Worker core should be less than available cores: core=%u cores=%u",
      worker_core, num_cores);

#ifdef SYSTEM_NAME_PICO
  TestAssert(worker_core == 1, "Pico worker should run on core 1, got %u",
             worker_core);
#endif

  return true;
}

TestMain(test_main)
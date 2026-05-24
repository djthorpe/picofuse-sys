#include <test.h>

#ifndef SYSTEM_NAME_PICO
typedef struct {
  sys_atomic_t entered;
  sys_atomic_t release;
  sys_atomic_t completed;
} thread_capacity_ctx_t;

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

static void capacity_worker(void *arg) {
  thread_capacity_ctx_t *ctx = (thread_capacity_ctx_t *)arg;

  sys_atomic_inc(&ctx->entered);
  while (sys_atomic_get(&ctx->release) == 0) {
    sys_sleep_ms(1);
  }
  sys_atomic_inc(&ctx->completed);
}
#endif

bool test_main(void) {
#ifdef SYSTEM_NAME_PICO
  return true;
#else
  thread_capacity_ctx_t ctx;

  sys_atomic_init(&ctx.entered, 0);
  sys_atomic_init(&ctx.release, 0);
  sys_atomic_init(&ctx.completed, 0);

  for (uint32_t index = 0; index < SYS_THREAD_CAPACITY; ++index) {
    TestAssert(sys_thread_create(capacity_worker, &ctx),
               "Failed to launch worker %u within SYS_THREAD_CAPACITY=%u",
               index, SYS_THREAD_CAPACITY);
    TestAssert(wait_for_atomic_value(&ctx.entered, index + 1, 1000),
               "Timed out waiting for worker %u to start", index);
  }

  TestAssert(!sys_thread_create(capacity_worker, &ctx),
             "sys_thread_create should fail after %u concurrent threads",
             SYS_THREAD_CAPACITY);

  sys_atomic_set(&ctx.release, 1);
  TestAssert(wait_for_atomic_value(&ctx.completed, SYS_THREAD_CAPACITY, 2000),
             "Timed out waiting for %u workers to finish", SYS_THREAD_CAPACITY);

  TestAssert(sys_thread_create(capacity_worker, &ctx),
             "sys_thread_create should succeed again after workers finish");
  TestAssert(wait_for_atomic_value(&ctx.entered, SYS_THREAD_CAPACITY + 1, 1000),
             "Timed out waiting for post-release worker to start");
  TestAssert(
      wait_for_atomic_value(&ctx.completed, SYS_THREAD_CAPACITY + 1, 1000),
      "Timed out waiting for post-release worker to finish");

#if !defined(__linux__)
  TestAssert(!sys_thread_create_on_core(capacity_worker, &ctx, 0),
             "sys_thread_create_on_core should fail on unsupported hosts");
#endif

  return true;
#endif
}

TestMain(test_main)
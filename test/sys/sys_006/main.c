#include <test.h>

typedef struct {
  sys_atomic_t *value;
  sys_atomic_t *done;
} atomic_thread_ctx_t;

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

static void atomic_worker(void *arg) {
  atomic_thread_ctx_t *ctx = (atomic_thread_ctx_t *)arg;
  sys_atomic_inc(ctx->value);
  sys_atomic_set(ctx->done, 1);
}

bool test_main(void) {
  sys_atomic_t value;
  sys_atomic_t done;
  atomic_thread_ctx_t ctx = {.value = &value, .done = &done};

  sys_atomic_init(&value, 5);
  TestAssert(sys_atomic_get(&value) == 5,
             "sys_atomic_init/sys_atomic_get mismatch: expected 5 got %u",
             sys_atomic_get(&value));

  sys_atomic_set(&value, 10);
  TestAssert(sys_atomic_get(&value) == 10,
             "sys_atomic_set should store 10, got %u", sys_atomic_get(&value));

  TestAssert(sys_atomic_inc(&value) == 11,
             "sys_atomic_inc should return 11, got %u", sys_atomic_get(&value));
  TestAssert(sys_atomic_dec(&value) == 10,
             "sys_atomic_dec should return 10, got %u", sys_atomic_get(&value));

  sys_atomic_set_bits(&value, 0x05);
  TestAssert(sys_atomic_get(&value) == 15,
             "sys_atomic_set_bits should produce 15, got %u",
             sys_atomic_get(&value));

  sys_atomic_clear_bits(&value, 0x03);
  TestAssert(sys_atomic_get(&value) == 12,
             "sys_atomic_clear_bits should produce 12, got %u",
             sys_atomic_get(&value));

  sys_atomic_init(&done, 0);
  TestAssert(launch_test_thread(atomic_worker, &ctx),
             "Failed to launch atomic worker thread");
  TestAssert(wait_for_atomic_value(&done, 1, 1000),
             "Timed out waiting for atomic worker completion");
  TestAssert(sys_atomic_get(&value) == 13,
             "Atomic worker should increment value to 13, got %u",
             sys_atomic_get(&value));

  return true;
}

TestMain(test_main)
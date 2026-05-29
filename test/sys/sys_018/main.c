#include <test.h>

typedef struct {
  sys_mem_arena_t *arena;
  uint32_t iterations;
  size_t alloc_size;
  size_t realloc_size;
  sys_atomic_t ready;
  sys_atomic_t done;
  sys_atomic_t worker_error;
} arena_thread_ctx_t;

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

static void fill_bytes(uint8_t *ptr, size_t size, uint8_t seed) {
  for (size_t index = 0; index < size; index++) {
    ptr[index] = (uint8_t)(seed + index);
  }
}

static bool check_bytes(const uint8_t *ptr, size_t size, uint8_t seed) {
  for (size_t index = 0; index < size; index++) {
    if (ptr[index] != (uint8_t)(seed + index)) {
      return false;
    }
  }
  return true;
}

static bool exercise_arena_once(sys_mem_arena_t *arena, size_t alloc_size,
                                size_t realloc_size, uint8_t seed) {
  const size_t second_size = alloc_size + 8u;
  const size_t third_size = alloc_size / 2u + 5u;

  uint8_t *first = sys_mem_arena_alloc(arena, alloc_size);
  if (first == NULL) {
    return false;
  }

  uint8_t *second = sys_mem_arena_alloc(arena, second_size);
  if (second == NULL) {
    sys_mem_arena_free(arena, first);
    return false;
  }

  uint8_t *third = sys_mem_arena_alloc(arena, third_size);
  if (third == NULL) {
    sys_mem_arena_free(arena, second);
    sys_mem_arena_free(arena, first);
    return false;
  }

  fill_bytes(first, alloc_size, seed);
  fill_bytes(second, second_size, (uint8_t)(seed + 17u));
  fill_bytes(third, third_size, (uint8_t)(seed + 33u));
  sys_sleep_ms(1);

  uint8_t *grown = sys_mem_arena_realloc(arena, second, realloc_size);
  if (grown == NULL) {
    sys_mem_arena_free(arena, third);
    sys_mem_arena_free(arena, second);
    sys_mem_arena_free(arena, first);
    return false;
  }

  if (!check_bytes(first, alloc_size, seed) ||
      !check_bytes(grown, second_size, (uint8_t)(seed + 17u)) ||
      !check_bytes(third, third_size, (uint8_t)(seed + 33u))) {
    sys_mem_arena_free(arena, third);
    sys_mem_arena_free(arena, grown);
    sys_mem_arena_free(arena, first);
    return false;
  }

  fill_bytes(grown, realloc_size, (uint8_t)(seed + 49u));
  sys_sleep_ms(1);

  if (!check_bytes(first, alloc_size, seed) ||
      !check_bytes(third, third_size, (uint8_t)(seed + 33u))) {
    sys_mem_arena_free(arena, third);
    sys_mem_arena_free(arena, grown);
    sys_mem_arena_free(arena, first);
    return false;
  }

  sys_mem_arena_free(arena, third);
  sys_mem_arena_free(arena, grown);
  sys_mem_arena_free(arena, first);
  return true;
}

static void arena_worker(void *arg) {
  arena_thread_ctx_t *ctx = (arena_thread_ctx_t *)arg;
  sys_atomic_set(&ctx->ready, 1);

  for (uint32_t index = 0; index < ctx->iterations; index++) {
    uint8_t seed = (uint8_t)(0x20u + (index & 0x1fu));
    if (!exercise_arena_once(ctx->arena, ctx->alloc_size, ctx->realloc_size,
                             seed)) {
      sys_atomic_set(&ctx->worker_error, index + 1u);
      break;
    }
  }

  sys_atomic_set(&ctx->done, 1);
}

bool test_main(void) {
  const uint32_t iterations = 64;
  const size_t alloc_size = 24;
  const size_t realloc_size = 40;
  arena_thread_ctx_t ctx;
  sys_mem_arena_stats_t stats = {0};

  sys_mem_arena_t *arena = sys_mem_arena_init(768, NULL, sys_malloc, sys_free);
  TestAssert(arena != NULL, "sys_mem_arena_init returned NULL");

  ctx.arena = arena;
  ctx.iterations = iterations;
  ctx.alloc_size = alloc_size;
  ctx.realloc_size = realloc_size;
  sys_atomic_init(&ctx.ready, 0);
  sys_atomic_init(&ctx.done, 0);
  sys_atomic_init(&ctx.worker_error, 0);

  TestAssert(launch_test_thread(arena_worker, &ctx),
             "Failed to launch arena worker thread");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "Timed out waiting for arena worker to start");

  for (uint32_t index = 0; index < iterations; index++) {
    uint8_t seed = (uint8_t)(0x80u + (index & 0x1fu));
    TestAssert(exercise_arena_once(arena, alloc_size, realloc_size, seed),
               "Main thread arena operation failed at iteration %u", index);
  }

  TestAssert(wait_for_atomic_value(&ctx.done, 1, 3000),
             "Timed out waiting for arena worker completion");
  TestAssert(sys_atomic_get(&ctx.worker_error) == 0,
             "Worker arena operation failed at iteration %u",
             sys_atomic_get(&ctx.worker_error) - 1u);

  TestAssert(sys_mem_arena_next(arena, &stats) == NULL,
             "single arena threaded test should not gain a successor");
  TestAssert(stats.used_bytes == 0,
             "threaded arena test should leave no live allocations");
  TestAssert(stats.allocations == 0,
             "threaded arena test should leave zero active allocations");

  sys_mem_arena_delete(arena);
  return true;
}

TestMain(test_main)
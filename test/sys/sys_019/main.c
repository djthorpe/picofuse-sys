#include <test.h>

typedef struct {
  sys_event_queue_t *queue;
  sys_atomic_t ready;
  sys_atomic_t done;
  sys_atomic_t event_value;
} event_queue_ctx_t;

static bool holds_events(uint32_t lhs, uint32_t rhs, uint32_t first,
                         uint32_t second) {
  return (lhs == first && rhs == second) || (lhs == second && rhs == first);
}

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

static void pop_worker(void *arg) {
  event_queue_ctx_t *ctx = (event_queue_ctx_t *)arg;

  sys_atomic_set(&ctx->ready, 1);
  sys_event_t event = sys_event_queue_pop(ctx->queue);
  sys_atomic_set(&ctx->event_value, (uint32_t)(uintptr_t)event);
  sys_atomic_set(&ctx->done, 1);
}

bool test_main(void) {
  event_queue_ctx_t ctx;
  event_queue_ctx_t ctx2;
  bool launched_second_consumer = false;
  sys_event_queue_t *queue = sys_event_queue_init(2);
  TestAssert(queue != NULL, "sys_event_queue_init returned NULL");
  TestAssert(sys_event_queue_valid(queue),
             "sys_event_queue_valid should accept a new queue");
  TestAssert(sys_event_queue_empty(queue), "new queue should report empty");
  TestAssert(sys_event_queue_size(queue) == 0,
             "new queue should start with size 0");

  TestAssert(!sys_event_queue_push(queue, NULL),
             "sys_event_queue_push should reject NULL events");
  TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)1u),
             "sys_event_queue_push should store first event");
  TestAssert(sys_event_queue_try_push(queue, (sys_event_t)(uintptr_t)2u),
             "sys_event_queue_try_push should store second event");
  TestAssert(sys_event_queue_size(queue) == 2, "queue should hold two events");

  TestAssert(sys_event_queue_lock(queue),
             "sys_event_queue_lock should succeed");
  TestAssert((uintptr_t)sys_event_queue_peek(queue) == 1u,
             "peek should return the oldest queued event");
  TestAssert(sys_event_queue_unlock(queue),
             "sys_event_queue_unlock should succeed");

  TestAssert(!sys_event_queue_try_push(queue, (sys_event_t)(uintptr_t)3u),
             "sys_event_queue_try_push should fail when the queue is full");
  TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)3u),
             "sys_event_queue_push should overwrite when the queue is full");
  TestAssert(sys_event_queue_lock(queue),
             "sys_event_queue_lock should succeed after overwrite push");
  TestAssert((uintptr_t)sys_event_queue_peek(queue) == 2u,
             "overwrite push should drop the oldest event");
  TestAssert(sys_event_queue_unlock(queue),
             "sys_event_queue_unlock should succeed after overwrite push");

  TestAssert((uintptr_t)sys_event_queue_try_pop(queue) == 2u,
             "sys_event_queue_try_pop should return the new oldest event");
  TestAssert((uintptr_t)sys_event_queue_try_pop(queue) == 3u,
             "sys_event_queue_try_pop should return the remaining event");
  TestAssert(sys_event_queue_try_pop(queue) == NULL,
             "sys_event_queue_try_pop should return NULL when empty");
  TestAssert(sys_event_queue_empty(queue),
             "queue should be empty after popping all events");

  uint64_t timeout_start = sys_timestamp_ms();
  TestAssert(sys_event_queue_timed_pop(queue, 20) == NULL,
             "timed pop should return NULL on timeout");
  TestAssert(sys_timestamp_ms() - timeout_start >= 10,
             "timed pop should wait before timing out");

  ctx.queue = queue;
  sys_atomic_init(&ctx.ready, 0);
  sys_atomic_init(&ctx.done, 0);
  sys_atomic_init(&ctx.event_value, 0);

  TestAssert(launch_test_thread(pop_worker, &ctx),
             "failed to launch blocking pop worker");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "timed out waiting for blocking pop worker readiness");
  TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)4u),
             "push should wake a blocked consumer");
  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "timed out waiting for blocking pop worker completion");
  TestAssert(sys_atomic_get(&ctx.event_value) == 4u,
             "blocking pop worker should receive the pushed event");

  ctx.queue = queue;
  ctx2.queue = queue;
  sys_atomic_set(&ctx.ready, 0);
  sys_atomic_set(&ctx.done, 0);
  sys_atomic_set(&ctx.event_value, 0);
  sys_atomic_init(&ctx2.ready, 0);
  sys_atomic_init(&ctx2.done, 0);
  sys_atomic_init(&ctx2.event_value, 0);

  TestAssert(launch_test_thread(pop_worker, &ctx),
             "failed to launch first multi-consumer worker");
  launched_second_consumer = launch_test_thread(pop_worker, &ctx2);
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "timed out waiting for first multi-consumer worker readiness");
  if (launched_second_consumer) {
    TestAssert(wait_for_atomic_value(&ctx2.ready, 1, 1000),
               "timed out waiting for second multi-consumer worker readiness");
  }

  TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)8u),
             "first multi-consumer push should succeed");
  if (launched_second_consumer) {
    TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)9u),
               "second multi-consumer push should succeed");
  }
  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "timed out waiting for first multi-consumer worker completion");
  if (launched_second_consumer) {
    TestAssert(wait_for_atomic_value(&ctx2.done, 1, 1000),
               "timed out waiting for second multi-consumer worker completion");
    TestAssert(holds_events(sys_atomic_get(&ctx.event_value),
                            sys_atomic_get(&ctx2.event_value), 8u, 9u),
               "multi-consumer workers should drain the two pushed events");
  } else {
    TestAssert(
        sys_atomic_get(&ctx.event_value) == 8u,
        "single-worker backends should still drain the first pushed event");
    TestAssert(sys_event_queue_empty(queue),
               "single-worker backends should leave the queue empty after the "
               "fallback path");
  }

  TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)5u),
             "push before shutdown should succeed");
  TestAssert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)6u),
             "second push before shutdown should succeed");
  sys_event_queue_shutdown(queue);
  TestAssert(sys_event_queue_valid(queue),
             "shutdown should not invalidate the queue object");
  TestAssert(!sys_event_queue_push(queue, (sys_event_t)(uintptr_t)7u),
             "push should fail after shutdown");
  TestAssert(!sys_event_queue_try_push(queue, (sys_event_t)(uintptr_t)7u),
             "try_push should fail after shutdown");
  TestAssert((uintptr_t)sys_event_queue_pop(queue) == 5u,
             "shutdown queue should still drain queued events");
  TestAssert((uintptr_t)sys_event_queue_pop(queue) == 6u,
             "shutdown queue should drain all queued events");
  TestAssert(sys_event_queue_pop(queue) == NULL,
             "shutdown queue should return NULL once drained");

  sys_event_queue_deinit(queue);

  queue = sys_event_queue_init(1);
  TestAssert(queue != NULL,
             "sys_event_queue_init should allow reuse after deinit");
  ctx.queue = queue;
  sys_atomic_set(&ctx.ready, 0);
  sys_atomic_set(&ctx.done, 0);
  sys_atomic_set(&ctx.event_value, 1);

  TestAssert(launch_test_thread(pop_worker, &ctx),
             "failed to launch shutdown pop worker");
  TestAssert(wait_for_atomic_value(&ctx.ready, 1, 1000),
             "timed out waiting for shutdown pop worker readiness");
  sys_event_queue_shutdown(queue);
  TestAssert(wait_for_atomic_value(&ctx.done, 1, 1000),
             "timed out waiting for shutdown pop worker completion");
  TestAssert(sys_atomic_get(&ctx.event_value) == 0,
             "blocked pop should return NULL after shutdown");

  sys_event_queue_deinit(queue);
  return true;
}

TestMain(test_main)
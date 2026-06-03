/**
 * @file
 * @brief Queue example.
 */

#include <picofuse/sys.h>

typedef struct {
  sys_event_queue_t *queue; /**< Queue shared by producer and consumer. */
  sys_waitgroup_t *wg;      /**< Waitgroup used to join the worker. */
} queue_example_ctx_t;

/**
 * @brief Consume events from the queue until shutdown is requested.
 *
 * The worker blocks on @c sys_event_queue_pop, prints each event ID, then
 * shuts the queue down once enough events have been received.
 */
static void consumer_worker(void *arg) {
  queue_example_ctx_t *ctx = (queue_example_ctx_t *)arg;
  uint32_t consumed = 0;

  sys_printf("consumer: running on core %u\n", sys_thread_core());

  while (consumed < 10u) {
    sys_event_t event = sys_event_queue_pop(ctx->queue);
    if (event == NULL) {
      break;
    }

    consumed++;
    sys_printf("consumer: event %u on core %u\n", (uint32_t)(uintptr_t)event,
               sys_thread_core());
  }

  sys_printf("consumer: consumed %u events, shutting down queue\n", consumed);
  sys_event_queue_shutdown(ctx->queue);
  sys_assert(sys_waitgroup_done(ctx->wg));
}

/**
 * @brief Launch the queue consumer on core 1 when available.
 *
 * This mirrors the multicore example and shows how the queue worker can be
 * pinned to a specific core or created normally on single-core targets.
 */
static bool launch_consumer(queue_example_ctx_t *ctx) {
  if (sys_thread_numcores() > 1 &&
      sys_thread_create_on_core(consumer_worker, ctx, 1)) {
    return true;
  }

  return sys_thread_create(consumer_worker, ctx);
}

/**
 * @brief Push numbered events into a queue and let a worker consume them.
 *
 * The example shows queue creation, waitgroup coordination, event pushing,
 * blocking consumption, and clean shutdown of the queue from the consumer.
 */
int main(void) {
  sys_init();

  sys_event_queue_t *queue = sys_event_queue_init(10);
  sys_waitgroup_t *wg = sys_waitgroup_init();
  queue_example_ctx_t ctx = {.queue = queue, .wg = wg};

  sys_assert(queue != NULL);
  sys_assert(wg != NULL);
  sys_assert(sys_waitgroup_add(wg, 1));
  sys_assert(launch_consumer(&ctx));

  sys_printf("producer: running on core %u\n", sys_thread_core());

  for (uint32_t event_id = 1; event_id <= 10u; event_id++) {
    uint32_t delay_ms = (sys_random_uint32() % 100u) + 1u;
    sys_printf("producer: event %u on core %u\n", event_id, sys_thread_core());
    sys_assert(sys_event_queue_push(queue, (sys_event_t)(uintptr_t)event_id));
    sys_sleep_ms(delay_ms);
  }

  sys_waitgroup_wait(wg);
  sys_event_queue_deinit(queue);

  sys_printf("queue example complete\n");
  sys_exit();
  return 0;
}
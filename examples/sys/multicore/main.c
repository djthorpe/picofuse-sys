/**
 * @file
 * @brief Multicore example.
 */

#include <picofuse/sys.h>

/**
 * @brief Print a short burst of messages from the current core.
 *
 * The example uses @c sys_thread_core to identify the executing CPU and
 * @c sys_sleep_ms with a random delay to make the interleaving visible.
 */
static void print_messages(const char *role) {
  for (uint32_t index = 0; index < 10; index++) {
    sys_printf("%s message %u on core %u\n", role, index + 1,
               sys_thread_core());
    sys_sleep_ms(sys_random_uint32() % 11);
  }
}

/**
 * @brief Worker routine that prints messages and signals completion.
 *
 * The worker receives the waitgroup handle, runs the shared message printer,
 * and then calls @c sys_waitgroup_done so the main thread can wait for it.
 */
static void multicore_worker(void *arg) {
  sys_waitgroup_t *wg = (sys_waitgroup_t *)arg;
  print_messages("worker");
  sys_waitgroup_done(wg);
}

/**
 * @brief Launch the worker on core 1 when possible, otherwise on any core.
 *
 * This shows the difference between @c sys_thread_create_on_core and the
 * fallback @c sys_thread_create path on single-core targets.
 */
static bool launch_multicore_worker(sys_waitgroup_t *wg) {
  if (sys_thread_numcores() > 1 &&
      sys_thread_create_on_core(multicore_worker, wg, 1)) {
    return true;
  }

  return sys_thread_create(multicore_worker, wg);
}

/**
 * @brief Launch work on both the current core and a secondary worker core.
 *
 * The example creates a waitgroup, starts the worker, prints messages from the
 * main thread, and waits until both sides finish before shutting down.
 */
int main(void) {
  sys_init();

  // Create a waitgroup and launch a function on core 1
  sys_waitgroup_t *wg = sys_waitgroup_init();
  sys_assert(wg != NULL);
  sys_assert(sys_waitgroup_add(wg, 1));
  sys_assert(launch_multicore_worker(wg));

  print_messages("main");

  sys_waitgroup_wait(wg);

  sys_printf("Shutting down\n");
  sys_exit();
}

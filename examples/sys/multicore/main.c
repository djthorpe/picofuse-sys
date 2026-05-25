#include <picofuse/sys.h>

static void print_messages(const char *role) {
  for (uint32_t index = 0; index < 10; index++) {
    sys_printf("%s message %u on core %u\n", role, index + 1,
               sys_thread_core());
    sys_sleep_ms(sys_random_uint32() % 11);
  }
}

static void multicore_worker(void *arg) {
  sys_waitgroup_t *wg = (sys_waitgroup_t *)arg;
  print_messages("worker");
  sys_waitgroup_done(wg);
}

static bool launch_multicore_worker(sys_waitgroup_t *wg) {
  if (sys_thread_numcores() > 1 &&
      sys_thread_create_on_core(multicore_worker, wg, 1)) {
    return true;
  }

  return sys_thread_create(multicore_worker, wg);
}

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

#include <test.h>

bool test_main(void) {
  uint8_t num_cores = sys_thread_numcores();
  uint8_t current_core = sys_thread_core();

  TestAssert(num_cores >= 1,
             "sys_thread_numcores should report at least one core, got %u",
             num_cores);
  TestAssert(current_core < num_cores,
             "sys_thread_core should be less than sys_thread_numcores: core=%u "
             "cores=%u",
             current_core, num_cores);

#ifdef SYSTEM_NAME_PICO
  TestAssert(num_cores == 2, "Pico should report exactly two cores, got %u",
             num_cores);
#endif

  return true;
}

TestMain(test_main)
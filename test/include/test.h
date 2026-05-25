
#pragma once

#include <inttypes.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <stdbool.h>

#ifndef PICOFUSE_TEST_NAME
#define PICOFUSE_TEST_NAME "unknown"
#endif

#define TestAssert(COND, ...)                                                  \
  do {                                                                         \
    if (!(COND)) {                                                             \
      sys_printf("Assertion failed: ");                                        \
      sys_printf(__VA_ARGS__);                                                 \
      sys_printf("\n");                                                        \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TestMain(ENTRYPOINT)                                                   \
  bool ENTRYPOINT(void);                                                       \
  int main(void) {                                                             \
    sys_init();                                                                \
    hw_init();                                                                 \
    uint64_t start_time_ms = sys_timestamp_ms();                               \
    bool success = ENTRYPOINT();                                               \
    uint64_t elapsed_time_ms = sys_timestamp_ms() - start_time_ms;             \
    sys_printf("TEST %s (%s) in %" PRIu64 " ms\n", success ? "PASS" : "FAIL",  \
               PICOFUSE_TEST_NAME, elapsed_time_ms);                           \
    hw_exit();                                                                 \
    sys_exit();                                                                \
    return success ? 0 : 1;                                                    \
  }

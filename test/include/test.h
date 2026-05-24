
#pragma once

#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <stdbool.h>

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
    bool success = ENTRYPOINT();                                               \
    hw_exit();                                                                 \
    sys_exit();                                                                \
    return success ? 0 : 1;                                                    \
  }
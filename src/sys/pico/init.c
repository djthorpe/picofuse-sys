#include "../printf/private.h"
#include "private.h"
#include <pico/stdlib.h>
#include <picofuse/sys.h>
#include <stdbool.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  _sys_mutex_module_init();
  _sys_cond_module_init();
  _sys_waitgroup_module_init();
  _sys_printf_module_init();
  stdio_init_all();
  sys_sleep_ms(1000);
  sys_timestamp_ms();
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  _sys_printf_module_deinit();
  while (true) {
    sys_sleep_ms(1000);
  }
}

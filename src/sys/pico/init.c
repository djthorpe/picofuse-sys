#include "private.h"
#include <pico/stdlib.h>
#include <picofuse/sys.h>
#include <stdbool.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  sys_pico_mutex_module_init();
  sys_pico_cond_module_init();
  stdio_init_all();
  sys_sleep_ms(1000);
  sys_timestamp_ms();
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  while (true) {
    sys_sleep_ms(1000);
  }
}

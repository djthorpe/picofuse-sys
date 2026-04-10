#include <pico/stdlib.h>
#include <picofuse/sys.h>
#include <stdbool.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  stdio_init_all();
  sys_sleep_ms(1000);
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  while (true) {
    sys_sleep_ms(1000);
  }
}

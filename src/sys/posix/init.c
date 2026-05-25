#include "../printf/private.h"
#include <picofuse/sys.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  _sys_printf_module_init();
  sys_timestamp_ms();
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) { _sys_printf_module_deinit(); }

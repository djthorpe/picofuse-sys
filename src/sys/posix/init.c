#include "../any/private.h"
#include "../printf/private.h"
#include <picofuse/sys.h>
#include <stdlib.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  sys_assert(_sys_mem_init(SYS_MEM_CAPACITY, malloc, free));
  _sys_printf_module_init();
  sys_timestamp_ms();
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  _sys_mem_deinit();
  _sys_printf_module_deinit();
}

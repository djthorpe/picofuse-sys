#include "../any/private.h"
#include "../printf/private.h"
#include <picofuse/sys.h>
#include <stdlib.h>

extern void _sys_timer_module_exit(void);

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  sys_assert(_sys_mem_init(SYS_MEM_CAPACITY, malloc, free));
  _sys_printf_module_init();
#ifndef NDEBUG
  _sys_debugf_module_init();
#endif
  sys_timestamp_ms();
  sys_debugf("[sys] init: sys=%s name=%s version=%s)", sys_env_system(),
             sys_env_name(), sys_env_version());
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  _sys_timer_module_exit();
  _sys_mem_module_exit();
  sys_debugf("[sys] exit");
#ifndef NDEBUG
  _sys_debugf_module_exit();
#endif
  _sys_printf_module_exit();
}

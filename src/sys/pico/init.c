#include "../any/private.h"
#include "../printf/private.h"
#include "date.h"
#include "private.h"
#include <mbedtls/platform.h>
#include <pico/stdlib.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  int mbedtls_result = mbedtls_platform_set_calloc_free(sys_calloc, sys_free);
  sys_assert(mbedtls_result == 0);
  _sys_mutex_module_init();
  sys_assert(_sys_mem_init(SYS_MEM_CAPACITY, malloc, free));
  _sys_date_init();
  _sys_cond_module_init();
  _sys_hash_module_init();
  _sys_waitgroup_module_init();
  _sys_timer_module_init();
  _sys_printf_module_init();
  stdio_init_all();
  sys_sleep_ms(1000);
  sys_timestamp_ms();
  sys_debugf("[sys] init sys=%s name=%s version=%s", sys_env_system(),
             sys_env_name(), sys_env_version());
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  _sys_timer_module_exit();
  _sys_date_module_exit();
  _sys_mem_module_exit();
  sys_debugf("[sys] exit");
  _sys_printf_module_exit();
  sys_halt();
}

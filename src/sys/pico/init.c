#include "../any/private.h"
#include "../printf/private.h"
#include "date.h"
#include "mbedtls_config.h"
#include "private.h"
#include <mbedtls/platform.h>
#include <mbedtls/platform_time.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdlib.h>

mbedtls_ms_time_t mbedtls_ms_time(void) { return time_us_64() / 1000; }

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  sys_assert(mbedtls_platform_set_calloc_free(sys_calloc, sys_free) == 0);
  _sys_mutex_module_init();
  sys_assert(_sys_mem_init(SYS_MEM_CAPACITY, malloc, free));
  _sys_date_init();
  _sys_cond_module_init();
  _sys_event_queue_module_init();
  _sys_hash_module_init();
  _sys_waitgroup_module_init();
  _sys_timer_module_init();
  _sys_printf_module_init();
#ifndef NDEBUG
  _sys_debugf_module_init();
#endif
  stdio_init_all();
  sys_sleep_ms(1000);
  sys_timestamp_ms();
  sys_debugf("sys", "init sys=%s name=%s version=%s", sys_env_system(),
             sys_env_name(), sys_env_version());
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  _sys_timer_module_exit();
  _sys_date_module_exit();
  _sys_mem_module_exit();
  sys_debugf("sys", "exit");
#ifndef NDEBUG
  _sys_debugf_module_exit();
#endif
  _sys_printf_module_exit();
  sys_halt();
}

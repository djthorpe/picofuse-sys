#include <picofuse/hw.h>
#include <picofuse/sys.h>

void _hw_watchdog_module_init(void);
void _hw_watchdog_module_exit(void);
void _hw_watchdog_poll(void);

/**
 * @brief Initializes the hardware system on startup.
 */
void hw_init(void) {
  sys_debugf("hw_init()\n");
  _hw_watchdog_module_init();
}

/**
 * @brief Cleans up the hardware system on shutdown.
 */
void hw_exit(void) {
  _hw_watchdog_module_exit();
  sys_debugf("hw_exit()\n");
}

/**
 * @brief Occasional polling function for the hardware system.
 */
void hw_poll(void) { _hw_watchdog_poll(); }

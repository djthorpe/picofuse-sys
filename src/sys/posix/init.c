#include <picofuse/sys.h>

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void) {
  sys_timestamp_ms(); // Initialize timestamp
}

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void) {
  // No-op stub implementation for unsupported platforms.
}

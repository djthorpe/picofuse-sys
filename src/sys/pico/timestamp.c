#include <pico/time.h>

/**
 * @brief Gets the number of milliseconds since the process launched.
 */
uint64_t sys_timestamp_ms(void) {
  absolute_time_t now = get_absolute_time();
  static uint64_t process_start_time_ms = 0;
  uint64_t current_time_ms = to_us_since_boot(now) / 1000;

  if (process_start_time_ms == 0) {
    process_start_time_ms = current_time_ms;
  }

  return current_time_ms - process_start_time_ms;
}

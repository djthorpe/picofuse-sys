#include <pico/time.h>

/**
 * @brief Gets the number of milliseconds since the process launched.
 */
uint64_t sys_timestamp_ms(void) {
  absolute_time_t now = get_absolute_time();
  return to_us_since_boot(now) / 1000;
}

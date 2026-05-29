#include "date.h"
#include <pico/sync.h>
#include <pico/time.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static int64_t _sys_date_offset_seconds = 0;
static int32_t _sys_date_tzoffset_seconds = 0;
static mutex_t _sys_date_mutex;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/**
 * @brief Return milliseconds since boot.
 * @return Milliseconds since boot.
 */
static int64_t _sys_date_now_ms(void) {
  absolute_time_t now = get_absolute_time();
  return (int64_t)(to_us_since_boot(now) / 1000u);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS

/**
 * @brief Initializes the date module.
 */
void _sys_date_init() {
  _sys_date_offset_seconds = 0;
  _sys_date_tzoffset_seconds = 0;
  mutex_init(&_sys_date_mutex);
}

/**
 * @brief Exits the date module.
 */
void _sys_date_module_exit() {
  _sys_date_offset_seconds = 0;
  _sys_date_tzoffset_seconds = 0;
}

/**
 * @brief Gets the current system time.
 * @param date Receives the current system date and time.
 * @return `true` on success, `false` on invalid arguments.
 */
bool sys_date_get_now(sys_date_t *date) {
  if (date == NULL) {
    return false;
  }

  mutex_enter_blocking(&_sys_date_mutex);

  int64_t ms = _sys_date_now_ms() + (_sys_date_offset_seconds * 1000);
  date->seconds = ms / 1000;
  date->nanoseconds = (int32_t)(ms % 1000) * 1000000;
  date->tzoffset = _sys_date_tzoffset_seconds;

  mutex_exit(&_sys_date_mutex);
  return true;
}

/**
 * @brief Sets the current system time.
 * @param date Desired system date and time.
 * @return `true` on success, `false` on invalid arguments.
 */
bool sys_date_set_now(const sys_date_t *date) {
  if (date == NULL || date->nanoseconds < 0 ||
      date->nanoseconds >= 1000000000) {
    return false;
  }

  mutex_enter_blocking(&_sys_date_mutex);

  int64_t current_ms = _sys_date_now_ms();
  int64_t desired_ms = (date->seconds * 1000) + (date->nanoseconds / 1000000);
  _sys_date_offset_seconds = (desired_ms - current_ms) / 1000;
  _sys_date_tzoffset_seconds = date->tzoffset;

  mutex_exit(&_sys_date_mutex);
  return true;
}
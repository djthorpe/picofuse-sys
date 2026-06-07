/**
 * @file sys/timestamp.h
 * @brief Defines timestamp, date, and time APIs.
 * @defgroup SystemTime Date and Time Operations
 * @ingroup System
 * @details
 * The SystemTime module provides monotonic timing and wall-clock date/time
 * operations for scheduling, time measurement, and timestamped application
 * logic.
 *
 * It includes two complementary concepts:
 * - Monotonic process time via `sys_timestamp_ms()` for elapsed-time
 *   measurement and interval checks.
 * - Wall-clock date/time APIs (see `sys/date.h`) for calendar/timezone-aware
 *   values, formatting inputs, and system clock updates.
 *
 * Guidance:
 * - Prefer `sys_timestamp_ms()` for durations/timeouts; it is intended for
 *   relative timing.
 * - Use `sys_date_*` APIs when you need real date/time values (UTC/local,
 *   year/month/day/hour/minute/second).
 * - Avoid mixing wall-clock time with interval math unless that behavior is
 *   explicitly required.
 *
 * Typical flow:
 * 1. Capture start time with `sys_timestamp_ms()`.
 * 2. Run work and compute elapsed duration from subsequent timestamp reads.
 * 3. Query `sys_date_get_now()` or component helpers when wall-clock output is
 *    needed.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Gets the number of milliseconds since the process launched.
 * @ingroup SystemTime
 */
uint64_t sys_timestamp_ms(void);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

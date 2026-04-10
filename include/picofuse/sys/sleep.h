/**
 * @file sys/sleep.h
 * @brief Defines the `sys_sleep` function for sleeping.
 * @details This file provides the declaration of the `sys_sleep` function,
 * which can be used to pause execution for a specified duration.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Pauses execution for a specified duration.
 * @ingroup SystemTiming
 * @param ms The number of milliseconds to sleep.
 */
void sys_sleep_ms(uint32_t ms);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

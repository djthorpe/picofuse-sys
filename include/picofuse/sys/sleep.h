/**
 * @file sleep.h
 * @defgroup SystemSleep Sleep Operations
 * @ingroup System
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
 * @brief Pauses the current thread for a specified duration.
 * @ingroup SystemSleep
 * @param ms The number of milliseconds to sleep.
 * @details This function blocks only the calling thread for approximately the
 * requested duration.
 */
void sys_sleep_ms(uint32_t ms);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

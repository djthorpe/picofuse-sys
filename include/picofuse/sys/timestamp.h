/**
 * @file timestamp.h
 * @defgroup SystemTime Time Operations
 * @ingroup System
 * @brief Defines the `sys_timestamp_ms` function for getting the current
 * timestamp.
 * @details This file provides the declaration of the `sys_timestamp_ms`
 * function, which can be used to get the current timestamp in milliseconds.
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

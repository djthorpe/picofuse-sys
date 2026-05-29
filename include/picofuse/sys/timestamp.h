/**
 * @file sys/timestamp.h
 * @brief Defines timestamp, date, and time APIs.
 * @defgroup SystemTime Date and Time Operations
 * @ingroup System
 * @details This module declares timestamp retrieval and wall-clock date/time
 * APIs.
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

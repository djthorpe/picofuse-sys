/**
 * @file sys/timestamp.h
 * @brief Defines timestamp retrieval APIs.
 * @defgroup SystemTime Time Operations
 * @ingroup System
 * @details This file declares functions for retrieving the current timestamp
 * in milliseconds.
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

/**
 * @file sys/panicf.h
 * @ingroup System
 * @brief Defines the `sys_panicf` function for formatted panic messages.
 * @details This file provides the declaration of the `sys_panicf` function,
 * which can be used to print formatted panic messages and halt the system.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////

void sys_panicf(const char *format, ...);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

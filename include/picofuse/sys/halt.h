/**
 * @file sys/halt.h
 * @ingroup System
 * @brief Defines the `sys_halt` function.
 * @details This file provides a platform-specific primitive that stops normal
 * execution and never returns.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////

_Noreturn void sys_halt(void);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
/**
 * @file sys/halt.h
 * @brief Defines the `sys_halt` function.
 * @details This file provides a platform-specific primitive that stops normal
 * execution and never returns.
 */

#pragma once

_Noreturn void sys_halt(void);
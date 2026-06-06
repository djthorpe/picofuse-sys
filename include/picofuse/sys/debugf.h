/**
 * @file sys/debugf.h
 * @brief Debug logging helpers for system output.
 * @defgroup SystemDebug Debug Output
 * @ingroup System
 */
#pragma once

#include "printf.h"

/**
 * @brief Debug-only formatted logging helper.
 * @ingroup SystemDebug
 *
 * Emits logs only when NDEBUG is not defined at compile time.
 */
#ifndef NDEBUG
void _sys_debugf_impl(const char *format, ...);
#define sys_debugf(...) _sys_debugf_impl(__VA_ARGS__)
#else
#define sys_debugf(...) ((void)0)
#endif

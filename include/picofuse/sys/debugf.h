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
 * Emits logs only when DEBUG is defined at compile time.
 */
#if defined(DEBUG)
static inline void _sys_debugf_impl(const char *format, ...) {
  va_list args;
  sys_printf("[DEBUG] ");
  va_start(args, format);
  (void)sys_vprintf(format, args);
  va_end(args);
  sys_printf("\\n");
}

#define sys_debugf(...) _sys_debugf_impl(__VA_ARGS__)
#else
#define sys_debugf(...) ((void)0)
#endif

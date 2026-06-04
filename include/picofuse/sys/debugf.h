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
#define sys_debugf(format, ...)                                                \
  sys_printf("[DEBUG] " format "\\n", ##__VA_ARGS__)
#else
#define sys_debugf(...) ((void)0)
#endif

/**
 * @file sys/printf.h
 * @ingroup System
 * @brief Defines the `sys_printf` function for formatted output.
 * @details This file provides the declaration of the `sys_printf` function,
 * which can be used to print formatted messages to the system output.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Prints formatted output to the system console.
 * @ingroup SystemFormatting
 * @param format A printf-style format string that specifies how subsequent
 *               arguments are formatted and printed.
 * @param ... Additional arguments corresponding to format specifiers in format.
 * @return The number of characters printed.
 */
size_t sys_printf(const char *format, ...);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

/**
 * @file sys/mem.h
 * @brief Defines memory-related functions and macros.
 * @details This file provides functions and macros for memory management
 * and manipulation.
 */

#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Fill a block of memory with a byte value.
 * @ingroup SystemMemory
 *
 * Writes the low 8 bits of `value` into each of the first `count` bytes of the
 * memory region starting at `dest`.
 *
 * @param dest Pointer to the destination memory region.
 * @param value Byte value to write.
 * @param count Number of bytes to set.
 * @return The original `dest` pointer.
 */
void *sys_memset(void *dest, int value, size_t count);

/**
 * @brief Copy a block of memory.
 * @ingroup SystemMemory
 *
 * Copies `count` bytes from the memory region pointed to by `src` into the
 * memory region pointed to by `dest`.
 *
 * @param dest Pointer to the destination memory region.
 * @param src Pointer to the source memory region.
 * @param count Number of bytes to copy.
 * @return The original `dest` pointer.
 */
void *sys_memcpy(void *dest, const void *src, size_t count);

/**
 * @brief Compute the length of a NULL-terminated string.
 * @ingroup SystemMemory
 *
 * Counts the number of characters in `str` before the terminating `\0`
 * character.
 *
 * @param str Pointer to the string to measure.
 * @return Number of characters preceding the terminating `\0`.
 */
size_t sys_strlen(const char *str);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

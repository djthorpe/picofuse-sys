/**
 * @file sys/mem.h
 * @brief Defines heap and string utility functions.
 * @defgroup SystemMemory Memory Operations
 * @ingroup System
 * @details
 * The SystemMemory module provides core byte/string helpers and default heap
 * allocation wrappers used across the runtime.
 *
 * It includes:
 * - Byte-oriented memory primitives (`sys_memset`, `sys_memcpy`,
 *   `sys_memcmp`).
 * - String-length helper (`sys_strlen`).
 * - Heap lifecycle APIs (`sys_malloc`, `sys_calloc`, `sys_realloc`,
 *   `sys_free`).
 *
 * Allocation wrappers are intentionally portable and may be backed by
 * platform allocators or arena-based implementations depending on target
 * configuration.
 *
 * Usage notes:
 * - `sys_calloc` returns zero-initialized memory.
 * - `sys_realloc(NULL, size)` behaves like `sys_malloc(size)`.
 * - `sys_free(NULL)` is a no-op.
 *
 * Typical flow:
 * 1. Allocate using `sys_malloc` or `sys_calloc`.
 * 2. Use memory utilities for initialization/copy/compare operations.
 * 3. Resize with `sys_realloc` when needed.
 * 4. Release with `sys_free`.
 */

#pragma once
#include "arena.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// MEMORY OPERATIONS

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
 * memory region pointed to by `dest`. Overlapping regions are handled safely.
 *
 * @param dest Pointer to the destination memory region.
 * @param src Pointer to the source memory region.
 * @param count Number of bytes to copy.
 * @return The original `dest` pointer.
 */
void *sys_memcpy(void *dest, const void *src, size_t count);

/**
 * @brief Compare two memory regions byte-by-byte.
 * @ingroup SystemMemory
 *
 * Compares the first `count` bytes of the memory regions pointed to by `lhs`
 * and `rhs`.
 *
 * @param lhs Pointer to the first memory region.
 * @param rhs Pointer to the second memory region.
 * @param count Number of bytes to compare.
 * @return Negative if `lhs` is less than `rhs`, zero if equal, positive if
 * `lhs` is greater than `rhs`.
 */
int sys_memcmp(const void *lhs, const void *rhs, size_t count);

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

/**
 * @brief Allocate an uninitialized block of memory.
 * @ingroup SystemMemory
 *
 * Reserves `size` bytes and returns a pointer to the allocated block, or NULL
 * when the allocation fails.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block, or NULL on failure.
 */
void *sys_malloc(size_t size);

/**
 * @brief Allocate zero-initialized memory for an array.
 * @ingroup SystemMemory
 *
 * Allocates space for `count` elements of `size` bytes each and initializes
 * the allocation to zero.
 *
 * @param count Number of elements to allocate.
 * @param size Size in bytes of each element.
 * @return Pointer to the allocated block, or NULL on failure.
 */
void *sys_calloc(size_t count, size_t size);

/**
 * @brief Resize a previously allocated block of memory.
 * @ingroup SystemMemory
 *
 * Changes the size of the allocation referenced by `ptr` to `size` bytes.
 * Passing NULL behaves like `sys_malloc(size)`.
 *
 * @param ptr Existing allocation to resize, or NULL.
 * @param size New size in bytes.
 * @return Pointer to the resized block, or NULL on failure.
 */
void *sys_realloc(void *ptr, size_t size);

/**
 * @brief Release a previously allocated block of memory.
 * @ingroup SystemMemory
 *
 * Frees the allocation referenced by `ptr`. Passing NULL has no effect.
 *
 * @param ptr Allocation to release, or NULL.
 */
void sys_free(void *ptr);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

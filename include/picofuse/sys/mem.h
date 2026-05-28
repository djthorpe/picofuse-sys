/**
 * @file sys/mem.h
 * @brief Defines memory and string utility functions.
 * @defgroup SystemMemory Memory Operations
 * @ingroup System
 * @details This file provides functions for memory management and byte-string
 * manipulation.
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Arena allocator handle.
 * @ingroup SystemMemory
 * @headerfile mem.h picofuse/sys.h
 */
typedef struct sys_mem_arena_t sys_mem_arena_t;

/**
 * @brief Snapshot of arena usage statistics.
 * @ingroup SystemMemory
 */
typedef struct sys_mem_arena_stats_t {
  size_t size_bytes;
  size_t used_bytes;
  size_t allocations;
} sys_mem_arena_stats_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Arena Lifecycle
 * @{ */

/**
 * @brief Initialize a new arena.
 * @ingroup SystemMemory
 * @param size Arena size in bytes.
 * @param prev Pointer to the previous arena, or `NULL` for the first arena.
 * @param malloc_fn Underlying allocation function used when `prev` is `NULL`.
 * @param free_fn Underlying deallocation function used when `prev` is `NULL`.
 * @return Pointer to the newly initialized arena, or `NULL` on failure.
 *
 * When creating the first arena in a chain, `malloc_fn` and `free_fn` must
 * point to the underlying allocator implementation to use. For subsequent
 * arenas, pass `NULL` for both callbacks; providing either callback when
 * `prev` is not `NULL` is an assertion failure.
 */
sys_mem_arena_t *sys_mem_arena_init(size_t size, sys_mem_arena_t *prev,
                                    void *(*malloc_fn)(size_t size),
                                    void (*free_fn)(void *ptr));

/**
 * @brief Delete an arena chain.
 * @ingroup SystemMemory
 * @param arena Pointer to an arena in the chain. Must be non-NULL.
 *
 * Deletes the chain beginning at the head arena associated with `arena`.
 */
void sys_mem_arena_delete(sys_mem_arena_t *arena);

/**
 * @brief Return the next arena in a chain.
 * @ingroup SystemMemory
 * @param arena Pointer to the current arena.
 * @param stats Optional pointer populated with stats for `arena` when non-NULL.
 * @return Pointer to the next arena, or `NULL` when the chain ends.
 */
sys_mem_arena_t *sys_mem_arena_next(sys_mem_arena_t *arena,
                                    sys_mem_arena_stats_t *stats);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/** @name Single-Arena Allocation
 * @{ */

/**
 * @brief Allocate memory from a single arena.
 * @ingroup SystemMemory
 * @param arena Arena that services the allocation request.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block, or `NULL` on failure.
 *
 * This function operates only on the supplied arena and does not traverse any
 * linked successor arenas.
 */
void *sys_mem_arena_alloc(sys_mem_arena_t *arena, size_t size);

/**
 * @brief Resize an allocation within a single arena.
 * @ingroup SystemMemory
 * @param arena Arena that owns the allocation.
 * @param ptr Existing allocation to resize, or `NULL`.
 * @param size New size in bytes.
 * @return Pointer to the resized block, or `NULL` on failure.
 *
 * This function operates only on the supplied arena and does not traverse any
 * linked successor arenas.
 */
void *sys_mem_arena_realloc(sys_mem_arena_t *arena, void *ptr, size_t size);

/**
 * @brief Release an allocation owned by a single arena.
 * @ingroup SystemMemory
 * @param arena Arena that owns the allocation.
 * @param ptr Allocation to release, or `NULL`.
 *
 * This function operates only on the supplied arena and does not traverse any
 * linked successor arenas.
 */
void sys_mem_arena_free(sys_mem_arena_t *arena, void *ptr);

/** @} */

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Allocator event kinds captured in the debug ring buffer.
 * @ingroup SystemMemory
 */
typedef enum sys_mem_event_type_t {
  sys_mem_event_malloc = 1,
  sys_mem_event_calloc,
  sys_mem_event_realloc,
  sys_mem_event_free,
} sys_mem_event_type_t;

/**
 * @brief Snapshot of aggregate allocator debug counters.
 * @ingroup SystemMemory
 */
typedef struct sys_mem_stats_t {
  size_t malloc_calls;
  size_t calloc_calls;
  size_t realloc_calls;
  size_t free_calls;
  size_t failed_allocations;
  size_t requested_bytes;
} sys_mem_stats_t;

/**
 * @brief Snapshot of a single allocator debug event.
 * @ingroup SystemMemory
 */
typedef struct sys_mem_event_t {
  size_t sequence;
  sys_mem_event_type_t type;
  uintptr_t old_ptr;
  uintptr_t new_ptr;
  size_t size;
  size_t count;
} sys_mem_event_t;

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
 * memory region pointed to by `dest`.
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

///////////////////////////////////////////////////////////////////////////////
// HEAP ALLOCATION

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

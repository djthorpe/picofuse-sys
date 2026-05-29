/**
 * @file sys/arena.h
 * @brief Defines arena allocator types and operations.
 * @defgroup SystemMemory Memory Operations
 * @ingroup System
 */

#pragma once
#include <stddef.h>

/**
 * @brief Default capacity for the process-wide arena chain.
 * @ingroup SystemMemory
 */
#ifndef SYS_MEM_CAPACITY
#define SYS_MEM_CAPACITY ((size_t)(32u * 1024u))
#endif

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Arena allocator handle.
 * @ingroup SystemMemory
 * @headerfile arena.h picofuse/sys.h
 */
typedef struct sys_mem_arena_t sys_mem_arena_t;

/**
 * @brief Snapshot of arena usage statistics.
 * @ingroup SystemMemory
 * @headerfile arena.h picofuse/sys.h
 */
typedef struct sys_mem_arena_stats_t {
  size_t size_bytes;  ///< Total payload capacity of the arena in bytes.
  size_t used_bytes;  ///< Number of payload bytes currently allocated.
  size_t allocations; ///< Number of active allocations in the arena.
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
 * arenas, `prev` must point to the current tail arena. In that case the
 * allocator callbacks are inherited from `prev`, and initialization returns
 * `NULL` if `prev` is not the tail or does not have valid allocator callbacks.
 */
sys_mem_arena_t *sys_mem_arena_init(size_t size, sys_mem_arena_t *prev,
                                    void *(*malloc_fn)(size_t size),
                                    void (*free_fn)(void *ptr));

/**
 * @brief Delete a single arena.
 * @ingroup SystemMemory
 * @param arena Pointer to an arena in the chain. Must be non-NULL.
 *
 * Removes `arena` from its chain and releases only that arena.
 *
 * If `arena` is the current head and a successor exists, the successor becomes
 * the new head for the remaining chain.
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

/** @name Arena Allocation
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

#ifdef __cplusplus
}
#endif
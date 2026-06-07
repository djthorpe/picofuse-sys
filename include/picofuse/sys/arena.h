/**
 * @file sys/arena.h
 * @brief Defines arena allocator types and operations.
 * @ingroup SystemMemory
 * @details
 * The arena API provides region-oriented allocation primitives used by the
 * default memory wrappers and by callers that need deterministic allocation
 * behavior.
 *
 * Arenas can be chained to grow capacity incrementally while preserving
 * locality and ownership boundaries. Allocation/reallocation/free operations
 * in this header operate on the supplied arena only; they do not traverse
 * successor arenas unless explicitly done by higher-level logic.
 *
 * Typical flow:
 * 1. Create an arena with `sys_mem_arena_init(...)`.
 * 2. Allocate/reallocate/free within that arena.
 * 3. Optionally walk the chain with `sys_mem_arena_next(...)`.
 * 4. Dump usage with `sys_mem_dump(...)` when debugging.
 * 5. Delete arenas with `sys_mem_arena_delete(...)`.
 */

/**
 * @defgroup SystemArenas Arena Allocators
 * @ingroup SystemMemory
 * @details
 * Arena allocators provide bounded, chainable memory regions with usage
 * introspection. They are useful when you need predictable allocation domains,
 * custom growth policies, or diagnostics separate from general-purpose heaps.
 */

#pragma once
#include <stddef.h>

/**
 * @brief Default capacity for the process-wide arena chain.
 * @ingroup SystemArenas
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
 * @ingroup SystemArenas
 * @headerfile arena.h picofuse/sys.h
 */
typedef struct sys_mem_arena_t sys_mem_arena_t;

/**
 * @brief Snapshot of arena usage statistics.
 * @ingroup SystemArenas
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
 * @ingroup SystemArenas
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
 * @ingroup SystemArenas
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
 * @ingroup SystemArenas
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
 * @brief Print arena-chain usage statistics.
 * @ingroup SystemArenas
 *
 * Walks the arena chain starting at `arena` and prints one line of usage
 * statistics per arena via `sys_printf`.
 *
 * Passing `NULL` reports statistics for the default arena chain managed by
 * the global heap wrappers.
 *
 * @param arena First arena in the chain to dump, or `NULL` for the default
 *              arena chain.
 */
void sys_mem_dump(sys_mem_arena_t *arena);

/**
 * @brief Allocate memory from a single arena.
 * @ingroup SystemArenas
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
 * @ingroup SystemArenas
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
 * @ingroup SystemArenas
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
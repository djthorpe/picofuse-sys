#include <picofuse/sys.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_mem_arena_t {
  sys_mem_arena_t *head;
  sys_mem_arena_t *next;
  sys_mutex_t *lock;
  void *(*malloc_fn)(size_t size);
  void (*free_fn)(void *ptr);
  size_t size;
  size_t used_bytes;
  size_t allocations;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Locks the head mutex for an arena chain. */
static bool _sys_mem_arena_lock(sys_mem_arena_t *arena) {
  sys_assert(arena != NULL);
  if (arena == NULL || arena->head == NULL) {
    return false;
  }

  sys_mutex_t *lock = arena->head->lock;
  if (lock == NULL) {
    return false;
  }

  return sys_mutex_lock(lock);
}

/** @brief Unlocks the head mutex for an arena chain. */
static void _sys_mem_arena_unlock(sys_mem_arena_t *arena) {
  sys_assert(arena != NULL);
  if (arena == NULL || arena->head == NULL) {
    return;
  }

  sys_mutex_t *lock = arena->head->lock;
  if (lock != NULL) {
    sys_mutex_unlock(lock);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and links a new arena into a chain. */
sys_mem_arena_t *sys_mem_arena_init(size_t size, sys_mem_arena_t *prev,
                                    void *(*malloc_fn)(size_t size),
                                    void (*free_fn)(void *ptr)) {
  // Check arguments
  if (prev == NULL) {
    sys_assert(size > 0);
    if (size == 0 || malloc_fn == NULL || free_fn == NULL) {
      return NULL;
    }
  } else {
    if (!_sys_mem_arena_lock(prev)) {
      return NULL;
    }

    sys_assert(prev->next == NULL);
    if (prev->next != NULL || prev->malloc_fn == NULL ||
        prev->free_fn == NULL) {
      _sys_mem_arena_unlock(prev);
      return NULL;
    }

    malloc_fn = prev->malloc_fn;
    free_fn = prev->free_fn;
  }
  sys_assert(malloc_fn != NULL);
  sys_assert(free_fn != NULL);

  // Create a memory region large enough for the arena header plus payload,
  // rounded up to a 64-byte boundary.
  size_t arena_struct_size = sizeof(sys_mem_arena_t);
  if (size > SIZE_MAX - arena_struct_size) {
    if (prev != NULL) {
      _sys_mem_arena_unlock(prev);
    }
    // Return NULL on size overflow
    return NULL;
  }
  size_t requested_size = arena_struct_size + size;
  size_t aligned_size = (requested_size + 63u) & ~(size_t)63u;
  void *region = malloc_fn(aligned_size);
  if (region == NULL) {
    if (prev != NULL) {
      _sys_mem_arena_unlock(prev);
    }
    // Return NULL on allocation failure
    return NULL;
  }

  // Initialize the arena
  sys_mem_arena_t *arena = region;
  sys_memset(arena, 0, arena_struct_size);
  arena->malloc_fn = malloc_fn;
  arena->free_fn = free_fn;
  arena->size = aligned_size - arena_struct_size;
  arena->used_bytes = 0;
  arena->allocations = 0;
  if (prev == NULL) {
    arena->head = arena;
    arena->lock = sys_mutex_init();
    if (arena->lock == NULL) {
      free_fn(region);
      return NULL;
    }
  } else {
    arena->head = prev->head;
    prev->next = arena;
  }

  // Unlock the previous arena
  if (prev != NULL) {
    _sys_mem_arena_unlock(prev);
  }

  return arena;
}

/** @brief Deletes an arena chain starting from its head arena. */
void sys_mem_arena_delete(sys_mem_arena_t *arena) {
  sys_assert(arena != NULL);
  if (arena == NULL || arena->head == NULL || arena->head->free_fn == NULL) {
    return;
  }

  sys_mem_arena_t *head = arena->head;
  void (*free_fn)(void *ptr) = head->free_fn;

  if (head->lock != NULL) {
    sys_mutex_deinit(head->lock);
    head->lock = NULL;
  }

  while (head != NULL) {
    sys_mem_arena_t *next = head->next;
    free_fn(head);
    head = next;
  }
}

/** @brief Returns the next arena in a chain. */
sys_mem_arena_t *sys_mem_arena_next(sys_mem_arena_t *arena,
                                    sys_mem_arena_stats_t *stats) {
  if (arena == NULL) {
    return NULL;
  }

  if (stats != NULL) {
    stats->size_bytes = arena->size;
    stats->used_bytes = arena->used_bytes;
    stats->allocations = arena->allocations;
  }

  return arena->next;
}

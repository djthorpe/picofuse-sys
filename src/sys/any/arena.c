#include <picofuse/sys.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef struct sys_mem_arena_header_t {
  struct sys_mem_arena_header_t *next;
  size_t size;
  void *ptr;
} sys_mem_arena_header_t;

struct sys_mem_arena_t {
  sys_mem_arena_t *head;
  sys_mem_arena_t *prev;
  sys_mem_arena_t *next;
  sys_mutex_t *lock;
  void *(*malloc_fn)(size_t size);
  void (*free_fn)(void *ptr);
  sys_mem_arena_header_t *alloc_head;
  size_t size;
  size_t used_bytes;
  size_t allocations;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Returns the alignment used for arena headers and payloads. */
static size_t _sys_mem_arena_alignment(void) { return _Alignof(max_align_t); }

/** @brief Rounds a size up to the arena alignment. */
static bool _sys_mem_arena_align_up(size_t value, size_t *aligned) {
  size_t alignment = _sys_mem_arena_alignment();
  size_t remainder = value % alignment;

  if (remainder == 0) {
    *aligned = value;
    return true;
  }

  size_t delta = alignment - remainder;
  if (value > SIZE_MAX - delta) {
    return false;
  }

  *aligned = value + delta;
  return true;
}

/** @brief Returns the payload offset from the arena base. */
static size_t _sys_mem_arena_payload_offset(void) {
  size_t offset = 0;
  bool ok = _sys_mem_arena_align_up(sizeof(sys_mem_arena_t), &offset);
  sys_assert(ok);
  return ok ? offset : sizeof(sys_mem_arena_t);
}

/** @brief Returns the first usable byte in an arena payload. */
static uint8_t *_sys_mem_arena_payload_begin(sys_mem_arena_t *arena) {
  sys_assert(arena != NULL);
  return (uint8_t *)arena + _sys_mem_arena_payload_offset();
}

/** @brief Returns the byte offset where an allocation ends. */
static size_t _sys_mem_arena_block_end_offset(sys_mem_arena_t *arena,
                                              sys_mem_arena_header_t *header) {
  sys_assert(arena != NULL);
  sys_assert(header != NULL);

  uint8_t *base = _sys_mem_arena_payload_begin(arena);
  return (size_t)((uint8_t *)header->ptr - base) + header->size;
}

/** @brief Locates the header associated with a payload pointer. */
static sys_mem_arena_header_t *
_sys_mem_arena_find_header(sys_mem_arena_t *arena, void *ptr,
                           sys_mem_arena_header_t **prev_out) {
  sys_assert(arena != NULL);

  sys_mem_arena_header_t *prev = NULL;
  sys_mem_arena_header_t *current = arena->alloc_head;
  while (current != NULL) {
    if (current->ptr == ptr) {
      break;
    }
    prev = current;
    current = current->next;
  }

  if (prev_out != NULL) {
    *prev_out = prev;
  }

  return current;
}

/** @brief Finds space for a new allocation and links its header into the list.
 */
static sys_mem_arena_header_t *
_sys_mem_arena_alloc_locked(sys_mem_arena_t *arena, size_t size) {
  sys_assert(arena != NULL);

  if (size == 0) {
    return NULL;
  }

  size_t cursor_offset = 0;
  sys_mem_arena_header_t *prev = NULL;
  sys_mem_arena_header_t *current = arena->alloc_head;

  while (true) {
    size_t header_offset = 0;
    size_t ptr_offset = 0;
    if (!_sys_mem_arena_align_up(cursor_offset, &header_offset) ||
        header_offset > SIZE_MAX - sizeof(sys_mem_arena_header_t) ||
        !_sys_mem_arena_align_up(header_offset + sizeof(sys_mem_arena_header_t),
                                 &ptr_offset)) {
      return NULL;
    }

    if (ptr_offset > arena->size || size > arena->size - ptr_offset) {
      return NULL;
    }

    size_t end_offset = ptr_offset + size;
    size_t limit_offset = current == NULL
                              ? arena->size
                              : (size_t)((uint8_t *)current -
                                         _sys_mem_arena_payload_begin(arena));
    if (end_offset <= limit_offset) {
      uint8_t *base = _sys_mem_arena_payload_begin(arena);
      sys_mem_arena_header_t *header =
          (sys_mem_arena_header_t *)(base + header_offset);
      header->next = current;
      header->size = size;
      header->ptr = base + ptr_offset;

      if (prev == NULL) {
        arena->alloc_head = header;
      } else {
        prev->next = header;
      }

      arena->used_bytes += size;
      arena->allocations++;
      return header;
    }

    if (current == NULL) {
      return NULL;
    }

    cursor_offset = _sys_mem_arena_block_end_offset(arena, current);
    prev = current;
    current = current->next;
  }
}

/** @brief Unlinks a header and updates stats. */
static void _sys_mem_arena_free_locked(sys_mem_arena_t *arena,
                                       sys_mem_arena_header_t *prev,
                                       sys_mem_arena_header_t *header) {
  sys_assert(arena != NULL);
  sys_assert(header != NULL);

  if (prev == NULL) {
    arena->alloc_head = header->next;
  } else {
    prev->next = header->next;
  }

  sys_assert(arena->used_bytes >= header->size);
  sys_assert(arena->allocations > 0);
  arena->used_bytes -= header->size;
  arena->allocations--;
}

/** @brief Repoints the head pointer for an arena suffix. */
static void _sys_mem_arena_set_head(sys_mem_arena_t *arena,
                                    sys_mem_arena_t *head) {
  while (arena != NULL) {
    arena->head = head;
    arena = arena->next;
  }
}

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
  size_t payload_offset = _sys_mem_arena_payload_offset();
  if (size > SIZE_MAX - payload_offset) {
    if (prev != NULL) {
      _sys_mem_arena_unlock(prev);
    }
    // Return NULL on size overflow
    return NULL;
  }
  size_t arena_struct_size = sizeof(sys_mem_arena_t);
  size_t requested_size = payload_offset + size;
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
  arena->alloc_head = NULL;
  arena->size = aligned_size - payload_offset;
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
    arena->prev = prev;
    prev->next = arena;
  }

  // Unlock the previous arena
  if (prev != NULL) {
    _sys_mem_arena_unlock(prev);
  }

  return arena;
}

/** @brief Deletes a single arena from its chain. */
void sys_mem_arena_delete(sys_mem_arena_t *arena) {
  sys_assert(arena != NULL);
  if (arena == NULL || arena->head == NULL || arena->head->free_fn == NULL ||
      !_sys_mem_arena_lock(arena)) {
    return;
  }

  sys_mem_arena_t *head = arena->head;
  sys_mem_arena_t *prev = arena->prev;
  sys_mem_arena_t *next = arena->next;
  void (*free_fn)(void *ptr) = head->free_fn;
  sys_mutex_t *lock = head->lock;
  bool destroy_lock = false;

  if (prev != NULL) {
    prev->next = next;
  }
  if (next != NULL) {
    next->prev = prev;
  }

  if (arena == head) {
    if (next != NULL) {
      next->lock = lock;
      _sys_mem_arena_set_head(next, next);
    } else {
      destroy_lock = true;
    }
    arena->lock = NULL;
  }

  arena->prev = NULL;
  arena->next = NULL;

  if (lock != NULL) {
    sys_mutex_unlock(lock);
    if (destroy_lock) {
      sys_mutex_deinit(lock);
    }
  }

  free_fn(arena);
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

/** @brief Returns the previous arena in a chain. */
sys_mem_arena_t *_sys_mem_arena_prev(sys_mem_arena_t *arena,
                                     sys_mem_arena_stats_t *stats) {
  if (arena == NULL) {
    return NULL;
  }

  if (stats != NULL) {
    stats->size_bytes = arena->size;
    stats->used_bytes = arena->used_bytes;
    stats->allocations = arena->allocations;
  }

  return arena->prev;
}

/** @brief Returns the size of an allocation when owned by an arena. */
size_t _sys_mem_arena_alloc_size(sys_mem_arena_t *arena, void *ptr) {
  if (arena == NULL || ptr == NULL || !_sys_mem_arena_lock(arena)) {
    return 0;
  }

  sys_mem_arena_header_t *header = _sys_mem_arena_find_header(arena, ptr, NULL);
  size_t size = header == NULL ? 0 : header->size;
  _sys_mem_arena_unlock(arena);
  return size;
}

/** @brief Allocates a block from a single arena. */
void *sys_mem_arena_alloc(sys_mem_arena_t *arena, size_t size) {
  if (arena == NULL || size == 0 || !_sys_mem_arena_lock(arena)) {
    return NULL;
  }

  sys_mem_arena_header_t *header = _sys_mem_arena_alloc_locked(arena, size);
  _sys_mem_arena_unlock(arena);
  return header == NULL ? NULL : header->ptr;
}

/** @brief Resizes an allocation owned by a single arena. */
void *sys_mem_arena_realloc(sys_mem_arena_t *arena, void *ptr, size_t size) {
  if (arena == NULL) {
    return NULL;
  }
  if (ptr == NULL) {
    return sys_mem_arena_alloc(arena, size);
  }
  if (size == 0) {
    sys_mem_arena_free(arena, ptr);
    return NULL;
  }
  if (!_sys_mem_arena_lock(arena)) {
    return NULL;
  }

  sys_mem_arena_header_t *prev = NULL;
  sys_mem_arena_header_t *header =
      _sys_mem_arena_find_header(arena, ptr, &prev);
  if (header == NULL) {
    _sys_mem_arena_unlock(arena);
    return NULL;
  }

  if (size <= header->size) {
    arena->used_bytes -= header->size - size;
    header->size = size;
    _sys_mem_arena_unlock(arena);
    return header->ptr;
  }

  size_t limit_offset = arena->size;
  if (header->next != NULL) {
    limit_offset =
        (size_t)((uint8_t *)header->next - _sys_mem_arena_payload_begin(arena));
  }
  size_t header_offset =
      (size_t)((uint8_t *)header - _sys_mem_arena_payload_begin(arena));
  size_t ptr_offset =
      (size_t)((uint8_t *)header->ptr - _sys_mem_arena_payload_begin(arena));
  if (header_offset <= ptr_offset && size <= limit_offset - ptr_offset) {
    arena->used_bytes += size - header->size;
    header->size = size;
    _sys_mem_arena_unlock(arena);
    return header->ptr;
  }

  sys_mem_arena_header_t *replacement =
      _sys_mem_arena_alloc_locked(arena, size);
  if (replacement == NULL) {
    _sys_mem_arena_unlock(arena);
    return NULL;
  }

  header = _sys_mem_arena_find_header(arena, ptr, &prev);
  sys_assert(header != NULL);
  if (header == NULL) {
    _sys_mem_arena_free_locked(arena, NULL, replacement);
    _sys_mem_arena_unlock(arena);
    return NULL;
  }

  sys_memcpy(replacement->ptr, header->ptr, header->size);
  _sys_mem_arena_free_locked(arena, prev, header);
  void *result = replacement->ptr;
  _sys_mem_arena_unlock(arena);
  return result;
}

/** @brief Releases an allocation owned by a single arena. */
void sys_mem_arena_free(sys_mem_arena_t *arena, void *ptr) {
  if (arena == NULL || ptr == NULL || !_sys_mem_arena_lock(arena)) {
    return;
  }

  sys_mem_arena_header_t *prev = NULL;
  sys_mem_arena_header_t *header =
      _sys_mem_arena_find_header(arena, ptr, &prev);
  if (header != NULL) {
    _sys_mem_arena_free_locked(arena, prev, header);
  }

  _sys_mem_arena_unlock(arena);
}

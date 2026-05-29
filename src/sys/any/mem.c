#include "private.h"
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef SYS_MEM_ARENA_GROWTH_MIN_SLACK
#define SYS_MEM_ARENA_GROWTH_MIN_SLACK ((size_t)1024u)
#endif

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static sys_mem_arena_t *_sys_mem_default_head = NULL;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/**
 * @brief Return the tail arena in a chain.
 * @param head Head of the chain.
 * @param stats Optional stats populated for the returned tail arena.
 * @return Tail arena, or `NULL` when the chain is empty.
 */
static sys_mem_arena_t *_sys_mem_default_tail(sys_mem_arena_t *head,
                                              sys_mem_arena_stats_t *stats) {
  if (head == NULL) {
    return NULL;
  }

  sys_mem_arena_t *current = head;
  sys_mem_arena_stats_t current_stats = {0};
  while (true) {
    sys_mem_arena_t *next = sys_mem_arena_next(current, &current_stats);
    if (next == NULL) {
      if (stats != NULL) {
        *stats = current_stats;
      }
      return current;
    }
    current = next;
  }
}

/**
 * @brief Compute the size of a newly appended arena.
 * @param previous_size Payload size of the current tail arena.
 * @param required_size Allocation size that triggered growth.
 * @param arena_size Output size for the new arena payload.
 * @return `true` when a size was produced, otherwise `false`.
 */
static bool _sys_mem_default_growth_size(size_t previous_size,
                                         size_t required_size,
                                         size_t *arena_size) {
  if (arena_size == NULL || required_size == 0) {
    return false;
  }

  size_t candidate = previous_size;
  if (required_size > candidate) {
    size_t slack = required_size / 2u;
    if (slack < SYS_MEM_ARENA_GROWTH_MIN_SLACK) {
      slack = SYS_MEM_ARENA_GROWTH_MIN_SLACK;
    }

    if (required_size > SIZE_MAX - slack) {
      candidate = required_size;
    } else {
      candidate = required_size + slack;
    }
  }

  if (candidate == 0) {
    return false;
  }

  *arena_size = candidate;
  return true;
}

/**
 * @brief Allocate from the default arena chain, growing it when necessary.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block, or `NULL` on failure.
 */
static void *_sys_mem_default_alloc(size_t size) {
  if (size == 0) {
    return NULL;
  }

  while (true) {
    sys_mem_arena_t *head = _sys_mem_default_head;
    sys_mem_arena_stats_t tail_stats = {0};
    sys_mem_arena_t *tail = _sys_mem_default_tail(head, &tail_stats);
    if (tail == NULL) {
      return NULL;
    }

    sys_mem_arena_t *current = tail;
    while (current != NULL) {
      void *ptr = sys_mem_arena_alloc(current, size);
      if (ptr != NULL) {
        return ptr;
      }
      current = _sys_mem_arena_prev(current, NULL);
    }

    size_t arena_size = 0;
    if (!_sys_mem_default_growth_size(tail_stats.size_bytes, size,
                                      &arena_size)) {
      return NULL;
    }

    sys_mem_arena_t *next = sys_mem_arena_init(arena_size, tail, NULL, NULL);
    if (next != NULL) {
      return sys_mem_arena_alloc(next, size);
    }

    if (_sys_mem_default_tail(_sys_mem_default_head, NULL) == tail) {
      return NULL;
    }
  }
}

/**
 * @brief Find the arena that owns a payload pointer.
 * @param ptr Payload pointer to locate.
 * @param size Optional allocation size populated for the owning arena.
 * @return Arena that owns `ptr`, or `NULL` when not found.
 */
static sys_mem_arena_t *_sys_mem_default_owner(void *ptr, size_t *size) {
  sys_mem_arena_t *tail = _sys_mem_default_tail(_sys_mem_default_head, NULL);
  while (tail != NULL) {
    size_t alloc_size = _sys_mem_arena_alloc_size(tail, ptr);
    if (alloc_size != 0) {
      if (size != NULL) {
        *size = alloc_size;
      }
      return tail;
    }
    tail = _sys_mem_arena_prev(tail, NULL);
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize the process-wide default arena.
 * @param capacity Arena capacity in bytes.
 * @param malloc_fn Backing allocator used for the first arena.
 * @param free_fn Backing deallocator paired with `malloc_fn`.
 * @return `true` when the default arena is ready, otherwise `false`.
 */
bool _sys_mem_init(size_t capacity, void *(*malloc_fn)(size_t),
                   void (*free_fn)(void *)) {
  if (_sys_mem_default_head != NULL) {
    return true;
  }

  if (capacity == 0 || malloc_fn == NULL || free_fn == NULL) {
    return false;
  }

  sys_mem_arena_t *arena =
      sys_mem_arena_init(capacity, NULL, malloc_fn, free_fn);
  if (arena == NULL) {
    return false;
  }

  _sys_mem_default_head = arena;
  return true;
}

/**
 * @brief Tear down the process-wide default arena chain.
 */
void _sys_mem_deinit(void) {
  sys_mem_arena_t *head = _sys_mem_default_head;
  _sys_mem_default_head = NULL;

#ifndef NDEBUG
  if (head != NULL) {
    sys_printf("mem deinit:\n");
    sys_mem_dump(head);
  }
#endif

  while (head != NULL) {
    sys_mem_arena_t *next = sys_mem_arena_next(head, NULL);
    sys_mem_arena_delete(head);
    head = next;
  }
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Fill a memory region with a byte value.
 * @param dest Destination memory region.
 * @param value Byte value to write.
 * @param count Number of bytes to set.
 * @return The original `dest` pointer.
 */
void *sys_memset(void *dest, int value, size_t count) {
  unsigned char *ptr = dest;
  while (count--) {
    *ptr++ = (unsigned char)value;
  }
  return dest;
}

/**
 * @brief Copy bytes from one memory region to another.
 * @param dest Destination memory region.
 * @param src Source memory region.
 * @param count Number of bytes to copy.
 * @return The original `dest` pointer.
 */
void *sys_memcpy(void *dest, const void *src, size_t count) {
  unsigned char *dst = dest;
  const unsigned char *source = src;
  uintptr_t dst_addr = (uintptr_t)dst;
  uintptr_t source_addr = (uintptr_t)source;

  if (dst == source || count == 0) {
    return dest;
  }

  if (dst_addr < source_addr || dst_addr - source_addr >= count) {
    while (count-- != 0) {
      *dst++ = *source++;
    }
    return dest;
  }

  dst += count;
  source += count;
  while (count-- != 0) {
    *--dst = *--source;
  }

  return dest;
}

/**
 * @brief Compare two memory regions byte by byte.
 * @param lhs First memory region.
 * @param rhs Second memory region.
 * @param count Number of bytes to compare.
 * @return Negative, zero, or positive depending on the first differing byte.
 */
int sys_memcmp(const void *lhs, const void *rhs, size_t count) {
  const unsigned char *left = lhs;
  const unsigned char *right = rhs;
  while (count--) {
    int diff = (int)*left++ - (int)*right++;
    if (diff != 0) {
      return diff;
    }
  }
  return 0;
}

/**
 * @brief Measure the length of a null-terminated string.
 * @param str String to measure.
 * @return Number of characters before the terminating null byte.
 */
size_t sys_strlen(const char *str) {
  const char *s = str;
  while (*s) {
    s++;
  }
  return s - str;
}

/**
 * @brief Print per-arena statistics for a chain.
 * @param arena First arena in the chain, or `NULL` for the default arena.
 */
void sys_mem_dump(sys_mem_arena_t *arena) {
  sys_mem_arena_t *current = arena == NULL ? _sys_mem_default_head : arena;
  size_t index = 0;

  if (current == NULL) {
    sys_printf("mem: no arenas\n");
    return;
  }

  while (current != NULL) {
    sys_mem_arena_stats_t stats = {0};
    sys_mem_arena_t *next = sys_mem_arena_next(current, &stats);
    sys_printf("mem arena %zu: size=%zu used=%zu allocations=%zu\n", index,
               stats.size_bytes, stats.used_bytes, stats.allocations);
    current = next;
    index++;
  }
}

/**
 * @brief Allocate zero-initialized memory from the default arena.
 * @param count Number of elements to allocate.
 * @param size Size of each element in bytes.
 * @return Pointer to the allocated block, or `NULL` on failure.
 */
void *sys_calloc(size_t count, size_t size) {
  if (count != 0 && size > SIZE_MAX / count) {
    return NULL;
  }
  size_t total_size = count * size;
  void *ptr = sys_malloc(total_size);
  if (ptr != NULL) {
    sys_memset(ptr, 0, total_size);
  }
  return ptr;
}

/**
 * @brief Resize an allocation owned by the default arena.
 * @param ptr Existing allocation, or `NULL`.
 * @param size New size in bytes.
 * @return Pointer to the resized allocation, or `NULL` on failure.
 */
void *sys_realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return sys_malloc(size);
  }

  if (size == 0) {
    sys_free(ptr);
    return NULL;
  }

  size_t current_size = 0;
  sys_mem_arena_t *owner = _sys_mem_default_owner(ptr, &current_size);
  if (owner == NULL) {
    return NULL;
  }

  void *resized = sys_mem_arena_realloc(owner, ptr, size);
  if (resized != NULL) {
    return resized;
  }

  void *replacement = _sys_mem_default_alloc(size);
  if (replacement == NULL) {
    return NULL;
  }

  size_t copy_size = current_size < size ? current_size : size;
  sys_memcpy(replacement, ptr, copy_size);
  sys_mem_arena_free(owner, ptr);
  return replacement;
}

/**
 * @brief Release an allocation owned by the default arena.
 * @param ptr Allocation to release, or `NULL`.
 */
void sys_free(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  sys_mem_arena_t *owner = _sys_mem_default_owner(ptr, NULL);
  if (owner != NULL) {
    sys_mem_arena_free(owner, ptr);
  }
}

/**
 * @brief Allocate memory from the default arena.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block, or `NULL` on failure.
 */
void *sys_malloc(size_t size) { return _sys_mem_default_alloc(size); }
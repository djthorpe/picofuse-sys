#include <picofuse/sys.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef SYS_MEM_EVENT_CAPACITY
#define SYS_MEM_EVENT_CAPACITY 32
#endif

typedef struct sys_mem_event_slot_t {
  uintptr_t old_ptr;
  uintptr_t new_ptr;
  size_t size;
  size_t count;
  unsigned int type;
  size_t sequence;
} sys_mem_event_slot_t;

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static size_t _sys_mem_malloc_calls = 0;
static size_t _sys_mem_calloc_calls = 0;
static size_t _sys_mem_realloc_calls = 0;
static size_t _sys_mem_free_calls = 0;
static size_t _sys_mem_failed_allocations = 0;
static size_t _sys_mem_requested_bytes = 0;
static size_t _sys_mem_event_sequence = 0;
static sys_mem_event_slot_t _sys_mem_events[SYS_MEM_EVENT_CAPACITY] = {{0}};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

static void _sys_mem_record_requested_bytes(size_t count, size_t size) {
  if (count != 0 && size <= SIZE_MAX / count) {
    __atomic_add_fetch(&_sys_mem_requested_bytes, count * size,
                       __ATOMIC_RELAXED);
  }
}

static void _sys_mem_record_failure(void) {
  __atomic_add_fetch(&_sys_mem_failed_allocations, 1, __ATOMIC_RELAXED);
}

static void _sys_mem_record_event(sys_mem_event_type_t type, uintptr_t old_ptr,
                                  uintptr_t new_ptr, size_t size,
                                  size_t count) {
  size_t sequence =
      __atomic_add_fetch(&_sys_mem_event_sequence, 1, __ATOMIC_RELAXED);
  sys_mem_event_slot_t *slot =
      &_sys_mem_events[(sequence - 1) % SYS_MEM_EVENT_CAPACITY];
  slot->old_ptr = old_ptr;
  slot->new_ptr = new_ptr;
  slot->size = size;
  slot->count = count;
  slot->type = (unsigned int)type;
  __atomic_store_n(&slot->sequence, sequence, __ATOMIC_RELEASE);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC API

void *sys_memset(void *dest, int value, size_t count) {
  unsigned char *ptr = dest;
  while (count--) {
    *ptr++ = (unsigned char)value;
  }
  return dest;
}

void *sys_memcpy(void *dest, const void *src, size_t count) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  while (count--) {
    *d++ = *s++;
  }
  return dest;
}

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

size_t sys_strlen(const char *str) {
  const char *s = str;
  while (*s) {
    s++;
  }
  return s - str;
}

void *sys_calloc(size_t count, size_t size) {
  __atomic_add_fetch(&_sys_mem_calloc_calls, 1, __ATOMIC_RELAXED);
  if (count != 0 && size > SIZE_MAX / count) {
    _sys_mem_record_failure();
    _sys_mem_record_event(sys_mem_event_calloc, 0, 0, size, count);
    return NULL;
  }

  _sys_mem_record_requested_bytes(count, size);
  void *ptr = calloc(count, size);
  if (ptr == NULL && count != 0 && size != 0) {
    _sys_mem_record_failure();
  }
  _sys_mem_record_event(sys_mem_event_calloc, 0, (uintptr_t)ptr, size, count);
  return ptr;
}

void *sys_realloc(void *ptr, size_t size) {
  __atomic_add_fetch(&_sys_mem_realloc_calls, 1, __ATOMIC_RELAXED);
  _sys_mem_record_requested_bytes(1, size);
  uintptr_t old_ptr = (uintptr_t)ptr;
  void *resized = realloc(ptr, size);
  if (resized == NULL && size != 0) {
    _sys_mem_record_failure();
  }
  _sys_mem_record_event(sys_mem_event_realloc, old_ptr, (uintptr_t)resized,
                        size, 1);
  return resized;
}

void sys_free(void *ptr) {
  __atomic_add_fetch(&_sys_mem_free_calls, 1, __ATOMIC_RELAXED);
  _sys_mem_record_event(sys_mem_event_free, (uintptr_t)ptr, 0, 0, 0);
  free(ptr);
}

void *sys_malloc(size_t size) {
  __atomic_add_fetch(&_sys_mem_malloc_calls, 1, __ATOMIC_RELAXED);
  _sys_mem_record_requested_bytes(1, size);
  void *ptr = malloc(size);
  if (ptr == NULL && size != 0) {
    _sys_mem_record_failure();
  }
  _sys_mem_record_event(sys_mem_event_malloc, 0, (uintptr_t)ptr, size, 1);
  return ptr;
}

void sys_mem_debug_reset(void) {
  __atomic_store_n(&_sys_mem_malloc_calls, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&_sys_mem_calloc_calls, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&_sys_mem_realloc_calls, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&_sys_mem_free_calls, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&_sys_mem_failed_allocations, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&_sys_mem_requested_bytes, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&_sys_mem_event_sequence, 0, __ATOMIC_RELAXED);
  for (size_t index = 0; index < SYS_MEM_EVENT_CAPACITY; index++) {
    _sys_mem_events[index].old_ptr = 0;
    _sys_mem_events[index].new_ptr = 0;
    _sys_mem_events[index].size = 0;
    _sys_mem_events[index].count = 0;
    _sys_mem_events[index].type = 0;
    __atomic_store_n(&_sys_mem_events[index].sequence, 0, __ATOMIC_RELAXED);
  }
}

void sys_mem_debug_stats(sys_mem_stats_t *stats) {
  if (stats == NULL) {
    return;
  }

  stats->malloc_calls =
      __atomic_load_n(&_sys_mem_malloc_calls, __ATOMIC_RELAXED);
  stats->calloc_calls =
      __atomic_load_n(&_sys_mem_calloc_calls, __ATOMIC_RELAXED);
  stats->realloc_calls =
      __atomic_load_n(&_sys_mem_realloc_calls, __ATOMIC_RELAXED);
  stats->free_calls = __atomic_load_n(&_sys_mem_free_calls, __ATOMIC_RELAXED);
  stats->failed_allocations =
      __atomic_load_n(&_sys_mem_failed_allocations, __ATOMIC_RELAXED);
  stats->requested_bytes =
      __atomic_load_n(&_sys_mem_requested_bytes, __ATOMIC_RELAXED);
}

size_t sys_mem_debug_events(sys_mem_event_t *events, size_t capacity) {
  size_t sequence = __atomic_load_n(&_sys_mem_event_sequence, __ATOMIC_ACQUIRE);
  size_t available =
      sequence < SYS_MEM_EVENT_CAPACITY ? sequence : SYS_MEM_EVENT_CAPACITY;
  if (events == NULL) {
    return available;
  }

  size_t copied = capacity < available ? capacity : available;
  size_t first_sequence = copied == 0 ? 0 : sequence - copied + 1;
  for (size_t index = 0; index < copied; index++) {
    size_t expected_sequence = first_sequence + index;
    sys_mem_event_slot_t *slot =
        &_sys_mem_events[(expected_sequence - 1) % SYS_MEM_EVENT_CAPACITY];
    size_t observed_sequence =
        __atomic_load_n(&slot->sequence, __ATOMIC_ACQUIRE);
    if (observed_sequence != expected_sequence) {
      events[index].sequence = 0;
      events[index].type = 0;
      events[index].old_ptr = 0;
      events[index].new_ptr = 0;
      events[index].size = 0;
      events[index].count = 0;
      continue;
    }

    events[index].sequence = observed_sequence;
    events[index].type = (sys_mem_event_type_t)slot->type;
    events[index].old_ptr = slot->old_ptr;
    events[index].new_ptr = slot->new_ptr;
    events[index].size = slot->size;
    events[index].count = slot->count;
  }

  return copied;
}
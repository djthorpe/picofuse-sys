#include <picofuse/sys.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef SYSTEM_NAME_PICO
#include <pico/critical_section.h>
#endif

// How long sys_event_queue_pop() waits per internal poll iteration. Pushes
// from IRQ context (see sys_event_queue_try_push) do not participate in the
// queue->mutex + not_empty pairing used for wakeups below, so a wakeup can
// occasionally be missed; the wait is bounded and the loop always re-checks
// the actual (lock-protected) queue afterward, so a missed wakeup costs at
// most one extra poll interval rather than an indefinite hang.
#define _SYS_EVENT_QUEUE_POP_POLL_MS 50u

struct sys_event_queue_t {
  size_t capacity;
  size_t head;
  size_t tail;
  size_t count;
  sys_mutex_t *mutex;
  sys_cond_t *not_empty;
  bool shutdown;
  sys_event_t items[];
};

#ifdef SYSTEM_NAME_PICO
// Shared by every queue's head/tail/count/items/shutdown fields, rather than
// a critical_section_t per queue instance: RP2350 has only 32 claimable spin
// locks total, shared with pico-sdk's own subsystems (cyw43/lwIP/
// BTstack/mbedtls among them), and each queue claiming its own burns through
// that budget fast (an app can easily create several queues). Each critical
// section below is only ring-buffer index bookkeeping, so sharing one lock
// across all queues costs a little extra (harmless) contention between
// unrelated queues, not correctness. A critical_section (not queue->mutex)
// so pushes from IRQ context (HID timer/gpio event sources) always succeed
// immediately instead of risking a same-core self-deadlock against a
// consumer already holding queue->mutex, or silently dropping the event
// under contention.
static critical_section_t _sys_event_queue_data_lock_shared;

void _sys_event_queue_module_init(void) {
  critical_section_init(&_sys_event_queue_data_lock_shared);
}
#endif

/** @brief Returns whether the queue has initialized synchronization state. */
static bool _sys_event_queue_valid_unlocked(sys_event_queue_t *queue) {
  return queue != NULL && queue->capacity > 0 && queue->mutex != NULL &&
         queue->not_empty != NULL;
}

/** @brief Returns the next circular-buffer index. */
static size_t _sys_event_queue_next_index(sys_event_queue_t *queue,
                                          size_t index) {
  sys_assert(queue != NULL);
  return (index + 1u) % queue->capacity;
}

/** @brief Pops the current tail entry while the queue is locked. */
static sys_event_t _sys_event_queue_pop_locked(sys_event_queue_t *queue) {
  sys_assert(queue != NULL);

  if (queue->count == 0) {
    return NULL;
  }

  sys_event_t event = queue->items[queue->tail];
  queue->items[queue->tail] = NULL;
  queue->tail = _sys_event_queue_next_index(queue, queue->tail);
  queue->count--;
  return event;
}

/** @brief Locks the ring-buffer fields; safe to call from IRQ context. */
static void _sys_event_queue_data_lock(sys_event_queue_t *queue) {
#ifdef SYSTEM_NAME_PICO
  (void)queue;
  critical_section_enter_blocking(&_sys_event_queue_data_lock_shared);
#else
  sys_mutex_lock(queue->mutex);
#endif
}

/** @brief Unlocks the ring-buffer fields. */
static void _sys_event_queue_data_unlock(sys_event_queue_t *queue) {
#ifdef SYSTEM_NAME_PICO
  (void)queue;
  critical_section_exit(&_sys_event_queue_data_lock_shared);
#else
  sys_mutex_unlock(queue->mutex);
#endif
}

sys_event_queue_t *sys_event_queue_init(size_t capacity) {
  if (capacity == 0 ||
      capacity > (SIZE_MAX - sizeof(sys_event_queue_t)) / sizeof(sys_event_t)) {
    return NULL;
  }

  size_t total_size =
      sizeof(sys_event_queue_t) + capacity * sizeof(sys_event_t);
  sys_event_queue_t *queue = (sys_event_queue_t *)sys_malloc(total_size);
  if (queue == NULL) {
    return NULL;
  }

  sys_memset(queue, 0, total_size);
  queue->capacity = capacity;
  queue->mutex = sys_mutex_init();
  if (queue->mutex == NULL) {
    sys_free(queue);
    return NULL;
  }

  queue->not_empty = sys_cond_init();
  if (queue->not_empty == NULL) {
    sys_mutex_deinit(queue->mutex);
    sys_free(queue);
    return NULL;
  }

  return queue;
}

void sys_event_queue_deinit(sys_event_queue_t *queue) {
  if (queue == NULL) {
    return;
  }

  if (_sys_event_queue_valid_unlocked(queue)) {
    _sys_event_queue_data_lock(queue);
    queue->shutdown = true;
    _sys_event_queue_data_unlock(queue);
    sys_cond_broadcast(queue->not_empty);
  }

  if (queue->not_empty != NULL) {
    sys_cond_deinit(queue->not_empty);
  }
  if (queue->mutex != NULL) {
    sys_mutex_deinit(queue->mutex);
  }
  sys_free(queue);
}

bool sys_event_queue_push(sys_event_queue_t *queue, sys_event_t event) {
  if (event == NULL || !_sys_event_queue_valid_unlocked(queue)) {
    return false;
  }

  _sys_event_queue_data_lock(queue);

  if (queue->shutdown) {
    _sys_event_queue_data_unlock(queue);
    return false;
  }

  queue->items[queue->head] = event;
  queue->head = _sys_event_queue_next_index(queue, queue->head);
  if (queue->count == queue->capacity) {
    queue->tail = queue->head;
  } else {
    queue->count++;
  }

  _sys_event_queue_data_unlock(queue);

  return sys_cond_broadcast(queue->not_empty);
}

bool sys_event_queue_try_push(sys_event_queue_t *queue, sys_event_t event) {
  // Safe to call from IRQ context (HID timer/gpio event sources): the data
  // lock never blocks (see _sys_event_queue_data_lock), and
  // sys_cond_broadcast() is IRQ-safe too (see src/sys/pico/cond.c).
  if (event == NULL || !_sys_event_queue_valid_unlocked(queue)) {
    return false;
  }

  _sys_event_queue_data_lock(queue);

  if (queue->shutdown || queue->count == queue->capacity) {
    _sys_event_queue_data_unlock(queue);
    return false;
  }

  queue->items[queue->head] = event;
  queue->head = _sys_event_queue_next_index(queue, queue->head);
  queue->count++;

  _sys_event_queue_data_unlock(queue);

  return sys_cond_broadcast(queue->not_empty);
}

sys_event_t sys_event_queue_peek(sys_event_queue_t *queue) {
  // Deliberately unlocked: pairs with sys_event_queue_lock()/unlock() (see
  // their doc comments), which callers already hold around this call.
  // Locking here too would self-deadlock against that outer lock.
  if (!_sys_event_queue_valid_unlocked(queue) || queue->count == 0) {
    return NULL;
  }

  return queue->items[queue->tail];
}

sys_event_t sys_event_queue_pop(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return NULL;
  }

  // A bounded poll loop rather than one indefinite wait: pushes from IRQ
  // context no longer serialize with this function's "check queue, then
  // wait" step via queue->mutex (see sys_event_queue_try_push), so a wakeup
  // can rarely be missed. Re-checking the queue every
  // _SYS_EVENT_QUEUE_POP_POLL_MS bounds the resulting extra latency instead
  // of risking an indefinite hang.
  for (;;) {
    _sys_event_queue_data_lock(queue);
    if (queue->count > 0) {
      sys_event_t event = _sys_event_queue_pop_locked(queue);
      _sys_event_queue_data_unlock(queue);
      return event;
    }
    bool shutdown = queue->shutdown;
    _sys_event_queue_data_unlock(queue);

    if (shutdown) {
      return NULL;
    }

    if (!sys_mutex_lock(queue->mutex)) {
      return NULL;
    }
    sys_cond_timedwait(queue->not_empty, queue->mutex,
                       _SYS_EVENT_QUEUE_POP_POLL_MS);
    sys_mutex_unlock(queue->mutex);
  }
}

sys_event_t sys_event_queue_try_pop(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return NULL;
  }

  _sys_event_queue_data_lock(queue);
  sys_event_t event = _sys_event_queue_pop_locked(queue);
  _sys_event_queue_data_unlock(queue);
  return event;
}

sys_event_t sys_event_queue_timed_pop(sys_event_queue_t *queue,
                                      uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    return sys_event_queue_pop(queue);
  }
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return NULL;
  }

  uint64_t deadline = sys_timestamp_ms() + timeout_ms;

  // See sys_event_queue_pop(): queue->mutex/not_empty are a best-effort,
  // low-latency wakeup hint here, not required for correctness. This loop
  // always re-checks the actual (lock-protected) queue below regardless of
  // whether the wait below is signaled or simply times out, so a wakeup
  // that races a concurrent IRQ-context push just costs one extra bounded
  // iteration, never a lost event.
  for (;;) {
    _sys_event_queue_data_lock(queue);
    if (queue->count > 0) {
      sys_event_t event = _sys_event_queue_pop_locked(queue);
      _sys_event_queue_data_unlock(queue);
      return event;
    }
    bool shutdown = queue->shutdown;
    _sys_event_queue_data_unlock(queue);

    if (shutdown) {
      return NULL;
    }

    uint64_t now = sys_timestamp_ms();
    if (now >= deadline) {
      return NULL;
    }

    uint64_t remaining = deadline - now;
    if (remaining > UINT32_MAX) {
      remaining = UINT32_MAX;
    }

    if (!sys_mutex_lock(queue->mutex)) {
      return NULL;
    }
    sys_cond_timedwait(queue->not_empty, queue->mutex, (uint32_t)remaining);
    sys_mutex_unlock(queue->mutex);
  }
}

size_t sys_event_queue_size(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return 0;
  }

  _sys_event_queue_data_lock(queue);
  size_t size = queue->count;
  _sys_event_queue_data_unlock(queue);
  return size;
}

size_t sys_event_queue_capacity(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return 0;
  }

  return queue->capacity;
}

bool sys_event_queue_empty(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return true;
  }

  _sys_event_queue_data_lock(queue);
  bool empty = queue->count == 0;
  _sys_event_queue_data_unlock(queue);
  return empty;
}

void sys_event_queue_shutdown(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return;
  }

  _sys_event_queue_data_lock(queue);
  queue->shutdown = true;
  _sys_event_queue_data_unlock(queue);

  sys_cond_broadcast(queue->not_empty);
}

bool sys_event_queue_lock(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return false;
  }

  return sys_mutex_lock(queue->mutex);
}

bool sys_event_queue_unlock(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return false;
  }

  return sys_mutex_unlock(queue->mutex);
}

bool sys_event_queue_valid(sys_event_queue_t *queue) {
  return _sys_event_queue_valid_unlocked(queue);
}

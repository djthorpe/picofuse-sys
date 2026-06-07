#include <picofuse/sys.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

  if (_sys_event_queue_valid_unlocked(queue) && sys_mutex_lock(queue->mutex)) {
    queue->shutdown = true;
    sys_cond_broadcast(queue->not_empty);
    sys_mutex_unlock(queue->mutex);
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
  if (event == NULL || !_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return false;
  }

  if (queue->shutdown) {
    sys_mutex_unlock(queue->mutex);
    return false;
  }

  queue->items[queue->head] = event;
  queue->head = _sys_event_queue_next_index(queue, queue->head);
  if (queue->count == queue->capacity) {
    queue->tail = queue->head;
  } else {
    queue->count++;
  }

  bool ok = sys_cond_broadcast(queue->not_empty);
  ok = sys_mutex_unlock(queue->mutex) && ok;
  return ok;
}

bool sys_event_queue_try_push(sys_event_queue_t *queue, sys_event_t event) {
  if (event == NULL || !_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return false;
  }

  if (queue->shutdown || queue->count == queue->capacity) {
    sys_mutex_unlock(queue->mutex);
    return false;
  }

  queue->items[queue->head] = event;
  queue->head = _sys_event_queue_next_index(queue, queue->head);
  queue->count++;

  bool ok = sys_cond_broadcast(queue->not_empty);
  ok = sys_mutex_unlock(queue->mutex) && ok;
  return ok;
}

sys_event_t sys_event_queue_peek(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue) || queue->count == 0) {
    return NULL;
  }

  return queue->items[queue->tail];
}

sys_event_t sys_event_queue_pop(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return NULL;
  }

  while (queue->count == 0 && !queue->shutdown) {
    if (!sys_cond_wait(queue->not_empty, queue->mutex)) {
      sys_mutex_unlock(queue->mutex);
      return NULL;
    }
  }

  sys_event_t event = _sys_event_queue_pop_locked(queue);
  sys_mutex_unlock(queue->mutex);
  return event;
}

sys_event_t sys_event_queue_try_pop(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return NULL;
  }

  sys_event_t event = _sys_event_queue_pop_locked(queue);
  sys_mutex_unlock(queue->mutex);
  return event;
}

sys_event_t sys_event_queue_timed_pop(sys_event_queue_t *queue,
                                      uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    return sys_event_queue_pop(queue);
  }
  if (!_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return NULL;
  }

  uint64_t deadline = sys_timestamp_ms() + timeout_ms;
  while (queue->count == 0 && !queue->shutdown) {
    uint64_t now = sys_timestamp_ms();
    if (now >= deadline) {
      sys_mutex_unlock(queue->mutex);
      return NULL;
    }

    uint64_t remaining = deadline - now;
    if (remaining > UINT32_MAX) {
      remaining = UINT32_MAX;
    }

    if (!sys_cond_timedwait(queue->not_empty, queue->mutex,
                            (uint32_t)remaining)) {
      sys_mutex_unlock(queue->mutex);
      return NULL;
    }
  }

  sys_event_t event = _sys_event_queue_pop_locked(queue);
  sys_mutex_unlock(queue->mutex);
  return event;
}

size_t sys_event_queue_size(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return 0;
  }

  size_t size = queue->count;
  sys_mutex_unlock(queue->mutex);
  return size;
}

size_t sys_event_queue_capacity(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue)) {
    return 0;
  }

  return queue->capacity;
}

bool sys_event_queue_empty(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return true;
  }

  bool empty = queue->count == 0;
  sys_mutex_unlock(queue->mutex);
  return empty;
}

void sys_event_queue_shutdown(sys_event_queue_t *queue) {
  if (!_sys_event_queue_valid_unlocked(queue) ||
      !sys_mutex_lock(queue->mutex)) {
    return;
  }

  queue->shutdown = true;
  sys_cond_broadcast(queue->not_empty);
  sys_mutex_unlock(queue->mutex);
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
#include <pico/critical_section.h>
#include <pico/mutex.h>
#include <pico/sem.h>
#include <picofuse/sys.h>

#include "pico_sys_internal.h"

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_cond_t {
  semaphore_t sem;
  mutex_t waiters_lock;
  int waiters_count;
  bool init;
};

static critical_section_t cond_pool_lock;
static bool cond_pool_lock_init = false;
static sys_cond_t cond_pool[SYS_COND_CAPACITY];
static size_t cond_pool_next_index = 0;

static bool _sys_cond_valid(const sys_cond_t *cond) {
  return cond != NULL && cond->init &&
         mutex_is_initialized((mutex_t *)&cond->waiters_lock);
}

static void _sys_cond_pool_lock_init(void) {
  if (!cond_pool_lock_init) {
    critical_section_init(&cond_pool_lock);
    cond_pool_lock_init = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

sys_cond_t *sys_cond_init(void) {
  _sys_cond_pool_lock_init();
  critical_section_enter_blocking(&cond_pool_lock);

  for (size_t offset = 0; offset < SYS_COND_CAPACITY; offset++) {
    size_t index = (cond_pool_next_index + offset) % SYS_COND_CAPACITY;
    sys_cond_t *cond = &cond_pool[index];
    if (cond->init) {
      continue;
    }

    sem_init(&cond->sem, 0, (int16_t)SYS_COND_CAPACITY);
    mutex_init(&cond->waiters_lock);
    cond->waiters_count = 0;
    cond->init = mutex_is_initialized(&cond->waiters_lock);
    if (!cond->init) {
      critical_section_exit(&cond_pool_lock);
      return NULL;
    }

    cond_pool_next_index = (index + 1) % SYS_COND_CAPACITY;
    critical_section_exit(&cond_pool_lock);
    return cond;
  }

  critical_section_exit(&cond_pool_lock);
  return NULL;
}

bool sys_cond_wait(sys_cond_t *cond, sys_mutex_t *mutex) {
  sys_assert(_sys_cond_valid(cond));
  sys_assert(sys_pico_mutex_valid(mutex));

  mutex_enter_blocking(&cond->waiters_lock);
  cond->waiters_count++;
  mutex_exit(&cond->waiters_lock);

  mutex_exit(&mutex->pmutex);
  sem_acquire_blocking(&cond->sem);

  mutex_enter_blocking(&cond->waiters_lock);
  cond->waiters_count--;
  mutex_exit(&cond->waiters_lock);

  mutex_enter_blocking(&mutex->pmutex);
  return true;
}

bool sys_cond_timedwait(sys_cond_t *cond, sys_mutex_t *mutex,
                        uint32_t timeout_ms) {
  sys_assert(_sys_cond_valid(cond));
  sys_assert(sys_pico_mutex_valid(mutex));

  if (timeout_ms == 0) {
    return sys_cond_wait(cond, mutex);
  }

  mutex_enter_blocking(&cond->waiters_lock);
  cond->waiters_count++;
  mutex_exit(&cond->waiters_lock);

  mutex_exit(&mutex->pmutex);
  bool signaled = sem_acquire_timeout_ms(&cond->sem, timeout_ms);

  mutex_enter_blocking(&cond->waiters_lock);
  cond->waiters_count--;
  mutex_exit(&cond->waiters_lock);

  mutex_enter_blocking(&mutex->pmutex);
  return signaled;
}

bool sys_cond_signal(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  mutex_enter_blocking(&cond->waiters_lock);
  bool has_waiters = cond->waiters_count > 0;
  mutex_exit(&cond->waiters_lock);

  if (has_waiters) {
    sem_release(&cond->sem);
  }

  return true;
}

bool sys_cond_broadcast(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  mutex_enter_blocking(&cond->waiters_lock);
  int waiters = cond->waiters_count;
  mutex_exit(&cond->waiters_lock);

  for (int index = 0; index < waiters; index++) {
    sem_release(&cond->sem);
  }

  return true;
}

void sys_cond_deinit(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  sys_cond_broadcast(cond);

  _sys_cond_pool_lock_init();
  critical_section_enter_blocking(&cond_pool_lock);
  cond->waiters_count = 0;
  cond->init = false;
  sem_reset(&cond->sem, 0);
  critical_section_exit(&cond_pool_lock);
}
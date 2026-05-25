#include <pico/critical_section.h>
#include <pico/mutex.h>
#include <picofuse/sys.h>

#include "pico_sys_internal.h"

///////////////////////////////////////////////////////////////////////////////
// TYPES

static critical_section_t mutex_pool_lock;
static bool mutex_pool_lock_init = false;
static sys_mutex_t mutex_pool[SYS_MUTEX_CAPACITY];
static size_t mutex_pool_next_index = 0;

static bool _sys_mutex_valid(const sys_mutex_t *mutex) {
  return sys_pico_mutex_valid(mutex);
}

static void _sys_mutex_pool_lock_init(void) {
  if (!mutex_pool_lock_init) {
    critical_section_init(&mutex_pool_lock);
    mutex_pool_lock_init = true;
  }
}

static bool _sys_mutex_init_handle(sys_mutex_t *mutex) {
  mutex_init(&mutex->pmutex);
  return mutex_is_initialized(&mutex->pmutex);
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

sys_mutex_t *sys_mutex_init(void) {
  _sys_mutex_pool_lock_init();
  critical_section_enter_blocking(&mutex_pool_lock);

  for (size_t offset = 0; offset < SYS_MUTEX_CAPACITY; offset++) {
    size_t index = (mutex_pool_next_index + offset) % SYS_MUTEX_CAPACITY;
    sys_mutex_t *mutex = &mutex_pool[index];
    if (mutex->init) {
      continue;
    }

    mutex->init = true;
    if (!_sys_mutex_init_handle(mutex)) {
      mutex->init = false;
      critical_section_exit(&mutex_pool_lock);
      return NULL;
    }

    mutex_pool_next_index = (index + 1) % SYS_MUTEX_CAPACITY;
    critical_section_exit(&mutex_pool_lock);
    return mutex;
  }

  critical_section_exit(&mutex_pool_lock);
  return NULL;
}

bool sys_mutex_lock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  mutex_enter_blocking(&mutex->pmutex);
  return true;
}

bool sys_mutex_trylock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  return mutex_try_enter(&mutex->pmutex, NULL);
}

bool sys_mutex_unlock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  mutex_exit(&mutex->pmutex);
  return true;
}

void sys_mutex_deinit(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));

  _sys_mutex_pool_lock_init();
  critical_section_enter_blocking(&mutex_pool_lock);
  mutex->init = false;
  critical_section_exit(&mutex_pool_lock);
}

#include "private.h"
#include <pico/critical_section.h>
#include <pico/mutex.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

static critical_section_t _sys_mutex_pool_lock;
static sys_mutex_t _sys_mutex_pool[SYS_MUTEX_CAPACITY];
static size_t _sys_mutex_pool_index = 0;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

void _sys_mutex_module_init(void);
static bool _sys_mutex_init_handle(sys_mutex_t *mutex);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and initializes a mutex from the static pool. */
sys_mutex_t *sys_mutex_init(void) {
  critical_section_enter_blocking(&_sys_mutex_pool_lock);

  for (size_t offset = 0; offset < SYS_MUTEX_CAPACITY; offset++) {
    size_t index = (_sys_mutex_pool_index + offset) % SYS_MUTEX_CAPACITY;
    sys_mutex_t *mutex = &_sys_mutex_pool[index];
    if (mutex->init) {
      continue;
    }

    mutex->init = true;
    if (!_sys_mutex_init_handle(mutex)) {
      mutex->init = false;
      critical_section_exit(&_sys_mutex_pool_lock);
      return NULL;
    }

    _sys_mutex_pool_index = (index + 1) % SYS_MUTEX_CAPACITY;
    critical_section_exit(&_sys_mutex_pool_lock);
    return mutex;
  }

  critical_section_exit(&_sys_mutex_pool_lock);
  return NULL;
}

/** @brief Deinitializes a mutex and returns its pool slot. */
void sys_mutex_deinit(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));

  critical_section_enter_blocking(&_sys_mutex_pool_lock);
  mutex->init = false;
  critical_section_exit(&_sys_mutex_pool_lock);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/** @brief Locks a mutex, blocking until it becomes available. */
bool sys_mutex_lock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  mutex_enter_blocking(&mutex->pmutex);
  return true;
}

/** @brief Attempts to lock a mutex without blocking. */
bool sys_mutex_trylock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  return mutex_try_enter(&mutex->pmutex, NULL);
}

/** @brief Unlocks a previously locked mutex. */
bool sys_mutex_unlock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  mutex_exit(&mutex->pmutex);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Initializes the Pico mutex pool lock. */
void _sys_mutex_module_init(void) {
  critical_section_init(&_sys_mutex_pool_lock);
}

/** @brief Initializes the native Pico mutex stored in a pool slot. */
static bool _sys_mutex_init_handle(sys_mutex_t *mutex) {
  mutex_init(&mutex->pmutex);
  return mutex_is_initialized(&mutex->pmutex);
}

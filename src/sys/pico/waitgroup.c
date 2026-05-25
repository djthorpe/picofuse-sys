#include "private.h"
#include <limits.h>
#include <pico/critical_section.h>
#include <pico/mutex.h>
#include <pico/sem.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_waitgroup_t {
  semaphore_t sem;
  mutex_t lock;
  int counter;
  int waiters;
  bool init;
};

static critical_section_t _sys_waitgroup_pool_lock;
static sys_waitgroup_t _sys_waitgroup_pool[SYS_WAITGROUP_CAPACITY];
static size_t _sys_waitgroup_pool_index = 0;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _sys_waitgroup_valid(const sys_waitgroup_t *wg);
static void _sys_waitgroup_deinit_handle(sys_waitgroup_t *wg);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and initializes a wait group from the static pool. */
sys_waitgroup_t *sys_waitgroup_init(void) {
  critical_section_enter_blocking(&_sys_waitgroup_pool_lock);

  for (size_t offset = 0; offset < SYS_WAITGROUP_CAPACITY; offset++) {
    size_t index =
        (_sys_waitgroup_pool_index + offset) % SYS_WAITGROUP_CAPACITY;
    sys_waitgroup_t *wg = &_sys_waitgroup_pool[index];
    if (wg->init) {
      continue;
    }

    sem_init(&wg->sem, 0, INT16_MAX);
    mutex_init(&wg->lock);
    wg->counter = 0;
    wg->waiters = 0;
    wg->init = mutex_is_initialized(&wg->lock);
    if (!wg->init) {
      critical_section_exit(&_sys_waitgroup_pool_lock);
      return NULL;
    }

    _sys_waitgroup_pool_index = (index + 1) % SYS_WAITGROUP_CAPACITY;
    critical_section_exit(&_sys_waitgroup_pool_lock);
    return wg;
  }

  critical_section_exit(&_sys_waitgroup_pool_lock);
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/** @brief Adds delta to the wait group counter. */
bool sys_waitgroup_add(sys_waitgroup_t *wg, int delta) {
  sys_assert(_sys_waitgroup_valid(wg));

  if (delta < 0) {
    return false;
  }

  mutex_enter_blocking(&wg->lock);

  bool ok = true;
  if (wg->counter > INT_MAX - delta) {
    ok = false;
  } else {
    wg->counter += delta;
  }

  mutex_exit(&wg->lock);
  return ok;
}

/** @brief Decrements the wait group counter by one. */
bool sys_waitgroup_done(sys_waitgroup_t *wg) {
  sys_assert(_sys_waitgroup_valid(wg));

  mutex_enter_blocking(&wg->lock);

  bool ok = true;
  if (wg->counter <= 0) {
    ok = false;
  } else {
    wg->counter--;
    if (wg->counter == 0) {
      for (int index = 0; index < wg->waiters; index++) {
        sem_release(&wg->sem);
      }
    }
  }

  mutex_exit(&wg->lock);
  return ok;
}

/** @brief Waits for the counter to reach zero, then releases the wait group. */
void sys_waitgroup_wait(sys_waitgroup_t *wg) {
  sys_assert(_sys_waitgroup_valid(wg));

  bool should_deinit = false;

  mutex_enter_blocking(&wg->lock);
  if (wg->counter > 0) {
    wg->waiters++;
    mutex_exit(&wg->lock);

    sem_acquire_blocking(&wg->sem);

    mutex_enter_blocking(&wg->lock);
    wg->waiters--;
    should_deinit = wg->waiters == 0;
  } else {
    should_deinit = true;
  }
  mutex_exit(&wg->lock);

  if (should_deinit) {
    _sys_waitgroup_deinit_handle(wg);
  }
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Returns true when a wait group handle is initialized. */
static bool _sys_waitgroup_valid(const sys_waitgroup_t *wg) {
  return wg != NULL && wg->init && mutex_is_initialized((mutex_t *)&wg->lock);
}

/** @brief Initializes the Pico waitgroup pool lock. */
void _sys_waitgroup_module_init(void) {
  critical_section_init(&_sys_waitgroup_pool_lock);
}

/** @brief Releases a wait group slot back to the static pool. */
static void _sys_waitgroup_deinit_handle(sys_waitgroup_t *wg) {
  critical_section_enter_blocking(&_sys_waitgroup_pool_lock);
  wg->counter = 0;
  wg->waiters = 0;
  wg->init = false;
  sem_reset(&wg->sem, 0);
  critical_section_exit(&_sys_waitgroup_pool_lock);
}
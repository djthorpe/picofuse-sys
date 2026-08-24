#include "private.h"
#include <pico/critical_section.h>
#include <pico/mutex.h>
#include <pico/sem.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_cond_t {
  semaphore_t sem;
  int waiters_count;
  int pending_signals;
  bool init;
};

static critical_section_t _sys_cond_pool_lock;
// Shared by every sys_cond_t's waiters-bookkeeping (see sys_cond_signal(),
// sys_cond_broadcast(), sys_cond_wait()/timedwait(), _sys_cond_finish_wait())
// rather than a critical_section_t per cond instance: RP2350 has only 32
// claimable spin locks total, shared with pico-sdk's own subsystems
// (cyw43/lwIP/BTstack/mbedtls among them), and SYS_COND_CAPACITY conds each
// claiming their own would burn through that budget fast. Each waiters_lock
// section here is only a few instructions of bookkeeping, so sharing one
// lock across all conds costs a little extra (harmless) contention, not
// correctness. critical_section_t (not mutex_t) because sys_cond_signal()/
// broadcast() must be safe to call from IRQ context (HID timer/gpio event
// sources push from an alarm/gpio IRQ handler): it disables interrupts for
// its short hold, which structurally rules out a same-core IRQ re-entering
// a lock the interrupted code already holds; mutex_enter_blocking() offers
// no such guarantee.
static critical_section_t _sys_cond_waiters_lock;
static sys_cond_t _sys_cond_pool[SYS_COND_CAPACITY];
static size_t _sys_cond_pool_index = 0;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _sys_cond_valid(const sys_cond_t *cond);
void _sys_cond_module_init(void);
static void _sys_cond_finish_wait(sys_cond_t *cond, bool signaled);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and initializes a condition variable from the static pool.
 */
sys_cond_t *sys_cond_init(void) {
  critical_section_enter_blocking(&_sys_cond_pool_lock);

  for (size_t offset = 0; offset < SYS_COND_CAPACITY; offset++) {
    size_t index = (_sys_cond_pool_index + offset) % SYS_COND_CAPACITY;
    sys_cond_t *cond = &_sys_cond_pool[index];
    if (cond->init) {
      continue;
    }

    sem_init(&cond->sem, 0, INT16_MAX);
    cond->waiters_count = 0;
    cond->pending_signals = 0;
    cond->init = true;

    _sys_cond_pool_index = (index + 1) % SYS_COND_CAPACITY;
    critical_section_exit(&_sys_cond_pool_lock);
    return cond;
  }

  critical_section_exit(&_sys_cond_pool_lock);
  return NULL;
}

/** @brief Deinitializes a condition variable and returns its pool slot. */
void sys_cond_deinit(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  sys_cond_broadcast(cond);

  critical_section_enter_blocking(&_sys_cond_pool_lock);
  cond->waiters_count = 0;
  cond->pending_signals = 0;
  cond->init = false;
  sem_reset(&cond->sem, 0);
  critical_section_exit(&_sys_cond_pool_lock);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/** @brief Waits until the condition variable is signaled. */
bool sys_cond_wait(sys_cond_t *cond, sys_mutex_t *mutex) {
  sys_assert(_sys_cond_valid(cond));
  sys_assert(_sys_mutex_valid(mutex));

  critical_section_enter_blocking(&_sys_cond_waiters_lock);
  cond->waiters_count++;
  critical_section_exit(&_sys_cond_waiters_lock);

  mutex_exit(&mutex->pmutex);
  sem_acquire_blocking(&cond->sem);

  _sys_cond_finish_wait(cond, true);

  mutex_enter_blocking(&mutex->pmutex);
  return true;
}

/** @brief Waits until signaled or the timeout expires. */
bool sys_cond_timedwait(sys_cond_t *cond, sys_mutex_t *mutex,
                        uint32_t timeout_ms) {
  sys_assert(_sys_cond_valid(cond));
  sys_assert(_sys_mutex_valid(mutex));

  if (timeout_ms == 0) {
    return sys_cond_wait(cond, mutex);
  }

  critical_section_enter_blocking(&_sys_cond_waiters_lock);
  cond->waiters_count++;
  critical_section_exit(&_sys_cond_waiters_lock);

  mutex_exit(&mutex->pmutex);
  bool signaled = sem_acquire_timeout_ms(&cond->sem, timeout_ms);

  _sys_cond_finish_wait(cond, signaled);

  mutex_enter_blocking(&mutex->pmutex);
  return signaled;
}

/** @brief Wakes one waiting thread when a waiter is present. */
bool sys_cond_signal(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  critical_section_enter_blocking(&_sys_cond_waiters_lock);
  bool has_waiters = cond->waiters_count > cond->pending_signals;

  if (has_waiters) {
    cond->pending_signals++;
    sem_release(&cond->sem);
  }

  critical_section_exit(&_sys_cond_waiters_lock);

  return true;
}

/** @brief Wakes all currently waiting threads. */
bool sys_cond_broadcast(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  critical_section_enter_blocking(&_sys_cond_waiters_lock);
  int waiters = cond->waiters_count - cond->pending_signals;
  cond->pending_signals += waiters;

  for (int index = 0; index < waiters; index++) {
    sem_release(&cond->sem);
  }

  critical_section_exit(&_sys_cond_waiters_lock);

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Returns true when a condition variable handle is initialized. */
static bool _sys_cond_valid(const sys_cond_t *cond) {
  return cond != NULL && cond->init;
}

/** @brief Initializes the Pico condition-variable pool lock. */
void _sys_cond_module_init(void) {
  critical_section_init(&_sys_cond_pool_lock);
  critical_section_init(&_sys_cond_waiters_lock);
}

/** @brief Reconciles waiter and signal accounting after a wait returns. */
static void _sys_cond_finish_wait(sys_cond_t *cond, bool signaled) {
  critical_section_enter_blocking(&_sys_cond_waiters_lock);
  cond->waiters_count--;

  if (signaled) {
    if (cond->pending_signals > 0) {
      cond->pending_signals--;
    }
  } else if (cond->pending_signals > cond->waiters_count) {
    cond->pending_signals--;
    (void)sem_try_acquire(&cond->sem);
  }

  critical_section_exit(&_sys_cond_waiters_lock);
}

#include <errno.h>
#include <picofuse/sys.h>
#include <pthread.h>
#include <time.h>

#include "mutex.h"

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_cond_t {
  pthread_cond_t pcond;
  bool init;
};

static pthread_mutex_t cond_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static sys_cond_t cond_pool[SYS_COND_CAPACITY];
static size_t cond_pool_next_index = 0;

static bool _sys_cond_valid(const sys_cond_t *cond) {
  return cond != NULL && cond->init;
}

static bool _sys_cond_init_handle(sys_cond_t *cond) {
#if !defined(__APPLE__) && defined(CLOCK_MONOTONIC) &&                         \
    defined(_POSIX_CLOCK_SELECTION) && (_POSIX_CLOCK_SELECTION >= 0)
  pthread_condattr_t attr;
  if (pthread_condattr_init(&attr) != 0) {
    return false;
  }

  if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) != 0) {
    pthread_condattr_destroy(&attr);
    return false;
  }

  int result = pthread_cond_init(&cond->pcond, &attr);
  pthread_condattr_destroy(&attr);
  return result == 0;
#else
  return pthread_cond_init(&cond->pcond, NULL) == 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

sys_cond_t *sys_cond_init(void) {
  if (pthread_mutex_lock(&cond_pool_lock) != 0) {
    return NULL;
  }

  for (size_t offset = 0; offset < SYS_COND_CAPACITY; offset++) {
    size_t index = (cond_pool_next_index + offset) % SYS_COND_CAPACITY;
    sys_cond_t *cond = &cond_pool[index];
    if (cond->init) {
      continue;
    }

    cond->init = true;
    if (!_sys_cond_init_handle(cond)) {
      cond->init = false;
      pthread_mutex_unlock(&cond_pool_lock);
      return NULL;
    }

    cond_pool_next_index = (index + 1) % SYS_COND_CAPACITY;
    pthread_mutex_unlock(&cond_pool_lock);
    return cond;
  }

  pthread_mutex_unlock(&cond_pool_lock);
  return NULL;
}

void sys_cond_deinit(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));

  int lock_result = pthread_mutex_lock(&cond_pool_lock);
  sys_assert(lock_result == 0);
  if (lock_result != 0) {
    return;
  }

  int destroy_result = pthread_cond_destroy(&cond->pcond);
  sys_assert(destroy_result == 0);
  if (destroy_result != 0) {
    pthread_mutex_unlock(&cond_pool_lock);
    return;
  }

  cond->init = false;

  int unlock_result = pthread_mutex_unlock(&cond_pool_lock);
  sys_assert(unlock_result == 0);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

bool sys_cond_wait(sys_cond_t *cond, sys_mutex_t *mutex) {
  sys_assert(_sys_cond_valid(cond));
  sys_assert(_sys_mutex_valid(mutex));
  return pthread_cond_wait(&cond->pcond, &mutex->pmutex) == 0;
}

bool sys_cond_timedwait(sys_cond_t *cond, sys_mutex_t *mutex,
                        uint32_t timeout_ms) {
  sys_assert(_sys_cond_valid(cond));
  sys_assert(_sys_mutex_valid(mutex));

  if (timeout_ms == 0) {
    return sys_cond_wait(cond, mutex);
  }

#if defined(__APPLE__)
  struct timespec rel_timeout = {
      .tv_sec = (time_t)(timeout_ms / 1000),
      .tv_nsec = (long)(timeout_ms % 1000) * 1000000L,
  };

  int result = pthread_cond_timedwait_relative_np(&cond->pcond, &mutex->pmutex,
                                                  &rel_timeout);
  return result == 0;
#else
  struct timespec abs_timeout;
  if (clock_gettime(CLOCK_MONOTONIC, &abs_timeout) != 0) {
    return false;
  }

  abs_timeout.tv_sec += timeout_ms / 1000;
  abs_timeout.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;

  if (abs_timeout.tv_nsec >= 1000000000L) {
    abs_timeout.tv_sec += 1;
    abs_timeout.tv_nsec -= 1000000000L;
  }

  int result =
      pthread_cond_timedwait(&cond->pcond, &mutex->pmutex, &abs_timeout);
  return result == 0;
#endif
}

bool sys_cond_signal(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));
  return pthread_cond_signal(&cond->pcond) == 0;
}

bool sys_cond_broadcast(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));
  return pthread_cond_broadcast(&cond->pcond) == 0;
}

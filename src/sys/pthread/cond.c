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
    if (pthread_cond_init(&cond->pcond, NULL) != 0) {
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

  if (pthread_mutex_lock(&cond_pool_lock) != 0) {
    return;
  }
  if (pthread_cond_destroy(&cond->pcond) != 0) {
    pthread_mutex_unlock(&cond_pool_lock);
    return;
  }
  cond->init = false;
  pthread_mutex_unlock(&cond_pool_lock);
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

  struct timespec abs_timeout;
  if (clock_gettime(CLOCK_REALTIME, &abs_timeout) != 0) {
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
}

bool sys_cond_signal(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));
  return pthread_cond_signal(&cond->pcond) == 0;
}

bool sys_cond_broadcast(sys_cond_t *cond) {
  sys_assert(_sys_cond_valid(cond));
  return pthread_cond_broadcast(&cond->pcond) == 0;
}

#include <picofuse/sys.h>
#include <pthread.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_mutex_t {
  pthread_mutex_t pmutex;
  bool init;
};

static pthread_mutex_t mutex_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static sys_mutex_t mutex_pool[SYS_MUTEX_CAPACITY];
static size_t mutex_pool_next_index = 0;

static bool _sys_mutex_valid(const sys_mutex_t *mutex) {
  return mutex != NULL && mutex->init;
}

static bool _sys_mutex_init_handle(sys_mutex_t *mutex) {
  pthread_mutexattr_t attr;
  if (pthread_mutexattr_init(&attr) != 0) {
    return false;
  }

  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0) {
    pthread_mutexattr_destroy(&attr);
    return false;
  }

  int result = pthread_mutex_init(&mutex->pmutex, &attr);
  pthread_mutexattr_destroy(&attr);
  return result == 0;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a new mutex
 */
sys_mutex_t *sys_mutex_init(void) {
  if (pthread_mutex_lock(&mutex_pool_lock) != 0) {
    return NULL;
  }

  for (size_t offset = 0; offset < SYS_MUTEX_CAPACITY; offset++) {
    size_t index = (mutex_pool_next_index + offset) % SYS_MUTEX_CAPACITY;
    sys_mutex_t *mutex = &mutex_pool[index];
    if (mutex->init) {
      continue;
    }

    mutex->init = true;
    if (!_sys_mutex_init_handle(mutex)) {
      mutex->init = false;
      pthread_mutex_unlock(&mutex_pool_lock);
      return NULL;
    }

    mutex_pool_next_index = (index + 1) % SYS_MUTEX_CAPACITY;
    pthread_mutex_unlock(&mutex_pool_lock);
    return mutex;
  }

  pthread_mutex_unlock(&mutex_pool_lock);
  return NULL;
}

/**
 * @brief Lock a mutex, by blocking
 */
bool sys_mutex_lock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  return pthread_mutex_lock(&mutex->pmutex) == 0;
}

/**
 * @brief Try to lock a mutex
 */
bool sys_mutex_trylock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  return pthread_mutex_trylock(&mutex->pmutex) == 0;
}

/**
 * @brief Unlock a mutex
 */
bool sys_mutex_unlock(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));
  return pthread_mutex_unlock(&mutex->pmutex) == 0;
}

/**
 * @brief Deinitialize a mutex
 */
void sys_mutex_deinit(sys_mutex_t *mutex) {
  sys_assert(_sys_mutex_valid(mutex));

  if (pthread_mutex_lock(&mutex_pool_lock) != 0) {
    return;
  }
  if (pthread_mutex_destroy(&mutex->pmutex) != 0) {
    pthread_mutex_unlock(&mutex_pool_lock);
    return;
  }
  mutex->init = false;
  pthread_mutex_unlock(&mutex_pool_lock);
}

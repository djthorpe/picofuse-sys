/**
 * @file thread.c
 * @brief pthread-based thread management implementation.
 */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <picofuse/sys.h>

#include <pthread.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <sched.h>
#include <sys/sysinfo.h>
#endif

typedef struct {
  sys_thread_func_t func;
  void *arg;
  bool in_use;
} thread_wrapper_t;

static pthread_mutex_t thread_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static thread_wrapper_t thread_pool[SYS_THREAD_CAPACITY];
static size_t thread_pool_next_index = 0;

static uint8_t sys_thread_clamp_core_count(long count) {
  if (count <= 0) {
    return 1;
  }

  return (uint8_t)(count > UINT8_MAX ? UINT8_MAX : count);
}

static void sys_thread_release_wrapper(thread_wrapper_t *wrapper) {
  sys_assert(wrapper != NULL);
  sys_assert(pthread_mutex_lock(&thread_pool_lock) == 0);
  wrapper->func = NULL;
  wrapper->arg = NULL;
  wrapper->in_use = false;
  pthread_mutex_unlock(&thread_pool_lock);
}

static thread_wrapper_t *sys_thread_claim_wrapper(sys_thread_func_t func,
                                                  void *arg) {
  sys_assert(func != NULL);
  if (pthread_mutex_lock(&thread_pool_lock) != 0) {
    return NULL;
  }

  for (size_t offset = 0; offset < SYS_THREAD_CAPACITY; offset++) {
    size_t index = (thread_pool_next_index + offset) % SYS_THREAD_CAPACITY;
    thread_wrapper_t *wrapper = &thread_pool[index];
    if (wrapper->in_use) {
      continue;
    }

    wrapper->func = func;
    wrapper->arg = arg;
    wrapper->in_use = true;
    thread_pool_next_index = (index + 1) % SYS_THREAD_CAPACITY;
    pthread_mutex_unlock(&thread_pool_lock);
    return wrapper;
  }

  pthread_mutex_unlock(&thread_pool_lock);
  return NULL;
}

static void *thread_wrapper(void *arg) {
  thread_wrapper_t *wrapper = (thread_wrapper_t *)arg;
  sys_thread_func_t func = wrapper->func;
  void *thread_arg = wrapper->arg;

  sys_thread_release_wrapper(wrapper);
  func(thread_arg);
  return NULL;
}

static bool sys_thread_create_with_attr(sys_thread_func_t func, void *arg,
                                        pthread_attr_t *attr,
                                        pthread_t *thread_out) {
  thread_wrapper_t *wrapper = sys_thread_claim_wrapper(func, arg);
  if (wrapper == NULL) {
    return false;
  }

  int result = pthread_create(thread_out, attr, thread_wrapper, wrapper);
  if (result != 0) {
    sys_thread_release_wrapper(wrapper);
    return false;
  }

  return true;
}

uint8_t sys_thread_numcores(void) {
#ifdef __APPLE__
  int mac_cores = 0;
  size_t len = sizeof(mac_cores);
  if (sysctlbyname("hw.ncpu", &mac_cores, &len, NULL, 0) == 0) {
    return sys_thread_clamp_core_count(mac_cores);
  }
#elif defined(__linux__)
  int linux_cores = get_nprocs();
  if (linux_cores > 0) {
    return sys_thread_clamp_core_count(linux_cores);
  }
#endif

  return sys_thread_clamp_core_count(sysconf(_SC_NPROCESSORS_ONLN));
}

bool sys_thread_create(sys_thread_func_t func, void *arg) {
  if (func == NULL) {
    return false;
  }

  pthread_t thread;
  pthread_attr_t attr;

  if (pthread_attr_init(&attr) != 0) {
    return false;
  }

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
    pthread_attr_destroy(&attr);
    return false;
  }

  bool ok = sys_thread_create_with_attr(func, arg, &attr, &thread);
  pthread_attr_destroy(&attr);
  return ok;
}

bool sys_thread_create_on_core(sys_thread_func_t func, void *arg,
                               uint8_t core) {
  if (func == NULL || core >= sys_thread_numcores()) {
    return false;
  }

  pthread_t thread;
  pthread_attr_t attr;

  if (pthread_attr_init(&attr) != 0) {
    return false;
  }

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
    pthread_attr_destroy(&attr);
    return false;
  }

  bool ok = sys_thread_create_with_attr(func, arg, &attr, &thread);
  pthread_attr_destroy(&attr);
  if (!ok) {
    return false;
  }

#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  (void)pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
#else
  (void)core;
#endif

  return true;
}

uint8_t sys_thread_core(void) {
#ifdef __linux__
  int cpu = sched_getcpu();
  if (cpu >= 0) {
    return (uint8_t)(cpu > UINT8_MAX ? UINT8_MAX : cpu);
  }
#endif

  return 0;
}

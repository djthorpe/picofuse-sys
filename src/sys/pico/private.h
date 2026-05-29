#pragma once
#include <pico/mutex.h>
#include <picofuse/sys.h>
#include <stdbool.h>

struct sys_mutex_t {
  mutex_t pmutex;
  bool init;
};

static inline bool _sys_mutex_valid(const sys_mutex_t *mutex) {
  return mutex != NULL && mutex->init &&
         mutex_is_initialized((mutex_t *)&mutex->pmutex);
}

/**
 * @brief Initializes the mutex subsystem
 */
extern void _sys_mutex_module_init(void);

/**
 * @brief Initializes the cond subsystem
 */
extern void _sys_cond_module_init(void);

/**
 * @brief Initializes the hash subsystem
 */
extern void _sys_hash_module_init(void);

/**
 * @brief Initializes the waitgroup subsystem
 */
extern void _sys_waitgroup_module_init(void);

/**
 * @brief Initializes the timer subsystem
 */
extern void _sys_timer_module_init(void);

/**
 * @brief Deinitializes the timer subsystem
 */
extern void _sys_timer_module_exit(void);

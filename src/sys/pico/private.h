#pragma once
#include <pico/mutex.h>
#include <picofuse/sys.h>
#include <stdbool.h>

struct sys_mutex_t {
  mutex_t pmutex;
  bool init;
};

static inline bool sys_pico_mutex_valid(const sys_mutex_t *mutex) {
  return mutex != NULL && mutex->init &&
         mutex_is_initialized((mutex_t *)&mutex->pmutex);
}

void sys_pico_mutex_module_init(void);
void sys_pico_cond_module_init(void);
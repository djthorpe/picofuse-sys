#pragma once

#include <pico/mutex.h>
#include <stdbool.h>

typedef struct sys_mutex_t sys_mutex_t;

struct sys_mutex_t {
  mutex_t pmutex;
  bool init;
};

static inline bool sys_pico_mutex_valid(const sys_mutex_t *mutex) {
  return mutex != NULL && mutex->init &&
         mutex_is_initialized((mutex_t *)&mutex->pmutex);
}
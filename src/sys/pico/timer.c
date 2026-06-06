#include "private.h"
#include <pico/critical_section.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <picofuse/sys.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_timer_t {
  void (*callback)(sys_timer_t *);
  uint32_t interval_ms;
  void *userdata;
  repeating_timer_t repeating_timer;
  bool init;
  bool running;
  bool callback_active;
};

static critical_section_t _sys_timer_pool_lock;
static sys_timer_t _sys_timer_pool[SYS_TIMER_CAPACITY];
static size_t _sys_timer_pool_index = 0;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _sys_timer_callback(repeating_timer_t *rt) {
  sys_timer_t *timer = (sys_timer_t *)rt->user_data;
  if (timer == NULL) {
    return false;
  }

  void (*callback)(sys_timer_t *) = NULL;
  critical_section_enter_blocking(&_sys_timer_pool_lock);
  if (timer->running && timer->callback != NULL && !timer->callback_active) {
    timer->callback_active = true;
    callback = timer->callback;
  }
  critical_section_exit(&_sys_timer_pool_lock);

  if (callback != NULL) {
    callback(timer);

    critical_section_enter_blocking(&_sys_timer_pool_lock);
    timer->callback_active = false;
    critical_section_exit(&_sys_timer_pool_lock);
  }

  return timer->running;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

void _sys_timer_module_init(void) {
  critical_section_init(&_sys_timer_pool_lock);
}

void _sys_timer_module_exit(void) {
  for (size_t i = 0; i < SYS_TIMER_CAPACITY; i++) {
    sys_timer_deinit(&_sys_timer_pool[i]);
  }
}

sys_timer_t *sys_timer_init(uint32_t interval_ms, void *userdata,
                            void (*callback)(sys_timer_t *)) {
  if (interval_ms == 0 || callback == NULL) {
    return NULL;
  }

  critical_section_enter_blocking(&_sys_timer_pool_lock);

  for (size_t offset = 0; offset < SYS_TIMER_CAPACITY; offset++) {
    size_t index = (_sys_timer_pool_index + offset) % SYS_TIMER_CAPACITY;
    sys_timer_t *timer = &_sys_timer_pool[index];
    if (timer->init || timer->callback_active) {
      continue;
    }

    timer->callback = callback;
    timer->interval_ms = interval_ms;
    timer->userdata = userdata;
    timer->running = false;
    timer->callback_active = false;
    timer->init = true;

    _sys_timer_pool_index = (index + 1) % SYS_TIMER_CAPACITY;
    critical_section_exit(&_sys_timer_pool_lock);
    return timer;
  }

  critical_section_exit(&_sys_timer_pool_lock);
  return NULL;
}

void sys_timer_deinit(sys_timer_t *timer) {
  if (timer == NULL || !timer->init) {
    return;
  }

  bool in_callback = __get_current_exception() != 0;

  if (timer->running) {
    timer->running = false;
    cancel_repeating_timer(&timer->repeating_timer);
  }

  if (!in_callback) {
    while (true) {
      critical_section_enter_blocking(&_sys_timer_pool_lock);
      bool active = timer->callback_active;
      critical_section_exit(&_sys_timer_pool_lock);

      if (!active) {
        break;
      }

      sleep_ms(1u);
    }
  }

  critical_section_enter_blocking(&_sys_timer_pool_lock);
  timer->init = false;
  if (!in_callback) {
    timer->callback_active = false;
  }
  critical_section_exit(&_sys_timer_pool_lock);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool sys_timer_start(sys_timer_t *timer) {
  if (timer == NULL || !timer->init || timer->running) {
    return false;
  }

  alarm_pool_t *pool = alarm_pool_get_default();
  if (pool == NULL) {
    return false;
  }

  timer->running = true;
  if (!alarm_pool_add_repeating_timer_ms(pool, (int32_t)timer->interval_ms,
                                         _sys_timer_callback, timer,
                                         &timer->repeating_timer)) {
    timer->running = false;
    return false;
  }

  return true;
}

bool sys_timer_valid(sys_timer_t *timer) {
  return timer != NULL && timer->init && timer->running;
}

void *sys_timer_get_userdata(sys_timer_t *timer) {
  if (timer == NULL || !timer->init) {
    return NULL;
  }
  return timer->userdata;
}

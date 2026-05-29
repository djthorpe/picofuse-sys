#include <dispatch/dispatch.h>
#include <picofuse/sys.h>
#include <pthread.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_timer_t {
  void (*callback)(sys_timer_t *);
  uint32_t interval_ms;
  void *userdata;
  dispatch_source_t source;
  bool init;
};

static pthread_mutex_t _sys_timer_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static sys_timer_t _sys_timer_pool[SYS_TIMER_CAPACITY];
static size_t _sys_timer_pool_index = 0;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE (module)

void _sys_timer_module_exit(void) {
  for (size_t i = 0; i < SYS_TIMER_CAPACITY; i++) {
    sys_timer_deinit(&_sys_timer_pool[i]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _sys_timer_callback(void *context) {
  sys_timer_t *timer = (sys_timer_t *)context;
  if (timer->callback != NULL) {
    timer->callback(timer);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

sys_timer_t *sys_timer_init(uint32_t interval_ms, void *userdata,
                            void (*callback)(sys_timer_t *)) {
  if (interval_ms == 0 || callback == NULL) {
    return NULL;
  }

  pthread_mutex_lock(&_sys_timer_pool_lock);

  for (size_t offset = 0; offset < SYS_TIMER_CAPACITY; offset++) {
    size_t index = (_sys_timer_pool_index + offset) % SYS_TIMER_CAPACITY;
    sys_timer_t *timer = &_sys_timer_pool[index];
    if (timer->init) {
      continue;
    }

    timer->callback = callback;
    timer->interval_ms = interval_ms;
    timer->userdata = userdata;
    timer->source = NULL;
    timer->init = true;

    _sys_timer_pool_index = (index + 1) % SYS_TIMER_CAPACITY;
    pthread_mutex_unlock(&_sys_timer_pool_lock);
    return timer;
  }

  pthread_mutex_unlock(&_sys_timer_pool_lock);
  return NULL;
}

void sys_timer_deinit(sys_timer_t *timer) {
  if (timer == NULL || !timer->init) {
    return;
  }

  if (timer->source != NULL) {
    dispatch_source_cancel(timer->source);
    dispatch_release(timer->source);
    timer->source = NULL;
  }

  pthread_mutex_lock(&_sys_timer_pool_lock);
  timer->init = false;
  pthread_mutex_unlock(&_sys_timer_pool_lock);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool sys_timer_start(sys_timer_t *timer) {
  if (timer == NULL || !timer->init || timer->source != NULL) {
    return false;
  }

  dispatch_queue_t queue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
  dispatch_source_t source =
      dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
  if (source == NULL) {
    return false;
  }

  int64_t interval_ns = (int64_t)timer->interval_ms * 1000000LL;
  dispatch_source_set_timer(source, dispatch_time(DISPATCH_TIME_NOW, interval_ns),
                            (uint64_t)interval_ns, 0);
  dispatch_set_context(source, timer);
  dispatch_source_set_event_handler_f(source, _sys_timer_callback);

  timer->source = source;
  dispatch_resume(source);
  return true;
}

bool sys_timer_valid(sys_timer_t *timer) {
  return timer != NULL && timer->init && timer->source != NULL;
}

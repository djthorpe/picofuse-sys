#include <picofuse/sys.h>
#include <stddef.h>

#define _POLL_INTERVAL_MS 10

///////////////////////////////////////////////////////////////////////////////
// STATE

static sys_event_queue_t *_queue = NULL;
static sys_runloop_func_t _callback = NULL;
static sys_runloop_poll_t _poll = NULL;
static sys_runloop_init_func_t _init = NULL;
static sys_runloop_exit_func_t _exit = NULL;
static sys_atomic_t _exit_value = {0};
static sys_atomic_t _running = {
    0}; // accepting new events (cleared by shutdown)
static sys_atomic_t _active = {
    0}; // sys_runloop_run is executing (cleared on return)
static sys_waitgroup_t *_wg = NULL;
static bool _owns_queue = false;

///////////////////////////////////////////////////////////////////////////////
// WORKER (index >= 1)

typedef struct {
  uint8_t worker_index;
} _worker_ctx_t;

static void _worker(void *arg) {
  _worker_ctx_t *ctx = (_worker_ctx_t *)arg;
  uint8_t idx = ctx->worker_index;
  sys_free(ctx);

  if (_init != NULL) {
    _init(idx);
  }

  sys_debugf("[sys] runloop: worker %u started", idx);

  sys_event_t event;
  while ((event = sys_event_queue_pop(_queue)) != NULL) {
    _callback(event);
  }

  sys_debugf("[sys] runloop: worker %u exiting", idx);
  if (_exit != NULL) {
    _exit(idx);
  }

  sys_waitgroup_done(_wg);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC

uint32_t sys_runloop_run_with_queue(uint8_t num_workers,
                                    sys_event_queue_t *queue,
                                    sys_runloop_init_func_t init,
                                    sys_runloop_func_t callback,
                                    sys_runloop_poll_t poll_fn,
                                    sys_runloop_exit_func_t exit_fn) {
  if (callback == NULL || sys_atomic_get(&_active)) {
    return 0;
  }
  if (queue != NULL && !sys_event_queue_valid(queue)) {
    return 0;
  }
  sys_atomic_set(&_active, 1);

  uint8_t ncores = sys_thread_numcores();
  if (num_workers == 0 || num_workers > ncores) {
    num_workers = ncores;
  }

  _callback = callback;
  _poll = poll_fn;
  _init = init;
  _exit = exit_fn;
  sys_atomic_set(&_exit_value, 0);

  if (queue != NULL) {
    _queue = queue;
    _owns_queue = false;
  } else {
    _queue = sys_event_queue_init(SYS_RUNLOOP_QUEUE_CAPACITY);
    _owns_queue = true;
    if (_queue == NULL) {
      sys_atomic_set(&_active, 0);
      return 0;
    }
  }

  sys_atomic_set(&_running, 1);

  // Start workers 1..num_workers-1 on additional cores/threads.
  // Add to the waitgroup before starting each thread so the counter is already
  // incremented when the worker calls sys_waitgroup_done(). Roll back with
  // sys_waitgroup_done() if thread creation fails. Fall back to
  // sys_thread_create on platforms where core pinning is unsupported.
  _wg = sys_waitgroup_init();
  if (_wg == NULL) {
    num_workers = 1;
  }
  for (uint8_t i = 1; i < num_workers; i++) {
    _worker_ctx_t *ctx = sys_calloc(1, sizeof(_worker_ctx_t));
    if (ctx == NULL) {
      continue;
    }
    ctx->worker_index = i;
    if (!sys_waitgroup_add(_wg, 1)) {
      sys_free(ctx);
      continue;
    }
    bool started = sys_thread_create_on_core(_worker, ctx, i);
    if (!started) {
      started = sys_thread_create(_worker, ctx);
    }
    if (!started) {
      sys_free(ctx);
      sys_waitgroup_done(_wg);
    }
  }

  // Worker 0: calling thread, optionally drives poll callback between events.
  if (_init != NULL) {
    _init(0);
  }

  sys_debugf("[sys] runloop: main thread starting");
  while (true) {
    sys_event_t event = sys_event_queue_timed_pop(_queue, _POLL_INTERVAL_MS);
    if (_poll != NULL) {
      _poll();
    }
    if (event != NULL) {
      _callback(event);
    } else if (!sys_atomic_get(&_running) && sys_event_queue_empty(_queue)) {
      break;
    }
  }
  sys_debugf("[sys] runloop: main thread exiting");

  if (_exit != NULL) {
    _exit(0);
  }

  // Wait for all other workers to drain and exit
  if (_wg != NULL) {
    sys_waitgroup_wait(_wg);
    _wg = NULL;
  }

  uint32_t result = sys_atomic_get(&_exit_value);
  if (_owns_queue) {
    sys_event_queue_deinit(_queue);
  }
  _queue = NULL;
  _owns_queue = false;
  _poll = NULL;
  sys_atomic_set(&_active, 0);

  return result;
}

uint32_t sys_runloop_run(uint8_t num_workers, sys_runloop_init_func_t init,
                         sys_runloop_func_t callback,
                         sys_runloop_poll_t poll_fn,
                         sys_runloop_exit_func_t exit_fn) {
  return sys_runloop_run_with_queue(num_workers, NULL, init, callback, poll_fn,
                                    exit_fn);
}

void sys_runloop_shutdown(uint32_t exit_value) {
  if (!sys_atomic_get(&_running)) {
    return;
  }
  sys_atomic_set(&_exit_value, exit_value);
  sys_atomic_set(&_running, 0);
  if (_queue != NULL) {
    sys_event_queue_shutdown(_queue);
  }
}

bool sys_runloop_post(sys_event_t event) {
  if (_queue == NULL || !sys_atomic_get(&_running)) {
    return false;
  }
  return sys_event_queue_try_push(_queue, event);
}

sys_event_queue_t *sys_runloop_queue(void) { return _queue; }

bool sys_runloop_valid(void) { return sys_atomic_get(&_active) != 0; }

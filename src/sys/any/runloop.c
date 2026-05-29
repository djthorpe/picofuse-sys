#include <picofuse/sys.h>
#include <stddef.h>

// Weak no-op stubs — overridden by the hw and net modules when linked.
// Defining them here (rather than declaring as weak extern) ensures the
// runloop links cleanly on all platforms regardless of which modules are present.
__attribute__((weak)) void hw_poll(void) {}
__attribute__((weak)) void net_poll(void) {}

#define _POLL_INTERVAL_MS 10

///////////////////////////////////////////////////////////////////////////////
// STATE

static sys_event_queue_t *_queue = NULL;
static sys_runloop_func_t _callback = NULL;
static sys_runloop_init_func_t _init = NULL;
static sys_runloop_exit_func_t _exit = NULL;
static uint32_t _exit_value = 0;
static sys_atomic_t _running = {0};
static sys_waitgroup_t *_wg = NULL;

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

  sys_event_t event;
  while ((event = sys_event_queue_pop(_queue)) != NULL) {
    _callback(event);
  }

  if (_exit != NULL) {
    _exit(idx);
  }

  sys_waitgroup_done(_wg);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC

uint32_t sys_runloop_run(uint8_t num_workers, sys_runloop_init_func_t init,
                         sys_runloop_func_t callback,
                         sys_runloop_exit_func_t exit_fn) {
  if (callback == NULL) {
    return 0;
  }

  uint8_t ncores = sys_thread_numcores();
  if (num_workers == 0 || num_workers > ncores) {
    num_workers = ncores;
  }

  _callback = callback;
  _init = init;
  _exit = exit_fn;
  _exit_value = 0;
  sys_atomic_set(&_running, 1);

  _queue = sys_event_queue_init(SYS_RUNLOOP_QUEUE_CAPACITY);
  if (_queue == NULL) {
    sys_atomic_set(&_running, 0);
    return 0;
  }

  // Start workers 1..num_workers-1 on additional cores/threads.
  // Add to the waitgroup only after the thread successfully starts so the
  // counter stays consistent if creation fails. Fall back to sys_thread_create
  // on platforms where core pinning is unsupported.
  _wg = sys_waitgroup_init();
  for (uint8_t i = 1; i < num_workers; i++) {
    _worker_ctx_t *ctx = sys_calloc(1, sizeof(_worker_ctx_t));
    if (ctx == NULL) {
      continue;
    }
    ctx->worker_index = i;
    bool started = sys_thread_create_on_core(_worker, ctx, i);
    if (!started) {
      started = sys_thread_create(_worker, ctx);
    }
    if (started) {
      sys_waitgroup_add(_wg, 1);
    } else {
      sys_free(ctx);
    }
  }

  // Worker 0: calling thread, drives hw_poll() between events
  if (_init != NULL) {
    _init(0);
  }

  while (true) {
    sys_event_t event = sys_event_queue_timed_pop(_queue, _POLL_INTERVAL_MS);
    hw_poll();
    net_poll();
    if (event != NULL) {
      _callback(event);
    } else if (!sys_atomic_get(&_running) && sys_event_queue_empty(_queue)) {
      break;
    }
  }

  if (_exit != NULL) {
    _exit(0);
  }

  // Wait for all other workers to drain and exit
  sys_waitgroup_wait(_wg);
  _wg = NULL;

  uint32_t result = _exit_value;
  sys_event_queue_deinit(_queue);
  _queue = NULL;

  return result;
}

void sys_runloop_shutdown(uint32_t exit_value) {
  if (!sys_atomic_get(&_running)) {
    return;
  }
  _exit_value = exit_value;
  sys_atomic_set(&_running, 0);
  if (_queue != NULL) {
    sys_event_queue_shutdown(_queue);
  }
}

bool sys_runloop_post(sys_event_t event) {
  if (_queue == NULL || !sys_atomic_get(&_running)) {
    return false;
  }
  return sys_event_queue_push(_queue, event);
}

bool sys_runloop_valid(void) {
  return sys_atomic_get(&_running) != 0;
}

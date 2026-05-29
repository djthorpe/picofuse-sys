/**
 * @file sys/runloop.h
 * @brief Single process-wide run loop for dispatching events across cores or threads.
 * @defgroup SystemEventRunloop Run Loop
 * @ingroup SystemEvents
 *
 * The run loop is a process-wide singleton that drives an event queue across
 * one or more workers. Call sys_runloop_run() from the main thread with an
 * event handler; it blocks until sys_runloop_shutdown() is called. Post
 * events from any thread or interrupt context with sys_runloop_post().
 *
 * @code
 *   static void on_event(sys_event_t event) {
 *     // handle event
 *     if (done) {
 *       sys_runloop_shutdown();
 *     }
 *   }
 *
 *   sys_runloop_post((sys_event_t)(uintptr_t)MY_EVENT);
 *   sys_runloop_run(2, on_event); // blocks; uses 2 workers
 * @endcode
 *
 * On Pico the calling thread counts as one worker; additional workers are
 * pinned to subsequent cores. On other platforms each additional worker is
 * a new thread.
 */
#pragma once

#include "event.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def SYS_RUNLOOP_QUEUE_CAPACITY
 * @ingroup SystemEventRunloop
 * @brief Maximum number of pending events.
 */
#ifndef SYS_RUNLOOP_QUEUE_CAPACITY
#define SYS_RUNLOOP_QUEUE_CAPACITY 32
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Per-worker initialisation callback.
 * @ingroup SystemEventRunloop
 * @param worker_index Zero-based index of the worker (0 = calling thread /
 *                     core 0, 1 = first additional worker, etc.).
 *
 * Called once on each worker before it starts draining the event queue. Use
 * this to register IRQ handlers, allocate thread-local resources, or perform
 * any other per-core setup. May be `NULL` if no initialisation is needed.
 */
typedef void (*sys_runloop_init_func_t)(uint8_t worker_index);

/**
 * @brief Event handler called on a worker for each posted event.
 * @ingroup SystemEventRunloop
 * @param event The event posted via sys_runloop_post().
 */
typedef void (*sys_runloop_func_t)(sys_event_t event);

/**
 * @brief Per-worker exit callback.
 * @ingroup SystemEventRunloop
 * @param worker_index Zero-based index of the worker.
 *
 * Called once on each worker after the event queue is drained and before the
 * worker exits. Use this to deregister IRQ handlers or release thread-local
 * resources. May be `NULL` if no cleanup is needed.
 */
typedef void (*sys_runloop_exit_func_t)(uint8_t worker_index);

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Start the run loop and block until shutdown.
 * @ingroup SystemEventRunloop
 * @param num_workers Total number of workers, including the calling thread.
 *                    Pass 0 to use all available cores. Values greater than
 *                    the number of available cores are clamped to that limit.
 * @param init  Called once per worker before it starts. May be `NULL`.
 * @param callback Handler invoked on a worker for each event dequeued.
 * @param exit  Called once per worker after the queue is drained. May be `NULL`.
 *
 * The calling thread becomes worker 0. If @p num_workers is greater than 1,
 * additional workers (1, 2, …) are started on other cores or threads, each
 * receiving their index in @p init and @p exit. Returns the exit value passed
 * to sys_runloop_shutdown() once all workers have finished draining the queue.
 */
uint32_t sys_runloop_run(uint8_t num_workers, sys_runloop_init_func_t init,
                         sys_runloop_func_t callback,
                         sys_runloop_exit_func_t exit);

/**
 * @brief Signal the run loop to stop accepting events and exit when drained.
 * @ingroup SystemEventRunloop
 * @param exit_value Value returned by sys_runloop_run() once all workers
 *                   have exited.
 *
 * After this call sys_runloop_post() returns `false`. Workers finish any
 * in-progress and already-queued events, then exit, causing sys_runloop_run()
 * to return @p exit_value. Safe to call from any thread or event handler. Has
 * no effect if the run loop is not running.
 */
void sys_runloop_shutdown(uint32_t exit_value);

/**
 * @brief Post an event to be processed by the run loop.
 * @ingroup SystemEventRunloop
 * @param event Non-NULL event payload.
 * @return `true` on success, `false` if the run loop is shut down or the
 *         queue is full.
 *
 * Safe to call from any thread or interrupt context before or after
 * sys_runloop_run(). Events are processed in the order posted.
 */
bool sys_runloop_post(sys_event_t event);

/**
 * @brief Check whether the run loop is currently running.
 * @ingroup SystemEventRunloop
 * @return `true` if sys_runloop_run() has been called and has not yet
 *         returned.
 */
bool sys_runloop_valid(void);

/** @} */

#ifdef __cplusplus
}
#endif

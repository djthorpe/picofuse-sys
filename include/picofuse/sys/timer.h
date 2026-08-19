/**
 * @file sys/timer.h
 * @brief Periodic and one-shot timer scheduling.
 * @defgroup SystemTimer Timers
 * @ingroup System
 * @details
 * The timer module provides lightweight callback-based scheduling for
 * periodic work and one-shot delays.
 *
 * Timers are allocated from an implementation-defined pool. After
 * `sys_timer_init()`, timers are configured but idle; work begins only after
 * `sys_timer_start()` succeeds.
 *
 * Callback model:
 * - The callback receives the owning `sys_timer_t *` handle.
 * - The timer userdata pointer is retrievable via `sys_timer_get_userdata()`.
 * - One-shot behavior is implemented by calling `sys_timer_deinit()` from
 *   within the callback.
 *
 * Lifecycle summary:
 * 1. Create with `sys_timer_init(interval_ms, userdata, callback)`.
 * 2. Start with `sys_timer_start()`.
 * 3. Optionally check run state using `sys_timer_valid()`.
 * 4. Release with `sys_timer_deinit()`.
 *
 * Platform note:
 * - On Pico, callbacks fire on the core that called `sys_timer_start()`, via
 *   the platform alarm pool.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @def SYS_TIMER_CAPACITY
 * @ingroup SystemTimer
 * @brief Maximum number of timers available in static-pool implementations.
 */
#ifndef SYS_TIMER_CAPACITY
#define SYS_TIMER_CAPACITY 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Timer.
 * @ingroup SystemTimer
 * @headerfile timer.h picofuse/sys.h
 */
typedef struct sys_timer_t sys_timer_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Allocate and configure a timer from the pool.
 * @ingroup SystemTimer
 * @param interval_ms Interval at which the timer fires, in milliseconds.
 * @param userdata Optional pointer passed to the callback on each fire.
 * @param callback Function called each time the timer fires.
 * @return Pointer to an initialized timer, or `NULL` if the pool is exhausted.
 *
 * The timer is not started until sys_timer_start() is called. Release the
 * timer with sys_timer_deinit() when it is no longer needed. Implementations
 * that use a static pool will return `NULL` after `SYS_TIMER_CAPACITY` timers
 * have been allocated.
 *
 * To implement a one-shot timer, call sys_timer_deinit() from inside the
 * callback.
 */
sys_timer_t *sys_timer_init(uint32_t interval_ms, void *userdata,
                            void (*callback)(sys_timer_t *));

/**
 * @brief Stop and release a timer back to the pool.
 * @ingroup SystemTimer
 * @param timer Timer to release.
 *
 * Stops the timer if it is running and returns its pool slot. If a callback is
 * currently executing on another thread or core, this call waits for it to
 * finish before returning. The pointer becomes invalid after this call. Safe
 * to call from within the timer callback to implement one-shot behaviour.
 */
void sys_timer_deinit(sys_timer_t *timer);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Start a timer.
 * @ingroup SystemTimer
 * @param timer Timer to start.
 * @return `true` on success, `false` if the timer is invalid or already
 * running.
 */
bool sys_timer_start(sys_timer_t *timer);

/**
 * @brief Check whether a timer is valid and running.
 * @ingroup SystemTimer
 * @param timer Timer to check.
 * @return `true` if the timer is allocated and currently running.
 */
bool sys_timer_valid(sys_timer_t *timer);

/**
 * @brief Return user data associated with a timer.
 * @ingroup SystemTimer
 * @param timer Timer to query.
 * @return User data pointer provided to @ref sys_timer_init, or `NULL`.
 */
void *sys_timer_get_userdata(sys_timer_t *timer);

/** @} */

#ifdef __cplusplus
}
#endif

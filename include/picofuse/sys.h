/**
 * @file sys.h
 * @brief Aggregates the core system abstraction headers and process lifecycle
 * hooks.
 * @defgroup System System Abstractions
 * @ingroup Picofuse
 * @details
 * The System module provides the runtime foundation used by higher-level
 * Picofuse components. It includes cross-platform abstractions for memory,
 * threading, synchronization, timing, event queues, runloops, diagnostics,
 * environment metadata, and process control.
 *
 * These interfaces are intended to normalize platform differences while
 * keeping APIs small and predictable for embedded and host targets alike.
 *
 * Runtime lifecycle is coordinated through:
 * - `sys_init()` to initialize global runtime resources.
 * - `sys_exit()` to release global runtime resources.
 *
 * Common usage patterns:
 * 1. Call `sys_init()` early in process startup.
 * 2. Create and use synchronization, timing, and event primitives as needed.
 * 3. Drive application logic using queues and/or `sys_runloop_*` helpers.
 * 4. Shut down gracefully and call `sys_exit()` before process termination.
 *
 * This header is a convenience aggregator for all public system abstraction
 * headers plus global lifecycle entry points.
 */
#pragma once
#include "sys/arena.h"
#include "sys/assert.h"
#include "sys/atomic.h"
#include "sys/cond.h"
#include "sys/date.h"
#include "sys/debugf.h"
#include "sys/env.h"
#include "sys/event.h"
#include "sys/halt.h"
#include "sys/hash.h"
#include "sys/mem.h"
#include "sys/mutex.h"
#include "sys/panicf.h"
#include "sys/printf.h"
#include "sys/random.h"
#include "sys/runloop.h"
#include "sys/sleep.h"
#include "sys/thread.h"
#include "sys/timer.h"
#include "sys/timestamp.h"
#include "sys/waitgroup.h"

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void);

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void);

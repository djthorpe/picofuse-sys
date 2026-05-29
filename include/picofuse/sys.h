/**
 * @file sys.h
 * @brief Aggregates the core system abstraction headers and process lifecycle
 * hooks.
 * @defgroup System System Abstractions
 * @ingroup Picofuse
 */
#pragma once
#include "sys/arena.h"
#include "sys/assert.h"
#include "sys/atomic.h"
#include "sys/cond.h"
#include "sys/date.h"
#include "sys/env.h"
#include "sys/event.h"
#include "sys/halt.h"
#include "sys/hash.h"
#include "sys/mem.h"
#include "sys/mutex.h"
#include "sys/panicf.h"
#include "sys/printf.h"
#include "sys/random.h"
#include "sys/sleep.h"
#include "sys/thread.h"
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

/**
 * @file sys.h
 * @defgroup System System Abstractions
 * @ingroup Picofuse
 * System abstraction layer
 */
#pragma once
#include "sys/assert.h"
#include "sys/atomic.h"
#include "sys/cond.h"
#include "sys/halt.h"
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

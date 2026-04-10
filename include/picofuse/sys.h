/**
 * @file sys.h
 * @ingroup System
 */
#pragma once
#include "sys/assert.h"
#include "sys/atomic.h"
#include "sys/halt.h"
#include "sys/mem.h"
#include "sys/panicf.h"
#include "sys/printf.h"
#include "sys/sleep.h"
#include "sys/timestamp.h"

/**
 * @brief Initializes the system on startup.
 */
void sys_init(void);

/**
 * @brief Cleans up the system on shutdown.
 */
void sys_exit(void);

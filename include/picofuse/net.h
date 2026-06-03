/**
 * @file net.h
 * @brief Aggregates network application interfaces.
 * @defgroup Network Network Applications
 * @ingroup Picofuse
 */
#pragma once
#include "net/mqtt.h"

/**
 * @brief Performs periodic network housekeeping.
 * @ingroup Network
 *
 * Must be called regularly from the main thread (worker 0) to service the
 * network stack. Called automatically by the run loop when the net module is
 * linked.
 */
void net_poll(void);

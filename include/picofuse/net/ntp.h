/**
 * @file ntp.h
 * @brief NTP (Network Time Protocol) interface
 * @defgroup NTP NTP
 * @ingroup Network
 *
 * NTP (Network Time Protocol) interface for network communication.
 *
 * This module provides functions to synchronize the system clock with a
 * remote NTP server and receive updates when the time changes.
 */
#pragma once
#include <picofuse/sys/date.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque NTP manager handle (singleton).
 * @ingroup NTP
 * @headerfile ntp.h picofuse/net.h
 */
typedef struct net_ntp_t net_ntp_t;

/**
 * @brief Result status for an NTP synchronization attempt.
 * @ingroup NTP
 */
typedef enum {
  net_ntp_status_success_t = 0, /**< Time was successfully synchronized. */
  net_ntp_status_timeout_t,     /**< No response received before the
                                    timeout. */
} net_ntp_status_t;

/**
 * @brief Callback invoked when an NTP synchronization attempt completes.
 * @ingroup NTP
 *
 * @param ntp       The NTP manager handle that generated the event.
 * @param status    Result of the sync attempt. Only
 *                  @ref net_ntp_status_success_t guarantees @p date is
 *                  populated.
 * @param date      Pointer to the updated UTC time on success; NULL on
 *                  failure. Valid only for the duration of the callback.
 * @param user_data Opaque pointer supplied at initialization time. Not
 *                  interpreted by the runtime.
 *
 * A failed @ref net_ntp_sync call always delivers one callback. A
 * successful one only delivers a callback if the resulting (seconds,
 * tzoffset) differs from the last one reported - see @ref net_ntp_sync for
 * the restriction on when the next sync may be started, which applies
 * either way. @ref net_ntp_set_tzoffset can also trigger this callback, on
 * its own, outside of any sync call - see its own doc.
 */
typedef void (*net_ntp_callback_t)(net_ntp_t *ntp, net_ntp_status_t status,
                                   const sys_date_t *date, void *user_data);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize (or re-initialize) the NTP manager.
 * @ingroup NTP
 *
 * @param callback  Optional callback invoked on successful time updates.
 * @param user_data Opaque pointer passed back to @p callback.
 * @return A pointer to the NTP manager handle (singleton), or NULL on error.
 *
 * The returned handle is opaque and must be released with @ref
 * net_ntp_deinit when no longer needed.
 */
net_ntp_t *net_ntp_init(net_ntp_callback_t callback, void *user_data);

/**
 * @brief Release NTP resources.
 * @ingroup NTP
 *
 * @param ntp The NTP manager handle to finalize. NULL is allowed and is a
 * no-op.
 */
void net_ntp_deinit(net_ntp_t *ntp);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Begin an asynchronous time synchronization against an NTP server.
 * @ingroup NTP
 *
 * @param ntp      NTP handle created by @ref net_ntp_init.
 * @param hostname NTP server hostname or IP address. May be NULL to use an
 * implementation-defined default.
 * @param port     Server port. Pass 0 to use the default NTP port (123).
 * @return true if the sync attempt was successfully initiated; false if
 * @p ntp is invalid, or a previous sync is still in flight.
 *
 * The result is reported via the callback provided at initialization (see
 * @ref net_ntp_callback_t) - a failure always fires it, a success only if
 * the synchronized time actually changed. Only one sync may be in flight at
 * a time, and this call itself is the only way to observe that a silent
 * (unchanged) success has finished: net_ntp_sync() returns false only while
 * genuinely still in flight, so if you need to know completion and cannot
 * rely on the callback, poll it (it costs nothing beyond the false return)
 * rather than waiting on a callback that may not come. Call it again to
 * retry after a failure, or periodically to keep the clock synchronized -
 * this module does not resync on its own.
 */
bool net_ntp_sync(net_ntp_t *ntp, const char *hostname, uint16_t port);

/**
 * @brief Set the timezone offset applied to future synchronized times.
 * @ingroup NTP
 *
 * @param ntp      NTP handle created by @ref net_ntp_init.
 * @param tzoffset Timezone offset in seconds east of UTC, copied verbatim
 * into @ref sys_date_t.tzoffset for every synchronized time reported from
 * this point on. NTP itself is always UTC; this does not affect the
 * synchronized time value, only how it is annotated. Defaults to 0 (UTC)
 * until set.
 *
 * If a sync has already completed since @ref net_ntp_init, and @p tzoffset
 * actually differs from the current one, this re-delivers the callback
 * immediately with the last synchronized time under the new offset -
 * outside of, and independent from, any @ref net_ntp_sync call. Otherwise
 * (no prior sync, or @p tzoffset unchanged) it is silent, and simply takes
 * effect for the next sync to complete.
 */
void net_ntp_set_tzoffset(net_ntp_t *ntp, int32_t tzoffset);

/** @} */

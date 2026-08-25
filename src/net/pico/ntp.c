#include <picofuse/net.h>
#include <picofuse/sys.h>

#ifdef PICO_CYW43_SUPPORTED
#include <lwip/apps/sntp.h>
#include <pico/cyw43_arch.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct net_ntp_t {
  bool initialized;
  net_ntp_callback_t callback;
  void *user_data;
  // Set from net_ntp_sync() (caller thread), cleared from net_ntp_set_date()
  // and _net_ntp_timeout_callback() (which can run on any core - the former
  // from wherever cyw43/lwIP is polled, the latter via the platform alarm
  // pool), so this needs atomic get/set rather than a plain bool. It also
  // arbitrates between those two completion paths: whichever observes it
  // set first claims the sync and performs its cleanup/callback; the other
  // sees it already cleared and does nothing further.
  sys_atomic_t syncing;
  sys_timer_t *timeout_timer;
  int32_t tzoffset;
  // Last (seconds, tzoffset) actually reported to callback, so
  // net_ntp_set_date()/net_ntp_set_tzoffset() only fire it on a real change.
  int64_t last_seconds;
  int32_t last_tzoffset;
  bool last_valid;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

// The NTP singleton.
static net_ntp_t _net_ntp = {0};

#ifdef PICO_CYW43_SUPPORTED
// How long to wait for a response before giving up on a sync attempt.
// lwIP's own SNTP_RECV_TIMEOUT (15s default) governs its first internal
// retry; this bounds the whole net_ntp_sync() attempt a little beyond that.
static const uint32_t _net_ntp_timeout_ms = 20000u;
#endif

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static void _net_ntp_report(net_ntp_t *ntp, int64_t seconds);
#ifdef PICO_CYW43_SUPPORTED
static void _net_ntp_timeout_callback(sys_timer_t *timer);
#endif

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

net_ntp_t *net_ntp_init(net_ntp_callback_t callback, void *user_data) {
  sys_debugf("net", "ntp_init: callback=%p userdata=%p", (void *)callback,
             user_data);

  // Reset any previous state before (re-)initializing, as other singletons
  // do.
  net_ntp_deinit(&_net_ntp);

  _net_ntp.initialized = true;
  _net_ntp.callback = callback;
  _net_ntp.user_data = user_data;
  sys_atomic_init(&_net_ntp.syncing, 0);

  return &_net_ntp;
}

void net_ntp_deinit(net_ntp_t *ntp) {
  if (ntp == NULL || !ntp->initialized) {
    return;
  }
  sys_debugf("net", "ntp_deinit: ntp=%p", ntp);

#ifdef PICO_CYW43_SUPPORTED
  if (sntp_enabled()) {
    cyw43_arch_lwip_begin();
    sntp_stop();
    cyw43_arch_lwip_end();
  }
  if (ntp->timeout_timer != NULL) {
    sys_timer_deinit(ntp->timeout_timer);
  }
#endif

  sys_memset(ntp, 0, sizeof(net_ntp_t));
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool net_ntp_sync(net_ntp_t *ntp, const char *hostname, uint16_t port) {
  // lwIP's SNTP client always uses the standard NTP port (123); this
  // backend has no way to honor a caller-supplied port.
  (void)port;

  if (ntp == NULL || ntp != &_net_ntp) {
    return false;
  }
  if (sys_atomic_get(&ntp->syncing) != 0) {
    sys_debugf("net", "ntp_sync: sync already in progress");
    return false;
  }

#ifdef PICO_CYW43_SUPPORTED
  cyw43_arch_lwip_begin();

  if (sntp_enabled()) {
    sntp_stop();
  }

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_init();

  // sntp_init() unconditionally (re-)applies lwipopts.h's SNTP_SERVER_ADDRESS
  // default to server 0, so a caller-supplied hostname must be set after
  // init to take effect, not before.
  if (hostname != NULL) {
    sntp_setservername(0, hostname);
  }

  cyw43_arch_lwip_end();

  ntp->timeout_timer =
      sys_timer_init(_net_ntp_timeout_ms, NULL, _net_ntp_timeout_callback);
  if (ntp->timeout_timer == NULL || !sys_timer_start(ntp->timeout_timer)) {
    sys_debugf("net", "ntp_sync: failed to arm timeout timer");
    if (ntp->timeout_timer != NULL) {
      sys_timer_deinit(ntp->timeout_timer);
      ntp->timeout_timer = NULL;
    }
    cyw43_arch_lwip_begin();
    sntp_stop();
    cyw43_arch_lwip_end();
    return false;
  }

  sys_atomic_set(&ntp->syncing, 1);
  return true;
#else
  (void)hostname;
  sys_debugf("net", "ntp_sync: unsupported on this platform");
  return false;
#endif
}

void net_ntp_set_tzoffset(net_ntp_t *ntp, int32_t tzoffset) {
  if (ntp == NULL || ntp != &_net_ntp) {
    return;
  }
  ntp->tzoffset = tzoffset;

  if (!ntp->last_valid) {
    return; // No synchronized time yet to re-report under the new offset.
  }
  _net_ntp_report(ntp, ntp->last_seconds);
}

/**
 * @brief Report a (seconds, tzoffset) pair to the callback, but only if it
 * differs from the last one reported - used by both net_ntp_set_date() and
 * net_ntp_set_tzoffset() so the callback only fires on a real change.
 */
static void _net_ntp_report(net_ntp_t *ntp, int64_t seconds) {
  if (ntp->last_valid && ntp->last_seconds == seconds &&
      ntp->last_tzoffset == ntp->tzoffset) {
    return; // Unchanged since the last report.
  }
  ntp->last_seconds = seconds;
  ntp->last_tzoffset = ntp->tzoffset;
  ntp->last_valid = true;

  if (ntp->callback == NULL) {
    return;
  }

  sys_date_t date = {0};
  date.seconds = seconds;
  date.tzoffset = ntp->tzoffset;
  ntp->callback(ntp, net_ntp_status_success_t, &date, ntp->user_data);
}

///////////////////////////////////////////////////////////////////////////////
// LWIP CALLBACK

#ifdef PICO_CYW43_SUPPORTED
/**
 * @brief Invoked by lwIP's SNTP client (see SNTP_SET_SYSTEM_TIME in
 * lwipopts.h) once a valid response is received. Updates the system clock
 * always, but only delivers the completion callback if the synchronized
 * (seconds, tzoffset) differs from the last one reported (see
 * _net_ntp_report()) - unless _net_ntp_timeout_callback() already claimed
 * this sync (see net_ntp_t.syncing).
 */
void net_ntp_set_date(uint32_t sec) {
  net_ntp_t *ntp = &_net_ntp;

  if (sys_atomic_get(&ntp->syncing) == 0) {
    return; // Already claimed by the timeout callback.
  }
  sys_atomic_set(&ntp->syncing, 0);

  sys_date_t date = {0};
  date.seconds = (int64_t)sec;
  date.tzoffset = ntp->tzoffset;
  sys_date_set_now(&date);

  sntp_stop();

  sys_timer_t *timer = ntp->timeout_timer;
  ntp->timeout_timer = NULL;
  if (timer != NULL) {
    sys_timer_deinit(timer);
  }

  _net_ntp_report(ntp, (int64_t)sec);
}

/**
 * @brief Fires _net_ntp_timeout_ms after a sync starts if no response has
 * arrived yet - unless net_ntp_set_date() already claimed this sync (see
 * net_ntp_t.syncing).
 */
static void _net_ntp_timeout_callback(sys_timer_t *timer) {
  net_ntp_t *ntp = &_net_ntp;

  if (sys_atomic_get(&ntp->syncing) == 0) {
    return; // Already claimed by net_ntp_set_date().
  }
  sys_atomic_set(&ntp->syncing, 0);

  cyw43_arch_lwip_begin();
  if (sntp_enabled()) {
    sntp_stop();
  }
  cyw43_arch_lwip_end();

  ntp->timeout_timer = NULL;

  if (ntp->callback != NULL) {
    ntp->callback(ntp, net_ntp_status_timeout_t, NULL, ntp->user_data);
  }

  // Safe to call from within the timer's own callback (one-shot pattern).
  sys_timer_deinit(timer);
}
#endif

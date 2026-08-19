#include <hardware/platform_defs.h>
#include <hardware/regs/watchdog.h>
#include <hardware/watchdog.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_watchdog_t {
  uint32_t timeout_ms;
  uint32_t ping_interval_ms;
  uint64_t last_feed_ms;
  bool disable;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

#if PICO_RP2040
// RP2040 decrements twice per tick (errata RP2040-E1)
#define WATCHDOG_XFACTOR 2u
#else
#define WATCHDOG_XFACTOR 1u
#endif

static struct hw_watchdog_t _hw_watchdog = {0};

void _hw_lock_enter(void);
void _hw_lock_exit(void);

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _hw_watchdog_is_valid(const hw_watchdog_t *watchdog) {
  return watchdog != NULL && watchdog->timeout_ms != 0u;
}

///////////////////////////////////////////////////////////////////////////////
// MODULE HOOKS

void _hw_watchdog_module_init(void) {}

void _hw_watchdog_module_exit(void) { hw_watchdog_deinit(&_hw_watchdog); }

void _hw_watchdog_poll(void) {
  hw_watchdog_t *watchdog = &_hw_watchdog;
  _hw_lock_enter();
  if (!_hw_watchdog_is_valid(watchdog) || watchdog->disable) {
    _hw_lock_exit();
    return;
  }

  uint64_t now_ms = sys_timestamp_ms();
  uint64_t next_feed_ms = watchdog->last_feed_ms + watchdog->ping_interval_ms;
  if (watchdog->last_feed_ms != 0u && now_ms < next_feed_ms) {
    _hw_lock_exit();
    return;
  }

  sys_debugf("[watchdog] ping");
  watchdog->last_feed_ms = now_ms;
  watchdog_update();
  _hw_lock_exit();
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC API

hw_watchdog_t *hw_watchdog_init(void) {
  hw_watchdog_t *watchdog = &_hw_watchdog;
  uint32_t timeout_ms;
  uint32_t ping_interval_ms;

  _hw_lock_enter();

  // Reset any previous session before reinitializing singleton state.
  if (_hw_watchdog_is_valid(watchdog)) {
    watchdog_disable();
    sys_memset(watchdog, 0, sizeof(*watchdog));
  }

  timeout_ms = hw_watchdog_maxtimeout_ms();
  if (timeout_ms == 0u) {
    _hw_lock_exit();
    return NULL;
  }

  ping_interval_ms = (timeout_ms * 64u) / 100u;
  if (ping_interval_ms == 0u) {
    ping_interval_ms = timeout_ms;
  }

  watchdog->timeout_ms = timeout_ms;
  watchdog->ping_interval_ms = ping_interval_ms;
  watchdog->last_feed_ms = 0u;
  watchdog->disable = true;
  _hw_lock_exit();
  return watchdog;
}

hw_watchdog_t *hw_watchdog_init_device(const char *device) {
  (void)device;
  return hw_watchdog_init();
}

void hw_watchdog_deinit(hw_watchdog_t *watchdog) {
  _hw_lock_enter();

  if (!_hw_watchdog_is_valid(watchdog)) {
    _hw_lock_exit();
    return;
  }

  watchdog_disable();
  sys_memset(watchdog, 0, sizeof(*watchdog));
  _hw_lock_exit();
}

uint32_t hw_watchdog_maxtimeout_ms(void) {
  return WATCHDOG_LOAD_BITS / (1000u * WATCHDOG_XFACTOR);
}

bool hw_watchdog_did_reset(hw_watchdog_t *watchdog) {
  bool did_reset;

  _hw_lock_enter();
  if (!_hw_watchdog_is_valid(watchdog)) {
    _hw_lock_exit();
    return false;
  }

  did_reset = watchdog_enable_caused_reboot();
  _hw_lock_exit();
  return did_reset;
}

void hw_watchdog_enable(hw_watchdog_t *watchdog, bool enable) {
  _hw_lock_enter();

  if (!_hw_watchdog_is_valid(watchdog)) {
    _hw_lock_exit();
    return;
  }

  if (enable) {
    watchdog_enable(watchdog->timeout_ms * WATCHDOG_XFACTOR, true);
    watchdog->disable = false;
    watchdog->last_feed_ms = 0u;
  } else {
    watchdog_disable();
    watchdog->disable = true;
  }

  _hw_lock_exit();
}

void hw_watchdog_reset(hw_watchdog_t *watchdog, uint32_t delay_ms) {
  uint32_t max_timeout_ms;

  _hw_lock_enter();

  if (!_hw_watchdog_is_valid(watchdog) || delay_ms == 0u) {
    _hw_lock_exit();
    return;
  }

  max_timeout_ms = hw_watchdog_maxtimeout_ms();
  if (delay_ms > max_timeout_ms) {
    delay_ms = max_timeout_ms;
  }

  watchdog_enable(delay_ms * WATCHDOG_XFACTOR, true);
  watchdog->disable = true;
  _hw_lock_exit();
}
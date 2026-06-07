/**
 * @file
 * @brief Enable the watchdog, keep feeding it, and then force a reset.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define FEED_INTERVAL_MS 100u
#define FEED_WINDOW_MS 30000u

static void reset_timer_callback(sys_timer_t *timer) {
  hw_watchdog_t *watchdog = (hw_watchdog_t *)sys_timer_get_userdata(timer);
  if (watchdog == NULL) {
    return;
  }

  sys_printf("Watchdog reset timer fired; forcing reset now\n");
  sys_printf("Calling hw_watchdog_reset()\n");
  hw_watchdog_reset(watchdog, 1u);
  sys_timer_deinit(timer);
}

int main(void) {
  sys_init();
  hw_init();

  hw_watchdog_t *watchdog = hw_watchdog_init();
  if (watchdog == NULL) {
    sys_printf("Watchdog backend is unavailable on this platform\n");
    hw_exit();
    sys_exit();
    return 0;
  }

  uint32_t max_timeout_ms = hw_watchdog_maxtimeout_ms();
  if (max_timeout_ms == 0u) {
    sys_printf("Watchdog backend reported zero timeout\n");
    hw_exit();
    sys_exit();
    return 0;
  }

  if (hw_watchdog_did_reset(watchdog)) {
    sys_printf("System was reset by the watchdog on the previous boot\n");
  } else {
    sys_printf("Previous boot was not watchdog-reset\n");
  }

  uint32_t feed_timeout_ms = max_timeout_ms / 2u;
  if (feed_timeout_ms == 0u) {
    feed_timeout_ms = max_timeout_ms;
  }

  sys_printf(
      "Watchdog armed with %u ms timeout; feeding for 30s before reset\n",
      (unsigned int)feed_timeout_ms);

  hw_watchdog_enable(watchdog, true);

  sys_timer_t *reset_timer =
      sys_timer_init(FEED_WINDOW_MS, watchdog, reset_timer_callback);
  if (reset_timer == NULL || !sys_timer_start(reset_timer)) {
    sys_printf("Failed to start watchdog reset timer\n");
    hw_exit();
    sys_exit();
    return 1;
  }

  uint64_t start_ms = sys_timestamp_ms();
  while ((sys_timestamp_ms() - start_ms) < FEED_WINDOW_MS) {
    hw_poll();
    sys_sleep_ms(FEED_INTERVAL_MS);
  }

  for (;;) {
    sys_sleep_ms(1000u);
  }
}

#include <test.h>

bool test_main(void) {
  hw_watchdog_deinit(NULL);
  TestAssert(!hw_watchdog_did_reset(NULL),
             "NULL watchdog did_reset should report false");
  hw_watchdog_enable(NULL, true);
  hw_watchdog_enable(NULL, false);
  hw_watchdog_reset(NULL, 100u);

#ifdef SYSTEM_NAME_PICO
  uint32_t max_timeout_ms = hw_watchdog_maxtimeout_ms();
  TestAssert(max_timeout_ms > 0u,
             "Pico watchdog max timeout should be > 0, got %u",
             (unsigned int)max_timeout_ms);

  hw_watchdog_t *watchdog = hw_watchdog_init();
  TestAssert(watchdog != NULL, "Pico watchdog init should succeed");

  hw_watchdog_t *watchdog_device = hw_watchdog_init_device("/dev/watchdog0");
  TestAssert(watchdog_device == watchdog,
             "Pico init_device should return singleton watchdog");

  // Enable feeding mode and exercise poll/update path.
  hw_watchdog_enable(watchdog, true);
  hw_poll();

  // Return to feeding mode and then stop activity.
  hw_watchdog_enable(watchdog, true);
  hw_poll();
  hw_watchdog_enable(watchdog, false);

  // did_reset should be callable on a valid handle.
  (void)hw_watchdog_did_reset(watchdog);

  hw_watchdog_deinit(watchdog);

  // Singleton should be reusable after deinit.
  hw_watchdog_t *watchdog2 = hw_watchdog_init();
  TestAssert(watchdog2 != NULL, "Pico watchdog re-init should succeed");
  hw_watchdog_deinit(watchdog2);
#else
  TestAssert(hw_watchdog_maxtimeout_ms() == 0u,
             "Stub watchdog max timeout should be 0");

  TestAssert(hw_watchdog_init() == NULL,
             "Stub watchdog init should return NULL");
  TestAssert(hw_watchdog_init_device("/dev/watchdog0") == NULL,
             "Stub watchdog init_device should return NULL");
#endif

  return true;
}

TestMain(test_main)

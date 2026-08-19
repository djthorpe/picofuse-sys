#include <hardware/adc.h>
#include <picofuse/sys.h>

#if PICO_CYW43_SUPPORTED
#include <pico/cyw43_arch.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

void _hw_led_module_init(void);
void _hw_led_module_exit(void);
void _hw_flash_module_init(void);
void _hw_flash_module_exit(void);
void _hw_watchdog_module_init(void);
void _hw_watchdog_module_exit(void);
void _hw_watchdog_poll(void);
void _hw_wifi_poll(void);
void _hw_usb_poll(void);
void _hw_lock_module_init(void);
void _hw_lock_module_exit(void);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initializes the hardware system on startup.
 */
void hw_init(void) {
  adc_init();
  _hw_lock_module_init();
  _hw_led_module_init();
  _hw_flash_module_init();
  _hw_watchdog_module_init();

#if PICO_CYW43_SUPPORTED
  if (cyw43_arch_init()) {
    sys_panicf("cyw43_arch_init failed");
  }
#endif
}

/**
 * @brief Cleans up the hardware system on shutdown.
 */
void hw_exit(void) {
  _hw_led_module_exit();
  _hw_flash_module_exit();
  _hw_watchdog_module_exit();
  _hw_lock_module_exit();
#if PICO_CYW43_SUPPORTED
  cyw43_arch_deinit();
#endif
}

/**
 * @brief Occasional polling function for the hardware system.
 */
void hw_poll(void) {
  _hw_watchdog_poll();
  _hw_usb_poll();

#if PICO_CYW43_SUPPORTED
  cyw43_arch_poll();
  _hw_wifi_poll();
#endif
}

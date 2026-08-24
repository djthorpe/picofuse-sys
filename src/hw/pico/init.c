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
 * @brief Link-time anchor forcing this translation unit's hw_init()/
 * hw_exit()/hw_poll() to be extracted whenever picofuse-hw is linked.
 * Nothing else in this file is referenced by any other translation unit, so
 * without this, a consumer that also links picofuse-app (which provides its
 * own weak hw_init()/hw_exit()/hw_poll() fallbacks in src/app/hw.c, bundled
 * in the SAME archive as the code that calls them) would resolve those
 * calls from picofuse-app's own archive before ever reaching
 * picofuse-hw.a on the link line, regardless of link order. Referenced via
 * a forced undefined symbol (`-u`) added in src/hw/pico/CMakeLists.txt;
 * never called directly.
 */
void _hw_pico_init_anchor(void) {}

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

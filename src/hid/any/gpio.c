#include "private.h"
#include <picofuse/sys.h>

#ifdef SYSTEM_NAME_PICO
#include <pico.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hid_gpio_device_init(void *userdata);
static bool _hid_gpio_device_deinit(void *userdata);
static hid_device_t *_hid_register_gpio_mode(hid_t *instance, uint8_t bank,
                                             uint8_t pin, uint16_t keycode,
                                             hw_gpio_mode_t mode);

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

/**
 * @brief Atomic flag indicating whether the global GPIO callback is installed.
 */
static sys_atomic_t _hid_gpio_callback_initialized = {0};

/**
 * @brief Canonical name used for GPIO-backed HID devices.
 */
const char *_hid_device_gpio_name = "gpio-input";

/**
 * @brief Shared lifecycle callback table for GPIO-backed HID devices.
 */
static const hid_device_callbacks_t _hid_gpio_device_callbacks = {
    .init = _hid_gpio_device_init,
    .read = NULL,
    .deinit = _hid_gpio_device_deinit,
};

#if defined(PICOLIPO_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PICOLIPO_USER_SW_PIN
#elif defined(PIMORONI_PICOLIPO_16MB) || defined(PIMORONI_PICOLIPO_4MB)
#define HID_USER_BUTTON_PIN 23
#elif defined(PICO_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PICO_USER_SW_PIN
#elif defined(PIMORONI_PICO_LIPO2_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PIMORONI_PICO_LIPO2_USER_SW_PIN
#elif defined(PIMORONI_PICO_LIPO2_RP2350)
#define HID_USER_BUTTON_PIN 30
#elif defined(PIMORONI_PICO_PLUS2_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PIMORONI_PICO_PLUS2_USER_SW_PIN
#elif defined(PIMORONI_PICO_PLUS2_W_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PIMORONI_PICO_PLUS2_W_USER_SW_PIN
#elif defined(SPARKFUN_IOTREDBOARD_RP2350_USER_SW_PIN)
#define HID_USER_BUTTON_PIN SPARKFUN_IOTREDBOARD_RP2350_USER_SW_PIN
#elif defined(TINY2040_USER_SW_PIN)
#define HID_USER_BUTTON_PIN TINY2040_USER_SW_PIN
#elif defined(TINY2350_USER_SW_PIN)
#define HID_USER_BUTTON_PIN TINY2350_USER_SW_PIN
#elif defined(PLASMA2040_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PLASMA2040_USER_SW_PIN
#elif defined(PLASMA2350_USER_SW_PIN)
#define HID_USER_BUTTON_PIN PLASMA2350_USER_SW_PIN
#elif defined(BADGER2040_USER_SW_PIN)
#define HID_USER_BUTTON_PIN BADGER2040_USER_SW_PIN
#elif defined(SERVO2040_USER_SW_PIN)
#define HID_USER_BUTTON_PIN SERVO2040_USER_SW_PIN
#elif defined(INTERSTATE75_USER_SW_PIN)
#define HID_USER_BUTTON_PIN INTERSTATE75_USER_SW_PIN
#elif defined(MOTOR2040_USER_SW_PIN)
#define HID_USER_BUTTON_PIN MOTOR2040_USER_SW_PIN
#elif defined(KEYBOW2040_USER_SW_PIN)
#define HID_USER_BUTTON_PIN KEYBOW2040_USER_SW_PIN
#elif defined(WEACT_STUDIO_RP2350B_USER_SW_PIN)
#define HID_USER_BUTTON_PIN WEACT_STUDIO_RP2350B_USER_SW_PIN
#elif defined(USR_SW_PIN)
#define HID_USER_BUTTON_PIN USR_SW_PIN
#elif defined(USR_BTN_PIN)
#define HID_USER_BUTTON_PIN USR_BTN_PIN
#elif defined(ADAFRUIT_MACROPAD_BUTTON_PIN)
#define HID_USER_BUTTON_PIN ADAFRUIT_MACROPAD_BUTTON_PIN
#elif defined(ADAFRUIT_FRUIT_JAM_BOOT_BUTTON_PIN)
#define HID_USER_BUTTON_PIN ADAFRUIT_FRUIT_JAM_BOOT_BUTTON_PIN
#endif

///////////////////////////////////////////////////////////////////////////////
// CALLBACK

/**
 * @brief Global GPIO callback used by the HID any backend.
 */
static void _hid_gpio_callback(uint8_t bank, uint8_t pin, hw_gpio_event_t event,
                               void *userdata) {
  uint32_t id = ((uint32_t)bank << 16) | (uint32_t)pin;
  hid_t *instance = NULL;
  hid_device_t *device = NULL;

  (void)userdata;

  if (!_hid_find_device_by_id(id, &instance, &device)) {
    return;
  }

  if (instance == NULL || device == NULL ||
      !sys_event_queue_valid(instance->queue)) {
    return;
  }

  if ((event & HW_GPIO_RISING) != 0) {
    if (hid_event_queue_keycode(device, hid_state_on, device->keycode)) {
      device->last_event_ms = sys_timestamp_ms();
    }
  }

  if ((event & HW_GPIO_FALLING) != 0) {
    if (hid_event_queue_keycode(device, hid_state_off, device->keycode)) {
      device->last_event_ms = sys_timestamp_ms();
    }
  }
}

/**
 * @brief Initialize a GPIO-backed HID device.
 */
static bool _hid_gpio_device_init(void *userdata) {
  hw_gpio_t *gpio = (hw_gpio_t *)userdata;
  return gpio != NULL && hw_gpio_valid(gpio);
}

/**
 * @brief Deinitialize a GPIO-backed HID device.
 */
static bool _hid_gpio_device_deinit(void *userdata) {
  hw_gpio_t *gpio = (hw_gpio_t *)userdata;
  hw_gpio_deinit(gpio);
  return true;
}

/**
 * @brief Register a GPIO-backed HID device using the requested GPIO mode.
 */
static hid_device_t *_hid_register_gpio_mode(hid_t *instance, uint8_t bank,
                                             uint8_t pin, uint16_t keycode,
                                             hw_gpio_mode_t mode) {
  if (instance == NULL) {
    sys_debugf("[hid] gpio register failed: instance is NULL");
    return NULL;
  }

  hw_gpio_t *gpio = hw_gpio_init(bank, pin, mode);
  if (gpio == NULL) {
    sys_debugf(
        "[hid] gpio register failed: hw_gpio_init bank=%u pin=%u mode=%u",
        (unsigned int)bank, (unsigned int)pin, (unsigned int)mode);
    return NULL;
  }

  uint32_t id = ((uint32_t)bank << 16) | (uint32_t)pin;
  hid_device_t *device =
      hid_register(instance, _hid_device_gpio_name, id, hid_type_gpio, 0u, gpio,
                   _hid_gpio_device_callbacks);
  if (device == NULL) {
    sys_debugf("[hid] gpio register failed: hid_register bank=%u pin=%u",
               (unsigned int)bank, (unsigned int)pin);
    hw_gpio_deinit(gpio);
    return NULL;
  }

  device->gpio = gpio;
  device->keycode = keycode;
  return device;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize the global GPIO callback once for HID any instances.
 */
void _hid_gpio_callback_init(void) {
  if (sys_atomic_get(&_hid_gpio_callback_initialized) != 0u) {
    return;
  }
  hw_gpio_set_callback(_hid_gpio_callback, NULL);
  sys_atomic_set(&_hid_gpio_callback_initialized, 1u);
  sys_debugf("[hid] gpio callback registered");
}

/**
 * @brief Remove the global GPIO callback when HID any no longer uses it.
 */
void _hid_gpio_callback_deinit(void) {
  if (_hid_has_valid_instances()) {
    return;
  }

  if (sys_atomic_get(&_hid_gpio_callback_initialized) == 0u) {
    return;
  }
  hw_gpio_set_callback(NULL, NULL);
  sys_atomic_set(&_hid_gpio_callback_initialized, 0u);
  sys_debugf("[hid] gpio callback de-registered");
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Register a floating GPIO input and map it to a HID keycode.
 */
hid_device_t *hid_register_gpio_input(hid_t *instance, uint8_t bank,
                                      uint8_t pin, uint16_t keycode) {
  return _hid_register_gpio_mode(instance, bank, pin, keycode, HW_GPIO_INPUT);
}

/**
 * @brief Register a pull-up GPIO input and map it to a HID keycode.
 */
hid_device_t *hid_register_gpio_pullup(hid_t *instance, uint8_t bank,
                                       uint8_t pin, uint16_t keycode) {
  return _hid_register_gpio_mode(instance, bank, pin, keycode, HW_GPIO_PULLUP);
}

/**
 * @brief Register a pull-down GPIO input and map it to a HID keycode.
 */
hid_device_t *hid_register_gpio_pulldown(hid_t *instance, uint8_t bank,
                                         uint8_t pin, uint16_t keycode) {
  return _hid_register_gpio_mode(instance, bank, pin, keycode,
                                 HW_GPIO_PULLDOWN);
}

/**
 * @brief Register the board user button when a known button pin macro exists.
 */
hid_device_t *hid_register_user_button(hid_t *instance, uint16_t keycode) {
#if defined(HID_USER_BUTTON_PIN)
  sys_debugf("[hid] user button pin selected: %u",
             (unsigned int)HID_USER_BUTTON_PIN);
  return hid_register_gpio_input(instance, 0u, (uint8_t)HID_USER_BUTTON_PIN,
                                 keycode);
#else
  sys_debugf("[hid] user button unavailable: no known board user-button macro");
  (void)instance;
  (void)keycode;
  return NULL;
#endif
}

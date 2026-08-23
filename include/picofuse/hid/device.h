/**
 * @file device.h
 * @brief HID device lifecycle and polling interface.
 * @ingroup HID
 */
#pragma once
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Maximum number of HID instances available in the backend pool.
 * @ingroup HID
 *
 * Override at compile time, for example: `-DHID_CAPACITY=2`.
 */
#ifndef HID_CAPACITY
#define HID_CAPACITY 1u
#endif

/**
 * @brief Maximum number of HID devices tracked by one HID instance.
 * @ingroup HID
 *
 * Override at compile time, for example: `-DHID_DEVICE_CAPACITY=16`.
 */
#ifndef HID_DEVICE_CAPACITY
#define HID_DEVICE_CAPACITY 32u
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque HID instance handle.
 * @ingroup HID
 */
typedef struct hid_t hid_t;

/**
 * @brief HID child-device descriptor tracked by a HID instance.
 * @ingroup HID
 */
typedef struct hid_device_t hid_device_t;

/**
 * @brief HID child-device type classification.
 * @ingroup HID
 */
typedef enum {
  hid_type_none = 0,
  hid_type_gpio = 1,
  hid_type_other = 2,
  hid_type_timer = 3,
  hid_type_signal = 4,
  hid_type_evdev = 5,
} hid_type_t;

/**
 * @brief Callback operation table for HID child-device backends.
 * @ingroup HID
 *
 * Each callback returns true on success and false on failure.
 */
typedef struct {
  bool (*init)(hid_device_t *device, void *userdata);
  bool (*read)(hid_device_t *device, void *userdata);
  bool (*deinit)(hid_device_t *device, void *userdata);
} hid_device_callbacks_t;

/**
 * @brief Coarse semantic classification of a HID device.
 * @ingroup HID
 *
 * Unlike hid_type_t (which identifies the backend mechanism, e.g. gpio vs
 * evdev vs timer), this describes what kind of thing the device is to an
 * application. For evdev devices this is determined heuristically from the
 * event/key/axis capability bits the kernel reports for the device node;
 * see hid_evdev_list().
 */
typedef enum {
  hid_class_unknown = 0,
  hid_class_keyboard = 1,
  hid_class_mouse = 2,
  hid_class_joystick = 3,
  hid_class_touchscreen = 4,
  hid_class_sensor = 5,
} hid_class_t;

/**
 * @brief Callback invoked once per device node found by hid_evdev_list().
 * @ingroup HID
 * @param name Identifier for the device node (for example
 * "/dev/input/event3" for evdev), suitable for passing to the matching
 * hid_register_* function. Valid only for the duration of the callback.
 * @param type Backend type that would be used to register this device (for
 * example hid_type_evdev). Always hid_type_evdev for hid_evdev_list().
 * @param hid_class Coarse classification of the device's capabilities.
 * @param userdata Opaque user pointer passed to hid_evdev_list().
 */
typedef void (*hid_device_list_callback_t)(const char *name, hid_type_t type,
                                           hid_class_t hid_class,
                                           void *userdata);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize a HID device instance.
 * @ingroup HID
 * @param queue Event queue used by this HID instance.
 * @return HID instance, or NULL on failure.
 */
hid_t *hid_init(sys_event_queue_t *queue);

/**
 * @brief Deinitialize a HID device instance.
 * @ingroup HID
 * @param instance HID instance.
 */
void hid_deinit(hid_t *instance);

/**
 * @brief Poll a HID device for pending input.
 * @ingroup HID
 * @param instance HID instance.
 * @retval true Input was processed.
 * @retval false No input was available or the device is invalid.
 */
bool hid_poll(hid_t *instance);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Register a generic HID child device using callback operations.
 * @ingroup HID
 * @param instance HID instance that owns the registration.
 * @param name Device name.
 * @param id Device identifier.
 * @param type Device type (backend mechanism) classification.
 * @param hid_class Device semantic classification (see hid_class_t). Pass
 * hid_class_unknown when none applies.
 * @param polling_interval_ms Polling interval in milliseconds for read
 * callbacks. Use 0 to evaluate on every hid_poll() call.
 * @param userdata Opaque user data passed to callback functions.
 * @param callbacks Callback operation table.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *hid_register(hid_t *instance, const char *name, uint32_t id,
                           hid_type_t type, hid_class_t hid_class,
                           uint32_t polling_interval_ms, void *userdata,
                           hid_device_callbacks_t callbacks);

/**
 * @brief Register a GPIO pin as HID input.
 * @ingroup HID
 * @param instance HID instance that owns the GPIO registration.
 * @param bank GPIO bank index.
 * @param pin GPIO pin index.
 * @param keycode HID keycode reported for this input.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *hid_register_gpio_input(hid_t *instance, uint8_t bank,
                                      uint8_t pin, uint16_t keycode);

/**
 * @brief Register a GPIO pin as HID input with pull-up.
 * @ingroup HID
 * @param instance HID instance that owns the GPIO registration.
 * @param bank GPIO bank index.
 * @param pin GPIO pin index.
 * @param keycode HID keycode reported for this input.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *hid_register_gpio_pullup(hid_t *instance, uint8_t bank,
                                       uint8_t pin, uint16_t keycode);

/**
 * @brief Register a GPIO pin as HID input with pull-down.
 * @ingroup HID
 * @param instance HID instance that owns the GPIO registration.
 * @param bank GPIO bank index.
 * @param pin GPIO pin index.
 * @param keycode HID keycode reported for this input.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *hid_register_gpio_pulldown(hid_t *instance, uint8_t bank,
                                         uint8_t pin, uint16_t keycode);

/**
 * @brief Register a user button as a HID input source.
 * @ingroup HID
 * @param instance HID instance that owns the user-button registration.
 * @param keycode HID keycode reported for this input.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *hid_register_user_button(hid_t *instance, uint16_t keycode);

/**
 * @brief Register a timer-backed HID source.
 * @ingroup HID
 * @param instance HID instance that owns the timer registration.
 * @param id Device identifier.
 * @param interval_ms Timer period in milliseconds.
 * @param repeating True for periodic timers, false for one-shot timers.
 * @param userdata Opaque user data stored with the timer.
 * @return Registered HID device descriptor, or NULL on failure.
 */
hid_device_t *hid_register_timer(hid_t *instance, uint32_t id,
                                 uint32_t interval_ms, bool repeating,
                                 void *userdata);

/**
 * @brief Register a Linux evdev device as a HID input source.
 * @ingroup HID
 * @param instance HID instance that owns the evdev registration.
 * @param path Path to the evdev device node (for example "/dev/input/event0").
 * @param exclusive When true, grab the device exclusively (`EVIOCGRAB`) so
 * no other process (X11, Wayland, another evdev reader) also receives its
 * events while registered.
 * @param userdata Opaque user data retrievable via hid_device_userdata().
 * @return Registered HID device descriptor, or NULL on failure.
 *
 * The device is opened non-blocking and polled on every hid_poll() call.
 */
hid_device_t *hid_register_evdev(hid_t *instance, const char *path,
                                 bool exclusive, void *userdata);

/**
 * @brief Enumerate available Linux evdev device nodes.
 * @ingroup HID
 * @param callback Callback invoked once for each discovered device node.
 * Must not be NULL.
 * @param userdata Opaque user pointer passed to @p callback.
 * @return Number of device nodes reported to @p callback.
 *
 * Scans `/dev/input` for evdev device nodes and reports each one's path and
 * a best-effort classification, so callers can decide which to pass to
 * hid_register_evdev(). Nodes are reported regardless of whether they are
 * already registered with a HID instance. This is a one-shot snapshot, not
 * a live hotplug subscription. Only available on Linux; returns 0 without
 * invoking @p callback on other platforms.
 */
size_t hid_evdev_list(hid_device_list_callback_t callback, void *userdata);

/**
 * @brief Register an environment-signal HID source.
 * @ingroup HID
 * @param instance HID instance that owns the signal registration.
 * @return Registered HID device descriptor, or NULL on failure.
 *
 * The signal source captures environment signals and emits
 * `hid_event_type_signal` events when those signals are observed.
 */
hid_device_t *hid_register_signal(hid_t *instance);

/**
 * @brief Deregister and remove a HID device.
 * @ingroup HID
 * @param instance HID instance that owns the device.
 * @param device HID device handle.
 * @retval true Device was removed.
 * @retval false Instance or device handle was invalid.
 */
bool hid_deregister(hid_t *instance, hid_device_t *device);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Enumerate registered HID devices.
 * @ingroup HID
 * @param device Current device pointer, or NULL to get the first device.
 * @return Next device pointer, or NULL when no more devices are available.
 */
hid_device_t *hid_device_next(hid_device_t *device);

/**
 * @brief Get metadata for a registered HID device.
 * @ingroup HID
 * @param device HID device handle.
 * @param out_name Receives device name when non-NULL.
 * @param out_id Receives device id when non-NULL.
 * @param out_type Receives device type when non-NULL.
 * @param out_class Receives device classification when non-NULL. Devices
 * registered with hid_class_unknown (the default choice when no more
 * specific classification applies) report hid_class_unknown here.
 * @retval true Metadata was returned.
 * @retval false Device handle was invalid.
 */
bool hid_device_info(const hid_device_t *device, const char **out_name,
                     uint32_t *out_id, hid_type_t *out_type,
                     hid_class_t *out_class);

/**
 * @brief Get the userdata pointer associated with a registered HID device.
 * @ingroup HID
 * @param device HID device handle.
 * @return Device userdata pointer, or NULL when the handle is invalid.
 */
void *hid_device_userdata(const hid_device_t *device);

/** @} */

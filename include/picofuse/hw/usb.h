/**
 * @file usb.h
 * @brief USB host interface
 * @defgroup USB USB
 * @ingroup Hardware
 *
 * USB host interface for hardware platforms.
 *
 * This module provides USB host functionality, including detection of devices
 * as they are attached and detached. On initialisation, the callback is fired
 * once for each device already connected to the host, with the
 * @ref hw_usb_event_attached event. Subsequently, the callback fires whenever
 * a device is physically attached or detached.
 *
 * The @ref hw_usb_device_t structure describes a connected device. The
 * @p bus and @p port fields together provide a backend-specific location
 * identifier that can usually be used to distinguish devices with identical
 * VID/PID and correlate a detach event with a prior attach event. Backends
 * that expose a stable topology provide physical-bus/port values; others may
 * use the best available location identifier for the platform. On detach, the
 * @p manufacturer, @p product and @p serial string fields may be empty, as
 * the device is no longer accessible; VID, PID, bus and port are populated
 * whenever the backend can determine them.
 *
 * Class-specific functionality (HID input, CDC-ACM serial streams, mass
 * storage) is handled by separate modules that consume the device information
 * provided here.
 *
 * On the Pico platform, the USB peripheral is fixed hardware and operates in
 * host mode exclusively. On Linux and macOS, the host controller is managed
 * via libusb. In both cases, @ref hw_usb_init takes no platform-specific
 * address parameter.
 *
 * @note On the Pico (RP2040), the USB peripheral and the UART/debug interface
 * share the same physical USB connector. Enabling USB host mode will prevent
 * the device from appearing as a USB serial device to a connected host PC.
 * Use a debug probe if you need simultaneous debug output.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/**
 * @def HW_USB_STRING_MAX_LENGTH
 * @ingroup USB
 * @brief Maximum length of USB string descriptor fields, excluding the null
 * terminator.
 */
#ifndef HW_USB_STRING_MAX_LENGTH
#define HW_USB_STRING_MAX_LENGTH 63
#endif

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief USB hotplug event type.
 * @ingroup USB
 */
typedef enum {
  hw_usb_event_attached = (1 << 0), ///< A device has been attached
  hw_usb_event_detached = (1 << 1), ///< A device has been detached
} hw_usb_event_t;

/**
 * @brief USB device class codes.
 * @ingroup USB
 *
 * These are the standard bDeviceClass descriptor values defined by USB.
 * A value of 0x00 means the class is defined at the interface level.
 */
typedef enum {
  hw_usb_device_class_per_interface = 0x00,
  hw_usb_device_class_audio = 0x01,
  hw_usb_device_class_communications = 0x02,
  hw_usb_device_class_hid = 0x03,
  hw_usb_device_class_physical = 0x05,
  hw_usb_device_class_image = 0x06,
  hw_usb_device_class_printer = 0x07,
  hw_usb_device_class_mass_storage = 0x08,
  hw_usb_device_class_hub = 0x09,
  hw_usb_device_class_cdc_data = 0x0A,
  hw_usb_device_class_smart_card = 0x0B,
  hw_usb_device_class_content_security = 0x0D,
  hw_usb_device_class_video = 0x0E,
  hw_usb_device_class_personal_healthcare = 0x0F,
  hw_usb_device_class_audio_video = 0x10,
  hw_usb_device_class_bluetooth = 0xE0,
  hw_usb_device_class_miscellaneous = 0xEF,
  hw_usb_device_class_application_specific = 0xFE,
  hw_usb_device_class_vendor_specific = 0xFF,
} hw_usb_device_class_t;

/**
 * @brief USB device subclass codes.
 * @ingroup USB
 *
 * Subclass values are class-specific USB descriptor codes. Only common raw
 * values are named here; callers may still observe any 8-bit descriptor value.
 */
typedef enum {
  hw_usb_device_subclass_none = 0x00,
  hw_usb_device_subclass_boot_interface = 0x01,
  hw_usb_device_subclass_abstract_control_model = 0x02,
  hw_usb_device_subclass_vendor_specific = 0xFF,
} hw_usb_device_subclass_t;

/**
 * @brief USB device protocol codes.
 * @ingroup USB
 *
 * Protocol values are class-specific USB descriptor codes. Only the raw
 * descriptor value is standardized here; callers may still observe any 8-bit
 * value defined by the device's class.
 */
typedef enum {
  hw_usb_device_protocol_none = 0x00,
} hw_usb_device_protocol_t;

/**
 * @brief Describes a USB device observed by the host.
 * @ingroup USB
 *
 * This structure is populated when a device is attached or detached.
 *
 * @note On detach, @p manufacturer, @p product and @p serial may be empty
 * strings. Callers should not rely on them being populated for
 * @ref hw_usb_event_detached.
 */
typedef struct {
  uint16_t vid;                                    ///< USB Vendor ID
  uint16_t pid;                                    ///< USB Product ID
  hw_usb_device_class_t device_class;              ///< USB device class code
  hw_usb_device_subclass_t device_subclass;        ///< USB device subclass code
  hw_usb_device_protocol_t device_protocol;        ///< USB device protocol code
  char manufacturer[HW_USB_STRING_MAX_LENGTH + 1]; ///< Manufacturer string
  char product[HW_USB_STRING_MAX_LENGTH + 1];      ///< Product string
  char serial[HW_USB_STRING_MAX_LENGTH + 1];       ///< Serial number string
} hw_usb_device_t;

/**
 * @brief Opaque USB host handle.
 * @ingroup USB
 */
typedef struct hw_usb_t hw_usb_t;

/**
 * @brief Callback invoked on USB hotplug events.
 * @ingroup USB
 *
 * @param usb    The USB host handle.
 * @param event  The hotplug event type (attached or detached).
 * @param device Descriptor of the device that was attached or detached.
 *               For normal attach/detach callbacks this is non-NULL.
 *               After initial enumeration completes, the callback is invoked
 *               once with @ref hw_usb_event_attached and @p device set to
 *               NULL as an "enumeration complete" marker.
 *               String fields may be empty on detach.
 * @param userdata Opaque user pointer supplied to @ref hw_usb_init.
 */
typedef void (*hw_usb_callback_t)(hw_usb_t *usb, hw_usb_event_t event,
                                  const hw_usb_device_t *device,
                                  void *userdata);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize the USB host subsystem.
 * @ingroup USB
 *
 * Initializes the USB host controller and registers the hotplug callback.
 * Backends may enumerate already-connected devices immediately or defer the
 * initial device callbacks until the next host poll cycle. When initial
 * enumeration completes, the callback is fired with @ref hw_usb_event_attached
 * for each attached device, then once more with @ref hw_usb_event_attached
 * and @p device set to NULL to mark completion, so that callers receive a
 * consistent view of attached devices regardless of when @ref hw_usb_init is
 * called.
 *
 * @param callback Callback to invoke on attach and detach events. Must not
 *                 be NULL.
 * @param userdata Opaque user pointer passed to the callback on each
 *                 invocation.
 * @return A USB host handle, or NULL if initialization fails.
 */
hw_usb_t *hw_usb_init(hw_usb_callback_t callback, void *userdata);

/**
 * @brief Deinitialize the USB host subsystem.
 * @ingroup USB
 *
 * Shuts down the USB host controller and releases all associated resources.
 * The hotplug callback is deregistered and will not be invoked after this
 * call returns. Safe to call on an already-deinitialized handle, in which
 * case it is a no-op.
 *
 * @param usb The USB host handle to deinitialize.
 */
void hw_usb_deinit(hw_usb_t *usb);

/**
 * @brief Determine whether a USB host handle is valid.
 * @ingroup USB
 *
 * @param usb The USB host handle to check.
 * @retval true  The handle is initialized and usable.
 * @retval false The handle is NULL or has been deinitialized.
 */
bool hw_usb_valid(const hw_usb_t *usb);

/** @} */
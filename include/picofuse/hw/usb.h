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
 * @p bus and @p port fields together uniquely identify a physical port, which
 * allows two devices with identical VID/PID to be distinguished, and allows
 * a detach event to be correlated with a prior attach event. On detach, the
 * @p manufacturer, @p product and @p serial string fields may be empty, as
 * the device is no longer accessible; VID, PID, bus and port are always
 * populated.
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
 * @brief Describes a USB device observed by the host.
 * @ingroup USB
 *
 * This structure is populated when a device is attached or detached. The
 * @p bus and @p port fields uniquely identify the physical port the device
 * is connected to, and remain stable across attach/detach events for the
 * same port.
 *
 * @note On detach, @p manufacturer, @p product and @p serial may be empty
 * strings. Callers should not rely on them being populated for
 * @ref hw_usb_event_detached. VID, PID, @p bus and @p port are always valid.
 *
 * @note The @p device_class, @p device_subclass and @p device_protocol fields
 * reflect the values in the USB device descriptor. For composite devices or
 * devices that define class information at the interface level, these may be
 * 0x00; in that case the class is determined per-interface by higher-level
 * modules.
 */
typedef struct {
  uint16_t vid;            ///< USB Vendor ID
  uint16_t pid;            ///< USB Product ID
  uint8_t device_class;    ///< USB device class code
  uint8_t device_subclass; ///< USB device subclass code
  uint8_t device_protocol; ///< USB device protocol code
  uint8_t bus;             ///< Host controller bus number
  uint8_t port;            ///< Port address on the bus
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
 *               Always non-NULL. String fields may be empty on detach.
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
 * Before returning, this function enumerates any devices already connected
 * to the host and fires the callback with @ref hw_usb_event_attached for
 * each one, so that callers receive a consistent view of attached devices
 * regardless of when @ref hw_usb_init is called.
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
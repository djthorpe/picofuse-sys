#include <picofuse/hw.h>
#include <picofuse/sys.h>

#include <libusb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_usb_t {
  hw_usb_callback_t callback;
  void *userdata;
  libusb_context *context;
  libusb_hotplug_callback_handle hotplug_handle;
  pthread_t thread;
  bool hotplug_registered;
  bool thread_started;
  atomic_bool running;
  atomic_bool cleanup_in_thread;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_usb_t _hw_usb_instance = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _hw_usb_populate_strings(libusb_device_handle *handle,
                                     const struct libusb_device_descriptor *dd,
                                     hw_usb_device_t *device) {
  if (handle == NULL || dd == NULL || device == NULL) {
    return;
  }

  if (dd->iManufacturer != 0) {
    int n = libusb_get_string_descriptor_ascii(
        handle, dd->iManufacturer, (unsigned char *)device->manufacturer,
        HW_USB_STRING_MAX_LENGTH);
    if (n < 0) {
      device->manufacturer[0] = '\0';
    } else {
      device->manufacturer[n] = '\0';
    }
  }

  if (dd->iProduct != 0) {
    int n = libusb_get_string_descriptor_ascii(handle, dd->iProduct,
                                               (unsigned char *)device->product,
                                               HW_USB_STRING_MAX_LENGTH);
    if (n < 0) {
      device->product[0] = '\0';
    } else {
      device->product[n] = '\0';
    }
  }

  if (dd->iSerialNumber != 0) {
    int n = libusb_get_string_descriptor_ascii(handle, dd->iSerialNumber,
                                               (unsigned char *)device->serial,
                                               HW_USB_STRING_MAX_LENGTH);
    if (n < 0) {
      device->serial[0] = '\0';
    } else {
      device->serial[n] = '\0';
    }
  }
}

static bool _hw_usb_make_device(libusb_device *dev, hw_usb_device_t *out) {
  if (dev == NULL || out == NULL) {
    return false;
  }

  struct libusb_device_descriptor dd = {0};
  if (libusb_get_device_descriptor(dev, &dd) != 0) {
    return false;
  }

  sys_memset(out, 0, sizeof(*out));
  out->vid = dd.idVendor;
  out->pid = dd.idProduct;
  out->device_class = dd.bDeviceClass;
  out->device_subclass = dd.bDeviceSubClass;
  out->device_protocol = dd.bDeviceProtocol;

  libusb_device_handle *handle = NULL;
  if (libusb_open(dev, &handle) == 0 && handle != NULL) {
    _hw_usb_populate_strings(handle, &dd, out);
    libusb_close(handle);
  }

  return true;
}

static void _hw_usb_emit_event(hw_usb_t *usb, hw_usb_event_t event,
                               libusb_device *dev) {
  if (!hw_usb_valid(usb) || usb->callback == NULL || dev == NULL) {
    return;
  }

  hw_usb_device_t device = {0};
  if (_hw_usb_make_device(dev, &device)) {
    usb->callback(usb, event, &device, usb->userdata);
  }
}

static int LIBUSB_CALL _hw_usb_hotplug_cb(libusb_context *context,
                                          libusb_device *dev,
                                          libusb_hotplug_event event,
                                          void *userdata) {
  (void)context;

  hw_usb_t *usb = (hw_usb_t *)userdata;
  if (!hw_usb_valid(usb)) {
    return 0;
  }

  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
    _hw_usb_emit_event(usb, hw_usb_event_attached, dev);
  } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
    _hw_usb_emit_event(usb, hw_usb_event_detached, dev);
  }

  return 0;
}

static bool _hw_usb_emit_attached_devices(hw_usb_t *usb) {
  libusb_device **list = NULL;
  ssize_t count = libusb_get_device_list(usb->context, &list);
  if (count < 0 || list == NULL) {
    return false;
  }

  for (ssize_t i = 0; i < count; i++) {
    if (list[i] != NULL) {
      _hw_usb_emit_event(usb, hw_usb_event_attached, list[i]);
    }
  }

  libusb_free_device_list(list, 1);

  // Signal that initial enumeration is complete.
  if (hw_usb_valid(usb) && usb->callback != NULL) {
    usb->callback(usb, hw_usb_event_attached, NULL, usb->userdata);
  }

  return true;
}

static void _hw_usb_cleanup(hw_usb_t *usb) {
  if (usb == NULL) {
    return;
  }

  if (usb->hotplug_registered && usb->context != NULL) {
    libusb_hotplug_deregister_callback(usb->context, usb->hotplug_handle);
  }

  usb->hotplug_registered = false;

  if (usb->context != NULL) {
    libusb_exit(usb->context);
  }

  usb->context = NULL;
}

static void *_hw_usb_event_thread(void *arg) {
  hw_usb_t *usb = (hw_usb_t *)arg;

  while (usb != NULL &&
         atomic_load_explicit(&usb->running, memory_order_acquire)) {
    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 200000,
    };

    int rc = libusb_handle_events_timeout(usb->context, &timeout);
    if (rc == LIBUSB_ERROR_INTERRUPTED) {
      continue;
    }

    if (rc < 0) {
      struct timespec ts = {
          .tv_sec = 0,
          .tv_nsec = 100000000,
      };
      nanosleep(&ts, NULL);
    }
  }

  if (usb != NULL &&
      atomic_load_explicit(&usb->cleanup_in_thread, memory_order_acquire)) {
    _hw_usb_cleanup(usb);
    usb->thread_started = false;
    usb->init = false;
    usb->callback = NULL;
    usb->userdata = NULL;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_usb_t *hw_usb_init(hw_usb_callback_t callback, void *userdata) {
  sys_debugf("[usb] usb_init: callback=%p userdata=%p", (void *)callback,
             userdata);
  hw_usb_deinit(&_hw_usb_instance);

  if (callback == NULL) {
    return NULL;
  }

  sys_memset(&_hw_usb_instance, 0, sizeof(_hw_usb_instance));
  _hw_usb_instance.callback = callback;
  _hw_usb_instance.userdata = userdata;

  if (libusb_init(&_hw_usb_instance.context) != 0 ||
      _hw_usb_instance.context == NULL) {
    hw_usb_deinit(&_hw_usb_instance);
    return NULL;
  }

  _hw_usb_instance.init = true;

  (void)_hw_usb_emit_attached_devices(&_hw_usb_instance);

  if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
    int rc = libusb_hotplug_register_callback(
        _hw_usb_instance.context,
        LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
        LIBUSB_HOTPLUG_NO_FLAGS, LIBUSB_HOTPLUG_MATCH_ANY,
        LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, _hw_usb_hotplug_cb,
        &_hw_usb_instance, &_hw_usb_instance.hotplug_handle);
    if (rc == 0) {
      _hw_usb_instance.hotplug_registered = true;
      atomic_store_explicit(&_hw_usb_instance.cleanup_in_thread, false,
                            memory_order_release);
      atomic_store_explicit(&_hw_usb_instance.running, true,
                            memory_order_release);
      if (pthread_create(&_hw_usb_instance.thread, NULL, _hw_usb_event_thread,
                         &_hw_usb_instance) == 0) {
        _hw_usb_instance.thread_started = true;
      } else {
        atomic_store_explicit(&_hw_usb_instance.running, false,
                              memory_order_release);
        atomic_store_explicit(&_hw_usb_instance.cleanup_in_thread, false,
                              memory_order_release);
      }
    }
  }

  return &_hw_usb_instance;
}

void hw_usb_deinit(hw_usb_t *usb) {
  sys_debugf("[usb] usb_deinit: usb=%p", usb);
  if (usb == NULL) {
    return;
  }

  atomic_store_explicit(&usb->running, false, memory_order_release);

  if (usb->thread_started) {
    if (!pthread_equal(pthread_self(), usb->thread)) {
      pthread_join(usb->thread, NULL);
      _hw_usb_cleanup(usb);
      usb->thread_started = false;
      usb->init = false;
      usb->callback = NULL;
      usb->userdata = NULL;
      sys_memset(usb, 0, sizeof(*usb));
      return;
    }

    atomic_store_explicit(&usb->cleanup_in_thread, true, memory_order_release);
    usb->init = false;
    return;
  }

  _hw_usb_cleanup(usb);
  usb->thread_started = false;
  sys_memset(usb, 0, sizeof(*usb));
}

bool hw_usb_valid(const hw_usb_t *usb) {
  return usb != NULL && usb->init && usb->callback != NULL &&
         usb->context != NULL;
}
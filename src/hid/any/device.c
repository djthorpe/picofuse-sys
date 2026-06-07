#include "private.h"
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

static hid_t _hid[HID_CAPACITY] = {0};

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

/**
 * @brief Report whether a HID instance slot is initialized.
 */
static inline bool _hid_valid(hid_t *instance) {
  return instance != NULL && instance->queue != NULL;
}

/**
 * @brief Check whether a device pointer belongs to a given HID instance.
 */
static inline bool _hid_device_belongs_to_instance(const hid_t *instance,
                                                   const hid_device_t *device) {
  if (instance == NULL || device == NULL) {
    return false;
  }
  ptrdiff_t slot = device - instance->devices;
  return slot >= 0 && (size_t)slot < HID_DEVICE_CAPACITY;
}

/**
 * @brief Report whether any HID instance is currently active.
 */
bool _hid_has_valid_instances(void) {
  size_t i;
  for (i = 0u; i < HID_CAPACITY; ++i) {
    if (_hid_valid(&_hid[i])) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Find a registered HID device by device id.
 */
bool _hid_find_device_by_id(uint32_t id, hid_t **out_instance,
                            hid_device_t **out_device) {
  size_t i;
  size_t j;

  if (out_instance == NULL || out_device == NULL) {
    return false;
  }

  *out_instance = NULL;
  *out_device = NULL;

  for (i = 0u; i < HID_CAPACITY; ++i) {
    if (!_hid_valid(&_hid[i])) {
      continue;
    }

    for (j = 0u; j < HID_DEVICE_CAPACITY; ++j) {
      if (_hid[i].devices[j].type == hid_type_none) {
        continue;
      }
      if (_hid[i].devices[j].id != id) {
        continue;
      }

      *out_instance = &_hid[i];
      *out_device = &_hid[i].devices[j];
      return true;
    }
  }

  return false;
}

/**
 * @brief Find a registered HID timer device by backing sys_timer handle.
 */
bool _hid_find_device_by_timer(sys_timer_t *timer, hid_t **out_instance,
                               hid_device_t **out_device) {
  size_t i;
  size_t j;

  if (timer == NULL || out_instance == NULL || out_device == NULL) {
    return false;
  }

  *out_instance = NULL;
  *out_device = NULL;

  for (i = 0u; i < HID_CAPACITY; ++i) {
    if (!_hid_valid(&_hid[i])) {
      continue;
    }

    for (j = 0u; j < HID_DEVICE_CAPACITY; ++j) {
      if (_hid[i].devices[j].type == hid_type_none) {
        continue;
      }
      if (_hid[i].devices[j].type != hid_type_timer) {
        continue;
      }
      if ((sys_timer_t *)_hid[i].devices[j].userdata != timer) {
        continue;
      }

      *out_instance = &_hid[i];
      *out_device = &_hid[i].devices[j];
      return true;
    }
  }

  return false;
}

/**
 * @brief Retain a free device slot from an instance-local device pool.
 */
hid_device_t *_hid_device_retain(hid_t *instance, const char *name,
                                 hid_type_t type) {
  size_t i;

  if (!_hid_valid(instance)) {
    return NULL;
  }

  for (i = 0u; i < HID_DEVICE_CAPACITY; ++i) {
    if (instance->devices[i].type == hid_type_none) {
      sys_memset(&instance->devices[i], 0, sizeof(instance->devices[i]));
      instance->devices[i].instance = instance;
      instance->devices[i].name = name;
      instance->devices[i].type = type;
      return &instance->devices[i];
    }
  }

  return NULL;
}

/**
 * @brief Release a previously retained device slot back to the pool.
 */
void _hid_device_release(hid_t *instance, hid_device_t *device) {
  ptrdiff_t slot;

  if (!_hid_valid(instance) || device == NULL) {
    return;
  }

  slot = device - instance->devices;
  if (slot < 0 || (size_t)slot >= HID_DEVICE_CAPACITY) {
    return;
  }

  sys_memset(&instance->devices[(size_t)slot], 0, sizeof(hid_device_t));
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/**
 * @brief Initialize a HID instance using the provided event queue.
 */
hid_t *hid_init(sys_event_queue_t *queue) {
  if (queue == NULL || sys_event_queue_valid(queue) == false) {
    return NULL;
  }

  // Obtain a hid instance from the pool and initialize it with the provided
  // event queue.
  size_t i;
  hid_t *instance = NULL;
  for (i = 0u; i < HID_CAPACITY; ++i) {
    if (_hid[i].queue == NULL) {
      sys_memset(&_hid[i], 0, sizeof(_hid[i]));
      _hid[i].queue = queue;
      instance = &_hid[i];
      break;
    }
  }
  if (instance == NULL) {
    return NULL;
  }

  if (!_hid_event_pool_init(instance)) {
    sys_memset(instance, 0, sizeof(hid_t));
    return NULL;
  }

  // Install the global GPIO callback if not already installed by another
  // instance.
  _hid_gpio_callback_init();

  // Return the initialized instance.
  return instance;
}

/**
 * @brief Deinitialize a HID instance and release all registered devices.
 */
void hid_deinit(hid_t *instance) {
  if (!_hid_valid(instance)) {
    return;
  }

  // Deregister all devices registered to this instance.
  size_t i;
  for (i = 0u; i < HID_DEVICE_CAPACITY; ++i) {
    if (instance->devices[i].type != hid_type_none) {
      (void)hid_deregister(instance, &instance->devices[i]);
    }
  }

  _hid_event_pool_deinit(instance);

  // Clear the instance data and return it to the pool.
  sys_memset(instance, 0, sizeof(hid_t));

  // Remove the global GPIO callback if this was the last valid instance.
  _hid_gpio_callback_deinit();
}

/**
 * @brief Poll a HID instance for pending input.
 */
bool hid_poll(hid_t *instance) {
  bool processed = false;
  uint64_t poll_time_ms;
  size_t i;

  if (!_hid_valid(instance)) {
    return false;
  }

  poll_time_ms = sys_timestamp_ms();
  for (i = 0u; i < HID_DEVICE_CAPACITY; ++i) {
    hid_device_t *device = &instance->devices[i];

    if (device->type == hid_type_none || device->callbacks.read == NULL) {
      continue;
    }

    if (device->polling_interval_ms != 0u && device->last_event_ms != 0u &&
        (poll_time_ms - device->last_event_ms) < device->polling_interval_ms) {
      continue;
    }

    device->last_event_ms = poll_time_ms;
    if (device->callbacks.read(device, device->userdata)) {
      processed = true;
    }
  }

  return processed;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Register a new HID device and run optional backend init.
 */
hid_device_t *hid_register(hid_t *instance, const char *name, uint32_t id,
                           hid_type_t type, uint32_t polling_interval_ms,
                           void *userdata, hid_device_callbacks_t callbacks) {
  hid_device_t *device;

  device = _hid_device_retain(instance, name, type);
  if (device == NULL) {
    return NULL;
  } else {
    device->id = id;
    device->polling_interval_ms = polling_interval_ms;
    device->userdata = userdata;
    device->callbacks = callbacks;
  }

  // Call the user-provided init callback if available, and release the device
  // on failure.
  if (device->callbacks.init != NULL &&
      !device->callbacks.init(device->userdata)) {
    _hid_device_release(instance, device);
    return NULL;
  } else {
    sys_debugf("[hid] device registered: name=%s id=%08X type=%u", device->name,
               (unsigned int)device->id, (unsigned int)device->type);
    return device;
  }
}

/**
 * @brief Deregister a HID device, running teardown callbacks when present.
 */
bool hid_deregister(hid_t *instance, hid_device_t *device) {
  bool deinit_called = false;

  if (!_hid_valid(instance) ||
      !_hid_device_belongs_to_instance(instance, device)) {
    return false;
  }

  if (device->callbacks.deinit != NULL) {
    deinit_called = true;
    (void)device->callbacks.deinit(device->userdata);
    device->userdata = NULL;
  }

  if (!deinit_called && device->type == hid_type_gpio &&
      device->userdata != NULL) {
    hw_gpio_deinit((hw_gpio_t *)device->userdata);
    device->userdata = NULL;
  }

  sys_debugf("[hid] device de-registered: name=%s id=%08X type=%u",
             device->name, (unsigned int)device->id,
             (unsigned int)device->type);
  _hid_device_release(instance, device);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/**
 * @brief Enumerate registered HID devices across all active instances.
 */
hid_device_t *hid_device_next(hid_device_t *device) {
  size_t i;
  size_t j;

  if (device == NULL) {
    for (i = 0u; i < HID_CAPACITY; ++i) {
      if (!_hid_valid(&_hid[i])) {
        continue;
      }
      for (j = 0u; j < HID_DEVICE_CAPACITY; ++j) {
        if (_hid[i].devices[j].type != hid_type_none) {
          return &_hid[i].devices[j];
        }
      }
    }
    return NULL;
  }

  for (i = 0u; i < HID_CAPACITY; ++i) {
    ptrdiff_t slot;

    if (!_hid_valid(&_hid[i])) {
      continue;
    }

    slot = device - _hid[i].devices;
    if (slot < 0 || (size_t)slot >= HID_DEVICE_CAPACITY) {
      continue;
    }

    for (j = (size_t)slot + 1u; j < HID_DEVICE_CAPACITY; ++j) {
      if (_hid[i].devices[j].type != hid_type_none) {
        return &_hid[i].devices[j];
      }
    }

    for (j = i + 1u; j < HID_CAPACITY; ++j) {
      size_t k;

      if (!_hid_valid(&_hid[j])) {
        continue;
      }

      for (k = 0u; k < HID_DEVICE_CAPACITY; ++k) {
        if (_hid[j].devices[k].type != hid_type_none) {
          return &_hid[j].devices[k];
        }
      }
    }

    return NULL;
  }

  return NULL;
}

/**
 * @brief Read metadata fields from a HID device descriptor.
 */
bool hid_device_info(const hid_device_t *device, const char **out_name,
                     uint32_t *out_id, hid_type_t *out_type) {
  size_t i;

  if (out_name != NULL) {
    *out_name = NULL;
  }
  if (out_id != NULL) {
    *out_id = 0u;
  }
  if (out_type != NULL) {
    *out_type = hid_type_none;
  }

  if (device == NULL) {
    return false;
  }

  for (i = 0u; i < HID_CAPACITY; ++i) {
    if (!_hid_valid(&_hid[i]) ||
        !_hid_device_belongs_to_instance(&_hid[i], device)) {
      continue;
    }

    if (device->type == hid_type_none) {
      return false;
    }

    if (out_name != NULL) {
      *out_name = device->name;
    }
    if (out_id != NULL) {
      *out_id = device->id;
    }
    if (out_type != NULL) {
      *out_type = device->type;
    }

    return true;
  }

  return false;
}

/**
 * @brief Read userdata from a HID device descriptor.
 */
void *hid_device_userdata(const hid_device_t *device) {
  size_t i;

  if (device == NULL) {
    return NULL;
  }

  for (i = 0u; i < HID_CAPACITY; ++i) {
    if (!_hid_valid(&_hid[i]) ||
        !_hid_device_belongs_to_instance(&_hid[i], device)) {
      continue;
    }

    if (device->type == hid_type_none) {
      return NULL;
    }

    if (device->type == hid_type_timer && device->userdata != NULL) {
      return sys_timer_get_userdata((sys_timer_t *)device->userdata);
    }

    return device->userdata;
  }

  return NULL;
}

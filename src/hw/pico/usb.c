#include <picofuse/hw.h>
#include <picofuse/sys.h>
#include <string.h>
#include <tusb.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_usb_t {
  hw_usb_callback_t callback;
  void *userdata;
  bool init;
  bool enumeration_complete;
  uint64_t init_time_ms;
};

typedef struct {
  bool used;
  uint8_t daddr;
  hw_usb_device_t device;
} hw_usb_device_cache_t;

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_usb_t _hw_usb_instance = {0};
static struct hw_usb_t *_hw_usb_active = NULL;
static hw_usb_device_cache_t _hw_usb_cache[16] = {0};
static const uint16_t _hw_usb_language_id = 0x0409;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _hw_usb_cache_clear(void) {
  sys_memset(_hw_usb_cache, 0, sizeof(_hw_usb_cache));
}

static hw_usb_device_cache_t *_hw_usb_cache_get(uint8_t daddr) {
  for (size_t i = 0; i < (sizeof(_hw_usb_cache) / sizeof(_hw_usb_cache[0]));
       ++i) {
    if (_hw_usb_cache[i].used && _hw_usb_cache[i].daddr == daddr) {
      return &_hw_usb_cache[i];
    }
  }

  return NULL;
}

static hw_usb_device_cache_t *_hw_usb_cache_set(uint8_t daddr,
                                                const hw_usb_device_t *device) {
  hw_usb_device_cache_t *entry = _hw_usb_cache_get(daddr);
  if (entry == NULL) {
    for (size_t i = 0; i < (sizeof(_hw_usb_cache) / sizeof(_hw_usb_cache[0]));
         ++i) {
      if (!_hw_usb_cache[i].used) {
        entry = &_hw_usb_cache[i];
        entry->used = true;
        entry->daddr = daddr;
        break;
      }
    }
  }

  if (entry != NULL && device != NULL) {
    entry->device = *device;
  }

  return entry;
}

static void _hw_usb_cache_remove(uint8_t daddr) {
  hw_usb_device_cache_t *entry = _hw_usb_cache_get(daddr);
  if (entry != NULL) {
    sys_memset(entry, 0, sizeof(*entry));
  }
}

static void _hw_usb_copy_utf16_string_to_ascii(char *dst, size_t dst_size,
                                               const uint16_t *utf16_desc) {
  if (dst == NULL || dst_size == 0) {
    return;
  }

  dst[0] = '\0';
  if (utf16_desc == NULL) {
    return;
  }

  uint8_t desc_len = (uint8_t)(utf16_desc[0] & 0xffu);
  if (desc_len < 2) {
    return;
  }

  size_t utf16_len = (size_t)(desc_len - 2u) / sizeof(uint16_t);
  size_t out = 0;
  for (size_t i = 0; i < utf16_len && (out + 1) < dst_size; ++i) {
    uint16_t ch = utf16_desc[i + 1];
    dst[out++] = (ch <= 0x7fu) ? (char)ch : '?';
  }

  dst[out] = '\0';
}

static void _hw_usb_fill_strings(uint8_t daddr, hw_usb_device_t *device) {
  uint16_t buffer[HW_USB_STRING_MAX_LENGTH + 2] = {0};

  if (tuh_descriptor_get_manufacturer_string_sync(daddr, _hw_usb_language_id,
                                                  buffer, sizeof(buffer)) ==
      XFER_RESULT_SUCCESS) {
    _hw_usb_copy_utf16_string_to_ascii(device->manufacturer,
                                       sizeof(device->manufacturer), buffer);
  }

  if (tuh_descriptor_get_product_string_sync(daddr, _hw_usb_language_id, buffer,
                                             sizeof(buffer)) ==
      XFER_RESULT_SUCCESS) {
    _hw_usb_copy_utf16_string_to_ascii(device->product, sizeof(device->product),
                                       buffer);
  }

  if (tuh_descriptor_get_serial_string_sync(daddr, _hw_usb_language_id, buffer,
                                            sizeof(buffer)) ==
      XFER_RESULT_SUCCESS) {
    _hw_usb_copy_utf16_string_to_ascii(device->serial, sizeof(device->serial),
                                       buffer);
  }
}

static bool _hw_usb_build_device(uint8_t daddr, hw_usb_device_t *device) {
  if (device == NULL) {
    return false;
  }

  uint16_t vid = 0;
  uint16_t pid = 0;
  if (!tuh_vid_pid_get(daddr, &vid, &pid)) {
    return false;
  }

  tusb_desc_device_t desc = {0};
  if (tuh_descriptor_get_device_sync(daddr, &desc, sizeof(desc)) !=
      XFER_RESULT_SUCCESS) {
    return false;
  }

  sys_memset(device, 0, sizeof(*device));
  device->vid = vid;
  device->pid = pid;
  device->device_class = desc.bDeviceClass;
  device->device_subclass = desc.bDeviceSubClass;
  device->device_protocol = desc.bDeviceProtocol;

  _hw_usb_fill_strings(daddr, device);
  return true;
}

static void _hw_usb_emit_enumeration_complete_if_ready(void) {
  if (_hw_usb_active == NULL || !_hw_usb_active->init ||
      _hw_usb_active->enumeration_complete) {
    return;
  }

  // Give TinyUSB a short startup window to deliver initial mount callbacks.
  if ((sys_timestamp_ms() - _hw_usb_active->init_time_ms) < 1000u) {
    return;
  }

  _hw_usb_active->callback(_hw_usb_active, hw_usb_event_attached, NULL,
                           _hw_usb_active->userdata);
  _hw_usb_active->enumeration_complete = true;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_usb_t *hw_usb_init(hw_usb_callback_t callback, void *userdata) {
  hw_usb_deinit(&_hw_usb_instance);

  if (callback == NULL) {
    return NULL;
  }

  if (!tuh_inited() && !tuh_init(0)) {
    return NULL;
  }

  sys_memset(&_hw_usb_instance, 0, sizeof(_hw_usb_instance));
  _hw_usb_instance.callback = callback;
  _hw_usb_instance.userdata = userdata;
  _hw_usb_instance.init = true;
  _hw_usb_instance.init_time_ms = sys_timestamp_ms();

  _hw_usb_cache_clear();
  _hw_usb_active = &_hw_usb_instance;

  return &_hw_usb_instance;
}

void hw_usb_deinit(hw_usb_t *usb) {
  if (usb == NULL) {
    return;
  }

  if (_hw_usb_active == usb) {
    _hw_usb_active = NULL;
  }

  _hw_usb_cache_clear();
  sys_memset(usb, 0, sizeof(*usb));
}

bool hw_usb_valid(const hw_usb_t *usb) {
  return usb != NULL && usb->init && usb->callback != NULL;
}

///////////////////////////////////////////////////////////////////////////////
// PLATFORM INTEGRATION

void _hw_usb_poll(void) {
  if (_hw_usb_active == NULL || !_hw_usb_active->init) {
    return;
  }

  tuh_task();
  _hw_usb_emit_enumeration_complete_if_ready();
}

void tuh_mount_cb(uint8_t daddr) {
  if (_hw_usb_active == NULL || !_hw_usb_active->init) {
    return;
  }

  hw_usb_device_t device = {0};
  if (_hw_usb_build_device(daddr, &device)) {
    _hw_usb_cache_set(daddr, &device);
    _hw_usb_active->callback(_hw_usb_active, hw_usb_event_attached, &device,
                             _hw_usb_active->userdata);
  }
}

void tuh_umount_cb(uint8_t daddr) {
  if (_hw_usb_active == NULL || !_hw_usb_active->init) {
    return;
  }

  hw_usb_device_cache_t *entry = _hw_usb_cache_get(daddr);
  if (entry != NULL) {
    _hw_usb_active->callback(_hw_usb_active, hw_usb_event_detached,
                             &entry->device, _hw_usb_active->userdata);
    _hw_usb_cache_remove(daddr);
    return;
  }

  // Fallback if the device was not cached.
  hw_usb_device_t device = {0};
  _hw_usb_active->callback(_hw_usb_active, hw_usb_event_detached, &device,
                           _hw_usb_active->userdata);
}

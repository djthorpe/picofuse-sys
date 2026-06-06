#pragma once

#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct hid_device_t {
  const char *name;
  uint32_t id;
  uint16_t keycode;
  hid_type_t type;
  uint32_t polling_interval_ms;
  uint64_t last_event_ms;
  void *userdata;
  hw_gpio_t *gpio;
  hid_device_callbacks_t callbacks;
};

struct hid_t {
  sys_event_queue_t *queue;
  hid_device_t devices[HID_DEVICE_CAPACITY];
};

void _hid_gpio_callback_init(void);
void _hid_gpio_callback_deinit(void);
bool _hid_has_valid_instances(void);
bool _hid_find_device_by_id(uint32_t id, hid_t **out_instance,
                            hid_device_t **out_device);
hid_device_t *_hid_device_retain(hid_t *instance, const char *name,
                                 hid_type_t type);
void _hid_device_release(hid_t *instance, hid_device_t *device);

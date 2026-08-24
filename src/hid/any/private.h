#pragma once

#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct hid_device_t {
  hid_t *instance;
  const char *name;
  uint32_t id;
  uint16_t keycode;
  hid_state_t state;
  hid_type_t type;
  bool gpio_invert; // When true, _hid_gpio_callback() reports a falling
                    // edge as hid_state_on and a rising edge as
                    // hid_state_off (see hid_register_user_button(), whose
                    // pull-up-wired, active-low button is electrically
                    // opposite the default rising=on/falling=off
                    // convention used by hid_register_gpio_input() et al).
  uint32_t polling_interval_ms;
  uint64_t last_event_ms;
  void *userdata;
  bool timer_repeating;
  bool timer_remove_after_event;
  int fd; // Backing file descriptor for hid_type_evdev devices.
  int mt_slot; // Current ABS_MT_SLOT for hid_type_evdev devices; persists
               // across hid_poll() calls since drivers often omit
               // re-selecting slot 0 once it is already selected.
  hid_class_t hid_class; // Defaults to hid_class_unknown (see the zeroing in
                         // _hid_device_retain); only hid_type_evdev
                         // currently sets anything more specific.
  hid_device_callbacks_t callbacks;
};

struct hid_t {
  sys_event_queue_t *queue;
  hid_event_t *events;
  hid_device_t devices[HID_DEVICE_CAPACITY];
};

void _hid_gpio_callback_init(void);
void _hid_gpio_callback_deinit(void);
bool _hid_has_valid_instances(void);
bool _hid_find_device_by_id(uint32_t id, hid_t **out_instance,
                            hid_device_t **out_device);
bool _hid_find_device_by_timer(sys_timer_t *timer, hid_t **out_instance,
                               hid_device_t **out_device);
hid_device_t *_hid_device_retain(hid_t *instance, const char *name,
                                 hid_type_t type);
void _hid_device_release(hid_t *instance, hid_device_t *device);

bool _hid_event_pool_init(hid_t *instance);
void _hid_event_pool_deinit(hid_t *instance);
hid_event_t *_hid_event_pool_retain(hid_t *instance);

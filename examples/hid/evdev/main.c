/**
 * @file
 * @brief Linux evdev HID example.
 *
 * With no arguments, lists the evdev device nodes found in /dev/input.
 * With a device path argument, registers that device and prints its HID
 * events until interrupted (Ctrl+C).
 */

#include <picofuse/hid.h>
#include <picofuse/sys.h>

#define EVDEV_POLL_INTERVAL_MS 5u
#define EVDEV_QUEUE_CAPACITY 32u

static volatile bool _running = true;

static const char *hid_class_name(hid_class_t hid_class) {
  switch (hid_class) {
  case hid_class_unknown:
    return "unknown";
  case hid_class_keyboard:
    return "keyboard";
  case hid_class_mouse:
    return "mouse";
  case hid_class_joystick:
    return "joystick";
  case hid_class_touchscreen:
    return "touchscreen";
  case hid_class_sensor:
    return "sensor";
  }
  return "unknown";
}

static void print_device(const char *name, hid_type_t type,
                         hid_class_t hid_class, void *userdata) {
  (void)type;
  (void)userdata;
  sys_printf("  %s (%s)\n", name, hid_class_name(hid_class));
}

static void handle_event(const hid_event_t *event) {
  const char *device_name = "unknown";
  uint32_t device_id = 0u;
  hid_type_t device_type = hid_type_none;
  if (event->device != NULL) {
    (void)hid_device_info(event->device, &device_name, &device_id,
                          &device_type, NULL);
  }

  switch (event->type) {
  case hid_event_type_keycode: {
    const hid_keycode_t *key_event = &event->data.keycode;
    sys_printf("src=%s id=0x%08X state=0x%08X keycode=%s\n", device_name,
               (unsigned int)device_id, (unsigned int)key_event->state,
               hid_keycode_to_string(key_event->keycode));
    break;
  }

  case hid_event_type_touch: {
    const hid_touch_t *touch_event = &event->data.touch;
    sys_printf("src=%s id=0x%08X slot=%u state=0x%08X x=%d y=%d\n",
               device_name, (unsigned int)device_id,
               (unsigned int)touch_event->slot,
               (unsigned int)touch_event->state, (int)touch_event->point.x,
               (int)touch_event->point.y);
    break;
  }

  case hid_event_type_signal:
    if (event->data.signal.signal == SYS_ENV_SIGNAL_TERM ||
        event->data.signal.signal == SYS_ENV_SIGNAL_INT ||
        event->data.signal.signal == SYS_ENV_SIGNAL_QUIT) {
      sys_printf("shutdown signal received\n");
      _running = false;
    }
    break;

  case hid_event_type_none:
  case hid_event_type_metric:
  case hid_event_type_timer:
    break;
  }
}

static int list_devices(void) {
  sys_printf("evdev devices in /dev/input:\n");
  size_t count = hid_evdev_list(print_device, NULL);
  sys_printf("%u device(s) found\n", (unsigned int)count);
  return 0;
}

static int capture_device(const char *path) {
  sys_event_queue_t *queue = sys_event_queue_init(EVDEV_QUEUE_CAPACITY);
  if (queue == NULL) {
    sys_printf("failed to create event queue\n");
    return 1;
  }

  hid_t *hid = hid_init(queue);
  if (hid == NULL) {
    sys_printf("hid_init failed\n");
    sys_event_queue_deinit(queue);
    return 1;
  }

  // Not exclusive, so the desktop environment (if any) keeps receiving
  // events from the device too while this example is running.
  hid_device_t *device = hid_register_evdev(hid, path, false, NULL);
  if (device == NULL) {
    sys_printf("failed to register evdev device: %s\n", path);
    hid_deinit(hid);
    sys_event_queue_deinit(queue);
    return 1;
  }

  const char *name = NULL;
  uint32_t id = 0u;
  hid_type_t type = hid_type_none;
  hid_class_t hid_class = hid_class_unknown;
  (void)hid_device_info(device, &name, &id, &type, &hid_class);
  sys_printf("capturing %s (\"%s\", id=0x%08X, class=%s); press Ctrl+C to "
             "stop\n",
             path, (name != NULL) ? name : "?", (unsigned int)id,
             hid_class_name(hid_class));

  if (hid_register_signal(hid) == NULL) {
    sys_printf("hid signal registration failed; Ctrl+C will hard-exit\n");
  }

  while (_running) {
    (void)hid_poll(hid);

    sys_event_t raw_event;
    while ((raw_event = sys_event_queue_try_pop(queue)) != NULL) {
      hid_event_t *event = (hid_event_t *)raw_event;
      handle_event(event);
      hid_event_free(event);
    }

    sys_sleep_ms(EVDEV_POLL_INTERVAL_MS);
  }

  hid_deinit(hid);
  sys_event_queue_deinit(queue);
  return 0;
}

int main(int argc, char **argv) {
  sys_init();

  int status = (argc > 1) ? capture_device(argv[1]) : list_devices();

  sys_exit();
  return status;
}

/**
 * @file
 * @brief HID user-button runloop example (single core, exits on exit key).
 */

#include <picofuse/dev.h>
#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define I2C_BAUD_RATE 100000u
#define POLL_INTERVAL_MS 20u
#define BME680_POLL_INTERVAL_MS 30000u
#define HID_TIMER_INTERVAL_MS 1000u
#define HID_TIMER_ID 0x54494D45u
#define HID_TIMER_ONESHOT_INTERVAL_MS 5000u
#define HID_TIMER_ONESHOT_ID 0x35534543u
#define EXIT_KEYCODE KEYCODE_ESC

static sys_event_queue_t *_hid_queue = NULL;
static hid_t *_hid = NULL;
static hid_device_t *_user_button = NULL;
static hw_i2c_t *_i2c = NULL;
static hid_device_t *_tca9555_hid = NULL;
static hid_device_t *_bme680_hid = NULL;
static hid_device_t *_timer_hid = NULL;
static hid_device_t *_timer_oneshot_hid = NULL;
static sys_timer_t *_poll_timer = NULL;
static sys_atomic_t _event_count;

static const char *_timer_userdata = "periodic_1s";
static const char *_timer_oneshot_userdata = "oneshot_5s";

static uint8_t _poll_sentinel;

#define POLL_EVENT ((sys_event_t) & _poll_sentinel)

/**
 * @brief Post periodic poll events to the runloop.
 */
static void poll_timer_callback(sys_timer_t *timer) {
  (void)timer;
  (void)sys_runloop_post(POLL_EVENT);
}

/**
 * @brief Poll hardware and HID sources between runloop event dispatches.
 */
static void poll_callback(void) {
  hw_poll();
  if (_hid != NULL) {
    (void)hid_poll(_hid);
  }
}

/**
 * @brief Initialize HID and timers on worker 0.
 */
static void on_init(uint8_t worker_index) {
  if (worker_index != 0u) {
    return;
  }

  _hid_queue = sys_event_queue_init(32u);
  sys_assert(_hid_queue != NULL);

  _hid = hid_init(_hid_queue);
  sys_assert(_hid != NULL);

  hw_init();

  _user_button = hid_register_user_button(_hid, EXIT_KEYCODE);
  if (_user_button != NULL) {
    sys_printf("hid user button registered (exit key=%s)\n",
               hid_keycode_to_string(EXIT_KEYCODE));
  } else {
    sys_printf("hid user button not available for this board\n");
  }

  _timer_hid = hid_register_timer(_hid, HID_TIMER_ID, HID_TIMER_INTERVAL_MS,
                                  true, (void *)_timer_userdata);
  if (_timer_hid != NULL) {
    sys_printf("hid timer registered (id=0x%08X interval=%u ms)\n",
               (unsigned int)HID_TIMER_ID, (unsigned int)HID_TIMER_INTERVAL_MS);
  } else {
    sys_printf("hid timer registration failed\n");
  }

  _timer_oneshot_hid = hid_register_timer(_hid, HID_TIMER_ONESHOT_ID,
                                          HID_TIMER_ONESHOT_INTERVAL_MS, false,
                                          (void *)_timer_oneshot_userdata);
  if (_timer_oneshot_hid != NULL) {
    sys_printf("hid one-shot timer registered (id=0x%08X interval=%u ms)\n",
               (unsigned int)HID_TIMER_ONESHOT_ID,
               (unsigned int)HID_TIMER_ONESHOT_INTERVAL_MS);
  } else {
    sys_printf("hid one-shot timer registration failed\n");
  }

  _i2c = hw_i2c_init_default(I2C_BAUD_RATE);
  if (hw_i2c_valid(_i2c)) {
    _tca9555_hid =
        dev_pimoroni_pad_register(_hid, _i2c, DEV_TCA9555_I2C_ADDR_ANY, 0u);
    if (_tca9555_hid != NULL) {
      const char *name = NULL;
      uint32_t id = 0u;
      hid_type_t type = hid_type_none;
      (void)hid_device_info(_tca9555_hid, &name, &id, &type);
      sys_printf("tca9555 detected on default i2c bus (addr=0x%02X)\n",
                 (unsigned int)id);
    } else {
      sys_printf("no tca9555 detected on default i2c bus\n");
    }

    _bme680_hid = dev_bme680_hid_register_i2c_with_interval(
        _hid, _i2c, NULL, BME680_POLL_INTERVAL_MS);
    if (_bme680_hid != NULL) {
      const char *name = NULL;
      uint32_t id = 0u;
      hid_type_t type = hid_type_none;
      (void)hid_device_info(_bme680_hid, &name, &id, &type);
      sys_printf(
          "bme680 detected on default i2c bus (chip=0x%02X, poll=%u ms)\n",
          (unsigned int)id, (unsigned int)BME680_POLL_INTERVAL_MS);
    } else {
      sys_printf("no bme680 detected on default i2c bus\n");
    }
  } else {
    _i2c = NULL;
  }

  _poll_timer = sys_timer_init(POLL_INTERVAL_MS, NULL, poll_timer_callback);
  sys_assert(_poll_timer != NULL);
  sys_assert(sys_timer_start(_poll_timer));
}

/**
 * @brief Handle runloop events by draining queued HID events.
 */
static void on_event(sys_event_t event) {
  if (event != POLL_EVENT || _hid_queue == NULL) {
    return;
  }

  while (true) {
    hid_event_t *hid_event = (hid_event_t *)sys_event_queue_try_pop(_hid_queue);
    const hid_keycode_t *key_event;
    const hid_metric_t *metric_event;
    const char *device_name = "unknown";
    uint32_t device_id = 0u;
    hid_type_t device_type = hid_type_none;

    if (hid_event == NULL) {
      break;
    }

    uint32_t count = sys_atomic_inc(&_event_count);
    if (hid_event->type == hid_event_type_keycode) {
      key_event = &hid_event->data.keycode;

      if (hid_event->device != NULL) {
        (void)hid_device_info(hid_event->device, &device_name, &device_id,
                              &device_type);
      }

      sys_printf("hid event %u: core=%u src=%s id=0x%08X type=%u state=0x%08X "
                 "keycode=%s\n",
                 (unsigned int)count, (unsigned int)sys_thread_core(),
                 device_name, (unsigned int)device_id,
                 (unsigned int)device_type, (unsigned int)key_event->state,
                 hid_keycode_to_string(key_event->keycode));

      if (key_event->keycode == EXIT_KEYCODE) {
        sys_printf("exit key pressed (%s), shutting down runloop\n",
                   hid_keycode_to_string(EXIT_KEYCODE));
        hid_event_free(hid_event);
        sys_runloop_shutdown(0u);
        break;
      }
    } else if (hid_event->type == hid_event_type_metric) {
      metric_event = &hid_event->data.metric;
      if (hid_event->device != NULL) {
        (void)hid_device_info(hid_event->device, &device_name, &device_id,
                              &device_type);
      }

      sys_printf("hid metric %u: core=%u src=%s id=0x%08X %s=%f %s\n",
                 (unsigned int)count, (unsigned int)sys_thread_core(),
                 device_name, (unsigned int)device_id,
                 (metric_event->name != NULL) ? metric_event->name : "unknown",
                 (double)metric_event->value,
                 (metric_event->unit != NULL) ? metric_event->unit : "");
    } else if (hid_event->type == hid_event_type_timer) {
      const char *timer_payload = (const char *)hid_event->data.timer.userdata;

      if (hid_event->device != NULL) {
        (void)hid_device_info(hid_event->device, &device_name, &device_id,
                              &device_type);
      }

      sys_printf("hid timer %u: core=%u src=%s id=0x%08X payload=%s\n",
                 (unsigned int)count, (unsigned int)sys_thread_core(),
                 device_name, (unsigned int)device_id,
                 (timer_payload != NULL) ? timer_payload : "none");
    }

    hid_event_free(hid_event);
  }
}

/**
 * @brief Clean up timers, HID registrations, and queue resources.
 */
static void on_exit(uint8_t worker_index) {
  if (worker_index != 0u) {
    return;
  }

  if (_poll_timer != NULL) {
    sys_timer_deinit(_poll_timer);
    _poll_timer = NULL;
  }

  if (_hid != NULL && _user_button != NULL) {
    (void)hid_deregister(_hid, _user_button);
    _user_button = NULL;
  }

  if (_hid != NULL && _tca9555_hid != NULL) {
    (void)hid_deregister(_hid, _tca9555_hid);
    _tca9555_hid = NULL;
  }

  if (_hid != NULL && _bme680_hid != NULL) {
    (void)hid_deregister(_hid, _bme680_hid);
    _bme680_hid = NULL;
  }

  if (_hid != NULL && _timer_hid != NULL) {
    (void)hid_deregister(_hid, _timer_hid);
    _timer_hid = NULL;
  }

  if (_hid != NULL && _timer_oneshot_hid != NULL) {
    const char *name = NULL;
    uint32_t id = 0u;
    hid_type_t type = hid_type_none;

    // One-shot timer may already have self-deregistered after first fire.
    if (hid_device_info(_timer_oneshot_hid, &name, &id, &type)) {
      (void)hid_deregister(_hid, _timer_oneshot_hid);
    }
    _timer_oneshot_hid = NULL;
  }

  if (_i2c != NULL) {
    hw_i2c_deinit(_i2c);
    _i2c = NULL;
  }

  if (_hid != NULL) {
    hid_deinit(_hid);
    _hid = NULL;
  }

  if (_hid_queue != NULL) {
    sys_event_queue_deinit(_hid_queue);
    _hid_queue = NULL;
  }

  hw_exit();
}

int main(void) {
  sys_init();
  sys_atomic_init(&_event_count, 0u);

  (void)sys_runloop_run(0, on_init, on_event, poll_callback, on_exit);

  sys_printf("hid runloop example complete, processed %u events\n",
             (unsigned int)sys_atomic_get(&_event_count));

  sys_exit();
  return 0;
}

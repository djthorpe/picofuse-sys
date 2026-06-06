/**
 * @file
 * @brief HID user-button runloop example (single core, exits on exit key).
 */

#include <picofuse/dev.h>
#include <picofuse/hid.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define I2C_BAUD_RATE 100000u
#define BME680_POLL_INTERVAL_MS 30000u
#define HID_TIMER_INTERVAL_MS 1000u
#define HID_TIMER_ID 0x54494D45u
#define HID_TIMER_ONESHOT_INTERVAL_MS 5000u
#define HID_TIMER_ONESHOT_ID 0x35534543u
#define EXIT_KEYCODE KEYCODE_ESC

static hid_t *_hid = NULL;
static hw_i2c_t *_i2c = NULL;
static hw_i2c_t *_touch_i2c = NULL;

static const char *_timer_userdata = "periodic_1s";
static const char *_timer_oneshot_userdata = "oneshot_5s";

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
  hid_device_t *user_button;
  hid_device_t *timer_hid;
  hid_device_t *timer_oneshot_hid;
  hid_device_t *tca9555_hid;
  hid_device_t *bme680_hid;

  // We only do init on the main thread
  if (worker_index != 0u) {
    return;
  }

  // Initialize hardware and HID
  hw_init();

  // Initialize the event producer
  _hid = hid_init(sys_runloop_queue());
  sys_assert(_hid != NULL);

  // Register a user button to exit the runloop when pressed.
  user_button = hid_register_user_button(_hid, EXIT_KEYCODE);
  if (user_button != NULL) {
    sys_printf("hid user button registered (exit key=%s)\n",
               hid_keycode_to_string(EXIT_KEYCODE));
  } else {
    sys_printf("hid user button not available for this board\n");
  }

  // Register timers to demonstrate periodic and one-shot timer events.
  timer_hid = hid_register_timer(_hid, HID_TIMER_ID, HID_TIMER_INTERVAL_MS,
                                 true, (void *)_timer_userdata);
  if (timer_hid != NULL) {
    sys_printf("hid timer registered (id=0x%08X interval=%u ms)\n",
               (unsigned int)HID_TIMER_ID, (unsigned int)HID_TIMER_INTERVAL_MS);
  } else {
    sys_printf("hid timer registration failed\n");
  }

  timer_oneshot_hid = hid_register_timer(_hid, HID_TIMER_ONESHOT_ID,
                                         HID_TIMER_ONESHOT_INTERVAL_MS, false,
                                         (void *)_timer_oneshot_userdata);
  if (timer_oneshot_hid != NULL) {
    sys_printf("hid one-shot timer registered (id=0x%08X interval=%u ms)\n",
               (unsigned int)HID_TIMER_ONESHOT_ID,
               (unsigned int)HID_TIMER_ONESHOT_INTERVAL_MS);
  } else {
    sys_printf("hid one-shot timer registration failed\n");
  }

  // Register the I2C devices after timers to demonstrate producing events
  _i2c = hw_i2c_init_default(I2C_BAUD_RATE);
  if (!hw_i2c_valid(_i2c)) {
    sys_printf("default i2c init failed (baud=%u)\n",
               (unsigned int)I2C_BAUD_RATE);
    _i2c = NULL;
    return;
  }

  tca9555_hid =
      dev_pimoroni_pad_register(_hid, _i2c, DEV_TCA9555_I2C_ADDR_ANY, 0u);
  if (tca9555_hid != NULL) {
    const char *name = NULL;
    uint32_t id = 0u;
    hid_type_t type = hid_type_none;
    (void)hid_device_info(tca9555_hid, &name, &id, &type);
    sys_printf("tca9555 detected on default i2c bus (addr=0x%02X)\n",
               (unsigned int)id);
  }

  bme680_hid =
      dev_bme680_hid_register_i2c(_hid, _i2c, NULL, BME680_POLL_INTERVAL_MS);
  if (bme680_hid != NULL) {
    const char *name = NULL;
    uint32_t id = 0u;
    hid_type_t type = hid_type_none;
    (void)hid_device_info(bme680_hid, &name, &id, &type);
    sys_printf("bme680 detected on default i2c bus (chip=0x%02X, poll=%u ms)\n",
               (unsigned int)id, (unsigned int)BME680_POLL_INTERVAL_MS);
  }

#if defined(PIMORONI_PRESTO_TOUCH_I2C) &&                                      \
    defined(PIMORONI_PRESTO_TOUCH_SDA_PIN) &&                                  \
    defined(PIMORONI_PRESTO_TOUCH_SCL_PIN) &&                                  \
    defined(PIMORONI_PRESTO_TOUCH_INT_PIN) &&                                  \
    defined(PIMORONI_PRESTO_TOUCH_I2C_ADDR)
  {
    hw_gpio_t *touch_sda =
        hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_SDA_PIN, HW_GPIO_I2C);
    hw_gpio_t *touch_scl =
        hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_SCL_PIN, HW_GPIO_I2C);
    hw_gpio_t *touch_int =
        hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_INT_PIN, HW_GPIO_PULLUP);

    if (!hw_gpio_valid(touch_sda) || !hw_gpio_valid(touch_scl) ||
        !hw_gpio_valid(touch_int)) {
      sys_printf("ft6236 touch GPIO init failed\n");
      hw_gpio_deinit(touch_int);
      hw_gpio_deinit(touch_scl);
      hw_gpio_deinit(touch_sda);
    } else {
      _touch_i2c = hw_i2c_init(PIMORONI_PRESTO_TOUCH_I2C, touch_sda, touch_scl,
                               I2C_BAUD_RATE);
      if (!hw_i2c_valid(_touch_i2c)) {
        sys_printf("ft6236 touch I2C init failed\n");
        hw_gpio_deinit(touch_int);
        hw_gpio_deinit(touch_scl);
        hw_gpio_deinit(touch_sda);
        _touch_i2c = NULL;
      } else {
        dev_ft6236_config_t touch_config = {0};
        dev_ft6236_default_config(&touch_config);
        touch_config.i2c_address = PIMORONI_PRESTO_TOUCH_I2C_ADDR;

        hid_device_t *touch_hid =
            dev_ft6236_hid_register(_hid, _touch_i2c, touch_int, &touch_config);
        if (touch_hid != NULL) {
          const char *name = NULL;
          uint32_t id = 0u;
          hid_type_t type = hid_type_none;
          (void)hid_device_info(touch_hid, &name, &id, &type);
          sys_printf("ft6236 touch registered (addr=0x%02X, i2c=%u)\n",
                     (unsigned int)id, (unsigned int)PIMORONI_PRESTO_TOUCH_I2C);
        } else {
          sys_printf("ft6236 touch registration failed\n");
          hw_i2c_deinit(_touch_i2c);
          _touch_i2c = NULL;
        }
      }
    }
  }
#endif
}

/**
 * @brief Handle runloop HID events.
 */
static void on_event(sys_event_t event) {
  hid_event_t *hid_event = (hid_event_t *)event;
  if (hid_event == NULL) {
    return;
  }

  const char *device_name = "unknown";
  uint32_t device_id = 0u;
  hid_type_t device_type = hid_type_none;
  if (hid_event->device != NULL) {
    (void)hid_device_info(hid_event->device, &device_name, &device_id,
                          &device_type);
  }

  switch (hid_event->type) {
  case hid_event_type_keycode:
    const hid_keycode_t *key_event = &hid_event->data.keycode;
    sys_printf("hid event: core=%u src=%s id=0x%08X type=%u state=0x%08X "
               "keycode=%s\n",
               (unsigned int)sys_thread_core(), device_name,
               (unsigned int)device_id, (unsigned int)device_type,
               (unsigned int)key_event->state,
               hid_keycode_to_string(key_event->keycode));

    if (key_event->keycode == EXIT_KEYCODE) {
      sys_printf("exit key pressed (%s), shutting down runloop\n",
                 hid_keycode_to_string(EXIT_KEYCODE));
      hid_event_free(hid_event);
      sys_runloop_shutdown(0u);
      return;
    }
    break;

  case hid_event_type_metric:
    const hid_metric_t *metric_event = &hid_event->data.metric;
    sys_printf("hid metric: core=%u src=%s id=0x%08X %s=%f %s\n",
               (unsigned int)sys_thread_core(), device_name,
               (unsigned int)device_id,
               (metric_event->name != NULL) ? metric_event->name : "unknown",
               (double)metric_event->value,
               (metric_event->unit != NULL) ? metric_event->unit : "");
    break;

  case hid_event_type_touch: {
    const hid_touch_t *touch_event = &hid_event->data.touch;
    sys_printf(
        "hid touch: core=%u src=%s id=0x%08X slot=%u state=0x%08X x=%d y=%d\n",
        (unsigned int)sys_thread_core(), device_name, (unsigned int)device_id,
        (unsigned int)touch_event->slot, (unsigned int)touch_event->state,
        (int)touch_event->point.x, (int)touch_event->point.y);
    break;
  }

  case hid_event_type_timer: {
    const char *timer_payload = (const char *)hid_event->data.timer.userdata;
    sys_printf("hid timer: core=%u src=%s id=0x%08X payload=%s\n",
               (unsigned int)sys_thread_core(), device_name,
               (unsigned int)device_id,
               (timer_payload != NULL) ? timer_payload : "none");
    break;
  }
  }

  hid_event_free(hid_event);
}

/**
 * @brief Clean up timers, HID registrations, and queue resources.
 */
static void on_exit(uint8_t worker_index) {
  if (worker_index != 0u) {
    return;
  }

  hid_deinit(_hid);
  if (_touch_i2c != NULL) {
    hw_i2c_deinit(_touch_i2c);
    _touch_i2c = NULL;
  }
  hw_i2c_deinit(_i2c);
  hw_exit();
}

int main(void) {
  sys_init();
  sys_runloop_run(0, on_init, on_event, poll_callback, on_exit);
  sys_exit();
  return 0;
}

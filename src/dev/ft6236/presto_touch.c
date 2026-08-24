#include <picofuse/dev.h>
#include <picofuse/hid.h>
#include <picofuse/sys.h>

#if defined(__has_include)
#if __has_include(<boards/presto.h>)
#include <boards/presto.h>
#endif
#endif

#if defined(PIMORONI_PRESTO_TOUCH_I2C) &&                                      \
    defined(PIMORONI_PRESTO_TOUCH_SDA_PIN) &&                                  \
    defined(PIMORONI_PRESTO_TOUCH_SCL_PIN) &&                                  \
    defined(PIMORONI_PRESTO_TOUCH_INT_PIN) &&                                  \
    defined(PIMORONI_PRESTO_TOUCH_I2C_ADDR)
#define _DEV_PRESTO_TOUCH_SUPPORTED 1
#else
#define _DEV_PRESTO_TOUCH_SUPPORTED 0
#endif

#if _DEV_PRESTO_TOUCH_SUPPORTED

typedef struct {
  dev_ft6236_t *device;
  hw_i2c_t *i2c;
  hw_gpio_t *sda;
  hw_gpio_t *scl;
  hid_event_t previous_events[DEV_FT6236_MAX_POINTS];
  bool has_previous_events;
} dev_presto_touch_hid_ctx_t;

static bool _dev_presto_touch_hid_init(hid_device_t *device, void *userdata);
static bool _dev_presto_touch_hid_read(hid_device_t *device, void *userdata);
static bool _dev_presto_touch_hid_deinit(hid_device_t *device, void *userdata);

static const hid_device_callbacks_t _dev_presto_touch_hid_callbacks = {
    .init = _dev_presto_touch_hid_init,
    .read = _dev_presto_touch_hid_read,
    .deinit = _dev_presto_touch_hid_deinit,
};

static bool _dev_presto_touch_active(const hid_event_t *event) {
  return event != NULL && event->type == hid_event_type_touch &&
         (event->data.touch.state & hid_state_on) != 0u;
}

static bool _dev_presto_touch_event_equal(const hid_event_t *lhs,
                                          const hid_event_t *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }

  if (lhs->type != hid_event_type_touch || rhs->type != hid_event_type_touch) {
    return false;
  }

  return lhs->data.touch.state == rhs->data.touch.state &&
         lhs->data.touch.slot == rhs->data.touch.slot &&
         lhs->data.touch.point.x == rhs->data.touch.point.x &&
         lhs->data.touch.point.y == rhs->data.touch.point.y;
}

static bool
_dev_presto_touch_frame_equal(const hid_event_t lhs[DEV_FT6236_MAX_POINTS],
                              const hid_event_t rhs[DEV_FT6236_MAX_POINTS]) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }

  for (size_t slot = 0u; slot < DEV_FT6236_MAX_POINTS; ++slot) {
    if (!_dev_presto_touch_event_equal(&lhs[slot], &rhs[slot])) {
      return false;
    }
  }

  return true;
}

static void
_dev_presto_touch_copy_events(hid_event_t dst[DEV_FT6236_MAX_POINTS],
                              const hid_event_t src[DEV_FT6236_MAX_POINTS]) {
  if (dst == NULL || src == NULL) {
    return;
  }

  sys_memcpy(dst, src, sizeof(hid_event_t) * DEV_FT6236_MAX_POINTS);
}

static void _dev_presto_touch_cleanup_bus(hw_i2c_t *i2c, hw_gpio_t *int_pin,
                                          hw_gpio_t *scl, hw_gpio_t *sda) {
  if (hw_i2c_valid(i2c)) {
    hw_i2c_deinit(i2c);
  }
  if (hw_gpio_valid(int_pin)) {
    hw_gpio_deinit(int_pin);
  }
  if (hw_gpio_valid(scl)) {
    hw_gpio_deinit(scl);
  }
  if (hw_gpio_valid(sda)) {
    hw_gpio_deinit(sda);
  }
}

static bool _dev_presto_touch_hid_init(hid_device_t *device, void *userdata) {
  dev_presto_touch_hid_ctx_t *ctx = (dev_presto_touch_hid_ctx_t *)userdata;
  (void)device;
  return ctx != NULL && ctx->device != NULL && hw_i2c_valid(ctx->i2c) &&
         hw_gpio_valid(ctx->sda) && hw_gpio_valid(ctx->scl);
}

static bool _dev_presto_touch_hid_read(hid_device_t *device, void *userdata) {
  dev_presto_touch_hid_ctx_t *ctx = (dev_presto_touch_hid_ctx_t *)userdata;
  hid_event_t current_events[DEV_FT6236_MAX_POINTS] = {0};
  uint8_t current_touch_count = 0u;
  bool any_queued = false;

  if (device == NULL || ctx == NULL || ctx->device == NULL) {
    return false;
  }

  if (!dev_ft6236_poll(ctx->device, current_events, &current_touch_count)) {
    return false;
  }

  if (ctx->has_previous_events &&
      _dev_presto_touch_frame_equal(current_events, ctx->previous_events)) {
    return false;
  }

  for (size_t slot = 0u; slot < DEV_FT6236_MAX_POINTS; ++slot) {
    const hid_event_t *current = &current_events[slot];
    const hid_event_t *previous = &ctx->previous_events[slot];

    if (_dev_presto_touch_active(current)) {
      any_queued = hid_event_queue_touch(device, current->data.touch.state,
                                         current->data.touch.point,
                                         current->data.touch.slot) ||
                   any_queued;
      continue;
    }

    if (ctx->has_previous_events && _dev_presto_touch_active(previous)) {
      hid_touch_t released = previous->data.touch;
      released.state = hid_state_off | hid_state_falling;
      any_queued = hid_event_queue_touch(device, released.state, released.point,
                                         released.slot) ||
                   any_queued;
    }
  }

  _dev_presto_touch_copy_events(ctx->previous_events, current_events);
  ctx->has_previous_events = true;
  (void)current_touch_count;
  return any_queued;
}

static bool _dev_presto_touch_hid_deinit(hid_device_t *device, void *userdata) {
  dev_presto_touch_hid_ctx_t *ctx = (dev_presto_touch_hid_ctx_t *)userdata;

  (void)device;

  if (ctx == NULL) {
    return false;
  }

  if (ctx->device != NULL) {
    dev_ft6236_deinit(ctx->device);
    ctx->device = NULL;
  }
  if (hw_i2c_valid(ctx->i2c)) {
    hw_i2c_deinit(ctx->i2c);
    ctx->i2c = NULL;
  }
  if (hw_gpio_valid(ctx->scl)) {
    hw_gpio_deinit(ctx->scl);
    ctx->scl = NULL;
  }
  if (hw_gpio_valid(ctx->sda)) {
    hw_gpio_deinit(ctx->sda);
    ctx->sda = NULL;
  }

  sys_free(ctx);
  return true;
}

hid_device_t *dev_presto_touch_register(hid_t *hid, uint32_t i2c_baud_rate) {
  hw_gpio_t *touch_sda;
  hw_gpio_t *touch_scl;
  hw_gpio_t *touch_int;
  hw_i2c_t *touch_i2c;
  dev_ft6236_t *device;
  dev_ft6236_config_t touch_config = {0};
  dev_presto_touch_hid_ctx_t *ctx;
  hid_device_t *hid_device;

  if (hid == NULL) {
    return NULL;
  }

  if (i2c_baud_rate == 0u) {
    i2c_baud_rate = 100000u;
  }

  touch_sda = hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_SDA_PIN, HW_GPIO_I2C);
  touch_scl = hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_SCL_PIN, HW_GPIO_I2C);
  touch_int = hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_INT_PIN, HW_GPIO_PULLUP);
  if (!hw_gpio_valid(touch_sda) || !hw_gpio_valid(touch_scl) ||
      !hw_gpio_valid(touch_int)) {
    _dev_presto_touch_cleanup_bus(NULL, touch_int, touch_scl, touch_sda);
    return NULL;
  }

  touch_i2c = hw_i2c_init(PIMORONI_PRESTO_TOUCH_I2C, touch_sda, touch_scl,
                          i2c_baud_rate);
  if (!hw_i2c_valid(touch_i2c)) {
    _dev_presto_touch_cleanup_bus(touch_i2c, touch_int, touch_scl, touch_sda);
    return NULL;
  }

  dev_ft6236_default_config(&touch_config);
  touch_config.i2c_address = PIMORONI_PRESTO_TOUCH_I2C_ADDR;

  device = dev_ft6236_init(touch_i2c, touch_int, &touch_config);
  if (device == NULL) {
    _dev_presto_touch_cleanup_bus(touch_i2c, touch_int, touch_scl, touch_sda);
    return NULL;
  }

  ctx = (dev_presto_touch_hid_ctx_t *)sys_calloc(1u, sizeof(*ctx));
  if (ctx == NULL) {
    dev_ft6236_deinit(device);
    _dev_presto_touch_cleanup_bus(touch_i2c, NULL, touch_scl, touch_sda);
    return NULL;
  }

  ctx->device = device;
  ctx->i2c = touch_i2c;
  ctx->sda = touch_sda;
  ctx->scl = touch_scl;

  hid_device = hid_register(hid, "presto_touch",
                            (uint32_t)PIMORONI_PRESTO_TOUCH_I2C_ADDR,
                            hid_type_other, hid_class_touchscreen, 0u, ctx,
                            _dev_presto_touch_hid_callbacks);
  if (hid_device == NULL) {
    _dev_presto_touch_hid_deinit(NULL, ctx);
    return NULL;
  }

  return hid_device;
}

#else

hid_device_t *dev_presto_touch_register(hid_t *hid, uint32_t i2c_baud_rate) {
  (void)hid;
  (void)i2c_baud_rate;
  sys_debugf(
      "[hid] presto touch register unavailable: board touch macros undefined");
  return NULL;
}

#endif
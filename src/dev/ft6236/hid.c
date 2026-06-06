#include <picofuse/dev.h>
#include <picofuse/hid.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

#define FT6236_DEFAULT_I2C_ADDR 0x48u

typedef struct {
  dev_ft6236_t *device;
  hid_event_t previous_events[DEV_FT6236_MAX_POINTS];
  bool has_previous_events;
} dev_ft6236_hid_ctx_t;

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

static bool _dev_ft6236_hid_init(void *userdata);
static bool _dev_ft6236_hid_read(hid_device_t *device, void *userdata);
static bool _dev_ft6236_hid_deinit(void *userdata);

static const hid_device_callbacks_t _dev_ft6236_hid_callbacks = {
    .init = _dev_ft6236_hid_init,
    .read = _dev_ft6236_hid_read,
    .deinit = _dev_ft6236_hid_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _dev_ft6236_touch_active(const hid_event_t *event) {
  return event != NULL && event->type == hid_event_type_touch &&
         (event->data.touch.state & hid_state_on) != 0u;
}

static void
_dev_ft6236_copy_events(hid_event_t dst[DEV_FT6236_MAX_POINTS],
                        const hid_event_t src[DEV_FT6236_MAX_POINTS]) {
  if (dst == NULL || src == NULL) {
    return;
  }

  sys_memcpy(dst, src, sizeof(hid_event_t) * DEV_FT6236_MAX_POINTS);
}

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

static bool _dev_ft6236_hid_init(void *userdata) {
  dev_ft6236_hid_ctx_t *ctx = (dev_ft6236_hid_ctx_t *)userdata;
  return ctx != NULL && ctx->device != NULL;
}

static bool _dev_ft6236_hid_read(hid_device_t *device, void *userdata) {
  dev_ft6236_hid_ctx_t *ctx = (dev_ft6236_hid_ctx_t *)userdata;
  hid_event_t current_events[DEV_FT6236_MAX_POINTS] = {0};
  uint8_t current_touch_count = 0u;
  bool any_queued = false;

  if (device == NULL || ctx == NULL || ctx->device == NULL) {
    return false;
  }

  if (!dev_ft6236_poll(ctx->device, current_events, &current_touch_count)) {
    return false;
  }

  for (size_t slot = 0u; slot < DEV_FT6236_MAX_POINTS; ++slot) {
    const hid_event_t *current = &current_events[slot];
    const hid_event_t *previous = &ctx->previous_events[slot];

    if (_dev_ft6236_touch_active(current)) {
      any_queued = hid_event_queue_touch(device, current->data.touch.state,
                                         current->data.touch.point,
                                         current->data.touch.slot) ||
                   any_queued;
      continue;
    }

    if (ctx->has_previous_events && _dev_ft6236_touch_active(previous)) {
      hid_touch_t released = previous->data.touch;
      released.state = hid_state_off | hid_state_falling;
      any_queued = hid_event_queue_touch(device, released.state, released.point,
                                         released.slot) ||
                   any_queued;
    }
  }

  _dev_ft6236_copy_events(ctx->previous_events, current_events);
  ctx->has_previous_events = true;
  (void)current_touch_count;
  return any_queued;
}

static bool _dev_ft6236_hid_deinit(void *userdata) {
  dev_ft6236_hid_ctx_t *ctx = (dev_ft6236_hid_ctx_t *)userdata;

  if (ctx == NULL || ctx->device == NULL) {
    return false;
  }

  dev_ft6236_deinit(ctx->device);
  sys_free(ctx);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *dev_ft6236_hid_register(hid_t *hid, hw_i2c_t *i2c,
                                      hw_gpio_t *int_pin,
                                      const dev_ft6236_config_t *config) {
  dev_ft6236_hid_ctx_t *ctx;
  dev_ft6236_config_t resolved = {0};
  dev_ft6236_t *device;
  hid_device_t *hid_device;

  if (hid == NULL || i2c == NULL) {
    return NULL;
  }

  dev_ft6236_default_config(&resolved);
  if (config != NULL) {
    resolved = *config;
    if (resolved.i2c_address == 0u) {
      resolved.i2c_address = FT6236_DEFAULT_I2C_ADDR;
    }
  }

  device = dev_ft6236_init(i2c, int_pin, &resolved);
  if (device == NULL) {
    return NULL;
  }

  ctx = (dev_ft6236_hid_ctx_t *)sys_calloc(1u, sizeof(*ctx));
  if (ctx == NULL) {
    dev_ft6236_deinit(device);
    return NULL;
  }

  ctx->device = device;

  hid_device = hid_register(hid, "ft6236", (uint32_t)resolved.i2c_address,
                            hid_type_other, 0u, ctx, _dev_ft6236_hid_callbacks);
  if (hid_device == NULL) {
    sys_free(ctx);
    dev_ft6236_deinit(device);
    return NULL;
  }

  return hid_device;
}

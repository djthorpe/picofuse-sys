#include <picofuse/dev.h>
#include <picofuse/hid.h>
#include <picofuse/sys.h>

#include "private.h"

typedef struct {
  dev_tca9555_t *device;
  hid_device_t *hid_device;
  uint16_t pending_value;
  uint16_t last_pressed;
  bool has_last_pressed;
  sys_atomic_t pending_flag;
} dev_pimoroni_pad_hid_ctx_t;

typedef struct {
  uint16_t pin;
  uint16_t keycode;
} dev_pimoroni_pad_keymap_t;

#define PIN_SW_UP (1u << 1)
#define PIN_SW_LEFT (1u << 2)
#define PIN_SW_RIGHT (1u << 3)
#define PIN_SW_DOWN (1u << 4)
#define PIN_SW_SELECT (1u << 5)
#define PIN_SW_START (1u << 11)
#define PIN_SW_A (1u << 12)
#define PIN_SW_X (1u << 13)
#define PIN_SW_B (1u << 14)
#define PIN_SW_Y (1u << 15)

#define SWITCH_MASK                                                            \
  (PIN_SW_UP | PIN_SW_LEFT | PIN_SW_RIGHT | PIN_SW_DOWN | PIN_SW_SELECT |      \
   PIN_SW_START | PIN_SW_A | PIN_SW_X | PIN_SW_B | PIN_SW_Y)

static const dev_pimoroni_pad_keymap_t _dev_pimoroni_pad_keymap[] = {
    {PIN_SW_UP, KEYCODE_UP},
    {PIN_SW_DOWN, KEYCODE_DOWN},
    {PIN_SW_LEFT, KEYCODE_LEFT},
    {PIN_SW_RIGHT, KEYCODE_RIGHT},
    // Board wiring maps these expander bits to opposite printed labels.
    {PIN_SW_A, KEYCODE_B},
    {PIN_SW_X, KEYCODE_Y},
    {PIN_SW_B, KEYCODE_A},
    {PIN_SW_Y, KEYCODE_X},
    {PIN_SW_SELECT, KEYCODE_KPMINUS},
    {PIN_SW_START, KEYCODE_KPPLUS},
};

static bool _dev_pimoroni_pad_hid_init(hid_device_t *device, void *userdata);
static bool _dev_pimoroni_pad_hid_read(hid_device_t *device, void *userdata);
static bool _dev_pimoroni_pad_hid_deinit(hid_device_t *device, void *userdata);
static bool _dev_pimoroni_pad_hid_event(dev_pimoroni_pad_hid_ctx_t *ctx,
                                        uint16_t value, bool value_valid);
static void _dev_pimoroni_pad_hid_callback(dev_tca9555_t *tca9555,
                                           uint16_t value, void *userdata);

/**
 * @brief Shared lifecycle/read callbacks used for Pimoroni pad registration.
 */
static const hid_device_callbacks_t _dev_pimoroni_pad_hid_callbacks = {
    .init = _dev_pimoroni_pad_hid_init,
    .read = _dev_pimoroni_pad_hid_read,
    .deinit = _dev_pimoroni_pad_hid_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

/**
 * @brief Validate Pimoroni pad userdata during HID registration.
 */
static bool _dev_pimoroni_pad_hid_init(hid_device_t *device, void *userdata) {
  dev_pimoroni_pad_hid_ctx_t *ctx = (dev_pimoroni_pad_hid_ctx_t *)userdata;
  (void)device;
  return ctx != NULL && ctx->device != NULL;
}

/**
 * @brief Read current Pimoroni pad input state during HID polling.
 */
static bool _dev_pimoroni_pad_hid_read(hid_device_t *device, void *userdata) {
  dev_pimoroni_pad_hid_ctx_t *ctx = (dev_pimoroni_pad_hid_ctx_t *)userdata;
  uint16_t value = 0u;
  (void)device;

  if (ctx == NULL || ctx->device == NULL) {
    return false;
  }

  if (_dev_tca9555_has_callback(ctx->device)) {
    if (sys_atomic_get(&ctx->pending_flag) == 0u) {
      return false;
    }

    value = ctx->pending_value;
    sys_atomic_set(&ctx->pending_flag, 0u);
    return _dev_pimoroni_pad_hid_event(ctx, value, true);
  }

  if (!dev_tca9555_read(ctx->device, &value)) {
    return false;
  }

  return _dev_pimoroni_pad_hid_event(ctx, value, true);
}

/**
 * @brief Restore default callback state on HID deregistration.
 */
static bool _dev_pimoroni_pad_hid_deinit(hid_device_t *device, void *userdata) {
  dev_pimoroni_pad_hid_ctx_t *ctx = (dev_pimoroni_pad_hid_ctx_t *)userdata;
  (void)device;
  if (ctx == NULL || ctx->device == NULL) {
    return false;
  }

  (void)_dev_tca9555_set_callback(ctx->device, NULL, NULL);
  dev_tca9555_deinit(ctx->device);
  sys_free(ctx);
  return true;
}

/**
 * @brief Emit a Pimoroni pad HID event from either poll or callback paths.
 */
static bool _dev_pimoroni_pad_hid_event(dev_pimoroni_pad_hid_ctx_t *ctx,
                                        uint16_t value, bool value_valid) {
  uint16_t pressed;
  uint16_t changed;
  bool any_queued = false;
  size_t i;

  if (!value_valid || ctx == NULL || ctx->hid_device == NULL) {
    return false;
  }

  // Pimoroni pad buttons are wired active-low on the TCA9555.
  pressed = (uint16_t)((~value) & SWITCH_MASK);

  if (!ctx->has_last_pressed) {
    changed = pressed;
    ctx->has_last_pressed = true;
  } else {
    changed = (uint16_t)(pressed ^ ctx->last_pressed);
  }

  if (changed == 0u) {
    ctx->last_pressed = pressed;
    return false;
  }

  for (i = 0u; i < (sizeof(_dev_pimoroni_pad_keymap) /
                    sizeof(_dev_pimoroni_pad_keymap[0]));
       ++i) {
    const dev_pimoroni_pad_keymap_t *entry = &_dev_pimoroni_pad_keymap[i];
    hid_state_t state;

    if ((changed & entry->pin) == 0u) {
      continue;
    }

    state = ((pressed & entry->pin) != 0u) ? hid_state_on : hid_state_off;
    any_queued =
        hid_event_queue_keycode(ctx->hid_device, state, entry->keycode) ||
        any_queued;
  }

  ctx->last_pressed = pressed;
  return any_queued;
}

/**
 * @brief Bridge Pimoroni pad change callbacks into HID polling.
 */
static void _dev_pimoroni_pad_hid_callback(dev_tca9555_t *tca9555,
                                           uint16_t value, void *userdata) {
  dev_pimoroni_pad_hid_ctx_t *ctx = (dev_pimoroni_pad_hid_ctx_t *)userdata;
  (void)tca9555;
  if (ctx == NULL) {
    return;
  }

  ctx->pending_value = value;
  sys_atomic_set(&ctx->pending_flag, 1u);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Initialize and register a TCA9555 handle as a HID child device.
 */
hid_device_t *dev_pimoroni_pad_register(hid_t *hid, hw_i2c_t *i2c,
                                        dev_tca9555_i2c_addr_t i2c_addr,
                                        uint16_t output_mask) {
  dev_pimoroni_pad_hid_ctx_t *ctx;
  dev_tca9555_t *device;
  hid_device_t *hid_device;

  if (hid == NULL || i2c == NULL) {
    return NULL;
  }

  device = dev_tca9555_init_i2c(i2c, i2c_addr, output_mask, NULL, NULL);
  if (device == NULL) {
    return NULL;
  }

  ctx = (dev_pimoroni_pad_hid_ctx_t *)sys_calloc(1u, sizeof(*ctx));
  if (ctx == NULL) {
    dev_tca9555_deinit(device);
    return NULL;
  }
  ctx->device = device;
  sys_atomic_init(&ctx->pending_flag, 0u);

  hid_device =
      hid_register(hid, "pimoroni_pad", (uint32_t)dev_tca9555_i2c_addr(device),
                   hid_type_other, hid_class_joystick, 0u, ctx,
                   _dev_pimoroni_pad_hid_callbacks);
  if (hid_device == NULL) {
    sys_free(ctx);
    dev_tca9555_deinit(device);
    return NULL;
  }

  ctx->hid_device = hid_device;

  if (!_dev_tca9555_set_callback(device, _dev_pimoroni_pad_hid_callback, ctx)) {
    (void)hid_deregister(hid, hid_device);
    return NULL;
  }

  return hid_device;
}

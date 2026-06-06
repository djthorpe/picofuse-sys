#include <picofuse/dev/ft6236.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define FT6236_DEFAULT_I2C_ADDR 0x48u

#define FT6236_REG_DATA_START 0x00u
#define FT6236_DATA_LENGTH 15u

#define FT6236_TOUCH_COUNT_MASK 0x0Fu
#define FT6236_TOUCH_EVENT_SHIFT 6u
#define FT6236_TOUCH_EVENT_MASK 0x03u
#define FT6236_TOUCH_POS_MASK 0x0Fu
#define FT6236_TOUCH_ID_SHIFT 4u
#define FT6236_TOUCH_ID_MASK 0x0Fu

#define FT6236_EVENT_DOWN 0u
#define FT6236_EVENT_UP 1u
#define FT6236_EVENT_CONTACT 2u
#define FT6236_EVENT_NONE 3u

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct dev_ft6236_t {
  hw_i2c_t *i2c;
  hw_gpio_t *int_pin;
  uint8_t i2c_address;
  bool irq_active_low;
  bool had_touch;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static void _dev_ft6236_clear_events(hid_event_t events[DEV_FT6236_MAX_POINTS],
                                     uint8_t *out_touch_count) {
  if (events == NULL) {
    return;
  }

  sys_memset(events, 0, sizeof(hid_event_t) * DEV_FT6236_MAX_POINTS);
  for (size_t i = 0; i < DEV_FT6236_MAX_POINTS; i++) {
    events[i].device = NULL;
    events[i].type = hid_event_type_touch;
    events[i].data.touch.state = hid_state_none;
    events[i].data.touch.point.x = 0;
    events[i].data.touch.point.y = 0;
    events[i].data.touch.slot = (uint8_t)i;
  }

  if (out_touch_count != NULL) {
    *out_touch_count = 0u;
  }
}

static bool _dev_ft6236_state_active(hid_state_t state) {
  return (state & hid_state_on) != 0;
}

static hid_state_t _dev_ft6236_event_to_state(uint8_t event_code) {
  switch (event_code) {
  case FT6236_EVENT_DOWN:
    return hid_state_on | hid_state_rising;
  case FT6236_EVENT_UP:
    return hid_state_off | hid_state_falling;
  case FT6236_EVENT_CONTACT:
    return hid_state_on | hid_state_repeat;
  case FT6236_EVENT_NONE:
  default:
    return hid_state_none;
  }
}

static bool _dev_ft6236_read_frame(dev_ft6236_t *ft6236, uint8_t *buffer,
                                   size_t length) {
  if (!dev_ft6236_valid(ft6236) || buffer == NULL ||
      length < FT6236_DATA_LENGTH) {
    return false;
  }

  return hw_i2c_read(ft6236->i2c, ft6236->i2c_address, FT6236_REG_DATA_START,
                     buffer, FT6236_DATA_LENGTH, 0u) == FT6236_DATA_LENGTH;
}

static void _dev_ft6236_parse_frame(const uint8_t *buffer,
                                    hid_event_t events[DEV_FT6236_MAX_POINTS],
                                    uint8_t *out_touch_count) {
  _dev_ft6236_clear_events(events, out_touch_count);
  if (buffer == NULL || events == NULL) {
    return;
  }

  uint8_t touches = buffer[2] & FT6236_TOUCH_COUNT_MASK;
  if (touches > DEV_FT6236_MAX_POINTS) {
    touches = DEV_FT6236_MAX_POINTS;
  }

  for (uint8_t index = 0u; index < touches; index++) {
    const uint8_t *point = &buffer[3u + ((size_t)index * 6u)];
    uint8_t touch_id =
        (point[2] >> FT6236_TOUCH_ID_SHIFT) & FT6236_TOUCH_ID_MASK;
    uint8_t slot = touch_id < DEV_FT6236_MAX_POINTS ? touch_id : index;

    hid_state_t state = _dev_ft6236_event_to_state(
        (point[0] >> FT6236_TOUCH_EVENT_SHIFT) & FT6236_TOUCH_EVENT_MASK);

    events[slot].type = hid_event_type_touch;
    events[slot].device = NULL;
    events[slot].data.touch.point.x =
        (int16_t)(((uint16_t)(point[0] & FT6236_TOUCH_POS_MASK) << 8) |
                  point[1]);
    events[slot].data.touch.point.y =
        (int16_t)(((uint16_t)(point[2] & FT6236_TOUCH_POS_MASK) << 8) |
                  point[3]);
    events[slot].data.touch.state = state;
    events[slot].data.touch.slot = slot;
    if (_dev_ft6236_state_active(state) && out_touch_count != NULL) {
      (*out_touch_count)++;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

void dev_ft6236_default_config(dev_ft6236_config_t *config) {
  if (config == NULL) {
    return;
  }

  config->i2c_address = FT6236_DEFAULT_I2C_ADDR;
  config->irq_active_low = true;
}

dev_ft6236_t *dev_ft6236_init(hw_i2c_t *i2c, hw_gpio_t *int_pin,
                              const dev_ft6236_config_t *config) {
  if (!hw_i2c_valid(i2c)) {
    return NULL;
  }

  dev_ft6236_config_t resolved = {0};
  dev_ft6236_default_config(&resolved);
  if (config != NULL) {
    resolved = *config;
    if (resolved.i2c_address == 0u) {
      resolved.i2c_address = FT6236_DEFAULT_I2C_ADDR;
    }
  }

  dev_ft6236_t *ft6236 = sys_calloc(1u, sizeof(*ft6236));
  if (ft6236 == NULL) {
    return NULL;
  }

  ft6236->i2c = i2c;
  ft6236->int_pin = int_pin;
  ft6236->i2c_address = resolved.i2c_address;
  ft6236->irq_active_low = resolved.irq_active_low;
  ft6236->init = true;

  if (hw_gpio_valid(ft6236->int_pin)) {
    hw_gpio_set_mode(ft6236->int_pin, HW_GPIO_PULLUP);
  }

  uint8_t frame[FT6236_DATA_LENGTH] = {0};
  if (!_dev_ft6236_read_frame(ft6236, frame, sizeof(frame))) {
    dev_ft6236_deinit(ft6236);
    return NULL;
  }

  return ft6236;
}

void dev_ft6236_deinit(dev_ft6236_t *ft6236) {
  if (!dev_ft6236_valid(ft6236)) {
    return;
  }

  sys_memset(ft6236, 0, sizeof(*ft6236));
  sys_free(ft6236);
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

bool dev_ft6236_valid(const dev_ft6236_t *ft6236) {
  return ft6236 != NULL && ft6236->init && hw_i2c_valid(ft6236->i2c) &&
         ft6236->i2c_address != 0u;
}

bool dev_ft6236_irq_active(const dev_ft6236_t *ft6236) {
  if (!dev_ft6236_valid(ft6236) || !hw_gpio_valid(ft6236->int_pin)) {
    return false;
  }

  bool level = hw_gpio_get(ft6236->int_pin);
  return ft6236->irq_active_low ? !level : level;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool dev_ft6236_poll(dev_ft6236_t *ft6236,
                     hid_event_t events[DEV_FT6236_MAX_POINTS],
                     uint8_t *out_touch_count) {
  if (!dev_ft6236_valid(ft6236) || events == NULL) {
    return false;
  }

  bool irq_active = dev_ft6236_irq_active(ft6236);
  if (hw_gpio_valid(ft6236->int_pin) && !irq_active && !ft6236->had_touch) {
    _dev_ft6236_clear_events(events, out_touch_count);
    return true;
  }

  uint8_t frame[FT6236_DATA_LENGTH] = {0};
  if (!_dev_ft6236_read_frame(ft6236, frame, sizeof(frame))) {
    return false;
  }

  _dev_ft6236_parse_frame(frame, events, out_touch_count);
  ft6236->had_touch =
      (out_touch_count != NULL)
          ? (*out_touch_count > 0u)
          : _dev_ft6236_state_active(events[0].data.touch.state) ||
                _dev_ft6236_state_active(events[1].data.touch.state);
  return true;
}
/**
 * @file
 * @brief Presto FT6236 touch example.
 */

#include <boards/presto.h>
#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define TOUCH_I2C_BAUD_RATE 100000u
#define TOUCH_IDLE_SLEEP_MS 2u

static volatile bool touch_irq_pending;

static void touch_gpio_event_cb(uint8_t bank, uint8_t pin,
                                hw_gpio_event_t event, void *userdata) {
  (void)userdata;

  if (bank != 0u || pin != PIMORONI_PRESTO_TOUCH_INT_PIN) {
    return;
  }

  if ((event & HW_GPIO_RISING) != 0u) {
    touch_irq_pending = true;
  }
  if ((event & HW_GPIO_FALLING) != 0u) {
    touch_irq_pending = true;
  }
}

static bool touch_events_equal(const hid_event_t lhs[DEV_FT6236_MAX_POINTS],
                               uint8_t lhs_touch_count,
                               const hid_event_t rhs[DEV_FT6236_MAX_POINTS],
                               uint8_t rhs_touch_count) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }

  if (lhs_touch_count != rhs_touch_count) {
    return false;
  }

  for (size_t index = 0u; index < DEV_FT6236_MAX_POINTS; index++) {
    const hid_event_t *lp = &lhs[index];
    const hid_event_t *rp = &rhs[index];

    if (lp->type != rp->type || lp->data.touch.state != rp->data.touch.state ||
        lp->data.touch.slot != rp->data.touch.slot ||
        lp->data.touch.point.x != rp->data.touch.point.x ||
        lp->data.touch.point.y != rp->data.touch.point.y) {
      return false;
    }
  }

  return true;
}

static const char *touch_state_name(hid_state_t state) {
  if ((state & hid_state_repeat) != 0u) {
    return "repeat";
  }
  if ((state & hid_state_on) != 0u) {
    return "on";
  }
  if ((state & hid_state_off) != 0u) {
    return "off";
  }
  return "none";
}

static void print_touch_data(const hid_event_t current[DEV_FT6236_MAX_POINTS],
                             uint8_t touch_count, bool irq_active) {
  if (current == NULL) {
    return;
  }

  sys_printf("touches=%u irq=%u\n", (unsigned int)touch_count,
             (unsigned int)(irq_active ? 1u : 0u));

  for (size_t index = 0u; index < DEV_FT6236_MAX_POINTS; index++) {
    const hid_event_t *event = &current[index];
    if (event->type != hid_event_type_touch ||
        event->data.touch.state == hid_state_none) {
      continue;
    }

    bool active = (event->data.touch.state & hid_state_on) != 0u;
    const char *state_name = touch_state_name(event->data.touch.state);

    sys_printf(
        "  state=%s point=%d,%d slot=%u active=%u raw=0x%04X\n", state_name,
        (int)event->data.touch.point.x, (int)event->data.touch.point.y,
        (unsigned int)event->data.touch.slot, (unsigned int)(active ? 1u : 0u),
        (unsigned int)event->data.touch.state);
  }
}

int main(void) {
  sys_init();
  hw_init();

  hw_gpio_t *touch_sda =
      hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_SDA_PIN, HW_GPIO_I2C);
  hw_gpio_t *touch_scl =
      hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_SCL_PIN, HW_GPIO_I2C);
  hw_gpio_t *touch_int =
      hw_gpio_init(0u, PIMORONI_PRESTO_TOUCH_INT_PIN, HW_GPIO_PULLUP);

  if (!hw_gpio_valid(touch_sda) || !hw_gpio_valid(touch_scl) ||
      !hw_gpio_valid(touch_int)) {
    sys_printf("Presto touch GPIO init failed\n");
    hw_gpio_deinit(touch_int);
    hw_gpio_deinit(touch_scl);
    hw_gpio_deinit(touch_sda);
    hw_exit();
    sys_exit();
    return 1;
  }

  hw_i2c_t *i2c = hw_i2c_init(PIMORONI_PRESTO_TOUCH_I2C, touch_sda, touch_scl,
                              TOUCH_I2C_BAUD_RATE);
  if (!hw_i2c_valid(i2c)) {
    sys_printf("Presto touch I2C init failed\n");
    hw_gpio_deinit(touch_int);
    hw_gpio_deinit(touch_scl);
    hw_gpio_deinit(touch_sda);
    hw_exit();
    sys_exit();
    return 1;
  }

  dev_ft6236_config_t config = {0};
  dev_ft6236_default_config(&config);
  config.i2c_address = PIMORONI_PRESTO_TOUCH_I2C_ADDR;

  dev_ft6236_t *touch = dev_ft6236_init(i2c, touch_int, &config);
  if (!dev_ft6236_valid(touch)) {
    sys_printf("FT6236 init failed at 0x%02X\n",
               (unsigned int)PIMORONI_PRESTO_TOUCH_I2C_ADDR);
    hw_i2c_deinit(i2c);
    hw_gpio_deinit(touch_int);
    hw_gpio_deinit(touch_scl);
    hw_gpio_deinit(touch_sda);
    hw_exit();
    sys_exit();
    return 1;
  }

  sys_printf("Presto touch ready on i2c%u addr=0x%02X irq_pin=%u\n",
             (unsigned int)PIMORONI_PRESTO_TOUCH_I2C,
             (unsigned int)config.i2c_address,
             (unsigned int)PIMORONI_PRESTO_TOUCH_INT_PIN);
  hw_gpio_set_callback(touch_gpio_event_cb, NULL);
  sys_printf("GPIO callback armed for irq pin %u\n",
             (unsigned int)PIMORONI_PRESTO_TOUCH_INT_PIN);
  sys_printf("Touch the panel to print coordinates and events\n");

  hid_event_t previous[DEV_FT6236_MAX_POINTS] = {0};
  hid_event_t current[DEV_FT6236_MAX_POINTS] = {0};
  uint8_t previous_touch_count = 0u;
  uint8_t current_touch_count = 0u;

  // Perform one initial read to establish baseline state.
  touch_irq_pending = true;

  while (true) {
    if (!touch_irq_pending) {
      sys_sleep_ms(TOUCH_IDLE_SLEEP_MS);
      continue;
    }
    touch_irq_pending = false;

    bool irq_active = dev_ft6236_irq_active(touch);

    if (!dev_ft6236_poll(touch, current, &current_touch_count)) {
      sys_printf("FT6236 poll failed\n");
      sys_sleep_ms(250u);
      continue;
    }

    bool changed = !touch_events_equal(previous, previous_touch_count, current,
                                       current_touch_count);
    if (changed || current_touch_count > 0u) {
      print_touch_data(current, current_touch_count, irq_active);
      sys_memcpy(previous, current, sizeof(previous));
      previous_touch_count = current_touch_count;
    }
  }

  dev_ft6236_deinit(touch);
  hw_i2c_deinit(i2c);
  hw_gpio_deinit(touch_int);
  hw_gpio_deinit(touch_scl);
  hw_gpio_deinit(touch_sda);
  hw_exit();
  sys_exit();
  return 0;
}
/**
 * @file
 * @brief TCA9555 input polling example.
 */

#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define I2C_BAUD_RATE 100000u
#define IDLE_PERIOD_MS 1000u

#define PIN_LED_R (1u << 6)
#define PIN_LED_Y (1u << 7)
#define PIN_LED_G (1u << 9)
#define PIN_LED_B (1u << 10)

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

#define OUTPUT_MASK (PIN_LED_R | PIN_LED_Y | PIN_LED_G | PIN_LED_B)
#define SWITCH_MASK                                                            \
  (PIN_SW_UP | PIN_SW_LEFT | PIN_SW_RIGHT | PIN_SW_DOWN | PIN_SW_SELECT |      \
   PIN_SW_START | PIN_SW_A | PIN_SW_X | PIN_SW_B | PIN_SW_Y)

typedef struct {
  uint16_t pin;
  const char *name;
} button_map_t;

static const button_map_t _buttons[] = {
    {PIN_SW_UP, "UP"},     {PIN_SW_LEFT, "LEFT"},     {PIN_SW_RIGHT, "RIGHT"},
    {PIN_SW_DOWN, "DOWN"}, {PIN_SW_SELECT, "SELECT"}, {PIN_SW_START, "START"},
    {PIN_SW_A, "B"},       {PIN_SW_X, "Y"},           {PIN_SW_B, "A"},
    {PIN_SW_Y, "X"},
};

static const uint16_t _led_pattern[] = {
    PIN_LED_R,
    PIN_LED_Y,
    PIN_LED_G,
    PIN_LED_B,
};

typedef struct {
  size_t led_index;
  bool exit_requested;
} app_state_t;

static void _print_pressed_buttons(uint16_t pressed_mask) {
  if (pressed_mask == 0u) {
    sys_printf("buttons: none\n");
    return;
  }

  sys_printf("buttons:");
  for (size_t i = 0u; i < (sizeof(_buttons) / sizeof(_buttons[0])); ++i) {
    if ((pressed_mask & _buttons[i].pin) != 0u) {
      sys_printf(" %s", _buttons[i].name);
    }
  }
  sys_printf("\n");
}

static void _on_tca9555_changed(dev_tca9555_t *tca9555, uint16_t value,
                                void *userdata) {
  app_state_t *state = (app_state_t *)userdata;
  if (state == NULL) {
    return;
  }

  // Buttons are wired active-low on this board.
  uint16_t pressed = (uint16_t)((~value) & SWITCH_MASK);
  _print_pressed_buttons(pressed);

  if ((pressed & PIN_SW_START) != 0u) {
    state->exit_requested = true;
    sys_printf("START pressed, exiting...\n");
    return;
  }

  uint16_t led = _led_pattern[state->led_index];
  if (!dev_tca9555_write(tca9555, led)) {
    sys_printf("TCA9555 write failed\n");
    return;
  }

  state->led_index = (state->led_index + 1u) %
                     (sizeof(_led_pattern) / sizeof(_led_pattern[0]));
}

int main(void) {
  sys_init();
  hw_init();

  app_state_t state = {0};

  hw_i2c_t *i2c = hw_i2c_init_default(I2C_BAUD_RATE);
  if (!hw_i2c_valid(i2c)) {
    sys_printf("I2C init failed (default bus unavailable)\n");
    hw_exit();
    sys_exit();
    return 1;
  }

  dev_tca9555_t *tca9555 = dev_tca9555_init_i2c(
      i2c, DEV_TCA9555_I2C_ADDR_ANY, OUTPUT_MASK, _on_tca9555_changed, &state);
  if (tca9555 == NULL) {
    sys_printf("TCA9555 init failed\n");
    hw_i2c_deinit(i2c);
    hw_exit();
    sys_exit();
    return 1;
  }

  sys_printf("TCA9555 ready (addr=0x%02X, output_mask=0x%04X)\n",
             (unsigned int)dev_tca9555_i2c_addr(tca9555),
             (unsigned int)OUTPUT_MASK);

  sys_printf("waiting for button changes...\n");

  while (!state.exit_requested) {
    sys_sleep_ms(IDLE_PERIOD_MS);
  }

  (void)dev_tca9555_write(tca9555, 0u);
  dev_tca9555_deinit(tca9555);
  hw_i2c_deinit(i2c);
  hw_exit();
  sys_exit();
  return 0;
}

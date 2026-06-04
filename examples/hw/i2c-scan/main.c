/**
 * @file
 * @brief I2C scan example.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define I2C_BAUD_RATE 100000u

static void print_scan_table(hw_i2c_t *i2c) {
  sys_printf("\nI2C device scan\n");
  sys_printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

  for (uint8_t row = 0; row < 8; ++row) {
    uint8_t base = (uint8_t)(row << 4);
    sys_printf("%02X: ", (unsigned int)base);

    for (uint8_t col = 0; col < 16; ++col) {
      uint8_t addr = (uint8_t)(base + col);

      // Skip reserved addresses 0x00-0x07 and 0x78-0x7F.
      if (addr < 0x08u || addr > 0x77u) {
        sys_printf("   ");
        continue;
      }

      if (hw_i2c_detect(i2c, addr)) {
        sys_printf("%02X ", (unsigned int)addr);
      } else {
        sys_printf("-- ");
      }
    }

    sys_printf("\n");
  }
}

int main(void) {
  sys_init();
  hw_init();
  hw_i2c_t *i2c = hw_i2c_init_default(I2C_BAUD_RATE);
  if (!hw_i2c_valid(i2c)) {
    sys_printf("I2C init failed (default bus unavailable)\n");
    hw_exit();
    sys_exit();
    return 1;
  }
  print_scan_table(i2c);
  hw_i2c_deinit(i2c);
  hw_exit();
  sys_exit();
  return 0;
}

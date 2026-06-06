/**
 * @file
 * @brief BME680 I2C read example.
 */

#include <picofuse/dev.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

#define I2C_BAUD_RATE 100000u
#define SAMPLE_PERIOD_MS 1000u

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

  dev_bme680_config_t config = {0};
  dev_bme680_default_config(&config);

  // Example tuning: slightly longer heater for a steadier gas channel.
  config.heater_temp_c = DEV_BME680_HEATER_TEMP_320C;
  config.heater_duration_ms = DEV_BME680_HEATER_DUR_120MS;

  dev_bme680_t *bme680 = dev_bme680_init_i2c(i2c, &config);
  if (bme680 == NULL) {
    sys_printf("BME680 init failed\n");
    hw_i2c_deinit(i2c);
    hw_exit();
    sys_exit();
    return 1;
  }

  sys_printf("BME680 ready (chip id=0x%02X)\n",
             (unsigned int)dev_bme680_chip_id(bme680));
  sys_printf(
      "config: os_t=%u os_p=%u os_h=%u filter=%u heater=%uC/%ums gas=%s\n",
      (unsigned int)config.os_temp, (unsigned int)config.os_press,
      (unsigned int)config.os_hum, (unsigned int)config.iir_filter,
      (unsigned int)config.heater_temp_c,
      (unsigned int)config.heater_duration_ms,
      config.gas_mode == DEV_BME680_GAS_ENABLED ? "on" : "off");

  while (true) {
    dev_bme680_data_t data = {0};
    if (dev_bme680_read_data(bme680, &data)) {
      sys_printf("T=%.2f C  P=%.2f Pa  H=%.2f %%  Gas=%.2f ohm\n",
                 data.temperature_c, data.pressure_pa, data.humidity_pct,
                 data.gas_resistance_ohms);
    } else {
      sys_printf("BME680 read failed\n");
    }

    sys_sleep_ms(SAMPLE_PERIOD_MS);
  }

  // Unreachable in this example's continuous loop.
  dev_bme680_deinit(bme680);
  hw_i2c_deinit(i2c);
  hw_exit();
  sys_exit();
  return 0;
}

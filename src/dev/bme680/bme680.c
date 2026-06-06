#include <picofuse/dev.h>
#include <picofuse/sys.h>

#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define BME680_I2C_ADDR_PRIMARY 0x76u
#define BME680_I2C_ADDR_SECONDARY 0x77u

#define BME680_REG_FIELD0 0x1Du
#define BME680_REG_RES_HEAT0 0x5Au
#define BME680_REG_GAS_WAIT0 0x64u
#define BME680_REG_CTRL_GAS_0 0x70u
#define BME680_REG_CTRL_GAS_1 0x71u
#define BME680_REG_CTRL_HUM 0x72u
#define BME680_REG_CTRL_MEAS 0x74u
#define BME680_REG_CONFIG 0x75u

#define BME680_REG_COEFF3 0x00u
#define BME680_REG_COEFF1 0x8Au
#define BME680_REG_CHIP_ID 0xD0u
#define BME680_REG_SOFT_RESET 0xE0u
#define BME680_REG_COEFF2 0xE1u
#define BME680_REG_VARIANT_ID 0xF0u

#define BME680_CHIP_ID 0x61u

#define BME680_VARIANT_GAS_LOW 0x00u
#define BME680_VARIANT_GAS_HIGH 0x01u

#define BME680_SOFT_RESET_CMD 0xB6u

#define BME680_LEN_COEFF1 23u
#define BME680_LEN_COEFF2 14u
#define BME680_LEN_COEFF3 5u
#define BME680_LEN_COEFF_ALL 42u
#define BME680_LEN_FIELD 17u

#define BME680_FIELD_NEW_DATA_MSK 0x80u
#define BME680_FIELD_GAS_INDEX_MSK 0x0Fu
#define BME680_FIELD_GAS_RANGE_MSK 0x0Fu
#define BME680_FIELD_GASM_VALID_MSK 0x20u
#define BME680_FIELD_HEAT_STAB_MSK 0x10u

#define BME680_BIT_H1_DATA_MSK 0x0Fu

#define BME680_RHRANGE_MSK 0x30u
#define BME680_RSERROR_MSK 0xF0u

#define BME680_HCTRL_MSK 0x08u
#define BME680_HCTRL_POS 3u

#define BME680_NBCONV_MSK 0x0Fu

#define BME680_RUN_GAS_MSK 0x30u
#define BME680_RUN_GAS_POS 4u

#define BME680_OSH_MSK 0x07u

#define BME680_OST_MSK 0xE0u
#define BME680_OST_POS 5u

#define BME680_OSP_MSK 0x1Cu
#define BME680_OSP_POS 2u

#define BME680_FILTER_MSK 0x1Cu
#define BME680_FILTER_POS 2u

#define BME680_MODE_MSK 0x03u

#define BME680_FORCED_MODE 0x01u

#define BME680_ENABLE_HEATER 0x00u
#define BME680_ENABLE_GAS_MEAS_LOW 0x01u
#define BME680_ENABLE_GAS_MEAS_HIGH 0x02u

#define BME680_DEFAULT_OS 0x01u
#define BME680_DEFAULT_FILTER 0x00u
#define BME680_DEFAULT_HEATER_TEMP_C 300u
#define BME680_DEFAULT_HEATER_DUR_MS 100u

#define BME680_AMBIENT_TEMP_C 25

#define BME680_MEAS_RETRY_COUNT 60u
#define BME680_MEAS_POLL_MS 5u

#define BME680_IO_RETRY_COUNT 3u
#define BME680_IO_RETRY_DELAY_MS 2u

#define BME680_OS_MIN 0u
#define BME680_OS_MAX 5u
#define BME680_FILTER_MIN 0u
#define BME680_FILTER_MAX 7u

#define BME680_SPI_READ_MASK 0x80u
#define BME680_SPI_WRITE_MASK 0x7Fu

#define BME680_CONCAT_BYTES(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))

#define BME680_SET_BITS(reg_data, msk, pos, data)                              \
  (((reg_data) & ~(msk)) | ((((data) << (pos)) & (msk))))

#define BME680_SET_BITS_POS_0(reg_data, msk, data)                             \
  (((reg_data) & ~(msk)) | ((data) & (msk)))

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef enum {
  DEV_BME680_BUS_I2C = 1,
  DEV_BME680_BUS_SPI = 2,
} _dev_bme680_bus_t;

typedef struct {
  uint16_t par_h1;
  uint16_t par_h2;
  int8_t par_h3;
  int8_t par_h4;
  int8_t par_h5;
  uint8_t par_h6;
  int8_t par_h7;

  int8_t par_gh1;
  int16_t par_gh2;
  int8_t par_gh3;

  uint16_t par_t1;
  int16_t par_t2;
  int8_t par_t3;

  uint16_t par_p1;
  int16_t par_p2;
  int8_t par_p3;
  int16_t par_p4;
  int16_t par_p5;
  int8_t par_p6;
  int8_t par_p7;
  int16_t par_p8;
  int16_t par_p9;
  uint8_t par_p10;

  int32_t t_fine;

  uint8_t res_heat_range;
  int8_t res_heat_val;
  int8_t range_sw_err;
} _dev_bme680_calib_t;

struct dev_bme680_t {
  _dev_bme680_bus_t bus;
  hw_i2c_t *i2c;
  hw_spi_t *spi;
  hw_gpio_t *cs_pin;
  uint8_t i2c_addr;
  uint8_t chip_id;
  uint8_t variant_id;
  int8_t ambient_temp_c;
  dev_bme680_config_t config;
  _dev_bme680_calib_t calib;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static bool _dev_bme680_is_spi(const dev_bme680_t *bme680) {
  return bme680 != NULL && bme680->bus == DEV_BME680_BUS_SPI;
}

static bool _dev_bme680_bus_ready(const dev_bme680_t *bme680) {
  if (bme680 == NULL || !bme680->init) {
    return false;
  }

  if (bme680->bus == DEV_BME680_BUS_I2C) {
    return hw_i2c_valid(bme680->i2c) &&
           (bme680->i2c_addr == BME680_I2C_ADDR_PRIMARY ||
            bme680->i2c_addr == BME680_I2C_ADDR_SECONDARY);
  }

  if (bme680->bus == DEV_BME680_BUS_SPI) {
    return hw_spi_valid(bme680->spi);
  }

  return false;
}

static uint8_t _dev_bme680_calc_gas_wait(uint16_t dur_ms) {
  uint8_t factor = 0;

  if (dur_ms >= 0xFC0u) {
    return 0xFFu;
  }

  while (dur_ms > 0x3Fu) {
    dur_ms /= 4u;
    factor += 1u;
  }

  return (uint8_t)(dur_ms + ((uint16_t)factor * 64u));
}

static uint8_t _dev_bme680_clamp_os(uint8_t os) {
  if (os > BME680_OS_MAX) {
    return 1u;
  }

  return os;
}

static uint8_t _dev_bme680_clamp_filter(uint8_t filter) {
  if (filter > BME680_FILTER_MAX) {
    return 0u;
  }

  return filter;
}

static uint8_t _dev_bme680_calc_res_heat(uint16_t temp_c,
                                         const dev_bme680_t *bme680) {
  if (temp_c > 400u) {
    temp_c = 400u;
  }

  int32_t var1 =
      ((((int32_t)bme680->ambient_temp_c * (int32_t)bme680->calib.par_gh3) /
        1000) *
       256);
  int32_t var2 =
      ((int32_t)bme680->calib.par_gh1 + 784) *
      ((((((int32_t)bme680->calib.par_gh2 + 154009) * (int32_t)temp_c * 5) /
         100) +
        3276800) /
       10);
  int32_t var3 = var1 + (var2 / 2);
  int32_t var4 = var3 / ((int32_t)bme680->calib.res_heat_range + 4);
  int32_t var5 = (131 * (int32_t)bme680->calib.res_heat_val) + 65536;
  int32_t heatr_res_x100 = (((var4 / var5) - 250) * 34);
  return (uint8_t)((heatr_res_x100 + 50) / 100);
}

static int16_t _dev_bme680_compensate_temperature(uint32_t adc_temp,
                                                  dev_bme680_t *bme680) {
  int64_t var1 =
      ((int32_t)adc_temp >> 3) - ((int32_t)bme680->calib.par_t1 << 1);
  int64_t var2 = (var1 * (int32_t)bme680->calib.par_t2) >> 11;
  int64_t var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
  var3 = (var3 * ((int32_t)bme680->calib.par_t3 << 4)) >> 14;

  bme680->calib.t_fine = (int32_t)(var2 + var3);
  return (int16_t)(((bme680->calib.t_fine * 5) + 128) >> 8);
}

static uint32_t _dev_bme680_compensate_pressure(uint32_t adc_pres,
                                                const dev_bme680_t *bme680) {
  const int32_t pres_ovf_check = INT32_C(0x40000000);

  int32_t var1 = ((int32_t)bme680->calib.t_fine >> 1) - 64000;
  int32_t var2 =
      ((((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)bme680->calib.par_p6) >>
      2;
  var2 = var2 + ((var1 * (int32_t)bme680->calib.par_p5) << 1);
  var2 = (var2 >> 2) + ((int32_t)bme680->calib.par_p4 << 16);

  var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) *
           ((int32_t)bme680->calib.par_p3 << 5)) >>
          3) +
         (((int32_t)bme680->calib.par_p2 * var1) >> 1);
  var1 = var1 >> 18;
  var1 = ((32768 + var1) * (int32_t)bme680->calib.par_p1) >> 15;
  if (var1 == 0) {
    return 0;
  }

  int32_t pressure_comp = 1048576 - (int32_t)adc_pres;
  pressure_comp = (int32_t)((pressure_comp - (var2 >> 12)) * (uint32_t)3125);
  if (pressure_comp >= pres_ovf_check) {
    pressure_comp = ((pressure_comp / var1) << 1);
  } else {
    pressure_comp = ((pressure_comp << 1) / var1);
  }

  var1 = ((int32_t)bme680->calib.par_p9 *
          (int32_t)(((pressure_comp >> 3) * (pressure_comp >> 3)) >> 13)) >>
         12;
  var2 = ((int32_t)(pressure_comp >> 2) * (int32_t)bme680->calib.par_p8) >> 13;
  int32_t var3 =
      ((int32_t)(pressure_comp >> 8) * (int32_t)(pressure_comp >> 8) *
       (int32_t)(pressure_comp >> 8) * (int32_t)bme680->calib.par_p10) >>
      17;
  pressure_comp =
      pressure_comp +
      ((var1 + var2 + var3 + ((int32_t)bme680->calib.par_p7 << 7)) >> 4);

  return (uint32_t)pressure_comp;
}

static uint32_t _dev_bme680_compensate_humidity(uint16_t adc_hum,
                                                const dev_bme680_t *bme680) {
  int32_t temp_scaled = (((int32_t)bme680->calib.t_fine * 5) + 128) >> 8;
  int32_t var1 = (int32_t)adc_hum - ((int32_t)bme680->calib.par_h1 * 16) -
                 (((temp_scaled * (int32_t)bme680->calib.par_h3) / 100) >> 1);
  int32_t var2 = ((int32_t)bme680->calib.par_h2 *
                  (((temp_scaled * (int32_t)bme680->calib.par_h4) / 100) +
                   ((((temp_scaled *
                       ((temp_scaled * (int32_t)bme680->calib.par_h5) / 100)) >>
                      6) /
                     100)) +
                   (int32_t)(1 << 14))) >>
                 10;
  int32_t var3 = var1 * var2;
  int32_t var4 = ((int32_t)bme680->calib.par_h6 << 7);
  var4 = (var4 + ((temp_scaled * (int32_t)bme680->calib.par_h7) / 100)) >> 4;
  int32_t var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
  int32_t var6 = (var4 * var5) >> 1;

  int32_t hum = (((var3 + var6) >> 10) * 1000) >> 12;
  if (hum > 100000) {
    hum = 100000;
  } else if (hum < 0) {
    hum = 0;
  }

  return (uint32_t)hum;
}

static uint32_t
_dev_bme680_calc_gas_resistance_low(uint16_t gas_res_adc, uint8_t gas_range,
                                    const dev_bme680_t *bme680) {
  static const uint32_t lookup_table1[16] = {
      2147483647u, 2147483647u, 2147483647u, 2147483647u,
      2147483647u, 2126008810u, 2147483647u, 2130303777u,
      2147483647u, 2147483647u, 2143188679u, 2136746228u,
      2147483647u, 2126008810u, 2147483647u, 2147483647u,
  };
  static const uint32_t lookup_table2[16] = {
      4096000000u, 2048000000u, 1024000000u, 512000000u, 255744255u, 127110228u,
      64000000u,   32258064u,   16016016u,   8000000u,   4000000u,   2000000u,
      1000000u,    500000u,     250000u,     125000u,
  };

  if (gas_range > 15u) {
    return 0u;
  }

  int64_t var1 = (int64_t)((1340 + (5 * (int64_t)bme680->calib.range_sw_err)) *
                           ((int64_t)lookup_table1[gas_range])) >>
                 16;
  uint64_t var2 =
      (((int64_t)((int64_t)gas_res_adc << 15) - (int64_t)16777216) + var1);
  int64_t var3 = (((int64_t)lookup_table2[gas_range] * (int64_t)var1) >> 9);
  return (uint32_t)((var3 + ((int64_t)var2 >> 1)) / (int64_t)var2);
}

static uint32_t _dev_bme680_calc_gas_resistance_high(uint16_t gas_res_adc,
                                                     uint8_t gas_range) {
  if (gas_range > 31u) {
    return 0u;
  }

  uint32_t var1 = UINT32_C(262144) >> gas_range;
  int32_t var2 = (int32_t)gas_res_adc - INT32_C(512);

  var2 *= 3;
  var2 = 4096 + var2;
  if (var2 <= 0) {
    return 0u;
  }

  uint32_t gas = (10000u * var1) / (uint32_t)var2;
  return gas * 100u;
}

static void _dev_bme680_spi_cs_set(dev_bme680_t *bme680, bool active) {
  if (bme680 == NULL || !hw_gpio_valid(bme680->cs_pin)) {
    return;
  }

  // External CS pin is active low.
  hw_gpio_set(bme680->cs_pin, active ? false : true);
}

static bool _dev_bme680_write_register(dev_bme680_t *bme680, uint8_t reg,
                                       uint8_t value) {
  if (!_dev_bme680_bus_ready(bme680)) {
    return false;
  }

  if (_dev_bme680_is_spi(bme680)) {
    _dev_bme680_spi_cs_set(bme680, true);
    size_t written =
        hw_spi_write(bme680->spi, reg & BME680_SPI_WRITE_MASK, &value, 1u, 0u);
    _dev_bme680_spi_cs_set(bme680, false);
    return written == 1u;
  }

  return hw_i2c_write(bme680->i2c, bme680->i2c_addr, reg, &value, 1u, 0u) == 1u;
}

static bool _dev_bme680_read_registers(dev_bme680_t *bme680, uint8_t reg,
                                       uint8_t *data, size_t len) {
  if (!_dev_bme680_bus_ready(bme680) || data == NULL || len == 0u) {
    return false;
  }

  if (_dev_bme680_is_spi(bme680)) {
    _dev_bme680_spi_cs_set(bme680, true);
    size_t read =
        hw_spi_read(bme680->spi, reg | BME680_SPI_READ_MASK, data, len, 0u);
    _dev_bme680_spi_cs_set(bme680, false);
    return read == len;
  }

  return hw_i2c_read(bme680->i2c, bme680->i2c_addr, reg, data, len, 0u) == len;
}

static bool _dev_bme680_read_register(dev_bme680_t *bme680, uint8_t reg,
                                      uint8_t *value) {
  return _dev_bme680_read_registers(bme680, reg, value, 1u);
}

static bool _dev_bme680_write_register_retry(dev_bme680_t *bme680, uint8_t reg,
                                             uint8_t value) {
  for (uint8_t attempt = 0; attempt < BME680_IO_RETRY_COUNT; ++attempt) {
    if (_dev_bme680_write_register(bme680, reg, value)) {
      return true;
    }

    sys_sleep_ms(BME680_IO_RETRY_DELAY_MS);
  }

  return false;
}

static bool _dev_bme680_read_register_retry(dev_bme680_t *bme680, uint8_t reg,
                                            uint8_t *value) {
  for (uint8_t attempt = 0; attempt < BME680_IO_RETRY_COUNT; ++attempt) {
    if (_dev_bme680_read_register(bme680, reg, value)) {
      return true;
    }

    sys_sleep_ms(BME680_IO_RETRY_DELAY_MS);
  }

  return false;
}

static bool _dev_bme680_read_calibration(dev_bme680_t *bme680) {
  uint8_t coeff[BME680_LEN_COEFF_ALL] = {0};

  if (!_dev_bme680_read_registers(bme680, BME680_REG_COEFF1, coeff,
                                  BME680_LEN_COEFF1)) {
    sys_debugf("bme680: coeff1 read failed");
    return false;
  }

  if (!_dev_bme680_read_registers(bme680, BME680_REG_COEFF2,
                                  &coeff[BME680_LEN_COEFF1],
                                  BME680_LEN_COEFF2)) {
    sys_debugf("bme680: coeff2 read failed");
    return false;
  }

  if (!_dev_bme680_read_registers(bme680, BME680_REG_COEFF3,
                                  &coeff[BME680_LEN_COEFF1 + BME680_LEN_COEFF2],
                                  BME680_LEN_COEFF3)) {
    sys_debugf("bme680: coeff3 read failed");
    return false;
  }

  bme680->calib.par_t1 = BME680_CONCAT_BYTES(coeff[32], coeff[31]);
  bme680->calib.par_t2 = (int16_t)BME680_CONCAT_BYTES(coeff[1], coeff[0]);
  bme680->calib.par_t3 = (int8_t)coeff[2];

  bme680->calib.par_p1 = BME680_CONCAT_BYTES(coeff[5], coeff[4]);
  bme680->calib.par_p2 = (int16_t)BME680_CONCAT_BYTES(coeff[7], coeff[6]);
  bme680->calib.par_p3 = (int8_t)coeff[8];
  bme680->calib.par_p4 = (int16_t)BME680_CONCAT_BYTES(coeff[11], coeff[10]);
  bme680->calib.par_p5 = (int16_t)BME680_CONCAT_BYTES(coeff[13], coeff[12]);
  bme680->calib.par_p6 = (int8_t)coeff[15];
  bme680->calib.par_p7 = (int8_t)coeff[14];
  bme680->calib.par_p8 = (int16_t)BME680_CONCAT_BYTES(coeff[19], coeff[18]);
  bme680->calib.par_p9 = (int16_t)BME680_CONCAT_BYTES(coeff[21], coeff[20]);
  bme680->calib.par_p10 = coeff[22];

  bme680->calib.par_h1 = (uint16_t)(((uint16_t)coeff[25] << 4) |
                                    (coeff[24] & BME680_BIT_H1_DATA_MSK));
  bme680->calib.par_h2 =
      (uint16_t)(((uint16_t)coeff[23] << 4) | ((uint8_t)(coeff[24] >> 4)));
  bme680->calib.par_h3 = (int8_t)coeff[26];
  bme680->calib.par_h4 = (int8_t)coeff[27];
  bme680->calib.par_h5 = (int8_t)coeff[28];
  bme680->calib.par_h6 = coeff[29];
  bme680->calib.par_h7 = (int8_t)coeff[30];

  bme680->calib.par_gh1 = (int8_t)coeff[35];
  bme680->calib.par_gh2 = (int16_t)BME680_CONCAT_BYTES(coeff[34], coeff[33]);
  bme680->calib.par_gh3 = (int8_t)coeff[36];

  bme680->calib.res_heat_range =
      (uint8_t)((coeff[39] & BME680_RHRANGE_MSK) / 16u);
  bme680->calib.res_heat_val = (int8_t)coeff[37];
  bme680->calib.range_sw_err =
      (int8_t)((int8_t)(coeff[41] & BME680_RSERROR_MSK) / 16);

  return true;
}

static bool _dev_bme680_configure(dev_bme680_t *bme680) {
  if (!_dev_bme680_write_register_retry(bme680, BME680_REG_SOFT_RESET,
                                        BME680_SOFT_RESET_CMD)) {
    sys_debugf("bme680: soft reset write failed");
    return false;
  }

  sys_sleep_ms(10);

  if (!_dev_bme680_read_calibration(bme680)) {
    sys_debugf("bme680: calibration read failed");
    return false;
  }

  uint8_t ctrl_hum =
      _dev_bme680_clamp_os(bme680->config.os_hum) & BME680_OSH_MSK;
  if (!_dev_bme680_write_register_retry(bme680, BME680_REG_CTRL_HUM,
                                        ctrl_hum)) {
    sys_debugf("bme680: ctrl_hum write failed");
    return false;
  }

  uint8_t config =
      BME680_SET_BITS(0u, BME680_FILTER_MSK, BME680_FILTER_POS,
                      _dev_bme680_clamp_filter(bme680->config.iir_filter));
  if (!_dev_bme680_write_register_retry(bme680, BME680_REG_CONFIG, config)) {
    sys_debugf("bme680: config write failed");
    return false;
  }

  if (bme680->config.gas_mode == DEV_BME680_GAS_DISABLED) {
    return true;
  }

  uint8_t res_heat =
      _dev_bme680_calc_res_heat((uint16_t)bme680->config.heater_temp_c, bme680);
  uint8_t gas_wait =
      _dev_bme680_calc_gas_wait((uint16_t)bme680->config.heater_duration_ms);
  if (!_dev_bme680_write_register_retry(bme680, BME680_REG_RES_HEAT0,
                                        res_heat) ||
      !_dev_bme680_write_register_retry(bme680, BME680_REG_GAS_WAIT0,
                                        gas_wait)) {
    sys_debugf("bme680: heater profile write failed (res_heat=%u gas_wait=%u)",
               (unsigned int)res_heat, (unsigned int)gas_wait);
    sys_debugf("bme680: continuing with heater/gas disabled");
    return true;
  }

  uint8_t ctrl_gas_0 = 0u;
  ctrl_gas_0 = BME680_SET_BITS(ctrl_gas_0, BME680_HCTRL_MSK, BME680_HCTRL_POS,
                               BME680_ENABLE_HEATER);
  if (!_dev_bme680_write_register_retry(bme680, BME680_REG_CTRL_GAS_0,
                                        ctrl_gas_0)) {
    sys_debugf("bme680: ctrl_gas_0 write failed");
    sys_debugf("bme680: continuing with gas path disabled");
    return true;
  }

  uint8_t run_gas = (bme680->variant_id == BME680_VARIANT_GAS_HIGH)
                        ? BME680_ENABLE_GAS_MEAS_HIGH
                        : BME680_ENABLE_GAS_MEAS_LOW;
  uint8_t ctrl_gas_1 = 0u;
  ctrl_gas_1 = BME680_SET_BITS_POS_0(ctrl_gas_1, BME680_NBCONV_MSK, 0u);
  ctrl_gas_1 = BME680_SET_BITS(ctrl_gas_1, BME680_RUN_GAS_MSK,
                               BME680_RUN_GAS_POS, run_gas);

  if (!_dev_bme680_write_register_retry(bme680, BME680_REG_CTRL_GAS_1,
                                        ctrl_gas_1)) {
    sys_debugf("bme680: ctrl_gas_1 write failed (variant=%u run_gas=%u)",
               (unsigned int)bme680->variant_id, (unsigned int)run_gas);
    sys_debugf("bme680: continuing with gas path disabled");
    return true;
  }

  return true;
}

void dev_bme680_default_config(dev_bme680_config_t *config) {
  if (config == NULL) {
    return;
  }

  config->os_temp = DEV_BME680_OVERSAMPLING_1X;
  config->os_press = DEV_BME680_OVERSAMPLING_1X;
  config->os_hum = DEV_BME680_OVERSAMPLING_1X;
  config->iir_filter = DEV_BME680_IIR_FILTER_OFF;
  config->heater_temp_c = DEV_BME680_HEATER_TEMP_300C;
  config->heater_duration_ms = DEV_BME680_HEATER_DUR_100MS;
  config->ambient_temp_c = DEV_BME680_AMBIENT_TEMP_25C;
  config->gas_mode = DEV_BME680_GAS_ENABLED;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_bme680_t *dev_bme680_init_i2c(hw_i2c_t *i2c,
                                  const dev_bme680_config_t *config) {
  if (!hw_i2c_valid(i2c)) {
    sys_debugf("bme680: i2c handle invalid");
    return NULL;
  }

  dev_bme680_t *bme680 = sys_calloc(1, sizeof(*bme680));
  if (bme680 == NULL) {
    return NULL;
  }

  bme680->bus = DEV_BME680_BUS_I2C;
  bme680->i2c = i2c;
  dev_bme680_default_config(&bme680->config);
  if (config != NULL) {
    bme680->config = *config;
  }
  bme680->ambient_temp_c = (int8_t)bme680->config.ambient_temp_c;
  bme680->init = true;

  uint8_t chip_id = 0u;
  bme680->i2c_addr = BME680_I2C_ADDR_PRIMARY;
  bool read_primary =
      _dev_bme680_read_register(bme680, BME680_REG_CHIP_ID, &chip_id);
  sys_debugf("bme680: probe addr 0x%02X read=%u chip_id=0x%02X",
             (unsigned int)BME680_I2C_ADDR_PRIMARY,
             (unsigned int)(read_primary ? 1u : 0u), (unsigned int)chip_id);

  if (!read_primary || chip_id != BME680_CHIP_ID) {
    bme680->i2c_addr = BME680_I2C_ADDR_SECONDARY;
    bool read_secondary =
        _dev_bme680_read_register(bme680, BME680_REG_CHIP_ID, &chip_id);
    sys_debugf("bme680: probe addr 0x%02X read=%u chip_id=0x%02X",
               (unsigned int)BME680_I2C_ADDR_SECONDARY,
               (unsigned int)(read_secondary ? 1u : 0u), (unsigned int)chip_id);
    if (!read_secondary || chip_id != BME680_CHIP_ID) {
      sys_debugf("bme680: chip id check failed on both addresses");
      dev_bme680_deinit(bme680);
      return NULL;
    }
  }

  bme680->chip_id = chip_id;

  sys_sleep_ms(2);
  if (!_dev_bme680_read_register_retry(bme680, BME680_REG_VARIANT_ID,
                                       &bme680->variant_id)) {
    bme680->variant_id = BME680_VARIANT_GAS_LOW;
    sys_debugf("bme680: variant id read failed, defaulting to GAS_LOW");
  }

  if (!_dev_bme680_configure(bme680)) {
    sys_debugf("bme680: configure failed");
    dev_bme680_deinit(bme680);
    return NULL;
  }

  return bme680;
}

dev_bme680_t *dev_bme680_init_spi(hw_spi_t *spi, hw_gpio_t *cs_pin,
                                  const dev_bme680_config_t *config) {
  if (!hw_spi_valid(spi)) {
    return NULL;
  }

  dev_bme680_t *bme680 = sys_calloc(1, sizeof(*bme680));
  if (bme680 == NULL) {
    return NULL;
  }

  bme680->bus = DEV_BME680_BUS_SPI;
  bme680->spi = spi;
  bme680->cs_pin = cs_pin;
  dev_bme680_default_config(&bme680->config);
  if (config != NULL) {
    bme680->config = *config;
  }
  bme680->ambient_temp_c = (int8_t)bme680->config.ambient_temp_c;
  bme680->init = true;

  if (hw_gpio_valid(bme680->cs_pin)) {
    hw_gpio_set_mode(bme680->cs_pin, HW_GPIO_OUTPUT);
    _dev_bme680_spi_cs_set(bme680, false);
  }

  uint8_t chip_id = 0u;
  if (!_dev_bme680_read_register(bme680, BME680_REG_CHIP_ID, &chip_id) ||
      chip_id != BME680_CHIP_ID) {
    dev_bme680_deinit(bme680);
    return NULL;
  }

  bme680->chip_id = chip_id;

  if (!_dev_bme680_read_register_retry(bme680, BME680_REG_VARIANT_ID,
                                       &bme680->variant_id)) {
    bme680->variant_id = BME680_VARIANT_GAS_LOW;
    sys_debugf("bme680: variant id read failed, defaulting to GAS_LOW");
  }

  if (!_dev_bme680_configure(bme680)) {
    dev_bme680_deinit(bme680);
    return NULL;
  }

  return bme680;
}

void dev_bme680_deinit(dev_bme680_t *bme680) {
  if (!_dev_bme680_bus_ready(bme680) || bme680->chip_id != BME680_CHIP_ID) {
    return;
  }

  sys_memset(bme680, 0, sizeof(*bme680));
  sys_free(bme680);
}

uint8_t dev_bme680_chip_id(const dev_bme680_t *bme680) {
  if (!_dev_bme680_bus_ready(bme680) || bme680->chip_id != BME680_CHIP_ID) {
    return 0u;
  }

  return bme680->chip_id;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool dev_bme680_read_data(dev_bme680_t *bme680, dev_bme680_data_t *data) {
  if (!_dev_bme680_bus_ready(bme680) || bme680->chip_id != BME680_CHIP_ID ||
      data == NULL) {
    return false;
  }

  uint8_t ctrl_meas = 0u;
  ctrl_meas = BME680_SET_BITS(ctrl_meas, BME680_OST_MSK, BME680_OST_POS,
                              _dev_bme680_clamp_os(bme680->config.os_temp));
  ctrl_meas = BME680_SET_BITS(ctrl_meas, BME680_OSP_MSK, BME680_OSP_POS,
                              _dev_bme680_clamp_os(bme680->config.os_press));
  ctrl_meas =
      BME680_SET_BITS_POS_0(ctrl_meas, BME680_MODE_MSK, BME680_FORCED_MODE);

  if (!_dev_bme680_write_register(bme680, BME680_REG_CTRL_MEAS, ctrl_meas)) {
    return false;
  }

  uint8_t field[BME680_LEN_FIELD] = {0};
  uint8_t attempts = BME680_MEAS_RETRY_COUNT;
  bool got_new_data = false;

  while (attempts-- > 0u) {
    sys_sleep_ms(BME680_MEAS_POLL_MS);

    if (!_dev_bme680_read_registers(bme680, BME680_REG_FIELD0, field,
                                    sizeof(field))) {
      return false;
    }

    if ((field[0] & BME680_FIELD_NEW_DATA_MSK) != 0u) {
      got_new_data = true;
      break;
    }
  }

  if (!got_new_data) {
    return false;
  }

  uint32_t adc_pres = ((uint32_t)field[2] * 4096u) |
                      ((uint32_t)field[3] * 16u) | ((uint32_t)field[4] / 16u);
  uint32_t adc_temp = ((uint32_t)field[5] * 4096u) |
                      ((uint32_t)field[6] * 16u) | ((uint32_t)field[7] / 16u);
  uint16_t adc_hum = ((uint16_t)field[8] * 256u) | (uint16_t)field[9];

  uint16_t adc_gas_res_low =
      (uint16_t)((uint32_t)field[13] * 4u | ((uint32_t)field[14] / 64u));
  uint16_t adc_gas_res_high =
      (uint16_t)((uint32_t)field[15] * 4u | ((uint32_t)field[16] / 64u));
  uint8_t gas_range_l = field[14] & BME680_FIELD_GAS_RANGE_MSK;
  uint8_t gas_range_h = field[16] & BME680_FIELD_GAS_RANGE_MSK;

  uint8_t status = field[0];
  if (bme680->variant_id == BME680_VARIANT_GAS_HIGH) {
    status = (uint8_t)(status | (field[16] & BME680_FIELD_GASM_VALID_MSK) |
                       (field[16] & BME680_FIELD_HEAT_STAB_MSK));
  } else {
    status = (uint8_t)(status | (field[14] & BME680_FIELD_GASM_VALID_MSK) |
                       (field[14] & BME680_FIELD_HEAT_STAB_MSK));
  }

  int16_t temp_x100 = _dev_bme680_compensate_temperature(adc_temp, bme680);
  uint32_t pressure_pa = _dev_bme680_compensate_pressure(adc_pres, bme680);
  uint32_t humidity_x1000 = _dev_bme680_compensate_humidity(adc_hum, bme680);

  uint32_t gas_ohms = 0u;
  bool gas_valid = false;
  if (bme680->config.gas_mode == DEV_BME680_GAS_ENABLED) {
    gas_valid = (status & BME680_FIELD_GASM_VALID_MSK) != 0u;
    if (bme680->variant_id == BME680_VARIANT_GAS_HIGH) {
      gas_ohms =
          _dev_bme680_calc_gas_resistance_high(adc_gas_res_high, gas_range_h);
    } else {
      gas_ohms = _dev_bme680_calc_gas_resistance_low(adc_gas_res_low,
                                                     gas_range_l, bme680);
    }

    if (!gas_valid) {
      gas_ohms = 0u;
    }
  }

  data->temperature_c = (float)temp_x100 / 100.0f;
  data->pressure_pa = (float)pressure_pa;
  data->humidity_pct = (float)humidity_x1000 / 1000.0f;
  data->gas_resistance_ohms = (float)gas_ohms;

  return true;
}

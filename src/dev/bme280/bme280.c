#include <picofuse/dev.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define BME280_I2C_ADDR_PRIMARY 0x76u
#define BME280_I2C_ADDR_SECONDARY 0x77u

#define BME280_REG_CHIP_ID 0xD0u
#define BME280_REG_RESET 0xE0u
#define BME280_REG_CTRL_HUM 0xF2u
#define BME280_REG_STATUS 0xF3u
#define BME280_REG_CTRL_MEAS 0xF4u
#define BME280_REG_CONFIG 0xF5u
#define BME280_REG_PRESS_MSB 0xF7u

#define BME280_REG_CALIB_00 0x88u
#define BME280_REG_CALIB_26 0xE1u

#define BME280_CHIP_ID 0x60u
#define BMP280_CHIP_ID 0x58u

#define BME280_SOFT_RESET_CMD 0xB6u
#define BME280_STATUS_MEASURING (1u << 3)

#define BME280_SPI_READ_MASK 0x80u
#define BME280_SPI_WRITE_MASK 0x7Fu

#define BME280_MEASUREMENT_TIMEOUT_MS 100u
#define BME280_DEFAULT_SEA_LEVEL_PRESSURE 101325.0f

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef enum {
  DEV_BME280_BUS_I2C = 1,
  DEV_BME280_BUS_SPI = 2,
} _dev_bme280_bus_t;

typedef struct {
  uint16_t dig_t1;
  int16_t dig_t2;
  int16_t dig_t3;
  uint16_t dig_p1;
  int16_t dig_p2;
  int16_t dig_p3;
  int16_t dig_p4;
  int16_t dig_p5;
  int16_t dig_p6;
  int16_t dig_p7;
  int16_t dig_p8;
  int16_t dig_p9;
  uint8_t dig_h1;
  int16_t dig_h2;
  uint8_t dig_h3;
  int16_t dig_h4;
  int16_t dig_h5;
  int8_t dig_h6;
  int32_t t_fine;
} _dev_bme280_calib_t;

struct dev_bme280_t {
  _dev_bme280_bus_t bus;
  hw_i2c_t *i2c;
  hw_spi_t *spi;
  hw_gpio_t *cs_pin;
  uint8_t i2c_addr;
  uint8_t chip_id;
  float temperature_offset_c;
  _dev_bme280_calib_t calib;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static dev_bme280_config_t _dev_bme280_default_config(void) {
  dev_bme280_config_t config = {0};
  config.temperature_oversampling = DRIVER_BME280_OVERSAMPLING_1X;
  config.pressure_oversampling = DRIVER_BME280_OVERSAMPLING_1X;
  config.humidity_oversampling = DRIVER_BME280_OVERSAMPLING_1X;
  config.filter = DRIVER_BME280_FILTER_OFF;
  config.temperature_offset_c = 0.0f;
  return config;
}

static bool _dev_bme280_valid_oversampling(driver_bme280_oversampling_t value) {
  return value <= DRIVER_BME280_OVERSAMPLING_16X;
}

static bool _dev_bme280_valid_filter(driver_bme280_filter_t value) {
  return value <= DRIVER_BME280_FILTER_16;
}

static dev_bme280_config_t
_dev_bme280_resolve_config(const dev_bme280_config_t *config) {
  dev_bme280_config_t resolved = _dev_bme280_default_config();
  if (config == NULL) {
    return resolved;
  }

  if (_dev_bme280_valid_oversampling(config->temperature_oversampling)) {
    resolved.temperature_oversampling = config->temperature_oversampling;
  }
  if (_dev_bme280_valid_oversampling(config->pressure_oversampling)) {
    resolved.pressure_oversampling = config->pressure_oversampling;
  }
  if (_dev_bme280_valid_oversampling(config->humidity_oversampling)) {
    resolved.humidity_oversampling = config->humidity_oversampling;
  }
  if (_dev_bme280_valid_filter(config->filter)) {
    resolved.filter = config->filter;
  }
  resolved.temperature_offset_c = config->temperature_offset_c;
  return resolved;
}

static bool _dev_bme280_is_spi(const dev_bme280_t *bme280) {
  return bme280 != NULL && bme280->bus == DEV_BME280_BUS_SPI;
}

static void _dev_bme280_spi_cs_set(dev_bme280_t *bme280, bool active) {
  if (bme280 == NULL || !hw_gpio_valid(bme280->cs_pin)) {
    return;
  }

  // External CS pin is active low.
  hw_gpio_set(bme280->cs_pin, active ? false : true);
}

static bool _dev_bme280_write_register(dev_bme280_t *bme280, uint8_t reg,
                                       uint8_t value) {
  if (!dev_bme280_valid(bme280)) {
    return false;
  }

  if (_dev_bme280_is_spi(bme280)) {
    _dev_bme280_spi_cs_set(bme280, true);
    size_t written =
        hw_spi_write(bme280->spi, reg & BME280_SPI_WRITE_MASK, &value, 1u, 0u);
    _dev_bme280_spi_cs_set(bme280, false);
    return written == 1u;
  }

  return hw_i2c_write(bme280->i2c, bme280->i2c_addr, reg, &value, 1u, 0u) == 1u;
}

static bool _dev_bme280_read_registers(dev_bme280_t *bme280, uint8_t reg,
                                       uint8_t *data, size_t len) {
  if (!dev_bme280_valid(bme280) || data == NULL || len == 0u) {
    return false;
  }

  if (_dev_bme280_is_spi(bme280)) {
    _dev_bme280_spi_cs_set(bme280, true);
    size_t read =
        hw_spi_read(bme280->spi, reg | BME280_SPI_READ_MASK, data, len, 0u);
    _dev_bme280_spi_cs_set(bme280, false);
    return read == len;
  }

  return hw_i2c_read(bme280->i2c, bme280->i2c_addr, reg, data, len, 0u) == len;
}

static bool _dev_bme280_read_register(dev_bme280_t *bme280, uint8_t reg,
                                      uint8_t *value) {
  return _dev_bme280_read_registers(bme280, reg, value, 1u);
}

static bool _dev_bme280_read_calibration(dev_bme280_t *bme280) {
  uint8_t calib[26] = {0};
  uint8_t calib_h[7] = {0};

  if (!_dev_bme280_read_registers(bme280, BME280_REG_CALIB_00, calib,
                                  sizeof(calib))) {
    return false;
  }

  if (bme280->chip_id == BME280_CHIP_ID) {
    if (!_dev_bme280_read_registers(bme280, BME280_REG_CALIB_26, calib_h,
                                    sizeof(calib_h))) {
      return false;
    }
  }

  bme280->calib.dig_t1 = (uint16_t)((calib[1] << 8) | calib[0]);
  bme280->calib.dig_t2 = (int16_t)((calib[3] << 8) | calib[2]);
  bme280->calib.dig_t3 = (int16_t)((calib[5] << 8) | calib[4]);

  bme280->calib.dig_p1 = (uint16_t)((calib[7] << 8) | calib[6]);
  bme280->calib.dig_p2 = (int16_t)((calib[9] << 8) | calib[8]);
  bme280->calib.dig_p3 = (int16_t)((calib[11] << 8) | calib[10]);
  bme280->calib.dig_p4 = (int16_t)((calib[13] << 8) | calib[12]);
  bme280->calib.dig_p5 = (int16_t)((calib[15] << 8) | calib[14]);
  bme280->calib.dig_p6 = (int16_t)((calib[17] << 8) | calib[16]);
  bme280->calib.dig_p7 = (int16_t)((calib[19] << 8) | calib[18]);
  bme280->calib.dig_p8 = (int16_t)((calib[21] << 8) | calib[20]);
  bme280->calib.dig_p9 = (int16_t)((calib[23] << 8) | calib[22]);

  if (bme280->chip_id == BME280_CHIP_ID) {
    bme280->calib.dig_h1 = calib[25];
    bme280->calib.dig_h2 = (int16_t)((calib_h[1] << 8) | calib_h[0]);
    bme280->calib.dig_h3 = calib_h[2];
    bme280->calib.dig_h4 = (int16_t)((calib_h[3] << 4) | (calib_h[4] & 0x0F));
    bme280->calib.dig_h5 = (int16_t)((calib_h[5] << 4) | (calib_h[4] >> 4));
    bme280->calib.dig_h6 = (int8_t)calib_h[6];
  } else {
    bme280->calib.dig_h1 = 0;
    bme280->calib.dig_h2 = 0;
    bme280->calib.dig_h3 = 0;
    bme280->calib.dig_h4 = 0;
    bme280->calib.dig_h5 = 0;
    bme280->calib.dig_h6 = 0;
  }

  return true;
}

static float _dev_bme280_compensate_temperature(dev_bme280_t *bme280,
                                                int32_t adc_t) {
  int32_t var1 = ((((adc_t >> 3) - ((int32_t)bme280->calib.dig_t1 << 1))) *
                  ((int32_t)bme280->calib.dig_t2)) >>
                 11;
  int32_t var2 = (((((adc_t >> 4) - ((int32_t)bme280->calib.dig_t1)) *
                    ((adc_t >> 4) - ((int32_t)bme280->calib.dig_t1))) >>
                   12) *
                  ((int32_t)bme280->calib.dig_t3)) >>
                 14;

  bme280->calib.t_fine = var1 + var2;
  int32_t t = (bme280->calib.t_fine * 5 + 128) >> 8;
  return ((float)t / 100.0f) + bme280->temperature_offset_c;
}

static float _dev_bme280_compensate_pressure(dev_bme280_t *bme280,
                                             int32_t adc_p) {
  int64_t var1 = ((int64_t)bme280->calib.t_fine) - 128000;
  int64_t var2 = var1 * var1 * (int64_t)bme280->calib.dig_p6;
  var2 = var2 + ((var1 * (int64_t)bme280->calib.dig_p5) << 17);
  var2 = var2 + (((int64_t)bme280->calib.dig_p4) << 35);

  var1 = ((var1 * var1 * (int64_t)bme280->calib.dig_p3) >> 8) +
         ((var1 * (int64_t)bme280->calib.dig_p2) << 12);
  var1 =
      (((((int64_t)1) << 47) + var1) * ((int64_t)bme280->calib.dig_p1)) >> 33;

  if (var1 == 0) {
    return 0.0f;
  }

  int64_t p = 1048576 - adc_p;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)bme280->calib.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)bme280->calib.dig_p8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)bme280->calib.dig_p7) << 4);

  return (float)p / 256.0f;
}

static float _dev_bme280_compensate_humidity(dev_bme280_t *bme280,
                                             int32_t adc_h) {
  int32_t v_x1_u32r = bme280->calib.t_fine - ((int32_t)76800);
  v_x1_u32r = (((((adc_h << 14) - (((int32_t)bme280->calib.dig_h4) << 20) -
                  (((int32_t)bme280->calib.dig_h5) * v_x1_u32r)) +
                 ((int32_t)16384)) >>
                15) *
               (((((((v_x1_u32r * ((int32_t)bme280->calib.dig_h6)) >> 10) *
                    (((v_x1_u32r * ((int32_t)bme280->calib.dig_h3)) >> 11) +
                     ((int32_t)32768))) >>
                   10) +
                  ((int32_t)2097152)) *
                     ((int32_t)bme280->calib.dig_h2) +
                 8192) >>
                14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                             ((int32_t)bme280->calib.dig_h1)) >>
                            4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

  return (float)(v_x1_u32r >> 12) / 1024.0f;
}

static bool _dev_bme280_configure(dev_bme280_t *bme280,
                                  const dev_bme280_config_t *config) {
  dev_bme280_config_t resolved = _dev_bme280_resolve_config(config);
  bme280->temperature_offset_c = resolved.temperature_offset_c;

  if (!_dev_bme280_write_register(bme280, BME280_REG_RESET,
                                  BME280_SOFT_RESET_CMD)) {
    return false;
  }

  sys_sleep_ms(10);

  if (!_dev_bme280_read_calibration(bme280)) {
    return false;
  }

  if (bme280->chip_id == BME280_CHIP_ID) {
    uint8_t ctrl_hum = (uint8_t)resolved.humidity_oversampling & 0x07u;
    if (!_dev_bme280_write_register(bme280, BME280_REG_CTRL_HUM, ctrl_hum)) {
      return false;
    }
  }

  uint8_t reg_config = ((uint8_t)resolved.filter & 0x07u) << 2;
  if (!_dev_bme280_write_register(bme280, BME280_REG_CONFIG, reg_config)) {
    return false;
  }

  uint8_t ctrl_meas =
      (((uint8_t)resolved.temperature_oversampling & 0x07u) << 5) |
      (((uint8_t)resolved.pressure_oversampling & 0x07u) << 2);

  return _dev_bme280_write_register(bme280, BME280_REG_CTRL_MEAS, ctrl_meas);
}

static float _dev_bme280_pow_1_5255(float x) {
  float ln_x = x - 1.0f;
  float ln_x2 = ln_x * ln_x;
  ln_x = ln_x - ln_x2 * 0.5f + ln_x2 * ln_x * 0.333333f;

  float exp_arg = ln_x * 0.1903f;
  float exp_arg2 = exp_arg * exp_arg;
  return 1.0f + exp_arg + exp_arg2 * 0.5f + exp_arg2 * exp_arg * 0.166667f;
}

static float _dev_bme280_pow_5255(float x) {
  float ln_x = x - 1.0f;
  float ln_x2 = ln_x * ln_x;
  ln_x = ln_x - ln_x2 * 0.5f + ln_x2 * ln_x * 0.333333f;

  float exp_arg = ln_x * 5.255f;
  float exp_arg2 = exp_arg * exp_arg;
  return 1.0f + exp_arg + exp_arg2 * 0.5f + exp_arg2 * exp_arg * 0.166667f;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

dev_bme280_t *dev_bme280_init_i2c(hw_i2c_t *i2c,
                                  const dev_bme280_config_t *config) {
  if (!hw_i2c_valid(i2c)) {
    return NULL;
  }

  dev_bme280_t *bme280 = sys_calloc(1, sizeof(*bme280));
  if (bme280 == NULL) {
    return NULL;
  }

  bme280->bus = DEV_BME280_BUS_I2C;
  bme280->i2c = i2c;

  uint8_t chip_id = 0u;
  bme280->i2c_addr = BME280_I2C_ADDR_PRIMARY;
  bool read_primary =
      _dev_bme280_read_register(bme280, BME280_REG_CHIP_ID, &chip_id);

  if (!read_primary ||
      (chip_id != BME280_CHIP_ID && chip_id != BMP280_CHIP_ID)) {
    bme280->i2c_addr = BME280_I2C_ADDR_SECONDARY;
    bool read_secondary =
        _dev_bme280_read_register(bme280, BME280_REG_CHIP_ID, &chip_id);
    if (!read_secondary ||
        (chip_id != BME280_CHIP_ID && chip_id != BMP280_CHIP_ID)) {
      sys_free(bme280);
      return NULL;
    }
  }

  bme280->chip_id = chip_id;
  bme280->init = true;

  if (!_dev_bme280_configure(bme280, config)) {
    dev_bme280_deinit(bme280);
    return NULL;
  }

  return bme280;
}

dev_bme280_t *dev_bme280_init_spi(hw_spi_t *spi, hw_gpio_t *cs_pin,
                                  const dev_bme280_config_t *config) {
  if (!hw_spi_valid(spi)) {
    return NULL;
  }

  dev_bme280_t *bme280 = sys_calloc(1, sizeof(*bme280));
  if (bme280 == NULL) {
    return NULL;
  }

  bme280->bus = DEV_BME280_BUS_SPI;
  bme280->spi = spi;
  bme280->cs_pin = cs_pin;

  if (hw_gpio_valid(bme280->cs_pin)) {
    hw_gpio_set_mode(bme280->cs_pin, HW_GPIO_OUTPUT);
    _dev_bme280_spi_cs_set(bme280, false);
  }

  uint8_t chip_id = 0u;
  bme280->init = true;
  if (!_dev_bme280_read_register(bme280, BME280_REG_CHIP_ID, &chip_id) ||
      (chip_id != BME280_CHIP_ID && chip_id != BMP280_CHIP_ID)) {
    dev_bme280_deinit(bme280);
    return NULL;
  }

  bme280->chip_id = chip_id;

  if (!_dev_bme280_configure(bme280, config)) {
    dev_bme280_deinit(bme280);
    return NULL;
  }

  return bme280;
}

void dev_bme280_deinit(dev_bme280_t *bme280) {
  if (!dev_bme280_valid(bme280)) {
    return;
  }

  (void)_dev_bme280_write_register(bme280, BME280_REG_CTRL_MEAS, 0x00u);
  sys_memset(bme280, 0, sizeof(*bme280));
  sys_free(bme280);
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

bool dev_bme280_valid(const dev_bme280_t *bme280) {
  if (bme280 == NULL || !bme280->init) {
    return false;
  }

  if (bme280->bus == DEV_BME280_BUS_I2C) {
    return hw_i2c_valid(bme280->i2c) &&
           (bme280->i2c_addr == BME280_I2C_ADDR_PRIMARY ||
            bme280->i2c_addr == BME280_I2C_ADDR_SECONDARY);
  }

  if (bme280->bus == DEV_BME280_BUS_SPI) {
    return hw_spi_valid(bme280->spi);
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool dev_bme280_read_data(dev_bme280_t *bme280, dev_bme280_data_t *data) {
  if (!dev_bme280_valid(bme280) || data == NULL) {
    return false;
  }

  uint8_t ctrl_meas = 0u;
  if (!_dev_bme280_read_register(bme280, BME280_REG_CTRL_MEAS, &ctrl_meas)) {
    return false;
  }

  ctrl_meas = (ctrl_meas & 0xFCu) | 0x01u;
  if (!_dev_bme280_write_register(bme280, BME280_REG_CTRL_MEAS, ctrl_meas)) {
    return false;
  }

  uint8_t status = 0u;
  uint32_t timeout = BME280_MEASUREMENT_TIMEOUT_MS;
  do {
    sys_sleep_ms(1);
    if (!_dev_bme280_read_register(bme280, BME280_REG_STATUS, &status)) {
      return false;
    }
  } while ((status & BME280_STATUS_MEASURING) != 0u && --timeout > 0u);

  if (timeout == 0u) {
    return false;
  }

  uint8_t raw[8] = {0};
  if (!_dev_bme280_read_registers(bme280, BME280_REG_PRESS_MSB, raw,
                                  sizeof(raw))) {
    return false;
  }

  int32_t adc_p =
      ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | ((int32_t)raw[2] >> 4);
  int32_t adc_t =
      ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | ((int32_t)raw[5] >> 4);
  int32_t adc_h = ((int32_t)raw[6] << 8) | (int32_t)raw[7];

  data->temperature_c = _dev_bme280_compensate_temperature(bme280, adc_t);
  data->pressure_pa = _dev_bme280_compensate_pressure(bme280, adc_p);
  if (bme280->chip_id == BME280_CHIP_ID) {
    data->humidity_pct = _dev_bme280_compensate_humidity(bme280, adc_h);
  } else {
    data->humidity_pct = 0.0f;
  }

  return true;
}

float dev_bme280_calculate_altitude(const dev_bme280_data_t *data,
                                    float sea_level_pressure) {
  sys_assert(data != NULL);

  if (sea_level_pressure == 0.0f) {
    sea_level_pressure = BME280_DEFAULT_SEA_LEVEL_PRESSURE;
  }

  float ratio = data->pressure_pa / sea_level_pressure;
  return 44330.0f * (1.0f - _dev_bme280_pow_1_5255(ratio));
}

float dev_bme280_calculate_sea_level_pressure(const dev_bme280_data_t *data,
                                              float altitude) {
  sys_assert(data != NULL);

  float ratio = 1.0f - (altitude / 44330.0f);
  return data->pressure_pa / _dev_bme280_pow_5255(ratio);
}

#include <hardware/spi.h>
#include <picofuse/hw.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_spi_t {
  spi_inst_t *instance;
  hw_gpio_t *sck;
  hw_gpio_t *tx;
  hw_gpio_t *rx;
  hw_gpio_t *cs;
  bool cs_active_low;
  uint32_t baud_rate;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_spi_t spis[NUM_SPIS] = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static hw_spi_config_t _hw_spi_default_config(void) {
  hw_spi_config_t config = {0};
  config.cs_active_low = false;
  config.mode = HW_SPI_MODE_0;
  config.bits_per_word = 8u;
  return config;
}

static spi_inst_t *_hw_spi_instance_for_index(uint8_t index) {
  switch (index) {
  case 0:
    return spi0;
#if NUM_SPIS > 1
  case 1:
    return spi1;
#endif
  default:
    return NULL;
  }
}

static void _hw_spi_set_cs(const hw_spi_t *spi, bool active) {
  if (spi != NULL && hw_gpio_valid(spi->cs)) {
    hw_gpio_set(spi->cs, spi->cs_active_low ? !active : active);
  }
}

static bool _hw_spi_map_mode(hw_spi_mode_t mode, spi_cpol_t *cpol,
                             spi_cpha_t *cpha) {
  if (cpol == NULL || cpha == NULL) {
    return false;
  }

  switch (mode) {
  case HW_SPI_MODE_0:
    *cpol = SPI_CPOL_0;
    *cpha = SPI_CPHA_0;
    return true;
  case HW_SPI_MODE_1:
    *cpol = SPI_CPOL_0;
    *cpha = SPI_CPHA_1;
    return true;
  case HW_SPI_MODE_2:
    *cpol = SPI_CPOL_1;
    *cpha = SPI_CPHA_0;
    return true;
  case HW_SPI_MODE_3:
    *cpol = SPI_CPOL_1;
    *cpha = SPI_CPHA_1;
    return true;
  default:
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

hw_spi_t *hw_spi_init_default(uint32_t baud_rate,
                              const hw_spi_config_t *config) {
  hw_spi_config_t settings =
      config != NULL ? *config : _hw_spi_default_config();
  hw_gpio_t *sck_pin = hw_gpio_init(0, PICO_DEFAULT_SPI_SCK_PIN, HW_GPIO_SPI);
  hw_gpio_t *tx_pin = hw_gpio_init(0, PICO_DEFAULT_SPI_TX_PIN, HW_GPIO_SPI);
  hw_gpio_t *rx_pin = hw_gpio_init(0, PICO_DEFAULT_SPI_RX_PIN, HW_GPIO_SPI);
#ifdef PICO_DEFAULT_SPI_CSN_PIN
  hw_gpio_t *cs_pin = hw_gpio_init(0, PICO_DEFAULT_SPI_CSN_PIN, HW_GPIO_OUTPUT);
#else
  hw_gpio_t *cs_pin = NULL;
#endif

  hw_spi_t *spi =
      hw_spi_init(0, sck_pin, tx_pin, rx_pin, cs_pin, baud_rate, &settings);
  if (spi == NULL) {
    if (hw_gpio_valid(cs_pin)) {
      hw_gpio_deinit(cs_pin);
    }
    if (hw_gpio_valid(rx_pin)) {
      hw_gpio_deinit(rx_pin);
    }
    if (hw_gpio_valid(tx_pin)) {
      hw_gpio_deinit(tx_pin);
    }
    if (hw_gpio_valid(sck_pin)) {
      hw_gpio_deinit(sck_pin);
    }
  }

  return spi;
}

hw_spi_t *hw_spi_init(uint8_t index, hw_gpio_t *sck_pin, hw_gpio_t *tx_pin,
                      hw_gpio_t *rx_pin, hw_gpio_t *cs_pin, uint32_t baud_rate,
                      const hw_spi_config_t *config) {
  hw_spi_config_t settings =
      config != NULL ? *config : _hw_spi_default_config();
  spi_inst_t *instance = _hw_spi_instance_for_index(index);

  if (instance == NULL || !hw_gpio_valid(sck_pin) || !hw_gpio_valid(tx_pin) ||
      !hw_gpio_valid(rx_pin) || baud_rate == 0 || settings.bits_per_word == 0) {
    return NULL;
  }

  spi_cpol_t cpol = SPI_CPOL_0;
  spi_cpha_t cpha = SPI_CPHA_0;
  if (!_hw_spi_map_mode(settings.mode, &cpol, &cpha)) {
    return NULL;
  }

  hw_spi_t *spi = &spis[index];
  if (spi->init) {
    hw_spi_deinit(spi);
  }

  hw_gpio_t *configured_cs = NULL;
  if (hw_gpio_valid(cs_pin)) {
    configured_cs = cs_pin;
  }

  hw_gpio_set_mode(sck_pin, HW_GPIO_SPI);
  hw_gpio_set_mode(tx_pin, HW_GPIO_SPI);
  hw_gpio_set_mode(rx_pin, HW_GPIO_SPI);
  if (configured_cs != NULL) {
    hw_gpio_set_mode(configured_cs, HW_GPIO_OUTPUT);
  }

  spi->instance = instance;
  spi->sck = sck_pin;
  spi->tx = tx_pin;
  spi->rx = rx_pin;
  spi->cs = configured_cs;
  spi->cs_active_low = settings.cs_active_low;
  spi->baud_rate = baud_rate;
  spi->init = true;

  spi_init(instance, baud_rate);
  spi_set_format(instance, settings.bits_per_word, cpol, cpha, SPI_MSB_FIRST);

  if (hw_gpio_valid(spi->cs)) {
    hw_gpio_set(spi->cs, !spi->cs_active_low);
    _hw_spi_set_cs(spi, false);
  }

  return spi;
}

hw_spi_t *hw_spi_init_device(const char *device, uint32_t baud_rate,
                             const hw_spi_config_t *config) {
  (void)device;
  (void)baud_rate;
  (void)config;
  return NULL;
}

void hw_spi_deinit(hw_spi_t *spi) {
  if (!hw_spi_valid(spi)) {
    return;
  }

  spi_deinit(spi->instance);
  if (hw_gpio_valid(spi->cs)) {
    hw_gpio_deinit(spi->cs);
  }
  hw_gpio_deinit(spi->sck);
  hw_gpio_deinit(spi->tx);
  hw_gpio_deinit(spi->rx);
  sys_memset(spi, 0, sizeof(*spi));
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

uint8_t hw_spi_count(void) { return NUM_SPIS; }

bool hw_spi_valid(const hw_spi_t *spi) {
  return spi != NULL && spi->instance != NULL && spi->baud_rate > 0;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

size_t hw_spi_xfr(hw_spi_t *spi, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_spi_valid(spi) || ((tx > 0 || rx > 0) && data == NULL)) {
    return 0;
  }

  _hw_spi_set_cs(spi, true);

  size_t bytes_transferred = 0;
  if (tx > 0 && rx > 0) {
    int ret =
        spi_write_read_blocking(spi->instance, data, (uint8_t *)data + tx, rx);
    if (ret == (int)rx) {
      bytes_transferred = tx + rx;
    }
  } else if (tx > 0) {
    int ret = spi_write_blocking(spi->instance, data, tx);
    if (ret == (int)tx) {
      bytes_transferred = tx;
    }
  } else if (rx > 0) {
    int ret = spi_read_blocking(spi->instance, 0x00, data, rx);
    if (ret == (int)rx) {
      bytes_transferred = rx;
    }
  }

  _hw_spi_set_cs(spi, false);
  return bytes_transferred;
}

size_t hw_spi_read(hw_spi_t *spi, uint8_t reg, void *data, size_t len,
                   uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_spi_valid(spi) || data == NULL || len == 0) {
    return 0;
  }

  _hw_spi_set_cs(spi, true);

  size_t bytes_transferred = 0;
  int ret = spi_write_blocking(spi->instance, &reg, 1);
  if (ret == 1) {
    ret = spi_read_blocking(spi->instance, 0x00, data, len);
    if (ret == (int)len) {
      bytes_transferred = len;
    }
  }

  _hw_spi_set_cs(spi, false);
  return bytes_transferred;
}

size_t hw_spi_write(hw_spi_t *spi, uint8_t reg, const void *data, size_t len,
                    uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_spi_valid(spi) || (data == NULL && len > 0)) {
    return 0;
  }

  _hw_spi_set_cs(spi, true);

  size_t bytes_transferred = 0;
  int ret = spi_write_blocking(spi->instance, &reg, 1);
  if (ret == 1) {
    if (len > 0) {
      ret = spi_write_blocking(spi->instance, data, len);
      if (ret == (int)len) {
        bytes_transferred = len;
      }
    }
  }

  _hw_spi_set_cs(spi, false);
  return bytes_transferred;
}
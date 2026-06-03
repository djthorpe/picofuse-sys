#include <picofuse/hw.h>
#include <picofuse/sys.h>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
// CONSTANTS

#define HW_SPI_MAX_ADAPTERS 8

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct hw_spi_t {
  int fd;
  uint32_t baud_rate;
  bool cs_active_low;
  bool init;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static struct hw_spi_t _hw_spi_instances[HW_SPI_MAX_ADAPTERS] = {0};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

static hw_spi_t *_hw_spi_alloc_slot(void) {
  for (size_t i = 0; i < HW_SPI_MAX_ADAPTERS; i++) {
    if (!_hw_spi_instances[i].init) {
      return &_hw_spi_instances[i];
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

uint8_t hw_spi_count(void) { return 0; }

hw_spi_t *hw_spi_init_default(bool cs_active_low, uint32_t baud_rate) {
  (void)cs_active_low;
  (void)baud_rate;
  return NULL;
}

hw_spi_t *hw_spi_init(uint8_t index, hw_gpio_t *sck_pin, hw_gpio_t *tx_pin,
                      hw_gpio_t *rx_pin, hw_gpio_t *cs_pin,
                      bool cs_active_low, uint32_t baud_rate) {
  (void)index;
  (void)sck_pin;
  (void)tx_pin;
  (void)rx_pin;
  (void)cs_pin;
  (void)cs_active_low;
  (void)baud_rate;
  return NULL;
}

hw_spi_t *hw_spi_init_device(const char *device, bool cs_active_low,
                             uint32_t baud_rate) {
  if (device == NULL || device[0] == '\0' || baud_rate == 0) {
    return NULL;
  }

  hw_spi_t *spi = _hw_spi_alloc_slot();
  if (spi == NULL) {
    return NULL;
  }

  int fd = open(device, O_RDWR);
  if (fd < 0) {
    return NULL;
  }

  uint8_t mode = cs_active_low ? 0u : SPI_CS_HIGH;
  uint8_t bits_per_word = 8u;

  if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
      ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 ||
      ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &baud_rate) < 0) {
    close(fd);
    return NULL;
  }

  sys_memset(spi, 0, sizeof(*spi));
  spi->fd = fd;
  spi->baud_rate = baud_rate;
  spi->cs_active_low = cs_active_low;
  spi->init = true;
  return spi;
}

void hw_spi_deinit(hw_spi_t *spi) {
  if (!hw_spi_valid(spi)) {
    return;
  }

  close(spi->fd);
  sys_memset(spi, 0, sizeof(*spi));
}

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

bool hw_spi_valid(const hw_spi_t *spi) {
  return spi != NULL && spi->init && spi->fd >= 0;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

size_t hw_spi_xfr(hw_spi_t *spi, void *data, size_t tx, size_t rx,
                  uint32_t timeout_ms) {
  if (!hw_spi_valid(spi) || ((tx > 0 || rx > 0) && data == NULL)) {
    return 0;
  }

  if (tx > UINT32_MAX || rx > UINT32_MAX || (tx == 0 && rx == 0)) {
    return 0;
  }

  uint8_t *bytes = data;
  struct spi_ioc_transfer xfr[2] = {0};
  uint32_t xfr_count = 0;

  if (tx > 0) {
    xfr[xfr_count].tx_buf = (uintptr_t)bytes;
    xfr[xfr_count].len = (uint32_t)tx;
    xfr[xfr_count].speed_hz = spi->baud_rate;
    xfr[xfr_count].bits_per_word = 8u;
    xfr[xfr_count].cs_change = (rx > 0) ? 0u : 1u;
    xfr_count++;
  }

  if (rx > 0) {
    xfr[xfr_count].rx_buf = (uintptr_t)(bytes + tx);
    xfr[xfr_count].len = (uint32_t)rx;
    xfr[xfr_count].speed_hz = spi->baud_rate;
    xfr[xfr_count].bits_per_word = 8u;
    xfr[xfr_count].cs_change = 1u;
    xfr_count++;
  }

  if (ioctl(spi->fd, SPI_IOC_MESSAGE(xfr_count), xfr) < 0) {
    return 0;
  }

  return tx + rx;
}

size_t hw_spi_read(hw_spi_t *spi, uint8_t reg, void *data, size_t len,
                   uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_spi_valid(spi) || (len > 0 && data == NULL) || len > UINT32_MAX) {
    return 0;
  }

  uint8_t command = reg;
  struct spi_ioc_transfer xfr[2] = {0};

  xfr[0].tx_buf = (uintptr_t)&command;
  xfr[0].len = 1u;
  xfr[0].speed_hz = spi->baud_rate;
  xfr[0].bits_per_word = 8u;
  xfr[0].cs_change = (len > 0) ? 0u : 1u;

  xfr[1].rx_buf = (uintptr_t)data;
  xfr[1].len = (uint32_t)len;
  xfr[1].speed_hz = spi->baud_rate;
  xfr[1].bits_per_word = 8u;
  xfr[1].cs_change = 1u;

  if (ioctl(spi->fd, SPI_IOC_MESSAGE((len > 0) ? 2 : 1), xfr) < 0) {
    return 0;
  }

  return len;
}

size_t hw_spi_write(hw_spi_t *spi, uint8_t reg, const void *data, size_t len,
                    uint32_t timeout_ms) {
  (void)timeout_ms;

  if (!hw_spi_valid(spi) || (len > 0 && data == NULL) || len > UINT32_MAX) {
    return 0;
  }

  uint8_t command = reg;
  struct spi_ioc_transfer xfr[2] = {0};

  xfr[0].tx_buf = (uintptr_t)&command;
  xfr[0].len = 1u;
  xfr[0].speed_hz = spi->baud_rate;
  xfr[0].bits_per_word = 8u;
  xfr[0].cs_change = (len > 0) ? 0u : 1u;

  xfr[1].tx_buf = (uintptr_t)data;
  xfr[1].len = (uint32_t)len;
  xfr[1].speed_hz = spi->baud_rate;
  xfr[1].bits_per_word = 8u;
  xfr[1].cs_change = 1u;

  if (ioctl(spi->fd, SPI_IOC_MESSAGE((len > 0) ? 2 : 1), xfr) < 0) {
    return 0;
  }

  return len;
}
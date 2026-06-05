/**
 * @file st7701.h
 * @brief ST7701 TFT display interface.
 * @defgroup ST7701 ST7701
 * @ingroup Device
 *
 * This module provides a device-level API for ST7701-based TFT display
 * controllers over SPI.
 */
#pragma once

#include <picofuse/hw.h>
#include <picofuse/pix/types.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque ST7701 handle.
 * @ingroup ST7701
 */
typedef struct dev_st7701_t dev_st7701_t;

/**
 * @brief ST7701 rotation modes.
 * @ingroup ST7701
 */
typedef enum {
  DEV_ST7701_ROTATION_0 = 0,   ///< No rotation.
  DEV_ST7701_ROTATION_90 = 1,  ///< Rotate 90 degrees clockwise.
  DEV_ST7701_ROTATION_180 = 2, ///< Rotate 180 degrees.
  DEV_ST7701_ROTATION_270 = 3, ///< Rotate 270 degrees clockwise.
} dev_st7701_rotation_t;

/**
 * @brief ST7701 interface pinout descriptors.
 * @ingroup ST7701
 *
 * These are logical pin ids documented from the board schematic. They are not
 * configured by this driver yet, but are stored for staged bring-up.
 */
typedef struct {
  uint8_t lcd_spi_data_pin; ///< LCD_SPI_DATA pin id.
  uint8_t lcd_spi_clk_pin;  ///< LCD_SPI_CLK pin id.
  uint8_t lcd_spi_cs_pin;   ///< LCD_SPI_CS pin id.

  uint8_t lcd_dot_clk_pin; ///< LCD_DOT_CLK pin id.
  uint8_t lcd_de_pin;      ///< LCD_DE pin id.
  uint8_t vsync_pin;       ///< VSYNC pin id.
  uint8_t hsync_pin;       ///< HSYNC pin id.

  uint8_t r2_pin; ///< R2 pin id.
  uint8_t r3_pin; ///< R3 pin id.
  uint8_t r4_pin; ///< R4 pin id.
  uint8_t r5_pin; ///< R5 pin id.
  uint8_t r6_pin; ///< R6 pin id.
  uint8_t r7_pin; ///< R7 pin id.

  uint8_t g2_pin; ///< G2 pin id.
  uint8_t g3_pin; ///< G3 pin id.
  uint8_t g4_pin; ///< G4 pin id.
  uint8_t g5_pin; ///< G5 pin id.
  uint8_t g6_pin; ///< G6 pin id.
  uint8_t g7_pin; ///< G7 pin id.

  uint8_t b2_pin; ///< B2 pin id.
  uint8_t b3_pin; ///< B3 pin id.
  uint8_t b4_pin; ///< B4 pin id.
  uint8_t b5_pin; ///< B5 pin id.
  uint8_t b6_pin; ///< B6 pin id.
  uint8_t b7_pin; ///< B7 pin id.
} dev_st7701_pinout_t;

/**
 * @brief Optional ST7701 initialization options.
 * @ingroup ST7701
 *
 * Pass `NULL` to @ref dev_st7701_init to use defaults.
 */
typedef struct {
  dev_st7701_rotation_t rotation; ///< Initial display rotation.
  dev_st7701_pinout_t pinout;     ///< Board/display signal mapping.
  bool backlight_active_low;      ///< True if backlight enable is active-low.

  // Optional backing buffers for staged driver bring-up.
  uint16_t *framebuffer; ///< Optional external framebuffer pointer.
  uint32_t *palette;     ///< Optional external palette pointer.
} dev_st7701_config_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize an ST7701 display over SPI.
 * @ingroup ST7701
 * @param spi SPI interface to use.
 * @param reset_pin GPIO used for hardware reset.
 * @param backlight_pin Optional GPIO used to enable backlight.
 * @param size Display size in pixels.
 * @param config Optional pointer to additional initialization options. Pass
 * `NULL` to use defaults.
 * @return ST7701 handle or `NULL` on failure.
 */
dev_st7701_t *dev_st7701_init(hw_spi_t *spi, hw_gpio_t *reset_pin,
                              hw_gpio_t *backlight_pin, pix_size_t size,
                              const dev_st7701_config_t *config);

/**
 * @brief Deinitialize an ST7701 display.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 *
 * Passing `NULL` is safe and is a no-op.
 */
void dev_st7701_deinit(dev_st7701_t *st7701);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Check whether an ST7701 handle is valid.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 * @retval true The handle is valid.
 * @retval false The handle is invalid.
 */
bool dev_st7701_valid(const dev_st7701_t *st7701);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Configure an SPI handle for ST7701 serial command mode.
 * @ingroup ST7701
 * @param spi SPI handle.
 * @retval true SPI was configured for mode-0, 9-bit frames.
 * @retval false SPI configuration failed.
 */
bool dev_st7701_configure_spi(hw_spi_t *spi);

/**
 * @brief Enable or disable display inversion mode.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 * @param enabled `true` to send INVON, `false` to send INVOFF.
 * @retval true Command was sent successfully.
 * @retval false Command failed.
 */
bool dev_st7701_set_inversion(dev_st7701_t *st7701, bool enabled);

/**
 * @brief Enable or disable force-all-pixels test mode.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 * @param enabled `true` to send ALLPON, `false` to send ALLPOFF.
 * @retval true Command was sent successfully.
 * @retval false Command failed.
 */
bool dev_st7701_set_all_pixels(dev_st7701_t *st7701, bool enabled);

/**
 * @brief Return panel to normal display mode.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 * @retval true Command was sent successfully.
 * @retval false Command failed.
 */
bool dev_st7701_set_normal_mode(dev_st7701_t *st7701);

/**
 * @brief Perform a hardware reset sequence.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 */
void dev_st7701_reset(dev_st7701_t *st7701);

/**
 * @brief Paint the full display from a framebuffer.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 * @param frame Source frame descriptor.
 * @retval true Paint/update was started successfully.
 * @retval false Paint/update failed.
 */
bool dev_st7701_paint(dev_st7701_t *st7701, const pix_frame_t *frame);

/**
 * @brief Paint a partial display region from a framebuffer.
 * @ingroup ST7701
 * @param st7701 ST7701 handle.
 * @param frame Source frame descriptor.
 * @param origin Region origin in panel pixel coordinates.
 * @param region_size Region size in pixels.
 * @retval true Paint/update was started successfully.
 * @retval false Paint/update failed.
 */
bool dev_st7701_paint_rect(dev_st7701_t *st7701, const pix_frame_t *frame,
                           pix_point_t origin, pix_size_t region_size);

/** @} */

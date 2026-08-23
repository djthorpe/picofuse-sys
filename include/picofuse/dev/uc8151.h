/**
 * @file uc8151.h
 * @brief UC8151 e-paper display interface.
 * @defgroup UC8151 UC8151
 * @ingroup Device
 *
 * This module provides a device-level API for monochrome UC8151 e-paper
 * display controllers over SPI.
 */
#pragma once

#include <picofuse/hw.h>
#include <picofuse/pix/types.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque UC8151 handle.
 * @ingroup UC8151
 */
typedef struct dev_uc8151_t dev_uc8151_t;

/**
 * @brief UC8151 update speed profile.
 * @ingroup UC8151
 */
typedef enum {
  DEV_UC8151_UPDATE_SPEED_DEFAULT = 0, ///< Default profile (~4.5 s)
  DEV_UC8151_UPDATE_SPEED_MEDIUM = 1,  ///< Medium profile (~2.0 s)
  DEV_UC8151_UPDATE_SPEED_FAST = 2,    ///< Fast profile (~0.8 s)
  DEV_UC8151_UPDATE_SPEED_TURBO = 3    ///< Turbo profile (~0.25 s)
} dev_uc8151_update_speed_t;

/**
 * @brief UC8151 rotation modes.
 * @ingroup UC8151
 */
typedef enum {
  DEV_UC8151_ROTATION_0 = 0,  ///< No rotation.
  DEV_UC8151_ROTATION_180 = 1 ///< 180-degree rotation.
} dev_uc8151_rotation_t;

/**
 * @brief Optional UC8151 initialization options.
 * @ingroup UC8151
 *
 * Pass `NULL` to @ref dev_uc8151_init to use backend defaults.
 */
typedef struct {
  bool inverted;                   ///< Start with color inversion enabled.
  bool blocking;                   ///< Wait for refresh completion in paint.
  dev_uc8151_update_speed_t speed; ///< Initial update speed profile.
  dev_uc8151_rotation_t rotation;  ///< Initial display rotation.
} dev_uc8151_config_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize a UC8151 display over SPI.
 * @ingroup UC8151
 * @param spi SPI interface to use.
 * @param dc_pin GPIO used for data/command selection.
 * @param reset_pin GPIO used for hardware reset.
 * @param busy_pin Busy GPIO input for ready/busy state.
 * @param size Display size in pixels.
 * @param config Optional pointer to additional initialization options. Pass
 * `NULL` to use default values.
 * @return UC8151 handle or NULL on failure.
 */
dev_uc8151_t *dev_uc8151_init(hw_spi_t *spi, hw_gpio_t *dc_pin,
                              hw_gpio_t *reset_pin, hw_gpio_t *busy_pin,
                              pix_size_t size,
                              const dev_uc8151_config_t *config);

/**
 * @brief Deinitialize a UC8151 display.
 * @ingroup UC8151
 * @param uc8151 UC8151 handle.
 *
 * Passing `NULL` is safe and is a no-op.
 */
void dev_uc8151_deinit(dev_uc8151_t *uc8151);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Perform a hardware reset sequence.
 * @ingroup UC8151
 * @param uc8151 UC8151 handle.
 */
void dev_uc8151_reset(dev_uc8151_t *uc8151);

/**
 * @brief Paint the full display from a 1bpp bitmap.
 * @ingroup UC8151
 * @param uc8151 UC8151 handle.
 * @param bitmap Source bitmap descriptor.
 *
 * @p bitmap dimensions must exactly match the initialized panel size.
 *
 * For @ref PIX_FMT_MONO, UC8151 expects column-major memory layout where each
 * column stores vertical bits and @ref pix_bitmap_t.stride is bytes per
 * column (`ceil(height / 8)`).
 * @retval true Paint/update was started successfully.
 * @retval false Paint/update failed.
 */
bool dev_uc8151_paint(dev_uc8151_t *uc8151, const pix_bitmap_t *bitmap);

/**
 * @brief Paint a partial display region from a 1bpp bitmap.
 * @ingroup UC8151
 * @param uc8151 UC8151 handle.
 * @param bitmap Source bitmap descriptor.
 * @param origin Region origin in panel pixel coordinates.
 * @param region_size Region size in pixels.
 *
 * Source pixels are read from the top-left of @p bitmap (0,0). @p origin
 * applies only to the destination position on the panel and is not used as a
 * source offset into @p bitmap.
 *
 * @p bitmap dimensions must exactly match @p region_size.
 *
 * For @ref PIX_FMT_MONO partial updates, bitmap data is column-major and
 * @ref pix_bitmap_t.stride is bytes per column (`region_size.h / 8`).
 * @retval true Paint/update was started successfully.
 * @retval false Paint/update failed.
 */
bool dev_uc8151_paint_rect(dev_uc8151_t *uc8151, const pix_bitmap_t *bitmap,
                           pix_point_t origin, pix_size_t region_size);

/** @} */

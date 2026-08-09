/**
 * @file framebuffer.h
 * @brief Linux framebuffer interface.
 * @defgroup Framebuffer Framebuffer
 * @ingroup Device
 *
 * This module provides a device-level API for Linux framebuffers.
 */
#pragma once
#include <picofuse/pix/types.h>
#include <stdbool.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque framebuffer handle.
 * @ingroup Framebuffer
 */
typedef struct dev_framebuffer_t dev_framebuffer_t;

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Initialize a Linux framebuffer.
 * @ingroup Framebuffer
 * @param device Path to the framebuffer device (e.g., "/dev/fb0").
 * @return Framebuffer handle or NULL on failure.
 */
dev_framebuffer_t *dev_framebuffer_init(const char *device);

/**
 * @brief Deinitialize a Linux framebuffer.
 * @ingroup Framebuffer
 * @param fb Framebuffer handle.
 */
void dev_framebuffer_deinit(dev_framebuffer_t *fb);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Get the width and height of the framebuffer.
 * @ingroup Framebuffer
 * @param fb Framebuffer handle.
 * @param format Optional pointer to receive the pixel format of the
 * framebuffer. Left unmodified when handle is invalid.
 * @return Size structure containing width and height in pixels, or {0, 0} when
 * handle is invalid.
 */
pix_size_t dev_framebuffer_info(const dev_framebuffer_t *fb,
                                pix_format_t *format);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Clear the framebuffer with a specified color.
 * @ingroup Framebuffer
 * @param fb Framebuffer handle.
 * @param color Color value to fill the framebuffer with.
 */
void dev_framebuffer_clear(const dev_framebuffer_t *fb,
                           const pix_color_t color);

/** @} */

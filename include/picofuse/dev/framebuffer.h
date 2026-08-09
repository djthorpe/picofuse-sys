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
 * @param frame Optional pointer to receive the framebuffer's frame
 * descriptor. On success, set to a pointer (valid for the lifetime of the
 * returned handle) with its ctx and lock/unlock/clear/set/copy methods
 * bound. Set to NULL on failure.
 * @return Framebuffer handle or NULL on failure.
 * @details Use the frame's own methods to interact with the device, e.g.
 * `frame->lock(frame)` before writing and `frame->unlock(frame)` after, or
 * `frame->clear(frame, color, op)` to fill it. Locking blocks, on a
 * best-effort basis, until the next vertical blanking interval before
 * returning, so that writes made before unlocking land during blanking
 * rather than tearing a frame already being scanned out. Callers are
 * expected to manage any back-buffering themselves; locking only
 * synchronizes access to the live framebuffer memory. When the underlying
 * driver does not support vsync notification, locking returns immediately
 * without blocking.
 */
dev_framebuffer_t *dev_framebuffer_init(const char *device,
                                        pix_frame_t **frame);

/**
 * @brief Deinitialize a Linux framebuffer.
 * @ingroup Framebuffer
 * @param fb Framebuffer handle.
 */
void dev_framebuffer_deinit(dev_framebuffer_t *fb);

/** @} */

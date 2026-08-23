/**
 * @file jpeg.h
 * @brief JPEG decoding into an in-memory pixel bitmap.
 * @defgroup Jpeg Jpeg
 * @ingroup Pixel
 */
#pragma once
#include "types.h"

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @name Lifecycle
 * @{ */

/**
 * @brief Decode a JPEG image into a newly allocated in-memory bitmap.
 * @ingroup Jpeg
 * @param data Pointer to the encoded JPEG image data.
 * @param size Size of @ref data, in bytes.
 * @return Pointer to a decoded bitmap, or NULL on failure. Use
 * @ref pix_frame_t.copy to blit it onto a frame. The bitmap and its pixel
 * memory are a single allocation; release both with a single call to
 * `sys_free()`.
 * @details Safe to call concurrently from multiple threads/cores: decoding
 * is internally serialized with a mutex, since the underlying decoder keeps
 * its state in a single global.
 */
pix_bitmap_t *pix_jpeg_init(const void *data, size_t size);

/** @} */

/**
 * @file types.h
 * @brief Common pixel types and structures.
 * @ingroup Pixel
 *
 * Shared type definitions used across the pixel library.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Point structure representing X,Y coordinates.
 * @ingroup Pixel
 */
typedef struct {
  int16_t x; ///< X coordinate
  int16_t y; ///< Y coordinate
} pix_point_t;

/**
 * @brief Size structure representing width and height dimensions.
 * @ingroup Pixel
 */
typedef struct {
  uint16_t w; ///< Width in pixels
  uint16_t h; ///< Height in pixels
} pix_size_t;

/**
 * @brief Pixel format enumeration defining color depth and layout.
 * @ingroup Pixel
 */
typedef enum {
  PIX_FMT_RGBA32, ///< 32-bit RGBA format with alpha channel
  PIX_FMT_RGB888, ///< 24-bit RGB format without alpha channel
  PIX_FMT_RGB565, ///< 16-bit RGB format without alpha channel
  PIX_FMT_MONO,   ///< Monochrome format (1-bit per pixel)
} pix_format_t;

/**
 * @brief Pixel operation types for drawing operations.
 * @ingroup Pixel
 */
typedef enum {
  PIX_SET ///< Set pixel operation
} pix_op_t;

/**
 * @brief Color value type for pixel operations.
 * @ingroup Pixel
 * @details Represents a color value, typically in RGBA format depending on the
 * pixel format.
 */
typedef uint32_t pix_color_t;

/**
 * @brief Plain in-memory pixel bitmap descriptor.
 * @ingroup Pixel
 * @details Describes a block of raw pixel memory with no backing device.
 * Unlike @ref pix_frame_t, a bitmap has no ctx and no lock/unlock/clear/
 * set/copy methods; callers read and write @ref data directly.
 */
typedef struct {
  void *data;       ///< Pointer to bitmap memory.
  pix_size_t size;  ///< Bitmap dimensions in pixels.
  size_t stride;    ///< Byte pitch between adjacent major-axis elements.
  pix_format_t fmt; ///< Pixel format used by @ref data.
} pix_bitmap_t;

/**
 * @brief Generic pixel framebuffer descriptor.
 * @ingroup Pixel
 * @details When backed by a device (for example a Linux framebuffer or an
 * e-ink panel), @ref ctx and the method pointers provide a common
 * lock/unlock/clear/set/copy interface over that backend, so drawing code
 * can target any conforming device without depending on its concrete type.
 * Each method receives the frame itself as its first argument; @ref ctx
 * carries whatever backend-specific handle the implementation needs (e.g.
 * a device handle) to reach that backend. Frames describing plain
 * in-memory pixel data (with no backing device) leave @ref ctx and the
 * method pointers NULL.
 */
typedef struct pix_frame_t pix_frame_t;

struct pix_frame_t {
  pix_size_t size;  ///< Frame dimensions in pixels.
  size_t stride;    ///< Byte pitch between adjacent major-axis elements.
  pix_format_t fmt; ///< Pixel format used by the frame's backing memory.
  void *ctx;        ///< Backend-specific context.

  /**
   * @brief Lock the frame's backing device for direct writing.
   * @param frame The frame to lock.
   * @return true if the device was locked, false if it could not be locked.
   */
  bool (*lock)(pix_frame_t *frame);

  /**
   * @brief Unlock a frame previously locked with @ref lock.
   * @param frame The frame to unlock.
   */
  void (*unlock)(pix_frame_t *frame);

  /**
   * @brief Fill the entire frame with a single color.
   * @param frame The frame to fill.
   * @param color Color value to fill the frame with.
   * @param op Operation used to combine @ref color with existing pixels.
   */
  void (*clear)(pix_frame_t *frame, pix_color_t color, pix_op_t op);

  /**
   * @brief Fill a rectangular region of the frame with a single color.
   * @param frame The frame to fill.
   * @param color Color value to fill the region with.
   * @param origin Origin coordinate of the region on @ref frame.
   * @param size Region size, in pixels. A size where both dimensions are 0
   * or 1 (i.e. {0,0}, {1,0}, {0,1} or {1,1}) sets a single pixel at
   * @ref origin.
   * @param op Operation used to combine @ref color with existing pixels.
   */
  void (*set)(pix_frame_t *frame, pix_color_t color, pix_point_t origin,
              pix_size_t size, pix_op_t op);

  /**
   * @brief Copy a source frame onto this frame.
   * @param frame The destination frame.
   * @param src Source frame to copy from.
   * @param origin Destination coordinate on @ref frame.
   * @param size Region of @ref src to copy, in pixels.
   * @param op Operation used to combine @ref src with existing pixels.
   */
  void (*copy)(pix_frame_t *frame, const pix_frame_t *src, pix_point_t origin,
               pix_size_t size, pix_op_t op);
};

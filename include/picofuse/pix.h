/**
 * @file pix.h
 * @brief Aggregates pixel and rendering interfaces.
 * @defgroup Pixel Pixel Library
 * @ingroup Picofuse
 */
#pragma once
#include "pix/color.h"
#include "pix/font.h"
#include "pix/jpeg.h"
#include "pix/types.h"

/**
 * @brief Initializes the pixel library on startup.
 * @ingroup Pixel
 * @details Must be called once, before any other `pix_*` function. Not
 * safe to call concurrently with itself or with other `pix_*` calls.
 */
void pix_init(void);

/**
 * @brief Cleans up the pixel library on shutdown.
 * @ingroup Pixel
 */
void pix_deinit(void);

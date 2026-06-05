/**
 * @file flash.h
 * @brief Flash storage helpers.
 * @defgroup Flash Flash
 * @ingroup Block
 */
#pragma once
#include "block.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Initialize a flash-backed block region.
 * @ingroup Flash
 *
 * Allocates a block region from the free flash area. Requested size is
 * rounded down to the flash erase-size granularity.
 *
 * @param size_bytes Requested region size in bytes.
 * @return A block descriptor on success, or `NULL` on failure.
 */
hw_block_t *hw_block_flash_init(size_t size_bytes);

/**
 * @brief Return the maximum currently available flash block size.
 * @ingroup Flash
 *
 * The returned value is aligned to the flash erase-size granularity.
 *
 * @return Maximum allocatable size in bytes.
 */
size_t hw_block_flash_free(void);

/**
 * @brief Deinitialize a flash-backed block region.
 * @ingroup Flash
 * @param block Block descriptor returned by @ref hw_block_flash_init.
 */
void hw_block_flash_deinit(hw_block_t *block);

/**
 * @brief Query the flash capacity available for block allocation.
 * @ingroup Flash
 *
 * Returns a writable region that starts after the currently linked program
 * image and extends to the end of on-board flash. The returned start offset is
 * aligned to a flash erase-sector boundary.
 *
 * The returned offset is relative to flash address 0 (not an XIP-mapped
 * address).
 *
 * @param out_offset_bytes Optional destination for free-region offset in bytes
 * from flash base.
 * @param out_size_bytes Optional destination for free-region size in bytes.
 * @retval true A non-empty free region is available.
 * @retval false Flash region information is unavailable or no free region
 * remains.
 */
bool hw_block_flash_get_capacity(uintptr_t *out_offset_bytes,
                                 size_t *out_size_bytes);

/** @} */

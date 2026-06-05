/**
 * @file block.h
 * @brief Generic block device interface.
 * @defgroup Block Block
 * @ingroup Hardware
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief Opaque block handle type.
 * @ingroup Block
 * @headerfile block.h hw/hw.h
 */
typedef struct hw_block_t hw_block_t;

///////////////////////////////////////////////////////////////////////////////
// PROPERTIES

/** @name Properties
 * @{ */

/**
 * @brief Return the number of addressable blocks.
 * @ingroup Block
 * @param block Block handle.
 * @return Number of blocks, or `0` when invalid.
 */
size_t hw_block_count(const hw_block_t *block);

/**
 * @brief Return the block size in bytes.
 * @ingroup Block
 * @param block Block handle.
 * @return Block size in bytes, or `0` when invalid.
 */
size_t hw_block_size(const hw_block_t *block);

/** @} */

///////////////////////////////////////////////////////////////////////////////
// METHODS

/** @name Methods
 * @{ */

/**
 * @brief Read one block into @p dst.
 * @ingroup Block
 * @param block Block handle.
 * @param index Block index to read.
 * @param dst Destination buffer of at least @ref hw_block_size bytes.
 * @retval true Read succeeded.
 * @retval false Invalid arguments or backend failure.
 */
bool hw_block_read(const hw_block_t *block, size_t index, void *dst);

/**
 * @brief Erase one block.
 * @ingroup Block
 * @param block Block handle.
 * @param index Block index to erase.
 * @retval true Erase succeeded.
 * @retval false Invalid arguments or backend failure.
 */
bool hw_block_erase(hw_block_t *block, size_t index);

/**
 * @brief Write one block from @p src.
 * @ingroup Block
 * @param block Block handle.
 * @param index Block index to write.
 * @param src Source buffer of at least @ref hw_block_size bytes.
 * @retval true Write succeeded.
 * @retval false Invalid arguments or backend failure.
 */
bool hw_block_write(hw_block_t *block, size_t index, const void *src);

/** @} */

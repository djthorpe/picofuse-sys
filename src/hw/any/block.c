#include <picofuse/hw.h>

#include "block.h"

bool hw_block_valid(const hw_block_t *block) {
  if (block == NULL || block->callbacks == NULL ||
      block->callbacks->valid == NULL) {
    return false;
  }

  return block->callbacks->valid(block, block->userdata);
}

size_t hw_block_count(const hw_block_t *block) {
  if (!hw_block_valid(block) || block->callbacks->count == NULL) {
    return 0u;
  }

  return block->callbacks->count(block, block->userdata);
}

size_t hw_block_size(const hw_block_t *block) {
  if (!hw_block_valid(block) || block->callbacks->size == NULL) {
    return 0u;
  }

  return block->callbacks->size(block, block->userdata);
}

bool hw_block_read(const hw_block_t *block, size_t index, void *dst) {
  if (!hw_block_valid(block) || dst == NULL || block->callbacks->read == NULL) {
    return false;
  }

  return block->callbacks->read(block, block->userdata, index, dst);
}

bool hw_block_erase(hw_block_t *block, size_t index) {
  if (!hw_block_valid(block) || block->callbacks->erase == NULL) {
    return false;
  }

  return block->callbacks->erase(block, block->userdata, index);
}

bool hw_block_write(hw_block_t *block, size_t index, const void *src) {
  if (!hw_block_valid(block) || src == NULL ||
      block->callbacks->write == NULL) {
    return false;
  }

  return block->callbacks->write(block, block->userdata, index, src);
}

void hw_block_deinit(hw_block_t *block) {
  if (block == NULL || block->callbacks == NULL ||
      block->callbacks->deinit == NULL) {
    return;
  }

  block->callbacks->deinit(block, block->userdata);
}

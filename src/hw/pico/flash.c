#include <hardware/flash.h>
#include <hardware/platform_defs.h>
#include <hardware/sync.h>
#include <picofuse/hw.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern char __flash_binary_end;

struct hw_block_t {
  uintptr_t offset_bytes;
  size_t size_bytes;
  size_t erase_size_bytes;
  size_t write_size_bytes;
};

static hw_block_t _hw_flash_block = {0};
static bool _hw_flash_block_allocated = false;

static bool _hw_block_flash_index_valid(const hw_block_t *block, size_t index) {
  if (block == NULL || block->erase_size_bytes == 0u) {
    return false;
  }

  size_t count = block->size_bytes / block->erase_size_bytes;
  if (index >= count) {
    return false;
  }

  return true;
}

bool hw_block_flash_get_capacity(uintptr_t *out_offset_bytes,
                                 size_t *out_size_bytes) {
  const uintptr_t image_end = (uintptr_t)&__flash_binary_end;
  if (image_end < XIP_BASE) {
    return false;
  }

  uintptr_t used_bytes = image_end - XIP_BASE;
  uintptr_t free_offset =
      (used_bytes + (FLASH_SECTOR_SIZE - 1u)) & ~(FLASH_SECTOR_SIZE - 1u);

  if (free_offset >= PICO_FLASH_SIZE_BYTES) {
    return false;
  }

  size_t free_size = (size_t)(PICO_FLASH_SIZE_BYTES - free_offset);
  if (free_size == 0u) {
    return false;
  }

  if (out_offset_bytes != NULL) {
    *out_offset_bytes = free_offset;
  }
  if (out_size_bytes != NULL) {
    *out_size_bytes = free_size;
  }

  return true;
}

size_t hw_block_count(const hw_block_t *block) {
  if (block == NULL || block->erase_size_bytes == 0u) {
    return 0u;
  }

  return block->size_bytes / block->erase_size_bytes;
}

size_t hw_block_size(const hw_block_t *block) {
  if (block == NULL) {
    return 0u;
  }

  return block->erase_size_bytes;
}

bool hw_block_read(const hw_block_t *block, size_t index, void *dst) {
  if (!_hw_block_flash_index_valid(block, index) || dst == NULL) {
    return false;
  }

  size_t block_size = hw_block_size(block);
  uintptr_t byte_offset = block->offset_bytes + index * block_size;
  const uint8_t *src = (const uint8_t *)(XIP_BASE + byte_offset);
  memcpy(dst, src, block_size);
  return true;
}

bool hw_block_erase(hw_block_t *block, size_t index) {
  if (!_hw_block_flash_index_valid(block, index)) {
    return false;
  }

  size_t block_size = hw_block_size(block);
  uintptr_t byte_offset = block->offset_bytes + index * block_size;

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase((uint32_t)byte_offset, block_size);
  restore_interrupts(ints);
  return true;
}

bool hw_block_write(hw_block_t *block, size_t index, const void *src) {
  if (!_hw_block_flash_index_valid(block, index) || src == NULL) {
    return false;
  }

  size_t block_size = hw_block_size(block);
  if ((block_size % FLASH_PAGE_SIZE) != 0u) {
    return false;
  }

  uintptr_t byte_offset = block->offset_bytes + index * block_size;

  uint32_t ints = save_and_disable_interrupts();
  flash_range_program((uint32_t)byte_offset, (const uint8_t *)src, block_size);
  restore_interrupts(ints);
  return true;
}

size_t hw_block_flash_free(void) {
  uintptr_t offset = 0u;
  size_t free_size = 0u;
  if (!hw_block_flash_get_capacity(&offset, &free_size)) {
    return 0u;
  }

  size_t aligned_free = free_size - (free_size % FLASH_SECTOR_SIZE);
  if (!_hw_flash_block_allocated) {
    return aligned_free;
  }

  if (aligned_free <= _hw_flash_block.size_bytes) {
    return 0u;
  }

  return aligned_free - _hw_flash_block.size_bytes;
}

hw_block_t *hw_block_flash_init(size_t size_bytes) {
  if (_hw_flash_block_allocated || size_bytes == 0u) {
    return NULL;
  }

  uintptr_t free_offset = 0u;
  size_t free_size = 0u;
  if (!hw_block_flash_get_capacity(&free_offset, &free_size)) {
    return NULL;
  }

  size_t aligned_size = size_bytes - (size_bytes % FLASH_SECTOR_SIZE);
  if (aligned_size == 0u || aligned_size > free_size) {
    return NULL;
  }

  _hw_flash_block.offset_bytes = free_offset;
  _hw_flash_block.size_bytes = aligned_size;
  _hw_flash_block.write_size_bytes = FLASH_PAGE_SIZE;
  _hw_flash_block.erase_size_bytes = FLASH_SECTOR_SIZE;

  _hw_flash_block_allocated = true;
  return &_hw_flash_block;
}

void hw_block_flash_deinit(hw_block_t *block) {
  if (!_hw_flash_block_allocated || block != &_hw_flash_block) {
    return;
  }

  memset(&_hw_flash_block, 0, sizeof(_hw_flash_block));
  _hw_flash_block_allocated = false;
}

#include <picofuse/hw.h>

struct hw_block_t {};

bool hw_block_flash_get_capacity(uintptr_t *out_offset_bytes,
                                 size_t *out_size_bytes) {
  if (out_offset_bytes != NULL) {
    *out_offset_bytes = (uintptr_t)0;
  }
  if (out_size_bytes != NULL) {
    *out_size_bytes = 0u;
  }
  return false;
}

size_t hw_block_count(const hw_block_t *block) {
  (void)block;
  return 0u;
}

size_t hw_block_size(const hw_block_t *block) {
  (void)block;
  return 0u;
}

bool hw_block_read(const hw_block_t *block, size_t index, void *dst) {
  (void)block;
  (void)index;
  (void)dst;
  return false;
}

bool hw_block_erase(hw_block_t *block, size_t index) {
  (void)block;
  (void)index;
  return false;
}

bool hw_block_write(hw_block_t *block, size_t index, const void *src) {
  (void)block;
  (void)index;
  (void)src;
  return false;
}

size_t hw_block_flash_free(void) { return 0u; }

hw_block_t *hw_block_flash_init(size_t size_bytes) {
  (void)size_bytes;
  return NULL;
}

void hw_block_flash_deinit(hw_block_t *block) { (void)block; }

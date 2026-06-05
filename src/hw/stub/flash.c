#include <picofuse/hw.h>

size_t hw_block_flash_get_capacity(void) { return 0u; }

hw_block_t *hw_block_flash_init(size_t size_bytes) {
  (void)size_bytes;
  return NULL;
}

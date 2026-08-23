#include <picofuse/sys.h>
#include <pico/bootrom.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Fill a memory region with a byte value.
 * @param dest Destination memory region.
 * @param value Byte value to write.
 * @param count Number of bytes to set.
 * @return The original `dest` pointer.
 * @details Delegates to the on-chip boot ROM's `_memset` routine (looked up
 * via ROM_FUNC_MEMSET), which is hand-optimized and costs no flash space,
 * unlike pulling in a full libc memset.
 */
void *sys_memset(void *dest, int value, size_t count) {
  rom_memset_fn func = (rom_memset_fn)rom_func_lookup_inline(ROM_FUNC_MEMSET);
  func((uint8_t *)dest, (uint8_t)value, (uint32_t)count);
  return dest;
}

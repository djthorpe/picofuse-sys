#include <pico/bootrom.h>
#include <picofuse/sys.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Fill a memory region with a byte value.
 * @param dest Destination memory region.
 * @param value Byte value to write.
 * @param count Number of bytes to set.
 * @return The original `dest` pointer.
 * @details On RP2040, this delegates to the on-chip boot ROM's `_memset`
 * routine (looked up via ROM_FUNC_MEMSET), which is hand-optimized and
 * costs no flash space. RP2350 does not expose that ROM function, so the
 * portable libc memset implementation is used instead.
 */
void *sys_memset(void *dest, int value, size_t count) {
#if PICO_RP2040
  rom_memset_fn func = (rom_memset_fn)rom_func_lookup_inline(ROM_FUNC_MEMSET);
  func((uint8_t *)dest, (uint8_t)value, (uint32_t)count);
  return dest;
#else
  return memset(dest, value, count);
#endif
}

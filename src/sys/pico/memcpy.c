#include <pico/bootrom.h>
#include <picofuse/sys.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Copy bytes from one memory region to another.
 * @param dest Destination memory region.
 * @param src Source memory region.
 * @param count Number of bytes to copy.
 * @return The original `dest` pointer.
 * @details On RP2040, non-overlapping (and safely forward-overlapping)
 * copies can be delegated to the on-chip boot ROM's `_memcpy` routine
 * (looked up via ROM_FUNC_MEMCPY), which is hand-optimized and costs no
 * flash space. The boot ROM routine gives no overlap guarantee, so
 * overlapping copies that a forward pass would corrupt fall back to a
 * manual backward byte copy, matching the documented sys_memcpy
 * overlap-safety contract. RP2350 lacks this ROM API, so we fall back to
 * the standard memmove implementation instead.
 */
void *sys_memcpy(void *dest, const void *src, size_t count) {
#if PICO_RP2040
  if (dest == src || count == 0) {
    return dest;
  }

  uintptr_t dst_addr = (uintptr_t)dest;
  uintptr_t src_addr = (uintptr_t)src;

  if (dst_addr < src_addr || dst_addr - src_addr >= count) {
    rom_memcpy_fn func = (rom_memcpy_fn)rom_func_lookup_inline(ROM_FUNC_MEMCPY);
    func((uint8_t *)dest, (const uint8_t *)src, (uint32_t)count);
    return dest;
  }

  unsigned char *d = (unsigned char *)dest + count;
  const unsigned char *s = (const unsigned char *)src + count;
  while (count-- != 0) {
    *--d = *--s;
  }

  return dest;
#else
  return memmove(dest, src, count);
#endif
}

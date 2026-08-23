#include <picofuse/sys.h>
#include <pico/bootrom.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Copy bytes from one memory region to another.
 * @param dest Destination memory region.
 * @param src Source memory region.
 * @param count Number of bytes to copy.
 * @return The original `dest` pointer.
 * @details Non-overlapping (and safely forward-overlapping) copies are
 * delegated to the on-chip boot ROM's `_memcpy` routine (looked up via
 * ROM_FUNC_MEMCPY), which is hand-optimized and costs no flash space,
 * unlike pulling in a full libc memcpy. The boot ROM routine gives no
 * overlap guarantee (like libc memcpy), so overlapping copies that a
 * forward pass would corrupt fall back to a manual backward byte copy,
 * matching the documented sys_memcpy overlap-safety contract.
 */
void *sys_memcpy(void *dest, const void *src, size_t count) {
  if (dest == src || count == 0) {
    return dest;
  }

  uintptr_t dst_addr = (uintptr_t)dest;
  uintptr_t src_addr = (uintptr_t)src;

  if (dst_addr < src_addr || dst_addr - src_addr >= count) {
    rom_memcpy_fn func =
        (rom_memcpy_fn)rom_func_lookup_inline(ROM_FUNC_MEMCPY);
    func((uint8_t *)dest, (const uint8_t *)src, (uint32_t)count);
    return dest;
  }

  unsigned char *d = (unsigned char *)dest + count;
  const unsigned char *s = (const unsigned char *)src + count;
  while (count-- != 0) {
    *--d = *--s;
  }

  return dest;
}

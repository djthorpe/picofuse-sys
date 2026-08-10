#include <picofuse/sys.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Copy bytes from one memory region to another.
 * @param dest Destination memory region.
 * @param src Source memory region.
 * @param count Number of bytes to copy.
 * @return The original `dest` pointer.
 * @details Forwards to the platform's libc, which is always available and
 * optimized on Darwin. Uses memmove rather than memcpy so overlapping
 * regions remain handled safely, matching the documented sys_memcpy
 * contract.
 */
void *sys_memcpy(void *dest, const void *src, size_t count) {
  return memmove(dest, src, count);
}

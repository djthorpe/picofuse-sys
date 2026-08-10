#include <picofuse/sys.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Fill a memory region with a byte value.
 * @param dest Destination memory region.
 * @param value Byte value to write.
 * @param count Number of bytes to set.
 * @return The original `dest` pointer.
 * @details Forwards to the platform's libc, which is always available and
 * optimized on Linux.
 */
void *sys_memset(void *dest, int value, size_t count) {
  return memset(dest, value, count);
}

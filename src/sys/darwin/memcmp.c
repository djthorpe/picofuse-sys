#include <picofuse/sys.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Compare two memory regions byte by byte.
 * @param lhs First memory region.
 * @param rhs Second memory region.
 * @param count Number of bytes to compare.
 * @return Negative, zero, or positive depending on the first differing byte.
 * @details Forwards to the platform's libc, which is always available and
 * optimized on Darwin.
 */
int sys_memcmp(const void *lhs, const void *rhs, size_t count) {
  return memcmp(lhs, rhs, count);
}

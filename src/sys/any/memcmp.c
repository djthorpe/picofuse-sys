#include <picofuse/sys.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Compare two memory regions byte by byte.
 * @param lhs First memory region.
 * @param rhs Second memory region.
 * @param count Number of bytes to compare.
 * @return Negative, zero, or positive depending on the first differing byte.
 * @details Portable byte-loop fallback used on platforms with no faster
 * platform-specific override (see e.g. sys/linux/memcmp.c). There is no
 * boot ROM memcmp on Pico, so this is also what Pico uses.
 */
int sys_memcmp(const void *lhs, const void *rhs, size_t count) {
  const unsigned char *left = lhs;
  const unsigned char *right = rhs;
  while (count--) {
    int diff = (int)*left++ - (int)*right++;
    if (diff != 0) {
      return diff;
    }
  }
  return 0;
}

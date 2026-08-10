#include <picofuse/sys.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Measure the length of a null-terminated string.
 * @param str String to measure.
 * @return Number of characters before the terminating null byte.
 * @details Forwards to the platform's libc, which is always available and
 * optimized on POSIX platforms.
 */
size_t sys_strlen(const char *str) { return strlen(str); }

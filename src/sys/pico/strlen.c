#include <picofuse/sys.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/**
 * @brief Measure the length of a null-terminated string.
 * @param str String to measure.
 * @return Number of characters before the terminating null byte.
 * @details There is no boot ROM strlen, but the newlib strlen linked in via
 * the Pico SDK's C library interface is still faster than a naive byte
 * loop, so this forwards to it instead of duplicating one.
 */
size_t sys_strlen(const char *str) { return strlen(str); }

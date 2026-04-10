#include <picofuse/sys.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC API

void *sys_memset(void *dest, int value, size_t count) {
  unsigned char *ptr = dest;
  while (count--) {
    *ptr++ = (unsigned char)value;
  }
  return dest;
}

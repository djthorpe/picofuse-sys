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

void *sys_memcpy(void *dest, const void *src, size_t count) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  while (count--) {
    *d++ = *s++;
  }
  return dest;
}

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

size_t sys_strlen(const char *str) {
  const char *s = str;
  while (*s) {
    s++;
  }
  return s - str;
}
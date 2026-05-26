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

size_t sys_strlen(const char *str) {
  const char *s = str;
  while (*s) {
    s++;
  }
  return s - str;
}
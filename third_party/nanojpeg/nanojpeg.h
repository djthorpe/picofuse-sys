#pragma once
#define NJ_USE_LIBC 0
#define NJ_USE_WIN32 0
#define NJ_CHROMA_FILTER 1
#undef _NJ_EXAMPLE_PROGRAM
#include <stddef.h>
#include "nanojpeg.c"
#include <picofuse/sys.h>

inline void *njAllocMem(int size) { return sys_malloc(size); }

inline void njFreeMem(void *ptr) { sys_free(ptr); }

inline void njFillMem(void *ptr, unsigned char value, int size) {
  sys_memset(ptr, value, size);
}

inline void njCopyMem(void *dest, const void *src, int size) {
  sys_memcpy(dest, src, size);
}

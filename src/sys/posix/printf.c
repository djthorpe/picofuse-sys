#include <picofuse/sys.h>
#include <stdarg.h>
#include <stdio.h>

size_t sys_printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = vprintf(format, args);
  va_end(args);
  return (result < 0) ? 0 : (size_t)result;
}

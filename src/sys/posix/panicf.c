#include <picofuse/sys.h>
#include <stdarg.h>
#include <stdio.h>

void sys_panicf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  sys_halt();
}

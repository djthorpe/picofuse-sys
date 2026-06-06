#include <picofuse/sys.h>

#ifndef NDEBUG

#include <stdarg.h>

static sys_mutex_t *_sys_debugf_mutex = NULL;

void _sys_debugf_impl(const char *format, ...) {
  if (format == NULL) {
    return;
  }
  if (_sys_debugf_mutex == NULL) {
    _sys_debugf_mutex = sys_mutex_init();
  }
  if (sys_mutex_lock(_sys_debugf_mutex)) {
    va_list args;
    va_start(args, format);
    sys_printf("[DEBUG] ");
    sys_vprintf(format, args);
    sys_printf("\n");
    va_end(args);
    sys_mutex_unlock(_sys_debugf_mutex);
  }
}

#endif

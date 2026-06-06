#include <picofuse/sys.h>

#ifndef NDEBUG

#include <stdarg.h>

static sys_mutex_t *_sys_debugf_mutex = NULL;

void _sys_debugf_module_init(void) {
  _sys_debugf_mutex = sys_mutex_init();
  sys_assert(_sys_debugf_mutex != NULL);
}

void _sys_debugf_module_exit(void) {
  if (_sys_debugf_mutex == NULL) {
    return;
  }
  sys_mutex_deinit(_sys_debugf_mutex);
  _sys_debugf_mutex = NULL;
}

void _sys_debugf_impl(const char *format, ...) {
  sys_assert(format != NULL);
  sys_assert(_sys_debugf_mutex != NULL);
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

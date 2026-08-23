#include "private.h"
#include <picofuse/pix.h>

static sys_mutex_t *_mutex = NULL;

void pix_init(void) { _mutex = sys_mutex_init(); }

void pix_deinit(void) {
  if (_mutex != NULL) {
    sys_mutex_deinit(_mutex);
    _mutex = NULL;
  }
}

sys_mutex_t *_pix_mutex(void) { return _mutex; }

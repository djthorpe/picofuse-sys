#pragma once
#include <picofuse/sys.h>

// Mutex created by pix_init() and released by pix_deinit(), guarding
// decoders that keep process-global state (e.g. nanojpeg's nj_context_t).
extern sys_mutex_t *_pix_mutex(void);

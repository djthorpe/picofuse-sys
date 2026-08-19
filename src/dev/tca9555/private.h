#pragma once

#include <picofuse/dev.h>
#include <stdbool.h>

bool _dev_tca9555_set_callback(dev_tca9555_t *tca9555,
                               dev_tca9555_callback_t callback,
                               void *callback_userdata);

bool _dev_tca9555_has_callback(const dev_tca9555_t *tca9555);

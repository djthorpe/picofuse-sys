#include <pico/critical_section.h>

static critical_section_t _hw_lock;

void _hw_lock_module_init(void) { critical_section_init(&_hw_lock); }

void _hw_lock_module_exit(void) { critical_section_deinit(&_hw_lock); }

void _hw_lock_enter(void) { critical_section_enter_blocking(&_hw_lock); }

void _hw_lock_exit(void) { critical_section_exit(&_hw_lock); }

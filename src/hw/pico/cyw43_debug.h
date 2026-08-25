#pragma once

// Force-included ahead of every translation unit in this target (see
// src/hw/pico/CMakeLists.txt) - including cyw43-driver's own sources, since
// cyw43_driver/cyw43_driver_picow are CMake INTERFACE libraries whose .c
// files get compiled as part of whichever target links them, i.e. this one.
//
// cyw43_config.h/cyw43_configport.h only supply their own CYW43_PRINTF
// (raw printf(), CYW43_DEBUG/_INFO/_WARN all route through it) when it isn't
// already defined (`#ifndef CYW43_PRINTF`). Defining it here first replaces
// that with this project's own tagged sys_debugf(), since libc printf() is
// disabled project-wide and panics if called (see
// pico_set_printf_implementation(NAME none)).
// Force-including this header runs it before the compiled .c file's own
// includes, so unlike a normal include site it can't rely on something
// earlier in the file having already pulled in <stddef.h> for size_t (which
// picofuse/sys/printf.h, reached via debugf.h below, needs but does not
// include itself).
#include <stddef.h>
#include <picofuse/sys/debugf.h>

#define CYW43_PRINTF(...) sys_debugf("wifi", __VA_ARGS__)

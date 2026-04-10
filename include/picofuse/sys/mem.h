/**
 * @file sys/mem.h
 * @brief Defines memory-related functions and macros.
 * @details This file provides functions and macros for memory management
 * and manipulation.
 */

#pragma once
#include <stddef.h>

void *sys_memset(void *dest, int value, size_t count);

#pragma once

#include <picofuse/sys/arena.h>
#include <stdbool.h>
#include <stddef.h>

extern bool _sys_mem_init(size_t capacity, void *(*malloc_fn)(size_t),
                          void (*free_fn)(void *));
extern void _sys_mem_module_exit(void);
extern sys_mem_arena_t *_sys_mem_arena_prev(sys_mem_arena_t *arena,
                                            sys_mem_arena_stats_t *stats);
extern size_t _sys_mem_arena_alloc_size(sys_mem_arena_t *arena, void *ptr);
#pragma once

#include <picofuse/hw/block.h>
#include <stdbool.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef struct hw_block_callbacks_t {
  bool (*valid)(const hw_block_t *block, void *userdata);
  size_t (*count)(const hw_block_t *block, void *userdata);
  size_t (*size)(const hw_block_t *block, void *userdata);
  bool (*read)(const hw_block_t *block, void *userdata, size_t index,
               void *dst);
  bool (*erase)(hw_block_t *block, void *userdata, size_t index);
  bool (*write)(hw_block_t *block, void *userdata, size_t index,
                const void *src);
  void (*deinit)(hw_block_t *block, void *userdata);
} hw_block_callbacks_t;

struct hw_block_t {
  const hw_block_callbacks_t *callbacks;
  void *userdata;
};

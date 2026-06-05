#include <hardware/flash.h>
#include <hardware/platform_defs.h>
#include <hardware/sync.h>
#include <pico/critical_section.h>
#include <pico/multicore.h>
#include <picofuse/hw.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../any/block.h"

extern char __flash_binary_end;

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef struct {
  uintptr_t offset_bytes;
  size_t size_bytes;
  size_t erase_size_bytes;
  size_t write_size_bytes;
} hw_flash_block_state_t;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hw_flash_block_valid_cb(const hw_block_t *block, void *userdata);
static size_t _hw_flash_block_count_cb(const hw_block_t *block, void *userdata);
static size_t _hw_flash_block_size_cb(const hw_block_t *block, void *userdata);
static bool _hw_flash_block_read_cb(const hw_block_t *block, void *userdata,
                                    size_t index, void *dst);
static bool _hw_flash_block_erase_cb(hw_block_t *block, void *userdata,
                                     size_t index);
static bool _hw_flash_block_write_cb(hw_block_t *block, void *userdata,
                                     size_t index, const void *src);
static void _hw_flash_block_deinit_cb(hw_block_t *block, void *userdata);

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static hw_flash_block_state_t _hw_flash_state = {0};
static critical_section_t _hw_flash_lock;

static const hw_block_callbacks_t _hw_flash_block_callbacks = {
    .valid = _hw_flash_block_valid_cb,
    .count = _hw_flash_block_count_cb,
    .size = _hw_flash_block_size_cb,
    .read = _hw_flash_block_read_cb,
    .erase = _hw_flash_block_erase_cb,
    .write = _hw_flash_block_write_cb,
    .deinit = _hw_flash_block_deinit_cb,
};

static hw_block_t _hw_flash_block = {
    .callbacks = &_hw_flash_block_callbacks,
    .userdata = &_hw_flash_state,
};

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

static bool
_hw_block_flash_allocated_unlocked(const hw_flash_block_state_t *state) {
  return state != NULL && state->size_bytes > 0u &&
         state->erase_size_bytes > 0u && state->write_size_bytes > 0u;
}

static bool
_hw_block_flash_valid_unlocked(const hw_block_t *block,
                               const hw_flash_block_state_t *state) {
  return _hw_block_flash_allocated_unlocked(state) &&
         block == &_hw_flash_block && state == &_hw_flash_state;
}

static bool
_hw_block_flash_index_valid_unlocked(const hw_block_t *block,
                                     const hw_flash_block_state_t *state,
                                     size_t index) {
  if (!_hw_block_flash_valid_unlocked(block, state)) {
    return false;
  }

  size_t count = state->size_bytes / state->erase_size_bytes;
  return index < count;
}

static bool _hw_block_flash_get_capacity_region(uintptr_t *out_offset_bytes,
                                                size_t *out_size_bytes) {
  const uintptr_t image_end = (uintptr_t)&__flash_binary_end;
  if (image_end < XIP_BASE) {
    return false;
  }

  uintptr_t used_bytes = image_end - XIP_BASE;
  uintptr_t free_offset =
      (used_bytes + (FLASH_SECTOR_SIZE - 1u)) & ~(FLASH_SECTOR_SIZE - 1u);

  if (free_offset >= PICO_FLASH_SIZE_BYTES) {
    return false;
  }

  size_t free_size = (size_t)(PICO_FLASH_SIZE_BYTES - free_offset);
  if (free_size == 0u) {
    return false;
  }

  if (out_offset_bytes != NULL) {
    *out_offset_bytes = free_offset;
  }
  if (out_size_bytes != NULL) {
    *out_size_bytes = free_size;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

void _hw_flash_module_init(void) { critical_section_init(&_hw_flash_lock); }

void _hw_flash_module_exit(void) {}

hw_block_t *hw_block_flash_init(size_t size_bytes) {
  critical_section_enter_blocking(&_hw_flash_lock);

  if (_hw_block_flash_allocated_unlocked(&_hw_flash_state) ||
      size_bytes == 0u) {
    critical_section_exit(&_hw_flash_lock);
    return NULL;
  }

  uintptr_t free_offset = 0u;
  size_t free_size = 0u;
  if (!_hw_block_flash_get_capacity_region(&free_offset, &free_size)) {
    critical_section_exit(&_hw_flash_lock);
    return NULL;
  }

  size_t aligned_size = size_bytes - (size_bytes % FLASH_SECTOR_SIZE);
  size_t aligned_free = free_size - (free_size % FLASH_SECTOR_SIZE);
  if (aligned_size == 0u || aligned_size > aligned_free) {
    critical_section_exit(&_hw_flash_lock);
    return NULL;
  }

  // Allocate from the end of free flash so firmware growth from lower
  // addresses is less likely to invalidate the reserved region.
  _hw_flash_state.offset_bytes = free_offset + (aligned_free - aligned_size);
  _hw_flash_state.size_bytes = aligned_size;
  _hw_flash_state.write_size_bytes = FLASH_PAGE_SIZE;
  _hw_flash_state.erase_size_bytes = FLASH_SECTOR_SIZE;

  critical_section_exit(&_hw_flash_lock);
  return &_hw_flash_block;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

size_t hw_block_flash_get_capacity(void) {
  critical_section_enter_blocking(&_hw_flash_lock);

  uintptr_t offset = 0u;
  size_t free_size = 0u;
  if (!_hw_block_flash_get_capacity_region(&offset, &free_size)) {
    critical_section_exit(&_hw_flash_lock);
    return 0u;
  }

  size_t aligned_free = free_size - (free_size % FLASH_SECTOR_SIZE);
  if (!_hw_block_flash_allocated_unlocked(&_hw_flash_state)) {
    critical_section_exit(&_hw_flash_lock);
    return aligned_free;
  }

  if (aligned_free <= _hw_flash_state.size_bytes) {
    critical_section_exit(&_hw_flash_lock);
    return 0u;
  }

  size_t remaining = aligned_free - _hw_flash_state.size_bytes;
  critical_section_exit(&_hw_flash_lock);
  return remaining;
}

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

static bool _hw_flash_block_valid_cb(const hw_block_t *block, void *userdata) {
  critical_section_enter_blocking(&_hw_flash_lock);
  bool valid = _hw_block_flash_valid_unlocked(
      block, (const hw_flash_block_state_t *)userdata);
  critical_section_exit(&_hw_flash_lock);
  return valid;
}

static size_t _hw_flash_block_count_cb(const hw_block_t *block,
                                       void *userdata) {
  critical_section_enter_blocking(&_hw_flash_lock);

  const hw_flash_block_state_t *state =
      (const hw_flash_block_state_t *)userdata;
  if (!_hw_block_flash_valid_unlocked(block, state)) {
    critical_section_exit(&_hw_flash_lock);
    return 0u;
  }

  size_t count = state->size_bytes / state->erase_size_bytes;
  critical_section_exit(&_hw_flash_lock);
  return count;
}

static size_t _hw_flash_block_size_cb(const hw_block_t *block, void *userdata) {
  critical_section_enter_blocking(&_hw_flash_lock);

  const hw_flash_block_state_t *state =
      (const hw_flash_block_state_t *)userdata;
  if (!_hw_block_flash_valid_unlocked(block, state)) {
    critical_section_exit(&_hw_flash_lock);
    return 0u;
  }

  size_t size = state->erase_size_bytes;
  critical_section_exit(&_hw_flash_lock);
  return size;
}

static bool _hw_flash_block_read_cb(const hw_block_t *block, void *userdata,
                                    size_t index, void *dst) {
  if (dst == NULL) {
    return false;
  }

  critical_section_enter_blocking(&_hw_flash_lock);

  const hw_flash_block_state_t *state =
      (const hw_flash_block_state_t *)userdata;
  if (!_hw_block_flash_index_valid_unlocked(block, state, index)) {
    critical_section_exit(&_hw_flash_lock);
    return false;
  }

  size_t block_size = state->erase_size_bytes;
  uintptr_t byte_offset = state->offset_bytes + index * block_size;
  const uint8_t *flash_src = (const uint8_t *)(XIP_BASE + byte_offset);
  memcpy(dst, flash_src, block_size);

  critical_section_exit(&_hw_flash_lock);
  return true;
}

static bool _hw_flash_block_erase_cb(hw_block_t *block, void *userdata,
                                     size_t index) {
  critical_section_enter_blocking(&_hw_flash_lock);

  hw_flash_block_state_t *state = (hw_flash_block_state_t *)userdata;
  if (!_hw_block_flash_index_valid_unlocked(block, state, index)) {
    critical_section_exit(&_hw_flash_lock);
    return false;
  }

  size_t block_size = state->erase_size_bytes;
  uintptr_t byte_offset = state->offset_bytes + index * block_size;

  // Flash erase/program requires no core executes from XIP flash while active.
  multicore_lockout_start_blocking();
  uint32_t irq_state = save_and_disable_interrupts();
  flash_range_erase((uint32_t)byte_offset, block_size);
  restore_interrupts(irq_state);
  multicore_lockout_end_blocking();
  critical_section_exit(&_hw_flash_lock);

  return true;
}

static bool _hw_flash_block_write_cb(hw_block_t *block, void *userdata,
                                     size_t index, const void *src) {
  if (src == NULL) {
    return false;
  }

  critical_section_enter_blocking(&_hw_flash_lock);

  hw_flash_block_state_t *state = (hw_flash_block_state_t *)userdata;
  if (!_hw_block_flash_index_valid_unlocked(block, state, index)) {
    critical_section_exit(&_hw_flash_lock);
    return false;
  }

  size_t block_size = state->erase_size_bytes;
  if ((block_size % FLASH_PAGE_SIZE) != 0u) {
    critical_section_exit(&_hw_flash_lock);
    return false;
  }

  uintptr_t byte_offset = state->offset_bytes + index * block_size;

  // Flash erase/program requires no core executes from XIP flash while active.
  multicore_lockout_start_blocking();
  uint32_t irq_state = save_and_disable_interrupts();
  flash_range_program((uint32_t)byte_offset, (const uint8_t *)src, block_size);
  restore_interrupts(irq_state);
  multicore_lockout_end_blocking();
  critical_section_exit(&_hw_flash_lock);

  return true;
}

static void _hw_flash_block_deinit_cb(hw_block_t *block, void *userdata) {
  critical_section_enter_blocking(&_hw_flash_lock);

  hw_flash_block_state_t *state = (hw_flash_block_state_t *)userdata;
  if (!_hw_block_flash_valid_unlocked(block, state)) {
    critical_section_exit(&_hw_flash_lock);
    return;
  }

  memset(state, 0, sizeof(*state));
  critical_section_exit(&_hw_flash_lock);
}

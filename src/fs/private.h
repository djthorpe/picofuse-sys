#pragma once
#include <picofuse/fs.h>
#include <stddef.h>

// Size of the inline backend-private storage block embedded in every
// fs_volume_t (see `ctx` below). Must be large enough to hold the largest
// backend context in use (see the _Static_assert in littlefs.h).
#ifndef FS_VOLUME_CTX_SIZE
#define FS_VOLUME_CTX_SIZE 512u
#endif

// Per-volume method table. Every volume-creating route (littlefs-backed
// RAM/file/flash, POSIX directory passthrough, etc.) provides one of these,
// so the shared fs_vol_*()/fs_file_*() wrappers can dispatch correctly
// regardless of which route created the volume. A file has no method table
// of its own - it dispatches through file->volume->ops, since a file's
// backend is always whichever backend owns the volume it was opened from.
// `deinit` is required; the rest may be NULL if a backend doesn't support
// that operation (the wrapper then reports failure).
struct fs_volume_ops_t {
  void (*deinit)(fs_volume_t *volume);
  size_t (*size)(fs_volume_t *volume, size_t *free);
  bool (*readdir)(fs_volume_t *volume, const char *path,
                  fs_file_t *iterator);
  fs_file_t (*stat)(fs_volume_t *volume, const char *path);
  bool (*mkdir)(fs_volume_t *volume, const char *path);
  bool (*remove)(fs_volume_t *volume, const char *path);
  bool (*move)(fs_volume_t *volume, const char *old_path,
              const char *new_path);

  fs_file_t (*file_create)(fs_volume_t *volume, const char *path);
  fs_file_t (*file_open)(fs_volume_t *volume, const char *path, bool write);
  bool (*file_close)(fs_file_t *file);
  size_t (*file_read)(fs_file_t *file, void *buffer, size_t size);
  size_t (*file_write)(fs_file_t *file, const void *buffer, size_t size);
  bool (*file_seek)(fs_file_t *file, size_t offset);
};

// Concrete backing type for the opaque fs_volume_t handle. Every
// volume-creating route initializes one of these in place and sets `ops` to
// its own method table, so the shared fs_vol_*() wrappers can dispatch
// correctly regardless of which route created the volume.
//
// `ctx` is a fixed, aligned block of backend-private storage rather than a
// heap allocation, so routes cast it in place (e.g.
// `(fs_lfs_ctx_t *)volume->ctx`) instead of allocating their own context.
struct fs_volume_t {
  const struct fs_volume_ops_t *ops; // Backend method table; NULL == free
                                      // slot (see ../any/pool.c).
  _Alignas(max_align_t) unsigned char ctx[FS_VOLUME_CTX_SIZE]; // Backend
                                                                // storage.
};

// FS_VOLUME_MAX (see picofuse/fs/volume.h) sizes the static volume pool
// backing fs_volume_t instances (see ../any/pool.c) rather than the heap.

// Claim a free slot from the static volume pool, or NULL if none remain.
extern fs_volume_t *_fs_vol_alloc(void);

// Return a slot to the static volume pool. Safe to call with NULL.
extern void _fs_vol_free(fs_volume_t *volume);

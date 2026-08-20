#pragma once
#include "../private.h"
#include "lfs.h"
#include <picofuse/fs/defs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

// Basic parameters for the in-memory block device
#define LFS_BLOCK_SIZE 512u    // Erase/program block size
#define LFS_READ_SIZE 16u      // Minimum read size
#define LFS_PROG_SIZE 16u      // Minimum program size
#define LFS_CACHE_SIZE 64u     // Cache (must be >= read/prog size)
#define LFS_LOOKAHEAD_SIZE 16u // Lookahead buffer size

// Backend-private data for a littlefs-backed volume, hung off
// fs_volume_t.ctx (see ../private.h).
typedef struct {
  lfs_t lfs;             // LittleFS instance
  struct lfs_config cfg; // Configuration
  void *storage;         // In-memory buffer OR FILE* when file-backed
  size_t storage_size;   // Size of allocated storage/file image
  bool file;             // true if file-backed volume
} fs_lfs_ctx_t;

_Static_assert(sizeof(fs_lfs_ctx_t) <= FS_VOLUME_CTX_SIZE,
               "fs_lfs_ctx_t exceeds FS_VOLUME_CTX_SIZE");

///////////////////////////////////////////////////////////////////////////////
// PATH CONFINEMENT (see path.c)
//
// littlefs has no host filesystem underneath to escape into, but its own
// path parser is otherwise permissive (bare "relative", ".", leading ".."
// would all be silently accepted). These mirror ../posix/path.c's stricter,
// realpath()-equivalent contract - require a leading '/', collapse "."/".."
// components, and require every *intermediate* component to already exist
// as a directory - so callers see the same rejection behavior regardless of
// which backend mounted the volume.

// NULL/empty path means the volume root everywhere in this API.
extern const char *_fs_lfs_path(const char *path);

// True if `path` (already resolved/canonical) names the volume root itself.
extern bool _fs_lfs_is_root(const char *path);

// Resolve a volume-relative `path` that must already exist, collapsing "."
// and ".." components and rejecting any attempt to climb above the volume
// root. Every component walked through except the final one must already
// exist as a directory (the final component's existence is the caller's to
// check, e.g. via lfs_stat()).
extern bool _fs_lfs_resolve(fs_lfs_ctx_t *ctx, const char *path,
                            char out[static FS_PATH_MAX + 1]);

// Resolve the (confined, existing) parent directory of a not-yet-existing
// `path`, and append its final component verbatim. The leaf must be a
// single path component; "." and ".." are rejected as leaf names.
extern bool _fs_lfs_resolve_new(fs_lfs_ctx_t *ctx, const char *path,
                                char out[static FS_PATH_MAX + 1]);

// Extract the final path component of an already-resolved, canonical path
// into a caller-provided buffer (size FS_PATH_MAX + 1).
extern void _fs_lfs_basename(const char *resolved,
                             char out[static FS_PATH_MAX + 1]);

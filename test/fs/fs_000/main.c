#include <test.h>

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
#include <stdlib.h>
#include <unistd.h>
#endif

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  char cwd[1024];
  TestAssert(getcwd(cwd, sizeof(cwd)) != NULL, "getcwd should succeed");

  fs_volume_t *volume = fs_vol_init_path(cwd);
  TestAssert(volume != NULL, "fs_vol_init_path(%s) should succeed", cwd);

  fs_vol_deinit(volume);
#else
  fs_volume_t *volume = fs_vol_init_path("/");
  TestAssert(volume == NULL,
             "fs_vol_init_path should be unsupported on this platform");
#endif

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  // A host file-backed volume should mount, creating and formatting a
  // fresh image when the file doesn't already hold one.
  char file_path[] = "/tmp/picofuse_fs_000_file_XXXXXX";
  int file_fd = mkstemp(file_path);
  TestAssert(file_fd >= 0, "mkstemp should create a scratch image file");
  close(file_fd);

  fs_volume_t *file_vol = fs_vol_init_file(file_path, 64 * 1024);
  TestAssert(file_vol != NULL, "fs_vol_init_file should succeed");
  fs_vol_deinit(file_vol);
  unlink(file_path);

  // A NULL/empty path names no file at all and must be rejected outright.
  TestAssert(fs_vol_init_file(NULL, 64 * 1024) == NULL,
             "fs_vol_init_file(NULL, ...) should fail");
  TestAssert(fs_vol_init_file("", 64 * 1024) == NULL,
             "fs_vol_init_file(\"\", ...) should fail");
#else
  TestAssert(fs_vol_init_file("/some/path", 64 * 1024) == NULL,
             "fs_vol_init_file should be unsupported on this platform");
#endif

  // A RAM-backed volume should mount unconditionally on every platform.
  fs_volume_t *mem = fs_vol_init_memory(NULL, 64 * 1024);
  TestAssert(mem != NULL, "fs_vol_init_memory(NULL, 64K) should succeed");
  fs_vol_deinit(mem);

  // size == 0 should round up to at least one block rather than fail.
  fs_volume_t *tiny = fs_vol_init_memory(NULL, 0);
  TestAssert(tiny != NULL, "fs_vol_init_memory(NULL, 0) should round up and "
                           "succeed");
  fs_vol_deinit(tiny);

  // Arena-backed allocation is not implemented yet - any non-NULL arena
  // must be rejected rather than silently falling back to the heap.
  sys_mem_arena_t *arena = sys_mem_arena_init(4096, NULL, sys_malloc, sys_free);
  TestAssert(arena != NULL, "sys_mem_arena_init should succeed");
  TestAssert(fs_vol_init_memory(arena, 4096) == NULL,
             "fs_vol_init_memory with a non-NULL arena should fail (not yet "
             "supported)");
  sys_mem_arena_delete(arena);

  return true;
}

TestMain(test_main)

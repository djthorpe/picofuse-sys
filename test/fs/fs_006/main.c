#include <test.h>

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool create_file(const char *path, const char *content) {
  FILE *f = fopen(path, "w");
  if (f == NULL) {
    return false;
  }
  size_t len = strlen(content);
  bool ok = fwrite(content, 1, len, f) == len;
  fclose(f);
  return ok;
}
#endif

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  char root[] = "/tmp/picofuse_fs_006_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  char path[512];
  sys_sprintf(path, sizeof(path), "%s/file_a.txt", root);
  TestAssert(create_file(path, "AAA"), "creating file_a.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/file_b.txt", root);
  TestAssert(create_file(path, "BBBBB"), "creating file_b.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/file_c.txt", root);
  TestAssert(create_file(path, "C"), "creating file_c.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/dir_empty", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir dir_empty should succeed");

  sys_sprintf(path, sizeof(path), "%s/dir_empty2", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir dir_empty2 should succeed");

  sys_sprintf(path, sizeof(path), "%s/dir_full", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir dir_full should succeed");

  sys_sprintf(path, sizeof(path), "%s/dir_full/inner.txt", root);
  TestAssert(create_file(path, "inner"), "creating inner.txt should succeed");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

  // Same-type overwrite (file -> existing file) succeeds, matching
  // rename(2)/littlefs semantics: the destination is atomically replaced.
  TestAssert(fs_vol_move(volume, "/file_a.txt", "/file_b.txt"),
             "move onto an existing file (same type) should succeed");
  fs_file_t st = fs_vol_stat(volume, "/file_a.txt");
  TestAssert(st.name[0] == '\0', "file_a.txt should be gone after move");
  st = fs_vol_stat(volume, "/file_b.txt");
  TestAssert(st.name[0] != '\0' && !st.dir && st.size == 3,
             "file_b.txt should now hold file_a.txt's content (3 bytes), "
             "got size %zu",
             st.size);

  // Type mismatch (file -> existing directory) is rejected.
  TestAssert(!fs_vol_move(volume, "/file_c.txt", "/dir_empty"),
             "move of a file onto an existing directory should be rejected");
  TestAssert(fs_vol_stat(volume, "/file_c.txt").name[0] != '\0',
             "file_c.txt should still exist after a rejected move");
  TestAssert(fs_vol_stat(volume, "/dir_empty").dir,
             "dir_empty should be untouched after a rejected move");

  // Type mismatch (directory -> existing file) is rejected.
  TestAssert(!fs_vol_move(volume, "/dir_empty", "/file_c.txt"),
             "move of a directory onto an existing file should be rejected");
  TestAssert(fs_vol_stat(volume, "/dir_empty").dir,
             "dir_empty should still exist after a rejected move");
  TestAssert(!fs_vol_stat(volume, "/file_c.txt").dir,
             "file_c.txt should be untouched after a rejected move");

  // Same-type overwrite (directory -> existing EMPTY directory) succeeds.
  TestAssert(fs_vol_move(volume, "/dir_empty", "/dir_empty2"),
             "move onto an existing empty directory should succeed");
  TestAssert(fs_vol_stat(volume, "/dir_empty").name[0] == '\0',
             "dir_empty should be gone after move");
  TestAssert(fs_vol_stat(volume, "/dir_empty2").dir,
             "dir_empty2 should still exist as a directory after move");

  // Directory -> existing NON-EMPTY directory is rejected.
  TestAssert(!fs_vol_move(volume, "/dir_empty2", "/dir_full"),
             "move onto an existing non-empty directory should be rejected");
  TestAssert(fs_vol_stat(volume, "/dir_empty2").dir,
             "dir_empty2 should still exist after a rejected move");
  st = fs_vol_stat(volume, "/dir_full/inner.txt");
  TestAssert(st.name[0] != '\0',
             "dir_full's contents should be untouched after a rejected "
             "move");

  // The volume root may never be overwritten by a move.
  TestAssert(!fs_vol_move(volume, "/file_c.txt", "/"),
             "move onto the volume root should be rejected");
  TestAssert(fs_vol_stat(volume, "/file_c.txt").name[0] != '\0',
             "file_c.txt should still exist after a rejected move");

  // Moving a path onto itself is a well-defined no-op success.
  TestAssert(fs_vol_move(volume, "/file_c.txt", "/file_c.txt"),
             "move of a path onto itself should succeed as a no-op");
  TestAssert(fs_vol_stat(volume, "/file_c.txt").name[0] != '\0',
             "file_c.txt should still exist after a self-move");

  fs_vol_deinit(volume);

  sys_sprintf(path, sizeof(path), "%s/file_b.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/file_c.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/dir_empty2", root);
  rmdir(path);
  sys_sprintf(path, sizeof(path), "%s/dir_full/inner.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/dir_full", root);
  rmdir(path);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

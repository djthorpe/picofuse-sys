#include <test.h>
#include <string.h>

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
#include <stdlib.h>
#include <unistd.h>
#endif

static bool write_file(fs_volume_t *volume, const char *path,
                       const char *content) {
  fs_file_t f = fs_file_create(volume, path);
  if (f.ctx == NULL) {
    return false;
  }
  size_t len = strlen(content);
  bool ok = fs_file_write(&f, content, len) == len;
  return fs_file_close(&f) && ok;
}

static bool cleanup_all(fs_volume_t *volume, const char *path) {
  typedef struct {
    char name[FS_PATH_MAX + 1];
    bool dir;
  } entry_t;
  entry_t entries[32];
  int count = 0;

  fs_file_t it;
  memset(&it, 0, sizeof(it));
  while (fs_vol_readdir(volume, path, &it)) {
    if (count >= 32) {
      return false;
    }
    strcpy(entries[count].name, it.name);
    entries[count].dir = it.dir;
    count++;
  }

  for (int i = 0; i < count; i++) {
    char child[512];
    int n = (strcmp(path, "/") == 0)
                ? sys_sprintf(child, sizeof(child), "/%s", entries[i].name)
                : sys_sprintf(child, sizeof(child), "%s/%s", path,
                              entries[i].name);
    if (n <= 0 || (size_t)n >= sizeof(child)) {
      return false;
    }
    if (entries[i].dir && !cleanup_all(volume, child)) {
      return false;
    }
    if (!fs_vol_remove(volume, child)) {
      return false;
    }
  }
  return true;
}

static bool build_fixture(fs_volume_t *volume) {
  return write_file(volume, "/file_a.txt", "AAA") &&
         write_file(volume, "/file_b.txt", "BBBBB") &&
         write_file(volume, "/file_c.txt", "C") &&
         fs_vol_mkdir(volume, "/dir_empty") &&
         fs_vol_mkdir(volume, "/dir_empty2") &&
         fs_vol_mkdir(volume, "/dir_full") &&
         write_file(volume, "/dir_full/inner.txt", "inner");
}

// Move/rename overwrite semantics match rename(2)/littlefs semantics on
// both backends: same-type targets are atomically replaced, type mismatches
// and non-empty directory targets are rejected, and the volume root can
// never be overwritten.
static bool run_checks(fs_volume_t *volume) {
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

  return true;
}

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  {
    char root[] = "/tmp/picofuse_fs_006_XXXXXX";
    TestAssert(mkdtemp(root) != NULL,
               "mkdtemp should create a scratch directory");

    fs_volume_t *volume = fs_vol_init_path(root);
    TestAssert(volume != NULL, "fs_vol_init_path should succeed");
    TestAssert(build_fixture(volume),
               "fixture setup (path backend) should succeed");
    TestAssert(run_checks(volume), "checks (path backend) should pass");
    TestAssert(cleanup_all(volume, "/"),
               "cleanup (path backend) should succeed");
    fs_vol_deinit(volume);

    TestAssert(rmdir(root) == 0,
               "rmdir of the now-empty scratch directory should succeed");
  }
#endif

  {
    fs_volume_t *volume = fs_vol_init_memory(NULL, 64 * 1024);
    TestAssert(volume != NULL, "fs_vol_init_memory should succeed");
    TestAssert(build_fixture(volume),
               "fixture setup (memory backend) should succeed");
    TestAssert(run_checks(volume), "checks (memory backend) should pass");
    fs_vol_deinit(volume);
  }

  return true;
}

TestMain(test_main)

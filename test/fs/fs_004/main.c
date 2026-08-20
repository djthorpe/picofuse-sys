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

typedef struct {
  int files;
  int dirs;
} walk_stats_t;

// Recursively walks `path` on `volume`, cross-checking every readdir()
// entry against an independent stat() of its full path.
static bool walk(fs_volume_t *volume, const char *path, walk_stats_t *stats) {
  fs_file_t it;
  memset(&it, 0, sizeof(it));

  while (fs_vol_readdir(volume, path, &it)) {
    char child[512];
    int n = (strcmp(path, "/") == 0)
                ? sys_sprintf(child, sizeof(child), "/%s", it.name)
                : sys_sprintf(child, sizeof(child), "%s/%s", path, it.name);
    if (n <= 0 || (size_t)n >= sizeof(child)) {
      return false;
    }

    fs_file_t st = fs_vol_stat(volume, child);
    if (st.name[0] == '\0') {
      return false; // stat should always find what readdir just reported
    }
    if (st.dir != it.dir) {
      return false;
    }

    if (it.dir) {
      stats->dirs++;
      if (!walk(volume, child, stats)) {
        return false;
      }
    } else {
      if (st.size != it.size) {
        return false;
      }
      stats->files++;
    }
  }
  return true;
}
#endif

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  char root[] = "/tmp/picofuse_fs_004_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  // root/
  //   file1.txt         (3 bytes)
  //   dirA/
  //     file2.txt        (5 bytes)
  //     dirB/
  //       file3.txt       (7 bytes)
  char path[512];
  sys_sprintf(path, sizeof(path), "%s/file1.txt", root);
  TestAssert(create_file(path, "abc"), "creating file1.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/dirA", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir dirA should succeed");

  sys_sprintf(path, sizeof(path), "%s/dirA/file2.txt", root);
  TestAssert(create_file(path, "abcde"), "creating file2.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/dirA/dirB", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir dirB should succeed");

  sys_sprintf(path, sizeof(path), "%s/dirA/dirB/file3.txt", root);
  TestAssert(create_file(path, "abcdefg"),
             "creating file3.txt should succeed");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

  walk_stats_t stats = {0};
  TestAssert(walk(volume, "/", &stats), "recursive walk should succeed");
  TestAssert(stats.files == 3, "expected 3 files, got %d", stats.files);
  TestAssert(stats.dirs == 2, "expected 2 directories, got %d", stats.dirs);

  fs_file_t f3 = fs_vol_stat(volume, "/dirA/dirB/file3.txt");
  TestAssert(f3.name[0] != '\0', "file3.txt should be found by stat");
  TestAssert(!f3.dir, "file3.txt should not be a directory");
  TestAssert(f3.size == 7, "file3.txt should be 7 bytes, got %zu", f3.size);

  fs_vol_deinit(volume);

  sys_sprintf(path, sizeof(path), "%s/dirA/dirB/file3.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/dirA/dirB", root);
  rmdir(path);
  sys_sprintf(path, sizeof(path), "%s/dirA/file2.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/dirA", root);
  rmdir(path);
  sys_sprintf(path, sizeof(path), "%s/file1.txt", root);
  unlink(path);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

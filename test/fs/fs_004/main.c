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

// root/
//   file1.txt         (3 bytes)
//   dirA/
//     file2.txt        (5 bytes)
//     dirB/
//       file3.txt       (7 bytes)
static bool build_fixture(fs_volume_t *volume) {
  return write_file(volume, "/file1.txt", "abc") &&
         fs_vol_mkdir(volume, "/dirA") &&
         write_file(volume, "/dirA/file2.txt", "abcde") &&
         fs_vol_mkdir(volume, "/dirA/dirB") &&
         write_file(volume, "/dirA/dirB/file3.txt", "abcdefg");
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

static bool run_checks(fs_volume_t *volume) {
  walk_stats_t stats = {0};
  TestAssert(walk(volume, "/", &stats), "recursive walk should succeed");
  TestAssert(stats.files == 3, "expected 3 files, got %d", stats.files);
  TestAssert(stats.dirs == 2, "expected 2 directories, got %d", stats.dirs);

  fs_file_t f3 = fs_vol_stat(volume, "/dirA/dirB/file3.txt");
  TestAssert(f3.name[0] != '\0', "file3.txt should be found by stat");
  TestAssert(!f3.dir, "file3.txt should not be a directory");
  TestAssert(f3.size == 7, "file3.txt should be 7 bytes, got %zu", f3.size);

  return true;
}

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  {
    char root[] = "/tmp/picofuse_fs_004_XXXXXX";
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

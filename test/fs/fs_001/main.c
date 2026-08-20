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

// Recursively remove everything under `path` (children before parents), so
// a POSIX-path volume's real scratch directory can be rmdir'd afterward.
// A no-op cost for a memory volume, whose storage fs_vol_deinit() frees
// outright, but kept generic so the same helper works for both backends.
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
  return write_file(volume, "/a.txt", "hello") &&
         write_file(volume, "/b.txt", "world!") &&
         fs_vol_mkdir(volume, "/subdir");
}

static bool run_checks(fs_volume_t *volume) {
  fs_file_t it;
  memset(&it, 0, sizeof(it));
  int count = 0;
  bool saw_a = false, saw_b = false, saw_subdir = false;
  while (fs_vol_readdir(volume, "/", &it)) {
    TestAssert(strcmp(it.name, ".") != 0, "readdir should not list \".\"");
    TestAssert(strcmp(it.name, "..") != 0, "readdir should not list \"..\"");
    if (strcmp(it.name, "a.txt") == 0) {
      saw_a = true;
      TestAssert(!it.dir, "a.txt should not be a directory");
      TestAssert(it.size == 5, "a.txt should be 5 bytes, got %zu", it.size);
    } else if (strcmp(it.name, "b.txt") == 0) {
      saw_b = true;
      TestAssert(!it.dir, "b.txt should not be a directory");
      TestAssert(it.size == 6, "b.txt should be 6 bytes, got %zu", it.size);
    } else if (strcmp(it.name, "subdir") == 0) {
      saw_subdir = true;
      TestAssert(it.dir, "subdir should be a directory");
    }
    count++;
  }
  TestAssert(saw_a && saw_b && saw_subdir,
             "readdir should list a.txt, b.txt and subdir");
  TestAssert(count == 3, "readdir should list exactly 3 entries, got %d",
             count);

  memset(&it, 0, sizeof(it));
  TestAssert(!fs_vol_readdir(volume, "/subdir", &it),
             "readdir on an empty subdir should immediately return false");

  memset(&it, 0, sizeof(it));
  TestAssert(!fs_vol_readdir(volume, "/nope", &it),
             "readdir on a nonexistent path should return false");

  memset(&it, 0, sizeof(it));
  TestAssert(!fs_vol_readdir(volume, "/a.txt", &it),
             "readdir on a regular file should return false");

  return true;
}

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  {
    char root[] = "/tmp/picofuse_fs_001_XXXXXX";
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

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
  char root[] = "/tmp/picofuse_fs_001_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  char path[512];
  sys_sprintf(path, sizeof(path), "%s/a.txt", root);
  TestAssert(create_file(path, "hello"), "creating a.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/b.txt", root);
  TestAssert(create_file(path, "world!"), "creating b.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/subdir", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir subdir should succeed");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

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

  fs_vol_deinit(volume);

  sys_sprintf(path, sizeof(path), "%s/a.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/b.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/subdir", root);
  rmdir(path);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

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
  char root[] = "/tmp/picofuse_fs_005_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  char path[512];
  sys_sprintf(path, sizeof(path), "%s/visible.txt", root);
  TestAssert(create_file(path, "abc"), "creating visible.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/.hidden.txt", root);
  TestAssert(create_file(path, "xyz"), "creating .hidden.txt should succeed");

  sys_sprintf(path, sizeof(path), "%s/visible_dir", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir visible_dir should succeed");

  sys_sprintf(path, sizeof(path), "%s/.hidden_dir", root);
  TestAssert(mkdir(path, 0777) == 0, "mkdir .hidden_dir should succeed");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

  // readdir should skip every hidden entry.
  fs_file_t it;
  memset(&it, 0, sizeof(it));
  int count = 0;
  bool saw_visible_file = false, saw_visible_dir = false;
  while (fs_vol_readdir(volume, "/", &it)) {
    TestAssert(it.name[0] != '.', "readdir should not list hidden entry \"%s\"",
               it.name);
    if (strcmp(it.name, "visible.txt") == 0) {
      saw_visible_file = true;
    } else if (strcmp(it.name, "visible_dir") == 0) {
      saw_visible_dir = true;
    }
    count++;
  }
  TestAssert(saw_visible_file && saw_visible_dir,
             "readdir should list both visible entries");
  TestAssert(count == 2, "readdir should list exactly 2 entries, got %d",
             count);

  // Hidden entries must still be directly reachable by explicit path -
  // "hidden" only affects enumeration, not access.
  fs_file_t hf = fs_vol_stat(volume, "/.hidden.txt");
  TestAssert(hf.name[0] != '\0', "stat should find .hidden.txt directly");
  TestAssert(!hf.dir, ".hidden.txt should not be a directory");
  TestAssert(hf.size == 3, ".hidden.txt should be 3 bytes, got %zu", hf.size);

  fs_file_t hd = fs_vol_stat(volume, "/.hidden_dir");
  TestAssert(hd.name[0] != '\0', "stat should find .hidden_dir directly");
  TestAssert(hd.dir, ".hidden_dir should be a directory");

  // mkdir/remove must work on hidden target names too.
  TestAssert(fs_vol_mkdir(volume, "/.newhidden"),
             "mkdir of a hidden directory should succeed");
  fs_file_t nh = fs_vol_stat(volume, "/.newhidden");
  TestAssert(nh.name[0] != '\0' && nh.dir,
             ".newhidden should exist as a directory after mkdir");

  // The newly created hidden directory still shouldn't show up in a
  // listing.
  memset(&it, 0, sizeof(it));
  count = 0;
  while (fs_vol_readdir(volume, "/", &it)) {
    count++;
  }
  TestAssert(count == 2,
             "readdir should still list exactly 2 entries after creating a "
             "hidden dir, got %d",
             count);

  TestAssert(fs_vol_remove(volume, "/.newhidden"),
             "remove of a hidden directory should succeed");
  nh = fs_vol_stat(volume, "/.newhidden");
  TestAssert(nh.name[0] == '\0', ".newhidden should be gone after remove");

  // move must work when the destination name is hidden.
  TestAssert(fs_vol_move(volume, "/visible.txt", "/.renamed.txt"),
             "move to a hidden destination should succeed");
  fs_file_t rn = fs_vol_stat(volume, "/.renamed.txt");
  TestAssert(rn.name[0] != '\0' && !rn.dir && rn.size == 3,
             ".renamed.txt should exist with the original content");

  fs_vol_deinit(volume);

  sys_sprintf(path, sizeof(path), "%s/.renamed.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/.hidden.txt", root);
  unlink(path);
  sys_sprintf(path, sizeof(path), "%s/.hidden_dir", root);
  rmdir(path);
  sys_sprintf(path, sizeof(path), "%s/visible_dir", root);
  rmdir(path);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

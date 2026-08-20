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

// Not readdir-based (unlike the other fs_* tests' cleanup helper): readdir
// deliberately skips hidden entries, and this fixture's leftovers include
// several ("/.hidden.txt", "/.hidden_dir", "/.renamed.txt") that a
// listing-driven walk would never see. The exact fixture is small and
// known, so remove it by name instead - tolerating paths already renamed
// or removed by run_checks(), same as the original per-file unlink calls.
static bool cleanup_all(fs_volume_t *volume) {
  fs_vol_remove(volume, "/visible.txt");
  fs_vol_remove(volume, "/.hidden.txt");
  fs_vol_remove(volume, "/visible_dir");
  fs_vol_remove(volume, "/.hidden_dir");
  fs_vol_remove(volume, "/.newhidden");
  fs_vol_remove(volume, "/.renamed.txt");
  return true;
}

static bool build_fixture(fs_volume_t *volume) {
  return write_file(volume, "/visible.txt", "abc") &&
         write_file(volume, "/.hidden.txt", "xyz") &&
         fs_vol_mkdir(volume, "/visible_dir") &&
         fs_vol_mkdir(volume, "/.hidden_dir");
}

static bool run_checks(fs_volume_t *volume) {
  // readdir should skip every hidden entry.
  fs_file_t it;
  memset(&it, 0, sizeof(it));
  int count = 0;
  bool saw_visible_file = false, saw_visible_dir = false;
  while (fs_vol_readdir(volume, "/", &it)) {
    TestAssert(it.name[0] != '.',
               "readdir should not list hidden entry \"%s\"", it.name);
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

  return true;
}

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  {
    char root[] = "/tmp/picofuse_fs_005_XXXXXX";
    TestAssert(mkdtemp(root) != NULL,
               "mkdtemp should create a scratch directory");

    fs_volume_t *volume = fs_vol_init_path(root);
    TestAssert(volume != NULL, "fs_vol_init_path should succeed");
    TestAssert(build_fixture(volume),
               "fixture setup (path backend) should succeed");
    TestAssert(run_checks(volume), "checks (path backend) should pass");
    TestAssert(cleanup_all(volume), "cleanup (path backend) should succeed");
    fs_vol_deinit(volume);

    TestAssert(rmdir(root) == 0,
               "rmdir of the now-empty scratch directory should succeed");
  }

  {
    char file_path[] = "/tmp/picofuse_fs_005_file_XXXXXX";
    int file_fd = mkstemp(file_path);
    TestAssert(file_fd >= 0, "mkstemp should create a scratch image file");
    close(file_fd);

    fs_volume_t *volume = fs_vol_init_file(file_path, 64 * 1024);
    TestAssert(volume != NULL, "fs_vol_init_file should succeed");
    TestAssert(build_fixture(volume),
               "fixture setup (file backend) should succeed");
    TestAssert(run_checks(volume), "checks (file backend) should pass");
    fs_vol_deinit(volume);

    unlink(file_path);
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

#include <test.h>

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
#include <stdlib.h>
#include <unistd.h>
#endif

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  char root[] = "/tmp/picofuse_fs_003_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

  size_t free_bytes = 0;
  size_t total = fs_vol_size(volume, &free_bytes);
  TestAssert(total > 0, "volume size should be non-zero, got %zu", total);
  TestAssert(free_bytes <= total,
             "free (%zu) should not exceed total (%zu)", free_bytes, total);

  size_t total_again = fs_vol_size(volume, NULL);
  TestAssert(total_again == total,
             "fs_vol_size with a NULL free pointer should still report the "
             "same total (%zu vs %zu)",
             total_again, total);

  size_t null_free = 123;
  TestAssert(fs_vol_size(NULL, &null_free) == 0,
             "fs_vol_size(NULL, ...) should report 0");
  TestAssert(null_free == 0,
             "fs_vol_size(NULL, ...) should zero the free-space output");

  fs_vol_deinit(volume);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

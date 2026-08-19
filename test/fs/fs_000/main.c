#include <test.h>

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
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

  return true;
}

TestMain(test_main)

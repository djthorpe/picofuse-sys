#include <test.h>
#include <string.h>

// Exercises only the generic dispatcher (src/fs/any/), never a backend, so
// this runs identically on every platform - no volume is ever mounted.
bool test_main(void) {
  fs_vol_deinit(NULL); // must not crash

  size_t free_bytes = 123;
  TestAssert(fs_vol_size(NULL, &free_bytes) == 0,
             "fs_vol_size(NULL, ...) should report 0");
  TestAssert(free_bytes == 0,
             "fs_vol_size(NULL, ...) should zero the free-space output");

  fs_file_t it;
  memset(&it, 0, sizeof(it));
  TestAssert(!fs_vol_readdir(NULL, "/", &it),
             "fs_vol_readdir(NULL, ...) should return false");

  fs_file_t st = fs_vol_stat(NULL, "/");
  TestAssert(st.name[0] == '\0', "fs_vol_stat(NULL, ...) should report "
                                  "not-found");

  TestAssert(!fs_vol_mkdir(NULL, "/x"), "fs_vol_mkdir(NULL, ...) should "
                                        "return false");
  TestAssert(!fs_vol_remove(NULL, "/x"), "fs_vol_remove(NULL, ...) should "
                                         "return false");
  TestAssert(!fs_vol_move(NULL, "/a", "/b"),
             "fs_vol_move(NULL, ...) should return false");

  TestAssert(fs_file_create(NULL, "/x").ctx == NULL,
             "fs_file_create(NULL, ...) should fail");
  TestAssert(fs_file_open(NULL, "/x", false).ctx == NULL,
             "fs_file_open(NULL, ...) should fail");
  TestAssert(!fs_file_close(NULL), "fs_file_close(NULL) should return false");

  char buf[8];
  TestAssert(fs_file_read(NULL, buf, sizeof(buf)) == 0,
             "fs_file_read(NULL, ...) should return 0");
  TestAssert(fs_file_write(NULL, buf, sizeof(buf)) == 0,
             "fs_file_write(NULL, ...) should return 0");
  TestAssert(!fs_file_seek(NULL, 0), "fs_file_seek(NULL, ...) should return "
                                     "false");

  return true;
}

TestMain(test_main)

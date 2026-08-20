#include <test.h>

#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  char root[] = "/tmp/picofuse_fs_007_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

  // Create a new file and write to it; size/pos should track the write.
  fs_file_t f = fs_file_create(volume, "/hello.txt");
  TestAssert(f.ctx != NULL, "fs_file_create should succeed");
  TestAssert(!f.dir, "created file should not be a directory");
  TestAssert(f.size == 0 && f.pos == 0, "new file should start empty");
  TestAssert(strcmp(f.name, "hello.txt") == 0,
             "created file should report its own name, got \"%s\"", f.name);

  size_t n = fs_file_write(&f, "Hello, world!", 13);
  TestAssert(n == 13, "write should report 13 bytes written, got %zu", n);
  TestAssert(f.pos == 13 && f.size == 13,
             "pos/size should track the write, got pos=%zu size=%zu", f.pos,
             f.size);

  TestAssert(fs_file_close(&f), "fs_file_close should succeed");
  TestAssert(f.ctx == NULL, "fs_file_close should clear ctx");

  // fs_vol_stat should agree with what was written.
  fs_file_t st = fs_vol_stat(volume, "/hello.txt");
  TestAssert(st.size == 13, "stat should report 13 bytes, got %zu", st.size);

  // Reopen read-only and read the content back.
  fs_file_t r = fs_file_open(volume, "/hello.txt", false);
  TestAssert(r.ctx != NULL, "fs_file_open should succeed");
  TestAssert(r.size == 13, "opened file should report 13 bytes, got %zu",
             r.size);

  char buf[32] = {0};
  size_t rn = fs_file_read(&r, buf, sizeof(buf));
  TestAssert(rn == 13, "read should return 13 bytes, got %zu", rn);
  TestAssert(memcmp(buf, "Hello, world!", 13) == 0,
             "read content should match what was written");
  TestAssert(r.pos == 13, "pos should track the read, got %zu", r.pos);

  // Reading past EOF returns 0, not an error.
  TestAssert(fs_file_read(&r, buf, sizeof(buf)) == 0,
             "read past EOF should return 0");

  // Seeking and a partial read.
  TestAssert(fs_file_seek(&r, 7), "seek should succeed");
  TestAssert(r.pos == 7, "pos should reflect the seek, got %zu", r.pos);
  memset(buf, 0, sizeof(buf));
  rn = fs_file_read(&r, buf, 6);
  TestAssert(rn == 6, "read after seek should return 6 bytes, got %zu", rn);
  TestAssert(memcmp(buf, "world!", 6) == 0,
             "read after seek should return \"world!\"");

  TestAssert(fs_file_close(&r), "fs_file_close should succeed");

  // A read-only handle must reject writes.
  fs_file_t ro = fs_file_open(volume, "/hello.txt", false);
  TestAssert(ro.ctx != NULL, "reopen for read should succeed");
  TestAssert(fs_file_write(&ro, "X", 1) == 0,
             "write to a read-only handle should be rejected");
  fs_file_close(&ro);

  // create() truncates an existing file to zero length.
  fs_file_t trunc = fs_file_create(volume, "/hello.txt");
  TestAssert(trunc.ctx != NULL, "re-create should succeed");
  TestAssert(trunc.size == 0, "re-create should truncate, got size %zu",
             trunc.size);
  fs_file_close(&trunc);
  st = fs_vol_stat(volume, "/hello.txt");
  TestAssert(st.size == 0, "stat should confirm truncation, got %zu",
             st.size);

  // open(write=true) allows writing to an existing file - a different
  // code path (O_RDWR open) than create()'s truncate-and-open.
  fs_file_t w = fs_file_open(volume, "/hello.txt", true);
  TestAssert(w.ctx != NULL, "fs_file_open(write=true) should succeed");
  TestAssert(w.size == 0, "file should still be empty, got %zu", w.size);
  n = fs_file_write(&w, "abcdef", 6);
  TestAssert(n == 6, "write via open(write=true) should report 6 bytes, got "
                      "%zu",
             n);
  TestAssert(w.pos == 6 && w.size == 6,
             "pos/size should track the write, got pos=%zu size=%zu", w.pos,
             w.size);
  TestAssert(fs_file_close(&w), "fs_file_close should succeed");

  st = fs_vol_stat(volume, "/hello.txt");
  TestAssert(st.size == 6,
             "stat should confirm the write via open(write=true), got %zu",
             st.size);

  // open() rejects a nonexistent file.
  fs_file_t missing = fs_file_open(volume, "/nope.txt", false);
  TestAssert(missing.ctx == NULL, "open of a missing file should fail");

  // create() rejects a missing parent directory.
  fs_file_t bad_parent = fs_file_create(volume, "/missing/deep.txt");
  TestAssert(bad_parent.ctx == NULL,
             "create with a missing parent should fail");

  // The generic dispatcher must be NULL-safe.
  TestAssert(fs_file_create(NULL, "/x").ctx == NULL,
             "create(NULL, ...) should fail");
  TestAssert(fs_file_open(NULL, "/x", false).ctx == NULL,
             "open(NULL, ...) should fail");
  TestAssert(fs_file_read(NULL, buf, sizeof(buf)) == 0,
             "read(NULL) should return 0");
  TestAssert(fs_file_write(NULL, "x", 1) == 0,
             "write(NULL) should return 0");
  TestAssert(!fs_file_seek(NULL, 0), "seek(NULL) should return false");
  TestAssert(!fs_file_close(NULL), "close(NULL) should return false");

  fs_vol_deinit(volume);

  char path[512];
  sys_sprintf(path, sizeof(path), "%s/hello.txt", root);
  unlink(path);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

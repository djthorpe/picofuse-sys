#include <test.h>
#include <string.h>

// Exercises fs_vol_init_flash() against real flash hardware. This test is
// still built and run by ctest on every platform - like fs_vol_init_flash()
// itself, it just degrades to asserting the "unsupported here" NULL return
// on host builds (see the #else branch below). The real flash-backed checks
// only actually execute on Pico, and even then only once someone flashes
// this test binary to a board (ctest can't run a Pico ELF/UF2 on the host).
// Kept to a single, non-exhaustive check rather than mirroring every
// fs_00X test against flash too: hw_block_flash_init() is a single,
// process-lifetime allocation (only one flash-backed volume can exist at a
// time), and every mount/format/erase cycle here wears real NOR flash,
// which has finite write endurance.
#if defined(SYSTEM_NAME_PICO)

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

#endif

bool test_main(void) {
#if defined(SYSTEM_NAME_PICO)
  // fs_vol_init_flash() allocates its region relative to this size on every
  // call (see volume.h), so the remount below must request the same value
  // to find the filesystem formatted here rather than a different region.
  const size_t volume_size = 16 * 1024;

  fs_volume_t *volume = fs_vol_init_flash(volume_size);
  TestAssert(volume != NULL, "fs_vol_init_flash should mount or format");

  size_t free_bytes = 0;
  size_t total = fs_vol_size(volume, &free_bytes);
  TestAssert(total > 0, "volume size should be non-zero, got %zu", total);
  TestAssert(free_bytes <= total,
             "free (%zu) should not exceed total (%zu)", free_bytes, total);

  TestAssert(write_file(volume, "/hello.txt", "flash!"),
             "create /hello.txt should succeed");
  fs_file_t st = fs_vol_stat(volume, "/hello.txt");
  TestAssert(st.name[0] != '\0' && !st.dir && st.size == 6,
             "hello.txt should exist with 6 bytes, got %zu", st.size);

  fs_file_t r = fs_file_open(volume, "/hello.txt", false);
  TestAssert(r.ctx != NULL, "reopen /hello.txt should succeed");
  char buf[16] = {0};
  size_t n = fs_file_read(&r, buf, sizeof(buf));
  TestAssert(n == 6 && memcmp(buf, "flash!", 6) == 0,
             "content should round-trip through real flash, got %zu bytes",
             n);
  fs_file_close(&r);

  TestAssert(fs_vol_mkdir(volume, "/sub"), "mkdir /sub should succeed");
  TestAssert(fs_vol_stat(volume, "/sub").dir, "/sub should be a directory");
  TestAssert(fs_vol_remove(volume, "/sub"), "remove empty /sub should succeed");
  TestAssert(fs_vol_stat(volume, "/sub").name[0] == '\0',
             "/sub should be gone after remove");

  // hw_block_flash_init() only ever hands out one region at a time, so a
  // second concurrent call must fail while `volume` is still mounted.
  TestAssert(fs_vol_init_flash(volume_size) == NULL,
             "a second concurrent fs_vol_init_flash should fail");

  fs_vol_deinit(volume);

  // Remounting with the same size should find the same, already-formatted
  // filesystem - confirming persistence across a real flash-backed
  // deinit/reinit, not just a fresh format each time.
  fs_volume_t *remounted = fs_vol_init_flash(volume_size);
  TestAssert(remounted != NULL, "remount with the same size should succeed");
  st = fs_vol_stat(remounted, "/hello.txt");
  TestAssert(st.name[0] != '\0' && st.size == 6,
             "hello.txt should survive a flash-backed remount");

  // Clean up so repeated flashes of this test don't accumulate content (and
  // thus unnecessary wear) across runs.
  TestAssert(fs_vol_remove(remounted, "/hello.txt"),
             "cleanup: remove /hello.txt should succeed");
  fs_vol_deinit(remounted);
#else
  TestAssert(fs_vol_init_flash(16 * 1024) == NULL,
             "fs_vol_init_flash should be unsupported on this platform");
#endif

  return true;
}

TestMain(test_main)

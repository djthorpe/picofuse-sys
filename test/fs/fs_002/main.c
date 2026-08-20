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

static bool build_fixture(fs_volume_t *volume) {
  return fs_vol_mkdir(volume, "/sub") &&
         write_file(volume, "/sub/inner.txt", "x") &&
         write_file(volume, "/file.txt", "x");
}

// Both backends must reject the same malformed/relative/escaping path
// forms - the POSIX backend confines against the real host filesystem, and
// the littlefs backend confines symbolically against its own volume root,
// but the documented contract (leading '/', no bare "."/"..", no climbing
// above the volume root) is identical either way.
static bool run_checks(fs_volume_t *volume) {
  // Volume paths must be root-relative and start with '/'; bare "." and
  // ".." are not accepted forms.
  fs_file_t st = fs_vol_stat(volume, ".");
  TestAssert(st.name[0] == '\0', "stat(\".\") should be rejected");

  st = fs_vol_stat(volume, "..");
  TestAssert(st.name[0] == '\0', "stat(\"..\") should be rejected");

  st = fs_vol_stat(volume, "relative");
  TestAssert(st.name[0] == '\0',
             "stat of a non-\"/\"-prefixed path should be rejected");

  // NULL/empty both mean "root", per the documented contract.
  st = fs_vol_stat(volume, NULL);
  TestAssert(st.dir, "stat(NULL) should report the volume root");

  fs_file_t root_st = fs_vol_stat(volume, "/");
  TestAssert(root_st.dir, "stat(\"/\") should report the volume root");
  TestAssert(strcmp(st.name, root_st.name) == 0,
             "stat(NULL) and stat(\"/\") should agree");

  st = fs_vol_stat(volume, "");
  TestAssert(st.dir && strcmp(st.name, root_st.name) == 0,
             "stat(\"\") should report the volume root");

  // Escaping above the volume root must fail rather than leak state from
  // beyond it.
  st = fs_vol_stat(volume, "/..");
  TestAssert(st.name[0] == '\0', "stat(\"/..\") should escape confinement "
                                  "and be rejected");

  st = fs_vol_stat(volume, "/../../../../../../etc/passwd");
  TestAssert(st.name[0] == '\0',
             "deep \"..\" traversal outside root should be rejected");

  // ".." that stays within the confined tree is legitimate and should
  // resolve back to root.
  st = fs_vol_stat(volume, "/sub/..");
  TestAssert(st.dir && strcmp(st.name, root_st.name) == 0,
             "\"/sub/..\" should resolve back to the volume root");

  // "/." canonicalizes to the volume root itself (which already exists),
  // so it succeeds via the documented "already exists" path rather than
  // ever reaching leaf-name validation.
  TestAssert(fs_vol_mkdir(volume, "/."),
             "mkdir(\"/.\") should report success (it is the root itself)");

  // "/.." has no existing parent to resolve within the confined tree, so
  // it falls through to leaf-name validation, which rejects ".." outright.
  TestAssert(!fs_vol_mkdir(volume, "/.."),
             "mkdir(\"/..\") should be rejected");

  // A leaf of "." or ".." under a parent that doesn't exist yet must still
  // be rejected (both for the missing parent and for the invalid leaf).
  TestAssert(!fs_vol_mkdir(volume, "/missing/."),
             "mkdir(\"/missing/.\") should be rejected");
  TestAssert(!fs_vol_mkdir(volume, "/missing/.."),
             "mkdir(\"/missing/..\") should be rejected");

  TestAssert(!fs_vol_mkdir(volume, "relative"),
             "mkdir of a non-\"/\"-prefixed path should be rejected");

  // A name that already exists as a regular file is not a directory, so
  // mkdir must reject it rather than silently reporting success.
  TestAssert(!fs_vol_mkdir(volume, "/file.txt"),
             "mkdir onto an existing file should be rejected");

  // The root itself already exists, so mkdir("/") reports success without
  // trying to create anything.
  TestAssert(fs_vol_mkdir(volume, "/"), "mkdir(\"/\") should report success");

  // The root must never be removed or moved out from under the volume.
  TestAssert(!fs_vol_remove(volume, "/"), "remove(\"/\") should be rejected");
  TestAssert(!fs_vol_remove(volume, "/.."),
             "remove(\"/..\") should be rejected");
  TestAssert(!fs_vol_move(volume, "/", "/elsewhere"),
             "move of the volume root should be rejected");

  // remove() must reject a non-empty directory and a nonexistent path.
  TestAssert(!fs_vol_remove(volume, "/sub"),
             "remove of a non-empty directory should be rejected");
  TestAssert(!fs_vol_remove(volume, "/nope"),
             "remove of a nonexistent path should be rejected");

  fs_file_t it;
  memset(&it, 0, sizeof(it));
  TestAssert(!fs_vol_readdir(volume, ".", &it),
             "readdir of a non-\"/\"-prefixed path should be rejected");

  return true;
}

bool test_main(void) {
#if defined(SYSTEM_NAME_LINUX) || defined(SYSTEM_NAME_DARWIN)
  {
    char root[] = "/tmp/picofuse_fs_002_XXXXXX";
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

  {
    char file_path[] = "/tmp/picofuse_fs_002_file_XXXXXX";
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

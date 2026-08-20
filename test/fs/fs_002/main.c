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
  char root[] = "/tmp/picofuse_fs_002_XXXXXX";
  TestAssert(mkdtemp(root) != NULL,
             "mkdtemp should create a scratch directory");

  char sub[512];
  sys_sprintf(sub, sizeof(sub), "%s/sub", root);
  TestAssert(mkdir(sub, 0777) == 0, "mkdir sub should succeed");

  char sub_inner[512];
  sys_sprintf(sub_inner, sizeof(sub_inner), "%s/sub/inner.txt", root);
  TestAssert(create_file(sub_inner, "x"),
             "creating sub/inner.txt should succeed");

  char file[512];
  sys_sprintf(file, sizeof(file), "%s/file.txt", root);
  TestAssert(create_file(file, "x"), "creating file.txt should succeed");

  fs_volume_t *volume = fs_vol_init_path(root);
  TestAssert(volume != NULL, "fs_vol_init_path should succeed");

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

  // Escaping above the volume root must fail rather than leak host state.
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

  fs_vol_deinit(volume);

  unlink(sub_inner);
  rmdir(sub);
  unlink(file);
  rmdir(root);
#endif

  return true;
}

TestMain(test_main)

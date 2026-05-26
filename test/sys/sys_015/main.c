#include <string.h>
#include <test.h>

static const uint8_t k_md5_abc[16] = {
    0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
    0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72,
};

static const uint8_t k_sha256_abc[32] = {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
    0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
    0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
};

static bool assert_digest(sys_hash_t *hash, const uint8_t *expected,
                          size_t expected_size, const char *label) {
  const uint8_t *digest = sys_hash_finalize(hash);
  if (digest == NULL) {
    return false;
  }
  if (sys_memcmp(digest, expected, expected_size) != 0) {
    return false;
  }

  const uint8_t *digest_again = sys_hash_finalize(hash);
  if (digest_again != digest) {
    return false;
  }
  if (sys_memcmp(digest_again, expected, expected_size) != 0) {
    return false;
  }

  (void)label;
  return true;
}

bool test_main(void) {
  sys_hash_t *hash = sys_hash_init(sys_hash_md5);
  TestAssert(hash != NULL, "sys_hash_init(md5) returned NULL");
  TestAssert(sys_hash_size(hash) == 16, "md5 digest size should be 16 bytes");
  TestAssert(sys_hash_update(hash, NULL, 0),
             "zero-length md5 update should succeed");
  TestAssert(sys_hash_update(hash, "abc", 3),
             "md5 update for 'abc' should succeed");
  TestAssert(assert_digest(hash, k_md5_abc, sizeof(k_md5_abc), "md5"),
             "md5 digest validation failed");
  TestAssert(!sys_hash_update(hash, "x", 1),
             "md5 update after finalize should fail");
  sys_hash_deinit(hash);

  hash = sys_hash_init(sys_hash_sha256);
  TestAssert(hash != NULL, "sys_hash_init(sha256) returned NULL");
  TestAssert(sys_hash_size(hash) == 32,
             "sha256 digest size should be 32 bytes");
  TestAssert(sys_hash_update(hash, "a", 1),
             "sha256 update for 'a' should succeed");
  TestAssert(sys_hash_update(hash, "b", 1),
             "sha256 update for 'b' should succeed");
  TestAssert(sys_hash_update(hash, "c", 1),
             "sha256 update for 'c' should succeed");
  TestAssert(assert_digest(hash, k_sha256_abc, sizeof(k_sha256_abc), "sha256"),
             "sha256 digest validation failed");
  sys_hash_deinit(hash);

  TestAssert(sys_hash_init((sys_hash_algorithm_t)0) == NULL,
             "unsupported hash algorithm should return NULL");
  TestAssert(sys_hash_djb2("abc") == (uintptr_t)193485963,
             "djb2('abc') returned unexpected value");
  TestAssert(sys_hash_djb2(NULL) == 0, "djb2(NULL) should return 0");

  sys_hash_t *hashes[SYS_HASH_CAPACITY];
  for (size_t index = 0; index < SYS_HASH_CAPACITY; index++) {
    hashes[index] = sys_hash_init(sys_hash_sha256);
    TestAssert(hashes[index] != NULL, "sys_hash_init failed at pool slot %zu",
               index);
  }

  hash = sys_hash_init(sys_hash_md5);
  TestAssert(hash == NULL, "sys_hash_init should fail after %u allocations",
             SYS_HASH_CAPACITY);

  for (size_t index = 0; index < SYS_HASH_CAPACITY; index++) {
    sys_hash_deinit(hashes[index]);
  }

  hash = sys_hash_init(sys_hash_md5);
  TestAssert(hash != NULL,
             "sys_hash_init should succeed again after pool slot release");
  sys_hash_deinit(hash);

  return true;
}

TestMain(test_main)
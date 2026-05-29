#include <stdint.h>
#include <test.h>

static void fill_bytes(uint8_t *bytes, size_t count, uint8_t seed) {
  for (size_t index = 0; index < count; index++) {
    bytes[index] = (uint8_t)(seed + index);
  }
}

static bool check_bytes(const uint8_t *bytes, size_t count, uint8_t seed) {
  for (size_t index = 0; index < count; index++) {
    TestAssert(bytes[index] == (uint8_t)(seed + index),
               "byte %zu should equal %u", index,
               (unsigned int)(uint8_t)(seed + index));
  }

  return true;
}

bool test_main(void) {
  uint8_t *bytes = sys_malloc(8);
  TestAssert(bytes != NULL, "sys_malloc returned NULL");

  for (size_t index = 0; index < 8; index++) {
    bytes[index] = (uint8_t)(index + 1);
  }

  uint32_t *words = sys_calloc(4, sizeof(uint32_t));
  TestAssert(words != NULL, "sys_calloc returned NULL");
  for (size_t index = 0; index < 4; index++) {
    TestAssert(words[index] == 0, "sys_calloc should zero-initialize word %zu",
               index);
  }

  uint8_t *resized = sys_realloc(bytes, 16);
  TestAssert(resized != NULL, "sys_realloc grow returned NULL");
  for (size_t index = 0; index < 8; index++) {
    TestAssert(resized[index] == (uint8_t)(index + 1),
               "sys_realloc should preserve byte %zu", index);
  }

  uint8_t *allocated_via_realloc = sys_realloc(NULL, 4);
  TestAssert(allocated_via_realloc != NULL,
             "sys_realloc(NULL, size) should allocate memory");

  void *overflow = sys_calloc((size_t)-1, 2);
  TestAssert(overflow == NULL,
             "sys_calloc should reject count * size overflow");

  sys_free(allocated_via_realloc);
  sys_free(resized);
  sys_free(words);
  sys_free(NULL);

  uint8_t *large_a = sys_malloc(20000);
  TestAssert(large_a != NULL,
             "sys_malloc should allocate a large block from the default chain");
  fill_bytes(large_a, 64, 11);

  uint8_t *large_b = sys_malloc(20000);
  TestAssert(
      large_b != NULL,
      "sys_malloc should grow the default chain when the head arena is full");
  fill_bytes(large_b, 64, 71);

  uint8_t *grown_across_chain = sys_realloc(large_a, 40000);
  TestAssert(grown_across_chain != NULL,
             "sys_realloc should grow across arenas when the owner arena "
             "cannot satisfy the request");
  TestAssert(check_bytes(grown_across_chain, 64, 11),
             "grown block should preserve its prefix bytes");
  TestAssert(check_bytes(large_b, 64, 71),
             "second large block should remain unchanged");

  sys_free(grown_across_chain);
  sys_free(large_b);

  return true;
}

TestMain(test_main)
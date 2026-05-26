#include <stdint.h>
#include <test.h>

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

  return true;
}

TestMain(test_main)
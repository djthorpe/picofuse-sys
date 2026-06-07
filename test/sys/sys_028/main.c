#include <stdint.h>
#include <test.h>

static bool strings_equal(const char *lhs, const char *rhs) {
  while (*lhs || *rhs) {
    if (*lhs != *rhs) {
      return false;
    }
    lhs++;
    rhs++;
  }
  return true;
}

#define ASSERT_SPRINTF(LABEL, EXPECTED, FORMAT, ...)                           \
  do {                                                                         \
    char actual[128];                                                          \
    size_t len = sys_sprintf(actual, sizeof(actual), FORMAT, __VA_ARGS__);     \
    TestAssert(strings_equal(actual, EXPECTED),                                \
               "%s mismatch: expected '%s', got '%s'", LABEL, EXPECTED,        \
               actual);                                                        \
    TestAssert(len == (sizeof(EXPECTED) - 1u),                                 \
               "%s length mismatch: expected %u got %u", LABEL,                \
               (unsigned int)(sizeof(EXPECTED) - 1u), (unsigned int)len);      \
  } while (0)

bool test_main(void) {
  int8_t s8 = (int8_t)-42;
  uint8_t u8 = (uint8_t)250;
  int16_t s16 = (int16_t)-12345;
  uint16_t u16 = (uint16_t)54321;
  int32_t s32 = (int32_t)-123456789;
  uint32_t u32 = UINT32_C(4000000000);
  int64_t s64 = INT64_C(-9223372036854775807);
  uint64_t u64 = UINT64_C(18446744073709551615);

  // %d/%u are implemented as 32-bit paths in this formatter.
  ASSERT_SPRINTF("int8 via %d", "-42", "%d", (int32_t)s8);
  ASSERT_SPRINTF("uint8 via %u", "250", "%u", (uint32_t)u8);
  ASSERT_SPRINTF("int16 via %d", "-12345", "%d", (int32_t)s16);
  ASSERT_SPRINTF("uint16 via %u", "54321", "%u", (uint32_t)u16);
  ASSERT_SPRINTF("int32 via %d", "-123456789", "%d", s32);
  ASSERT_SPRINTF("uint32 via %u", "4000000000", "%u", u32);

  // %ld/%lu are implemented as 64-bit paths in this formatter.
  ASSERT_SPRINTF("int64 via %ld", "-9223372036854775807", "%ld", s64);
  ASSERT_SPRINTF("uint64 via %lu", "18446744073709551615", "%lu", u64);
  ASSERT_SPRINTF(
      "uint64 via %#lb",
      "0b1111111111111111111111111111111111111111111111111111111111111111",
      "%#lb", u64);

  return true;
}

TestMain(test_main)

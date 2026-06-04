#include <inttypes.h>
#include <stdint.h>
#include <test.h>

static bool check_random_u32(void) {
  uint32_t first = sys_random_uint32();
  bool saw_difference = false;
  uint32_t bit_or = first;
  uint32_t bit_and = first;

  for (uint32_t i = 0; i < 64; ++i) {
    uint32_t value = sys_random_uint32();
    if (value != first) {
      saw_difference = true;
    }

    bit_or |= value;
    bit_and &= value;
  }

  TestAssert(saw_difference,
             "sys_random_uint32 returned the same value across all samples: %u",
             (unsigned int)first);
  TestAssert(bit_or != 0u,
             "sys_random_uint32 produced only zero bits in sample window");
  TestAssert(bit_and != UINT32_MAX,
             "sys_random_uint32 produced only one bits in sample window");
  return true;
}

static bool check_random_u64(void) {
  uint64_t first = sys_random_uint64();
  bool saw_difference = false;
  bool saw_upper_bits = (first >> 32) != 0;
  uint64_t bit_or = first;
  uint64_t bit_and = first;

  for (uint32_t i = 0; i < 64; ++i) {
    uint64_t value = sys_random_uint64();
    if (value != first) {
      saw_difference = true;
    }
    if ((value >> 32) != 0) {
      saw_upper_bits = true;
    }

    bit_or |= value;
    bit_and &= value;
  }

  TestAssert(
      saw_difference,
      "sys_random_uint64 returned the same value across all samples: %llu",
      (unsigned long long)first);
  TestAssert(saw_upper_bits,
             "sys_random_uint64 never produced a value with upper 32 bits set");
  TestAssert(bit_or != 0u,
             "sys_random_uint64 produced only zero bits in sample window");
  TestAssert(bit_and != UINT64_MAX,
             "sys_random_uint64 produced only one bits in sample window");
  return true;
}

bool test_main(void) {
  if (!check_random_u32()) {
    return false;
  }

  if (!check_random_u64()) {
    return false;
  }

  return true;
}

TestMain(test_main)
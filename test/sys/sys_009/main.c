#include <inttypes.h>
#include <stdint.h>
#include <test.h>

static bool check_random_u32(void) {
  uint32_t first = sys_random_uint32();
  bool saw_difference = false;

  for (uint32_t i = 0; i < 16; ++i) {
    uint32_t value = sys_random_uint32();
    if (value != first) {
      saw_difference = true;
      break;
    }
  }

  TestAssert(
      saw_difference,
      "sys_random_uint32 returned the same value across all samples: %" PRIu32,
      first);
  return true;
}

static bool check_random_u64(void) {
  uint64_t first = sys_random_uint64();
  bool saw_difference = false;
  bool saw_upper_bits = (first >> 32) != 0;

  for (uint32_t i = 0; i < 16; ++i) {
    uint64_t value = sys_random_uint64();
    if (value != first) {
      saw_difference = true;
    }
    if ((value >> 32) != 0) {
      saw_upper_bits = true;
    }
    if (saw_difference && saw_upper_bits) {
      break;
    }
  }

  TestAssert(
      saw_difference,
      "sys_random_uint64 returned the same value across all samples: %" PRIu64,
      first);
  TestAssert(saw_upper_bits,
             "sys_random_uint64 never produced a value with upper 32 bits set");
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
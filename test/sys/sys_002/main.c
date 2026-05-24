
#include <test.h>

bool test_main(void) {
  uint64_t ts = sys_timestamp_ms();
  TestAssert(ts <= 10, "Initial timestamp should be near zero");
  return true;
}

TestMain(test_main)

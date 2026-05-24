
#include <test.h>

bool test_main(void) {
  TestAssert(true, "Hello, world!\n");
  return true;
}

TestMain(test_main)

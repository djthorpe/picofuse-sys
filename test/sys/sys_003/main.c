#include <inttypes.h>
#include <test.h>

static bool check_elapsed(const char *label, uint64_t start, uint64_t end,
                          uint64_t min_delta_ms) {
  TestAssert(end >= start, "%s went backwards: start=%" PRIu64 ", end=%" PRIu64,
             label, start, end);

  uint64_t delta_ms = end - start;
  TestAssert(delta_ms >= min_delta_ms,
             "%s advanced too little after sleep: delta=%" PRIu64
             " ms, expected at least %" PRIu64 " ms",
             label, delta_ms, min_delta_ms);
  return true;
}

bool test_main(void) {
  uint64_t ts0 = sys_timestamp_ms();

  sys_sleep_ms(25);
  uint64_t ts1 = sys_timestamp_ms();

  sys_sleep_ms(40);
  uint64_t ts2 = sys_timestamp_ms();

  if (!check_elapsed("sleep_1", ts0, ts1, 15)) {
    return false;
  }

  if (!check_elapsed("sleep_2", ts1, ts2, 30)) {
    return false;
  }

  if (!check_elapsed("total", ts0, ts2, 55)) {
    return false;
  }

  return true;
}

TestMain(test_main)
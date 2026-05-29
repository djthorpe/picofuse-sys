#include <stdint.h>
#include <test.h>

static bool in_range_u8(uint8_t value, uint8_t min, uint8_t max) {
  return value >= min && value <= max;
}

static bool in_range_u16(uint16_t value, uint16_t min, uint16_t max) {
  return value >= min && value <= max;
}

#ifdef SYSTEM_NAME_PICO
static int64_t abs_i64(int64_t value) { return value < 0 ? -value : value; }
#endif

bool test_main(void) {
  sys_date_t now = {0};
  uint8_t hours = 0;
  uint8_t minutes = 0;
  uint8_t seconds = 0;
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t weekday = 0;

  TestAssert(!sys_date_get_now(NULL), "sys_date_get_now should reject NULL");
  TestAssert(!sys_date_set_now(NULL), "sys_date_set_now should reject NULL");
  TestAssert(sys_date_get_now(&now), "sys_date_get_now should succeed");
  TestAssert(now.nanoseconds >= 0,
             "sys_date_get_now should return non-negative nanoseconds");
  TestAssert(now.nanoseconds < 1000000000,
             "sys_date_get_now should return sub-second nanoseconds");

  TestAssert(sys_date_get_time_utc(NULL, &hours, &minutes, &seconds),
             "sys_date_get_time_utc(NULL, ...) should use the current time");
  TestAssert(in_range_u8(hours, 0, 23), "UTC hours should be in range");
  TestAssert(in_range_u8(minutes, 0, 59), "UTC minutes should be in range");
  TestAssert(in_range_u8(seconds, 0, 59), "UTC seconds should be in range");

  TestAssert(sys_date_get_date_utc(NULL, &year, &month, &day, &weekday),
             "sys_date_get_date_utc(NULL, ...) should use the current time");
  TestAssert(in_range_u16(year, 1970, 65535), "UTC year should be in range");
  TestAssert(in_range_u8(month, 1, 12), "UTC month should be in range");
  TestAssert(in_range_u8(day, 1, 31), "UTC day should be in range");
  TestAssert(in_range_u8(weekday, 0, 6), "UTC weekday should be in range");

  sys_date_t sample = {
      .seconds = 0,
      .nanoseconds = 123456789,
      .tzoffset = 2 * 60 * 60,
  };

  TestAssert(sys_date_set_date_utc(&sample, 2024, 2, 29),
             "sys_date_set_date_utc should accept leap day");
  TestAssert(sample.nanoseconds == 123456789,
             "sys_date_set_date_utc should preserve nanoseconds");
  TestAssert(sample.tzoffset == 2 * 60 * 60,
             "sys_date_set_date_utc should preserve timezone offset");
  TestAssert(sys_date_set_time_utc(&sample, 23, 45, 12),
             "sys_date_set_time_utc should accept a valid time");
  TestAssert(sample.nanoseconds == 123456789,
             "sys_date_set_time_utc should preserve nanoseconds");
  TestAssert(sample.tzoffset == 2 * 60 * 60,
             "sys_date_set_time_utc should preserve timezone offset");

  hours = 0;
  minutes = 0;
  seconds = 0;
  year = 0;
  month = 0;
  day = 0;
  weekday = 0;

  TestAssert(sys_date_get_time_utc(&sample, &hours, &minutes, &seconds),
             "sys_date_get_time_utc should extract a constructed UTC time");
  TestAssert(hours == 23 && minutes == 45 && seconds == 12,
             "constructed UTC time should round-trip");
  TestAssert(sys_date_get_date_utc(&sample, &year, &month, &day, &weekday),
             "sys_date_get_date_utc should extract a constructed UTC date");
  TestAssert(year == 2024 && month == 2 && day == 29,
             "constructed UTC date should round-trip");
  TestAssert(weekday == 4,
             "2024-02-29 should round-trip as Thursday (weekday 4)");

  TestAssert(sys_date_get_time_local(&sample, &hours, &minutes, &seconds),
             "sys_date_get_time_local should apply timezone offset");
  TestAssert(hours == 1 && minutes == 45 && seconds == 12,
             "positive timezone offset should roll local time into next day");
  TestAssert(sys_date_get_date_local(&sample, &year, &month, &day, &weekday),
             "sys_date_get_date_local should apply timezone offset");
  TestAssert(year == 2024 && month == 3 && day == 1,
             "positive timezone offset should roll local date into next month");
  TestAssert(weekday == 5, "2024-03-01 should be Friday (weekday 5)");

  TestAssert(!sys_date_set_time_utc(NULL, 0, 0, 0),
             "sys_date_set_time_utc should reject NULL");
  TestAssert(!sys_date_set_time_utc(&sample, 24, 0, 0),
             "sys_date_set_time_utc should reject invalid hours");
  TestAssert(!sys_date_set_time_utc(&sample, 23, 60, 0),
             "sys_date_set_time_utc should reject invalid minutes");
  TestAssert(!sys_date_set_time_utc(&sample, 23, 59, 60),
             "sys_date_set_time_utc should reject invalid seconds");

  TestAssert(!sys_date_set_date_utc(NULL, 2024, 1, 1),
             "sys_date_set_date_utc should reject NULL");
  TestAssert(!sys_date_set_date_utc(&sample, 1899, 1, 1),
             "sys_date_set_date_utc should reject years before 1900");
  TestAssert(!sys_date_set_date_utc(&sample, 2024, 0, 1),
             "sys_date_set_date_utc should reject month 0");
  TestAssert(!sys_date_set_date_utc(&sample, 2024, 13, 1),
             "sys_date_set_date_utc should reject month 13");
  TestAssert(!sys_date_set_date_utc(&sample, 2024, 1, 0),
             "sys_date_set_date_utc should reject day 0");
  TestAssert(!sys_date_set_date_utc(&sample, 2024, 1, 32),
             "sys_date_set_date_utc should reject day 32");

  sys_date_t start = {.seconds = 10, .nanoseconds = 500, .tzoffset = 0};
  sys_date_t end = {.seconds = 12, .nanoseconds = 250, .tzoffset = 0};
  TestAssert(sys_date_compare_ns(&start, &end) == 1999999750LL,
             "sys_date_compare_ns should include both seconds and nanoseconds");
  TestAssert(sys_date_compare_ns(&end, &start) == -1999999750LL,
             "sys_date_compare_ns should preserve sign");
  TestAssert(sys_date_compare_ns(&start, NULL) == 0,
             "sys_date_compare_ns should return 0 for NULL end dates");

#ifdef SYSTEM_NAME_PICO
  sys_date_t before = {0};
  sys_date_t target = {0};
  sys_date_t after = {0};

  TestAssert(sys_date_get_now(&before),
             "Pico sys_date_get_now should succeed before set_now");
  target = before;
  target.seconds += 2;
  target.nanoseconds = 250000000;
  target.tzoffset = 90 * 60;

  TestAssert(sys_date_set_now(&target),
             "Pico sys_date_set_now should succeed without host privileges");
  TestAssert(sys_date_get_now(&after),
             "Pico sys_date_get_now should succeed after set_now");
  TestAssert(abs_i64(sys_date_compare_ns(&target, &after)) < 500000000LL,
             "Pico sys_date_set_now should update the readable clock value");
  TestAssert(after.tzoffset == target.tzoffset,
             "Pico sys_date_set_now should preserve timezone offset");
#endif

  return true;
}

TestMain(test_main)
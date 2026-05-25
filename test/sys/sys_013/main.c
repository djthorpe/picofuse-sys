#include <math.h>
#include <stddef.h>
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

static size_t string_length(const char *str) {
  size_t len = 0;
  while (str[len]) {
    len++;
  }
  return len;
}

static size_t call_sys_vsprintf(char *buf, size_t sz, const char *format, ...) {
  va_list va;
  va_start(va, format);
  size_t len = sys_vsprintf(buf, sz, format, va);
  va_end(va);
  return len;
}

static size_t call_sys_vsprintf_ex(char *buf, size_t sz, const char *format,
                                   sys_printf_format_handler_t custom_handler,
                                   ...) {
  va_list va;
  va_start(va, custom_handler);
  size_t len = sys_vsprintf_ex(buf, sz, format, va, custom_handler);
  va_end(va);
  return len;
}

#define ASSERT_FORMAT(LABEL, EXPECTED, FORMAT, ...)                            \
  do {                                                                         \
    char actual[128];                                                          \
    size_t actual_len =                                                        \
        call_sys_vsprintf(actual, sizeof(actual), FORMAT, ##__VA_ARGS__);      \
    TestAssert(strings_equal(actual, EXPECTED),                                \
               "%s mismatch: expected '%s', got '%s'", LABEL, EXPECTED,        \
               actual);                                                        \
    TestAssert(actual_len == string_length(EXPECTED),                          \
               "%s length mismatch: expected %zu, got %zu", LABEL,             \
               string_length(EXPECTED), actual_len);                           \
  } while (0)

static const char *test_custom_format_handler(char format, va_list *va) {
  (void)va;
  if (format == 'Q') {
    return "custom";
  }
  return NULL;
}

bool test_main(void) {
  char buf[128];
  char expected_ptr[2 + sizeof(void *) * 2 + 1];

  ASSERT_FORMAT("%%c", "Z", "%c", 'Z');
  ASSERT_FORMAT("%%3c", "  Z", "%3c", 'Z');
  ASSERT_FORMAT("%%-3c", "Z  ", "%-3c", 'Z');

  ASSERT_FORMAT("%%s", "hello", "%s", "hello");
  ASSERT_FORMAT("%%-5s", "hi   ", "%-5s", "hi");
  ASSERT_FORMAT("%%7s null", " <null>", "%7s", (const char *)NULL);

  ASSERT_FORMAT("%%d", "-17", "%d", -17);
  ASSERT_FORMAT("%%+d", "+17", "%+d", 17);
  ASSERT_FORMAT("%%05d", "-0042", "%05d", -42);
  ASSERT_FORMAT("%%-05d", "42   ", "%-05d", 42);
  ASSERT_FORMAT("%%ld", "-123456789", "%ld", -123456789L);
  ASSERT_FORMAT("%%zd", "-17", "%zd", (ptrdiff_t)-17);
  ASSERT_FORMAT("%%zu", "17", "%zu", (size_t)17);

  ASSERT_FORMAT("%%u", "42", "%u", 42U);
  ASSERT_FORMAT("%%#x", "0x2a", "%#x", 42U);
  ASSERT_FORMAT("%%#08x", "0x00002a", "%#08x", 42U);
  ASSERT_FORMAT("%%#X", "0X2A", "%#X", 42U);
  ASSERT_FORMAT("%%#b", "0b101", "%#b", 5U);
  ASSERT_FORMAT("%%#o", "011", "%#o", 9U);
  ASSERT_FORMAT("%%%%", "100% ok", "100%% ok");

  ASSERT_FORMAT("%%f", "1.500000", "%f", 1.5);
  ASSERT_FORMAT("%%.2f", "3.14", "%.2f", 3.14159);
  ASSERT_FORMAT("%%+08.2f", "+0001.50", "%+08.2f", 1.5);
  ASSERT_FORMAT("%% f", " 1.500000", "% f", 1.5);
  ASSERT_FORMAT("%%e", "1.250000e+01", "%e", 12.5);
  ASSERT_FORMAT("%%E", "1.250000E-01", "%E", 0.125);
  ASSERT_FORMAT("%%g fixed", "12.34", "%g", 12.34);
  ASSERT_FORMAT("%%.3g exp", "1.23e-05", "%.3g", 0.00001234);
  ASSERT_FORMAT("%%f nan", "nan", "%f", NAN);
  ASSERT_FORMAT("%%F nan", "NAN", "%F", NAN);
  ASSERT_FORMAT("%%f inf", "inf", "%f", INFINITY);
  ASSERT_FORMAT("%%+f inf", "+inf", "%+f", INFINITY);
  ASSERT_FORMAT("%%f -inf", "-inf", "%f", -INFINITY);
  ASSERT_FORMAT("%%E inf", "INF", "%E", INFINITY);

  expected_ptr[0] = '0';
  expected_ptr[1] = 'x';
  for (size_t index = 2; index < sizeof(expected_ptr) - 1; index++) {
    expected_ptr[index] = '0';
  }
  expected_ptr[sizeof(expected_ptr) - 1] = '\0';

  ASSERT_FORMAT("%%p null", expected_ptr, "%p", (void *)NULL);

  size_t len = sys_sprintf(buf, sizeof(buf), "hello world");
  TestAssert(strings_equal(buf, "hello world"),
             "sys_sprintf basic output mismatch: '%s'", buf);
  TestAssert(len == 11, "sys_sprintf basic length should be 11, got %zu", len);

  len = sys_sprintf(buf, 5, "hello world");
  TestAssert(strings_equal(buf, "hell"),
             "sys_sprintf truncation output mismatch: '%s'", buf);
  TestAssert(len == 11, "sys_sprintf truncation length should be 11, got %zu",
             len);

  len = sys_sprintf(NULL, 0, "abc %d", 7);
  TestAssert(len == 5, "sys_sprintf NULL buffer length should be 5, got %zu",
             len);

  len = call_sys_vsprintf(buf, sizeof(buf), "%#x %s %d", 255U, "ok", -3);
  TestAssert(strings_equal(buf, "0xff ok -3"),
             "sys_vsprintf output mismatch: '%s'", buf);
  TestAssert(len == 10, "sys_vsprintf length should be 10, got %zu", len);

  len = sys_sprintf_ex(buf, sizeof(buf), "[%Q]", test_custom_format_handler);
  TestAssert(strings_equal(buf, "[custom]"),
             "custom format handler output mismatch: '%s'", buf);
  TestAssert(len == 8, "custom format length should be 8, got %zu", len);

  len = call_sys_vsprintf_ex(buf, sizeof(buf), "[%Q:%d]",
                             test_custom_format_handler, 9);
  TestAssert(strings_equal(buf, "[custom:9]"),
             "sys_vsprintf_ex output mismatch: '%s'", buf);
  TestAssert(len == 10, "sys_vsprintf_ex length should be 10, got %zu", len);

  return true;
}

TestMain(test_main)
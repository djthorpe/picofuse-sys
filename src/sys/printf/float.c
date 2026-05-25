#include "private.h"
#include <float.h>
#include <stdint.h>

#define SYS_PRINTF_DEFAULT_FLOAT_PRECISION 6U
#define SYS_PRINTF_MAX_FLOAT_PRECISION 9U
#define SYS_PRINTF_FLOAT_BUFFER_SIZE 128U

static const uint64_t _sys_printf_pow10[] = {
    1ULL,      10ULL,      100ULL,      1000ULL,      10000ULL,
    100000ULL, 1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL,
};

static size_t _sys_printf_write_chars(struct sys_printf_state *state,
                                      const char *str, size_t len) {
  size_t total_chars = 0;

  for (size_t index = 0; index < len; index++) {
    total_chars += state->putch(state, str[index]);
  }

  return total_chars;
}

static size_t _sys_printf_emit_formatted(struct sys_printf_state *state,
                                         const char *str, size_t len,
                                         bool zero_pad) {
  size_t total_chars = 0;
  size_t padding = (state->width > len) ? state->width - len : 0;

  if (!(state->flags & SYS_PRINTF_FLAG_LEFT)) {
    if (zero_pad && padding > 0 && len > 0 &&
        (str[0] == '-' || str[0] == '+' || str[0] == ' ')) {
      total_chars += state->putch(state, *str++);
      len--;
    }

    char pad_char = zero_pad ? '0' : ' ';
    for (size_t index = 0; index < padding; index++) {
      total_chars += state->putch(state, pad_char);
    }
  }

  total_chars += _sys_printf_write_chars(state, str, len);

  if (state->flags & SYS_PRINTF_FLAG_LEFT) {
    for (size_t index = 0; index < padding; index++) {
      total_chars += state->putch(state, ' ');
    }
  }

  return total_chars;
}

static unsigned int _sys_printf_precision(const struct sys_printf_state *state,
                                          bool adaptive) {
  unsigned int precision = (state->flags & SYS_PRINTF_FLAG_PRECISION)
                               ? (unsigned int)state->precision
                               : SYS_PRINTF_DEFAULT_FLOAT_PRECISION;

  if (adaptive && precision == 0U) {
    precision = 1U;
  }

  if (precision > SYS_PRINTF_MAX_FLOAT_PRECISION) {
    precision = SYS_PRINTF_MAX_FLOAT_PRECISION;
  }

  return precision;
}

static char _sys_printf_sign_char(const struct sys_printf_state *state,
                                  bool negative) {
  if (negative) {
    return '-';
  }
  if (state->flags & SYS_PRINTF_FLAG_SIGN) {
    return '+';
  }
  if (state->flags & SYS_PRINTF_FLAG_SPACE) {
    return ' ';
  }
  return '\0';
}

static unsigned long long _sys_printf_round_half_even(double value) {
  unsigned long long truncated = (unsigned long long)value;
  double diff = value - (double)truncated;

  if (diff > 0.5) {
    return truncated + 1ULL;
  }
  if (diff < 0.5) {
    return truncated;
  }

  return (truncated & 1ULL) ? (truncated + 1ULL) : truncated;
}

static size_t _sys_printf_write_unsigned(char *dst, unsigned long long value) {
  char reversed[32];
  size_t len = 0;

  do {
    reversed[len++] = (char)('0' + (value % 10ULL));
    value /= 10ULL;
  } while (value > 0ULL);

  for (size_t index = 0; index < len; index++) {
    dst[index] = reversed[len - index - 1];
  }

  return len;
}

static size_t _sys_printf_append_fraction(char *dst, unsigned long long value,
                                          unsigned int precision) {
  for (unsigned int index = 0; index < precision; index++) {
    unsigned int power_index = precision - index - 1;
    dst[index] =
        (char)('0' + ((value / _sys_printf_pow10[power_index]) % 10ULL));
  }

  return (size_t)precision;
}

static size_t _sys_printf_trim_fraction(char *buffer, size_t len,
                                        bool keep_decimal_point) {
  while (len > 0 && buffer[len - 1] == '0') {
    len--;
  }

  if (!keep_decimal_point && len > 0 && buffer[len - 1] == '.') {
    len--;
  }

  return len;
}

static size_t _sys_printf_format_special(struct sys_printf_state *state,
                                         double value, bool upper) {
  const char *word = NULL;
  bool negative = __builtin_signbit(value);

  if (__builtin_isnan(value)) {
    word = upper ? "NAN" : "nan";
    negative = false;
  } else if (__builtin_isinf(value)) {
    word = upper ? "INF" : "inf";
  } else {
    return 0;
  }

  char buffer[8];
  size_t len = 0;
  char sign_char = _sys_printf_sign_char(state, negative);

  if (sign_char) {
    buffer[len++] = sign_char;
  }

  while (*word) {
    buffer[len++] = *word++;
  }

  return _sys_printf_emit_formatted(state, buffer, len, false);
}

static size_t _sys_printf_format_exponential(struct sys_printf_state *state,
                                             double value,
                                             unsigned int precision,
                                             bool trim_trailing, bool upper) {
  char buffer[SYS_PRINTF_FLOAT_BUFFER_SIZE];
  size_t len = 0;
  bool negative = __builtin_signbit(value);
  double abs_value = negative ? -value : value;
  char sign_char = _sys_printf_sign_char(state, negative);
  int exponent = 0;

  if (sign_char) {
    buffer[len++] = sign_char;
  }

  if (abs_value == 0.0) {
    buffer[len++] = '0';
    if (precision > 0U || (state->flags & SYS_PRINTF_FLAG_PREFIX)) {
      buffer[len++] = '.';
      len += _sys_printf_append_fraction(&buffer[len], 0U, precision);
      if (trim_trailing && !(state->flags & SYS_PRINTF_FLAG_PREFIX)) {
        len = _sys_printf_trim_fraction(buffer, len, false);
      }
    }
  } else {
    while (abs_value >= 10.0) {
      abs_value /= 10.0;
      exponent++;
    }
    while (abs_value < 1.0) {
      abs_value *= 10.0;
      exponent--;
    }

    unsigned long long scale = _sys_printf_pow10[precision];
    unsigned long long mantissa =
        _sys_printf_round_half_even(abs_value * (double)scale);

    if (mantissa >= 10ULL * scale) {
      mantissa /= 10ULL;
      exponent++;
    }

    unsigned long long whole = mantissa / scale;
    unsigned long long fraction = mantissa % scale;

    len += _sys_printf_write_unsigned(&buffer[len], whole);
    if (precision > 0U || (state->flags & SYS_PRINTF_FLAG_PREFIX)) {
      buffer[len++] = '.';
      len += _sys_printf_append_fraction(&buffer[len], fraction, precision);
      if (trim_trailing && !(state->flags & SYS_PRINTF_FLAG_PREFIX)) {
        len = _sys_printf_trim_fraction(buffer, len, false);
      }
    }
  }

  buffer[len++] = upper ? 'E' : 'e';
  buffer[len++] = exponent < 0 ? '-' : '+';

  unsigned int abs_exponent =
      (unsigned int)(exponent < 0 ? -exponent : exponent);
  char exponent_digits[8];
  size_t exponent_len =
      _sys_printf_write_unsigned(exponent_digits, abs_exponent);

  for (size_t index = exponent_len; index < 2U; index++) {
    buffer[len++] = '0';
  }

  for (size_t index = 0; index < exponent_len; index++) {
    buffer[len++] = exponent_digits[index];
  }

  return _sys_printf_emit_formatted(state, buffer, len,
                                    (state->flags & SYS_PRINTF_FLAG_PAD) &&
                                        !(state->flags & SYS_PRINTF_FLAG_LEFT));
}

static size_t _sys_printf_format_fixed(struct sys_printf_state *state,
                                       double value, unsigned int precision,
                                       bool trim_trailing, bool upper) {
  if (value > (double)UINT64_MAX || value < -(double)UINT64_MAX) {
    return _sys_printf_format_exponential(state, value, precision,
                                          trim_trailing, upper);
  }

  char buffer[SYS_PRINTF_FLOAT_BUFFER_SIZE];
  size_t len = 0;
  bool negative = __builtin_signbit(value);
  double abs_value = negative ? -value : value;
  char sign_char = _sys_printf_sign_char(state, negative);
  unsigned long long whole = (unsigned long long)abs_value;
  unsigned long long fraction = 0;

  if (precision > 0U) {
    double scaled_fraction =
        (abs_value - (double)whole) * (double)_sys_printf_pow10[precision];
    fraction = _sys_printf_round_half_even(scaled_fraction);

    if (fraction >= _sys_printf_pow10[precision]) {
      fraction = 0U;
      whole++;
    }
  } else {
    whole = _sys_printf_round_half_even(abs_value);
  }

  if (sign_char) {
    buffer[len++] = sign_char;
  }

  len += _sys_printf_write_unsigned(&buffer[len], whole);

  if (precision > 0U || (state->flags & SYS_PRINTF_FLAG_PREFIX)) {
    buffer[len++] = '.';
    len += _sys_printf_append_fraction(&buffer[len], fraction, precision);
    if (trim_trailing && !(state->flags & SYS_PRINTF_FLAG_PREFIX)) {
      len = _sys_printf_trim_fraction(buffer, len, false);
    }
  }

  return _sys_printf_emit_formatted(state, buffer, len,
                                    (state->flags & SYS_PRINTF_FLAG_PAD) &&
                                        !(state->flags & SYS_PRINTF_FLAG_LEFT));
}

size_t _sys_printf_putf(struct sys_printf_state *state, char spec,
                        va_list *va) {
  double value = va_arg(*va, double);
  bool upper = (spec == 'E' || spec == 'F' || spec == 'G');

  if (__builtin_isnan(value) || __builtin_isinf(value)) {
    return _sys_printf_format_special(state, value, upper);
  }

  if (spec == 'f' || spec == 'F') {
    return _sys_printf_format_fixed(
        state, value, _sys_printf_precision(state, false), false, upper);
  }

  if (spec == 'e' || spec == 'E') {
    return _sys_printf_format_exponential(
        state, value, _sys_printf_precision(state, false), false, upper);
  }

  unsigned int significant_digits = _sys_printf_precision(state, true);
  double abs_value = __builtin_signbit(value) ? -value : value;
  int exponent = 0;

  if (abs_value > 0.0) {
    while (abs_value >= 10.0) {
      abs_value /= 10.0;
      exponent++;
    }
    while (abs_value < 1.0) {
      abs_value *= 10.0;
      exponent--;
    }
  }

  if (abs_value == 0.0 ||
      (exponent >= -4 && exponent < (int)significant_digits)) {
    unsigned int precision =
        (unsigned int)((int)significant_digits - exponent - 1);
    return _sys_printf_format_fixed(state, value, precision, true, upper);
  }

  return _sys_printf_format_exponential(state, value, significant_digits - 1U,
                                        true, upper);
}
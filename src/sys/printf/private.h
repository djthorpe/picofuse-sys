#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

/**
 * @brief Initializes the printf mutex for thread-safe printing.
 */
extern void _sys_printf_module_init(void);

/**
 * @brief Finalizes the printf mutex and cleans up resources.
 */
extern void _sys_printf_module_exit(void);

///////////////////////////////////////////////////////////////////////////////
// TYPES

typedef enum {
  SYS_PRINTF_FLAG_SIZET = 1 << 0,  /**< Flag for size_t specifier */
  SYS_PRINTF_FLAG_LONG = 1 << 1,   /**< Flag for long integer specifier */
  SYS_PRINTF_FLAG_LEFT = 1 << 2,   /**< Flag for left alignment */
  SYS_PRINTF_FLAG_SIGN = 1 << 3,   /**< Flag to force numeric sign */
  SYS_PRINTF_FLAG_PREFIX = 1 << 4, /**< Flag to force 0, 0x or 0b prefix */
  SYS_PRINTF_FLAG_PAD = 1 << 5,    /**< Flag for zero-padding */
  SYS_PRINTF_FLAG_NEG = 1 << 6,    /**< Flag to process as negative number */
  SYS_PRINTF_FLAG_HEX = 1 << 7,    /**< Flag for hexadecimal output */
  SYS_PRINTF_FLAG_BIN = 1 << 8,    /**< Flag for binary output */
  SYS_PRINTF_FLAG_OCT = 1 << 9,    /**< Flag for octal output */
  SYS_PRINTF_FLAG_UPPER = 1 << 10, /**< Flag for uppercase output */
  SYS_PRINTF_FLAG_SPACE =
      1 << 11, /**< Flag to prefix positive values with a space */
  SYS_PRINTF_FLAG_PRECISION = 1 << 12 /**< Flag for explicit precision */
} sys_printf_flags_t;

struct sys_printf_state {
  char *buffer; /**< Buffer for formatted output */
  size_t size;  /**< Size of the buffer, including null terminator */
  size_t pos;   /**< Current position in the buffer */
  size_t (*putch)(struct sys_printf_state *state,
                  char ch); /**< Function to output a character */
  const char *(*custom)(char format, va_list *va); /**< Custom format handler */
  size_t width;             /**< Width specifier for padding */
  size_t precision;         /**< Precision specifier for formatting */
  sys_printf_flags_t flags; /**< Current format flags */
};

extern size_t _sys_printf_putf(struct sys_printf_state *state, char spec,
                               va_list *va);

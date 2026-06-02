#include <limits.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static inline void _sys_random_init(void) {
  static bool init = false;

  if (!init) {
    srand((unsigned int)time(NULL));
    init = true;
  }
}

/**
 * @brief Returns a random number as a 32-bit unsigned integer.
 * @note This function is not thread-safe.
 */
uint32_t sys_random_uint32(void) {
  _sys_random_init();

  if (RAND_MAX == INT32_MAX) {
    return ((uint32_t)rand() & 0xFFFFu) << 16 | ((uint32_t)rand() & 0xFFFFu);
  }

  sys_panicf("sys_random_uint32: RAND_MAX is too small");
}

/**
 * @brief Returns a random number as a 64-bit unsigned integer.
 * @note This function is not thread-safe.
 */
uint64_t sys_random_uint64(void) {
  _sys_random_init();

  uint64_t result = 0;
  for (int i = 0; i < 4; i++) {
    result = (result << 16) | ((uint64_t)rand() & 0xFFFFu);
  }

  return result;
}

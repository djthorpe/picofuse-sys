#include <picofuse/sys.h>

#include <openssl/rand.h>
#include <stdint.h>

static void _sys_random_fill(void *dst, size_t size) {
  if (RAND_bytes((unsigned char *)dst, (int)size) != 1) {
    sys_panicf("RAND_bytes failed");
  }
}

/**
 * @brief Returns a random number as a 32-bit unsigned integer.
 */
uint32_t sys_random_uint32(void) {
  uint32_t value = 0;
  _sys_random_fill(&value, sizeof(value));
  return value;
}

/**
 * @brief Returns a random number as a 64-bit unsigned integer.
 */
uint64_t sys_random_uint64(void) {
  uint64_t value = 0;
  _sys_random_fill(&value, sizeof(value));
  return value;
}

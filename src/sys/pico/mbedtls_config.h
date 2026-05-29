#pragma once

/* Some mbedTLS sources assume INT_MAX is visible through the config header. */
#include <limits.h>

/* Bare-metal Pico builds do not provide Unix/Windows entropy polling hooks. */
#define MBEDTLS_NO_PLATFORM_ENTROPY

/* Use the SDK's hardware entropy source when the broader stack needs it. */
#define MBEDTLS_ENTROPY_HARDWARE_ALT

/* Route any dynamic mbedTLS allocations through the system allocator hooks. */
#define MBEDTLS_PLATFORM_MEMORY

/* Keep the digest modules enabled for this hash backend. */
#define MBEDTLS_MD5_C
#define MBEDTLS_SHA256_C

/* Pico hashing uses a single active handle, so the hardware SHA-256 backend
 * can be used safely here.
 */
#if LIB_PICO_SHA256
#define MBEDTLS_SHA256_ALT
#endif

#include <mbedtls/mbedtls_config.h>

/* This backend does not rely on libc-backed facilities. */
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_TIMING_C

#include "private.h"
#include <limits.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>
#include <pico/critical_section.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_hash_t {
  union {
    mbedtls_md5_context md5;
    mbedtls_sha256_context sha256;
  } ctx;
  uint8_t digest[SYS_HASH_SIZE];
  size_t size;
  sys_hash_algorithm_t algorithm;
  bool init;
};

static critical_section_t _sys_hash_pool_lock;
static sys_hash_t _sys_hash_pool[SYS_HASH_CAPACITY];
static size_t _sys_hash_pool_next_index = 0;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _sys_hash_valid(const sys_hash_t *hash);
static size_t _sys_hash_digest_size(sys_hash_algorithm_t algorithm);
static bool _sys_hash_init_handle(sys_hash_t *hash,
                                  sys_hash_algorithm_t algorithm);
static void _sys_hash_free_handle(sys_hash_t *hash);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and initializes a hash context from the static pool. */
sys_hash_t *sys_hash_init(sys_hash_algorithm_t algorithm) {
  size_t digest_size = _sys_hash_digest_size(algorithm);
  if (digest_size == 0 || digest_size > SYS_HASH_SIZE) {
    return NULL;
  }

  critical_section_enter_blocking(&_sys_hash_pool_lock);

  for (size_t offset = 0; offset < SYS_HASH_CAPACITY; offset++) {
    size_t index = (_sys_hash_pool_next_index + offset) % SYS_HASH_CAPACITY;
    sys_hash_t *hash = &_sys_hash_pool[index];
    if (hash->init) {
      continue;
    }

    hash->size = digest_size;
    hash->algorithm = algorithm;
    hash->init = true;
    sys_memset(hash->digest, 0, sizeof(hash->digest));

    if (!_sys_hash_init_handle(hash, algorithm)) {
      hash->size = 0;
      hash->algorithm = 0;
      hash->init = false;
      critical_section_exit(&_sys_hash_pool_lock);
      return NULL;
    }

    _sys_hash_pool_next_index = (index + 1) % SYS_HASH_CAPACITY;
    critical_section_exit(&_sys_hash_pool_lock);
    return hash;
  }

  critical_section_exit(&_sys_hash_pool_lock);
  return NULL;
}

/** @brief Releases a hash context and returns its pool slot. */
void sys_hash_deinit(sys_hash_t *hash) {
  sys_assert(_sys_hash_valid(hash));
  if (!_sys_hash_valid(hash)) {
    return;
  }

  critical_section_enter_blocking(&_sys_hash_pool_lock);

  _sys_hash_free_handle(hash);
  hash->size = 0;
  hash->algorithm = 0;
  hash->init = false;
  sys_memset(hash->digest, 0, sizeof(hash->digest));

  critical_section_exit(&_sys_hash_pool_lock);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/** @brief Returns the digest size for an initialized hash context. */
size_t sys_hash_size(const sys_hash_t *hash) {
  if (!_sys_hash_valid(hash)) {
    return 0;
  }

  return hash->size;
}

/** @brief Updates the hash context with more data. */
bool sys_hash_update(sys_hash_t *hash, const void *data, size_t size) {
  sys_assert(_sys_hash_valid(hash));

  if (!_sys_hash_valid(hash) || hash->algorithm == 0) {
    return false;
  }

  if (size == 0) {
    return true;
  }

  if (data == NULL) {
    return false;
  }

  switch (hash->algorithm) {
  case sys_hash_md5: {
    int rc = mbedtls_md5_update(&hash->ctx.md5, data, size);
    if (rc != 0) {
      sys_printf("sys_hash_update(md5, %zu) failed: %d\n", size, rc);
    }
    return rc == 0;
  }
  case sys_hash_sha256: {
    int rc = mbedtls_sha256_update(&hash->ctx.sha256, data, size);
    if (rc != 0) {
      sys_printf("sys_hash_update(sha256, %zu) failed: %d\n", size, rc);
    }
    return rc == 0;
  }
  default:
    return false;
  }
}

/** @brief Finalizes the digest and returns a pointer to the stored bytes. */
const uint8_t *sys_hash_finalize(sys_hash_t *hash) {
  sys_assert(_sys_hash_valid(hash));

  if (!_sys_hash_valid(hash)) {
    return NULL;
  }

  if (hash->algorithm == 0) {
    return hash->digest;
  }

  bool ok = false;
  int rc = 0;
  switch (hash->algorithm) {
  case sys_hash_md5:
    rc = mbedtls_md5_finish(&hash->ctx.md5, hash->digest);
    ok = rc == 0;
    mbedtls_md5_free(&hash->ctx.md5);
    break;
  case sys_hash_sha256:
    rc = mbedtls_sha256_finish(&hash->ctx.sha256, hash->digest);
    ok = rc == 0;
    mbedtls_sha256_free(&hash->ctx.sha256);
    break;
  default:
    break;
  }

  if (!ok) {
    sys_printf("sys_hash_finalize(%u) failed: %d\n", (unsigned)hash->algorithm,
               rc);
    sys_memset(hash->digest, 0, sizeof(hash->digest));
    return NULL;
  }

  hash->algorithm = 0;
  return hash->digest;
}

/** @brief Computes a djb2 hash for a NULL-terminated string. */
uintptr_t sys_hash_djb2(const char *str) {
  if (str == NULL) {
    return 0;
  }

  uintptr_t hash = 5381;
  while (*str != '\0') {
    hash = ((hash << 5) + hash) + (unsigned char)*str;
    str++;
  }

  return hash;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Returns true when a hash handle is initialized. */
static bool _sys_hash_valid(const sys_hash_t *hash) {
  return hash != NULL && hash->init;
}

/** @brief Returns the digest size for a supported hash algorithm. */
static size_t _sys_hash_digest_size(sys_hash_algorithm_t algorithm) {
  switch (algorithm) {
  case sys_hash_md5:
    return 16;
  case sys_hash_sha256:
    return 32;
  default:
    return 0;
  }
}

/** @brief Initializes the algorithm-specific mbedTLS state. */
static bool _sys_hash_init_handle(sys_hash_t *hash,
                                  sys_hash_algorithm_t algorithm) {
  switch (algorithm) {
  case sys_hash_md5:
    mbedtls_md5_init(&hash->ctx.md5);
    {
      int rc = mbedtls_md5_starts(&hash->ctx.md5);
      if (rc != 0) {
        sys_printf("sys_hash_init(md5) start failed: %d\n", rc);
      }
      return rc == 0;
    }
  case sys_hash_sha256:
    mbedtls_sha256_init(&hash->ctx.sha256);
    {
      int rc = mbedtls_sha256_starts(&hash->ctx.sha256, 0);
      if (rc != 0) {
        sys_printf("sys_hash_init(sha256) start failed: %d\n", rc);
      }
      return rc == 0;
    }
  default:
    return false;
  }
}

/** @brief Releases the algorithm-specific mbedTLS state if still active. */
static void _sys_hash_free_handle(sys_hash_t *hash) {
  switch (hash->algorithm) {
  case sys_hash_md5:
    mbedtls_md5_free(&hash->ctx.md5);
    break;
  case sys_hash_sha256:
    mbedtls_sha256_free(&hash->ctx.sha256);
    break;
  default:
    break;
  }
}

/** @brief Initializes the Pico hash pool lock. */
void _sys_hash_module_init(void) {
  critical_section_init(&_sys_hash_pool_lock);
}
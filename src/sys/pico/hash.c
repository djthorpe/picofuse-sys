#include "private.h"
#include <limits.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>
#include <pico/mutex.h>
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

static mutex_t _sys_hash_lock;
static sys_hash_t _sys_hash_handle;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _sys_hash_owned(const sys_hash_t *hash);
static bool _sys_hash_valid(const sys_hash_t *hash);
static size_t _sys_hash_digest_size(sys_hash_algorithm_t algorithm);
static bool _sys_hash_init_handle(sys_hash_t *hash,
                                  sys_hash_algorithm_t algorithm);
static void _sys_hash_free_handle(sys_hash_t *hash);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and initializes the single Pico hash context. */
sys_hash_t *sys_hash_init(sys_hash_algorithm_t algorithm) {
  size_t digest_size = _sys_hash_digest_size(algorithm);
  if (digest_size == 0 || digest_size > SYS_HASH_SIZE) {
    return NULL;
  }

  mutex_enter_blocking(&_sys_hash_lock);

  if (_sys_hash_handle.init) {
    mutex_exit(&_sys_hash_lock);
    return NULL;
  }

  _sys_hash_handle.size = digest_size;
  _sys_hash_handle.algorithm = algorithm;
  _sys_hash_handle.init = true;
  sys_memset(_sys_hash_handle.digest, 0, sizeof(_sys_hash_handle.digest));

  if (!_sys_hash_init_handle(&_sys_hash_handle, algorithm)) {
    _sys_hash_handle.size = 0;
    _sys_hash_handle.algorithm = 0;
    _sys_hash_handle.init = false;
    mutex_exit(&_sys_hash_lock);
    return NULL;
  }

  mutex_exit(&_sys_hash_lock);
  return &_sys_hash_handle;
}

/** @brief Releases the single Pico hash context. */
void sys_hash_deinit(sys_hash_t *hash) {
  sys_assert(_sys_hash_owned(hash));
  if (!_sys_hash_owned(hash)) {
    return;
  }

  mutex_enter_blocking(&_sys_hash_lock);

  if (!_sys_hash_valid(hash)) {
    mutex_exit(&_sys_hash_lock);
    return;
  }

  _sys_hash_free_handle(hash);
  hash->size = 0;
  hash->algorithm = 0;
  hash->init = false;
  sys_memset(hash->digest, 0, sizeof(hash->digest));

  mutex_exit(&_sys_hash_lock);
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS

/** @brief Returns the digest size for an initialized hash context. */
size_t sys_hash_size(const sys_hash_t *hash) {
  if (!_sys_hash_owned(hash)) {
    return 0;
  }

  mutex_enter_blocking(&_sys_hash_lock);
  size_t size = _sys_hash_valid(hash) ? hash->size : 0;
  mutex_exit(&_sys_hash_lock);
  return size;
}

/** @brief Updates the hash context with more data. */
bool sys_hash_update(sys_hash_t *hash, const void *data, size_t size) {
  sys_assert(_sys_hash_owned(hash));

  if (!_sys_hash_owned(hash) || data == NULL) {
    if (size == 0 && _sys_hash_owned(hash)) {
      data = "";
    } else {
      return false;
    }
  }

  mutex_enter_blocking(&_sys_hash_lock);

  if (!_sys_hash_valid(hash) || hash->algorithm == 0) {
    mutex_exit(&_sys_hash_lock);
    return false;
  }

  if (size == 0) {
    mutex_exit(&_sys_hash_lock);
    return true;
  }

  bool ok = false;

  switch (hash->algorithm) {
  case sys_hash_md5:
    ok = mbedtls_md5_update(&hash->ctx.md5, data, size) == 0;
    break;
  case sys_hash_sha256:
    ok = mbedtls_sha256_update(&hash->ctx.sha256, data, size) == 0;
    break;
  default:
    break;
  }

  mutex_exit(&_sys_hash_lock);
  return ok;
}

/** @brief Finalizes the digest and returns a pointer to the stored bytes. */
const uint8_t *sys_hash_finalize(sys_hash_t *hash) {
  sys_assert(_sys_hash_owned(hash));

  if (!_sys_hash_owned(hash)) {
    return NULL;
  }

  mutex_enter_blocking(&_sys_hash_lock);

  if (!_sys_hash_valid(hash)) {
    mutex_exit(&_sys_hash_lock);
    return NULL;
  }

  if (hash->algorithm == 0) {
    const uint8_t *digest = hash->digest;
    mutex_exit(&_sys_hash_lock);
    return digest;
  }

  bool ok = false;
  switch (hash->algorithm) {
  case sys_hash_md5:
    ok = mbedtls_md5_finish(&hash->ctx.md5, hash->digest) == 0;
    mbedtls_md5_free(&hash->ctx.md5);
    break;
  case sys_hash_sha256:
    ok = mbedtls_sha256_finish(&hash->ctx.sha256, hash->digest) == 0;
    mbedtls_sha256_free(&hash->ctx.sha256);
    break;
  default:
    break;
  }

  hash->algorithm = 0;

  if (!ok) {
    hash->size = 0;
    sys_memset(hash->digest, 0, sizeof(hash->digest));
    mutex_exit(&_sys_hash_lock);
    return NULL;
  }

  const uint8_t *digest = hash->digest;
  mutex_exit(&_sys_hash_lock);
  return digest;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

/** @brief Returns true when a hash pointer refers to the Pico hash handle. */
static bool _sys_hash_owned(const sys_hash_t *hash) {
  return hash == &_sys_hash_handle;
}

/** @brief Returns true when a hash handle is initialized. */
static bool _sys_hash_valid(const sys_hash_t *hash) {
  return _sys_hash_owned(hash) && hash->init;
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
    if (mbedtls_md5_starts(&hash->ctx.md5) != 0) {
      mbedtls_md5_free(&hash->ctx.md5);
      return false;
    }
    return true;
  case sys_hash_sha256:
    mbedtls_sha256_init(&hash->ctx.sha256);
    if (mbedtls_sha256_starts(&hash->ctx.sha256, 0) != 0) {
      mbedtls_sha256_free(&hash->ctx.sha256);
      return false;
    }
    return true;
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

/** @brief Initializes the Pico hash lock. */
void _sys_hash_module_init(void) { mutex_init(&_sys_hash_lock); }
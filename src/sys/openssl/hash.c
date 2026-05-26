#include <openssl/evp.h>
#include <picofuse/sys.h>
#include <pthread.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct sys_hash_t {
  EVP_MD_CTX *ctx;
  sys_hash_algorithm_t algorithm;
  uint8_t digest[SYS_HASH_SIZE];
  size_t size;
  bool init;
};

static pthread_mutex_t _sys_hash_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static sys_hash_t _sys_hash_pool[SYS_HASH_CAPACITY];
static size_t _sys_hash_pool_next_index = 0;

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static const EVP_MD *_sys_hash_evp_md(sys_hash_algorithm_t algorithm,
                                      size_t *size);
static bool _sys_hash_valid(const sys_hash_t *hash);

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

/** @brief Allocates and initializes a hash context from the static pool. */
sys_hash_t *sys_hash_init(sys_hash_algorithm_t algorithm) {
  size_t digest_size = 0;
  const EVP_MD *md = _sys_hash_evp_md(algorithm, &digest_size);
  if (md == NULL || digest_size > SYS_HASH_SIZE) {
    return NULL;
  }

  if (pthread_mutex_lock(&_sys_hash_pool_lock) != 0) {
    return NULL;
  }

  for (size_t offset = 0; offset < SYS_HASH_CAPACITY; offset++) {
    size_t index = (_sys_hash_pool_next_index + offset) % SYS_HASH_CAPACITY;
    sys_hash_t *hash = &_sys_hash_pool[index];
    if (hash->init) {
      continue;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
      pthread_mutex_unlock(&_sys_hash_pool_lock);
      return NULL;
    }

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
      EVP_MD_CTX_free(ctx);
      pthread_mutex_unlock(&_sys_hash_pool_lock);
      return NULL;
    }

    hash->ctx = ctx;
    hash->algorithm = algorithm;
    hash->size = digest_size;
    hash->init = true;
    sys_memset(hash->digest, 0, sizeof(hash->digest));

    _sys_hash_pool_next_index = (index + 1) % SYS_HASH_CAPACITY;
    pthread_mutex_unlock(&_sys_hash_pool_lock);
    return hash;
  }

  pthread_mutex_unlock(&_sys_hash_pool_lock);
  return NULL;
}

/** @brief Releases a hash context and returns its pool slot. */
void sys_hash_deinit(sys_hash_t *hash) {
  sys_assert(_sys_hash_valid(hash));

  int lock_result = pthread_mutex_lock(&_sys_hash_pool_lock);
  sys_assert(lock_result == 0);
  if (lock_result != 0) {
    return;
  }

  if (hash->ctx != NULL) {
    EVP_MD_CTX_free(hash->ctx);
  }

  hash->ctx = NULL;
  hash->algorithm = 0;
  hash->size = 0;
  hash->init = false;
  sys_memset(hash->digest, 0, sizeof(hash->digest));

  int unlock_result = pthread_mutex_unlock(&_sys_hash_pool_lock);
  sys_assert(unlock_result == 0);
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

  if (!_sys_hash_valid(hash) || hash->ctx == NULL) {
    return false;
  }

  if (size == 0) {
    return true;
  }

  if (data == NULL || hash->ctx == NULL) {
    return false;
  }

  return EVP_DigestUpdate(hash->ctx, data, size) == 1;
}

/** @brief Finalizes the digest and returns a pointer to the stored bytes. */
const uint8_t *sys_hash_finalize(sys_hash_t *hash) {
  sys_assert(_sys_hash_valid(hash));

  if (!_sys_hash_valid(hash)) {
    return NULL;
  }

  if (hash->ctx == NULL) {
    return hash->digest;
  }

  unsigned int digest_size = 0;
  bool ok = EVP_DigestFinal_ex(hash->ctx, hash->digest, &digest_size) == 1;
  EVP_MD_CTX_free(hash->ctx);
  hash->ctx = NULL;

  if (!ok || digest_size != hash->size) {
    sys_memset(hash->digest, 0, sizeof(hash->digest));
    return NULL;
  }

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

/** @brief Maps a public hash algorithm to its OpenSSL EVP descriptor. */
static const EVP_MD *_sys_hash_evp_md(sys_hash_algorithm_t algorithm,
                                      size_t *size) {
  switch (algorithm) {
  case sys_hash_md5:
    *size = 16;
    return EVP_md5();
  case sys_hash_sha256:
    *size = 32;
    return EVP_sha256();
  default:
    *size = 0;
    return NULL;
  }
}

/** @brief Returns true when a hash handle is initialized. */
static bool _sys_hash_valid(const sys_hash_t *hash) {
  return hash != NULL && hash->init;
}
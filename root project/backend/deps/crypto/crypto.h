#ifndef _SERVECOSYS_CRYPTO_H_
#define _SERVECOSYS_CRYPTO_H_

#include <stddef.h>
#include <stdint.h>

#define CRYPTO_SHA256_DIGEST_SIZE 32
#define CRYPTO_SHA256_BLOCK_SIZE  64
#define CRYPTO_RSA2048_KEY_SIZE   256

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buffer[CRYPTO_SHA256_BLOCK_SIZE];
    size_t   buffer_offset;
} crypto_sha256_ctx_t;

void crypto_sha256_init(crypto_sha256_ctx_t *ctx);
void crypto_sha256_update(crypto_sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void crypto_sha256_final(crypto_sha256_ctx_t *ctx, uint8_t *digest);
void crypto_sha256(const uint8_t *data, size_t len, uint8_t *digest);

int  crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                        const uint8_t *data, size_t data_len,
                        uint8_t *out_mac);

int  crypto_rsa2048_verify(const uint8_t *modulus, size_t mod_len,
                           uint32_t exponent,
                           const uint8_t *digest, size_t digest_size,
                           const uint8_t *signature);

int  crypto_random_bytes(uint8_t *buffer, size_t len);

#endif

#ifndef _SERVECOSYS_CRYPTO_H_
#define _SERVECOSYS_CRYPTO_H_

#include <efi.h>
#include <efilib.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

#define RSA2048_KEY_SIZE   256
#define RSA_PKCS1_PAD_SIZE 11

typedef struct {
    UINT32 state[8];
    UINT64 bit_count;
    UINT8  buffer[SHA256_BLOCK_SIZE];
    UINTN  buffer_offset;
} sha256_ctx_t;

typedef struct {
    UINT8  modulus[RSA2048_KEY_SIZE];
    UINT32 exponent;
    UINTN  modulus_size;
} rsa_pubkey_t;

VOID sha256_init(sha256_ctx_t *ctx);
VOID sha256_update(sha256_ctx_t *ctx, const UINT8 *data, UINTN len);
VOID sha256_final(sha256_ctx_t *ctx, UINT8 *digest);

BOOLEAN rsa2048_verify(const rsa_pubkey_t *key, const UINT8 *digest, UINTN digest_size, const UINT8 *signature);
BOOLEAN sha256_hmac(const UINT8 *key, UINTN key_len, const UINT8 *data, UINTN data_len, UINT8 *out_mac);

#endif

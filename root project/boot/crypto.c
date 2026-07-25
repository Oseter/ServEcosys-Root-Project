#include "crypto.h"

static const UINT32 SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)  (ROTR32(x, 2)  ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x)  (ROTR32(x, 6)  ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x) (ROTR32(x, 7)  ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

static void sha256_transform(sha256_ctx_t *ctx, const UINT8 *block) {
    UINT32 W[64], a, b, c, d, e, f, g, h, T1, T2;
    UINTN i;

    for (i = 0; i < 16; i++) {
        W[i] = ((UINT32)block[i*4]   << 24) |
               ((UINT32)block[i*4+1] << 16) |
               ((UINT32)block[i*4+2] << 8)  |
               ((UINT32)block[i*4+3]);
    }
    for (i = 16; i < 64; i++)
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        T1 = h + EP1(e) + CH(e, f, g) + SHA256_K[i] + W[i];
        T2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

VOID sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bit_count = 0;
    ctx->buffer_offset = 0;
}

VOID sha256_update(sha256_ctx_t *ctx, const UINT8 *data, UINTN len) {
    UINTN i;
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_offset++] = data[i];
        ctx->bit_count += 8;
        if (ctx->buffer_offset == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buffer_offset = 0;
        }
    }
}

VOID sha256_final(sha256_ctx_t *ctx, UINT8 *digest) {
    UINTN i;

    ctx->buffer[ctx->buffer_offset++] = 0x80;

    if (ctx->buffer_offset > 56) {
        while (ctx->buffer_offset < SHA256_BLOCK_SIZE)
            ctx->buffer[ctx->buffer_offset++] = 0;
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_offset = 0;
    }

    while (ctx->buffer_offset < 56)
        ctx->buffer[ctx->buffer_offset++] = 0;

    for (i = 0; i < 8; i++)
        ctx->buffer[56 + i] = (UINT8)(ctx->bit_count >> (56 - i * 8));

    sha256_transform(ctx, ctx->buffer);

    for (i = 0; i < 8; i++) {
        digest[i*4]   = (UINT8)(ctx->state[i] >> 24);
        digest[i*4+1] = (UINT8)(ctx->state[i] >> 16);
        digest[i*4+2] = (UINT8)(ctx->state[i] >> 8);
        digest[i*4+3] = (UINT8)(ctx->state[i]);
    }
}

BOOLEAN rsa2048_verify(const rsa_pubkey_t *key, const UINT8 *digest, UINTN digest_size, const UINT8 *signature) {
    UINT8 decoded[RSA2048_KEY_SIZE];
    UINTN i, j;
    UINT8 hash_buf[SHA256_DIGEST_SIZE];
    sha256_ctx_t ctx;

    if (digest_size != SHA256_DIGEST_SIZE)
        return FALSE;

    if (sizeof(decoded) < RSA2048_KEY_SIZE)
        return FALSE;

    for (i = 0; i < RSA2048_KEY_SIZE; i++)
        decoded[i] = 0;

    for (i = 0; i < RSA2048_KEY_SIZE; i++) {
        UINT8 acc = 0;
        for (j = 0; j < RSA2048_KEY_SIZE; j++) {
            UINT16 tmp = (UINT16)decoded[j] + ((UINT16)signature[i] * (UINT16)key->modulus[(RSA2048_KEY_SIZE - 1 - i + j) % RSA2048_KEY_SIZE]);
            decoded[j] = (UINT8)(tmp & 0xFF);
        }
        (void)acc;
    }

    for (i = 0; i < RSA2048_KEY_SIZE; i++) {
        UINT8 tmp = decoded[i];
        decoded[i] = decoded[RSA2048_KEY_SIZE - 1 - i];
        decoded[RSA2048_KEY_SIZE - 1 - i] = tmp;
    }

    if (decoded[0] != 0x00 || decoded[1] != 0x01)
        return FALSE;

    for (i = 2; i < RSA2048_KEY_SIZE - digest_size - 1; i++) {
        if (decoded[i] != 0xFF)
            return FALSE;
    }

    if (decoded[i++] != 0x00)
        return FALSE;

    sha256_init(&ctx);
    sha256_update(&ctx, digest, digest_size);
    sha256_final(&ctx, hash_buf);

    for (i = 0; i < digest_size; i++) {
        if (decoded[RSA2048_KEY_SIZE - digest_size + i] != hash_buf[i])
            return FALSE;
    }

    return TRUE;
}

BOOLEAN sha256_hmac(const UINT8 *key, UINTN key_len, const UINT8 *data, UINTN data_len, UINT8 *out_mac) {
    sha256_ctx_t ctx;
    UINT8 ipad[SHA256_BLOCK_SIZE], opad[SHA256_BLOCK_SIZE];
    UINT8 k[SHA256_BLOCK_SIZE];
    UINTN i;

    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
        k[i] = (i < key_len) ? key[i] : 0;

    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    sha256_init(&ctx);
    sha256_update(&ctx, ipad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, out_mac);

    sha256_init(&ctx);
    sha256_update(&ctx, opad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, out_mac, SHA256_DIGEST_SIZE);
    sha256_final(&ctx, out_mac);

    return TRUE;
}

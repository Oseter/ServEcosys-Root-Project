#include "crypto.h"

/* DER-encoded DigestInfo prefix for SHA-256 (rsp #2.2.1, RFC 3447) */
static const UINT8 SHA256_DIGEST_INFO[19] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20
};

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

/*
 * Fixed-width big integers for modular exponentiation (RSA-2048).
 * Numbers are stored little-endian by 32-bit limb (v[0] = least significant).
 * BN     = limbs needed for a 2048-bit modulus (64).
 * BN_PROD = limbs for a product (128).
 */
#define BN        64
#define BN_PROD   128

static int bn_cmp(const UINT32 *a, const UINT32 *b, int n)
{
    int i;
    for (i = n - 1; i >= 0; i--) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
    }
    return 0;
}

static void bn_sub(UINT32 *a, const UINT32 *b, int n)
{
    UINT64 borrow = 0;
    int i;
    for (i = 0; i < n; i++) {
        UINT64 cur = (UINT64)a[i] - b[i] - borrow;
        a[i] = (UINT32)cur;
        borrow = (UINT32)(cur >> 32) & 1;
    }
}

/* r = (r << 1) | bit  (bit is 0 or 1) */
static void bn_shl1(UINT32 *r, int n, int bit)
{
    UINT32 carry = bit ? 1 : 0;
    int i;
    for (i = 0; i < n; i++) {
        UINT64 cur = ((UINT64)r[i] << 1) | carry;
        r[i] = (UINT32)cur;
        carry = (UINT32)(cur >> 32);
    }
}

/* out = a * b ; a,b are n limbs, out is 2n limbs */
static void bn_mul(const UINT32 *a, const UINT32 *b, int n, UINT32 *out)
{
    int i, j;
    for (i = 0; i < 2 * n; i++) out[i] = 0;
    for (i = 0; i < n; i++) {
        UINT32 carry = 0;
        for (j = 0; j < n; j++) {
            UINT64 cur = (UINT64)out[i + j] + (UINT64)a[i] * b[j] + carry;
            out[i + j] = (UINT32)cur;
            carry = (UINT32)(cur >> 32);
        }
        out[i + n] = carry;
    }
}

/* r = in mod m ; in is in_limbs wide, m and r are mn limbs */
static void bn_reduce(const UINT32 *in, int in_limbs,
                      const UINT32 *m, int mn, UINT32 *r)
{
    int i;
    for (i = 0; i < mn; i++) r[i] = 0;
    for (i = in_limbs * 32 - 1; i >= 0; i--) {
        int bit = (in[i >> 5] >> (i & 31)) & 1;
        int top = (r[mn - 1] >> 31) & 1;   /* bit shifted out of the window */
        bn_shl1(r, mn, bit);
        if (top || bn_cmp(r, m, mn) >= 0)
            bn_sub(r, m, mn);
    }
}

/* out = (a * b) mod m ; a,b,m are BN limbs, out is BN limbs */
static void bn_mul_mod(UINT32 *out, const UINT32 *a, const UINT32 *b,
                       const UINT32 *m)
{
    UINT32 prod[BN_PROD];
    UINT32 rem[BN];
    bn_mul(a, b, BN, prod);
    bn_reduce(prod, BN_PROD, m, BN, rem);
    CopyMem(out, rem, sizeof(rem));
}

/* out = base^exp mod m */
static void bn_modexp(UINT32 *out, const UINT32 *base, UINT32 exp,
                      const UINT32 *m)
{
    UINT32 result[BN];
    UINT32 b[BN];
    UINT32 tmp[BN];
    int i;

    for (i = 0; i < BN; i++) result[i] = 0;
    result[0] = 1;
    CopyMem(b, base, sizeof(b));

    while (exp) {
        if (exp & 1)
            bn_mul_mod(result, result, b, m);
        exp >>= 1;
        if (exp) {
            bn_mul_mod(tmp, b, b, m);
            CopyMem(b, tmp, sizeof(b));
        }
    }

    CopyMem(out, result, sizeof(result));
}

/* Convert a big-endian byte array (byte 0 = most significant) into a
 * little-endian limb array of BN limbs. */
static void bn_from_bytes(UINT32 *out, const UINT8 *bytes, UINTN nbytes)
{
    UINTN b;
    for (b = 0; b < BN; b++) out[b] = 0;
    for (b = 0; b < nbytes; b++) {
        int limb = (int)(nbytes - 1 - b) >> 2;
        int sh = 24 - ((int)(b & 3) * 8);
        out[limb] |= (UINT32)bytes[b] << sh;
    }
}

/* Convert a little-endian limb array into a big-endian byte array. */
static void bn_to_bytes(const UINT32 *in, UINT8 *bytes, UINTN nbytes)
{
    UINTN b;
    for (b = 0; b < nbytes; b++) {
        int limb = (int)(nbytes - 1 - b) >> 2;
        int sh = 24 - ((int)(b & 3) * 8);
        bytes[b] = (UINT8)(in[limb] >> sh);
    }
}

BOOLEAN rsa2048_verify(const rsa_pubkey_t *key, const UINT8 *digest, UINTN digest_size, const UINT8 *signature) {
    UINT32 n_limbs[BN];
    UINT32 s_limbs[BN];
    UINT32 m_limbs[BN];
    UINT8 em[RSA2048_KEY_SIZE];
    UINTN i, ps_len, k;

    if (digest_size != SHA256_DIGEST_SIZE)
        return FALSE;

    if (key->modulus_size != RSA2048_KEY_SIZE ||
        key->exponent != 0x10001)
        return FALSE;

    bn_from_bytes(n_limbs, key->modulus, RSA2048_KEY_SIZE);
    bn_from_bytes(s_limbs, signature, RSA2048_KEY_SIZE);

    /* Recover m = signature^exponent mod modulus. */
    bn_modexp(m_limbs, s_limbs, key->exponent, n_limbs);
    bn_to_bytes(m_limbs, em, RSA2048_KEY_SIZE);

    /* PKCS#1 v1.5 encoding: 00 01 PS(FF..FF) 00 T */
    if (em[0] != 0x00 || em[1] != 0x01)
        return FALSE;

    i = 2;
    ps_len = 0;
    while (i < RSA2048_KEY_SIZE && em[i] == 0xFF) {
        ps_len++;
        i++;
    }

    if (ps_len < 8)                    /* PS must be at least 8 bytes */
        return FALSE;
    if (i >= RSA2048_KEY_SIZE || em[i] != 0x00) /* 00 separator */
        return FALSE;
    i++;                               /* now at T (DigestInfo || digest) */

    if (RSA2048_KEY_SIZE - i != sizeof(SHA256_DIGEST_INFO) + SHA256_DIGEST_SIZE)
        return FALSE;

    for (k = 0; k < sizeof(SHA256_DIGEST_INFO); k++)
        if (em[i + k] != SHA256_DIGEST_INFO[k])
            return FALSE;

    for (k = 0; k < digest_size; k++)
        if (em[i + sizeof(SHA256_DIGEST_INFO) + k] != digest[k])
            return FALSE;

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

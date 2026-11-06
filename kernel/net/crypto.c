/* mOS — primitivas criptográficas para o TLS 1.3 (escritas do zero).
 *
 *  - SHA-256 / SHA-384 / SHA-512 (FIPS 180-4)
 *  - HMAC (RFC 2104), HKDF (RFC 5869) e HKDF-Expand-Label (RFC 8446 §7.1)
 *  - X25519 (RFC 7748) — escada de Montgomery em tempo constante, corpo em 16 limbs
 *  - ChaCha20, Poly1305 e o AEAD ChaCha20-Poly1305 (RFC 8439)
 *  - AES-128/256 e GCM (NIST SP 800-38D)
 *  - CSPRNG: DRBG baseado em ChaCha20 com "fast key erasure", semeado por
 *    jitter de rdtsc, timer e RDRAND/RDSEED quando a CPU oferece.
 *
 * Observação de segurança: o AES aqui é a versão "por bytes" com S-box em tabela;
 * NÃO é imune a ataques de cache-timing. O GHASH, X25519, ChaCha20 e Poly1305
 * são em tempo constante. Por isso o cliente TLS prefere ChaCha20-Poly1305.
 */
#ifdef MOS_HOST_TEST
#include "tls_shim.h"
#else
#include "kernel.h"
#endif
#include "crypto.h"

/* ======================================================================
 * utilidades
 * ====================================================================== */
static inline u32 ld32le(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void st32le(u8 *p, u32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static inline u32 ld32be(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | (u32)p[3]; }
static inline void st32be(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static inline u64 ld64be(const u8 *p) { return (u64)ld32be(p) << 32 | ld32be(p + 4); }
static inline void st64be(u8 *p, u64 v) { st32be(p, (u32)(v >> 32)); st32be(p + 4, (u32)v); }
static inline void st64le(u8 *p, u64 v) { st32le(p, (u32)v); st32le(p + 4, (u32)(v >> 32)); }
static inline u32 rotr32(u32 x, int n) { return (x >> n) | (x << (32 - n)); }
static inline u32 rotl32(u32 x, int n) { return (x << n) | (x >> (32 - n)); }
static inline u64 rotr64(u64 x, int n) { return (x >> n) | (x << (64 - n)); }

int ct_memeq(const void *a, const void *b, usize n) {
    const u8 *x = a, *y = b;
    u8 d = 0;
    for (usize i = 0; i < n; i++) d |= x[i] ^ y[i];
    return d == 0;
}

void secure_zero(void *p, usize n) {
    volatile u8 *v = p;
    while (n--) *v++ = 0;
}

/* ======================================================================
 * SHA-2
 * ====================================================================== */
/* constantes do SHA-512; as do SHA-256 são os 32 bits altos das 64 primeiras */
static const u64 K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};
static const u64 IV512[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};
static const u64 IV384[8] = {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL,
};

static void sha256_block(u32 *h, const u8 *p) {
    u32 w[64];
    for (int i = 0; i < 16; i++) w[i] = ld32be(p + 4 * i);
    for (int i = 16; i < 64; i++) {
        u32 s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        u32 s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        u32 S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        u32 ch = (e & f) ^ (~e & g);
        u32 t1 = hh + S1 + ch + (u32)(K512[i] >> 32) + w[i];
        u32 S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        u32 mj = (a & b) ^ (a & c) ^ (b & c);
        u32 t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void sha256_init(sha256_ctx *c) {
    for (int i = 0; i < 8; i++) c->h[i] = (u32)(IV512[i] >> 32);
    c->total = 0;
    c->n = 0;
}

void sha256_update(sha256_ctx *c, const void *data, usize len) {
    const u8 *p = data;
    c->total += len;
    while (len) {
        if (c->n == 0 && len >= 64) { sha256_block(c->h, p); p += 64; len -= 64; continue; }
        u32 k = 64 - c->n;
        if (k > len) k = (u32)len;
        memcpy(c->buf + c->n, p, k);
        c->n += k; p += k; len -= k;
        if (c->n == 64) { sha256_block(c->h, c->buf); c->n = 0; }
    }
}

void sha256_final(sha256_ctx *c, u8 out[32]) {
    u64 bits = c->total * 8;
    u8 pad = 0x80;
    sha256_update(c, &pad, 1);
    u8 z = 0;
    while (c->n != 56) sha256_update(c, &z, 1);
    u8 lb[8];
    st64be(lb, bits);
    sha256_update(c, lb, 8);
    for (int i = 0; i < 8; i++) st32be(out + 4 * i, c->h[i]);
}

void sha256(const void *data, usize len, u8 out[32]) {
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, out);
}

/* SHA-1 (FIPS 180-4): só para identificadores do OCSP (CertID, ResponderID byKey);
 * NUNCA aceito como hash de assinatura. */
static void sha1_block(u32 *h, const u8 *p) {
    u32 w[80];
    for (int i = 0; i < 16; i++) w[i] = ld32be(p + 4 * i);
    for (int i = 16; i < 80; i++) w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
        else { f = b ^ c ^ d; k = 0xca62c1d6; }
        u32 t = rotl32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl32(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void sha1(const void *data, usize len, u8 out[20]) {
    u32 h[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    const u8 *p = data;
    usize n = len;
    for (; n >= 64; p += 64, n -= 64) sha1_block(h, p);
    u8 b[128];
    memset(b, 0, sizeof(b));
    memcpy(b, p, n);
    b[n] = 0x80;
    usize tot = n + 9 <= 64 ? 64 : 128;
    st64be(b + tot - 8, (u64)len * 8);
    sha1_block(h, b);
    if (tot == 128) sha1_block(h, b + 64);
    for (int i = 0; i < 5; i++) st32be(out + 4 * i, h[i]);
}

static void sha512_block(u64 *h, const u8 *p) {
    u64 w[80];
    for (int i = 0; i < 16; i++) w[i] = ld64be(p + 8 * i);
    for (int i = 16; i < 80; i++) {
        u64 s0 = rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        u64 s1 = rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u64 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 80; i++) {
        u64 S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
        u64 ch = (e & f) ^ (~e & g);
        u64 t1 = hh + S1 + ch + K512[i] + w[i];
        u64 S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
        u64 mj = (a & b) ^ (a & c) ^ (b & c);
        u64 t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void sha512_init(sha512_ctx *c) {
    for (int i = 0; i < 8; i++) c->h[i] = IV512[i];
    c->total = 0; c->n = 0; c->outlen = 64;
}

void sha384_init(sha512_ctx *c) {
    for (int i = 0; i < 8; i++) c->h[i] = IV384[i];
    c->total = 0; c->n = 0; c->outlen = 48;
}

void sha512_update(sha512_ctx *c, const void *data, usize len) {
    const u8 *p = data;
    c->total += len;
    while (len) {
        if (c->n == 0 && len >= 128) { sha512_block(c->h, p); p += 128; len -= 128; continue; }
        u32 k = 128 - c->n;
        if (k > len) k = (u32)len;
        memcpy(c->buf + c->n, p, k);
        c->n += k; p += k; len -= k;
        if (c->n == 128) { sha512_block(c->h, c->buf); c->n = 0; }
    }
}

void sha512_final(sha512_ctx *c, u8 *out) {
    u64 bits = c->total * 8;
    u8 pad = 0x80, z = 0;
    sha512_update(c, &pad, 1);
    while (c->n != 112) sha512_update(c, &z, 1);
    u8 lb[16] = {0};
    st64be(lb + 8, bits);
    sha512_update(c, lb, 16);
    u8 full[64];
    for (int i = 0; i < 8; i++) st64be(full + 8 * i, c->h[i]);
    memcpy(out, full, c->outlen);
}

/* ---------- hash genérico ---------- */
u32 hash_len(int alg) { return alg == HASH_SHA384 ? 48 : alg == HASH_SHA512 ? 64 : 32; }
static u32 hash_block_len(int alg) { return alg == HASH_SHA384 || alg == HASH_SHA512 ? 128 : 64; }

void hash_init(hash_ctx *c, int alg) {
    c->alg = alg;
    if (alg == HASH_SHA384) sha384_init(&c->u.s512);
    else if (alg == HASH_SHA512) sha512_init(&c->u.s512);
    else sha256_init(&c->u.s256);
}

void hash_update(hash_ctx *c, const void *data, usize len) {
    if (c->alg == HASH_SHA384 || c->alg == HASH_SHA512) sha512_update(&c->u.s512, data, len);
    else sha256_update(&c->u.s256, data, len);
}

void hash_final(hash_ctx *c, u8 *out) {
    if (c->alg == HASH_SHA384 || c->alg == HASH_SHA512) sha512_final(&c->u.s512, out);
    else sha256_final(&c->u.s256, out);
}

void hash_peek(const hash_ctx *c, u8 *out) {
    hash_ctx t = *c;
    hash_final(&t, out);
}

void hash_buf(int alg, const void *data, usize len, u8 *out) {
    hash_ctx c;
    hash_init(&c, alg);
    hash_update(&c, data, len);
    hash_final(&c, out);
}

/* ======================================================================
 * HMAC / HKDF
 * ====================================================================== */
void hmac(int alg, const u8 *key, usize klen, const void *msg, usize mlen, u8 *out) {
    u32 bl = hash_block_len(alg), hl = hash_len(alg);
    u8 k[128], pad[128], inner[HASH_MAX_LEN];
    memset(k, 0, sizeof(k));
    if (klen > bl) hash_buf(alg, key, klen, k);
    else if (klen) memcpy(k, key, klen);
    hash_ctx c;
    for (u32 i = 0; i < bl; i++) pad[i] = k[i] ^ 0x36;
    hash_init(&c, alg);
    hash_update(&c, pad, bl);
    hash_update(&c, msg, mlen);
    hash_final(&c, inner);
    for (u32 i = 0; i < bl; i++) pad[i] = k[i] ^ 0x5c;
    hash_init(&c, alg);
    hash_update(&c, pad, bl);
    hash_update(&c, inner, hl);
    hash_final(&c, out);
    secure_zero(k, sizeof(k));
    secure_zero(pad, sizeof(pad));
}

void hkdf_extract(int alg, const u8 *salt, usize slen, const u8 *ikm, usize ilen, u8 *prk) {
    u8 zero[HASH_MAX_LEN] = {0};
    if (!salt || !slen) { salt = zero; slen = hash_len(alg); }
    hmac(alg, salt, slen, ikm, ilen, prk);
}

void hkdf_expand(int alg, const u8 *prk, const u8 *info, usize ilen, u8 *out, usize olen) {
    u32 hl = hash_len(alg);
    u8 t[HASH_MAX_LEN], msg[HASH_MAX_LEN + 300];
    usize tl = 0, done = 0;
    u8 ctr = 1;
    if (ilen > 290) ilen = 290;   /* rótulos do TLS são bem menores */
    while (done < olen) {
        usize m = 0;
        memcpy(msg, t, tl); m += tl;
        if (ilen) memcpy(msg + m, info, ilen);
        m += ilen;
        msg[m++] = ctr++;
        hmac(alg, prk, hl, msg, m, t);
        tl = hl;
        usize k = olen - done < hl ? olen - done : hl;
        memcpy(out + done, t, k);
        done += k;
    }
    secure_zero(t, sizeof(t));
}

void hkdf_expand_label(int alg, const u8 *secret, const char *label,
                       const u8 *ctx, usize clen, u8 *out, usize olen) {
    u8 info[2 + 1 + 255 + 1 + 255];
    usize ll = strlen(label), n = 0;
    info[n++] = (u8)(olen >> 8);
    info[n++] = (u8)olen;
    info[n++] = (u8)(6 + ll);
    memcpy(info + n, "tls13 ", 6); n += 6;
    memcpy(info + n, label, ll); n += ll;
    info[n++] = (u8)clen;
    if (clen) memcpy(info + n, ctx, clen);
    n += clen;
    hkdf_expand(alg, secret, info, n, out, olen);
}

/* ======================================================================
 * X25519 — aritmética em GF(2^255-19) com 16 limbs de 16 bits (estilo TweetNaCl)
 * ====================================================================== */
typedef i64 gf[16];

static void car25519(gf o) {
    for (int i = 0; i < 16; i++) {
        o[i] += (i64)1 << 16;
        i64 c = o[i] >> 16;
        if (i < 15) o[i + 1] += c - 1;
        else o[0] += 38 * (c - 1);
        o[i] -= c * 65536;
    }
}

static void sel25519(gf p, gf q, int b) {
    i64 c = ~((i64)b - 1);
    for (int i = 0; i < 16; i++) {
        i64 t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(u8 *o, const gf n) {
    gf m, t;
    for (int i = 0; i < 16; i++) t[i] = n[i];
    car25519(t); car25519(t); car25519(t);
    for (int j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        int b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (int i = 0; i < 16; i++) { o[2 * i] = (u8)t[i]; o[2 * i + 1] = (u8)(t[i] >> 8); }
}

static void unpack25519(gf o, const u8 *n) {
    for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8);
    o[15] &= 0x7fff;
}

static void fadd(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void fsub(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }

static void fmul(gf o, const gf a, const gf b) {
    i64 t[31];
    for (int i = 0; i < 31; i++) t[i] = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; i++) o[i] = t[i];
    car25519(o);
    car25519(o);
}

static void fsqr(gf o, const gf a) { fmul(o, a, a); }

static void finv(gf o, const gf in) {
    gf c;
    for (int i = 0; i < 16; i++) c[i] = in[i];
    for (int a = 253; a >= 0; a--) {
        fsqr(c, c);
        if (a != 2 && a != 4) fmul(c, c, in);
    }
    for (int i = 0; i < 16; i++) o[i] = c[i];
}

void x25519(u8 out[32], const u8 scalar[32], const u8 point[32]) {
    static const gf k121665 = {0xDB41, 1};
    u8 z[32];
    gf x, a, b, c, d, e, f;
    for (int i = 0; i < 32; i++) z[i] = scalar[i];
    z[31] = (z[31] & 127) | 64;
    z[0] &= 248;
    unpack25519(x, point);
    for (int i = 0; i < 16; i++) { b[i] = x[i]; d[i] = a[i] = c[i] = 0; }
    a[0] = d[0] = 1;
    for (int i = 254; i >= 0; --i) {
        int r = (z[i >> 3] >> (i & 7)) & 1;
        sel25519(a, b, r);
        sel25519(c, d, r);
        fadd(e, a, c);
        fsub(a, a, c);
        fadd(c, b, d);
        fsub(b, b, d);
        fsqr(d, e);
        fsqr(f, a);
        fmul(a, c, a);
        fmul(c, b, e);
        fadd(e, a, c);
        fsub(a, a, c);
        fsqr(b, a);
        fsub(c, d, f);
        fmul(a, c, k121665);
        fadd(a, a, d);
        fmul(c, c, a);
        fmul(a, d, f);
        fmul(d, b, x);
        fsqr(b, e);
        sel25519(a, b, r);
        sel25519(c, d, r);
    }
    finv(c, c);
    fmul(a, a, c);
    pack25519(out, a);
    secure_zero(z, sizeof(z));
}

void x25519_base(u8 out[32], const u8 scalar[32]) {
    static const u8 nine[32] = {9};
    x25519(out, scalar, nine);
}

/* ======================================================================
 * ChaCha20
 * ====================================================================== */
#define QR(a, b, c, d)                    \
    a += b; d ^= a; d = rotl32(d, 16);    \
    c += d; b ^= c; b = rotl32(b, 12);    \
    a += b; d ^= a; d = rotl32(d, 8);     \
    c += d; b ^= c; b = rotl32(b, 7)

void chacha20_block(const u8 key[32], u32 counter, const u8 nonce[12], u8 out[64]) {
    u32 s[16], x[16];
    s[0] = 0x61707865; s[1] = 0x3320646e; s[2] = 0x79622d32; s[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) s[4 + i] = ld32le(key + 4 * i);
    s[12] = counter;
    s[13] = ld32le(nonce); s[14] = ld32le(nonce + 4); s[15] = ld32le(nonce + 8);
    for (int i = 0; i < 16; i++) x[i] = s[i];
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; i++) st32le(out + 4 * i, x[i] + s[i]);
    secure_zero(x, sizeof(x));
    secure_zero(s, sizeof(s));
}

void chacha20_xor(const u8 key[32], u32 counter, const u8 nonce[12], const u8 *in, u8 *out, usize len) {
    u8 ks[64];
    while (len) {
        chacha20_block(key, counter++, nonce, ks);
        usize k = len < 64 ? len : 64;
        for (usize i = 0; i < k; i++) out[i] = in[i] ^ ks[i];
        in += k; out += k; len -= k;
    }
    secure_zero(ks, sizeof(ks));
}

/* ======================================================================
 * Poly1305 (limbs de 26 bits, estilo poly1305-donna-32)
 * ====================================================================== */
void poly1305_init(poly1305_ctx *c, const u8 key[32]) {
    c->r[0] = (ld32le(key + 0)) & 0x3ffffff;
    c->r[1] = (ld32le(key + 3) >> 2) & 0x3ffff03;
    c->r[2] = (ld32le(key + 6) >> 4) & 0x3ffc0ff;
    c->r[3] = (ld32le(key + 9) >> 6) & 0x3f03fff;
    c->r[4] = (ld32le(key + 12) >> 8) & 0x00fffff;
    for (int i = 0; i < 5; i++) c->h[i] = 0;
    for (int i = 0; i < 4; i++) c->pad[i] = ld32le(key + 16 + 4 * i);
    c->n = 0;
}

static void poly1305_blocks(poly1305_ctx *c, const u8 *m, usize len, u32 hibit) {
    u32 r0 = c->r[0], r1 = c->r[1], r2 = c->r[2], r3 = c->r[3], r4 = c->r[4];
    u32 s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    u32 h0 = c->h[0], h1 = c->h[1], h2 = c->h[2], h3 = c->h[3], h4 = c->h[4];
    while (len >= 16) {
        h0 += (ld32le(m + 0)) & 0x3ffffff;
        h1 += (ld32le(m + 3) >> 2) & 0x3ffffff;
        h2 += (ld32le(m + 6) >> 4) & 0x3ffffff;
        h3 += (ld32le(m + 9) >> 6) & 0x3ffffff;
        h4 += (ld32le(m + 12) >> 8) | hibit;
        u64 d0 = (u64)h0 * r0 + (u64)h1 * s4 + (u64)h2 * s3 + (u64)h3 * s2 + (u64)h4 * s1;
        u64 d1 = (u64)h0 * r1 + (u64)h1 * r0 + (u64)h2 * s4 + (u64)h3 * s3 + (u64)h4 * s2;
        u64 d2 = (u64)h0 * r2 + (u64)h1 * r1 + (u64)h2 * r0 + (u64)h3 * s4 + (u64)h4 * s3;
        u64 d3 = (u64)h0 * r3 + (u64)h1 * r2 + (u64)h2 * r1 + (u64)h3 * r0 + (u64)h4 * s4;
        u64 d4 = (u64)h0 * r4 + (u64)h1 * r3 + (u64)h2 * r2 + (u64)h3 * r1 + (u64)h4 * r0;
        u32 cc;
        cc = (u32)(d0 >> 26); h0 = (u32)d0 & 0x3ffffff;
        d1 += cc; cc = (u32)(d1 >> 26); h1 = (u32)d1 & 0x3ffffff;
        d2 += cc; cc = (u32)(d2 >> 26); h2 = (u32)d2 & 0x3ffffff;
        d3 += cc; cc = (u32)(d3 >> 26); h3 = (u32)d3 & 0x3ffffff;
        d4 += cc; cc = (u32)(d4 >> 26); h4 = (u32)d4 & 0x3ffffff;
        h0 += cc * 5; cc = h0 >> 26; h0 &= 0x3ffffff;
        h1 += cc;
        m += 16; len -= 16;
    }
    c->h[0] = h0; c->h[1] = h1; c->h[2] = h2; c->h[3] = h3; c->h[4] = h4;
}

void poly1305_update(poly1305_ctx *c, const u8 *m, usize len) {
    if (c->n) {
        u32 k = 16 - c->n;
        if (k > len) k = (u32)len;
        memcpy(c->buf + c->n, m, k);
        c->n += k; m += k; len -= k;
        if (c->n < 16) return;
        poly1305_blocks(c, c->buf, 16, 1u << 24);
        c->n = 0;
    }
    usize full = len & ~(usize)15;
    if (full) { poly1305_blocks(c, m, full, 1u << 24); m += full; len -= full; }
    if (len) { memcpy(c->buf, m, len); c->n = (u32)len; }
}

void poly1305_final(poly1305_ctx *c, u8 tag[16]) {
    if (c->n) {
        c->buf[c->n] = 1;
        for (u32 i = c->n + 1; i < 16; i++) c->buf[i] = 0;
        poly1305_blocks(c, c->buf, 16, 0);
    }
    u32 h0 = c->h[0], h1 = c->h[1], h2 = c->h[2], h3 = c->h[3], h4 = c->h[4], cc;
    cc = h1 >> 26; h1 &= 0x3ffffff;
    h2 += cc; cc = h2 >> 26; h2 &= 0x3ffffff;
    h3 += cc; cc = h3 >> 26; h3 &= 0x3ffffff;
    h4 += cc; cc = h4 >> 26; h4 &= 0x3ffffff;
    h0 += cc * 5; cc = h0 >> 26; h0 &= 0x3ffffff;
    h1 += cc;
    /* g = h + 5 - 2^130; escolhe g se não ficou negativo */
    u32 g0 = h0 + 5; cc = g0 >> 26; g0 &= 0x3ffffff;
    u32 g1 = h1 + cc; cc = g1 >> 26; g1 &= 0x3ffffff;
    u32 g2 = h2 + cc; cc = g2 >> 26; g2 &= 0x3ffffff;
    u32 g3 = h3 + cc; cc = g3 >> 26; g3 &= 0x3ffffff;
    u32 g4 = h4 + cc - (1u << 26);
    u32 mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1; h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3; h4 = (h4 & mask) | g4;
    h0 = (h0 | (h1 << 26));
    h1 = ((h1 >> 6) | (h2 << 20));
    h2 = ((h2 >> 12) | (h3 << 14));
    h3 = ((h3 >> 18) | (h4 << 8));
    u64 f;
    f = (u64)h0 + c->pad[0]; h0 = (u32)f;
    f = (u64)h1 + c->pad[1] + (f >> 32); h1 = (u32)f;
    f = (u64)h2 + c->pad[2] + (f >> 32); h2 = (u32)f;
    f = (u64)h3 + c->pad[3] + (f >> 32); h3 = (u32)f;
    st32le(tag, h0); st32le(tag + 4, h1); st32le(tag + 8, h2); st32le(tag + 12, h3);
    secure_zero(c, sizeof(*c));
}

/* ---------- AEAD ChaCha20-Poly1305 ---------- */
static void cp_mac(const u8 key[32], const u8 nonce[12], const u8 *aad, usize alen,
                   const u8 *ct, usize len, u8 tag[16]) {
    u8 blk[64], z[16] = {0}, lens[16];
    chacha20_block(key, 0, nonce, blk);
    poly1305_ctx p;
    poly1305_init(&p, blk);
    poly1305_update(&p, aad, alen);
    if (alen & 15) poly1305_update(&p, z, 16 - (alen & 15));
    poly1305_update(&p, ct, len);
    if (len & 15) poly1305_update(&p, z, 16 - (len & 15));
    st64le(lens, alen);
    st64le(lens + 8, len);
    poly1305_update(&p, lens, 16);
    poly1305_final(&p, tag);
    secure_zero(blk, sizeof(blk));
}

void chacha20poly1305_seal(const u8 key[32], const u8 nonce[12], const u8 *aad, usize alen,
                           const u8 *in, usize len, u8 *out, u8 tag[16]) {
    chacha20_xor(key, 1, nonce, in, out, len);
    cp_mac(key, nonce, aad, alen, out, len, tag);
}

int chacha20poly1305_open(const u8 key[32], const u8 nonce[12], const u8 *aad, usize alen,
                          const u8 *in, usize len, u8 *out, const u8 tag[16]) {
    u8 t[16];
    cp_mac(key, nonce, aad, alen, in, len, t);
    if (!ct_memeq(t, tag, 16)) return -1;
    chacha20_xor(key, 1, nonce, in, out, len);
    return 0;
}

/* ======================================================================
 * AES (por bytes; S-box gerada na primeira utilização)
 * ====================================================================== */
static u8 aes_sbox[256];
static bool aes_ready;

static inline u8 xtime(u8 x) { return (u8)((x << 1) ^ (0x1b & (u8)(0 - (x >> 7)))); }

static u8 gf8_mul(u8 a, u8 b) {
    u8 p = 0;
    for (int i = 0; i < 8; i++) {
        p ^= a & (u8)(0 - (b & 1));
        a = xtime(a);
        b >>= 1;
    }
    return p;
}

static void aes_tables(void) {
    for (int x = 0; x < 256; x++) {
        /* inverso multiplicativo: x^254 */
        u8 inv = 1, base = (u8)x;
        for (int e = 254; e; e >>= 1) {
            if (e & 1) inv = gf8_mul(inv, base);
            base = gf8_mul(base, base);
        }
        if (x == 0) inv = 0;
        u8 s = inv;
        for (int k = 1; k <= 4; k++) s ^= (u8)((inv << k) | (inv >> (8 - k)));
        aes_sbox[x] = s ^ 0x63;
    }
    aes_ready = true;
}

void aes_setkey(aes_ctx *c, const u8 *key, u32 keylen) {
    if (!aes_ready) aes_tables();
    u32 nk = keylen / 4;
    c->rounds = nk + 6;
    u32 total = 4 * (c->rounds + 1);
    u8 *w = c->rk;
    memcpy(w, key, keylen);
    u8 rcon = 1;
    for (u32 i = nk; i < total; i++) {
        u8 t[4];
        memcpy(t, w + 4 * (i - 1), 4);
        if (i % nk == 0) {
            u8 tmp = t[0];
            t[0] = aes_sbox[t[1]] ^ rcon;
            t[1] = aes_sbox[t[2]];
            t[2] = aes_sbox[t[3]];
            t[3] = aes_sbox[tmp];
            rcon = xtime(rcon);
        } else if (nk > 6 && i % nk == 4) {
            for (int k = 0; k < 4; k++) t[k] = aes_sbox[t[k]];
        }
        for (int k = 0; k < 4; k++) w[4 * i + k] = w[4 * (i - nk) + k] ^ t[k];
    }
}

void aes_encrypt_block(const aes_ctx *c, const u8 in[16], u8 out[16]) {
    u8 s[16], t[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ c->rk[i];
    for (int r = 1; r <= c->rounds; r++) {
        /* SubBytes + ShiftRows */
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                t[col * 4 + row] = aes_sbox[s[((col + row) & 3) * 4 + row]];
        if (r != c->rounds) {
            /* MixColumns */
            for (int col = 0; col < 4; col++) {
                u8 *a = t + col * 4;
                u8 a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
                u8 all = a0 ^ a1 ^ a2 ^ a3;
                s[col * 4 + 0] = a0 ^ all ^ xtime(a0 ^ a1);
                s[col * 4 + 1] = a1 ^ all ^ xtime(a1 ^ a2);
                s[col * 4 + 2] = a2 ^ all ^ xtime(a2 ^ a3);
                s[col * 4 + 3] = a3 ^ all ^ xtime(a3 ^ a0);
            }
        } else {
            memcpy(s, t, 16);
        }
        const u8 *rk = c->rk + 16 * r;
        for (int i = 0; i < 16; i++) s[i] ^= rk[i];
    }
    memcpy(out, s, 16);
}

/* ---------- GCM ---------- */
static void ghash_mul(u64 *yh, u64 *yl, u64 hh, u64 hl) {
    u64 zh = 0, zl = 0, vh = hh, vl = hl, xh = *yh, xl = *yl;
    for (int i = 0; i < 128; i++) {
        u64 bit = i < 64 ? (xh >> (63 - i)) & 1 : (xl >> (127 - i)) & 1;
        u64 m = 0 - bit;
        zh ^= vh & m;
        zl ^= vl & m;
        u64 lsb = vl & 1;
        vl = (vl >> 1) | (vh << 63);
        vh = (vh >> 1) ^ (0xE100000000000000ULL & (0 - lsb));
    }
    *yh = zh;
    *yl = zl;
}

static void ghash_data(const aes_gcm_ctx *c, u64 *yh, u64 *yl, const u8 *p, usize len) {
    while (len) {
        u8 b[16] = {0};
        usize k = len < 16 ? len : 16;
        memcpy(b, p, k);
        *yh ^= ld64be(b);
        *yl ^= ld64be(b + 8);
        ghash_mul(yh, yl, c->hh, c->hl);
        p += k; len -= k;
    }
}

void aes_gcm_init(aes_gcm_ctx *c, const u8 *key, u32 keylen) {
    aes_setkey(&c->aes, key, keylen);
    u8 z[16] = {0}, h[16];
    aes_encrypt_block(&c->aes, z, h);
    c->hh = ld64be(h);
    c->hl = ld64be(h + 8);
}

static void gcm_ctr(const aes_gcm_ctx *c, const u8 iv[12], const u8 *in, u8 *out, usize len) {
    u8 ctr[16], ks[16];
    memcpy(ctr, iv, 12);
    u32 n = 2;
    while (len) {
        st32be(ctr + 12, n++);
        aes_encrypt_block(&c->aes, ctr, ks);
        usize k = len < 16 ? len : 16;
        for (usize i = 0; i < k; i++) out[i] = in[i] ^ ks[i];
        in += k; out += k; len -= k;
    }
}

static void gcm_tag(const aes_gcm_ctx *c, const u8 iv[12], const u8 *aad, usize alen,
                    const u8 *ct, usize len, u8 tag[16]) {
    u64 yh = 0, yl = 0;
    ghash_data(c, &yh, &yl, aad, alen);
    ghash_data(c, &yh, &yl, ct, len);
    yh ^= (u64)alen * 8;
    yl ^= (u64)len * 8;
    ghash_mul(&yh, &yl, c->hh, c->hl);
    u8 j0[16], ek[16];
    memcpy(j0, iv, 12);
    st32be(j0 + 12, 1);
    aes_encrypt_block(&c->aes, j0, ek);
    st64be(tag, yh ^ ld64be(ek));
    st64be(tag + 8, yl ^ ld64be(ek + 8));
}

void aes_gcm_seal(const aes_gcm_ctx *c, const u8 iv[12], const u8 *aad, usize alen,
                  const u8 *in, usize len, u8 *out, u8 tag[16]) {
    gcm_ctr(c, iv, in, out, len);
    gcm_tag(c, iv, aad, alen, out, len, tag);
}

int aes_gcm_open(const aes_gcm_ctx *c, const u8 iv[12], const u8 *aad, usize alen,
                 const u8 *in, usize len, u8 *out, const u8 tag[16]) {
    u8 t[16];
    gcm_tag(c, iv, aad, alen, in, len, t);
    if (!ct_memeq(t, tag, 16)) return -1;
    gcm_ctr(c, iv, in, out, len);
    return 0;
}

/* ======================================================================
 * CSPRNG — DRBG ChaCha20 ("fast key erasure")
 * ====================================================================== */
static u8 rng_key[32];
static u64 rng_ctr;
static bool rng_seeded;

static inline void cpuid_(u32 leaf, u32 sub, u32 *a, u32 *b, u32 *c, u32 *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(sub));
}
static inline int rdrand64(u64 *v) {
    u8 ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*v), "=qm"(ok) :: "cc");
    return ok;
}
static inline int rdseed64(u64 *v) {
    u8 ok;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*v), "=qm"(ok) :: "cc");
    return ok;
}

static void rng_seed(void) {
    sha256_ctx h;
    sha256_init(&h);
    sha256_update(&h, rng_key, 32);           /* estado anterior (se houver) */
    u64 t0 = rdtsc(), tk = timer_ticks();
    sha256_update(&h, &t0, 8);
    sha256_update(&h, &tk, 8);
    void *sp = &h;
    sha256_update(&h, &sp, sizeof(sp));

    /* instruções de hardware, se existirem */
    u32 a, b, c, d;
    cpuid_(0, 0, &a, &b, &c, &d);
    u32 maxleaf = a;
    cpuid_(1, 0, &a, &b, &c, &d);
    if (c & (1u << 30)) {
        for (int i = 0; i < 8; i++) {
            u64 v;
            for (int tries = 0; tries < 10; tries++)
                if (rdrand64(&v)) { sha256_update(&h, &v, 8); break; }
        }
    }
    if (maxleaf >= 7) {
        cpuid_(7, 0, &a, &b, &c, &d);
        if (b & (1u << 18)) {
            for (int i = 0; i < 4; i++) {
                u64 v;
                for (int tries = 0; tries < 20; tries++)
                    if (rdseed64(&v)) { sha256_update(&h, &v, 8); break; }
            }
        }
    }

    /* jitter: variação do tempo de pequenas cargas de trabalho */
    volatile u32 acc = 0x9e3779b9;
    u64 prev = rdtsc();
    for (int i = 0; i < 1024; i++) {
        for (int k = 0; k < (int)(prev & 15) + 8; k++) acc = acc * 1103515245u + 12345u + k;
        u64 now = rdtsc();
        u32 delta = (u32)(now - prev);
        sha256_update(&h, &delta, 4);
        prev = now;
    }
    u32 av = acc;
    sha256_update(&h, &av, 4);
    tk = timer_ticks();
    sha256_update(&h, &tk, 8);
    sha256_final(&h, rng_key);
    rng_seeded = true;
}

void crypto_rng_add_entropy(const void *data, usize len) {
    sha256_ctx h;
    u64 t = rdtsc();
    sha256_init(&h);
    sha256_update(&h, rng_key, 32);
    sha256_update(&h, data, len);
    sha256_update(&h, &t, 8);
    sha256_final(&h, rng_key);
}

void crypto_random(void *buf, usize len) {
    if (!rng_seeded) rng_seed();
    u8 nonce[12], blk[64];
    st64le(nonce, rng_ctr++);
    st32le(nonce + 8, (u32)rdtsc());
    u8 *o = buf;
    u32 ctr = 1;
    while (len) {
        chacha20_block(rng_key, ctr++, nonce, blk);
        usize k = len < 64 ? len : 64;
        memcpy(o, blk, k);
        o += k; len -= k;
    }
    /* apaga a chave usada: a próxima vem do bloco 0 */
    chacha20_block(rng_key, 0, nonce, blk);
    memcpy(rng_key, blk, 32);
    secure_zero(blk, sizeof(blk));
}

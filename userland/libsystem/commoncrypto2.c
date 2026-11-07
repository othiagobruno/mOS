/*
 * mOS libSystem — o resto do CommonCrypto que o Chromium/Electron importa (inventário do machocheck no
 * VS Code): MD5, SHA-384/512, HMAC (CCHmac*), PBKDF2 (CCKeyDerivationPBKDF) e AES (CCCrypt e CCCryptor
 * em ECB, CBC e CTR, com PKCS#7). O OSCrypt do Chromium deriva a chave com PBKDF2-SHA1 a partir da senha
 * guardada no Keychain e cifra os cookies/segredos com AES-128-CBC: é o caminho que precisa estar certo.
 * Layouts dos contextos iguais aos do macOS (o app aloca CC_MD5_CTX, CC_SHA512_CTX e CCHmacContext).
 */
#include "sys.h"
#include "libc.h"

typedef unsigned char u8_;
typedef unsigned int u32_;
typedef unsigned long long u64_;

/* ---- SHA-1 e SHA-256 vêm de commoncrypto.c */
typedef struct { u32_ h0, h1, h2, h3, h4, Nl, Nh; u32_ data[16]; int num; } CC_SHA1_CTX;
typedef struct { u32_ count[2]; u32_ hash[8]; u32_ wbuf[16]; } CC_SHA256_CTX;
int CC_SHA1_Init(CC_SHA1_CTX *c);
int CC_SHA1_Update(CC_SHA1_CTX *c, const void *data, u32_ len);
int CC_SHA1_Final(u8_ *md, CC_SHA1_CTX *c);
int CC_SHA256_Init(CC_SHA256_CTX *c);
int CC_SHA256_Update(CC_SHA256_CTX *c, const void *data, u32_ len);
int CC_SHA256_Final(u8_ *md, CC_SHA256_CTX *c);

/* ================================================================ MD5 */
typedef struct { u32_ A, B, C, D, Nl, Nh; u32_ data[16]; int num; } CC_MD5_CTX;
static u32_ rol32(u32_ x, int n) { return (x << n) | (x >> (32 - n)); }
static const u32_ md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
static const u8_ md5_r[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20,
                              5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
static void md5_block(CC_MD5_CTX *c, const u8_ *p) {
    u32_ w[16];
    for (int i = 0; i < 16; i++) w[i] = (u32_)p[i * 4] | (u32_)p[i * 4 + 1] << 8 | (u32_)p[i * 4 + 2] << 16 | (u32_)p[i * 4 + 3] << 24;
    u32_ a = c->A, b = c->B, cc = c->C, d = c->D;
    for (int i = 0; i < 64; i++) {
        u32_ f; int g;
        if (i < 16) { f = (b & cc) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & cc); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ cc ^ d; g = (3 * i + 5) & 15; }
        else { f = cc ^ (b | ~d); g = (7 * i) & 15; }
        u32_ t = d; d = cc; cc = b;
        b = b + rol32(a + f + md5_k[i] + w[g], md5_r[i]);
        a = t;
    }
    c->A += a; c->B += b; c->C += cc; c->D += d;
}
int CC_MD5_Init(CC_MD5_CTX *c) {
    memset(c, 0, sizeof *c);
    c->A = 0x67452301; c->B = 0xefcdab89; c->C = 0x98badcfe; c->D = 0x10325476;
    return 1;
}
int CC_MD5_Update(CC_MD5_CTX *c, const void *data, u32_ len) {
    const u8_ *p = data;
    u8_ *buf = (u8_ *)c->data;
    u64_ total = ((u64_)c->Nh << 32 | c->Nl) + len;
    c->Nl = (u32_)total; c->Nh = (u32_)(total >> 32);
    while (len--) {
        buf[c->num++] = *p++;
        if (c->num == 64) { md5_block(c, buf); c->num = 0; }
    }
    return 1;
}
int CC_MD5_Final(u8_ *md, CC_MD5_CTX *c) {
    u64_ bits = ((u64_)c->Nh << 32 | c->Nl) * 8;
    u8_ *buf = (u8_ *)c->data;
    buf[c->num++] = 0x80;
    if (c->num > 56) { while (c->num < 64) buf[c->num++] = 0; md5_block(c, buf); c->num = 0; }
    while (c->num < 56) buf[c->num++] = 0;
    for (int i = 0; i < 8; i++) buf[56 + i] = (u8_)(bits >> (8 * i));
    md5_block(c, buf);
    u32_ h[4] = {c->A, c->B, c->C, c->D};
    for (int i = 0; i < 16; i++) md[i] = (u8_)(h[i / 4] >> (8 * (i % 4)));
    return 1;
}
u8_ *CC_MD5(const void *data, u32_ len, u8_ *md) { CC_MD5_CTX c; CC_MD5_Init(&c); CC_MD5_Update(&c, data, len); CC_MD5_Final(md, &c); return md; }

/* ================================================================ SHA-512 / SHA-384 */
typedef struct { u64_ count[2]; u64_ hash[8]; u64_ wbuf[16]; } CC_SHA512_CTX;
static const u64_ k512[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull,
    0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull, 0x12835b0145706fbeull,
    0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull, 0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
    0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
    0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull, 0x983e5152ee66dfabull,
    0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
    0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull,
    0x53380d139d95b3dfull, 0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
    0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
    0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull, 0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull,
    0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull,
    0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull, 0xca273eceea26619cull,
    0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull,
    0x113f9804bef90daeull, 0x1b710b35131c471bull, 0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
    0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull};
static u64_ ror64(u64_ x, int n) { return (x >> n) | (x << (64 - n)); }
static void sha512_block(CC_SHA512_CTX *c, const u8_ *p) {
    u64_ w[80];
    for (int i = 0; i < 16; i++) {
        u64_ v = 0;
        for (int j = 0; j < 8; j++) v = v << 8 | p[i * 8 + j];
        w[i] = v;
    }
    for (int i = 16; i < 80; i++) {
        u64_ s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        u64_ s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u64_ a = c->hash[0], b = c->hash[1], cc = c->hash[2], d = c->hash[3], e = c->hash[4], f = c->hash[5], g = c->hash[6], h = c->hash[7];
    for (int i = 0; i < 80; i++) {
        u64_ S1 = ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41);
        u64_ ch = (e & f) ^ (~e & g);
        u64_ t1 = h + S1 + ch + k512[i] + w[i];
        u64_ S0 = ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39);
        u64_ mj = (a & b) ^ (a & cc) ^ (b & cc);
        u64_ t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->hash[0] += a; c->hash[1] += b; c->hash[2] += cc; c->hash[3] += d;
    c->hash[4] += e; c->hash[5] += f; c->hash[6] += g; c->hash[7] += h;
}
int CC_SHA512_Init(CC_SHA512_CTX *c) {
    static const u64_ iv[8] = {0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
                               0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};
    memset(c, 0, sizeof *c);
    memcpy(c->hash, iv, sizeof iv);
    return 1;
}
int CC_SHA384_Init(CC_SHA512_CTX *c) {
    static const u64_ iv[8] = {0xcbbb9d5dc1059ed8ull, 0x629a292a367cd507ull, 0x9159015a3070dd17ull, 0x152fecd8f70e5939ull,
                               0x67332667ffc00b31ull, 0x8eb44a8768581511ull, 0xdb0c2e0d64f98fa7ull, 0x47b5481dbefa4fa4ull};
    memset(c, 0, sizeof *c);
    memcpy(c->hash, iv, sizeof iv);
    return 1;
}
int CC_SHA512_Update(CC_SHA512_CTX *c, const void *data, u32_ len) {
    const u8_ *p = data;
    u8_ *buf = (u8_ *)c->wbuf;
    while (len--) {
        u64_ used = c->count[0] & 127;
        buf[used] = *p++;
        if (++c->count[0] == 0) c->count[1]++;
        if (used == 127) sha512_block(c, buf);
    }
    return 1;
}
static void sha512_finish(CC_SHA512_CTX *c, u8_ *md, int words) {
    u64_ bytes = c->count[0];
    u64_ hi = c->count[1] << 3 | bytes >> 61, lo = bytes << 3;
    u8_ *buf = (u8_ *)c->wbuf;
    u64_ used = bytes & 127;
    buf[used++] = 0x80;
    if (used > 112) { while (used < 128) buf[used++] = 0; sha512_block(c, buf); used = 0; }
    while (used < 112) buf[used++] = 0;
    for (int i = 0; i < 8; i++) { buf[112 + i] = (u8_)(hi >> (56 - 8 * i)); buf[120 + i] = (u8_)(lo >> (56 - 8 * i)); }
    sha512_block(c, buf);
    for (int i = 0; i < words; i++)
        for (int j = 0; j < 8; j++) md[i * 8 + j] = (u8_)(c->hash[i] >> (56 - 8 * j));
}
int CC_SHA512_Final(u8_ *md, CC_SHA512_CTX *c) { sha512_finish(c, md, 8); return 1; }
int CC_SHA384_Final(u8_ *md, CC_SHA512_CTX *c) { sha512_finish(c, md, 6); return 1; }
int CC_SHA384_Update(CC_SHA512_CTX *c, const void *data, u32_ len) { return CC_SHA512_Update(c, data, len); }
u8_ *CC_SHA512(const void *d, u32_ n, u8_ *md) { CC_SHA512_CTX c; CC_SHA512_Init(&c); CC_SHA512_Update(&c, d, n); CC_SHA512_Final(md, &c); return md; }
u8_ *CC_SHA384(const void *d, u32_ n, u8_ *md) { CC_SHA512_CTX c; CC_SHA384_Init(&c); CC_SHA512_Update(&c, d, n); CC_SHA384_Final(md, &c); return md; }

/* ================================================================ HMAC */
enum { kCCHmacAlgSHA1, kCCHmacAlgMD5, kCCHmacAlgSHA256, kCCHmacAlgSHA384, kCCHmacAlgSHA512, kCCHmacAlgSHA224 };
typedef union { CC_SHA1_CTX s1; CC_MD5_CTX m5; CC_SHA256_CTX s256; CC_SHA512_CTX s512; } hctx_u;
/* CCHmacContext do macOS tem 384 bytes (u32 ctx[96]); o nosso cabe: 4 + 208 + 128 + folga */
typedef struct { u32_ alg; u32_ pad_; hctx_u h; u8_ opad[128]; } hmac_t;
typedef char hmac_fits_[sizeof(hmac_t) <= 384 ? 1 : -1];
static int h_block(u32_ alg) { return alg == kCCHmacAlgSHA384 || alg == kCCHmacAlgSHA512 ? 128 : 64; }
static int h_len(u32_ alg) {
    switch (alg) { case kCCHmacAlgSHA1: return 20; case kCCHmacAlgMD5: return 16; case kCCHmacAlgSHA256: return 32;
                   case kCCHmacAlgSHA384: return 48; case kCCHmacAlgSHA512: return 64; }
    return 32;
}
static void h_init(u32_ alg, hctx_u *h) {
    switch (alg) {
    case kCCHmacAlgSHA1: CC_SHA1_Init(&h->s1); break;
    case kCCHmacAlgMD5: CC_MD5_Init(&h->m5); break;
    case kCCHmacAlgSHA384: CC_SHA384_Init(&h->s512); break;
    case kCCHmacAlgSHA512: CC_SHA512_Init(&h->s512); break;
    default: CC_SHA256_Init(&h->s256); break;
    }
}
static void h_update(u32_ alg, hctx_u *h, const void *d, size_t n) {
    const u8_ *p = d;
    while (n) {
        u32_ k = n > 0x40000000 ? 0x40000000 : (u32_)n;
        switch (alg) {
        case kCCHmacAlgSHA1: CC_SHA1_Update(&h->s1, p, k); break;
        case kCCHmacAlgMD5: CC_MD5_Update(&h->m5, p, k); break;
        case kCCHmacAlgSHA384: case kCCHmacAlgSHA512: CC_SHA512_Update(&h->s512, p, k); break;
        default: CC_SHA256_Update(&h->s256, p, k); break;
        }
        p += k; n -= k;
    }
}
static void h_final(u32_ alg, hctx_u *h, u8_ *md) {
    switch (alg) {
    case kCCHmacAlgSHA1: CC_SHA1_Final(md, &h->s1); break;
    case kCCHmacAlgMD5: CC_MD5_Final(md, &h->m5); break;
    case kCCHmacAlgSHA384: CC_SHA384_Final(md, &h->s512); break;
    case kCCHmacAlgSHA512: CC_SHA512_Final(md, &h->s512); break;
    default: CC_SHA256_Final(md, &h->s256); break;
    }
}
void CCHmacInit(void *ctx, u32_ alg, const void *key, size_t klen) {
    hmac_t *m = ctx;
    if (alg == kCCHmacAlgSHA224) alg = kCCHmacAlgSHA256;     /* SHA-224 não implementado: o Chromium não usa */
    m->alg = alg;
    int bs = h_block(alg);
    u8_ k[128];
    memset(k, 0, sizeof k);
    if (klen > (size_t)bs) { hctx_u t; h_init(alg, &t); h_update(alg, &t, key, klen); h_final(alg, &t, k); }
    else if (klen) memcpy(k, key, klen);
    u8_ ipad[128];
    for (int i = 0; i < bs; i++) { ipad[i] = k[i] ^ 0x36; m->opad[i] = k[i] ^ 0x5c; }
    h_init(alg, &m->h);
    h_update(alg, &m->h, ipad, (size_t)bs);
}
void CCHmacUpdate(void *ctx, const void *d, size_t n) { hmac_t *m = ctx; h_update(m->alg, &m->h, d, n); }
void CCHmacFinal(void *ctx, void *mac) {
    hmac_t *m = ctx;
    u8_ inner[64];
    h_final(m->alg, &m->h, inner);
    hctx_u o;
    h_init(m->alg, &o);
    h_update(m->alg, &o, m->opad, (size_t)h_block(m->alg));
    h_update(m->alg, &o, inner, (size_t)h_len(m->alg));
    h_final(m->alg, &o, mac);
}
void CCHmac(u32_ alg, const void *key, size_t klen, const void *d, size_t n, void *mac) {
    hmac_t m;
    CCHmacInit(&m, alg, key, klen);
    CCHmacUpdate(&m, d, n);
    CCHmacFinal(&m, mac);
}

/* ================================================================ PBKDF2 */
enum { kCCPBKDF2 = 2 };
enum { kCCPRFHmacAlgSHA1 = 1, kCCPRFHmacAlgSHA224, kCCPRFHmacAlgSHA256, kCCPRFHmacAlgSHA384, kCCPRFHmacAlgSHA512 };
int CCKeyDerivationPBKDF(u32_ algorithm, const char *pw, size_t pwlen, const u8_ *salt, size_t saltlen, u32_ prf,
                         u32_ rounds, u8_ *out, size_t outlen) {
    if (algorithm != kCCPBKDF2 || !out || !rounds) return -4300;
    u32_ alg = prf == kCCPRFHmacAlgSHA1 ? kCCHmacAlgSHA1 : prf == kCCPRFHmacAlgSHA384 ? kCCHmacAlgSHA384 :
               prf == kCCPRFHmacAlgSHA512 ? kCCHmacAlgSHA512 : kCCHmacAlgSHA256;
    int hl = h_len(alg);
    u8_ u[64], t[64];
    for (u32_ blk = 1; outlen; blk++) {
        hmac_t m;
        u8_ be[4] = {(u8_)(blk >> 24), (u8_)(blk >> 16), (u8_)(blk >> 8), (u8_)blk};
        CCHmacInit(&m, alg, pw, pwlen);
        CCHmacUpdate(&m, salt, saltlen);
        CCHmacUpdate(&m, be, 4);
        CCHmacFinal(&m, u);
        memcpy(t, u, (size_t)hl);
        for (u32_ r = 1; r < rounds; r++) {
            CCHmac(alg, pw, pwlen, u, (size_t)hl, u);
            for (int i = 0; i < hl; i++) t[i] ^= u[i];
        }
        size_t k = outlen < (size_t)hl ? outlen : (size_t)hl;
        memcpy(out, t, k);
        out += k; outlen -= k;
    }
    return 0;
}

/* ================================================================ AES */
static u8_ sbox[256], inv_sbox[256];
static int aes_ready;
static u8_ xt(u8_ x) { return (u8_)((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }
static u8_ gmul(u8_ a, u8_ b) { u8_ r = 0; while (b) { if (b & 1) r ^= a; a = xt(a); b >>= 1; } return r; }
static void aes_tables(void) {
    if (aes_ready) return;
    /* S-box gerada: inverso multiplicativo em GF(2^8) seguido da transformação afim */
    u8_ p = 1, q = 1;
    do {
        p = (u8_)(p ^ (p << 1) ^ ((p & 0x80) ? 0x1b : 0));
        q ^= (u8_)(q << 1); q ^= (u8_)(q << 2); q ^= (u8_)(q << 4);
        if (q & 0x80) q ^= 0x09;
        u8_ x = (u8_)(q ^ (u8_)((q << 1) | (q >> 7)) ^ (u8_)((q << 2) | (q >> 6)) ^ (u8_)((q << 3) | (q >> 5)) ^ (u8_)((q << 4) | (q >> 4)));
        sbox[p] = (u8_)(x ^ 0x63);
    } while (p != 1);
    sbox[0] = 0x63;
    for (int i = 0; i < 256; i++) inv_sbox[sbox[i]] = (u8_)i;
    aes_ready = 1;
}
typedef struct { int nr; u8_ rk[240]; } aes_key_t;
static int aes_setkey(aes_key_t *k, const u8_ *key, size_t len) {
    aes_tables();
    if (len != 16 && len != 24 && len != 32) return -1;
    int nk = (int)len / 4;
    k->nr = nk + 6;
    int total = 4 * (k->nr + 1);
    memcpy(k->rk, key, len);
    u8_ rcon = 1;
    for (int i = nk; i < total; i++) {
        u8_ t[4];
        memcpy(t, k->rk + (i - 1) * 4, 4);
        if (i % nk == 0) {
            u8_ a = t[0];
            t[0] = (u8_)(sbox[t[1]] ^ rcon); t[1] = sbox[t[2]]; t[2] = sbox[t[3]]; t[3] = sbox[a];
            rcon = xt(rcon);
        } else if (nk > 6 && i % nk == 4) {
            for (int j = 0; j < 4; j++) t[j] = sbox[t[j]];
        }
        for (int j = 0; j < 4; j++) k->rk[i * 4 + j] = (u8_)(k->rk[(i - nk) * 4 + j] ^ t[j]);
    }
    return 0;
}
static void add_rk(u8_ *s, const u8_ *rk) { for (int i = 0; i < 16; i++) s[i] ^= rk[i]; }
static void aes_enc(const aes_key_t *k, const u8_ *in, u8_ *out) {
    u8_ s[16], t[16];
    memcpy(s, in, 16);
    add_rk(s, k->rk);
    for (int r = 1; r <= k->nr; r++) {
        for (int i = 0; i < 16; i++) s[i] = sbox[s[i]];
        for (int c = 0; c < 4; c++) for (int rw = 0; rw < 4; rw++) t[c * 4 + rw] = s[((c + rw) & 3) * 4 + rw];
        if (r != k->nr)
            for (int c = 0; c < 4; c++) {
                u8_ *col = t + c * 4, a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                col[0] = (u8_)(xt(a0) ^ xt(a1) ^ a1 ^ a2 ^ a3);
                col[1] = (u8_)(a0 ^ xt(a1) ^ xt(a2) ^ a2 ^ a3);
                col[2] = (u8_)(a0 ^ a1 ^ xt(a2) ^ xt(a3) ^ a3);
                col[3] = (u8_)(xt(a0) ^ a0 ^ a1 ^ a2 ^ xt(a3));
            }
        memcpy(s, t, 16);
        add_rk(s, k->rk + r * 16);
    }
    memcpy(out, s, 16);
}
static void aes_dec(const aes_key_t *k, const u8_ *in, u8_ *out) {
    u8_ s[16], t[16];
    memcpy(s, in, 16);
    add_rk(s, k->rk + k->nr * 16);
    for (int r = k->nr - 1; r >= 0; r--) {
        for (int c = 0; c < 4; c++) for (int rw = 0; rw < 4; rw++) t[((c + rw) & 3) * 4 + rw] = s[c * 4 + rw];
        for (int i = 0; i < 16; i++) t[i] = inv_sbox[t[i]];
        add_rk(t, k->rk + r * 16);
        if (r)
            for (int c = 0; c < 4; c++) {
                u8_ *col = t + c * 4, a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                col[0] = (u8_)(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
                col[1] = (u8_)(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
                col[2] = (u8_)(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11));
                col[3] = (u8_)(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14));
            }
        memcpy(s, t, 16);
    }
    memcpy(out, s, 16);
}

/* ---- CCCryptor */
enum { kCCSuccess = 0, kCCParamError = -4300, kCCBufferTooSmall = -4301, kCCMemoryFailure = -4302,
       kCCAlignmentError = -4303, kCCDecodeError = -4304, kCCUnimplemented = -4305 };
enum { kCCEncrypt = 0, kCCDecrypt = 1 };
enum { kCCAlgorithmAES = 0 };
enum { kCCOptionPKCS7Padding = 1, kCCOptionECBMode = 2 };
enum { kCCModeECB = 1, kCCModeCBC = 2, kCCModeCTR = 4 };
typedef struct {
    int op, mode, pad;
    aes_key_t key;
    u8_ iv[16];          /* CBC: bloco anterior; CTR: contador */
    u8_ buf[16];         /* bytes pendentes (bloco incompleto ou último bloco guardado na decifragem) */
    int nbuf;
    u8_ ks[16]; int nks; /* CTR: fluxo de chave não usado */
} cryptor_t;

static void ctr_inc(u8_ *c) { for (int i = 15; i >= 0; i--) if (++c[i]) break; }
static void do_block(cryptor_t *c, const u8_ *in, u8_ *out) {
    u8_ t[16];
    if (c->mode == kCCModeECB) { if (c->op == kCCEncrypt) aes_enc(&c->key, in, out); else aes_dec(&c->key, in, out); return; }
    if (c->op == kCCEncrypt) {
        for (int i = 0; i < 16; i++) t[i] = in[i] ^ c->iv[i];
        aes_enc(&c->key, t, out);
        memcpy(c->iv, out, 16);
    } else {
        u8_ prev[16];
        memcpy(prev, in, 16);
        aes_dec(&c->key, in, t);
        for (int i = 0; i < 16; i++) out[i] = t[i] ^ c->iv[i];
        memcpy(c->iv, prev, 16);
    }
}
int CCCryptorCreateWithMode(u32_ op, u32_ mode, u32_ alg, u32_ padding, const void *iv, const void *key, size_t klen,
                            const void *tweak, size_t tlen, int rounds, u32_ options, void **ref) {
    (void)tweak; (void)tlen; (void)rounds; (void)options;
    if (!ref) return kCCParamError;
    if (alg != kCCAlgorithmAES) return kCCUnimplemented;
    if (mode != kCCModeECB && mode != kCCModeCBC && mode != kCCModeCTR) return kCCUnimplemented;
    cryptor_t *c = calloc(1, sizeof *c);
    if (!c) return kCCMemoryFailure;
    c->op = (int)op; c->mode = (int)mode; c->pad = padding == 1;
    if (aes_setkey(&c->key, key, klen)) { free(c); return kCCParamError; }
    if (iv) memcpy(c->iv, iv, 16);
    *ref = c;
    return kCCSuccess;
}
int CCCryptorCreate(u32_ op, u32_ alg, u32_ options, const void *key, size_t klen, const void *iv, void **ref) {
    return CCCryptorCreateWithMode(op, (options & kCCOptionECBMode) ? kCCModeECB : kCCModeCBC, alg,
                                   (options & kCCOptionPKCS7Padding) ? 1 : 0, iv, key, klen, 0, 0, 0, 0, ref);
}
int CCCryptorRelease(void *ref) { if (ref) { memset(ref, 0, sizeof(cryptor_t)); free(ref); } return kCCSuccess; }
int CCCryptorReset(void *ref, const void *iv) {
    cryptor_t *c = ref;
    if (!c) return kCCParamError;
    if (iv) memcpy(c->iv, iv, 16); else memset(c->iv, 0, 16);
    c->nbuf = 0; c->nks = 0;
    return kCCSuccess;
}
size_t CCCryptorGetOutputLength(void *ref, size_t inlen, int final) {
    cryptor_t *c = ref;
    if (!c) return 0;
    if (c->mode == kCCModeCTR) return inlen;
    size_t t = inlen + (size_t)c->nbuf;
    if (final && c->pad && c->op == kCCEncrypt) return (t / 16 + 1) * 16;
    return final ? (t + 15) / 16 * 16 : t / 16 * 16;
}
int CCCryptorUpdate(void *ref, const void *in, size_t inlen, void *out, size_t avail, size_t *moved) {
    cryptor_t *c = ref;
    const u8_ *p = in;
    u8_ *o = out;
    size_t w = 0;
    if (!c) return kCCParamError;
    if (c->mode == kCCModeCTR) {
        if (avail < inlen) return kCCBufferTooSmall;
        for (size_t i = 0; i < inlen; i++) {
            if (!c->nks) { aes_enc(&c->key, c->iv, c->ks); ctr_inc(c->iv); c->nks = 16; }
            o[i] = p[i] ^ c->ks[16 - c->nks--];
        }
        if (moved) *moved = inlen;
        return kCCSuccess;
    }
    /* na decifragem com padding o último bloco completo fica guardado para o Final tirar o PKCS#7 */
    int hold = c->op == kCCDecrypt && c->pad;
    size_t need = CCCryptorGetOutputLength(c, inlen, 0);
    if (hold && need >= 16 && (inlen + (size_t)c->nbuf) % 16 == 0) need -= 16;
    if (avail < need) return kCCBufferTooSmall;
    for (size_t i = 0; i < inlen; i++) {
        if (c->nbuf == 16) {
            if (w + 16 > avail) return kCCBufferTooSmall;
            do_block(c, c->buf, o + w);
            w += 16;
            c->nbuf = 0;
        }
        c->buf[c->nbuf++] = p[i];
    }
    if (c->nbuf == 16 && !hold) {
        if (w + 16 > avail) return kCCBufferTooSmall;
        do_block(c, c->buf, o + w);
        w += 16;
        c->nbuf = 0;
    }
    if (moved) *moved = w;
    return kCCSuccess;
}
int CCCryptorFinal(void *ref, void *out, size_t avail, size_t *moved) {
    cryptor_t *c = ref;
    u8_ *o = out;
    if (moved) *moved = 0;
    if (!c) return kCCParamError;
    if (c->mode == kCCModeCTR) return kCCSuccess;
    if (c->op == kCCEncrypt) {
        if (!c->pad) { if (c->nbuf) return kCCAlignmentError; return kCCSuccess; }
        if (avail < 16) return kCCBufferTooSmall;
        u8_ v = (u8_)(16 - c->nbuf);
        while (c->nbuf < 16) c->buf[c->nbuf++] = v;
        do_block(c, c->buf, o);
        c->nbuf = 0;
        if (moved) *moved = 16;
        return kCCSuccess;
    }
    if (!c->pad) return c->nbuf ? kCCAlignmentError : kCCSuccess;
    if (c->nbuf != 16) return c->nbuf ? kCCAlignmentError : kCCDecodeError;
    u8_ t[16];
    do_block(c, c->buf, t);
    c->nbuf = 0;
    u8_ v = t[15];
    if (!v || v > 16) return kCCDecodeError;
    for (int i = 16 - v; i < 16; i++) if (t[i] != v) return kCCDecodeError;
    size_t n = (size_t)(16 - v);
    if (avail < n) return kCCBufferTooSmall;
    memcpy(o, t, n);
    if (moved) *moved = n;
    return kCCSuccess;
}
int CCCrypt(u32_ op, u32_ alg, u32_ options, const void *key, size_t klen, const void *iv, const void *in, size_t inlen,
            void *out, size_t avail, size_t *moved) {
    void *ref = 0;
    size_t a = 0, b = 0;
    if (moved) *moved = 0;
    int r = CCCryptorCreate(op, alg, options, key, klen, iv, &ref);
    if (r) return r;
    size_t need = CCCryptorGetOutputLength(ref, inlen, 1);
    if (avail < need && !(op == kCCDecrypt)) { CCCryptorRelease(ref); if (moved) *moved = need; return kCCBufferTooSmall; }
    r = CCCryptorUpdate(ref, in, inlen, out, avail, &a);
    if (!r) r = CCCryptorFinal(ref, (u8_ *)out + a, avail - a, &b);
    CCCryptorRelease(ref);
    if (moved) *moved = r ? 0 : a + b;
    return r;
}

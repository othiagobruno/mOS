/*
 * mOS libSystem — núcleo C do CryptoKit (userland/sdk/swift/CryptoKit/CryptoKit.swift chama estas funções).
 *
 * As primitivas vêm do TLS do kernel, compiladas também aqui (ck_crypto.c = kernel/net/crypto.c, ck_pk.c =
 * kernel/net/pk.c): SHA-2, HMAC, HKDF, X25519, ChaCha20-Poly1305, AES-GCM, P-256/P-384/P-521 (ECDH, ECDSA).
 * MD5/SHA-1 (Insecure.*) e AES-KW usam o CommonCrypto do libSystem. O Ed25519 (Curve25519.Signing) é deste
 * arquivo, no estilo do TweetNaCl (domínio público): corpo GF(2^255-19) em 16 limbs, coordenadas estendidas.
 * Todas devolvem 0 em sucesso. mos_ck_selftest() confere os vetores dos RFCs.
 */
#define MOS_HOST_TEST 1                    /* os cabeçalhos do kernel pelo shim de userland/libsystem/kshim */
#include "sys.h"
#include "kshim/crypto.h"
#include "kshim/x509.h"

void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int CC_SHA1_Init(void *c);
int CC_SHA1_Update(void *c, const void *d, u32 n);
int CC_SHA1_Final(u8 *md, void *c);
int CC_MD5_Init(void *c);
int CC_MD5_Update(void *c, const void *d, u32 n);
int CC_MD5_Final(u8 *md, void *c);
void CCHmac(u32 alg, const void *key, size_t klen, const void *d, size_t n, void *mac);
int CCCrypt(u32 op, u32 alg, u32 options, const void *key, size_t klen, const void *iv, const void *in, size_t inlen,
            void *out, size_t outcap, size_t *moved);

/* ================================================================ hashes (incrementais) */
enum { CK_MD5 = 1, CK_SHA1 = 2, CK_SHA256 = 3, CK_SHA384 = 4, CK_SHA512 = 5 };
typedef struct { int alg; union { sha256_ctx s256; sha512_ctx s512; u8 cc[256]; } u; } ck_hash_t;
int mos_ck_hash_len(int alg) { return alg == CK_MD5 ? 16 : alg == CK_SHA1 ? 20 : alg == CK_SHA256 ? 32 : alg == CK_SHA384 ? 48 : alg == CK_SHA512 ? 64 : 0; }
int mos_ck_hash_block(int alg) { return alg == CK_SHA384 || alg == CK_SHA512 ? 128 : 64; }
void *mos_ck_hash_new(int alg) {
    ck_hash_t *h = malloc(sizeof *h);
    if (!h) return 0;
    memset(h, 0, sizeof *h);
    h->alg = alg;
    switch (alg) {
    case CK_MD5: CC_MD5_Init(h->u.cc); break;
    case CK_SHA1: CC_SHA1_Init(h->u.cc); break;
    case CK_SHA256: sha256_init(&h->u.s256); break;
    case CK_SHA384: sha384_init(&h->u.s512); break;
    case CK_SHA512: sha512_init(&h->u.s512); break;
    default: free(h); return 0;
    }
    return h;
}
void *mos_ck_hash_copy(const void *p) { ck_hash_t *h = malloc(sizeof *h); if (h) memcpy(h, p, sizeof *h); return h; }
void mos_ck_hash_update(void *p, const void *data, size_t n) {
    ck_hash_t *h = p;
    const u8 *d = data;
    switch (h->alg) {
    case CK_MD5: while (n) { u32 k = n > 0x40000000 ? 0x40000000 : (u32)n; CC_MD5_Update(h->u.cc, d, k); d += k; n -= k; } break;
    case CK_SHA1: while (n) { u32 k = n > 0x40000000 ? 0x40000000 : (u32)n; CC_SHA1_Update(h->u.cc, d, k); d += k; n -= k; } break;
    case CK_SHA256: sha256_update(&h->u.s256, d, n); break;
    default: sha512_update(&h->u.s512, d, n); break;
    }
}
/* finaliza sem destruir o contexto (o HashFunction do CryptoKit é um valor: finalize() pode vir de uma cópia) */
void mos_ck_hash_final(const void *p, u8 *out) {
    ck_hash_t t = *(const ck_hash_t *)p;
    switch (t.alg) {
    case CK_MD5: CC_MD5_Final(out, t.u.cc); break;
    case CK_SHA1: CC_SHA1_Final(out, t.u.cc); break;
    case CK_SHA256: sha256_final(&t.u.s256, out); break;
    default: sha512_final(&t.u.s512, out); break;
    }
}
void mos_ck_hash_free(void *p) { if (p) { memset(p, 0, sizeof(ck_hash_t)); free(p); } }
int mos_ck_hash(int alg, const void *data, size_t n, u8 *out) {
    void *h = mos_ck_hash_new(alg);
    if (!h) return -1;
    mos_ck_hash_update(h, data, n);
    mos_ck_hash_final(h, out);
    mos_ck_hash_free(h);
    return 0;
}

/* ================================================================ HMAC e HKDF (qualquer hash acima) */
int mos_ck_hmac(int alg, const u8 *key, size_t klen, const void *msg, size_t mlen, u8 *out) {
    static const u32 cc[] = {0, 1 /* MD5 */, 0 /* SHA1 */, 2, 3, 4};
    if (alg < CK_MD5 || alg > CK_SHA512) return -1;
    CCHmac(cc[alg], key, klen, msg, mlen, out);
    return 0;
}
/* RFC 5869 */
int mos_ck_hkdf(int alg, const u8 *ikm, size_t ilen, const u8 *salt, size_t slen, const u8 *info, size_t nlen, u8 *out, size_t olen) {
    int hl = mos_ck_hash_len(alg);
    if (!hl || olen > (size_t)hl * 255) return -1;
    u8 zero[64] = {0}, prk[64], t[64];
    if (!salt || !slen) { salt = zero; slen = (size_t)hl; }
    mos_ck_hmac(alg, salt, slen, ikm, ilen, prk);
    size_t done = 0, tl = 0;
    u8 *buf = malloc((size_t)hl + nlen + 1);
    if (!buf) return -1;
    for (u8 c = 1; done < olen; c++) {
        memcpy(buf, t, tl);
        if (nlen) memcpy(buf + tl, info, nlen);
        buf[tl + nlen] = c;
        mos_ck_hmac(alg, prk, (size_t)hl, buf, tl + nlen + 1, t);
        tl = (size_t)hl;
        size_t k = olen - done < tl ? olen - done : tl;
        memcpy(out + done, t, k);
        done += k;
    }
    free(buf);
    secure_zero(prk, sizeof prk);
    return 0;
}

/* ================================================================ AEADs */
int mos_ck_aes_gcm_seal(const u8 *key, size_t klen, const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, u8 *out, u8 tag[16]) {
    if (klen != 16 && klen != 24 && klen != 32) return -2;
    aes_gcm_ctx c;
    aes_gcm_init(&c, key, (u32)klen);
    aes_gcm_seal(&c, nonce, aad, alen, in, n, out, tag);
    secure_zero(&c, sizeof c);
    return 0;
}
int mos_ck_aes_gcm_open(const u8 *key, size_t klen, const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, const u8 tag[16], u8 *out) {
    if (klen != 16 && klen != 24 && klen != 32) return -2;
    aes_gcm_ctx c;
    aes_gcm_init(&c, key, (u32)klen);
    int r = aes_gcm_open(&c, nonce, aad, alen, in, n, out, tag);
    secure_zero(&c, sizeof c);
    return r == 0 ? 0 : -1;
}
int mos_ck_chachapoly_seal(const u8 key[32], const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, u8 *out, u8 tag[16]) {
    chacha20poly1305_seal(key, nonce, aad, alen, in, n, out, tag);
    return 0;
}
int mos_ck_chachapoly_open(const u8 key[32], const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, const u8 tag[16], u8 *out) {
    return chacha20poly1305_open(key, nonce, aad, alen, in, n, out, tag) == 0 ? 0 : -1;
}
/* AES Key Wrap (RFC 3394), com o AES-ECB do CommonCrypto */
static int aes_block(int dec, const u8 *key, size_t klen, const u8 in[16], u8 out[16]) {
    size_t moved = 0;
    return CCCrypt(dec ? 1 : 0, 0, 2 /* ECB */, key, klen, 0, in, 16, out, 16, &moved) == 0 && moved == 16 ? 0 : -1;
}
int mos_ck_aes_wrap(const u8 *kek, size_t klen, const u8 *key, size_t n, u8 *out) {
    if (n % 8 || n < 16) return -1;
    size_t m = n / 8;
    u8 a[8], b[16];
    memset(a, 0xA6, 8);
    memcpy(out + 8, key, n);
    for (int j = 0; j < 6; j++)
        for (size_t i = 1; i <= m; i++) {
            memcpy(b, a, 8); memcpy(b + 8, out + 8 * i, 8);
            if (aes_block(0, kek, klen, b, b)) return -1;
            u64 t = (u64)(m * (size_t)j + i);
            memcpy(a, b, 8);
            for (int k = 0; k < 8; k++) a[7 - k] ^= (u8)(t >> (8 * k));
            memcpy(out + 8 * i, b + 8, 8);
        }
    memcpy(out, a, 8);
    return 0;
}
int mos_ck_aes_unwrap(const u8 *kek, size_t klen, const u8 *in, size_t n, u8 *out) {
    if (n % 8 || n < 24) return -1;
    size_t m = n / 8 - 1;
    u8 a[8], b[16], *r = malloc(n);
    if (!r) return -1;
    memcpy(a, in, 8);
    memcpy(r, in + 8, n - 8);
    for (int j = 5; j >= 0; j--)
        for (size_t i = m; i >= 1; i--) {
            u64 t = (u64)(m * (size_t)j + i);
            memcpy(b, a, 8);
            for (int k = 0; k < 8; k++) b[7 - k] ^= (u8)(t >> (8 * k));
            memcpy(b + 8, r + 8 * (i - 1), 8);
            if (aes_block(1, kek, klen, b, b)) { free(r); return -1; }
            memcpy(a, b, 8);
            memcpy(r + 8 * (i - 1), b + 8, 8);
        }
    static const u8 iv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};
    int ok = ct_memeq(a, iv, 8);
    if (ok) memcpy(out, r, n - 8);
    secure_zero(r, n);
    free(r);
    return ok ? 0 : -1;
}
void mos_ck_random(void *buf, size_t n) { crypto_random(buf, n); }
int mos_ck_equal(const void *a, const void *b, size_t n) { return ct_memeq(a, b, n); }

/* ================================================================ X25519 */
int mos_ck_x25519_public(const u8 priv[32], u8 pub[32]) { x25519_base(pub, priv); return 0; }
int mos_ck_x25519(const u8 priv[32], const u8 peer[32], u8 shared[32]) {
    x25519(shared, priv, peer);
    u8 z = 0;
    for (int i = 0; i < 32; i++) z |= shared[i];
    return z ? 0 : -1;                                        /* ponto de ordem pequena: o CryptoKit também recusa */
}

/* ================================================================ Ed25519 */
typedef i64 gf[16];
static const gf gf0, gf1 = {1};
static const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070, 0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
static const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0, 0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
static const gf GX = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c, 0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
static const gf GY = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
static const gf SQRTM1 = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43, 0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};
static void fset(gf r, const gf a) { for (int i = 0; i < 16; i++) r[i] = a[i]; }
static void car(gf o) {
    for (int i = 0; i < 16; i++) {
        o[i] += (i64)1 << 16;
        i64 c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}
static void fsel(gf p, gf q, int b) { i64 c = ~(i64)(b - 1); for (int i = 0; i < 16; i++) { i64 t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; } }
static void fpack(u8 *o, const gf n) {
    gf m, t;
    fset(t, n);
    car(t); car(t); car(t);
    for (int j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        int b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        fsel(t, m, 1 - b);
    }
    for (int i = 0; i < 16; i++) { o[2 * i] = (u8)(t[i] & 0xff); o[2 * i + 1] = (u8)(t[i] >> 8); }
}
static int fneq(const gf a, const gf b) { u8 c[32], d[32]; fpack(c, a); fpack(d, b); return !ct_memeq(c, d, 32); }
static u8 fpar(const gf a) { u8 d[32]; fpack(d, a); return d[0] & 1; }
static void funpack(gf o, const u8 *n) { for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8); o[15] &= 0x7fff; }
static void fA(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void fZ(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void fM(gf o, const gf a, const gf b) {
    i64 t[31];
    for (int i = 0; i < 31; i++) t[i] = 0;
    for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; i++) o[i] = t[i];
    car(o); car(o);
}
static void fS(gf o, const gf a) { fM(o, a, a); }
static void finv(gf o, const gf in) { gf c; fset(c, in); for (int a = 253; a >= 0; a--) { fS(c, c); if (a != 2 && a != 4) fM(c, c, in); } fset(o, c); }
static void fpow2523(gf o, const gf in) { gf c; fset(c, in); for (int a = 250; a >= 0; a--) { fS(c, c); if (a != 1) fM(c, c, in); } fset(o, c); }
static void padd(gf p[4], gf q[4]) {
    gf a, b, c, d, t, e, f, g, h;
    fZ(a, p[1], p[0]); fZ(t, q[1], q[0]); fM(a, a, t);
    fA(b, p[0], p[1]); fA(t, q[0], q[1]); fM(b, b, t);
    fM(c, p[3], q[3]); fM(c, c, D2);
    fM(d, p[2], q[2]); fA(d, d, d);
    fZ(e, b, a); fZ(f, d, c); fA(g, d, c); fA(h, b, a);
    fM(p[0], e, f); fM(p[1], h, g); fM(p[2], g, f); fM(p[3], e, h);
}
static void pcswap(gf p[4], gf q[4], u8 b) { for (int i = 0; i < 4; i++) fsel(p[i], q[i], b); }
static void ppack(u8 *r, gf p[4]) { gf tx, ty, zi; finv(zi, p[2]); fM(tx, p[0], zi); fM(ty, p[1], zi); fpack(r, ty); r[31] ^= (u8)(fpar(tx) << 7); }
static void pmul(gf p[4], gf q[4], const u8 *s) {
    fset(p[0], gf0); fset(p[1], gf1); fset(p[2], gf1); fset(p[3], gf0);
    for (int i = 255; i >= 0; --i) { u8 b = (u8)((s[i / 8] >> (i & 7)) & 1); pcswap(p, q, b); padd(q, p); padd(p, p); pcswap(p, q, b); }
}
static void pbase(gf p[4], const u8 *s) { gf q[4]; fset(q[0], GX); fset(q[1], GY); fset(q[2], gf1); fM(q[3], GX, GY); pmul(p, q, s); }
static const i64 Lord[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};
static void modL(u8 *r, i64 x[64]) {
    i64 carry;
    int i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * Lord[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; j++) { x[j] += carry - (x[31] >> 4) * Lord[j]; carry = x[j] >> 8; x[j] &= 255; }
    for (j = 0; j < 32; j++) x[j] -= carry * Lord[j];
    for (i = 0; i < 32; i++) { x[i + 1] += x[i] >> 8; r[i] = (u8)(x[i] & 255); }
}
static void reduce64(u8 *r) { i64 x[64]; for (int i = 0; i < 64; i++) x[i] = (i64)(u64)r[i]; for (int i = 0; i < 64; i++) r[i] = 0; modL(r, x); }
static void sha512(const void *a, size_t an, const void *b, size_t bn, const void *c, size_t cn, u8 out[64]) {
    sha512_ctx h;
    sha512_init(&h);
    if (an) sha512_update(&h, a, an);
    if (bn) sha512_update(&h, b, bn);
    if (cn) sha512_update(&h, c, cn);
    sha512_final(&h, out);
}
int mos_ck_ed25519_public(const u8 seed[32], u8 pub[32]) {
    u8 d[64];
    gf p[4];
    sha512(seed, 32, 0, 0, 0, 0, d);
    d[0] &= 248; d[31] &= 127; d[31] |= 64;
    pbase(p, d);
    ppack(pub, p);
    secure_zero(d, sizeof d);
    return 0;
}
int mos_ck_ed25519_sign(const u8 seed[32], const u8 *msg, size_t n, u8 sig[64]) {
    u8 d[64], r[64], h[64], pk[32];
    gf p[4];
    i64 x[64];
    sha512(seed, 32, 0, 0, 0, 0, d);
    d[0] &= 248; d[31] &= 127; d[31] |= 64;
    pbase(p, d);
    ppack(pk, p);
    sha512(d + 32, 32, msg, n, 0, 0, r);
    reduce64(r);
    pbase(p, r);
    ppack(sig, p);
    sha512(sig, 32, pk, 32, msg, n, h);
    reduce64(h);
    for (int i = 0; i < 64; i++) x[i] = 0;
    for (int i = 0; i < 32; i++) x[i] = (i64)(u64)r[i];
    for (int i = 0; i < 32; i++) for (int j = 0; j < 32; j++) x[i + j] += h[i] * (i64)(u64)d[j];
    modL(sig + 32, x);
    secure_zero(d, sizeof d);
    secure_zero(r, sizeof r);
    return 0;
}
static int unpackneg(gf r[4], const u8 p[32]) {
    gf t, chk, num, den, den2, den4, den6;
    fset(r[2], gf1);
    funpack(r[1], p);
    fS(num, r[1]);
    fM(den, num, D);
    fZ(num, num, r[2]);
    fA(den, r[2], den);
    fS(den2, den); fS(den4, den2); fM(den6, den4, den2); fM(t, den6, num); fM(t, t, den);
    fpow2523(t, t); fM(t, t, num); fM(t, t, den); fM(t, t, den); fM(r[0], t, den);
    fS(chk, r[0]); fM(chk, chk, den);
    if (fneq(chk, num)) fM(r[0], r[0], SQRTM1);
    fS(chk, r[0]); fM(chk, chk, den);
    if (fneq(chk, num)) return -1;
    if (fpar(r[0]) == (p[31] >> 7)) fZ(r[0], gf0, r[0]);
    fM(r[3], r[0], r[1]);
    return 0;
}
int mos_ck_ed25519_verify(const u8 pub[32], const u8 *msg, size_t n, const u8 sig[64]) {
    gf p[4], q[4];
    u8 h[64], t[32];
    if (sig[63] & 0xE0) return -1;                            /* S >= 2^253: não canônico */
    if (unpackneg(q, pub)) return -1;
    sha512(sig, 32, pub, 32, msg, n, h);
    reduce64(h);
    pmul(p, q, h);
    pbase(q, sig + 32);
    padd(p, q);
    ppack(t, p);
    return ct_memeq(sig, t, 32) ? 0 : -1;
}

/* ================================================================ NIST P-256 / P-384 / P-521 */
static int curve_id(int bits) { return bits == 256 ? EC_P256 : bits == 384 ? EC_P384 : bits == 521 ? EC_P521 : 0; }
int mos_ck_ec_size(int bits) { return bits == 256 ? 32 : bits == 384 ? 48 : bits == 521 ? 66 : 0; }
int mos_ck_ec_generate(int bits, u8 *priv, u8 *pub) { int c = curve_id(bits); return c ? ecdh_keygen(c, priv, pub) : -1; }
int mos_ck_ec_public(int bits, const u8 *priv, u8 *pub) { int c = curve_id(bits); return c ? ec_public_key(c, priv, pub) : -1; }
int mos_ck_ec_sign(int bits, const u8 *priv, const u8 *digest, size_t dlen, u8 *rs) {
    int c = curve_id(bits), B = mos_ck_ec_size(bits);
    return c ? ecdsa_sign(c, priv, digest, (u32)dlen, rs, rs + B) : -1;
}
int mos_ck_ec_verify(int bits, const u8 *pub, const u8 *digest, size_t dlen, const u8 *rs) {
    int c = curve_id(bits), B = mos_ck_ec_size(bits);
    return c ? ecdsa_verify(c, pub, (u32)(1 + 2 * B), digest, (u32)dlen, rs, (u32)B, rs + B, (u32)B) : -1;
}
int mos_ck_ec_shared(int bits, const u8 *priv, const u8 *peer, u8 *out) {
    int c = curve_id(bits), B = mos_ck_ec_size(bits);
    return c ? ecdh_shared(c, out, priv, peer, (u32)(1 + 2 * B)) : -1;
}

/* ================================================================ autoteste (vetores dos RFCs) */
static int hexeq(const u8 *b, size_t n, const char *hex) {
    for (size_t i = 0; i < n; i++) {
        int hi = hex[2 * i], lo = hex[2 * i + 1];
        int v = ((hi <= '9' ? hi - '0' : hi - 'a' + 10) << 4) | (lo <= '9' ? lo - '0' : lo - 'a' + 10);
        if (b[i] != (u8)v) return 0;
    }
    return 1;
}
static void unhex(u8 *b, const char *hex) {
    for (size_t i = 0; hex[2 * i]; i++) {
        int hi = hex[2 * i], lo = hex[2 * i + 1];
        b[i] = (u8)(((hi <= '9' ? hi - '0' : hi - 'a' + 10) << 4) | (lo <= '9' ? lo - '0' : lo - 'a' + 10));
    }
}
/* devolve um bit por teste que FALHOU (0 = tudo certo) */
int mos_ck_selftest(void) {
    int fail = 0, bit = 0;
    u8 o[128], k[64], m[256], t[16];
    /* 0: SHA-256("abc") */
    mos_ck_hash(CK_SHA256, "abc", 3, o);
    if (!hexeq(o, 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) fail |= 1 << bit; bit++;
    /* 1: SHA-512("abc") */
    mos_ck_hash(CK_SHA512, "abc", 3, o);
    if (!hexeq(o, 64, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f")) fail |= 1 << bit; bit++;
    /* 2: MD5("abc"), 3: SHA-1("abc") */
    mos_ck_hash(CK_MD5, "abc", 3, o);
    if (!hexeq(o, 16, "900150983cd24fb0d6963f7d28e17f72")) fail |= 1 << bit; bit++;
    mos_ck_hash(CK_SHA1, "abc", 3, o);
    if (!hexeq(o, 20, "a9993e364706816aba3e25717850c26c9cd0d89d")) fail |= 1 << bit; bit++;
    /* 4: HMAC-SHA256, RFC 4231 caso 2 */
    mos_ck_hmac(CK_SHA256, (const u8 *)"Jefe", 4, "what do ya want for nothing?", 28, o);
    if (!hexeq(o, 32, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")) fail |= 1 << bit; bit++;
    /* 5: HKDF-SHA256, RFC 5869 caso 1 */
    { u8 ikm[22], salt[13], info[10];
      memset(ikm, 0x0b, 22);
      for (int i = 0; i < 13; i++) salt[i] = (u8)i;
      for (int i = 0; i < 10; i++) info[i] = (u8)(0xf0 + i);
      mos_ck_hkdf(CK_SHA256, ikm, 22, salt, 13, info, 10, o, 42);
      if (!hexeq(o, 42, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865")) fail |= 1 << bit; }
    bit++;
    /* 6: AES-128-GCM, chave e IV zero, texto vazio (NIST caso 1) */
    memset(k, 0, 32);
    mos_ck_aes_gcm_seal(k, 16, k, 0, 0, 0, 0, o, t);
    if (!hexeq(t, 16, "58e2fccefa7e3061367f1d57a4e7455a")) fail |= 1 << bit; bit++;
    /* 16: AES-192-GCM, NIST caso 7 (chave e IV zero, texto vazio) */
    memset(k, 0, 32);
    mos_ck_aes_gcm_seal(k, 24, k, 0, 0, 0, 0, o, t);
    if (!hexeq(t, 16, "cd33b28ac773f74ba00ed1f312572435")) fail |= 1 << 16;
    /* 7: ChaCha20-Poly1305 ida e volta (o vetor do RFC 8439 já é testado no tlstest) e adulteração */
    for (int i = 0; i < 32; i++) k[i] = (u8)(0x80 + i);
    memcpy(m, "Ladies and Gentlemen of the class of '99", 40);
    mos_ck_chachapoly_seal(k, k, (const u8 *)"aad", 3, m, 40, o, t);
    { u8 back[40]; int ok = mos_ck_chachapoly_open(k, k, (const u8 *)"aad", 3, o, 40, t, back) == 0 && !memcmp(back, m, 40);
      o[0] ^= 1; ok = ok && mos_ck_chachapoly_open(k, k, (const u8 *)"aad", 3, o, 40, t, back) != 0;
      if (!ok) fail |= 1 << bit; }
    bit++;
    /* 8: X25519, RFC 7748 §6.1 (chave pública de Alice) */
    unhex(k, "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    mos_ck_x25519_public(k, o);
    if (!hexeq(o, 32, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a")) fail |= 1 << bit; bit++;
    /* 9: Ed25519, RFC 8032 teste 1 (mensagem vazia) */
    unhex(k, "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    mos_ck_ed25519_public(k, o);
    if (!hexeq(o, 32, "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")) fail |= 1 << bit; bit++;
    { u8 sig[64];
      mos_ck_ed25519_sign(k, 0, 0, sig);
      if (!hexeq(sig, 64, "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b")) fail |= 1 << bit;
      bit++;
      /* 11: a verificação aceita a boa e recusa a adulterada */
      int ok = mos_ck_ed25519_verify(o, 0, 0, sig) == 0;
      sig[5] ^= 0x10;
      ok = ok && mos_ck_ed25519_verify(o, 0, 0, sig) != 0;
      if (!ok) fail |= 1 << bit;
      bit++; }
    /* 12..14: ECDSA e ECDH nas três curvas: assina/verifica, adultera, e os dois lados chegam ao mesmo segredo */
    int sizes[3] = {256, 384, 521};
    for (int c = 0; c < 3; c++) {
        int B = mos_ck_ec_size(sizes[c]);
        u8 pa[66], pb[66], qa[133], qb[133], qa2[133], rs[132], sa[66], sb[66], dg[64];
        int ok = mos_ck_ec_generate(sizes[c], pa, qa) == 0 && mos_ck_ec_generate(sizes[c], pb, qb) == 0;
        ok = ok && mos_ck_ec_public(sizes[c], pa, qa2) == 0 && !memcmp(qa, qa2, (size_t)(1 + 2 * B));
        mos_ck_hash(sizes[c] == 256 ? CK_SHA256 : sizes[c] == 384 ? CK_SHA384 : CK_SHA512, "mOS", 3, dg);
        int dl = sizes[c] == 256 ? 32 : sizes[c] == 384 ? 48 : 64;
        ok = ok && mos_ck_ec_sign(sizes[c], pa, dg, (size_t)dl, rs) == 0 && mos_ck_ec_verify(sizes[c], qa, dg, (size_t)dl, rs) == 0;
        dg[0] ^= 1;
        ok = ok && mos_ck_ec_verify(sizes[c], qa, dg, (size_t)dl, rs) != 0;
        ok = ok && mos_ck_ec_shared(sizes[c], pa, qb, sa) == 0 && mos_ck_ec_shared(sizes[c], pb, qa, sb) == 0 && !memcmp(sa, sb, (size_t)B);
        if (!ok) fail |= 1 << bit;
        bit++;
    }
    /* 15: AES Key Wrap, RFC 3394 §4.1 */
    unhex(k, "000102030405060708090a0b0c0d0e0f");
    unhex(m, "00112233445566778899aabbccddeeff");
    mos_ck_aes_wrap(k, 16, m, 16, o);
    { int ok = hexeq(o, 24, "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5");
      u8 back[16];
      ok = ok && mos_ck_aes_unwrap(k, 16, o, 24, back) == 0 && !memcmp(back, m, 16);
      if (!ok) fail |= 1 << bit; }
    bit++;
    return fail;
}

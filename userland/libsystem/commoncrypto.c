/*
 * mOS libSystem — pedaços do CommonCrypto (CC_SHA1, CC_SHA256), do libuuid (uuid_*) e do execinfo
 * (backtrace) que apps do macOS e o motor de grafo do OpenSwiftUI (Compute) importam.
 * CCRandomGenerateBytes já está em extra.c.
 */
#if defined(CK_HOST)
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#else
#include "sys.h"
#include "libc.h"
#endif

typedef unsigned char u8_;
typedef unsigned int u32_;
typedef unsigned long long u64_;

static u32_ rol(u32_ x, int n) { return (x << n) | (x >> (32 - n)); }
static u32_ ror(u32_ x, int n) { return (x >> n) | (x << (32 - n)); }
static u32_ be32(const u8_ *p) { return (u32_)p[0] << 24 | (u32_)p[1] << 16 | (u32_)p[2] << 8 | p[3]; }
static void put_be32(u8_ *p, u32_ v) { p[0] = (u8_)(v >> 24); p[1] = (u8_)(v >> 16); p[2] = (u8_)(v >> 8); p[3] = (u8_)v; }

/* ---- SHA-1: CC_SHA1_CTX tem o layout do macOS (h0..h4, Nl, Nh, data[16], num) */
typedef struct { u32_ h0, h1, h2, h3, h4, Nl, Nh; u32_ data[16]; int num; } CC_SHA1_CTX;

static void sha1_block(CC_SHA1_CTX *c, const u8_ *b) {
    u32_ w[80];
    for (int i = 0; i < 16; i++) w[i] = be32(b + 4 * i);
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    u32_ a = c->h0, bb = c->h1, cc = c->h2, d = c->h3, e = c->h4;
    for (int i = 0; i < 80; i++) {
        u32_ f, k;
        if (i < 20) { f = (bb & cc) | (~bb & d); k = 0x5A827999; }
        else if (i < 40) { f = bb ^ cc ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (bb & cc) | (bb & d) | (cc & d); k = 0x8F1BBCDC; }
        else { f = bb ^ cc ^ d; k = 0xCA62C1D6; }
        u32_ t = rol(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rol(bb, 30); bb = a; a = t;
    }
    c->h0 += a; c->h1 += bb; c->h2 += cc; c->h3 += d; c->h4 += e;
}
int CC_SHA1_Init(CC_SHA1_CTX *c) {
    memset(c, 0, sizeof *c);
    c->h0 = 0x67452301; c->h1 = 0xEFCDAB89; c->h2 = 0x98BADCFE; c->h3 = 0x10325476; c->h4 = 0xC3D2E1F0;
    return 1;
}
int CC_SHA1_Update(CC_SHA1_CTX *c, const void *data, u32_ len) {
    const u8_ *p = data;
    u8_ *buf = (u8_ *)c->data;
    u64_ bits = ((u64_)c->Nh << 32 | c->Nl) + (u64_)len * 8;
    c->Nl = (u32_)bits; c->Nh = (u32_)(bits >> 32);
    while (len--) {
        buf[c->num++] = *p++;
        if (c->num == 64) { sha1_block(c, buf); c->num = 0; }
    }
    return 1;
}
int CC_SHA1_Final(u8_ *md, CC_SHA1_CTX *c) {
    u8_ *buf = (u8_ *)c->data;
    u64_ bits = (u64_)c->Nh << 32 | c->Nl;
    buf[c->num++] = 0x80;
    if (c->num > 56) { while (c->num < 64) buf[c->num++] = 0; sha1_block(c, buf); c->num = 0; }
    while (c->num < 56) buf[c->num++] = 0;
    put_be32(buf + 56, (u32_)(bits >> 32)); put_be32(buf + 60, (u32_)bits);
    sha1_block(c, buf);
    put_be32(md, c->h0); put_be32(md + 4, c->h1); put_be32(md + 8, c->h2); put_be32(md + 12, c->h3); put_be32(md + 16, c->h4);
    memset(c, 0, sizeof *c);
    return 1;
}
u8_ *CC_SHA1(const void *data, u32_ len, u8_ *md) { CC_SHA1_CTX c; CC_SHA1_Init(&c); CC_SHA1_Update(&c, data, len); CC_SHA1_Final(md, &c); return md; }

/* ---- SHA-256: CC_SHA256_CTX {count[2], hash[8], wbuf[16]} como no macOS */
typedef struct { u32_ count[2]; u32_ hash[8]; u32_ wbuf[16]; } CC_SHA256_CTX;
static const u32_ K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
static void sha256_block(CC_SHA256_CTX *c, const u8_ *b) {
    u32_ w[64], s[8];
    for (int i = 0; i < 16; i++) w[i] = be32(b + 4 * i);
    for (int i = 16; i < 64; i++) {
        u32_ s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    for (int i = 0; i < 8; i++) s[i] = c->hash[i];
    for (int i = 0; i < 64; i++) {
        u32_ S1 = ror(s[4], 6) ^ ror(s[4], 11) ^ ror(s[4], 25), ch = (s[4] & s[5]) ^ (~s[4] & s[6]);
        u32_ t1 = s[7] + S1 + ch + K256[i] + w[i];
        u32_ S0 = ror(s[0], 2) ^ ror(s[0], 13) ^ ror(s[0], 22), mj = (s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]);
        u32_ t2 = S0 + mj;
        s[7] = s[6]; s[6] = s[5]; s[5] = s[4]; s[4] = s[3] + t1; s[3] = s[2]; s[2] = s[1]; s[1] = s[0]; s[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) c->hash[i] += s[i];
}
int CC_SHA256_Init(CC_SHA256_CTX *c) {
    static const u32_ iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memset(c, 0, sizeof *c);
    memcpy(c->hash, iv, sizeof iv);
    return 1;
}
int CC_SHA256_Update(CC_SHA256_CTX *c, const void *data, u32_ len) {
    const u8_ *p = data;
    u8_ *buf = (u8_ *)c->wbuf;
    u64_ bytes = (u64_)c->count[1] << 32 | c->count[0];
    while (len--) {
        buf[bytes % 64] = *p++;
        bytes++;
        if (bytes % 64 == 0) sha256_block(c, buf);
    }
    c->count[0] = (u32_)bytes; c->count[1] = (u32_)(bytes >> 32);
    return 1;
}
int CC_SHA256_Final(u8_ *md, CC_SHA256_CTX *c) {
    u8_ *buf = (u8_ *)c->wbuf;
    u64_ bytes = (u64_)c->count[1] << 32 | c->count[0];
    u64_ bits = bytes * 8;
    unsigned n = (unsigned)(bytes % 64);
    buf[n++] = 0x80;
    if (n > 56) { while (n < 64) buf[n++] = 0; sha256_block(c, buf); n = 0; }
    while (n < 56) buf[n++] = 0;
    put_be32(buf + 56, (u32_)(bits >> 32)); put_be32(buf + 60, (u32_)bits);
    sha256_block(c, buf);
    for (int i = 0; i < 8; i++) put_be32(md + 4 * i, c->hash[i]);
    memset(c, 0, sizeof *c);
    return 1;
}
u8_ *CC_SHA256(const void *data, u32_ len, u8_ *md) { CC_SHA256_CTX c; CC_SHA256_Init(&c); CC_SHA256_Update(&c, data, len); CC_SHA256_Final(md, &c); return md; }

/* ---- libuuid */
typedef u8_ uuid_t_[16];
void arc4random_buf(void *buf, size_t n);
void uuid_clear(u8_ *u) { memset(u, 0, 16); }
void uuid_copy(u8_ *dst, const u8_ *src) { memcpy(dst, src, 16); }
int uuid_compare(const u8_ *a, const u8_ *b) { return memcmp(a, b, 16); }
int uuid_is_null(const u8_ *u) { for (int i = 0; i < 16; i++) if (u[i]) return 0; return 1; }
void uuid_generate_random(u8_ *u) { arc4random_buf(u, 16); u[6] = (u8_)((u[6] & 0x0F) | 0x40); u[8] = (u8_)((u[8] & 0x3F) | 0x80); }
void uuid_generate(u8_ *u) { uuid_generate_random(u); }
static void unparse(const u8_ *u, char *out, const char *hex) {
    int j = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out[j++] = '-';
        out[j++] = hex[u[i] >> 4]; out[j++] = hex[u[i] & 15];
    }
    out[j] = 0;
}
void uuid_unparse_upper(const u8_ *u, char *out) { unparse(u, out, "0123456789ABCDEF"); }
void uuid_unparse_lower(const u8_ *u, char *out) { unparse(u, out, "0123456789abcdef"); }
void uuid_unparse(const u8_ *u, char *out) { uuid_unparse_upper(u, out); }     /* como no macOS: maiúsculas */
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
int uuid_parse(const char *in, u8_ *u) {
    int j = 0;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (in[i] != '-') return -1; continue; }
        int h = hexv(in[i]), l = hexv(in[i + 1]);
        if (h < 0 || l < 0) return -1;
        u[j++] = (u8_)(h << 4 | l);
        i++;
    }
    return in[36] ? -1 : 0;
}

/* ---- execinfo: o mOS ainda não desenrola a pilha do processo */
int backtrace(void **buffer, int size) { (void)buffer; (void)size; return 0; }
char **backtrace_symbols(void *const *buffer, int size) { (void)buffer; (void)size; return 0; }
void backtrace_symbols_fd(void *const *buffer, int size, int fd) { (void)buffer; (void)size; (void)fd; }
void backtrace_image_offsets(void *const *array, void *image_offsets, int size) { (void)array; (void)image_offsets; (void)size; }

/*
 * mOS — curva elíptica NIST P-256 (secp256r1) para o LE Secure Connections do Bluetooth:
 * geração de chave pública e ECDH. Inteiros de 256 bits em 8 palavras de 32 bits (little-endian),
 * redução rápida módulo p (FIPS 186-4 D.2.3), coordenadas jacobianas, inversão por Fermat.
 * Não é de tempo constante: usado só no pareamento, com chaves efêmeras.
 */
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

typedef struct { u32 v[8]; } fe;            /* elemento do corpo (mod p), v[0] menos significativo */

static const fe P  = {{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF}};
static const fe GX = {{0xD898C296, 0xF4A13945, 0x2DEB33A0, 0x77037D81, 0x63A440F2, 0xF8BCE6E5, 0xE12C4247, 0x6B17D1F2}};
static const fe GY = {{0x37BF51F5, 0xCBB64068, 0x6B315ECE, 0x2BCE3357, 0x7C0F9E16, 0x8EE7EB4A, 0xFE1A7F9B, 0x4FE342E2}};
static const fe B  = {{0x27D2604B, 0x3BCE3C3E, 0xCC53B0F6, 0x651D06B0, 0x769886BC, 0xB3EBBD55, 0xAA3A93E7, 0x5AC635D8}};

static int cmp(const fe *a, const fe *b) { for (int i = 7; i >= 0; i--) if (a->v[i] != b->v[i]) return a->v[i] > b->v[i] ? 1 : -1; return 0; }
static u32 add_raw(fe *r, const fe *a, const fe *b) { u64 c = 0; for (int i = 0; i < 8; i++) { c += (u64)a->v[i] + b->v[i]; r->v[i] = (u32)c; c >>= 32; } return (u32)c; }
static u32 sub_raw(fe *r, const fe *a, const fe *b) { i64 c = 0; for (int i = 0; i < 8; i++) { c += (i64)a->v[i] - b->v[i]; r->v[i] = (u32)c; c >>= 32; } return (u32)(c & 1); }
static void fadd(fe *r, const fe *a, const fe *b) { if (add_raw(r, a, b) || cmp(r, &P) >= 0) sub_raw(r, r, &P); }
static void fsub(fe *r, const fe *a, const fe *b) { if (sub_raw(r, a, b)) add_raw(r, r, &P); }

/* redução de um produto de 512 bits (c[0..15]) módulo p */
static void reduce(fe *r, const u32 *c) {
    i64 t[8];
    #define C(i) ((i64)c[i])
    t[0] = C(0) + C(8) + C(9) - C(11) - C(12) - C(13) - C(14);
    t[1] = C(1) + C(9) + C(10) - C(12) - C(13) - C(14) - C(15);
    t[2] = C(2) + C(10) + C(11) - C(13) - C(14) - C(15);
    t[3] = C(3) + 2 * C(11) + 2 * C(12) + C(13) - C(15) - C(8) - C(9);
    t[4] = C(4) + 2 * C(12) + 2 * C(13) + C(14) - C(9) - C(10);
    t[5] = C(5) + 2 * C(13) + 2 * C(14) + C(15) - C(10) - C(11);
    t[6] = C(6) + 3 * C(14) + 2 * C(15) + C(13) - C(8) - C(9);
    t[7] = C(7) + 3 * C(15) + C(8) - C(10) - C(11) - C(12) - C(13);
    #undef C
    /* propaga os vai-uns; o excesso (positivo ou negativo) volta como múltiplos de 2^256 mod p */
    i64 carry = 0;
    for (int i = 0; i < 8; i++) { t[i] += carry; carry = t[i] >> 32; t[i] &= 0xFFFFFFFF; }
    while (carry) {
        /* 2^256 ≡ 2^224 - 2^192 - 2^96 + 1 (mod p) */
        i64 k = carry;
        carry = 0;
        t[0] += k; t[3] -= k; t[6] -= k; t[7] += k;
        for (int i = 0; i < 8; i++) { t[i] += carry; carry = t[i] >> 32; t[i] &= 0xFFFFFFFF; }
    }
    for (int i = 0; i < 8; i++) r->v[i] = (u32)t[i];
    while (cmp(r, &P) >= 0) sub_raw(r, r, &P);
}

static void fmul(fe *r, const fe *a, const fe *b) {
    u32 c[16] = {0};
    for (int i = 0; i < 8; i++) {
        u64 carry = 0;
        for (int j = 0; j < 8; j++) {
            u64 x = (u64)a->v[i] * b->v[j] + c[i + j] + carry;
            c[i + j] = (u32)x;
            carry = x >> 32;
        }
        c[i + 8] = (u32)carry;
    }
    reduce(r, c);
}

static void finv(fe *r, const fe *a) {                       /* a^(p-2) */
    fe e = P, x = {{1}}, base = *a;
    e.v[0] -= 2;
    for (int i = 0; i < 256; i++) {
        if (e.v[i / 32] >> (i % 32) & 1) fmul(&x, &x, &base);
        fmul(&base, &base, &base);
    }
    *r = x;
}

/* ---- pontos em coordenadas jacobianas (X/Z², Y/Z³); Z = 0 é o infinito */
typedef struct { fe x, y, z; } jp;

static bool is_zero(const fe *a) { u32 o = 0; for (int i = 0; i < 8; i++) o |= a->v[i]; return !o; }

static void dbl(jp *r, const jp *p) {
    if (is_zero(&p->z) || is_zero(&p->y)) { memset(r, 0, sizeof *r); return; }
    fe s, m, t, y2, z2;
    fmul(&y2, &p->y, &p->y);
    fmul(&s, &p->x, &y2); fadd(&s, &s, &s); fadd(&s, &s, &s);          /* S = 4·X·Y² */
    fmul(&z2, &p->z, &p->z);
    fe a, b2;
    fsub(&a, &p->x, &z2); fadd(&b2, &p->x, &z2); fmul(&m, &a, &b2);     /* M = 3(X-Z²)(X+Z²) (a = -3) */
    fadd(&t, &m, &m); fadd(&m, &m, &t);
    fe x3, y3, z3, y4;
    fmul(&x3, &m, &m); fsub(&x3, &x3, &s); fsub(&x3, &x3, &s);
    fmul(&y4, &y2, &y2); fadd(&y4, &y4, &y4); fadd(&y4, &y4, &y4); fadd(&y4, &y4, &y4);
    fsub(&t, &s, &x3); fmul(&y3, &m, &t); fsub(&y3, &y3, &y4);
    fmul(&z3, &p->y, &p->z); fadd(&z3, &z3, &z3);
    r->x = x3; r->y = y3; r->z = z3;
}

static void add_pt(jp *r, const jp *p, const jp *q) {
    if (is_zero(&p->z)) { *r = *q; return; }
    if (is_zero(&q->z)) { *r = *p; return; }
    fe z1z1, z2z2, u1, u2, s1, s2, h, rr;
    fmul(&z1z1, &p->z, &p->z); fmul(&z2z2, &q->z, &q->z);
    fmul(&u1, &p->x, &z2z2); fmul(&u2, &q->x, &z1z1);
    fmul(&s1, &p->y, &q->z); fmul(&s1, &s1, &z2z2);
    fmul(&s2, &q->y, &p->z); fmul(&s2, &s2, &z1z1);
    fsub(&h, &u2, &u1); fsub(&rr, &s2, &s1);
    if (is_zero(&h)) { if (is_zero(&rr)) dbl(r, p); else memset(r, 0, sizeof *r); return; }
    fe h2, h3, u1h2, x3, y3, z3, t;
    fmul(&h2, &h, &h); fmul(&h3, &h2, &h); fmul(&u1h2, &u1, &h2);
    fmul(&x3, &rr, &rr); fsub(&x3, &x3, &h3); fsub(&x3, &x3, &u1h2); fsub(&x3, &x3, &u1h2);
    fsub(&t, &u1h2, &x3); fmul(&y3, &rr, &t); fmul(&t, &s1, &h3); fsub(&y3, &y3, &t);
    fmul(&z3, &p->z, &q->z); fmul(&z3, &z3, &h);
    r->x = x3; r->y = y3; r->z = z3;
}

static void to_affine(fe *x, fe *y, const jp *p) {
    fe zi, zi2, zi3;
    finv(&zi, &p->z);
    fmul(&zi2, &zi, &zi); fmul(&zi3, &zi2, &zi);
    fmul(x, &p->x, &zi2); fmul(y, &p->y, &zi3);
}

static bool on_curve(const fe *x, const fe *y) {          /* y² = x³ - 3x + b */
    fe l, r, t;
    fmul(&l, y, y);
    fmul(&r, x, x); fmul(&r, &r, x);
    fadd(&t, x, x); fadd(&t, &t, x); fsub(&r, &r, &t); fadd(&r, &r, &B);
    return cmp(&l, &r) == 0;
}

static void scalar_mul(fe *ox, fe *oy, const u32 k[8], const fe *x, const fe *y) {
    jp acc = {0}, base = {*x, *y, {{1}}};
    for (int i = 255; i >= 0; i--) {
        dbl(&acc, &acc);
        if (k[i / 32] >> (i % 32) & 1) add_pt(&acc, &acc, &base);
    }
    to_affine(ox, oy, &acc);
}

/* ---- API (bytes em little-endian, como no SMP do Bluetooth) */
static void load(fe *a, const u8 *le) { for (int i = 0; i < 8; i++) a->v[i] = (u32)le[i * 4] | (u32)le[i * 4 + 1] << 8 | (u32)le[i * 4 + 2] << 16 | (u32)le[i * 4 + 3] << 24; }
static void store(u8 *le, const fe *a) { for (int i = 0; i < 8; i++) { le[i * 4] = (u8)a->v[i]; le[i * 4 + 1] = (u8)(a->v[i] >> 8); le[i * 4 + 2] = (u8)(a->v[i] >> 16); le[i * 4 + 3] = (u8)(a->v[i] >> 24); } }

/* chave pública = priv·G; priv deve estar em [1, n-1] */
void p256_public(const u8 priv_le[32], u8 x_le[32], u8 y_le[32]) {
    fe k, x, y;
    load(&k, priv_le);
    scalar_mul(&x, &y, k.v, &GX, &GY);
    store(x_le, &x);
    store(y_le, &y);
}

/* DHKey = coordenada x de priv·(peer); false se o ponto do outro lado não estiver na curva */
bool p256_ecdh(const u8 priv_le[32], const u8 px_le[32], const u8 py_le[32], u8 dh_le[32]) {
    fe k, px, py, x, y;
    load(&k, priv_le); load(&px, px_le); load(&py, py_le);
    if (cmp(&px, &P) >= 0 || cmp(&py, &P) >= 0 || !on_curve(&px, &py)) return false;
    scalar_mul(&x, &y, k.v, &px, &py);
    store(dh_le, &x);
    return true;
}

bool p256_on_curve(const u8 x_le[32], const u8 y_le[32]) { fe x, y; load(&x, x_le); load(&y, y_le); return on_curve(&x, &y); }

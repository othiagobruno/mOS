/*
 * mOS — tradutor arm64: criptografia do AdvSIMD (frente SIMD): AES (FEAT_AES), PMULL de 64 bits
 * (FEAT_PMULL), SHA1/SHA256 (FEAT_SHA1/SHA256), SHA512 e SHA3 (EOR3/RAX1/XAR/BCAX). Pseudocódigo
 * do ARM ARM (AESSubBytes, AESShiftRows, AESMixColumns, SHAhashSIGMA*, SHA256hash...).
 */
#include "interp_simd.h"

/* ---------------------------------------------------------------------------------------------
 * AES
 * ------------------------------------------------------------------------------------------- */

static a64_u8 g_sbox[256], g_isbox[256];
static int g_sbox_ok;

static a64_u8 gmul(a64_u8 a, a64_u8 b) {
    a64_u8 r = 0;
    while (b) {
        if (b & 1) r ^= a;
        a = (a64_u8)((a << 1) ^ ((a & 0x80) ? 0x1B : 0));
        b >>= 1;
    }
    return r;
}

static void sbox_init(void) {
    if (__atomic_load_n(&g_sbox_ok, __ATOMIC_ACQUIRE)) return;
    for (int x = 0; x < 256; x++) {
        a64_u8 inv = 0;
        if (x) {                                          /* x^254 */
            a64_u8 p = (a64_u8)x, r = 1;
            for (int e = 254; e; e >>= 1) { if (e & 1) r = gmul(r, p); p = gmul(p, p); }
            inv = r;
        }
        a64_u8 b = inv, s = b;
        for (int k = 1; k <= 4; k++) s ^= (a64_u8)((b << k) | (b >> (8 - k)));
        s ^= 0x63;
        g_sbox[x] = s;
        g_isbox[s] = (a64_u8)x;
    }
    __atomic_store_n(&g_sbox_ok, 1, __ATOMIC_RELEASE);
}

static void mixcol(a64_u8 *s, int inv) {
    for (int col = 0; col < 4; col++) {
        a64_u8 *a = s + 4 * col, t[4];
        for (int r = 0; r < 4; r++) {
            if (!inv)
                t[r] = (a64_u8)(gmul(a[r], 2) ^ gmul(a[(r + 1) & 3], 3) ^ a[(r + 2) & 3] ^ a[(r + 3) & 3]);
            else
                t[r] = (a64_u8)(gmul(a[r], 14) ^ gmul(a[(r + 1) & 3], 11) ^ gmul(a[(r + 2) & 3], 13) ^ gmul(a[(r + 3) & 3], 9));
        }
        for (int r = 0; r < 4; r++) a[r] = t[r];
    }
}

int a64s_aes(a64_cpu *c, a64_u32 i) {
    int op = SB(i, 16, 12), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (op < 4 || op > 7) return UDF;
    sbox_init();
    vr x = c->v[n], r;
    switch (op) {
    case 4: case 5: {                                     /* AESE AESD */
        a64_u8 st[16];
        for (int k = 0; k < 16; k++) st[k] = (a64_u8)(c->v[d].b[k] ^ x.b[k]);
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++) {
                int src = op == 4 ? 4 * ((col + row) & 3) + row : 4 * ((col - row) & 3) + row;
                r.b[4 * col + row] = op == 4 ? g_sbox[st[src]] : g_isbox[st[src]];
            }
        break;
    }
    default:                                              /* AESMC AESIMC */
        r = x;
        mixcol(r.b, op == 7);
        break;
    }
    c->v[d] = r;
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * PMULL/PMULL2 de 64 bits
 * ------------------------------------------------------------------------------------------- */

int a64s_pmull64(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    a64_u64 x = c->v[n].d[q], y = c->v[m].d[q];
    u128 r = 0;
    for (int b = 0; b < 64; b++) if ((y >> b) & 1) r ^= (u128)x << b;
    c->v[d].q = r;
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * SHA1 / SHA256
 * ------------------------------------------------------------------------------------------- */

static a64_u32 sha_choose(a64_u32 x, a64_u32 y, a64_u32 z) { return ((y ^ z) & x) ^ z; }
static a64_u32 sha_major(a64_u32 x, a64_u32 y, a64_u32 z) { return (x & y) | ((x | y) & z); }
static a64_u32 sha_parity(a64_u32 x, a64_u32 y, a64_u32 z) { return x ^ y ^ z; }
static a64_u32 sigma0(a64_u32 x) { return ror32(x, 2) ^ ror32(x, 13) ^ ror32(x, 22); }
static a64_u32 sigma1(a64_u32 x) { return ror32(x, 6) ^ ror32(x, 11) ^ ror32(x, 25); }

/* SHA256hash(X, Y, W, part1) */
static vr sha256hash(vr X, vr Y, const vr *W, int part1) {
    for (int e = 0; e < 4; e++) {
        a64_u32 chs = sha_choose(Y.s[0], Y.s[1], Y.s[2]);
        a64_u32 maj = sha_major(X.s[0], X.s[1], X.s[2]);
        a64_u32 t = Y.s[3] + sigma1(Y.s[0]) + chs + W->s[e];
        X.s[3] = t + X.s[3];
        Y.s[3] = t + sigma0(X.s[0]) + maj;
        /* <Y, X> = ROL(Y : X, 32) */
        a64_u32 yt = Y.s[3], xt = X.s[3];
        Y.s[3] = Y.s[2]; Y.s[2] = Y.s[1]; Y.s[1] = Y.s[0]; Y.s[0] = xt;
        X.s[3] = X.s[2]; X.s[2] = X.s[1]; X.s[1] = X.s[0]; X.s[0] = yt;
    }
    return part1 ? X : Y;
}

int a64s_sha3reg(a64_cpu *c, a64_u32 i) {
    int op = SB(i, 14, 12), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    vr X = c->v[d], Yv = c->v[n], W = c->v[m], r;
    switch (op) {
    case 0: case 1: case 2: {                             /* SHA1C SHA1P SHA1M */
        a64_u32 Y = Yv.s[0];
        for (int e = 0; e < 4; e++) {
            a64_u32 t = op == 0 ? sha_choose(X.s[1], X.s[2], X.s[3])
                      : op == 1 ? sha_parity(X.s[1], X.s[2], X.s[3]) : sha_major(X.s[1], X.s[2], X.s[3]);
            Y = Y + rol32(X.s[0], 5) + t + W.s[e];
            X.s[1] = rol32(X.s[1], 30);
            /* <Y, X> = ROL(Y : X, 32) */
            a64_u32 ny = X.s[3];
            X.s[3] = X.s[2]; X.s[2] = X.s[1]; X.s[1] = X.s[0]; X.s[0] = Y;
            Y = ny;
        }
        r = X;
        break;
    }
    case 3:                                               /* SHA1SU0 */
        r.d[0] = X.d[1]; r.d[1] = Yv.d[0];
        r.d[0] ^= X.d[0] ^ W.d[0];
        r.d[1] ^= X.d[1] ^ W.d[1];
        break;
    case 4: r = sha256hash(X, Yv, &W, 1); break;          /* SHA256H */
    case 5: r = sha256hash(Yv, X, &W, 0); break;          /* SHA256H2 */
    case 6: {                                             /* SHA256SU1 */
        a64_u32 T0[4] = { Yv.s[1], Yv.s[2], Yv.s[3], W.s[0] };
        for (int e = 0; e < 2; e++) {
            a64_u32 el = W.s[2 + e];
            el = ror32(el, 17) ^ ror32(el, 19) ^ (el >> 10);
            r.s[e] = el + X.s[e] + T0[e];
        }
        for (int e = 2; e < 4; e++) {
            a64_u32 el = r.s[e - 2];
            el = ror32(el, 17) ^ ror32(el, 19) ^ (el >> 10);
            r.s[e] = el + X.s[e] + T0[e];
        }
        break;
    }
    default: return UDF;
    }
    c->v[d] = r;
    return NEXT;
}

int a64s_sha2reg(a64_cpu *c, a64_u32 i) {
    int op = SB(i, 16, 12), n = SB(i, 9, 5), d = SB(i, 4, 0);
    vr x = c->v[n], dv = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    switch (op) {
    case 0:                                               /* SHA1H */
        r.s[0] = rol32(x.s[0], 30);
        break;
    case 1: {                                             /* SHA1SU1 */
        a64_u32 T[4] = { dv.s[0] ^ x.s[1], dv.s[1] ^ x.s[2], dv.s[2] ^ x.s[3], dv.s[3] };
        r.s[0] = rol32(T[0], 1);
        r.s[1] = rol32(T[1], 1);
        r.s[2] = rol32(T[2], 1);
        r.s[3] = rol32(T[3], 1) ^ rol32(T[0], 2);
        break;
    }
    case 2: {                                             /* SHA256SU0 */
        a64_u32 T[4] = { dv.s[1], dv.s[2], dv.s[3], x.s[0] };
        for (int e = 0; e < 4; e++) {
            a64_u32 el = T[e];
            el = ror32(el, 7) ^ ror32(el, 18) ^ (el >> 3);
            r.s[e] = el + dv.s[e];
        }
        break;
    }
    default: return UDF;
    }
    c->v[d] = r;
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * op0 = 1100: EOR3/BCAX, SHA512H/H2/SU1, RAX1, XAR, SHA512SU0 (SM3/SM4 não anunciados → UDF)
 * ------------------------------------------------------------------------------------------- */

int a64s_crypto_c(a64_cpu *c, a64_u32 i) {
    if (SB(i, 31, 24) != 0xCE) return UDF;
    int m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0), ra = SB(i, 14, 10);
    vr X = c->v[n], Y = c->v[m], W = c->v[d], r;
    switch (SB(i, 23, 21)) {
    case 0: case 1:                                       /* EOR3 BCAX */
        if (SBT(i, 15)) return UDF;
        for (int k = 0; k < 2; k++)
            r.d[k] = SBT(i, 21) ? X.d[k] ^ (Y.d[k] & ~c->v[ra].d[k]) : X.d[k] ^ Y.d[k] ^ c->v[ra].d[k];
        break;
    case 3:
        if (!SBT(i, 15) || SBT(i, 14) || SB(i, 13, 12) != 0) return UDF;
        switch (SB(i, 11, 10)) {
        case 0: {                                         /* SHA512H */
            a64_u64 s1 = ror64(Y.d[1], 14) ^ ror64(Y.d[1], 18) ^ ror64(Y.d[1], 41);
            a64_u64 hi = (Y.d[1] & X.d[0]) ^ (~Y.d[1] & X.d[1]);
            hi = hi + s1 + W.d[1];
            a64_u64 tmp = hi + Y.d[0];
            s1 = ror64(tmp, 14) ^ ror64(tmp, 18) ^ ror64(tmp, 41);
            a64_u64 lo = (tmp & Y.d[1]) ^ (~tmp & X.d[0]);
            lo = lo + s1 + W.d[0];
            r.d[0] = lo; r.d[1] = hi;
            break;
        }
        case 1: {                                         /* SHA512H2 */
            a64_u64 s0 = ror64(Y.d[0], 28) ^ ror64(Y.d[0], 34) ^ ror64(Y.d[0], 39);
            a64_u64 hi = (X.d[0] & Y.d[1]) ^ (X.d[0] & Y.d[0]) ^ (Y.d[1] & Y.d[0]);
            hi = hi + s0 + W.d[1];
            s0 = ror64(hi, 28) ^ ror64(hi, 34) ^ ror64(hi, 39);
            a64_u64 lo = (hi & Y.d[0]) ^ (hi & Y.d[1]) ^ (Y.d[1] & Y.d[0]);
            lo = lo + s0 + W.d[0];
            r.d[0] = lo; r.d[1] = hi;
            break;
        }
        case 2: {                                         /* SHA512SU1 */
            a64_u64 s1 = ror64(X.d[1], 19) ^ ror64(X.d[1], 61) ^ (X.d[1] >> 6);
            r.d[1] = W.d[1] + s1 + Y.d[1];
            s1 = ror64(X.d[0], 19) ^ ror64(X.d[0], 61) ^ (X.d[0] >> 6);
            r.d[0] = W.d[0] + s1 + Y.d[0];
            break;
        }
        default:                                          /* RAX1 */
            for (int k = 0; k < 2; k++) r.d[k] = X.d[k] ^ ror64(Y.d[k], 63);
            break;
        }
        break;
    case 4: {                                             /* XAR */
        int imm6 = SB(i, 15, 10);
        for (int k = 0; k < 2; k++) r.d[k] = ror64(X.d[k] ^ Y.d[k], (unsigned)imm6);
        break;
    }
    case 6:
        if (SB(i, 20, 10) != 0x020) return UDF;           /* 00000 1000 00: SHA512SU0 (01 = SM4E) */
        {
            a64_u64 s0 = ror64(W.d[1], 1) ^ ror64(W.d[1], 8) ^ (W.d[1] >> 7);
            r.d[0] = W.d[0] + s0;
            s0 = ror64(X.d[0], 1) ^ ror64(X.d[0], 8) ^ (X.d[0] >> 7);
            r.d[1] = W.d[1] + s0;
        }
        break;
    default:
        return UDF;
    }
    c->v[d] = r;
    return NEXT;
}

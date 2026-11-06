/* mOS — verificação de assinaturas de chave pública (RSA e ECDSA).
 *
 * Aritmética multiprecisão com limbs de 32 bits (produtos em u64; nada de __int128,
 * que exigiria compiler-rt no alvo UEFI). Multiplicação de Montgomery (CIOS) serve
 * tanto para a exponenciação modular do RSA (até 4096 bits) quanto para os corpos
 * primos de P-256/P-384 e para a aritmética módulo a ordem do grupo. RSA até 8192 bits.
 *
 * As verificações (RSA/ECDSA) não envolvem segredos, então não há preocupação
 * com tempo constante. A exceção é o ECDH P-256 no fim do arquivo (TLS 1.2/1.3),
 * que usa um escalar secreto — ver o comentário lá.
 */
#include "x509.h"

#define BN_LIMBS 258                     /* 8192 bits + folga */
#define EC_LIMBS 12                      /* 384 bits */

typedef struct {
    int n;                               /* número de limbs */
    u32 m0inv;                           /* -m^-1 mod 2^32 */
    u32 m[BN_LIMBS];
    u32 rr[BN_LIMBS];                    /* R^2 mod m */
    u32 one[BN_LIMBS];                   /* R mod m (o 1 na forma de Montgomery) */
} mont_t;

/* ---------------------------------------------------------------- inteiros */
static int bn_cmp(const u32 *a, const u32 *b, int n) {
    for (int i = n - 1; i >= 0; i--)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}

static u32 bn_add(u32 *r, const u32 *a, const u32 *b, int n) {
    u64 c = 0;
    for (int i = 0; i < n; i++) { c += (u64)a[i] + b[i]; r[i] = (u32)c; c >>= 32; }
    return (u32)c;
}

static u32 bn_sub(u32 *r, const u32 *a, const u32 *b, int n) {
    u64 br = 0;
    for (int i = 0; i < n; i++) {
        u64 t = (u64)a[i] - b[i] - br;
        r[i] = (u32)t;
        br = (t >> 32) & 1;
    }
    return (u32)br;
}

static bool bn_is_zero(const u32 *a, int n) {
    u32 z = 0;
    for (int i = 0; i < n; i++) z |= a[i];
    return z == 0;
}

static void bn_set_word(u32 *r, int n, u32 w) {
    memset(r, 0, (usize)n * 4);
    r[0] = w;
}

/* big-endian -> limbs; falso se o valor não couber em n limbs */
static bool bn_from_be(u32 *r, int n, const u8 *b, u32 len) {
    memset(r, 0, (usize)n * 4);
    while (len && !*b) { b++; len--; }
    if (len > (u32)n * 4) return false;
    for (u32 i = 0; i < len; i++) r[i / 4] |= (u32)b[len - 1 - i] << (8 * (i % 4));
    return true;
}

static void bn_to_be(u8 *out, u32 len, const u32 *a, int n) {
    for (u32 i = 0; i < len; i++) {
        u32 li = i / 4;
        out[len - 1 - i] = li < (u32)n ? (u8)(a[li] >> (8 * (i % 4))) : 0;
    }
}

static int bn_bits(const u32 *a, int n) {
    for (int i = n - 1; i >= 0; i--)
        if (a[i]) {
            int b = 32;
            while (!(a[i] >> (b - 1))) b--;
            return i * 32 + b;
        }
    return 0;
}

static inline int bn_bit(const u32 *a, int i) { return (a[i >> 5] >> (i & 31)) & 1; }

/* ---------------------------------------------------------------- Montgomery */
static bool mont_init(mont_t *M, const u32 *m, int n) {
    if (n < 1 || n > BN_LIMBS || !(m[0] & 1)) return false;
    M->n = n;
    memcpy(M->m, m, (usize)n * 4);
    u32 inv = 1;
    for (int k = 0; k < 5; k++) inv *= 2 - m[0] * inv;    /* Newton: m0 * inv ≡ 1 (mod 2^32) */
    M->m0inv = 0u - inv;
    /* 2^i mod m por duplicações sucessivas: i = 32n dá R, i = 64n dá R^2 */
    u32 x[BN_LIMBS];
    bn_set_word(x, n, 1);
    if (n == 1 && m[0] == 1) return false;
    for (int i = 0; i < 64 * n; i++) {
        if (i == 32 * n) memcpy(M->one, x, (usize)n * 4);
        u32 top = bn_add(x, x, x, n);
        if (top || bn_cmp(x, M->m, n) >= 0) bn_sub(x, x, M->m, n);
    }
    memcpy(M->rr, x, (usize)n * 4);
    return true;
}

/* r = a * b * R^-1 mod m   (a, b < m; r pode coincidir com a ou b) */
static void mont_mul(const mont_t *M, u32 *r, const u32 *a, const u32 *b) {
    int n = M->n;
    u32 t[BN_LIMBS + 2];
    memset(t, 0, (usize)(n + 2) * 4);
    for (int i = 0; i < n; i++) {
        u64 c = 0;
        u32 bi = b[i];
        for (int j = 0; j < n; j++) {
            c += (u64)t[j] + (u64)a[j] * bi;
            t[j] = (u32)c;
            c >>= 32;
        }
        c += t[n];
        t[n] = (u32)c;
        t[n + 1] = (u32)(c >> 32);
        u32 q = t[0] * M->m0inv;
        c = ((u64)t[0] + (u64)q * M->m[0]) >> 32;
        for (int j = 1; j < n; j++) {
            c += (u64)t[j] + (u64)q * M->m[j];
            t[j - 1] = (u32)c;
            c >>= 32;
        }
        c += t[n];
        t[n - 1] = (u32)c;
        t[n] = t[n + 1] + (u32)(c >> 32);
    }
    if (t[n] || bn_cmp(t, M->m, n) >= 0) bn_sub(t, t, M->m, n);
    memcpy(r, t, (usize)n * 4);
}

static void mont_to(const mont_t *M, u32 *r, const u32 *a) { mont_mul(M, r, a, M->rr); }
static void mont_from(const mont_t *M, u32 *r, const u32 *a) {
    u32 one[BN_LIMBS];
    bn_set_word(one, M->n, 1);
    mont_mul(M, r, a, one);
}

/* r = base^exp na forma de Montgomery (base já convertida); exp em limbs (en limbs) */
static void mont_pow(const mont_t *M, u32 *r, const u32 *base, const u32 *exp, int en) {
    u32 acc[BN_LIMBS];
    memcpy(acc, M->one, (usize)M->n * 4);
    for (int i = bn_bits(exp, en) - 1; i >= 0; i--) {
        mont_mul(M, acc, acc, acc);
        if (bn_bit(exp, i)) mont_mul(M, acc, acc, base);
    }
    memcpy(r, acc, (usize)M->n * 4);
}

/* ======================================================================
 * RSA
 * ====================================================================== */
u32 rsa_modulus_bits(const u8 *n, u32 nlen) {
    while (nlen && !*n) { n++; nlen--; }
    if (!nlen) return 0;
    u32 b = 8;
    while (!(n[0] >> (b - 1))) b--;
    return (nlen - 1) * 8 + b;
}

/* em = sig^e mod n, com k = bytes do módulo; devolve k ou 0 em erro */
static u32 rsa_public(const u8 *n, u32 nlen, const u8 *e, u32 elen, const u8 *sig, u32 siglen, u8 *em) {
    while (nlen && !*n) { n++; nlen--; }
    u32 k = nlen;
    if (k < 64 || k > 1024) return 0;                /* 512..8192 bits */
    while (siglen > k && !*sig) { sig++; siglen--; }
    if (siglen > k) return 0;
    int limbs = (int)((k + 3) / 4);
    mont_t *M = kmalloc(sizeof(mont_t));
    u32 *buf = kmalloc(3 * BN_LIMBS * 4);
    if (!M || !buf) { kfree(M); kfree(buf); return 0; }
    u32 *mod = buf, *s = buf + BN_LIMBS, *ex = buf + 2 * BN_LIMBS;
    u32 ret = 0;
    bn_from_be(mod, limbs, n, k);
    bn_from_be(s, limbs, sig, siglen);
    if (!bn_from_be(ex, BN_LIMBS, e, elen) || bn_is_zero(ex, BN_LIMBS)) goto out;
    if (!mont_init(M, mod, limbs)) goto out;
    if (bn_cmp(s, mod, limbs) >= 0) goto out;        /* representante fora do intervalo */
    mont_to(M, s, s);
    mont_pow(M, s, s, ex, BN_LIMBS);
    mont_from(M, s, s);
    bn_to_be(em, k, s, limbs);
    ret = k;
out:
    kfree(M);
    kfree(buf);
    return ret;
}

int rsa_verify_pkcs1(const u8 *n, u32 nlen, const u8 *e, u32 elen, int halg,
                     const u8 *digest, const u8 *sig, u32 siglen) {
    static const u8 di256[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
    static const u8 di384[] = {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
    static const u8 di512[] = {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
    const u8 *di = halg == HASH_SHA384 ? di384 : halg == HASH_SHA512 ? di512 : di256;
    u32 dil = sizeof(di256), hl = hash_len(halg);
    u8 em[1024];
    u32 k = rsa_public(n, nlen, e, elen, sig, siglen, em);
    if (!k || k < dil + hl + 11) return -1;
    /* EM = 00 01 FF..FF 00 || DigestInfo || H */
    u32 ps = k - 3 - dil - hl;
    int bad = em[0] != 0x00 || em[1] != 0x01 || em[2 + ps] != 0x00;
    for (u32 i = 0; i < ps; i++) bad |= em[2 + i] != 0xff;
    bad |= memcmp(em + 3 + ps, di, dil) != 0;
    bad |= memcmp(em + 3 + ps + dil, digest, hl) != 0;
    return bad ? -1 : 0;
}

static void mgf1_xor(int halg, const u8 *seed, u32 slen, u8 *out, u32 olen) {
    u32 hl = hash_len(halg);
    u8 h[HASH_MAX_LEN], ctr[4];
    for (u32 c = 0, off = 0; off < olen; c++) {
        ctr[0] = (u8)(c >> 24); ctr[1] = (u8)(c >> 16); ctr[2] = (u8)(c >> 8); ctr[3] = (u8)c;
        hash_ctx hc;
        hash_init(&hc, halg);
        hash_update(&hc, seed, slen);
        hash_update(&hc, ctr, 4);
        hash_final(&hc, h);
        for (u32 i = 0; i < hl && off < olen; i++) out[off++] ^= h[i];
    }
}

int rsa_verify_pss(const u8 *n, u32 nlen, const u8 *e, u32 elen, int halg,
                   const u8 *digest, const u8 *sig, u32 siglen, int saltlen) {
    u8 buf[1024];
    u32 k = rsa_public(n, nlen, e, elen, sig, siglen, buf);
    if (!k) return -1;
    u32 hl = hash_len(halg);
    u32 embits = rsa_modulus_bits(n, nlen) - 1;
    u32 emlen = (embits + 7) / 8;
    u8 *em = buf;
    if (emlen < k) {                     /* módulo com bits ≡ 1 (mod 8): byte inicial deve ser 0 */
        if (buf[0]) return -1;
        em++;
    }
    if (emlen < hl + 2 || em[emlen - 1] != 0xbc) return -1;
    u32 dblen = emlen - hl - 1;
    u8 *db = em, *h = em + dblen;
    u32 topbits = 8 * emlen - embits;
    u8 topmask = (u8)(0xff >> topbits);
    if (db[0] & ~topmask) return -1;
    mgf1_xor(halg, h, hl, db, dblen);
    db[0] &= topmask;
    u32 i = 0;
    while (i < dblen && db[i] == 0) i++;
    if (i == dblen || db[i] != 0x01) return -1;
    i++;
    u32 slen = dblen - i;
    if (saltlen >= 0 && slen != (u32)saltlen) return -1;
    /* H' = Hash(00*8 || mHash || salt) */
    static const u8 zeros[8] = {0};
    hash_ctx hc;
    u8 hh[HASH_MAX_LEN];
    hash_init(&hc, halg);
    hash_update(&hc, zeros, 8);
    hash_update(&hc, digest, hl);
    hash_update(&hc, db + i, slen);
    hash_final(&hc, hh);
    return memcmp(hh, h, hl) ? -1 : 0;
}

/* ======================================================================
 * ECDSA sobre P-256 / P-384 (a = -3), coordenadas jacobianas
 * ====================================================================== */
typedef struct {
    bool ready, ok;
    int n;                               /* limbs */
    u32 bytes;
    mont_t P, N;                         /* corpo primo e ordem do grupo */
    u32 b[EC_LIMBS], gx[EC_LIMBS], gy[EC_LIMBS];   /* forma de Montgomery (mod p) */
    u32 p_2[EC_LIMBS], n_2[EC_LIMBS];    /* p-2 e n-2 (expoentes de Fermat) */
} curve_t;

typedef struct { u32 x[EC_LIMBS], y[EC_LIMBS], z[EC_LIMBS]; } jpt;

static curve_t g_p256, g_p384;

static const char *P256_HEX[] = {
    "ffffffff00000001000000000000000000000000ffffffffffffffffffffffff",
    "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
    "5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b",
    "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296",
    "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
};
static const char *P384_HEX[] = {
    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff",
    "ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973",
    "b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef",
    "aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7",
    "3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
};

static void hex_limbs(u32 *r, int n, const char *h) {
    u8 b[EC_LIMBS * 4];
    u32 len = (u32)strlen(h) / 2;
    for (u32 i = 0; i < len; i++) {
        int v = 0;
        for (int k = 0; k < 2; k++) {
            char ch = h[2 * i + k];
            v = v * 16 + (ch >= 'a' ? ch - 'a' + 10 : ch - '0');
        }
        b[i] = (u8)v;
    }
    bn_from_be(r, n, b, len);
}

/* --- operações no corpo (forma de Montgomery) --- */
static void f_add(const curve_t *C, u32 *r, const u32 *a, const u32 *b) {
    u32 c = bn_add(r, a, b, C->n);
    if (c || bn_cmp(r, C->P.m, C->n) >= 0) bn_sub(r, r, C->P.m, C->n);
}
static void f_sub(const curve_t *C, u32 *r, const u32 *a, const u32 *b) {
    if (bn_sub(r, a, b, C->n)) bn_add(r, r, C->P.m, C->n);
}
static void f_mul(const curve_t *C, u32 *r, const u32 *a, const u32 *b) { mont_mul(&C->P, r, a, b); }

/* y^2 == x^3 - 3x + b ? (x, y na forma de Montgomery) */
static bool on_curve(const curve_t *C, const u32 *x, const u32 *y) {
    u32 l[EC_LIMBS], r[EC_LIMBS], t[EC_LIMBS];
    f_mul(C, l, y, y);
    f_mul(C, r, x, x);
    f_mul(C, r, r, x);
    f_add(C, t, x, x);
    f_add(C, t, t, x);
    f_sub(C, r, r, t);
    f_add(C, r, r, C->b);
    return bn_cmp(l, r, C->n) == 0;
}

static void pt_dbl(const curve_t *C, jpt *r, const jpt *p) {
    int n = C->n;
    if (bn_is_zero(p->z, n)) { *r = *p; return; }
    u32 delta[EC_LIMBS], gamma[EC_LIMBS], beta[EC_LIMBS], alpha[EC_LIMBS], t1[EC_LIMBS], t2[EC_LIMBS];
    u32 x3[EC_LIMBS], y3[EC_LIMBS], z3[EC_LIMBS];
    f_mul(C, delta, p->z, p->z);
    f_mul(C, gamma, p->y, p->y);
    f_mul(C, beta, p->x, gamma);
    f_sub(C, t1, p->x, delta);
    f_add(C, t2, p->x, delta);
    f_mul(C, alpha, t1, t2);
    f_add(C, t1, alpha, alpha);
    f_add(C, alpha, t1, alpha);                 /* alpha = 3(x-δ)(x+δ) */
    f_mul(C, x3, alpha, alpha);
    f_add(C, t1, beta, beta);
    f_add(C, t1, t1, t1);                       /* 4β */
    f_add(C, t2, t1, t1);                       /* 8β */
    f_sub(C, x3, x3, t2);
    f_add(C, z3, p->y, p->z);
    f_mul(C, z3, z3, z3);
    f_sub(C, z3, z3, gamma);
    f_sub(C, z3, z3, delta);
    f_sub(C, t1, t1, x3);
    f_mul(C, y3, alpha, t1);
    f_mul(C, t2, gamma, gamma);
    f_add(C, t2, t2, t2);
    f_add(C, t2, t2, t2);
    f_add(C, t2, t2, t2);                       /* 8γ² */
    f_sub(C, y3, y3, t2);
    memcpy(r->x, x3, sizeof(x3));
    memcpy(r->y, y3, sizeof(y3));
    memcpy(r->z, z3, sizeof(z3));
}

static void pt_add(const curve_t *C, jpt *r, const jpt *p, const jpt *q) {
    int n = C->n;
    if (bn_is_zero(p->z, n)) { *r = *q; return; }
    if (bn_is_zero(q->z, n)) { *r = *p; return; }
    u32 z1z1[EC_LIMBS], z2z2[EC_LIMBS], u1[EC_LIMBS], u2[EC_LIMBS], s1[EC_LIMBS], s2[EC_LIMBS];
    u32 h[EC_LIMBS], rr[EC_LIMBS], hh[EC_LIMBS], hhh[EC_LIMBS], v[EC_LIMBS], t[EC_LIMBS];
    u32 x3[EC_LIMBS], y3[EC_LIMBS], z3[EC_LIMBS];
    f_mul(C, z1z1, p->z, p->z);
    f_mul(C, z2z2, q->z, q->z);
    f_mul(C, u1, p->x, z2z2);
    f_mul(C, u2, q->x, z1z1);
    f_mul(C, s1, p->y, q->z);
    f_mul(C, s1, s1, z2z2);
    f_mul(C, s2, q->y, p->z);
    f_mul(C, s2, s2, z1z1);
    f_sub(C, h, u2, u1);
    f_sub(C, rr, s2, s1);
    if (bn_is_zero(h, n)) {
        if (bn_is_zero(rr, n)) { pt_dbl(C, r, p); return; }
        memset(r, 0, sizeof(*r));               /* P + (-P) = infinito */
        return;
    }
    f_mul(C, hh, h, h);
    f_mul(C, hhh, h, hh);
    f_mul(C, v, u1, hh);
    f_mul(C, x3, rr, rr);
    f_sub(C, x3, x3, hhh);
    f_sub(C, x3, x3, v);
    f_sub(C, x3, x3, v);
    f_sub(C, t, v, x3);
    f_mul(C, y3, rr, t);
    f_mul(C, t, s1, hhh);
    f_sub(C, y3, y3, t);
    f_mul(C, z3, p->z, q->z);
    f_mul(C, z3, z3, h);
    memcpy(r->x, x3, sizeof(x3));
    memcpy(r->y, y3, sizeof(y3));
    memcpy(r->z, z3, sizeof(z3));
}

static const curve_t *curve_get(int id) {
    curve_t *C = id == EC_P256 ? &g_p256 : id == EC_P384 ? &g_p384 : NULL;
    if (!C) return NULL;
    if (__atomic_load_n(&C->ready, __ATOMIC_ACQUIRE)) return C->ok ? C : NULL;
    /* inicialização idempotente: corridas apenas repetem o mesmo cálculo */
    curve_t *T = kmalloc(sizeof(curve_t));
    if (!T) return NULL;
    memset(T, 0, sizeof(*T));
    const char **hx = id == EC_P256 ? P256_HEX : P384_HEX;
    T->n = id == EC_P256 ? 8 : 12;
    T->bytes = (u32)T->n * 4;
    u32 p[EC_LIMBS], nn[EC_LIMBS], two[EC_LIMBS];
    hex_limbs(p, T->n, hx[0]);
    hex_limbs(nn, T->n, hx[1]);
    hex_limbs(T->b, T->n, hx[2]);
    hex_limbs(T->gx, T->n, hx[3]);
    hex_limbs(T->gy, T->n, hx[4]);
    T->ok = mont_init(&T->P, p, T->n) && mont_init(&T->N, nn, T->n);
    if (T->ok) {
        mont_to(&T->P, T->b, T->b);
        mont_to(&T->P, T->gx, T->gx);
        mont_to(&T->P, T->gy, T->gy);
        bn_set_word(two, T->n, 2);
        bn_sub(T->p_2, p, two, T->n);
        bn_sub(T->n_2, nn, two, T->n);
        T->ok = on_curve(T, T->gx, T->gy);
        if (!T->ok) kprintf("[pk] ERRO: gerador da curva %d fora da curva (constantes corrompidas)\n", id);
    }
    T->ready = false;
    if (!__atomic_load_n(&C->ready, __ATOMIC_ACQUIRE)) {
        memcpy(C, T, sizeof(*T));
        __atomic_store_n(&C->ready, true, __ATOMIC_RELEASE);
    }
    kfree(T);
    return C->ok ? C : NULL;
}

/* x afim (forma normal) de um ponto jacobiano não-infinito */
static void pt_affine_x(const curve_t *C, u32 *x, const jpt *p) {
    u32 zi[EC_LIMBS];
    mont_pow(&C->P, zi, p->z, C->p_2, C->n);
    f_mul(C, zi, zi, zi);
    f_mul(C, x, p->x, zi);
    mont_from(&C->P, x, x);
}

/* r = u1·G + u2·Q (truque de Shamir) */
static void ec_double_mul(const curve_t *C, jpt *r, const u32 *u1, const jpt *G, const u32 *u2, const jpt *Q) {
    jpt gq, acc;
    pt_add(C, &gq, G, Q);
    memset(&acc, 0, sizeof(acc));
    int bits = bn_bits(u1, C->n);
    int b2 = bn_bits(u2, C->n);
    if (b2 > bits) bits = b2;
    for (int i = bits - 1; i >= 0; i--) {
        pt_dbl(C, &acc, &acc);
        int a = bn_bit(u1, i), b = bn_bit(u2, i);
        if (a && b) pt_add(C, &acc, &acc, &gq);
        else if (a) pt_add(C, &acc, &acc, G);
        else if (b) pt_add(C, &acc, &acc, Q);
    }
    *r = acc;
}

int ecdsa_verify(int curve, const u8 *pub, u32 publen, const u8 *digest, u32 dlen,
                 const u8 *rb, u32 rlen, const u8 *sb, u32 slen) {
    const curve_t *C = curve_get(curve);
    if (!C) return -1;
    int n = C->n;
    u32 B = C->bytes;
    if (publen != 1 + 2 * B || pub[0] != 0x04) return -1;
    jpt G, Q, R;
    if (!bn_from_be(Q.x, n, pub + 1, B) || !bn_from_be(Q.y, n, pub + 1 + B, B)) return -1;
    if (bn_cmp(Q.x, C->P.m, n) >= 0 || bn_cmp(Q.y, C->P.m, n) >= 0) return -1;
    mont_to(&C->P, Q.x, Q.x);
    mont_to(&C->P, Q.y, Q.y);
    if (!on_curve(C, Q.x, Q.y)) return -1;
    memcpy(Q.z, C->P.one, (usize)n * 4);
    memcpy(G.x, C->gx, (usize)n * 4);
    memcpy(G.y, C->gy, (usize)n * 4);
    memcpy(G.z, C->P.one, (usize)n * 4);

    u32 r[EC_LIMBS], s[EC_LIMBS], e[EC_LIMBS], w[EC_LIMBS], u1[EC_LIMBS], u2[EC_LIMBS], x[EC_LIMBS];
    if (!bn_from_be(r, n, rb, rlen) || !bn_from_be(s, n, sb, slen)) return -1;
    if (bn_is_zero(r, n) || bn_is_zero(s, n) || bn_cmp(r, C->N.m, n) >= 0 || bn_cmp(s, C->N.m, n) >= 0) return -1;
    /* e = bits mais à esquerda do hash (as ordens de P-256/P-384 têm 256/384 bits exatos) */
    bn_from_be(e, n, digest, dlen > B ? B : dlen);
    if (bn_cmp(e, C->N.m, n) >= 0) bn_sub(e, e, C->N.m, n);
    /* w = s^-1 mod n (Fermat), u1 = e·w, u2 = r·w */
    mont_to(&C->N, w, s);
    mont_pow(&C->N, w, w, C->n_2, n);            /* w = s^-1 · R */
    mont_mul(&C->N, u1, e, w);                   /* e · s^-1 (forma normal) */
    mont_mul(&C->N, u2, r, w);
    ec_double_mul(C, &R, u1, &G, u2, &Q);
    if (bn_is_zero(R.z, n)) return -1;
    pt_affine_x(C, x, &R);
    if (bn_cmp(x, C->N.m, n) >= 0) bn_sub(x, x, C->N.m, n);
    return bn_cmp(x, r, n) == 0 ? 0 : -1;
}

int ec_selftest(int curve) {
    const curve_t *C = curve_get(curve);
    if (!C) return -1;
    jpt G, R, Z;
    memcpy(G.x, C->gx, sizeof(G.x));
    memcpy(G.y, C->gy, sizeof(G.y));
    memcpy(G.z, C->P.one, sizeof(G.z));
    memset(&Z, 0, sizeof(Z));
    u32 zero[EC_LIMBS] = {0};
    ec_double_mul(C, &R, C->N.m, &G, zero, &Z);   /* n·G deve ser o ponto no infinito */
    return bn_is_zero(R.z, C->n) ? 0 : -1;
}

/* ======================================================================
 * ECDH sobre P-256 / P-384 (secp256r1/secp384r1: ECDHE do TLS 1.2 e key_share do TLS 1.3)
 *
 * Aqui o escalar é SECRETO. A multiplicação usa uma escada de Montgomery com
 * trocas condicionais por máscara (mesma sequência de operações para qualquer
 * escalar), mas as primitivas de corpo acima (f_add/f_sub, subtração final de
 * mont_mul, casos especiais de pt_add) ainda têm desvios dependentes de dados:
 * não é rigorosamente tempo constante. Suficiente para chaves efêmeras de uso único.
 * ====================================================================== */
static void pt_cswap(jpt *a, jpt *b, u32 bit, int n) {
    u32 m = 0u - bit;
    for (int i = 0; i < n; i++) {
        u32 t;
        t = m & (a->x[i] ^ b->x[i]); a->x[i] ^= t; b->x[i] ^= t;
        t = m & (a->y[i] ^ b->y[i]); a->y[i] ^= t; b->y[i] ^= t;
        t = m & (a->z[i] ^ b->z[i]); a->z[i] ^= t; b->z[i] ^= t;
    }
}

/* r = k·P (k com C->n limbs) */
static void ec_ladder(const curve_t *C, jpt *r, const u32 *k, const jpt *P) {
    jpt R0, R1;
    memset(&R0, 0, sizeof(R0));                  /* infinito */
    R1 = *P;
    u32 swap = 0;
    for (int i = C->n * 32 - 1; i >= 0; i--) {
        u32 b = (u32)bn_bit(k, i);
        swap ^= b;
        pt_cswap(&R0, &R1, swap, C->n);
        swap = b;
        pt_add(C, &R1, &R0, &R1);
        pt_dbl(C, &R0, &R0);
    }
    pt_cswap(&R0, &R1, swap, C->n);
    *r = R0;
    secure_zero(&R0, sizeof(R0));
    secure_zero(&R1, sizeof(R1));
}

/* coordenadas afins (forma normal) de um ponto jacobiano não-infinito */
static void pt_affine(const curve_t *C, u32 *x, u32 *y, const jpt *p) {
    u32 zi[EC_LIMBS], z2[EC_LIMBS], z3[EC_LIMBS];
    mont_pow(&C->P, zi, p->z, C->p_2, C->n);
    f_mul(C, z2, zi, zi);
    f_mul(C, z3, z2, zi);
    f_mul(C, x, p->x, z2);
    f_mul(C, y, p->y, z3);
    mont_from(&C->P, x, x);
    mont_from(&C->P, y, y);
}

/* chave efêmera: priv = escalar big-endian (B bytes), pub = 04 || X || Y (1 + 2B bytes) */
int ecdh_keygen(int curve, u8 *priv, u8 *pub) {
    const curve_t *C = curve_get(curve);
    if (!C) return -1;
    int n = C->n;
    u32 B = C->bytes;
    u32 d[EC_LIMBS];
    for (int tries = 0;; tries++) {              /* 1 <= d < n por rejeição */
        if (tries > 64) return -1;
        crypto_random(priv, B);
        bn_from_be(d, n, priv, B);
        if (!bn_is_zero(d, n) && bn_cmp(d, C->N.m, n) < 0) break;
    }
    jpt G, R;
    memcpy(G.x, C->gx, sizeof(G.x));
    memcpy(G.y, C->gy, sizeof(G.y));
    memcpy(G.z, C->P.one, sizeof(G.z));
    ec_ladder(C, &R, d, &G);
    secure_zero(d, sizeof(d));
    if (bn_is_zero(R.z, n)) return -1;
    u32 x[EC_LIMBS], y[EC_LIMBS];
    pt_affine(C, x, y, &R);
    pub[0] = 0x04;
    bn_to_be(pub + 1, B, x, n);
    bn_to_be(pub + 1 + B, B, y, n);
    return 0;
}

/* segredo compartilhado = X de priv·peer (B bytes); valida o ponto do par */
int ecdh_shared(int curve, u8 *shared, const u8 *priv, const u8 *peer, u32 peerlen) {
    const curve_t *C = curve_get(curve);
    if (!C) return -1;
    int n = C->n;
    u32 B = C->bytes;
    if (peerlen != 1 + 2 * B || peer[0] != 0x04) return -1;
    jpt Q, R;
    if (!bn_from_be(Q.x, n, peer + 1, B) || !bn_from_be(Q.y, n, peer + 1 + B, B)) return -1;
    if (bn_cmp(Q.x, C->P.m, n) >= 0 || bn_cmp(Q.y, C->P.m, n) >= 0) return -1;
    mont_to(&C->P, Q.x, Q.x);
    mont_to(&C->P, Q.y, Q.y);
    if (!on_curve(C, Q.x, Q.y)) return -1;      /* rejeita pontos inválidos (ataque de curva inválida) */
    memcpy(Q.z, C->P.one, (usize)n * 4);
    u32 d[EC_LIMBS];
    bn_from_be(d, n, priv, B);
    if (bn_is_zero(d, n) || bn_cmp(d, C->N.m, n) >= 0) { secure_zero(d, sizeof(d)); return -1; }
    ec_ladder(C, &R, d, &Q);
    secure_zero(d, sizeof(d));
    if (bn_is_zero(R.z, n)) return -1;
    u32 x[EC_LIMBS], y[EC_LIMBS];
    pt_affine(C, x, y, &R);
    bn_to_be(shared, B, x, n);
    secure_zero(&R, sizeof(R));
    secure_zero(x, sizeof(x));
    return 0;
}

int ecdh_p256_keygen(u8 priv[32], u8 pub[65]) { return ecdh_keygen(EC_P256, priv, pub); }
int ecdh_p256(u8 shared[32], const u8 priv[32], const u8 *peer, u32 peerlen) {
    return ecdh_shared(EC_P256, shared, priv, peer, peerlen);
}
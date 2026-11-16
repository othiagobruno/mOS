/* mOS — RSA do crypto.subtle (motor web): números grandes em limbs de 32 bits, multiplicação de Montgomery,
 * Miller–Rabin, geração de chaves (p, q, d, dp, dq, qi) e as operações pública e privada (privada pelo CRT, com
 * conferência pela pública para não vazar falha). Até 4096 bits de módulo; expoente público de até 32 bits. */
#include "web.h"
#include "crypto.h"
#include "web_rsa.h"

#define ML 132   /* limbs: 4096 bits + folga */

typedef struct { u32 v[ML]; int n; } bn_t;   /* n = limbs usados (os demais são zero) */

static void bz(bn_t *a, int n) { memset(a->v, 0, sizeof a->v); a->n = n; }
static void bfrom(bn_t *a, int n, const u8 *b, u32 len) {
    bz(a, n);
    for (u32 i = 0; i < len; i++) {
        u32 bit = (len - 1 - i) * 8;
        if (bit / 32 < (u32)ML) a->v[bit / 32] |= (u32)b[i] << (bit % 32);
    }
}
static void bto(const bn_t *a, u8 *out, u32 len) {
    for (u32 i = 0; i < len; i++) {
        u32 bit = (len - 1 - i) * 8;
        out[i] = bit / 32 < (u32)ML ? (u8)(a->v[bit / 32] >> (bit % 32)) : 0;
    }
}
static int bcmp_(const bn_t *a, const bn_t *b, int n) {
    for (int i = n - 1; i >= 0; i--) if (a->v[i] != b->v[i]) return a->v[i] > b->v[i] ? 1 : -1;
    return 0;
}
static u32 bsub(bn_t *r, const bn_t *a, const bn_t *b, int n) {
    u64 br = 0;
    for (int i = 0; i < n; i++) { u64 d = (u64)a->v[i] - b->v[i] - br; r->v[i] = (u32)d; br = (d >> 32) & 1; }
    return (u32)br;
}
static int bbits(const bn_t *a, int n) {
    for (int i = n - 1; i >= 0; i--) if (a->v[i]) { int b = 32; while (!(a->v[i] >> (b - 1))) b--; return i * 32 + b; }
    return 0;
}
static u32 bmod_small(const bn_t *a, int n, u32 m) {
    u64 r = 0;
    for (int i = n - 1; i >= 0; i--) r = ((r << 32) | a->v[i]) % m;
    return (u32)r;
}

/* ---------------- Montgomery */
typedef struct { bn_t m, rr; u32 minv; int n; } mt_t;
static void mt_mul(const mt_t *M, bn_t *r, const bn_t *a, const bn_t *b) {
    int n = M->n;
    u32 t[2 * ML + 2];
    memset(t, 0, sizeof(u32) * (usize)(2 * n + 2));
    for (int i = 0; i < n; i++) {
        u64 c = 0;
        for (int j = 0; j < n; j++) { c += (u64)a->v[i] * b->v[j] + t[i + j]; t[i + j] = (u32)c; c >>= 32; }
        for (int k = i + n; c; k++) { c += t[k]; t[k] = (u32)c; c >>= 32; }
        u32 q = t[i] * M->minv;
        c = 0;
        for (int j = 0; j < n; j++) { c += (u64)q * M->m.v[j] + t[i + j]; t[i + j] = (u32)c; c >>= 32; }
        for (int k = i + n; c; k++) { c += t[k]; t[k] = (u32)c; c >>= 32; }
    }
    bn_t o;
    bz(&o, n);
    for (int i = 0; i < n; i++) o.v[i] = t[n + i];
    if (t[2 * n] || bcmp_(&o, &M->m, n) >= 0) bsub(&o, &o, &M->m, n);
    *r = o;
}
static bool mt_init(mt_t *M, const bn_t *m, int n) {
    if (!(m->v[0] & 1)) return false;
    M->m = *m;
    M->n = n;
    u32 x = 1;   /* −m⁻¹ mod 2³² por Newton */
    for (int i = 0; i < 5; i++) x *= 2 - m->v[0] * x;
    M->minv = (u32)-x;
    /* R² mod m: 1 dobrado 2·32·n vezes com redução */
    bn_t r;
    bz(&r, n);
    r.v[0] = 1;
    for (int i = 0; i < 64 * n; i++) {
        u32 top = r.v[n - 1] >> 31;
        for (int j = n - 1; j > 0; j--) r.v[j] = r.v[j] << 1 | r.v[j - 1] >> 31;
        r.v[0] <<= 1;
        if (top || bcmp_(&r, m, n) >= 0) bsub(&r, &r, m, n);
    }
    M->rr = r;
    return true;
}
/* r = b^e mod m (b < m) */
static void mt_pow(const mt_t *M, bn_t *r, const bn_t *b, const bn_t *e, int en) {
    int n = M->n;
    bn_t x, one, acc;
    mt_mul(M, &x, b, &M->rr);
    bz(&one, n);
    one.v[0] = 1;
    mt_mul(M, &acc, &one, &M->rr);
    for (int i = bbits(e, en) - 1; i >= 0; i--) {
        mt_mul(M, &acc, &acc, &acc);
        if (e->v[i / 32] >> (i % 32) & 1) mt_mul(M, &acc, &acc, &x);
    }
    mt_mul(M, r, &acc, &one);
}

/* a mod m, a com an limbs, m com mn limbs (divisão longa bit a bit; só fora do caminho quente) */
static void bmod(bn_t *r, const bn_t *a, int an, const bn_t *m, int mn) {
    bn_t rem;
    bz(&rem, mn + 1);
    for (int i = bbits(a, an) - 1; i >= 0; i--) {
        for (int j = mn; j > 0; j--) rem.v[j] = rem.v[j] << 1 | rem.v[j - 1] >> 31;
        rem.v[0] = rem.v[0] << 1 | (a->v[i / 32] >> (i % 32) & 1);
        bn_t mm = *m;
        mm.v[mn] = 0;
        if (bcmp_(&rem, &mm, mn + 1) >= 0) bsub(&rem, &rem, &mm, mn + 1);
    }
    bz(r, mn);
    for (int i = 0; i < mn; i++) r->v[i] = rem.v[i];
}
static void bmul(bn_t *r, const bn_t *a, int an, const bn_t *b, int bn_) {
    bn_t o;
    bz(&o, an + bn_);
    for (int i = 0; i < an; i++) {
        u64 c = 0;
        for (int j = 0; j < bn_; j++) { c += (u64)a->v[i] * b->v[j] + o.v[i + j]; o.v[i + j] = (u32)c; c >>= 32; }
        o.v[i + bn_] = (u32)c;
    }
    *r = o;
}

/* ---------------- primos */
static const u16 small_primes[] = { 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97,
    101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
    229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367,
    373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509,
    521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661,
    673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829,
    839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997 };

static bool miller_rabin(const bn_t *p, int n, int rounds) {
    mt_t M;
    if (!mt_init(&M, p, n)) return false;
    bn_t d, pm1, one;
    bz(&one, n);
    one.v[0] = 1;
    bsub(&pm1, p, &one, n);
    pm1.n = n;
    d = pm1;
    int s = 0;
    while (!(d.v[0] & 1)) {   /* d = (p−1)/2^s */
        for (int i = 0; i < n; i++) d.v[i] = d.v[i] >> 1 | (i + 1 < n ? d.v[i + 1] << 31 : 0);
        s++;
    }
    for (int r = 0; r < rounds; r++) {
        bn_t a, x;
        bz(&a, n);
        crypto_random(a.v, (usize)(n - 1) * 4);   /* um limb a menos que p: a < p */
        if (bbits(&a, n) < 2) a.v[0] = 2;
        mt_pow(&M, &x, &a, &d, n);
        if (!bcmp_(&x, &one, n) || !bcmp_(&x, &pm1, n)) continue;
        bool ok = false;
        bn_t xm, onem;
        mt_mul(&M, &xm, &x, &M.rr);   /* para o domínio de Montgomery */
        mt_mul(&M, &onem, &pm1, &M.rr);
        for (int i = 1; i < s && !ok; i++) {
            mt_mul(&M, &xm, &xm, &xm);
            if (!bcmp_(&xm, &onem, n)) ok = true;
        }
        if (!ok) return false;
    }
    return true;
}

/* primo de bits bits com os dois bits de cima ligados e (p−1) primo com e */
static void gen_prime(bn_t *p, int bits, u32 e) {
    int n = (bits + 31) / 32;
    for (;;) {
        bz(p, n);
        crypto_random(p->v, (usize)n * 4);
        int top = (bits - 1) % 32;
        if (top < 31) p->v[n - 1] &= (2u << top) - 1;
        p->v[n - 1] |= 1u << top;
        if (top) p->v[n - 1] |= 1u << (top - 1); else p->v[n - 2] |= 0x80000000u;
        p->v[0] |= 1;
        for (int tries = 0; tries < 4096; tries++) {
            bool comp = false;
            for (u32 i = 0; i < ARRAY_LEN(small_primes) && !comp; i++) if (!bmod_small(p, n, small_primes[i])) comp = true;
            if (!comp && e > 1 && bmod_small(p, n, e) == 1) comp = true;   /* gcd(e, p−1) ≠ 1 quando e é primo */
            if (!comp && miller_rabin(p, n, bits >= 1024 ? 5 : 10)) return;
            /* próximo ímpar */
            u64 c = 2;
            for (int i = 0; i < n && c; i++) { c += p->v[i]; p->v[i] = (u32)c; c >>= 32; }
            if (bbits(p, n) != bits) break;
        }
    }
}

/* inverso de x mod m (pequenos) por Euclides estendido; 0 se não existe */
static u64 inv_small(u64 x, u64 m) {
    i64 t = 0, nt = 1;
    i64 r = (i64)m, nr = (i64)(x % m);
    while (nr) { i64 q = r / nr, tmp = t - q * nt; t = nt; nt = tmp; tmp = r - q * nr; r = nr; nr = tmp; }
    if (r != 1) return 0;
    return (u64)(t < 0 ? t + (i64)m : t);
}

/* e⁻¹ mod m, com e pequeno: d = (k·m + 1)/e, k = −m⁻¹ mod e */
static bool inv_mod_by_small_e(bn_t *d, const bn_t *m, int n, u32 e) {
    u32 r = bmod_small(m, n, e);
    u64 ir = inv_small(r, e);
    if (!ir) return false;
    u64 k = (e - ir) % e;
    bn_t t;
    bz(&t, n + 1);
    u64 c = 1;
    for (int i = 0; i < n; i++) { c += (u64)m->v[i] * k; t.v[i] = (u32)c; c >>= 32; }
    t.v[n] = (u32)c;
    u64 rem = 0;   /* divide por e */
    for (int i = n; i >= 0; i--) { u64 cur = rem << 32 | t.v[i]; t.v[i] = (u32)(cur / e); rem = cur % e; }
    if (rem) return false;
    bz(d, n);
    for (int i = 0; i < n; i++) d->v[i] = t.v[i];
    return true;
}

static u8 *dup_bn(const bn_t *a, int n, u32 *len) {
    int bytes = (bbits(a, n) + 7) / 8;
    if (!bytes) bytes = 1;
    u8 *o = kmalloc((usize)bytes);
    if (o) bto(a, o, (u32)bytes);
    *len = (u32)bytes;
    return o;
}

void wrsa_free(wrsa_key_t *k) {
    u8 **f[] = { &k->n, &k->e, &k->d, &k->p, &k->q, &k->dp, &k->dq, &k->qi };
    for (u32 i = 0; i < ARRAY_LEN(f); i++) { if (*f[i]) kfree(*f[i]); *f[i] = NULL; }
}

bool wrsa_generate(u32 bits, const u8 *e8, u32 el, wrsa_key_t *k) {
    memset(k, 0, sizeof *k);
    if (bits < 512 || bits > 4096 || bits % 16 || !el || el > 4) return false;
    u32 e = 0;
    for (u32 i = 0; i < el; i++) e = e << 8 | e8[i];
    if (e < 3 || !(e & 1)) return false;
    int half = (int)bits / 2, hn = (half + 31) / 32, nn = ((int)bits + 31) / 32;
    bn_t p, q, n, phi, d, one, pm1, qm1, dp, dq, qi;
    for (;;) {
        gen_prime(&p, half, e);
        gen_prime(&q, half, e);
        if (!bcmp_(&p, &q, hn)) continue;
        if (bcmp_(&p, &q, hn) < 0) { bn_t t = p; p = q; q = t; }   /* p > q (qi = q⁻¹ mod p) */
        bmul(&n, &p, hn, &q, hn);
        if (bbits(&n, nn) != (int)bits) continue;
        break;
    }
    bz(&one, hn);
    one.v[0] = 1;
    bsub(&pm1, &p, &one, hn);
    bsub(&qm1, &q, &one, hn);
    bmul(&phi, &pm1, hn, &qm1, hn);
    if (!inv_mod_by_small_e(&d, &phi, nn, e) || !inv_mod_by_small_e(&dp, &pm1, hn, e) || !inv_mod_by_small_e(&dq, &qm1, hn, e)) return false;
    /* qi = q^(p−2) mod p */
    mt_t Mp;
    if (!mt_init(&Mp, &p, hn)) return false;
    bn_t pm2, two;
    bz(&two, hn);
    two.v[0] = 2;
    bsub(&pm2, &p, &two, hn);
    mt_pow(&Mp, &qi, &q, &pm2, hn);
    k->nl = bits / 8;
    k->n = kmalloc(k->nl);
    if (k->n) bto(&n, k->n, k->nl);
    k->e = kmalloc(el);
    if (k->e) memcpy(k->e, e8, el);
    k->el = el;
    k->d = dup_bn(&d, nn, &k->dl);
    k->p = dup_bn(&p, hn, &k->pl);
    k->q = dup_bn(&q, hn, &k->ql);
    k->dp = dup_bn(&dp, hn, &k->dpl);
    k->dq = dup_bn(&dq, hn, &k->dql);
    k->qi = dup_bn(&qi, hn, &k->qil);
    secure_zero(&p, sizeof p); secure_zero(&q, sizeof q); secure_zero(&d, sizeof d);
    secure_zero(&dp, sizeof dp); secure_zero(&dq, sizeof dq); secure_zero(&phi, sizeof phi);
    if (!k->n || !k->e || !k->d || !k->p || !k->q || !k->dp || !k->dq || !k->qi) { wrsa_free(k); return false; }
    return true;
}

/* m^e mod n; in < n, out com nl bytes */
bool wrsa_public(const u8 *n8, u32 nl, const u8 *e8, u32 el, const u8 *in, u32 inl, u8 *out) {
    if (nl > 512 || inl > nl) return false;
    int n = (int)(nl + 3) / 4;
    bn_t N, X, E, R;
    bfrom(&N, n, n8, nl);
    bfrom(&X, n, in, inl);
    bfrom(&E, (int)(el + 3) / 4, e8, el);
    if (bcmp_(&X, &N, n) >= 0) return false;
    mt_t M;
    if (!mt_init(&M, &N, n)) return false;
    mt_pow(&M, &R, &X, &E, (int)(el + 3) / 4);
    bto(&R, out, nl);
    return true;
}

/* c^d mod n pelo CRT (p, q, dp, dq, qi); confere pela pública */
bool wrsa_private(const wrsa_key_t *k, const u8 *in, u32 inl, u8 *out) {
    if (!k->p || !k->q || !k->dp || !k->dq || !k->qi || k->nl > 512 || inl > k->nl) return false;
    int n = (int)(k->nl + 3) / 4, hn = (int)(MAX(k->pl, k->ql) + 3) / 4;
    bn_t C, P, Q, DP, DQ, QI, cp, cq, m1, m2, h, t, M;
    bfrom(&C, n, in, inl);
    bfrom(&P, hn, k->p, k->pl);
    bfrom(&Q, hn, k->q, k->ql);
    bfrom(&DP, hn, k->dp, k->dpl);
    bfrom(&DQ, hn, k->dq, k->dql);
    bfrom(&QI, hn, k->qi, k->qil);
    mt_t Mp, Mq;
    if (!mt_init(&Mp, &P, hn) || !mt_init(&Mq, &Q, hn)) return false;
    bmod(&cp, &C, n, &P, hn);
    bmod(&cq, &C, n, &Q, hn);
    mt_pow(&Mp, &m1, &cp, &DP, hn);
    mt_pow(&Mq, &m2, &cq, &DQ, hn);
    /* h = qi·(m1 − m2) mod p */
    bn_t m2p;
    bmod(&m2p, &m2, hn, &P, hn);
    if (bcmp_(&m1, &m2p, hn) >= 0) bsub(&t, &m1, &m2p, hn);
    else { bsub(&t, &P, &m2p, hn); u64 c = 0; for (int i = 0; i < hn; i++) { c += (u64)t.v[i] + m1.v[i]; t.v[i] = (u32)c; c >>= 32; } }
    t.n = hn;
    bn_t tm, qim;
    mt_mul(&Mp, &tm, &t, &Mp.rr);
    mt_mul(&Mp, &qim, &QI, &Mp.rr);
    mt_mul(&Mp, &h, &tm, &qim);
    bn_t one;
    bz(&one, hn);
    one.v[0] = 1;
    mt_mul(&Mp, &h, &h, &one);
    /* m = m2 + h·q */
    bmul(&M, &h, hn, &Q, hn);
    u64 c = 0;
    for (int i = 0; i < n; i++) { c += (u64)M.v[i] + (i < hn ? m2.v[i] : 0); M.v[i] = (u32)c; c >>= 32; }
    bto(&M, out, k->nl);
    secure_zero(&m1, sizeof m1); secure_zero(&m2, sizeof m2); secure_zero(&DP, sizeof DP); secure_zero(&DQ, sizeof DQ);
    /* conferência: out^e mod n tem que voltar à entrada (falha de CRT vaza p) */
    u8 *chk = kmalloc(k->nl), *inp = kmalloc(k->nl);
    if (!chk || !inp) { kfree(chk); kfree(inp); return false; }
    memset(inp, 0, k->nl);
    memcpy(inp + k->nl - inl, in, inl);
    bool ok = wrsa_public(k->n, k->nl, k->e, k->el, out, k->nl, chk) && !memcmp(chk, inp, k->nl);
    kfree(chk);
    kfree(inp);
    if (!ok) memset(out, 0, k->nl);
    return ok;
}

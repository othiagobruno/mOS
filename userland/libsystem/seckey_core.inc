/* mOS libSystem — núcleo do SecKey (Security.framework): RSA e curvas elípticas, sem CF nem Objective-C.
 *
 * Incluído por seckey.c (libSystem) e por tools/seckeytest.c (host). Quem inclui fornece u8/u32/u64/usize,
 * malloc/free/memcpy/memset/memcmp e os cabeçalhos crypto.h e x509.h do kernel (SHA-2, crypto_random,
 * ecdsa_sign/verify, ecdh_*, ec_public_key em kernel/net/pk.c).
 *
 *  - RSA: aritmética multiprecisão própria (limbs de 32 bits, até 8192 bits de módulo), Montgomery com janela
 *    de 4 bits, divisão de Knuth (algoritmo D), geração de chaves (Miller-Rabin, e = 65537, p > q, d mod λ),
 *    operação privada por CRT com conferência pela pública.
 *  - Paddings: PKCS#1 v1.5 (assinatura com e sem DigestInfo, cifra tipo 2), PSS (sal = tamanho do hash, como o
 *    macOS; na verificação qualquer sal), OAEP (MGF1 com o mesmo hash, rótulo vazio) e raw.
 *  - Formatos externos do macOS: RSA = RSAPublicKey/RSAPrivateKey (PKCS#1, DER); EC = X9.63 (04||X||Y e
 *    04||X||Y||K). Na importação também se aceitam SubjectPublicKeyInfo, PKCS#8 e ECPrivateKey (SEC1).
 *  - ECDSA: assinatura X9.62 (DER) e RFC 4754 (r||s), ECDH padrão/cofator (cofator 1 nas curvas NIST) com
 *    ou sem a KDF X9.63.
 * Todas as funções devolvem 0 em sucesso, salvo quando o nome diz outra coisa. */

/* ================================================================ hashes */
enum { SKH_NONE = 0, SKH_SHA1, SKH_SHA224, SKH_SHA256, SKH_SHA384, SKH_SHA512, SKH_MD5 };
static u32 sk_hash_len(int h) {
    switch (h) {
    case SKH_SHA1: return 20;
    case SKH_SHA224: return 28;
    case SKH_SHA256: return 32;
    case SKH_SHA384: return 48;
    case SKH_SHA512: return 64;
    case SKH_MD5: return 16;
    default: return 0;
    }
}
/* MD5 só aparece como DigestInfo de um digest que o app já calculou (kSecPaddingPKCS1MD5) */
static int sk_hash(int h, const u8 *d, usize n, u8 *out) {
    switch (h) {
    case SKH_SHA1: sha1(d, n, out); return 0;
    case SKH_SHA256: sha256(d, n, out); return 0;
    case SKH_SHA224: {
        static const u32 iv[8] = {0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
                                  0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4};
        sha256_ctx c;
        u8 full[32];
        sha256_init(&c);
        memcpy(c.h, iv, sizeof iv);
        sha256_update(&c, d, n);
        sha256_final(&c, full);
        memcpy(out, full, 28);
        return 0;
    }
    case SKH_SHA384:
    case SKH_SHA512: {
        sha512_ctx c;
        if (h == SKH_SHA384) sha384_init(&c); else sha512_init(&c);
        sha512_update(&c, d, n);
        sha512_final(&c, out);
        return 0;
    }
    default: return -1;
    }
}

/* ================================================================ inteiros grandes */
#define RB_L 520                              /* 8192 bits de módulo: produtos de até 512 limbs + folga */
typedef struct { int n; u32 d[RB_L]; } rb_t; /* little-endian; n = limbs usados, d[n-1] != 0 (ou n = 0) */

static void rb_norm(rb_t *a) { while (a->n > 0 && !a->d[a->n - 1]) a->n--; }
static void rb_set(rb_t *a, u32 w) { a->n = w ? 1 : 0; a->d[0] = w; }
static void rb_copy(rb_t *r, const rb_t *a) { r->n = a->n; memcpy(r->d, a->d, (usize)a->n * 4); }
static int rb_is_zero(const rb_t *a) { return a->n == 0; }
static int rb_from_be(rb_t *r, const u8 *b, usize len) {
    while (len && !*b) { b++; len--; }
    if (len > (usize)RB_L * 4) return -1;
    r->n = (int)((len + 3) / 4);
    memset(r->d, 0, (usize)r->n * 4);
    for (usize i = 0; i < len; i++) r->d[i / 4] |= (u32)b[len - 1 - i] << (8 * (i % 4));
    rb_norm(r);
    return 0;
}
static usize rb_bytes(const rb_t *a);
static int rb_to_be(const rb_t *a, u8 *out, usize len) {
    if (rb_bytes(a) > len) return -1;
    for (usize i = 0; i < len; i++) {
        usize limb = i / 4;
        out[len - 1 - i] = limb < (usize)a->n ? (u8)(a->d[limb] >> (8 * (i % 4))) : 0;
    }
    return 0;
}
static u32 rb_bits(const rb_t *a) {
    if (!a->n) return 0;
    u32 top = a->d[a->n - 1], b = 0;
    while (top) { b++; top >>= 1; }
    return (u32)(a->n - 1) * 32 + b;
}
static usize rb_bytes(const rb_t *a) { return (rb_bits(a) + 7) / 8; }
static int rb_bit(const rb_t *a, u32 i) { return (int)(i / 32) < a->n ? (int)((a->d[i / 32] >> (i % 32)) & 1) : 0; }
static int rb_cmp(const rb_t *a, const rb_t *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}
static void rb_add(rb_t *r, const rb_t *a, const rb_t *b) {
    int n = a->n > b->n ? a->n : b->n;
    u64 c = 0;
    for (int i = 0; i < n; i++) {
        c += (u64)(i < a->n ? a->d[i] : 0) + (i < b->n ? b->d[i] : 0);
        r->d[i] = (u32)c;
        c >>= 32;
    }
    r->n = n;
    if (c && n < RB_L) r->d[r->n++] = (u32)c;
    rb_norm(r);
}
/* r = a - b, a >= b */
static void rb_sub(rb_t *r, const rb_t *a, const rb_t *b) {
    i64 br = 0;
    for (int i = 0; i < a->n; i++) {
        i64 t = (i64)a->d[i] - (i < b->n ? b->d[i] : 0) - br;
        br = t < 0;
        r->d[i] = (u32)t;
    }
    r->n = a->n;
    rb_norm(r);
}
static void rb_add_w(rb_t *r, const rb_t *a, u32 w) { rb_t t; rb_set(&t, w); rb_add(r, a, &t); }
static void rb_sub_w(rb_t *r, const rb_t *a, u32 w) { rb_t t; rb_set(&t, w); rb_sub(r, a, &t); }
/* r = a * b (r pode ser a ou b) */
static int rb_mul(rb_t *r, const rb_t *a, const rb_t *b) {
    if (a->n + b->n > RB_L) return -1;
    u32 t[RB_L];
    int n = a->n + b->n;
    memset(t, 0, (usize)n * 4);
    for (int i = 0; i < a->n; i++) {
        u64 c = 0;
        for (int j = 0; j < b->n; j++) {
            c += (u64)a->d[i] * b->d[j] + t[i + j];
            t[i + j] = (u32)c;
            c >>= 32;
        }
        t[i + b->n] = (u32)c;
    }
    memcpy(r->d, t, (usize)n * 4);
    r->n = n;
    rb_norm(r);
    return 0;
}
static void rb_mul_w(rb_t *r, const rb_t *a, u32 w) {
    u64 c = 0;
    for (int i = 0; i < a->n; i++) { c += (u64)a->d[i] * w; r->d[i] = (u32)c; c >>= 32; }
    r->n = a->n;
    if (c && r->n < RB_L) r->d[r->n++] = (u32)c;
    rb_norm(r);
}
/* q = a / w (q pode ser NULL); devolve o resto */
static u32 rb_div_w(rb_t *q, const rb_t *a, u32 w) {
    u64 rem = 0;
    for (int i = a->n - 1; i >= 0; i--) {
        u64 cur = rem << 32 | a->d[i];
        if (q) q->d[i] = (u32)(cur / w);
        rem = cur % w;
    }
    if (q) { q->n = a->n; rb_norm(q); }
    return (u32)rem;
}
static int clz32(u32 x) { int n = 0; if (!x) return 32; while (!(x & 0x80000000u)) { x <<= 1; n++; } return n; }
/* Knuth, algoritmo D (como o divmnu do Hacker's Delight). q e/ou r podem ser NULL; b != 0. */
static int rb_divmod(rb_t *q, rb_t *r, const rb_t *a, const rb_t *b) {
    if (!b->n) return -1;
    if (rb_cmp(a, b) < 0) {
        if (r) rb_copy(r, a);
        if (q) q->n = 0;
        return 0;
    }
    if (b->n == 1) {
        rb_t qq;
        u32 rem = rb_div_w(&qq, a, b->d[0]);
        if (q) rb_copy(q, &qq);
        if (r) rb_set(r, rem);
        return 0;
    }
    int n = b->n, m = a->n - b->n, s = clz32(b->d[n - 1]);
    u32 un[RB_L + 2], vn[RB_L + 1], qd[RB_L];
    for (int i = n - 1; i > 0; i--) vn[i] = s ? (b->d[i] << s) | (b->d[i - 1] >> (32 - s)) : b->d[i];
    vn[0] = b->d[0] << s;
    un[a->n] = s ? a->d[a->n - 1] >> (32 - s) : 0;
    for (int i = a->n - 1; i > 0; i--) un[i] = s ? (a->d[i] << s) | (a->d[i - 1] >> (32 - s)) : a->d[i];
    un[0] = a->d[0] << s;
    for (int j = m; j >= 0; j--) {
        u64 num = (u64)un[j + n] << 32 | un[j + n - 1];
        u64 qhat = num / vn[n - 1], rhat = num % vn[n - 1];
        while (qhat >> 32 || qhat * vn[n - 2] > (rhat << 32 | un[j + n - 2])) {
            qhat--;
            rhat += vn[n - 1];
            if (rhat >> 32) break;
        }
        i64 k = 0, t;
        for (int i = 0; i < n; i++) {
            u64 p = qhat * vn[i];
            t = (i64)un[i + j] - k - (i64)(p & 0xffffffffu);
            un[i + j] = (u32)t;
            k = (i64)(p >> 32) - (t >> 32);
        }
        t = (i64)un[j + n] - k;
        un[j + n] = (u32)t;
        qd[j] = (u32)qhat;
        if (t < 0) {
            qd[j]--;
            u64 c = 0;
            for (int i = 0; i < n; i++) {
                c += (u64)un[i + j] + vn[i];
                un[i + j] = (u32)c;
                c >>= 32;
            }
            un[j + n] += (u32)c;
        }
    }
    if (q) { q->n = m + 1; memcpy(q->d, qd, (usize)(m + 1) * 4); rb_norm(q); }
    if (r) {
        r->n = n;
        for (int i = 0; i < n; i++) r->d[i] = s ? (un[i] >> s) | (un[i + 1] << (32 - s)) : un[i];
        rb_norm(r);
    }
    return 0;
}
static int rb_mod(rb_t *r, const rb_t *a, const rb_t *m) { return rb_divmod(0, r, a, m); }

/* ---- Montgomery (módulo ímpar) */
typedef struct { int n; u32 m[RB_L / 2]; u32 minv; u32 rr[RB_L / 2]; u32 one[RB_L / 2]; } rmont_t;
static void mm_mul(const rmont_t *M, u32 *r, const u32 *a, const u32 *b) {
    int n = M->n;
    u32 t[RB_L / 2 + 2];
    memset(t, 0, (usize)(n + 2) * 4);
    for (int i = 0; i < n; i++) {
        u64 c = 0, s;
        for (int j = 0; j < n; j++) { s = (u64)t[j] + (u64)a[j] * b[i] + c; t[j] = (u32)s; c = s >> 32; }
        s = (u64)t[n] + c; t[n] = (u32)s; t[n + 1] = (u32)(s >> 32);
        u32 mq = t[0] * M->minv;
        s = (u64)t[0] + (u64)mq * M->m[0]; c = s >> 32;
        for (int j = 1; j < n; j++) { s = (u64)t[j] + (u64)mq * M->m[j] + c; t[j - 1] = (u32)s; c = s >> 32; }
        s = (u64)t[n] + c; t[n - 1] = (u32)s; c = s >> 32;
        t[n] = t[n + 1] + (u32)c; t[n + 1] = 0;
    }
    int ge = t[n] != 0;
    if (!ge) {
        ge = 1;
        for (int i = n - 1; i >= 0; i--) if (t[i] != M->m[i]) { ge = t[i] > M->m[i]; break; }
    }
    if (ge) {
        i64 br = 0;
        for (int i = 0; i < n; i++) { i64 x = (i64)t[i] - M->m[i] - br; br = x < 0; t[i] = (u32)x; }
    }
    memcpy(r, t, (usize)n * 4);
}
static int mont_setup(rmont_t *M, const rb_t *m) {
    if (!m->n || !(m->d[0] & 1) || m->n > RB_L / 2) return -1;
    M->n = m->n;
    memcpy(M->m, m->d, (usize)m->n * 4);
    u32 x = m->d[0];
    for (int i = 0; i < 5; i++) x *= 2 - m->d[0] * x;
    M->minv = (u32)(0u - x);
    rb_t big, r;
    big.n = 2 * m->n + 1;
    memset(big.d, 0, (usize)big.n * 4);
    big.d[2 * m->n] = 1;                                    /* R² = 2^(64n) */
    rb_mod(&r, &big, m);
    memset(M->rr, 0, (usize)m->n * 4);
    memcpy(M->rr, r.d, (usize)r.n * 4);
    big.n = m->n + 1;
    memset(big.d, 0, (usize)big.n * 4);
    big.d[m->n] = 1;                                        /* R mod m = 1 em Montgomery */
    rb_mod(&r, &big, m);
    memset(M->one, 0, (usize)m->n * 4);
    memcpy(M->one, r.d, (usize)r.n * 4);
    return 0;
}
/* r = b^e mod m (m ímpar). Janela fixa de 4 bits: o mesmo número de multiplicações para todo expoente. */
static int rb_modexp(rb_t *r, const rb_t *b, const rb_t *e, const rb_t *m) {
    rmont_t *M = malloc(sizeof *M);
    u32 *tab = malloc((usize)16 * (RB_L / 2) * 4);
    if (!M || !tab || mont_setup(M, m)) { free(M); free(tab); return -1; }
    int n = M->n;
    rb_t bb;
    rb_mod(&bb, b, m);
    u32 x[RB_L / 2], acc[RB_L / 2];
    memset(x, 0, sizeof x);
    memcpy(x, bb.d, (usize)bb.n * 4);
    memcpy(tab, M->one, (usize)n * 4);
    mm_mul(M, tab + n, x, M->rr);                           /* b·R */
    for (int i = 2; i < 16; i++) mm_mul(M, tab + i * n, tab + (i - 1) * n, tab + n);
    memcpy(acc, M->one, (usize)n * 4);
    u32 bits = rb_bits(e);
    int top = (int)((bits + 3) / 4);
    for (int w = top - 1; w >= 0; w--) {
        for (int k = 0; k < 4; k++) mm_mul(M, acc, acc, acc);
        u32 v = (u32)(rb_bit(e, (u32)w * 4) | rb_bit(e, (u32)w * 4 + 1) << 1 | rb_bit(e, (u32)w * 4 + 2) << 2 |
                      rb_bit(e, (u32)w * 4 + 3) << 3);
        u32 sel[RB_L / 2];
        memset(sel, 0, (usize)n * 4);
        for (u32 i = 0; i < 16; i++) {                      /* leitura da tabela sem índice secreto */
            u32 mask = 0u - (u32)(i == v);
            for (int j = 0; j < n; j++) sel[j] |= tab[i * (u32)n + (u32)j] & mask;
        }
        mm_mul(M, acc, acc, sel);
    }
    u32 one[RB_L / 2];
    memset(one, 0, (usize)n * 4);
    one[0] = 1;
    mm_mul(M, acc, acc, one);
    r->n = n;
    memcpy(r->d, acc, (usize)n * 4);
    rb_norm(r);
    memset(tab, 0, (usize)16 * (RB_L / 2) * 4);
    free(tab);
    free(M);
    return 0;
}
static void rb_gcd(rb_t *g, const rb_t *a, const rb_t *b) {
    rb_t x, y, t;
    rb_copy(&x, a);
    rb_copy(&y, b);
    while (!rb_is_zero(&y)) { rb_mod(&t, &x, &y); rb_copy(&x, &y); rb_copy(&y, &t); }
    rb_copy(g, &x);
}
static void rb_random_bits(rb_t *r, u32 bits) {
    u32 nl = (bits + 31) / 32;
    crypto_random(r->d, (usize)nl * 4);
    if (bits % 32) r->d[nl - 1] &= (1u << (bits % 32)) - 1;
    r->n = (int)nl;
    rb_norm(r);
}

/* ---- primos */
static u16 sk_small_primes[310];
static int sk_nsmall;
static void sk_primes_init(void) {
    if (sk_nsmall) return;
    static u8 comp[2048];
    int k = 0;
    for (int i = 3; i < 2048 && k < 310; i += 2) {
        if (comp[i]) continue;
        sk_small_primes[k++] = (u16)i;
        for (int j = i * i; j < 2048; j += 2 * i) comp[j] = 1;
    }
    sk_nsmall = k;
}
static int rb_miller_rabin(const rb_t *n, int rounds) {
    rb_t nm1, d, a, x, t, two;
    rb_sub_w(&nm1, n, 1);
    rb_copy(&d, &nm1);
    u32 s = 0;
    while (!rb_bit(&d, 0)) { rb_div_w(&d, &d, 2); s++; }
    rb_set(&two, 2);
    u32 nb = rb_bits(n);
    for (int r = 0; r < rounds; r++) {
        do rb_random_bits(&a, nb - 1); while (rb_cmp(&a, &two) < 0);
        if (rb_modexp(&x, &a, &d, n)) return 0;
        if ((x.n == 1 && x.d[0] == 1) || !rb_cmp(&x, &nm1)) continue;
        int ok = 0;
        for (u32 i = 1; i < s; i++) {
            rb_mul(&t, &x, &x);
            rb_mod(&x, &t, n);
            if (!rb_cmp(&x, &nm1)) { ok = 1; break; }
            if (x.n == 1 && x.d[0] == 1) break;
        }
        if (!ok) return 0;
    }
    return 1;
}
/* primo de `bits` bits com os dois bits do topo ligados (p·q tem exatamente 2·bits) e mdc(p-1, e) = 1 */
static int rb_gen_prime(rb_t *p, u32 bits, u32 e) {
    sk_primes_init();
    int rounds = bits >= 1024 ? 5 : bits >= 512 ? 8 : 16;
    u32 rem[310];
    for (int tries = 0; tries < 4096; tries++) {
        rb_random_bits(p, bits);
        if (rb_bits(p) < bits) { if ((int)((bits - 1) / 32) >= p->n) p->n = (int)((bits - 1) / 32) + 1; }
        p->d[(bits - 1) / 32] |= 1u << ((bits - 1) % 32);
        p->d[(bits - 2) / 32] |= 1u << ((bits - 2) % 32);
        p->d[0] |= 1;
        rb_norm(p);
        for (int i = 0; i < sk_nsmall; i++) rem[i] = rb_div_w(0, p, sk_small_primes[i]);
        for (u32 delta = 0; delta < 1u << 16; delta += 2) {
            int bad = 0;
            for (int i = 0; i < sk_nsmall && !bad; i++) if ((rem[i] + delta) % sk_small_primes[i] == 0) bad = 1;
            if (bad) continue;
            rb_t c;
            rb_add_w(&c, p, delta);
            if (rb_bits(&c) != bits) break;
            if (rb_div_w(0, &c, e) == 1) continue;          /* e | p-1 */
            if (rb_miller_rabin(&c, rounds)) { rb_copy(p, &c); return 0; }
        }
    }
    return -1;
}

/* ================================================================ DER mínimo */
typedef struct { u8 *b; usize n, cap; int err; } sk_buf;
static void sb_put(sk_buf *s, const void *d, usize n) {
    if (s->err) return;
    if (s->n + n > s->cap) {
        usize nc = s->cap ? s->cap * 2 : 256;
        while (nc < s->n + n) nc *= 2;
        u8 *nb = malloc(nc);
        if (!nb) { s->err = 1; return; }
        if (s->b) { memcpy(nb, s->b, s->n); free(s->b); }
        s->b = nb;
        s->cap = nc;
    }
    memcpy(s->b + s->n, d, n);
    s->n += n;
}
static void sb_byte(sk_buf *s, u8 c) { sb_put(s, &c, 1); }
static void der_len(sk_buf *s, usize n) {
    if (n < 128) { sb_byte(s, (u8)n); return; }
    u8 t[8];
    int k = 0;
    while (n) { t[k++] = (u8)n; n >>= 8; }
    sb_byte(s, (u8)(0x80 | k));
    while (k) sb_byte(s, t[--k]);
}
static void der_tlv(sk_buf *s, u8 tag, const u8 *v, usize n) { sb_byte(s, tag); der_len(s, n); sb_put(s, v, n); }
/* INTEGER sem sinal a partir de bytes big-endian */
static void der_uint(sk_buf *s, const u8 *b, usize n) {
    while (n > 1 && !*b) { b++; n--; }
    if (!n) { static const u8 z = 0; b = &z; n = 1; }
    int pad = b[0] & 0x80 ? 1 : 0;
    sb_byte(s, 0x02);
    der_len(s, n + (usize)pad);
    if (pad) sb_byte(s, 0);
    sb_put(s, b, n);
}
static void der_rb(sk_buf *s, const rb_t *a) {
    u8 tmp[RB_L * 4];
    usize n = rb_bytes(a);
    if (!n) n = 1;
    rb_to_be(a, tmp, n);
    der_uint(s, tmp, n);
}
static void der_wrap(sk_buf *out, u8 tag, sk_buf *in) { der_tlv(out, tag, in->b, in->n); if (in->err) out->err = 1; free(in->b); }

typedef struct { const u8 *p, *end; } der_rd;
/* lê um TLV; devolve 0 e o conteúdo em v e n */
static int der_get(der_rd *r, u8 *tag, const u8 **v, usize *n) {
    if (r->end - r->p < 2) return -1;
    *tag = *r->p++;
    usize len = *r->p++;
    if (len & 0x80) {
        int k = (int)(len & 0x7f);
        if (k < 1 || k > 4 || r->end - r->p < k) return -1;
        len = 0;
        while (k--) len = len << 8 | *r->p++;
    }
    if ((usize)(r->end - r->p) < len) return -1;
    *v = r->p;
    *n = len;
    r->p += len;
    return 0;
}
static int der_expect(der_rd *r, u8 want, const u8 **v, usize *n) {
    u8 t;
    if (der_get(r, &t, v, n) || t != want) return -1;
    return 0;
}
static int der_get_rb(der_rd *r, rb_t *a) {
    const u8 *v;
    usize n;
    if (der_expect(r, 0x02, &v, &n) || !n || (v[0] & 0x80)) return -1;
    return rb_from_be(a, v, n);
}

/* OIDs (conteúdo) */
static const u8 OID_RSA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const u8 OID_ECPUB[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
static const u8 OID_P256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
static const u8 OID_P384[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
static const u8 OID_P521[] = {0x2b, 0x81, 0x04, 0x00, 0x23};

/* ================================================================ chaves */
enum { SK_RSA = 1, SK_EC = 2 };
typedef struct {
    int type, priv, curve;
    u32 bits;
    rb_t *n, *e, *d, *p, *q, *dp, *dq, *qi;       /* RSA (d..qi só na privada) */
    u8 pub[1 + 2 * 66];                           /* EC: 04 || X || Y */
    u32 publen, B;                                /* B = bytes de uma coordenada */
    u8 k[66];                                     /* EC: escalar privado (B bytes) */
} sk_key;

static u32 sk_curve_bytes(int c) { return c == EC_P256 ? 32 : c == EC_P384 ? 48 : c == EC_P521 ? 66 : 0; }
static u32 sk_curve_bits(int c) { return c == EC_P256 ? 256 : c == EC_P384 ? 384 : c == EC_P521 ? 521 : 0; }
static int sk_curve_for_bits(u32 bits) { return bits == 256 ? EC_P256 : bits == 384 ? EC_P384 : bits == 521 ? EC_P521 : 0; }

static rb_t *rb_new(void) { rb_t *r = malloc(sizeof *r); if (r) r->n = 0; return r; }
static void rb_wipe_free(rb_t *r) { if (r) { memset(r, 0, sizeof *r); free(r); } }
static void sk_free(sk_key *k) {
    if (!k) return;
    rb_wipe_free(k->n); rb_wipe_free(k->e); rb_wipe_free(k->d); rb_wipe_free(k->p);
    rb_wipe_free(k->q); rb_wipe_free(k->dp); rb_wipe_free(k->dq); rb_wipe_free(k->qi);
    memset(k, 0, sizeof *k);
    free(k);
}
static sk_key *sk_alloc(int type) {
    sk_key *k = malloc(sizeof *k);
    if (!k) return 0;
    memset(k, 0, sizeof *k);
    k->type = type;
    return k;
}
static int sk_rsa_alloc(sk_key *k, int priv) {
    k->n = rb_new(); k->e = rb_new();
    if (!k->n || !k->e) return -1;
    if (priv) {
        k->d = rb_new(); k->p = rb_new(); k->q = rb_new(); k->dp = rb_new(); k->dq = rb_new(); k->qi = rb_new();
        if (!k->d || !k->p || !k->q || !k->dp || !k->dq || !k->qi) return -1;
    }
    k->priv = priv;
    return 0;
}
/* cópia só com a parte pública */
static sk_key *sk_public_of(const sk_key *s) {
    sk_key *k = sk_alloc(s->type);
    if (!k) return 0;
    k->bits = s->bits;
    k->curve = s->curve;
    if (s->type == SK_RSA) {
        if (sk_rsa_alloc(k, 0)) { sk_free(k); return 0; }
        rb_copy(k->n, s->n);
        rb_copy(k->e, s->e);
    } else {
        memcpy(k->pub, s->pub, s->publen);
        k->publen = s->publen;
        k->B = s->B;
    }
    return k;
}
static u32 sk_block_size(const sk_key *k) { return k->type == SK_RSA ? (u32)rb_bytes(k->n) : k->B; }

/* ---- geração */
static sk_key *sk_rsa_generate(u32 bits) {
    if (bits < 1024 || bits > 8192 || bits % 8) return 0;
    sk_key *k = sk_alloc(SK_RSA);
    if (!k || sk_rsa_alloc(k, 1)) { sk_free(k); return 0; }
    rb_set(k->e, 65537);
    for (int attempt = 0; attempt < 16; attempt++) {
        if (rb_gen_prime(k->p, bits / 2, 65537) || rb_gen_prime(k->q, bits - bits / 2, 65537)) break;
        int c = rb_cmp(k->p, k->q);
        if (!c) continue;
        if (c < 0) { rb_t t; rb_copy(&t, k->p); rb_copy(k->p, k->q); rb_copy(k->q, &t); }
        rb_mul(k->n, k->p, k->q);
        if (rb_bits(k->n) != bits) continue;
        rb_t p1, q1, g, lam, t;
        rb_sub_w(&p1, k->p, 1);
        rb_sub_w(&q1, k->q, 1);
        rb_gcd(&g, &p1, &q1);
        rb_mul(&t, &p1, &q1);
        rb_divmod(&lam, 0, &t, &g);                          /* λ = mmc(p-1, q-1) */
        /* d = e⁻¹ mod λ com e pequeno: k·λ ≡ -1 (mod e), d = (k·λ + 1) / e */
        u32 e = 65537, r = rb_div_w(0, &lam, e);
        i64 t0 = 0, t1 = 1, r0 = e, r1 = r;
        while (r1) { i64 qq = r0 / r1, x = r0 - qq * r1; r0 = r1; r1 = x; x = t0 - qq * t1; t0 = t1; t1 = x; }
        if (r0 != 1) continue;
        i64 inv = t0 % (i64)e;
        if (inv < 0) inv += e;                               /* r⁻¹ mod e */
        u32 kk = (u32)((e - (u64)inv) % e);
        rb_mul_w(&t, &lam, kk);
        rb_add_w(&t, &t, 1);
        if (rb_div_w(k->d, &t, e) != 0) continue;
        rb_mod(k->dp, k->d, &p1);
        rb_mod(k->dq, k->d, &q1);
        rb_t pm2;
        rb_sub_w(&pm2, k->p, 2);
        rb_modexp(k->qi, k->q, &pm2, k->p);                  /* q⁻¹ mod p (p primo) */
        k->bits = bits;
        return k;
    }
    sk_free(k);
    return 0;
}
static sk_key *sk_ec_generate(u32 bits) {
    int c = sk_curve_for_bits(bits);
    if (!c) return 0;
    sk_key *k = sk_alloc(SK_EC);
    if (!k) return 0;
    k->curve = c;
    k->B = sk_curve_bytes(c);
    k->bits = sk_curve_bits(c);
    k->publen = 1 + 2 * k->B;
    if (ecdh_keygen(c, k->k, k->pub)) { sk_free(k); return 0; }
    k->priv = 1;
    return k;
}

/* ---- exportação (formato externo do macOS) */
static int sk_export(const sk_key *k, u8 **out, usize *outlen) {
    sk_buf s = {0};
    if (k->type == SK_EC) {
        sb_put(&s, k->pub, k->publen);
        if (k->priv) sb_put(&s, k->k, k->B);
    } else {
        sk_buf in = {0};
        if (k->priv) {
            static const u8 zero = 0;
            der_uint(&in, &zero, 1);
            der_rb(&in, k->n); der_rb(&in, k->e); der_rb(&in, k->d); der_rb(&in, k->p);
            der_rb(&in, k->q); der_rb(&in, k->dp); der_rb(&in, k->dq); der_rb(&in, k->qi);
        } else {
            der_rb(&in, k->n);
            der_rb(&in, k->e);
        }
        der_wrap(&s, 0x30, &in);
    }
    if (s.err) { free(s.b); return -1; }
    *out = s.b;
    *outlen = s.n;
    return 0;
}
/* SubjectPublicKeyInfo (para certificados, PEM e SecKeyCopyExternalRepresentation de quem quer X.509) */
__attribute__((unused)) static int sk_export_spki(const sk_key *k, u8 **out, usize *outlen) {
    sk_buf alg = {0}, bits = {0}, all = {0};
    u8 *raw;
    usize rn;
    sk_key *pub = k->priv ? sk_public_of(k) : 0;
    if (sk_export(pub ? pub : k, &raw, &rn)) { sk_free(pub); return -1; }
    sk_free(pub);
    if (k->type == SK_RSA) {
        der_tlv(&alg, 0x06, OID_RSA, sizeof OID_RSA);
        sb_byte(&alg, 0x05); sb_byte(&alg, 0x00);
    } else {
        der_tlv(&alg, 0x06, OID_ECPUB, sizeof OID_ECPUB);
        if (k->curve == EC_P256) der_tlv(&alg, 0x06, OID_P256, sizeof OID_P256);
        else if (k->curve == EC_P384) der_tlv(&alg, 0x06, OID_P384, sizeof OID_P384);
        else der_tlv(&alg, 0x06, OID_P521, sizeof OID_P521);
    }
    sb_byte(&bits, 0);
    sb_put(&bits, raw, rn);
    free(raw);
    sk_buf in = {0};
    der_wrap(&in, 0x30, &alg);
    der_wrap(&in, 0x03, &bits);
    der_wrap(&all, 0x30, &in);
    if (all.err) { free(all.b); return -1; }
    *out = all.b;
    *outlen = all.n;
    return 0;
}

/* ---- importação */
static int sk_rsa_check_priv(sk_key *k);
static sk_key *sk_rsa_from_pkcs1(const u8 *b, usize n, int priv) {
    der_rd r = {b, b + n}, in;
    const u8 *v;
    usize vn;
    if (der_expect(&r, 0x30, &v, &vn)) return 0;
    in.p = v; in.end = v + vn;
    sk_key *k = sk_alloc(SK_RSA);
    if (!k || sk_rsa_alloc(k, priv)) { sk_free(k); return 0; }
    if (priv) {
        rb_t ver;
        if (der_get_rb(&in, &ver) || ver.n || der_get_rb(&in, k->n) || der_get_rb(&in, k->e) || der_get_rb(&in, k->d) ||
            der_get_rb(&in, k->p) || der_get_rb(&in, k->q) || der_get_rb(&in, k->dp) || der_get_rb(&in, k->dq) ||
            der_get_rb(&in, k->qi) || sk_rsa_check_priv(k)) { sk_free(k); return 0; }
    } else if (der_get_rb(&in, k->n) || der_get_rb(&in, k->e)) { sk_free(k); return 0; }
    k->bits = rb_bits(k->n);
    if (k->bits < 512 || k->bits > 8192 || !(k->n->d[0] & 1) || rb_is_zero(k->e)) { sk_free(k); return 0; }
    return k;
}
/* p·q = n e e·d ≡ 1 mod (p-1) e (q-1): recusa uma chave privada corrompida antes de assinar com ela */
static int sk_rsa_check_priv(sk_key *k) {
    rb_t t, p1, q1, x;
    if (rb_is_zero(k->p) || rb_is_zero(k->q)) return -1;
    rb_mul(&t, k->p, k->q);
    if (rb_cmp(&t, k->n)) return -1;
    rb_sub_w(&p1, k->p, 1);
    rb_sub_w(&q1, k->q, 1);
    rb_mul(&t, k->e, k->dp);
    rb_mod(&x, &t, &p1);
    if (!(x.n == 1 && x.d[0] == 1)) return -1;
    rb_mul(&t, k->e, k->dq);
    rb_mod(&x, &t, &q1);
    if (!(x.n == 1 && x.d[0] == 1)) return -1;
    rb_mul(&t, k->qi, k->q);
    rb_mod(&x, &t, k->p);
    if (!(x.n == 1 && x.d[0] == 1)) return -1;
    return 0;
}
static sk_key *sk_ec_from_x963(const u8 *b, usize n, int priv, int curve_hint) {
    int c = curve_hint;
    if (!c) {
        if (priv) c = n == 97 ? EC_P256 : n == 145 ? EC_P384 : n == 199 ? EC_P521 : 0;
        else c = n == 65 ? EC_P256 : n == 97 ? EC_P384 : n == 133 ? EC_P521 : 0;
    }
    u32 B = sk_curve_bytes(c);
    if (!c || n != (priv ? 1 + 3 * B : 1 + 2 * B) || b[0] != 0x04) return 0;
    sk_key *k = sk_alloc(SK_EC);
    if (!k) return 0;
    k->curve = c;
    k->B = B;
    k->bits = sk_curve_bits(c);
    k->publen = 1 + 2 * B;
    memcpy(k->pub, b, k->publen);
    if (priv) {
        u8 chk[1 + 2 * 66];
        memcpy(k->k, b + k->publen, B);
        k->priv = 1;
        if (ec_public_key(c, k->k, chk) || memcmp(chk, k->pub, k->publen)) { sk_free(k); return 0; }
    }
    return k;
}
static int sk_curve_from_oid(const u8 *v, usize n) {
    if (n == sizeof OID_P256 && !memcmp(v, OID_P256, n)) return EC_P256;
    if (n == sizeof OID_P384 && !memcmp(v, OID_P384, n)) return EC_P384;
    if (n == sizeof OID_P521 && !memcmp(v, OID_P521, n)) return EC_P521;
    return 0;
}
/* SubjectPublicKeyInfo -> chave pública */
static sk_key *sk_from_spki(const u8 *b, usize n) {
    der_rd r = {b, b + n}, in, alg;
    const u8 *v, *av, *oid, *bits;
    usize vn, an, on, bn;
    if (der_expect(&r, 0x30, &v, &vn)) return 0;
    in.p = v; in.end = v + vn;
    if (der_expect(&in, 0x30, &av, &an) || der_expect(&in, 0x03, &bits, &bn) || bn < 2 || bits[0]) return 0;
    alg.p = av; alg.end = av + an;
    if (der_expect(&alg, 0x06, &oid, &on)) return 0;
    if (on == sizeof OID_RSA && !memcmp(oid, OID_RSA, on)) return sk_rsa_from_pkcs1(bits + 1, bn - 1, 0);
    if (on == sizeof OID_ECPUB && !memcmp(oid, OID_ECPUB, on)) {
        const u8 *cv;
        usize cn;
        if (der_expect(&alg, 0x06, &cv, &cn)) return 0;
        return sk_ec_from_x963(bits + 1, bn - 1, 0, sk_curve_from_oid(cv, cn));
    }
    return 0;
}
/* ECPrivateKey (SEC1): SEQ { INT 1, OCTET k, [0] curva, [1] BIT STRING pub } */
static sk_key *sk_ec_from_sec1(const u8 *b, usize n, int curve_hint) {
    der_rd r = {b, b + n}, in;
    const u8 *v, *kv;
    usize vn, kn;
    rb_t ver;
    if (der_expect(&r, 0x30, &v, &vn)) return 0;
    in.p = v; in.end = v + vn;
    if (der_get_rb(&in, &ver) || ver.n != 1 || ver.d[0] != 1 || der_expect(&in, 0x04, &kv, &kn)) return 0;
    int c = curve_hint;
    while (in.p < in.end) {
        u8 t;
        const u8 *xv;
        usize xn;
        if (der_get(&in, &t, &xv, &xn)) return 0;
        if (t == 0xa0) {
            der_rd o = {xv, xv + xn};
            const u8 *cv;
            usize cn;
            if (!der_expect(&o, 0x06, &cv, &cn)) c = sk_curve_from_oid(cv, cn);
        }
    }
    if (!c) c = kn == 32 ? EC_P256 : kn == 48 ? EC_P384 : kn == 66 ? EC_P521 : 0;
    u32 B = sk_curve_bytes(c);
    if (!c || kn > B) return 0;
    sk_key *k = sk_alloc(SK_EC);
    if (!k) return 0;
    k->curve = c;
    k->B = B;
    k->bits = sk_curve_bits(c);
    k->publen = 1 + 2 * B;
    memset(k->k, 0, sizeof k->k);
    memcpy(k->k + (B - kn), kv, kn);
    if (ec_public_key(c, k->k, k->pub)) { sk_free(k); return 0; }
    k->priv = 1;
    return k;
}
/* PKCS#8 PrivateKeyInfo: SEQ { INT 0, AlgorithmIdentifier, OCTET STRING chave } */
static sk_key *sk_from_pkcs8(const u8 *b, usize n) {
    der_rd r = {b, b + n}, in, alg;
    const u8 *v, *av, *oid, *kv;
    usize vn, an, on, kn;
    rb_t ver;
    if (der_expect(&r, 0x30, &v, &vn)) return 0;
    in.p = v; in.end = v + vn;
    if (der_get_rb(&in, &ver) || ver.n > 1 || der_expect(&in, 0x30, &av, &an) || der_expect(&in, 0x04, &kv, &kn)) return 0;
    alg.p = av; alg.end = av + an;
    if (der_expect(&alg, 0x06, &oid, &on)) return 0;
    if (on == sizeof OID_RSA && !memcmp(oid, OID_RSA, on)) return sk_rsa_from_pkcs1(kv, kn, 1);
    if (on == sizeof OID_ECPUB && !memcmp(oid, OID_ECPUB, on)) {
        const u8 *cv;
        usize cn;
        int c = der_expect(&alg, 0x06, &cv, &cn) ? 0 : sk_curve_from_oid(cv, cn);
        return sk_ec_from_sec1(kv, kn, c);
    }
    return 0;
}
/* type: SK_RSA, SK_EC ou 0 (descobre); priv: 1, 0 ou -1 (descobre) */
static sk_key *sk_import(const u8 *b, usize n, int type, int priv) {
    if (!b || n < 2) return 0;
    sk_key *k = 0;
    if (type != SK_RSA && b[0] == 0x04) {
        if (priv != 0) k = sk_ec_from_x963(b, n, 1, 0);
        if (!k && priv != 1) k = sk_ec_from_x963(b, n, 0, 0);
        if (k) return k;
    }
    if (b[0] != 0x30) return 0;
    if (priv != 0) {
        if (type != SK_EC && (k = sk_rsa_from_pkcs1(b, n, 1))) return k;
        if ((k = sk_from_pkcs8(b, n))) return k;
        if (type != SK_RSA && (k = sk_ec_from_sec1(b, n, 0))) return k;
    }
    if (priv != 1) {
        if (type != SK_EC && (k = sk_rsa_from_pkcs1(b, n, 0))) return k;
        if ((k = sk_from_spki(b, n))) return k;
    }
    return 0;
}

/* ================================================================ RSA: operações e paddings */
/* out (k bytes) = in^e mod n; in < n */
static int sk_rsa_public_op(const sk_key *key, const u8 *in, usize inlen, u8 *out) {
    usize k = rb_bytes(key->n);
    rb_t m, c;
    if (inlen > k || rb_from_be(&m, in, inlen) || rb_cmp(&m, key->n) >= 0) return -1;
    if (rb_modexp(&c, &m, key->e, key->n)) return -1;
    return rb_to_be(&c, out, k);
}
/* out (k bytes) = in^d mod n por CRT, conferido pela pública (falha de cálculo não vaza p/q) */
static int sk_rsa_private_op(const sk_key *key, const u8 *in, usize inlen, u8 *out) {
    if (!key->priv) return -1;
    usize k = rb_bytes(key->n);
    rb_t c, m1, m2, h, t, m;
    if (inlen > k || rb_from_be(&c, in, inlen) || rb_cmp(&c, key->n) >= 0) return -1;
    if (rb_modexp(&m1, &c, key->dp, key->p) || rb_modexp(&m2, &c, key->dq, key->q)) return -1;
    rb_mod(&t, &m2, key->p);
    if (rb_cmp(&m1, &t) >= 0) rb_sub(&h, &m1, &t);
    else { rb_add(&h, &m1, key->p); rb_sub(&h, &h, &t); }
    rb_mul(&t, &h, key->qi);
    rb_mod(&h, &t, key->p);
    rb_mul(&t, &h, key->q);
    rb_add(&m, &t, &m2);
    rb_t back;
    if (rb_modexp(&back, &m, key->e, key->n) || rb_cmp(&back, &c)) return -1;
    int r = rb_to_be(&m, out, k);
    memset(&m1, 0, sizeof m1); memset(&m2, 0, sizeof m2); memset(&h, 0, sizeof h); memset(&m, 0, sizeof m);
    return r;
}

static const u8 *sk_digest_info(int h, usize *n) {
    static const u8 md5[] = {0x30, 0x20, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05, 0x05, 0x00, 0x04, 0x10};
    static const u8 s1[] = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
    static const u8 s224[] = {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c};
    static const u8 s256[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
    static const u8 s384[] = {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
    static const u8 s512[] = {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
    switch (h) {
    case SKH_MD5: *n = sizeof md5; return md5;
    case SKH_SHA1: *n = sizeof s1; return s1;
    case SKH_SHA224: *n = sizeof s224; return s224;
    case SKH_SHA256: *n = sizeof s256; return s256;
    case SKH_SHA384: *n = sizeof s384; return s384;
    case SKH_SHA512: *n = sizeof s512; return s512;
    default: *n = 0; return 0;
    }
}
/* EM tipo 1: 00 01 FF..FF 00 [DigestInfo] T; h = SKH_NONE assina T como veio (DigestPKCS1v15Raw) */
static int sk_emsa_pkcs1(int h, const u8 *t, usize tn, u8 *em, usize k) {
    usize pn;
    const u8 *pre = sk_digest_info(h, &pn);
    if (h != SKH_NONE && (!pre || tn != sk_hash_len(h))) return -1;
    if (tn + pn + 11 > k) return -1;
    usize ps = k - tn - pn - 3;
    em[0] = 0; em[1] = 1;
    memset(em + 2, 0xff, ps);
    em[2 + ps] = 0;
    if (pn) memcpy(em + 3 + ps, pre, pn);
    memcpy(em + 3 + ps + pn, t, tn);
    return 0;
}
static void sk_mgf1(int h, const u8 *seed, usize sn, u8 *out, usize on) {
    u8 buf[512 + 4], d[64];
    u32 hl = sk_hash_len(h), ctr = 0;
    memcpy(buf, seed, sn);
    for (usize done = 0; done < on; ctr++) {
        buf[sn] = (u8)(ctr >> 24); buf[sn + 1] = (u8)(ctr >> 16); buf[sn + 2] = (u8)(ctr >> 8); buf[sn + 3] = (u8)ctr;
        sk_hash(h, buf, sn + 4, d);
        for (u32 i = 0; i < hl && done < on; i++) out[done++] ^= d[i];
    }
}
/* EMSA-PSS-ENCODE com sal de hLen bytes; emBits = bits(n) - 1 */
static int sk_emsa_pss(int h, const u8 *mh, usize mhn, u32 embits, u8 *em) {
    u32 hl = sk_hash_len(h);
    usize emlen = (embits + 7) / 8;
    if (!hl || mhn != hl || emlen < 2 * hl + 2) return -1;
    u8 mp[8 + 64 + 64], H[64], salt[64];
    crypto_random(salt, hl);
    memset(mp, 0, 8);
    memcpy(mp + 8, mh, hl);
    memcpy(mp + 8 + hl, salt, hl);
    sk_hash(h, mp, 8 + 2 * (usize)hl, H);
    usize dbn = emlen - hl - 1;
    memset(em, 0, dbn);
    em[dbn - hl - 1] = 0x01;
    memcpy(em + dbn - hl, salt, hl);
    sk_mgf1(h, H, hl, em, dbn);
    em[0] &= (u8)(0xff >> (8 * emlen - embits));
    memcpy(em + dbn, H, hl);
    em[emlen - 1] = 0xbc;
    return 0;
}
/* EMSA-PSS-VERIFY, qualquer tamanho de sal */
static int sk_emsa_pss_verify(int h, const u8 *mh, usize mhn, const u8 *emin, usize emlen, u32 embits) {
    u32 hl = sk_hash_len(h);
    if (!hl || mhn != hl || emlen < 2 * (usize)hl + 2 || emin[emlen - 1] != 0xbc) return -1;
    u8 em[1024];
    if (emlen > sizeof em) return -1;
    memcpy(em, emin, emlen);
    usize dbn = emlen - hl - 1;
    if (em[0] & (u8)~(0xff >> (8 * emlen - embits))) return -1;
    const u8 *H = em + dbn;
    sk_mgf1(h, H, hl, em, dbn);
    em[0] &= (u8)(0xff >> (8 * emlen - embits));
    usize i = 0;
    while (i < dbn && !em[i]) i++;
    if (i >= dbn || em[i] != 0x01) return -1;
    const u8 *salt = em + i + 1;
    usize sn = dbn - i - 1;
    u8 mp[8 + 64 + 1024], H2[64];
    if (8 + hl + sn > sizeof mp) return -1;
    memset(mp, 0, 8);
    memcpy(mp + 8, mh, hl);
    memcpy(mp + 8 + hl, salt, sn);
    sk_hash(h, mp, 8 + hl + sn, H2);
    return memcmp(H, H2, hl) ? -1 : 0;
}
static int sk_oaep_encode(int h, const u8 *m, usize mn, u8 *em, usize k) {
    u32 hl = sk_hash_len(h);
    if (!hl || k < 2 * (usize)hl + 2 || mn > k - 2 * hl - 2) return -1;
    usize dbn = k - hl - 1;
    u8 *seed = em + 1, *db = em + 1 + hl;
    em[0] = 0;
    sk_hash(h, (const u8 *)"", 0, db);
    memset(db + hl, 0, dbn - hl - mn - 1);
    db[dbn - mn - 1] = 0x01;
    memcpy(db + dbn - mn, m, mn);
    crypto_random(seed, hl);
    sk_mgf1(h, seed, hl, db, dbn);
    sk_mgf1(h, db, dbn, seed, hl);
    return 0;
}
static int sk_oaep_decode(int h, u8 *em, usize k, u8 *out, usize *outn) {
    u32 hl = sk_hash_len(h);
    if (!hl || k < 2 * (usize)hl + 2) return -1;
    usize dbn = k - hl - 1;
    u8 *seed = em + 1, *db = em + 1 + hl, lh[64];
    sk_mgf1(h, db, dbn, seed, hl);
    sk_mgf1(h, seed, hl, db, dbn);
    sk_hash(h, (const u8 *)"", 0, lh);
    u32 bad = em[0];
    for (u32 i = 0; i < hl; i++) bad |= (u32)(db[i] ^ lh[i]);
    usize idx = 0;
    u32 found = 0;
    for (usize i = hl; i < dbn; i++) {                        /* sem desvio pelo conteúdo */
        u32 is1 = db[i] == 1, is0 = db[i] == 0;
        u32 take = is1 & !found;
        idx = take ? i : idx;
        found |= is1;
        bad |= !found & !is0 & !is1;
    }
    if (bad || !found) return -1;
    usize mn = dbn - idx - 1;
    if (mn > *outn) return -1;
    memcpy(out, db + idx + 1, mn);
    *outn = mn;
    return 0;
}
static int sk_pkcs1_type2_encode(const u8 *m, usize mn, u8 *em, usize k) {
    if (mn + 11 > k) return -1;
    usize ps = k - mn - 3;
    em[0] = 0; em[1] = 2;
    crypto_random(em + 2, ps);
    for (usize i = 0; i < ps; i++) while (!em[2 + i]) crypto_random(em + 2 + i, 1);
    em[2 + ps] = 0;
    memcpy(em + 3 + ps, m, mn);
    return 0;
}
static int sk_pkcs1_type2_decode(const u8 *em, usize k, u8 *out, usize *outn) {
    if (k < 11 || em[0] || em[1] != 2) return -1;
    usize i = 2;
    while (i < k && em[i]) i++;
    if (i >= k || i < 10) return -1;
    usize mn = k - i - 1;
    if (mn > *outn) return -1;
    memcpy(out, em + i + 1, mn);
    *outn = mn;
    return 0;
}

/* ================================================================ algoritmos (kSecKeyAlgorithm*) */
enum { SKA_NONE = 0, SKA_RSA_SIG_RAW, SKA_RSA_SIG_PKCS1, SKA_RSA_SIG_PSS, SKA_RSA_ENC_RAW, SKA_RSA_ENC_PKCS1,
       SKA_RSA_ENC_OAEP, SKA_ECDSA_X962, SKA_ECDSA_RFC4754, SKA_ECDH, SKA_ECIES, SKA_RSA_OAEP_AESGCM };
enum { SKO_SIGN = 0, SKO_VERIFY = 1, SKO_ENCRYPT = 2, SKO_DECRYPT = 3, SKO_EXCHANGE = 4 };
typedef struct { int kind, hash, message, x963, cofactor, variable_iv; } sk_alg;

static int sk_has(const char *s, const char *w) {
    for (; *s; s++) {
        int i = 0;
        while (w[i] && s[i] && ((s[i] | 0x20) == (w[i] | 0x20))) i++;
        if (!w[i]) return 1;
    }
    return 0;
}
static int sk_hash_in(const char *s) {
    if (sk_has(s, "SHA224")) return SKH_SHA224;
    if (sk_has(s, "SHA256")) return SKH_SHA256;
    if (sk_has(s, "SHA384")) return SKH_SHA384;
    if (sk_has(s, "SHA512")) return SKH_SHA512;
    if (sk_has(s, "SHA1")) return SKH_SHA1;
    if (sk_has(s, "MD5")) return SKH_MD5;
    return SKH_NONE;
}
/* entende os nomes "algid:RSASignatureDigestPSSSHA256" do mOS e os "algid:sign:RSA:digest-PSS:SHA256" */
static int sk_parse_alg(const char *s, sk_alg *a) {
    memset(a, 0, sizeof *a);
    if (!s) return -1;
    a->hash = sk_hash_in(s);
    a->message = sk_has(s, "Message");
    if (sk_has(s, "ECIES")) {
        a->kind = SKA_ECIES;
        a->cofactor = sk_has(s, "Cofactor");
        a->variable_iv = sk_has(s, "VariableIV");
        if (!sk_has(s, "AESGCM") || !a->hash) return -1;
    } else if (sk_has(s, "ECDH")) {
        a->kind = SKA_ECDH;
        a->cofactor = sk_has(s, "Cofactor");
        a->x963 = sk_has(s, "X963");
        if (a->x963 && !a->hash) return -1;
    } else if (sk_has(s, "ECDSA")) {
        a->kind = sk_has(s, "RFC4754") ? SKA_ECDSA_RFC4754 : SKA_ECDSA_X962;
        if (a->message && !a->hash) return -1;
    } else if (sk_has(s, "RSA")) {
        int sig = sk_has(s, "Signature") || sk_has(s, "sign:");
        if (sig) {
            if (sk_has(s, "PSS")) a->kind = SKA_RSA_SIG_PSS;
            else if (sk_has(s, "PKCS1")) a->kind = SKA_RSA_SIG_PKCS1;
            else if (sk_has(s, "Raw")) a->kind = SKA_RSA_SIG_RAW;
            else return -1;
            if (a->kind == SKA_RSA_SIG_PSS && !a->hash) return -1;
            if (a->message && !a->hash) return -1;
        } else {
            if (sk_has(s, "OAEP")) a->kind = sk_has(s, "AESGCM") ? SKA_RSA_OAEP_AESGCM : SKA_RSA_ENC_OAEP;
            else if (sk_has(s, "PKCS1")) a->kind = SKA_RSA_ENC_PKCS1;
            else if (sk_has(s, "Raw")) a->kind = SKA_RSA_ENC_RAW;
            else return -1;
            if ((a->kind == SKA_RSA_ENC_OAEP || a->kind == SKA_RSA_OAEP_AESGCM) && !a->hash) return -1;
        }
    } else return -1;
    return 0;
}
static int sk_supports(const sk_key *k, int op, const sk_alg *a) {
    int rsa = k->type == SK_RSA;
    switch (a->kind) {
    case SKA_RSA_SIG_RAW: case SKA_RSA_SIG_PKCS1: case SKA_RSA_SIG_PSS:
        return rsa && ((op == SKO_SIGN && k->priv) || op == SKO_VERIFY);
    case SKA_RSA_ENC_RAW: case SKA_RSA_ENC_PKCS1: case SKA_RSA_ENC_OAEP: case SKA_RSA_OAEP_AESGCM:
        if (!rsa) return 0;
        if (a->kind == SKA_RSA_OAEP_AESGCM) return 0;  /* idem */
        if (op == SKO_DECRYPT) return k->priv;
        if (op != SKO_ENCRYPT || a->kind == SKA_RSA_OAEP_AESGCM) return 0;
        if (a->kind == SKA_RSA_ENC_OAEP || a->kind == SKA_RSA_OAEP_AESGCM)
            return rb_bytes(k->n) >= 2 * (usize)sk_hash_len(a->hash) + 2;
        return 1;
    case SKA_ECDSA_X962: case SKA_ECDSA_RFC4754:
        return !rsa && ((op == SKO_SIGN && k->priv) || op == SKO_VERIFY);
    case SKA_ECDH: return !rsa && op == SKO_EXCHANGE && k->priv;
    case SKA_ECIES: return 0;                     /* ECIES ainda não: ver docs/servicos-seguranca-plano.md */
    default: return 0;
    }
}

/* digest a assinar: hash da mensagem (Message*) ou o próprio dado (Digest*) */
static int sk_prepare_digest(const sk_alg *a, const u8 *d, usize n, u8 *out, usize *on) {
    if (a->message) {
        if (sk_hash(a->hash, d, n, out)) return -1;
        *on = sk_hash_len(a->hash);
        return 0;
    }
    if (n > 1024) return -1;
    memcpy(out, d, n);
    *on = n;
    return 0;
}

static int sk_ecdsa_der(const u8 *r, const u8 *s, u32 B, u8 **out, usize *on) {
    sk_buf in = {0}, all = {0};
    der_uint(&in, r, B);
    der_uint(&in, s, B);
    der_wrap(&all, 0x30, &in);
    if (all.err) { free(all.b); return -1; }
    *out = all.b;
    *on = all.n;
    return 0;
}
static int sk_ecdsa_undo_der(const u8 *sig, usize n, u8 *r, u8 *s, u32 B) {
    der_rd rd = {sig, sig + n}, in;
    const u8 *v, *rv, *sv;
    usize vn, rn, sn;
    if (der_expect(&rd, 0x30, &v, &vn) || rd.p != rd.end) return -1;
    in.p = v; in.end = v + vn;
    if (der_expect(&in, 0x02, &rv, &rn) || der_expect(&in, 0x02, &sv, &sn) || in.p != in.end) return -1;
    while (rn > 1 && !rv[0]) { rv++; rn--; }
    while (sn > 1 && !sv[0]) { sv++; sn--; }
    if (rn > B || sn > B) return -1;
    memset(r, 0, B); memcpy(r + B - rn, rv, rn);
    memset(s, 0, B); memcpy(s + B - sn, sv, sn);
    return 0;
}

/* assinatura: *out (malloc) */
static int sk_sign(const sk_key *k, const sk_alg *a, const u8 *d, usize n, u8 **out, usize *on) {
    if (!sk_supports(k, SKO_SIGN, a)) return -1;
    u8 dg[1024];
    usize dn;
    if (sk_prepare_digest(a, d, n, dg, &dn)) return -1;
    if (k->type == SK_EC) {
        u8 r[66], s[66];
        if (ecdsa_sign(k->curve, k->k, dg, (u32)dn, r, s)) return -1;
        if (a->kind == SKA_ECDSA_RFC4754) {
            u8 *b = malloc(2 * (usize)k->B);
            if (!b) return -1;
            memcpy(b, r, k->B);
            memcpy(b + k->B, s, k->B);
            *out = b;
            *on = 2 * (usize)k->B;
            return 0;
        }
        return sk_ecdsa_der(r, s, k->B, out, on);
    }
    usize kb = rb_bytes(k->n);
    u8 *em = malloc(kb), *sig = malloc(kb);
    if (!em || !sig) { free(em); free(sig); return -1; }
    int rc;
    if (a->kind == SKA_RSA_SIG_RAW) {
        rc = dn > kb ? -1 : 0;
        if (!rc) { memset(em, 0, kb - dn); memcpy(em + kb - dn, dg, dn); }
    } else if (a->kind == SKA_RSA_SIG_PKCS1) rc = sk_emsa_pkcs1(a->hash, dg, dn, em, kb);
    else {
        u32 embits = rb_bits(k->n) - 1;
        usize emlen = (embits + 7) / 8;
        memset(em, 0, kb);
        rc = sk_emsa_pss(a->hash, dg, dn, embits, em + (kb - emlen));
    }
    if (!rc) rc = sk_rsa_private_op(k, em, kb, sig);
    free(em);
    if (rc) { free(sig); return -1; }
    *out = sig;
    *on = kb;
    return 0;
}
/* 0 = assinatura válida */
static int sk_verify(const sk_key *k, const sk_alg *a, const u8 *d, usize n, const u8 *sig, usize sn) {
    if (!sk_supports(k, SKO_VERIFY, a)) return -1;
    u8 dg[1024];
    usize dn;
    if (sk_prepare_digest(a, d, n, dg, &dn)) return -1;
    if (k->type == SK_EC) {
        u8 r[66], s[66];
        if (a->kind == SKA_ECDSA_RFC4754) {
            if (sn != 2 * (usize)k->B) return -1;
            memcpy(r, sig, k->B);
            memcpy(s, sig + k->B, k->B);
        } else if (sk_ecdsa_undo_der(sig, sn, r, s, k->B)) return -1;
        return ecdsa_verify(k->curve, k->pub, k->publen, dg, (u32)dn, r, k->B, s, k->B) ? -1 : 0;
    }
    usize kb = rb_bytes(k->n);
    if (sn != kb) return -1;
    u8 *em = malloc(kb), *want = malloc(kb);
    if (!em || !want) { free(em); free(want); return -1; }
    int rc = sk_rsa_public_op(k, sig, sn, em);
    if (!rc) {
        if (a->kind == SKA_RSA_SIG_RAW) {
            rc = dn > kb ? -1 : 0;
            if (!rc) { memset(want, 0, kb - dn); memcpy(want + kb - dn, dg, dn); rc = memcmp(em, want, kb) ? -1 : 0; }
        } else if (a->kind == SKA_RSA_SIG_PKCS1) {
            rc = sk_emsa_pkcs1(a->hash, dg, dn, want, kb);
            if (!rc) rc = memcmp(em, want, kb) ? -1 : 0;
        } else {
            u32 embits = rb_bits(k->n) - 1;
            usize emlen = (embits + 7) / 8;
            rc = (kb > emlen && em[0]) ? -1 : sk_emsa_pss_verify(a->hash, dg, dn, em + (kb - emlen), emlen, embits);
        }
    }
    free(em);
    free(want);
    return rc;
}

/* X9.63 KDF: hash(Z || contador || SharedInfo) */
static int sk_x963_kdf(int h, const u8 *z, usize zn, const u8 *info, usize in, u8 *out, usize on) {
    u32 hl = sk_hash_len(h);
    usize bn = zn + 4 + in;
    u8 *buf = malloc(bn ? bn : 1), d[64];
    if (!buf || !hl) { free(buf); return -1; }
    memcpy(buf, z, zn);
    if (in) memcpy(buf + zn + 4, info, in);
    for (u32 ctr = 1, done = 0; done < on; ctr++) {
        buf[zn] = (u8)(ctr >> 24); buf[zn + 1] = (u8)(ctr >> 16); buf[zn + 2] = (u8)(ctr >> 8); buf[zn + 3] = (u8)ctr;
        sk_hash(h, buf, bn, d);
        for (u32 i = 0; i < hl && done < on; i++) out[done++] = d[i];
    }
    memset(buf, 0, bn);
    free(buf);
    return 0;
}
/* ECDH: segredo = X de k·Q; com X9.63 a saída tem `want` bytes (0 = tamanho da coordenada) */
static int sk_exchange(const sk_key *k, const sk_alg *a, const sk_key *peer, const u8 *info, usize in, usize want,
                       u8 **out, usize *on) {
    if (!sk_supports(k, SKO_EXCHANGE, a) || !peer || peer->type != SK_EC || peer->curve != k->curve) return -1;
    u8 z[66];
    if (ecdh_shared(k->curve, z, k->k, peer->pub, peer->publen)) return -1;
    usize n = a->x963 ? (want ? want : k->B) : k->B;
    u8 *b = malloc(n);
    if (!b) return -1;
    if (a->x963) {
        if (sk_x963_kdf(a->hash, z, k->B, info, in, b, n)) { free(b); return -1; }
    } else memcpy(b, z, n);
    memset(z, 0, sizeof z);
    *out = b;
    *on = n;
    return 0;
}

static int sk_encrypt(const sk_key *k, const sk_alg *a, const u8 *d, usize n, u8 **out, usize *on) {
    if (!sk_supports(k, SKO_ENCRYPT, a) || k->type != SK_RSA || a->kind == SKA_RSA_OAEP_AESGCM) return -1;
    usize kb = rb_bytes(k->n);
    u8 *em = malloc(kb), *c = malloc(kb);
    if (!em || !c) { free(em); free(c); return -1; }
    int rc;
    if (a->kind == SKA_RSA_ENC_RAW) {
        rc = n > kb ? -1 : 0;
        if (!rc) { memset(em, 0, kb - n); memcpy(em + kb - n, d, n); }
    } else if (a->kind == SKA_RSA_ENC_PKCS1) rc = sk_pkcs1_type2_encode(d, n, em, kb);
    else rc = sk_oaep_encode(a->hash, d, n, em, kb);
    if (!rc) rc = sk_rsa_public_op(k, em, kb, c);
    free(em);
    if (rc) { free(c); return -1; }
    *out = c;
    *on = kb;
    return 0;
}
static int sk_decrypt(const sk_key *k, const sk_alg *a, const u8 *d, usize n, u8 **out, usize *on) {
    if (!sk_supports(k, SKO_DECRYPT, a) || k->type != SK_RSA || a->kind == SKA_RSA_OAEP_AESGCM) return -1;
    usize kb = rb_bytes(k->n);
    if (n != kb) return -1;
    u8 *em = malloc(kb), *m = malloc(kb);
    if (!em || !m) { free(em); free(m); return -1; }
    int rc = sk_rsa_private_op(k, d, n, em);
    usize mn = kb;
    if (!rc) {
        if (a->kind == SKA_RSA_ENC_RAW) memcpy(m, em, kb);
        else if (a->kind == SKA_RSA_ENC_PKCS1) rc = sk_pkcs1_type2_decode(em, kb, m, &mn);
        else rc = sk_oaep_decode(a->hash, em, kb, m, &mn);
    }
    memset(em, 0, kb);
    free(em);
    if (rc) { free(m); return -1; }
    *out = m;
    *on = mn;
    return 0;
}

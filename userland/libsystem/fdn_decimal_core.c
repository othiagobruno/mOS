/*
 * mOS libSystem — aritmética do NSDecimal (soma, subtração, produto, divisão, potência, arredondamento nos
 * quatro modos, normalização, compactação, comparação, texto e double). As contas correm num inteiro de 384
 * bits e voltam a 38 dígitos com o arredondamento pedido; o resultado sai compacto, como na Foundation.
 */
#include "fdn_decimal_core.h"

int snprintf(char *s, unsigned long n, const char *f, ...);
double strtod(const char *s, char **e);

#define NW 12
typedef struct { unsigned int w[NW]; } big_t;

static void bz(big_t *b) { for (int i = 0; i < NW; i++) b->w[i] = 0; }
static int b_zero(const big_t *b) { for (int i = 0; i < NW; i++) if (b->w[i]) return 0; return 1; }
static int b_bits(const big_t *b) {
    for (int i = NW - 1; i >= 0; i--)
        if (b->w[i]) { int n = 32; while (!(b->w[i] >> (n - 1))) n--; return i * 32 + n; }
    return 0;
}
static int b_fits(const big_t *b) { return b_bits(b) <= 128; }
static unsigned b_mul_small(big_t *b, unsigned k) {
    unsigned long long c = 0;
    for (int i = 0; i < NW; i++) { c += (unsigned long long)b->w[i] * k; b->w[i] = (unsigned)c; c >>= 32; }
    return (unsigned)c;
}
static void b_add_small(big_t *b, unsigned k) {
    unsigned long long c = k;
    for (int i = 0; i < NW && c; i++) { c += b->w[i]; b->w[i] = (unsigned)c; c >>= 32; }
}
static unsigned b_div_small(big_t *b, unsigned k) {
    unsigned long long r = 0;
    for (int i = NW - 1; i >= 0; i--) { r = r << 32 | b->w[i]; b->w[i] = (unsigned)(r / k); r %= k; }
    return (unsigned)r;
}
static int b_cmp(const big_t *a, const big_t *b) {
    for (int i = NW - 1; i >= 0; i--) if (a->w[i] != b->w[i]) return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}
static void b_add(big_t *r, const big_t *a, const big_t *b) {
    unsigned long long c = 0;
    for (int i = 0; i < NW; i++) { c += (unsigned long long)a->w[i] + b->w[i]; r->w[i] = (unsigned)c; c >>= 32; }
}
static void b_sub(big_t *r, const big_t *a, const big_t *b) { /* a >= b */
    long long br = 0;
    for (int i = 0; i < NW; i++) {
        long long v = (long long)a->w[i] - b->w[i] - br;
        br = v < 0;
        r->w[i] = (unsigned)(v + (br ? 4294967296LL : 0));
    }
}
static void b_mul(big_t *r, const big_t *a, const big_t *b) {
    unsigned long long acc[2 * NW];
    for (int i = 0; i < 2 * NW; i++) acc[i] = 0;
    for (int i = 0; i < NW; i++) {
        if (!a->w[i]) continue;
        unsigned long long c = 0;
        for (int j = 0; j + i < NW; j++) {
            unsigned long long v = (unsigned long long)a->w[i] * b->w[j] + acc[i + j] + c;
            acc[i + j] = v & 0xFFFFFFFFu;
            c = v >> 32;
        }
    }
    for (int i = 0; i < NW; i++) r->w[i] = (unsigned)acc[i];
}
static int b_bit(const big_t *b, int i) { return (b->w[i / 32] >> (i % 32)) & 1; }
static void b_shl1(big_t *b) {
    for (int i = NW - 1; i > 0; i--) b->w[i] = b->w[i] << 1 | b->w[i - 1] >> 31;
    b->w[0] <<= 1;
}
static void b_divmod(big_t *q, big_t *r, const big_t *a, const big_t *d) {
    bz(q); bz(r);
    for (int i = b_bits(a) - 1; i >= 0; i--) {
        b_shl1(r);
        r->w[0] |= (unsigned)b_bit(a, i);
        if (b_cmp(r, d) >= 0) { b_sub(r, r, d); q->w[i / 32] |= 1u << (i % 32); }
    }
}
static void from_dec(big_t *b, const nsdec_t *d) {
    bz(b);
    for (int i = 0; i < (int)d->length && i < 8; i++) b->w[i / 2] |= (unsigned)d->m[i] << (16 * (i & 1));
}
static void write_raw(nsdec_t *d, const big_t *b, int exp, int neg) {
    nsdec_t z = {0};
    *d = z;
    int n = 0;
    for (int i = 0; i < 8; i++) { d->m[i] = (unsigned short)(b->w[i / 2] >> (16 * (i & 1))); if (d->m[i]) n = i + 1; }
    d->length = (unsigned)n;
    d->exp = n ? exp : 0;
    d->neg = n ? (unsigned)neg : 0;
    d->compact = 0;
}

int fdn_dec_isnan(const nsdec_t *d) { return d->length == 0 && d->neg; }
void fdn_dec_nan(nsdec_t *d) { nsdec_t z = {0}; *d = z; d->neg = 1; }

static void compact_big(big_t *m, int *exp) {
    if (b_zero(m)) { *exp = 0; return; }
    while (*exp < 127) {
        big_t t = *m;
        if (b_div_small(&t, 10)) break;
        *m = t;
        (*exp)++;
    }
}

/* volta a 38 dígitos (128 bits) com expoente em [min_exp, 127], arredondando no modo pedido */
static int reduce(nsdec_t *r, big_t *m, int exp, int neg, int mode, int min_exp) {
    unsigned rd = 0;
    int sticky = 0, removed = 0, st = DEC_OK;
    if (min_exp < -128) min_exp = -128;
    while (!b_zero(m) && (!b_fits(m) || exp < min_exp)) {
        sticky |= rd != 0;
        rd = b_div_small(m, 10);
        exp++;
        removed = 1;
    }
    if (removed && (rd || sticky)) {
        st = DEC_PREC;
        int up = 0;
        switch (mode) {
        case DEC_DOWN: up = neg; break;
        case DEC_UP: up = !neg; break;
        case DEC_BANKERS: up = rd > 5 || (rd == 5 && (sticky || (m->w[0] & 1))); break;
        default: up = rd >= 5; break;
        }
        if (up) {
            b_add_small(m, 1);
            if (!b_fits(m)) { unsigned x = b_div_small(m, 10); exp++; if (x >= 5) b_add_small(m, 1); }
        }
        if (b_zero(m)) { write_raw(r, m, 0, 0); r->compact = 1; return DEC_UNDER; }
    }
    if (b_zero(m)) exp = 0;
    while (exp > 127) {
        big_t t = *m;
        if (b_mul_small(&t, 10) || !b_fits(&t)) break;
        *m = t;
        exp--;
    }
    if (exp > 127) { fdn_dec_nan(r); return DEC_OVER; }
    compact_big(m, &exp);
    write_raw(r, m, exp, neg);
    r->compact = 1;
    return st;
}

void fdn_dec_from_u64(nsdec_t *d, unsigned long long mant, int exp, int neg) {
    big_t b;
    bz(&b);
    b.w[0] = (unsigned)mant;
    b.w[1] = (unsigned)(mant >> 32);
    reduce(d, &b, exp, neg, DEC_PLAIN, -128);
}
void fdn_dec_from_i64(nsdec_t *d, long long v) {
    unsigned long long u = v < 0 ? 0ULL - (unsigned long long)v : (unsigned long long)v;
    fdn_dec_from_u64(d, u, 0, v < 0);
}

int fdn_dec_add(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode) {
    if (fdn_dec_isnan(a) || fdn_dec_isnan(b)) { fdn_dec_nan(r); return DEC_OVER; }
    big_t A, B, R;
    from_dec(&A, a); from_dec(&B, b);
    int ea = a->exp, eb = b->exp, na = a->neg, nb = b->neg;
    if (b_zero(&B)) return reduce(r, &A, ea, na, mode, -128);
    if (b_zero(&A)) return reduce(r, &B, eb, nb, mode, -128);
    if (ea < eb) { big_t t = A; A = B; B = t; int x = ea; ea = eb; eb = x; x = na; na = nb; nb = x; }
    int diff = ea - eb;
    while (diff > 0 && b_bits(&A) < 372) { b_mul_small(&A, 10); ea--; diff--; }
    if (diff > 0) {
        /* B ficou dezenas de dígitos abaixo da precisão: entra só como "um pouco mais/menos" */
        b_mul_small(&A, 10);
        ea--;
        bz(&B);
        B.w[0] = 1;
    }
    int neg;
    if (na == nb) { b_add(&R, &A, &B); neg = na; }
    else if (b_cmp(&A, &B) >= 0) { b_sub(&R, &A, &B); neg = na; }
    else { b_sub(&R, &B, &A); neg = nb; }
    if (b_zero(&R)) neg = 0;
    return reduce(r, &R, ea, neg, mode, -128);
}
int fdn_dec_sub(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode) {
    nsdec_t nb = *b;
    if (!fdn_dec_isnan(b) && b->length) nb.neg ^= 1;
    return fdn_dec_add(r, a, &nb, mode);
}
int fdn_dec_mul(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode) {
    if (fdn_dec_isnan(a) || fdn_dec_isnan(b)) { fdn_dec_nan(r); return DEC_OVER; }
    big_t A, B, R;
    from_dec(&A, a); from_dec(&B, b);
    b_mul(&R, &A, &B);
    return reduce(r, &R, (int)a->exp + (int)b->exp, a->neg ^ b->neg, mode, -128);
}
int fdn_dec_div(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode) {
    if (fdn_dec_isnan(a) || fdn_dec_isnan(b)) { fdn_dec_nan(r); return DEC_OVER; }
    big_t A, B, Q, Rm;
    from_dec(&A, a); from_dec(&B, b);
    if (b_zero(&B)) { fdn_dec_nan(r); return DEC_DIV0; }
    if (b_zero(&A)) return reduce(r, &A, 0, 0, mode, -128);
    int ea = a->exp;
    while (b_bits(&A) < b_bits(&B) + 134) { b_mul_small(&A, 10); ea--; }
    b_divmod(&Q, &Rm, &A, &B);
    b_mul_small(&Q, 10);
    if (!b_zero(&Rm)) b_add_small(&Q, 1); /* dígito de guarda: só marca que sobrou resto */
    return reduce(r, &Q, ea - (int)b->exp - 1, a->neg ^ b->neg, mode, -128);
}
/* potência exata até ~57 dígitos de trabalho; o que cai abaixo vira só dígito de guarda */
typedef struct { big_t m; int exp, sticky; } wide_t;
static void wide_trim(wide_t *w) { while (b_bits(&w->m) > 190) { w->sticky |= b_div_small(&w->m, 10) != 0; w->exp++; } }
static void wide_mul(wide_t *r, const wide_t *a, const wide_t *b) {
    big_t p;
    b_mul(&p, &a->m, &b->m);
    r->m = p;
    r->exp = a->exp + b->exp;
    r->sticky = a->sticky | b->sticky;
    wide_trim(r);
}
int fdn_dec_pow(nsdec_t *r, const nsdec_t *a, unsigned long power, int mode) {
    if (fdn_dec_isnan(a)) { fdn_dec_nan(r); return DEC_OVER; }
    wide_t acc, base;
    bz(&acc.m); acc.m.w[0] = 1; acc.exp = 0; acc.sticky = 0;
    from_dec(&base.m, a); base.exp = a->exp; base.sticky = 0;
    int neg = a->neg && (power & 1);
    while (power) {
        if (power & 1) wide_mul(&acc, &acc, &base);
        power >>= 1;
        if (power) wide_mul(&base, &base, &base);
        if (acc.exp > 100000 || base.exp > 100000) { fdn_dec_nan(r); return DEC_OVER; }
        if (acc.exp < -100000) break;
    }
    if (acc.sticky) { b_mul_small(&acc.m, 10); b_add_small(&acc.m, 1); acc.exp--; }
    if (acc.exp < -100000) { bz(&acc.m); acc.exp = 0; }
    return reduce(r, &acc.m, acc.exp, neg, mode, -128);
}
int fdn_dec_pow10(nsdec_t *r, const nsdec_t *a, int power, int mode) {
    if (fdn_dec_isnan(a)) { fdn_dec_nan(r); return DEC_OVER; }
    big_t A;
    from_dec(&A, a);
    return reduce(r, &A, (int)a->exp + power, a->neg, mode, -128);
}
int fdn_dec_round(nsdec_t *r, const nsdec_t *a, int scale, int mode) {
    if (fdn_dec_isnan(a)) { fdn_dec_nan(r); return DEC_OK; }
    if (scale == DEC_NO_SCALE) { *r = *a; return DEC_OK; }
    big_t A;
    from_dec(&A, a);
    int st = reduce(r, &A, a->exp, a->neg, mode, -scale);
    return st == DEC_PREC ? DEC_OK : st; /* arredondar de propósito não é perda de precisão */
}
int fdn_dec_normalize(nsdec_t *a, nsdec_t *b, int mode) {
    if (fdn_dec_isnan(a) || fdn_dec_isnan(b)) return DEC_OVER;
    big_t A, B;
    from_dec(&A, a); from_dec(&B, b);
    int ea = a->exp, eb = b->exp;
    nsdec_t *hi = a, *lo = b;
    big_t *H = &A, *L = &B;
    int *eh = &ea, *el = &eb;
    if (ea < eb) { hi = b; lo = a; H = &B; L = &A; eh = &eb; el = &ea; }
    if (b_zero(H)) *eh = *el;
    while (*eh > *el) {
        big_t t = *H;
        if (b_mul_small(&t, 10) || !b_fits(&t)) break;
        *H = t;
        (*eh)--;
    }
    int st = DEC_OK;
    if (*eh > *el) {
        nsdec_t tmp;
        st = reduce(&tmp, L, *el, lo->neg, mode, *eh);
        from_dec(L, &tmp);
        /* reduce compacta: refaz o expoente certo multiplicando de volta */
        int e = tmp.exp;
        while (e > *eh) { b_mul_small(L, 10); e--; }
        *el = *eh;
    }
    int nh = hi->neg, nl = lo->neg;
    write_raw(hi, H, *eh, nh);
    write_raw(lo, L, *el, nl);
    if (!hi->length) hi->exp = *eh;
    if (!lo->length) lo->exp = *el;
    return st;
}
void fdn_dec_compact(nsdec_t *d) {
    if (fdn_dec_isnan(d)) return;
    big_t m;
    from_dec(&m, d);
    int e = d->exp;
    compact_big(&m, &e);
    write_raw(d, &m, e, d->neg);
    d->compact = 1;
}
static int digits(const big_t *m, char *out) { /* decimal em ordem normal; devolve o tamanho */
    char t[128];
    int n = 0;
    big_t c = *m;
    do { t[n++] = (char)('0' + b_div_small(&c, 10)); } while (!b_zero(&c));
    for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
    out[n] = 0;
    return n;
}
int fdn_dec_cmp(const nsdec_t *a, const nsdec_t *b) {
    int an = fdn_dec_isnan(a), bn = fdn_dec_isnan(b);
    if (an || bn) return an && bn ? 0 : an ? -1 : 1;
    big_t A, B;
    from_dec(&A, a); from_dec(&B, b);
    int za = b_zero(&A), zb = b_zero(&B);
    int sa = za ? 0 : a->neg ? -1 : 1, sb = zb ? 0 : b->neg ? -1 : 1;
    if (sa != sb) return sa < sb ? -1 : 1;
    if (!sa) return 0;
    char buf[128];
    int ma = digits(&A, buf) + a->exp, mb = digits(&B, buf) + b->exp, mag;
    if (ma != mb) mag = ma < mb ? -1 : 1;
    else {
        int ea = a->exp, eb = b->exp;
        while (ea > eb) { b_mul_small(&A, 10); ea--; }
        while (eb > ea) { b_mul_small(&B, 10); eb--; }
        mag = b_cmp(&A, &B);
    }
    return sa > 0 ? mag : -mag;
}
int fdn_dec_to_str(const nsdec_t *d, char *out, int cap, const char *sep) {
    char buf[512];
    int n = 0;
    if (!sep || !*sep) sep = ".";
    if (fdn_dec_isnan(d)) n = snprintf(buf, sizeof buf, "NaN");
    else {
        big_t m;
        char dg[128];
        from_dec(&m, d);
        int len = digits(&m, dg), e = b_zero(&m) ? 0 : d->exp;
        if (d->neg && !b_zero(&m)) buf[n++] = '-';
        if (e >= 0) {
            for (int i = 0; i < len; i++) buf[n++] = dg[i];
            if (!(len == 1 && dg[0] == '0')) for (int i = 0; i < e; i++) buf[n++] = '0';
        } else if (-e >= len) {
            buf[n++] = '0';
            for (const char *p = sep; *p; p++) buf[n++] = *p;
            for (int i = 0; i < -e - len; i++) buf[n++] = '0';
            for (int i = 0; i < len; i++) buf[n++] = dg[i];
        } else {
            for (int i = 0; i < len + e; i++) buf[n++] = dg[i];
            for (const char *p = sep; *p; p++) buf[n++] = *p;
            for (int i = len + e; i < len; i++) buf[n++] = dg[i];
        }
        buf[n] = 0;
    }
    int w = 0;
    for (; w < n && w < cap - 1; w++) out[w] = buf[w];
    if (cap > 0) out[w] = 0;
    return w;
}
int fdn_dec_parse(nsdec_t *d, const char *s, const char *sep) {
    const char *p = s;
    int neg = 0, exp = 0, any = 0;
    big_t m;
    bz(&m);
    if (!sep || !*sep) sep = ".";
    int sl = 0;
    while (sep[sl]) sl++;
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    for (int frac = 0;; p++) {
        if (*p >= '0' && *p <= '9') {
            any = 1;
            if (b_bits(&m) < 360) { b_mul_small(&m, 10); b_add_small(&m, (unsigned)(*p - '0')); if (frac) exp--; }
            else if (!frac) exp++;
            continue;
        }
        int is_sep = !frac;
        for (int i = 0; is_sep && i < sl; i++) if (p[i] != sep[i]) is_sep = 0;
        if (is_sep) { frac = 1; p += sl - 1; continue; }
        break;
    }
    if (!any) { fdn_dec_nan(d); return 0; }
    if ((*p == 'e' || *p == 'E') &&
        ((p[1] >= '0' && p[1] <= '9') || ((p[1] == '+' || p[1] == '-') && p[2] >= '0' && p[2] <= '9'))) {
        p++;
        int en = 0, ev = 0;
        if (*p == '+' || *p == '-') en = *p++ == '-';
        while (*p >= '0' && *p <= '9') { if (ev < 100000) ev = ev * 10 + (*p - '0'); p++; }
        exp += en ? -ev : ev;
    }
    reduce(d, &m, exp, neg, DEC_PLAIN, -128);
    return (int)(p - s);
}
double fdn_dec_to_double(const nsdec_t *d) {
    char b[200];
    if (fdn_dec_isnan(d)) return __builtin_nan("");
    big_t m;
    from_dec(&m, d);
    char dg[128];
    digits(&m, dg);
    snprintf(b, sizeof b, "%s%se%d", d->neg ? "-" : "", dg, (int)d->exp);
    return strtod(b, 0);
}
void fdn_dec_from_double(nsdec_t *d, double v) {
    if (v != v || v - v != 0) { fdn_dec_nan(d); return; }
    char b[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(b, sizeof b, "%.*g", prec, v);
        if (strtod(b, 0) == v) break;
    }
    fdn_dec_parse(d, b, ".");
}
long long fdn_dec_to_i64(const nsdec_t *d) {
    if (fdn_dec_isnan(d)) return 0;
    big_t m;
    from_dec(&m, d);
    int e = d->exp;
    for (; e < 0 && !b_zero(&m); e++) b_div_small(&m, 10);
    for (; e > 0 && b_bits(&m) < 64; e--) b_mul_small(&m, 10);
    unsigned long long u = (unsigned long long)m.w[1] << 32 | m.w[0];
    if (b_bits(&m) > 63) u = d->neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL;
    return d->neg ? (long long)(0ULL - u) : (long long)u;
}
unsigned long long fdn_dec_to_u64(const nsdec_t *d) {
    if (fdn_dec_isnan(d)) return 0;
    if (d->neg) return (unsigned long long)fdn_dec_to_i64(d);
    big_t m;
    from_dec(&m, d);
    int e = d->exp;
    for (; e < 0 && !b_zero(&m); e++) b_div_small(&m, 10);
    for (; e > 0 && b_bits(&m) <= 64; e--) b_mul_small(&m, 10);
    if (b_bits(&m) > 64) return ~0ULL;
    return (unsigned long long)m.w[1] << 32 | m.w[0];
}

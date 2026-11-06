/* mOS — motor JavaScript "Brisa": números (conversão exata texto<->double) e funções matemáticas */
#include "js_int.h"

/* ============================================================ inteiros grandes (para conversões exatas) */
#define BIGW 128
typedef struct { u32 w[BIGW]; int n; } big;

static void big_set(big *b, u64 v) {
    b->n = 0;
    while (v) { b->w[b->n++] = (u32)v; v >>= 32; }
}

static void big_mul_small(big *b, u32 m) {
    u64 c = 0;
    for (int i = 0; i < b->n; i++) {
        u64 t = (u64)b->w[i] * m + c;
        b->w[i] = (u32)t;
        c = t >> 32;
    }
    if (c && b->n < BIGW) b->w[b->n++] = (u32)c;
}

static void big_shl(big *b, int bits) {
    if (!b->n) return;
    int words = bits / 32, r = bits % 32;
    if (b->n + words + 1 > BIGW) return;
    if (r) {
        u32 carry = 0;
        for (int i = 0; i < b->n; i++) {
            u32 x = b->w[i];
            b->w[i] = x << r | carry;
            carry = x >> (32 - r);
        }
        if (carry) b->w[b->n++] = carry;
    }
    if (words) {
        for (int i = b->n - 1; i >= 0; i--) b->w[i + words] = b->w[i];
        for (int i = 0; i < words; i++) b->w[i] = 0;
        b->n += words;
    }
}

static void big_pow5(big *b, int e) {
    while (e >= 13) { big_mul_small(b, 1220703125u); e -= 13; }
    u32 m = 1;
    while (e-- > 0) m *= 5;
    if (m > 1) big_mul_small(b, m);
}

static void big_pow10(big *b, int e) { big_pow5(b, e); big_shl(b, e); }

static int big_cmp(const big *a, const big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--) if (a->w[i] != b->w[i]) return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

/* destrutivo: escreve os dígitos decimais (sem zeros à esquerda); devolve a quantidade */
static int big_todec(big *b, char *out) {
    char tmp[1300];
    int k = 0;
    while (b->n) {
        u64 r = 0;
        for (int i = b->n - 1; i >= 0; i--) {
            u64 cur = r << 32 | b->w[i];
            b->w[i] = (u32)(cur / 1000000000u);
            r = cur % 1000000000u;
        }
        while (b->n && !b->w[b->n - 1]) b->n--;
        for (int j = 0; j < 9; j++) { tmp[k++] = (char)('0' + r % 10); r /= 10; if (!b->n && !r) break; }
    }
    if (!k) tmp[k++] = '0';
    while (k > 1 && tmp[k - 1] == '0') k--;
    for (int i = 0; i < k; i++) out[i] = tmp[k - 1 - i];
    return k;
}

static u64 dbits(double d) { union { double d; u64 u; } x; x.d = d; return x.u; }
static double bitsd(u64 u) { union { double d; u64 u; } x; x.u = u; return x.d; }

/* valor exato de mant * 2^e como dígitos decimais; *point = posição da vírgula (valor = 0.D * 10^point) */
static int exact_dec(u64 mant, int e, char *dig, int *point) {
    big b;
    big_set(&b, mant);
    int e10 = 0;
    if (e >= 0) big_shl(&b, e);
    else { big_pow5(&b, -e); e10 = e; }
    int n = big_todec(&b, dig);
    int z = 0;
    while (n - z > 1 && dig[n - 1 - z] == '0') z++;
    n -= z;
    e10 += z;
    *point = n + e10;
    return n;
}

static int dec_cmp(const char *a, int na, int pa, const char *b, int nb, int pb) {
    if (pa != pb) return pa < pb ? -1 : 1;
    int n = MAX(na, nb);
    for (int i = 0; i < n; i++) {
        char x = i < na ? a[i] : '0', y = i < nb ? b[i] : '0';
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

/* arredonda dígitos para k algarismos (meio para cima); devolve nova posição da vírgula */
static int round_digits(const char *d, int n, int point, int k, char *out, bool up_only, bool down_only) {
    if (k >= n) { memcpy(out, d, (usize)n); for (int i = n; i < k; i++) out[i] = '0'; return point; }
    memcpy(out, d, (usize)k);
    bool up = up_only ? true : down_only ? false : d[k] >= '5';
    if (up) {
        int i = k - 1;
        while (i >= 0 && out[i] == '9') { out[i] = '0'; i--; }
        if (i < 0) { out[0] = '1'; for (int j = 1; j < k; j++) out[j] = '0'; return point + 1; }
        out[i]++;
    }
    return point;
}

/* menor representação decimal que lê de volta o mesmo double (d > 0, finito) */
static int shortest(double v, char *out, int *point) {
    u64 bits = dbits(v);
    u64 m = bits & ((1ull << 52) - 1);
    int ex = (int)((bits >> 52) & 0x7FF);
    int e;
    if (ex == 0) e = -1074; else { m |= 1ull << 52; e = ex - 1075; }
    static char D[1300], L[1300], H[1300];
    int pd, pl, ph;
    int nd = exact_dec(m, e, D, &pd);
    int nl, nh;
    if (m == (1ull << 52) && ex > 1) nl = exact_dec(4 * m - 1, e - 2, L, &pl);
    else nl = exact_dec(2 * m - 1, e - 1, L, &pl);
    nh = exact_dec(2 * m + 1, e - 1, H, &ph);
    bool even = (m & 1) == 0;
    char c[24];
    for (int k = 1; k <= 17 && k < nd; k++) {
        bool first_up = D[k] >= '5';
        for (int t = 0; t < 2; t++) {
            bool up = t == 0 ? first_up : !first_up;
            int pc = round_digits(D, nd, pd, k, c, up, !up);
            int a = dec_cmp(L, nl, pl, c, k, pc), b = dec_cmp(c, k, pc, H, nh, ph);
            if ((a < 0 || (even && a == 0)) && (b < 0 || (even && b == 0))) {
                int n = k;
                while (n > 1 && c[n - 1] == '0') n--;
                memcpy(out, c, (usize)n);
                *point = pc;
                return n;
            }
        }
    }
    int n = MIN(nd, 17);
    int pc = round_digits(D, nd, pd, n, out, false, false);
    while (n > 1 && out[n - 1] == '0') n--;
    *point = pc;
    return n;
}

static void put_exp(char *o, int *k, int e) {
    o[(*k)++] = 'e';
    o[(*k)++] = e < 0 ? '-' : '+';
    if (e < 0) e = -e;
    char t[8];
    int n = 0;
    do { t[n++] = (char)('0' + e % 10); e /= 10; } while (e);
    while (n) o[(*k)++] = t[--n];
}

void js_numfmt(double d, char *out, int cap) {
    if (d != d) { strncpy(out, "NaN", (usize)cap); return; }
    if (d == 0) { strncpy(out, "0", (usize)cap); return; }
    if (!m_isfinite(d)) { strncpy(out, d > 0 ? "Infinity" : "-Infinity", (usize)cap); return; }
    char buf[64];
    int k = 0;
    if (d < 0) { buf[k++] = '-'; d = -d; }
    if (d < 9007199254740992.0 && (double)(u64)d == d) {
        u64 v = (u64)d;
        char t[24];
        int n = 0;
        do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (n) buf[k++] = t[--n];
        buf[k] = 0;
        strncpy(out, buf, (usize)cap);
        out[cap - 1] = 0;
        return;
    }
    char s[24];
    int n;
    int kk = shortest(d, s, &n);
    if (kk <= n && n <= 21) {
        for (int i = 0; i < kk; i++) buf[k++] = s[i];
        for (int i = kk; i < n; i++) buf[k++] = '0';
    } else if (0 < n && n <= 21) {
        for (int i = 0; i < n; i++) buf[k++] = s[i];
        buf[k++] = '.';
        for (int i = n; i < kk; i++) buf[k++] = s[i];
    } else if (-6 < n && n <= 0) {
        buf[k++] = '0'; buf[k++] = '.';
        for (int i = 0; i < -n; i++) buf[k++] = '0';
        for (int i = 0; i < kk; i++) buf[k++] = s[i];
    } else {
        buf[k++] = s[0];
        if (kk > 1) { buf[k++] = '.'; for (int i = 1; i < kk; i++) buf[k++] = s[i]; }
        put_exp(buf, &k, n - 1);
    }
    buf[k] = 0;
    strncpy(out, buf, (usize)cap);
    out[cap - 1] = 0;
}

static int exact_of(double d, char *D, int *point) {
    u64 bits = dbits(d);
    u64 m = bits & ((1ull << 52) - 1);
    int ex = (int)((bits >> 52) & 0x7FF);
    int e;
    if (ex == 0) e = -1074; else { m |= 1ull << 52; e = ex - 1075; }
    return exact_dec(m, e, D, point);
}

int js_num_fixed(double d, int frac, char *out, int cap) {
    if (d != d || !m_isfinite(d) || m_abs(d) >= 1e21) { js_numfmt(d, out, cap); return (int)strlen(out); }
    static char D[1300], N[1400];
    char buf[256];
    int k = 0;
    bool neg = d < 0;
    if (neg) d = -d;
    int n = 0, point = 0, nn = 0;
    if (d == 0) { D[0] = '0'; n = 1; point = 1; }
    else n = exact_of(d, D, &point);
    int keep = point + frac;
    if (d == 0 || keep < 0) { N[0] = '0'; nn = 1; }
    else if (keep == 0) { N[0] = D[0] >= '5' ? '1' : '0'; nn = 1; }
    else if (keep >= n) { memcpy(N, D, (usize)n); for (int i = n; i < keep; i++) N[i] = '0'; nn = keep; }
    else {
        int rp = round_digits(D, n, point, keep, N + 1, false, false);
        if (rp != point) { N[0] = '1'; nn = keep + 1; }
        else { memmove(N, N + 1, (usize)keep); nn = keep; }
    }
    if (nn > 200) nn = 200;
    if (neg) buf[k++] = '-';
    if (nn <= frac) {
        buf[k++] = '0';
        if (frac) {
            buf[k++] = '.';
            for (int i = 0; i < frac - nn; i++) buf[k++] = '0';
            for (int i = 0; i < nn; i++) buf[k++] = N[i];
        }
    } else {
        for (int i = 0; i < nn - frac; i++) buf[k++] = N[i];
        if (frac) { buf[k++] = '.'; for (int i = nn - frac; i < nn; i++) buf[k++] = N[i]; }
    }
    buf[k] = 0;
    strncpy(out, buf, (usize)cap);
    out[cap - 1] = 0;
    return k;
}
int js_num_expo(double d, int frac, char *out, int cap) {
    if (d != d || !m_isfinite(d)) { js_numfmt(d, out, cap); return (int)strlen(out); }
    static char D[1300];
    char R[128], buf[160];
    int k = 0;
    if (d < 0) { buf[k++] = '-'; d = -d; }
    int n, point;
    if (d == 0) { D[0] = '0'; n = 1; point = 1; }
    else if (frac < 0) n = shortest(d, D, &point);
    else n = exact_of(d, D, &point);
    int digits = frac < 0 ? n : frac + 1;
    if (digits > 101) digits = 101;
    int rp = d == 0 ? 1 : round_digits(D, n, point, digits, R, false, false);
    buf[k++] = R[0];
    if (digits > 1) { buf[k++] = '.'; for (int i = 1; i < digits; i++) buf[k++] = R[i]; }
    put_exp(buf, &k, d == 0 ? 0 : rp - 1);
    buf[k] = 0;
    strncpy(out, buf, (usize)cap);
    out[cap - 1] = 0;
    return k;
}

int js_num_prec(double d, int prec, char *out, int cap) {
    if (d != d || !m_isfinite(d)) { js_numfmt(d, out, cap); return (int)strlen(out); }
    static char D[1300];
    char R[128], buf[200];
    int k = 0;
    if (d < 0) { buf[k++] = '-'; d = -d; }
    int n, point;
    if (d == 0) { D[0] = '0'; n = 1; point = 1; }
    else n = exact_of(d, D, &point);
    int rp = d == 0 ? 1 : round_digits(D, n, point, prec, R, false, false);
    int e = rp - 1;
    if (e < -6 || e >= prec) {
        buf[k++] = R[0];
        if (prec > 1) { buf[k++] = '.'; for (int i = 1; i < prec; i++) buf[k++] = R[i]; }
        put_exp(buf, &k, e);
    } else if (e >= 0) {
        for (int i = 0; i <= e; i++) buf[k++] = R[i];
        if (prec > e + 1) { buf[k++] = '.'; for (int i = e + 1; i < prec; i++) buf[k++] = R[i]; }
    } else {
        buf[k++] = '0'; buf[k++] = '.';
        for (int i = 0; i < -e - 1; i++) buf[k++] = '0';
        for (int i = 0; i < prec; i++) buf[k++] = R[i];
    }
    buf[k] = 0;
    strncpy(out, buf, (usize)cap);
    out[cap - 1] = 0;
    return k;
}

void js_num_radix(double d, int radix, char *out, int cap) {
    if (radix == 10 || d != d || !m_isfinite(d)) { js_numfmt(d, out, cap); return; }
    char buf[1200];
    int k = 0;
    bool neg = d < 0;
    if (neg) d = -d;
    double ip = m_floor(d), fp = d - ip;
    char t[1100];
    int n = 0;
    if (ip == 0) t[n++] = '0';
    while (ip >= 1 && n < 1090) {
        double q = m_floor(ip / radix);
        int r = (int)(ip - q * radix);
        if (r < 0) r = 0;
        if (r >= radix) r = radix - 1;
        t[n++] = "0123456789abcdefghijklmnopqrstuvwxyz"[r];
        ip = q;
    }
    if (neg) buf[k++] = '-';
    while (n) buf[k++] = t[--n];
    if (fp > 0) {
        buf[k++] = '.';
        for (int i = 0; i < 52 && fp > 0; i++) {
            fp *= radix;
            int dg = (int)fp;
            fp -= dg;
            buf[k++] = "0123456789abcdefghijklmnopqrstuvwxyz"[dg];
        }
    }
    buf[k] = 0;
    strncpy(out, buf, (usize)cap);
    out[cap - 1] = 0;
}

/* ============================================================ texto -> número */
static const double p10[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                             1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

static double scalbn_(double x, int n) {
    while (n > 1000) { x *= 8.98846567431158e307; n -= 1023; }
    while (n < -1000) { x *= 2.2250738585072014e-308; n += 1022; }
    if (n == 0) return x;
    return x * bitsd((u64)(n + 1023) << 52);
}

/* corrige a aproximação z de mant*10^e10 comparando com bignums (arredondamento correto) */
static double refine(u64 mant, int e10, double z) {
    for (int iter = 0; iter < 4; iter++) {
        if (z == 0 || !m_isfinite(z)) return z;
        u64 bits = dbits(z);
        u64 M = bits & ((1ull << 52) - 1);
        int ex = (int)((bits >> 52) & 0x7FF);
        int E;
        if (ex == 0) E = -1074; else { M |= 1ull << 52; E = ex - 1075; }
        /* compara X = mant*10^e10 com Y = (2M+1)*2^(E-1) (ponto médio superior) */
        big X, Y;
        big_set(&X, mant);
        big_set(&Y, 2 * M + 1);
        if (e10 >= 0) big_pow10(&X, e10); else big_pow10(&Y, -e10);
        if (E - 1 >= 0) big_shl(&Y, E - 1); else big_shl(&X, 1 - E);
        int c = big_cmp(&X, &Y);
        if (c > 0 || (c == 0 && (M & 1))) { z = bitsd(bits + 1); continue; }
        /* ponto médio inferior */
        big_set(&X, mant);
        u64 lowm = (M == (1ull << 52) && ex > 1) ? 4 * M - 1 : 2 * M - 1;
        int lowe = (M == (1ull << 52) && ex > 1) ? E - 2 : E - 1;
        big_set(&Y, lowm);
        if (e10 >= 0) big_pow10(&X, e10); else big_pow10(&Y, -e10);
        if (lowe >= 0) big_shl(&Y, lowe); else big_shl(&X, -lowe);
        c = big_cmp(&X, &Y);
        if (c < 0 || (c == 0 && (M & 1))) { z = bitsd(bits - 1); continue; }
        return z;
    }
    return z;
}

double js_strtod(const char *s, int len, int *used) {
    int i = 0;
    u64 mant = 0;
    int nd = 0, e10 = 0;
    bool any = false;
    while (i < len && (isdigit((u8)s[i]) || s[i] == '_')) {
        if (s[i] == '_') { i++; continue; }
        any = true;
        if (nd < 19) { if (mant || s[i] != '0') { mant = mant * 10 + (u64)(s[i] - '0'); if (mant) nd++; } }
        else e10++;
        i++;
    }
    if (i < len && s[i] == '.') {
        i++;
        while (i < len && (isdigit((u8)s[i]) || s[i] == '_')) {
            if (s[i] == '_') { i++; continue; }
            any = true;
            if (nd < 19) { mant = mant * 10 + (u64)(s[i] - '0'); if (mant) nd++; e10--; }
            i++;
        }
    }
    if (!any) { if (used) *used = 0; return jv_n(JS_NAN_BITS); }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        int j = i + 1;
        bool neg = false;
        if (j < len && (s[j] == '+' || s[j] == '-')) { neg = s[j] == '-'; j++; }
        if (j < len && isdigit((u8)s[j])) {
            int ev = 0;
            while (j < len && isdigit((u8)s[j])) { if (ev < 100000) ev = ev * 10 + (s[j] - '0'); j++; }
            e10 += neg ? -ev : ev;
            i = j;
        }
    }
    if (used) *used = i;
    if (!mant) return 0;
    if (e10 + nd > 310) return JS_INF;
    if (e10 + nd < -330) return 0;
    double z;
    if (mant < (1ull << 53) && e10 >= -22 && e10 <= 22) {
        z = (double)mant;
        return e10 >= 0 ? z * p10[e10] : z / p10[-e10];
    }
    z = (double)mant;
    int e = e10;
    while (e > 22) { z *= 1e22; e -= 22; }
    while (e < -22) { z /= 1e22; e += 22; }
    z = e >= 0 ? z * p10[e] : z / p10[-e];
    if (z == 0 && mant) z = 4.9406564584124654e-324;
    if (!m_isfinite(z)) z = 1.7976931348623157e308;
    return refine(mant, e10, z);
}

static bool is_ws(u32 c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0B || c == 0x0C || c == 0xA0 || c == 0xFEFF ||
           c == 0x2028 || c == 0x2029 || c == 0x3000 || (c >= 0x2000 && c <= 0x200A) || c == 0x1680 || c == 0x202F || c == 0x205F;
}

double js_str_to_number(jsstr *str) {
    const char *s = str->d;
    int len = (int)str->len;
    int a = 0, b = len;
    for (;;) { if (a >= b) break; int t = a; u32 c = js_utf8_get(s, b, &t); if (!is_ws(c)) break; a = t; }
    while (b > a) {
        int t = b - 1;
        while (t > a && ((u8)s[t] & 0xC0) == 0x80) t--;
        int q = t;
        u32 c = js_utf8_get(s, b, &q);
        if (!is_ws(c)) break;
        b = t;
    }
    if (a >= b) return 0;
    s += a;
    len = b - a;
    if (len > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X' || s[1] == 'o' || s[1] == 'O' || s[1] == 'b' || s[1] == 'B')) {
        int radix = (s[1] | 32) == 'x' ? 16 : (s[1] | 32) == 'o' ? 8 : 2;
        double v = 0;
        for (int i = 2; i < len; i++) {
            int c = s[i], d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if ((c | 32) >= 'a' && (c | 32) <= 'z') d = (c | 32) - 'a' + 10;
            else return jv_n(JS_NAN_BITS);
            if (d >= radix) return jv_n(JS_NAN_BITS);
            v = v * radix + d;
        }
        return v;
    }
    bool neg = false;
    int i = 0;
    if (s[0] == '+' || s[0] == '-') { neg = s[0] == '-'; i = 1; }
    if (len - i == 8 && !strncmp(s + i, "Infinity", 8)) return neg ? -JS_INF : JS_INF;
    for (int k = i; k < len; k++) if (s[k] == '_') return jv_n(JS_NAN_BITS);
    int used;
    double v = js_strtod(s + i, len - i, &used);
    if (used != len - i || used == 0) return jv_n(JS_NAN_BITS);
    return neg ? -v : v;
}

/* ============================================================ matemática */
double m_abs(double x) { return bitsd(dbits(x) & ~(1ull << 63)); }
bool m_isnan(double x) { return x != x; }
bool m_isfinite(double x) { return ((dbits(x) >> 52) & 0x7FF) != 0x7FF; }

double m_sqrt(double x) {
    double r;
    __asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x));
    return r;
}

double m_trunc(double x) {
    if (!(m_abs(x) < 4503599627370496.0)) return x;
    double t = (double)(i64)x;
    if (t == 0 && x < 0) return -0.0;
    return t;
}

double m_floor(double x) {
    if (!(m_abs(x) < 4503599627370496.0)) return x;
    if (x == 0) return x;
    double t = (double)(i64)x;
    if (t > x) t -= 1;
    return t;
}

double m_ceil(double x) {
    if (!(m_abs(x) < 4503599627370496.0)) return x;
    double t = (double)(i64)x;
    if (t < x) t += 1;
    if (t == 0 && x < 0) return -0.0;
    return t;
}

double m_round(double x) {
    if (!(m_abs(x) < 4503599627370496.0)) return x;
    double r = m_floor(x);
    if (x - r >= 0.5) r += 1;
    if (r == 0 && (x < 0 || (x == 0 && 1 / x < 0))) return -0.0;
    return r;
}

static double ldexp_(double x, int e) { return scalbn_(x, e); }

static int ilogb_(double x) {
    u64 b = dbits(x);
    int ex = (int)((b >> 52) & 0x7FF);
    if (ex == 0) { x *= 18014398509481984.0; b = dbits(x); ex = (int)((b >> 52) & 0x7FF) - 54; }
    return ex - 1023;
}

double m_fmod(double a, double b) {
    if (a != a || b != b || !m_isfinite(a) || b == 0) return jv_n(JS_NAN_BITS);
    if (!m_isfinite(b)) return a;
    if (a == 0) return a;
    double x = m_abs(a), y = m_abs(b);
    if (x < y) return a;
    if (x < 9007199254740992.0 && y < 9007199254740992.0 && (double)(u64)x == x && (double)(u64)y == y) {
        double r = (double)((u64)x % (u64)y);
        return a < 0 ? -r : r;
    }
    while (x >= y) {
        int k = ilogb_(x) - ilogb_(y);
        double t = ldexp_(y, k);
        if (t > x) t = ldexp_(y, k - 1);
        x -= t;
    }
    return a < 0 ? -x : x;
}

static const double ln2_hi = 6.93147180369123816490e-01, ln2_lo = 1.90821492927058770002e-10, inv_ln2 = 1.44269504088896338700e+00;

double m_exp(double x) {
    if (x != x) return x;
    if (x > 709.782712893384) return JS_INF;
    if (x < -745.1332191019411) return 0;
    if (m_abs(x) < 1e-300) return 1 + x;
    int k = (int)(x * inv_ln2 + (x < 0 ? -0.5 : 0.5));
    double hi = x - k * ln2_hi, lo = k * ln2_lo;
    double r = hi - lo;
    double t = r * r;
    double c = r - t * (1.66666666666666019037e-01 + t * (-2.77777777770155933842e-03 + t * (6.61375632143793436117e-05 +
               t * (-1.65339022054652515390e-06 + t * 4.13813679705723846039e-08))));
    double y = 1 - ((lo - (r * c) / (2 - c)) - hi);
    return ldexp_(y, k);
}

double m_log(double x) {
    if (x != x || x < 0) return jv_n(JS_NAN_BITS);
    if (x == 0) return -JS_INF;
    if (!m_isfinite(x)) return x;
    u64 b = dbits(x);
    int k = 0;
    if (((b >> 52) & 0x7FF) == 0) { x *= 18014398509481984.0; k -= 54; b = dbits(x); }
    k += (int)((b >> 52) & 0x7FF) - 1023;
    u64 mb = b & ((1ull << 52) - 1);
    double m = bitsd(mb | (1023ull << 52));   /* [1,2) */
    if (m > 1.4142135623730951) { m *= 0.5; k++; }
    double f = m - 1;
    double s = f / (2 + f), z = s * s, w = z * z;
    double t1 = w * (3.999999999940941908e-01 + w * (2.222219843214978396e-01 + w * 1.531383769920937332e-01));
    double t2 = z * (6.666666666666735130e-01 + w * (2.857142874366239149e-01 + w * (1.818357216161805012e-01 + w * 1.479819860511658591e-01)));
    double R = t2 + t1, hfsq = 0.5 * f * f;
    return k * ln2_hi - ((hfsq - (s * (hfsq + R) + k * ln2_lo)) - f);
}

double m_log2(double x) {
    if (x > 0 && m_isfinite(x)) {
        u64 b = dbits(x);
        if (!(b & ((1ull << 52) - 1)) && ((b >> 52) & 0x7FF)) return (double)((int)((b >> 52) & 0x7FF) - 1023);
    }
    return m_log(x) * 1.44269504088896340736;
}

double m_log10(double x) {
    for (int i = 0; i < 23; i++) if (x == p10[i]) return i;
    return m_log(x) * 0.434294481903251827651;
}

static bool is_int(double x) { return m_isfinite(x) && m_trunc(x) == x; }

double m_pow(double b, double e) {
    if (e != e) return e;
    if (e == 0) return 1;
    if (b != b) return b;
    if (!m_isfinite(e)) {
        double ab = m_abs(b);
        if (ab == 1) return jv_n(JS_NAN_BITS);
        return (ab > 1) == (e > 0) ? JS_INF : 0;
    }
    if (b == 0) {
        bool odd = is_int(e) && m_fmod(m_abs(e), 2) == 1;
        if (e < 0) return odd && 1 / b < 0 ? -JS_INF : JS_INF;
        return odd ? b : 0;
    }
    if (!m_isfinite(b)) {
        bool odd = is_int(e) && m_fmod(m_abs(e), 2) == 1;
        if (b > 0) return e > 0 ? JS_INF : 0;
        if (e > 0) return odd ? -JS_INF : JS_INF;
        return odd ? -0.0 : 0;
    }
    if (is_int(e) && m_abs(e) <= 1024) {
        if (b == 10 && e >= 0 && e <= 22) return p10[(int)e];
        long long n = (long long)m_abs(e);
        double r = 1, x = b;
        while (n) { if (n & 1) r *= x; x *= x; n >>= 1; }
        return e < 0 ? 1 / r : r;
    }
    if (b < 0) {
        if (!is_int(e)) return jv_n(JS_NAN_BITS);
        double r = m_exp(e * m_log(-b));
        return m_fmod(m_abs(e), 2) == 1 ? -r : r;
    }
    return m_exp(e * m_log(b));
}

/* redução para [-pi/4, pi/4]; devolve o quadrante */
static int rem_pio2(double x, double *y) {
    double n = m_round(x * 6.36619772367581382433e-01);
    double r = x - n * 1.57079632673412561417e+00;
    r -= n * 6.07710050630396597660e-11;
    r -= n * 2.02226624879595063154e-21;
    *y = r;
    long long q = (long long)n;
    return (int)(q & 3);
}

static double k_sin(double x) {
    double z = x * x, v = z * x;
    double r = 8.33333333332248946124e-03 + z * (-1.98412698298579493134e-04 + z * (2.75573137070700676789e-06 +
               z * (-2.50507602534068634195e-08 + z * 1.58969099521155010221e-10)));
    return x + v * (-1.66666666666666324348e-01 + z * r);
}

static double k_cos(double x) {
    double z = x * x;
    double r = z * (4.16666666666666019037e-02 + z * (-1.38888888888741095749e-03 + z * (2.48015872894767294178e-05 +
               z * (-2.75573143513906633035e-07 + z * (2.08757232129817482790e-09 + z * -1.13596475577881948265e-11)))));
    double hz = 0.5 * z, w = 1 - hz;
    return w + (((1 - w) - hz) + z * r);
}

double m_sin(double x) {
    if (!m_isfinite(x)) return jv_n(JS_NAN_BITS);
    if (m_abs(x) < 0.7853981633974483) return k_sin(x);
    double y;
    int q = rem_pio2(x, &y);
    switch (q) { case 0: return k_sin(y); case 1: return k_cos(y); case 2: return -k_sin(y); default: return -k_cos(y); }
}

double m_cos(double x) {
    if (!m_isfinite(x)) return jv_n(JS_NAN_BITS);
    if (m_abs(x) < 0.7853981633974483) return k_cos(x);
    double y;
    int q = rem_pio2(x, &y);
    switch (q) { case 0: return k_cos(y); case 1: return -k_sin(y); case 2: return -k_cos(y); default: return k_sin(y); }
}

double m_tan(double x) { return m_sin(x) / m_cos(x); }

double m_atan(double x) {
    static const double hi[] = {4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01, 1.57079632679489655800e+00};
    static const double lo[] = {2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17, 6.12323399573676603587e-17};
    static const double aT[] = {3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
                                -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
                                6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
                                -3.65315727442169155270e-02, 1.62858201153657823623e-02};
    if (x != x) return x;
    bool neg = x < 0;
    double ax = m_abs(x);
    if (ax > 1e19) return neg ? -1.5707963267948966 : 1.5707963267948966;
    int id;
    if (ax < 0.4375) { id = -1; }
    else if (ax < 0.6875) { id = 0; ax = (2 * ax - 1) / (2 + ax); }
    else if (ax < 1.1875) { id = 1; ax = (ax - 1) / (ax + 1); }
    else if (ax < 2.4375) { id = 2; ax = (ax - 1.5) / (1 + 1.5 * ax); }
    else { id = 3; ax = -1 / ax; }
    double z = ax * ax, w = z * z;
    double s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    double s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    double r;
    if (id < 0) r = ax - ax * (s1 + s2);
    else r = hi[id] - ((ax * (s1 + s2) - lo[id]) - ax);
    return neg ? -r : r;
}

double m_atan2(double y, double x) {
    if (x != x || y != y) return jv_n(JS_NAN_BITS);
    const double pi = 3.141592653589793;
    if (x == 0 && y == 0) {
        bool xneg = 1 / x < 0;
        if (xneg) return 1 / y < 0 ? -pi : pi;
        return y;
    }
    if (!m_isfinite(x) && !m_isfinite(y)) {
        double r = x > 0 ? pi / 4 : 3 * pi / 4;
        return y < 0 ? -r : r;
    }
    if (x == 0) return y > 0 ? pi / 2 : -pi / 2;
    if (!m_isfinite(x)) {
        if (x > 0) return y < 0 || (y == 0 && 1 / y < 0) ? -0.0 : 0.0;
        return y < 0 || (y == 0 && 1 / y < 0) ? -pi : pi;
    }
    if (!m_isfinite(y)) return y > 0 ? pi / 2 : -pi / 2;
    double a = m_atan(m_abs(y / x));
    if (x > 0) return y < 0 || (y == 0 && 1 / y < 0) ? -a : a;
    return y < 0 || (y == 0 && 1 / y < 0) ? -(pi - a) : pi - a;
}

double m_asin(double x) {
    if (!(x >= -1 && x <= 1)) return jv_n(JS_NAN_BITS);
    return m_atan2(x, m_sqrt((1 - x) * (1 + x)));
}

double m_acos(double x) {
    if (!(x >= -1 && x <= 1)) return jv_n(JS_NAN_BITS);
    return m_atan2(m_sqrt((1 - x) * (1 + x)), x);
}

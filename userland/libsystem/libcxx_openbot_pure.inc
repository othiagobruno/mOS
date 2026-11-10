/* Núcleo testável no host: string (layout libc++ x86_64), path POSIX, to_chars,
 * stoi/stod, sort, align e um demangler Itanium. Sem I/O do sistema. */
#ifndef LIBCXX_OPENBOT_PURE_INC
#define LIBCXX_OPENBOT_PURE_INC

#include <stddef.h>
#include <stdint.h>
#ifdef CX_HOST
#include <string.h>
#include <stdlib.h>
#else
void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strlen(const char *s);
long long strtoll(const char *s, char **end, int base);
double strtod(const char *s, char **end);
#endif

typedef struct { uint64_t w0, size; char *data; } ob_str;
typedef struct { uint64_t w0, size; uint32_t *data; } ob_wstr;

static int ob_slong(const ob_str *s) { return ((const unsigned char *)s)[0] & 1; }
static size_t ob_ssize(const ob_str *s) { return ob_slong(s) ? (size_t)s->size : ((const unsigned char *)s)[0] >> 1; }
static char *ob_sdata(ob_str *s) { return ob_slong(s) ? s->data : (char *)s + 1; }
static const char *ob_scdata(const ob_str *s) { return ob_sdata((ob_str *)s); }
static size_t ob_scap(const ob_str *s) { return ob_slong(s) ? (size_t)((s->w0 & ~1ull) - 1) : 22; }
static void ob_sset(ob_str *s, size_t n) {
    if (ob_slong(s)) s->size = n; else ((unsigned char *)s)[0] = (unsigned char)(n << 1);
    ob_sdata(s)[n] = 0;
}
static void ob_sempty(ob_str *s) { s->w0 = 0; s->size = 0; s->data = 0; }
static void ob_sreserve(ob_str *s, size_t n) {
    size_t cap = ob_scap(s);
    size_t want, alloc, len;
    char *nb;
    if (n <= cap) return;
    want = n > 2 * cap ? n : 2 * cap;
    alloc = (want + 1 + 15) & ~(size_t)15;
    nb = (char *)malloc(alloc);
    len = ob_ssize(s);
    memcpy(nb, ob_sdata(s), len + 1);
    if (ob_slong(s)) free(s->data);
    s->w0 = alloc | 1;
    s->size = len;
    s->data = nb;
}
static void ob_sassign_n(ob_str *s, const char *p, size_t n) {
    char *tmp = 0;
    if (p && n && p >= ob_scdata(s) && p <= ob_scdata(s) + ob_ssize(s)) {
        tmp = (char *)malloc(n ? n : 1);
        memcpy(tmp, p, n);
        p = tmp;
    }
    ob_sreserve(s, n);
    if (n) memmove(ob_sdata(s), p, n);
    ob_sset(s, n);
    free(tmp);
}
static void ob_sappend_n(ob_str *s, const char *p, size_t n) {
    size_t len = ob_ssize(s);
    char stack[64], *src = (char *)p, *heap = 0;
    if (p && p >= ob_sdata(s) && p <= ob_sdata(s) + len && len + n > ob_scap(s)) {
        src = n <= sizeof stack ? stack : (heap = (char *)malloc(n ? n : 1));
        memcpy(src, p, n);
    }
    ob_sreserve(s, len + n);
    if (n) memmove(ob_sdata(s) + len, src, n);
    ob_sset(s, len + n);
    free(heap);
}
static void ob_sfree(ob_str *s) { if (ob_slong(s)) free(s->data); ob_sempty(s); }
static void ob_scopy(ob_str *d, const ob_str *s) { ob_sempty(d); ob_sassign_n(d, ob_scdata(s), ob_ssize(s)); }
static int ob_scmp_n(const ob_str *s, const char *p, size_t n) {
    size_t sn = ob_ssize(s), m = sn < n ? sn : n;
    int c = m ? memcmp(ob_scdata(s), p, m) : 0;
    if (c) return c;
    return sn < n ? -1 : sn > n ? 1 : 0;
}
static size_t ob_sfind(const ob_str *s, char c, size_t pos) {
    size_t n = ob_ssize(s);
    const char *p;
    if (pos > n) pos = n;
    p = (const char *)memchr(ob_scdata(s) + pos, c, n - pos);
    return p ? (size_t)(p - ob_scdata(s)) : (size_t)-1;
}
static size_t ob_srfind(const ob_str *s, char c, size_t pos) {
    size_t n = ob_ssize(s), i;
    const char *p = ob_scdata(s);
    if (n == 0) return (size_t)-1;
    if (pos >= n) pos = n - 1;
    for (i = pos + 1; i-- > 0; ) if (p[i] == c) return i;
    return (size_t)-1;
}

static int ob_wlong(const ob_wstr *s) { return ((const unsigned char *)s)[0] & 1; }
static size_t ob_wsize(const ob_wstr *s) { return ob_wlong(s) ? (size_t)s->size : ((const unsigned char *)s)[0] >> 1; }
static uint32_t *ob_wdata(ob_wstr *s) { return ob_wlong(s) ? s->data : (uint32_t *)((char *)s + 4); }
static size_t ob_wcap(const ob_wstr *s) { return ob_wlong(s) ? (size_t)((s->w0 & ~1ull) - 1) : 4; }
static void ob_wset(ob_wstr *s, size_t n) {
    if (ob_wlong(s)) s->size = n; else ((unsigned char *)s)[0] = (unsigned char)(n << 1);
    ob_wdata(s)[n] = 0;
}
static void ob_wreserve(ob_wstr *s, size_t n) {
    size_t cap = ob_wcap(s), want, alloc, len;
    uint32_t *nb;
    if (n <= cap) return;
    want = n > 2 * cap ? n : 2 * cap;
    alloc = (want + 1 + 3) & ~(size_t)3;
    nb = (uint32_t *)malloc(alloc * sizeof(uint32_t));
    len = ob_wsize(s);
    memcpy(nb, ob_wdata(s), (len + 1) * sizeof(uint32_t));
    if (ob_wlong(s)) free(s->data);
    s->w0 = alloc | 1;
    s->size = len;
    s->data = nb;
}
static void ob_wappend_n(ob_wstr *s, const uint32_t *p, size_t n) {
    size_t len = ob_wsize(s);
    ob_wreserve(s, len + n);
    if (n) memcpy(ob_wdata(s) + len, p, n * sizeof(uint32_t));
    ob_wset(s, len + n);
}

/* ---- path (POSIX: root-name vazio) ---- */
static int ob_is_sep(char c) { return c == '/'; }
static void ob_path_root_dir(ob_str *d, const ob_str *p) {
    const char *s = ob_scdata(p);
    ob_sempty(d);
    if (ob_ssize(p) && ob_is_sep(s[0])) ob_sassign_n(d, "/", 1);
}
static void ob_path_filename(ob_str *d, const ob_str *p) {
    const char *s = ob_scdata(p);
    size_t n = ob_ssize(p), i = n;
    ob_sempty(d);
    if (!n) return;
    if (n == 1 && ob_is_sep(s[0])) return;
    while (i && !ob_is_sep(s[i - 1])) i--;
    if (i == n) return;                 /* termina em separador: filename vazio */
    ob_sassign_n(d, s + i, n - i);
}
static void ob_path_extension(ob_str *d, const ob_str *p) {
    ob_str fn;
    const char *s;
    size_t n, i;
    ob_sempty(&fn);
    ob_path_filename(&fn, p);
    s = ob_scdata(&fn); n = ob_ssize(&fn);
    ob_sempty(d);
    if (n <= 1 || (n == 2 && s[0] == '.' && s[1] == '.')) { ob_sfree(&fn); return; }
    for (i = n; i-- > 1; ) if (s[i] == '.') { ob_sassign_n(d, s + i, n - i); break; }
    ob_sfree(&fn);
}
static void ob_path_parent(ob_str *d, const ob_str *p) {
    const char *s = ob_scdata(p);
    size_t n = ob_ssize(p), i;
    ob_sempty(d);
    if (!n) return;
    i = n;
    while (i && ob_is_sep(s[i - 1])) i--;
    while (i && !ob_is_sep(s[i - 1])) i--;
    if (!i) return;
    while (i > 1 && ob_is_sep(s[i - 1])) i--;
    ob_sassign_n(d, s, i);
}
static void ob_path_relative(ob_str *d, const ob_str *p) {
    const char *s = ob_scdata(p);
    size_t n = ob_ssize(p), i = 0;
    ob_sempty(d);
    if (n && ob_is_sep(s[0])) i = 1;
    ob_sassign_n(d, s + i, n - i);
}
static void ob_path_root_name(ob_str *d, const ob_str *p) { (void)p; ob_sempty(d); }

static void ob_path_normal(ob_str *d, const ob_str *p) {
    const char *s = ob_scdata(p);
    size_t n = ob_ssize(p), i = 0;
    int abs = n && ob_is_sep(s[0]);
    char *parts[256];
    size_t plen[256];
    int np = 0, k;
    ob_sempty(d);
    while (i < n) {
        size_t b;
        while (i < n && ob_is_sep(s[i])) i++;
        if (i >= n) break;
        b = i;
        while (i < n && !ob_is_sep(s[i])) i++;
        if (i - b == 1 && s[b] == '.') continue;
        if (i - b == 2 && s[b] == '.' && s[b + 1] == '.') {
            if (np && !(plen[np - 1] == 2 && parts[np - 1][0] == '.' && parts[np - 1][1] == '.')) { free(parts[--np]); continue; }
            if (abs) continue;
        }
        if (np < 256) {
            parts[np] = (char *)malloc(i - b + 1);
            memcpy(parts[np], s + b, i - b);
            parts[np][i - b] = 0;
            plen[np] = i - b;
            np++;
        }
    }
    if (abs) ob_sappend_n(d, "/", 1);
    for (k = 0; k < np; k++) {
        if (ob_ssize(d) && ob_scdata(d)[ob_ssize(d) - 1] != '/') ob_sappend_n(d, "/", 1);
        ob_sappend_n(d, parts[k], plen[k]);
        free(parts[k]);
    }
    if (!ob_ssize(d)) ob_sassign_n(d, abs ? "/" : ".", abs ? 1 : 1);
}

static void ob_path_relative_to(ob_str *d, const ob_str *p, const ob_str *base) {
    const char *a = ob_scdata(p), *b = ob_scdata(base);
    size_t na = ob_ssize(p), nb = ob_ssize(base), i = 0, ja, jb;
    int abs_a = na && a[0] == '/', abs_b = nb && b[0] == '/';
    ob_sempty(d);
    if (abs_a != abs_b) return;
    while (i < na && i < nb && a[i] == b[i]) i++;
    if (i < na && i < nb) {
        while (i && a[i - 1] != '/') i--;
    } else if (i == na && i < nb && b[i] != '/') {
        while (i && a[i - 1] != '/') i--;
    } else if (i == nb && i < na && a[i] != '/') {
        while (i && a[i - 1] != '/') i--;
    }
    ja = i; jb = i;
    while (jb < nb) {
        size_t e;
        while (jb < nb && b[jb] == '/') jb++;
        if (jb >= nb) break;
        e = jb;
        while (e < nb && b[e] != '/') e++;
        if (!(e - jb == 1 && b[jb] == '.')) {
            if (e - jb == 2 && b[jb] == '.' && b[jb + 1] == '.') { ob_sfree(d); ob_sempty(d); return; }
            if (ob_ssize(d)) ob_sappend_n(d, "/", 1);
            ob_sappend_n(d, "..", 2);
        }
        jb = e;
    }
    while (ja < na) {
        size_t e;
        while (ja < na && a[ja] == '/') ja++;
        if (ja >= na) break;
        e = ja;
        while (e < na && a[e] != '/') e++;
        if (!(e - ja == 1 && a[ja] == '.')) {
            if (ob_ssize(d)) ob_sappend_n(d, "/", 1);
            ob_sappend_n(d, a + ja, e - ja);
        }
        ja = e;
    }
}

/* iterator: stashed 0, path* 24, view 32, state 48 */
typedef struct {
    ob_str stashed;
    const ob_str *path;
    const char *entry;
    uint64_t entry_n;
    int state;
} ob_piter;

static void ob_piter_set(ob_piter *it, const ob_str *p, size_t b, size_t e, int st) {
    memset(it, 0, sizeof *it);
    it->path = p;
    it->entry = ob_scdata(p) + b;
    it->entry_n = e - b;
    it->state = st;
    ob_sassign_n(&it->stashed, it->entry, (size_t)it->entry_n);
}
static void ob_path_begin(ob_piter *it, const ob_str *p) {
    const char *s = ob_scdata(p);
    size_t n = ob_ssize(p), i = 0;
    if (!n) { memset(it, 0, sizeof *it); it->path = p; it->state = 4; it->entry = s; return; }
    if (s[0] == '/') { ob_piter_set(it, p, 0, 1, 2); return; }
    while (i < n && s[i] != '/') i++;
    ob_piter_set(it, p, 0, i, 3);
}
static void ob_path_end(ob_piter *it, const ob_str *p) {
    memset(it, 0, sizeof *it);
    it->path = p;
    it->entry = ob_scdata(p) + ob_ssize(p);
    it->state = 4;
}
static void ob_piter_inc(ob_piter *it) {
    const ob_str *p = it->path;
    const char *s = ob_scdata(p);
    size_t n = ob_ssize(p), i;
    if (!p || it->state == 4) return;
    i = (size_t)(it->entry - s) + (size_t)it->entry_n;
    if (it->state == 2) {
        /* root "/" consumido */
    }
    if (i >= n) { ob_path_end(it, p); return; }
    if (s[i] == '/') {
        size_t j = i;
        while (j < n && s[j] == '/') j++;
        if (j >= n) {
            /* barra final: elemento vazio, depois o fim */
            if (it->entry_n == 0 && it->state == 3) { ob_path_end(it, p); return; }
            ob_piter_set(it, p, n, n, 3);
            return;
        }
        i = j;
    }
    {
        size_t e = i;
        while (e < n && s[e] != '/') e++;
        ob_piter_set(it, p, i, e, 3);
    }
}

/* ---- números ---- */
typedef struct { char *ptr; int ec; } ob_tcr;

static int ob_write(char *b, char *e, char **o, const char *s, int n) {
    if (*o + n > e) return 0;
    memcpy(*o, s, (size_t)n);
    *o += n;
    return 1;
}
static int ob_u64(char *b, char *e, char **o, uint64_t v) {
    char tmp[32];
    int n = 0;
    if (!v) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n) if (!ob_write(b, e, o, &tmp[--n], 1)) return 0;
    return 1;
}

static ob_tcr ob_to_chars_f(char *first, char *last, double v, int fmt, int prec, int has_prec) {
    ob_tcr r;
    char *o = first;
    int neg = 0, sci, pr, exp10 = 0, i;
    double x, scale;
    uint64_t ip, frac;
    r.ptr = last; r.ec = 84;
    if (first >= last) return r;
    if (v != v) { if (!ob_write(first, last, &o, "nan", 3)) return r; r.ptr = o; r.ec = 0; return r; }
    if (v < 0) { neg = 1; v = -v; }
    if (v > 1e300) { if (!ob_write(first, last, &o, neg ? "-inf" : "inf", neg ? 4 : 3)) return r; r.ptr = o; r.ec = 0; return r; }
    if (!has_prec) prec = 6;
    if (prec < 0) prec = 6;
    if (prec > 18) prec = 18;
    if (fmt == 0) fmt = 3;
    x = v;
    if (x != 0) {
        while (x >= 10 && exp10 < 400) { x /= 10; exp10++; }
        while (x < 1 && exp10 > -400) { x *= 10; exp10--; }
    }
    sci = (fmt & 1) && !(fmt & 2);
    if (fmt == 3) sci = exp10 < -4 || exp10 >= prec;
    if (fmt == 4) {
        /* hexfloat aproximado em double */
        union { double d; uint64_t u; } u;
        int be;
        uint64_t mant;
        char hex[32];
        int hn = 0;
        u.d = neg ? -v : v;
        if (!ob_write(first, last, &o, neg ? "-0x" : "0x", neg ? 3 : 2)) return r;
        if ((u.u & 0x7ff0000000000000ull) == 0) { if (!ob_write(first, last, &o, "0p+0", 4)) return r; r.ptr = o; r.ec = 0; return r; }
        be = (int)((u.u >> 52) & 0x7ff) - 1023;
        mant = (u.u & 0xfffffffffffffull) | 0x10000000000000ull;
        if (!ob_write(first, last, &o, "1", 1)) return r;
        if (mant & 0xfffffffffffffull) {
            if (!ob_write(first, last, &o, ".", 1)) return r;
            mant &= 0xfffffffffffffull;
            for (i = 0; i < 13 && mant; i++) {
                int d = (int)((mant >> 48) & 0xf);
                hex[hn++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
                mant = (mant << 4) & 0xfffffffffffffull;
            }
            while (hn && hex[hn - 1] == '0') hn--;
            if (!ob_write(first, last, &o, hex, hn)) return r;
        }
        if (!ob_write(first, last, &o, "p", 1)) return r;
        if (be < 0) { if (!ob_write(first, last, &o, "-", 1)) return r; be = -be; }
        else if (!ob_write(first, last, &o, "+", 1)) return r;
        if (!ob_u64(first, last, &o, (uint64_t)be)) return r;
        r.ptr = o; r.ec = 0; return r;
    }
    pr = sci ? prec : (fmt == 2 && has_prec ? prec : (prec > exp10 ? prec - (exp10 < 0 ? 0 : exp10) - (fmt == 2 ? 0 : 1) : 0));
    if (fmt == 2 && has_prec) {
        /* fixed: prec casas depois da vírgula */
        pr = prec;
        scale = 1;
        for (i = 0; i < pr; i++) scale *= 10;
        v = v * scale + 0.5;
        ip = (uint64_t)(v / scale);
        frac = (uint64_t)(v - (double)ip * scale);
        if (neg && !ob_write(first, last, &o, "-", 1)) return r;
        if (!ob_u64(first, last, &o, ip)) return r;
        if (pr) {
            char dig[24];
            if (!ob_write(first, last, &o, ".", 1)) return r;
            for (i = pr - 1; i >= 0; i--) { dig[i] = (char)('0' + (frac % 10)); frac /= 10; }
            if (!ob_write(first, last, &o, dig, pr)) return r;
        }
        r.ptr = o; r.ec = 0; return r;
    }
    /* general / scientific a partir da mantissa x em [1,10) */
    {
        int nd = prec > 0 ? prec : 1;
        double mant = x;
        uint64_t digits;
        int use_exp = sci;
        int k;
        scale = 1;
        for (i = 1; i < nd; i++) scale *= 10;
        mant = mant * scale + 0.5;
        if (mant >= scale * 10) { mant /= 10; exp10++; }
        digits = (uint64_t)mant;
        if (neg && !ob_write(first, last, &o, "-", 1)) return r;
        if (!use_exp) {
            if (exp10 >= 0 && exp10 < nd) {
                char dig[24];
                int n = 0;
                uint64_t t = digits;
                if (!t) dig[n++] = '0';
                while (t) { dig[n++] = (char)('0' + t % 10); t /= 10; }
                while (n < nd) dig[n++] = '0';
                /* dig está invertido, n == nd */
                for (k = 0; k < n / 2; k++) { char c = dig[k]; dig[k] = dig[n - 1 - k]; dig[n - 1 - k] = c; }
                if (!ob_write(first, last, &o, dig, exp10 + 1)) return r;
                while (nd > exp10 + 1 && dig[nd - 1] == '0') nd--;
                if (nd > exp10 + 1) {
                    if (!ob_write(first, last, &o, ".", 1)) return r;
                    if (!ob_write(first, last, &o, dig + exp10 + 1, nd - exp10 - 1)) return r;
                }
            } else if (exp10 < 0) {
                char dig[24];
                int n = 0;
                uint64_t t = digits;
                int zeros = -exp10 - 1;
                if (!ob_write(first, last, &o, "0.", 2)) return r;
                for (k = 0; k < zeros; k++) if (!ob_write(first, last, &o, "0", 1)) return r;
                if (!t) dig[n++] = '0';
                while (t) { dig[n++] = (char)('0' + t % 10); t /= 10; }
                while (n < nd) dig[n++] = '0';
                for (k = 0; k < n / 2; k++) { char c = dig[k]; dig[k] = dig[n - 1 - k]; dig[n - 1 - k] = c; }
                while (n && dig[n - 1] == '0') n--;
                if (!ob_write(first, last, &o, dig, n)) return r;
            } else use_exp = 1;
        }
        if (use_exp) {
            char dig[24];
            int n = 0;
            uint64_t t = digits;
            if (!t) dig[n++] = '0';
            while (t) { dig[n++] = (char)('0' + t % 10); t /= 10; }
            while (n < nd) dig[n++] = '0';
            for (k = 0; k < n / 2; k++) { char c = dig[k]; dig[k] = dig[n - 1 - k]; dig[n - 1 - k] = c; }
            if (!ob_write(first, last, &o, dig, 1)) return r;
            while (n > 1 && dig[n - 1] == '0') n--;
            if (n > 1) {
                if (!ob_write(first, last, &o, ".", 1)) return r;
                if (!ob_write(first, last, &o, dig + 1, n - 1)) return r;
            }
            if (!ob_write(first, last, &o, "e", 1)) return r;
            if (exp10 < 0) { if (!ob_write(first, last, &o, "-", 1)) return r; exp10 = -exp10; }
            else if (!ob_write(first, last, &o, "+", 1)) return r;
            if (exp10 < 10 && !ob_write(first, last, &o, "0", 1)) return r;
            if (!ob_u64(first, last, &o, (uint64_t)exp10)) return r;
        }
    }
    r.ptr = o; r.ec = 0; return r;
}

enum { OB_OK = 0, OB_INVAL = 1, OB_RANGE = 2 };
static int ob_stoi(const char *s, size_t n, size_t *idx, int base, long long *out, long long lo, long long hi) {
    char buf[128], *end = 0;
    long long v;
    size_t i = 0;
    if (n >= sizeof buf) n = sizeof buf - 1;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    if (i >= n) return OB_INVAL;
    memcpy(buf, s + i, n - i); buf[n - i] = 0;
    v = strtoll(buf, &end, base);
    if (end == buf) return OB_INVAL;
    if (v < lo || v > hi) return OB_RANGE;
    if (idx) *idx = i + (size_t)(end - buf);
    *out = v;
    return OB_OK;
}
static int ob_stod(const char *s, size_t n, size_t *idx, double *out) {
    char buf[256], *end = 0;
    size_t i = 0;
    if (n >= sizeof buf) n = sizeof buf - 1;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i >= n) return OB_INVAL;
    memcpy(buf, s + i, n - i); buf[n - i] = 0;
    *out = strtod(buf, &end);
    if (end == buf) return OB_INVAL;
    if (idx) *idx = i + (size_t)(end - buf);
    return OB_OK;
}

static void *ob_align(size_t al, size_t sz, void **ptr, size_t *space) {
    uintptr_t p, a;
    size_t pad;
    if (!al || (al & (al - 1))) return 0;
    p = (uintptr_t)*ptr;
    a = (p + al - 1) & ~(uintptr_t)(al - 1);
    pad = (size_t)(a - p);
    if (sz > *space || pad > *space - sz) return 0;
    *ptr = (void *)a;
    *space -= pad;
    return *ptr;
}

#define OB_SORT(name, T) \
static void name(T *a, T *b) { \
    T *i, *j, key; \
    for (i = a + 1; i < b; i++) { \
        key = *i; j = i; \
        while (j > a && *(j - 1) > key) { *j = *(j - 1); j--; } \
        *j = key; \
    } \
}
OB_SORT(ob_sort_c, char)
OB_SORT(ob_sort_f, float)
OB_SORT(ob_sort_i, int)
OB_SORT(ob_sort_m, unsigned long)
OB_SORT(ob_sort_x, long long)

/* demangler Itanium suficiente para nomes aninhados, tipos básicos e templates simples.
 * Devolve NULL se o nome não for reconhecido (contrato de __cxa_demangle). */
static const char *ob_ty(char c) {
    switch (c) {
    case 'v': return "void"; case 'b': return "bool"; case 'c': return "char";
    case 'a': return "signed char"; case 'h': return "unsigned char";
    case 's': return "short"; case 't': return "unsigned short";
    case 'i': return "int"; case 'j': return "unsigned int";
    case 'l': return "long"; case 'm': return "unsigned long";
    case 'x': return "long long"; case 'y': return "unsigned long long";
    case 'f': return "float"; case 'd': return "double"; case 'e': return "long double";
    case 'n': return "__int128"; case 'w': return "wchar_t";
    default: return 0;
    }
}
static int ob_srcname(const char **pp, char *o, size_t cap, size_t *n) {
    const char *p = *pp;
    unsigned len = 0;
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') { len = len * 10 + (unsigned)(*p - '0'); p++; }
    if (len >= cap - *n) return 0;
    memcpy(o + *n, p, len);
    *n += len;
    *pp = p + len;
    return 1;
}
static int ob_type(const char **pp, char *o, size_t cap, size_t *n);
static int ob_put(char *o, size_t cap, size_t *n, const char *s) {
    size_t k = strlen(s);
    if (*n + k >= cap) return 0;
    memcpy(o + *n, s, k);
    *n += k;
    return 1;
}
static int ob_nested(const char **pp, char *o, size_t cap, size_t *n) {
    const char *p = *pp;
    int first = 1;
    if (*p == 'N') p++;
    while (*p && *p != 'E') {
        if (*p == 'S') {
            p++;
            if (*p == 't') { if (!ob_put(o, cap, n, "std")) return 0; p++; }
            else if (*p == '_') p++;
            else return 0;
        } else if (*p >= '0' && *p <= '9') {
            if (!first && !ob_put(o, cap, n, "::")) return 0;
            if (!ob_srcname(&p, o, cap, n)) return 0;
        } else if (*p == 'I') {
            p++;
            if (!ob_put(o, cap, n, "<")) return 0;
            while (*p && *p != 'E') {
                if (!ob_type(&p, o, cap, n)) return 0;
                if (*p != 'E') { if (!ob_put(o, cap, n, ", ")) return 0; }
            }
            if (*p != 'E') return 0;
            p++;
            if (!ob_put(o, cap, n, ">")) return 0;
        } else return 0;
        first = 0;
    }
    if (*p != 'E') return 0;
    *pp = p + 1;
    return 1;
}
static int ob_type(const char **pp, char *o, size_t cap, size_t *n) {
    const char *p = *pp;
    const char *t;
    if (*p == 'P' || *p == 'R' || *p == 'O') {
        char k = *p++;
        if (!ob_type(&p, o, cap, n)) return 0;
        if (!ob_put(o, cap, n, k == 'P' ? "*" : k == 'R' ? "&" : "&&")) return 0;
        *pp = p; return 1;
    }
    if (*p == 'N' || (*p >= '0' && *p <= '9')) {
        if (*p != 'N') {
            if (!ob_srcname(&p, o, cap, n)) return 0;
            *pp = p; return 1;
        }
        if (!ob_nested(&p, o, cap, n)) return 0;
        *pp = p; return 1;
    }
    t = ob_ty(*p);
    if (!t) return 0;
    p++;
    if (!ob_put(o, cap, n, t)) return 0;
    *pp = p;
    return 1;
}
static char *ob_demangle(const char *mangled) {
    const char *p = mangled;
    char buf[1024];
    size_t n = 0;
    char *out;
    int args = 0;
    if (!p || p[0] != '_' || p[1] != 'Z') return 0;
    p += 2;
    if (*p == 'N') { if (!ob_nested(&p, buf, sizeof buf, &n)) return 0; }
    else if (*p >= '0' && *p <= '9') { if (!ob_srcname(&p, buf, sizeof buf, &n)) return 0; }
    else if (*p == 'S' && p[1] == 't') { if (!ob_put(buf, sizeof buf, &n, "std")) return 0; p += 2; }
    else return 0;
    if (*p) {
        if (!ob_put(buf, sizeof buf, &n, "(")) return 0;
        while (*p) {
            if (args && !ob_put(buf, sizeof buf, &n, ", ")) return 0;
            if (!ob_type(&p, buf, sizeof buf, &n)) return 0;
            args = 1;
        }
        if (!ob_put(buf, sizeof buf, &n, ")")) return 0;
    }
    buf[n] = 0;
    out = (char *)malloc(n + 1);
    if (out) memcpy(out, buf, n + 1);
    return out;
}

#endif

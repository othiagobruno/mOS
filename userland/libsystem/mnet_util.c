/* mOS — mnet: memória crescente, cabeçalhos, texto sem caixa e datas HTTP (ver mnet.h). */
#include "mnet.h"

int mnet_buf_put(mnet_buf_t *b, const void *d, size_t n) {
    if (!n) return 0;
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 256;
        while (c < b->n + n + 1) c *= 2;
        uint8_t *p = realloc(b->p, c);
        if (!p) return -1;
        b->p = p; b->cap = c;
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
    b->p[b->n] = 0;                         /* sempre terminado em zero (não conta em n) */
    return 0;
}
int mnet_buf_puts(mnet_buf_t *b, const char *s) { return mnet_buf_put(b, s, strlen(s)); }
int mnet_buf_putc(mnet_buf_t *b, int c) { uint8_t x = (uint8_t)c; return mnet_buf_put(b, &x, 1); }
int mnet_buf_putu(mnet_buf_t *b, unsigned long long v) {
    char t[24]; int i = 24;
    do { t[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    return mnet_buf_put(b, t + i, (size_t)(24 - i));
}
void mnet_buf_free(mnet_buf_t *b) { free(b->p); b->p = 0; b->n = b->cap = 0; }
void mnet_buf_consume(mnet_buf_t *b, size_t n) {
    if (n >= b->n) { b->n = 0; if (b->p) b->p[0] = 0; return; }
    memmove(b->p, b->p + n, b->n - n);
    b->n -= n;
    b->p[b->n] = 0;
}

int mnet_lc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int mnet_ieqn(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (mnet_lc((unsigned char)a[i]) != mnet_lc((unsigned char)b[i])) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}
int mnet_ieq(const char *a, const char *b) {
    for (;; a++, b++) {
        if (mnet_lc((unsigned char)*a) != mnet_lc((unsigned char)*b)) return 0;
        if (!*a) return 1;
    }
}
char *mnet_strndup(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) return 0;
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}
static int ows(int c) { return c == ' ' || c == '\t'; }
int mnet_tok_has(const char *list, const char *tok) {
    size_t tl = strlen(tok);
    const char *p = list;
    while (p && *p) {
        while (*p == ',' || ows(*p)) p++;
        const char *s = p;
        while (*p && *p != ',' && *p != ';') p++;
        const char *e = p;
        while (e > s && ows(e[-1])) e--;
        if ((size_t)(e - s) == tl && mnet_ieqn(s, tok, tl)) return 1;
        while (*p && *p != ',') p++;
    }
    return 0;
}

/* ---- cabeçalhos */
int mnet_hdrs_add(mnet_hdrs_t *h, const char *name, size_t nl, const char *value, size_t vl) {
    if (h->n == h->cap) {
        int c = h->cap ? h->cap * 2 : 16;
        mnet_hdr_t *v = realloc(h->v, (size_t)c * sizeof *v);
        if (!v) return -1;
        h->v = v; h->cap = c;
    }
    char *n = mnet_strndup(name, nl), *v = mnet_strndup(value, vl);
    if (!n || !v) { free(n); free(v); return -1; }
    h->v[h->n].name = n;
    h->v[h->n].value = v;
    h->n++;
    return 0;
}
void mnet_hdrs_del(mnet_hdrs_t *h, const char *name) {
    int w = 0;
    for (int i = 0; i < h->n; i++) {
        if (mnet_ieq(h->v[i].name, name)) { free(h->v[i].name); free(h->v[i].value); continue; }
        h->v[w++] = h->v[i];
    }
    h->n = w;
}
int mnet_hdrs_set(mnet_hdrs_t *h, const char *name, const char *value) {
    mnet_hdrs_del(h, name);
    return mnet_hdrs_add(h, name, strlen(name), value, strlen(value));
}
const char *mnet_hdrs_get(const mnet_hdrs_t *h, const char *name) {
    for (int i = 0; i < h->n; i++)
        if (mnet_ieq(h->v[i].name, name)) return h->v[i].value;
    return 0;
}
char *mnet_hdrs_join(const mnet_hdrs_t *h, const char *name) {
    mnet_buf_t b = {0};
    int any = 0;
    for (int i = 0; i < h->n; i++) {
        if (!mnet_ieq(h->v[i].name, name)) continue;
        if (any && mnet_buf_put(&b, ", ", 2)) goto fail;
        if (mnet_buf_puts(&b, h->v[i].value)) goto fail;
        any = 1;
    }
    if (!any) return 0;
    if (!b.p) { char *e = malloc(1); if (e) *e = 0; return e; }
    return (char *)b.p;
fail:
    mnet_buf_free(&b);
    return 0;
}
void mnet_hdrs_free(mnet_hdrs_t *h) {
    for (int i = 0; i < h->n; i++) { free(h->v[i].name); free(h->v[i].value); }
    free(h->v);
    h->v = 0; h->n = h->cap = 0;
}

/* ---- datas */
static long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static const char mon3[12][4] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
static int delim(int c) {
    return c == 0x09 || (c >= 0x20 && c <= 0x2F) || (c >= 0x3B && c <= 0x40) || (c >= 0x5B && c <= 0x60) ||
           (c >= 0x7B && c <= 0x7E);
}
static int dig(int c) { return c >= '0' && c <= '9'; }

/* RFC 6265 §5.1.1 — aceita IMF-fixdate, RFC 850, asctime e as variações que os servidores mandam. */
long long mnet_http_date(const char *s, size_t n) {
    int have_time = 0, have_day = 0, have_mon = 0, have_year = 0;
    int hh = 0, mi = 0, ss = 0, day = 0, mon = 0, year = 0;
    size_t i = 0;
    while (i < n) {
        while (i < n && delim((unsigned char)s[i])) i++;
        size_t a = i;
        while (i < n && !delim((unsigned char)s[i])) i++;
        size_t len = i - a;
        if (!len) continue;
        const char *t = s + a;
        if (!have_time) {               /* time = 1*2DIGIT ":" 1*2DIGIT ":" 1*2DIGIT ( non-digit *OCTET ) */
            size_t k = 0; int f[3] = {0, 0, 0}, ok = 1;
            for (int part = 0; part < 3 && ok; part++) {
                size_t st = k;
                while (k < len && dig(t[k]) && k - st < 2) f[part] = f[part] * 10 + (t[k++] - '0');
                if (k == st) ok = 0;
                if (part < 2) { if (k < len && t[k] == ':') k++; else ok = 0; }
            }
            if (ok && (k == len || !dig(t[k]))) { have_time = 1; hh = f[0]; mi = f[1]; ss = f[2]; continue; }
        }
        if (!have_day && dig(t[0]) && (len == 1 || dig(t[1])) && (len <= 2 || !dig(t[2]))) {
            day = t[0] - '0';
            if (len > 1 && dig(t[1])) day = day * 10 + (t[1] - '0');
            have_day = 1;
            continue;
        }
        if (!have_mon && len >= 3) {
            int m = -1;
            for (int k = 0; k < 12; k++) if (mnet_ieqn(t, mon3[k], 3)) { m = k; break; }
            if (m >= 0) { mon = m + 1; have_mon = 1; continue; }
        }
        if (!have_year && dig(t[0]) && dig(t[1])) {
            size_t k = 0; int y = 0;
            while (k < len && k < 4 && dig(t[k])) y = y * 10 + (t[k++] - '0');
            if (k >= 2 && (k == len || !dig(t[k]))) { year = y; have_year = 1; continue; }
        }
    }
    if (!have_time || !have_day || !have_mon || !have_year) return -1;
    if (year >= 70 && year <= 99) year += 1900;
    else if (year >= 0 && year <= 69) year += 2000;
    if (day < 1 || day > 31 || year < 1601 || hh > 23 || mi > 59 || ss > 59) return -1;
    static const int mdays[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (day > mdays[mon - 1]) return -1;
    if (mon == 2 && day == 29 && !((year % 4 == 0 && year % 100) || year % 400 == 0)) return -1;
    return days_from_civil(year, (unsigned)mon, (unsigned)day) * 86400 + hh * 3600 + mi * 60 + ss;
}

size_t mnet_fmt_date(long long t, char out[30]) {
    static const char wd[7][4] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};
    static const char mo[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    long long days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
    long long sec = t - days * 86400;
    long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
    int w = (int)(((days % 7) + 7) % 7);
    char *p = out;
    memcpy(p, wd[w], 3); p += 3; *p++ = ','; *p++ = ' ';
    *p++ = (char)('0' + d / 10); *p++ = (char)('0' + d % 10); *p++ = ' ';
    memcpy(p, mo[m - 1], 3); p += 3; *p++ = ' ';
    *p++ = (char)('0' + (y / 1000) % 10); *p++ = (char)('0' + (y / 100) % 10);
    *p++ = (char)('0' + (y / 10) % 10); *p++ = (char)('0' + y % 10); *p++ = ' ';
    int h = (int)(sec / 3600), mi = (int)(sec / 60 % 60), s = (int)(sec % 60);
    *p++ = (char)('0' + h / 10); *p++ = (char)('0' + h % 10); *p++ = ':';
    *p++ = (char)('0' + mi / 10); *p++ = (char)('0' + mi % 10); *p++ = ':';
    *p++ = (char)('0' + s / 10); *p++ = (char)('0' + s % 10);
    memcpy(p, " GMT", 4); p += 4;
    *p = 0;
    return (size_t)(p - out);
}

/* ---- DNS reverso (CFHost kCFHostNames, NSHost): nome PTR e leitura da resposta (RFC 1035 §4.1, compressão §4.1.4) */
int mnet_dns_ptr_name(const uint8_t *sa, size_t n, char *q, size_t qn) {
    static const char hx[] = "0123456789abcdef";
    size_t k = 0;
    if (n >= 8 && sa[1] == 2) {
        for (int i = 7; i >= 4; i--) {
            unsigned v = sa[i];
            char t[4]; int tl = 0;
            do { t[tl++] = (char)('0' + v % 10); v /= 10; } while (v);
            if (k + (size_t)tl + 1 >= qn) return -1;
            while (tl) q[k++] = t[--tl];
            q[k++] = '.';
        }
        if (k + 13 > qn) return -1;
        memcpy(q + k, "in-addr.arpa", 13);
        return 0;
    }
    if (n >= 24 && sa[1] == 30) {
        if (qn < 64 + 9) return -1;
        for (int i = 15; i >= 0; i--) { q[k++] = hx[sa[8 + i] & 15]; q[k++] = '.'; q[k++] = hx[sa[8 + i] >> 4]; q[k++] = '.'; }
        memcpy(q + k, "ip6.arpa", 9);
        return 0;
    }
    return -1;
}
/* nome no formato DNS a partir de off (com ponteiros); devolve o tamanho ocupado no lugar, ou -1 */
static long dns_name(const uint8_t *m, size_t len, size_t off, char *out, size_t on) {
    size_t k = 0, used = 0;
    int jumped = 0, hops = 0;
    for (;;) {
        if (off >= len) return -1;
        uint8_t l = m[off];
        if ((l & 0xc0) == 0xc0) {
            if (off + 1 >= len || ++hops > 32) return -1;
            if (!jumped) used += 2;
            jumped = 1;
            off = (size_t)(l & 0x3f) << 8 | m[off + 1];
            continue;
        }
        if (l & 0xc0) return -1;
        if (!jumped) used += 1u + l;
        if (!l) break;
        if (off + 1 + l > len) return -1;
        if (out) {
            if (k + l + 2 > on) return -1;
            if (k) out[k++] = '.';
            memcpy(out + k, m + off + 1, l);
            k += l;
        }
        off += 1u + l;
    }
    if (out) { if (!on) return -1; out[k] = 0; }
    return (long)used;
}
int mnet_dns_first_ptr(const uint8_t *m, size_t len, char *out, size_t on) {
    if (len < 12) return -1;
    if ((m[3] & 15) != 0) return -1;                                  /* RCODE */
    int qd = m[4] << 8 | m[5], an = m[6] << 8 | m[7];
    size_t p = 12;
    for (int i = 0; i < qd; i++) { long k = dns_name(m, len, p, 0, 0); if (k < 0) return -1; p += (size_t)k + 4; }
    for (int i = 0; i < an; i++) {
        long k = dns_name(m, len, p, 0, 0);
        if (k < 0 || p + (size_t)k + 10 > len) return -1;
        p += (size_t)k;
        int type = m[p] << 8 | m[p + 1];
        size_t rdlen = (size_t)(m[p + 8] << 8 | m[p + 9]);
        p += 10;
        if (p + rdlen > len) return -1;
        if (type == 12) return dns_name(m, len, p, out, on) < 0 ? -1 : 0;
        p += rdlen;
    }
    return -1;
}

/* Núcleo puro do CoreFoundation que o host testa sem o runtime ObjC.
 * Calendário gregoriano, fuso, UUID, saco, NFC latino, URL, base64 e XML mínimo.
 * Incluído por cf_public.c e por tools/cftest.c. */
#ifndef CF_PUBLIC_CORE_INC
#define CF_PUBLIC_CORE_INC
/* O teste de host define CF_HOST e traz a libc. No libSystem as declarações bastam. */
#ifdef CF_HOST
#include <string.h>
#include <stdlib.h>
#else
void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void *realloc(void *p, unsigned long n);
void free(void *p);
void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);
int memcmp(const void *a, const void *b, unsigned long n);
unsigned long strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);
char *strchr(const char *s, int c);
char *strstr(const char *h, const char *n);
int atoi(const char *s);
#endif
typedef unsigned char cf_u8;
typedef unsigned int cf_u32;

static int cf_is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* dias desde 1970-01-01 (algoritmo civil de Howard Hinnant, domínio público na forma) */
static int cf_days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe - 719468;
}
static void cf_civil_from_days(int z, int *y, int *m, int *d) {
    z += 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y_ = (int)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d_ = doy - (153 * mp + 2) / 5 + 1;
    unsigned m_ = mp + (mp < 10 ? 3 : -9);
    y_ += (m_ <= 2);
    if (y) *y = y_;
    if (m) *m = (int)m_;
    if (d) *d = (int)d_;
}

/* CFAbsoluteTime: segundos desde 2001-01-01 UTC. 978307200 = segundos 1970→2001. */
enum { CF_UNIX_TO_ABS = 978307200 };
static double cf_abs_from_ymdhms(int y, int mo, int d, int h, int mi, int s) {
    int days = cf_days_from_civil(y, (unsigned)mo, (unsigned)d);
    return (double)days * 86400.0 + h * 3600.0 + mi * 60.0 + s - (double)CF_UNIX_TO_ABS;
}
static void cf_ymdhms_from_abs(double at, int *y, int *mo, int *d, int *h, int *mi, int *s, int *wd) {
    double u = at + (double)CF_UNIX_TO_ABS;
    if (u < 0) u = 0;
    long long sec = (long long)u;
    int days = (int)(sec / 86400);
    int sod = (int)(sec % 86400);
    cf_civil_from_days(days, y, mo, d);
    if (h) *h = sod / 3600;
    if (mi) *mi = (sod / 60) % 60;
    if (s) *s = sod % 60;
    if (wd) *wd = (days + 4) % 7 + 1; /* 1 = domingo; 1970-01-01 foi quinta */
}

static int cf_last_sunday(int y, int month) {
    int dim[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && cf_is_leap(y)) dim[2] = 29;
    int wd = 0;
    cf_ymdhms_from_abs(cf_abs_from_ymdhms(y, month, dim[month], 0, 0, 0), 0, 0, 0, 0, 0, 0, &wd);
    int back = wd == 1 ? 0 : wd - 1;
    return dim[month] - back;
}
static int cf_nth_sunday(int y, int month, int n) {
    int wd = 0;
    cf_ymdhms_from_abs(cf_abs_from_ymdhms(y, month, 1, 0, 0, 0), 0, 0, 0, 0, 0, 0, &wd);
    int first = wd == 1 ? 1 : 1 + (8 - wd);
    return first + (n - 1) * 7;
}

/* segundos a leste de UTC (positivo = leste). Sem nome: 0. */
static int cf_tz_offset(const char *name, double abs_utc) {
    if (!name || !name[0] || !strcmp(name, "GMT") || !strcmp(name, "UTC") || !strcmp(name, "Etc/UTC") || !strcmp(name, "Etc/GMT"))
        return 0;
    if (!strncmp(name, "GMT", 3) && (name[3] == '+' || name[3] == '-')) {
        int sign = name[3] == '-' ? -1 : 1;
        int hh = 0, mm = 0;
        if (name[4]) hh = atoi(name + 4);
        const char *c = strchr(name, ':');
        if (c) mm = atoi(c + 1);
        return sign * (hh * 3600 + mm * 60);
    }
    int y = 0, mo = 0, d = 0;
    cf_ymdhms_from_abs(abs_utc, &y, &mo, &d, 0, 0, 0, 0);
    int dst = 0;
    if (!strcmp(name, "America/Sao_Paulo") || !strcmp(name, "America/Fortaleza") || !strcmp(name, "America/Recife"))
        return -3 * 3600;
    if (!strcmp(name, "America/Manaus")) return -4 * 3600;
    if (!strcmp(name, "America/Noronha")) return -2 * 3600;
    if (!strcmp(name, "Asia/Tokyo")) return 9 * 3600;
    if (!strcmp(name, "Asia/Shanghai") || !strcmp(name, "Asia/Hong_Kong")) return 8 * 3600;
    if (!strcmp(name, "Australia/Sydney")) {
        int start = cf_nth_sunday(y, 10, 1);
        int end = cf_nth_sunday(y, 4, 1);
        dst = (mo > 10 || (mo == 10 && d >= start) || mo < 4 || (mo == 4 && d < end));
        return (10 + dst) * 3600;
    }
    if (!strcmp(name, "America/New_York") || !strcmp(name, "America/Detroit") || !strcmp(name, "US/Eastern")) {
        int start = cf_nth_sunday(y, 3, 2), end = cf_nth_sunday(y, 11, 1);
        dst = (mo > 3 && mo < 11) || (mo == 3 && d >= start) || (mo == 11 && d < end);
        return dst ? -4 * 3600 : -5 * 3600;
    }
    if (!strcmp(name, "America/Chicago") || !strcmp(name, "US/Central")) {
        int start = cf_nth_sunday(y, 3, 2), end = cf_nth_sunday(y, 11, 1);
        dst = (mo > 3 && mo < 11) || (mo == 3 && d >= start) || (mo == 11 && d < end);
        return dst ? -5 * 3600 : -6 * 3600;
    }
    if (!strcmp(name, "America/Denver")) {
        int start = cf_nth_sunday(y, 3, 2), end = cf_nth_sunday(y, 11, 1);
        dst = (mo > 3 && mo < 11) || (mo == 3 && d >= start) || (mo == 11 && d < end);
        return dst ? -6 * 3600 : -7 * 3600;
    }
    if (!strcmp(name, "America/Los_Angeles") || !strcmp(name, "US/Pacific")) {
        int start = cf_nth_sunday(y, 3, 2), end = cf_nth_sunday(y, 11, 1);
        dst = (mo > 3 && mo < 11) || (mo == 3 && d >= start) || (mo == 11 && d < end);
        return dst ? -7 * 3600 : -8 * 3600;
    }
    if (!strcmp(name, "Europe/London") || !strcmp(name, "Europe/Lisbon") || !strcmp(name, "Europe/Dublin")) {
        int start = cf_last_sunday(y, 3), end = cf_last_sunday(y, 10);
        dst = (mo > 3 && mo < 10) || (mo == 3 && d >= start) || (mo == 10 && d < end);
        return dst ? 3600 : 0;
    }
    if (!strcmp(name, "Europe/Paris") || !strcmp(name, "Europe/Berlin") || !strcmp(name, "Europe/Madrid") || !strcmp(name, "Europe/Rome")) {
        int start = cf_last_sunday(y, 3), end = cf_last_sunday(y, 10);
        dst = (mo > 3 && mo < 10) || (mo == 3 && d >= start) || (mo == 10 && d < end);
        return dst ? 2 * 3600 : 3600;
    }
    return 0;
}
static const char *cf_tz_abbr(const char *name, double abs_utc) {
    int off = cf_tz_offset(name, abs_utc);
    if (!name) return "GMT";
    if (!strcmp(name, "America/Sao_Paulo")) return "BRT";
    if (!strcmp(name, "America/New_York")) return off == -4 * 3600 ? "EDT" : "EST";
    if (!strcmp(name, "America/Chicago")) return off == -5 * 3600 ? "CDT" : "CST";
    if (!strcmp(name, "America/Los_Angeles")) return off == -7 * 3600 ? "PDT" : "PST";
    if (!strcmp(name, "Europe/London")) return off ? "BST" : "GMT";
    if (!strcmp(name, "Europe/Paris")) return off == 7200 ? "CEST" : "CET";
    if (!strcmp(name, "Asia/Tokyo")) return "JST";
    return "GMT";
}

static void cf_uuid_format(const cf_u8 b[16], char out[37]) {
    static const char *hx = "0123456789ABCDEF";
    int o = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out[o++] = '-';
        out[o++] = hx[b[i] >> 4];
        out[o++] = hx[b[i] & 15];
    }
    out[o] = 0;
}
static int cf_hex_nibble(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int cf_uuid_parse(const char *s, cf_u8 b[16]) {
    if (!s) return 0;
    int n = 0;
    for (; *s && n < 16; s++) {
        if (*s == '-') continue;
        int hi = cf_hex_nibble((unsigned char)*s);
        if (hi < 0 || !s[1]) return 0;
        int lo = cf_hex_nibble((unsigned char)s[1]);
        if (lo < 0) return 0;
        b[n++] = (cf_u8)((hi << 4) | lo);
        s++;
    }
    return n == 16;
}

/* saco com igualdade de ponteiro */
typedef struct { const void *p; int n; } cf_bag_slot;
typedef struct { cf_bag_slot *s; int n, cap; } cf_bag;
static int cf_bag_find(cf_bag *b, const void *p) {
    for (int i = 0; i < b->n; i++) if (b->s[i].p == p) return i;
    return -1;
}
static void cf_bag_add(cf_bag *b, const void *p) {
    int i = cf_bag_find(b, p);
    if (i >= 0) { b->s[i].n++; return; }
    if (b->n == b->cap) {
        int c = b->cap ? b->cap * 2 : 4;
        cf_bag_slot *ns = (cf_bag_slot *)realloc(b->s, (size_t)c * sizeof *ns);
        if (!ns) return;
        b->s = ns;
        b->cap = c;
    }
    b->s[b->n].p = p;
    b->s[b->n].n = 1;
    b->n++;
}
static int cf_bag_count(cf_bag *b, const void *p) {
    if (!p) {
        int t = 0;
        for (int i = 0; i < b->n; i++) t += b->s[i].n;
        return t;
    }
    int i = cf_bag_find(b, p);
    return i < 0 ? 0 : b->s[i].n;
}
static void cf_bag_remove(cf_bag *b, const void *p) {
    int i = cf_bag_find(b, p);
    if (i < 0) return;
    if (--b->s[i].n > 0) return;
    b->s[i] = b->s[b->n - 1];
    b->n--;
}

/* Latin-1 pré-composto ↔ base + combinante. Æ/Ø/Þ/ß ficam como estão. */
static int cf_latin_decomp(unsigned cp, unsigned *base, unsigned *comb) {
    static const unsigned bases[] = {
        'A','A','A','A','A','A', 0, 'C','E','E','E','E','I','I','I','I',
        0, 'N','O','O','O','O','O', 0, 'O','U','U','U','U','Y', 0, 0,
        'a','a','a','a','a','a', 0, 'c','e','e','e','e','i','i','i','i',
        0, 'n','o','o','o','o','o', 0, 'o','u','u','u','u','y', 0, 'y'
    };
    static const unsigned combs[] = {
        0x300,0x301,0x302,0x303,0x308,0x30A, 0, 0x327,0x300,0x301,0x302,0x308,0x300,0x301,0x302,0x308,
        0, 0x303,0x300,0x301,0x302,0x303,0x308, 0, 0, 0x300,0x301,0x302,0x308,0x301, 0, 0,
        0x300,0x301,0x302,0x303,0x308,0x30A, 0, 0x327,0x300,0x301,0x302,0x308,0x300,0x301,0x302,0x308,
        0, 0x303,0x300,0x301,0x302,0x303,0x308, 0, 0, 0x300,0x301,0x302,0x308,0x301, 0, 0x308
    };
    if (cp < 0xC0 || cp > 0xFF) return 0;
    unsigned b = bases[cp - 0xC0];
    if (!b) return 0;
    *base = b;
    *comb = combs[cp - 0xC0];
    return 1;
}
static unsigned cf_latin_comp(unsigned base, unsigned comb) {
    unsigned b, c;
    for (unsigned cp = 0xC0; cp <= 0xFF; cp++)
        if (cf_latin_decomp(cp, &b, &c) && b == base && c == comb) return cp;
    return 0;
}
static int cf_utf8_dec(const unsigned char *s, unsigned *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { *cp = ((s[0] & 31) << 6) | (s[1] & 63); return 2; }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((s[0] & 15) << 12) | ((s[1] & 63) << 6) | (s[2] & 63);
        return 3;
    }
    *cp = s[0];
    return 1;
}
static int cf_utf8_enc(unsigned cp, char *o) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | (cp >> 6)); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    o[0] = (char)(0xE0 | (cp >> 12)); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63));
    return 3;
}
/* form: 0/1 = NFD, 2/3 = NFC. Devolve bytes escritos, sem o zero final. */
static int cf_norm_utf8(const char *in, int form, char *out, int cap) {
    unsigned cps[256];
    int n = 0;
    const unsigned char *s = (const unsigned char *)(in ? in : "");
    while (*s && n < 256) {
        unsigned cp;
        int k = cf_utf8_dec(s, &cp);
        s += k;
        unsigned b, c;
        if ((form == 0 || form == 1) && cf_latin_decomp(cp, &b, &c)) {
            if (n < 255) { cps[n++] = b; cps[n++] = c; }
        } else cps[n++] = cp;
    }
    if (form == 2 || form == 3) {
        int w = 0;
        for (int i = 0; i < n; i++) {
            if (i + 1 < n) {
                unsigned comp = cf_latin_comp(cps[i], cps[i + 1]);
                if (comp) { cps[w++] = comp; i++; continue; }
            }
            cps[w++] = cps[i];
        }
        n = w;
    }
    int o = 0;
    for (int i = 0; i < n; i++) {
        char tmp[4];
        int k = cf_utf8_enc(cps[i], tmp);
        if (o + k >= cap) break;
        memcpy(out + o, tmp, (size_t)k);
        o += k;
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
    return o;
}
static int cf_xml_unescape(const char *in, char *out, int cap) {
    int o = 0;
    for (const char *s = in ? in : ""; *s && o + 1 < cap; ) {
        if (*s == '&') {
            const char *r = 0; char ch = 0; int k = 0;
            if (!strncmp(s, "&amp;", 5)) { ch = '&'; k = 5; }
            else if (!strncmp(s, "&lt;", 4)) { ch = '<'; k = 4; }
            else if (!strncmp(s, "&gt;", 4)) { ch = '>'; k = 4; }
            else if (!strncmp(s, "&quot;", 6)) { ch = '"'; k = 6; }
            else if (!strncmp(s, "&apos;", 6)) { ch = '\''; k = 6; }
            else if (s[1] == '#') {
                int v = 0, hex = s[2] == 'x' || s[2] == 'X';
                const char *p = s + (hex ? 3 : 2);
                while (*p && *p != ';') {
                    int d = *p >= '0' && *p <= '9' ? *p - '0' : hex && *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : hex && *p >= 'A' && *p <= 'F' ? *p - 'A' + 10 : -1;
                    if (d < 0) break;
                    v = hex ? (v << 4) | d : v * 10 + d;
                    p++;
                }
                if (*p == ';' && v > 0 && v < 128) { ch = (char)v; k = (int)(p - s) + 1; }
            }
            if (k) { out[o++] = ch; s += k; (void)r; continue; }
        }
        out[o++] = *s++;
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
    return o;
}

typedef struct { char scheme[16], host[128], path[256], query[256]; int port; } cf_url_parts;
static void cf_url_parse(const char *s, cf_url_parts *u) {
    memset(u, 0, sizeof *u);
    u->port = -1;
    if (!s) return;
    const char *p = strstr(s, "://");
    const char *rest = s;
    if (p && (size_t)(p - s) < sizeof u->scheme) {
        memcpy(u->scheme, s, (size_t)(p - s));
        u->scheme[p - s] = 0;
        rest = p + 3;
    }
    const char *slash = strchr(rest, '/');
    const char *q = strchr(rest, '?');
    const char *host_end = rest + strlen(rest);
    if (slash && (!q || slash < q)) host_end = slash;
    else if (q) host_end = q;
    const char *colon = 0;
    for (const char *c = rest; c < host_end; c++) if (*c == ':') colon = c;
    size_t hl = (size_t)((colon ? colon : host_end) - rest);
    if (hl >= sizeof u->host) hl = sizeof u->host - 1;
    memcpy(u->host, rest, hl);
    u->host[hl] = 0;
    if (colon && colon < host_end) u->port = atoi(colon + 1);
    if (slash && (!q || slash < q)) {
        size_t pl = (size_t)((q ? q : slash + strlen(slash)) - slash);
        if (pl >= sizeof u->path) pl = sizeof u->path - 1;
        memcpy(u->path, slash, pl);
        u->path[pl] = 0;
    }
    if (q) {
        size_t ql = strlen(q + 1);
        if (ql >= sizeof u->query) ql = sizeof u->query - 1;
        memcpy(u->query, q + 1, ql);
        u->query[ql] = 0;
    }
}

static int cf_b64(const cf_u8 *in, int n, char *out, int cap) {
    static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < n; i += 3) {
        int v = in[i] << 16;
        if (i + 1 < n) v |= in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        char chunk[4];
        chunk[0] = t[(v >> 18) & 63];
        chunk[1] = t[(v >> 12) & 63];
        chunk[2] = i + 1 < n ? t[(v >> 6) & 63] : '=';
        chunk[3] = i + 2 < n ? t[v & 63] : '=';
        if (o + 4 >= cap) break;
        memcpy(out + o, chunk, 4);
        o += 4;
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
    return o;
}
static int cf_xml_escape(const char *in, char *out, int cap) {
    int o = 0;
    for (const char *s = in ? in : ""; *s; s++) {
        const char *r = *s == '&' ? "&amp;" : *s == '<' ? "&lt;" : *s == '>' ? "&gt;" : *s == '"' ? "&quot;" : 0;
        if (r) {
            int k = (int)strlen(r);
            if (o + k >= cap) break;
            memcpy(out + o, r, (size_t)k);
            o += k;
        } else {
            if (o + 1 >= cap) break;
            out[o++] = *s;
        }
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
    return o;
}

/* separadores: 0 = en (. e ,), 1 = pt/de (, e .), 2 = fr (, e espaço) */
static int cf_locale_kind(const char *id) {
    if (!id) return 1;
    if (!strncmp(id, "en", 2)) return 0;
    if (!strncmp(id, "fr", 2)) return 2;
    return 1;
}
static void cf_locale_seps(const char *id, char *dec, char *grp) {
    int k = cf_locale_kind(id);
    if (dec) *dec = k == 0 ? '.' : ',';
    if (grp) *grp = k == 0 ? ',' : k == 2 ? ' ' : '.';
}
/* inteiro com agrupamento de 3. frac < 0 = sem casas. */
static int cf_fmt_grouped(long long v, int frac, double whole, char dec, char grp, char *out, int cap) {
    int neg = v < 0;
    unsigned long long a = (unsigned long long)(neg ? -v : v);
    char dig[32];
    int n = 0;
    do { dig[n++] = (char)('0' + a % 10); a /= 10; } while (a && n < 30);
    char tmp[64];
    int o = 0;
    if (neg) tmp[o++] = '-';
    for (int i = n - 1; i >= 0; i--) {
        tmp[o++] = dig[i];
        if (i > 0 && i % 3 == 0 && grp) tmp[o++] = grp;
    }
    if (frac > 0) {
        tmp[o++] = dec;
        double f = whole < 0 ? -whole : whole;
        f = f - (double)(long long)f;
        for (int i = 0; i < frac; i++) {
            f *= 10;
            int d = (int)f;
            if (d > 9) d = 9;
            tmp[o++] = (char)('0' + d);
            f -= d;
        }
    }
    tmp[o] = 0;
    if (o >= cap) o = cap - 1;
    if (cap) { memcpy(out, tmp, (size_t)o); out[o] = 0; }
    return o;
}

/* anel de bytes compartilhado por um par de streams */
enum { CF_RING = 4096 };
typedef struct { cf_u8 b[CF_RING]; int r, w, n; int closed_w, closed_r; } cf_ring;
static int cf_ring_write(cf_ring *g, const cf_u8 *p, int n) {
    if (!g || g->closed_w) return -1;
    int k = 0;
    while (k < n && g->n < CF_RING) {
        g->b[g->w] = p[k++];
        g->w = (g->w + 1) % CF_RING;
        g->n++;
    }
    return k;
}
static int cf_ring_read(cf_ring *g, cf_u8 *p, int n) {
    if (!g) return -1;
    if (!g->n) return g->closed_w ? 0 : 0;
    int k = 0;
    while (k < n && g->n) {
        p[k++] = g->b[g->r];
        g->r = (g->r + 1) % CF_RING;
        g->n--;
    }
    return k;
}

/* versão Carbon NumVersion a partir de "M.m.b" */
static cf_u32 cf_version_bcd(const char *s) {
    int M = 0, m = 0, b = 0;
    if (!s) return 0;
    M = atoi(s);
    const char *p = strchr(s, '.');
    if (p) { m = atoi(p + 1); p = strchr(p + 1, '.'); if (p) b = atoi(p + 1); }
    if (M > 99) M = 99;
    if (m > 9) m = 9;
    if (b > 9) b = 9;
    return ((cf_u32)M << 24) | ((cf_u32)m << 20) | ((cf_u32)b << 16);
}

/* próxima virada de fuso, ou 0 se o nome não tem horário de verão */
static double cf_tz_next_dst(const char *name, double at) {
    int base = cf_tz_offset(name, at);
    int i;
    for (i = 1; i < 400; i++) {
        double t = at + (double)i * 86400.0;
        if (cf_tz_offset(name, t) != base) return t;
    }
    return 0;
}

/* ---- bplist00: o mesmo layout que plist.c lê (trailer de 32, offset e ref em big-endian) ---- */
typedef struct {
    unsigned char *body; int bn, bc;
    int *off, n, cap;
} cf_bpw;
static void bpw_byte(cf_bpw *w, unsigned char c) {
    if (w->bn + 1 > w->bc) {
        int c2 = w->bc ? w->bc * 2 : 64;
        unsigned char *n = (unsigned char *)realloc(w->body, (size_t)c2);
        if (!n) return;
        w->body = n; w->bc = c2;
    }
    w->body[w->bn++] = c;
}
static void bpw_bytes(cf_bpw *w, const void *p, int n) {
    int i; for (i = 0; i < n; i++) bpw_byte(w, ((const unsigned char *)p)[i]);
}
static int bpw_slot(cf_bpw *w) {
    if (w->n == w->cap) {
        int c = w->cap ? w->cap * 2 : 8;
        int *n = (int *)realloc(w->off, (size_t)c * sizeof(int));
        if (!n) return -1;
        w->off = n; w->cap = c;
    }
    w->off[w->n] = w->bn;
    return w->n++;
}
static cf_bpw *cf_bpw_new(void) { return (cf_bpw *)calloc(1, sizeof(cf_bpw)); }
static void cf_bpw_free(cf_bpw *w) { if (!w) return; free(w->body); free(w->off); free(w); }
static int cf_bpw_bool(cf_bpw *w, int v) { int i = bpw_slot(w); bpw_byte(w, v ? 0x09 : 0x08); return i; }
static int cf_bpw_int(cf_bpw *w, long long v) {
    int i = bpw_slot(w), k; unsigned long long u = (unsigned long long)v;
    bpw_byte(w, 0x13);
    for (k = 7; k >= 0; k--) bpw_byte(w, (unsigned char)(u >> (k * 8)));
    return i;
}
static int cf_bpw_str(cf_bpw *w, const char *s) {
    int i = bpw_slot(w), n = s ? (int)strlen(s) : 0, k;
    if (n < 15) bpw_byte(w, (unsigned char)(0x50 + n));
    else if (n < 256) { bpw_byte(w, 0x5F); bpw_byte(w, 0x10); bpw_byte(w, (unsigned char)n); }
    else { bpw_byte(w, 0x5F); bpw_byte(w, 0x11); bpw_byte(w, (unsigned char)(n >> 8)); bpw_byte(w, (unsigned char)n); }
    for (k = 0; k < n; k++) bpw_byte(w, (unsigned char)s[k]);
    return i;
}
static int cf_bpw_data(cf_bpw *w, const void *p, int n) {
    int i = bpw_slot(w);
    if (n < 15) bpw_byte(w, (unsigned char)(0x40 + n));
    else { bpw_byte(w, 0x4F); bpw_byte(w, 0x10); bpw_byte(w, (unsigned char)n); }
    bpw_bytes(w, p, n);
    return i;
}
static int cf_bpw_arr(cf_bpw *w, const int *idx, int n) {
    int i = bpw_slot(w), k;
    if (n < 15) bpw_byte(w, (unsigned char)(0xA0 + n));
    else { bpw_byte(w, 0xAF); bpw_byte(w, 0x10); bpw_byte(w, (unsigned char)n); }
    for (k = 0; k < n; k++) bpw_byte(w, (unsigned char)idx[k]);
    return i;
}
static int cf_bpw_dict(cf_bpw *w, const int *keys, const int *vals, int n) {
    int i = bpw_slot(w), k;
    if (n < 15) bpw_byte(w, (unsigned char)(0xD0 + n));
    else { bpw_byte(w, 0xDF); bpw_byte(w, 0x10); bpw_byte(w, (unsigned char)n); }
    for (k = 0; k < n; k++) bpw_byte(w, (unsigned char)keys[k]);
    for (k = 0; k < n; k++) bpw_byte(w, (unsigned char)vals[k]);
    return i;
}
static int cf_bpw_finish(cf_bpw *w, int top, unsigned char **out, int *outn) {
    int osz = 1, i, k, total;
    for (i = 0; i < w->n; i++) if (w->off[i] + 8 >= 256) osz = 2;
    total = 8 + w->bn + w->n * osz + 32;
    if (osz == 1) for (i = 0; i < w->n; i++) if (w->off[i] + 8 >= 256) osz = 2;
    if (total >= 256 && osz < 2) osz = 2;
    {
        unsigned char *b = (unsigned char *)calloc(1, (size_t)(8 + w->bn + w->n * 4 + 32));
        int n = 0, ot;
        if (!b) return -1;
        memcpy(b, "bplist00", 8); n = 8;
        memcpy(b + n, w->body, (size_t)w->bn); n += w->bn;
        ot = n;
        for (i = 0; i < w->n; i++) {
            int off = w->off[i] + 8;
            for (k = osz - 1; k >= 0; k--) b[n++] = (unsigned char)(off >> (k * 8));
        }
        while (n + 32 > 8 + w->bn + w->n * osz + 32) { /* tamanho fechado */ break; }
        {
            unsigned char *nb = (unsigned char *)realloc(b, (size_t)n + 32);
            unsigned long long nobj = (unsigned long long)w->n, topu = (unsigned long long)top, otu = (unsigned long long)ot;
            if (!nb) { free(b); return -1; }
            b = nb;
            memset(b + n, 0, 32);
            b[n + 6] = (unsigned char)osz;
            b[n + 7] = 1;
            for (k = 0; k < 8; k++) b[n + 8 + k] = (unsigned char)(nobj >> (56 - 8 * k));
            for (k = 0; k < 8; k++) b[n + 16 + k] = (unsigned char)(topu >> (56 - 8 * k));
            for (k = 0; k < 8; k++) b[n + 24 + k] = (unsigned char)(otu >> (56 - 8 * k));
            n += 32;
            *out = b; if (outn) *outn = n;
        }
    }
    return 0;
}

typedef struct { const unsigned char *b; int n, osz, rsz, nobj, top, otab; } cf_bpr;
static unsigned long long bp_be(const unsigned char *p, int n) {
    unsigned long long v = 0; int i; for (i = 0; i < n; i++) v = (v << 8) | p[i]; return v;
}
static int cf_bpr_open(cf_bpr *r, const unsigned char *b, int n) {
    const unsigned char *t;
    memset(r, 0, sizeof *r);
    if (!b || n < 40 || memcmp(b, "bplist00", 8)) return -1;
    t = b + n - 32;
    r->b = b; r->n = n; r->osz = t[6]; r->rsz = t[7];
    r->nobj = (int)bp_be(t + 8, 8); r->top = (int)bp_be(t + 16, 8); r->otab = (int)bp_be(t + 24, 8);
    if (!r->osz || !r->rsz || r->otab >= n || r->nobj < 1) return -1;
    return 0;
}
static const unsigned char *cf_bpr_obj(cf_bpr *r, int idx, int *kind, unsigned long long *ival, const unsigned char **bytes, int *len) {
    int off; const unsigned char *p; int t, m, n;
    if (idx < 0 || idx >= r->nobj) return 0;
    off = (int)bp_be(r->b + r->otab + idx * r->osz, r->osz);
    if (off < 0 || off >= r->n) return 0;
    p = r->b + off; t = *p >> 4; m = *p & 15;
    if (kind) *kind = t;
    if (t == 0) { if (ival) *ival = (unsigned long long)(m == 9); return p; }
    if (t == 1) { n = 1 << m; if (ival) *ival = bp_be(p + 1, n > 8 ? 8 : n); return p; }
    p++;
    if (m == 15) { int nb = 1 << (*p & 15); n = (int)bp_be(p + 1, nb); p += 1 + nb; }
    else n = m;
    if (len) *len = n;
    if (bytes) *bytes = p;
    return p;
}

/* um passo do CFSocket no run loop: se o descritor tem leitura, entrega os bytes */
#ifdef CF_HOST
#include <poll.h>
#include <sys/socket.h>
#define cf_pollfd struct pollfd
#else
typedef struct { int fd; short events, revents; } cf_pollfd;
int poll(cf_pollfd *fds, unsigned n, int timeout);
long recv(int fd, void *b, unsigned long n, int flags);
#endif
static int cf_sock_service(int fd, unsigned flags, void (*fn)(void *info, int kind, const void *data, int n), void *info) {
    cf_pollfd p;
    unsigned char buf[512];
    long n;
    if (fd < 0) return -1;
    memset(&p, 0, sizeof p);
    p.fd = fd; p.events = 1;
    if (poll(&p, 1, 0) <= 0 || !(p.revents & 1)) return 0;
    n = recv(fd, buf, sizeof buf, 0);
    if (n < 0) return -1;
    if (fn && (flags & 1)) fn(info, flags == 3 ? 3 : 1, flags == 3 ? buf : 0, (int)n);
    return 1;
}

#endif

/* Formatadores de Foundation usados no lugar do FoundationInternationalization
 * onde o ICU não está ligado neste processo: número, data gregoriana, lista,
 * medida (comprimento e massa), decimal em base 10 e um Markdown mínimo.
 *
 * Não são os ~278 símbolos mangled. O oracle de saída é este arquivo, coberto
 * por tools/fmttest.c. Locale desconhecido cai em en. Separador de milhar do
 * francês é espaço comum (não o espaço estreito do ICU).
 */
#ifdef FMT_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

enum { MF_TEXT = 0, MF_BOLD = 1, MF_ITALIC = 2, MF_CODE = 3, MF_LINK = 4 };
enum { MF_DATE_NUMERIC = 0, MF_DATE_ABBREV = 1, MF_DATE_LONG = 2, MF_DATE_COMPLETE = 3 };
enum { MF_TIME_SHORT = 1, MF_TIME_STANDARD = 2 };

typedef struct {
    int kind;
    int start;
    int len;
    int href;
    int href_len;
} mf_span;

static void mf_lang(const char *locale, char *out, size_t cap) {
    size_t i = 0;
    if (!locale || !locale[0]) locale = "en";
    for (; locale[i] && locale[i] != '_' && locale[i] != '-' && i + 1 < cap; i++) {
        char c = locale[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[i] = 0;
    if (!out[0]) { out[0] = 'e'; out[1] = 'n'; out[2] = 0; }
}

static int mf_is(const char *lang, const char *code) { return strcmp(lang, code) == 0; }

static void mf_seps(const char *locale, char *group, char *decimal) {
    char lang[8];
    mf_lang(locale, lang, sizeof lang);
    if (mf_is(lang, "fr")) { *group = ' '; *decimal = ','; }
    else if (mf_is(lang, "en")) { *group = ','; *decimal = '.'; }
    else { *group = '.'; *decimal = ','; }
}

static int mf_put(char *out, size_t cap, const char *s) {
    size_t n = strlen(s);
    if (n + 1 > cap) return -1;
    memcpy(out, s, n + 1);
    return (int)n;
}

static int mf_integer(const char *locale, long long v, int grouping, char *out, size_t cap) {
    if (!out || cap < 2) return -1;
    char group, dec;
    mf_seps(locale, &group, &dec);
    (void)dec;
    int neg = v < 0;
    unsigned long long x = neg ? (unsigned long long)(-(v + 1)) + 1 : (unsigned long long)v;
    char digs[32];
    int n = 0;
    if (x == 0) digs[n++] = '0';
    while (x && n < 32) { digs[n++] = (char)('0' + (x % 10)); x /= 10; }
    char tmp[48];
    int o = 0;
    if (neg) tmp[o++] = '-';
    for (int i = n - 1; i >= 0; i--) {
        tmp[o++] = digs[i];
        int left = i;
        if (grouping && left > 0 && left % 3 == 0) tmp[o++] = group;
    }
    tmp[o] = 0;
    return mf_put(out, cap, tmp);
}

static int mf_decimal(const char *locale, double v, int frac, int grouping, char *out, size_t cap) {
    if (!out || cap < 2 || frac < 0 || frac > 6) return -1;
    if (v != v) return mf_put(out, cap, "NaN");
    char group, dec;
    mf_seps(locale, &group, &dec);
    int neg = v < 0;
    double x = neg ? -v : v;
    double scale = 1;
    for (int i = 0; i < frac; i++) scale *= 10;
    unsigned long long scaled = (unsigned long long)(x * scale + 0.5);
    unsigned long long div = 1;
    for (int i = 0; i < frac; i++) div *= 10;
    unsigned long long ip = frac ? scaled / div : scaled;
    unsigned long long fp = frac ? scaled % div : 0;
    char body[48];
    long long shown = neg ? -(long long)ip : (long long)ip;
    if (ip > 9223372036854775807ULL) return -1;
    if (mf_integer(locale, shown, grouping, body, sizeof body) < 0) return -1;
    if (!frac) return mf_put(out, cap, body);
    char tmp[64];
    int k = snprintf(tmp, sizeof tmp, "%s%c", body, dec);
    for (int i = frac - 1; i >= 0 && k > 0 && (size_t)k + 1 < sizeof tmp; i--) {
        tmp[k++] = (char)('0' + (fp / 1));
        unsigned long long p10 = 1;
        for (int j = 0; j < i; j++) p10 *= 10;
        tmp[k - 1] = (char)('0' + (fp / p10) % 10);
    }
    tmp[k] = 0;
    (void)group;
    return mf_put(out, cap, tmp);
}

static long long mf_days(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long long)era * 146097 + (long long)doe - 719468;
}

static int mf_weekday(int y, int m, int d) {
    long long days = mf_days(y, m, d);
    int wd = (int)((days + 4) % 7);
    if (wd < 0) wd += 7;
    return wd;
}

static const char *mf_month(const char *lang, int m, int wide) {
    static const char *en_w[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    static const char *en_a[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char *pt_w[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
    static const char *pt_a[] = {"jan.", "fev.", "mar.", "abr.", "mai.", "jun.", "jul.", "ago.", "set.", "out.", "nov.", "dez."};
    static const char *de_w[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};
    static const char *de_a[] = {"Jan.", "Feb.", "März", "Apr.", "Mai", "Juni", "Juli", "Aug.", "Sep.", "Okt.", "Nov.", "Dez."};
    static const char *fr_w[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"};
    static const char *fr_a[] = {"janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc."};
    const char **t = wide ? en_w : en_a;
    if (mf_is(lang, "pt")) t = wide ? pt_w : pt_a;
    else if (mf_is(lang, "de")) t = wide ? de_w : de_a;
    else if (mf_is(lang, "fr")) t = wide ? fr_w : fr_a;
    if (m < 1) m = 1;
    if (m > 12) m = 12;
    return t[m - 1];
}

static const char *mf_wday(const char *lang, int wd) {
    static const char *en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *pt[] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"};
    static const char *de[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
    static const char *fr[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
    const char **t = en;
    if (mf_is(lang, "pt")) t = pt;
    else if (mf_is(lang, "de")) t = de;
    else if (mf_is(lang, "fr")) t = fr;
    if (wd < 0 || wd > 6) wd = 0;
    return t[wd];
}

static int mf_date(const char *locale, int y, int m, int d, int style, char *out, size_t cap) {
    char lang[8];
    char tmp[160];
    mf_lang(locale, lang, sizeof lang);
    if (m < 1 || m > 12 || d < 1 || d > 31) return -1;
    if (style == MF_DATE_NUMERIC) {
        if (mf_is(lang, "en")) snprintf(tmp, sizeof tmp, "%d/%d/%d", m, d, y);
        else if (mf_is(lang, "de")) snprintf(tmp, sizeof tmp, "%02d.%02d.%04d", d, m, y);
        else snprintf(tmp, sizeof tmp, "%02d/%02d/%04d", d, m, y);
    } else if (style == MF_DATE_ABBREV || style == MF_DATE_LONG) {
        int wide = style == MF_DATE_LONG;
        const char *mo = mf_month(lang, m, wide);
        if (mf_is(lang, "pt")) snprintf(tmp, sizeof tmp, "%d de %s de %d", d, mo, y);
        else if (mf_is(lang, "de")) snprintf(tmp, sizeof tmp, "%d. %s %d", d, mo, y);
        else if (mf_is(lang, "fr")) snprintf(tmp, sizeof tmp, "%d %s %d", d, mo, y);
        else snprintf(tmp, sizeof tmp, "%s %d, %d", mo, d, y);
    } else if (style == MF_DATE_COMPLETE) {
        const char *wd = mf_wday(lang, mf_weekday(y, m, d));
        const char *mo = mf_month(lang, m, 1);
        if (mf_is(lang, "pt")) snprintf(tmp, sizeof tmp, "%s, %d de %s de %d", wd, d, mo, y);
        else if (mf_is(lang, "de")) snprintf(tmp, sizeof tmp, "%s, %d. %s %d", wd, d, mo, y);
        else if (mf_is(lang, "fr")) snprintf(tmp, sizeof tmp, "%s %d %s %d", wd, d, mo, y);
        else snprintf(tmp, sizeof tmp, "%s, %s %d, %d", wd, mo, d, y);
    } else return -1;
    return mf_put(out, cap, tmp);
}

static int mf_time(const char *locale, int hour, int min, int sec, int style, char *out, size_t cap) {
    char lang[8];
    char tmp[32];
    mf_lang(locale, lang, sizeof lang);
    if (hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 60) return -1;
    int h12 = mf_is(lang, "en");
    if (h12) {
        int h = hour % 12;
        if (h == 0) h = 12;
        const char *ap = hour < 12 ? "AM" : "PM";
        if (style == MF_TIME_STANDARD) snprintf(tmp, sizeof tmp, "%d:%02d:%02d %s", h, min, sec, ap);
        else snprintf(tmp, sizeof tmp, "%d:%02d %s", h, min, ap);
    } else if (style == MF_TIME_STANDARD) snprintf(tmp, sizeof tmp, "%02d:%02d:%02d", hour, min, sec);
    else snprintf(tmp, sizeof tmp, "%02d:%02d", hour, min);
    return mf_put(out, cap, tmp);
}

static const char *mf_and(const char *lang) {
    if (mf_is(lang, "pt")) return " e ";
    if (mf_is(lang, "de")) return " und ";
    if (mf_is(lang, "fr")) return " et ";
    return " and ";
}

static int mf_list(const char *locale, const char **items, int n, char *out, size_t cap) {
    char lang[8];
    mf_lang(locale, lang, sizeof lang);
    if (!out || cap < 1 || n < 0) return -1;
    if (n == 0) { out[0] = 0; return 0; }
    size_t o = 0;
    for (int i = 0; i < n; i++) {
        const char *sep = "";
        if (i > 0 && i == n - 1) sep = mf_and(lang);
        else if (i > 0) sep = ", ";
        /* inglês com 3+ itens usa vírgula de Oxford: "A, B, and C" */
        if (i > 0 && i == n - 1 && n >= 3 && mf_is(lang, "en")) sep = ", and ";
        const char *it = items[i] ? items[i] : "";
        size_t ns = strlen(sep), ni = strlen(it);
        if (o + ns + ni + 1 > cap) return -1;
        memcpy(out + o, sep, ns);
        o += ns;
        memcpy(out + o, it, ni);
        o += ni;
    }
    out[o] = 0;
    return (int)o;
}

static int mf_unit(const char *u, int *dim, double *factor) {
    struct { const char *n; int dim; double f; } t[] = {
        {"m", 1, 1}, {"km", 1, 1000}, {"cm", 1, 0.01}, {"mm", 1, 0.001},
        {"ft", 1, 0.3048}, {"in", 1, 0.0254}, {"mi", 1, 1609.344},
        {"kg", 2, 1}, {"g", 2, 0.001}, {"lb", 2, 0.45359237}, {"oz", 2, 0.028349523125},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) if (u && !strcmp(u, t[i].n)) {
        *dim = t[i].dim;
        *factor = t[i].f;
        return 0;
    }
    return -1;
}

static int mf_convert(const char *from, const char *to, double v, double *out) {
    int d1 = 0, d2 = 0;
    double f1 = 0, f2 = 0;
    if (!out || mf_unit(from, &d1, &f1) || mf_unit(to, &d2, &f2) || d1 != d2 || f2 == 0) return -1;
    *out = v * f1 / f2;
    return 0;
}

typedef struct { int neg; int n; int scale; char d[48]; } mf_dec;

static int mf_dec_parse(mf_dec *o, const char *s) {
    memset(o, 0, sizeof *o);
    if (!s || !s[0]) return -1;
    if (*s == '-') { o->neg = 1; s++; }
    else if (*s == '+') s++;
    const char *dot = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '.') { if (dot) return -1; dot = p; continue; }
        if (*p < '0' || *p > '9') return -1;
    }
    for (const char *p = s; *p; p++) if (*p != '.') {
        if (o->n < 47) o->d[o->n++] = *p;
    }
    o->scale = dot ? (int)strlen(dot + 1) : 0;
    int z = 0;
    while (z < o->n - 1 && o->d[z] == '0') z++;
    if (z) { memmove(o->d, o->d + z, (size_t)(o->n - z)); o->n -= z; }
    if (o->n == 1 && o->d[0] == '0') o->neg = 0;
    return 0;
}

static void mf_dec_pad(mf_dec *a, int scale) {
    while (a->scale < scale && a->n < 47) { a->d[a->n++] = '0'; a->scale++; }
}

static int mf_dec_cmp_mag(const mf_dec *a, const mf_dec *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    return memcmp(a->d, b->d, (size_t)a->n);
}

static void mf_dec_fmt(const mf_dec *a, char *out, size_t cap) {
    mf_dec t = *a;
    while (t.scale > 0 && t.n > 0 && t.d[t.n - 1] == '0') { t.n--; t.scale--; }
    int z = 0;
    while (z < t.n - t.scale - 1 && t.d[z] == '0') z++;
    if (z) { memmove(t.d, t.d + z, (size_t)(t.n - z)); t.n -= z; }
    if (t.n == 0) { t.n = 1; t.d[0] = '0'; t.scale = 0; t.neg = 0; }
    int all0 = 1;
    for (int i = 0; i < t.n; i++) if (t.d[i] != '0') all0 = 0;
    if (all0) t.neg = 0;
    int ip = t.n - t.scale;
    if (ip <= 0) {
        char tmp[64];
        int k = 0;
        if (t.neg) tmp[k++] = '-';
        tmp[k++] = '0';
        tmp[k++] = '.';
        for (int i = 0; i < -ip && k < 60; i++) tmp[k++] = '0';
        for (int i = 0; i < t.n && k < 60; i++) tmp[k++] = t.d[i];
        tmp[k] = 0;
        snprintf(out, cap, "%s", tmp);
        return;
    }
    char tmp[64];
    int k = 0;
    if (t.neg) tmp[k++] = '-';
    for (int i = 0; i < ip && k < 60; i++) tmp[k++] = t.d[i];
    if (t.scale > 0 && k < 60) {
        tmp[k++] = '.';
        for (int i = 0; i < t.scale && k < 60; i++) tmp[k++] = t.d[ip + i];
    }
    tmp[k] = 0;
    snprintf(out, cap, "%s", tmp);
}

static int mf_dec_add(const char *as, const char *bs, char *out, size_t cap) {
    mf_dec a, b;
    if (!out || cap < 2 || mf_dec_parse(&a, as) || mf_dec_parse(&b, bs)) return -1;
    int sc = a.scale > b.scale ? a.scale : b.scale;
    mf_dec_pad(&a, sc);
    mf_dec_pad(&b, sc);
    while (a.n < b.n && a.n < 47) { memmove(a.d + 1, a.d, (size_t)a.n); a.d[0] = '0'; a.n++; }
    while (b.n < a.n && b.n < 47) { memmove(b.d + 1, b.d, (size_t)b.n); b.d[0] = '0'; b.n++; }
    mf_dec o;
    memset(&o, 0, sizeof o);
    o.scale = sc;
    if (a.neg == b.neg) {
        int carry = 0;
        o.n = a.n;
        for (int i = a.n - 1; i >= 0; i--) {
            int s = (a.d[i] - '0') + (b.d[i] - '0') + carry;
            o.d[i] = (char)('0' + (s % 10));
            carry = s / 10;
        }
        if (carry && o.n < 47) { memmove(o.d + 1, o.d, (size_t)o.n); o.d[0] = (char)('0' + carry); o.n++; }
        o.neg = a.neg;
    } else {
        const mf_dec *hi = &a, *lo = &b;
        if (mf_dec_cmp_mag(&a, &b) < 0) { hi = &b; lo = &a; }
        int borrow = 0;
        o.n = hi->n;
        for (int i = hi->n - 1; i >= 0; i--) {
            int s = (hi->d[i] - '0') - (lo->d[i] - '0') - borrow;
            if (s < 0) { s += 10; borrow = 1; } else borrow = 0;
            o.d[i] = (char)('0' + s);
        }
        o.neg = hi->neg;
    }
    mf_dec_fmt(&o, out, cap);
    return 0;
}

static int mf_span_add(mf_span *sp, int *n, int cap, int kind, int start, int len, int href, int hlen) {
    if (*n >= cap || len < 0) return -1;
    sp[*n].kind = kind;
    sp[*n].start = start;
    sp[*n].len = len;
    sp[*n].href = href;
    sp[*n].href_len = hlen;
    (*n)++;
    return 0;
}

static int mf_markdown(const char *src, char *plain, size_t pcap, mf_span *sp, int scap, char *hrefs, size_t hcap) {
    if (!src || !plain || pcap < 1) return -1;
    size_t n = strlen(src), p = 0, h = 0;
    int ns = 0;
    for (size_t i = 0; i < n;) {
        int kind = MF_TEXT;
        size_t take = 1, mark = 0, hend = 0;
        const char *href = 0;
        size_t hlen = 0;
        if (i + 1 < n && src[i] == '*' && src[i + 1] == '*') {
            const char *c = strstr(src + i + 2, "**");
            if (c) { kind = MF_BOLD; mark = 2; take = (size_t)(c - (src + i + 2)); hend = 2; }
        } else if (src[i] == '`') {
            const char *c = strchr(src + i + 1, '`');
            if (c) { kind = MF_CODE; mark = 1; take = (size_t)(c - (src + i + 1)); hend = 1; }
        } else if (src[i] == '[') {
            const char *rb = strchr(src + i + 1, ']');
            if (rb && rb[1] == '(') {
                const char *rp = strchr(rb + 2, ')');
                if (rp) {
                    kind = MF_LINK; mark = 1; take = (size_t)(rb - (src + i + 1));
                    href = rb + 2; hlen = (size_t)(rp - (rb + 2)); hend = (size_t)(rp - rb) + 1;
                }
            }
        } else if (src[i] == '*') {
            const char *c = strchr(src + i + 1, '*');
            if (c && c != src + i + 1) { kind = MF_ITALIC; mark = 1; take = (size_t)(c - (src + i + 1)); hend = 1; }
        }
        if (kind == MF_TEXT) {
            if (p + 1 >= pcap) return -1;
            if (ns > 0 && sp[ns - 1].kind == MF_TEXT) sp[ns - 1].len++;
            else if (mf_span_add(sp, &ns, scap, MF_TEXT, (int)p, 1, -1, 0)) return -1;
            plain[p++] = src[i++];
            continue;
        }
        if (p + take >= pcap) return -1;
        int hoff = -1, hln = 0;
        if (href) {
            if (h + hlen + 1 > hcap || !hrefs) return -1;
            memcpy(hrefs + h, href, hlen);
            hrefs[h + hlen] = 0;
            hoff = (int)h;
            hln = (int)hlen;
            h += hlen + 1;
        }
        if (mf_span_add(sp, &ns, scap, kind, (int)p, (int)take, hoff, hln)) return -1;
        memcpy(plain + p, src + i + mark, take);
        p += take;
        i += mark + take + hend;
    }
    plain[p] = 0;
    return ns;
}

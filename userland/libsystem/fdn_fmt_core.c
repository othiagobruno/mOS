/*
 * mOS libSystem — núcleo dos formatters (fdn_fmt_core.h), sobre a ICU.
 * Teste de host: tools/fdn_fmt_test.c (a ICU do Linux, mesma tabela de fdn_icu.c).
 */
#include "fdn_fmt_core.h"
#ifdef FDN_ICU_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#else
#include "libc.h"
int snprintf(char *b, size_t n, const char *fmt, ...);
double floor(double x);
double fabs(double x);
double pow(double x, double y);
char *strstr(const char *h, const char *n);
char *strcat(char *d, const char *s);
char *strcpy(char *d, const char *s);
#endif

#define ICU (fdn_icu())

static int put(char *out, int cap, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    if (cap > 0) { int k = n < cap - 1 ? n : cap - 1; memcpy(out, s, (size_t)k); out[k] = 0; }
    return n;
}
static int put16(char *out, int cap, const UChar *s, int32_t n) { return fdn_utf8_from_u16(s, n, out, cap); }

/* ===================================================================== datas */

void fdn_df_init(fdn_df *f) {
    memset(f, 0, sizeof *f);
    strcpy(f->cal, "gregorian");
    f->lenient = 0;
}

void fdn_df_reset(fdn_df *f) {
    if (f->udat && ICU->udat_close) ICU->udat_close(f->udat);
    f->udat = 0;
}

static int icu_style(int ns) {
    switch (ns) { case 1: return UDAT_SHORT; case 2: return UDAT_MEDIUM; case 3: return UDAT_LONG; case 4: return UDAT_FULL; }
    return UDAT_NONE;
}

static void *df_open(fdn_df *f) {
    char loc[160];
    UChar tz[96], pat[256];
    UErrorCode e = U_ZERO_ERROR;
    long fixed;
    char zname[96];
    if (f->udat) return f->udat;
    if (!ICU->ok) return 0;
    fdn_loc_with_keyword(f->locale, "calendar", f->cal[0] ? f->cal : "gregorian", loc, sizeof loc);
    if (fdn_tz_parse_fixed(f->tz, &fixed) == 0) {
        long a = fixed < 0 ? -fixed : fixed;
        if (fixed) snprintf(zname, sizeof zname, "GMT%c%02ld:%02ld", fixed < 0 ? '-' : '+', a / 3600, a % 3600 / 60);
        else strcpy(zname, "GMT");
    } else snprintf(zname, sizeof zname, "%s", f->tz[0] ? f->tz : "GMT");
    int32_t tl = fdn_u16_from_utf8(zname, -1, tz, 96);
    if (f->pattern[0]) {
        int32_t pl = fdn_u16_from_utf8(f->pattern, -1, pat, 256);
        f->udat = ICU->udat_open(UDAT_PATTERN, UDAT_PATTERN, loc, tz, tl, pat, pl, &e);
    } else {
        if (!f->date_style && !f->time_style) return 0;   /* sem estilo e sem formato: a Foundation devolve "" */
        int ds = icu_style(f->date_style);
        if (f->relative && ds != UDAT_NONE) ds |= UDAT_RELATIVE;
        f->udat = ICU->udat_open(icu_style(f->time_style), ds, loc, tz, tl, 0, 0, &e);
    }
    if (U_FAILURE(e)) { f->udat = 0; return 0; }
    ICU->udat_setLenient(f->udat, (UBool)(f->lenient ? 1 : 0));
    if (f->has_two_digit) { e = U_ZERO_ERROR; ICU->udat_set2DigitYearStart(f->udat, f->two_digit_start * 1000.0, &e); }
    if (f->first_weekday || f->min_days) {
        UErrorCode e2 = U_ZERO_ERROR;
        void *c = ICU->ucal_open(tz, tl, loc, 0, &e2);
        if (!U_FAILURE(e2) && c) {
            if (f->first_weekday) ICU->ucal_setAttribute(c, UCAL_FIRST_DAY_OF_WEEK, f->first_weekday);
            if (f->min_days) ICU->ucal_setAttribute(c, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK, f->min_days);
            ICU->ucal_setAttribute(c, UCAL_LENIENT, f->lenient ? 1 : 0);
            ICU->udat_setCalendar(f->udat, c);
            ICU->ucal_close(c);
        }
    }
    return f->udat;
}

int fdn_df_format(fdn_df *f, double t, char *out, int cap) {
    UChar u[512];
    UErrorCode e = U_ZERO_ERROR;
    void *d = df_open(f);
    if (cap > 0) out[0] = 0;
    if (!d) return 0;
    int32_t n = ICU->udat_format(d, floor(t * 1000.0 + 0.5e-3), u, 512, 0, &e);
    if (U_FAILURE(e)) return 0;
    return put16(out, cap, u, n);
}

int fdn_df_parse(fdn_df *f, const char *s, int32_t start, double *t, int32_t *end) {
    UChar *u;
    int32_t n, pos = start;
    UErrorCode e = U_ZERO_ERROR;
    void *d = df_open(f);
    if (!d || !s) return 0;
    u = fdn_u16_dup(s, &n);
    if (!u) return 0;
    UDate r = ICU->udat_parse(d, u, n, &pos, &e);
    free(u);
    if (U_FAILURE(e) || pos == start) return 0;
    if (end) *end = pos;
    *t = r / 1000.0;
    return 1;
}

int fdn_df_pattern(fdn_df *f, char *out, int cap) {
    UChar u[256];
    UErrorCode e = U_ZERO_ERROR;
    if (f->pattern[0]) return put(out, cap, f->pattern, -1);
    void *d = df_open(f);
    if (cap > 0) out[0] = 0;
    if (!d) return 0;
    int32_t n = ICU->udat_toPattern(d, 0, u, 256, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}

int fdn_df_template(const char *locale, const char *tmpl, char *out, int cap) {
    UErrorCode e = U_ZERO_ERROR;
    UChar sk[128], r[256];
    char loc[160];
    if (cap > 0) out[0] = 0;
    if (!ICU->ok || !tmpl) return 0;
    fdn_loc_canonical(locale, loc, sizeof loc);
    void *g = ICU->udatpg_open(loc, &e);
    if (U_FAILURE(e) || !g) return 0;
    int32_t sl = fdn_u16_from_utf8(tmpl, -1, sk, 128);
    int32_t n = ICU->udatpg_getBestPattern(g, sk, sl, r, 256, &e);
    ICU->udatpg_close(g);
    return U_FAILURE(e) ? 0 : put16(out, cap, r, n);
}

/* NSISO8601DateFormatOptions -> padrão ICU (en_US_POSIX, gregoriano) */
int fdn_iso_pattern(unsigned long o, char *out, int cap) {
    char b[96] = "";
    int year = o & 1, month = o & 2, week = o & 4, day = o & 16, time = o & 32, zone = o & 64;
    const char *ds = (o & 256) ? "-" : "", *ts = (o & 512) ? ":" : "";
    if (year) strcat(b, week ? "YYYY" : "yyyy");
    if (month && !week) { if (b[0]) strcat(b, ds); strcat(b, "MM"); }
    if (week) { if (b[0]) strcat(b, ds); strcat(b, "'W'ww"); }
    if (day) {
        if (b[0]) strcat(b, ds);
        if (week) strcat(b, "ee");
        else if (month) strcat(b, "dd");
        else strcat(b, "DDD");
    }
    if (time) {
        if (b[0]) strcat(b, (o & 128) ? " " : "'T'");
        strcat(b, "HH"); strcat(b, ts); strcat(b, "mm"); strcat(b, ts); strcat(b, "ss");
        if (o & 2048) strcat(b, ".SSS");
    }
    if (zone) strcat(b, (o & 1024) ? "XXXXX" : "XXXX");
    return put(out, cap, b, -1);
}

/* ===================================================================== números */

static int icu_nstyle(int ns) {
    switch (ns) {
    case FDN_NS_NONE: return 1;              /* decimal sem agrupamento e sem fração (ajustado em fdn_nf_open) */
    case FDN_NS_DECIMAL: return 1;
    case FDN_NS_CURRENCY: return 2;
    case FDN_NS_PERCENT: return 3;
    case FDN_NS_SCIENTIFIC: return 4;
    case FDN_NS_SPELLOUT: return 5;
    case FDN_NS_ORDINAL: return 6;
    case FDN_NS_CURRENCY_ISO: return 10;
    case FDN_NS_CURRENCY_PLURAL: return 11;
    case FDN_NS_CURRENCY_ACCOUNTING: return 12;
    }
    return 1;
}

void *fdn_nf_open(int ns_style, const char *locale) {
    UErrorCode e = U_ZERO_ERROR;
    char loc[160];
    if (!ICU->ok) return 0;
    fdn_loc_canonical(locale, loc, sizeof loc);
    void *nf = ICU->unum_open(icu_nstyle(ns_style), 0, 0, loc, 0, &e);
    if (U_FAILURE(e)) return 0;
    if (ns_style == FDN_NS_NONE) {
        ICU->unum_setAttribute(nf, 1 /* GROUPING_USED */, 0);
        ICU->unum_setAttribute(nf, 6 /* MAX_FRACTION_DIGITS */, 0);
        ICU->unum_setAttribute(nf, 7 /* MIN_FRACTION_DIGITS */, 0);
    }
    /* a Foundation arredonda meio para o par no estilo decimal (UNUM_ROUND_HALFEVEN, que já é o padrão da ICU) */
    return nf;
}
void fdn_nf_close(void *nf) { if (nf && ICU->unum_close) ICU->unum_close(nf); }

int fdn_nf_format_double(void *nf, double v, char *out, int cap) {
    UChar u[512];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!nf) return 0;
    int32_t n = ICU->unum_formatDouble(nf, v, u, 512, 0, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}
int fdn_nf_format_int64(void *nf, int64_t v, char *out, int cap) {
    UChar u[512];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!nf) return 0;
    int32_t n = ICU->unum_formatInt64(nf, v, u, 512, 0, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}
int fdn_nf_format_decimal(void *nf, const char *dec, char *out, int cap) {
    UChar u[512];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!nf || !ICU->unum_formatDecimal) return 0;
    int32_t n = ICU->unum_formatDecimal(nf, dec, (int32_t)strlen(dec), u, 512, 0, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}
int fdn_nf_parse(void *nf, const char *s, double *v, int64_t *iv, int *is_int, int32_t *end, int32_t *len16) {
    UErrorCode e = U_ZERO_ERROR;
    int32_t n, pos = 0;
    UChar *u;
    if (!nf || !s) return 0;
    u = fdn_u16_dup(s, &n);
    if (!u) return 0;
    double d = ICU->unum_parseDouble(nf, u, n, &pos, &e);
    if (len16) *len16 = n;
    if (U_FAILURE(e) || pos == 0) { free(u); return 0; }
    if (end) *end = pos;
    *v = d;
    int integral = d == floor(d) && fabs(d) < 9.2e18;
    if (integral) {
        int32_t p2 = 0;
        UErrorCode e2 = U_ZERO_ERROR;
        int64_t i = ICU->unum_parseInt64(nf, u, n, &p2, &e2);
        if (!U_FAILURE(e2) && p2 == pos) { if (iv) *iv = i; } else if (iv) *iv = (int64_t)d;
    }
    if (is_int) *is_int = integral;
    free(u);
    return 1;
}
int fdn_nf_get_attr(void *nf, int a) { return nf ? ICU->unum_getAttribute(nf, a) : 0; }
void fdn_nf_set_attr(void *nf, int a, int v) { if (nf) ICU->unum_setAttribute(nf, a, v); }
double fdn_nf_get_dattr(void *nf, int a) { return nf ? ICU->unum_getDoubleAttribute(nf, a) : 0; }
void fdn_nf_set_dattr(void *nf, int a, double v) { if (nf) ICU->unum_setDoubleAttribute(nf, a, v); }
int fdn_nf_get_symbol(void *nf, int sym, char *out, int cap) {
    UChar u[64];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!nf) return 0;
    int32_t n = ICU->unum_getSymbol(nf, sym, u, 64, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}
void fdn_nf_set_symbol(void *nf, int sym, const char *s) {
    UChar u[64];
    UErrorCode e = U_ZERO_ERROR;
    if (!nf || !s) return;
    int32_t n = fdn_u16_from_utf8(s, -1, u, 64);
    ICU->unum_setSymbol(nf, sym, u, n, &e);
}
int fdn_nf_get_text(void *nf, int which, char *out, int cap) {
    UChar u[128];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!nf) return 0;
    int32_t n = ICU->unum_getTextAttribute(nf, which, u, 128, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}
void fdn_nf_set_text(void *nf, int which, const char *s) {
    UChar u[128];
    UErrorCode e = U_ZERO_ERROR;
    if (!nf || !s) return;
    int32_t n = fdn_u16_from_utf8(s, -1, u, 128);
    ICU->unum_setTextAttribute(nf, which, u, n, &e);
}
int fdn_nf_pattern(void *nf, char *out, int cap) {
    UChar u[256];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!nf) return 0;
    int32_t n = ICU->unum_toPattern(nf, 0, u, 256, &e);
    return U_FAILURE(e) ? 0 : put16(out, cap, u, n);
}
int fdn_nf_apply_pattern(void *nf, const char *p) {
    UChar u[256];
    UErrorCode e = U_ZERO_ERROR;
    if (!nf || !p) return 0;
    int32_t n = fdn_u16_from_utf8(p, -1, u, 256);
    ICU->unum_applyPattern(nf, 0, u, n, 0, &e);
    return !U_FAILURE(e);
}
int fdn_currency_digits(const char *code, double *rounding) {
    UChar u[4];
    UErrorCode e = U_ZERO_ERROR;
    if (rounding) *rounding = 0;
    if (!ICU->ok || !code || strlen(code) != 3) return -1;
    fdn_u16_from_utf8(code, 3, u, 4);
    u[3] = 0;
    int32_t d = ICU->ucurr_getDefaultFractionDigits(u, &e);
    return U_FAILURE(e) ? -1 : d;
}

/* ===================================================================== unidades, listas, extenso */

int fdn_unit_format(const char *locale, const char *unit, double v, int width, int frac, char *out, int cap) {
    char sk[160], loc[160];
    UChar u16[160], r[256];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    if (!ICU->ok || !ICU->unumf_openForSkeletonAndLocale) return 0;
    fdn_loc_canonical(locale, loc, sizeof loc);
    const char *w = width == 1 ? "unit-width-narrow" : width == 2 ? "unit-width-full-name" : "unit-width-short";
    if (frac >= 0) snprintf(sk, sizeof sk, "measure-unit/%s %s .%.*s", unit, w, frac > 0 ? frac : 0, "##########");
    else snprintf(sk, sizeof sk, "measure-unit/%s %s", unit, w);
    if (frac == 0) snprintf(sk, sizeof sk, "measure-unit/%s %s precision-integer", unit, w);
    int32_t sl = fdn_u16_from_utf8(sk, -1, u16, 160);
    void *f = ICU->unumf_openForSkeletonAndLocale(u16, sl, loc, &e);
    if (U_FAILURE(e) || !f) return 0;
    void *res = ICU->unumf_openResult(&e);
    ICU->unumf_formatDouble(f, v, res, &e);
    int32_t n = U_FAILURE(e) ? 0 : ICU->unumf_resultToString(res, r, 256, &e);
    ICU->unumf_closeResult(res);
    ICU->unumf_close(f);
    return U_FAILURE(e) ? 0 : put16(out, cap, r, n);
}

int fdn_list_format(const char *locale, int type, int width, const char *const *items, int n, char *out, int cap) {
    UErrorCode e = U_ZERO_ERROR;
    char loc[160];
    if (cap > 0) out[0] = 0;
    if (n <= 0) return 0;
    if (!ICU->ok || !ICU->ulistfmt_openForType) {
        /* sem ICU: vírgulas */
        int o = 0;
        for (int i = 0; i < n; i++) o += snprintf(out + (o < cap ? o : cap), (size_t)(o < cap ? cap - o : 0), "%s%s", i ? ", " : "", items[i]);
        return o;
    }
    fdn_loc_canonical(locale, loc, sizeof loc);
    void *lf = ICU->ulistfmt_openForType(loc, type, width, &e);
    if (U_FAILURE(e) || !lf) return 0;
    UChar **u = malloc(sizeof(UChar *) * (size_t)n);
    int32_t *l = malloc(sizeof(int32_t) * (size_t)n);
    UChar *r = malloc(sizeof(UChar) * 8192);
    int ret = 0;
    if (u && l && r) {
        for (int i = 0; i < n; i++) u[i] = fdn_u16_dup(items[i] ? items[i] : "", &l[i]);
        int32_t m = ICU->ulistfmt_format(lf, (const UChar *const *)u, l, n, r, 8192, &e);
        if (!U_FAILURE(e)) ret = put16(out, cap, r, m);
        for (int i = 0; i < n; i++) free(u[i]);
    }
    free(u); free(l); free(r);
    ICU->ulistfmt_close(lf);
    return ret;
}

int fdn_spellout(const char *locale, double v, char *out, int cap) {
    void *nf = fdn_nf_open(FDN_NS_SPELLOUT, locale);
    int r = fdn_nf_format_double(nf, v, out, cap);
    fdn_nf_close(nf);
    return r;
}

int fdn_reldate(const char *locale, double v, int unit, int width, int named, int spellout, char *out, int cap) {
    UErrorCode e = U_ZERO_ERROR;
    UChar r[256];
    char loc[160];
    if (cap > 0) out[0] = 0;
    if (!ICU->ok || !ICU->ureldatefmt_open) return 0;
    fdn_loc_canonical(locale, loc, sizeof loc);
    void *nf = spellout ? fdn_nf_open(FDN_NS_SPELLOUT, loc) : 0;
    void *f = ICU->ureldatefmt_open(loc, nf, width, 0x100 /* UDISPCTX_CAPITALIZATION_NONE */, &e);
    if (U_FAILURE(e) || !f) { fdn_nf_close(nf); return 0; }
    int32_t n = named ? ICU->ureldatefmt_format(f, v, unit, r, 256, &e) : ICU->ureldatefmt_formatNumeric(f, v, unit, r, 256, &e);
    ICU->ureldatefmt_close(f);   /* fecha também o nf adotado */
    return U_FAILURE(e) ? 0 : put16(out, cap, r, n);
}

int fdn_interval_format(const char *locale, const char *skeleton, const char *tz, double a, double b, char *out, int cap) {
    UErrorCode e = U_ZERO_ERROR;
    UChar sk[64], z[96], r[512];
    char loc[160];
    if (cap > 0) out[0] = 0;
    if (!ICU->ok || !ICU->udtitvfmt_open) return 0;
    fdn_loc_canonical(locale, loc, sizeof loc);
    int32_t sl = fdn_u16_from_utf8(skeleton, -1, sk, 64);
    int32_t zl = fdn_u16_from_utf8(tz && tz[0] ? tz : "GMT", -1, z, 96);
    void *f = ICU->udtitvfmt_open(loc, sk, sl, z, zl, &e);
    if (U_FAILURE(e) || !f) return 0;
    int32_t n = ICU->udtitvfmt_format(f, a * 1000.0, b * 1000.0, r, 512, 0, &e);
    ICU->udtitvfmt_close(f);
    return U_FAILURE(e) ? 0 : put16(out, cap, r, n);
}

/* --------------------------------------------------------------------- bytes */

static const char *zero_word(const char *lang) {
    static const char *const t[][2] = {
        {"pt", "Zero"}, {"es", "Cero"}, {"fr", "Zéro"}, {"de", "Null"}, {"it", "Zero"}, {"nl", "Nul"}, {"sv", "Noll"},
        {"da", "Nul"}, {"nb", "Null"}, {"fi", "Nolla"}, {"pl", "Zero"}, {"ru", "Ноль"}, {"ja", "ゼロ"}, {"zh", "零"},
        {"ko", "0"}, {"tr", "Sıfır"}, {"cs", "Nula"},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) if (!strcmp(lang, t[i][0])) return t[i][1];
    return "Zero";
}

int fdn_bytes_format(const char *locale, int64_t bytes, unsigned allowed, int style, int nonnumeric, int unit, int count,
                     int actual, int adaptive, int zero_pad, char *out, int cap) {
    static const char *const sym[] = {"bytes", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    char lang[16], num[96], word[160], res[320];
    int binary = style == 1 || style == 3;
    double base = binary ? 1024.0 : 1000.0;
    double v = (double)(bytes < 0 ? -bytes : bytes);
    int u = 0;
    unsigned mask = allowed ? allowed : 0xFFFF;
    if (cap > 0) out[0] = 0;
    fdn_locale_part(locale, FDN_LP_LANGUAGE, lang, sizeof lang);
    /* a maior unidade permitida em que o valor fica >= 1; os bits sobem de byte (1) até YB (0xFF00) */
    int lowest = 0;
    while (lowest < 8 && !(mask & (1u << lowest))) lowest++;
    if (lowest > 8) lowest = 0;
    u = lowest;
    for (int k = lowest + 1; k <= 8; k++) {
        unsigned bit = k == 8 ? 0xFF00u : (1u << k);
        if (!(mask & bit)) continue;
        if (v >= pow(base, k) || (v >= pow(base, k) * 0.9995 && adaptive)) u = k;
    }
    /* File/Decimal: zero bytes vira "Zero KB" (allowsNonnumericFormatting) */
    if (bytes == 0 && nonnumeric) {
        if (!unit) return put(out, cap, zero_word(lang), -1);
        snprintf(res, sizeof res, "%s %s", zero_word(lang), (mask & 2) || !allowed ? "KB" : sym[u]);
        if (!count) return put(out, cap, (mask & 2) || !allowed ? "KB" : sym[u], -1);
        return put(out, cap, res, -1);
    }
    double scaled = v / pow(base, u);
    int frac = 0;
    if (u > 0) frac = adaptive ? (u == 1 ? 0 : u == 2 ? 1 : 2) : 2;
    void *nf = fdn_nf_open(FDN_NS_DECIMAL, locale);
    fdn_nf_set_attr(nf, 6, frac);
    fdn_nf_set_attr(nf, 7, zero_pad ? frac : 0);
    fdn_nf_set_attr(nf, 11, 6 /* HALFUP */);
    fdn_nf_format_double(nf, bytes < 0 ? -scaled : scaled, num, sizeof num);
    fdn_nf_close(nf);
    if (u == 0) {
        /* "1 byte" / "999 bytes" pela ICU, no idioma do locale */
        if (fdn_unit_format(locale, "digital-byte", (double)bytes, 2, 0, word, sizeof word) <= 0) snprintf(word, sizeof word, "%s bytes", num);
        if (!unit) return put(out, cap, num, -1);
        if (!count) {
            /* só a palavra: tira o número do começo */
            const char *p = word;
            while (*p && *p != ' ' && *p != '\xc2') p++;
            while (*p == ' ') p++;
            return put(out, cap, *p ? p : word, -1);
        }
        return put(out, cap, word, -1);
    }
    if (!unit) snprintf(res, sizeof res, "%s", num);
    else if (!count) snprintf(res, sizeof res, "%s", sym[u]);
    else snprintf(res, sizeof res, "%s %s", num, sym[u]);
    if (actual && unit && count) {
        char all[96], bw[96];
        void *nf2 = fdn_nf_open(FDN_NS_DECIMAL, locale);
        fdn_nf_format_int64(nf2, bytes, all, sizeof all);
        fdn_nf_close(nf2);
        if (fdn_unit_format(locale, "digital-byte", (double)bytes, 2, 0, bw, sizeof bw) > 0) {
            /* troca o número da ICU pelo número com agrupamento do locale */
            const char *p = bw;
            while (*p && *p != ' ') p++;
            char tmp[640];
            snprintf(tmp, sizeof tmp, "%s (%s%s)", res, all, p);
            return put(out, cap, tmp, -1);
        }
        char tmp[640];
        snprintf(tmp, sizeof tmp, "%s (%s bytes)", res, all);
        return put(out, cap, tmp, -1);
    }
    return put(out, cap, res, -1);
}

/* --------------------------------------------------------------------- durações */

int fdn_duration_format(const char *locale, int style, const fdn_dur_part *p, int n, int negative, char *out, int cap) {
    char parts[8][256];
    const char *ptr[8];
    if (cap > 0) out[0] = 0;
    if (n <= 0) return 0;
    if (n > 8) n = 8;
    if (style == 0) {
        /* posicional: "1:02:03", "2:03" (a primeira parte sem zero à esquerda), dias e mais ficam "1d 2:03:04" como o macOS */
        char b[128] = "";
        int o = 0;
        if (negative) o += snprintf(b + o, sizeof b - (size_t)o, "-");
        for (int i = 0; i < n; i++) {
            const char *un = p[i].unit;
            int pos = !strcmp(un, "duration-hour") || !strcmp(un, "duration-minute") || !strcmp(un, "duration-second");
            if (!pos) {
                char w[64];
                fdn_unit_format(locale, un, p[i].value, 1, 0, w, sizeof w);
                o += snprintf(b + o, sizeof b - (size_t)o, "%s ", w);
                continue;
            }
            int first_pos = 1;
            for (int k = 0; k < i; k++)
                if (!strcmp(p[k].unit, "duration-hour") || !strcmp(p[k].unit, "duration-minute") || !strcmp(p[k].unit, "duration-second")) first_pos = 0;
            double v = p[i].value;
            if (first_pos) o += snprintf(b + o, sizeof b - (size_t)o, "%.0f", floor(v));
            else o += snprintf(b + o, sizeof b - (size_t)o, ":%02.0f", floor(v));
        }
        return put(out, cap, b, -1);
    }
    int width = style == 1 ? 1 : style == 2 || style == 5 ? 0 : 2;
    for (int i = 0; i < n; i++) {
        double v = p[i].value;
        int frac = v == floor(v) ? 0 : 1;
        fdn_unit_format(locale, p[i].unit, negative && i == 0 ? -v : v, width, frac, parts[i], sizeof parts[i]);
        if (style == 4) {
            /* por extenso: o número da ICU vira o extenso do mesmo locale */
            char sp[96], tmp[384];
            fdn_spellout(locale, v, sp, sizeof sp);
            const char *q = parts[i];
            while (*q && ((*q >= '0' && *q <= '9') || *q == '.' || *q == ',' || *q == '-')) q++;
            snprintf(tmp, sizeof tmp, "%.95s%.250s", sp, q);
            snprintf(parts[i], sizeof parts[i], "%.250s", tmp);
        }
        if (style == 5) {
            /* breve: "1hr 2min" — o curto sem o espaço entre número e unidade */
            char *sp = strchr(parts[i], ' ');
            if (sp) memmove(sp, sp + 1, strlen(sp));
        }
        ptr[i] = parts[i];
    }
    if (style == 1 || style == 5) {
        /* abreviado e breve: separados por espaço, sem vírgula */
        int o = 0;
        char b[512] = "";
        for (int i = 0; i < n; i++) o += snprintf(b + o, sizeof b - (size_t)o, "%s%s", i ? " " : "", ptr[i]);
        return put(out, cap, b, -1);
    }
    /* curto, completo e por extenso: a lista de unidades do locale ("1 hora e 2 minutos" / "1 hr, 2 min") */
    return fdn_list_format(locale, 2 /* ULISTFMT_TYPE_UNITS */, style == 2 ? 1 : 0, ptr, n, out, cap);
}

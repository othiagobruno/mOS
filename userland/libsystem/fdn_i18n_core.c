/*
 * mOS libSystem — núcleo de locale, fuso e calendário (fdn_i18n_core.h), sobre a ICU.
 * Sem Objective-C: o teste de host (tools/fdn_i18n_test.c) usa a ICU do Linux com a mesma tabela.
 */
#include "fdn_i18n_core.h"
#ifdef FDN_ICU_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include "libc.h"
int snprintf(char *b, size_t n, const char *fmt, ...);
double floor(double x);
char *getenv(const char *n);
int open(const char *p, int f, ...);
long read(int fd, void *b, size_t n);
int close(int fd);
long readlink(const char *p, char *b, size_t n);
char *strstr(const char *h, const char *n);
#endif

#define ICU (fdn_icu())

static int put(char *out, int cap, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    if (cap > 0) {
        int k = n < cap - 1 ? n : cap - 1;
        memcpy(out, s, (size_t)k);
        out[k] = 0;
    }
    return n;
}
static int put_u16(char *out, int cap, const UChar *s, int32_t n) {
    int r = fdn_utf8_from_u16(s, n, out, cap);
    return r;
}

/* ================================================================ locale */

int fdn_loc_canonical(const char *in, char *out, int cap) {
    char tmp[160], res[160];
    UErrorCode e = U_ZERO_ERROR;
    const fdn_icu_t *I = ICU;
    if (!in) in = "";
    if (!I->ok) {   /* sem ICU: troca '-' por '_' e mais nada */
        int n = put(out, cap, in, -1);
        for (int i = 0; out[i]; i++) if (out[i] == '-') out[i] = '_';
        return n;
    }
    /* BCP 47 ("pt-BR", "zh-Hant-TW", "en-US-u-ca-buddhist") vem pelo forLanguageTag; o resto é o formato ICU */
    if (strchr(in, '-') && !strchr(in, '_') && !strchr(in, '@')) {
        int32_t parsed = 0;
        I->uloc_forLanguageTag(in, tmp, sizeof tmp, &parsed, &e);
        if (U_FAILURE(e) || !tmp[0]) { e = U_ZERO_ERROR; put(tmp, sizeof tmp, in, -1); }
    } else put(tmp, sizeof tmp, in, -1);
    e = U_ZERO_ERROR;
    I->uloc_canonicalize(tmp, res, sizeof res, &e);
    if (U_FAILURE(e)) put(res, sizeof res, tmp, -1);
    /* a Foundation escreve "root" como "" e as keywords em minúsculas, como a ICU */
    if (!strcmp(res, "root")) res[0] = 0;
    return put(out, cap, res, -1);
}

int fdn_loc_language_tag(const char *in, char *out, int cap) {
    char canon[160], tag[160];
    UErrorCode e = U_ZERO_ERROR;
    fdn_loc_canonical(in, canon, sizeof canon);
    if (!ICU->ok || !ICU->uloc_toLanguageTag) {
        int n = put(out, cap, canon, -1);
        for (int i = 0; out[i]; i++) if (out[i] == '_') out[i] = '-';
        return n;
    }
    ICU->uloc_toLanguageTag(canon, tag, sizeof tag, 0, &e);
    if (U_FAILURE(e)) return put(out, cap, canon, -1);
    if (!strcmp(tag, "und")) tag[0] = 0;
    return put(out, cap, tag, -1);
}

static const char *kw_of(int part) {
    switch (part) {
    case FDN_LP_CALENDAR: return "calendar";
    case FDN_LP_COLLATION: return "collation";
    case FDN_LP_CURRENCY: return "currency";
    case FDN_LP_NUMBERS: return "numbers";
    }
    return 0;
}

int fdn_locale_part(const char *loc, int part, char *out, int cap) {
    char canon[160], buf[96];
    UErrorCode e = U_ZERO_ERROR;
    const fdn_icu_t *I = ICU;
    int n = 0;
    fdn_loc_canonical(loc, canon, sizeof canon);
    buf[0] = 0;
    if (!I->ok) {   /* "ll_Ssss_RR" à mão */
        char *p = canon, *f[4] = {0};
        int k = 0;
        char *at = strchr(canon, '@');
        if (at) *at = 0;
        while (p && k < 4) { f[k++] = p; p = strchr(p, '_'); if (p) *p++ = 0; }
        const char *lang = f[0] ? f[0] : "", *script = "", *region = "", *var = "";
        int i = 1;
        if (f[i] && strlen(f[i]) == 4) script = f[i++];
        if (f[i]) region = f[i++];
        if (f[i]) var = f[i];
        const char *r = part == FDN_LP_LANGUAGE ? lang : part == FDN_LP_SCRIPT ? script : part == FDN_LP_REGION ? region :
                        part == FDN_LP_VARIANT ? var : "";
        return put(out, cap, r, -1);
    }
    switch (part) {
    case FDN_LP_LANGUAGE: n = I->uloc_getLanguage(canon, buf, sizeof buf, &e); break;
    case FDN_LP_SCRIPT: n = I->uloc_getScript(canon, buf, sizeof buf, &e); break;
    case FDN_LP_REGION: n = I->uloc_getCountry(canon, buf, sizeof buf, &e); break;
    case FDN_LP_VARIANT: n = I->uloc_getVariant(canon, buf, sizeof buf, &e); break;
    case FDN_LP_BASE: n = I->uloc_getBaseName(canon, buf, sizeof buf, &e); break;
    default: n = I->uloc_getKeywordValue(canon, kw_of(part), buf, sizeof buf, &e); break;
    }
    if (U_FAILURE(e) || n < 0) buf[0] = 0;
    return put(out, cap, buf, -1);
}

int fdn_loc_with_keyword(const char *loc, const char *key, const char *val, char *out, int cap) {
    char canon[160];
    UErrorCode e = U_ZERO_ERROR;
    fdn_loc_canonical(loc, canon, sizeof canon);
    if (ICU->ok && ICU->uloc_setKeywordValue) {
        ICU->uloc_setKeywordValue(key, val, canon, sizeof canon, &e);
        if (U_FAILURE(e)) return put(out, cap, canon, -1);
        return put(out, cap, canon, -1);
    }
    char t[200];
    snprintf(t, sizeof t, "%s%c%s=%s", canon, strchr(canon, '@') ? ';' : '@', key, val ? val : "");
    return put(out, cap, t, -1);
}

static int display_kw(const char *display, const char *key, const char *val, char *out, int cap) {
    char loc[128];
    UChar u[256];
    UErrorCode e = U_ZERO_ERROR;
    snprintf(loc, sizeof loc, "und@%s=%s", key, val);
    int32_t n = ICU->uloc_getDisplayKeywordValue(loc, key, display, u, 256, &e);
    if (U_FAILURE(e) || n <= 0) return 0;
    return put_u16(out, cap, u, n);
}

int fdn_loc_display(const char *display_loc, int kind, const char *code, char *out, int cap) {
    const fdn_icu_t *I = ICU;
    char disp[160];
    UChar u[512];
    UErrorCode e = U_ZERO_ERROR;
    int32_t n = 0;
    if (cap > 0) out[0] = 0;
    if (!I->ok || !code || !code[0]) return 0;
    fdn_loc_canonical(display_loc, disp, sizeof disp);
    switch (kind) {
    case FDN_LD_IDENT: {
        char c[160];
        fdn_loc_canonical(code, c, sizeof c);
        n = I->uloc_getDisplayName(c, disp, u, 512, &e);
        break;
    }
    case FDN_LD_LANGUAGE: n = I->uloc_getDisplayLanguage(code, disp, u, 512, &e); break;
    case FDN_LD_REGION: {
        char c[32];
        snprintf(c, sizeof c, "und_%s", code);
        n = I->uloc_getDisplayCountry(c, disp, u, 512, &e);
        break;
    }
    case FDN_LD_SCRIPT: {
        char c[32];
        snprintf(c, sizeof c, "und_%s", code);
        n = I->uloc_getDisplayScript(c, disp, u, 512, &e);
        break;
    }
    case FDN_LD_VARIANT: {
        char c[48];
        snprintf(c, sizeof c, "und_US_%s", code);
        n = I->uloc_getDisplayVariant(c, disp, u, 512, &e);
        break;
    }
    case FDN_LD_CALENDAR: return display_kw(disp, "calendar", code, out, cap);
    case FDN_LD_COLLATION: return display_kw(disp, "collation", code, out, cap);
    case FDN_LD_CURRENCY_NAME:
    case FDN_LD_CURRENCY_SYMBOL: {
        UChar cc[4];
        UBool choice = 0;
        int32_t len = 0;
        fdn_u16_from_utf8(code, 3, cc, 4);
        cc[3] = 0;
        const UChar *r = I->ucurr_getName(cc, disp, kind == FDN_LD_CURRENCY_NAME ? 1 : 0, &choice, &len, &e);
        if (U_FAILURE(e) || !r) return 0;
        /* a ICU devolve o próprio código quando não conhece a moeda; a Foundation devolve nil */
        if (kind == FDN_LD_CURRENCY_NAME && len == 3 && r[0] == cc[0] && r[1] == cc[1] && r[2] == cc[2]) return 0;
        return put_u16(out, cap, r, len);
    }
    default: return 0;
    }
    /* a ICU devolve o código quando não tem nome (U_USING_DEFAULT_WARNING = -127, U_USING_FALLBACK = -128 é aceito) */
    if (U_FAILURE(e) || n <= 0 || e == -127) return 0;
    return put_u16(out, cap, u, n);
}

static int num_symbol(const char *loc, int style, int sym, char *out, int cap) {
    UErrorCode e = U_ZERO_ERROR;
    void *nf = ICU->unum_open(style, 0, 0, loc, 0, &e);
    UChar u[64];
    int32_t n;
    if (U_FAILURE(e) || !nf) return 0;
    n = ICU->unum_getSymbol(nf, sym, u, 64, &e);
    ICU->unum_close(nf);
    if (U_FAILURE(e)) return 0;
    return put_u16(out, cap, u, n);
}

int fdn_loc_info(const char *loc, int what, char *out, int cap) {
    const fdn_icu_t *I = ICU;
    char canon[160];
    UErrorCode e = U_ZERO_ERROR;
    if (cap > 0) out[0] = 0;
    fdn_loc_canonical(loc, canon, sizeof canon);
    if (!I->ok) {
        int pt = !strncmp(canon, "pt", 2) || !strncmp(canon, "de", 2) || !strncmp(canon, "es", 2) || !strncmp(canon, "fr", 2) || !strncmp(canon, "it", 2);
        switch (what) {
        case FDN_LN_DECIMAL: return put(out, cap, pt ? "," : ".", -1);
        case FDN_LN_GROUPING: return put(out, cap, pt ? "." : ",", -1);
        case FDN_LN_CALENDAR_ID: return put(out, cap, "gregorian", -1);
        }
        return 0;
    }
    switch (what) {
    case FDN_LN_DECIMAL: return num_symbol(canon, 1, 0, out, cap);
    case FDN_LN_GROUPING: return num_symbol(canon, 1, 1, out, cap);
    case FDN_LN_CURRENCY_SYMBOL: return num_symbol(canon, 2, 8, out, cap);
    case FDN_LN_CURRENCY_CODE: {
        UChar u[8];
        int32_t n = I->ucurr_forLocale(canon, u, 8, &e);
        if (U_FAILURE(e) || n <= 0) return 0;
        return put_u16(out, cap, u, n);
    }
    case FDN_LN_QUOTE_BEGIN: case FDN_LN_QUOTE_END: case FDN_LN_ALT_QUOTE_BEGIN: case FDN_LN_ALT_QUOTE_END: {
        void *ld = I->ulocdata_open(canon, &e);
        UChar u[8];
        int32_t n;
        if (U_FAILURE(e) || !ld) return 0;
        n = I->ulocdata_getDelimiter(ld, what - FDN_LN_QUOTE_BEGIN, u, 8, &e);
        I->ulocdata_close(ld);
        if (U_FAILURE(e)) return 0;
        return put_u16(out, cap, u, n);
    }
    case FDN_LN_CALENDAR_ID: {
        char kw[48];
        if (fdn_locale_part(canon, FDN_LP_CALENDAR, kw, sizeof kw) > 0) {
            /* a Foundation chama "gregorian" o que o ICU chama "gregorian"; "ethiopic-amete-alem" -> "ethiopic-amete-alem" */
            return put(out, cap, kw, -1);
        }
        void *c = I->ucal_open(0, 0, canon, 0, &e);
        if (U_FAILURE(e) || !c) return put(out, cap, "gregorian", -1);
        const char *t = I->ucal_getType(c, &e);
        int r = put(out, cap, (!U_FAILURE(e) && t) ? t : "gregorian", -1);
        I->ucal_close(c);
        return r;
    }
    case FDN_LN_MEASUREMENT: {
        int32_t m = I->ulocdata_getMeasurementSystem(canon, &e);
        if (U_FAILURE(e)) m = 0;
        return put(out, cap, m == 1 ? "U.S." : m == 2 ? "U.K." : "Metric", -1);
    }
    }
    return 0;
}

int fdn_loc_uses_metric(const char *loc) {
    char m[16];
    fdn_loc_info(loc, FDN_LN_MEASUREMENT, m, sizeof m);
    return strcmp(m, "U.S.") != 0;
}

int fdn_loc_direction(const char *loc, int line) {
    UErrorCode e = U_ZERO_ERROR;
    char canon[160];
    int32_t d;
    if (!ICU->ok) return 1;
    fdn_loc_canonical(loc, canon, sizeof canon);
    d = line ? ICU->uloc_getLineOrientation(canon, &e) : ICU->uloc_getCharacterOrientation(canon, &e);
    if (U_FAILURE(e) || d < 0 || d > 3) return 0;
    return d + 1;   /* ULOC_LAYOUT_LTR=0 -> NSLocaleLanguageDirectionLeftToRight=1 */
}

static void *cal_for_locale(const char *loc) {
    UErrorCode e = U_ZERO_ERROR;
    char canon[160];
    UChar gmt[4] = {'G', 'M', 'T', 0};
    fdn_loc_canonical(loc, canon, sizeof canon);
    void *c = ICU->ucal_open(gmt, 3, canon, 0, &e);
    return U_FAILURE(e) ? 0 : c;
}

int fdn_loc_first_weekday(const char *loc) {
    void *c;
    int r = 1;
    if (!ICU->ok || !(c = cal_for_locale(loc))) return 1;
    r = ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK);
    ICU->ucal_close(c);
    return r >= 1 && r <= 7 ? r : 1;
}

int fdn_loc_min_days(const char *loc) {
    void *c;
    int r = 1;
    if (!ICU->ok || !(c = cal_for_locale(loc))) return 1;
    r = ICU->ucal_getAttribute(c, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
    ICU->ucal_close(c);
    return r >= 1 && r <= 7 ? r : 1;
}

int fdn_loc_each_available(fdn_each_fn cb, void *ctx) {
    int n;
    if (!ICU->ok) { cb("en_US", ctx); cb("pt_BR", ctx); return 2; }
    n = ICU->uloc_countAvailable();
    for (int i = 0; i < n; i++) cb(ICU->uloc_getAvailable(i), ctx);
    return n;
}

static int each_list(const char *const *l, fdn_each_fn cb, void *ctx) {
    int n = 0;
    for (; l && l[n]; n++) cb(l[n], ctx);
    return n;
}
int fdn_loc_each_iso_language(fdn_each_fn cb, void *ctx) { return ICU->ok ? each_list(ICU->uloc_getISOLanguages(), cb, ctx) : 0; }
int fdn_loc_each_iso_region(fdn_each_fn cb, void *ctx) { return ICU->ok ? each_list(ICU->uloc_getISOCountries(), cb, ctx) : 0; }

int fdn_loc_each_currency(int common_only, fdn_each_fn cb, void *ctx) {
    UErrorCode e = U_ZERO_ERROR;
    void *en;
    const char *s;
    int n = 0;
    if (!ICU->ok) return 0;
    /* UCURR_COMMON|UCURR_NON_DEPRECATED (1|8) para commonISOCurrencyCodes; UCURR_ALL para ISOCurrencyCodes */
    en = ICU->ucurr_openISOCurrencies(common_only ? (1u | 8u) : 0x7fffffffu, &e);
    if (U_FAILURE(e) || !en) return 0;
    while ((s = ICU->uenum_next(en, 0, &e)) && !U_FAILURE(e)) { cb(s, ctx); n++; }
    ICU->uenum_close(en);
    return n;
}

int fdn_loc_exemplar(const char *loc, void (*cb)(uint32_t, uint32_t, void *), void *ctx) {
    UErrorCode e = U_ZERO_ERROR;
    char canon[160];
    void *ld, *set;
    int n, k = 0;
    if (!ICU->ok) return 0;
    fdn_loc_canonical(loc, canon, sizeof canon);
    ld = ICU->ulocdata_open(canon, &e);
    if (U_FAILURE(e) || !ld) return 0;
    set = ICU->ulocdata_getExemplarSet(ld, 0, 0, 0 /* ULOCDATA_ES_STANDARD */, &e);
    ICU->ulocdata_close(ld);
    if (U_FAILURE(e) || !set) return 0;
    n = ICU->uset_getItemCount(set);
    for (int i = 0; i < n; i++) {
        UChar32 lo, hi;
        UChar str[32];
        UErrorCode e2 = U_ZERO_ERROR;
        int32_t sl = ICU->uset_getItem(set, i, &lo, &hi, str, 32, &e2);
        if (U_FAILURE(e2)) continue;
        if (sl == 0) { cb((uint32_t)lo, (uint32_t)hi, ctx); k++; }   /* faixa; as strings (dígrafos) ficam fora, como no NSCharacterSet */
    }
    ICU->uset_close(set);
    return k;
}

/* ================================================================ fuso */

int fdn_tz_parse_fixed(const char *name, long *secs) {
    const char *p = name;
    long sign = 1, h = 0, m = 0, s = 0;
    int digits = 0;
    if (!p) return -1;
    if (!strcmp(p, "GMT") || !strcmp(p, "UTC") || !strcmp(p, "Z") || !strcmp(p, "UT")) { *secs = 0; return 0; }
    if (!strncmp(p, "GMT", 3) || !strncmp(p, "UTC", 3)) p += 3;
    if (*p != '+' && *p != '-') return -1;
    sign = *p == '-' ? -1 : 1;
    p++;
    /* "3", "03", "0300", "03:00", "030000" */
    const char *q = p;
    while (*q >= '0' && *q <= '9') { digits++; q++; }
    if (digits == 0) return -1;
    if (*q == ':') {
        if (digits > 2) return -1;
        h = strtol(p, 0, 10);
        p = q + 1;
        if (!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9')) return -1;
        m = (p[0] - '0') * 10 + (p[1] - '0');
        p += 2;
        if (*p == ':') { p++; s = strtol(p, (char **)&p, 10); }
        if (*p) return -1;
    } else {
        if (*q) return -1;
        if (digits <= 2) h = strtol(p, 0, 10);
        else if (digits <= 4) { long v = strtol(p, 0, 10); h = v / 100; m = v % 100; }
        else if (digits == 6) { long v = strtol(p, 0, 10); h = v / 10000; m = v / 100 % 100; s = v % 100; }
        else return -1;
    }
    if (h > 18 || m > 59 || s > 59) return -1;
    *secs = sign * (h * 3600 + m * 60 + s);
    return 0;
}

void fdn_tz_fixed_name(long secs, char *out, int cap) {
    long a = secs < 0 ? -secs : secs;
    if (secs == 0) { put(out, cap, "GMT", -1); return; }
    snprintf(out, (size_t)cap, "GMT%c%02ld%02ld", secs < 0 ? '-' : '+', a / 3600, a % 3600 / 60);
}

/* NSTimeZone.abbreviationDictionary do macOS (as chaves que os apps usam com timeZoneWithAbbreviation:) */
static const char *const tz_abbr[][2] = {
    {"ADT", "America/Halifax"}, {"AKDT", "America/Juneau"}, {"AKST", "America/Juneau"}, {"ART", "America/Argentina/Buenos_Aires"},
    {"AST", "America/Halifax"}, {"BDT", "Asia/Dhaka"}, {"BRST", "America/Sao_Paulo"}, {"BRT", "America/Sao_Paulo"},
    {"BST", "Europe/London"}, {"CAT", "Africa/Harare"}, {"CDT", "America/Chicago"}, {"CEST", "Europe/Paris"},
    {"CET", "Europe/Paris"}, {"CLST", "America/Santiago"}, {"CLT", "America/Santiago"}, {"COT", "America/Bogota"},
    {"CST", "America/Chicago"}, {"EAT", "Africa/Addis_Ababa"}, {"EDT", "America/New_York"}, {"EEST", "Europe/Athens"},
    {"EET", "Europe/Athens"}, {"EST", "America/New_York"}, {"GMT", "GMT"}, {"GST", "Asia/Dubai"},
    {"HKT", "Asia/Hong_Kong"}, {"HST", "Pacific/Honolulu"}, {"ICT", "Asia/Bangkok"}, {"IRST", "Asia/Tehran"},
    {"IST", "Asia/Kolkata"}, {"JST", "Asia/Tokyo"}, {"KST", "Asia/Seoul"}, {"MDT", "America/Denver"},
    {"MSD", "Europe/Moscow"}, {"MSK", "Europe/Moscow"}, {"MST", "America/Phoenix"}, {"NZDT", "Pacific/Auckland"},
    {"NZST", "Pacific/Auckland"}, {"PDT", "America/Los_Angeles"}, {"PET", "America/Lima"}, {"PHT", "Asia/Manila"},
    {"PKT", "Asia/Karachi"}, {"PST", "America/Los_Angeles"}, {"SGT", "Asia/Singapore"}, {"TRT", "Europe/Istanbul"},
    {"UTC", "UTC"}, {"WAT", "Africa/Lagos"}, {"WEST", "Europe/Lisbon"}, {"WET", "Europe/Lisbon"}, {"WIT", "Asia/Jakarta"},
};
const char *fdn_tz_from_abbreviation(const char *abbr) {
    if (!abbr) return 0;
    for (size_t i = 0; i < sizeof tz_abbr / sizeof tz_abbr[0]; i++)
        if (!strcmp(abbr, tz_abbr[i][0])) return tz_abbr[i][1];
    return 0;
}

int fdn_tz_canonical(const char *name, char *out, int cap) {
    UChar u[96], r[96];
    UBool sys = 0;
    UErrorCode e = U_ZERO_ERROR;
    int32_t n;
    long secs;
    if (!name || !name[0]) return 0;
    if (fdn_tz_parse_fixed(name, &secs) == 0) {
        if (!strcmp(name, "GMT") || !strcmp(name, "UTC")) return put(out, cap, name, -1);
        fdn_tz_fixed_name(secs, out, cap);
        return (int)strlen(out);
    }
    if (!ICU->ok) return 0;
    n = fdn_u16_from_utf8(name, -1, u, 96);
    n = ICU->ucal_getCanonicalTimeZoneID(u, n, r, 96, &sys, &e);
    if (U_FAILURE(e) || !sys) return 0;
    /* o macOS mantém o nome pedido quando é um alias conhecido ("US/Pacific" continua "US/Pacific") */
    return put(out, cap, name, -1);
}

static int cfg_value(const char *key, char *out, int cap) {
    int fd = open("/Sistema/Biblioteca/Preferencias/com.mos.sistema.cfg", 0);
    char buf[1024];
    long n;
    if (fd < 0) return 0;
    n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;
    size_t kl = strlen(key);
    for (char *p = buf; p && *p; ) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            char *v = p + kl + 1;
            size_t vl = strlen(v);
            while (vl && (v[vl - 1] == '\r' || v[vl - 1] == ' ')) v[--vl] = 0;
            return put(out, cap, v, (int)vl);
        }
        p = nl ? nl + 1 : 0;
    }
    return 0;
}

int fdn_tz_system(char *out, int cap) {
    char v[128], canon[96];
    const char *tz = getenv("TZ");
    if (tz && tz[0] == ':') tz++;
    if (tz && tz[0] && fdn_tz_canonical(tz, canon, sizeof canon) > 0) return put(out, cap, canon, -1);
    long n = readlink("/etc/localtime", v, sizeof v - 1);
    if (n > 0) {
        v[n] = 0;
        char *z = strstr(v, "zoneinfo/");
        if (z && fdn_tz_canonical(z + 9, canon, sizeof canon) > 0) return put(out, cap, canon, -1);
    }
    if (cfg_value("fuso", v, sizeof v) > 0 && fdn_tz_canonical(v, canon, sizeof canon) > 0) return put(out, cap, canon, -1);
    /* o padrão do kernel (kernel/dev/rtc.c: UTC-3, Brasília) */
    return put(out, cap, "America/Sao_Paulo", -1);
}

static void *tz_cal(const char *name, double t) {
    UChar u[96];
    UErrorCode e = U_ZERO_ERROR;
    int32_t n = fdn_u16_from_utf8(name, -1, u, 96);
    void *c = ICU->ucal_open(u, n, "en_US_POSIX", 1 /* UCAL_GREGORIAN */, &e);
    if (U_FAILURE(e) || !c) return 0;
    ICU->ucal_setMillis(c, t * 1000.0, &e);
    return c;
}

long fdn_tz_offset(const char *name, double t, long *dst) {
    long fixed;
    void *c;
    UErrorCode e = U_ZERO_ERROR;
    if (dst) *dst = 0;
    if (fdn_tz_parse_fixed(name, &fixed) == 0) return fixed;
    if (!ICU->ok || !(c = tz_cal(name, t))) return 0;
    long zo = ICU->ucal_get(c, UCAL_ZONE_OFFSET, &e) / 1000;
    long d = ICU->ucal_get(c, UCAL_DST_OFFSET, &e) / 1000;
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    if (dst) *dst = d;
    return zo + d;
}

int fdn_tz_name(const char *name, int style, const char *loc, double t, char *out, int cap) {
    void *c;
    UChar u[128];
    UErrorCode e = U_ZERO_ERROR;
    int32_t n;
    char canon[160];
    long fixed;
    if (cap > 0) out[0] = 0;
    if (fdn_tz_parse_fixed(name, &fixed) == 0) {
        char g[24];
        fdn_tz_fixed_name(fixed, g, sizeof g);
        return put(out, cap, g, -1);
    }
    if (!ICU->ok || !(c = tz_cal(name, t))) return 0;
    fdn_loc_canonical(loc, canon, sizeof canon);
    if (style == FDN_TZN_GENERIC || style == FDN_TZN_SHORT_GENERIC) {
        /* nome genérico ("Horário de Brasília"): o padrão "vvvv"/"v" do udat */
        UChar pat[6] = {'v', 'v', 'v', 'v', 0};
        UChar zid[96];
        int32_t zl = fdn_u16_from_utf8(name, -1, zid, 96);
        if (style == FDN_TZN_SHORT_GENERIC) pat[1] = 0;
        void *df = ICU->udat_open(UDAT_PATTERN, UDAT_PATTERN, canon, zid, zl, pat, style == FDN_TZN_SHORT_GENERIC ? 1 : 4, &e);
        ICU->ucal_close(c);
        if (U_FAILURE(e) || !df) return 0;
        n = ICU->udat_format(df, t * 1000.0, u, 128, 0, &e);
        ICU->udat_close(df);
        return U_FAILURE(e) ? 0 : put_u16(out, cap, u, n);
    }
    n = ICU->ucal_getTimeZoneDisplayName(c, style, canon, u, 128, &e);
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    return put_u16(out, cap, u, n);
}

int fdn_tz_abbreviation(const char *name, double t, char *out, int cap) {
    long dst = 0;
    fdn_tz_offset(name, t, &dst);
    /* o macOS dá a abreviação curta do en_US ("PST", "EDT") e, sem uma, "GMT-3" */
    return fdn_tz_name(name, dst ? FDN_TZN_SHORT_DST : FDN_TZN_SHORT_STANDARD, "en_US", t, out, cap);
}

int fdn_tz_next_transition(const char *name, double after, double *out) {
    void *c;
    UErrorCode e = U_ZERO_ERROR;
    UDate d = 0;
    long fixed;
    if (fdn_tz_parse_fixed(name, &fixed) == 0 || !ICU->ok || !ICU->ucal_getTimeZoneTransitionDate) return 0;
    if (!(c = tz_cal(name, after))) return 0;
    UBool ok = ICU->ucal_getTimeZoneTransitionDate(c, UCAL_TZ_TRANSITION_NEXT, &d, &e);
    ICU->ucal_close(c);
    if (!ok || U_FAILURE(e)) return 0;
    *out = d / 1000.0;
    return 1;
}

int fdn_tz_each_known(fdn_each_fn cb, void *ctx) {
    UErrorCode e = U_ZERO_ERROR;
    void *en;
    const char *s;
    int n = 0;
    if (!ICU->ok) { cb("America/Sao_Paulo", ctx); cb("GMT", ctx); return 2; }
    en = ICU->ucal_openTimeZones(&e);
    if (U_FAILURE(e) || !en) return 0;
    while ((s = ICU->uenum_next(en, 0, &e)) && !U_FAILURE(e)) {
        /* knownTimeZoneNames do macOS: só "Área/Cidade" (sem "Etc/", "SystemV/", aliases "US/" nem "GMT") */
        if (!strchr(s, '/') || !strncmp(s, "Etc/", 4) || !strncmp(s, "SystemV/", 8) || !strncmp(s, "US/", 3)) continue;
        cb(s, ctx);
        n++;
    }
    ICU->uenum_close(en);
    return n;
}

const char *fdn_tz_data_version(void) {
    UErrorCode e = U_ZERO_ERROR;
    const char *v = ICU->ok && ICU->ucal_getTZDataVersion ? ICU->ucal_getTZDataVersion(&e) : 0;
    return v && !U_FAILURE(e) ? v : "2023c";
}

/* ================================================================ calendário */

void fdn_comps_clear(fdn_comps *c) {
    c->era = c->year = c->month = c->day = c->hour = c->minute = c->second = c->nanosecond = c->weekday =
        c->weekday_ordinal = c->quarter = c->week_of_month = c->week_of_year = c->year_for_week = c->day_of_year = FDN_UNDEF;
    c->leap_month = -1;
}

int64_t *fdn_comps_field(fdn_comps *c, uint64_t unit) {
    switch (unit) {
    case FDN_U_ERA: return &c->era;
    case FDN_U_YEAR: return &c->year;
    case FDN_U_MONTH: return &c->month;
    case FDN_U_DAY: return &c->day;
    case FDN_U_HOUR: return &c->hour;
    case FDN_U_MINUTE: return &c->minute;
    case FDN_U_SECOND: return &c->second;
    case FDN_U_NANOSECOND: return &c->nanosecond;
    case FDN_U_WEEKDAY: return &c->weekday;
    case FDN_U_WEEKDAY_ORDINAL: return &c->weekday_ordinal;
    case FDN_U_QUARTER: return &c->quarter;
    case FDN_U_WEEK_OF_MONTH: return &c->week_of_month;
    case FDN_U_WEEK_OF_YEAR: return &c->week_of_year;
    case FDN_U_YEAR_FOR_WEEK: return &c->year_for_week;
    case FDN_U_DAY_OF_YEAR: return &c->day_of_year;
    }
    return 0;
}

static const char *const cal_idents[] = {
    "gregorian", "buddhist", "chinese", "coptic", "ethiopic", "ethiopic-amete-alem", "hebrew", "iso8601", "indian",
    "islamic", "islamic-civil", "japanese", "persian", "roc", "islamic-tbla", "islamic-umalqura", "dangi",
    "bangla", "gujarati", "kannada", "malayalam", "marathi", "odia", "tamil", "telugu", "vikram", "dangi", 0,
};
int fdn_cal_ident_ok(const char *ident) {
    for (int i = 0; ident && cal_idents[i]; i++) if (!strcmp(ident, cal_idents[i])) return 1;
    return 0;
}

void fdn_cal_init(fdn_cal *k, const char *ident, const char *locale, const char *tz) {
    memset(k, 0, sizeof *k);
    put(k->ident, sizeof k->ident, ident && ident[0] ? ident : "gregorian", -1);
    fdn_loc_canonical(locale ? locale : "", k->locale, sizeof k->locale);
    if (tz && tz[0]) put(k->tz, sizeof k->tz, tz, -1);
    else fdn_tz_system(k->tz, sizeof k->tz);
}

/* O UCalendar do fdn_cal: locale com @calendar=ident (iso8601 é gregoriano com semana ISO), fuso e atributos. */
static void *cal_open(const fdn_cal *k) {
    char loc[160];
    UChar z[96];
    UErrorCode e = U_ZERO_ERROR;
    long fixed;
    char zname[96];
    const char *ident = !strcmp(k->ident, "iso8601") ? "iso8601" : k->ident;
    if (!ICU->ok) return 0;
    fdn_loc_with_keyword(k->locale, "calendar", ident, loc, sizeof loc);
    /* desvio fixo: a ICU entende "GMT-03:00" */
    if (fdn_tz_parse_fixed(k->tz, &fixed) == 0) {
        long a = fixed < 0 ? -fixed : fixed;
        if (fixed) snprintf(zname, sizeof zname, "GMT%c%02ld:%02ld", fixed < 0 ? '-' : '+', a / 3600, a % 3600 / 60);
        else snprintf(zname, sizeof zname, "GMT");
    } else put(zname, sizeof zname, k->tz[0] ? k->tz : "GMT", -1);
    int32_t zl = fdn_u16_from_utf8(zname, -1, z, 96);
    void *c = ICU->ucal_open(z, zl, loc, 0, &e);
    if (U_FAILURE(e) || !c) return 0;
    if (!strcmp(k->ident, "iso8601")) {
        ICU->ucal_setAttribute(c, UCAL_FIRST_DAY_OF_WEEK, 2);
        ICU->ucal_setAttribute(c, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK, 4);
    }
    if (k->first_weekday >= 1 && k->first_weekday <= 7) ICU->ucal_setAttribute(c, UCAL_FIRST_DAY_OF_WEEK, k->first_weekday);
    if (k->min_days >= 1 && k->min_days <= 7) ICU->ucal_setAttribute(c, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK, k->min_days);
    ICU->ucal_setAttribute(c, UCAL_LENIENT, 1);
    return c;
}

int fdn_cal_first_weekday(const fdn_cal *k) {
    void *c = cal_open(k);
    int r = 1;
    if (!c) return k->first_weekday ? k->first_weekday : 1;
    r = ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK);
    ICU->ucal_close(c);
    return r;
}

int fdn_cal_min_days(const fdn_cal *k) {
    void *c = cal_open(k);
    int r = 1;
    if (!c) return k->min_days ? k->min_days : 1;
    r = ICU->ucal_getAttribute(c, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
    ICU->ucal_close(c);
    return r;
}

/* ICU trabalha em milissegundos inteiros; o resto vai para nanossegundos */
static double ms_floor(double t, double *frac_ns) {
    double ms = floor(t * 1000.0);
    if (frac_ns) {
        double ns = (t - ms / 1000.0) * 1e9;
        if (ns < 0) ns = 0;
        *frac_ns = ns;
    }
    return ms;
}

static int32_t g(void *c, int f, UErrorCode *e) { return ICU->ucal_get(c, f, e); }

int fdn_cal_components(const fdn_cal *k, uint64_t units, double t, fdn_comps *o) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    double frac;
    fdn_comps_clear(o);
    if (!c) return 0;
    double ms = ms_floor(t, &frac);
    ICU->ucal_setMillis(c, ms, &e);
    if (units & FDN_U_ERA) o->era = g(c, UCAL_ERA, &e);
    if (units & FDN_U_YEAR) o->year = g(c, UCAL_YEAR, &e);
    if (units & FDN_U_MONTH) o->month = g(c, UCAL_MONTH, &e) + 1;
    if (units & FDN_U_DAY) o->day = g(c, UCAL_DATE, &e);
    if (units & FDN_U_HOUR) o->hour = g(c, UCAL_HOUR_OF_DAY, &e);
    if (units & FDN_U_MINUTE) o->minute = g(c, UCAL_MINUTE, &e);
    if (units & FDN_U_SECOND) o->second = g(c, UCAL_SECOND, &e);
    if (units & FDN_U_NANOSECOND) o->nanosecond = (int64_t)g(c, UCAL_MILLISECOND, &e) * 1000000 + (int64_t)(frac + 0.5);
    if (units & FDN_U_WEEKDAY) o->weekday = g(c, UCAL_DAY_OF_WEEK, &e);
    if (units & FDN_U_WEEKDAY_ORDINAL) o->weekday_ordinal = g(c, UCAL_DAY_OF_WEEK_IN_MONTH, &e);
    if (units & FDN_U_QUARTER) {
        /* trimestre: o mês dividido em quatro partes do ano (13 meses no hebraico contam como o gregoriano faz: 3 por trimestre) */
        int32_t m = g(c, UCAL_MONTH, &e);
        o->quarter = m / 3 + 1;
        if (o->quarter > 4) o->quarter = 4;
    }
    if (units & FDN_U_WEEK_OF_MONTH) o->week_of_month = g(c, UCAL_WEEK_OF_MONTH, &e);
    if (units & FDN_U_WEEK_OF_YEAR) o->week_of_year = g(c, UCAL_WEEK_OF_YEAR, &e);
    if (units & FDN_U_YEAR_FOR_WEEK) o->year_for_week = g(c, UCAL_YEAR_WOY, &e);
    if (units & FDN_U_DAY_OF_YEAR) o->day_of_year = g(c, UCAL_DAY_OF_YEAR, &e);
    if (units & (FDN_U_MONTH | FDN_U_YEAR)) o->leap_month = g(c, UCAL_IS_LEAP_MONTH, &e) ? 1 : 0;
    ICU->ucal_close(c);
    return !U_FAILURE(e);
}

static int defined(int64_t v) { return v != FDN_UNDEF; }

int fdn_cal_date(const fdn_cal *k, const fdn_comps *x, double *out) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    if (!c) return 0;
    ICU->ucal_clear(c);
    /* o que não veio fica no menor valor, como no macOS: ano 1 da era atual, 1º de janeiro, 00:00:00 */
    int week_based = defined(x->year_for_week) || (defined(x->week_of_year) && !defined(x->month) && !defined(x->day));
    if (defined(x->era)) ICU->ucal_set(c, UCAL_ERA, (int32_t)x->era);
    if (week_based) {
        ICU->ucal_set(c, UCAL_YEAR_WOY, (int32_t)(defined(x->year_for_week) ? x->year_for_week : defined(x->year) ? x->year : 1));
        ICU->ucal_set(c, UCAL_WEEK_OF_YEAR, (int32_t)(defined(x->week_of_year) ? x->week_of_year : 1));
        ICU->ucal_set(c, UCAL_DAY_OF_WEEK, (int32_t)(defined(x->weekday) ? x->weekday : ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK)));
    } else {
        ICU->ucal_set(c, UCAL_YEAR, (int32_t)(defined(x->year) ? x->year : 1));
        if (defined(x->day_of_year) && !defined(x->month) && !defined(x->day)) {
            ICU->ucal_set(c, UCAL_DAY_OF_YEAR, (int32_t)x->day_of_year);
        } else {
            int64_t month = defined(x->month) ? x->month - 1 : defined(x->quarter) ? (x->quarter - 1) * 3 : 0;
            ICU->ucal_set(c, UCAL_MONTH, (int32_t)month);
            if (x->leap_month == 1) ICU->ucal_set(c, UCAL_IS_LEAP_MONTH, 1);
            if (defined(x->day)) ICU->ucal_set(c, UCAL_DATE, (int32_t)x->day);
            else if (defined(x->weekday) && defined(x->weekday_ordinal)) {
                ICU->ucal_set(c, UCAL_DAY_OF_WEEK, (int32_t)x->weekday);
                ICU->ucal_set(c, UCAL_DAY_OF_WEEK_IN_MONTH, (int32_t)x->weekday_ordinal);
            } else if (defined(x->weekday) && defined(x->week_of_month)) {
                ICU->ucal_set(c, UCAL_DAY_OF_WEEK, (int32_t)x->weekday);
                ICU->ucal_set(c, UCAL_WEEK_OF_MONTH, (int32_t)x->week_of_month);
            } else if (defined(x->week_of_month)) {
                ICU->ucal_set(c, UCAL_WEEK_OF_MONTH, (int32_t)x->week_of_month);
                ICU->ucal_set(c, UCAL_DAY_OF_WEEK, ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK));
            } else ICU->ucal_set(c, UCAL_DATE, 1);
        }
    }
    /* hora, minuto e segundo podem passar do limite (25h, 61min): o modo leniente normaliza, como o NSCalendar */
    ICU->ucal_set(c, UCAL_HOUR_OF_DAY, (int32_t)(defined(x->hour) ? x->hour : 0));
    ICU->ucal_set(c, UCAL_MINUTE, (int32_t)(defined(x->minute) ? x->minute : 0));
    ICU->ucal_set(c, UCAL_SECOND, (int32_t)(defined(x->second) ? x->second : 0));
    ICU->ucal_set(c, UCAL_MILLISECOND, 0);
    double ms = ICU->ucal_getMillis(c, &e);
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    *out = ms / 1000.0 + (defined(x->nanosecond) ? (double)x->nanosecond / 1e9 : 0.0);
    return 1;
}

int fdn_cal_add(const fdn_cal *k, const fdn_comps *x, double t, uint64_t opts, double *out) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    double frac;
    if (!c) return 0;
    double ms = ms_floor(t, &frac);
    ICU->ucal_setMillis(c, ms, &e);
    void (*op)(void *, int32_t, int32_t, UErrorCode *) = (opts & FDN_CO_WRAP) ? ICU->ucal_roll : ICU->ucal_add;
    /* ordem do NSCalendar: do maior para o menor */
    if (defined(x->era)) op(c, UCAL_ERA, (int32_t)x->era, &e);
    if (defined(x->year)) op(c, UCAL_YEAR, (int32_t)x->year, &e);
    if (defined(x->year_for_week)) op(c, UCAL_YEAR_WOY, (int32_t)x->year_for_week, &e);
    if (defined(x->quarter)) op(c, UCAL_MONTH, (int32_t)(x->quarter * 3), &e);
    if (defined(x->month)) op(c, UCAL_MONTH, (int32_t)x->month, &e);
    if (defined(x->week_of_year)) op(c, UCAL_WEEK_OF_YEAR, (int32_t)x->week_of_year, &e);
    if (defined(x->week_of_month)) op(c, UCAL_WEEK_OF_MONTH, (int32_t)x->week_of_month, &e);
    if (defined(x->day)) op(c, UCAL_DATE, (int32_t)x->day, &e);
    if (defined(x->day_of_year)) op(c, UCAL_DAY_OF_YEAR, (int32_t)x->day_of_year, &e);
    if (defined(x->weekday)) op(c, UCAL_DAY_OF_WEEK, (int32_t)x->weekday, &e);
    if (defined(x->weekday_ordinal)) op(c, UCAL_DAY_OF_WEEK_IN_MONTH, (int32_t)x->weekday_ordinal, &e);
    if (defined(x->hour)) op(c, UCAL_HOUR_OF_DAY, (int32_t)x->hour, &e);
    if (defined(x->minute)) op(c, UCAL_MINUTE, (int32_t)x->minute, &e);
    if (defined(x->second)) op(c, UCAL_SECOND, (int32_t)x->second, &e);
    double r = ICU->ucal_getMillis(c, &e);
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    double res = r / 1000.0 + frac / 1e9;
    if (defined(x->nanosecond)) res += (double)x->nanosecond / 1e9;
    *out = res;
    return 1;
}

int fdn_cal_difference(const fdn_cal *k, uint64_t units, double from, double to, fdn_comps *o) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    double ffrac, tfrac;
    fdn_comps_clear(o);
    if (!c) return 0;
    double fm = ms_floor(from, &ffrac), tm = ms_floor(to, &tfrac);
    ICU->ucal_setMillis(c, fm, &e);
    /* ucal_getFieldDifference avança o calendário: cada unidade pega o que sobrou da maior */
    if (units & FDN_U_ERA) o->era = ICU->ucal_getFieldDifference(c, tm, UCAL_ERA, &e);
    if (units & FDN_U_YEAR) o->year = ICU->ucal_getFieldDifference(c, tm, UCAL_YEAR, &e);
    if (units & FDN_U_YEAR_FOR_WEEK) o->year_for_week = ICU->ucal_getFieldDifference(c, tm, UCAL_YEAR_WOY, &e);
    if (units & FDN_U_QUARTER) o->quarter = ICU->ucal_getFieldDifference(c, tm, UCAL_MONTH, &e) / 3;
    if (units & FDN_U_QUARTER) { /* devolve os meses que não fecharam um trimestre */
        ICU->ucal_setMillis(c, fm, &e);
        if (units & FDN_U_YEAR) ICU->ucal_add(c, UCAL_YEAR, (int32_t)o->year, &e);
        ICU->ucal_add(c, UCAL_MONTH, (int32_t)(o->quarter * 3), &e);
    }
    if (units & FDN_U_MONTH) o->month = ICU->ucal_getFieldDifference(c, tm, UCAL_MONTH, &e);
    if (units & FDN_U_WEEK_OF_YEAR) o->week_of_year = ICU->ucal_getFieldDifference(c, tm, UCAL_WEEK_OF_YEAR, &e);
    if (units & FDN_U_WEEK_OF_MONTH) o->week_of_month = ICU->ucal_getFieldDifference(c, tm, UCAL_WEEK_OF_MONTH, &e);
    if (units & FDN_U_DAY) o->day = ICU->ucal_getFieldDifference(c, tm, UCAL_DATE, &e);
    if (units & FDN_U_DAY_OF_YEAR) o->day_of_year = ICU->ucal_getFieldDifference(c, tm, UCAL_DAY_OF_YEAR, &e);
    if (units & FDN_U_WEEKDAY) o->weekday = ICU->ucal_getFieldDifference(c, tm, UCAL_DAY_OF_WEEK, &e);
    if (units & FDN_U_WEEKDAY_ORDINAL) o->weekday_ordinal = ICU->ucal_getFieldDifference(c, tm, UCAL_WEEK_OF_MONTH, &e);
    if (units & FDN_U_HOUR) o->hour = ICU->ucal_getFieldDifference(c, tm, UCAL_HOUR_OF_DAY, &e);
    if (units & FDN_U_MINUTE) o->minute = ICU->ucal_getFieldDifference(c, tm, UCAL_MINUTE, &e);
    if (units & FDN_U_SECOND) o->second = ICU->ucal_getFieldDifference(c, tm, UCAL_SECOND, &e);
    if (units & FDN_U_NANOSECOND) {
        double rest = ICU->ucal_getMillis(c, &e);
        o->nanosecond = (int64_t)((tm - rest) * 1e6 + (tfrac - ffrac));
    }
    ICU->ucal_close(c);
    return !U_FAILURE(e);
}

static int unit_field(uint64_t u) {
    switch (u) {
    case FDN_U_ERA: return UCAL_ERA;
    case FDN_U_YEAR: return UCAL_YEAR;
    case FDN_U_MONTH: return UCAL_MONTH;
    case FDN_U_DAY: return UCAL_DATE;
    case FDN_U_HOUR: return UCAL_HOUR_OF_DAY;
    case FDN_U_MINUTE: return UCAL_MINUTE;
    case FDN_U_SECOND: return UCAL_SECOND;
    case FDN_U_WEEKDAY: return UCAL_DAY_OF_WEEK;
    case FDN_U_WEEKDAY_ORDINAL: return UCAL_DAY_OF_WEEK_IN_MONTH;
    case FDN_U_WEEK_OF_MONTH: return UCAL_WEEK_OF_MONTH;
    case FDN_U_WEEK_OF_YEAR: return UCAL_WEEK_OF_YEAR;
    case FDN_U_YEAR_FOR_WEEK: return UCAL_YEAR_WOY;
    case FDN_U_DAY_OF_YEAR: return UCAL_DAY_OF_YEAR;
    case FDN_U_QUARTER: return UCAL_MONTH;
    }
    return -1;
}

/* início da unidade que contém t: zera os campos menores */
static double start_of(void *c, uint64_t unit, UErrorCode *e) {
    int f = unit_field(unit);
    if (unit == FDN_U_NANOSECOND) return ICU->ucal_getMillis(c, e);
    ICU->ucal_set(c, UCAL_MILLISECOND, 0);
    if (unit == FDN_U_SECOND) return ICU->ucal_getMillis(c, e);
    ICU->ucal_set(c, UCAL_SECOND, 0);
    if (unit == FDN_U_MINUTE) return ICU->ucal_getMillis(c, e);
    ICU->ucal_set(c, UCAL_MINUTE, 0);
    if (unit == FDN_U_HOUR) return ICU->ucal_getMillis(c, e);
    ICU->ucal_set(c, UCAL_HOUR_OF_DAY, 0);
    (void)f;
    switch (unit) {
    case FDN_U_DAY: case FDN_U_WEEKDAY: case FDN_U_DAY_OF_YEAR: break;
    case FDN_U_WEEK_OF_MONTH: case FDN_U_WEEK_OF_YEAR: case FDN_U_WEEKDAY_ORDINAL: {
        int32_t fd = ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK);
        int32_t dow = ICU->ucal_get(c, UCAL_DAY_OF_WEEK, e);
        int32_t back = unit == FDN_U_WEEKDAY_ORDINAL ? (ICU->ucal_get(c, UCAL_DATE, e) - 1) % 7 : (dow - fd + 7) % 7;
        ICU->ucal_add(c, UCAL_DATE, -back, e);
        break;
    }
    case FDN_U_MONTH: ICU->ucal_set(c, UCAL_DATE, 1); break;
    case FDN_U_QUARTER: {
        int32_t m = ICU->ucal_get(c, UCAL_MONTH, e);
        ICU->ucal_set(c, UCAL_DATE, 1);
        ICU->ucal_set(c, UCAL_MONTH, m - m % 3);
        break;
    }
    case FDN_U_YEAR: ICU->ucal_set(c, UCAL_DAY_OF_YEAR, 1); break;
    case FDN_U_YEAR_FOR_WEEK: {
        int32_t y = ICU->ucal_get(c, UCAL_YEAR_WOY, e);
        ICU->ucal_set(c, UCAL_YEAR_WOY, y);
        ICU->ucal_set(c, UCAL_WEEK_OF_YEAR, 1);
        ICU->ucal_set(c, UCAL_DAY_OF_WEEK, ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK));
        break;
    }
    case FDN_U_ERA: {
        int32_t era = ICU->ucal_get(c, UCAL_ERA, e);
        int32_t ymin = ICU->ucal_getLimit(c, UCAL_YEAR, UCAL_ACTUAL_MINIMUM, e);
        ICU->ucal_set(c, UCAL_ERA, era);
        ICU->ucal_set(c, UCAL_YEAR, ymin);
        ICU->ucal_set(c, UCAL_DAY_OF_YEAR, 1);
        break;
    }
    default: return -1e300;
    }
    return ICU->ucal_getMillis(c, e);
}

static int add_one(void *c, uint64_t unit, UErrorCode *e) {
    switch (unit) {
    case FDN_U_QUARTER: ICU->ucal_add(c, UCAL_MONTH, 3, e); return 1;
    case FDN_U_WEEK_OF_MONTH: case FDN_U_WEEK_OF_YEAR: case FDN_U_WEEKDAY_ORDINAL: ICU->ucal_add(c, UCAL_DATE, 7, e); return 1;
    case FDN_U_WEEKDAY: case FDN_U_DAY_OF_YEAR: ICU->ucal_add(c, UCAL_DATE, 1, e); return 1;
    case FDN_U_YEAR_FOR_WEEK: ICU->ucal_add(c, UCAL_YEAR_WOY, 1, e); return 1;
    case FDN_U_ERA: {
        int32_t era = ICU->ucal_get(c, UCAL_ERA, e), emax = ICU->ucal_getLimit(c, UCAL_ERA, UCAL_MAXIMUM, e);
        if (era >= emax) return 0;
        ICU->ucal_set(c, UCAL_ERA, era + 1);
        ICU->ucal_set(c, UCAL_YEAR, ICU->ucal_getLimit(c, UCAL_YEAR, UCAL_MINIMUM, e));
        ICU->ucal_set(c, UCAL_DAY_OF_YEAR, 1);
        return 1;
    }
    }
    int f = unit_field(unit);
    if (f < 0) return 0;
    ICU->ucal_add(c, f, 1, e);
    return 1;
}

int fdn_cal_interval(const fdn_cal *k, uint64_t unit, double t, double *start, double *len) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    if (!c) return 0;
    if (unit == FDN_U_NANOSECOND) { *start = t; *len = 1e-9; ICU->ucal_close(c); return 1; }
    ICU->ucal_setMillis(c, ms_floor(t, 0), &e);
    double s = start_of(c, unit, &e);
    if (s < -1e299) { ICU->ucal_close(c); return 0; }
    ICU->ucal_setMillis(c, s, &e);
    double end;
    if (add_one(c, unit, &e)) end = ICU->ucal_getMillis(c, &e);
    else end = 1e300;
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    *start = s / 1000.0;
    *len = (end - s) / 1000.0;
    return 1;
}

int fdn_cal_range(const fdn_cal *k, uint64_t smaller, uint64_t larger, double t, long *loc, long *len) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    double s, l;
    if (!c) return 0;
    ICU->ucal_setMillis(c, ms_floor(t, 0), &e);
    int f = unit_field(smaller);
    long lo, hi;
    if (smaller == FDN_U_WEEKDAY && (larger == FDN_U_WEEK_OF_MONTH || larger == FDN_U_WEEK_OF_YEAR)) { lo = 1; hi = 7; }
    else if (smaller == FDN_U_MONTH && larger == FDN_U_YEAR) {
        lo = ICU->ucal_getLimit(c, UCAL_MONTH, UCAL_ACTUAL_MINIMUM, &e) + 1;
        hi = ICU->ucal_getLimit(c, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM, &e) + 1;
    } else if (smaller == FDN_U_DAY && (larger == FDN_U_MONTH || larger == FDN_U_YEAR || larger == FDN_U_WEEK_OF_YEAR || larger == FDN_U_WEEK_OF_MONTH)) {
        if (larger == FDN_U_MONTH) { lo = 1; hi = ICU->ucal_getLimit(c, UCAL_DATE, UCAL_ACTUAL_MAXIMUM, &e); }
        else if (larger == FDN_U_YEAR) { lo = 1; hi = ICU->ucal_getLimit(c, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &e); }
        else {
            /* dias do mês que caem na semana de t */
            ICU->ucal_close(c);
            if (!fdn_cal_interval(k, FDN_U_WEEK_OF_MONTH, t, &s, &l)) return 0;
            fdn_comps a, b;
            fdn_cal_components(k, FDN_U_DAY | FDN_U_MONTH, s, &a);
            fdn_cal_components(k, FDN_U_DAY | FDN_U_MONTH, s + l - 1, &b);
            fdn_comps now;
            fdn_cal_components(k, FDN_U_MONTH, t, &now);
            long first = a.month == now.month ? (long)a.day : 1;
            long last = b.month == now.month ? (long)b.day : first + 6;
            if (b.month != now.month) {
                long loc2, len2;
                fdn_cal_range(k, FDN_U_DAY, FDN_U_MONTH, t, &loc2, &len2);
                last = loc2 + len2 - 1;
            }
            *loc = first; *len = last - first + 1;
            return 1;
        }
    } else if (smaller == FDN_U_HOUR && larger == FDN_U_DAY) { lo = 0; hi = 23; }
    else if (smaller == FDN_U_MINUTE && larger == FDN_U_HOUR) { lo = 0; hi = 59; }
    else if (smaller == FDN_U_SECOND && larger == FDN_U_MINUTE) { lo = 0; hi = 59; }
    else if (smaller == FDN_U_WEEK_OF_MONTH && larger == FDN_U_MONTH) {
        lo = ICU->ucal_getLimit(c, UCAL_WEEK_OF_MONTH, UCAL_ACTUAL_MINIMUM, &e);
        hi = ICU->ucal_getLimit(c, UCAL_WEEK_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &e);
    } else if (smaller == FDN_U_WEEK_OF_YEAR && larger == FDN_U_YEAR) {
        lo = ICU->ucal_getLimit(c, UCAL_WEEK_OF_YEAR, UCAL_ACTUAL_MINIMUM, &e);
        hi = ICU->ucal_getLimit(c, UCAL_WEEK_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &e);
    } else if (smaller == FDN_U_WEEKDAY_ORDINAL && larger == FDN_U_MONTH) {
        long dl, dn;
        ICU->ucal_close(c);
        fdn_cal_range(k, FDN_U_DAY, FDN_U_MONTH, t, &dl, &dn);
        *loc = 1; *len = (dn + 6) / 7;
        return 1;
    } else if (smaller == FDN_U_YEAR && larger == FDN_U_ERA) {
        lo = ICU->ucal_getLimit(c, UCAL_YEAR, UCAL_ACTUAL_MINIMUM, &e);
        hi = ICU->ucal_getLimit(c, UCAL_YEAR, UCAL_ACTUAL_MAXIMUM, &e);
    } else if (smaller == FDN_U_QUARTER && larger == FDN_U_YEAR) { lo = 1; hi = 4; }
    else if (f >= 0) {
        lo = ICU->ucal_getLimit(c, f, UCAL_ACTUAL_MINIMUM, &e);
        hi = ICU->ucal_getLimit(c, f, UCAL_ACTUAL_MAXIMUM, &e);
        if (smaller == FDN_U_MONTH) { lo++; hi++; }
    } else { ICU->ucal_close(c); return 0; }
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    *loc = lo;
    *len = hi - lo + 1;
    return 1;
}

long fdn_cal_ordinality(const fdn_cal *k, uint64_t smaller, uint64_t larger, double t) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    long r = -1;
    if (!c) return -1;
    ICU->ucal_setMillis(c, ms_floor(t, 0), &e);
    if (smaller == FDN_U_DAY && larger == FDN_U_MONTH) r = g(c, UCAL_DATE, &e);
    else if (smaller == FDN_U_DAY && larger == FDN_U_YEAR) r = g(c, UCAL_DAY_OF_YEAR, &e);
    else if ((smaller == FDN_U_DAY || smaller == FDN_U_WEEKDAY) && (larger == FDN_U_WEEK_OF_MONTH || larger == FDN_U_WEEK_OF_YEAR))
        r = (g(c, UCAL_DAY_OF_WEEK, &e) - ICU->ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK) + 7) % 7 + 1;
    else if (smaller == FDN_U_WEEK_OF_MONTH && larger == FDN_U_MONTH) r = g(c, UCAL_WEEK_OF_MONTH, &e);
    else if (smaller == FDN_U_WEEK_OF_YEAR && (larger == FDN_U_YEAR || larger == FDN_U_YEAR_FOR_WEEK)) r = g(c, UCAL_WEEK_OF_YEAR, &e);
    else if (smaller == FDN_U_MONTH && larger == FDN_U_YEAR) r = g(c, UCAL_MONTH, &e) + 1;
    else if (smaller == FDN_U_MONTH && larger == FDN_U_QUARTER) r = g(c, UCAL_MONTH, &e) % 3 + 1;
    else if (smaller == FDN_U_QUARTER && larger == FDN_U_YEAR) r = g(c, UCAL_MONTH, &e) / 3 + 1;
    else if (smaller == FDN_U_HOUR && larger == FDN_U_DAY) r = g(c, UCAL_HOUR_OF_DAY, &e) + 1;
    else if (smaller == FDN_U_MINUTE && larger == FDN_U_HOUR) r = g(c, UCAL_MINUTE, &e) + 1;
    else if (smaller == FDN_U_SECOND && larger == FDN_U_MINUTE) r = g(c, UCAL_SECOND, &e) + 1;
    else if (smaller == FDN_U_YEAR && larger == FDN_U_ERA) r = g(c, UCAL_YEAR, &e);
    else if (smaller == FDN_U_WEEKDAY_ORDINAL && larger == FDN_U_MONTH) r = g(c, UCAL_DAY_OF_WEEK_IN_MONTH, &e);
    else {
        /* caso geral: conta quantas unidades menores cabem entre o início da maior e t */
        double s, l;
        ICU->ucal_close(c);
        if (!fdn_cal_interval(k, larger, t, &s, &l)) return -1;
        c = cal_open(k);
        if (!c) return -1;
        ICU->ucal_setMillis(c, s * 1000.0, &e);
        int f = unit_field(smaller);
        if (smaller == FDN_U_NANOSECOND) { ICU->ucal_close(c); return (long)((t - s) * 1e9) + 1; }
        if (f < 0) { ICU->ucal_close(c); return -1; }
        if (smaller == FDN_U_WEEK_OF_YEAR || smaller == FDN_U_WEEK_OF_MONTH) f = UCAL_WEEK_OF_YEAR;
        if (smaller == FDN_U_WEEKDAY || smaller == FDN_U_DAY_OF_YEAR) f = UCAL_DATE;
        r = ICU->ucal_getFieldDifference(c, ms_floor(t, 0), f, &e) + 1;
    }
    ICU->ucal_close(c);
    return U_FAILURE(e) ? -1 : r;
}

int fdn_cal_limit(const fdn_cal *k, uint64_t unit, int maximum, long *loc, long *len) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    int f = unit_field(unit);
    long lo, hi;
    if (!c) return 0;
    if (unit == FDN_U_NANOSECOND) { *loc = 0; *len = 1000000000; ICU->ucal_close(c); return 1; }
    if (unit == FDN_U_QUARTER) { *loc = 1; *len = 4; ICU->ucal_close(c); return 1; }
    if (f < 0) { ICU->ucal_close(c); return 0; }
    lo = ICU->ucal_getLimit(c, f, maximum ? UCAL_MINIMUM : UCAL_GREATEST_MINIMUM, &e);
    hi = ICU->ucal_getLimit(c, f, maximum ? UCAL_MAXIMUM : UCAL_LEAST_MAXIMUM, &e);
    if (unit == FDN_U_MONTH) { lo++; hi++; }
    ICU->ucal_close(c);
    if (U_FAILURE(e)) return 0;
    *loc = lo;
    *len = hi - lo + 1;
    return 1;
}

int fdn_cal_is_weekend(const fdn_cal *k, double t) {
    void *c = cal_open(k);
    UErrorCode e = U_ZERO_ERROR;
    int r;
    if (!c) return 0;
    ICU->ucal_setMillis(c, ms_floor(t, 0), &e);
    int32_t dow = g(c, UCAL_DAY_OF_WEEK, &e);
    /* UCAL_WEEKDAY=0, UCAL_WEEKEND=1, UCAL_WEEKEND_ONSET=2, UCAL_WEEKEND_CEASE=3 */
    int32_t type = ICU->ucal_getDayOfWeekType ? ICU->ucal_getDayOfWeekType(c, dow, &e) : (dow == 1 || dow == 7);
    r = type != 0;
    ICU->ucal_close(c);
    return r;
}

int fdn_cal_weekend_range(const fdn_cal *k, double t, int next, double *start, double *len) {
    double day0, dl;
    double d;
    if (!fdn_cal_interval(k, FDN_U_DAY, t, &day0, &dl)) return 0;
    d = day0;
    if (!next) {
        /* fim de semana que contém t: volta até o primeiro dia de fim de semana */
        if (!fdn_cal_is_weekend(k, t)) return 0;
        for (int i = 0; i < 7; i++) {
            double prev, pl;
            if (!fdn_cal_interval(k, FDN_U_DAY, d - 1, &prev, &pl) || !fdn_cal_is_weekend(k, prev)) break;
            d = prev;
        }
    } else {
        /* próximo fim de semana a partir do dia seguinte ao fim de semana atual */
        int in = fdn_cal_is_weekend(k, d);
        for (int i = 0; i < 14 && in; i++) { double n, nl; fdn_cal_interval(k, FDN_U_DAY, d + dl + 1, &n, &nl); d = n; dl = nl; in = fdn_cal_is_weekend(k, d); }
        for (int i = 0; i < 14 && !fdn_cal_is_weekend(k, d); i++) { double n, nl; fdn_cal_interval(k, FDN_U_DAY, d + dl + 1, &n, &nl); d = n; dl = nl; }
        if (!fdn_cal_is_weekend(k, d)) return 0;
    }
    double end = d;
    for (int i = 0; i < 7; i++) {
        double n, nl;
        fdn_cal_interval(k, FDN_U_DAY, end, &n, &nl);
        if (!fdn_cal_is_weekend(k, n)) break;
        end = n + nl;
    }
    *start = d;
    *len = end - d;
    return 1;
}

static int comps_match(const fdn_comps *have, const fdn_comps *want) {
#define M(f) if (want->f != FDN_UNDEF && have->f != want->f) return 0;
    M(era) M(year) M(month) M(day) M(hour) M(minute) M(second) M(weekday) M(weekday_ordinal) M(quarter)
    M(week_of_month) M(week_of_year) M(year_for_week) M(day_of_year)
#undef M
    if (want->leap_month >= 0 && have->leap_month != want->leap_month) return 0;
    return 1;
}

/* menor unidade pedida: é o passo da busca */
static uint64_t finest(const fdn_comps *w) {
    if (w->nanosecond != FDN_UNDEF || w->second != FDN_UNDEF) return FDN_U_SECOND;
    if (w->minute != FDN_UNDEF) return FDN_U_MINUTE;
    if (w->hour != FDN_UNDEF) return FDN_U_HOUR;
    if (w->day != FDN_UNDEF || w->weekday != FDN_UNDEF || w->day_of_year != FDN_UNDEF || w->weekday_ordinal != FDN_UNDEF) return FDN_U_DAY;
    if (w->week_of_year != FDN_UNDEF || w->week_of_month != FDN_UNDEF) return FDN_U_WEEK_OF_YEAR;
    if (w->month != FDN_UNDEF || w->quarter != FDN_UNDEF) return FDN_U_MONTH;
    return FDN_U_YEAR;
}

int fdn_cal_next_match(const fdn_cal *k, double after, const fdn_comps *w, uint64_t opts, double *out) {
    /*
     * nextDateAfterDate:matchingComponents:options: — anda do início da próxima unidade mais fina pedida, de
     * unidade em unidade, e para na primeira data cujos componentes batem. Os campos mais grossos que a unidade
     * pulam em blocos (um dia sem a hora certa pula para o dia seguinte), então o laço é curto.
     */
    uint64_t step = finest(w), all = FDN_U_ERA | FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY | FDN_U_HOUR | FDN_U_MINUTE |
        FDN_U_SECOND | FDN_U_WEEKDAY | FDN_U_WEEKDAY_ORDINAL | FDN_U_QUARTER | FDN_U_WEEK_OF_MONTH | FDN_U_WEEK_OF_YEAR |
        FDN_U_YEAR_FOR_WEEK | FDN_U_DAY_OF_YEAR;
    int back = (opts & FDN_CO_BACKWARDS) != 0;
    double s, l, t;
    if (!fdn_cal_interval(k, step, after, &s, &l)) return 0;
    t = back ? (s < after ? s : s - 1e-3) : s + l;
    if (back && s >= after) { fdn_cal_interval(k, step, s - 1e-3, &s, &l); t = s; }
    for (long i = 0; i < 400000; i++) {
        fdn_comps have;
        fdn_cal_components(k, all, t, &have);
        if (comps_match(&have, w)) {
            if (w->nanosecond != FDN_UNDEF) t += (double)w->nanosecond / 1e9;
            *out = t;
            return 1;
        }
        /* pula a unidade maior que já não bate */
        uint64_t jump = step;
        if (w->year != FDN_UNDEF && have.year != w->year) jump = FDN_U_YEAR;
        else if (w->month != FDN_UNDEF && have.month != w->month) jump = FDN_U_MONTH;
        else if ((w->day != FDN_UNDEF && have.day != w->day) || (w->weekday != FDN_UNDEF && have.weekday != w->weekday)) jump = step == FDN_U_WEEK_OF_YEAR ? step : FDN_U_DAY;
        else if (w->hour != FDN_UNDEF && have.hour != w->hour) jump = step == FDN_U_HOUR ? step : (step > FDN_U_HOUR ? step : FDN_U_HOUR);
        if (jump > step && step < FDN_U_DAY && jump < FDN_U_DAY) jump = step;
        if (back) {
            if (!fdn_cal_interval(k, jump, t, &s, &l)) return 0;
            fdn_cal_interval(k, step, s - 1e-3, &s, &l);
            t = s;
        } else {
            if (!fdn_cal_interval(k, jump, t, &s, &l)) return 0;
            t = s + l;
        }
        if (w->year != FDN_UNDEF && !back && have.year > w->year && w->era == FDN_UNDEF) return 0;
        if (w->year != FDN_UNDEF && back && have.year < w->year) return 0;
    }
    return 0;
}

int fdn_cal_symbols(const fdn_cal *k, int kind, fdn_each_fn cb, void *ctx) {
    static const int map[][2] = {
        /* tipo UDateFormatSymbolType, primeiro índice */
        {0, 0}, {7, 0}, {1, 0}, {2, 0}, {8, 0}, {10, 0}, {11, 0}, {12, 0}, {3, 1}, {4, 1}, {9, 1}, {13, 1}, {14, 1},
        {15, 1}, {16, 0}, {17, 0}, {18, 0}, {19, 0}, {5, 0}, {5, 1},
    };
    char loc[160];
    UChar u[128];
    UErrorCode e = U_ZERO_ERROR;
    int n = 0;
    if (!ICU->ok || kind < 0 || kind > FDN_SYM_PM) return 0;
    fdn_loc_with_keyword(k->locale, "calendar", k->ident, loc, sizeof loc);
    void *df = ICU->udat_open(UDAT_SHORT, UDAT_SHORT, loc, 0, 0, 0, 0, &e);
    if (U_FAILURE(e) || !df) return 0;
    int type = map[kind][0], first = map[kind][1];
    int count = ICU->udat_countSymbols(df, type);
    if (kind == FDN_SYM_AM || kind == FDN_SYM_PM) { first = kind == FDN_SYM_AM ? 0 : 1; count = first + 1; }
    for (int i = first; i < count; i++) {
        UErrorCode e2 = U_ZERO_ERROR;
        int32_t len = ICU->udat_getSymbols(df, type, i, u, 128, &e2);
        char s[256];
        if (U_FAILURE(e2)) continue;
        /* meses: o ICU devolve 13 no gregoriano (o último vazio); a Foundation só os que têm texto */
        if (len == 0 && (type == 1 || type == 2 || type == 8 || type == 10 || type == 11 || type == 12)) continue;
        fdn_utf8_from_u16(u, len, s, sizeof s);
        cb(s, ctx);
        n++;
    }
    ICU->udat_close(df);
    return n;
}

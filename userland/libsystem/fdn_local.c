/*
 * Locale, Locale.Region e TimeZone do overlay Swift do Foundation. Nenhum é @frozen: o app passa o
 * valor por endereço e lê o tamanho na metadata (fdn_meta.S, tabela fdn_vwt_obj). Por dentro:
 *   Locale         NSString com o identificador, referência forte
 *   Locale.Region  NSString com o código da região, referência forte
 *   TimeZone       NSTimeZone, referência forte
 * Optional<T> desses tipos tem 9 bytes (o valor e o byte de tag, sem inabitante extra).
 * O lado ObjC é fdn_ns_loc_* / fdn_ns_tz_* (fdn_tipos_ns.c; falso no teste de host).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fdn_tipos.h"

#define SW __attribute__((swiftcall))
#define CTX __attribute__((swift_context))
#define USED __attribute__((used))
#if defined(__x86_64__)
#define SRET(sym, impl) __asm__(".text\n.p2align 4\n.globl \"" sym "\"\n\"" sym "\":\n" \
    "  movq %r8, %r9\n  movq %rcx, %r8\n  movq %rdx, %rcx\n  movq %rsi, %rdx\n" \
    "  movq %rdi, %rsi\n  movq %rax, %rdi\n  jmp " #impl "\n")
#else
#define SRET(sym, impl)
#endif

#define FDN_REF_1970 978307200.0

static fdn_str opt_str(const char *s) {
    fdn_str z = { 0, 0 };
    return s ? fdn_str_make(s, -1) : z;
}

/* Optional<referência>: 8 bytes do objeto e o byte de tag (0 = some, 1 = none) */
static void opt_obj(void **out, void *o) {
    if (o) { *out = o; fdn_enum_tag_store(out, 0, 1, 8); }
    else fdn_enum_tag_store(out, 1, 1, 8);
}

/* ---------------------------------------------------------------- Locale */

/*
 * O NSLocale deste Foundation só conhece o locale do sistema (initWithLocaleIdentifier: devolve o atual),
 * então o valor guarda o próprio identificador (NSString) e as partes saem dele aqui.
 * "pt_BR", "en-US", "zh_Hans_CN", "en_US_POSIX", "de_DE@currency=EUR".
 */
static const char *const euro[] = { "AT", "BE", "CY", "DE", "EE", "ES", "FI", "FR", "GR", "HR", "IE", "IT", "LT",
                                    "LU", "LV", "MT", "NL", "PT", "SI", "SK", 0 };
static const char *const moedas[][2] = { { "US", "USD" }, { "BR", "BRL" }, { "GB", "GBP" }, { "JP", "JPY" },
    { "CN", "CNY" }, { "IN", "INR" }, { "CA", "CAD" }, { "AU", "AUD" }, { "MX", "MXN" }, { "AR", "ARS" },
    { "CH", "CHF" }, { "KR", "KRW" }, { "RU", "RUB" }, { "SE", "SEK" }, { "NO", "NOK" }, { "DK", "DKK" },
    { "PL", "PLN" }, { "CL", "CLP" }, { "CO", "COP" }, { "UY", "UYU" }, { "PY", "PYG" }, { "PE", "PEN" },
    { "ZA", "ZAR" }, { "TR", "TRY" }, { "IL", "ILS" }, { "NZ", "NZD" }, { "SG", "SGD" }, { "HK", "HKD" },
    { "TW", "TWD" }, { "AO", "AOA" }, { "MZ", "MZN" }, { "CV", "CVE" }, { 0, 0 } };
/* idiomas com vírgula decimal e ponto (ou espaço) de milhar */
static const char *const virgula[] = { "pt", "de", "fr", "es", "it", "nl", "ru", "pl", "tr", "sv", "nb", "da",
                                       "fi", "cs", "uk", "ro", "hu", "el", "id", "vi", 0 };

static int in_list(const char *const *l, const char *s) {
    for (; *l; l++) if (strcmp(*l, s) == 0) return 1;
    return 0;
}

/* 0 = existe (texto em out), -1 = nil */
int fdn_loc_part(const char *id, int part, char *out, int cap) {
    char lang[16] = "", region[8] = "";
    const char *p = id ? id : "";
    size_t k = 0;
    out[0] = 0;
    if (part == FDN_LOC_ID) {
        size_t n = strlen(p);
        if ((int)n >= cap) n = (size_t)cap - 1;
        memcpy(out, p, n);
        out[n] = 0;
        return 0;
    }
    while (p[k] && p[k] != '_' && p[k] != '-' && p[k] != '@' && k < sizeof lang - 1) { lang[k] = p[k]; k++; }
    lang[k] = 0;
    p += k;
    /* componentes seguintes: script (4 letras) é pulado, região é 2 letras ou 3 dígitos */
    while (*p == '_' || *p == '-') {
        size_t m = 1;
        while (p[m] && p[m] != '_' && p[m] != '-' && p[m] != '@') m++;
        if ((m - 1 == 2 && p[1] >= 'A' && p[1] <= 'Z') || (m - 1 == 3 && p[1] >= '0' && p[1] <= '9')) {
            memcpy(region, p + 1, m - 1);
            region[m - 1] = 0;
            break;
        }
        p += m;
    }
    switch (part) {
    case FDN_LOC_LANG:
        if (!lang[0]) return -1;
        strcpy(out, lang);
        return 0;
    case FDN_LOC_REGION:
        if (!region[0]) return -1;
        strcpy(out, region);
        return 0;
    case FDN_LOC_CURRENCY: {
        const char *at = strstr(id ? id : "", "currency=");
        if (at && strlen(at + 9) >= 3) { memcpy(out, at + 9, 3); out[3] = 0; return 0; }
        if (!region[0]) return -1;
        if (in_list(euro, region)) { strcpy(out, "EUR"); return 0; }
        for (int i = 0; moedas[i][0]; i++)
            if (strcmp(moedas[i][0], region) == 0) { strcpy(out, moedas[i][1]); return 0; }
        return -1;
    }
    case FDN_LOC_DECIMAL:
        strcpy(out, in_list(virgula, lang) ? "," : ".");
        return 0;
    case FDN_LOC_GROUPING:
        if (in_list(virgula, lang)) strcpy(out, (strcmp(lang, "fr") == 0 || strcmp(lang, "ru") == 0 || strcmp(lang, "pl") == 0 ||
                                                 strcmp(lang, "sv") == 0 || strcmp(lang, "nb") == 0 || strcmp(lang, "fi") == 0 ||
                                                 strcmp(lang, "cs") == 0 || strcmp(lang, "uk") == 0) ? "\xe2\x80\xaf" : ".");
        else strcpy(out, ",");
        return 0;
    }
    return -1;
}

static const char *loc_id(void *self) { return fdn_ns_utf8(self); }

SRET("$s10Foundation6LocaleV7currentACvgZ", fdn_loc_current);
static SW USED void fdn_loc_current(void **out) { *out = fdn_ns_string(fdn_ns_loc_current_id()); }
SRET("$s10Foundation6LocaleV19autoupdatingCurrentACvgZ", fdn_loc_auto);
static SW USED void fdn_loc_auto(void **out) { *out = fdn_ns_string(fdn_ns_loc_current_id()); }
SRET("$s10Foundation6LocaleV10identifierACSS_tcfC", fdn_loc_init_id);
static SW USED void fdn_loc_init_id(void **out, fdn_str s) {
    char buf[128];
    if (fdn_str_read(s, buf, (long)sizeof buf) < 0) buf[0] = 0;
    *out = fdn_ns_string(buf);
}
SW void *fdn_loc_to_ns(void *const *self CTX) __asm__("$s10Foundation6LocaleV19_bridgeToObjectiveCSo8NSLocaleCyF");
SW void *fdn_loc_to_ns(void *const *self CTX) { return fdn_ns_loc_obj(loc_id(*self)); }
SRET("$s10Foundation6LocaleV36_unconditionallyBridgeFromObjectiveCyACSo8NSLocaleCSgFZ", fdn_loc_from_ns);
static SW USED void fdn_loc_from_ns(void **out, void *ns) {
    const char *id = ns ? fdn_ns_loc_obj_id(ns) : 0;
    *out = fdn_ns_string(id ? id : fdn_ns_loc_current_id());
}

SW fdn_str fdn_loc_id(void *const *self CTX) __asm__("$s10Foundation6LocaleV10identifierSSvg");
SW fdn_str fdn_loc_id(void *const *self CTX) {
    const char *s = loc_id(*self);
    return fdn_str_make(s ? s : "", -1);
}
#define LOC_OPT(nome, sym, parte) \
    SW fdn_str nome(void *const *self CTX) __asm__(sym); \
    SW fdn_str nome(void *const *self CTX) { \
        char b[64]; \
        return fdn_loc_part(loc_id(*self), parte, b, (int)sizeof b) == 0 ? fdn_str_make(b, -1) : opt_str(0); \
    }
LOC_OPT(fdn_loc_lang, "$s10Foundation6LocaleV12languageCodeSSSgvg", FDN_LOC_LANG)
LOC_OPT(fdn_loc_region_code, "$s10Foundation6LocaleV10regionCodeSSSgvg", FDN_LOC_REGION)
LOC_OPT(fdn_loc_currency, "$s10Foundation6LocaleV12currencyCodeSSSgvg", FDN_LOC_CURRENCY)
LOC_OPT(fdn_loc_decimal, "$s10Foundation6LocaleV16decimalSeparatorSSSgvg", FDN_LOC_DECIMAL)
LOC_OPT(fdn_loc_grouping, "$s10Foundation6LocaleV17groupingSeparatorSSSgvg", FDN_LOC_GROUPING)

/* usesMetricSystem: só EUA, Libéria e Mianmar não usam o métrico */
SW _Bool fdn_loc_metric(void *const *self CTX) __asm__("$s10Foundation6LocaleV16usesMetricSystemSbvg");
SW _Bool fdn_loc_metric(void *const *self CTX) {
    char r[8];
    if (fdn_loc_part(loc_id(*self), FDN_LOC_REGION, r, (int)sizeof r) != 0) return 1;
    return !(strcmp(r, "US") == 0 || strcmp(r, "LR") == 0 || strcmp(r, "MM") == 0);
}
SW _Bool fdn_loc_eq(void *const *a, void *const *b) __asm__("$s10Foundation6LocaleV2eeoiySbAC_ACtFZ");
SW _Bool fdn_loc_eq(void *const *a, void *const *b) {
    const char *x = loc_id(*a), *y = loc_id(*b);
    return x && y && strcmp(x, y) == 0;
}

/* var region: Locale.Region? (o NSString do código) */
SRET("$s10Foundation6LocaleV6regionAC6RegionVSgvg", fdn_loc_region);
static SW USED void fdn_loc_region(void **out, void *const *self CTX) {
    char r[8];
    opt_obj(out, fdn_loc_part(loc_id(*self), FDN_LOC_REGION, r, (int)sizeof r) == 0 ? fdn_ns_string(r) : 0);
}
SW fdn_str fdn_region_id(void *const *self CTX) __asm__("$s10Foundation6LocaleV6RegionV10identifierSSvg");
SW fdn_str fdn_region_id(void *const *self CTX) {
    const char *s = fdn_ns_utf8(*self);
    return fdn_str_make(s ? s : "", -1);
}

/* ---------------------------------------------------------------- TimeZone */

SRET("$s10Foundation8TimeZoneV7currentACvgZ", fdn_tz_current);
static SW USED void fdn_tz_current(void **out) { *out = fdn_ns_tz_current(); }
SRET("$s10Foundation8TimeZoneV19autoupdatingCurrentACvgZ", fdn_tz_auto);
static SW USED void fdn_tz_auto(void **out) { *out = fdn_ns_tz_current(); }
SW void *fdn_tz_to_ns(void *const *self CTX) __asm__("$s10Foundation8TimeZoneV19_bridgeToObjectiveCSo06NSTimeC0CyF");
SW void *fdn_tz_to_ns(void *const *self CTX) { fdn_ns_retain(*self); return *self; }
SRET("$s10Foundation8TimeZoneV36_unconditionallyBridgeFromObjectiveCyACSo06NSTimeC0CSgFZ", fdn_tz_from_ns);
static SW USED void fdn_tz_from_ns(void **out, void *ns) {
    if (ns) { fdn_ns_retain(ns); *out = ns; }
    else *out = fdn_ns_tz_current();
}
/* init?(secondsFromGMT:): como o swift-foundation, só entre -18 h e +18 h */
SRET("$s10Foundation8TimeZoneV14secondsFromGMTACSgSi_tcfC", fdn_tz_init_secs);
static SW USED void fdn_tz_init_secs(void **out, long secs) {
    opt_obj(out, secs >= -18 * 3600 && secs <= 18 * 3600 ? fdn_ns_tz_with_offset(secs) : 0);
}
SRET("$s10Foundation8TimeZoneV10identifierACSgSSh_tcfC", fdn_tz_init_id);
static SW USED void fdn_tz_init_id(void **out, fdn_str s) {
    char buf[128];
    opt_obj(out, fdn_str_read(s, buf, (long)sizeof buf) > 0 ? fdn_ns_tz_with_name(buf) : 0);
}
SRET("$s10Foundation8TimeZoneV12abbreviationACSgSSh_tcfC", fdn_tz_init_abbr);
static SW USED void fdn_tz_init_abbr(void **out, fdn_str s) {
    char buf[64];
    opt_obj(out, fdn_str_read(s, buf, (long)sizeof buf) > 0 ? fdn_ns_tz_with_abbr(buf) : 0);
}
SW fdn_str fdn_tz_id(void *const *self CTX) __asm__("$s10Foundation8TimeZoneV10identifierSSvg");
SW fdn_str fdn_tz_id(void *const *self CTX) {
    const char *s = fdn_ns_tz_name(*self);
    return fdn_str_make(s ? s : "", -1);
}
/* secondsFromGMT(for: Date): a Date chega por endereço (Double desde 2001) */
SW long fdn_tz_secs_for(const double *date, void *const *self CTX) __asm__("$s10Foundation8TimeZoneV14secondsFromGMT3forSiAA4DateV_tF");
SW long fdn_tz_secs_for(const double *date, void *const *self CTX) { return fdn_ns_tz_offset(*self, *date + FDN_REF_1970); }
SW fdn_str fdn_tz_abbr_for(const double *date, void *const *self CTX) __asm__("$s10Foundation8TimeZoneV12abbreviation3forSSSgAA4DateV_tF");
SW fdn_str fdn_tz_abbr_for(const double *date, void *const *self CTX) { return opt_str(fdn_ns_tz_abbr(*self, *date + FDN_REF_1970)); }
SW _Bool fdn_tz_eq(void *const *a, void *const *b) __asm__("$s10Foundation8TimeZoneV2eeoiySbAC_ACtFZ");
SW _Bool fdn_tz_eq(void *const *a, void *const *b) {
    const char *x = fdn_ns_tz_name(*a), *y = fdn_ns_tz_name(*b);
    return x && y && strcmp(x, y) == 0;
}

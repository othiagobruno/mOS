/*
 * mOS libSystem — NSLocale, NSTimeZone, NSCalendar e NSDateComponents, e as funções C de CFLocale, CFTimeZone,
 * CFCalendar e CFGregorianDate. CF e NS são o mesmo objeto (ponte toll-free), como no macOS.
 *
 * O trabalho de verdade está em fdn_i18n_core.c (ICU). Aqui ficam o estado de cada objeto, a memória e a
 * tradução entre NSString/NSDate/NSArray e os tipos C do núcleo.
 *
 * As classes são definidas em cx_fdn_i18n_init(), chamada pelo boot do ObjC logo depois do cx_foundation_init:
 * antes do foundation_extra.c, cujas versões antigas ficam atrás de "if (!cx_class_named(...))".
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_i18n_core.h"
#include <stdarg.h>

typedef struct { long loc, len; } ns_range_t;
char *strncpy(char *d, const char *s, size_t n);
char *strncat(char *d, const char *s, size_t n);

double floor(double x);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP cx_lookup(cx_class_t *cls, SEL sel);
id cx_num_d(double v);

#define CF_EPOCH 978307200.0   /* 2001-01-01 em segundos desde 1970 */

static id cls(const char *n) { return (id)cx_class_named(n); }
static const char *cs(id s) { return s ? cx_cstr(s) : ""; }
static id str(const char *s) { return cx_str(s ? s : ""); }
static id str_or_nil(const char *s) { return s && s[0] ? cx_str(s) : 0; }
static id num_b(int v) { return MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)(v ? 1 : 0)); }
static double date_t(id d) { return d ? MSG0(double, d, "timeIntervalSince1970") : 0; }
static id date_new(double t) { return MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", t); }
static void super_dealloc(id self) {
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static void each_to_array(const char *s, void *ctx) { cx_arr_add((id)ctx, cx_str(s)); }
static id arr_new(void) { return MSG0(id, cls("NSMutableArray"), "array"); }
static id arr_copy(id a) { return MSG0(id, a, "copy") ? objc_autorelease(MSG0(id, a, "copy")) : a; }

/* ======================================================================== NSLocale */

cx_class_t NSLocale_c __asm__("OBJC_CLASS_$_NSLocale");
cx_class_t NSLocale_m __asm__("OBJC_METACLASS_$_NSLocale");
cx_class_t NSTimeZone_c __asm__("OBJC_CLASS_$_NSTimeZone");
cx_class_t NSTimeZone_m __asm__("OBJC_METACLASS_$_NSTimeZone");
cx_class_t NSCalendar_c __asm__("OBJC_CLASS_$_NSCalendar");
cx_class_t NSCalendar_m __asm__("OBJC_METACLASS_$_NSCalendar");
cx_class_t NSDateComponents_c __asm__("OBJC_CLASS_$_NSDateComponents");
cx_class_t NSDateComponents_m __asm__("OBJC_METACLASS_$_NSDateComponents");

typedef struct { cx_class_t *isa; char ident[96]; int autoupdating; } loc_t;

static const char *loc_current_ident(void) {
    static char cur[96];
    /* AppleLocale do domínio global (o painel de idioma e região grava ali) e, sem ele, o idioma do sistema */
    id ud = MSG0(id, cls("NSUserDefaults"), "standardUserDefaults");
    id v = ud ? MSG(id, ud, "objectForKey:", cx_str("AppleLocale")) : 0;
    const char *base = v && cx_is_str(v) ? cx_cstr(v) : cx_locale_id();
    fdn_loc_canonical(base, cur, sizeof cur);
    return cur;
}

static loc_t *loc_alloc(cx_class_t *k) { return (loc_t *)cx_alloc_instance(k ? k : &NSLocale_c); }
static id loc_make(const char *ident) {
    loc_t *l = loc_alloc(0);
    fdn_loc_canonical(ident, l->ident, sizeof l->ident);
    return (id)l;
}
static const char *loc_id(id self) {
    loc_t *l = (loc_t *)self;
    if (!self) return loc_current_ident();
    return l->autoupdating ? loc_current_ident() : l->ident;
}
/* identificador C de qualquer locale (NSLocale nosso, de app, ou nil = atual) */
const char *fdn_locale_ident_of(id loc) {
    if (!loc) return loc_current_ident();
    if (cx_is_subclass(object_getClass(loc), &NSLocale_c)) return loc_id(loc);
    id s = MSG0(id, loc, "localeIdentifier");
    return s ? cx_cstr(s) : loc_current_ident();
}

static id loc_s_current(id k, SEL c) {
    (void)k; (void)c;
    /* a cada chamada um objeto com o identificador da hora, como o +currentLocale do macOS depois de uma mudança */
    static id cached;
    static char cached_id[96];
    const char *cur = loc_current_ident();
    if (!cached || strcmp(cached_id, cur)) {
        id n = loc_make(cur);
        if (cached) objc_release(cached);
        cached = n;
        strncpy(cached_id, cur, sizeof cached_id - 1);
    }
    return cached;
}
static id loc_s_auto(id k, SEL c) {
    (void)k; (void)c;
    static id a;
    if (!a) { loc_t *l = loc_alloc(0); l->autoupdating = 1; a = (id)l; }
    return a;
}
static id loc_s_system(id k, SEL c) { (void)k; (void)c; static id s; if (!s) s = loc_make(""); return s; }
static id loc_init_ident(id self, SEL c, id ident) { (void)c; fdn_loc_canonical(cs(ident), ((loc_t *)self)->ident, sizeof ((loc_t *)self)->ident); return self; }
static id loc_init(id self, SEL c) { return loc_init_ident(self, c, cx_str("")); }
static id loc_s_with(id k, SEL c, id ident) { (void)c; return objc_autorelease(loc_init_ident((id)loc_alloc((cx_class_t *)k), 0, ident)); }
static id loc_ident(id self, SEL c) { (void)c; return str(loc_id(self)); }
static id loc_part(id self, int part) { char b[96]; fdn_locale_part(loc_id(self), part, b, sizeof b); return str_or_nil(b); }
static id loc_lang(id self, SEL c) { (void)c; return loc_part(self, FDN_LP_LANGUAGE); }
static id loc_region(id self, SEL c) { (void)c; return loc_part(self, FDN_LP_REGION); }
static id loc_script(id self, SEL c) { (void)c; return loc_part(self, FDN_LP_SCRIPT); }
static id loc_variant(id self, SEL c) { (void)c; return loc_part(self, FDN_LP_VARIANT); }
static id loc_collation(id self, SEL c) { (void)c; return loc_part(self, FDN_LP_COLLATION); }
static id loc_lang_ident(id self, SEL c) {
    (void)c;
    char b[96], t[96];
    fdn_locale_part(loc_id(self), FDN_LP_BASE, b, sizeof b);
    fdn_loc_language_tag(b, t, sizeof t);
    return str(t);
}
static id loc_info(id self, int what) { char b[64]; fdn_loc_info(loc_id(self), what, b, sizeof b); return str_or_nil(b); }
static id loc_decimal(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_DECIMAL); }
static id loc_grouping(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_GROUPING); }
static id loc_cur_symbol(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_CURRENCY_SYMBOL); }
static id loc_cur_code(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_CURRENCY_CODE); }
static id loc_qb(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_QUOTE_BEGIN); }
static id loc_qe(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_QUOTE_END); }
static id loc_aqb(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_ALT_QUOTE_BEGIN); }
static id loc_aqe(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_ALT_QUOTE_END); }
static id loc_cal_ident(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_CALENDAR_ID); }
static id loc_measure(id self, SEL c) { (void)c; return loc_info(self, FDN_LN_MEASUREMENT); }
static BOOL loc_metric(id self, SEL c) { (void)c; return (BOOL)fdn_loc_uses_metric(loc_id(self)); }
static id loc_collator(id self, SEL c) {
    (void)c;
    char col[48];
    fdn_locale_part(loc_id(self), FDN_LP_COLLATION, col, sizeof col);
    if (col[0]) { char b[160]; snprintf(b, sizeof b, "%s@collation=%s", loc_id(self), col); return str(b); }
    return str(loc_id(self));
}
static id cal_for(const char *ident, const char *locale, const char *tz);
static id loc_calendar(id self, SEL c) {
    (void)c;
    char ci[48];
    fdn_loc_info(loc_id(self), FDN_LN_CALENDAR_ID, ci, sizeof ci);
    return objc_autorelease(cal_for(ci, loc_id(self), 0));
}
static void exemplar_cb(uint32_t lo, uint32_t hi, void *ctx) {
    ns_range_t r = {(i64)lo, (i64)(hi - lo + 1)};
    MSG(void, (id)ctx, "addCharactersInRange:", r);
}
static id loc_exemplar(id self, SEL c) {
    (void)c;
    id set = MSG0(id, cls("NSMutableCharacterSet"), "new");
    if (!set) return 0;
    fdn_loc_exemplar(loc_id(self), exemplar_cb, set);
    id r = MSG0(id, set, "copy");
    objc_release(set);
    return objc_autorelease(r);
}

static id loc_display(id self, int kind, id code) {
    char b[256];
    if (!code) return 0;
    return fdn_loc_display(loc_id(self), kind, cs(code), b, sizeof b) > 0 ? str(b) : 0;
}
static id loc_ls_ident(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_IDENT, v); }
static id loc_ls_lang(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_LANGUAGE, v); }
static id loc_ls_region(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_REGION, v); }
static id loc_ls_script(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_SCRIPT, v); }
static id loc_ls_variant(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_VARIANT, v); }
static id loc_ls_calendar(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_CALENDAR, v); }
static id loc_ls_collation(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_COLLATION, v); }
static id loc_ls_currency(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_CURRENCY_NAME, v); }
static id loc_ls_collator(id self, SEL c, id v) { (void)c; return loc_display(self, FDN_LD_IDENT, v); }

/* chaves de -objectForKey: e -displayNameForKey:value: (as kCFLocale*Key) */
enum { K_ID, K_LANG, K_REGION, K_SCRIPT, K_VARIANT, K_EXEMPLAR, K_CALENDAR, K_CAL_ID, K_COLLATION, K_METRIC,
       K_MEASURE, K_DECIMAL, K_GROUPING, K_CUR_SYMBOL, K_CUR_CODE, K_COLLATOR, K_QB, K_QE, K_AQB, K_AQE, K_NONE };
static int loc_key(id key) {
    static const char *const k[] = {
        "locale:id", "kCFLocaleLanguageCodeKey", "kCFLocaleCountryCodeKey", "kCFLocaleScriptCodeKey",
        "kCFLocaleVariantCodeKey", "kCFLocaleExemplarCharacterSetKey", "kCFLocaleCalendarKey", "calendar", "collation",
        "kCFLocaleUsesMetricSystemKey", "kCFLocaleMeasurementSystemKey", "kCFLocaleDecimalSeparatorKey",
        "kCFLocaleGroupingSeparatorKey", "kCFLocaleCurrencySymbolKey", "currency", "kCFLocaleCollatorIdentifierKey",
        "kCFLocaleQuotationBeginDelimiterKey", "kCFLocaleQuotationEndDelimiterKey",
        "kCFLocaleAlternateQuotationBeginDelimiterKey", "kCFLocaleAlternateQuotationEndDelimiterKey",
    };
    const char *s = cs(key);
    for (int i = 0; i < K_NONE; i++) if (!strcmp(s, k[i])) return i;
    /* nomes antigos que alguns apps passam */
    if (!strcmp(s, "kCFLocaleIdentifierKey") || !strcmp(s, "localeIdentifier")) return K_ID;
    if (!strcmp(s, "kCFLocaleCurrencyCodeKey")) return K_CUR_CODE;
    if (!strcmp(s, "kCFLocaleCalendarIdentifierKey")) return K_CAL_ID;
    if (!strcmp(s, "kCFLocaleCollationIdentifierKey")) return K_COLLATION;
    return K_NONE;
}
static id loc_object(id self, SEL c, id key) {
    (void)c;
    switch (loc_key(key)) {
    case K_ID: return loc_ident(self, 0);
    case K_LANG: return loc_lang(self, 0);
    case K_REGION: return loc_region(self, 0);
    case K_SCRIPT: return loc_script(self, 0);
    case K_VARIANT: return loc_variant(self, 0);
    case K_EXEMPLAR: return loc_exemplar(self, 0);
    case K_CALENDAR: return loc_calendar(self, 0);
    case K_CAL_ID: return loc_cal_ident(self, 0);
    case K_COLLATION: return loc_collation(self, 0);
    case K_METRIC: return num_b(fdn_loc_uses_metric(loc_id(self)));
    case K_MEASURE: return loc_measure(self, 0);
    case K_DECIMAL: return loc_decimal(self, 0);
    case K_GROUPING: return loc_grouping(self, 0);
    case K_CUR_SYMBOL: return loc_cur_symbol(self, 0);
    case K_CUR_CODE: return loc_cur_code(self, 0);
    case K_COLLATOR: return loc_collator(self, 0);
    case K_QB: return loc_qb(self, 0);
    case K_QE: return loc_qe(self, 0);
    case K_AQB: return loc_aqb(self, 0);
    case K_AQE: return loc_aqe(self, 0);
    }
    return 0;
}
static id loc_display_key(id self, SEL c, id key, id value) {
    (void)c;
    switch (loc_key(key)) {
    case K_ID: case K_COLLATOR: return loc_display(self, FDN_LD_IDENT, value);
    case K_LANG: return loc_display(self, FDN_LD_LANGUAGE, value);
    case K_REGION: return loc_display(self, FDN_LD_REGION, value);
    case K_SCRIPT: return loc_display(self, FDN_LD_SCRIPT, value);
    case K_VARIANT: return loc_display(self, FDN_LD_VARIANT, value);
    case K_CAL_ID: return loc_display(self, FDN_LD_CALENDAR, value);
    case K_COLLATION: return loc_display(self, FDN_LD_COLLATION, value);
    case K_CUR_CODE: return loc_display(self, FDN_LD_CURRENCY_NAME, value);
    case K_CUR_SYMBOL: return loc_display(self, FDN_LD_CURRENCY_SYMBOL, value);
    }
    return 0;
}

static id loc_s_available(id k, SEL c) { (void)k; (void)c; id a = arr_new(); fdn_loc_each_available(each_to_array, a); return arr_copy(a); }
static id loc_s_iso_lang(id k, SEL c) { (void)k; (void)c; id a = arr_new(); fdn_loc_each_iso_language(each_to_array, a); return arr_copy(a); }
static id loc_s_iso_region(id k, SEL c) { (void)k; (void)c; id a = arr_new(); fdn_loc_each_iso_region(each_to_array, a); return arr_copy(a); }
static id loc_s_iso_cur(id k, SEL c) { (void)k; (void)c; id a = arr_new(); fdn_loc_each_currency(0, each_to_array, a); return arr_copy(a); }
static id loc_s_common_cur(id k, SEL c) { (void)k; (void)c; id a = arr_new(); fdn_loc_each_currency(1, each_to_array, a); return arr_copy(a); }
static id loc_s_preferred(id k, SEL c) {
    (void)k; (void)c;
    /* AppleLanguages do usuário; sem ele, o idioma do sistema e o inglês (o macOS sempre tem um fallback) */
    id ud = MSG0(id, cls("NSUserDefaults"), "standardUserDefaults");
    id v = ud ? MSG(id, ud, "objectForKey:", cx_str("AppleLanguages")) : 0;
    if (v && MSG(BOOL, v, "isKindOfClass:", cls("NSArray")) && MSG0(u64, v, "count")) return v;
    id a = arr_new();
    char t[96];
    fdn_loc_language_tag(cx_locale_id(), t, sizeof t);
    cx_arr_add(a, cx_str(t));
    if (strcmp(t, "en-US")) cx_arr_add(a, cx_str("en-US"));
    return arr_copy(a);
}
static id loc_components(id ident) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    const char *s = cs(ident);
    char b[96];
    static const struct { int part; const char *key; } P[] = {
        {FDN_LP_LANGUAGE, "kCFLocaleLanguageCodeKey"}, {FDN_LP_SCRIPT, "kCFLocaleScriptCodeKey"},
        {FDN_LP_REGION, "kCFLocaleCountryCodeKey"}, {FDN_LP_VARIANT, "kCFLocaleVariantCodeKey"},
        {FDN_LP_CALENDAR, "calendar"}, {FDN_LP_COLLATION, "collation"}, {FDN_LP_CURRENCY, "currency"},
        {FDN_LP_NUMBERS, "numbers"},
    };
    for (size_t i = 0; i < sizeof P / sizeof P[0]; i++) {
        fdn_locale_part(s, P[i].part, b, sizeof b);
        if (b[0]) MSG(void, d, "setObject:forKey:", cx_str(b), cx_str(P[i].key));
    }
    return d;
}
static id loc_s_components(id k, SEL c, id ident) { (void)k; (void)c; return loc_components(ident); }
static id loc_from_components(id dict) {
    char b[200] = "", kw[160] = "";
    const char *order[4] = {"kCFLocaleLanguageCodeKey", "kCFLocaleScriptCodeKey", "kCFLocaleCountryCodeKey", "kCFLocaleVariantCodeKey"};
    for (int i = 0; i < 4; i++) {
        id v = MSG(id, dict, "objectForKey:", cx_str(order[i]));
        if (!v) continue;
        if (i) strncat(b, "_", sizeof b - strlen(b) - 1);
        strncat(b, cx_cstr(v), sizeof b - strlen(b) - 1);
    }
    const char *kws[4] = {"calendar", "collation", "currency", "numbers"};
    for (int i = 0; i < 4; i++) {
        id v = MSG(id, dict, "objectForKey:", cx_str(kws[i]));
        if (!v) continue;
        snprintf(kw + strlen(kw), sizeof kw - strlen(kw), "%s%s=%s", kw[0] ? ";" : "@", kws[i], cx_cstr(v));
    }
    strncat(b, kw, sizeof b - strlen(b) - 1);
    return str(b);
}
static id loc_s_from_components(id k, SEL c, id dict) { (void)k; (void)c; return loc_from_components(dict); }
static id loc_s_canon_loc(id k, SEL c, id s) { (void)k; (void)c; char b[160]; fdn_loc_canonical(cs(s), b, sizeof b); return str(b); }
static id loc_s_canon_lang(id k, SEL c, id s) { (void)k; (void)c; char b[160]; fdn_loc_language_tag(cs(s), b, sizeof b); return str(b); }
static i64 loc_s_char_dir(id k, SEL c, id s) { (void)k; (void)c; return fdn_loc_direction(cs(s), 0); }
static i64 loc_s_line_dir(id k, SEL c, id s) { (void)k; (void)c; return fdn_loc_direction(cs(s), 1); }
static BOOL loc_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &NSLocale_c)) return NO;
    return !strcmp(loc_id(self), loc_id(o));
}
static u64 loc_hash(id self, SEL c) { (void)c; u64 h = 5381; for (const char *p = loc_id(self); *p; p++) h = h * 33 + (u8)*p; return h; }
static id loc_copy(id self, SEL c) { (void)c; return objc_retain(self); }
static id loc_copy_zone(id self, SEL c, void *z) { (void)z; return loc_copy(self, c); }
static id loc_desc(id self, SEL c) {
    (void)c;
    char b[160];
    snprintf(b, sizeof b, "%s (%s)", loc_id(self), ((loc_t *)self)->autoupdating ? "current" : "fixed");
    return str(b);
}
static void loc_encode(id self, SEL c, id coder) { (void)c; MSG(void, coder, "encodeObject:forKey:", str(loc_id(self)), cx_str("NS.identifier")); }
static id loc_init_coder(id self, SEL c, id coder) { (void)c; return loc_init_ident(self, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.identifier"))); }
static BOOL yes_m(id k, SEL c) { (void)k; (void)c; return YES; }

/* ======================================================================== NSTimeZone */

typedef struct { cx_class_t *isa; char name[64]; id data; int is_local; } tz_t;
static char g_default_tz[64];     /* +setDefaultTimeZone: */
static id g_abbr_dict;            /* +setAbbreviationDictionary: */

static const char *tz_name_c(id tz) {
    static char sys[64];
    tz_t *t = (tz_t *)tz;
    if (!tz) { fdn_tz_system(sys, sizeof sys); return sys; }
    if (!cx_is_subclass(object_getClass(tz), &NSTimeZone_c)) {
        if (cx_is_str(tz)) return cx_cstr(tz);   /* CFTimeZone antigo era uma string */
        id n = MSG0(id, tz, "name");
        return n ? cx_cstr(n) : "GMT";
    }
    if (t->is_local) {
        if (g_default_tz[0]) return g_default_tz;
        fdn_tz_system(sys, sizeof sys);
        return sys;
    }
    return t->name;
}
const char *fdn_timezone_name_of(id tz) { return tz_name_c(tz); }

static id tz_make(const char *name) {
    tz_t *t = (tz_t *)cx_alloc_instance(&NSTimeZone_c);
    strncpy(t->name, name, sizeof t->name - 1);
    return (id)t;
}
/* fusos já criados: o macOS devolve o mesmo objeto para o mesmo nome */
static id tz_cache[64];
static int tz_ncache;
static volatile int tz_lock;
static id tz_named_c(const char *name) {
    char canon[64];
    if (!name || fdn_tz_canonical(name, canon, sizeof canon) <= 0) return 0;
    while (__atomic_exchange_n(&tz_lock, 1, __ATOMIC_ACQUIRE)) { }
    for (int i = 0; i < tz_ncache; i++)
        if (!strcmp(((tz_t *)tz_cache[i])->name, canon)) { id r = tz_cache[i]; __atomic_store_n(&tz_lock, 0, __ATOMIC_RELEASE); return r; }
    id t = tz_make(canon);
    if (tz_ncache < 64) tz_cache[tz_ncache++] = t;
    else objc_autorelease(t);
    __atomic_store_n(&tz_lock, 0, __ATOMIC_RELEASE);
    return t;
}
static id tz_s_system(id k, SEL c) {
    (void)k; (void)c;
    char n[64];
    fdn_tz_system(n, sizeof n);
    return tz_named_c(n);
}
static id tz_s_local(id k, SEL c) {
    (void)k; (void)c;
    static id l;
    if (!l) { tz_t *t = (tz_t *)cx_alloc_instance(&NSTimeZone_c); t->is_local = 1; l = (id)t; }
    return l;
}
static id tz_s_default(id k, SEL c) { return g_default_tz[0] ? tz_named_c(g_default_tz) : tz_s_system(k, c); }
static void tz_s_set_default(id k, SEL c, id tz) {
    (void)k; (void)c;
    if (!tz) { g_default_tz[0] = 0; return; }
    strncpy(g_default_tz, tz_name_c(tz), sizeof g_default_tz - 1);
}
static void tz_s_reset(id k, SEL c) { (void)k; (void)c; }
static id tz_s_named(id k, SEL c, id name) { (void)k; (void)c; return name ? tz_named_c(cx_cstr(name)) : 0; }
static id tz_s_named_data(id k, SEL c, id name, id data) {
    id t = tz_s_named(k, c, name);
    if (t && data && !((tz_t *)t)->data) ((tz_t *)t)->data = objc_retain(data);
    return t;
}
static id tz_s_abbr(id k, SEL c, id abbr) {
    (void)k; (void)c;
    if (!abbr) return 0;
    if (g_abbr_dict) {
        id n = MSG(id, g_abbr_dict, "objectForKey:", abbr);
        if (n) return tz_named_c(cx_cstr(n));
    }
    const char *n = fdn_tz_from_abbreviation(cx_cstr(abbr));
    if (n) return tz_named_c(n);
    long s;
    return fdn_tz_parse_fixed(cx_cstr(abbr), &s) == 0 ? tz_named_c(cx_cstr(abbr)) : 0;
}
static id tz_s_seconds(id k, SEL c, i64 secs) {
    (void)k; (void)c;
    char n[24];
    if (secs < -18 * 3600 || secs > 18 * 3600) return 0;
    fdn_tz_fixed_name((long)secs, n, sizeof n);
    return tz_named_c(n);
}
static id tz_s_known(id k, SEL c) { (void)k; (void)c; id a = arr_new(); fdn_tz_each_known(each_to_array, a); return arr_copy(a); }
static id tz_s_abbr_dict(id k, SEL c) {
    (void)k; (void)c;
    if (g_abbr_dict) return g_abbr_dict;
    static const char *const ab[] = {"ADT", "AKDT", "AKST", "ART", "AST", "BDT", "BRST", "BRT", "BST", "CAT", "CDT", "CEST", "CET",
        "CLST", "CLT", "COT", "CST", "EAT", "EDT", "EEST", "EET", "EST", "GMT", "GST", "HKT", "HST", "ICT", "IRST", "IST", "JST",
        "KST", "MDT", "MSD", "MSK", "MST", "NZDT", "NZST", "PDT", "PET", "PHT", "PKT", "PST", "SGT", "TRT", "UTC", "WAT", "WEST",
        "WET", "WIT", 0};
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    for (int i = 0; ab[i]; i++) MSG(void, d, "setObject:forKey:", cx_str(fdn_tz_from_abbreviation(ab[i])), cx_str(ab[i]));
    return d;
}
static void tz_s_set_abbr_dict(id k, SEL c, id d) { (void)k; (void)c; if (g_abbr_dict) objc_release(g_abbr_dict); g_abbr_dict = d ? MSG0(id, d, "copy") : 0; }
static id tz_s_version(id k, SEL c) { (void)k; (void)c; return str(fdn_tz_data_version()); }
static id tz_init_name(id self, SEL c, id name) {
    (void)c;
    char canon[64];
    if (!name || fdn_tz_canonical(cx_cstr(name), canon, sizeof canon) <= 0) { objc_release(self); return 0; }
    strncpy(((tz_t *)self)->name, canon, sizeof ((tz_t *)self)->name - 1);
    return self;
}
static id tz_init_name_data(id self, SEL c, id name, id data) {
    id r = tz_init_name(self, c, name);
    if (r && data) ((tz_t *)r)->data = objc_retain(data);
    return r;
}
static id tz_name(id self, SEL c) { (void)c; return str(tz_name_c(self)); }
static id tz_data(id self, SEL c) {
    (void)c;
    tz_t *t = (tz_t *)self;
    if (t->data) return t->data;
    /* o arquivo TZif do sistema, quando existe; senão, dados vazios (o macOS nunca devolve nil) */
    char p[160];
    snprintf(p, sizeof p, "/usr/share/zoneinfo/%s", tz_name_c(self));
    id d = MSG(id, cls("NSData"), "dataWithContentsOfFile:", cx_str(p));
    return d ? d : MSG0(id, cls("NSData"), "data");
}
static i64 tz_seconds_for(id self, SEL c, id date) { (void)c; return fdn_tz_offset(tz_name_c(self), date ? date_t(date) : cx_now(), 0); }
static i64 tz_seconds(id self, SEL c) { return tz_seconds_for(self, c, 0); }
static id tz_abbr_for(id self, SEL c, id date) { (void)c; char b[32]; fdn_tz_abbreviation(tz_name_c(self), date ? date_t(date) : cx_now(), b, sizeof b); return str(b); }
static id tz_abbr(id self, SEL c) { return tz_abbr_for(self, c, 0); }
static BOOL tz_dst_for(id self, SEL c, id date) { (void)c; long d = 0; fdn_tz_offset(tz_name_c(self), date ? date_t(date) : cx_now(), &d); return d != 0; }
static BOOL tz_dst(id self, SEL c) { return tz_dst_for(self, c, 0); }
static double tz_dst_off_for(id self, SEL c, id date) { (void)c; long d = 0; fdn_tz_offset(tz_name_c(self), date ? date_t(date) : cx_now(), &d); return (double)d; }
static double tz_dst_off(id self, SEL c) { return tz_dst_off_for(self, c, 0); }
static id tz_next_for(id self, SEL c, id date) {
    (void)c;
    double t;
    return fdn_tz_next_transition(tz_name_c(self), date ? date_t(date) : cx_now(), &t) ? date_new(t) : 0;
}
static id tz_next(id self, SEL c) { return tz_next_for(self, c, 0); }
static id tz_localized(id self, SEL c, i64 style, id locale) {
    (void)c;
    char b[160];
    return fdn_tz_name(tz_name_c(self), (int)style, fdn_locale_ident_of(locale), cx_now(), b, sizeof b) > 0 ? str(b) : 0;
}
static BOOL tz_equal_tz(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &NSTimeZone_c)) return NO;
    return !strcmp(tz_name_c(self), tz_name_c(o));
}
static u64 tz_hash(id self, SEL c) { (void)c; u64 h = 5381; for (const char *p = tz_name_c(self); *p; p++) h = h * 33 + (u8)*p; return h; }
static id tz_desc(id self, SEL c) {
    (void)c;
    char b[160], ab[32];
    double now = cx_now();
    long dst = 0, off = fdn_tz_offset(tz_name_c(self), now, &dst);
    fdn_tz_abbreviation(tz_name_c(self), now, ab, sizeof ab);
    snprintf(b, sizeof b, "%s (%s) offset %ld%s", tz_name_c(self), ab, off, dst ? " (Daylight)" : "");
    return str(b);
}
static void tz_encode(id self, SEL c, id coder) { (void)c; MSG(void, coder, "encodeObject:forKey:", str(tz_name_c(self)), cx_str("NS.name")); }
static id tz_init_coder(id self, SEL c, id coder) { (void)c; return tz_init_name(self, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.name"))); }
static void tz_dealloc(id self, SEL c) { (void)c; tz_t *t = (tz_t *)self; if (t->data) objc_release(t->data); super_dealloc(self); }

/* ======================================================================== NSCalendar */

typedef struct { cx_class_t *isa; fdn_cal k; id locale; id tz; int autoupdating; } cal_t;
typedef struct { cx_class_t *isa; fdn_comps c; id cal; id tz; } dc_t;

/* sincroniza o fdn_cal com o locale e o fuso do objeto (o autoupdating segue o usuário) */
static const fdn_cal *calk(id self) {
    cal_t *k = (cal_t *)self;
    const char *loc = k->locale ? fdn_locale_ident_of(k->locale) : loc_current_ident();
    const char *tz = k->tz ? tz_name_c(k->tz) : tz_name_c(tz_s_default(0, 0));
    if (strcmp(k->k.locale, loc)) strncpy(k->k.locale, loc, sizeof k->k.locale - 1);
    if (strcmp(k->k.tz, tz)) strncpy(k->k.tz, tz, sizeof k->k.tz - 1);
    return &k->k;
}
static id cal_for(const char *ident, const char *locale, const char *tz) {
    cal_t *k = (cal_t *)cx_alloc_instance(&NSCalendar_c);
    fdn_cal_init(&k->k, ident, locale, tz);
    if (locale) k->locale = objc_retain(loc_make(locale));
    if (tz) k->tz = objc_retain(tz_named_c(tz));
    return (id)k;
}
static id cal_s_current(id kk, SEL c) {
    (void)kk; (void)c;
    char ci[48];
    fdn_loc_info(loc_current_ident(), FDN_LN_CALENDAR_ID, ci, sizeof ci);
    cal_t *k = (cal_t *)cal_for(ci, 0, 0);
    return objc_autorelease((id)k);
}
static id cal_s_auto(id kk, SEL c) {
    (void)kk; (void)c;
    static id a;
    if (!a) { char ci[48]; fdn_loc_info(loc_current_ident(), FDN_LN_CALENDAR_ID, ci, sizeof ci); a = cal_for(ci, 0, 0); ((cal_t *)a)->autoupdating = 1; }
    return a;
}
static id cal_init_ident(id self, SEL c, id ident) {
    (void)c;
    if (!ident || !fdn_cal_ident_ok(cx_cstr(ident))) { objc_release(self); return 0; }
    fdn_cal_init(&((cal_t *)self)->k, cx_cstr(ident), 0, 0);
    return self;
}
static id cal_s_with(id kk, SEL c, id ident) {
    (void)kk; (void)c;
    if (!ident || !fdn_cal_ident_ok(cx_cstr(ident))) return 0;
    return objc_autorelease(cal_for(cx_cstr(ident), 0, 0));
}
static id cal_ident(id self, SEL c) { (void)c; return str(((cal_t *)self)->k.ident); }
static id cal_locale(id self, SEL c) { (void)c; cal_t *k = (cal_t *)self; return k->locale ? k->locale : loc_s_current(0, 0); }
static void cal_set_locale(id self, SEL c, id l) { (void)c; cal_t *k = (cal_t *)self; id o = k->locale; k->locale = l ? objc_retain(l) : 0; if (o) objc_release(o); }
static id cal_tz(id self, SEL c) { (void)c; cal_t *k = (cal_t *)self; return k->tz ? k->tz : tz_s_default(0, 0); }
static void cal_set_tz(id self, SEL c, id tz) { (void)c; cal_t *k = (cal_t *)self; id o = k->tz; k->tz = tz ? objc_retain(tz) : 0; if (o) objc_release(o); }
static u64 cal_first_wd(id self, SEL c) { (void)c; return (u64)fdn_cal_first_weekday(calk(self)); }
static void cal_set_first_wd(id self, SEL c, u64 v) { (void)c; ((cal_t *)self)->k.first_weekday = (int)v; }
static u64 cal_min_days(id self, SEL c) { (void)c; return (u64)fdn_cal_min_days(calk(self)); }
static void cal_set_min_days(id self, SEL c, u64 v) { (void)c; ((cal_t *)self)->k.min_days = (int)v; }
static id cal_syms(id self, int kind) { id a = arr_new(); fdn_cal_symbols(calk(self), kind, each_to_array, a); return arr_copy(a); }
#define SYM(fn, kind) static id fn(id self, SEL c) { (void)c; return cal_syms(self, kind); }
SYM(cal_eras, FDN_SYM_ERAS) SYM(cal_long_eras, FDN_SYM_LONG_ERAS) SYM(cal_months, FDN_SYM_MONTHS)
SYM(cal_short_months, FDN_SYM_SHORT_MONTHS) SYM(cal_vs_months, FDN_SYM_VERY_SHORT_MONTHS)
SYM(cal_sa_months, FDN_SYM_SA_MONTHS) SYM(cal_sa_short_months, FDN_SYM_SA_SHORT_MONTHS)
SYM(cal_sa_vs_months, FDN_SYM_SA_VERY_SHORT_MONTHS) SYM(cal_weekdays, FDN_SYM_WEEKDAYS)
SYM(cal_short_weekdays, FDN_SYM_SHORT_WEEKDAYS) SYM(cal_vs_weekdays, FDN_SYM_VERY_SHORT_WEEKDAYS)
SYM(cal_sa_weekdays, FDN_SYM_SA_WEEKDAYS) SYM(cal_sa_short_weekdays, FDN_SYM_SA_SHORT_WEEKDAYS)
SYM(cal_sa_vs_weekdays, FDN_SYM_SA_VERY_SHORT_WEEKDAYS) SYM(cal_quarters, FDN_SYM_QUARTERS)
SYM(cal_short_quarters, FDN_SYM_SHORT_QUARTERS) SYM(cal_sa_quarters, FDN_SYM_SA_QUARTERS)
SYM(cal_sa_short_quarters, FDN_SYM_SA_SHORT_QUARTERS)
#undef SYM
static id cal_am(id self, SEL c) { (void)c; id a = cal_syms(self, FDN_SYM_AM); return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : str("AM"); }
static id cal_pm(id self, SEL c) { (void)c; id a = cal_syms(self, FDN_SYM_PM); return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : str("PM"); }

static ns_range_t nsr(long loc, long len) { ns_range_t r = {loc, len}; return r; }
static ns_range_t not_found(void) { return nsr(0x7fffffffffffffffLL, 0); }
static ns_range_t cal_min_range(id self, SEL c, u64 unit) { (void)c; long l, n; return fdn_cal_limit(calk(self), unit, 0, &l, &n) ? nsr(l, n) : not_found(); }
static ns_range_t cal_max_range(id self, SEL c, u64 unit) { (void)c; long l, n; return fdn_cal_limit(calk(self), unit, 1, &l, &n) ? nsr(l, n) : not_found(); }
static ns_range_t cal_range(id self, SEL c, u64 small, u64 large, id date) { (void)c; long l, n; return fdn_cal_range(calk(self), small, large, date_t(date), &l, &n) ? nsr(l, n) : not_found(); }
static u64 cal_ordinality(id self, SEL c, u64 small, u64 large, id date) {
    (void)c;
    long r = fdn_cal_ordinality(calk(self), small, large, date_t(date));
    return r < 0 ? (u64)0x7fffffffffffffffULL : (u64)r;
}
static BOOL cal_range_start(id self, SEL c, u64 unit, id *start, double *len, id date) {
    (void)c;
    double s, l;
    if (!fdn_cal_interval(calk(self), unit, date_t(date), &s, &l)) return NO;
    if (start) *start = date_new(s);
    if (len) *len = l;
    return YES;
}

static dc_t *dc_alloc(void) { dc_t *d = (dc_t *)cx_alloc_instance(&NSDateComponents_c); fdn_comps_clear(&d->c); return d; }
static id comps_obj(id cal, const fdn_comps *c, u64 units) {
    dc_t *d = dc_alloc();
    d->c = *c;
    if (units & FDN_U_CALENDAR) d->cal = MSG0(id, cal, "copy");
    if (units & FDN_U_TIMEZONE) d->tz = objc_retain(cal_tz(cal, 0));
    return objc_autorelease((id)d);
}
static fdn_comps comps_of(id dc) {
    fdn_comps r;
    if (dc && cx_is_subclass(object_getClass(dc), &NSDateComponents_c)) return ((dc_t *)dc)->c;
    fdn_comps_clear(&r);
    return r;
}
static id cal_components(id self, SEL c, u64 units, id date) {
    (void)c;
    fdn_comps x;
    fdn_cal_components(calk(self), units, date_t(date), &x);
    return comps_obj(self, &x, units);
}
static id cal_components_tz(id self, SEL c, id tz, id date) {
    (void)c;
    id copy = MSG0(id, self, "copy");
    cal_set_tz(copy, 0, tz);
    fdn_comps x;
    fdn_cal_components(calk(copy), 0x3FFFFFFF, date_t(date), &x);
    id r = comps_obj(copy, &x, 0x3FFFFFFF | FDN_U_CALENDAR | FDN_U_TIMEZONE);
    objc_release(copy);
    return r;
}
static id cal_date_from(id self, SEL c, id comps) {
    (void)c;
    fdn_comps x = comps_of(comps);
    double t;
    dc_t *d = (dc_t *)comps;
    id use = self;
    if (comps && d->tz) { use = MSG0(id, self, "copy"); cal_set_tz(use, 0, d->tz); }
    int ok = fdn_cal_date(calk(use), &x, &t);
    if (use != self) objc_release(use);
    return ok ? date_new(t) : 0;
}
static id cal_by_adding(id self, SEL c, id comps, id date, u64 opts) {
    (void)c;
    fdn_comps x = comps_of(comps);
    double t;
    return date && fdn_cal_add(calk(self), &x, date_t(date), opts, &t) ? date_new(t) : 0;
}
static id cal_by_adding_unit(id self, SEL c, u64 unit, i64 value, id date, u64 opts) {
    (void)c;
    fdn_comps x;
    fdn_comps_clear(&x);
    int64_t *f = fdn_comps_field(&x, unit);
    double t;
    if (!f || !date) return 0;
    *f = value;
    return fdn_cal_add(calk(self), &x, date_t(date), opts, &t) ? date_new(t) : 0;
}
static id cal_diff(id self, SEL c, u64 units, id from, id to, u64 opts) {
    (void)c; (void)opts;
    fdn_comps x;
    fdn_cal_difference(calk(self), units, date_t(from), date_t(to), &x);
    return comps_obj(self, &x, units);
}
static id cal_diff_comps(id self, SEL c, u64 units, id a, id b, u64 opts) {
    id da = cal_date_from(self, c, a), db = cal_date_from(self, c, b);
    return da && db ? cal_diff(self, c, units, da, db, opts) : 0;
}
static i64 cal_component(id self, SEL c, u64 unit, id date) {
    (void)c;
    fdn_comps x;
    fdn_cal_components(calk(self), unit, date_t(date), &x);
    if (unit == FDN_U_CALENDAR || unit == FDN_U_TIMEZONE) return 0;
    int64_t *f = fdn_comps_field(&x, unit);
    return f ? *f : 0x7fffffffffffffffLL;
}
static void cal_get_era(id self, SEL c, i64 *era, i64 *year, i64 *month, i64 *day, id date) {
    (void)c;
    fdn_comps x;
    fdn_cal_components(calk(self), FDN_U_ERA | FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY, date_t(date), &x);
    if (era) *era = x.era;
    if (year) *year = x.year;
    if (month) *month = x.month;
    if (day) *day = x.day;
}
static void cal_get_week(id self, SEL c, i64 *era, i64 *yfw, i64 *woy, i64 *wd, id date) {
    (void)c;
    fdn_comps x;
    fdn_cal_components(calk(self), FDN_U_ERA | FDN_U_YEAR_FOR_WEEK | FDN_U_WEEK_OF_YEAR | FDN_U_WEEKDAY, date_t(date), &x);
    if (era) *era = x.era;
    if (yfw) *yfw = x.year_for_week;
    if (woy) *woy = x.week_of_year;
    if (wd) *wd = x.weekday;
}
static void cal_get_time(id self, SEL c, i64 *h, i64 *m, i64 *s, i64 *ns, id date) {
    (void)c;
    fdn_comps x;
    fdn_cal_components(calk(self), FDN_U_HOUR | FDN_U_MINUTE | FDN_U_SECOND | FDN_U_NANOSECOND, date_t(date), &x);
    if (h) *h = x.hour;
    if (m) *m = x.minute;
    if (s) *s = x.second;
    if (ns) *ns = x.nanosecond;
}
static id cal_date_era(id self, SEL c, i64 era, i64 y, i64 mo, i64 d, i64 h, i64 mi, i64 s, i64 ns) {
    (void)c;
    fdn_comps x;
    double t;
    fdn_comps_clear(&x);
    x.era = era; x.year = y; x.month = mo; x.day = d; x.hour = h; x.minute = mi; x.second = s; x.nanosecond = ns;
    return fdn_cal_date(calk(self), &x, &t) ? date_new(t) : 0;
}
static id cal_date_week(id self, SEL c, i64 era, i64 yfw, i64 woy, i64 wd, i64 h, i64 mi, i64 s, i64 ns) {
    (void)c;
    fdn_comps x;
    double t;
    fdn_comps_clear(&x);
    x.era = era; x.year_for_week = yfw; x.week_of_year = woy; x.weekday = wd; x.hour = h; x.minute = mi; x.second = s; x.nanosecond = ns;
    return fdn_cal_date(calk(self), &x, &t) ? date_new(t) : 0;
}
static id cal_start_of_day(id self, SEL c, id date) {
    (void)c;
    double s, l;
    return fdn_cal_interval(calk(self), FDN_U_DAY, date_t(date), &s, &l) ? date_new(s) : 0;
}
static i64 cal_compare(id self, SEL c, id a, id b, u64 unit) {
    (void)c;
    double sa, sb, l;
    if (!fdn_cal_interval(calk(self), unit, date_t(a), &sa, &l) || !fdn_cal_interval(calk(self), unit, date_t(b), &sb, &l)) return 0;
    return sa < sb ? -1 : sa > sb ? 1 : 0;
}
static BOOL cal_is_equal_gran(id self, SEL c, id a, id b, u64 unit) { return cal_compare(self, c, a, b, unit) == 0; }
static BOOL cal_same_day(id self, SEL c, id a, id b) { return cal_compare(self, c, a, b, FDN_U_DAY) == 0; }
static BOOL cal_day_offset(id self, id date, int days) {
    double s, l, ds, dl;
    fdn_comps x;
    fdn_comps_clear(&x);
    x.day = days;
    double ref;
    if (!fdn_cal_add(calk(self), &x, cx_now(), 0, &ref)) return NO;
    if (!fdn_cal_interval(calk(self), FDN_U_DAY, ref, &s, &l) || !fdn_cal_interval(calk(self), FDN_U_DAY, date_t(date), &ds, &dl)) return NO;
    return s == ds;
}
static BOOL cal_today(id self, SEL c, id d) { (void)c; return cal_day_offset(self, d, 0); }
static BOOL cal_yesterday(id self, SEL c, id d) { (void)c; return cal_day_offset(self, d, -1); }
static BOOL cal_tomorrow(id self, SEL c, id d) { (void)c; return cal_day_offset(self, d, 1); }
static BOOL cal_weekend(id self, SEL c, id d) { (void)c; return (BOOL)fdn_cal_is_weekend(calk(self), date_t(d)); }
static BOOL cal_weekend_range(id self, SEL c, id *start, double *len, id date) {
    (void)c;
    double s, l;
    if (!fdn_cal_weekend_range(calk(self), date_t(date), 0, &s, &l)) return NO;
    if (start) *start = date_new(s);
    if (len) *len = l;
    return YES;
}
static BOOL cal_next_weekend(id self, SEL c, id *start, double *len, u64 opts, id date) {
    (void)c;
    double s, l;
    if (opts & FDN_CO_BACKWARDS) {
        /* para trás: o fim de semana que acabou antes de date */
        double t = date_t(date);
        for (int i = 0; i < 14; i++) {
            t -= 86400;
            if (fdn_cal_weekend_range(calk(self), t, 0, &s, &l) && s + l <= date_t(date)) goto found;
        }
        return NO;
    }
    if (!fdn_cal_weekend_range(calk(self), date_t(date), 1, &s, &l)) return NO;
found:
    if (start) *start = date_new(s);
    if (len) *len = l;
    return YES;
}
static id cal_next_match(id self, SEL c, id date, id comps, u64 opts) {
    (void)c;
    fdn_comps w = comps_of(comps);
    double t;
    return fdn_cal_next_match(calk(self), date_t(date), &w, opts, &t) ? date_new(t) : 0;
}
static id cal_next_unit(id self, SEL c, id date, u64 unit, i64 value, u64 opts) {
    fdn_comps w;
    double t;
    fdn_comps_clear(&w);
    int64_t *f = fdn_comps_field(&w, unit);
    (void)c;
    if (!f) return 0;
    *f = value;
    return fdn_cal_next_match(calk(self), date_t(date), &w, opts, &t) ? date_new(t) : 0;
}
static id cal_next_hms(id self, SEL c, id date, i64 h, i64 m, i64 s, u64 opts) {
    fdn_comps w;
    double t;
    (void)c;
    fdn_comps_clear(&w);
    w.hour = h; w.minute = m; w.second = s;
    return fdn_cal_next_match(calk(self), date_t(date), &w, opts, &t) ? date_new(t) : 0;
}
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id, BOOL, BOOL *); } blk_t;
static void cal_enumerate(id self, SEL c, id start, id comps, u64 opts, void *block) {
    (void)c;
    fdn_comps w = comps_of(comps);
    double t = date_t(start), n;
    blk_t *b = block;
    BOOL stop = NO;
    for (int i = 0; i < 100000 && !stop; i++) {
        if (!fdn_cal_next_match(calk(self), t, &w, opts, &n)) break;
        b->invoke(b, date_new(n), YES, &stop);
        t = n;
    }
}
static BOOL cal_matches(id self, SEL c, id date, id comps) {
    (void)c;
    fdn_comps have, w = comps_of(comps);
    fdn_cal_components(calk(self), 0x3FFFFFFF, date_t(date), &have);
#define M(f) if (w.f != FDN_UNDEF && w.f != have.f) return NO;
    M(era) M(year) M(month) M(day) M(hour) M(minute) M(second) M(weekday) M(weekday_ordinal) M(quarter)
    M(week_of_month) M(week_of_year) M(year_for_week) M(day_of_year)
#undef M
    return YES;
}
static id cal_set_unit(id self, SEL c, u64 unit, i64 value, id date, u64 opts) {
    /* dateBySettingUnit: a próxima data (a partir de date) em que a unidade vale value; as menores voltam ao início */
    fdn_comps now, w;
    double t, s, l;
    (void)c;
    fdn_cal_components(calk(self), unit, date_t(date), &now);
    int64_t *f = fdn_comps_field(&now, unit);
    if (!f) return 0;
    if (*f == value) return date;
    fdn_comps_clear(&w);
    *fdn_comps_field(&w, unit) = value;
    if (!fdn_cal_next_match(calk(self), date_t(date), &w, opts | FDN_CO_NEXT_TIME, &t)) return 0;
    if (!fdn_cal_interval(calk(self), unit, t, &s, &l)) return 0;
    return date_new(s);
}
static id cal_set_hms(id self, SEL c, i64 h, i64 m, i64 s, id date, u64 opts) {
    (void)c; (void)opts;
    fdn_comps x;
    double t;
    fdn_cal_components(calk(self), FDN_U_ERA | FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY, date_t(date), &x);
    x.hour = h; x.minute = m; x.second = s;
    return fdn_cal_date(calk(self), &x, &t) ? date_new(t) : 0;
}
static id cal_copy(id self, SEL c) {
    (void)c;
    cal_t *o = (cal_t *)self, *k = (cal_t *)cx_alloc_instance(&NSCalendar_c);
    k->k = o->k;
    k->locale = o->locale ? objc_retain(o->locale) : 0;
    k->tz = o->tz ? objc_retain(o->tz) : 0;
    return (id)k;
}
static id cal_copy_zone(id self, SEL c, void *z) { (void)z; return cal_copy(self, c); }
static BOOL cal_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &NSCalendar_c)) return NO;
    const fdn_cal *a = calk(self), *b = calk(o);
    return !strcmp(a->ident, b->ident) && !strcmp(a->locale, b->locale) && !strcmp(a->tz, b->tz) &&
           fdn_cal_first_weekday(a) == fdn_cal_first_weekday(b) && fdn_cal_min_days(a) == fdn_cal_min_days(b);
}
static u64 cal_hash(id self, SEL c) { (void)c; u64 h = 5381; for (const char *p = calk(self)->ident; *p; p++) h = h * 33 + (u8)*p; return h; }
static id cal_desc(id self, SEL c) {
    (void)c;
    char b[200];
    const fdn_cal *k = calk(self);
    snprintf(b, sizeof b, "<NSCalendar> %s (locale %s, time zone %s, first weekday %d, min days %d)", k->ident, k->locale, k->tz,
             fdn_cal_first_weekday(k), fdn_cal_min_days(k));
    return str(b);
}
static void cal_encode(id self, SEL c, id coder) {
    (void)c;
    MSG(void, coder, "encodeObject:forKey:", str(((cal_t *)self)->k.ident), cx_str("NS.identifier"));
    MSG(void, coder, "encodeObject:forKey:", cal_tz(self, 0), cx_str("NS.timezone"));
    MSG(void, coder, "encodeObject:forKey:", cal_locale(self, 0), cx_str("NS.locale"));
    MSG(void, coder, "encodeInteger:forKey:", (i64)cal_first_wd(self, 0), cx_str("NS.firstwkdy"));
    MSG(void, coder, "encodeInteger:forKey:", (i64)cal_min_days(self, 0), cx_str("NS.mindays"));
}
static id cal_init_coder(id self, SEL c, id coder) {
    (void)c;
    id r = cal_init_ident(self, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.identifier")));
    if (!r) return 0;
    cal_set_tz(r, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.timezone")));
    cal_set_locale(r, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.locale")));
    ((cal_t *)r)->k.first_weekday = (int)MSG(i64, coder, "decodeIntegerForKey:", cx_str("NS.firstwkdy"));
    ((cal_t *)r)->k.min_days = (int)MSG(i64, coder, "decodeIntegerForKey:", cx_str("NS.mindays"));
    return r;
}
static void cal_dealloc(id self, SEL c) {
    (void)c;
    cal_t *k = (cal_t *)self;
    if (k->locale) objc_release(k->locale);
    if (k->tz) objc_release(k->tz);
    super_dealloc(self);
}

/* ======================================================================== NSDateComponents */

static id dc_init(id self, SEL c) { (void)c; fdn_comps_clear(&((dc_t *)self)->c); return self; }
#define DCP(get, set, field) \
    static i64 dc_g_##field(id self, SEL c) { (void)c; return ((dc_t *)self)->c.field; } \
    static void dc_s_##field(id self, SEL c, i64 v) { (void)c; ((dc_t *)self)->c.field = v; }
DCP(era, setEra, era) DCP(year, setYear, year) DCP(month, setMonth, month) DCP(day, setDay, day)
DCP(hour, setHour, hour) DCP(minute, setMinute, minute) DCP(second, setSecond, second)
DCP(nanosecond, setNanosecond, nanosecond) DCP(weekday, setWeekday, weekday)
DCP(weekdayOrdinal, setWeekdayOrdinal, weekday_ordinal) DCP(quarter, setQuarter, quarter)
DCP(weekOfMonth, setWeekOfMonth, week_of_month) DCP(weekOfYear, setWeekOfYear, week_of_year)
DCP(yearForWeekOfYear, setYearForWeekOfYear, year_for_week) DCP(dayOfYear, setDayOfYear, day_of_year)
#undef DCP
static i64 dc_week(id self, SEL c) { (void)c; return ((dc_t *)self)->c.week_of_year; }   /* -week (obsoleto) */
static void dc_set_week(id self, SEL c, i64 v) { (void)c; ((dc_t *)self)->c.week_of_year = v; }
static BOOL dc_leap(id self, SEL c) { (void)c; return ((dc_t *)self)->c.leap_month == 1; }
static void dc_set_leap(id self, SEL c, BOOL v) { (void)c; ((dc_t *)self)->c.leap_month = v ? 1 : 0; }
static id dc_cal(id self, SEL c) { (void)c; return ((dc_t *)self)->cal; }
static void dc_set_cal(id self, SEL c, id cal) {
    (void)c;
    dc_t *d = (dc_t *)self;
    id o = d->cal;
    d->cal = cal ? MSG0(id, cal, "copy") : 0;
    if (o) objc_release(o);
    if (d->cal && d->tz) cal_set_tz(d->cal, 0, d->tz);
}
static id dc_tz(id self, SEL c) { (void)c; return ((dc_t *)self)->tz; }
static void dc_set_tz(id self, SEL c, id tz) {
    (void)c;
    dc_t *d = (dc_t *)self;
    id o = d->tz;
    d->tz = tz ? objc_retain(tz) : 0;
    if (o) objc_release(o);
    if (d->cal && tz) cal_set_tz(d->cal, 0, tz);
}
static id dc_date(id self, SEL c) { (void)c; dc_t *d = (dc_t *)self; return d->cal ? cal_date_from(d->cal, 0, self) : 0; }
static BOOL dc_valid_in(id self, SEL c, id cal) {
    (void)c;
    /* válido quando a data montada devolve os mesmos componentes que vieram */
    if (!cal) return NO;
    dc_t *d = (dc_t *)self;
    id date = cal_date_from(cal, 0, self);
    if (!date) return NO;
    fdn_comps back;
    fdn_cal_components(calk(cal), 0x3FFFFFFF, date_t(date), &back);
#define V(f) if (d->c.f != FDN_UNDEF && d->c.f != back.f) return NO;
    V(era) V(year) V(month) V(day) V(hour) V(minute) V(second) V(weekday) V(weekday_ordinal) V(week_of_month)
    V(week_of_year) V(year_for_week) V(day_of_year)
#undef V
    if (d->c.quarter != FDN_UNDEF && d->c.quarter != back.quarter) return NO;
    return YES;
}
static BOOL dc_valid(id self, SEL c) { return dc_valid_in(self, c, ((dc_t *)self)->cal); }
static i64 dc_value_for(id self, SEL c, u64 unit) { (void)c; int64_t *f = fdn_comps_field(&((dc_t *)self)->c, unit); return f ? *f : FDN_UNDEF; }
static void dc_set_value_for(id self, SEL c, i64 v, u64 unit) { (void)c; int64_t *f = fdn_comps_field(&((dc_t *)self)->c, unit); if (f) *f = v; }
static id dc_copy(id self, SEL c) {
    (void)c;
    dc_t *o = (dc_t *)self, *d = dc_alloc();
    d->c = o->c;
    d->cal = o->cal ? MSG0(id, o->cal, "copy") : 0;
    d->tz = o->tz ? objc_retain(o->tz) : 0;
    return (id)d;
}
static id dc_copy_zone(id self, SEL c, void *z) { (void)z; return dc_copy(self, c); }
static BOOL dc_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &NSDateComponents_c)) return NO;
    dc_t *a = (dc_t *)self, *b = (dc_t *)o;
    if (memcmp(&a->c, &b->c, sizeof a->c)) return NO;
    if ((a->cal != 0) != (b->cal != 0) || (a->cal && !cal_equal(a->cal, 0, b->cal))) return NO;
    if ((a->tz != 0) != (b->tz != 0) || (a->tz && !tz_equal_tz(a->tz, 0, b->tz))) return NO;
    return YES;
}
static u64 dc_hash(id self, SEL c) {
    (void)c;
    const fdn_comps *x = &((dc_t *)self)->c;
    u64 h = 0;
    const int64_t *p = &x->era;
    for (int i = 0; i < 15; i++) h = h * 31 + (u64)p[i];
    return h;
}
static id dc_desc(id self, SEL c) {
    (void)c;
    char b[600];
    int o = 0;
    const fdn_comps *x = &((dc_t *)self)->c;
    static const char *const names[] = {"Era", "Calendar Year", "Month", "Day", "Hour", "Minute", "Second", "Nanosecond",
        "Weekday", "Weekday Ordinal", "Quarter", "Week of Month", "Week of Year", "Year for Week of Year", "Day of Year"};
    const int64_t *p = &x->era;
    o += snprintf(b + o, sizeof b - (size_t)o, "<NSDateComponents>");
    for (int i = 0; i < 15; i++)
        if (p[i] != FDN_UNDEF) o += snprintf(b + o, sizeof b - (size_t)o, " %s: %lld", names[i], (long long)p[i]);
    if (x->leap_month == 1) snprintf(b + o, sizeof b - (size_t)o, " Leap Month: 1");
    return str(b);
}
static void dc_encode(id self, SEL c, id coder) {
    (void)c;
    static const char *const keys[] = {"NS.era", "NS.year", "NS.month", "NS.day", "NS.hour", "NS.minute", "NS.second",
        "NS.nanosec", "NS.weekday", "NS.weekdayOrdinal", "NS.quarter", "NS.weekOfMonth", "NS.weekOfYear", "NS.yearForWOY", "NS.dayOfYear"};
    dc_t *d = (dc_t *)self;
    const int64_t *p = &d->c.era;
    for (int i = 0; i < 15; i++) MSG(void, coder, "encodeInteger:forKey:", (i64)p[i], cx_str(keys[i]));
    MSG(void, coder, "encodeBool:forKey:", (BOOL)(d->c.leap_month == 1), cx_str("NS.isLeapMonth"));
    if (d->cal) MSG(void, coder, "encodeObject:forKey:", d->cal, cx_str("NS.calendar"));
    if (d->tz) MSG(void, coder, "encodeObject:forKey:", d->tz, cx_str("NS.timezone"));
}
static id dc_init_coder(id self, SEL c, id coder) {
    (void)c;
    static const char *const keys[] = {"NS.era", "NS.year", "NS.month", "NS.day", "NS.hour", "NS.minute", "NS.second",
        "NS.nanosec", "NS.weekday", "NS.weekdayOrdinal", "NS.quarter", "NS.weekOfMonth", "NS.weekOfYear", "NS.yearForWOY", "NS.dayOfYear"};
    dc_t *d = (dc_t *)dc_init(self, 0);
    int64_t *p = &d->c.era;
    for (int i = 0; i < 15; i++)
        if (MSG(BOOL, coder, "containsValueForKey:", cx_str(keys[i]))) p[i] = MSG(i64, coder, "decodeIntegerForKey:", cx_str(keys[i]));
    if (MSG(BOOL, coder, "containsValueForKey:", cx_str("NS.isLeapMonth")))
        d->c.leap_month = MSG(BOOL, coder, "decodeBoolForKey:", cx_str("NS.isLeapMonth")) ? 1 : 0;
    dc_set_cal(self, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.calendar")));
    dc_set_tz(self, 0, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.timezone")));
    return self;
}
static void dc_dealloc(id self, SEL c) {
    (void)c;
    dc_t *d = (dc_t *)self;
    if (d->cal) objc_release(d->cal);
    if (d->tz) objc_release(d->tz);
    super_dealloc(self);
}

/* ======================================================================== CF (toll-free) */

typedef struct { long location, length; } cf_range_t;
static id ret(id o) { return o ? objc_retain(o) : 0; }

u64 CFLocaleGetTypeID(void) { return 25; }
id CFLocaleCopyCurrent(void) { return ret(loc_s_current(0, 0)); }
id CFLocaleGetSystem(void) { return loc_s_system(0, 0); }
id CFLocaleCreate(void *alloc, id ident) { (void)alloc; return ident ? loc_make(cx_cstr(ident)) : 0; }
id CFLocaleCreateCopy(void *alloc, id loc) { (void)alloc; return ret(loc); }
id CFLocaleGetIdentifier(id loc) { return objc_autorelease(ret(str(fdn_locale_ident_of(loc)))); }
id CFLocaleGetValue(id loc, id key) {
    id l = loc && cx_is_subclass(object_getClass(loc), &NSLocale_c) ? loc : loc_make(fdn_locale_ident_of(loc));
    id r = loc_object(l, 0, key);
    if (l != loc) objc_release(l);
    return r;
}
id CFLocaleCopyDisplayNameForPropertyValue(id displayLocale, id key, id value) {
    id l = displayLocale && cx_is_subclass(object_getClass(displayLocale), &NSLocale_c) ? displayLocale : loc_make(fdn_locale_ident_of(displayLocale));
    id r = loc_display_key(l, 0, key, value);
    if (l != displayLocale) objc_release(l);
    return ret(r);
}
id CFLocaleCopyAvailableLocaleIdentifiers(void) { return ret(loc_s_available(0, 0)); }
id CFLocaleCopyISOLanguageCodes(void) { return ret(loc_s_iso_lang(0, 0)); }
id CFLocaleCopyISOCountryCodes(void) { return ret(loc_s_iso_region(0, 0)); }
id CFLocaleCopyISOCurrencyCodes(void) { return ret(loc_s_iso_cur(0, 0)); }
id CFLocaleCopyCommonISOCurrencyCodes(void) { return ret(loc_s_common_cur(0, 0)); }
id CFLocaleCopyPreferredLanguages(void) { return ret(loc_s_preferred(0, 0)); }
id CFLocaleCreateCanonicalLocaleIdentifierFromString(void *alloc, id s) { (void)alloc; return ret(loc_s_canon_loc(0, 0, s)); }
id CFLocaleCreateCanonicalLanguageIdentifierFromString(void *alloc, id s) { (void)alloc; return ret(loc_s_canon_lang(0, 0, s)); }
id CFLocaleCreateCanonicalLocaleIdentifierFromScriptManagerCodes(void *alloc, short lang, short region) {
    (void)alloc; (void)region;
    /* códigos do Script Manager: só os de idioma mais comuns (0 inglês, 8 espanhol, 1 francês, 2 alemão, 3 italiano, 71 português) */
    const char *l = lang == 0 ? "en" : lang == 1 ? "fr" : lang == 2 ? "de" : lang == 3 ? "it" : lang == 6 ? "es" : lang == 8 ? "pt" : "en";
    return ret(str(l));
}
id CFLocaleCreateComponentsFromLocaleIdentifier(void *alloc, id ident) { (void)alloc; return ret(loc_components(ident)); }
id CFLocaleCreateLocaleIdentifierFromComponents(void *alloc, id dict) { (void)alloc; return ret(loc_from_components(dict)); }
long CFLocaleGetLanguageCharacterDirection(id iso) { return fdn_loc_direction(cs(iso), 0); }
long CFLocaleGetLanguageLineDirection(id iso) { return fdn_loc_direction(cs(iso), 1); }

u64 CFTimeZoneGetTypeID(void) { return 27; }
id CFTimeZoneCopySystem(void) { return ret(tz_s_system(0, 0)); }
void CFTimeZoneResetSystem(void) { }
id CFTimeZoneCopyDefault(void) { return ret(tz_s_default(0, 0)); }
void CFTimeZoneSetDefault(id tz) { tz_s_set_default(0, 0, tz); }
id CFTimeZoneCreateWithName(void *alloc, id name, unsigned char try_abbrev) {
    (void)alloc;
    id t = tz_s_named(0, 0, name);
    if (!t && try_abbrev) t = tz_s_abbr(0, 0, name);
    return ret(t);
}
id CFTimeZoneCreate(void *alloc, id name, id data) { (void)alloc; return ret(tz_s_named_data(0, 0, name, data)); }
id CFTimeZoneCreateWithTimeIntervalFromGMT(void *alloc, double ti) { (void)alloc; return ret(tz_s_seconds(0, 0, (i64)ti)); }
id CFTimeZoneGetName(id tz) { return objc_autorelease(ret(str(tz_name_c(tz)))); }
id CFTimeZoneGetData(id tz) { return tz && cx_is_subclass(object_getClass(tz), &NSTimeZone_c) ? tz_data(tz, 0) : 0; }
double CFTimeZoneGetSecondsFromGMT(id tz, double at) { return (double)fdn_tz_offset(tz_name_c(tz), at + CF_EPOCH, 0); }
id CFTimeZoneCopyAbbreviation(id tz, double at) { char b[32]; fdn_tz_abbreviation(tz_name_c(tz), at + CF_EPOCH, b, sizeof b); return ret(str(b)); }
unsigned char CFTimeZoneIsDaylightSavingTime(id tz, double at) { long d = 0; fdn_tz_offset(tz_name_c(tz), at + CF_EPOCH, &d); return d != 0; }
double CFTimeZoneGetDaylightSavingTimeOffset(id tz, double at) { long d = 0; fdn_tz_offset(tz_name_c(tz), at + CF_EPOCH, &d); return (double)d; }
double CFTimeZoneGetNextDaylightSavingTimeTransition(id tz, double at) {
    double t;
    return fdn_tz_next_transition(tz_name_c(tz), at + CF_EPOCH, &t) ? t - CF_EPOCH : 0.0;
}
id CFTimeZoneCopyLocalizedName(id tz, long style, id locale) {
    char b[160];
    return fdn_tz_name(tz_name_c(tz), (int)style, fdn_locale_ident_of(locale), cx_now(), b, sizeof b) > 0 ? ret(str(b)) : 0;
}
id CFTimeZoneCopyKnownNames(void) { return ret(tz_s_known(0, 0)); }
id CFTimeZoneCopyAbbreviationDictionary(void) { return ret(tz_s_abbr_dict(0, 0)); }
void CFTimeZoneSetAbbreviationDictionary(id d) { tz_s_set_abbr_dict(0, 0, d); }

u64 CFCalendarGetTypeID(void) { return 28; }
id CFCalendarCopyCurrent(void) { return ret(cal_s_current(0, 0)); }
id CFCalendarCreateWithIdentifier(void *alloc, id ident) { (void)alloc; return ident && fdn_cal_ident_ok(cx_cstr(ident)) ? cal_for(cx_cstr(ident), 0, 0) : 0; }
id CFCalendarGetIdentifier(id cal) { return cal ? objc_autorelease(ret(cal_ident(cal, 0))) : 0; }
id CFCalendarCopyLocale(id cal) { return ret(cal_locale(cal, 0)); }
void CFCalendarSetLocale(id cal, id loc) { cal_set_locale(cal, 0, loc); }
id CFCalendarCopyTimeZone(id cal) { return ret(cal_tz(cal, 0)); }
void CFCalendarSetTimeZone(id cal, id tz) { cal_set_tz(cal, 0, tz); }
long CFCalendarGetFirstWeekday(id cal) { return (long)cal_first_wd(cal, 0); }
void CFCalendarSetFirstWeekday(id cal, long v) { cal_set_first_wd(cal, 0, (u64)v); }
long CFCalendarGetMinimumDaysInFirstWeek(id cal) { return (long)cal_min_days(cal, 0); }
void CFCalendarSetMinimumDaysInFirstWeek(id cal, long v) { cal_set_min_days(cal, 0, (u64)v); }
static cf_range_t cfr(ns_range_t r) { cf_range_t c = {r.loc == 0x7fffffffffffffffLL ? -1 : r.loc, r.loc == 0x7fffffffffffffffLL ? 0 : r.len}; return c; }
cf_range_t CFCalendarGetMinimumRangeOfUnit(id cal, unsigned long unit) { return cfr(cal_min_range(cal, 0, unit)); }
cf_range_t CFCalendarGetMaximumRangeOfUnit(id cal, unsigned long unit) { return cfr(cal_max_range(cal, 0, unit)); }
cf_range_t CFCalendarGetRangeOfUnit(id cal, unsigned long small, unsigned long large, double at) {
    long l, n;
    cf_range_t r = {-1, 0};
    if (fdn_cal_range(calk(cal), small, large, at + CF_EPOCH, &l, &n)) { r.location = l; r.length = n; }
    return r;
}
long CFCalendarGetOrdinalityOfUnit(id cal, unsigned long small, unsigned long large, double at) {
    long r = fdn_cal_ordinality(calk(cal), small, large, at + CF_EPOCH);
    return r < 0 ? -1 : r;
}
unsigned char CFCalendarGetTimeRangeOfUnit(id cal, unsigned long unit, double at, double *startp, double *tip) {
    double s, l;
    if (!fdn_cal_interval(calk(cal), unit, at + CF_EPOCH, &s, &l)) return 0;
    if (startp) *startp = s - CF_EPOCH;
    if (tip) *tip = l;
    return 1;
}
/* "yMdHms" e os outros caracteres de componente das funções CFCalendar*Components / *AbsoluteTime */
static int64_t *desc_field(fdn_comps *c, char ch) {
    switch (ch) {
    case 'G': return &c->era;
    case 'y': return &c->year;
    case 'Q': return &c->quarter;
    case 'M': return &c->month;
    case 'd': return &c->day;
    case 'D': return &c->day_of_year;
    case 'H': return &c->hour;
    case 'm': return &c->minute;
    case 's': return &c->second;
    case 'S': return &c->nanosecond;
    case 'E': return &c->weekday;
    case 'F': return &c->weekday_ordinal;
    case 'w': return &c->week_of_year;
    case 'W': return &c->week_of_month;
    case 'Y': return &c->year_for_week;
    }
    return 0;
}
static uint64_t desc_unit(char ch) {
    switch (ch) {
    case 'G': return FDN_U_ERA; case 'y': return FDN_U_YEAR; case 'Q': return FDN_U_QUARTER; case 'M': return FDN_U_MONTH;
    case 'd': return FDN_U_DAY; case 'D': return FDN_U_DAY_OF_YEAR; case 'H': return FDN_U_HOUR; case 'm': return FDN_U_MINUTE;
    case 's': return FDN_U_SECOND; case 'S': return FDN_U_NANOSECOND; case 'E': return FDN_U_WEEKDAY;
    case 'F': return FDN_U_WEEKDAY_ORDINAL; case 'w': return FDN_U_WEEK_OF_YEAR; case 'W': return FDN_U_WEEK_OF_MONTH;
    case 'Y': return FDN_U_YEAR_FOR_WEEK; case 'l': return FDN_U_MONTH;
    }
    return 0;
}
unsigned char CFCalendarComposeAbsoluteTime(id cal, double *at, const char *desc, ...) {
    fdn_comps x;
    va_list ap;
    double t;
    if (!cal || !at || !desc) return 0;
    fdn_comps_clear(&x);
    va_start(ap, desc);
    for (const char *p = desc; *p; p++) {
        int v = va_arg(ap, int);
        if (*p == 'l') { x.leap_month = v ? 1 : 0; continue; }
        int64_t *f = desc_field(&x, *p);
        if (!f) { va_end(ap); return 0; }
        *f = v;
    }
    va_end(ap);
    if (!fdn_cal_date(calk(cal), &x, &t)) return 0;
    *at = t - CF_EPOCH;
    return 1;
}
unsigned char CFCalendarDecomposeAbsoluteTime(id cal, double at, const char *desc, ...) {
    fdn_comps x;
    va_list ap;
    uint64_t units = 0;
    if (!cal || !desc) return 0;
    for (const char *p = desc; *p; p++) units |= desc_unit(*p);
    fdn_cal_components(calk(cal), units, at + CF_EPOCH, &x);
    va_start(ap, desc);
    for (const char *p = desc; *p; p++) {
        int *out = va_arg(ap, int *);
        int64_t *f = *p == 'l' ? 0 : desc_field(&x, *p);
        if (*p == 'l') { if (out) *out = x.leap_month == 1; continue; }
        if (!f) { va_end(ap); return 0; }
        if (out) *out = (int)*f;
    }
    va_end(ap);
    return 1;
}
unsigned char CFCalendarAddComponents(id cal, double *at, unsigned long options, const char *desc, ...) {
    fdn_comps x;
    va_list ap;
    double t;
    if (!cal || !at || !desc) return 0;
    fdn_comps_clear(&x);
    va_start(ap, desc);
    for (const char *p = desc; *p; p++) {
        int v = va_arg(ap, int);
        int64_t *f = desc_field(&x, *p);
        if (!f) { va_end(ap); return 0; }
        *f = v;
    }
    va_end(ap);
    if (!fdn_cal_add(calk(cal), &x, *at + CF_EPOCH, options, &t)) return 0;
    *at = t - CF_EPOCH;
    return 1;
}
unsigned char CFCalendarGetComponentDifference(id cal, double start, double end, unsigned long options, const char *desc, ...) {
    fdn_comps x;
    va_list ap;
    uint64_t units = 0;
    (void)options;
    if (!cal || !desc) return 0;
    for (const char *p = desc; *p; p++) units |= desc_unit(*p);
    fdn_cal_difference(calk(cal), units, start + CF_EPOCH, end + CF_EPOCH, &x);
    va_start(ap, desc);
    for (const char *p = desc; *p; p++) {
        int *out = va_arg(ap, int *);
        int64_t *f = desc_field(&x, *p);
        if (!f) { va_end(ap); return 0; }
        if (out) *out = (int)*f;
    }
    va_end(ap);
    return 1;
}

/* CFGregorianDate (obsoleto, ainda importado): sempre o calendário gregoriano, no fuso dado (nil = GMT) */
typedef struct { int32_t year; int8_t month, day, hour, minute; double second; } cf_gdate_t;
typedef struct { int32_t years, months, days, hours, minutes; double seconds; } cf_gunits_t;
static fdn_cal greg(id tz) {
    fdn_cal k;
    fdn_cal_init(&k, "gregorian", "en_US_POSIX", tz ? tz_name_c(tz) : "GMT");
    return k;
}
cf_gdate_t CFAbsoluteTimeGetGregorianDate(double at, id tz) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    cf_gdate_t g;
    double t = at + CF_EPOCH;
    fdn_cal_components(&k, FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY | FDN_U_HOUR | FDN_U_MINUTE | FDN_U_SECOND | FDN_U_NANOSECOND | FDN_U_ERA, t, &x);
    g.year = (int32_t)(x.era == 0 ? 1 - x.year : x.year);
    g.month = (int8_t)x.month; g.day = (int8_t)x.day; g.hour = (int8_t)x.hour; g.minute = (int8_t)x.minute;
    g.second = (double)x.second + (double)x.nanosecond / 1e9;
    return g;
}
double CFGregorianDateGetAbsoluteTime(cf_gdate_t g, id tz) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    double t;
    fdn_comps_clear(&x);
    if (g.year <= 0) { x.era = 0; x.year = 1 - g.year; } else { x.era = 1; x.year = g.year; }
    x.month = g.month; x.day = g.day; x.hour = g.hour; x.minute = g.minute;
    x.second = (int64_t)floor(g.second);
    x.nanosecond = (int64_t)((g.second - floor(g.second)) * 1e9 + 0.5);
    if (!fdn_cal_date(&k, &x, &t)) return 0;
    return t - CF_EPOCH;
}
unsigned char CFGregorianDateIsValid(cf_gdate_t g, unsigned long flags) {
    /* kCFGregorianUnitsYears 1, Months 2, Days 4, Hours 8, Minutes 16, Seconds 32 */
    static const int dim[13] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if ((flags & 2) && (g.month < 1 || g.month > 12)) return 0;
    if (flags & 4) {
        if (g.day < 1 || g.month < 1 || g.month > 12 || g.day > dim[(int)g.month]) return 0;
        if (g.month == 2 && g.day == 29) {
            int y = g.year;
            if (!((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 0;
        }
    }
    if ((flags & 8) && (g.hour < 0 || g.hour > 23)) return 0;
    if ((flags & 16) && (g.minute < 0 || g.minute > 59)) return 0;
    if ((flags & 32) && (g.second < 0 || g.second >= 60)) return 0;
    return 1;
}
double CFAbsoluteTimeAddGregorianUnits(double at, id tz, cf_gunits_t u) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    double t;
    fdn_comps_clear(&x);
    x.year = u.years; x.month = u.months; x.day = u.days; x.hour = u.hours; x.minute = u.minutes;
    if (!fdn_cal_add(&k, &x, at + CF_EPOCH, 0, &t)) return at;
    return t - CF_EPOCH + u.seconds;
}
cf_gunits_t CFAbsoluteTimeGetDifferenceAsGregorianUnits(double at1, double at2, id tz, unsigned long flags) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    cf_gunits_t r = {0, 0, 0, 0, 0, 0};
    uint64_t units = 0;
    if (flags & 1) units |= FDN_U_YEAR;
    if (flags & 2) units |= FDN_U_MONTH;
    if (flags & 4) units |= FDN_U_DAY;
    if (flags & 8) units |= FDN_U_HOUR;
    if (flags & 16) units |= FDN_U_MINUTE;
    if (flags & 32) units |= FDN_U_SECOND | FDN_U_NANOSECOND;
    fdn_cal_difference(&k, units, at2 + CF_EPOCH, at1 + CF_EPOCH, &x);
    if (flags & 1) r.years = (int32_t)x.year;
    if (flags & 2) r.months = (int32_t)x.month;
    if (flags & 4) r.days = (int32_t)x.day;
    if (flags & 8) r.hours = (int32_t)x.hour;
    if (flags & 16) r.minutes = (int32_t)x.minute;
    if (flags & 32) r.seconds = (double)x.second + (double)x.nanosecond / 1e9;
    return r;
}
int32_t CFAbsoluteTimeGetDayOfWeek(double at, id tz) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    fdn_cal_components(&k, FDN_U_WEEKDAY, at + CF_EPOCH, &x);
    return (int32_t)(x.weekday == 1 ? 7 : x.weekday - 1);   /* CF: segunda = 1 ... domingo = 7 */
}
int32_t CFAbsoluteTimeGetDayOfYear(double at, id tz) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    fdn_cal_components(&k, FDN_U_DAY_OF_YEAR, at + CF_EPOCH, &x);
    return (int32_t)x.day_of_year;
}
int32_t CFAbsoluteTimeGetWeekOfYear(double at, id tz) {
    fdn_cal k = greg(tz);
    fdn_comps x;
    strncpy(k.ident, "iso8601", sizeof k.ident);   /* CF usa a semana ISO aqui */
    fdn_cal_components(&k, FDN_U_WEEK_OF_YEAR, at + CF_EPOCH, &x);
    return (int32_t)x.week_of_year;
}

/* ======================================================================== registro */

#define CM(cl, s, f) cx_add_cmethod(&cl##_c, s, (void *)(f))
#define IM(cl, s, f) cx_add_method(&cl##_c, s, (void *)(f))

void cx_fdn_i18n_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSCalendar") == &NSCalendar_c) return;

    cx_define_class(&NSLocale_c, &NSLocale_m, "NSLocale", ob, (u32)(sizeof(loc_t) - 8));
    CM(NSLocale, "currentLocale", loc_s_current); CM(NSLocale, "autoupdatingCurrentLocale", loc_s_auto);
    CM(NSLocale, "systemLocale", loc_s_system); CM(NSLocale, "localeWithLocaleIdentifier:", loc_s_with);
    CM(NSLocale, "availableLocaleIdentifiers", loc_s_available); CM(NSLocale, "ISOLanguageCodes", loc_s_iso_lang);
    CM(NSLocale, "ISOCountryCodes", loc_s_iso_region); CM(NSLocale, "ISOCurrencyCodes", loc_s_iso_cur);
    CM(NSLocale, "commonISOCurrencyCodes", loc_s_common_cur); CM(NSLocale, "preferredLanguages", loc_s_preferred);
    CM(NSLocale, "componentsFromLocaleIdentifier:", loc_s_components);
    CM(NSLocale, "localeIdentifierFromComponents:", loc_s_from_components);
    CM(NSLocale, "canonicalLocaleIdentifierFromString:", loc_s_canon_loc);
    CM(NSLocale, "canonicalLanguageIdentifierFromString:", loc_s_canon_lang);
    CM(NSLocale, "characterDirectionForLanguage:", loc_s_char_dir); CM(NSLocale, "lineDirectionForLanguage:", loc_s_line_dir);
    CM(NSLocale, "supportsSecureCoding", yes_m);
    IM(NSLocale, "init", loc_init); IM(NSLocale, "initWithLocaleIdentifier:", loc_init_ident);
    IM(NSLocale, "localeIdentifier", loc_ident); IM(NSLocale, "objectForKey:", loc_object);
    IM(NSLocale, "displayNameForKey:value:", loc_display_key);
    IM(NSLocale, "languageCode", loc_lang); IM(NSLocale, "languageIdentifier", loc_lang_ident);
    IM(NSLocale, "countryCode", loc_region); IM(NSLocale, "regionCode", loc_region);
    IM(NSLocale, "scriptCode", loc_script); IM(NSLocale, "variantCode", loc_variant);
    IM(NSLocale, "exemplarCharacterSet", loc_exemplar); IM(NSLocale, "calendarIdentifier", loc_cal_ident);
    IM(NSLocale, "collationIdentifier", loc_collation); IM(NSLocale, "collatorIdentifier", loc_collator);
    IM(NSLocale, "usesMetricSystem", loc_metric); IM(NSLocale, "decimalSeparator", loc_decimal);
    IM(NSLocale, "groupingSeparator", loc_grouping); IM(NSLocale, "currencySymbol", loc_cur_symbol);
    IM(NSLocale, "currencyCode", loc_cur_code); IM(NSLocale, "quotationBeginDelimiter", loc_qb);
    IM(NSLocale, "quotationEndDelimiter", loc_qe); IM(NSLocale, "alternateQuotationBeginDelimiter", loc_aqb);
    IM(NSLocale, "alternateQuotationEndDelimiter", loc_aqe);
    IM(NSLocale, "localizedStringForLocaleIdentifier:", loc_ls_ident); IM(NSLocale, "localizedStringForLanguageCode:", loc_ls_lang);
    IM(NSLocale, "localizedStringForCountryCode:", loc_ls_region); IM(NSLocale, "localizedStringForScriptCode:", loc_ls_script);
    IM(NSLocale, "localizedStringForVariantCode:", loc_ls_variant); IM(NSLocale, "localizedStringForCalendarIdentifier:", loc_ls_calendar);
    IM(NSLocale, "localizedStringForCollationIdentifier:", loc_ls_collation);
    IM(NSLocale, "localizedStringForCurrencyCode:", loc_ls_currency); IM(NSLocale, "localizedStringForCollatorIdentifier:", loc_ls_collator);
    IM(NSLocale, "isEqual:", loc_equal); IM(NSLocale, "hash", loc_hash); IM(NSLocale, "copy", loc_copy);
    IM(NSLocale, "copyWithZone:", loc_copy_zone); IM(NSLocale, "description", loc_desc);
    IM(NSLocale, "encodeWithCoder:", loc_encode); IM(NSLocale, "initWithCoder:", loc_init_coder);

    cx_define_class(&NSTimeZone_c, &NSTimeZone_m, "NSTimeZone", ob, (u32)(sizeof(tz_t) - 8));
    CM(NSTimeZone, "systemTimeZone", tz_s_system); CM(NSTimeZone, "localTimeZone", tz_s_local);
    CM(NSTimeZone, "defaultTimeZone", tz_s_default); CM(NSTimeZone, "setDefaultTimeZone:", tz_s_set_default);
    CM(NSTimeZone, "resetSystemTimeZone", tz_s_reset); CM(NSTimeZone, "timeZoneWithName:", tz_s_named);
    CM(NSTimeZone, "timeZoneWithName:data:", tz_s_named_data); CM(NSTimeZone, "timeZoneWithAbbreviation:", tz_s_abbr);
    CM(NSTimeZone, "timeZoneForSecondsFromGMT:", tz_s_seconds); CM(NSTimeZone, "knownTimeZoneNames", tz_s_known);
    CM(NSTimeZone, "abbreviationDictionary", tz_s_abbr_dict); CM(NSTimeZone, "setAbbreviationDictionary:", tz_s_set_abbr_dict);
    CM(NSTimeZone, "timeZoneDataVersion", tz_s_version); CM(NSTimeZone, "supportsSecureCoding", yes_m);
    IM(NSTimeZone, "initWithName:", tz_init_name); IM(NSTimeZone, "initWithName:data:", tz_init_name_data);
    IM(NSTimeZone, "name", tz_name); IM(NSTimeZone, "data", tz_data);
    IM(NSTimeZone, "secondsFromGMT", tz_seconds); IM(NSTimeZone, "secondsFromGMTForDate:", tz_seconds_for);
    IM(NSTimeZone, "abbreviation", tz_abbr); IM(NSTimeZone, "abbreviationForDate:", tz_abbr_for);
    IM(NSTimeZone, "isDaylightSavingTime", tz_dst); IM(NSTimeZone, "isDaylightSavingTimeForDate:", tz_dst_for);
    IM(NSTimeZone, "daylightSavingTimeOffset", tz_dst_off); IM(NSTimeZone, "daylightSavingTimeOffsetForDate:", tz_dst_off_for);
    IM(NSTimeZone, "nextDaylightSavingTimeTransition", tz_next);
    IM(NSTimeZone, "nextDaylightSavingTimeTransitionAfterDate:", tz_next_for);
    IM(NSTimeZone, "localizedName:locale:", tz_localized); IM(NSTimeZone, "isEqualToTimeZone:", tz_equal_tz);
    IM(NSTimeZone, "isEqual:", tz_equal_tz); IM(NSTimeZone, "hash", tz_hash); IM(NSTimeZone, "description", tz_desc);
    IM(NSTimeZone, "copy", loc_copy); IM(NSTimeZone, "copyWithZone:", loc_copy_zone);
    IM(NSTimeZone, "encodeWithCoder:", tz_encode); IM(NSTimeZone, "initWithCoder:", tz_init_coder);
    IM(NSTimeZone, "dealloc", tz_dealloc);

    cx_define_class(&NSCalendar_c, &NSCalendar_m, "NSCalendar", ob, (u32)(sizeof(cal_t) - 8));
    CM(NSCalendar, "currentCalendar", cal_s_current); CM(NSCalendar, "autoupdatingCurrentCalendar", cal_s_auto);
    CM(NSCalendar, "calendarWithIdentifier:", cal_s_with); CM(NSCalendar, "supportsSecureCoding", yes_m);
    IM(NSCalendar, "initWithCalendarIdentifier:", cal_init_ident); IM(NSCalendar, "calendarIdentifier", cal_ident);
    IM(NSCalendar, "locale", cal_locale); IM(NSCalendar, "setLocale:", cal_set_locale);
    IM(NSCalendar, "timeZone", cal_tz); IM(NSCalendar, "setTimeZone:", cal_set_tz);
    IM(NSCalendar, "firstWeekday", cal_first_wd); IM(NSCalendar, "setFirstWeekday:", cal_set_first_wd);
    IM(NSCalendar, "minimumDaysInFirstWeek", cal_min_days); IM(NSCalendar, "setMinimumDaysInFirstWeek:", cal_set_min_days);
    IM(NSCalendar, "eraSymbols", cal_eras); IM(NSCalendar, "longEraSymbols", cal_long_eras);
    IM(NSCalendar, "monthSymbols", cal_months); IM(NSCalendar, "shortMonthSymbols", cal_short_months);
    IM(NSCalendar, "veryShortMonthSymbols", cal_vs_months); IM(NSCalendar, "standaloneMonthSymbols", cal_sa_months);
    IM(NSCalendar, "shortStandaloneMonthSymbols", cal_sa_short_months);
    IM(NSCalendar, "veryShortStandaloneMonthSymbols", cal_sa_vs_months);
    IM(NSCalendar, "weekdaySymbols", cal_weekdays); IM(NSCalendar, "shortWeekdaySymbols", cal_short_weekdays);
    IM(NSCalendar, "veryShortWeekdaySymbols", cal_vs_weekdays); IM(NSCalendar, "standaloneWeekdaySymbols", cal_sa_weekdays);
    IM(NSCalendar, "shortStandaloneWeekdaySymbols", cal_sa_short_weekdays);
    IM(NSCalendar, "veryShortStandaloneWeekdaySymbols", cal_sa_vs_weekdays);
    IM(NSCalendar, "quarterSymbols", cal_quarters); IM(NSCalendar, "shortQuarterSymbols", cal_short_quarters);
    IM(NSCalendar, "standaloneQuarterSymbols", cal_sa_quarters);
    IM(NSCalendar, "shortStandaloneQuarterSymbols", cal_sa_short_quarters);
    IM(NSCalendar, "AMSymbol", cal_am); IM(NSCalendar, "PMSymbol", cal_pm);
    IM(NSCalendar, "minimumRangeOfUnit:", cal_min_range); IM(NSCalendar, "maximumRangeOfUnit:", cal_max_range);
    IM(NSCalendar, "rangeOfUnit:inUnit:forDate:", cal_range); IM(NSCalendar, "ordinalityOfUnit:inUnit:forDate:", cal_ordinality);
    IM(NSCalendar, "rangeOfUnit:startDate:interval:forDate:", cal_range_start);
    IM(NSCalendar, "dateFromComponents:", cal_date_from); IM(NSCalendar, "components:fromDate:", cal_components);
    IM(NSCalendar, "dateByAddingComponents:toDate:options:", cal_by_adding);
    IM(NSCalendar, "components:fromDate:toDate:options:", cal_diff);
    IM(NSCalendar, "components:fromDateComponents:toDateComponents:options:", cal_diff_comps);
    IM(NSCalendar, "getEra:year:month:day:fromDate:", cal_get_era);
    IM(NSCalendar, "getEra:yearForWeekOfYear:weekOfYear:weekday:fromDate:", cal_get_week);
    IM(NSCalendar, "getHour:minute:second:nanosecond:fromDate:", cal_get_time);
    IM(NSCalendar, "component:fromDate:", cal_component);
    IM(NSCalendar, "dateWithEra:year:month:day:hour:minute:second:nanosecond:", cal_date_era);
    IM(NSCalendar, "dateWithEra:yearForWeekOfYear:weekOfYear:weekday:hour:minute:second:nanosecond:", cal_date_week);
    IM(NSCalendar, "startOfDayForDate:", cal_start_of_day); IM(NSCalendar, "componentsInTimeZone:fromDate:", cal_components_tz);
    IM(NSCalendar, "compareDate:toDate:toUnitGranularity:", cal_compare);
    IM(NSCalendar, "isDate:equalToDate:toUnitGranularity:", cal_is_equal_gran);
    IM(NSCalendar, "isDate:inSameDayAsDate:", cal_same_day); IM(NSCalendar, "isDateInToday:", cal_today);
    IM(NSCalendar, "isDateInYesterday:", cal_yesterday); IM(NSCalendar, "isDateInTomorrow:", cal_tomorrow);
    IM(NSCalendar, "isDateInWeekend:", cal_weekend);
    IM(NSCalendar, "rangeOfWeekendStartDate:interval:containingDate:", cal_weekend_range);
    IM(NSCalendar, "nextWeekendStartDate:interval:options:afterDate:", cal_next_weekend);
    IM(NSCalendar, "dateByAddingUnit:value:toDate:options:", cal_by_adding_unit);
    IM(NSCalendar, "enumerateDatesStartingAfterDate:matchingComponents:options:usingBlock:", cal_enumerate);
    IM(NSCalendar, "nextDateAfterDate:matchingComponents:options:", cal_next_match);
    IM(NSCalendar, "nextDateAfterDate:matchingUnit:value:options:", cal_next_unit);
    IM(NSCalendar, "nextDateAfterDate:matchingHour:minute:second:options:", cal_next_hms);
    IM(NSCalendar, "dateBySettingUnit:value:ofDate:options:", cal_set_unit);
    IM(NSCalendar, "dateBySettingHour:minute:second:ofDate:options:", cal_set_hms);
    IM(NSCalendar, "date:matchesComponents:", cal_matches);
    IM(NSCalendar, "copy", cal_copy); IM(NSCalendar, "copyWithZone:", cal_copy_zone);
    IM(NSCalendar, "isEqual:", cal_equal); IM(NSCalendar, "hash", cal_hash); IM(NSCalendar, "description", cal_desc);
    IM(NSCalendar, "encodeWithCoder:", cal_encode); IM(NSCalendar, "initWithCoder:", cal_init_coder);
    IM(NSCalendar, "dealloc", cal_dealloc);

    cx_define_class(&NSDateComponents_c, &NSDateComponents_m, "NSDateComponents", ob, (u32)(sizeof(dc_t) - 8));
    CM(NSDateComponents, "supportsSecureCoding", yes_m);
    IM(NSDateComponents, "init", dc_init);
#define REG(get, set, field) IM(NSDateComponents, #get, dc_g_##field); IM(NSDateComponents, #set ":", dc_s_##field);
    REG(era, setEra, era) REG(year, setYear, year) REG(month, setMonth, month) REG(day, setDay, day)
    REG(hour, setHour, hour) REG(minute, setMinute, minute) REG(second, setSecond, second)
    REG(nanosecond, setNanosecond, nanosecond) REG(weekday, setWeekday, weekday)
    REG(weekdayOrdinal, setWeekdayOrdinal, weekday_ordinal) REG(quarter, setQuarter, quarter)
    REG(weekOfMonth, setWeekOfMonth, week_of_month) REG(weekOfYear, setWeekOfYear, week_of_year)
    REG(yearForWeekOfYear, setYearForWeekOfYear, year_for_week) REG(dayOfYear, setDayOfYear, day_of_year)
#undef REG
    IM(NSDateComponents, "week", dc_week); IM(NSDateComponents, "setWeek:", dc_set_week);
    IM(NSDateComponents, "isLeapMonth", dc_leap); IM(NSDateComponents, "setLeapMonth:", dc_set_leap);
    IM(NSDateComponents, "calendar", dc_cal); IM(NSDateComponents, "setCalendar:", dc_set_cal);
    IM(NSDateComponents, "timeZone", dc_tz); IM(NSDateComponents, "setTimeZone:", dc_set_tz);
    IM(NSDateComponents, "date", dc_date); IM(NSDateComponents, "isValidDate", dc_valid);
    IM(NSDateComponents, "isValidDateInCalendar:", dc_valid_in);
    IM(NSDateComponents, "valueForComponent:", dc_value_for); IM(NSDateComponents, "setValue:forComponent:", dc_set_value_for);
    IM(NSDateComponents, "copy", dc_copy); IM(NSDateComponents, "copyWithZone:", dc_copy_zone);
    IM(NSDateComponents, "isEqual:", dc_equal); IM(NSDateComponents, "hash", dc_hash);
    IM(NSDateComponents, "description", dc_desc);
    IM(NSDateComponents, "encodeWithCoder:", dc_encode); IM(NSDateComponents, "initWithCoder:", dc_init_coder);
    IM(NSDateComponents, "dealloc", dc_dealloc);
}

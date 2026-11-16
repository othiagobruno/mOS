/*
 * mOS libSystem — NSScanner completo, como no macOS (antes ficava em foundation_more.c, contava a posição em bytes
 * UTF-8, só pulava espaço ASCII e não tinha conjuntos):
 *  - scanLocation em unidades UTF-16; fora da string levanta NSRangeException;
 *  - charactersToBeSkipped (padrão whitespaceAndNewlineCharacterSet; nil = não pula nada) e caseSensitive
 *    (padrão NO), respeitados em todas as leituras; leitura que falha não move a posição;
 *  - locale (o separador decimal vem dele; +localizedScannerWithString: usa o locale atual);
 *  - scanString/UpToString/Characters/UpToCharacters, inteiros com saturação (Int, Integer, LongLong,
 *    UnsignedLongLong), hexadecimais, Float/Double, HexFloat/HexDouble (C99) e Decimal.
 * A lógica está em fdn_scanner_core.c (teste de host tools/fdn_scanner_test.c).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_scanner_core.h"
#include "fdn_decimal_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);
id cx_str_n(const char *s, u64 n);
int cx_charset_has(id set, u32 ch);

cx_class_t NSScanner_c __asm__("OBJC_CLASS_$_NSScanner");
cx_class_t NSScanner_m __asm__("OBJC_METACLASS_$_NSScanner");

typedef struct {
    cx_class_t *isa;
    id str, skip, locale;
    UChar *u;
    int32_t n, pos;
    int case_sensitive;
} sc_t;
#define S(o) ((sc_t *)(o))

static id cls(const char *n) { return (id)cx_class_named(n); }

static int skip_fn(void *ctx, uint32_t c) { return cx_charset_has((id)ctx, c); }
static int set_fn(void *ctx, uint32_t c) { return cx_charset_has((id)ctx, c); }

static UChar dec_sep(sc_t *s) {
    if (!s->locale) return '.';
    id d = MSG0(id, s->locale, "decimalSeparator");
    const char *p = d ? MSG0(const char *, d, "UTF8String") : 0;
    if (!p || !*p) return '.';
    int32_t n = 0;
    UChar *u = fdn_u16_dup(p, &n);
    UChar r = n == 1 ? u[0] : '.';
    free(u);
    return r;
}
static scn_t core(sc_t *s) {
    scn_t c = { s->u, s->n, s->pos, s->skip ? skip_fn : 0, s->skip, s->case_sensitive, dec_sep(s) };
    return c;
}
static id sub(sc_t *s, int32_t st, int32_t n) {
    char *u = fdn_utf8_dup(s->u + st, n);
    id r = objc_autorelease(cx_str_n(u, strlen(u)));
    free(u);
    return r;
}
static UChar *u16_of(id str, int32_t *n) {
    const char *p = str ? MSG0(const char *, str, "UTF8String") : 0;
    return fdn_u16_dup(p ? p : "", n);
}

/* ---------------------------------------------------------------- criação */
static id k_alloc(id k, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)k); }
static id i_init_str(id self, SEL c, id str) {
    (void)c;
    sc_t *s = S(self);
    s->str = MSG0(id, str ? str : cx_str(""), "copy");
    s->u = u16_of(s->str, &s->n);
    s->pos = 0;
    s->skip = objc_retain(MSG0(id, cls("NSCharacterSet"), "whitespaceAndNewlineCharacterSet"));
    s->case_sensitive = 0;
    s->locale = 0;
    return self;
}
static id k_with(id k, SEL c, id str) { return objc_autorelease(i_init_str(cx_alloc_instance((cx_class_t *)k), c, str)); }
static id k_localized(id k, SEL c, id str) {
    id r = k_with(k, c, str);
    S(r)->locale = objc_retain(MSG0(id, cls("NSLocale"), "currentLocale"));
    return r;
}
static id i_init(id self, SEL c) { return i_init_str(self, c, cx_str("")); }
static void i_dealloc(id self, SEL c) {
    (void)c;
    sc_t *s = S(self);
    objc_release(s->str); objc_release(s->skip); objc_release(s->locale);
    free(s->u);
    cx_kvo_forget(self);
    cx_object_free(self);
}
static id i_copy(id self, SEL c) {
    (void)c;
    sc_t *s = S(self);
    id r = i_init_str(cx_alloc_instance(*(cx_class_t **)self), 0, s->str);
    S(r)->pos = s->pos;
    objc_release(S(r)->skip); S(r)->skip = objc_retain(s->skip);
    S(r)->case_sensitive = s->case_sensitive;
    S(r)->locale = objc_retain(s->locale);
    return r;
}
static id i_copy_zone(id self, SEL c, void *z) { (void)z; return i_copy(self, c); }

/* ---------------------------------------------------------------- propriedades */
static id i_string(id self, SEL c) { (void)c; return S(self)->str; }
static u64 i_loc(id self, SEL c) { (void)c; return (u64)S(self)->pos; }
static void i_set_loc(id self, SEL c, u64 loc) {
    (void)c;
    if (loc > (u64)S(self)->n) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSRangeException"),
                   cx_str("*** -[NSScanner setScanLocation:]: Index out of bounds"), (id)0);
        MSG0(void, e, "raise");
        return;
    }
    S(self)->pos = (int32_t)loc;
}
static id i_skip(id self, SEL c) { (void)c; return S(self)->skip; }
static void i_set_skip(id self, SEL c, id set) {
    (void)c;
    id n = set ? MSG0(id, set, "copy") : 0;
    objc_release(S(self)->skip);
    S(self)->skip = n;
}
static BOOL i_case(id self, SEL c) { (void)c; return S(self)->case_sensitive ? YES : NO; }
static void i_set_case(id self, SEL c, BOOL v) { (void)c; S(self)->case_sensitive = v != 0; }
static id i_locale(id self, SEL c) { (void)c; return S(self)->locale; }
static void i_set_locale(id self, SEL c, id l) { (void)c; id o = S(self)->locale; S(self)->locale = objc_retain(l); objc_release(o); }
static BOOL i_at_end(id self, SEL c) { (void)c; scn_t k = core(S(self)); return scn_at_end(&k) ? YES : NO; }
/* posição depois do skip, sem mover (o Scanner.swift usa no scanCharacter) */
static u64 i_skipped(id self, SEL c) { (void)c; scn_t k = core(S(self)); return (u64)scn_skipped(&k); }

/* ---------------------------------------------------------------- texto */
static BOOL fin(id self, scn_t *k, int ok, int32_t st, int32_t n, id *out) {
    if (!ok) return NO;
    S(self)->pos = k->pos;
    if (out) *out = sub(S(self), st, n);
    return YES;
}
static BOOL i_scan_string(id self, SEL c, id str, id *out) {
    (void)c;
    scn_t k = core(S(self));
    int32_t m = 0, st = 0, n = 0;
    UChar *t = u16_of(str, &m);
    int ok = scn_string(&k, t, m, &st, &n);
    free(t);
    return fin(self, &k, ok, st, n, out);
}
static BOOL i_scan_upto_string(id self, SEL c, id str, id *out) {
    (void)c;
    scn_t k = core(S(self));
    int32_t m = 0, st = 0, n = 0;
    UChar *t = u16_of(str, &m);
    int ok = scn_upto_string(&k, t, m, &st, &n);
    free(t);
    return fin(self, &k, ok, st, n, out);
}
static BOOL i_scan_chars(id self, SEL c, id set, id *out) {
    (void)c;
    scn_t k = core(S(self));
    int32_t st = 0, n = 0;
    int ok = set && scn_chars(&k, set_fn, set, &st, &n);
    return fin(self, &k, ok, st, n, out);
}
static BOOL i_scan_upto_chars(id self, SEL c, id set, id *out) {
    (void)c;
    scn_t k = core(S(self));
    int32_t st = 0, n = 0;
    int ok = set && scn_upto_chars(&k, set_fn, set, &st, &n);
    return fin(self, &k, ok, st, n, out);
}

/* ---------------------------------------------------------------- números */
#define INTSCAN(name, T, lo, hi) \
    static BOOL name(id self, SEL c, T *out) { \
        (void)c; scn_t k = core(S(self)); long long v; \
        if (!scn_i64(&k, lo, hi, &v)) return NO; \
        S(self)->pos = k.pos; if (out) *out = (T)v; return YES; }
INTSCAN(i_scan_int, int, -2147483647LL - 1, 2147483647LL)
INTSCAN(i_scan_integer, long, (long long)(-9223372036854775807LL - 1), 9223372036854775807LL)
INTSCAN(i_scan_ll, long long, (long long)(-9223372036854775807LL - 1), 9223372036854775807LL)
static BOOL i_scan_ull(id self, SEL c, unsigned long long *out) {
    (void)c; scn_t k = core(S(self)); unsigned long long v;
    if (!scn_u64(&k, &v)) return NO;
    S(self)->pos = k.pos; if (out) *out = v; return YES;
}
static BOOL i_scan_hex_int(id self, SEL c, unsigned *out) {
    (void)c; scn_t k = core(S(self)); unsigned long long v;
    if (!scn_hex(&k, 0xFFFFFFFFull, &v)) return NO;
    S(self)->pos = k.pos; if (out) *out = (unsigned)v; return YES;
}
static BOOL i_scan_hex_ll(id self, SEL c, unsigned long long *out) {
    (void)c; scn_t k = core(S(self)); unsigned long long v;
    if (!scn_hex(&k, ~0ULL, &v)) return NO;
    S(self)->pos = k.pos; if (out) *out = v; return YES;
}
static BOOL i_scan_double(id self, SEL c, double *out) {
    (void)c; scn_t k = core(S(self)); double v;
    if (!scn_double(&k, &v)) return NO;
    S(self)->pos = k.pos; if (out) *out = v; return YES;
}
static BOOL i_scan_float(id self, SEL c, float *out) {
    double v;
    if (!i_scan_double(self, c, out ? &v : 0)) return NO;
    if (out) *out = (float)v;
    return YES;
}
static BOOL i_scan_hex_double(id self, SEL c, double *out) {
    (void)c; scn_t k = core(S(self)); double v;
    if (!scn_hex_double(&k, &v)) return NO;
    S(self)->pos = k.pos; if (out) *out = v; return YES;
}
static BOOL i_scan_hex_float(id self, SEL c, float *out) {
    double v;
    if (!i_scan_hex_double(self, c, out ? &v : 0)) return NO;
    if (out) *out = (float)v;
    return YES;
}
static BOOL i_scan_decimal(id self, SEL c, nsdec_t *out) {
    (void)c;
    scn_t k = core(S(self));
    char buf[256];
    int32_t from = 0;
    scn_ascii_prefix(&k, buf, sizeof buf, &from);
    nsdec_t d;
    int used = fdn_dec_parse(&d, buf, ".");
    if (used <= 0) return NO;
    S(self)->pos = from + used;
    if (out) *out = d;
    return YES;
}

static id i_desc(id self, SEL c) {
    (void)c;
    char b[96];
    snprintf(b, sizeof b, "<NSScanner: %p> location %d of %d", (void *)self, S(self)->pos, S(self)->n);
    return cx_str(b);
}

void cx_fdn_scanner_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSScanner") == &NSScanner_c) return;
    cx_define_class(&NSScanner_c, &NSScanner_m, "NSScanner", ob, (u32)(sizeof(sc_t) - 8));
    cx_class_t *k = &NSScanner_c;
#define M(s, f) cx_add_method(k, s, (void *)(f))
#define CM(s, f) cx_add_cmethod(k, s, (void *)(f))
    CM("alloc", k_alloc); CM("allocWithZone:", k_alloc);
    CM("scannerWithString:", k_with); CM("localizedScannerWithString:", k_localized);
    M("init", i_init); M("initWithString:", i_init_str); M("dealloc", i_dealloc);
    M("copy", i_copy); M("copyWithZone:", i_copy_zone); M("description", i_desc);
    M("string", i_string); M("scanLocation", i_loc); M("setScanLocation:", i_set_loc);
    M("charactersToBeSkipped", i_skip); M("setCharactersToBeSkipped:", i_set_skip);
    M("caseSensitive", i_case); M("setCaseSensitive:", i_set_case);
    M("locale", i_locale); M("setLocale:", i_set_locale);
    M("isAtEnd", i_at_end); M("_mos_skippedLocation", i_skipped);
    M("scanString:intoString:", i_scan_string); M("scanUpToString:intoString:", i_scan_upto_string);
    M("scanCharactersFromSet:intoString:", i_scan_chars); M("scanUpToCharactersFromSet:intoString:", i_scan_upto_chars);
    M("scanInt:", i_scan_int); M("scanInteger:", i_scan_integer); M("scanLongLong:", i_scan_ll);
    M("scanUnsignedLongLong:", i_scan_ull); M("scanHexInt:", i_scan_hex_int); M("scanHexLongLong:", i_scan_hex_ll);
    M("scanFloat:", i_scan_float); M("scanDouble:", i_scan_double);
    M("scanHexFloat:", i_scan_hex_float); M("scanHexDouble:", i_scan_hex_double); M("scanDecimal:", i_scan_decimal);
#undef M
#undef CM
}

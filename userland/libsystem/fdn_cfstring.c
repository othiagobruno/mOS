/*
 * mOS libSystem — funções de texto do CFString em UTF-16 sobre fdn_text_core (ICU), no lugar das versões
 * espalhadas (darwin.c, cf_rest.c, chrome.c, vscode_cf.c, postico_cf.c, posixmore.c, fdn_misc.c) que
 * comparavam byte a byte, mudavam a caixa só em ASCII e tinham Capitalize/Pad/Insert/Delete/LineBounds faltando.
 * Opções kCFCompare*: 1 sem caixa, 4 para trás, 8 âncora, 16 não literal, 32 localizado, 64 numérico,
 * 128 sem acentos, 256 sem largura, 512 ordem forçada (o CF é literal por padrão, o NSString não).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_text_core.h"

typedef long CFIndex;
typedef struct { CFIndex loc, len; } CFRange;
id cx_str_n(const char *s, u64 n);
int cx_charset_has(id set, u32 ch);

static id cls(const char *n) { return (id)cx_class_named(n); }
static UChar *U16(id s, int32_t *n) {
    const char *p = s ? MSG0(const char *, s, "UTF8String") : 0;
    return fdn_u16_dup(p ? p : "", n);
}
static id new_str(const UChar *u, int32_t n) {
    char *c = fdn_utf8_dup(u, n);
    id r = cx_str_n(c, strlen(c));
    free(c);
    return r;
}
static void set_text(id m, const UChar *u, int32_t n) {
    if (!m) return;
    id s = new_str(u, n);
    MSG(void, m, "setString:", s);
    MSG0(void, s, "release");
}
static const char *loc_id(id loc) {
    if (!loc) return 0;
    id i = MSG0(id, loc, "localeIdentifier");
    return i ? MSG0(const char *, i, "UTF8String") : "";
}
static const char *current_loc(void) { return loc_id(MSG0(id, cls("NSLocale"), "currentLocale")); }
static unsigned tx_opts(u64 cf) {
    unsigned o = (unsigned)(cf & (1 | 4 | 8 | 64 | 128 | 256 | 512));
    if (!(cf & 16)) o |= TX_LITERAL;
    return o;
}
static int clip(CFRange *r, int32_t n) {
    if (r->loc < 0 || r->len < 0 || r->loc > n || r->loc + r->len > n) return 0;
    return 1;
}

/* ---------------------------------------------------------------- comparação e busca */
long CFStringCompareWithOptionsAndLocale(id a, id b, CFRange r, u64 opts, id locale) {
    int32_t an, bn;
    UChar *x = U16(a, &an), *y = U16(b, &bn);
    long res = 0;
    if (clip(&r, an)) {
        const char *l = (opts & 32) ? (locale ? loc_id(locale) : current_loc()) : 0;
        res = tx_compare(x + r.loc, (int32_t)r.len, y, bn, tx_opts(opts), l);
    }
    free(x); free(y);
    return res;
}
long CFStringCompareWithOptions(id a, id b, CFRange r, u64 opts) { return CFStringCompareWithOptionsAndLocale(a, b, r, opts, 0); }
long CFStringCompare(id a, id b, u64 opts) {
    CFRange r = { 0, a ? (CFIndex)MSG0(u64, a, "length") : 0 };
    return CFStringCompareWithOptionsAndLocale(a, b, r, opts, 0);
}
u8 CFStringFindWithOptionsAndLocale(id s, id find, CFRange r, u64 opts, id locale, CFRange *res) {
    (void)locale;
    int32_t n, fn, loc = 0, len = 0;
    UChar *u = U16(s, &n), *f = U16(find, &fn);
    int ok = clip(&r, n) && fn > 0 && tx_search(u + r.loc, (int32_t)r.len, f, fn, tx_opts(opts), &loc, &len);
    free(u); free(f);
    if (ok && res) { res->loc = r.loc + loc; res->len = len; }
    return (u8)ok;
}
u8 CFStringFindWithOptions(id s, id find, CFRange r, u64 opts, CFRange *res) { return CFStringFindWithOptionsAndLocale(s, find, r, opts, 0, res); }
CFRange CFStringFind(id s, id find, u64 opts) {
    CFRange r = { 0, s ? (CFIndex)MSG0(u64, s, "length") : 0 }, out = { -1, 0 };   /* kCFNotFound */
    if (!CFStringFindWithOptions(s, find, r, opts, &out)) { out.loc = -1; out.len = 0; }
    return out;
}
u8 CFStringHasPrefix(id s, id p) { CFRange r; return CFStringFindWithOptions(s, p, (CFRange){ 0, s ? (CFIndex)MSG0(u64, s, "length") : 0 }, 8, &r); }
u8 CFStringHasSuffix(id s, id p) { CFRange r; return CFStringFindWithOptions(s, p, (CFRange){ 0, s ? (CFIndex)MSG0(u64, s, "length") : 0 }, 8 | 4, &r); }
static int set_has(void *ctx, uint32_t c) { return cx_charset_has((id)ctx, c); }
u8 CFStringFindCharacterFromSet(id s, id set, CFRange r, u64 opts, CFRange *res) {
    int32_t n, l = 0, m = 0;
    UChar *u = U16(s, &n);
    int ok = set && clip(&r, n) && tx_find_set(u, (int32_t)r.loc, (int32_t)(r.loc + r.len), (unsigned)(opts & (4 | 8)), set_has, set, &l, &m);
    free(u);
    if (ok && res) { res->loc = l; res->len = m; }
    return (u8)ok;
}

/* ---------------------------------------------------------------- caracteres e faixas */
u16 CFStringGetCharacterAtIndex(id s, CFIndex i) {
    return s && i >= 0 ? MSG(u16, s, "characterAtIndex:", (u64)i) : 0;
}
void CFStringGetCharacters(id s, CFRange r, u16 *out) {
    int32_t n;
    UChar *u = U16(s, &n);
    if (out && clip(&r, n)) memcpy(out, u + r.loc, (u64)r.len * 2);
    free(u);
}
/* o texto fica em UTF-8: não há ponteiro UTF-16 interno (NULL é a resposta válida do CF) */
const u16 *CFStringGetCharactersPtr(id s) { (void)s; return 0; }
CFRange CFStringGetRangeOfComposedCharactersAtIndex(id s, CFIndex i) {
    int32_t n, st = 0, len = 0;
    UChar *u = U16(s, &n);
    CFRange r = { i, 0 };
    if (i >= 0 && i < n) { tx_composed(u, n, (int32_t)i, &st, &len); r.loc = st; r.len = len; }
    free(u);
    return r;
}
/* kCFStringGraphemeCluster 1, ComposedCharacterCluster 2, CursorMovementCluster 3, BackwardDeletionCluster 4 */
CFRange CFStringGetRangeOfCharacterClusterAtIndex(id s, CFIndex i, int type) { (void)type; return CFStringGetRangeOfComposedCharactersAtIndex(s, i); }
static void bounds(id s, CFRange r, int para, CFIndex *b, CFIndex *e, CFIndex *ce) {
    int32_t n, a = 0, z = 0, c = 0;
    UChar *u = U16(s, &n);
    if (clip(&r, n)) {
        tx_line(u, n, (int32_t)r.loc, (int32_t)r.len, para, &a, &z, &c);
        if (b) *b = a;
        if (e) *e = z;
        if (ce) *ce = c;
    }
    free(u);
}
void CFStringGetLineBounds(id s, CFRange r, CFIndex *b, CFIndex *e, CFIndex *ce) { bounds(s, r, 0, b, e, ce); }
void CFStringGetParagraphBounds(id s, CFRange r, CFIndex *b, CFIndex *e, CFIndex *ce) { bounds(s, r, 1, b, e, ce); }
id CFStringCreateWithSubstring(void *alloc, id s, CFRange r) {
    (void)alloc;
    int32_t n;
    UChar *u = U16(s, &n);
    id o = clip(&r, n) ? new_str(u + r.loc, (int32_t)r.len) : 0;
    free(u);
    return o;
}

/* ---------------------------------------------------------------- CFMutableString */
static void case_of(id m, id locale, int kind) {
    int32_t n, k = 0;
    UChar *u = U16(m, &n), *r = tx_case(kind, u, n, locale ? loc_id(locale) : "", &k);
    if (r) set_text(m, r, k);
    free(u); free(r);
}
void CFStringUppercase(id m, id locale) { case_of(m, locale, 0); }
void CFStringLowercase(id m, id locale) { case_of(m, locale, 1); }
void CFStringCapitalize(id m, id locale) { case_of(m, locale, 2); }
/* kCFStringNormalizationFormD 0, KD 1, C 2, KC 3 */
void CFStringNormalize(id m, long form) {
    static const char *sel[] = { "decomposedStringWithCanonicalMapping", "decomposedStringWithCompatibilityMapping",
                                 "precomposedStringWithCanonicalMapping", "precomposedStringWithCompatibilityMapping" };
    if (!m || form < 0 || form > 3) return;
    id r = MSG0(id, m, sel[form]);
    if (r) MSG(void, m, "setString:", r);
}
void CFStringFold(id m, u64 opts, id locale) {
    (void)locale;
    int32_t n, k = 0;
    UChar *u = U16(m, &n), *r = tx_fold(u, n, (unsigned)(opts & (1 | 128 | 256)), &k);
    if (r) set_text(m, r, k);
    free(u); free(r);
}
u8 CFStringTransform(id m, CFRange *range, id transform, u8 reverse) {
    int32_t n, k = 0;
    UChar *u = U16(m, &n);
    CFRange r = range ? *range : (CFRange){ 0, n };
    if (!transform || !clip(&r, n)) { free(u); return 0; }
    UChar *t = tx_transform(u + r.loc, (int32_t)r.len, MSG0(const char *, transform, "UTF8String"), reverse != 0, &k);
    if (!t) { free(u); return 0; }
    UChar *o = malloc((u64)(n - r.len + k + 1) * 2);
    memcpy(o, u, (u64)r.loc * 2);
    memcpy(o + r.loc, t, (u64)k * 2);
    memcpy(o + r.loc + k, u + r.loc + r.len, (u64)(n - r.loc - r.len) * 2);
    set_text(m, o, n - (int32_t)r.len + k);
    if (range) range->len = k;
    free(u); free(t); free(o);
    return 1;
}
static void splice(id m, CFRange r, const UChar *p, int32_t pn) {
    int32_t n;
    UChar *u = U16(m, &n);
    if (!clip(&r, n)) { free(u); return; }
    UChar *o = malloc((u64)(n - r.len + pn + 1) * 2);
    memcpy(o, u, (u64)r.loc * 2);
    if (pn) memcpy(o + r.loc, p, (u64)pn * 2);
    memcpy(o + r.loc + pn, u + r.loc + r.len, (u64)(n - r.loc - r.len) * 2);
    set_text(m, o, n - (int32_t)r.len + pn);
    free(u); free(o);
}
void CFStringReplace(id m, CFRange r, id with) { int32_t pn; UChar *p = U16(with, &pn); splice(m, r, p, pn); free(p); }
void CFStringReplaceAll(id m, id with) { if (m) MSG(void, m, "setString:", with ? with : cx_str("")); }
void CFStringInsert(id m, CFIndex i, id s) { CFStringReplace(m, (CFRange){ i, 0 }, s); }
void CFStringDelete(id m, CFRange r) { splice(m, r, 0, 0); }
void CFStringAppendCharacters(id m, const u16 *chars, CFIndex n) {
    if (!m || !chars || n <= 0) return;
    id s = new_str(chars, (int32_t)n);
    MSG(void, m, "appendString:", s);
    MSG0(void, s, "release");
}
CFIndex CFStringFindAndReplace(id m, id find, id with, CFRange r, u64 opts) {
    int32_t n, fn, pn, on = 0, k = 0;
    UChar *u = U16(m, &n), *f = U16(find, &fn), *p = U16(with, &pn);
    if (clip(&r, n)) {
        UChar *o = tx_replace(u, n, (int32_t)r.loc, (int32_t)r.len, f, fn, p, pn, tx_opts(opts), &on, &k);
        if (k) set_text(m, o, on);
        free(o);
    }
    free(u); free(f); free(p);
    return k;
}
/* tira trimString repetido das duas pontas */
void CFStringTrim(id m, id trim) {
    int32_t n, tn;
    UChar *u = U16(m, &n), *t = U16(trim, &tn);
    int32_t a = 0, e = n;
    if (tn > 0) {
        while (e - a >= tn && !memcmp(u + a, t, (u64)tn * 2)) a += tn;
        while (e - a >= tn && !memcmp(u + e - tn, t, (u64)tn * 2)) e -= tn;
    }
    if (a || e != n) set_text(m, u + a, e - a);
    free(u); free(t);
}
void CFStringTrimWhitespace(id m) {
    id ws = MSG0(id, cls("NSCharacterSet"), "whitespaceAndNewlineCharacterSet");
    if (m) { id r = MSG(id, m, "stringByTrimmingCharactersInSet:", ws); if (r) MSG(void, m, "setString:", r); }
}
/* encurta para length ou completa repetindo padString a partir de indexIntoPad */
void CFStringPad(id m, id pad, CFIndex length, CFIndex idx) {
    int32_t n, pn;
    UChar *u = U16(m, &n);
    if (length < n) { set_text(m, u, (int32_t)length); free(u); return; }
    UChar *p = U16(pad, &pn);
    if (!pn || length == n) { free(u); free(p); return; }
    UChar *o = malloc((u64)(length + 1) * 2);
    memcpy(o, u, (u64)n * 2);
    for (CFIndex i = n, j = idx % pn; i < length; i++, j = (j + 1) % pn) o[i] = p[j];
    set_text(m, o, (int32_t)length);
    free(u); free(p); free(o);
}
id CFStringCreateByCombiningStrings(void *alloc, id array, id sep) {
    (void)alloc;
    id r = array ? MSG(id, array, "componentsJoinedByString:", sep ? sep : cx_str("")) : cx_str("");
    return r ? MSG0(id, r, "copy") : 0;
}
id CFStringCreateArrayBySeparatingStrings(void *alloc, id s, id sep) {
    (void)alloc;
    id r = s && sep ? MSG(id, s, "componentsSeparatedByString:", sep) : 0;
    return r ? MSG0(id, r, "copy") : 0;
}

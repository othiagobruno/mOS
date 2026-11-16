/*
 * mOS libSystem — NSString/NSMutableString em UTF-16 sobre a ICU, por cima do básico de foundation.c:
 *  - characterAtIndex: com pares substitutos (antes dava U+FFFD fora do plano básico), getCharacters:;
 *  - compare/caseInsensitive/localized/localizedStandardCompare com opções e collation do locale (ucol);
 *  - rangeOfString:options:range:locale: (caixa, acentos, largura, âncora, para trás, regex), contains e
 *    variantes localizadas, rangeOfCharacterFromSet:, sequências compostas (grafemas);
 *  - caixa pela ICU (ß -> SS, i turco), com e sem locale; normalização NFC/NFD/NFKC/NFKD; dobra;
 *    transformações (NSStringTransform* = ids da ICU);
 *  - linhas e parágrafos, enumerateSubstringsInRange:options:usingBlock: (linhas, parágrafos, grafemas,
 *    palavras, frases; para trás), enumerateLinesUsingBlock:;
 *  - substituições com opções e intervalo (com NSRegularExpressionSearch usa modelo $1 da ICU), padding,
 *    commonPrefix, caminhos (isAbsolutePath, til, symlinks, completePathIntoString…);
 *  - NSMutableString: replaceCharactersInRange:withString: (o primitivo), replaceOccurrences…, applyTransform….
 * A lógica de texto está em fdn_text_core.c (teste de host tools/fdn_text_test.c).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_text_core.h"

id objc_autorelease(id o);
id cx_str_n(const char *s, u64 n);
int cx_charset_has(id set, u32 ch);
id NSHomeDirectory(void);
char *realpath(const char *p, char *out);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
int stat(const char *p, void *st);

extern cx_class_t NSString_c __asm__("OBJC_CLASS_$_NSString");
extern cx_class_t NSMutableString_c __asm__("OBJC_CLASS_$_NSMutableString");

typedef struct { u64 loc, len; } rng_t;
#define NOTFOUND 0x7fffffffffffffffULL
static const rng_t NONE = { NOTFOUND, 0 };

static id cls(const char *n) { return (id)cx_class_named(n); }
static void *blk_inv(void *b) { return *(void **)((u8 *)b + 16); }

/* ---------------------------------------------------------------- UTF-16 de qualquer NSString */
static UChar *U16(id s, int32_t *n) {
    *n = 0;
    if (!s) return fdn_u16_dup("", n);
    if (cx_is_str(s)) return fdn_u16_dup(((nsstr_t *)s)->s ? ((nsstr_t *)s)->s : "", n);
    /* subclasse de app: os primitivos são length e characterAtIndex: */
    u64 len = MSG0(u64, s, "length");
    UChar *u = malloc((len + 1) * sizeof(UChar));
    for (u64 i = 0; i < len; i++) u[i] = MSG(u16, s, "characterAtIndex:", i);
    u[len] = 0;
    *n = (int32_t)len;
    return u;
}
static id NS(const UChar *u, int32_t n) {
    char *c = fdn_utf8_dup(u, n);
    id r = objc_autorelease(cx_str_n(c, strlen(c)));
    free(c);
    return r;
}
static void raise_range(const char *what, u64 loc, u64 len, u64 n) {
    char b[256];
    snprintf(b, sizeof b, "*** -[NSString %s]: Range {%llu, %llu} out of bounds; string length %llu", what,
             (unsigned long long)loc, (unsigned long long)len, (unsigned long long)n);
    id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSRangeException"), cx_str(b), (id)0);
    MSG0(void, e, "raise");
}
static int bad_range(const char *what, rng_t r, int32_t n) {
    if (r.loc > (u64)n || r.len > (u64)n - r.loc) { raise_range(what, r.loc, r.len, (u64)n); return 1; }
    return 0;
}
static const char *locale_id(id loc) {
    if (!loc) return 0;
    id l = loc;
    if (!MSG(BOOL, l, "respondsToSelector:", sel_registerName("localeIdentifier"))) l = MSG0(id, cls("NSLocale"), "currentLocale");
    id i = MSG0(id, l, "localeIdentifier");
    return i ? MSG0(const char *, i, "UTF8String") : "";
}
static const char *current_locale(void) { return locale_id(MSG0(id, cls("NSLocale"), "currentLocale")); }

/* ---------------------------------------------------------------- caracteres */
/* direto no UTF-8 do nosso layout, sem alocar (é chamado em laço pelos apps) */
static u16 s_char_at(id self, SEL c, u64 idx) {
    (void)c;
    nsstr_t *o = (nsstr_t *)self;
    UChar ch = 0;
    size_t total = 0;
    if (tx_utf8_unit_at(o->s ? o->s : "", o->n, idx, &ch, &total)) return ch;
    raise_range("characterAtIndex:", idx, 0, total);
    return 0;
}
static void s_get_chars_range(id self, SEL c, UChar *buf, rng_t r) {
    (void)c;
    int32_t n;
    UChar *u = U16(self, &n);
    if (!bad_range("getCharacters:range:", r, n)) memcpy(buf, u + r.loc, r.len * sizeof(UChar));
    free(u);
}
static void s_get_chars(id self, SEL c, UChar *buf) {
    (void)c;
    int32_t n;
    UChar *u = U16(self, &n);
    memcpy(buf, u, (u64)n * sizeof(UChar));
    free(u);
}

/* ---------------------------------------------------------------- comparação */
static i64 cmp(id self, id other, u64 opts, rng_t r, const char *loc, const char *what) {
    int32_t an, bn;
    UChar *a = U16(self, &an), *b = U16(other, &bn);
    i64 res = 0;
    if (!bad_range(what, r, an)) res = tx_compare(a + r.loc, (int32_t)r.len, b, bn, (unsigned)opts, loc);
    free(a); free(b);
    return res;
}
static u64 len_of(id s) { return MSG0(u64, s, "length"); }
static i64 s_cmp_locale(id self, SEL c, id o, u64 opts, rng_t r, id loc) {
    (void)c;
    return cmp(self, o, opts, r, loc ? locale_id(loc) : 0, "compare:options:range:locale:");
}
static i64 s_cmp_range(id self, SEL c, id o, u64 opts, rng_t r) { return s_cmp_locale(self, c, o, opts, r, 0); }
static i64 s_cmp_opts(id self, SEL c, id o, u64 opts) { rng_t r = { 0, len_of(self) }; return s_cmp_locale(self, c, o, opts, r, 0); }
static i64 s_cmp(id self, SEL c, id o) { return s_cmp_opts(self, c, o, 0); }
static i64 s_casecmp(id self, SEL c, id o) { return s_cmp_opts(self, c, o, TX_CASE); }
static i64 s_loc_cmp(id self, SEL c, id o) { (void)c; rng_t r = { 0, len_of(self) }; return cmp(self, o, 0, r, current_locale(), "localizedCompare:"); }
static i64 s_loc_casecmp(id self, SEL c, id o) { (void)c; rng_t r = { 0, len_of(self) }; return cmp(self, o, TX_CASE, r, current_locale(), "localizedCaseInsensitiveCompare:"); }
/* o Finder: sem caixa, números pelo valor, largura ignorada, ordem total */
static i64 s_std_cmp(id self, SEL c, id o) {
    (void)c; rng_t r = { 0, len_of(self) };
    return cmp(self, o, TX_CASE | TX_NUMERIC | TX_WIDTH | TX_FORCED, r, current_locale(), "localizedStandardCompare:");
}

/* ---------------------------------------------------------------- busca */
static rng_t search(id self, id needle, u64 opts, rng_t r, const char *what) {
    int32_t hn, nn, loc = 0, len = 0;
    UChar *h = U16(self, &hn), *nd = U16(needle, &nn);
    rng_t out = NONE;
    if (!bad_range(what, r, hn) && nn > 0 && tx_search(h + r.loc, (int32_t)r.len, nd, nn, (unsigned)opts, &loc, &len)) {
        out.loc = r.loc + (u64)loc; out.len = (u64)len;
    }
    free(h); free(nd);
    return out;
}
static rng_t s_range_loc(id self, SEL c, id p, u64 opts, rng_t r, id loc) { (void)c; (void)loc; return search(self, p, opts, r, "rangeOfString:options:range:locale:"); }
static rng_t s_range_r(id self, SEL c, id p, u64 opts, rng_t r) { (void)c; return search(self, p, opts, r, "rangeOfString:options:range:"); }
static rng_t s_range_o(id self, SEL c, id p, u64 opts) { (void)c; rng_t r = { 0, len_of(self) }; return search(self, p, opts, r, "rangeOfString:options:"); }
static rng_t s_range(id self, SEL c, id p) { (void)c; rng_t r = { 0, len_of(self) }; return search(self, p, 0, r, "rangeOfString:"); }
static BOOL s_contains(id self, SEL c, id p) { return s_range(self, c, p).loc != NOTFOUND; }
static BOOL s_loc_case_contains(id self, SEL c, id p) { return s_range_o(self, c, p, TX_CASE).loc != NOTFOUND; }
static rng_t s_std_range(id self, SEL c, id p) { return s_range_o(self, c, p, TX_CASE | TX_DIACRITIC); }
static BOOL s_std_contains(id self, SEL c, id p) { return s_std_range(self, c, p).loc != NOTFOUND; }

static int set_has(void *ctx, uint32_t c) { return cx_charset_has((id)ctx, c); }
static rng_t charset_search(id self, id set, u64 opts, rng_t r) {
    int32_t n, l = 0, m = 0;
    UChar *u = U16(self, &n);
    rng_t out = NONE;
    if (!bad_range("rangeOfCharacterFromSet:options:range:", r, n) && set &&
        tx_find_set(u, (int32_t)r.loc, (int32_t)(r.loc + r.len), (unsigned)opts, set_has, set, &l, &m)) {
        out.loc = (u64)l; out.len = (u64)m;
    }
    free(u);
    return out;
}
static rng_t s_cs_range_r(id self, SEL c, id set, u64 o, rng_t r) { (void)c; return charset_search(self, set, o, r); }
static rng_t s_cs_range_o(id self, SEL c, id set, u64 o) { (void)c; rng_t r = { 0, len_of(self) }; return charset_search(self, set, o, r); }
static rng_t s_cs_range(id self, SEL c, id set) { return s_cs_range_o(self, c, set, 0); }

static rng_t s_composed_at(id self, SEL c, u64 idx) {
    (void)c;
    int32_t n, st = 0, len = 0;
    UChar *u = U16(self, &n);
    if (idx >= (u64)n) { free(u); raise_range("rangeOfComposedCharacterSequenceAtIndex:", idx, 0, (u64)n); return NONE; }
    tx_composed(u, n, (int32_t)idx, &st, &len);
    free(u);
    rng_t r = { (u64)st, (u64)len };
    return r;
}
static rng_t s_composed_range(id self, SEL c, rng_t r) {
    (void)c;
    int32_t n, st = 0, len = 0, st2 = 0, len2 = 0;
    UChar *u = U16(self, &n);
    if (bad_range("rangeOfComposedCharacterSequencesForRange:", r, n)) { free(u); return NONE; }
    if (!r.len) {
        if (r.loc < (u64)n) { tx_composed(u, n, (int32_t)r.loc, &st, &len); r.loc = (u64)st; }
        free(u);
        r.len = 0;
        return r;
    }
    tx_composed(u, n, (int32_t)r.loc, &st, &len);
    tx_composed(u, n, (int32_t)(r.loc + r.len - 1), &st2, &len2);
    free(u);
    rng_t o = { (u64)st, (u64)(st2 + len2 - st) };
    return o;
}

/* ---------------------------------------------------------------- caixa, normalização, dobra, transformação */
static id cased(id self, int kind, const char *loc) {
    int32_t n, m = 0;
    UChar *u = U16(self, &n), *r = tx_case(kind, u, n, loc ? loc : "", &m);
    id o = r ? NS(r, m) : NS(u, n);
    free(u); free(r);
    return o;
}
static id s_upper(id self, SEL c) { (void)c; return cased(self, 0, ""); }
static id s_lower(id self, SEL c) { (void)c; return cased(self, 1, ""); }
static id s_cap(id self, SEL c) { (void)c; return cased(self, 2, ""); }
static id s_loc_upper(id self, SEL c) { (void)c; return cased(self, 0, current_locale()); }
static id s_loc_lower(id self, SEL c) { (void)c; return cased(self, 1, current_locale()); }
static id s_loc_cap(id self, SEL c) { (void)c; return cased(self, 2, current_locale()); }
static id s_upper_l(id self, SEL c, id l) { (void)c; return cased(self, 0, locale_id(l)); }
static id s_lower_l(id self, SEL c, id l) { (void)c; return cased(self, 1, locale_id(l)); }
static id s_cap_l(id self, SEL c, id l) { (void)c; return cased(self, 2, locale_id(l)); }

static id normalized(id self, int which) {
    const fdn_icu_t *I = fdn_icu();
    int32_t n;
    UChar *u = U16(self, &n);
    if (!I || !I->ok) { id o = NS(u, n); free(u); return o; }
    UErrorCode e = 0;
    const void *nm = which == 0 ? I->unorm2_getNFCInstance(&e) : which == 1 ? I->unorm2_getNFDInstance(&e)
                   : which == 2 ? I->unorm2_getNFKCInstance(&e) : I->unorm2_getNFKDInstance(&e);
    int32_t cap = n * 4 + 16;
    UChar *o = malloc((u64)cap * sizeof(UChar));
    e = 0;
    int32_t m = nm ? I->unorm2_normalize(nm, u, n, o, cap, &e) : -1;
    if (e == 15 && m > 0) {          /* U_BUFFER_OVERFLOW_ERROR */
        free(o); cap = m + 1; o = malloc((u64)cap * sizeof(UChar)); e = 0;
        m = I->unorm2_normalize(nm, u, n, o, cap, &e);
    }
    id r = (e > 0 || m < 0) ? NS(u, n) : NS(o, m);
    free(u); free(o);
    return r;
}
static id s_nfc(id self, SEL c) { (void)c; return normalized(self, 0); }
static id s_nfd(id self, SEL c) { (void)c; return normalized(self, 1); }
static id s_nfkc(id self, SEL c) { (void)c; return normalized(self, 2); }
static id s_nfkd(id self, SEL c) { (void)c; return normalized(self, 3); }

static id s_fold(id self, SEL c, u64 opts, id loc) {
    (void)c; (void)loc;
    int32_t n, m = 0;
    UChar *u = U16(self, &n), *r = tx_fold(u, n, (unsigned)opts, &m);
    id o = r ? NS(r, m) : NS(u, n);
    free(u); free(r);
    return o;
}
static id s_transform(id self, SEL c, id name, BOOL reverse) {
    (void)c;
    if (!name) return 0;
    int32_t n, m = 0;
    UChar *u = U16(self, &n), *r = tx_transform(u, n, MSG0(const char *, name, "UTF8String"), reverse != 0, &m);
    id o = r ? NS(r, m) : 0;     /* transformação desconhecida: nil, como no macOS */
    free(u); free(r);
    return o;
}

/* ---------------------------------------------------------------- linhas e parágrafos */
static void line_parts(id self, rng_t r, int para, u64 *s, u64 *e, u64 *ce, const char *what) {
    int32_t n, a = 0, b = 0, cc = 0;
    UChar *u = U16(self, &n);
    if (!bad_range(what, r, n)) {
        tx_line(u, n, (int32_t)r.loc, (int32_t)r.len, para, &a, &b, &cc);
        if (s) *s = (u64)a;
        if (e) *e = (u64)b;
        if (ce) *ce = (u64)cc;
    }
    free(u);
}
static void s_get_line(id self, SEL c, u64 *s, u64 *e, u64 *ce, rng_t r) { (void)c; line_parts(self, r, 0, s, e, ce, "getLineStart:end:contentsEnd:forRange:"); }
static void s_get_para(id self, SEL c, u64 *s, u64 *e, u64 *ce, rng_t r) { (void)c; line_parts(self, r, 1, s, e, ce, "getParagraphStart:end:contentsEnd:forRange:"); }
static rng_t s_line_range(id self, SEL c, rng_t r) {
    (void)c; u64 s = 0, e = 0;
    line_parts(self, r, 0, &s, &e, 0, "lineRangeForRange:");
    rng_t o = { s, e - s };
    return o;
}
static rng_t s_para_range(id self, SEL c, rng_t r) {
    (void)c; u64 s = 0, e = 0;
    line_parts(self, r, 1, &s, &e, 0, "paragraphRangeForRange:");
    rng_t o = { s, e - s };
    return o;
}

/* ---------------------------------------------------------------- enumeração */
/* NSStringEnumeration*: 0 linhas, 1 parágrafos, 2 grafemas, 3 palavras, 4 frases, 5 posições do cursor,
 * 6 grupos de apagar; 1<<8 para trás, 1<<9 sem substring, 1<<10 localizado */
static void enumerate(id self, rng_t r, u64 opts, void *blk, int lines_api) {
    int32_t n;
    UChar *u = U16(self, &n);
    if (bad_range("enumerateSubstringsInRange:options:usingBlock:", r, n)) { free(u); return; }
    int cnt = 0;
    const char *loc = (opts & (1 << 10)) ? current_locale() : "";
    int32_t *v0 = tx_enum(u, n, (int32_t)r.loc, (int32_t)(r.loc + r.len), (int)(opts & 0xFF), loc, &cnt);
    typedef void (*sub_fn)(void *, id, rng_t, rng_t, BOOL *);
    typedef void (*line_fn)(void *, id, BOOL *);
    int back = (opts & (1 << 8)) != 0, nosub = (opts & (1 << 9)) != 0;
    BOOL stop = NO;
    for (int k = 0; k < cnt && !stop; k++) {
        int idx = back ? cnt - 1 - k : k;
        int32_t *v = v0 + idx * 4;
        id sub = nosub ? 0 : NS(u + v[0], v[1] - v[0]);
        if (lines_api) ((line_fn)blk_inv(blk))(blk, sub, &stop);
        else {
            rng_t sr = { (u64)v[0], (u64)(v[1] - v[0]) }, er = { (u64)v[2], (u64)(v[3] - v[2]) };
            ((sub_fn)blk_inv(blk))(blk, sub, sr, er, &stop);
        }
    }
    free(v0);
    free(u);
}
static void s_enum_subs(id self, SEL c, rng_t r, u64 opts, void *blk) { (void)c; if (blk) enumerate(self, r, opts, blk, 0); }
static void s_enum_lines(id self, SEL c, void *blk) { (void)c; rng_t r = { 0, len_of(self) }; if (blk) enumerate(self, r, 0, blk, 1); }

/* ---------------------------------------------------------------- substituição */
/* resultado em UTF-16 de trocar as ocorrências; count = quantas */
static UChar *replaced(id self, id target, id repl, u64 opts, rng_t r, int32_t *on, u64 *count, const char *what) {
    int32_t n, tn, rn, k = 0;
    UChar *u = U16(self, &n), *t = U16(target, &tn), *p = U16(repl, &rn), *o;
    if (bad_range(what, r, n)) { *on = n; *count = 0; free(t); free(p); return u; }
    o = tx_replace(u, n, (int32_t)r.loc, (int32_t)r.len, t, tn, p, rn, (unsigned)opts, on, &k);
    *count = (u64)k;
    free(u); free(t); free(p);
    return o;
}
static id s_replacing_opts(id self, SEL c, id t, id rp, u64 opts, rng_t r) {
    (void)c;
    int32_t m; u64 k;
    UChar *o = replaced(self, t, rp, opts, r, &m, &k, "stringByReplacingOccurrencesOfString:withString:options:range:");
    id s = NS(o, m);
    free(o);
    return s;
}
static id s_replacing_chars(id self, SEL c, rng_t r, id rp) {
    (void)c;
    int32_t n, rn;
    UChar *u = U16(self, &n), *p = U16(rp, &rn);
    id s = 0;
    if (!bad_range("stringByReplacingCharactersInRange:withString:", r, n)) {
        UChar *o = malloc((u64)(n - (int32_t)r.len + rn + 1) * sizeof(UChar));
        memcpy(o, u, r.loc * sizeof(UChar));
        memcpy(o + r.loc, p, (u64)rn * sizeof(UChar));
        memcpy(o + r.loc + rn, u + r.loc + r.len, (n - r.loc - r.len) * sizeof(UChar));
        s = NS(o, n - (int32_t)r.len + rn);
        free(o);
    }
    free(u); free(p);
    return s;
}
static id s_common_prefix(id self, SEL c, id o, u64 opts) {
    (void)c;
    int32_t an, bn;
    UChar *a = U16(self, &an), *b = U16(o, &bn);
    int32_t i = 0, j = 0;
    /* grafema a grafema, comparado com as opções (caixa, literal) */
    while (i < an && j < bn) {
        int32_t s1, l1, s2, l2;
        tx_composed(a, an, i, &s1, &l1);
        tx_composed(b, bn, j, &s2, &l2);
        if (tx_compare(a + i, l1 - (i - s1), b + j, l2 - (j - s2), (unsigned)opts, 0) != 0) break;
        i = s1 + l1; j = s2 + l2;
    }
    id r = NS(a, i);
    free(a); free(b);
    return r;
}

/* ---------------------------------------------------------------- caminhos */
static BOOL s_is_abs(id self, SEL c) { (void)c; const char *p = MSG0(const char *, self, "UTF8String"); return p && (p[0] == '/' || p[0] == '~'); }
static id s_abbrev_tilde(id self, SEL c) {
    (void)c;
    const char *p = MSG0(const char *, self, "UTF8String"), *h = MSG0(const char *, NSHomeDirectory(), "UTF8String");
    u64 hl = h ? strlen(h) : 0;
    if (p && hl > 1 && !strncmp(p, h, hl) && (p[hl] == 0 || p[hl] == '/')) {
        char *b = malloc(strlen(p) - hl + 2);
        b[0] = '~'; strcpy(b + 1, p + hl);
        id r = objc_autorelease(cx_str(b));
        free(b);
        return r;
    }
    return self;
}
static id s_resolve_links(id self, SEL c) {
    (void)c;
    id ex = MSG0(id, self, "stringByExpandingTildeInPath");
    const char *p = MSG0(const char *, ex, "UTF8String");
    if (!p || p[0] != '/') return MSG0(id, ex, "stringByStandardizingPath");
    char out[1024];
    if (!realpath(p, out)) return MSG0(id, ex, "stringByStandardizingPath");
    /* como no macOS: /private/{var,tmp,etc} volta ao nome curto */
    const char *r = out;
    if (!strncmp(out, "/private/var/", 13) || !strcmp(out, "/private/var") || !strncmp(out, "/private/tmp/", 13) ||
        !strcmp(out, "/private/tmp") || !strncmp(out, "/private/etc/", 13) || !strcmp(out, "/private/etc")) r = out + 8;
    return objc_autorelease(cx_str(r));
}
static id s_strings_appending(id self, SEL c, id paths) {
    (void)c;
    id m = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = paths ? MSG0(u64, paths, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id p = MSG(id, paths, "objectAtIndex:", i);
        id j = MSG(id, self, "stringByAppendingPathComponent:", p);
        MSG(void, m, "addObject:", j);
    }
    return m;
}
static BOOL s_get_fsrep(id self, SEL c, char *buf, u64 max) {
    (void)c;
    const char *p = MSG0(const char *, self, "fileSystemRepresentation");
    if (!p || !buf || strlen(p) + 1 > max) return NO;
    memcpy(buf, p, strlen(p) + 1);
    return YES;
}
/* completamento de caminho do diálogo de abrir: arquivos que começam com o último componente */
static u64 s_complete(id self, SEL c, id *out, BOOL cs, id *matches, id types) {
    (void)c;
    id ex = MSG0(id, self, "stringByExpandingTildeInPath");
    const char *full = MSG0(const char *, ex, "UTF8String");
    if (!full) return 0;
    const char *slash = strrchr(full, '/');
    char dir[1024];
    const char *pre;
    if (slash) { u64 dl = (u64)(slash - full); if (dl >= sizeof dir) return 0; memcpy(dir, full, dl); dir[dl] = 0; if (!dl) strcpy(dir, "/"); pre = slash + 1; }
    else { strcpy(dir, "."); pre = full; }
    void *d = opendir(dir);
    if (!d) return 0;
    id list = MSG0(id, cls("NSMutableArray"), "array");
    u64 pl = strlen(pre);
    const u8 *e;
    while ((e = readdir(d))) {
        const char *name = (const char *)e + 21;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (cs ? strncmp(name, pre, pl) : strncasecmp(name, pre, pl)) continue;
        if (types && MSG0(u64, types, "count")) {
            const char *dot = strrchr(name, '.');
            if (!dot || !MSG(BOOL, types, "containsObject:", cx_str(dot + 1))) continue;
        }
        char b[2048];
        snprintf(b, sizeof b, "%s%s%s", slash ? dir : "", slash && strcmp(dir, "/") ? "/" : "", name);
        MSG(void, list, "addObject:", cx_str(b));
    }
    closedir(d);
    u64 n = MSG0(u64, list, "count");
    if (matches) *matches = MSG(id, list, "sortedArrayUsingSelector:", sel_registerName("compare:"));
    if (out && n) {
        /* o maior prefixo comum entre as escolhas */
        const char *f = MSG0(const char *, MSG(id, list, "objectAtIndex:", (u64)0), "UTF8String");
        u64 k = strlen(f);
        for (u64 i = 1; i < n; i++) {
            const char *o = MSG0(const char *, MSG(id, list, "objectAtIndex:", i), "UTF8String");
            u64 j = 0;
            while (j < k && o[j] && (cs ? o[j] == f[j] : tolower((u8)o[j]) == tolower((u8)f[j]))) j++;
            k = j;
        }
        *out = objc_autorelease(cx_str_n(f, k));
    }
    return n;
}

/* ---------------------------------------------------------------- NSMutableString */
static void set_u16(id self, const UChar *u, int32_t n) { MSG(void, self, "setString:", NS(u, n)); }
static void m_replace_chars(id self, SEL c, rng_t r, id s) {
    id o = s_replacing_chars(self, c, r, s);
    if (o) MSG(void, self, "setString:", o);
}
static u64 m_replace_occ(id self, SEL c, id t, id rp, u64 opts, rng_t r) {
    (void)c;
    int32_t m; u64 k;
    UChar *o = replaced(self, t, rp, opts, r, &m, &k, "replaceOccurrencesOfString:withString:options:range:");
    if (k) set_u16(self, o, m);
    free(o);
    return k;
}
static BOOL m_apply_transform(id self, SEL c, id name, BOOL reverse, rng_t r, rng_t *upd) {
    (void)c;
    int32_t n, m = 0;
    UChar *u = U16(self, &n);
    if (bad_range("applyTransform:reverse:range:updatedRange:", r, n) || !name) { free(u); return NO; }
    UChar *t = tx_transform(u + r.loc, (int32_t)r.len, MSG0(const char *, name, "UTF8String"), reverse != 0, &m);
    if (!t) { free(u); return NO; }
    UChar *o = malloc((u64)(n - (int32_t)r.len + m + 1) * sizeof(UChar));
    memcpy(o, u, r.loc * sizeof(UChar));
    memcpy(o + r.loc, t, (u64)m * sizeof(UChar));
    memcpy(o + r.loc + m, u + r.loc + r.len, (n - r.loc - r.len) * sizeof(UChar));
    set_u16(self, o, n - (int32_t)r.len + m);
    if (upd) { upd->loc = r.loc; upd->len = (u64)m; }
    free(u); free(t); free(o);
    return YES;
}

void cx_fdn_string_init(void) {
    cx_class_t *k = &NSString_c, *mk = &NSMutableString_c;
#define M(s, f) cx_add_method(k, s, (void *)(f))
    M("characterAtIndex:", s_char_at); M("getCharacters:range:", s_get_chars_range); M("getCharacters:", s_get_chars);
    M("compare:", s_cmp); M("compare:options:", s_cmp_opts); M("compare:options:range:", s_cmp_range);
    M("compare:options:range:locale:", s_cmp_locale); M("caseInsensitiveCompare:", s_casecmp);
    M("localizedCompare:", s_loc_cmp); M("localizedCaseInsensitiveCompare:", s_loc_casecmp);
    M("localizedStandardCompare:", s_std_cmp);
    M("rangeOfString:", s_range); M("rangeOfString:options:", s_range_o); M("rangeOfString:options:range:", s_range_r);
    M("rangeOfString:options:range:locale:", s_range_loc); M("containsString:", s_contains);
    M("localizedCaseInsensitiveContainsString:", s_loc_case_contains);
    M("localizedStandardContainsString:", s_std_contains); M("localizedStandardRangeOfString:", s_std_range);
    M("rangeOfCharacterFromSet:", s_cs_range); M("rangeOfCharacterFromSet:options:", s_cs_range_o);
    M("rangeOfCharacterFromSet:options:range:", s_cs_range_r);
    M("rangeOfComposedCharacterSequenceAtIndex:", s_composed_at);
    M("rangeOfComposedCharacterSequencesForRange:", s_composed_range);
    M("uppercaseString", s_upper); M("lowercaseString", s_lower); M("capitalizedString", s_cap);
    M("localizedUppercaseString", s_loc_upper); M("localizedLowercaseString", s_loc_lower);
    M("localizedCapitalizedString", s_loc_cap);
    M("uppercaseStringWithLocale:", s_upper_l); M("lowercaseStringWithLocale:", s_lower_l);
    M("capitalizedStringWithLocale:", s_cap_l);
    M("precomposedStringWithCanonicalMapping", s_nfc); M("decomposedStringWithCanonicalMapping", s_nfd);
    M("precomposedStringWithCompatibilityMapping", s_nfkc); M("decomposedStringWithCompatibilityMapping", s_nfkd);
    M("stringByFoldingWithOptions:locale:", s_fold); M("stringByApplyingTransform:reverse:", s_transform);
    M("getLineStart:end:contentsEnd:forRange:", s_get_line); M("lineRangeForRange:", s_line_range);
    M("getParagraphStart:end:contentsEnd:forRange:", s_get_para); M("paragraphRangeForRange:", s_para_range);
    M("enumerateSubstringsInRange:options:usingBlock:", s_enum_subs); M("enumerateLinesUsingBlock:", s_enum_lines);
    M("stringByReplacingOccurrencesOfString:withString:options:range:", s_replacing_opts);
    M("stringByReplacingCharactersInRange:withString:", s_replacing_chars);
    M("commonPrefixWithString:options:", s_common_prefix);
    M("isAbsolutePath", s_is_abs); M("stringByAbbreviatingWithTildeInPath", s_abbrev_tilde);
    M("stringByResolvingSymlinksInPath", s_resolve_links); M("stringsByAppendingPaths:", s_strings_appending);
    M("getFileSystemRepresentation:maxLength:", s_get_fsrep);
    M("completePathIntoString:caseSensitive:matchesIntoArray:filterTypes:", s_complete);
#undef M
    cx_add_method(mk, "replaceCharactersInRange:withString:", (void *)m_replace_chars);
    cx_add_method(mk, "replaceOccurrencesOfString:withString:options:range:", (void *)m_replace_occ);
    cx_add_method(mk, "applyTransform:reverse:range:updatedRange:", (void *)m_apply_transform);
}

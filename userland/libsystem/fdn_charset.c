/*
 * mOS libSystem — NSCharacterSet/NSMutableCharacterSet e CFCharacterSet (toll-free: o mesmo objeto), sobre as
 * faixas Unicode de fdn_charset_core.c. Os conjuntos predefinidos vêm das propriedades Unicode da ICU, com as
 * definições da Foundation (letterCharacterSet = L + M, whitespace = Zs + tab etc.). Antes havia quatro
 * implementações parciais (bitmap só do BMP, strings soltas, ASCII) e CFCharacterSet era outra classe.
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_charset_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);

typedef struct { cx_class_t *isa; crs_t s; u64 flags; } cs_t;
#define C(o) ((cs_t *)(o))
extern cx_class_t NSCharacterSet_c __asm__("OBJC_CLASS_$_NSCharacterSet");
cx_class_t mcs_c __asm__("OBJC_CLASS_$_NSMutableCharacterSet"), mcs_m __asm__("OBJC_METACLASS_$_NSMutableCharacterSet");
static id cls(const char *n) { return (id)cx_class_named(n); }
static id ar(id o) { return objc_autorelease(o); }
typedef struct { u64 loc, len; } rng_t;

/* ---------------------------------------------------------------- texto -> escalares */
static u32 utf8_next(const u8 **pp) {
    const u8 *p = *pp;
    u32 c = *p++;
    if (c >= 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        c = (c & 7) << 18 | (u32)(p[0] & 0x3F) << 12 | (u32)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); p += 3;
    } else if (c >= 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
        c = (c & 15) << 12 | (u32)(p[0] & 0x3F) << 6 | (p[1] & 0x3F); p += 2;
    } else if (c >= 0xC0 && (p[0] & 0xC0) == 0x80) {
        c = (c & 31) << 6 | (p[0] & 0x3F); p += 1;
    }
    *pp = p;
    return c;
}
static void add_string(crs_t *s, id str, int remove) {
    const u8 *p = (const u8 *)(str ? cx_cstr(str) : "");
    while (*p) { u32 c = utf8_next(&p); if (remove) crs_remove(s, c, c); else crs_add(s, c, c); }
}

/* ---------------------------------------------------------------- conjuntos predefinidos */
enum { P_CONTROL = 1, P_WHITESPACE, P_WHITESPACE_NL, P_DIGIT, P_LETTER, P_LOWER, P_UPPER, P_NONBASE, P_DECOMPOSABLE,
       P_ALNUM, P_PUNCT, P_ILLEGAL, P_CAPITALIZED, P_SYMBOL, P_NEWLINE,
       P_URL_USER, P_URL_PASSWORD, P_URL_HOST, P_URL_PATH, P_URL_QUERY, P_URL_FRAGMENT, P_COUNT };
static const char *const pattern[P_COUNT] = {
    [P_CONTROL] = "[[:Cc:][:Cf:]]",
    [P_WHITESPACE] = "[[:Zs:]\\u0009]",
    [P_WHITESPACE_NL] = "[[:Z:]\\u0009-\\u000D\\u0085]",
    [P_DIGIT] = "[:Nd:]",
    [P_LETTER] = "[[:L:][:M:]]",
    [P_LOWER] = "[:Ll:]",
    [P_UPPER] = "[[:Lu:][:Lt:]]",
    [P_NONBASE] = "[:M:]",
    [P_DECOMPOSABLE] = "[:dt=can:]",
    [P_ALNUM] = "[[:L:][:M:][:N:]]",
    [P_PUNCT] = "[:P:]",
    [P_ILLEGAL] = "[:Cn:]",
    [P_CAPITALIZED] = "[:Lt:]",
    [P_SYMBOL] = "[:S:]",
    [P_NEWLINE] = "[\\u000A-\\u000D\\u0085\\u2028\\u2029]",
};
/* RFC 3986, como os URL*AllowedCharacterSet da Foundation */
static const char *const url_chars[P_COUNT] = {
    [P_URL_USER] = "!$&'()*+,-.;=_~",
    [P_URL_PASSWORD] = "!$&'()*+,-.;=_~",
    [P_URL_HOST] = "!$&'()*+,-.:;=[]_~",
    [P_URL_PATH] = "!$&'()*+,-./:=@_~",
    [P_URL_QUERY] = "!$&'()*+,-./:;=?@_~",
    [P_URL_FRAGMENT] = "!$&'()*+,-./:;=?@_~",
};
/* sem a ICU (não deveria acontecer no mOS): o mínimo em ASCII/Latin-1 */
static void fallback(crs_t *s, int k) {
    switch (k) {
    case P_CONTROL: crs_add(s, 0, 0x1F); crs_add(s, 0x7F, 0x9F); break;
    case P_WHITESPACE: crs_add(s, ' ', ' '); crs_add(s, 9, 9); crs_add(s, 0xA0, 0xA0); break;
    case P_WHITESPACE_NL: crs_add(s, ' ', ' '); crs_add(s, 9, 13); crs_add(s, 0x85, 0x85); crs_add(s, 0xA0, 0xA0); break;
    case P_DIGIT: crs_add(s, '0', '9'); break;
    case P_ALNUM: crs_add(s, '0', '9'); /* fallthrough */
    case P_LETTER: crs_add(s, 'A', 'Z'); crs_add(s, 'a', 'z'); crs_add(s, 0xC0, 0xD6); crs_add(s, 0xD8, 0xF6); crs_add(s, 0xF8, 0x24F); break;
    case P_LOWER: crs_add(s, 'a', 'z'); break;
    case P_UPPER: case P_CAPITALIZED: crs_add(s, 'A', 'Z'); break;
    case P_PUNCT: add_string(s, cx_str("!\"#%&'()*,-./:;?@[\\]_{}"), 0); break;
    case P_SYMBOL: add_string(s, cx_str("$+<=>^`|~"), 0); break;
    case P_NEWLINE: crs_add(s, 10, 13); crs_add(s, 0x85, 0x85); crs_add(s, 0x2028, 0x2029); break;
    default: break;
    }
}
static void build(crs_t *s, int k) {
    if (url_chars[k]) {
        crs_add(s, 'A', 'Z'); crs_add(s, 'a', 'z'); crs_add(s, '0', '9');
        for (const char *p = url_chars[k]; *p; p++) crs_add(s, (u8)*p, (u8)*p);
        return;
    }
    if (!crs_from_icu_pattern(s, pattern[k])) fallback(s, k);
}
static id mk(cx_class_t *k) { return cx_alloc_instance(k); }
static int is_mutable_class(id k) { return cx_is_subclass((cx_class_t *)k, &mcs_c); }
static id predefined(id k, int which) {
    static id cache[P_COUNT];
    static volatile int lock;
    if (!cache[which]) {
        id s = mk(&NSCharacterSet_c);
        build(&C(s)->s, which);
        while (__sync_lock_test_and_set(&lock, 1)) {}
        if (!cache[which]) cache[which] = s; else objc_release(s);
        __sync_lock_release(&lock);
    }
    if (!k || !is_mutable_class(k)) return cache[which];
    id m = mk((cx_class_t *)k);
    crs_copy(&C(m)->s, &C(cache[which])->s);
    return ar(m);
}
#define PRE(name, k) static id name(id cls_, SEL c) { (void)c; return predefined(cls_, k); }
PRE(p_control, P_CONTROL) PRE(p_ws, P_WHITESPACE) PRE(p_wsnl, P_WHITESPACE_NL) PRE(p_digit, P_DIGIT)
PRE(p_letter, P_LETTER) PRE(p_lower, P_LOWER) PRE(p_upper, P_UPPER) PRE(p_nonbase, P_NONBASE)
PRE(p_decomp, P_DECOMPOSABLE) PRE(p_alnum, P_ALNUM) PRE(p_punct, P_PUNCT) PRE(p_illegal, P_ILLEGAL)
PRE(p_cap, P_CAPITALIZED) PRE(p_symbol, P_SYMBOL) PRE(p_newline, P_NEWLINE)
PRE(p_url_user, P_URL_USER) PRE(p_url_pass, P_URL_PASSWORD) PRE(p_url_host, P_URL_HOST)
PRE(p_url_path, P_URL_PATH) PRE(p_url_query, P_URL_QUERY) PRE(p_url_frag, P_URL_FRAGMENT)

/* ---------------------------------------------------------------- outra implementação de NSCharacterSet */
IMP cx_lookup(cx_class_t *cls, SEL sel);
static BOOL i_member(id s, SEL c, u16 ch);
/* nosso layout de verdade: a classe não trocou o characterIsMember: (subclasse de app guarda outra coisa) */
static int ours(id o) {
    if (!o || !cx_is_subclass(*(cx_class_t **)o, &NSCharacterSet_c)) return 0;
    return cx_lookup(*(cx_class_t **)o, sel_registerName("characterIsMember:")) == (IMP)i_member;
}
/* um conjunto que não é nosso (subclasse de um app): lê pelo bitmapRepresentation */
static void read_any(crs_t *d, id o) {
    if (!o) return;
    if (ours(o)) { crs_copy(d, &C(o)->s); return; }
    id b = MSG0(id, o, "bitmapRepresentation");
    if (b) crs_from_bitmap(d, MSG0(const u8 *, b, "bytes"), (u32)MSG0(u64, b, "length"));
}

/* pertinência de um escalar em qualquer NSCharacterSet (o NSScanner usa): direto nas faixas quando é nosso,
 * senão pela subclasse (o CharacterSet do Swift vem assim): -characterIsMember: no plano básico, que é o método
 * que toda subclasse precisa ter, e -longCharacterIsMember: fora dele */
int cx_charset_has(id set, u32 ch) {
    if (!set) return 0;
    if (ours(set)) return crs_has(&C(set)->s, ch);
    if (ch <= 0xFFFF) return MSG(BOOL, set, "characterIsMember:", (u16)ch) != 0;
    return MSG(BOOL, set, "longCharacterIsMember:", ch) != 0;
}

/* ---------------------------------------------------------------- construtores */
static id k_alloc(id k, SEL c) { (void)c; return mk((cx_class_t *)k); }
static id k_range(id k, SEL c, rng_t r) {
    (void)c;
    id s = mk((cx_class_t *)k);
    if (r.len) crs_add(&C(s)->s, (u32)r.loc, (u32)(r.loc + r.len - 1));
    return ar(s);
}
static id k_string(id k, SEL c, id str) { (void)c; id s = mk((cx_class_t *)k); add_string(&C(s)->s, str, 0); return ar(s); }
static id k_bitmap(id k, SEL c, id data) {
    (void)c;
    id s = mk((cx_class_t *)k);
    if (data) crs_from_bitmap(&C(s)->s, MSG0(const u8 *, data, "bytes"), (u32)MSG0(u64, data, "length"));
    return ar(s);
}
static id k_file(id k, SEL c, id path) {
    id d = path ? MSG(id, cls("NSData"), "dataWithContentsOfFile:", path) : 0;
    return d ? k_bitmap(k, c, d) : 0;
}
static id i_init(id s, SEL c) { (void)c; return s; }
static void i_dealloc(id s, SEL c) { (void)c; crs_free(&C(s)->s); cx_kvo_forget(s); cx_object_free(s); }

/* ---------------------------------------------------------------- consulta */
static BOOL i_member(id s, SEL c, u16 ch) { (void)c; return crs_has(&C(s)->s, ch); }
static BOOL i_long_member(id s, SEL c, u32 ch) { (void)c; return crs_has(&C(s)->s, ch); }
static BOOL i_has_plane(id s, SEL c, u8 p) { (void)c; return crs_has_plane(&C(s)->s, p); }
static id i_bitmap(id s, SEL c) {
    (void)c;
    u32 n;
    u8 *b = crs_bitmap(&C(s)->s, &n);
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, (u64)n);
    free(b);
    return d;
}
static id i_inverted(id s, SEL c) {
    (void)c;
    id r = mk(&NSCharacterSet_c);
    crs_copy(&C(r)->s, &C(s)->s);
    crs_invert(&C(r)->s);
    return ar(r);
}
static BOOL i_superset(id s, SEL c, id o) {
    (void)c;
    crs_t t = {0};
    read_any(&t, o);
    BOOL r = crs_superset(&C(s)->s, &t);
    crs_free(&t);
    return r;
}
static BOOL i_equal(id s, SEL c, id o) {
    (void)c;
    if (o == s) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)&NSCharacterSet_c)) return NO;
    crs_t t = {0};
    read_any(&t, o);
    BOOL r = crs_equal(&C(s)->s, &t);
    crs_free(&t);
    return r;
}
static u64 i_hash(id s, SEL c) { (void)c; crs_t *x = &C(s)->s; return x->n ? (u64)x->n * 31 + x->r[0].lo : 0; }
static id copy_as(id s, cx_class_t *k) { id r = mk(k); crs_copy(&C(r)->s, &C(s)->s); return r; }
static id i_copy(id s, SEL c) { (void)c; return *(cx_class_t **)s == &NSCharacterSet_c ? objc_retain(s) : copy_as(s, &NSCharacterSet_c); }
static id i_copy_zone(id s, SEL c, void *z) { (void)z; return i_copy(s, c); }
static id i_mcopy(id s, SEL c) { (void)c; return copy_as(s, &mcs_c); }
static id i_mcopy_zone(id s, SEL c, void *z) { (void)z; return i_mcopy(s, c); }
static id i_desc(id s, SEL c) {
    (void)c;
    char b[96];
    snprintf(b, sizeof b, "<%s %p>", *(cx_class_t **)s == &mcs_c ? "NSMutableCharacterSet" : "NSCharacterSet", (void *)s);
    return ar(cx_str(b));
}
/* NSCoding: o bitmap em NSBitmap (formato do bitmapRepresentation) */
static void i_encode(id s, SEL c, id coder) { MSG(void, coder, "encodeObject:forKey:", i_bitmap(s, c), cx_str("NSBitmap")); }
static id i_decode(id s, SEL c, id coder) {
    (void)c;
    id d = MSG(id, coder, "decodeObjectForKey:", cx_str("NSBitmap"));
    if (d) crs_from_bitmap(&C(s)->s, MSG0(const u8 *, d, "bytes"), (u32)MSG0(u64, d, "length"));
    return s;
}
static BOOL yes(id s, SEL c) { (void)s; (void)c; return YES; }

/* ---------------------------------------------------------------- mutável */
static void m_add_range(id s, SEL c, rng_t r) { (void)c; if (r.len) crs_add(&C(s)->s, (u32)r.loc, (u32)(r.loc + r.len - 1)); }
static void m_remove_range(id s, SEL c, rng_t r) { (void)c; if (r.len) crs_remove(&C(s)->s, (u32)r.loc, (u32)(r.loc + r.len - 1)); }
static void m_add_string(id s, SEL c, id str) { (void)c; add_string(&C(s)->s, str, 0); }
static void m_remove_string(id s, SEL c, id str) { (void)c; add_string(&C(s)->s, str, 1); }
static void m_union(id s, SEL c, id o) { (void)c; crs_t t = {0}; read_any(&t, o); crs_union(&C(s)->s, &t); crs_free(&t); }
static void m_intersect(id s, SEL c, id o) { (void)c; crs_t t = {0}; read_any(&t, o); crs_intersect(&C(s)->s, &t); crs_free(&t); }
static void m_invert(id s, SEL c) { (void)c; crs_invert(&C(s)->s); }

/* ---------------------------------------------------------------- CFCharacterSet (o mesmo objeto) */
typedef struct { long loc, len; } cf_range_t;
u64 CFCharacterSetGetTypeID(void) { return 45; }
id CFCharacterSetGetPredefined(long which) { return which >= 1 && which <= P_NEWLINE ? predefined(0, (int)which) : 0; }
static id own(id o) { return o; }
id CFCharacterSetCreateWithCharactersInRange(void *a, cf_range_t r) { (void)a; rng_t x = {(u64)r.loc, (u64)r.len}; return objc_retain(k_range((id)&NSCharacterSet_c, 0, x)); }
id CFCharacterSetCreateWithCharactersInString(void *a, id str) { (void)a; return objc_retain(k_string((id)&NSCharacterSet_c, 0, str)); }
id CFCharacterSetCreateWithBitmapRepresentation(void *a, id data) { (void)a; return objc_retain(k_bitmap((id)&NSCharacterSet_c, 0, data)); }
id CFCharacterSetCreateInvertedSet(void *a, id s) {
    (void)a;
    id r = mk(&NSCharacterSet_c);
    read_any(&C(r)->s, s);
    crs_invert(&C(r)->s);
    return own(r);
}
unsigned char CFCharacterSetIsCharacterMember(id s, u16 ch) { return s && (ours(s) ? crs_has(&C(s)->s, ch) : MSG(BOOL, s, "characterIsMember:", ch)); }
unsigned char CFCharacterSetIsLongCharacterMember(id s, u32 ch) { return s && (ours(s) ? crs_has(&C(s)->s, ch) : MSG(BOOL, s, "longCharacterIsMember:", ch)); }
unsigned char CFCharacterSetIsSupersetOfSet(id s, id o) { return s && i_superset(s, 0, o); }
unsigned char CFCharacterSetHasMemberInPlane(id s, long p) { return s && p >= 0 && crs_has_plane(&C(s)->s, (u32)p); }
id CFCharacterSetCreateBitmapRepresentation(void *a, id s) { (void)a; return s ? objc_retain(i_bitmap(s, 0)) : 0; }
id CFCharacterSetCreateMutable(void *a) { (void)a; return own(mk(&mcs_c)); }
id CFCharacterSetCreateCopy(void *a, id s) { (void)a; id r = mk(&NSCharacterSet_c); read_any(&C(r)->s, s); return own(r); }
id CFCharacterSetCreateMutableCopy(void *a, id s) { (void)a; id r = mk(&mcs_c); read_any(&C(r)->s, s); return own(r); }
void CFCharacterSetAddCharactersInRange(id s, cf_range_t r) { if (s && r.len > 0) crs_add(&C(s)->s, (u32)r.loc, (u32)(r.loc + r.len - 1)); }
void CFCharacterSetRemoveCharactersInRange(id s, cf_range_t r) { if (s && r.len > 0) crs_remove(&C(s)->s, (u32)r.loc, (u32)(r.loc + r.len - 1)); }
void CFCharacterSetAddCharactersInString(id s, id str) { if (s) add_string(&C(s)->s, str, 0); }
void CFCharacterSetRemoveCharactersInString(id s, id str) { if (s) add_string(&C(s)->s, str, 1); }
void CFCharacterSetUnion(id s, id o) { if (s) m_union(s, 0, o); }
void CFCharacterSetIntersect(id s, id o) { if (s) m_intersect(s, 0, o); }
void CFCharacterSetInvert(id s) { if (s) crs_invert(&C(s)->s); }

void cx_fdn_charset_init(void) {
    cx_class_t *k = &NSCharacterSet_c, *m = &mcs_c;
    if (cx_class_named("NSMutableCharacterSet") == &mcs_c) return;
    cx_define_class(&mcs_c, &mcs_m, "NSMutableCharacterSet", k, 0);
#define M(c, s, f) cx_add_method(c, s, (void *)(f))
#define CM(c, s, f) cx_add_cmethod(c, s, (void *)(f))
    CM(k, "alloc", k_alloc); CM(k, "allocWithZone:", k_alloc); CM(k, "supportsSecureCoding", yes);
    CM(k, "controlCharacterSet", p_control); CM(k, "whitespaceCharacterSet", p_ws);
    CM(k, "whitespaceAndNewlineCharacterSet", p_wsnl); CM(k, "decimalDigitCharacterSet", p_digit);
    CM(k, "letterCharacterSet", p_letter); CM(k, "lowercaseLetterCharacterSet", p_lower);
    CM(k, "uppercaseLetterCharacterSet", p_upper); CM(k, "nonBaseCharacterSet", p_nonbase);
    CM(k, "alphanumericCharacterSet", p_alnum); CM(k, "decomposableCharacterSet", p_decomp);
    CM(k, "illegalCharacterSet", p_illegal); CM(k, "punctuationCharacterSet", p_punct);
    CM(k, "capitalizedLetterCharacterSet", p_cap); CM(k, "symbolCharacterSet", p_symbol);
    CM(k, "newlineCharacterSet", p_newline);
    CM(k, "URLUserAllowedCharacterSet", p_url_user); CM(k, "URLPasswordAllowedCharacterSet", p_url_pass);
    CM(k, "URLHostAllowedCharacterSet", p_url_host); CM(k, "URLPathAllowedCharacterSet", p_url_path);
    CM(k, "URLQueryAllowedCharacterSet", p_url_query); CM(k, "URLFragmentAllowedCharacterSet", p_url_frag);
    CM(k, "characterSetWithRange:", k_range); CM(k, "characterSetWithCharactersInString:", k_string);
    CM(k, "characterSetWithBitmapRepresentation:", k_bitmap); CM(k, "characterSetWithContentsOfFile:", k_file);
    M(k, "init", i_init); M(k, "dealloc", i_dealloc);
    M(k, "characterIsMember:", i_member); M(k, "longCharacterIsMember:", i_long_member); M(k, "hasMemberInPlane:", i_has_plane);
    M(k, "bitmapRepresentation", i_bitmap); M(k, "invertedSet", i_inverted); M(k, "isSupersetOfSet:", i_superset);
    M(k, "isEqual:", i_equal); M(k, "hash", i_hash); M(k, "description", i_desc);
    M(k, "copy", i_copy); M(k, "copyWithZone:", i_copy_zone); M(k, "mutableCopy", i_mcopy); M(k, "mutableCopyWithZone:", i_mcopy_zone);
    M(k, "encodeWithCoder:", i_encode); M(k, "initWithCoder:", i_decode);
    M(m, "addCharactersInRange:", m_add_range); M(m, "removeCharactersInRange:", m_remove_range);
    M(m, "addCharactersInString:", m_add_string); M(m, "removeCharactersInString:", m_remove_string);
    M(m, "formUnionWithCharacterSet:", m_union); M(m, "formIntersectionWithCharacterSet:", m_intersect); M(m, "invert", m_invert);
#undef M
#undef CM
}

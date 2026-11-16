/*
 * mOS libSystem — NSAttributedString e NSMutableAttributedString como no macOS: atributos por trecho
 * (fdn_astr_core.c) e um cluster de primitivas. As primitivas são -string, -attributesAtIndex:effectiveRange:,
 * -replaceCharactersInRange:withString: e -setAttributes:range:; todo o resto (attribute:atIndex:…,
 * longestEffectiveRange, enumerate…, add/remove, append/insert/delete, setAttributedString:, substring,
 * igualdade, cópia, mutableString) só chama as primitivas por mensagem. Assim uma subclasse (o NSTextStorage do
 * AppKit) sobrescreve só as quatro e herda o resto. Antes: um dicionário para a string toda (appkit.c) e
 * setAttributes:range: vazio (foundation_more.c). Os métodos de desenho (drawAtPoint:, size…) ficam no AppKit.
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_astr_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);
id cx_str_n(const char *s, u64 n);

cx_class_t NSAttributedString_c __asm__("OBJC_CLASS_$_NSAttributedString");
cx_class_t NSAttributedString_m __asm__("OBJC_METACLASS_$_NSAttributedString");
cx_class_t NSMutableAttributedString_c __asm__("OBJC_CLASS_$_NSMutableAttributedString");
cx_class_t NSMutableAttributedString_m __asm__("OBJC_METACLASS_$_NSMutableAttributedString");
extern cx_class_t NSMutableString_c __asm__("OBJC_CLASS_$_NSMutableString");
static cx_class_t ms_c, ms_m;   /* a mutableString viva */

typedef struct { u64 loc, len; } rng_t;
typedef struct { cx_class_t *isa; id str; runs_t runs; id proxy; int editing; } as_t;
typedef struct { cx_class_t *isa; u64 flags; char *s; u64 n; id owner; } msp_t;   /* nsstr_t + dono */
#define AS(o) ((as_t *)(o))
#define NOTFOUND 0x7fffffffffffffffULL

static id cls(const char *n) { return (id)cx_class_named(n); }
static void *blk_inv(void *b) { return *(void **)((u8 *)b + 16); }
static void *rc_retain(void *v) { return objc_retain((id)v); }
static void rc_release(void *v) { objc_release((id)v); }
static int rc_equal(void *a, void *b) { return MSG(BOOL, (id)a, "isEqual:", (id)b) != 0; }
static const runs_cb CB = { rc_retain, rc_release, rc_equal };

static id empty_dict(void) { return MSG0(id, cls("NSDictionary"), "dictionary"); }
static u64 length_of(id self) { id s = MSG0(id, self, "string"); return s ? MSG0(u64, s, "length") : 0; }
static void raise_range(const char *sel, u64 loc, u64 len, u64 n) {
    char b[200];
    snprintf(b, sizeof b, "*** -[NSAttributedString %s]: Range or index {%llu, %llu} out of bounds; string length %llu", sel,
             (unsigned long long)loc, (unsigned long long)len, (unsigned long long)n);
    id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSRangeException"), cx_str(b), (id)0);
    MSG0(void, e, "raise");
}
static int bad(id self, const char *sel, rng_t r) {
    u64 n = length_of(self);
    if (r.loc > n || r.len > n - r.loc) { raise_range(sel, r.loc, r.len, n); return 1; }
    return 0;
}

/* ---------------------------------------------------------------- primitivas concretas */
static id p_string(id self, SEL c) { (void)c; return AS(self)->str; }
static id p_attrs(id self, SEL c, u64 idx, rng_t *eff) {
    (void)c;
    u64 n = runs_total(&AS(self)->runs);
    if (idx >= n) { raise_range("attributesAtIndex:effectiveRange:", idx, 0, n); return 0; }
    u64 st = 0, len = 0;
    id v = (id)runs_at(&AS(self)->runs, idx, &st, &len);
    if (eff) { eff->loc = st; eff->len = len; }
    return v;
}
static void sync_proxy(id self);
static void p_replace(id self, SEL c, rng_t r, id s) {
    (void)c;
    as_t *a = AS(self);
    u64 n = runs_total(&a->runs);
    if (r.loc > n || r.len > n - r.loc) { raise_range("replaceCharactersInRange:withString:", r.loc, r.len, n); return; }
    u64 sn = s ? MSG0(u64, s, "length") : 0;
    /* os caracteres novos herdam os atributos do primeiro trocado, ou do anterior quando é inserção */
    id attrs = 0;
    if (n) attrs = (id)runs_at(&a->runs, r.len ? r.loc : (r.loc ? r.loc - 1 : 0), 0, 0);
    if (!attrs) attrs = empty_dict();
    objc_retain(attrs);
    MSG(void, a->str, "replaceCharactersInRange:withString:", r, s ? s : cx_str(""));
    runs_replace(&a->runs, &CB, r.loc, r.len, sn, attrs);
    objc_release(attrs);
    sync_proxy(self);
}
static void p_set_attrs(id self, SEL c, id attrs, rng_t r) {
    (void)c;
    as_t *a = AS(self);
    u64 n = runs_total(&a->runs);
    if (r.loc > n || r.len > n - r.loc) { raise_range("setAttributes:range:", r.loc, r.len, n); return; }
    id d = attrs ? MSG0(id, attrs, "copy") : objc_retain(empty_dict());
    runs_set(&a->runs, &CB, r.loc, r.len, d);
    objc_release(d);
}

/* ---------------------------------------------------------------- criação */
static id setup(id self, id s, id attrs) {
    as_t *a = AS(self);
    a->str = MSG(id, cls("NSMutableString"), "stringWithString:", s ? s : cx_str(""));
    objc_retain(a->str);
    u64 n = MSG0(u64, a->str, "length");
    id d = attrs ? MSG0(id, attrs, "copy") : objc_retain(empty_dict());
    runs_replace(&a->runs, &CB, 0, 0, n, d);
    objc_release(d);
    return self;
}
static id k_alloc(id k, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)k); }
static id i_init(id self, SEL c) { (void)c; return setup(self, 0, 0); }
static id i_init_str(id self, SEL c, id s) { (void)c; return setup(self, s, 0); }
static id i_init_str_attrs(id self, SEL c, id s, id attrs) { (void)c; return setup(self, s, attrs); }
static id i_init_astr(id self, SEL c, id o) {
    (void)c;
    setup(self, o ? MSG0(id, o, "string") : 0, 0);
    u64 n = o ? length_of(o) : 0;
    for (u64 i = 0; i < n;) {
        rng_t e = { 0, 0 };
        id d = MSG(id, o, "attributesAtIndex:effectiveRange:", i, &e);
        if (!e.len) e.len = 1;
        rng_t r = { i, e.loc + e.len - i };
        p_set_attrs(self, 0, d, r);
        i += r.len;
    }
    return self;
}
static void i_dealloc(id self, SEL c) {
    (void)c;
    as_t *a = AS(self);
    if (a->proxy) ((msp_t *)a->proxy)->owner = 0;
    runs_free(&a->runs, &CB);
    objc_release(a->str);
    cx_kvo_forget(self);
    cx_object_free(self);
}

/* ---------------------------------------------------------------- leitura (sobre as primitivas) */
static u64 m_length(id self, SEL c) { (void)c; return length_of(self); }
static id m_attr(id self, SEL c, id name, u64 idx, rng_t *eff) {
    (void)c;
    rng_t e = { 0, 0 };
    id d = MSG(id, self, "attributesAtIndex:effectiveRange:", idx, eff ? &e : (rng_t *)0);
    id v = d && name ? MSG(id, d, "objectForKey:", name) : 0;
    if (eff) {
        /* a faixa onde este atributo tem o mesmo valor (pode juntar trechos vizinhos) */
        u64 n = length_of(self), lo = e.loc, hi = e.loc + e.len;
        while (lo > 0) {
            rng_t p = { 0, 0 };
            id dp = MSG(id, self, "attributesAtIndex:effectiveRange:", lo - 1, &p);
            id vp = dp ? MSG(id, dp, "objectForKey:", name) : 0;
            if (!(vp == v || (vp && v && MSG(BOOL, vp, "isEqual:", v)))) break;
            lo = p.loc;
        }
        while (hi < n) {
            rng_t p = { 0, 0 };
            id dp = MSG(id, self, "attributesAtIndex:effectiveRange:", hi, &p);
            id vp = dp ? MSG(id, dp, "objectForKey:", name) : 0;
            if (!(vp == v || (vp && v && MSG(BOOL, vp, "isEqual:", v)))) break;
            hi = p.loc + p.len;
        }
        eff->loc = lo; eff->len = hi - lo;
    }
    return v;
}
static id m_attrs_longest(id self, SEL c, u64 idx, rng_t *eff, rng_t lim) {
    (void)c;
    rng_t e = { 0, 0 };
    id d = MSG(id, self, "attributesAtIndex:effectiveRange:", idx, &e);
    if (eff) {
        u64 lo = e.loc, hi = e.loc + e.len, a = lim.loc, z = lim.loc + lim.len;
        while (lo > a) {
            rng_t p = { 0, 0 };
            id dp = MSG(id, self, "attributesAtIndex:effectiveRange:", lo - 1, &p);
            if (!(dp == d || MSG(BOOL, dp, "isEqual:", d))) break;
            lo = p.loc;
        }
        while (hi < z) {
            rng_t p = { 0, 0 };
            id dp = MSG(id, self, "attributesAtIndex:effectiveRange:", hi, &p);
            if (!(dp == d || MSG(BOOL, dp, "isEqual:", d))) break;
            hi = p.loc + p.len;
        }
        if (lo < a) lo = a;
        if (hi > z) hi = z;
        eff->loc = lo; eff->len = hi - lo;
    }
    return d;
}
static id m_attr_longest(id self, SEL c, id name, u64 idx, rng_t *eff, rng_t lim) {
    rng_t e = { 0, 0 };
    id v = m_attr(self, c, name, idx, eff ? &e : 0);
    if (eff) {
        u64 lo = e.loc < lim.loc ? lim.loc : e.loc, hi = e.loc + e.len, z = lim.loc + lim.len;
        if (hi > z) hi = z;
        eff->loc = lo; eff->len = hi > lo ? hi - lo : 0;
    }
    return v;
}
static id m_substring(id self, SEL c, rng_t r) {
    (void)c;
    if (bad(self, "attributedSubstringFromRange:", r)) return 0;
    id s = MSG0(id, self, "string");
    id sub = MSG(id, s, "substringWithRange:", r);
    id o = setup(cx_alloc_instance(&NSAttributedString_c), sub, 0);
    for (u64 i = r.loc; i < r.loc + r.len;) {
        rng_t e = { 0, 0 };
        id d = MSG(id, self, "attributesAtIndex:effectiveRange:", i, &e);
        u64 end = e.loc + e.len < r.loc + r.len ? e.loc + e.len : r.loc + r.len;
        if (end <= i) end = i + 1;
        rng_t t = { i - r.loc, end - i };
        p_set_attrs(o, 0, d, t);
        i = end;
    }
    return objc_autorelease(o);
}
static BOOL m_equal_astr(id self, SEL c, id o) {
    (void)c;
    if (o == self) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)&NSAttributedString_c)) return NO;
    if (!MSG(BOOL, MSG0(id, self, "string"), "isEqualToString:", MSG0(id, o, "string"))) return NO;
    u64 n = length_of(self);
    for (u64 i = 0; i < n;) {
        rng_t a = { 0, 0 }, b = { 0, 0 };
        id x = MSG(id, self, "attributesAtIndex:effectiveRange:", i, &a), y = MSG(id, o, "attributesAtIndex:effectiveRange:", i, &b);
        if (!(x == y || MSG(BOOL, x, "isEqual:", y))) return NO;
        u64 ea = a.loc + a.len, eb = b.loc + b.len, e = ea < eb ? ea : eb;
        i = e > i ? e : i + 1;
    }
    return YES;
}
static u64 m_hash(id self, SEL c) { (void)c; id s = MSG0(id, self, "string"); return s ? MSG0(u64, s, "hash") : 0; }
static id m_copy(id self, SEL c) {
    (void)c;
    if (*(cx_class_t **)self == &NSAttributedString_c) return objc_retain(self);
    return i_init_astr(cx_alloc_instance(&NSAttributedString_c), 0, self);
}
static id m_copy_zone(id self, SEL c, void *z) { (void)z; return m_copy(self, c); }
static id m_mcopy(id self, SEL c) { (void)c; return i_init_astr(cx_alloc_instance(&NSMutableAttributedString_c), 0, self); }
static id m_mcopy_zone(id self, SEL c, void *z) { (void)z; return m_mcopy(self, c); }
static id m_desc(id self, SEL c) {
    (void)c;
    id out = MSG0(id, cls("NSMutableString"), "string");
    id s = MSG0(id, self, "string");
    u64 n = length_of(self);
    for (u64 i = 0; i < n;) {
        rng_t e = { 0, 0 };
        id d = MSG(id, self, "attributesAtIndex:effectiveRange:", i, &e);
        if (!e.len) e.len = 1;
        rng_t r = { i, e.loc + e.len - i };
        id piece = MSG(id, s, "substringWithRange:", r);
        MSG(void, out, "appendString:", piece);
        MSG(void, out, "appendString:", MSG0(id, d, "description"));
        i += r.len;
    }
    return out;
}
static BOOL m_has_attachments(id self, SEL c, rng_t r) {
    (void)c;
    for (u64 i = r.loc; i < r.loc + r.len;) {
        rng_t e = { 0, 0 };
        id v = m_attr(self, 0, cx_str("NSAttachment"), i, &e);
        if (v) return YES;
        i = e.loc + e.len > i ? e.loc + e.len : i + 1;
    }
    return NO;
}
/* NSAttributedStringEnumerationReverse = 1<<1, LongestEffectiveRangeNotRequired = 1<<20 */
typedef struct { u64 loc, len; id v; } piece_t;
static void enumerate(id self, rng_t r, u64 opts, id name, void *blk) {
    if (bad(self, "enumerateAttributesInRange:options:usingBlock:", r) || !blk) return;
    int longest = !(opts & (1u << 20));
    u64 cap = 16, k = 0;
    piece_t *p = malloc(cap * sizeof *p);
    for (u64 i = r.loc; i < r.loc + r.len;) {
        rng_t e = { 0, 0 };
        id v = name ? (longest ? m_attr_longest(self, 0, name, i, &e, r) : m_attr(self, 0, name, i, &e))
                    : (longest ? m_attrs_longest(self, 0, i, &e, r) : MSG(id, self, "attributesAtIndex:effectiveRange:", i, &e));
        u64 a = e.loc < i ? i : e.loc, z = e.loc + e.len;
        if (z > r.loc + r.len) z = r.loc + r.len;
        if (z <= a) z = a + 1;
        if (k == cap) { cap *= 2; p = realloc(p, cap * sizeof *p); }
        p[k].loc = a; p[k].len = z - a; p[k].v = objc_retain(v); k++;
        i = z;
    }
    typedef void (*fn_t)(void *, id, rng_t, BOOL *);
    BOOL stop = NO;
    for (u64 j = 0; j < k && !stop; j++) {
        piece_t *q = p + ((opts & 2) ? k - 1 - j : j);
        rng_t rr = { q->loc, q->len };
        ((fn_t)blk_inv(blk))(blk, q->v, rr, &stop);
    }
    for (u64 j = 0; j < k; j++) objc_release(p[j].v);
    free(p);
}
static void m_enum_attrs(id self, SEL c, rng_t r, u64 opts, void *blk) { (void)c; enumerate(self, r, opts, 0, blk); }
static void m_enum_attr(id self, SEL c, id name, rng_t r, u64 opts, void *blk) { (void)c; if (name) enumerate(self, r, opts, name, blk); }

/* ---------------------------------------------------------------- mutação (sobre as primitivas) */
static void edit_attrs(id self, rng_t r, id name, id value, id many, int remove) {
    for (u64 i = r.loc; i < r.loc + r.len;) {
        rng_t e = { 0, 0 };
        id d = MSG(id, self, "attributesAtIndex:effectiveRange:", i, &e);
        u64 z = e.loc + e.len < r.loc + r.len ? e.loc + e.len : r.loc + r.len;
        if (z <= i) z = i + 1;
        id m = d ? MSG0(id, d, "mutableCopy") : MSG0(id, cls("NSMutableDictionary"), "new");
        if (many) MSG(void, m, "addEntriesFromDictionary:", many);
        else if (remove) MSG(void, m, "removeObjectForKey:", name);
        else MSG(void, m, "setObject:forKey:", value, name);
        rng_t t = { i, z - i };
        MSG(void, self, "setAttributes:range:", m, t);
        objc_release(m);
        i = z;
    }
}
static void mm_add_attr(id self, SEL c, id name, id value, rng_t r) {
    (void)c;
    if (!name || !value) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSInvalidArgumentException"),
                   cx_str("-[NSMutableAttributedString addAttribute:value:range:]: nil value"), (id)0);
        MSG0(void, e, "raise");
        return;
    }
    if (!bad(self, "addAttribute:value:range:", r)) edit_attrs(self, r, name, value, 0, 0);
}
static void mm_add_attrs(id self, SEL c, id attrs, rng_t r) { (void)c; if (attrs && !bad(self, "addAttributes:range:", r)) edit_attrs(self, r, 0, 0, attrs, 0); }
static void mm_remove_attr(id self, SEL c, id name, rng_t r) { (void)c; if (name && !bad(self, "removeAttribute:range:", r)) edit_attrs(self, r, name, 0, 0, 1); }
static void mm_replace_astr(id self, SEL c, rng_t r, id as) {
    (void)c;
    id s = as ? MSG0(id, as, "string") : cx_str("");
    MSG(void, self, "replaceCharactersInRange:withString:", r, s);
    u64 n = as ? length_of(as) : 0;
    for (u64 i = 0; i < n;) {
        rng_t e = { 0, 0 };
        id d = MSG(id, as, "attributesAtIndex:effectiveRange:", i, &e);
        u64 z = e.loc + e.len > i ? e.loc + e.len : i + 1;
        if (z > n) z = n;
        rng_t t = { r.loc + i, z - i };
        MSG(void, self, "setAttributes:range:", d, t);
        i = z;
    }
}
static void mm_insert(id self, SEL c, id as, u64 idx) { rng_t r = { idx, 0 }; mm_replace_astr(self, c, r, as); }
static void mm_append(id self, SEL c, id as) { rng_t r = { length_of(self), 0 }; mm_replace_astr(self, c, r, as); }
static void mm_delete(id self, SEL c, rng_t r) { (void)c; MSG(void, self, "replaceCharactersInRange:withString:", r, cx_str("")); }
static void mm_set_astr(id self, SEL c, id as) { rng_t r = { 0, length_of(self) }; mm_replace_astr(self, c, r, as); }
static void mm_begin(id self, SEL c) { (void)c; AS(self)->editing++; }
static void mm_end(id self, SEL c) { (void)c; if (AS(self)->editing > 0) AS(self)->editing--; }
static void mm_fix(id self, SEL c, rng_t r) { (void)self; (void)c; (void)r; }   /* o AppKit corrige fonte/parágrafo/anexos */
static id mm_copy(id self, SEL c) { (void)c; return i_init_astr(cx_alloc_instance(&NSAttributedString_c), 0, self); }

/* ---------------------------------------------------------------- mutableString viva */
static void ms_resync(id p) {
    msp_t *m = (msp_t *)p;
    const char *u = m->owner ? MSG0(const char *, MSG0(id, m->owner, "string"), "UTF8String") : "";
    if (!u) u = "";
    free(m->s);
    m->n = strlen(u);
    m->s = malloc(m->n + 1);
    memcpy(m->s, u, m->n + 1);
}
static void sync_proxy(id self) { if (AS(self)->proxy) ms_resync(AS(self)->proxy); }
static id mm_mutable_string(id self, SEL c) {
    (void)c;
    if (AS(self)->proxy) return AS(self)->proxy;
    id p = cx_alloc_instance(&ms_c);
    ((msp_t *)p)->owner = objc_retain(self);
    ms_resync(p);
    AS(self)->proxy = p;
    return objc_autorelease(p);
}
static void ms_replace(id p, SEL c, rng_t r, id s) {
    (void)c;
    msp_t *m = (msp_t *)p;
    if (m->owner) MSG(void, m->owner, "replaceCharactersInRange:withString:", r, s);
    ms_resync(p);
}
static void ms_set(id p, SEL c, id s) { rng_t r = { 0, MSG0(u64, p, "length") }; ms_replace(p, c, r, s); }
static void ms_append(id p, SEL c, id s) { rng_t r = { MSG0(u64, p, "length"), 0 }; ms_replace(p, c, r, s); }
static void ms_insert(id p, SEL c, id s, u64 i) { rng_t r = { i, 0 }; ms_replace(p, c, r, s); }
static void ms_delete(id p, SEL c, rng_t r) { ms_replace(p, c, r, cx_str("")); }
static void ms_dealloc(id p, SEL c) {
    (void)c;
    msp_t *m = (msp_t *)p;
    if (m->owner) {
        if (AS(m->owner)->proxy == p) AS(m->owner)->proxy = 0;
        objc_release(m->owner);
    }
    free(m->s);
    cx_kvo_forget(p);
    cx_object_free(p);
}

void cx_fdn_astr_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSAttributedString") == &NSAttributedString_c) return;
    cx_define_class(&NSAttributedString_c, &NSAttributedString_m, "NSAttributedString", ob, (u32)(sizeof(as_t) - 8));
    cx_define_class(&NSMutableAttributedString_c, &NSMutableAttributedString_m, "NSMutableAttributedString", &NSAttributedString_c, 0);
    cx_define_class(&ms_c, &ms_m, "__NSAttributedStringMutableString", &NSMutableString_c, (u32)(sizeof(msp_t) - sizeof(nsstr_t)));
    cx_class_t *k = &NSAttributedString_c, *mk = &NSMutableAttributedString_c;
#define A(s, f) cx_add_method(k, s, (void *)(f))
#define MA(s, f) cx_add_method(mk, s, (void *)(f))
    cx_add_cmethod(k, "alloc", (void *)k_alloc); cx_add_cmethod(k, "allocWithZone:", (void *)k_alloc);
    A("init", i_init); A("initWithString:", i_init_str); A("initWithString:attributes:", i_init_str_attrs);
    A("initWithAttributedString:", i_init_astr); A("dealloc", i_dealloc);
    A("string", p_string); A("attributesAtIndex:effectiveRange:", p_attrs);
    A("length", m_length); A("attribute:atIndex:effectiveRange:", m_attr);
    A("attributesAtIndex:longestEffectiveRange:inRange:", m_attrs_longest);
    A("attribute:atIndex:longestEffectiveRange:inRange:", m_attr_longest);
    A("attributedSubstringFromRange:", m_substring); A("isEqualToAttributedString:", m_equal_astr); A("isEqual:", m_equal_astr);
    A("hash", m_hash); A("copy", m_copy); A("copyWithZone:", m_copy_zone); A("mutableCopy", m_mcopy); A("mutableCopyWithZone:", m_mcopy_zone);
    A("description", m_desc); A("containsAttachmentsInRange:", m_has_attachments);
    A("enumerateAttributesInRange:options:usingBlock:", m_enum_attrs); A("enumerateAttribute:inRange:options:usingBlock:", m_enum_attr);
    MA("replaceCharactersInRange:withString:", p_replace); MA("setAttributes:range:", p_set_attrs);
    MA("mutableString", mm_mutable_string); MA("addAttribute:value:range:", mm_add_attr); MA("addAttributes:range:", mm_add_attrs);
    MA("removeAttribute:range:", mm_remove_attr); MA("replaceCharactersInRange:withAttributedString:", mm_replace_astr);
    MA("insertAttributedString:atIndex:", mm_insert); MA("appendAttributedString:", mm_append);
    MA("deleteCharactersInRange:", mm_delete); MA("setAttributedString:", mm_set_astr);
    MA("beginEditing", mm_begin); MA("endEditing", mm_end); MA("fixAttributesInRange:", mm_fix); MA("copy", mm_copy);
#undef A
#undef MA
    cx_class_t *m = &ms_c;
    cx_add_method(m, "replaceCharactersInRange:withString:", (void *)ms_replace); cx_add_method(m, "setString:", (void *)ms_set);
    cx_add_method(m, "appendString:", (void *)ms_append); cx_add_method(m, "insertString:atIndex:", (void *)ms_insert);
    cx_add_method(m, "deleteCharactersInRange:", (void *)ms_delete); cx_add_method(m, "dealloc", (void *)ms_dealloc);
}

/* ---------------------------------------------------------------- CFAttributedString (regra Create/Copy: +1) */
typedef long CFIndex;
typedef struct { CFIndex loc, len; } CFRange;
static rng_t R(CFRange r) { rng_t o = { (u64)r.loc, (u64)r.len }; return o; }
id CFAttributedStringCreate(void *alloc, id str, id attrs) {
    (void)alloc;
    return setup(cx_alloc_instance(&NSAttributedString_c), str, attrs);
}
id CFAttributedStringCreateWithSubstring(void *alloc, id a, CFRange r) {
    (void)alloc;
    id s = a ? MSG(id, a, "attributedSubstringFromRange:", R(r)) : 0;
    return s ? MSG0(id, s, "copy") : 0;
}
id CFAttributedStringCreateCopy(void *alloc, id a) { (void)alloc; return a ? MSG0(id, a, "copy") : 0; }
id CFAttributedStringCreateMutable(void *alloc, CFIndex max) { (void)alloc; (void)max; return setup(cx_alloc_instance(&NSMutableAttributedString_c), 0, 0); }
id CFAttributedStringCreateMutableCopy(void *alloc, CFIndex max, id a) { (void)alloc; (void)max; return a ? MSG0(id, a, "mutableCopy") : CFAttributedStringCreateMutable(0, 0); }
id CFAttributedStringGetAttributes(id a, CFIndex i, CFRange *eff) { return a ? MSG(id, a, "attributesAtIndex:effectiveRange:", (u64)i, (rng_t *)eff) : 0; }
id CFAttributedStringGetAttribute(id a, CFIndex i, id name, CFRange *eff) { return a ? MSG(id, a, "attribute:atIndex:effectiveRange:", name, (u64)i, (rng_t *)eff) : 0; }
id CFAttributedStringGetAttributesAndLongestEffectiveRange(id a, CFIndex i, CFRange lim, CFRange *eff) {
    return a ? MSG(id, a, "attributesAtIndex:longestEffectiveRange:inRange:", (u64)i, (rng_t *)eff, R(lim)) : 0;
}
id CFAttributedStringGetAttributeAndLongestEffectiveRange(id a, CFIndex i, id name, CFRange lim, CFRange *eff) {
    return a ? MSG(id, a, "attribute:atIndex:longestEffectiveRange:inRange:", name, (u64)i, (rng_t *)eff, R(lim)) : 0;
}
/* clearOtherAttributes: troca tudo; senão junta aos que já existem */
void CFAttributedStringSetAttributes(id a, CFRange r, id attrs, u8 clear) {
    if (!a) return;
    if (clear) MSG(void, a, "setAttributes:range:", attrs, R(r));
    else if (attrs) MSG(void, a, "addAttributes:range:", attrs, R(r));
}
void CFAttributedStringSetAttribute(id a, CFRange r, id name, id value) { if (a && name && value) MSG(void, a, "addAttribute:value:range:", name, value, R(r)); }
void CFAttributedStringRemoveAttribute(id a, CFRange r, id name) { if (a && name) MSG(void, a, "removeAttribute:range:", name, R(r)); }
void CFAttributedStringReplaceString(id a, CFRange r, id s) { if (a) MSG(void, a, "replaceCharactersInRange:withString:", R(r), s ? s : cx_str("")); }
void CFAttributedStringReplaceAttributedString(id a, CFRange r, id with) { if (a) MSG(void, a, "replaceCharactersInRange:withAttributedString:", R(r), with); }
id CFAttributedStringGetMutableString(id a) { return a ? MSG0(id, a, "mutableString") : 0; }
void CFAttributedStringBeginEditing(id a) { if (a) MSG0(void, a, "beginEditing"); }
void CFAttributedStringEndEditing(id a) { if (a) MSG0(void, a, "endEditing"); }

/*
 * mOS libSystem — TextKit 2 do AppKit, sobre o motor do TextKit 1 (appkit_textkit.c):
 *   NSTextRange e as localizações (_MOSTextLocation: deslocamento UTF-16, compare:), NSTextContentManager e
 *   NSTextContentStorage (o NSTextStorage por baixo, parágrafos como NSTextParagraph, transações de edição),
 *   NSTextLayoutManager (um NSLayoutManager + NSTextContainer internos sobre o mesmo storage; fragmentos por parágrafo,
 *   linhas, segmentos para seleção/realce, posição ↔ localização), NSTextLayoutFragment e NSTextLineFragment (desenho
 *   pelo layout manager interno), NSTextSelection, NSTextSelectionNavigation (movimentos por caractere, palavra, linha,
 *   parágrafo e documento) e NSTextViewportLayoutController (fragmentos visíveis com o delegate).
 *   NSTextView: textLayoutManager e textContentStorage sobre o storage da view; initUsingTextLayoutManager:.
 * Registro: cx_appkit_textkit2_init(), chamado do cx_appkit_input_init (depois do TextKit 1 e do NSTextView).
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
extern id NSApp;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RNG;
#define NOTFOUND ((u64)0x7fffffffffffffffULL)

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSTextRange) K(NSTextContentManager) K(NSTextContentStorage) K(NSTextElement) K(NSTextParagraph) K(NSTextLayoutManager)
K(NSTextLayoutFragment) K(NSTextLineFragment) K(NSTextSelection) K(NSTextSelectionNavigation) K(NSTextViewportLayoutController)
K(_MOSTextLocation)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static char k_props, k_weak1, k_weak2;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &k_props);
    if (!d) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(o, &k_props, d, 1); }
    return d;
}
static id pget(id o, const char *k) { return MSG(id, props(o), "objectForKey:", cx_str(k)); }
static void pset(id o, const char *k, id v) { if (v) MSG(void, props(o), "setObject:forKey:", v, cx_str(k)); else MSG(void, props(o), "removeObjectForKey:", cx_str(k)); }
static i64 pgi(id o, const char *k, i64 d) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : d; }
static void psi(id o, const char *k, i64 v) { pset(o, k, cx_num_i(v)); }
static id wget(id o, char *k) { return objc_getAssociatedObject(o, k); }
static void wset(id o, char *k, id v) { objc_setAssociatedObject(o, k, v, 0); }
static id arr(void) { return MSG0(id, (id)kls("NSMutableArray"), "array"); }
static id rect_val(R r) { return MSG(id, (id)kls("NSValue"), "valueWithRect:", r); }
static R val_rect(id v) { return v ? ((R (*)(id, SEL))objc_msgSend_stret)(v, sl("rectValue")) : mkr(0, 0, 0, 0); }
static void *block_fn(void *b) { return *(void **)((u8 *)b + 16); }

/* ================================================================ localizações e faixas */
static id loc_new(i64 off) { id l = objc_autorelease(cx_alloc_instance(&_MOSTextLocation_c)); psi(l, "o", off < 0 ? 0 : off); return l; }
static i64 loc_off(id l) { return l ? pgi(l, "o", 0) : 0; }
static i64 loc_compare(id self, SEL c, id other) { (void)c; i64 a = loc_off(self), b = loc_off(other); return a < b ? -1 : a > b ? 1 : 0; }
static BOOL loc_equal(id self, SEL c, id o) { (void)c; return o && object_getClass(o) == &_MOSTextLocation_c && loc_off(o) == loc_off(self); }
static u64 loc_hash(id self, SEL c) { (void)c; return (u64)loc_off(self); }
static id loc_desc(id self, SEL c) { (void)c; char b[48]; snprintf(b, sizeof b, "<location %lld>", (long long)loc_off(self)); return cx_str(b); }
static id loc_copy(id self, SEL c, void *z) { (void)c; (void)z; return objc_retain(self); }
static id range_new(i64 a, i64 b) {
    id r = objc_autorelease(cx_alloc_instance(&NSTextRange_c));
    pset(r, "s", loc_new(a)); pset(r, "e", loc_new(b < a ? a : b));
    return r;
}
static i64 r_a(id r) { return loc_off(pget(r, "s")); }
static i64 r_b(id r) { return loc_off(pget(r, "e")); }
static RNG r_nsr(id r) { i64 a = r_a(r), b = r_b(r); return (RNG){(u64)a, (u64)(b - a)}; }
static id tr_init(id self, SEL c, id s, id e) { (void)c; pset(self, "s", s); pset(self, "e", e ? e : s); return self; }
static id tr_init1(id self, SEL c, id s) { return tr_init(self, c, s, s); }
static id tr_loc(id self, SEL c) { (void)c; return pget(self, "s"); }
static id tr_end(id self, SEL c) { (void)c; return pget(self, "e"); }
static BOOL tr_empty(id self, SEL c) { (void)c; return r_a(self) == r_b(self); }
static BOOL tr_eq(id self, SEL c, id o) { (void)c; return o && responds(o, "location") && r_a(o) == r_a(self) && r_b(o) == r_b(self); }
static BOOL tr_contains_loc(id self, SEL c, id l) { (void)c; i64 o = loc_off(l); return o >= r_a(self) && o < r_b(self); }
static BOOL tr_contains_range(id self, SEL c, id r) { (void)c; return r_a(r) >= r_a(self) && r_b(r) <= r_b(self); }
static BOOL tr_intersects(id self, SEL c, id r) { (void)c; return r_a(r) < r_b(self) && r_a(self) < r_b(r); }
static id tr_intersection(id self, SEL c, id r) { (void)c; i64 a = r_a(self) > r_a(r) ? r_a(self) : r_a(r), b = r_b(self) < r_b(r) ? r_b(self) : r_b(r); return a <= b ? range_new(a, b) : 0; }
static id tr_union(id self, SEL c, id r) { (void)c; i64 a = r_a(self) < r_a(r) ? r_a(self) : r_a(r), b = r_b(self) > r_b(r) ? r_b(self) : r_b(r); return range_new(a, b); }
static id tr_desc(id self, SEL c) { (void)c; char b[64]; snprintf(b, sizeof b, "<NSTextRange %lld-%lld>", (long long)r_a(self), (long long)r_b(self)); return cx_str(b); }
static id tr_copy(id self, SEL c, void *z) { (void)c; (void)z; return objc_retain(self); }

/* ================================================================ NSTextContentStorage */
static id cs_storage(id self) {
    id ts = pget(self, "ts");
    if (!ts) { ts = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSTextStorage"), "alloc"), "init")); pset(self, "ts", ts); }
    return ts;
}
static id cs_init(id self, SEL c) { (void)c; cs_storage(self); pset(self, "tlms", arr()); return self; }
static id cs_ts(id self, SEL c) { (void)c; return cs_storage(self); }
static void cs_set_ts(id self, SEL c, id ts) { (void)c; pset(self, "ts", ts); }
static id cs_astr(id self, SEL c) { (void)c; return cs_storage(self); }
static void cs_set_astr(id self, SEL c, id a) { (void)c; MSG(void, cs_storage(self), "setAttributedString:", a); }
static u64 cs_len(id self) { return MSG0(u64, cs_storage(self), "length"); }
static id cs_doc_range(id self, SEL c) { (void)c; return range_new(0, (i64)cs_len(self)); }
static id cs_loc_offset(id self, SEL c, id l, i64 off) {
    (void)c;
    i64 o = loc_off(l) + off;
    if (o < 0 || o > (i64)cs_len(self)) return 0;
    return loc_new(o);
}
static i64 cs_offset(id self, SEL c, id a, id b) { (void)self; (void)c; return loc_off(b) - loc_off(a); }
static id cs_delegate(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void cs_set_delegate(id self, SEL c, id d) { (void)c; wset(self, &k_weak1, d); }
static id cs_tlms(id self, SEL c) { (void)c; id a = pget(self, "tlms"); if (!a) { a = arr(); pset(self, "tlms", a); } return a; }
static void cs_add_tlm(id self, SEL c, id tlm) { (void)c; id a = cs_tlms(self, 0); if (tlm && !MSG(BOOL, a, "containsObject:", tlm)) MSG(void, a, "addObject:", tlm); MSG(void, tlm, "setTextContentManager:", self); }
static void cs_remove_tlm(id self, SEL c, id tlm) { (void)c; MSG(void, cs_tlms(self, 0), "removeObject:", tlm); }
static id cs_primary(id self, SEL c) { (void)c; id p = wget(self, &k_weak2); if (p) return p; id a = cs_tlms(self, 0); return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : 0; }
static void cs_set_primary(id self, SEL c, id t) { (void)c; wset(self, &k_weak2, t); }
static void cs_perform_tx(id self, SEL c, void *block) {
    (void)c;
    id ts = cs_storage(self);
    MSG0(void, ts, "beginEditing");
    psi(self, "inTx", 1);
    if (block) ((void (*)(void *))block_fn(block))(block);
    psi(self, "inTx", 0);
    MSG0(void, ts, "endEditing");
}
static BOOL cs_has_tx(id self, SEL c) { (void)c; return pgi(self, "inTx", 0) != 0; }
static void cs_sync(id self, SEL c) { (void)self; (void)c; }
static BOOL cs_auto_sync(id self, SEL c) { (void)self; (void)c; return YES; }
/* parágrafo que contém o deslocamento */
static RNG para_at(id self, u64 o) {
    id s = MSG0(id, cs_storage(self), "string");
    u64 n = cs_len(self);
    if (o > n) o = n;
    if (!n) return (RNG){0, 0};
    return MSG(RNG, s, "paragraphRangeForRange:", ((RNG){o < n ? o : n - 1, 0}));
}
static id make_paragraph(id cs, RNG pr) {
    id p = objc_autorelease(cx_alloc_instance(&NSTextParagraph_c));
    id sub = MSG(id, cs_storage(cs), "attributedSubstringFromRange:", pr);
    pset(p, "astr", sub);
    pset(p, "range", range_new((i64)pr.loc, (i64)(pr.loc + pr.len)));
    wset(p, &k_weak1, cs);
    return p;
}
static id cs_elements_for_range(id self, SEL c, id r) {
    (void)c;
    id out = arr();
    u64 o = (u64)r_a(r), end = (u64)r_b(r), n = cs_len(self);
    if (!n) return out;
    while (o < end && o < n) {
        RNG pr = para_at(self, o);
        MSG(void, out, "addObject:", make_paragraph(self, pr));
        o = pr.loc + pr.len;
        if (!pr.len) break;
    }
    return out;
}
static id cs_enum_elements(id self, SEL c, id from, u64 opts, void *block) {
    (void)c;
    u64 n = cs_len(self);
    BOOL rev = (opts & 1) != 0;   /* NSTextContentManagerEnumerationOptionsReverse */
    u64 o = from ? (u64)loc_off(from) : (rev ? n : 0);
    BOOL (*inv)(void *, id) = (BOOL (*)(void *, id))block_fn(block);
    id last = 0;
    while (n) {
        if (rev) { if (o == 0) break; RNG pr = para_at(self, o - 1); id p = make_paragraph(self, pr); last = pr.loc ? loc_new((i64)pr.loc) : 0; if (!inv(block, p)) break; o = pr.loc; }
        else { if (o >= n) break; RNG pr = para_at(self, o); id p = make_paragraph(self, pr); last = loc_new((i64)(pr.loc + pr.len)); if (!inv(block, p) || !pr.len) break; o = pr.loc + pr.len; }
    }
    return last;
}
static id cs_adjusted(id self, SEL c, id r, BOOL editing) { (void)self; (void)c; (void)editing; return r; }
static void cs_replace_elements(id self, SEL c, id r, id elems) {
    (void)c;
    id ts = cs_storage(self);
    id joined = MSG0(id, MSG0(id, (id)kls("NSMutableAttributedString"), "alloc"), "init");
    for (u64 i = 0, n = elems ? MSG0(u64, elems, "count") : 0; i < n; i++) {
        id e = MSG(id, elems, "objectAtIndex:", i);
        id a = responds(e, "attributedString") ? MSG0(id, e, "attributedString") : 0;
        if (a) MSG(void, joined, "appendAttributedString:", a);
    }
    MSG(void, ts, "replaceCharactersInRange:withAttributedString:", r_nsr(r), joined);
    objc_release(joined);
}
static id cs_loc_for_offset_in_doc(id self, SEL c, u64 o) { (void)self; (void)c; return loc_new((i64)o); }

/* ---- NSTextElement / NSTextParagraph */
static id te_range(id self, SEL c) { (void)c; return pget(self, "range"); }
static void te_set_range(id self, SEL c, id r) { (void)c; pset(self, "range", r); }
static id te_manager(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void te_set_manager(id self, SEL c, id m) { (void)c; wset(self, &k_weak1, m); }
static id tp_init_astr(id self, SEL c, id a) { (void)c; pset(self, "astr", a); return self; }
static id tp_astr(id self, SEL c) { (void)c; return pget(self, "astr"); }
static id tp_content_range(id self, SEL c) {
    (void)c;
    id r = pget(self, "range");
    i64 a = r_a(r), b = r_b(r);
    id s = MSG0(id, pget(self, "astr"), "string");
    u64 n = s ? MSG0(u64, s, "length") : 0;
    if (n && MSG(unsigned short, s, "characterAtIndex:", n - 1) == '\n') b--;
    return range_new(a, b);
}
static id tp_separator_range(id self, SEL c) { id cr = tp_content_range(self, c); return range_new(r_b(cr), r_b(pget(self, "range"))); }

/* ================================================================ NSTextLayoutManager */
/* motor: NSLayoutManager + NSTextContainer próprios sobre o NSTextStorage do content storage */
static id tlm_engine(id self) {
    id lm = pget(self, "lm");
    if (lm) return lm;
    lm = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSLayoutManager"), "alloc"), "init"));
    id tc = pget(self, "tc");
    if (!tc) { tc = objc_autorelease(((id (*)(id, SEL, S))objc_msgSend)(MSG0(id, (id)kls("NSTextContainer"), "alloc"), sl("initWithSize:"), (S){1e7, 1e7})); pset(self, "tc", tc); }
    MSG(void, lm, "addTextContainer:", tc);
    pset(self, "lm", lm);
    id cm = wget(self, &k_weak1);
    if (cm) MSG(void, MSG0(id, cm, "textStorage"), "addLayoutManager:", lm);
    return lm;
}
static id tlm_init(id self, SEL c) { (void)c; pset(self, "sels", arr()); return self; }
static id tlm_cm(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void tlm_set_cm(id self, SEL c, id cm) {
    (void)c;
    id old = wget(self, &k_weak1);
    id lm = pget(self, "lm");
    if (old && lm) MSG(void, MSG0(id, old, "textStorage"), "removeLayoutManager:", lm);
    wset(self, &k_weak1, cm);
    if (cm && lm) MSG(void, MSG0(id, cm, "textStorage"), "addLayoutManager:", lm);
    if (cm && !MSG(BOOL, MSG0(id, cm, "textLayoutManagers"), "containsObject:", self)) MSG(void, cm, "addTextLayoutManager:", self);
}
static id tlm_tc(id self, SEL c) { (void)c; tlm_engine(self); return pget(self, "tc"); }
static void tlm_set_tc(id self, SEL c, id tc) {
    (void)c;
    id lm = tlm_engine(self);
    MSG(void, lm, "removeTextContainerAtIndex:", (u64)0);
    if (tc) { MSG(void, lm, "addTextContainer:", tc); pset(self, "tc", tc); }
}
static id tlm_delegate(id self, SEL c) { (void)c; return wget(self, &k_weak2); }
static void tlm_set_delegate(id self, SEL c, id d) { (void)c; wset(self, &k_weak2, d); }
static id tlm_doc_range(id self, SEL c) { (void)c; id cm = tlm_cm(self, 0); return cm ? MSG0(id, cm, "documentRange") : range_new(0, 0); }
static u64 tlm_len(id self) { id cm = tlm_cm(self, 0); return cm ? MSG0(u64, MSG0(id, cm, "textStorage"), "length") : 0; }
/* fragmento de um parágrafo: retângulo pelo motor, linhas pelos fragmentos do TextKit 1 */
static id make_fragment(id self, id para) {
    id f = objc_autorelease(cx_alloc_instance(&NSTextLayoutFragment_c));
    pset(f, "elem", para);
    wset(f, &k_weak1, self);
    id lm = tlm_engine(self), tc = pget(self, "tc");
    RNG pr = r_nsr(pget(para, "range"));
    R box = mkr(0, 0, 0, 0);
    id lines = arr();
    if (pr.len) {
        box = ((R (*)(id, SEL, RNG, id))objc_msgSend_stret)(lm, sl("boundingRectForGlyphRange:inTextContainer:"), pr, tc);
        u64 i = pr.loc;
        while (i < pr.loc + pr.len) {
            RNG eff = {0, 0};
            R fr = ((R (*)(id, SEL, u64, RNG *))objc_msgSend_stret)(lm, sl("lineFragmentRectForGlyphAtIndex:effectiveRange:"), i, &eff);
            R used = ((R (*)(id, SEL, u64, RNG *))objc_msgSend_stret)(lm, sl("lineFragmentUsedRectForGlyphAtIndex:effectiveRange:"), i, (RNG *)0);
            if (!eff.len) break;
            id ln = objc_autorelease(cx_alloc_instance(&NSTextLineFragment_c));
            id ts = MSG0(id, tlm_cm(self, 0), "textStorage");
            pset(ln, "astr", MSG(id, ts, "attributedSubstringFromRange:", eff));
            pset(ln, "crange", MSG(id, (id)kls("NSValue"), "valueWithRange:", ((RNG){eff.loc - pr.loc, eff.len})));
            pset(ln, "abs", MSG(id, (id)kls("NSValue"), "valueWithRange:", eff));
            pset(ln, "frame", rect_val(fr)); pset(ln, "used", rect_val(used));
            wset(ln, &k_weak1, self);
            MSG(void, lines, "addObject:", ln);
            box = i == pr.loc ? fr : mkr(box.x < fr.x ? box.x : fr.x, box.y < fr.y ? box.y : fr.y, (box.x + box.w > fr.x + fr.w ? box.x + box.w : fr.x + fr.w) - (box.x < fr.x ? box.x : fr.x), fr.y + fr.h - (box.y < fr.y ? box.y : fr.y));
            i = eff.loc + eff.len;
        }
    } else box = ((R (*)(id, SEL))objc_msgSend_stret)(lm, sl("extraLineFragmentRect"));
    pset(f, "frame", rect_val(box));
    pset(f, "lines", lines);
    return f;
}
static id tlm_enum_fragments(id self, SEL c, id from, u64 opts, void *block) {
    (void)c;
    id cm = tlm_cm(self, 0);
    if (!cm) return 0;
    BOOL (*inv)(void *, id) = (BOOL (*)(void *, id))block_fn(block);
    id ts = MSG0(id, cm, "textStorage");
    u64 n = MSG0(u64, ts, "length");
    BOOL rev = (opts & 1) != 0;   /* NSTextLayoutFragmentEnumerationOptionsReverse */
    u64 o = from ? (u64)loc_off(from) : (rev ? n : 0);
    id last = 0;
    if (!n) { id p = make_paragraph(cm, (RNG){0, 0}); inv(block, make_fragment(self, p)); return 0; }
    while (1) {
        RNG pr;
        if (rev) { if (!o) break; pr = MSG(RNG, MSG0(id, ts, "string"), "paragraphRangeForRange:", ((RNG){o - 1, 0})); }
        else { if (o >= n) break; pr = MSG(RNG, MSG0(id, ts, "string"), "paragraphRangeForRange:", ((RNG){o, 0})); }
        id frag = make_fragment(self, make_paragraph(cm, pr));
        last = loc_new((i64)(rev ? pr.loc : pr.loc + pr.len));
        if (!inv(block, frag) || !pr.len) break;
        o = rev ? pr.loc : pr.loc + pr.len;
    }
    return last;
}
static id tlm_fragment_for_loc(id self, SEL c, id l) {
    (void)c;
    id cm = tlm_cm(self, 0);
    if (!cm) return 0;
    id ts = MSG0(id, cm, "textStorage");
    u64 n = MSG0(u64, ts, "length"), o = (u64)loc_off(l);
    RNG pr = n ? MSG(RNG, MSG0(id, ts, "string"), "paragraphRangeForRange:", ((RNG){o < n ? o : n - 1, 0})) : (RNG){0, 0};
    return make_fragment(self, make_paragraph(cm, pr));
}
static id tlm_fragment_for_pos(id self, SEL c, P p) {
    (void)c;
    id lm = tlm_engine(self);
    u64 i = ((u64 (*)(id, SEL, P, id, double *))objc_msgSend)(lm, sl("characterIndexForPoint:inTextContainer:fractionOfDistanceBetweenInsertionPoints:"), p, pget(self, "tc"), (double *)0);
    return tlm_fragment_for_loc(self, 0, loc_new((i64)i));
}
static void tlm_ensure(id self, SEL c, id r) { (void)c; (void)r; MSG(void, tlm_engine(self), "ensureLayoutForTextContainer:", pget(self, "tc")); }
static void tlm_ensure_bounds(id self, SEL c, R b) { (void)b; tlm_ensure(self, c, 0); }
static void tlm_invalidate(id self, SEL c, id r) { (void)c; MSG(void, tlm_engine(self), "invalidateLayoutForCharacterRange:actualCharacterRange:", r ? r_nsr(r) : ((RNG){0, tlm_len(self)}), (RNG *)0); }
static R tlm_usage(id self, SEL c) { (void)c; return ((R (*)(id, SEL, id))objc_msgSend_stret)(tlm_engine(self), sl("usedRectForTextContainer:"), pget(self, "tc")); }
static id tlm_sels(id self, SEL c) { (void)c; id a = pget(self, "sels"); return a ? a : arr(); }
static void tlm_set_sels(id self, SEL c, id a) { (void)c; pset(self, "sels", a ? MSG0(id, a, "mutableCopy") : arr()); if (a) objc_release(pget(self, "sels")); }
static id tlm_nav(id self, SEL c) {
    (void)c;
    id nav = pget(self, "nav");
    if (!nav) { nav = objc_autorelease(cx_alloc_instance(&NSTextSelectionNavigation_c)); wset(nav, &k_weak1, self); pset(self, "nav", nav); }
    return nav;
}
static void tlm_set_nav(id self, SEL c, id nav) { (void)c; pset(self, "nav", nav); }
static id tlm_viewport(id self, SEL c) {
    (void)c;
    id v = pget(self, "vp");
    if (!v) { v = objc_autorelease(cx_alloc_instance(&NSTextViewportLayoutController_c)); wset(v, &k_weak1, self); pset(self, "vp", v); }
    return v;
}
/* segmentos (seleção, realce): um retângulo por linha atingida */
static void tlm_enum_segments(id self, SEL c, id r, i64 type, u64 opts, void *block) {
    (void)c; (void)type; (void)opts;
    id lm = tlm_engine(self);
    RNG nr = r_nsr(r);
    u64 cnt = 0;
    R *rs = ((R *(*)(id, SEL, RNG, RNG, id, u64 *))objc_msgSend)(lm, sl("rectArrayForCharacterRange:withinSelectedCharacterRange:inTextContainer:rectCount:"), nr, nr, pget(self, "tc"), &cnt);
    BOOL (*inv)(void *, id, R, double, id) = (BOOL (*)(void *, id, R, double, id))block_fn(block);
    for (u64 i = 0; i < cnt; i++) {
        R seg = rs[i];
        if (!inv(block, r, seg, 0, pget(self, "tc"))) break;
    }
}
static void tlm_replace_contents(id self, SEL c, id r, id astr) {
    (void)c;
    id ts = MSG0(id, tlm_cm(self, 0), "textStorage");
    MSG(void, ts, "replaceCharactersInRange:withAttributedString:", r_nsr(r), astr);
}
static void tlm_replace_elements(id self, SEL c, id r, id elems) { (void)c; MSG(void, tlm_cm(self, 0), "replaceContentsInRange:withTextElements:", r, elems); }
static id tlm_location_from(id self, SEL c, id l, i64 off) { (void)c; return MSG(id, tlm_cm(self, 0), "locationFromLocation:withOffset:", l, off); }
static i64 tlm_offset(id self, SEL c, id a, id b) { (void)self; (void)c; return loc_off(b) - loc_off(a); }
static id tlm_rendering_attrs(id self, SEL c, id l) { (void)self; (void)c; (void)l; return MSG0(id, (id)kls("NSDictionary"), "dictionary"); }
static BOOL tlm_uses_hyph(id self, SEL c) { (void)self; (void)c; return NO; }
static void tlm_noop1(id self, SEL c, id x) { (void)self; (void)c; (void)x; }

/* ---- NSTextLayoutFragment / NSTextLineFragment */
static id tlf_elem(id self, SEL c) { (void)c; return pget(self, "elem"); }
static id tlf_range(id self, SEL c) { (void)c; return pget(pget(self, "elem"), "range"); }
static id tlf_lines(id self, SEL c) { (void)c; return pget(self, "lines"); }
static R tlf_frame(id self, SEL c) { (void)c; return val_rect(pget(self, "frame")); }
static R tlf_render(id self, SEL c) { (void)c; R f = val_rect(pget(self, "frame")); return mkr(0, 0, f.w, f.h); }
static i64 tlf_state(id self, SEL c) { (void)self; (void)c; return 3; }   /* NSTextLayoutFragmentStateLayoutAvailable */
static id tlf_tlm(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void tlf_draw(id self, SEL c, P at, void *ctx) {
    (void)c; (void)ctx;
    id tlm = wget(self, &k_weak1);
    id lm = pget(tlm, "lm");
    RNG pr = r_nsr(pget(pget(self, "elem"), "range"));
    R f = val_rect(pget(self, "frame"));
    P o = {at.x - f.x, at.y - f.y};   /* o ponto é o canto do fragmento; o motor desenha em coordenadas do contêiner */
    ((void (*)(id, SEL, RNG, P))objc_msgSend)(lm, sl("drawBackgroundForGlyphRange:atPoint:"), pr, o);
    ((void (*)(id, SEL, RNG, P))objc_msgSend)(lm, sl("drawGlyphsForGlyphRange:atPoint:"), pr, o);
}
static id tlf_attachments(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)kls("NSArray"), "array"); }
static void tlf_invalidate(id self, SEL c) { (void)self; (void)c; }
static id tln_astr(id self, SEL c) { (void)c; return pget(self, "astr"); }
static RNG tln_crange(id self, SEL c) { (void)c; return MSG0(RNG, pget(self, "crange"), "rangeValue"); }
static R tln_bounds(id self, SEL c) { (void)c; R f = val_rect(pget(self, "frame")); R u = val_rect(pget(self, "used")); return mkr(u.x, f.y, u.w, f.h); }
static P tln_glyph_origin(id self, SEL c) { (void)c; R u = val_rect(pget(self, "used")); return (P){u.x, 0}; }
static P tln_loc_for_char(id self, SEL c, i64 i) {
    (void)c;
    id tlm = wget(self, &k_weak1);
    RNG abs = MSG0(RNG, pget(self, "abs"), "rangeValue");
    P p = ((P (*)(id, SEL, u64))objc_msgSend)(pget(tlm, "lm"), sl("locationForGlyphAtIndex:"), abs.loc + (u64)i);
    return p;
}
static i64 tln_char_for_point(id self, SEL c, P p) {
    (void)c;
    id tlm = wget(self, &k_weak1);
    R f = val_rect(pget(self, "frame"));
    RNG abs = MSG0(RNG, pget(self, "abs"), "rangeValue");
    u64 i = ((u64 (*)(id, SEL, P, id, double *))objc_msgSend)(pget(tlm, "lm"), sl("characterIndexForPoint:inTextContainer:fractionOfDistanceBetweenInsertionPoints:"),
                                                             (P){p.x, f.y + f.h / 2}, pget(tlm, "tc"), (double *)0);
    return (i64)(i >= abs.loc ? i - abs.loc : 0);
}
static void tln_draw(id self, SEL c, P at, void *ctx) {
    (void)c; (void)ctx;
    id tlm = wget(self, &k_weak1);
    RNG abs = MSG0(RNG, pget(self, "abs"), "rangeValue");
    R f = val_rect(pget(self, "frame"));
    P o = {at.x - f.x, at.y - f.y};
    ((void (*)(id, SEL, RNG, P))objc_msgSend)(pget(tlm, "lm"), sl("drawGlyphsForGlyphRange:atPoint:"), abs, o);
}

/* ================================================================ NSTextSelection e navegação */
static id ts_init_ranges(id self, SEL c, id ranges, i64 aff, i64 gran) { (void)c; pset(self, "ranges", ranges ? MSG0(id, ranges, "copy") : arr()); if (ranges) objc_release(pget(self, "ranges")); psi(self, "aff", aff); psi(self, "gran", gran); return self; }
static id ts_init_range(id self, SEL c, id r, i64 aff, i64 gran) { id a = MSG(id, (id)kls("NSArray"), "arrayWithObject:", r); return ts_init_ranges(self, c, a, aff, gran); }
static id ts_init_loc(id self, SEL c, id l, i64 aff) { return ts_init_range(self, c, range_new(loc_off(l), loc_off(l)), aff, 0); }
static id ts_ranges(id self, SEL c) { (void)c; return pget(self, "ranges"); }
static i64 ts_aff(id self, SEL c) { (void)c; return pgi(self, "aff", 0); }
static i64 ts_gran(id self, SEL c) { (void)c; return pgi(self, "gran", 0); }
static BOOL ts_logical(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL ts_transient(id self, SEL c) { (void)c; return pgi(self, "transient", 0) != 0; }
static id ts_typing(id self, SEL c) { (void)c; id t = pget(self, "typing"); return t ? t : MSG0(id, (id)kls("NSDictionary"), "dictionary"); }
static void ts_set_typing(id self, SEL c, id t) { (void)c; pset(self, "typing", t); }
static id ts_copy(id self, SEL c, void *z) { (void)c; (void)z; return objc_retain(self); }
/* destino de um movimento: direção (0 frente, 1 trás, 2 direita, 3 esquerda, 4 cima, 5 baixo), destino
 * (0 caractere, 1 palavra, 2 linha, 3 frase, 4 parágrafo, 5 contêiner, 6 documento) */
static int word_ch(unsigned short c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0xC0; }
static id nav_destination(id self, SEL c, id sel_, i64 dir, i64 dest, BOOL extend, BOOL confined) {
    (void)c; (void)confined;
    id tlm = wget(self, &k_weak1);
    id ts = MSG0(id, tlm_cm(tlm, 0), "textStorage");
    id s = MSG0(id, ts, "string");
    u64 n = MSG0(u64, ts, "length");
    id r0 = MSG0(id, MSG0(id, sel_, "textRanges"), "firstObject");
    i64 a = r_a(r0), b = r_b(r0);
    int fwd = dir == 0 || dir == 2 || dir == 5;
    i64 head = fwd ? b : a, t = head;
    switch (dest) {
    case 0: t = fwd ? (a != b && !extend ? b : head + 1) : (a != b && !extend ? a : head - 1); break;
    case 1:
        if (fwd) { while (t < (i64)n && !word_ch(MSG(unsigned short, s, "characterAtIndex:", (u64)t))) t++; while (t < (i64)n && word_ch(MSG(unsigned short, s, "characterAtIndex:", (u64)t))) t++; }
        else { while (t > 0 && !word_ch(MSG(unsigned short, s, "characterAtIndex:", (u64)(t - 1)))) t--; while (t > 0 && word_ch(MSG(unsigned short, s, "characterAtIndex:", (u64)(t - 1)))) t--; }
        break;
    case 2: {
        RNG eff = {0, 0};
        ((R (*)(id, SEL, u64, RNG *))objc_msgSend_stret)(tlm_engine(tlm), sl("lineFragmentRectForGlyphAtIndex:effectiveRange:"), (u64)(head < (i64)n ? head : (n ? (i64)n - 1 : 0)), &eff);
        if (dir == 4 || dir == 5) {   /* cima/baixo: mesma coluna na linha vizinha */
            P p = ((P (*)(id, SEL, u64))objc_msgSend)(tlm_engine(tlm), sl("locationForGlyphAtIndex:"), (u64)head);
            R fr = ((R (*)(id, SEL, u64, RNG *))objc_msgSend_stret)(tlm_engine(tlm), sl("lineFragmentRectForGlyphAtIndex:effectiveRange:"), (u64)(head < (i64)n ? head : (n ? (i64)n - 1 : 0)), (RNG *)0);
            P q = {p.x + fr.x, dir == 4 ? fr.y - 1 : fr.y + fr.h + 1};
            t = q.y < 0 ? 0 : (i64)((u64 (*)(id, SEL, P, id, double *))objc_msgSend)(tlm_engine(tlm), sl("characterIndexForPoint:inTextContainer:fractionOfDistanceBetweenInsertionPoints:"), q, pget(tlm, "tc"), (double *)0);
        } else t = fwd ? (i64)(eff.loc + eff.len) - (eff.len && eff.loc + eff.len <= n && MSG(unsigned short, s, "characterAtIndex:", eff.loc + eff.len - 1) == '\n' ? 1 : 0) : (i64)eff.loc;
        break;
    }
    case 3: case 4: {
        RNG pr = n ? MSG(RNG, s, "paragraphRangeForRange:", ((RNG){(u64)(head < (i64)n ? head : (i64)n - 1), 0})) : (RNG){0, 0};
        t = fwd ? (i64)(pr.loc + pr.len) : (i64)pr.loc;
        break;
    }
    default: t = fwd ? (i64)n : 0;
    }
    if (t < 0) t = 0;
    if (t > (i64)n) t = (i64)n;
    id nr;
    if (extend) { i64 anchor = fwd ? a : b; nr = t >= anchor ? range_new(anchor, t) : range_new(t, anchor); }
    else nr = range_new(t, t);
    id out = objc_autorelease(cx_alloc_instance(&NSTextSelection_c));
    return ts_init_range(out, 0, nr, 0, dest == 1 ? 1 : 0);
}
static id nav_deletion_ranges(id self, SEL c, id sel_, i64 dir, i64 dest, BOOL allows) {
    (void)c; (void)allows;
    id r0 = MSG0(id, MSG0(id, sel_, "textRanges"), "firstObject");
    if (r_a(r0) != r_b(r0)) return MSG(id, (id)kls("NSArray"), "arrayWithObject:", r0);
    id d = nav_destination(self, 0, sel_, dir, dest, YES, NO);
    return MSG0(id, d, "textRanges");
}
static id nav_at_point(id self, SEL c, P p, id anchors, i64 mods, id sel_, R bounds) {
    (void)c; (void)anchors; (void)mods; (void)sel_; (void)bounds;
    id tlm = wget(self, &k_weak1);
    u64 i = ((u64 (*)(id, SEL, P, id, double *))objc_msgSend)(tlm_engine(tlm), sl("characterIndexForPoint:inTextContainer:fractionOfDistanceBetweenInsertionPoints:"), p, pget(tlm, "tc"), (double *)0);
    id out = objc_autorelease(cx_alloc_instance(&NSTextSelection_c));
    ts_init_range(out, 0, range_new((i64)i, (i64)i), 0, 0);
    return MSG(id, (id)kls("NSArray"), "arrayWithObject:", out);
}
static id nav_tlm(id self, SEL c) { (void)c; return wget(self, &k_weak1); }

/* ================================================================ NSTextViewportLayoutController */
static R vp_bounds(id self, SEL c) {
    (void)c;
    id d = wget(self, &k_weak2);
    if (d && responds(d, "viewportBoundsForTextViewportLayoutController:")) return ((R (*)(id, SEL, id))objc_msgSend_stret)(d, sl("viewportBoundsForTextViewportLayoutController:"), self);
    id tlm = wget(self, &k_weak1);
    return tlm_usage(tlm, 0);
}
static id vp_delegate(id self, SEL c) { (void)c; return wget(self, &k_weak2); }
static void vp_set_delegate(id self, SEL c, id d) { (void)c; wset(self, &k_weak2, d); }
static id vp_range(id self, SEL c) { (void)c; id r = pget(self, "range"); return r ? r : tlm_doc_range(wget(self, &k_weak1), 0); }
static id vp_tlm(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void vp_layout(id self, SEL c) {
    (void)c;
    id tlm = wget(self, &k_weak1), d = wget(self, &k_weak2);
    if (!tlm) return;
    if (d && responds(d, "textViewportLayoutControllerWillLayout:")) MSG(void, d, "textViewportLayoutControllerWillLayout:", self);
    R vb = vp_bounds(self, 0);
    id cm = tlm_cm(tlm, 0);
    id ts = cm ? MSG0(id, cm, "textStorage") : 0;
    u64 n = ts ? MSG0(u64, ts, "length") : 0, o = 0;
    i64 first = -1, last = 0;
    if (!n && d && responds(d, "textViewportLayoutController:configureRenderingSurfaceForTextLayoutFragment:")) {
        id frag = make_fragment(tlm, make_paragraph(cm, (RNG){0, 0}));
        MSG(void, d, "textViewportLayoutController:configureRenderingSurfaceForTextLayoutFragment:", self, frag);
    }
    while (o < n) {
        RNG pr = MSG(RNG, MSG0(id, ts, "string"), "paragraphRangeForRange:", ((RNG){o, 0}));
        id frag = make_fragment(tlm, make_paragraph(cm, pr));
        R f = val_rect(pget(frag, "frame"));
        if (f.y + f.h >= vb.y && f.y <= vb.y + vb.h) {
            if (first < 0) first = (i64)pr.loc;
            last = (i64)(pr.loc + pr.len);
            if (d && responds(d, "textViewportLayoutController:configureRenderingSurfaceForTextLayoutFragment:"))
                MSG(void, d, "textViewportLayoutController:configureRenderingSurfaceForTextLayoutFragment:", self, frag);
        } else if (f.y > vb.y + vb.h) break;
        o = pr.loc + pr.len;
        if (!pr.len) break;
    }
    pset(self, "range", range_new(first < 0 ? 0 : first, last));
    if (d && responds(d, "textViewportLayoutControllerDidLayout:")) MSG(void, d, "textViewportLayoutControllerDidLayout:", self);
}
static void vp_adjust(id self, SEL c, double dy) { (void)self; (void)c; (void)dy; }
static void vp_relocate(id self, SEL c, id l) { (void)self; (void)c; (void)l; }

/* ================================================================ NSTextView (TextKit 2) */
static id tv_tlm(id self, SEL c) {
    (void)c;
    id t = pget(self, "tk2tlm");
    if (t) return t;
    id cs = objc_autorelease(MSG0(id, MSG0(id, (id)&NSTextContentStorage_c, "alloc"), "init"));
    MSG(void, cs, "setTextStorage:", MSG0(id, self, "textStorage"));
    t = objc_autorelease(MSG0(id, MSG0(id, (id)&NSTextLayoutManager_c, "alloc"), "init"));
    pset(t, "tc", MSG0(id, self, "textContainer"));
    MSG(void, cs, "addTextLayoutManager:", t);
    pset(self, "tk2tlm", t); pset(self, "tk2cs", cs);
    return t;
}
static id tv_tcs(id self, SEL c) { tv_tlm(self, c); return pget(self, "tk2cs"); }
static id tv_init_tk2(id self, SEL c, BOOL use) { (void)c; (void)use; return ((id (*)(id, SEL, R))objc_msgSend)(self, sl("initWithFrame:"), mkr(0, 0, 100, 100)); }
static id tv_init_frame_tk2(id self, SEL c, R f, BOOL use) { (void)c; (void)use; return ((id (*)(id, SEL, R))objc_msgSend)(self, sl("initWithFrame:"), f); }
static id tv_class_tk2(id k, SEL c, BOOL use) { (void)c; (void)use; return objc_autorelease(MSG0(id, MSG0(id, k, "alloc"), "init")); }

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
static cx_class_t *ensure(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    cx_class_t *s = kls(super);
    return cx_define_class(k, m, name, s ? s : kls("NSObject"), 0);
}
void cx_appkit_textkit2_init(void) {
    cx_class_t *k;
    k = ensure(&_MOSTextLocation_c, &_MOSTextLocation_m, "_MOSTextLocation", "NSObject");
    ADD(k, "compare:", loc_compare); ADD(k, "isEqual:", loc_equal); ADD(k, "hash", loc_hash); ADD(k, "description", loc_desc); ADD(k, "copyWithZone:", loc_copy);
    k = ensure(&NSTextRange_c, &NSTextRange_m, "NSTextRange", "NSObject");
    ADD(k, "initWithLocation:endLocation:", tr_init); ADD(k, "initWithLocation:", tr_init1); ADD(k, "location", tr_loc); ADD(k, "endLocation", tr_end);
    ADD(k, "isEmpty", tr_empty); ADD(k, "isEqualToTextRange:", tr_eq); ADD(k, "isEqual:", tr_eq); ADD(k, "containsLocation:", tr_contains_loc);
    ADD(k, "containsRange:", tr_contains_range); ADD(k, "intersectsWithTextRange:", tr_intersects); ADD(k, "textRangeByIntersectingWithTextRange:", tr_intersection);
    ADD(k, "textRangeByFormingUnionWithTextRange:", tr_union); ADD(k, "description", tr_desc); ADD(k, "copyWithZone:", tr_copy);
    cx_class_t *cm = ensure(&NSTextContentManager_c, &NSTextContentManager_m, "NSTextContentManager", "NSObject");
    k = ensure(&NSTextContentStorage_c, &NSTextContentStorage_m, "NSTextContentStorage", "NSTextContentManager");
    cx_class_t *targets[2] = {cm, k};
    for (int i = 0; i < 2; i++) {
        cx_class_t *t = targets[i];
        ADD(t, "init", cs_init); ADD(t, "textStorage", cs_ts); ADD(t, "setTextStorage:", cs_set_ts);
        ADD(t, "attributedString", cs_astr); ADD(t, "setAttributedString:", cs_set_astr); ADD(t, "documentRange", cs_doc_range);
        ADD(t, "locationFromLocation:withOffset:", cs_loc_offset); ADD(t, "offsetFromLocation:toLocation:", cs_offset);
        ADD(t, "delegate", cs_delegate); ADD(t, "setDelegate:", cs_set_delegate);
        ADD(t, "textLayoutManagers", cs_tlms); ADD(t, "addTextLayoutManager:", cs_add_tlm); ADD(t, "removeTextLayoutManager:", cs_remove_tlm);
        ADD(t, "primaryTextLayoutManager", cs_primary); ADD(t, "setPrimaryTextLayoutManager:", cs_set_primary);
        ADD(t, "performEditingTransactionUsingBlock:", cs_perform_tx); ADD(t, "hasEditingTransaction", cs_has_tx);
        ADD(t, "synchronizeTextLayoutManagers:", cs_sync); ADD(t, "automaticallySynchronizesTextLayoutManagers", cs_auto_sync);
        ADD(t, "automaticallySynchronizesToBackingStore", cs_auto_sync);
        ADD(t, "textElementsForRange:", cs_elements_for_range); ADD(t, "enumerateTextElementsFromLocation:options:usingBlock:", cs_enum_elements);
        ADD(t, "adjustedRangeFromRange:forEditingTextSelection:", cs_adjusted); ADD(t, "replaceContentsInRange:withTextElements:", cs_replace_elements);
        ADD(t, "locationFromOffset:", cs_loc_for_offset_in_doc);
    }
    cx_class_t *te = ensure(&NSTextElement_c, &NSTextElement_m, "NSTextElement", "NSObject");
    ADD(te, "elementRange", te_range); ADD(te, "setElementRange:", te_set_range); ADD(te, "textContentManager", te_manager); ADD(te, "setTextContentManager:", te_set_manager);
    k = ensure(&NSTextParagraph_c, &NSTextParagraph_m, "NSTextParagraph", "NSTextElement");
    ADD(k, "initWithAttributedString:", tp_init_astr); ADD(k, "attributedString", tp_astr);
    ADD(k, "paragraphContentRange", tp_content_range); ADD(k, "paragraphSeparatorRange", tp_separator_range);
    k = ensure(&NSTextLayoutManager_c, &NSTextLayoutManager_m, "NSTextLayoutManager", "NSObject");
    ADD(k, "init", tlm_init); ADD(k, "textContentManager", tlm_cm); ADD(k, "setTextContentManager:", tlm_set_cm);
    ADD(k, "textContainer", tlm_tc); ADD(k, "setTextContainer:", tlm_set_tc); ADD(k, "delegate", tlm_delegate); ADD(k, "setDelegate:", tlm_set_delegate);
    ADD(k, "documentRange", tlm_doc_range); ADD(k, "enumerateTextLayoutFragmentsFromLocation:options:usingBlock:", tlm_enum_fragments);
    ADD(k, "textLayoutFragmentForLocation:", tlm_fragment_for_loc); ADD(k, "textLayoutFragmentForPosition:", tlm_fragment_for_pos);
    ADD(k, "ensureLayoutForRange:", tlm_ensure); ADD(k, "ensureLayoutForBounds:", tlm_ensure_bounds); ADD(k, "invalidateLayoutForRange:", tlm_invalidate);
    ADD(k, "usageBoundsForTextContainer", tlm_usage); ADD(k, "textSelections", tlm_sels); ADD(k, "setTextSelections:", tlm_set_sels);
    ADD(k, "textSelectionNavigation", tlm_nav); ADD(k, "setTextSelectionNavigation:", tlm_set_nav); ADD(k, "textViewportLayoutController", tlm_viewport);
    ADD(k, "enumerateTextSegmentsInRange:type:options:usingBlock:", tlm_enum_segments);
    ADD(k, "replaceContentsInRange:withAttributedString:", tlm_replace_contents); ADD(k, "replaceContentsInRange:withTextElements:", tlm_replace_elements);
    ADD(k, "locationFromLocation:withOffset:", tlm_location_from); ADD(k, "offsetFromLocation:toLocation:", tlm_offset);
    ADD(k, "renderingAttributesForLink:atLocation:", tlm_rendering_attrs); ADD(k, "usesHyphenation", tlm_uses_hyph);
    ADD(k, "invalidateRenderingAttributesForTextRange:", tlm_noop1);
    k = ensure(&NSTextLayoutFragment_c, &NSTextLayoutFragment_m, "NSTextLayoutFragment", "NSObject");
    ADD(k, "textElement", tlf_elem); ADD(k, "rangeInElement", tlf_range); ADD(k, "textLineFragments", tlf_lines);
    ADD(k, "layoutFragmentFrame", tlf_frame); ADD(k, "renderingSurfaceBounds", tlf_render); ADD(k, "state", tlf_state);
    ADD(k, "textLayoutManager", tlf_tlm); ADD(k, "drawAtPoint:inContext:", tlf_draw); ADD(k, "textAttachmentViewProviders", tlf_attachments);
    ADD(k, "invalidateLayout", tlf_invalidate);
    k = ensure(&NSTextLineFragment_c, &NSTextLineFragment_m, "NSTextLineFragment", "NSObject");
    ADD(k, "attributedString", tln_astr); ADD(k, "characterRange", tln_crange); ADD(k, "typographicBounds", tln_bounds);
    ADD(k, "glyphOrigin", tln_glyph_origin); ADD(k, "locationForCharacterAtIndex:", tln_loc_for_char); ADD(k, "characterIndexForPoint:", tln_char_for_point);
    ADD(k, "drawAtPoint:inContext:", tln_draw);
    k = ensure(&NSTextSelection_c, &NSTextSelection_m, "NSTextSelection", "NSObject");
    ADD(k, "initWithRanges:affinity:granularity:", ts_init_ranges); ADD(k, "initWithRange:affinity:granularity:", ts_init_range);
    ADD(k, "initWithLocation:affinity:", ts_init_loc); ADD(k, "textRanges", ts_ranges); ADD(k, "affinity", ts_aff); ADD(k, "granularity", ts_gran);
    ADD(k, "isLogical", ts_logical); ADD(k, "isTransient", ts_transient); ADD(k, "typingAttributes", ts_typing); ADD(k, "setTypingAttributes:", ts_set_typing);
    ADD(k, "copyWithZone:", ts_copy);
    k = ensure(&NSTextSelectionNavigation_c, &NSTextSelectionNavigation_m, "NSTextSelectionNavigation", "NSObject");
    ADD(k, "destinationSelectionForTextSelection:direction:destination:extending:confined:", nav_destination);
    ADD(k, "deletionRangesForTextSelection:direction:destination:allowsDecomposition:", nav_deletion_ranges);
    ADD(k, "textSelectionsInteractingAtPoint:inContainerAtLocation:anchors:modifiers:selecting:bounds:", nav_at_point);
    ADD(k, "textSelectionDataSource", nav_tlm);
    k = ensure(&NSTextViewportLayoutController_c, &NSTextViewportLayoutController_m, "NSTextViewportLayoutController", "NSObject");
    ADD(k, "viewportBounds", vp_bounds); ADD(k, "viewportRange", vp_range); ADD(k, "delegate", vp_delegate); ADD(k, "setDelegate:", vp_set_delegate);
    ADD(k, "textLayoutManager", vp_tlm); ADD(k, "layoutViewport", vp_layout); ADD(k, "adjustViewportByVerticalOffset:", vp_adjust);
    ADD(k, "relocateViewportToTextLocation:", vp_relocate);
    cx_class_t *tv = kls("NSTextView");
    if (tv) {
        ADD(tv, "textLayoutManager", tv_tlm); ADD(tv, "textContentStorage", tv_tcs);
        ADD(tv, "initUsingTextLayoutManager:", tv_init_tk2); ADD(tv, "initWithFrame:usingTextLayoutManager:", tv_init_frame_tk2);
        cx_add_cmethod(tv, "textViewUsingTextLayoutManager:", (void *)tv_class_tk2);
    }
}

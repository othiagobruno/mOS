/*
 * mOS libSystem — TextKit 1 do AppKit:
 *   NSTextStorage: subclasse do NSMutableAttributedString (fdn_astr.c, do Eduardo) que sobrescreve as quatro primitivas,
 *     acumula edited:range:changeInLength: e, no fim da edição, chama o delegate, as notificações e os layout managers.
 *   NSTextContainer: tamanho, lineFragmentPadding, exclusionPaths, máximo de linhas, rastreio da view.
 *   NSLayoutManager: linhas pelo CTTypesetter (CoreText da Mariana) com o NSParagraphStyle de cada parágrafo
 *     (appkit_tk_core.c, teste tests-appkit/tk_host.c), vários contêineres, linha extra, consultas de glifo/caractere/ponto,
 *     atributos temporários e desenho de fundo, texto, sublinhado e tachado.
 *   Glifos: um glifo por caractere UTF-16 (o mOS desenha pelo texto do trecho; CTRun dá os glifos quando o app pede
 *     CGGlyph em glyphAtIndex:).
 * Registro: cx_appkit_textkit_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_tk_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id cx_color(double r, double g, double b, double a);
void NSRectFill(struct { double x, y, w, h; } r);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RNG;
typedef struct { long loc, len; } ct_range_t;
typedef struct { double x, y; } ct_pt_t;
id CTTypesetterCreateWithAttributedString(id astr);
long CTTypesetterSuggestLineBreak(id ts, long start, double width);
id CTTypesetterCreateLine(id ts, ct_range_t range);
double CTLineGetTypographicBounds(id line, double *ascent, double *descent, double *leading);
double CTLineGetTrailingWhitespaceWidth(id line);
double CTLineGetOffsetForStringIndex(id line, long idx, double *secondary);
long CTLineGetStringIndexForPosition(id line, ct_pt_t pos);
void CFRelease(id o);
extern id NSFontAttributeName, NSForegroundColorAttributeName, NSBackgroundColorAttributeName, NSParagraphStyleAttributeName;
extern id NSUnderlineStyleAttributeName, NSStrikethroughStyleAttributeName, NSUnderlineColorAttributeName, NSStrikethroughColorAttributeName;

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static R runion(R a, R b) {
    if (a.w <= 0 && a.h <= 0) return b;
    if (b.w <= 0 && b.h <= 0) return a;
    double x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    double x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    return mkr(x0, y0, x1 - x0, y1 - y0);
}
static void fillr(R r) { ((void (*)(R))NSRectFill)(r); }
#define NOTFOUND ((u64)0x7fffffffffffffffULL)

/* propriedades por objeto */
static char k_props;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &k_props);
    if (!d) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(o, &k_props, d, 1); }
    return d;
}
static id pget(id o, const char *k) { return MSG(id, props(o), "objectForKey:", cx_str(k)); }
static void pset(id o, const char *k, id v) { if (v) MSG(void, props(o), "setObject:forKey:", v, cx_str(k)); else MSG(void, props(o), "removeObjectForKey:", cx_str(k)); }
static i64 pgi(id o, const char *k, i64 d) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : d; }
static double pgd(id o, const char *k, double d) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : d; }
static void psi(id o, const char *k, i64 v) { pset(o, k, cx_num_i(v)); }
static void psd(id o, const char *k, double v) { pset(o, k, cx_num_d(v)); }
static char k_weak1, k_weak2, k_weak3;
static id wget(id o, char *k) { return objc_getAssociatedObject(o, k); }
static void wset(id o, char *k, id v) { objc_setAssociatedObject(o, k, v, 0); }
static id arr(void) { return MSG0(id, (id)kls("NSMutableArray"), "array"); }

/* ================================================================ NSTextStorage */
static IMP sup_replace, sup_set_attrs;
static void ts_note(id self, u64 mask, RNG r, i64 delta) { MSG(void, self, "edited:range:changeInLength:", mask, r, delta); }
static void ts_replace(id self, SEL c, RNG r, id str) {
    u64 n = str ? MSG0(u64, str, "length") : 0;
    ((void (*)(id, SEL, RNG, id))sup_replace)(self, c, r, str);
    ts_note(self, 2 /* NSTextStorageEditedCharacters */, r, (i64)n - (i64)r.len);
}
static void ts_set_attrs(id self, SEL c, id attrs, RNG r) {
    ((void (*)(id, SEL, id, RNG))sup_set_attrs)(self, c, attrs, r);
    ts_note(self, 1 /* NSTextStorageEditedAttributes */, r, 0);
}
static void ts_edited(id self, SEL c, u64 mask, RNG r, i64 delta) {
    (void)c;
    u64 m = (u64)pgi(self, "mask", 0);
    i64 d = pgi(self, "delta", 0);
    u64 a = (u64)pgi(self, "eloc", (i64)NOTFOUND), b = a == NOTFOUND ? 0 : a + (u64)pgi(self, "elen", 0);
    /* a faixa editada acumula, já deslocada pela mudança de tamanho */
    u64 na = r.loc, nb = r.loc + r.len + (delta > 0 ? (u64)delta : 0);
    if (delta < 0 && r.len >= (u64)-delta) nb = r.loc + r.len + (u64)delta;
    if (a == NOTFOUND) { a = na; b = nb; }
    else {
        if (b > r.loc) b = (u64)((i64)b + delta);
        if (na < a) a = na;
        if (nb > b) b = nb;
    }
    psi(self, "mask", (i64)(m | mask));
    psi(self, "delta", d + delta);
    psi(self, "eloc", (i64)a); psi(self, "elen", (i64)(b > a ? b - a : 0));
    if (!pgi(self, "editing", 0)) MSG0(void, self, "processEditing");
}
static void ts_begin(id self, SEL c) { (void)c; psi(self, "editing", pgi(self, "editing", 0) + 1); }
static void ts_end(id self, SEL c) {
    (void)c;
    i64 n = pgi(self, "editing", 0) - 1;
    psi(self, "editing", n < 0 ? 0 : n);
    if (n <= 0 && pgi(self, "mask", 0)) MSG0(void, self, "processEditing");
}
static u64 ts_mask(id self, SEL c) { (void)c; return (u64)pgi(self, "mask", 0); }
static RNG ts_erange(id self, SEL c) { (void)c; u64 a = (u64)pgi(self, "eloc", (i64)NOTFOUND); return (RNG){a, a == NOTFOUND ? 0 : (u64)pgi(self, "elen", 0)}; }
static i64 ts_delta(id self, SEL c) { (void)c; return pgi(self, "delta", 0); }
static void post_note(const char *name, id obj) {
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:", cx_str(name), obj);
}
static void ts_process(id self, SEL c) {
    (void)c;
    id d = wget(self, &k_weak1);
    u64 mask = (u64)pgi(self, "mask", 0);
    RNG er = ts_erange(self, 0);
    i64 delta = pgi(self, "delta", 0);
    if (d && responds(d, "textStorage:willProcessEditing:range:changeInLength:"))
        MSG(void, d, "textStorage:willProcessEditing:range:changeInLength:", self, mask, er, delta);
    post_note("NSTextStorageWillProcessEditingNotification", self);
    MSG(void, self, "fixAttributesInRange:", er);
    if (d && responds(d, "textStorage:didProcessEditing:range:changeInLength:"))
        MSG(void, d, "textStorage:didProcessEditing:range:changeInLength:", self, mask, er, delta);
    post_note("NSTextStorageDidProcessEditingNotification", self);
    psi(self, "mask", 0); psi(self, "delta", 0); psi(self, "eloc", (i64)NOTFOUND); psi(self, "elen", 0);
    id lms = pget(self, "lms");
    for (u64 i = 0, n = lms ? MSG0(u64, lms, "count") : 0; i < n; i++) {
        id lm = MSG(id, lms, "objectAtIndex:", i);
        MSG(void, lm, "processEditingForTextStorage:edited:range:changeInLength:invalidatedRange:", self, mask, er, delta, er);
    }
}
/* fixAttributesInRange:: sem fonte, a do sistema (o que o AppKit garante antes do layout) */
static void ts_fix(id self, SEL c, RNG r) {
    (void)c;
    u64 n = MSG0(u64, self, "length");
    if (!n) return;
    if (r.loc >= n) return;
    if (r.loc + r.len > n) r.len = n - r.loc;
    u64 i = r.loc;
    while (i < r.loc + r.len) {
        RNG eff = {0, 0};
        id f = MSG(id, self, "attribute:atIndex:effectiveRange:", NSFontAttributeName, i, &eff);
        u64 end = eff.loc + eff.len;
        if (end <= i) end = i + 1;
        if (end > r.loc + r.len) end = r.loc + r.len;
        if (!f) {
            id sys = MSG(id, (id)kls("NSFont"), "userFontOfSize:", 12.0);
            /* direto na superclasse: não gera outro edited: */
            id attrs = MSG(id, self, "attributesAtIndex:effectiveRange:", i, (RNG *)0);
            id m = attrs ? MSG0(id, attrs, "mutableCopy") : MSG0(id, (id)kls("NSMutableDictionary"), "new");
            MSG(void, m, "setObject:forKey:", sys, NSFontAttributeName);
            ((void (*)(id, SEL, id, RNG))sup_set_attrs)(self, sl("setAttributes:range:"), m, (RNG){i, end - i});
            objc_release(m);
        }
        i = end;
    }
}
static id ts_lms(id self, SEL c) { (void)c; id a = pget(self, "lms"); return a ? a : arr(); }
static void ts_add_lm(id self, SEL c, id lm) {
    (void)c;
    id a = pget(self, "lms");
    if (!a) { a = arr(); pset(self, "lms", a); }
    if (lm && !MSG(BOOL, a, "containsObject:", lm)) { MSG(void, a, "addObject:", lm); MSG(void, lm, "setTextStorage:", self); }
}
static void ts_remove_lm(id self, SEL c, id lm) { (void)c; id a = pget(self, "lms"); if (a) MSG(void, a, "removeObject:", lm); }
static id ts_delegate(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void ts_set_delegate(id self, SEL c, id d) { (void)c; wset(self, &k_weak1, d); }
static BOOL ts_lazy(id self, SEL c) { (void)self; (void)c; return NO; }
static void ts_invalidate_attrs(id self, SEL c, RNG r) { (void)c; MSG(void, self, "fixAttributesInRange:", r); }
static void ts_ensure_fixed(id self, SEL c, RNG r) { (void)self; (void)c; (void)r; }
static id ts_font(id self, SEL c) { (void)c; return MSG0(u64, self, "length") ? MSG(id, self, "attribute:atIndex:effectiveRange:", NSFontAttributeName, (u64)0, (RNG *)0) : 0; }
static void ts_set_font(id self, SEL c, id f) { (void)c; u64 n = MSG0(u64, self, "length"); if (n && f) MSG(void, self, "addAttribute:value:range:", NSFontAttributeName, f, ((RNG){0, n})); }
static id ts_fg(id self, SEL c) { (void)c; return MSG0(u64, self, "length") ? MSG(id, self, "attribute:atIndex:effectiveRange:", NSForegroundColorAttributeName, (u64)0, (RNG *)0) : 0; }
static void ts_set_fg(id self, SEL c, id col) { (void)c; u64 n = MSG0(u64, self, "length"); if (n && col) MSG(void, self, "addAttribute:value:range:", NSForegroundColorAttributeName, col, ((RNG){0, n})); }

/* ================================================================ NSTextContainer */
static id tc_init_size(id self, SEL c, S z) {
    (void)c;
    psd(self, "w", z.w); psd(self, "h", z.h); psd(self, "pad", 5); psi(self, "maxLines", 0);
    return self;
}
static id tc_init(id self, SEL c) { return tc_init_size(self, c, (S){1e7, 1e7}); }
static S tc_size(id self, SEL c) { (void)c; return (S){pgd(self, "w", 1e7), pgd(self, "h", 1e7)}; }
static void tc_invalidate(id self) { id lm = wget(self, &k_weak1); if (lm) MSG(void, lm, "textContainerChangedGeometry:", self); }
static void tc_set_size(id self, SEL c, S z) { (void)c; psd(self, "w", z.w); psd(self, "h", z.h); tc_invalidate(self); }
static double tc_pad(id self, SEL c) { (void)c; return pgd(self, "pad", 5); }
static void tc_set_pad(id self, SEL c, double v) { (void)c; psd(self, "pad", v); tc_invalidate(self); }
static BOOL tc_wtrack(id self, SEL c) { (void)c; return pgi(self, "wtrack", 0) != 0; }
static void tc_set_wtrack(id self, SEL c, BOOL b) { (void)c; psi(self, "wtrack", b); }
static BOOL tc_htrack(id self, SEL c) { (void)c; return pgi(self, "htrack", 0) != 0; }
static void tc_set_htrack(id self, SEL c, BOOL b) { (void)c; psi(self, "htrack", b); }
static id tc_lm(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void tc_set_lm(id self, SEL c, id lm) { (void)c; wset(self, &k_weak1, lm); }
static id tc_tv(id self, SEL c) { (void)c; return wget(self, &k_weak2); }
static void tc_set_tv(id self, SEL c, id tv) { (void)c; wset(self, &k_weak2, tv); }
static id tc_excl(id self, SEL c) { (void)c; id a = pget(self, "excl"); return a ? a : MSG0(id, (id)kls("NSArray"), "array"); }
static void tc_set_excl(id self, SEL c, id a) { (void)c; pset(self, "excl", a ? MSG0(id, a, "copy") : 0); if (a) objc_release(pget(self, "excl")); tc_invalidate(self); }
static i64 tc_max_lines(id self, SEL c) { (void)c; return pgi(self, "maxLines", 0); }
static void tc_set_max_lines(id self, SEL c, i64 n) { (void)c; psi(self, "maxLines", n); tc_invalidate(self); }
static i64 tc_lbm(id self, SEL c) { (void)c; return pgi(self, "lbm", 0); }
static void tc_set_lbm(id self, SEL c, i64 m) { (void)c; psi(self, "lbm", m); tc_invalidate(self); }
static BOOL tc_simple(id self, SEL c) { (void)c; id a = pget(self, "excl"); return !a || !MSG0(u64, a, "count"); }
/* proposta → retângulo livre das exclusões (bounds de cada caminho); o que sobra à direita volta em remaining */
static R tc_fragment(id self, SEL c, R prop, u64 idx, i64 dir, R *remaining) {
    (void)c; (void)idx; (void)dir;
    if (remaining) *remaining = mkr(0, 0, 0, 0);
    double W = pgd(self, "w", 1e7), H = pgd(self, "h", 1e7);
    if (prop.y + prop.h > H || prop.y < 0) return mkr(0, 0, 0, 0);
    if (prop.x + prop.w > W) prop.w = W - prop.x;
    id ex = pget(self, "excl");
    for (u64 i = 0, n = ex ? MSG0(u64, ex, "count") : 0; i < n; i++) {
        id path = MSG(id, ex, "objectAtIndex:", i);
        R b = ((R (*)(id, SEL))objc_msgSend_stret)(path, sl("bounds"));
        if (b.y >= prop.y + prop.h || b.y + b.h <= prop.y) continue;
        if (b.x + b.w <= prop.x || b.x >= prop.x + prop.w) continue;
        if (b.x <= prop.x) { double nx = b.x + b.w; prop.w -= nx - prop.x; prop.x = nx; }
        else if (b.x + b.w >= prop.x + prop.w) prop.w = b.x - prop.x;
        else {                                      /* no meio: a esquerda agora, a direita sobra */
            if (remaining) *remaining = mkr(b.x + b.w, prop.y, prop.x + prop.w - (b.x + b.w), prop.h);
            prop.w = b.x - prop.x;
        }
        if (prop.w <= 0) return mkr(0, 0, 0, 0);
    }
    return prop;
}
static BOOL tc_contains(id self, SEL c, P p) {
    (void)c;
    if (p.x < 0 || p.y < 0 || p.x > pgd(self, "w", 1e7) || p.y > pgd(self, "h", 1e7)) return NO;
    id ex = pget(self, "excl");
    for (u64 i = 0, n = ex ? MSG0(u64, ex, "count") : 0; i < n; i++)
        if (MSG(BOOL, MSG(id, ex, "objectAtIndex:", i), "containsPoint:", p)) return NO;
    return YES;
}

/* ================================================================ NSLayoutManager */
typedef struct { long loc, len; int tc; double x, y, w, h, ux, uw, base, asc; } lm_line;
static char k_lines, k_ctlines;
static id lm_storage(id self, SEL c) { (void)c; return wget(self, &k_weak1); }
static void lm_invalidate_all(id self) { psi(self, "valid", 0); }
static void lm_set_storage(id self, SEL c, id ts) { (void)c; wset(self, &k_weak1, ts); lm_invalidate_all(self); }
static id lm_containers(id self, SEL c) { (void)c; id a = pget(self, "tcs"); if (!a) { a = arr(); pset(self, "tcs", a); } return a; }
static void lm_add_tc(id self, SEL c, id tc) { (void)c; MSG(void, lm_containers(self, 0), "addObject:", tc); MSG(void, tc, "setLayoutManager:", self); lm_invalidate_all(self); }
static void lm_insert_tc(id self, SEL c, id tc, u64 i) { (void)c; MSG(void, lm_containers(self, 0), "insertObject:atIndex:", tc, i); MSG(void, tc, "setLayoutManager:", self); lm_invalidate_all(self); }
static void lm_remove_tc(id self, SEL c, u64 i) { (void)c; MSG(void, lm_containers(self, 0), "removeObjectAtIndex:", i); lm_invalidate_all(self); }
static void lm_tc_changed(id self, SEL c, id tc) { (void)c; (void)tc; lm_invalidate_all(self); }
static void lm_replace_storage(id self, SEL c, id ts) {
    (void)c;
    id old = wget(self, &k_weak1);
    if (old) MSG(void, old, "removeLayoutManager:", self);
    if (ts) MSG(void, ts, "addLayoutManager:", self);
    lm_invalidate_all(self);
}
static id lm_delegate(id self, SEL c) { (void)c; return wget(self, &k_weak2); }
static void lm_set_delegate(id self, SEL c, id d) { (void)c; wset(self, &k_weak2, d); }
static double font_ascent(id f) { return f && responds(f, "ascender") ? MSG0(double, f, "ascender") : 11; }
static double font_descent(id f) { double d = f && responds(f, "descender") ? MSG0(double, f, "descender") : -3; return d < 0 ? -d : d; }
static double font_leading(id f) { return f && responds(f, "leading") ? MSG0(double, f, "leading") : 0; }
static double lm_default_line_h(id self, SEL c, id f) { (void)self; (void)c; return font_ascent(f) + font_descent(f) + font_leading(f); }
static double lm_default_baseline(id self, SEL c, id f) { (void)self; (void)c; return font_ascent(f); }
static void para_from(tk_para *p, id ps) {
    tk_para_default(p);
    if (!ps) return;
    p->alignment = (int)MSG0(i64, ps, "alignment");
    p->first_indent = MSG0(double, ps, "firstLineHeadIndent"); p->head_indent = MSG0(double, ps, "headIndent");
    p->tail_indent = MSG0(double, ps, "tailIndent"); p->line_spacing = MSG0(double, ps, "lineSpacing");
    p->para_spacing = MSG0(double, ps, "paragraphSpacing");
    p->para_spacing_before = responds(ps, "paragraphSpacingBefore") ? MSG0(double, ps, "paragraphSpacingBefore") : 0;
    p->min_h = MSG0(double, ps, "minimumLineHeight"); p->max_h = MSG0(double, ps, "maximumLineHeight");
    p->multiple = MSG0(double, ps, "lineHeightMultiple");
}
static lm_line *lines_of(id self, long *n) {
    id d = objc_getAssociatedObject(self, &k_lines);
    *n = d ? (long)(MSG0(u64, d, "length") / sizeof(lm_line)) : 0;
    return d ? MSG0(lm_line *, d, "mutableBytes") : 0;
}
static id typing_font(id self) {
    id tv = MSG0(id, MSG0(id, self, "firstTextView"), "self");
    id ta = tv && responds(tv, "typingAttributes") ? MSG0(id, tv, "typingAttributes") : 0;
    id f = ta ? MSG(id, ta, "objectForKey:", NSFontAttributeName) : 0;
    return f ? f : MSG(id, (id)kls("NSFont"), "userFontOfSize:", 12.0);
}
static void lm_layout(id self) {
    if (pgi(self, "valid", 0)) return;
    psi(self, "valid", 1);
    id ts = lm_storage(self, 0);
    id data = MSG0(id, (id)kls("NSMutableData"), "data");
    id ctl = arr();
    objc_setAssociatedObject(self, &k_lines, data, 1);
    objc_setAssociatedObject(self, &k_ctlines, ctl, 1);
    pset(self, "extra", 0);
    id tcs = lm_containers(self, 0);
    u64 ntc = MSG0(u64, tcs, "count");
    if (!ntc) return;
    id str = ts ? MSG0(id, ts, "string") : 0;
    long n = str ? (long)MSG0(u64, str, "length") : 0;
    id setter = n ? CTTypesetterCreateWithAttributedString(ts) : 0;
    long pos = 0;
    u64 ci = 0;
    double y = 0;
    long lines_here = 0;
    while (pos < n && ci < ntc) {
        id tc = MSG(id, tcs, "objectAtIndex:", ci);
        S tsz = ((S (*)(id, SEL))objc_msgSend)(tc, sl("size"));
        double pad = MSG0(double, tc, "lineFragmentPadding");
        i64 maxl = MSG0(i64, tc, "maximumNumberOfLines");
        id ps = MSG(id, ts, "attribute:atIndex:effectiveRange:", NSParagraphStyleAttributeName, (u64)pos, (RNG *)0);
        tk_para p; para_from(&p, ps);
        unsigned short prev = pos ? MSG(unsigned short, str, "characterAtIndex:", (u64)(pos - 1)) : '\n';
        int first = prev == '\n' || prev == 0x2029;
        double avail = tk_avail_width(&p, tsz.w, pad, first);
        /* exclusões: a largura livre na altura desta linha (estimada pela fonte) */
        id f = MSG(id, ts, "attribute:atIndex:effectiveRange:", NSFontAttributeName, (u64)pos, (RNG *)0);
        double est_h = lm_default_line_h(self, 0, f ? f : typing_font(self));
        R rem;
        R frag = ((R (*)(id, SEL, R, u64, i64, R *))objc_msgSend_stret)(tc, sl("lineFragmentRectForProposedRect:atIndex:writingDirection:remainingRect:"),
                                                                     mkr(0, y, tsz.w, est_h), (u64)pos, (i64)0, &rem);
        if (frag.w <= 0 || (maxl > 0 && lines_here >= maxl)) {          /* contêiner cheio: o próximo */
            if (frag.w <= 0 && y + est_h <= tsz.h && tsz.h < 1e6) { y += est_h; continue; }   /* exclusão cobre a linha toda */
            ci++; y = 0; lines_here = 0;
            continue;
        }
        double excl_left = frag.x, excl_w = frag.w;
        if (excl_w < tsz.w) avail = avail < excl_w - pad ? avail : excl_w - pad;
        long brk = CTTypesetterSuggestLineBreak(setter, pos, avail > 1 ? avail : 1);
        if (brk < 1) brk = 1;
        if (pos + brk > n) brk = n - pos;
        id line = CTTypesetterCreateLine(setter, (ct_range_t){pos, brk});
        double a = 0, d = 0, l = 0;
        double tw = CTLineGetTypographicBounds(line, &a, &d, &l);
        if (a + d <= 0) { a = font_ascent(f); d = font_descent(f); }
        double tws = CTLineGetTrailingWhitespaceWidth(line);
        unsigned short last = MSG(unsigned short, str, "characterAtIndex:", (u64)(pos + brk - 1));
        int pend = last == '\n' || last == 0x2029 || pos + brk == n;
        tk_line L = tk_place(&p, tsz.w, pad, first, first, pend, y, a, d, l, tw, tws);
        if (tsz.h < 1e6 && y + L.h > tsz.h && lines_here > 0) { CFRelease(line); ci++; y = 0; lines_here = 0; continue; }
        if (excl_left > 0) L.used_x += excl_left;
        lm_line rec = {pos, brk, (int)ci, L.x, L.y, L.w, L.h, L.used_x, L.used_w, L.baseline, a};
        MSG(void, data, "appendBytes:length:", &rec, (u64)sizeof rec);
        MSG(void, ctl, "addObject:", line);
        CFRelease(line);
        y += L.h;
        pos += brk;
        lines_here++;
    }
    if (setter) CFRelease(setter);
    /* linha extra: texto vazio ou terminado em quebra (onde o cursor fica) */
    unsigned short lastc = n ? MSG(unsigned short, str, "characterAtIndex:", (u64)(n - 1)) : '\n';
    if (pos >= n && (lastc == '\n' || lastc == 0x2029) && ci < ntc) {
        id tc = MSG(id, tcs, "objectAtIndex:", ci);
        S tsz = ((S (*)(id, SEL))objc_msgSend)(tc, sl("size"));
        id f = typing_font(self);
        double h = lm_default_line_h(self, 0, f), pad = MSG0(double, tc, "lineFragmentPadding");
        pset(self, "extra", MSG(id, (id)kls("NSValue"), "valueWithRect:", mkr(0, y, tsz.w, h)));
        pset(self, "extraUsed", MSG(id, (id)kls("NSValue"), "valueWithRect:", mkr(pad, y, 1, h)));
        psi(self, "extraTC", (i64)ci);
    }
}
/* linha que contém o caractere i (ou a última) */
static long line_for(id self, u64 i) {
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    for (long k = 0; k < n; k++) if ((u64)L[k].loc <= i && i < (u64)(L[k].loc + L[k].len)) return k;
    return n ? n - 1 : -1;
}
static id ct_line(id self, long k) { id a = objc_getAssociatedObject(self, &k_ctlines); return a && k >= 0 && (u64)k < MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)k) : 0; }
static double x_of(id self, long k, u64 idx) {
    long n; lm_line *L = lines_of(self, &n);
    id cl = ct_line(self, k);
    return L[k].ux + (cl ? CTLineGetOffsetForStringIndex(cl, (long)idx, 0) : 0);
}
static u64 lm_num_glyphs(id self, SEL c) { (void)c; id ts = lm_storage(self, 0); return ts ? MSG0(u64, ts, "length") : 0; }
static RNG lm_glyph_range_for_chars(id self, SEL c, RNG r, RNG *actual) { (void)self; (void)c; if (actual) *actual = r; return r; }
static RNG lm_char_range_for_glyphs(id self, SEL c, RNG r, RNG *actual) { (void)self; (void)c; if (actual) *actual = r; return r; }
static u64 lm_char_for_glyph(id self, SEL c, u64 g) { (void)self; (void)c; return g; }
static u64 lm_glyph_for_char(id self, SEL c, u64 i) { (void)self; (void)c; return i; }
static u32 lm_glyph_at(id self, SEL c, u64 g) {
    (void)c;
    id ts = lm_storage(self, 0);
    return ts && g < MSG0(u64, ts, "length") ? MSG(unsigned short, MSG0(id, ts, "string"), "characterAtIndex:", g) : 0;
}
static BOOL lm_valid_glyph(id self, SEL c, u64 g) { return g < lm_num_glyphs(self, c); }
static R lm_used_rect(id self, SEL c, id tc) {
    (void)c;
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    i64 ci = (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc);
    R u = mkr(0, 0, 0, 0);
    for (long k = 0; k < n; k++) if (L[k].tc == ci) u = runion(u, mkr(L[k].ux, L[k].y, L[k].uw, L[k].h));
    id ex = pget(self, "extraUsed");
    if (ex && pgi(self, "extraTC", -1) == ci) u = runion(u, ((R (*)(id, SEL))objc_msgSend_stret)(ex, sl("rectValue")));
    return u;
}
static RNG lm_glyph_range_tc(id self, SEL c, id tc) {
    (void)c;
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    i64 ci = (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc);
    long a = -1, b = -1;
    for (long k = 0; k < n; k++) if (L[k].tc == ci) { if (a < 0) a = L[k].loc; b = L[k].loc + L[k].len; }
    return a < 0 ? (RNG){0, 0} : (RNG){(u64)a, (u64)(b - a)};
}
static id lm_tc_for_glyph(id self, SEL c, u64 g, RNG *eff) {
    (void)c;
    long k = line_for(self, g);
    if (k < 0) return MSG0(u64, lm_containers(self, 0), "count") ? MSG(id, lm_containers(self, 0), "objectAtIndex:", (u64)0) : 0;
    long n; lm_line *L = lines_of(self, &n);
    id tc = MSG(id, lm_containers(self, 0), "objectAtIndex:", (u64)L[k].tc);
    if (eff) *eff = lm_glyph_range_tc(self, 0, tc);
    return tc;
}
static id lm_tc_for_glyph3(id self, SEL c, u64 g, RNG *eff, BOOL noLayout) { (void)noLayout; return lm_tc_for_glyph(self, c, g, eff); }
static R lm_frag_rect(id self, SEL c, u64 g, RNG *eff) {
    (void)c;
    long k = line_for(self, g);
    long n; lm_line *L = lines_of(self, &n);
    if (k < 0) { if (eff) *eff = (RNG){0, 0}; return mkr(0, 0, 0, 0); }
    if (eff) *eff = (RNG){(u64)L[k].loc, (u64)L[k].len};
    return mkr(L[k].x, L[k].y, L[k].w, L[k].h);
}
static R lm_frag_rect3(id self, SEL c, u64 g, RNG *eff, BOOL nl) { (void)nl; return lm_frag_rect(self, c, g, eff); }
static R lm_frag_used(id self, SEL c, u64 g, RNG *eff) {
    (void)c;
    long k = line_for(self, g);
    long n; lm_line *L = lines_of(self, &n);
    if (k < 0) { if (eff) *eff = (RNG){0, 0}; return mkr(0, 0, 0, 0); }
    if (eff) *eff = (RNG){(u64)L[k].loc, (u64)L[k].len};
    return mkr(L[k].ux, L[k].y, L[k].uw, L[k].h);
}
static R lm_frag_used3(id self, SEL c, u64 g, RNG *eff, BOOL nl) { (void)nl; return lm_frag_used(self, c, g, eff); }
static P lm_location(id self, SEL c, u64 g) {
    (void)c;
    long k = line_for(self, g);
    long n; lm_line *L = lines_of(self, &n);
    if (k < 0) return (P){0, 0};
    return (P){x_of(self, k, g) - L[k].x, L[k].base};   /* relativo ao fragmento da linha, y na linha de base */
}
static R lm_extra(id self, SEL c) { (void)c; lm_layout(self); id v = pget(self, "extra"); return v ? ((R (*)(id, SEL))objc_msgSend_stret)(v, sl("rectValue")) : mkr(0, 0, 0, 0); }
static R lm_extra_used(id self, SEL c) { (void)c; lm_layout(self); id v = pget(self, "extraUsed"); return v ? ((R (*)(id, SEL))objc_msgSend_stret)(v, sl("rectValue")) : mkr(0, 0, 0, 0); }
static id lm_extra_tc(id self, SEL c) { (void)c; lm_layout(self); i64 i = pgi(self, "extraTC", -1); return pget(self, "extra") && i >= 0 ? MSG(id, lm_containers(self, 0), "objectAtIndex:", (u64)i) : 0; }
/* retângulos de uma faixa, linha a linha */
static int range_rects(id self, RNG r, i64 ci, R *out, int cap) {
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    int m = 0;
    u64 a = r.loc, b = r.loc + r.len;
    for (long k = 0; k < n && m < cap; k++) {
        if (ci >= 0 && L[k].tc != ci) continue;
        u64 la = (u64)L[k].loc, lb = (u64)(L[k].loc + L[k].len);
        if (b <= la || a >= lb) { if (!(r.len == 0 && a == la)) continue; }
        u64 s = a > la ? a : la, e = b < lb ? b : lb;
        double x0 = x_of(self, k, s), x1 = x_of(self, k, e);
        if (e == lb && b > lb) x1 = L[k].ux + L[k].uw;   /* a seleção atravessa a quebra: até o fim usado */
        out[m++] = mkr(x0, L[k].y, x1 > x0 ? x1 - x0 : (r.len ? 0 : 1), L[k].h);
    }
    return m;
}
static R lm_bounding(id self, SEL c, RNG r, id tc) {
    (void)c;
    i64 ci = tc ? (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc) : -1;
    R rs[512];
    int m = range_rects(self, r, ci, rs, 512);
    R u = mkr(0, 0, 0, 0);
    for (int i = 0; i < m; i++) u = runion(u, rs[i]);
    if (!m && r.loc >= lm_num_glyphs(self, 0)) u = lm_extra_used(self, 0);
    return u;
}
static RNG lm_range_for_rect(id self, SEL c, R rect, id tc) {
    (void)c;
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    i64 ci = tc ? (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc) : -1;
    long a = -1, b = -1;
    for (long k = 0; k < n; k++) {
        if (ci >= 0 && L[k].tc != ci) continue;
        if (L[k].y + L[k].h <= rect.y || L[k].y >= rect.y + rect.h) continue;
        if (a < 0) a = L[k].loc;
        b = L[k].loc + L[k].len;
    }
    return a < 0 ? (RNG){0, 0} : (RNG){(u64)a, (u64)(b - a)};
}
static u64 lm_char_for_point(id self, SEL c, P p, id tc, double *frac) {
    (void)c;
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    i64 ci = tc ? (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc) : -1;
    if (frac) *frac = 0;
    long best = -1;
    for (long k = 0; k < n; k++) {
        if (ci >= 0 && L[k].tc != ci) continue;
        if (best < 0 || p.y >= L[k].y) best = k;
        if (p.y < L[k].y + L[k].h) { best = k; break; }
    }
    if (best < 0) return 0;
    if (p.y >= L[best].y + L[best].h && pget(self, "extra")) return lm_num_glyphs(self, 0);
    id cl = ct_line(self, best);
    long idx = cl ? CTLineGetStringIndexForPosition(cl, (ct_pt_t){p.x - L[best].ux, 0}) : L[best].loc;
    long end = L[best].loc + L[best].len;
    id str = MSG0(id, lm_storage(self, 0), "string");
    if (idx >= end && end > L[best].loc) {               /* depois do fim: antes da quebra da linha */
        unsigned short ch = MSG(unsigned short, str, "characterAtIndex:", (u64)(end - 1));
        if (ch == '\n' || ch == 0x2029 || end < (long)lm_num_glyphs(self, 0)) idx = end - 1;
        if (ch != '\n' && ch != 0x2029 && end == (long)lm_num_glyphs(self, 0)) idx = end;
    }
    if (idx < L[best].loc) idx = L[best].loc;
    return (u64)idx;
}
static u64 lm_char_for_point4(id self, SEL c, P p, id tc, double *frac) { return lm_char_for_point(self, c, p, tc, frac); }
static u64 lm_glyph_for_point(id self, SEL c, P p, id tc) { return lm_char_for_point(self, c, p, tc, 0); }
static u64 lm_glyph_for_point3(id self, SEL c, P p, id tc, double *frac) { return lm_char_for_point(self, c, p, tc, frac); }
static void lm_enum_lines(id self, SEL c, RNG r, void *block) {
    (void)c;
    lm_layout(self);
    long n; lm_line *L = lines_of(self, &n);
    void (*inv)(void *, R, R, id, RNG, BOOL *) = *(void (**)(void *, R, R, id, RNG, BOOL *))((u8 *)block + 16);
    for (long k = 0; k < n; k++) {
        u64 la = (u64)L[k].loc, lb = (u64)(L[k].loc + L[k].len);
        if (lb <= r.loc || la >= r.loc + r.len) continue;
        BOOL stop = NO;
        id tc = MSG(id, lm_containers(self, 0), "objectAtIndex:", (u64)L[k].tc);
        inv(block, mkr(L[k].x, L[k].y, L[k].w, L[k].h), mkr(L[k].ux, L[k].y, L[k].uw, L[k].h), tc, (RNG){la, lb - la}, &stop);
        if (stop) break;
    }
}
static void lm_enum_rects(id self, SEL c, RNG r, RNG sel, id tc, void *block) {
    (void)c; (void)sel;
    i64 ci = tc ? (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc) : -1;
    R rs[512];
    int m = range_rects(self, r, ci, rs, 512);
    void (*inv)(void *, R, BOOL *) = *(void (**)(void *, R, BOOL *))((u8 *)block + 16);
    for (int i = 0; i < m; i++) { BOOL stop = NO; inv(block, rs[i], &stop); if (stop) break; }
}
static R *lm_rect_array(id self, SEL c, RNG r, RNG sel, id tc, u64 *count) {
    (void)c; (void)sel;
    static R buf[512];
    i64 ci = tc ? (i64)MSG(u64, lm_containers(self, 0), "indexOfObjectIdenticalTo:", tc) : -1;
    int m = range_rects(self, r, ci, buf, 512);
    if (count) *count = (u64)m;
    return buf;
}
static void lm_ensure(id self, SEL c, id x) { (void)c; (void)x; lm_layout(self); }
static void lm_ensure_r(id self, SEL c, RNG r) { (void)c; (void)r; lm_layout(self); }
static void lm_invalidate_layout(id self, SEL c, RNG r, RNG *actual) { (void)c; if (actual) *actual = r; lm_invalidate_all(self); }
static void lm_invalidate_glyphs(id self, SEL c, RNG r, i64 delta, RNG *actual) { (void)c; (void)delta; if (actual) *actual = r; lm_invalidate_all(self); }
static void redisplay_views(id self) {
    id tcs = lm_containers(self, 0);
    for (u64 i = 0, n = MSG0(u64, tcs, "count"); i < n; i++) {
        id tv = MSG0(id, MSG(id, tcs, "objectAtIndex:", i), "textView");
        if (tv) MSG(void, tv, "setNeedsDisplay:", (BOOL)YES);
    }
}
static void lm_invalidate_display(id self, SEL c, RNG r) { (void)c; (void)r; redisplay_views(self); }
static void lm_process_editing(id self, SEL c, id ts, u64 mask, RNG r, i64 delta, RNG inv) {
    (void)c; (void)ts; (void)mask; (void)r; (void)delta; (void)inv;
    lm_invalidate_all(self);
    redisplay_views(self);
    id tcs = lm_containers(self, 0);
    for (u64 i = 0, n = MSG0(u64, tcs, "count"); i < n; i++) {
        id tv = MSG0(id, MSG(id, tcs, "objectAtIndex:", i), "textView");
        if (tv && responds(tv, "_cxStorageEdited:delta:")) MSG(void, tv, "_cxStorageEdited:delta:", r, delta);
    }
}
static id lm_first_tv(id self, SEL c) {
    (void)c;
    id tcs = lm_containers(self, 0);
    return MSG0(u64, tcs, "count") ? MSG0(id, MSG(id, tcs, "objectAtIndex:", (u64)0), "textView") : 0;
}
#define LM_BOOL(get, key, def) \
    static BOOL lm_g_##get(id self, SEL c) { (void)c; return pgi(self, key, def) != 0; } \
    static void lm_s_##get(id self, SEL c, BOOL b) { (void)c; psi(self, key, b); lm_invalidate_all(self); }
LM_BOOL(nonContig, "nonContig", 0) LM_BOOL(leading, "leading", 1) LM_BOOL(invisibles, "invis", 0) LM_BOOL(controls, "controls", 0)
LM_BOOL(background, "bgLayout", 1) LM_BOOL(hyphen, "hyphen", 0)
static BOOL lm_has_noncontig(id self, SEL c) { (void)self; (void)c; return NO; }

/* atributos temporários (corretor, buscar): por faixa, só para o desenho */
static id temp_list(id self) { id a = pget(self, "temps"); if (!a) { a = arr(); pset(self, "temps", a); } return a; }
static void lm_add_temp(id self, SEL c, id name, id value, RNG r) {
    (void)c;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", name, cx_str("name")); MSG(void, d, "setObject:forKey:", value, cx_str("value"));
    id rv = MSG(id, (id)kls("NSValue"), "valueWithRange:", r);
    MSG(void, d, "setObject:forKey:", rv, cx_str("range"));
    MSG(void, temp_list(self), "addObject:", d);
    redisplay_views(self);
}
static RNG range_of(id d) { return MSG0(RNG, MSG(id, d, "objectForKey:", cx_str("range")), "rangeValue"); }
static void lm_remove_temp(id self, SEL c, id name, RNG r) {
    (void)c;
    id a = temp_list(self);
    for (u64 i = MSG0(u64, a, "count"); i-- > 0;) {
        id d = MSG(id, a, "objectAtIndex:", i);
        RNG q = range_of(d);
        if (q.loc + q.len <= r.loc || q.loc >= r.loc + r.len) continue;
        if (MSG(BOOL, MSG(id, d, "objectForKey:", cx_str("name")), "isEqual:", name)) MSG(void, a, "removeObjectAtIndex:", i);
    }
    redisplay_views(self);
}
static void lm_set_temps(id self, SEL c, id attrs, RNG r) {
    (void)c;
    id keys = attrs ? MSG0(id, attrs, "allKeys") : 0;
    for (u64 i = 0, n = keys ? MSG0(u64, keys, "count") : 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        lm_remove_temp(self, 0, k, r);
        lm_add_temp(self, 0, k, MSG(id, attrs, "objectForKey:", k), r);
    }
}
static void lm_add_temps(id self, SEL c, id attrs, RNG r) { lm_set_temps(self, c, attrs, r); }
static id lm_temps_at(id self, SEL c, u64 i, RNG *eff) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    id a = temp_list(self);
    u64 lo = 0, hi = lm_num_glyphs(self, 0);
    for (u64 k = 0, n = MSG0(u64, a, "count"); k < n; k++) {
        id d = MSG(id, a, "objectAtIndex:", k);
        RNG q = range_of(d);
        if (i >= q.loc && i < q.loc + q.len) {
            id tv_ = MSG(id, d, "objectForKey:", cx_str("value")), tn_ = MSG(id, d, "objectForKey:", cx_str("name"));
            MSG(void, out, "setObject:forKey:", tv_, tn_);
            if (q.loc > lo) lo = q.loc;
            if (q.loc + q.len < hi) hi = q.loc + q.len;
        } else if (q.loc > i && q.loc < hi) hi = q.loc;
        else if (q.loc + q.len <= i && q.loc + q.len > lo) lo = q.loc + q.len;
    }
    if (eff) *eff = (RNG){lo, hi > lo ? hi - lo : 0};
    return out;
}
static id lm_temp_attr(id self, SEL c, id name, u64 i, RNG *eff) { id d = lm_temps_at(self, c, i, eff); return MSG(id, d, "objectForKey:", name); }

/* ---- desenho */
static id run_attrs(id self, id ts, u64 i, RNG *eff) {
    RNG e1 = {0, 0}, e2 = {0, 0};
    id base = MSG(id, ts, "attributesAtIndex:effectiveRange:", i, &e1);
    id tmp = lm_temps_at(self, 0, i, &e2);
    u64 a = e1.loc > e2.loc ? e1.loc : e2.loc, b1 = e1.loc + e1.len, b2 = e2.loc + e2.len, b = b1 < b2 ? b1 : b2;
    if (b <= i) b = i + 1;
    if (eff) *eff = (RNG){a, b - a};
    if (!MSG0(u64, tmp, "count")) return base;
    id m = base ? MSG0(id, base, "mutableCopy") : MSG0(id, (id)kls("NSMutableDictionary"), "new");
    MSG(void, m, "addEntriesFromDictionary:", tmp);
    return objc_autorelease(m);
}
static void lm_draw_bg(id self, SEL c, RNG gr, P origin) {
    (void)c;
    lm_layout(self);
    id ts = lm_storage(self, 0);
    long n; lm_line *L = lines_of(self, &n);
    for (long k = 0; k < n; k++) {
        u64 la = (u64)L[k].loc, lb = (u64)(L[k].loc + L[k].len);
        if (lb <= gr.loc || la >= gr.loc + gr.len) continue;
        u64 i = la > gr.loc ? la : gr.loc, end = lb < gr.loc + gr.len ? lb : gr.loc + gr.len;
        while (i < end) {
            RNG eff;
            id at = run_attrs(self, ts, i, &eff);
            u64 e = eff.loc + eff.len < end ? eff.loc + eff.len : end;
            id bg = at ? MSG(id, at, "objectForKey:", NSBackgroundColorAttributeName) : 0;
            if (bg) {
                double x0 = x_of(self, k, i), x1 = x_of(self, k, e);
                MSG0(void, bg, "setFill");
                fillr(mkr(origin.x + x0, origin.y + L[k].y, x1 - x0, L[k].h));
            }
            i = e > i ? e : i + 1;
        }
    }
}
static void lm_draw_glyphs(id self, SEL c, RNG gr, P origin) {
    (void)c;
    lm_layout(self);
    id ts = lm_storage(self, 0);
    id str = ts ? MSG0(id, ts, "string") : 0;
    long n; lm_line *L = lines_of(self, &n);
    for (long k = 0; k < n; k++) {
        u64 la = (u64)L[k].loc, lb = (u64)(L[k].loc + L[k].len);
        if (lb <= gr.loc || la >= gr.loc + gr.len) continue;
        u64 end = lb < gr.loc + gr.len ? lb : gr.loc + gr.len;
        while (end > la) {                                   /* a quebra de linha não se desenha */
            unsigned short ch = MSG(unsigned short, str, "characterAtIndex:", end - 1);
            if (ch != '\n' && ch != '\r' && ch != 0x2029 && ch != 0x2028) break;
            end--;
        }
        u64 i = la > gr.loc ? la : gr.loc;
        while (i < end) {
            RNG eff;
            id at = run_attrs(self, ts, i, &eff);
            u64 e = eff.loc + eff.len < end ? eff.loc + eff.len : end;
            if (e <= i) e = i + 1;
            id sub = MSG(id, ts, "attributedSubstringFromRange:", ((RNG){i, e - i}));
            if (at && MSG0(u64, MSG(id, sub, "attributesAtIndex:effectiveRange:", (u64)0, (RNG *)0), "count") != MSG0(u64, at, "count")) {
                id m = MSG0(id, sub, "mutableCopy");
                MSG(void, m, "setAttributes:range:", at, ((RNG){0, e - i}));
                sub = objc_autorelease(m);
            }
            id f = at ? MSG(id, at, "objectForKey:", NSFontAttributeName) : 0;
            double asc = font_ascent(f);
            double x = origin.x + x_of(self, k, i), base = origin.y + L[k].y + L[k].base;
            ((void (*)(id, SEL, P))objc_msgSend)(sub, sl("drawAtPoint:"), (P){x, base - asc});
            double w = x_of(self, k, e) - x_of(self, k, i);
            id u = at ? MSG(id, at, "objectForKey:", NSUnderlineStyleAttributeName) : 0;
            id fg = at ? MSG(id, at, "objectForKey:", NSForegroundColorAttributeName) : 0;
            if (u && MSG0(i64, u, "integerValue")) {
                id uc = MSG(id, at, "objectForKey:", NSUnderlineColorAttributeName);
                MSG0(void, uc ? uc : fg ? fg : cx_color(0, 0, 0, 1), "setFill");
                double th = (MSG0(i64, u, "integerValue") & 0xF) == 2 ? 2 : 1;      /* NSUnderlineStyleThick */
                fillr(mkr(x, base + 1.5, w, th));
                if ((MSG0(i64, u, "integerValue") & 0xF) == 9) fillr(mkr(x, base + 4, w, 1));   /* Double */
            }
            id st = at ? MSG(id, at, "objectForKey:", NSStrikethroughStyleAttributeName) : 0;
            if (st && MSG0(i64, st, "integerValue")) {
                id sc = MSG(id, at, "objectForKey:", NSStrikethroughColorAttributeName);
                MSG0(void, sc ? sc : fg ? fg : cx_color(0, 0, 0, 1), "setFill");
                fillr(mkr(x, base - asc * 0.32, w, 1));
            }
            i = e;
        }
    }
}
static void lm_underline(id self, SEL c, RNG gr, i64 style, double base, R frag, RNG cr, P origin) {
    (void)self; (void)c; (void)gr; (void)cr;
    if (!style) return;
    fillr(mkr(origin.x + frag.x, origin.y + frag.y + base + 1.5, frag.w, (style & 0xF) == 2 ? 2 : 1));
}
static void lm_fill_bg_rects(id self, SEL c, R *rects, u64 n, RNG gr, id color) {
    (void)self; (void)c; (void)gr;
    if (color) MSG0(void, color, "setFill");
    for (u64 i = 0; i < n; i++) fillr(rects[i]);
}

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
static cx_class_t *ensure(const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    static cx_class_t pool_c[4], pool_m[4];
    static int used;
    if (used >= 4) return 0;
    cx_class_t *s = kls(super);
    return cx_define_class(&pool_c[used], &pool_m[used], name, s ? s : kls("NSObject"), 0) ? &pool_c[used++] : 0;
}
void cx_appkit_textkit_init(void) {
    cx_class_t *mas = kls("NSMutableAttributedString");
    cx_class_t *ts = ensure("NSTextStorage", "NSMutableAttributedString");
    cx_class_t *tc = ensure("NSTextContainer", "NSObject");
    cx_class_t *lm = ensure("NSLayoutManager", "NSObject");
    if (!mas || !ts || !tc || !lm) return;
    sup_replace = class_getMethodImplementation(mas, sl("replaceCharactersInRange:withString:"));
    sup_set_attrs = class_getMethodImplementation(mas, sl("setAttributes:range:"));
    ADD(ts, "replaceCharactersInRange:withString:", ts_replace); ADD(ts, "setAttributes:range:", ts_set_attrs);
    ADD(ts, "edited:range:changeInLength:", ts_edited); ADD(ts, "beginEditing", ts_begin); ADD(ts, "endEditing", ts_end);
    ADD(ts, "editedMask", ts_mask); ADD(ts, "editedRange", ts_erange); ADD(ts, "changeInLength", ts_delta);
    ADD(ts, "processEditing", ts_process); ADD(ts, "fixAttributesInRange:", ts_fix);
    ADD(ts, "layoutManagers", ts_lms); ADD(ts, "addLayoutManager:", ts_add_lm); ADD(ts, "removeLayoutManager:", ts_remove_lm);
    ADD(ts, "delegate", ts_delegate); ADD(ts, "setDelegate:", ts_set_delegate); ADD(ts, "fixesAttributesLazily", ts_lazy);
    ADD(ts, "invalidateAttributesInRange:", ts_invalidate_attrs); ADD(ts, "ensureAttributesAreFixedInRange:", ts_ensure_fixed);
    ADD(ts, "font", ts_font); ADD(ts, "setFont:", ts_set_font); ADD(ts, "foregroundColor", ts_fg); ADD(ts, "setForegroundColor:", ts_set_fg);
    ADD(tc, "init", tc_init); ADD(tc, "initWithSize:", tc_init_size); ADD(tc, "initWithContainerSize:", tc_init_size);
    ADD(tc, "size", tc_size); ADD(tc, "setSize:", tc_set_size); ADD(tc, "containerSize", tc_size); ADD(tc, "setContainerSize:", tc_set_size);
    ADD(tc, "lineFragmentPadding", tc_pad); ADD(tc, "setLineFragmentPadding:", tc_set_pad);
    ADD(tc, "widthTracksTextView", tc_wtrack); ADD(tc, "setWidthTracksTextView:", tc_set_wtrack);
    ADD(tc, "heightTracksTextView", tc_htrack); ADD(tc, "setHeightTracksTextView:", tc_set_htrack);
    ADD(tc, "layoutManager", tc_lm); ADD(tc, "setLayoutManager:", tc_set_lm); ADD(tc, "textView", tc_tv); ADD(tc, "setTextView:", tc_set_tv);
    ADD(tc, "exclusionPaths", tc_excl); ADD(tc, "setExclusionPaths:", tc_set_excl);
    ADD(tc, "maximumNumberOfLines", tc_max_lines); ADD(tc, "setMaximumNumberOfLines:", tc_set_max_lines);
    ADD(tc, "lineBreakMode", tc_lbm); ADD(tc, "setLineBreakMode:", tc_set_lbm); ADD(tc, "isSimpleRectangularTextContainer", tc_simple);
    ADD(tc, "lineFragmentRectForProposedRect:atIndex:writingDirection:remainingRect:", tc_fragment); ADD(tc, "containsPoint:", tc_contains);
    ADD(lm, "textStorage", lm_storage); ADD(lm, "setTextStorage:", lm_set_storage); ADD(lm, "replaceTextStorage:", lm_replace_storage);
    ADD(lm, "textContainers", lm_containers); ADD(lm, "addTextContainer:", lm_add_tc); ADD(lm, "insertTextContainer:atIndex:", lm_insert_tc);
    ADD(lm, "removeTextContainerAtIndex:", lm_remove_tc); ADD(lm, "textContainerChangedGeometry:", lm_tc_changed);
    ADD(lm, "delegate", lm_delegate); ADD(lm, "setDelegate:", lm_set_delegate); ADD(lm, "firstTextView", lm_first_tv);
    ADD(lm, "textViewForBeginningOfSelection", lm_first_tv);
    ADD(lm, "defaultLineHeightForFont:", lm_default_line_h); ADD(lm, "defaultBaselineOffsetForFont:", lm_default_baseline);
    ADD(lm, "numberOfGlyphs", lm_num_glyphs); ADD(lm, "glyphRangeForCharacterRange:actualCharacterRange:", lm_glyph_range_for_chars);
    ADD(lm, "characterRangeForGlyphRange:actualGlyphRange:", lm_char_range_for_glyphs); ADD(lm, "characterIndexForGlyphAtIndex:", lm_char_for_glyph);
    ADD(lm, "glyphIndexForCharacterAtIndex:", lm_glyph_for_char); ADD(lm, "glyphAtIndex:", lm_glyph_at); ADD(lm, "CGGlyphAtIndex:", lm_glyph_at);
    ADD(lm, "isValidGlyphIndex:", lm_valid_glyph);
    ADD(lm, "usedRectForTextContainer:", lm_used_rect); ADD(lm, "glyphRangeForTextContainer:", lm_glyph_range_tc);
    ADD(lm, "textContainerForGlyphAtIndex:effectiveRange:", lm_tc_for_glyph);
    ADD(lm, "textContainerForGlyphAtIndex:effectiveRange:withoutAdditionalLayout:", lm_tc_for_glyph3);
    ADD(lm, "lineFragmentRectForGlyphAtIndex:effectiveRange:", lm_frag_rect); ADD(lm, "lineFragmentRectForGlyphAtIndex:effectiveRange:withoutAdditionalLayout:", lm_frag_rect3);
    ADD(lm, "lineFragmentUsedRectForGlyphAtIndex:effectiveRange:", lm_frag_used); ADD(lm, "lineFragmentUsedRectForGlyphAtIndex:effectiveRange:withoutAdditionalLayout:", lm_frag_used3);
    ADD(lm, "locationForGlyphAtIndex:", lm_location);
    ADD(lm, "extraLineFragmentRect", lm_extra); ADD(lm, "extraLineFragmentUsedRect", lm_extra_used); ADD(lm, "extraLineFragmentTextContainer", lm_extra_tc);
    ADD(lm, "boundingRectForGlyphRange:inTextContainer:", lm_bounding); ADD(lm, "glyphRangeForBoundingRect:inTextContainer:", lm_range_for_rect);
    ADD(lm, "glyphRangeForBoundingRectWithoutAdditionalLayout:inTextContainer:", lm_range_for_rect);
    ADD(lm, "characterIndexForPoint:inTextContainer:fractionOfDistanceBetweenInsertionPoints:", lm_char_for_point4);
    ADD(lm, "glyphIndexForPoint:inTextContainer:", lm_glyph_for_point); ADD(lm, "glyphIndexForPoint:inTextContainer:fractionOfDistanceThroughGlyph:", lm_glyph_for_point3);
    ADD(lm, "enumerateLineFragmentsForGlyphRange:usingBlock:", lm_enum_lines);
    ADD(lm, "enumerateEnclosingRectsForGlyphRange:withinSelectedGlyphRange:inTextContainer:usingBlock:", lm_enum_rects);
    ADD(lm, "rectArrayForGlyphRange:withinSelectedGlyphRange:inTextContainer:rectCount:", lm_rect_array);
    ADD(lm, "rectArrayForCharacterRange:withinSelectedCharacterRange:inTextContainer:rectCount:", lm_rect_array);
    ADD(lm, "ensureLayoutForTextContainer:", lm_ensure); ADD(lm, "ensureGlyphsForGlyphRange:", lm_ensure_r);
    ADD(lm, "ensureLayoutForGlyphRange:", lm_ensure_r); ADD(lm, "ensureLayoutForCharacterRange:", lm_ensure_r);
    ADD(lm, "ensureLayoutForBoundingRect:inTextContainer:", lm_ensure_r); ADD(lm, "ensureGlyphsForCharacterRange:", lm_ensure_r);
    ADD(lm, "invalidateLayoutForCharacterRange:actualCharacterRange:", lm_invalidate_layout);
    ADD(lm, "invalidateGlyphsForCharacterRange:changeInLength:actualCharacterRange:", lm_invalidate_glyphs);
    ADD(lm, "invalidateDisplayForCharacterRange:", lm_invalidate_display); ADD(lm, "invalidateDisplayForGlyphRange:", lm_invalidate_display);
    ADD(lm, "processEditingForTextStorage:edited:range:changeInLength:invalidatedRange:", lm_process_editing);
    ADD(lm, "allowsNonContiguousLayout", lm_g_nonContig); ADD(lm, "setAllowsNonContiguousLayout:", lm_s_nonContig);
    ADD(lm, "hasNonContiguousLayout", lm_has_noncontig);
    ADD(lm, "usesFontLeading", lm_g_leading); ADD(lm, "setUsesFontLeading:", lm_s_leading);
    ADD(lm, "showsInvisibleCharacters", lm_g_invisibles); ADD(lm, "setShowsInvisibleCharacters:", lm_s_invisibles);
    ADD(lm, "showsControlCharacters", lm_g_controls); ADD(lm, "setShowsControlCharacters:", lm_s_controls);
    ADD(lm, "backgroundLayoutEnabled", lm_g_background); ADD(lm, "setBackgroundLayoutEnabled:", lm_s_background);
    ADD(lm, "hyphenationFactor", lm_g_hyphen); ADD(lm, "setUsesDefaultHyphenation:", lm_s_hyphen);
    ADD(lm, "addTemporaryAttribute:value:forCharacterRange:", lm_add_temp); ADD(lm, "removeTemporaryAttribute:forCharacterRange:", lm_remove_temp);
    ADD(lm, "setTemporaryAttributes:forCharacterRange:", lm_set_temps); ADD(lm, "addTemporaryAttributes:forCharacterRange:", lm_add_temps);
    ADD(lm, "temporaryAttributesAtCharacterIndex:effectiveRange:", lm_temps_at); ADD(lm, "temporaryAttribute:atCharacterIndex:effectiveRange:", lm_temp_attr);
    ADD(lm, "drawBackgroundForGlyphRange:atPoint:", lm_draw_bg); ADD(lm, "drawGlyphsForGlyphRange:atPoint:", lm_draw_glyphs);
    ADD(lm, "drawUnderlineForGlyphRange:underlineType:baselineOffset:lineFragmentRect:lineFragmentGlyphRange:containerOrigin:", lm_underline);
    ADD(lm, "fillBackgroundRectArray:count:forCharacterRange:color:", lm_fill_bg_rects);
    (void)k_weak3;
}

/*
 * mOS libSystem — NSTextView sobre o TextKit 1 (appkit_textkit.c): NSTextStorage → NSLayoutManager → NSTextContainer.
 *   Texto rico (atributos por trecho, typingAttributes), seleção por clique, duplo clique (palavra), triplo (parágrafo),
 *   shift e arrasto (laço do nextEventMatchingMask:), teclado pelo interpretKeyEvents: (moveLeft:, moveWordRight:...,
 *   deleteBackward:, insertNewline:...), desfazer pelo NSUndoManager, copiar e colar (rico e simples), NSTextInputClient
 *   (texto marcado do método de entrada), delegate e notificações do NSText/NSTextView, redimensionamento automático,
 *   comandos de fonte, cor e alinhamento, Serviços (writeSelectionToPasteboard:/readSelectionFromPasteboard:) e
 *   acessibilidade (AXTextArea). Antes o NSTextView guardava só uma string C e devolvia a si mesmo como textStorage.
 * Registro: cx_appkit_textview_init(), chamado do cx_appkit_input_init (depois do TextKit).
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
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id cx_color(double r, double g, double b, double a);
void NSBeep(void);
extern id NSApp;
extern id NSFontAttributeName, NSForegroundColorAttributeName, NSBackgroundColorAttributeName, NSParagraphStyleAttributeName;
extern id NSUnderlineStyleAttributeName;
extern id NSPasteboardTypeString;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RNG;
void NSRectFill(R r);
#define NOTFOUND ((u64)0x7fffffffffffffffULL)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }

static char k_props, k_delegate;
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

static id storage(id tv) { return pget(tv, "storage"); }
static id lm(id tv) { return pget(tv, "lm"); }
static id tcont(id tv) { return pget(tv, "tc"); }
static u64 text_len(id tv) { id s = storage(tv); return s ? MSG0(u64, s, "length") : 0; }
static id text_str(id tv) { id s = storage(tv); return s ? MSG0(id, s, "string") : cx_str(""); }
static unsigned short ch_at(id tv, u64 i) { return MSG(unsigned short, text_str(tv), "characterAtIndex:", i); }
static RNG sel(id tv) { return (RNG){(u64)pgi(tv, "sloc", 0), (u64)pgi(tv, "slen", 0)}; }
static S inset(id tv) { return (S){pgd(tv, "insetW", 0), pgd(tv, "insetH", 0)}; }
static void redraw(id tv) { MSG(void, tv, "setNeedsDisplay:", (BOOL)YES); }
static id default_font(void) { return MSG(id, (id)kls("NSFont"), "userFontOfSize:", 12.0); }

/* ================================================================ montagem */
static id typing_attrs(id self, SEL c);
static void tv_size_to_text(id self);
static void build(id self, id tc) {
    if (!tc) {
        R b = msg_rect(self, "bounds");
        tc = objc_autorelease(((id (*)(id, SEL, S))objc_msgSend)(MSG0(id, (id)kls("NSTextContainer"), "alloc"), sl("initWithSize:"), (S){b.w > 0 ? b.w : 1e7, 1e7}));
        MSG(void, tc, "setWidthTracksTextView:", (BOOL)YES);
    }
    id ts = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSTextStorage"), "alloc"), "init"));
    id l = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSLayoutManager"), "alloc"), "init"));
    MSG(void, l, "addTextContainer:", tc);
    MSG(void, ts, "addLayoutManager:", l);
    MSG(void, tc, "setTextView:", self);
    pset(self, "storage", ts); pset(self, "lm", l); pset(self, "tc", tc);
    psi(self, "editable", 1); psi(self, "selectable", 1); psi(self, "rich", 1); psi(self, "drawsBG", 1); psi(self, "undo", 0);
    psi(self, "vres", 1); psi(self, "hres", 0); psd(self, "minW", 0); psd(self, "minH", 0); psd(self, "maxW", 1e7); psd(self, "maxH", 1e7);
    psi(self, "mloc", (i64)NOTFOUND);
}
static IMP view_init_frame;
static id tv_init_frame_tc(id self, SEL c, R f, id tc) {
    (void)c;
    self = ((id (*)(id, SEL, R))view_init_frame)(self, sl("initWithFrame:"), f);
    if (!self) return 0;
    build(self, tc);
    return self;
}
static id tv_init_frame(id self, SEL c, R f) { return tv_init_frame_tc(self, c, f, 0); }
static id tv_init_coder(id self, SEL c, id coder) { (void)coder; return tv_init_frame(self, c, mkr(0, 0, 100, 100)); }
static id tv_storage(id self, SEL c) { (void)c; return storage(self); }
static id tv_lm(id self, SEL c) { (void)c; return lm(self); }
static id tv_tc(id self, SEL c) { (void)c; return tcont(self); }
static void tv_set_tc(id self, SEL c, id tc) {
    (void)c;
    id old = tcont(self);
    if (old) MSG(void, old, "setTextView:", (id)0);
    pset(self, "tc", tc);
    if (tc) { MSG(void, tc, "setTextView:", self); id l = MSG0(id, tc, "layoutManager"); if (l) pset(self, "lm", l); }
    redraw(self);
}
static void tv_replace_tc(id self, SEL c, id tc) {
    id l = lm(self);
    if (l && tc) { MSG(void, l, "removeTextContainerAtIndex:", (u64)0); MSG(void, l, "insertTextContainer:atIndex:", tc, (u64)0); }
    tv_set_tc(self, c, tc);
}
static S tv_inset(id self, SEL c) { (void)c; return inset(self); }
static void tv_set_inset(id self, SEL c, S z) { (void)c; psd(self, "insetW", z.w); psd(self, "insetH", z.h); tv_size_to_text(self); redraw(self); }
static P tv_origin(id self, SEL c) { (void)c; S i = inset(self); return (P){i.w, i.h}; }
static BOOL yes_imp(id self, SEL c) { (void)self; (void)c; return YES; }

/* ================================================================ delegate e notificações */
static id tv_delegate(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_delegate); }
static void tv_set_delegate(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_delegate, d, 0); }
static id note(id self, const char *name) { return MSG(id, (id)kls("NSNotification"), "notificationWithName:object:", cx_str(name), self); }
static void post(id self, const char *name, const char *dsel) {
    id n = note(self, name);
    id d = tv_delegate(self, 0);
    if (d && dsel && responds(d, dsel)) MSG(void, d, dsel, n);
    MSG(void, MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter"), "postNotification:", n);
}

/* ================================================================ seleção */
static void set_sel(id self, RNG r, int notify) {
    u64 n = text_len(self);
    if (r.loc > n) r.loc = n;
    if (r.loc + r.len > n) r.len = n - r.loc;
    RNG old = sel(self);
    id d = tv_delegate(self, 0);
    if (notify && d && responds(d, "textView:willChangeSelectionFromCharacterRange:toCharacterRange:"))
        r = MSG(RNG, d, "textView:willChangeSelectionFromCharacterRange:toCharacterRange:", self, old, r);
    psi(self, "sloc", (i64)r.loc); psi(self, "slen", (i64)r.len);
    if (r.len == 0) {   /* cursor: os atributos de digitação vêm do caractere anterior */
        id ts = storage(self);
        if (n && ts) {
            u64 at = r.loc ? r.loc - 1 : 0;
            id a = MSG(id, ts, "attributesAtIndex:effectiveRange:", at, (RNG *)0);
            if (a && MSG0(u64, a, "count")) pset(self, "typing", a);
        }
    }
    redraw(self);
    if (notify && (old.loc != r.loc || old.len != r.len)) post(self, "NSTextViewDidChangeSelectionNotification", "textViewDidChangeSelection:");
}
static RNG tv_sel_range(id self, SEL c) { (void)c; return sel(self); }
static void tv_set_sel_range(id self, SEL c, RNG r) { (void)c; psi(self, "anchor", (i64)r.loc); set_sel(self, r, 1); }
static void tv_set_sel_affinity(id self, SEL c, RNG r, u64 aff, BOOL still) { (void)aff; (void)still; tv_set_sel_range(self, c, r); }
static id tv_sel_ranges(id self, SEL c) { (void)c; id v = MSG(id, (id)kls("NSValue"), "valueWithRange:", sel(self)); return MSG(id, (id)kls("NSArray"), "arrayWithObject:", v); }
static void tv_set_sel_ranges(id self, SEL c, id a) {
    (void)c;
    if (!a || !MSG0(u64, a, "count")) return;
    RNG r = MSG0(RNG, MSG(id, a, "objectAtIndex:", (u64)0), "rangeValue");
    set_sel(self, r, 1);
}
static u64 tv_affinity(id self, SEL c) { (void)self; (void)c; return 0; }
static u64 tv_granularity(id self, SEL c) { (void)c; return (u64)pgi(self, "gran", 0); }
static void tv_set_granularity(id self, SEL c, u64 g) { (void)c; psi(self, "gran", (i64)g); }

/* ================================================================ edição */
static id undo_manager(id self) {
    id d = tv_delegate(self, 0);
    if (d && responds(d, "undoManagerForTextView:")) { id u = MSG(id, d, "undoManagerForTextView:", self); if (u) return u; }
    id w = MSG0(id, self, "window");
    id u = w && responds(w, "undoManager") ? MSG0(id, w, "undoManager") : 0;
    if (!u) {
        u = pget(self, "ownUndo");
        if (!u) { u = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSUndoManager"), "alloc"), "init")); pset(self, "ownUndo", u); }
    }
    return u;
}
static id tv_undo_manager(id self, SEL c) { (void)c; return pgi(self, "undo", 0) ? undo_manager(self) : 0; }
static BOOL should_change(id self, RNG r, id str) {
    if (!pgi(self, "editable", 1)) return NO;
    id d = tv_delegate(self, 0);
    if (d && responds(d, "textView:shouldChangeTextInRange:replacementString:") && !MSG(BOOL, d, "textView:shouldChangeTextInRange:replacementString:", self, r, str)) return NO;
    if (!pgi(self, "began", 0)) {
        id dd = tv_delegate(self, 0);
        if (dd && responds(dd, "textShouldBeginEditing:") && !MSG(BOOL, dd, "textShouldBeginEditing:", self)) return NO;
        psi(self, "began", 1);
        post(self, "NSTextDidBeginEditingNotification", "textDidBeginEditing:");
    }
    return YES;
}
static void did_change(id self) {
    tv_size_to_text(self);
    post(self, "NSTextDidChangeNotification", "textDidChange:");
}
/* trocar [r] por astr (atributos dela) com desfazer */
static void undo_replace(id self, SEL c, id info);
static void replace_with(id self, RNG r, id astr, int undoable, const char *action) {
    id ts = storage(self);
    if (!ts) return;
    if (undoable && pgi(self, "undo", 0)) {
        id u = undo_manager(self);
        id oldtxt = MSG(id, ts, "attributedSubstringFromRange:", r);
        id info = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
        u64 nl = astr ? MSG0(u64, astr, "length") : 0;
        id rv = MSG(id, (id)kls("NSValue"), "valueWithRange:", ((RNG){r.loc, nl}));
        MSG(void, info, "setObject:forKey:", rv, cx_str("range"));
        MSG(void, info, "setObject:forKey:", oldtxt, cx_str("text"));
        MSG(void, u, "registerUndoWithTarget:selector:object:", self, sl("_cxUndoReplace:"), info);
        if (action) MSG(void, u, "setActionName:", cx_str(action));
    }
    MSG0(void, ts, "beginEditing");
    if (astr) MSG(void, ts, "replaceCharactersInRange:withAttributedString:", r, astr);
    else MSG(void, ts, "deleteCharactersInRange:", r);
    MSG0(void, ts, "endEditing");
}
static void undo_replace(id self, SEL c, id info) {
    (void)c;
    RNG r = MSG0(RNG, MSG(id, info, "objectForKey:", cx_str("range")), "rangeValue");
    id txt = MSG(id, info, "objectForKey:", cx_str("text"));
    replace_with(self, r, txt, 1, 0);
    set_sel(self, (RNG){r.loc, txt ? MSG0(u64, txt, "length") : 0}, 1);
    did_change(self);
}
static id attributed(id self, id str) {
    if (!str) return 0;
    if (MSG(BOOL, str, "isKindOfClass:", (id)kls("NSAttributedString"))) {
        if (pgi(self, "rich", 1)) return str;
        str = MSG0(id, str, "string");
    }
    return objc_autorelease(MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:attributes:", str, typing_attrs(self, 0)));
}
/* inserir como digitação: na seleção (ou em replacementRange), com shouldChange, desfazer e notificação */
static int edit(id self, RNG r, id str, const char *action) {
    id a = attributed(self, str);
    id plain = a ? MSG0(id, a, "string") : cx_str("");
    if (!should_change(self, r, plain)) { NSBeep(); return 0; }
    replace_with(self, r, a, 1, action);
    u64 nl = a ? MSG0(u64, a, "length") : 0;
    set_sel(self, (RNG){r.loc + nl, 0}, 1);
    psi(self, "anchor", (i64)(r.loc + nl));
    did_change(self);
    MSG(void, self, "scrollRangeToVisible:", sel(self));
    return 1;
}
static void tv_insert_text_r(id self, SEL c, id str, RNG rr) {
    (void)c;
    RNG r = rr.loc == NOTFOUND ? sel(self) : rr;
    u64 mloc = (u64)pgi(self, "mloc", (i64)NOTFOUND);
    if (mloc != NOTFOUND && rr.loc == NOTFOUND) r = (RNG){mloc, (u64)pgi(self, "mlen", 0)};   /* o texto marcado vira o definitivo */
    psi(self, "mloc", (i64)NOTFOUND); psi(self, "mlen", 0);
    edit(self, r, str, "Digitação");
}
static void tv_insert_text(id self, SEL c, id str) { tv_insert_text_r(self, c, str, (RNG){NOTFOUND, 0}); }
static void tv_replace_chars(id self, SEL c, RNG r, id str) {
    (void)c;
    replace_with(self, r, attributed(self, str ? str : cx_str("")), 0, 0);
    did_change(self);
}
static BOOL tv_should_change(id self, SEL c, RNG r, id str) { (void)c; return should_change(self, r, str); }
static void tv_did_change(id self, SEL c) { (void)c; did_change(self); }

/* ================================================================ conteúdo e atributos */
static id tv_string(id self, SEL c) { (void)c; return MSG0(id, text_str(self), "copy"); }
static void tv_set_string(id self, SEL c, id s) {
    (void)c;
    id ts = storage(self);
    if (!ts) return;
    u64 n = text_len(self);
    id a = objc_autorelease(MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:attributes:", s ? s : cx_str(""), typing_attrs(self, 0)));
    MSG0(void, ts, "beginEditing");
    MSG(void, ts, "replaceCharactersInRange:withAttributedString:", ((RNG){0, n}), a);
    MSG0(void, ts, "endEditing");
    set_sel(self, (RNG){0, 0}, 0);
    tv_size_to_text(self);
}
static id tv_attributed_string(id self, SEL c) { (void)c; return objc_autorelease(MSG0(id, storage(self), "copy")); }
static void tv_set_attributed(id self, SEL c, id a) {
    (void)c;
    id ts = storage(self);
    if (!ts) return;
    MSG(void, ts, "setAttributedString:", a ? a : MSG0(id, (id)kls("NSAttributedString"), "new"));
    set_sel(self, (RNG){0, 0}, 0);
    tv_size_to_text(self);
}
static void tv_append_attributed(id self, SEL c, id a) { (void)c; id ts = storage(self); if (ts && a) MSG(void, ts, "appendAttributedString:", a); tv_size_to_text(self); }
static id typing_attrs(id self, SEL c) {
    (void)c;
    id t = pget(self, "typing");
    if (t && MSG0(u64, t, "count")) return t;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    id f = pget(self, "font");
    MSG(void, d, "setObject:forKey:", f ? f : default_font(), NSFontAttributeName);
    id col = pget(self, "textColor");
    MSG(void, d, "setObject:forKey:", col ? col : cx_color(0, 0, 0, 1), NSForegroundColorAttributeName);
    id ps = pget(self, "paraStyle");
    if (ps) MSG(void, d, "setObject:forKey:", ps, NSParagraphStyleAttributeName);
    return d;
}
static void tv_set_typing(id self, SEL c, id a) { (void)c; pset(self, "typing", a ? MSG0(id, a, "copy") : 0); if (a) objc_release(pget(self, "typing")); }
static void set_attr_all(id self, id key, id val) {
    id ts = storage(self);
    u64 n = text_len(self);
    if (ts && n && val) MSG(void, ts, "addAttribute:value:range:", key, val, ((RNG){0, n}));
    id t = MSG0(id, typing_attrs(self, 0), "mutableCopy");
    if (val) MSG(void, t, "setObject:forKey:", val, key);
    pset(self, "typing", t);
    objc_release(t);
    tv_size_to_text(self);
    redraw(self);
}
static id tv_font(id self, SEL c) { (void)c; id t = typing_attrs(self, 0); return MSG(id, t, "objectForKey:", NSFontAttributeName); }
static void tv_set_font(id self, SEL c, id f) { (void)c; pset(self, "font", f); set_attr_all(self, NSFontAttributeName, f); }
static void tv_set_font_range(id self, SEL c, id f, RNG r) { (void)c; id ts = storage(self); if (ts && f && r.len) MSG(void, ts, "addAttribute:value:range:", NSFontAttributeName, f, r); }
static id tv_text_color(id self, SEL c) { (void)c; return MSG(id, typing_attrs(self, 0), "objectForKey:", NSForegroundColorAttributeName); }
static void tv_set_text_color(id self, SEL c, id col) { (void)c; pset(self, "textColor", col); set_attr_all(self, NSForegroundColorAttributeName, col); }
static void tv_set_text_color_range(id self, SEL c, id col, RNG r) { (void)c; id ts = storage(self); if (ts && r.len) { if (col) MSG(void, ts, "addAttribute:value:range:", NSForegroundColorAttributeName, col, r); else MSG(void, ts, "removeAttribute:range:", NSForegroundColorAttributeName, r); } }
static id para_with_alignment(id self, i64 al) {
    id ps = MSG(id, typing_attrs(self, 0), "objectForKey:", NSParagraphStyleAttributeName);
    id m = ps ? MSG0(id, ps, "mutableCopy") : MSG0(id, (id)kls("NSMutableParagraphStyle"), "new");
    MSG(void, m, "setAlignment:", al);
    return objc_autorelease(m);
}
static i64 tv_alignment(id self, SEL c) { (void)c; id ps = MSG(id, typing_attrs(self, 0), "objectForKey:", NSParagraphStyleAttributeName); return ps ? MSG0(i64, ps, "alignment") : 4; }
static void tv_set_alignment(id self, SEL c, i64 al) { (void)c; id ps = para_with_alignment(self, al); pset(self, "paraStyle", ps); set_attr_all(self, NSParagraphStyleAttributeName, ps); }
/* faixa de parágrafos da seleção */
static RNG para_range(id self, RNG r) { return MSG(RNG, text_str(self), "paragraphRangeForRange:", r); }
static void tv_set_alignment_range(id self, SEL c, i64 al, RNG r) {
    (void)c;
    id ts = storage(self);
    if (!ts) return;
    RNG pr = para_range(self, r);
    if (pr.len) MSG(void, ts, "addAttribute:value:range:", NSParagraphStyleAttributeName, para_with_alignment(self, al), pr);
    tv_size_to_text(self);
}
static void align_cmd(id self, i64 al) {
    RNG r = sel(self);
    if (!pgi(self, "rich", 1) || !text_len(self)) { tv_set_alignment(self, 0, al); return; }
    if (!should_change(self, para_range(self, r), 0)) return;
    tv_set_alignment_range(self, 0, al, r);
    did_change(self);
}
static void tv_align_left(id self, SEL c, id s) { (void)c; (void)s; align_cmd(self, 0); }
static void tv_align_right(id self, SEL c, id s) { (void)c; (void)s; align_cmd(self, 1); }
static void tv_align_center(id self, SEL c, id s) { (void)c; (void)s; align_cmd(self, 2); }
static void tv_align_justified(id self, SEL c, id s) { (void)c; (void)s; align_cmd(self, 3); }
static void tv_align_natural(id self, SEL c, id s) { (void)c; (void)s; align_cmd(self, 4); }
static id tv_default_para(id self, SEL c) { (void)c; return pget(self, "paraStyle"); }
static void tv_set_default_para(id self, SEL c, id ps) { (void)c; pset(self, "paraStyle", ps); }
/* comandos de fonte e cor (painel de fontes, menu Formatar) */
static void tv_change_font(id self, SEL c, id sender) {
    (void)c;
    id fm = sender ? sender : MSG0(id, (id)kls("NSFontManager"), "sharedFontManager");
    RNG r = sel(self);
    id ts = storage(self);
    if (!r.len || !pgi(self, "rich", 1)) {
        id f = MSG(id, fm, "convertFont:", tv_font(self, 0));
        if (!pgi(self, "rich", 1)) { tv_set_font(self, 0, f); return; }
        id t = MSG0(id, typing_attrs(self, 0), "mutableCopy");
        MSG(void, t, "setObject:forKey:", f, NSFontAttributeName);
        pset(self, "typing", t); objc_release(t);
        return;
    }
    if (!should_change(self, r, 0)) return;
    MSG0(void, ts, "beginEditing");
    u64 i = r.loc;
    while (i < r.loc + r.len) {
        RNG eff;
        id f = MSG(id, ts, "attribute:atIndex:effectiveRange:", NSFontAttributeName, i, &eff);
        u64 e = eff.loc + eff.len < r.loc + r.len ? eff.loc + eff.len : r.loc + r.len;
        if (e <= i) e = i + 1;
        id nf = MSG(id, fm, "convertFont:", f ? f : default_font());
        if (nf) MSG(void, ts, "addAttribute:value:range:", NSFontAttributeName, nf, ((RNG){i, e - i}));
        i = e;
    }
    MSG0(void, ts, "endEditing");
    did_change(self);
}
static void tv_change_color(id self, SEL c, id sender) {
    (void)c;
    id col = sender && responds(sender, "color") ? MSG0(id, sender, "color") : 0;
    if (!col) return;
    RNG r = sel(self);
    if (!r.len || !pgi(self, "rich", 1)) { tv_set_text_color(self, 0, col); return; }
    if (!should_change(self, r, 0)) return;
    tv_set_text_color_range(self, 0, col, r);
    did_change(self);
}
static void tv_underline(id self, SEL c, id sender) {
    (void)c; (void)sender;
    RNG r = sel(self);
    id ts = storage(self);
    if (!r.len) {
        id t = MSG0(id, typing_attrs(self, 0), "mutableCopy");
        id u = MSG(id, t, "objectForKey:", NSUnderlineStyleAttributeName);
        MSG(void, t, "setObject:forKey:", cx_num_i(u && MSG0(i64, u, "integerValue") ? 0 : 1), NSUnderlineStyleAttributeName);
        pset(self, "typing", t); objc_release(t);
        return;
    }
    if (!should_change(self, r, 0)) return;
    id u = MSG(id, ts, "attribute:atIndex:effectiveRange:", NSUnderlineStyleAttributeName, r.loc, (RNG *)0);
    MSG(void, ts, "addAttribute:value:range:", NSUnderlineStyleAttributeName, cx_num_i(u && MSG0(i64, u, "integerValue") ? 0 : 1), r);
    did_change(self);
}

/* ================================================================ flags */
#define FLAG(get, set, key, def) \
    static BOOL tvg_##get(id self, SEL c) { (void)c; return pgi(self, key, def) != 0; } \
    static void tvs_##get(id self, SEL c, BOOL b) { (void)c; psi(self, key, b); redraw(self); }
FLAG(editable, setEditable, "editable", 1) FLAG(selectable, setSelectable, "selectable", 1) FLAG(rich, setRichText, "rich", 1)
FLAG(drawsBG, setDrawsBackground, "drawsBG", 1) FLAG(undo, setAllowsUndo, "undo", 0) FLAG(field, setFieldEditor, "field", 0)
FLAG(graphics, setImportsGraphics, "graphics", 0) FLAG(hres, setHorizontallyResizable, "hres", 0) FLAG(vres, setVerticallyResizable, "vres", 1)
FLAG(findBar, setUsesFindBar, "findBar", 0) FLAG(findPanel, setUsesFindPanel, "findPanel", 0) FLAG(ruler, setUsesRuler, "ruler", 0)
FLAG(rulerVis, setRulerVisible, "rulerVis", 0) FLAG(spell, setContinuousSpellCheckingEnabled, "spell", 0) FLAG(grammar, setGrammarCheckingEnabled, "grammar", 0)
FLAG(quotes, setAutomaticQuoteSubstitutionEnabled, "quotes", 0) FLAG(dashes, setAutomaticDashSubstitutionEnabled, "dashes", 0)
FLAG(links, setAutomaticLinkDetectionEnabled, "links", 0) FLAG(textRepl, setAutomaticTextReplacementEnabled, "textRepl", 0)
FLAG(spellCorr, setAutomaticSpellingCorrectionEnabled, "spellCorr", 0) FLAG(dataDet, setAutomaticDataDetectionEnabled, "dataDet", 0)
FLAG(smartInsert, setSmartInsertDeleteEnabled, "smartInsert", 1) FLAG(fontPanel, setUsesFontPanel, "fontPanel", 1)
FLAG(inspector, setUsesInspectorBar, "inspector", 0) FLAG(docAttrs, setAllowsDocumentBackgroundColorChange, "docBG", 0)
FLAG(imageEdit, setAllowsImageEditing, "imageEdit", 0) FLAG(incremental, setDisplaysLinkToolTips, "linkTips", 1)
static id tv_bg(id self, SEL c) { (void)c; id b = pget(self, "bg"); return b ? b : cx_color(1, 1, 1, 1); }
static void tv_set_bg(id self, SEL c, id col) { (void)c; pset(self, "bg", col); redraw(self); }
static id tv_ins_color(id self, SEL c) { (void)c; id v = pget(self, "insColor"); return v ? v : cx_color(0, 0, 0, 1); }
static void tv_set_ins_color(id self, SEL c, id col) { (void)c; pset(self, "insColor", col); redraw(self); }
static id tv_sel_attrs(id self, SEL c) {
    (void)c;
    id v = pget(self, "selAttrs");
    if (v) return v;
    return MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", cx_color(0.70, 0.84, 1.0, 1), NSBackgroundColorAttributeName);
}
static void tv_set_sel_attrs(id self, SEL c, id a) { (void)c; pset(self, "selAttrs", a); redraw(self); }
static id tv_link_attrs(id self, SEL c) { (void)c; return pget(self, "linkAttrs"); }
static void tv_set_link_attrs(id self, SEL c, id a) { (void)c; pset(self, "linkAttrs", a); }
static id tv_marked_attrs(id self, SEL c) { (void)c; id v = pget(self, "markAttrs"); return v ? v : MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", cx_num_i(1), NSUnderlineStyleAttributeName); }
static void tv_set_marked_attrs(id self, SEL c, id a) { (void)c; pset(self, "markAttrs", a); }
static S tv_min(id self, SEL c) { (void)c; return (S){pgd(self, "minW", 0), pgd(self, "minH", 0)}; }
static void tv_set_min(id self, SEL c, S z) { (void)c; psd(self, "minW", z.w); psd(self, "minH", z.h); }
static S tv_max(id self, SEL c) { (void)c; return (S){pgd(self, "maxW", 1e7), pgd(self, "maxH", 1e7)}; }
static void tv_set_max(id self, SEL c, S z) { (void)c; psd(self, "maxW", z.w); psd(self, "maxH", z.h); }

/* ================================================================ tamanho */
static void tv_size_to_text(id self) {
    id l = lm(self), tc = tcont(self);
    if (!l || !tc) return;
    R b = msg_rect(self, "frame");
    S in = inset(self);
    if (MSG0(BOOL, tc, "widthTracksTextView")) {
        S cs = ((S (*)(id, SEL))objc_msgSend)(tc, sl("size"));
        double want = b.w - 2 * in.w;
        if (want > 0 && cs.w != want) ((void (*)(id, SEL, S))objc_msgSend)(tc, sl("setSize:"), (S){want, cs.h});
    }
    if (!pgi(self, "vres", 1) && !pgi(self, "hres", 0)) { redraw(self); return; }
    R u = ((R (*)(id, SEL, id))objc_msgSend_stret)(l, sl("usedRectForTextContainer:"), tc);
    double h = u.y + u.h + 2 * in.h, w = b.w;
    if (pgi(self, "hres", 0)) w = u.x + u.w + 2 * in.w;
    /* dentro de rolagem: pelo menos a altura visível (o clique embaixo do texto cai na view) */
    id sv = MSG0(id, self, "superview");
    if (sv && responds(sv, "documentVisibleRect")) { R vr = msg_rect(sv, "bounds"); if (h < vr.h) h = vr.h; }
    if (h < pgd(self, "minH", 0)) h = pgd(self, "minH", 0);
    if (h > pgd(self, "maxH", 1e7)) h = pgd(self, "maxH", 1e7);
    if (w < pgd(self, "minW", 0)) w = pgd(self, "minW", 0);
    if (w > pgd(self, "maxW", 1e7)) w = pgd(self, "maxW", 1e7);
    if (h != b.h || w != b.w) ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setFrameSize:"), (S){w, h});
    redraw(self);
}
static void tv_size_to_fit(id self, SEL c) { (void)c; tv_size_to_text(self); }
static IMP view_set_frame_size;
static void tv_set_frame_size(id self, SEL c, S z) {
    ((void (*)(id, SEL, S))view_set_frame_size)(self, c, z);
    id tc = tcont(self);
    if (tc && MSG0(BOOL, tc, "widthTracksTextView")) {
        S in = inset(self);
        S cs = ((S (*)(id, SEL))objc_msgSend)(tc, sl("size"));
        if (cs.w != z.w - 2 * in.w) ((void (*)(id, SEL, S))objc_msgSend)(tc, sl("setSize:"), (S){z.w - 2 * in.w, cs.h});
    }
}
/* o storage mudou por fora (app editou o textStorage): seleção dentro do texto e tamanho */
static void tv_storage_edited(id self, SEL c, RNG r, i64 delta) {
    (void)c; (void)r; (void)delta;
    RNG s = sel(self);
    u64 n = text_len(self);
    if (s.loc > n || s.loc + s.len > n) set_sel(self, (RNG){n, 0}, 0);
    tv_size_to_text(self);
}

/* ================================================================ desenho */
static BOOL is_first(id self) { id w = MSG0(id, self, "window"); return w && MSG0(id, w, "firstResponder") == self; }
static BOOL tv_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static R caret_rect(id self) {
    id l = lm(self), tc = tcont(self);
    S in = inset(self);
    RNG s = sel(self);
    u64 n = text_len(self);
    if (s.loc >= n && n == 0) {
        R e = ((R (*)(id, SEL))objc_msgSend_stret)(l, sl("extraLineFragmentUsedRect"));
        if (e.h <= 0) e = mkr(5, 0, 1, 15);
        return mkr(in.w + e.x, in.h + e.y, 1, e.h);
    }
    R b = ((R (*)(id, SEL, RNG, id))objc_msgSend_stret)(l, sl("boundingRectForGlyphRange:inTextContainer:"), ((RNG){s.loc, 0}), tc);
    if (b.h <= 0 && s.loc >= n) {
        R e = ((R (*)(id, SEL))objc_msgSend_stret)(l, sl("extraLineFragmentUsedRect"));
        if (e.h > 0) return mkr(in.w + e.x, in.h + e.y, 1, e.h);
        R lb = ((R (*)(id, SEL, RNG, id))objc_msgSend_stret)(l, sl("boundingRectForGlyphRange:inTextContainer:"), ((RNG){n - 1, 1}), tc);
        return mkr(in.w + lb.x + lb.w, in.h + lb.y, 1, lb.h);
    }
    return mkr(in.w + b.x, in.h + b.y, 1, b.h);
}
static void tv_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    R b = msg_rect(self, "bounds");
    if (pgi(self, "drawsBG", 1)) { MSG0(void, tv_bg(self, 0), "setFill"); NSRectFill(b); }
    id l = lm(self), tc = tcont(self);
    if (!l || !tc) return;
    S in = inset(self);
    P o = {in.w, in.h};
    RNG all = ((RNG (*)(id, SEL, id))objc_msgSend)(l, sl("glyphRangeForTextContainer:"), tc);
    ((void (*)(id, SEL, RNG, P))objc_msgSend)(l, sl("drawBackgroundForGlyphRange:atPoint:"), all, o);
    RNG s = sel(self);
    if (s.len && pgi(self, "selectable", 1)) {
        id sa = tv_sel_attrs(self, 0);
        id col = MSG(id, sa, "objectForKey:", NSBackgroundColorAttributeName);
        if (!is_first(self)) col = cx_color(0.86, 0.86, 0.86, 1);
        MSG0(void, col ? col : cx_color(0.70, 0.84, 1.0, 1), "setFill");
        u64 cnt = 0;
        R *rs = ((R *(*)(id, SEL, RNG, RNG, id, u64 *))objc_msgSend)(l, sl("rectArrayForCharacterRange:withinSelectedCharacterRange:inTextContainer:rectCount:"), s, s, tc, &cnt);
        for (u64 i = 0; i < cnt; i++) NSRectFill(mkr(rs[i].x + o.x, rs[i].y + o.y, rs[i].w, rs[i].h));
    }
    ((void (*)(id, SEL, RNG, P))objc_msgSend)(l, sl("drawGlyphsForGlyphRange:atPoint:"), all, o);
    if (!s.len && is_first(self) && pgi(self, "editable", 1)) {
        R cr = caret_rect(self);
        if (responds(self, "drawInsertionPointInRect:color:turnedOn:")) MSG(void, self, "drawInsertionPointInRect:color:turnedOn:", cr, tv_ins_color(self, 0), (BOOL)YES);
        else { MSG0(void, tv_ins_color(self, 0), "setFill"); NSRectFill(cr); }
    }
}
static void tv_draw_ip(id self, SEL c, R r, id col, BOOL on) { (void)self; (void)c; if (!on) return; MSG0(void, col, "setFill"); NSRectFill(r); }
static void tv_draw_bg_rect(id self, SEL c, R r) { (void)c; if (pgi(self, "drawsBG", 1)) { MSG0(void, tv_bg(self, 0), "setFill"); NSRectFill(r); } }

/* ================================================================ mouse */
static u64 index_at(id self, P p) {
    S in = inset(self);
    return ((u64 (*)(id, SEL, P, id, double *))objc_msgSend)(lm(self), sl("characterIndexForPoint:inTextContainer:fractionOfDistanceBetweenInsertionPoints:"),
                                                            (P){p.x - in.w, p.y - in.h}, tcont(self), (double *)0);
}
static P local_pt(id self, id ev) { P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow")); return ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), w, (id)0); }
static int is_word_char(unsigned short c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0xC0; }
static RNG word_at(id self, u64 i) {
    u64 n = text_len(self);
    if (!n) return (RNG){0, 0};
    if (i >= n) i = n - 1;
    int w = is_word_char(ch_at(self, i));
    u64 a = i, b = i + 1;
    while (a > 0 && is_word_char(ch_at(self, a - 1)) == w && ch_at(self, a - 1) != '\n') a--;
    while (b < n && is_word_char(ch_at(self, b)) == w && ch_at(self, b) != '\n') b++;
    return (RNG){a, b - a};
}
static RNG granular(id self, u64 a, u64 b, i64 gran) {
    if (b < a) { u64 t = a; a = b; b = t; }
    if (gran == 1) { RNG ra = word_at(self, a), rb = word_at(self, b ? b - (b > a ? 1 : 0) : 0); u64 s = ra.loc, e = rb.loc + rb.len; return (RNG){s, e > s ? e - s : 0}; }
    if (gran == 2) return para_range(self, (RNG){a, b - a});
    return (RNG){a, b - a};
}
static void tv_mouse_down(id self, SEL c, id ev) {
    (void)c;
    if (!pgi(self, "selectable", 1)) return;
    id w = MSG0(id, self, "window");
    if (w && MSG0(id, w, "firstResponder") != self) MSG(BOOL, w, "makeFirstResponder:", self);
    P p = local_pt(self, ev);
    u64 i = index_at(self, p);
    i64 clicks = MSG0(i64, ev, "clickCount"), gran = clicks >= 3 ? 2 : clicks == 2 ? 1 : 0;
    u64 mods = MSG0(u64, ev, "modifierFlags");
    u64 anchor = (u64)pgi(self, "anchor", (i64)sel(self).loc);
    if (!(mods & (1u << 17))) anchor = i;
    psi(self, "gran", gran);
    RNG r = granular(self, anchor, i, gran);
    if (gran == 0 && !(mods & (1u << 17))) r = (RNG){i, 0};
    set_sel(self, r, 1);
    psi(self, "anchor", (i64)anchor);
    id until = MSG0(id, (id)kls("NSDate"), "distantFuture");
    u64 mask = (1ULL << 6) | (1ULL << 2);
    for (;;) {
        id e = ((id (*)(id, SEL, u64, id, id, BOOL))objc_msgSend)(NSApp, sl("nextEventMatchingMask:untilDate:inMode:dequeue:"), mask, until, cx_str("NSEventTrackingRunLoopMode"), (BOOL)YES);
        if (!e || MSG0(u64, e, "type") == 2) break;
        u64 j = index_at(self, local_pt(self, e));
        RNG nr = granular(self, anchor, j, gran);
        if (gran == 0) nr = j >= anchor ? (RNG){anchor, j - anchor} : (RNG){j, anchor - j};
        set_sel(self, nr, 1);
        MSG(void, self, "autoscroll:", e);
    }
}

/* ================================================================ teclado e comandos */
static void tv_key_down(id self, SEL c, id ev) {
    (void)c;
    if (!pgi(self, "editable", 1) && !pgi(self, "selectable", 1)) { id nr = MSG0(id, self, "nextResponder"); if (nr) MSG(void, nr, "keyDown:", ev); return; }
    id ctx = responds(self, "inputContext") ? MSG0(id, self, "inputContext") : 0;
    if (ctx && MSG(BOOL, ctx, "handleEvent:", ev)) return;
    id evs = MSG(id, (id)kls("NSArray"), "arrayWithObject:", ev);
    MSG(void, self, "interpretKeyEvents:", evs);
}
static BOOL tv_accepts_fr(id self, SEL c) { (void)c; return pgi(self, "selectable", 1) != 0; }
static BOOL tv_resign(id self, SEL c) {
    (void)c;
    if (pgi(self, "began", 0)) {
        id d = tv_delegate(self, 0);
        if (d && responds(d, "textShouldEndEditing:") && !MSG(BOOL, d, "textShouldEndEditing:", self)) return NO;
        psi(self, "began", 0);
        post(self, "NSTextDidEndEditingNotification", "textDidEndEditing:");
    }
    redraw(self);
    return YES;
}
static BOOL tv_become(id self, SEL c) { (void)c; redraw(self); return YES; }
static void move_to(id self, u64 i, int extend) {
    u64 n = text_len(self);
    if (i > n) i = n;
    if (!extend) { psi(self, "anchor", (i64)i); set_sel(self, (RNG){i, 0}, 1); }
    else {
        u64 a = (u64)pgi(self, "anchor", (i64)sel(self).loc);
        set_sel(self, i >= a ? (RNG){a, i - a} : (RNG){i, a - i}, 1);
    }
    psi(self, "head", (i64)i);
    MSG(void, self, "scrollRangeToVisible:", ((RNG){i, 0}));
}
static u64 head(id self) { RNG s = sel(self); i64 h = pgi(self, "head", -1); return h >= 0 && ((u64)h == s.loc || (u64)h == s.loc + s.len) ? (u64)h : s.loc + s.len; }
static u64 word_left(id self, u64 i) { while (i > 0 && !is_word_char(ch_at(self, i - 1))) i--; while (i > 0 && is_word_char(ch_at(self, i - 1))) i--; return i; }
static u64 word_right(id self, u64 i) { u64 n = text_len(self); while (i < n && !is_word_char(ch_at(self, i))) i++; while (i < n && is_word_char(ch_at(self, i))) i++; return i; }
static RNG line_frag(id self, u64 i) {
    RNG eff = {0, 0};
    u64 n = text_len(self);
    ((R (*)(id, SEL, u64, RNG *))objc_msgSend_stret)(lm(self), sl("lineFragmentRectForGlyphAtIndex:effectiveRange:"), i < n ? i : (n ? n - 1 : 0), &eff);
    return eff;
}
static u64 vertical(id self, u64 i, int dir, double lines) {
    R cr;
    S in = inset(self);
    RNG keep = sel(self);
    psi(self, "sloc", (i64)i); psi(self, "slen", 0);
    cr = caret_rect(self);
    psi(self, "sloc", (i64)keep.loc); psi(self, "slen", (i64)keep.len);
    double x = pgd(self, "goalX", -1);
    if (x < 0) x = cr.x;
    double y = dir < 0 ? cr.y - 1 - (lines - 1) * cr.h : cr.y + cr.h + 1 + (lines - 1) * cr.h;
    if (y < in.h) return 0;
    return index_at(self, (P){x, y});
}
#define MOVE(name, expr, ext) static void tv_##name(id self, SEL c, id s) { (void)c; (void)s; u64 h = head(self); RNG sr = sel(self); (void)sr; psd(self, "goalX", -1); move_to(self, (expr), ext); }
MOVE(move_left, sr.len && !0 ? sr.loc : (h ? h - 1 : 0), 0)
MOVE(move_right, sr.len ? sr.loc + sr.len : h + 1, 0)
MOVE(move_left_sel, h ? h - 1 : 0, 1)
MOVE(move_right_sel, h + 1, 1)
MOVE(move_word_left, word_left(self, sr.len ? sr.loc : h), 0)
MOVE(move_word_right, word_right(self, sr.len ? sr.loc + sr.len : h), 0)
MOVE(move_word_left_sel, word_left(self, h), 1)
MOVE(move_word_right_sel, word_right(self, h), 1)
MOVE(move_bol, line_frag(self, h).loc, 0)
MOVE(move_bol_sel, line_frag(self, h).loc, 1)
MOVE(move_bod, 0, 0)
MOVE(move_bod_sel, 0, 1)
MOVE(move_eod, text_len(self), 0)
MOVE(move_eod_sel, text_len(self), 1)
MOVE(move_bop, para_range(self, ((RNG){h, 0})).loc, 0)
MOVE(move_bop_sel, para_range(self, ((RNG){h, 0})).loc, 1)
static u64 eol(id self, u64 h) {
    RNG lf = line_frag(self, h);
    u64 e = lf.loc + lf.len;
    if (e > lf.loc && e <= text_len(self) && (ch_at(self, e - 1) == '\n' || (e < text_len(self) && ch_at(self, e - 1) == ' '))) e--;
    return e;
}
static u64 eop(id self, u64 h) { RNG pr = para_range(self, (RNG){h, 0}); u64 e = pr.loc + pr.len; if (e > pr.loc && ch_at(self, e - 1) == '\n') e--; return e; }
MOVE(move_eol, eol(self, h), 0)
MOVE(move_eol_sel, eol(self, h), 1)
MOVE(move_eop, eop(self, h), 0)
MOVE(move_eop_sel, eop(self, h), 1)
static void vmove(id self, int dir, int ext, double lines) {
    u64 h = head(self);
    if (pgd(self, "goalX", -1) < 0) {
        RNG k = sel(self);
        psi(self, "sloc", (i64)h); psi(self, "slen", 0);
        psd(self, "goalX", caret_rect(self).x);
        psi(self, "sloc", (i64)k.loc); psi(self, "slen", (i64)k.len);
    }
    double g = pgd(self, "goalX", -1);
    move_to(self, dir > 0 && h >= text_len(self) ? text_len(self) : vertical(self, h, dir, lines), ext);
    psd(self, "goalX", g);
}
static void tv_move_up(id self, SEL c, id s) { (void)c; (void)s; vmove(self, -1, 0, 1); }
static void tv_move_down(id self, SEL c, id s) { (void)c; (void)s; vmove(self, 1, 0, 1); }
static void tv_move_up_sel(id self, SEL c, id s) { (void)c; (void)s; vmove(self, -1, 1, 1); }
static void tv_move_down_sel(id self, SEL c, id s) { (void)c; (void)s; vmove(self, 1, 1, 1); }
static double page_lines(id self) { id sv = MSG0(id, self, "superview"); R b = sv ? msg_rect(sv, "bounds") : msg_rect(self, "bounds"); R cr = caret_rect(self); return cr.h > 0 ? b.h / cr.h - 1 : 10; }
static void tv_page_up(id self, SEL c, id s) { (void)c; (void)s; vmove(self, -1, 0, page_lines(self)); }
static void tv_page_down(id self, SEL c, id s) { (void)c; (void)s; vmove(self, 1, 0, page_lines(self)); }
static void tv_page_up_sel(id self, SEL c, id s) { (void)c; (void)s; vmove(self, -1, 1, page_lines(self)); }
static void tv_page_down_sel(id self, SEL c, id s) { (void)c; (void)s; vmove(self, 1, 1, page_lines(self)); }
static void delete_range(id self, RNG r, const char *action) { if (r.len) edit(self, r, cx_str(""), action); }
static void tv_delete_backward(id self, SEL c, id s) {
    (void)c; (void)s;
    RNG r = sel(self);
    if (!r.len) { if (!r.loc) { NSBeep(); return; } r = MSG(RNG, text_str(self), "rangeOfComposedCharacterSequenceAtIndex:", r.loc - 1); }
    delete_range(self, r, "Apagar");
}
static void tv_delete_forward(id self, SEL c, id s) {
    (void)c; (void)s;
    RNG r = sel(self);
    if (!r.len) { if (r.loc >= text_len(self)) { NSBeep(); return; } r = MSG(RNG, text_str(self), "rangeOfComposedCharacterSequenceAtIndex:", r.loc); }
    delete_range(self, r, "Apagar");
}
static void tv_delete_word_back(id self, SEL c, id s) { (void)c; (void)s; RNG r = sel(self); if (r.len) { delete_range(self, r, "Apagar"); return; } u64 a = word_left(self, r.loc); delete_range(self, (RNG){a, r.loc - a}, "Apagar"); }
static void tv_delete_word_fwd(id self, SEL c, id s) { (void)c; (void)s; RNG r = sel(self); if (r.len) { delete_range(self, r, "Apagar"); return; } u64 b = word_right(self, r.loc); delete_range(self, (RNG){r.loc, b - r.loc}, "Apagar"); }
static void tv_delete_bol(id self, SEL c, id s) { (void)c; (void)s; RNG r = sel(self); u64 a = line_frag(self, r.loc).loc; if (r.len) a = r.loc; delete_range(self, (RNG){a, r.loc + r.len - a}, "Apagar"); }
static void tv_delete_eol(id self, SEL c, id s) { (void)c; (void)s; RNG r = sel(self); u64 e = eol(self, r.loc); if (e == r.loc && e < text_len(self)) e++; delete_range(self, (RNG){r.loc, e - r.loc}, "Apagar"); }
static void tv_delete(id self, SEL c, id s) { (void)c; (void)s; delete_range(self, sel(self), "Apagar"); }
static int field_end(id self, i64 movement) {
    if (!pgi(self, "field", 0)) return 0;
    id d = tv_delegate(self, 0);
    (void)d;
    id info = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", cx_num_i(movement), cx_str("NSTextMovement"));
    id n = MSG(id, (id)kls("NSNotification"), "notificationWithName:object:userInfo:", cx_str("NSTextDidEndEditingNotification"), self, info);
    psi(self, "began", 0);
    if (d && responds(d, "textDidEndEditing:")) MSG(void, d, "textDidEndEditing:", n);
    MSG(void, MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter"), "postNotification:", n);
    return 1;
}
static BOOL delegate_command(id self, SEL cmd) {
    id d = tv_delegate(self, 0);
    return d && responds(d, "textView:doCommandBySelector:") && MSG(BOOL, d, "textView:doCommandBySelector:", self, cmd);
}
static void tv_do_command(id self, SEL c, SEL cmd) {
    (void)c;
    if (delegate_command(self, cmd)) return;
    if (cmd && cx_responds(object_getClass(self), cmd)) { ((void (*)(id, SEL, id))objc_msgSend)(self, cmd, (id)0); return; }
    id nr = MSG0(id, self, "nextResponder");
    if (nr && responds(nr, "doCommandBySelector:")) MSG(void, nr, "doCommandBySelector:", cmd); else NSBeep();
}
static void tv_newline(id self, SEL c, id s) { (void)c; (void)s; if (field_end(self, 0x10)) return; edit(self, sel(self), cx_str("\n"), "Digitação"); }
static void tv_newline_ign(id self, SEL c, id s) { (void)c; (void)s; edit(self, sel(self), cx_str("\n"), "Digitação"); }
static void tv_tab(id self, SEL c, id s) { (void)c; (void)s; if (field_end(self, 0x11)) return; edit(self, sel(self), cx_str("\t"), "Digitação"); }
static void tv_backtab(id self, SEL c, id s) { (void)c; (void)s; if (field_end(self, 0x12)) return; id w = MSG0(id, self, "window"); if (w) MSG(void, w, "selectPreviousKeyView:", self); }
static void tv_tab_ign(id self, SEL c, id s) { (void)c; (void)s; edit(self, sel(self), cx_str("\t"), "Digitação"); }
static void tv_cancel(id self, SEL c, id s) { (void)c; (void)s; if (field_end(self, 0x17)) return; }
static void tv_select_all(id self, SEL c, id s) { (void)c; (void)s; psi(self, "anchor", 0); set_sel(self, (RNG){0, text_len(self)}, 1); }
static void tv_select_word(id self, SEL c, id s) { (void)c; (void)s; set_sel(self, word_at(self, sel(self).loc), 1); }
static void tv_select_line(id self, SEL c, id s) { (void)c; (void)s; set_sel(self, line_frag(self, sel(self).loc), 1); }
static void tv_select_para(id self, SEL c, id s) { (void)c; (void)s; set_sel(self, para_range(self, sel(self)), 1); }
static void case_cmd(id self, const char *how) {
    RNG r = sel(self);
    if (!r.len) r = word_at(self, r.loc);
    if (!r.len) return;
    id sub = MSG(id, text_str(self), "substringWithRange:", r);
    id out = MSG0(id, sub, how);
    if (!should_change(self, r, out)) return;
    id ts = storage(self);
    MSG(void, ts, "replaceCharactersInRange:withString:", r, out);
    set_sel(self, (RNG){r.loc, MSG0(u64, out, "length")}, 1);
    did_change(self);
}
static void tv_upper(id self, SEL c, id s) { (void)c; (void)s; case_cmd(self, "uppercaseString"); }
static void tv_lower(id self, SEL c, id s) { (void)c; (void)s; case_cmd(self, "lowercaseString"); }
static void tv_capital(id self, SEL c, id s) { (void)c; (void)s; case_cmd(self, "capitalizedString"); }
static void tv_transpose(id self, SEL c, id s) {
    (void)c; (void)s;
    RNG r = sel(self);
    u64 n = text_len(self), i = r.loc;
    if (r.len || n < 2) { NSBeep(); return; }
    if (i >= n) i = n - 1;
    if (i == 0) i = 1;
    id a = MSG(id, text_str(self), "substringWithRange:", ((RNG){i - 1, 1})), b = MSG(id, text_str(self), "substringWithRange:", ((RNG){i, 1}));
    id swapped = MSG(id, b, "stringByAppendingString:", a);
    edit(self, (RNG){i - 1, 2}, swapped, "Transpor");
}

/* ================================================================ copiar e colar, Serviços */
static id tv_writable_types(id self, SEL c) {
    (void)c;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    if (pgi(self, "rich", 1)) MSG(void, a, "addObject:", cx_str("public.rtf"));
    MSG(void, a, "addObject:", NSPasteboardTypeString);
    return a;
}
static id tv_readable_types(id self, SEL c) { return tv_writable_types(self, c); }
static BOOL tv_write_sel(id self, SEL c, id pb, id types) {
    (void)c;
    RNG r = sel(self);
    if (!r.len) return NO;
    id sub = MSG(id, storage(self), "attributedSubstringFromRange:", r);
    MSG(i64, pb, "declareTypes:owner:", types ? types : tv_writable_types(self, 0), (id)0);
    BOOL ok = MSG(BOOL, pb, "setString:forType:", MSG0(id, sub, "string"), NSPasteboardTypeString);
    if (pgi(self, "rich", 1) && responds(sub, "RTFFromRange:documentAttributes:")) {
        id rtf = MSG(id, sub, "RTFFromRange:documentAttributes:", ((RNG){0, r.len}), (id)0);
        if (rtf) MSG(BOOL, pb, "setData:forType:", rtf, cx_str("public.rtf"));
    }
    return ok;
}
static BOOL tv_read_sel(id self, SEL c, id pb) {
    (void)c;
    id rich = 0;
    if (pgi(self, "rich", 1) && responds((id)kls("NSAttributedString"), "alloc")) {
        id rtf = MSG(id, pb, "dataForType:", cx_str("public.rtf"));
        if (rtf) {
            id a = MSG0(id, (id)kls("NSAttributedString"), "alloc");
            if (responds(a, "initWithRTF:documentAttributes:")) rich = objc_autorelease(MSG(id, a, "initWithRTF:documentAttributes:", rtf, (id *)0));
            else objc_release(a);
        }
    }
    if (rich) return edit(self, sel(self), rich, "Colar");
    id s = MSG(id, pb, "stringForType:", NSPasteboardTypeString);
    if (!s) return NO;
    return edit(self, sel(self), s, "Colar");
}
static id tv_valid_requestor(id self, SEL c, id send, id ret) {
    (void)c;
    BOOL send_ok = !send || (sel(self).len && MSG(BOOL, tv_writable_types(self, 0), "containsObject:", send));
    BOOL ret_ok = !ret || (pgi(self, "editable", 1) && MSG(BOOL, tv_readable_types(self, 0), "containsObject:", ret));
    return send_ok && ret_ok && (send || ret) ? self : 0;
}
static id general_pb(void) { return MSG0(id, (id)kls("NSPasteboard"), "generalPasteboard"); }
static void tv_copy(id self, SEL c, id s) { (void)c; (void)s; if (!tv_write_sel(self, 0, general_pb(), 0)) NSBeep(); }
static void tv_cut(id self, SEL c, id s) { (void)c; (void)s; if (!sel(self).len || !pgi(self, "editable", 1)) { NSBeep(); return; } tv_write_sel(self, 0, general_pb(), 0); delete_range(self, sel(self), "Recortar"); }
static void tv_paste(id self, SEL c, id s) { (void)c; (void)s; if (!pgi(self, "editable", 1) || !tv_read_sel(self, 0, general_pb())) NSBeep(); }
static void tv_paste_plain(id self, SEL c, id s) {
    (void)c; (void)s;
    id str = MSG(id, general_pb(), "stringForType:", NSPasteboardTypeString);
    if (!str || !pgi(self, "editable", 1)) { NSBeep(); return; }
    edit(self, sel(self), str, "Colar");
}
static BOOL tv_validate(id self, SEL c, id item) {
    (void)c;
    SEL a = responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    BOOL ed = pgi(self, "editable", 1) != 0, has = sel(self).len > 0;
    if (a == sl("copy:")) return has;
    if (a == sl("cut:") || a == sl("delete:")) return has && ed;
    if (a == sl("paste:") || a == sl("pasteAsPlainText:") || a == sl("pasteAsRichText:")) return ed && MSG(id, general_pb(), "stringForType:", NSPasteboardTypeString) != 0;
    if (a == sl("selectAll:")) return text_len(self) > 0;
    if (a == sl("alignLeft:") || a == sl("alignCenter:") || a == sl("alignRight:") || a == sl("alignJustified:") || a == sl("underline:")) return ed;
    return YES;
}

/* ================================================================ NSTextInputClient (método de entrada) */
static void tv_set_marked(id self, SEL c, id str, RNG selr, RNG rr) {
    (void)c;
    u64 mloc = (u64)pgi(self, "mloc", (i64)NOTFOUND);
    RNG r = rr.loc != NOTFOUND ? rr : mloc != NOTFOUND ? (RNG){mloc, (u64)pgi(self, "mlen", 0)} : sel(self);
    id a = attributed(self, str);
    u64 n = a ? MSG0(u64, a, "length") : 0;
    id m = a ? MSG0(id, a, "mutableCopy") : 0;
    if (m && n) MSG(void, m, "addAttributes:range:", tv_marked_attrs(self, 0), ((RNG){0, n}));
    replace_with(self, r, m ? objc_autorelease(m) : 0, 0, 0);
    if (n) { psi(self, "mloc", (i64)r.loc); psi(self, "mlen", (i64)n); } else { psi(self, "mloc", (i64)NOTFOUND); psi(self, "mlen", 0); }
    set_sel(self, (RNG){r.loc + selr.loc, selr.len}, 1);
    tv_size_to_text(self);
}
static void tv_unmark(id self, SEL c) {
    (void)c;
    u64 mloc = (u64)pgi(self, "mloc", (i64)NOTFOUND);
    if (mloc != NOTFOUND) {
        RNG r = {mloc, (u64)pgi(self, "mlen", 0)};
        id ts = storage(self);
        if (ts && r.len) MSG(void, ts, "setAttributes:range:", typing_attrs(self, 0), r);
    }
    psi(self, "mloc", (i64)NOTFOUND); psi(self, "mlen", 0);
}
static BOOL tv_has_marked(id self, SEL c) { (void)c; return (u64)pgi(self, "mloc", (i64)NOTFOUND) != NOTFOUND; }
static RNG tv_marked_range(id self, SEL c) { (void)c; u64 m = (u64)pgi(self, "mloc", (i64)NOTFOUND); return m == NOTFOUND ? (RNG){NOTFOUND, 0} : (RNG){m, (u64)pgi(self, "mlen", 0)}; }
static id tv_attr_sub(id self, SEL c, RNG r, RNG *actual) {
    (void)c;
    u64 n = text_len(self);
    if (r.loc > n) return 0;
    if (r.loc + r.len > n) r.len = n - r.loc;
    if (actual) *actual = r;
    return MSG(id, storage(self), "attributedSubstringFromRange:", r);
}
static id tv_valid_marked_attrs(id self, SEL c) { (void)self; (void)c; return MSG(id, (id)kls("NSArray"), "arrayWithObject:", NSUnderlineStyleAttributeName); }
static R tv_first_rect(id self, SEL c, RNG r, RNG *actual) {
    (void)c;
    if (actual) *actual = r;
    S in = inset(self);
    R b = ((R (*)(id, SEL, RNG, id))objc_msgSend_stret)(lm(self), sl("boundingRectForGlyphRange:inTextContainer:"), r, tcont(self));
    R v = mkr(b.x + in.w, b.y + in.h, b.w ? b.w : 1, b.h);
    R w = ((R (*)(id, SEL, R, id))objc_msgSend_stret)(self, sl("convertRect:toView:"), v, (id)0);
    id win = MSG0(id, self, "window");
    return win ? ((R (*)(id, SEL, R))objc_msgSend_stret)(win, sl("convertRectToScreen:"), w) : w;
}
static u64 tv_char_for_point(id self, SEL c, P screen) {
    (void)c;
    id win = MSG0(id, self, "window");
    R wr = win ? ((R (*)(id, SEL, R))objc_msgSend_stret)(win, sl("convertRectFromScreen:"), mkr(screen.x, screen.y, 0, 0)) : mkr(screen.x, screen.y, 0, 0);
    P lp = ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), (P){wr.x, wr.y}, (id)0);
    return index_at(self, lp);
}

/* ================================================================ rolagem e acessibilidade */
static void tv_scroll_range(id self, SEL c, RNG r) {
    (void)c;
    S in = inset(self);
    R b = ((R (*)(id, SEL, RNG, id))objc_msgSend_stret)(lm(self), sl("boundingRectForGlyphRange:inTextContainer:"), r, tcont(self));
    if (b.h <= 0) b = caret_rect(self); else { b.x += in.w; b.y += in.h; }
    MSG(BOOL, self, "scrollRectToVisible:", b);
}
static void tv_center_sel(id self, SEL c, id s) { (void)c; (void)s; tv_scroll_range(self, 0, sel(self)); }
static id tv_ax_role(id self, SEL c) { (void)self; (void)c; return cx_str("AXTextArea"); }
static id tv_ax_value(id self, SEL c) { (void)c; return tv_string(self, 0); }
static void tv_ax_set_value(id self, SEL c, id v) { (void)c; if (pgi(self, "editable", 1)) { tv_set_string(self, 0, v); did_change(self); } }
static RNG tv_ax_sel_range(id self, SEL c) { (void)c; return sel(self); }
static void tv_ax_set_sel_range(id self, SEL c, RNG r) { (void)c; set_sel(self, r, 1); }
static id tv_ax_sel_text(id self, SEL c) { (void)c; RNG r = sel(self); return MSG(id, text_str(self), "substringWithRange:", r); }
static i64 tv_ax_chars(id self, SEL c) { (void)c; return (i64)text_len(self); }
static id tv_ax_string_for_range(id self, SEL c, RNG r) { (void)c; u64 n = text_len(self); if (r.loc > n) return 0; if (r.loc + r.len > n) r.len = n - r.loc; return MSG(id, text_str(self), "substringWithRange:", r); }

/* ================================================================ fábricas */
static id tv_scrollable(id k, SEL c, int rich) {
    (void)c;
    R f = mkr(0, 0, 400, 300);
    id sv = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSScrollView"), "alloc"), sl("initWithFrame:"), f));
    id tv = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithFrame:"), f));
    psi(tv, "rich", rich);
    MSG(void, tv, "setAutoresizingMask:", (u64)2);
    MSG(void, sv, "setHasVerticalScroller:", (BOOL)YES);
    MSG(void, sv, "setDocumentView:", tv);
    return sv;
}
static id tv_scrollable_rich(id k, SEL c) { return tv_scrollable(k, c, 1); }
static id tv_scrollable_plain(id k, SEL c) { return tv_scrollable(k, c, 0); }
static id tv_field_editor(id k, SEL c) {
    (void)c;
    id tv = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithFrame:"), mkr(0, 0, 100, 20)));
    psi(tv, "field", 1); psi(tv, "rich", 0);
    return tv;
}

/* ================================================================ registro */
#define ADD(s, f) cx_add_method(k, s, (void *)(f))
void cx_appkit_textview_init(void) {
    cx_class_t *k = kls("NSTextView"), *view = kls("NSView");
    if (!k || !view) return;
    view_init_frame = class_getMethodImplementation(view, sl("initWithFrame:"));
    view_set_frame_size = class_getMethodImplementation(view, sl("setFrameSize:"));
    ADD("initWithFrame:", tv_init_frame); ADD("initWithFrame:textContainer:", tv_init_frame_tc); ADD("initWithCoder:", tv_init_coder);
    ADD("textStorage", tv_storage); ADD("layoutManager", tv_lm); ADD("textContainer", tv_tc); ADD("setTextContainer:", tv_set_tc);
    ADD("replaceTextContainer:", tv_replace_tc); ADD("textContainerInset", tv_inset); ADD("setTextContainerInset:", tv_set_inset);
    ADD("textContainerOrigin", tv_origin); ADD("invalidateTextContainerOrigin", tv_size_to_fit);
    ADD("delegate", tv_delegate); ADD("setDelegate:", tv_set_delegate);
    ADD("selectedRange", tv_sel_range); ADD("setSelectedRange:", tv_set_sel_range); ADD("setSelectedRange:affinity:stillSelecting:", tv_set_sel_affinity);
    ADD("selectedRanges", tv_sel_ranges); ADD("setSelectedRanges:", tv_set_sel_ranges); ADD("setSelectedRanges:affinity:stillSelecting:", tv_set_sel_ranges);
    ADD("selectionAffinity", tv_affinity); ADD("selectionGranularity", tv_granularity); ADD("setSelectionGranularity:", tv_set_granularity);
    ADD("undoManager", tv_undo_manager); ADD("_cxUndoReplace:", undo_replace);
    ADD("insertText:", tv_insert_text); ADD("insertText:replacementRange:", tv_insert_text_r);
    ADD("replaceCharactersInRange:withString:", tv_replace_chars);
    ADD("shouldChangeTextInRange:replacementString:", tv_should_change); ADD("didChangeText", tv_did_change);
    ADD("string", tv_string); ADD("setString:", tv_set_string); ADD("attributedString", tv_attributed_string);
    ADD("setAttributedString:", tv_set_attributed); ADD("appendAttributedString:", tv_append_attributed);
    ADD("typingAttributes", typing_attrs); ADD("setTypingAttributes:", tv_set_typing);
    ADD("font", tv_font); ADD("setFont:", tv_set_font); ADD("setFont:range:", tv_set_font_range);
    ADD("textColor", tv_text_color); ADD("setTextColor:", tv_set_text_color); ADD("setTextColor:range:", tv_set_text_color_range);
    ADD("alignment", tv_alignment); ADD("setAlignment:", tv_set_alignment); ADD("setAlignment:range:", tv_set_alignment_range);
    ADD("alignLeft:", tv_align_left); ADD("alignRight:", tv_align_right); ADD("alignCenter:", tv_align_center);
    ADD("alignJustified:", tv_align_justified); ADD("alignNatural:", tv_align_natural);
    ADD("defaultParagraphStyle", tv_default_para); ADD("setDefaultParagraphStyle:", tv_set_default_para);
    ADD("changeFont:", tv_change_font); ADD("changeColor:", tv_change_color); ADD("underline:", tv_underline);
    ADD("isEditable", tvg_editable); ADD("setEditable:", tvs_editable); ADD("isSelectable", tvg_selectable); ADD("setSelectable:", tvs_selectable); ADD("isRichText", tvg_rich); ADD("setRichText:", tvs_rich); ADD("drawsBackground", tvg_drawsBG); ADD("setDrawsBackground:", tvs_drawsBG);
    ADD("allowsUndo", tvg_undo); ADD("setAllowsUndo:", tvs_undo); ADD("isFieldEditor", tvg_field); ADD("setFieldEditor:", tvs_field); ADD("importsGraphics", tvg_graphics); ADD("setImportsGraphics:", tvs_graphics);
    ADD("isHorizontallyResizable", tvg_hres); ADD("setHorizontallyResizable:", tvs_hres); ADD("isVerticallyResizable", tvg_vres); ADD("setVerticallyResizable:", tvs_vres);
    ADD("usesFindBar", tvg_findBar); ADD("setUsesFindBar:", tvs_findBar); ADD("usesFindPanel", tvg_findPanel); ADD("setUsesFindPanel:", tvs_findPanel); ADD("usesRuler", tvg_ruler); ADD("setUsesRuler:", tvs_ruler); ADD("isRulerVisible", tvg_rulerVis); ADD("setRulerVisible:", tvs_rulerVis);
    ADD("isContinuousSpellCheckingEnabled", tvg_spell); ADD("setContinuousSpellCheckingEnabled:", tvs_spell); ADD("isGrammarCheckingEnabled", tvg_grammar); ADD("setGrammarCheckingEnabled:", tvs_grammar);
    ADD("isAutomaticQuoteSubstitutionEnabled", tvg_quotes); ADD("setAutomaticQuoteSubstitutionEnabled:", tvs_quotes); ADD("isAutomaticDashSubstitutionEnabled", tvg_dashes); ADD("setAutomaticDashSubstitutionEnabled:", tvs_dashes);
    ADD("isAutomaticLinkDetectionEnabled", tvg_links); ADD("setAutomaticLinkDetectionEnabled:", tvs_links); ADD("isAutomaticTextReplacementEnabled", tvg_textRepl); ADD("setAutomaticTextReplacementEnabled:", tvs_textRepl);
    ADD("isAutomaticSpellingCorrectionEnabled", tvg_spellCorr); ADD("setAutomaticSpellingCorrectionEnabled:", tvs_spellCorr); ADD("isAutomaticDataDetectionEnabled", tvg_dataDet); ADD("setAutomaticDataDetectionEnabled:", tvs_dataDet);
    ADD("smartInsertDeleteEnabled", tvg_smartInsert); ADD("setSmartInsertDeleteEnabled:", tvs_smartInsert); ADD("usesFontPanel", tvg_fontPanel); ADD("setUsesFontPanel:", tvs_fontPanel); ADD("usesInspectorBar", tvg_inspector); ADD("setUsesInspectorBar:", tvs_inspector);
    ADD("allowsDocumentBackgroundColorChange", tvg_docAttrs); ADD("setAllowsDocumentBackgroundColorChange:", tvs_docAttrs); ADD("allowsImageEditing", tvg_imageEdit); ADD("setAllowsImageEditing:", tvs_imageEdit);
    ADD("displaysLinkToolTips", tvg_incremental); ADD("setDisplaysLinkToolTips:", tvs_incremental);
    ADD("isEditable", tvg_editable); ADD("isSelectable", tvg_selectable); ADD("isRichText", tvg_rich); ADD("drawsBackground", tvg_drawsBG);
    ADD("allowsUndo", tvg_undo); ADD("isFieldEditor", tvg_field); ADD("importsGraphics", tvg_graphics);
    ADD("isHorizontallyResizable", tvg_hres); ADD("isVerticallyResizable", tvg_vres);
    ADD("backgroundColor", tv_bg); ADD("setBackgroundColor:", tv_set_bg); ADD("insertionPointColor", tv_ins_color); ADD("setInsertionPointColor:", tv_set_ins_color);
    ADD("selectedTextAttributes", tv_sel_attrs); ADD("setSelectedTextAttributes:", tv_set_sel_attrs);
    ADD("linkTextAttributes", tv_link_attrs); ADD("setLinkTextAttributes:", tv_set_link_attrs);
    ADD("markedTextAttributes", tv_marked_attrs); ADD("setMarkedTextAttributes:", tv_set_marked_attrs);
    ADD("minSize", tv_min); ADD("setMinSize:", tv_set_min); ADD("maxSize", tv_max); ADD("setMaxSize:", tv_set_max);
    ADD("sizeToFit", tv_size_to_fit); ADD("setFrameSize:", tv_set_frame_size); ADD("_cxStorageEdited:delta:", tv_storage_edited);
    ADD("isFlipped", tv_flipped); ADD("drawRect:", tv_draw); ADD("drawInsertionPointInRect:color:turnedOn:", tv_draw_ip);
    ADD("drawViewBackgroundInRect:", tv_draw_bg_rect);
    ADD("mouseDown:", tv_mouse_down); ADD("keyDown:", tv_key_down); ADD("acceptsFirstResponder", tv_accepts_fr);
    ADD("resignFirstResponder", tv_resign); ADD("becomeFirstResponder", tv_become); ADD("doCommandBySelector:", tv_do_command);
    ADD("moveLeft:", tv_move_left); ADD("moveRight:", tv_move_right); ADD("moveBackward:", tv_move_left); ADD("moveForward:", tv_move_right);
    ADD("moveLeftAndModifySelection:", tv_move_left_sel); ADD("moveRightAndModifySelection:", tv_move_right_sel);
    ADD("moveBackwardAndModifySelection:", tv_move_left_sel); ADD("moveForwardAndModifySelection:", tv_move_right_sel);
    ADD("moveWordLeft:", tv_move_word_left); ADD("moveWordRight:", tv_move_word_right); ADD("moveWordBackward:", tv_move_word_left); ADD("moveWordForward:", tv_move_word_right);
    ADD("moveWordLeftAndModifySelection:", tv_move_word_left_sel); ADD("moveWordRightAndModifySelection:", tv_move_word_right_sel);
    ADD("moveWordBackwardAndModifySelection:", tv_move_word_left_sel); ADD("moveWordForwardAndModifySelection:", tv_move_word_right_sel);
    ADD("moveUp:", tv_move_up); ADD("moveDown:", tv_move_down); ADD("moveUpAndModifySelection:", tv_move_up_sel); ADD("moveDownAndModifySelection:", tv_move_down_sel);
    ADD("moveToBeginningOfLine:", tv_move_bol); ADD("moveToEndOfLine:", tv_move_eol); ADD("moveToLeftEndOfLine:", tv_move_bol); ADD("moveToRightEndOfLine:", tv_move_eol);
    ADD("moveToBeginningOfLineAndModifySelection:", tv_move_bol_sel); ADD("moveToEndOfLineAndModifySelection:", tv_move_eol_sel);
    ADD("moveToLeftEndOfLineAndModifySelection:", tv_move_bol_sel); ADD("moveToRightEndOfLineAndModifySelection:", tv_move_eol_sel);
    ADD("moveToBeginningOfDocument:", tv_move_bod); ADD("moveToEndOfDocument:", tv_move_eod);
    ADD("moveToBeginningOfDocumentAndModifySelection:", tv_move_bod_sel); ADD("moveToEndOfDocumentAndModifySelection:", tv_move_eod_sel);
    ADD("moveToBeginningOfParagraph:", tv_move_bop); ADD("moveToEndOfParagraph:", tv_move_eop);
    ADD("moveToBeginningOfParagraphAndModifySelection:", tv_move_bop_sel); ADD("moveToEndOfParagraphAndModifySelection:", tv_move_eop_sel);
    ADD("pageUp:", tv_page_up); ADD("pageDown:", tv_page_down); ADD("pageUpAndModifySelection:", tv_page_up_sel); ADD("pageDownAndModifySelection:", tv_page_down_sel);
    ADD("scrollPageUp:", tv_page_up); ADD("scrollPageDown:", tv_page_down);
    ADD("deleteBackward:", tv_delete_backward); ADD("deleteForward:", tv_delete_forward); ADD("deleteBackwardByDecomposingPreviousCharacter:", tv_delete_backward);
    ADD("deleteWordBackward:", tv_delete_word_back); ADD("deleteWordForward:", tv_delete_word_fwd);
    ADD("deleteToBeginningOfLine:", tv_delete_bol); ADD("deleteToEndOfLine:", tv_delete_eol);
    ADD("deleteToBeginningOfParagraph:", tv_delete_bol); ADD("deleteToEndOfParagraph:", tv_delete_eol); ADD("delete:", tv_delete);
    ADD("insertNewline:", tv_newline); ADD("insertLineBreak:", tv_newline_ign); ADD("insertParagraphSeparator:", tv_newline);
    ADD("insertNewlineIgnoringFieldEditor:", tv_newline_ign); ADD("insertTab:", tv_tab); ADD("insertBacktab:", tv_backtab);
    ADD("insertTabIgnoringFieldEditor:", tv_tab_ign); ADD("cancelOperation:", tv_cancel);
    ADD("selectAll:", tv_select_all); ADD("selectWord:", tv_select_word); ADD("selectLine:", tv_select_line); ADD("selectParagraph:", tv_select_para);
    ADD("uppercaseWord:", tv_upper); ADD("lowercaseWord:", tv_lower); ADD("capitalizeWord:", tv_capital); ADD("transpose:", tv_transpose);
    ADD("copy:", tv_copy); ADD("cut:", tv_cut); ADD("paste:", tv_paste); ADD("pasteAsPlainText:", tv_paste_plain); ADD("pasteAsRichText:", tv_paste);
    ADD("validateMenuItem:", tv_validate); ADD("validateUserInterfaceItem:", tv_validate);
    ADD("writablePasteboardTypes", tv_writable_types); ADD("readablePasteboardTypes", tv_readable_types);
    ADD("writeSelectionToPasteboard:types:", tv_write_sel); ADD("readSelectionFromPasteboard:", tv_read_sel);
    ADD("validRequestorForSendType:returnType:", tv_valid_requestor);
    ADD("setMarkedText:selectedRange:replacementRange:", tv_set_marked); ADD("unmarkText", tv_unmark); ADD("hasMarkedText", tv_has_marked);
    ADD("markedRange", tv_marked_range); ADD("attributedSubstringForProposedRange:actualRange:", tv_attr_sub);
    ADD("validAttributesForMarkedText", tv_valid_marked_attrs); ADD("firstRectForCharacterRange:actualRange:", tv_first_rect);
    ADD("characterIndexForPoint:", tv_char_for_point);
    ADD("scrollRangeToVisible:", tv_scroll_range); ADD("centerSelectionInVisibleArea:", tv_center_sel);
    ADD("accessibilityRole", tv_ax_role); ADD("accessibilityValue", tv_ax_value); ADD("setAccessibilityValue:", tv_ax_set_value);
    ADD("accessibilitySelectedTextRange", tv_ax_sel_range); ADD("setAccessibilitySelectedTextRange:", tv_ax_set_sel_range);
    ADD("accessibilitySelectedText", tv_ax_sel_text); ADD("accessibilityNumberOfCharacters", tv_ax_chars);
    ADD("accessibilityStringForRange:", tv_ax_string_for_range);
    cx_add_cmethod(k, "scrollableTextView", (void *)tv_scrollable_rich);
    cx_add_cmethod(k, "scrollableDocumentContentTextView", (void *)tv_scrollable_rich);
    cx_add_cmethod(k, "scrollablePlainDocumentContentTextView", (void *)tv_scrollable_plain);
    cx_add_cmethod(k, "fieldEditor", (void *)tv_field_editor);
    (void)yes_imp;
}

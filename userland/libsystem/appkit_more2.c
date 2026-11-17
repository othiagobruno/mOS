/*
 * mOS libSystem — mais AppKit do corpus (lote de 17 nov.):
 *   Funções: NSAccessibilityUnignoredDescendant, NSRectFillListWithGrays, NSDrawWindowBackground, _NSPrependToKillRing,
 *     _NSDrawCarbonThemeBezel/_NSDrawCarbonThemeListBox; NSViewNoInstrinsicMetric.
 *   Texto: NSTypesetter/NSATSTypesetter (o layout é o do NSLayoutManager do mOS; o typesetter guarda as opções e calcula
 *     as métricas de linha), NSGlyphGenerator, NSGlyphInfo, NSTextFinder (busca na string do cliente, próxima/anterior,
 *     selecionar tudo, substituir), NSTextInsertionIndicator, NSTextCheckingController, NSTextAttachmentViewProvider.
 *   Árvore: NSTreeNode (filhos, pai, indexPath, descendente, ordenação) e NSTreeController (conteúdo pelos keyPaths,
 *     arrangedObjects, seleção por indexPaths, inserir/remover).
 *   Outros: NSInputManager, NSHelpManager (ajuda contextual, livro de ajuda pelo Info.plist aberto no navegador),
 *     NSCustomImageRep, NSPICTImageRep (caixa do cabeçalho e a prévia que o ImageIO decodificar), NSGlassEffectView e
 *     NSGlassEffectContainerView, NSPressureConfiguration, NSSliderAccessory(+Behavior), NSMenuItemBadge,
 *     NSStoryboardSegue, NSTableViewRowAction, NSButtonTouchBarItem, NSCollectionViewFlowLayoutInvalidationContext,
 *     NSTintConfiguration, NSToolTipManager, NSWritingToolsCoordinator, NSAnimationManager,
 *     NSSplitViewItemAccessoryViewController, NSSpeechRecognizer (comandos guardados; o mOS não tem motor de voz),
 *     NSPathCell, NSFrameView/NSNextStepFrame, NSFunctionRow.
 * Registro: cx_appkit_more2_init(), chamado do cx_appkit_input_init.
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
const char *sel_getName(SEL s);
id cx_color(double r, double g, double b, double a);
void NSBeep(void);
extern id NSApp;
extern id NSFontAttributeName, NSForegroundColorAttributeName;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RNG;
void NSRectFill(R r);
void NSFrameRect(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSTypesetter) K(NSATSTypesetter) K(NSGlyphGenerator) K(NSGlyphInfo) K(NSTextFinder) K(NSTextInsertionIndicator)
K(NSTextCheckingController) K(NSTextAttachmentViewProvider) K(NSTreeNode) K(NSTreeController) K(NSInputManager) K(NSHelpManager)
K(NSCustomImageRep) K(NSPICTImageRep) K(NSGlassEffectView) K(NSGlassEffectContainerView) K(NSPressureConfiguration)
K(NSSliderAccessory) K(NSSliderAccessoryBehavior) K(NSMenuItemBadge) K(NSStoryboardSegue) K(NSTableViewRowAction)
K(NSButtonTouchBarItem) K(NSCollectionViewFlowLayoutInvalidationContext) K(NSTintConfiguration) K(NSToolTipManager)
K(NSWritingToolsCoordinator) K(NSAnimationManager) K(NSSplitViewItemAccessoryViewController) K(NSSpeechRecognizer)
K(NSPathCell) K(NSFrameView) K(NSNextStepFrame) K(NSFunctionRow)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static char k_props, k_weak;
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
static id arr(void) { return MSG0(id, (id)kls("NSMutableArray"), "array"); }
static id wget(id o) { return objc_getAssociatedObject(o, &k_weak); }
static void wset(id o, id v) { objc_setAssociatedObject(o, &k_weak, v, 0); }
static void redraw(id v) { if (responds(v, "setNeedsDisplay:")) MSG(void, v, "setNeedsDisplay:", (BOOL)YES); }
static void *block_fn(void *b) { return *(void **)((u8 *)b + 16); }
void *_Block_copy(const void *b);
void _Block_release(const void *b);
/* bloco guardado num NSValue (o dono o solta em dealloc via associação) */
static id block_box(void *b) { return b ? MSG(id, (id)kls("NSValue"), "valueWithPointer:", _Block_copy(b)) : 0; }
static void *block_of(id v) { return v ? MSG0(void *, v, "pointerValue") : 0; }

/* propriedades genéricas: getter/setter de objeto, inteiro, real e booleano */
#define OP(fn, key) static id g_##fn(id self, SEL c) { (void)c; return pget(self, key); } static void s_##fn(id self, SEL c, id v) { (void)c; pset(self, key, v); redraw(self); }
#define IP(fn, key, def) static i64 g_##fn(id self, SEL c) { (void)c; return pgi(self, key, def); } static void s_##fn(id self, SEL c, i64 v) { (void)c; psi(self, key, v); redraw(self); }
#define DP(fn, key, def) static double g_##fn(id self, SEL c) { (void)c; return pgd(self, key, def); } static void s_##fn(id self, SEL c, double v) { (void)c; psd(self, key, v); redraw(self); }
#define BP(fn, key, def) static BOOL g_##fn(id self, SEL c) { (void)c; return pgi(self, key, def) != 0; } static void s_##fn(id self, SEL c, BOOL v) { (void)c; psi(self, key, v); redraw(self); }

/* ================================================================ funções */
const double NSViewNoInstrinsicMetric = -1.0;   /* o nome com o erro de digitação que a Apple manteve */
id NSAccessibilityUnignoredDescendant(id elem) {
    for (int guard = 0; elem && guard < 64; guard++) {
        BOOL ignored = responds(elem, "isAccessibilityElement") ? !MSG0(BOOL, elem, "isAccessibilityElement")
                     : responds(elem, "accessibilityIsIgnored") ? MSG0(BOOL, elem, "accessibilityIsIgnored") : NO;
        if (!ignored) return elem;
        id kids = responds(elem, "accessibilityChildren") ? MSG0(id, elem, "accessibilityChildren") : 0;
        if (!kids || MSG0(u64, kids, "count") != 1) return 0;   /* só desce quando há um único caminho */
        elem = MSG(id, kids, "objectAtIndex:", (u64)0);
    }
    return 0;
}
void NSRectFillListWithGrays(const R *rects, const double *grays, i64 n) {
    for (i64 i = 0; rects && grays && i < n; i++) { MSG0(void, cx_color(grays[i], grays[i], grays[i], 1), "setFill"); NSRectFill(rects[i]); }
}
void NSDrawWindowBackground(R r) { MSG0(void, MSG0(id, (id)kls("NSColor"), "windowBackgroundColor"), "setFill"); NSRectFill(r); }
/* anel de "recortar" das teclas do Emacs (⌃K junta, ⌃Y cola): o texto mais recente fica na frente */
static id g_kill_ring;
void _NSPrependToKillRing(id s) {
    if (!s) return;
    if (!g_kill_ring) g_kill_ring = objc_retain(MSG0(id, (id)kls("NSMutableString"), "string"));
    MSG(void, g_kill_ring, "insertString:atIndex:", s, (u64)0);
}
void _NSAppendToKillRing(id s) {
    if (!s) return;
    if (!g_kill_ring) g_kill_ring = objc_retain(MSG0(id, (id)kls("NSMutableString"), "string"));
    MSG(void, g_kill_ring, "appendString:", s);
}
id _NSYankFromKillRing(void) { return g_kill_ring ? MSG0(id, g_kill_ring, "copy") : cx_str(""); }
void _NSNewKillRingSequence(void) { if (g_kill_ring) MSG(void, g_kill_ring, "setString:", cx_str("")); }
void _NSInitializeKillRing(void) { if (!g_kill_ring) g_kill_ring = objc_retain(MSG0(id, (id)kls("NSMutableString"), "string")); }
/* depois de colar, o próximo recorte começa uma sequência nova (como o yank do Emacs) */
void _NSSetKillRingToYankedState(void) { _NSNewKillRingSequence(); }
static void bezel(R f, BOOL enabled, double radius) {
    id p = ((id (*)(id, SEL, R, double, double))objc_msgSend)((id)kls("NSBezierPath"), sl("bezierPathWithRoundedRect:xRadius:yRadius:"), mkr(f.x + 0.5, f.y + 0.5, f.w - 1, f.h - 1), radius, radius);
    MSG0(void, cx_color(1, 1, 1, enabled ? 1 : 0.6), "setFill"); MSG0(void, p, "fill");
    MSG0(void, cx_color(0.70, 0.70, 0.72, 1), "setStroke"); MSG0(void, p, "stroke");
}
void _NSDrawCarbonThemeBezel(R f, BOOL enabled, BOOL flipped) { (void)flipped; bezel(f, enabled, 0); }
void _NSDrawCarbonThemeListBox(R f, BOOL enabled, BOOL flipped, BOOL always) { (void)flipped; (void)always; bezel(f, enabled, 0); }

/* ================================================================ NSTypesetter / NSATSTypesetter / glifos */
static id ts_shared(id k, SEL c) {
    (void)c;
    static id s_ts, s_ats;
    id *slot = (cx_class_t *)k == &NSATSTypesetter_c ? &s_ats : &s_ts;
    if (!*slot) { *slot = cx_alloc_instance((cx_class_t *)k); psd(*slot, "pad", 5); psi(*slot, "leading", 1); psi(*slot, "behavior", -1); }
    return *slot;
}
static i64 ts_default_behavior(id k, SEL c) { (void)k; (void)c; return 3; }   /* NSTypesetterBehavior_10_4 e depois */
BP(tsLeading, "leading", 1) IP(tsBehavior, "behavior", -1) DP(tsPad, "pad", 5) DP(tsHyph, "hyph", 0) BP(tsBidi, "bidi", 1) BP(tsScreen, "screen", 0)
static id ts_lm(id self, SEL c) { (void)c; return wget(self); }
static id ts_attrs_str(id self, SEL c) { (void)c; id lm = wget(self); return lm ? MSG0(id, lm, "textStorage") : 0; }
/* métricas de linha pela fonte (o que o NSLayoutManager do mOS também usa) */
static double ts_line_spacing_after(id self, SEL c, u64 g, R r) { (void)self; (void)c; (void)g; (void)r; return 0; }
static double ts_para_before(id self, SEL c, u64 g, R r) { (void)c; (void)r; id lm = wget(self); id ts = lm ? MSG0(id, lm, "textStorage") : 0; extern id NSParagraphStyleAttributeName; id ps = ts && g < MSG0(u64, ts, "length") ? MSG(id, ts, "attribute:atIndex:effectiveRange:", NSParagraphStyleAttributeName, g, (RNG *)0) : 0; return ps && responds(ps, "paragraphSpacingBefore") ? MSG0(double, ps, "paragraphSpacingBefore") : 0; }
static double ts_para_after(id self, SEL c, u64 g, R r) { (void)c; (void)r; id lm = wget(self); id ts = lm ? MSG0(id, lm, "textStorage") : 0; extern id NSParagraphStyleAttributeName; id ps = ts && g < MSG0(u64, ts, "length") ? MSG(id, ts, "attribute:atIndex:effectiveRange:", NSParagraphStyleAttributeName, g, (RNG *)0) : 0; return ps ? MSG0(double, ps, "paragraphSpacing") : 0; }
static void ts_layout_glyphs(id self, SEL c, id lm, u64 start, u64 maxlines, u64 *next) {
    (void)c; (void)maxlines;
    wset(self, lm);
    if (lm) MSG(void, lm, "ensureLayoutForGlyphRange:", ((RNG){start, MSG0(u64, lm, "numberOfGlyphs") - start}));
    if (next) *next = lm ? MSG0(u64, lm, "numberOfGlyphs") : start;
}
static RNG ts_layout_chars(id self, SEL c, RNG r, id lm, u64 maxlines) {
    (void)c; (void)maxlines;
    wset(self, lm);
    if (lm) MSG(void, lm, "ensureLayoutForCharacterRange:", r);
    return r;
}
static S ts_glyph_extent(id self, SEL c) { (void)self; (void)c; S z = {0, 0}; return z; }
static id gg_shared(id k, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)k); return s; }
static void gg_generate(id self, SEL c, u64 n, id storage, u64 *glyph_idx, u64 *char_idx) {
    (void)self; (void)c; (void)storage;
    /* um glifo por caractere, como o NSLayoutManager do mOS */
    if (glyph_idx) *glyph_idx += n;
    if (char_idx) *char_idx += n;
}
static id gi_with_glyph(id k, SEL c, u32 g, id font, id base) { (void)c; id gi = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(gi, "g", g); pset(gi, "font", font); pset(gi, "base", base); return gi; }
static id gi_with_name(id k, SEL c, id name, id font, id base) { (void)c; id gi = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(gi, "name", name); pset(gi, "font", font); pset(gi, "base", base); return gi; }
static u32 gi_glyph(id self, SEL c) { (void)c; return (u32)pgi(self, "g", 0); }
static id gi_base(id self, SEL c) { (void)c; return pget(self, "base"); }
static id gi_name(id self, SEL c) { (void)c; return pget(self, "name"); }

/* ================================================================ NSTextFinder */
static id tf_client(id self, SEL c) { (void)c; return wget(self); }
static void tf_set_client(id self, SEL c, id cl) { (void)c; wset(self, cl); }
OP(tfContainer, "container") BP(tfIncremental, "incremental", 0) IP(tfStyle, "incStyle", 0)
static id tf_text(id cl) {
    if (responds(cl, "string")) return MSG0(id, cl, "string");
    return 0;
}
static RNG tf_sel(id cl) {
    if (responds(cl, "firstSelectedRange")) return MSG0(RNG, cl, "firstSelectedRange");
    if (responds(cl, "selectedRange")) return MSG0(RNG, cl, "selectedRange");
    return (RNG){0, 0};
}
static void tf_select(id cl, RNG r) {
    if (responds(cl, "setSelectedRanges:")) { id v = MSG(id, (id)kls("NSValue"), "valueWithRange:", r); id a1 = MSG(id, (id)kls("NSArray"), "arrayWithObject:", v); MSG(void, cl, "setSelectedRanges:", a1); }
    else if (responds(cl, "setSelectedRange:")) MSG(void, cl, "setSelectedRange:", r);
    if (responds(cl, "scrollRangeToVisible:")) MSG(void, cl, "scrollRangeToVisible:", r);
}
/* o texto procurado: o da busca guardada (painel) ou o do pasteboard de busca do sistema */
static id tf_query(id self) {
    id q = pget(self, "query");
    if (q && MSG0(u64, q, "length")) return q;
    id pb = MSG(id, (id)kls("NSPasteboard"), "pasteboardWithName:", cx_str("Apple CFPasteboard find"));
    extern id NSPasteboardTypeString;
    id s = pb ? MSG(id, pb, "stringForType:", NSPasteboardTypeString) : 0;
    return s;
}
static void tf_find(id self, int backward) {
    id cl = wget(self), text = tf_text(cl), q = tf_query(self);
    if (!text || !q || !MSG0(u64, q, "length")) { NSBeep(); return; }
    RNG cur = tf_sel(cl);
    u64 n = MSG0(u64, text, "length");
    u64 opts = 1 | (backward ? 4 : 0);   /* NSCaseInsensitiveSearch | NSBackwardsSearch */
    RNG scope = backward ? (RNG){0, cur.loc} : (RNG){cur.loc + cur.len, n - (cur.loc + cur.len)};
    RNG f = MSG(RNG, text, "rangeOfString:options:range:", q, opts, scope);
    if (f.loc == (u64)0x7fffffffffffffffULL) {   /* dá a volta */
        f = MSG(RNG, text, "rangeOfString:options:range:", q, opts, ((RNG){0, n}));
        if (f.loc == (u64)0x7fffffffffffffffULL) { NSBeep(); return; }
    }
    tf_select(cl, f);
}
/* NSTextFinderAction: ShowFindInterface 1, Next 2, Previous 3, ReplaceAll 4, Replace 5, ReplaceAndFind 6,
 * SetSearchString 7, ReplaceAllInSelection 8, SelectAll 9, SelectAllInSelection 10, HideFindInterface 11 */
static void tf_perform(id self, SEL c, i64 action) {
    (void)c;
    id cl = wget(self);
    switch (action) {
    case 1: {   /* a barra de busca: pede o texto num alerta com campo */
        id al = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init"));
        MSG(void, al, "setMessageText:", cx_str("Buscar"));
        id field = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSTextField"), "alloc"), sl("initWithFrame:"), mkr(0, 0, 260, 22)));
        if (tf_query(self)) MSG(void, field, "setStringValue:", tf_query(self));
        MSG(void, al, "setAccessoryView:", field);
        MSG(id, al, "addButtonWithTitle:", cx_str("Próximo")); MSG(id, al, "addButtonWithTitle:", cx_str("Cancelar"));
        if (MSG0(i64, al, "runModal") == 1000) { pset(self, "query", MSG0(id, field, "stringValue")); tf_find(self, 0); }
        break;
    }
    case 2: tf_find(self, 0); break;
    case 3: tf_find(self, 1); break;
    case 7: {   /* usar a seleção como texto de busca */
        id text = tf_text(cl);
        RNG r = tf_sel(cl);
        if (text && r.len) pset(self, "query", MSG(id, text, "substringWithRange:", r));
        break;
    }
    case 9: case 10: {
        id text = tf_text(cl), q = tf_query(self);
        if (!text || !q) { NSBeep(); break; }
        u64 n = MSG0(u64, text, "length");
        RNG f = MSG(RNG, text, "rangeOfString:options:range:", q, (u64)1, ((RNG){0, n}));
        if (f.loc == (u64)0x7fffffffffffffffULL) NSBeep(); else tf_select(cl, f);
        break;
    }
    case 4: case 5: case 6: case 8: {
        id rep = pget(self, "replace");
        id text = tf_text(cl), q = tf_query(self);
        if (!text || !q || !rep || !responds(cl, "replaceCharactersInRange:withString:")) { NSBeep(); break; }
        RNG cur = tf_sel(cl);
        if (action == 5 || action == 6) {
            if (cur.len && MSG(BOOL, MSG(id, text, "substringWithRange:", cur), "caseInsensitiveCompare:", q) == 0) MSG(void, cl, "replaceCharactersInRange:withString:", cur, rep);
            if (action == 6) tf_find(self, 0);
            break;
        }
        RNG scope = action == 8 ? cur : (RNG){0, MSG0(u64, text, "length")};
        for (;;) {
            text = tf_text(cl);
            RNG f = MSG(RNG, text, "rangeOfString:options:range:", q, (u64)(1 | 4), scope);   /* de trás para a frente: as posições não andam */
            if (f.loc == (u64)0x7fffffffffffffffULL) break;
            MSG(void, cl, "replaceCharactersInRange:withString:", f, rep);
            scope.len = f.loc - scope.loc;
        }
        if (responds(cl, "didReplaceCharacters")) MSG0(void, cl, "didReplaceCharacters");
        break;
    }
    case 11: break;
    }
}
static BOOL tf_validate(id self, SEL c, i64 action) {
    (void)c;
    id cl = wget(self);
    if (!cl) return NO;
    if (action >= 4 && action <= 6) return responds(cl, "replaceCharactersInRange:withString:") && (!responds(cl, "isEditable") || MSG0(BOOL, cl, "isEditable"));
    return YES;
}
static void tf_cancel(id self, SEL c) { (void)self; (void)c; }
static void tf_note_changed(id self, SEL c) { (void)self; (void)c; }
static id tf_draw_highlight(id k, SEL c, R r) { (void)k; (void)c; MSG0(void, cx_color(1, 0.9, 0.2, 1), "setFill"); NSRectFill(r); return 0; }
/* NSTextInsertionIndicator */
IP(tiMode, "mode", 0) OP(tiColor, "color") OP(tiEffects, "effects") IP(tiAutoHide, "autoHide", 1)
static void ti_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    if (pgi(self, "mode", 0) == 2) return;   /* NSTextInsertionIndicatorDisplayModeHidden */
    R b = msg_rect(self, "bounds");
    id col = pget(self, "color");
    MSG0(void, col ? col : MSG0(id, (id)kls("NSColor"), "controlAccentColor"), "setFill");
    NSRectFill(mkr(b.x, b.y, b.w > 2 ? 2 : b.w, b.h));
}
/* NSTextCheckingController: verifica o texto do cliente pelo NSSpellChecker */
static id tcc_init(id self, SEL c, id client) { (void)c; wset(self, client); return self; }
static id tcc_client(id self, SEL c) { (void)c; return wget(self); }
static void tcc_check(id self, SEL c, RNG r) {
    (void)c;
    id cl = wget(self);
    id text = cl && responds(cl, "attributedSubstringForProposedRange:actualRange:") ? MSG(id, cl, "attributedSubstringForProposedRange:actualRange:", r, (RNG *)0) : 0;
    if (!text) return;
    id sc = MSG0(id, (id)kls("NSSpellChecker"), "sharedSpellChecker");
    RNG bad = MSG(RNG, sc, "checkSpellingOfString:startingAt:", MSG0(id, text, "string"), (i64)0);
    pset(self, "lastBad", MSG(id, (id)kls("NSValue"), "valueWithRange:", ((RNG){r.loc + bad.loc, bad.len})));
}
static void tcc_noop(id self, SEL c) { (void)self; (void)c; }
static void tcc_noop1(id self, SEL c, id x) { (void)self; (void)c; (void)x; }
/* NSTextAttachmentViewProvider */
static id tavp_init(id self, SEL c, id att, id parent, id layout, id loc) { (void)c; pset(self, "att", att); pset(self, "parent", parent); pset(self, "layout", layout); pset(self, "loc", loc); return self; }
OP(tavpView, "view") BP(tavpTracks, "tracks", 0)
static id tavp_att(id self, SEL c) { (void)c; return pget(self, "att"); }
static id tavp_loc(id self, SEL c) { (void)c; return pget(self, "loc"); }
static void tavp_load(id self, SEL c) { (void)self; (void)c; }
static R tavp_bounds(id self, SEL c, id attrs, id loc, id tc, P pos) { (void)c; (void)attrs; (void)loc; (void)tc; (void)pos; id v = pget(self, "view"); return v ? msg_rect(v, "bounds") : mkr(0, 0, 0, 0); }

/* ================================================================ NSTreeNode */
static id tn_new(id k, SEL c, id obj) { (void)c; id n = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(n, "obj", obj); pset(n, "kids", arr()); return n; }
static id tn_init(id self, SEL c, id obj) { (void)c; pset(self, "obj", obj); pset(self, "kids", arr()); return self; }
static id tn_obj(id self, SEL c) { (void)c; return pget(self, "obj"); }
static id tn_kids(id self, SEL c) { (void)c; id a = pget(self, "kids"); return a ? MSG0(id, a, "copy") : MSG0(id, (id)kls("NSArray"), "array"); }
static id tn_parent(id self, SEL c) { (void)c; return wget(self); }
static BOOL tn_leaf(id self, SEL c) { (void)c; id a = pget(self, "kids"); return !a || !MSG0(u64, a, "count"); }
/* mutableChildNodes: um proxy simples — o próprio array, com o pai ajustado em cada mudança feita pelos nossos métodos;
 * quem mexe direto no array pede ao pai por -_cxAdopt */
static id tn_mkids(id self, SEL c) { (void)c; id a = pget(self, "kids"); if (!a) { a = arr(); pset(self, "kids", a); } for (u64 i = 0, n = MSG0(u64, a, "count"); i < n; i++) wset(MSG(id, a, "objectAtIndex:", i), self); return a; }
static id tn_index_path(id self, SEL c) {
    (void)c;
    u64 idx[64]; int n = 0;
    for (id cur = self, p = wget(cur); p && n < 64; cur = p, p = wget(p)) {
        idx[n++] = MSG(u64, pget(p, "kids"), "indexOfObjectIdenticalTo:", cur);
    }
    u64 rev[64];
    for (int i = 0; i < n; i++) rev[i] = idx[n - 1 - i];
    return MSG(id, (id)kls("NSIndexPath"), "indexPathWithIndexes:length:", rev, (u64)n);
}
static id tn_descendant(id self, SEL c, id ip) {
    (void)c;
    id cur = self;
    for (u64 i = 0, n = ip ? MSG0(u64, ip, "length") : 0; i < n && cur; i++) {
        u64 k = MSG(u64, ip, "indexAtPosition:", i);
        id kids = pget(cur, "kids");
        cur = kids && k < MSG0(u64, kids, "count") ? MSG(id, kids, "objectAtIndex:", k) : 0;
    }
    return cur;
}
static void tn_sort(id self, SEL c, id descs, BOOL recursive) {
    (void)c;
    id kids = pget(self, "kids");
    if (!kids || !descs) return;
    /* ordena pelos representedObject */
    u64 n = MSG0(u64, kids, "count");
    for (u64 i = 1; i < n; i++)
        for (u64 j = i; j > 0; j--) {
            id a = MSG(id, kids, "objectAtIndex:", j - 1), b = MSG(id, kids, "objectAtIndex:", j);
            i64 r = 0;
            for (u64 d = 0, nd = MSG0(u64, descs, "count"); d < nd && !r; d++)
                r = MSG(i64, MSG(id, descs, "objectAtIndex:", d), "compareObject:toObject:", pget(a, "obj"), pget(b, "obj"));
            if (r <= 0) break;
            MSG(void, kids, "exchangeObjectAtIndex:withObjectAtIndex:", j - 1, j);
        }
    if (recursive) for (u64 i = 0; i < n; i++) tn_sort(MSG(id, kids, "objectAtIndex:", i), 0, descs, YES);
}

/* ================================================================ NSTreeController */
static id tc_init(id self, SEL c) { (void)c; psi(self, "editable", 1); pset(self, "selIPs", arr()); return self; }
static id tc_init_content(id self, SEL c, id content) { tc_init(self, c); pset(self, "content", content); return self; }
OP(tcChildrenKP, "childrenKeyPath") OP(tcCountKP, "countKeyPath") OP(tcLeafKP, "leafKeyPath") OP(tcSortDescs, "sortDescriptors")
BP(tcPreserves, "preserves", 1) BP(tcAvoidsEmpty, "avoidsEmpty", 1) BP(tcAlwaysUsesMulti, "alwaysMulti", 0) BP(tcSelectsInserted, "selectsInserted", 1)
static id children_of(id self, id obj) {
    id kp = pget(self, "childrenKeyPath");
    if (!obj || !kp) return 0;
    id leafkp = pget(self, "leafKeyPath");
    if (leafkp && MSG0(BOOL, MSG(id, obj, "valueForKeyPath:", leafkp), "boolValue")) return 0;
    return MSG(id, obj, "valueForKeyPath:", kp);
}
static id build_node(id self, id obj) {
    id n = tn_new((id)&NSTreeNode_c, 0, obj);
    id kids = children_of(self, obj);
    id descs = pget(self, "sortDescriptors");
    if (kids && descs && MSG0(u64, descs, "count")) kids = MSG(id, kids, "sortedArrayUsingDescriptors:", descs);
    for (u64 i = 0, k = kids ? MSG0(u64, kids, "count") : 0; i < k; i++) {
        id child = build_node(self, MSG(id, kids, "objectAtIndex:", i));
        wset(child, n);
        MSG(void, pget(n, "kids"), "addObject:", child);
    }
    return n;
}
static id tc_content(id self, SEL c) { (void)c; return pget(self, "content"); }
static void tc_set_content(id self, SEL c, id content) {
    (void)c;
    if (responds(self, "willChangeValueForKey:")) MSG(void, self, "willChangeValueForKey:", cx_str("arrangedObjects"));
    pset(self, "content", content);
    pset(self, "root", 0);
    if (responds(self, "didChangeValueForKey:")) MSG(void, self, "didChangeValueForKey:", cx_str("arrangedObjects"));
}
static id tc_arranged(id self, SEL c) {
    (void)c;
    id root = pget(self, "root");
    if (root) return root;
    root = tn_new((id)&NSTreeNode_c, 0, 0);
    id content = pget(self, "content");
    id tops = content ? (MSG(BOOL, content, "isKindOfClass:", (id)kls("NSArray")) ? content : MSG(id, (id)kls("NSArray"), "arrayWithObject:", content)) : 0;
    id descs = pget(self, "sortDescriptors");
    if (tops && descs && MSG0(u64, descs, "count")) tops = MSG(id, tops, "sortedArrayUsingDescriptors:", descs);
    for (u64 i = 0, n = tops ? MSG0(u64, tops, "count") : 0; i < n; i++) {
        id child = build_node(self, MSG(id, tops, "objectAtIndex:", i));
        wset(child, root);
        MSG(void, pget(root, "kids"), "addObject:", child);
    }
    pset(self, "root", root);
    return root;
}
static void tc_rearrange(id self, SEL c) { (void)c; if (responds(self, "willChangeValueForKey:")) MSG(void, self, "willChangeValueForKey:", cx_str("arrangedObjects")); pset(self, "root", 0); if (responds(self, "didChangeValueForKey:")) MSG(void, self, "didChangeValueForKey:", cx_str("arrangedObjects")); }
static id tc_sel_ips(id self, SEL c) { (void)c; return MSG0(id, pget(self, "selIPs"), "copy"); }
static BOOL tc_set_sel_ips(id self, SEL c, id ips) {
    (void)c;
    if (responds(self, "willChangeValueForKey:")) { MSG(void, self, "willChangeValueForKey:", cx_str("selectionIndexPaths")); MSG(void, self, "willChangeValueForKey:", cx_str("selectedObjects")); }
    pset(self, "selIPs", ips ? MSG0(id, ips, "mutableCopy") : arr());
    if (ips) objc_release(pget(self, "selIPs"));
    if (responds(self, "didChangeValueForKey:")) { MSG(void, self, "didChangeValueForKey:", cx_str("selectionIndexPaths")); MSG(void, self, "didChangeValueForKey:", cx_str("selectedObjects")); }
    return YES;
}
static id tc_sel_ip(id self, SEL c) { (void)c; id a = pget(self, "selIPs"); return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : 0; }
static BOOL tc_set_sel_ip(id self, SEL c, id ip) { return tc_set_sel_ips(self, c, ip ? MSG(id, (id)kls("NSArray"), "arrayWithObject:", ip) : 0); }
static id tc_sel_nodes(id self, SEL c) {
    (void)c;
    id out = arr(), ips = pget(self, "selIPs"), root = tc_arranged(self, 0);
    for (u64 i = 0, n = MSG0(u64, ips, "count"); i < n; i++) { id node = tn_descendant(root, 0, MSG(id, ips, "objectAtIndex:", i)); if (node) MSG(void, out, "addObject:", node); }
    return out;
}
static id tc_sel_objects(id self, SEL c) {
    (void)c;
    id out = arr(), nodes = tc_sel_nodes(self, 0);
    for (u64 i = 0, n = MSG0(u64, nodes, "count"); i < n; i++) { id o = pget(MSG(id, nodes, "objectAtIndex:", i), "obj"); if (o) MSG(void, out, "addObject:", o); }
    return out;
}
/* inserir: no array de filhos do pai do índice (ou no conteúdo de topo) */
static id mutable_children(id self, id ip_parent) {
    if (!ip_parent || !MSG0(u64, ip_parent, "length")) {
        id content = pget(self, "content");
        if (!content || !MSG(BOOL, content, "isKindOfClass:", (id)kls("NSMutableArray"))) {
            id m = content ? MSG0(id, content, "mutableCopy") : MSG0(id, (id)kls("NSMutableArray"), "new");
            pset(self, "content", m); objc_release(m);
        }
        return pget(self, "content");
    }
    id node = tn_descendant(tc_arranged(self, 0), 0, ip_parent);
    id obj = node ? pget(node, "obj") : 0;
    id kp = pget(self, "childrenKeyPath");
    return obj && kp ? MSG(id, obj, "mutableArrayValueForKeyPath:", kp) : 0;
}
static void tc_insert(id self, SEL c, id obj, id ip) {
    (void)c;
    if (!obj || !ip || !MSG0(u64, ip, "length")) return;
    u64 len = MSG0(u64, ip, "length");
    id parent_ip = MSG0(id, ip, "indexPathByRemovingLastIndex");
    id kids = mutable_children(self, len > 1 ? parent_ip : 0);
    u64 at = MSG(u64, ip, "indexAtPosition:", len - 1);
    if (!kids) return;
    if (at > MSG0(u64, kids, "count")) at = MSG0(u64, kids, "count");
    MSG(void, kids, "insertObject:atIndex:", obj, at);
    tc_rearrange(self, 0);
    if (pgi(self, "selectsInserted", 1)) tc_set_sel_ip(self, 0, ip);
}
static void tc_insert_objects(id self, SEL c, id objs, id ips) {
    (void)c;
    for (u64 i = 0, n = objs ? MSG0(u64, objs, "count") : 0; i < n; i++) tc_insert(self, 0, MSG(id, objs, "objectAtIndex:", i), MSG(id, ips, "objectAtIndex:", i));
}
static void tc_remove_at(id self, SEL c, id ip) {
    (void)c;
    if (!ip || !MSG0(u64, ip, "length")) return;
    u64 len = MSG0(u64, ip, "length");
    id kids = mutable_children(self, len > 1 ? MSG0(id, ip, "indexPathByRemovingLastIndex") : 0);
    u64 at = MSG(u64, ip, "indexAtPosition:", len - 1);
    if (kids && at < MSG0(u64, kids, "count")) MSG(void, kids, "removeObjectAtIndex:", at);
    tc_rearrange(self, 0);
    tc_set_sel_ips(self, 0, 0);
}
static void tc_remove_ips(id self, SEL c, id ips) {
    (void)c;
    id sorted = MSG(id, ips, "sortedArrayUsingSelector:", sl("compare:"));
    for (u64 i = MSG0(u64, sorted, "count"); i-- > 0;) tc_remove_at(self, 0, MSG(id, sorted, "objectAtIndex:", i));
}
static void tc_remove(id self, SEL c, id sender) { (void)c; (void)sender; tc_remove_ips(self, 0, MSG0(id, pget(self, "selIPs"), "copy")); }
static id tc_new_object(id self, SEL c) { (void)c; id k = pget(self, "objectClass"); return MSG0(id, MSG0(id, k ? k : (id)kls("NSMutableDictionary"), "alloc"), "init"); }
static void tc_add(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id o = MSG0(id, self, "newObject");
    id content = pget(self, "content");
    u64 n = content && responds(content, "count") ? MSG0(u64, content, "count") : 0;
    tc_insert(self, 0, o, MSG(id, (id)kls("NSIndexPath"), "indexPathWithIndex:", n));
    objc_release(o);
}
static void tc_add_child(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id sel = tc_sel_ip(self, 0);
    if (!sel) { tc_add(self, 0, 0); return; }
    id node = tn_descendant(tc_arranged(self, 0), 0, sel);
    id kids = node ? pget(node, "kids") : 0;
    id o = MSG0(id, self, "newObject");
    tc_insert(self, 0, o, MSG(id, sel, "indexPathByAddingIndex:", kids ? MSG0(u64, kids, "count") : 0));
    objc_release(o);
}
static void tc_insert_sel(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id sel = tc_sel_ip(self, 0);
    id o = MSG0(id, self, "newObject");
    tc_insert(self, 0, o, sel ? sel : MSG(id, (id)kls("NSIndexPath"), "indexPathWithIndex:", (u64)0));
    objc_release(o);
}
static void tc_insert_child(id self, SEL c, id sender) { tc_add_child(self, c, sender); }
static BOOL tc_can_add(id self, SEL c) { (void)c; return pgi(self, "editable", 1) != 0; }
static BOOL tc_can_remove(id self, SEL c) { (void)c; return pgi(self, "editable", 1) && MSG0(u64, pget(self, "selIPs"), "count") > 0; }
static BOOL tc_can_add_child(id self, SEL c) { (void)c; return pgi(self, "editable", 1) && pget(self, "childrenKeyPath") != 0; }
static id tc_child_kp_for_node(id self, SEL c, id node) { (void)node; return g_tcChildrenKP(self, c); }
static id tc_count_kp_for_node(id self, SEL c, id node) { (void)node; return g_tcCountKP(self, c); }
static id tc_leaf_kp_for_node(id self, SEL c, id node) { (void)node; return g_tcLeafKP(self, c); }
static void tc_move_node(id self, SEL c, id node, id ip) {
    (void)c;
    id obj = node ? pget(node, "obj") : 0;
    id from = node ? tn_index_path(node, 0) : 0;
    if (!obj || !from) return;
    objc_retain(obj);
    tc_remove_at(self, 0, from);
    tc_insert(self, 0, obj, ip);
    objc_release(obj);
}

/* ================================================================ NSInputManager, NSHelpManager */
static id im_current(id k, SEL c) { (void)c; static id s; if (!s) { s = cx_alloc_instance((cx_class_t *)k); pset(s, "name", cx_str("mOS")); } return s; }
static void im_cycle(id k, SEL c, id sender) { (void)k; (void)c; (void)sender; }
static id im_name(id self, SEL c) { (void)c; return pget(self, "name"); }
static BOOL im_no(id self, SEL c) { (void)self; (void)c; return NO; }
static BOOL im_wants(id self, SEL c, id ev) { (void)self; (void)c; (void)ev; return NO; }
static id im_image(id self, SEL c) { (void)self; (void)c; return 0; }
static id im_lang(id self, SEL c) { (void)self; (void)c; return cx_str("pt_BR"); }
static id hm_shared(id k, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)k); return s; }
static BOOL g_ctx_help;
static BOOL hm_is_ctx(id k, SEL c) { (void)k; (void)c; return g_ctx_help; }
static void hm_set_ctx(id k, SEL c, BOOL b) { (void)k; (void)c; g_ctx_help = b; }
static char k_help;
static void hm_set_help(id self, SEL c, id help, id obj) { (void)self; (void)c; if (obj) objc_setAssociatedObject(obj, &k_help, help, 1); }
static void hm_remove_help(id self, SEL c, id obj) { (void)self; (void)c; if (obj) objc_setAssociatedObject(obj, &k_help, 0, 1); }
static id hm_help_for(id self, SEL c, id obj) { (void)self; (void)c; return obj ? objc_getAssociatedObject(obj, &k_help) : 0; }
static BOOL hm_show_help(id self, SEL c, id obj, P hint) {
    (void)self; (void)c; (void)hint;
    id help = obj ? objc_getAssociatedObject(obj, &k_help) : 0;
    if (!help) return NO;
    id al = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init"));
    MSG(void, al, "setMessageText:", cx_str("Ajuda"));
    id txt = MSG(BOOL, help, "isKindOfClass:", (id)kls("NSAttributedString")) ? MSG0(id, help, "string") : help;
    MSG(void, al, "setInformativeText:", txt);
    MSG0(i64, al, "runModal");
    return YES;
}
/* o livro de ajuda do app: CFBundleHelpBookFolder dentro de Resources; a âncora vira #âncora no index */
static id help_index(void) {
    id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id folder = MSG(id, mb, "objectForInfoDictionaryKey:", cx_str("CFBundleHelpBookFolder"));
    if (!folder) return 0;
    id res = MSG0(id, mb, "resourcePath");
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    const char *cands[] = {"index.html", "index.htm", "pt.lproj/index.html", "en.lproj/index.html", "Base.lproj/index.html", 0};
    for (int i = 0; cands[i]; i++) {
        id p = MSG(id, MSG(id, res, "stringByAppendingPathComponent:", folder), "stringByAppendingPathComponent:", cx_str(cands[i]));
        if (MSG(BOOL, fm, "fileExistsAtPath:", p)) return p;
    }
    return 0;
}
static void hm_open_anchor(id self, SEL c, id anchor, id book) {
    (void)self; (void)c; (void)book;
    id idx = help_index();
    if (!idx) { NSBeep(); return; }
    id url = MSG(id, (id)kls("NSURL"), "fileURLWithPath:", idx);
    if (anchor) { id s = MSG(id, MSG0(id, url, "absoluteString"), "stringByAppendingFormat:", cx_str("#%@"), anchor); url = MSG(id, (id)kls("NSURL"), "URLWithString:", s); }
    MSG(BOOL, MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace"), "openURL:", url);
}
static void hm_find(id self, SEL c, id query, id book) { hm_open_anchor(self, c, 0, book); (void)query; }
static BOOL hm_register(id self, SEL c, id bundle) { (void)self; (void)c; (void)bundle; return help_index() != 0; }

/* ================================================================ image reps */
static id cir_init_sel(id self, SEL c, SEL draw, id delegate) { (void)c; pset(self, "sel", draw ? cx_str(sel_getName(draw)) : 0); wset(self, delegate); return self; }
static id cir_init_handler(id self, SEL c, S z, BOOL flipped, void *handler) { (void)c; pset(self, "size", MSG(id, (id)kls("NSValue"), "valueWithSize:", z)); psi(self, "flipped", flipped); pset(self, "handler", block_box(handler)); return self; }
static SEL cir_sel(id self, SEL c) { (void)c; id v = pget(self, "sel"); return v ? sl(cx_cstr(v)) : 0; }
static id cir_delegate(id self, SEL c) { (void)c; return wget(self); }
static void *cir_handler(id self, SEL c) { (void)c; return block_of(pget(self, "handler")); }
static S cir_size(id self, SEL c) { (void)c; id v = pget(self, "size"); return v ? ((S (*)(id, SEL))objc_msgSend)(v, sl("sizeValue")) : (S){0, 0}; }
static void cir_set_size(id self, SEL c, S z) { (void)c; pset(self, "size", MSG(id, (id)kls("NSValue"), "valueWithSize:", z)); }
static BOOL cir_draw_in(id self, SEL c, R r) {
    (void)c;
    void *h = block_of(pget(self, "handler"));
    if (h) return ((BOOL (*)(void *, R))block_fn(h))(h, r);
    id d = wget(self);
    SEL s = cir_sel(self, 0);
    if (d && s) { ((void (*)(id, SEL, id))objc_msgSend)(d, s, self); return YES; }
    return NO;
}
static BOOL cir_draw(id self, SEL c) { S z = cir_size(self, 0); return cir_draw_in(self, c, mkr(0, 0, z.w, z.h)); }
static BOOL cir_draw_at(id self, SEL c, P p) { S z = cir_size(self, 0); return cir_draw_in(self, c, mkr(p.x, p.y, z.w, z.h)); }
/* PICT: o cabeçalho de 512 bytes, depois picSize (2) e o picFrame (top, left, bottom, right em 16 bits big-endian) */
static id pict_init(id self, SEL c, id data) {
    (void)c;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    const u8 *b = data ? MSG0(const u8 *, data, "bytes") : 0;
    u64 off = n >= 522 && !(b[0] == 0 && b[1] == 0x0a) ? 512 : 0;   /* com ou sem o cabeçalho de arquivo */
    if (!b || n < off + 10) { objc_release(self); return 0; }
    i16 top = (i16)(b[off + 2] << 8 | b[off + 3]), left = (i16)(b[off + 4] << 8 | b[off + 5]);
    i16 bottom = (i16)(b[off + 6] << 8 | b[off + 7]), right = (i16)(b[off + 8] << 8 | b[off + 9]);
    if (bottom <= top || right <= left) { objc_release(self); return 0; }
    pset(self, "data", data);
    pset(self, "box", MSG(id, (id)kls("NSValue"), "valueWithRect:", mkr(left, top, right - left, bottom - top)));
    id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithData:", data);   /* o ImageIO decodifica se souber */
    if (img) { pset(self, "img", img); objc_release(img); }
    return self;
}
static id pict_with(id k, SEL c, id data) { id r = pict_init(cx_alloc_instance((cx_class_t *)k), c, data); return r ? objc_autorelease(r) : 0; }
static id pict_data(id self, SEL c) { (void)c; return pget(self, "data"); }
static R pict_box(id self, SEL c) { (void)c; id v = pget(self, "box"); return v ? ((R (*)(id, SEL))objc_msgSend_stret)(v, sl("rectValue")) : mkr(0, 0, 0, 0); }
static S pict_size(id self, SEL c) { R b = pict_box(self, c); return (S){b.w, b.h}; }
static BOOL pict_draw_in(id self, SEL c, R r) {
    (void)c;
    id img = pget(self, "img");
    if (img) { ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(img, sl("drawInRect:fromRect:operation:fraction:"), r, mkr(0, 0, 0, 0), (u64)2, 1.0); return YES; }
    MSG0(void, cx_color(0.9, 0.9, 0.9, 1), "setFill"); NSRectFill(r);   /* sem decodificador: o quadro do tamanho certo */
    return NO;
}
static BOOL pict_draw(id self, SEL c) { S z = pict_size(self, 0); return pict_draw_in(self, c, mkr(0, 0, z.w, z.h)); }

/* ================================================================ vidro, pressão, acessório do slider, selo, segue */
OP(geContent, "content") DP(geRadius, "radius", 0) OP(geTint, "tint") IP(geStyle, "style", 0) DP(geSpacing, "spacing", 0)
static void ge_set_content(id self, SEL c, id v) {
    (void)c;
    id old = pget(self, "content");
    if (old && old != v) MSG0(void, old, "removeFromSuperview");
    pset(self, "content", v);
    if (v) { ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), msg_rect(self, "bounds")); MSG(void, self, "addSubview:", v); }
}
static void ge_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    R b = msg_rect(self, "bounds");
    double rad = pgd(self, "radius", 0);
    id p = ((id (*)(id, SEL, R, double, double))objc_msgSend)((id)kls("NSBezierPath"), sl("bezierPathWithRoundedRect:xRadius:yRadius:"), b, rad, rad);
    id tint = pget(self, "tint");
    MSG0(void, tint ? MSG(id, tint, "colorWithAlphaComponent:", 0.25) : cx_color(1, 1, 1, pgi(self, "style", 0) == 1 ? 0.35 : 0.55), "setFill");
    MSG0(void, p, "fill");
    MSG0(void, cx_color(1, 1, 1, 0.6), "setStroke"); MSG0(void, p, "stroke");
}
static id pc_init(id self, SEL c, i64 behavior) { (void)c; psi(self, "behavior", behavior); return self; }
static i64 pc_behavior(id self, SEL c) { (void)c; return pgi(self, "behavior", 0); }
static void pc_set(id self, SEL c) { (void)self; (void)c; }   /* sem Force Touch: a configuração vale para o evento seguinte */
static id sa_with_image(id k, SEL c, id img) { (void)c; id a = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(a, "image", img); psi(a, "enabled", 1); return a; }
OP(saBehavior, "behavior") BP(saEnabled, "enabled", 1)
static id sab_auto(id k, SEL c) { (void)c; id b = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(b, "kind", 0); return b; }
static id sab_value_step(id k, SEL c) { (void)c; id b = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(b, "kind", 1); return b; }
static id sab_value_reset(id k, SEL c) { (void)c; id b = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(b, "kind", 2); return b; }
static id sab_with_target(id k, SEL c, id t, SEL a) { (void)c; id b = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(b, "kind", 3); wset(b, t); pset(b, "action", a ? cx_str(sel_getName(a)) : 0); return b; }
static id sab_with_handler(id k, SEL c, void *h) { (void)c; id b = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(b, "kind", 4); pset(b, "handler", block_box(h)); return b; }
static void sab_handle(id self, SEL c, id sender) {
    (void)c;
    i64 kind = pgi(self, "kind", 0);
    if (kind == 3) { id v = pget(self, "action"); if (v) MSG(BOOL, NSApp, "sendAction:to:from:", sl(cx_cstr(v)), wget(self), sender); }
    else if (kind == 4) { void *h = block_of(pget(self, "handler")); if (h) ((void (*)(void *, id))block_fn(h))(h, sender); }
}
static id mb_init_count(id self, SEL c, i64 n) { (void)c; psi(self, "count", n); psi(self, "type", 0); return self; }
static id mb_init_string(id self, SEL c, id s) { (void)c; pset(self, "string", s); psi(self, "type", 0); return self; }
static id mb_init_type(id self, SEL c, i64 n, i64 type) { (void)c; psi(self, "count", n); psi(self, "type", type); return self; }
static id mb_updates(id k, SEL c, i64 n) { (void)c; return objc_autorelease(mb_init_type(cx_alloc_instance((cx_class_t *)k), 0, n, 1)); }
static id mb_new_items(id k, SEL c, i64 n) { (void)c; return objc_autorelease(mb_init_type(cx_alloc_instance((cx_class_t *)k), 0, n, 2)); }
static id mb_alerts(id k, SEL c, i64 n) { (void)c; return objc_autorelease(mb_init_type(cx_alloc_instance((cx_class_t *)k), 0, n, 3)); }
static i64 mb_count(id self, SEL c) { (void)c; return pgi(self, "count", 0); }
static id mb_string(id self, SEL c) {
    (void)c;
    id s = pget(self, "string");
    if (s) return s;
    char b[32]; snprintf(b, sizeof b, "%lld", (long long)pgi(self, "count", 0));
    return cx_str(b);
}
static i64 mb_type(id self, SEL c) { (void)c; return pgi(self, "type", 0); }
static id seg_init(id self, SEL c, id ident, id src, id dst) { (void)c; pset(self, "ident", ident); pset(self, "src", src); pset(self, "dst", dst); return self; }
static id seg_ident(id self, SEL c) { (void)c; return pget(self, "ident"); }
static id seg_src(id self, SEL c) { (void)c; return pget(self, "src"); }
static id seg_dst(id self, SEL c) { (void)c; return pget(self, "dst"); }
static id seg_with_handler(id k, SEL c, id ident, id src, id dst, void *h) { id s = objc_autorelease(seg_init(cx_alloc_instance((cx_class_t *)k), c, ident, src, dst)); pset(s, "handler", block_box(h)); return s; }
static void seg_perform(id self, SEL c) {
    (void)c;
    void *h = block_of(pget(self, "handler"));
    if (h) { ((void (*)(void *))block_fn(h))(h); return; }
    id src = pget(self, "src"), dst = pget(self, "dst");
    if (!dst) return;
    if (MSG(BOOL, dst, "isKindOfClass:", (id)kls("NSWindowController"))) { MSG(void, dst, "showWindow:", src); return; }
    if (MSG(BOOL, dst, "isKindOfClass:", (id)kls("NSViewController")) && src && responds(src, "presentViewControllerAsModalWindow:")) { MSG(void, src, "presentViewControllerAsModalWindow:", dst); return; }
}

/* ================================================================ ações de linha, touch bar, invalidação, tint... */
static id tra_with(id k, SEL c, i64 style, id title, void *h) { (void)c; id a = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(a, "style", style); pset(a, "title", title); pset(a, "handler", block_box(h)); return a; }
OP(traTitle, "title") OP(traBg, "bg") OP(traImage, "image")
static i64 tra_style(id self, SEL c) { (void)c; return pgi(self, "style", 0); }
static void *tra_handler(id self, SEL c) { (void)c; return block_of(pget(self, "handler")); }
static id btbi_with_title(id k, SEL c, id ident, id title, id target, SEL action) {
    (void)c;
    id it = MSG(id, MSG0(id, k, "alloc"), "initWithIdentifier:", ident);
    pset(it, "title", title); wset(it, target); pset(it, "action", action ? cx_str(sel_getName(action)) : 0); psi(it, "enabled", 1);
    return objc_autorelease(it);
}
static id btbi_with_image(id k, SEL c, id ident, id img, id target, SEL action) { id it = btbi_with_title(k, c, ident, 0, target, action); pset(it, "image", img); return it; }
static id btbi_with_both(id k, SEL c, id ident, id title, id img, id target, SEL action) { id it = btbi_with_title(k, c, ident, title, target, action); pset(it, "image", img); return it; }
OP(btTitle, "title") OP(btImage, "image") OP(btBezel, "bezelColor") BP(btEnabled, "enabled", 1)
static id bt_target(id self, SEL c) { (void)c; return wget(self); }
static void bt_set_target(id self, SEL c, id t) { (void)c; wset(self, t); }
static SEL bt_action(id self, SEL c) { (void)c; id v = pget(self, "action"); return v ? sl(cx_cstr(v)) : 0; }
static void bt_set_action(id self, SEL c, SEL a) { (void)c; pset(self, "action", a ? cx_str(sel_getName(a)) : 0); }
BP(fliMetrics, "metrics", 0) BP(fliAttrs, "attrs", 0)
OP(tcfAccent, "accent")
static id tcf_default(id k, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)k); return s; }
static id tcf_with_color(id k, SEL c, id col) { (void)c; id t = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(t, "base", col); return t; }
static id tcf_base(id self, SEL c) { (void)c; return pget(self, "base"); }
static BOOL tcf_adapts(id self, SEL c) { (void)c; return pget(self, "base") == 0; }
static id ttm_shared(id k, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)k); return s; }
static void ttm_noop(id self, SEL c) { (void)self; (void)c; }
static void ttm_order_out(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; }
static BOOL wtc_available(id k, SEL c) { (void)k; (void)c; return NO; }   /* o mOS não tem as Ferramentas de Escrita (modelo de linguagem) */
OP(wtcDelegate, "delegate") IP(wtcBehavior, "behavior", 0) IP(wtcResult, "result", 0)
static id am_shared(id k, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)k); return s; }
static void am_noop1(id self, SEL c, id x) { (void)self; (void)c; (void)x; }
OP(svaView, "view") IP(svaAttr, "attr", 0) BP(svaHidden, "hidden", 0)
static id sr_init(id self, SEL c) { (void)c; psi(self, "listen", 0); return self; }
OP(srCommands, "commands") OP(srTitle, "title") OP(srDelegate, "delegate") BP(srForeground, "foreground", 0) BP(srBlocks, "blocks", 0)
static void sr_start(id self, SEL c) { (void)c; psi(self, "listen", 1); }
static void sr_stop(id self, SEL c) { (void)c; psi(self, "listen", 0); }

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
#define PROP(k, get, set, fn) ADD(k, get, g_##fn); ADD(k, set, s_##fn)
static cx_class_t *ensure(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    cx_class_t *s = kls(super);
    return cx_define_class(k, m, name, s ? s : kls("NSObject"), 0);
}
void cx_appkit_more2_init(void) {
    cx_class_t *k;
    k = ensure(&NSTypesetter_c, &NSTypesetter_m, "NSTypesetter", "NSObject");
    cx_class_t *ats = ensure(&NSATSTypesetter_c, &NSATSTypesetter_m, "NSATSTypesetter", "NSTypesetter");
    cx_add_cmethod(k, "sharedSystemTypesetter", (void *)ts_shared); cx_add_cmethod(k, "defaultTypesetterBehavior", (void *)ts_default_behavior);
    cx_add_cmethod(ats, "sharedTypesetter", (void *)ts_shared);
    PROP(k, "usesFontLeading", "setUsesFontLeading:", tsLeading); PROP(k, "typesetterBehavior", "setTypesetterBehavior:", tsBehavior);
    PROP(k, "lineFragmentPadding", "setLineFragmentPadding:", tsPad); PROP(k, "hyphenationFactor", "setHyphenationFactor:", tsHyph);
    PROP(k, "bidiProcessingEnabled", "setBidiProcessingEnabled:", tsBidi); PROP(k, "usesScreenFonts", "setUsesScreenFonts:", tsScreen);
    ADD(k, "layoutManager", ts_lm); ADD(k, "attributedString", ts_attrs_str);
    ADD(k, "lineSpacingAfterGlyphAtIndex:withProposedLineFragmentRect:", ts_line_spacing_after);
    ADD(k, "paragraphSpacingBeforeGlyphAtIndex:withProposedLineFragmentRect:", ts_para_before);
    ADD(k, "paragraphSpacingAfterGlyphAtIndex:withProposedLineFragmentRect:", ts_para_after);
    ADD(k, "layoutGlyphsInLayoutManager:startingAtGlyphIndex:maxNumberOfLineFragments:nextGlyphIndex:", ts_layout_glyphs);
    ADD(k, "layoutCharactersInRange:forLayoutManager:maximumNumberOfLineFragments:", ts_layout_chars);
    ADD(k, "boundingBoxForControlGlyphAtIndex:forTextContainer:proposedLineFragment:glyphPosition:characterIndex:", ts_glyph_extent);
    k = ensure(&NSGlyphGenerator_c, &NSGlyphGenerator_m, "NSGlyphGenerator", "NSObject");
    cx_add_cmethod(k, "sharedGlyphGenerator", (void *)gg_shared);
    ADD(k, "generateGlyphsForGlyphStorage:desiredNumberOfCharacters:glyphIndex:characterIndex:", gg_generate);
    k = ensure(&NSGlyphInfo_c, &NSGlyphInfo_m, "NSGlyphInfo", "NSObject");
    cx_add_cmethod(k, "glyphInfoWithCGGlyph:forFont:baseString:", (void *)gi_with_glyph);
    cx_add_cmethod(k, "glyphInfoWithGlyph:forFont:baseString:", (void *)gi_with_glyph);
    cx_add_cmethod(k, "glyphInfoWithGlyphName:forFont:baseString:", (void *)gi_with_name);
    ADD(k, "glyphID", gi_glyph); ADD(k, "baseString", gi_base); ADD(k, "glyphName", gi_name);
    k = ensure(&NSTextFinder_c, &NSTextFinder_m, "NSTextFinder", "NSObject");
    ADD(k, "client", tf_client); ADD(k, "setClient:", tf_set_client);
    PROP(k, "findBarContainer", "setFindBarContainer:", tfContainer);
    PROP(k, "isIncrementalSearchingEnabled", "setIncrementalSearchingEnabled:", tfIncremental);
    PROP(k, "incrementalSearchingShouldDimContentView", "setIncrementalSearchingShouldDimContentView:", tfIncremental);
    ADD(k, "performAction:", tf_perform); ADD(k, "validateAction:", tf_validate); ADD(k, "cancelFindIndicator", tf_cancel);
    ADD(k, "noteClientStringWillChange", tf_note_changed); ADD(k, "incrementalMatchRanges", (void *)arr);
    cx_add_cmethod(k, "drawIncrementalMatchHighlightInRect:", (void *)tf_draw_highlight);
    k = ensure(&NSTextInsertionIndicator_c, &NSTextInsertionIndicator_m, "NSTextInsertionIndicator", "NSView");
    PROP(k, "displayMode", "setDisplayMode:", tiMode); PROP(k, "color", "setColor:", tiColor);
    PROP(k, "effectsViewInserter", "setEffectsViewInserter:", tiEffects); PROP(k, "automaticModeOptions", "setAutomaticModeOptions:", tiAutoHide);
    ADD(k, "drawRect:", ti_draw);
    k = ensure(&NSTextCheckingController_c, &NSTextCheckingController_m, "NSTextCheckingController", "NSObject");
    ADD(k, "initWithClient:", tcc_init); ADD(k, "client", tcc_client); ADD(k, "checkTextInRange:types:options:", tcc_check);
    ADD(k, "checkTextInDocument:", tcc_noop1); ADD(k, "checkTextInSelection:", tcc_noop1); ADD(k, "invalidate", tcc_noop);
    ADD(k, "didChangeTextInRange:", tcc_check); ADD(k, "considerTextCheckingForRange:", tcc_check);
    k = ensure(&NSTextAttachmentViewProvider_c, &NSTextAttachmentViewProvider_m, "NSTextAttachmentViewProvider", "NSObject");
    ADD(k, "initWithTextAttachment:parentView:textLayoutManager:location:", tavp_init);
    PROP(k, "view", "setView:", tavpView); PROP(k, "tracksTextAttachmentViewBounds", "setTracksTextAttachmentViewBounds:", tavpTracks);
    ADD(k, "textAttachment", tavp_att); ADD(k, "location", tavp_loc); ADD(k, "loadView", tavp_load);
    ADD(k, "attachmentBoundsForAttributes:location:textContainer:proposedLineFragment:position:", tavp_bounds);
    k = ensure(&NSTreeNode_c, &NSTreeNode_m, "NSTreeNode", "NSObject");
    cx_add_cmethod(k, "treeNodeWithRepresentedObject:", (void *)tn_new);
    ADD(k, "initWithRepresentedObject:", tn_init); ADD(k, "representedObject", tn_obj); ADD(k, "childNodes", tn_kids);
    ADD(k, "mutableChildNodes", tn_mkids); ADD(k, "parentNode", tn_parent); ADD(k, "isLeaf", tn_leaf); ADD(k, "indexPath", tn_index_path);
    ADD(k, "descendantNodeAtIndexPath:", tn_descendant); ADD(k, "sortWithSortDescriptors:recursively:", tn_sort);
    cx_class_t *oc = kls("NSObjectController");
    k = ensure(&NSTreeController_c, &NSTreeController_m, "NSTreeController", oc ? "NSObjectController" : "NSObject");
    ADD(k, "init", tc_init); ADD(k, "initWithContent:", tc_init_content); ADD(k, "content", tc_content); ADD(k, "setContent:", tc_set_content);
    PROP(k, "childrenKeyPath", "setChildrenKeyPath:", tcChildrenKP); PROP(k, "countKeyPath", "setCountKeyPath:", tcCountKP);
    PROP(k, "leafKeyPath", "setLeafKeyPath:", tcLeafKP); PROP(k, "sortDescriptors", "setSortDescriptors:", tcSortDescs);
    PROP(k, "preservesSelection", "setPreservesSelection:", tcPreserves); PROP(k, "avoidsEmptySelection", "setAvoidsEmptySelection:", tcAvoidsEmpty);
    PROP(k, "alwaysUsesMultipleValuesMarker", "setAlwaysUsesMultipleValuesMarker:", tcAlwaysUsesMulti);
    PROP(k, "selectsInsertedObjects", "setSelectsInsertedObjects:", tcSelectsInserted);
    ADD(k, "arrangedObjects", tc_arranged); ADD(k, "rearrangeObjects", tc_rearrange);
    ADD(k, "selectionIndexPaths", tc_sel_ips); ADD(k, "setSelectionIndexPaths:", tc_set_sel_ips);
    ADD(k, "selectionIndexPath", tc_sel_ip); ADD(k, "setSelectionIndexPath:", tc_set_sel_ip);
    ADD(k, "selectedNodes", tc_sel_nodes); ADD(k, "selectedObjects", tc_sel_objects);
    ADD(k, "insertObject:atArrangedObjectIndexPath:", tc_insert); ADD(k, "insertObjects:atArrangedObjectIndexPaths:", tc_insert_objects);
    ADD(k, "removeObjectAtArrangedObjectIndexPath:", tc_remove_at); ADD(k, "removeObjectsAtArrangedObjectIndexPaths:", tc_remove_ips);
    ADD(k, "remove:", tc_remove); ADD(k, "add:", tc_add); ADD(k, "addChild:", tc_add_child); ADD(k, "insert:", tc_insert_sel); ADD(k, "insertChild:", tc_insert_child);
    ADD(k, "newObject", tc_new_object); ADD(k, "canAdd", tc_can_add); ADD(k, "canRemove", tc_can_remove); ADD(k, "canAddChild", tc_can_add_child);
    ADD(k, "canInsert", tc_can_add); ADD(k, "canInsertChild", tc_can_add_child);
    ADD(k, "childrenKeyPathForNode:", tc_child_kp_for_node); ADD(k, "countKeyPathForNode:", tc_count_kp_for_node); ADD(k, "leafKeyPathForNode:", tc_leaf_kp_for_node);
    ADD(k, "moveNode:toIndexPath:", tc_move_node);
    k = ensure(&NSInputManager_c, &NSInputManager_m, "NSInputManager", "NSObject");
    cx_add_cmethod(k, "currentInputManager", (void *)im_current); cx_add_cmethod(k, "cycleToNextInputLanguage:", (void *)im_cycle);
    cx_add_cmethod(k, "cycleToNextInputServerInLanguage:", (void *)im_cycle);
    ADD(k, "localizedInputManagerName", im_name); ADD(k, "language", im_lang); ADD(k, "image", im_image);
    ADD(k, "wantsToHandleMouseEvents", im_no); ADD(k, "wantsToInterpretAllKeystrokes", im_no); ADD(k, "wantsToDelayTextChangeNotifications", im_no);
    ADD(k, "handleMouseEvent:", im_wants);
    k = ensure(&NSHelpManager_c, &NSHelpManager_m, "NSHelpManager", "NSObject");
    cx_add_cmethod(k, "sharedHelpManager", (void *)hm_shared); cx_add_cmethod(k, "isContextHelpModeActive", (void *)hm_is_ctx);
    cx_add_cmethod(k, "setContextHelpModeActive:", (void *)hm_set_ctx);
    ADD(k, "setContextHelp:forObject:", hm_set_help); ADD(k, "removeContextHelpForObject:", hm_remove_help);
    ADD(k, "contextHelpForObject:", hm_help_for); ADD(k, "showContextHelpForObject:locationHint:", hm_show_help);
    ADD(k, "openHelpAnchor:inBook:", hm_open_anchor); ADD(k, "findString:inBook:", hm_find); ADD(k, "registerBooksInBundle:", hm_register);
    k = ensure(&NSCustomImageRep_c, &NSCustomImageRep_m, "NSCustomImageRep", "NSImageRep");
    ADD(k, "initWithDrawSelector:delegate:", cir_init_sel); ADD(k, "initWithSize:flipped:drawingHandler:", cir_init_handler);
    ADD(k, "drawSelector", cir_sel); ADD(k, "delegate", cir_delegate); ADD(k, "drawingHandler", cir_handler);
    ADD(k, "size", cir_size); ADD(k, "setSize:", cir_set_size); ADD(k, "draw", cir_draw); ADD(k, "drawAtPoint:", cir_draw_at); ADD(k, "drawInRect:", cir_draw_in);
    k = ensure(&NSPICTImageRep_c, &NSPICTImageRep_m, "NSPICTImageRep", "NSImageRep");
    cx_add_cmethod(k, "imageRepWithData:", (void *)pict_with);
    ADD(k, "initWithData:", pict_init); ADD(k, "PICTRepresentation", pict_data); ADD(k, "boundingBox", pict_box); ADD(k, "size", pict_size);
    ADD(k, "draw", pict_draw); ADD(k, "drawInRect:", pict_draw_in);
    cx_class_t *ge = ensure(&NSGlassEffectView_c, &NSGlassEffectView_m, "NSGlassEffectView", "NSView");
    cx_class_t *gec = ensure(&NSGlassEffectContainerView_c, &NSGlassEffectContainerView_m, "NSGlassEffectContainerView", "NSView");
    ADD(ge, "contentView", g_geContent); ADD(ge, "setContentView:", ge_set_content); PROP(ge, "cornerRadius", "setCornerRadius:", geRadius);
    PROP(ge, "tintColor", "setTintColor:", geTint); PROP(ge, "style", "setStyle:", geStyle); ADD(ge, "drawRect:", ge_draw);
    ADD(gec, "contentView", g_geContent); ADD(gec, "setContentView:", ge_set_content); PROP(gec, "spacing", "setSpacing:", geSpacing);
    k = ensure(&NSPressureConfiguration_c, &NSPressureConfiguration_m, "NSPressureConfiguration", "NSObject");
    ADD(k, "initWithPressureBehavior:", pc_init); ADD(k, "pressureBehavior", pc_behavior); ADD(k, "set", pc_set);
    k = ensure(&NSSliderAccessory_c, &NSSliderAccessory_m, "NSSliderAccessory", "NSObject");
    cx_add_cmethod(k, "accessoryWithImage:", (void *)sa_with_image);
    PROP(k, "behavior", "setBehavior:", saBehavior); PROP(k, "isEnabled", "setEnabled:", saEnabled);
    k = ensure(&NSSliderAccessoryBehavior_c, &NSSliderAccessoryBehavior_m, "NSSliderAccessoryBehavior", "NSObject");
    cx_add_cmethod(k, "automaticBehavior", (void *)sab_auto); cx_add_cmethod(k, "valueStepBehavior", (void *)sab_value_step);
    cx_add_cmethod(k, "valueResetBehavior", (void *)sab_value_reset); cx_add_cmethod(k, "behaviorWithTarget:action:", (void *)sab_with_target);
    cx_add_cmethod(k, "behaviorWithHandler:", (void *)sab_with_handler); ADD(k, "handleAction:", sab_handle);
    k = ensure(&NSMenuItemBadge_c, &NSMenuItemBadge_m, "NSMenuItemBadge", "NSObject");
    ADD(k, "initWithCount:", mb_init_count); ADD(k, "initWithString:", mb_init_string); ADD(k, "initWithCount:type:", mb_init_type);
    cx_add_cmethod(k, "updatesWithCount:", (void *)mb_updates); cx_add_cmethod(k, "newItemsWithCount:", (void *)mb_new_items);
    cx_add_cmethod(k, "alertsWithCount:", (void *)mb_alerts);
    ADD(k, "itemCount", mb_count); ADD(k, "stringValue", mb_string); ADD(k, "type", mb_type);
    k = ensure(&NSStoryboardSegue_c, &NSStoryboardSegue_m, "NSStoryboardSegue", "NSObject");
    ADD(k, "initWithIdentifier:source:destination:", seg_init); cx_add_cmethod(k, "segueWithIdentifier:source:destination:performHandler:", (void *)seg_with_handler);
    ADD(k, "identifier", seg_ident); ADD(k, "sourceController", seg_src); ADD(k, "destinationController", seg_dst); ADD(k, "perform", seg_perform);
    k = ensure(&NSTableViewRowAction_c, &NSTableViewRowAction_m, "NSTableViewRowAction", "NSObject");
    cx_add_cmethod(k, "rowActionWithStyle:title:handler:", (void *)tra_with);
    PROP(k, "title", "setTitle:", traTitle); PROP(k, "backgroundColor", "setBackgroundColor:", traBg); PROP(k, "image", "setImage:", traImage);
    ADD(k, "style", tra_style); ADD(k, "handler", tra_handler);
    k = ensure(&NSButtonTouchBarItem_c, &NSButtonTouchBarItem_m, "NSButtonTouchBarItem", "NSTouchBarItem");
    cx_add_cmethod(k, "buttonTouchBarItemWithIdentifier:title:target:action:", (void *)btbi_with_title);
    cx_add_cmethod(k, "buttonTouchBarItemWithIdentifier:image:target:action:", (void *)btbi_with_image);
    cx_add_cmethod(k, "buttonTouchBarItemWithIdentifier:title:image:target:action:", (void *)btbi_with_both);
    PROP(k, "title", "setTitle:", btTitle); PROP(k, "image", "setImage:", btImage); PROP(k, "bezelColor", "setBezelColor:", btBezel);
    PROP(k, "isEnabled", "setEnabled:", btEnabled); ADD(k, "target", bt_target); ADD(k, "setTarget:", bt_set_target);
    ADD(k, "action", bt_action); ADD(k, "setAction:", bt_set_action);
    k = ensure(&NSCollectionViewFlowLayoutInvalidationContext_c, &NSCollectionViewFlowLayoutInvalidationContext_m, "NSCollectionViewFlowLayoutInvalidationContext",
               kls("NSCollectionViewLayoutInvalidationContext") ? "NSCollectionViewLayoutInvalidationContext" : "NSObject");
    PROP(k, "invalidateFlowLayoutDelegateMetrics", "setInvalidateFlowLayoutDelegateMetrics:", fliMetrics);
    PROP(k, "invalidateFlowLayoutAttributes", "setInvalidateFlowLayoutAttributes:", fliAttrs);
    k = ensure(&NSTintConfiguration_c, &NSTintConfiguration_m, "NSTintConfiguration", "NSObject");
    cx_add_cmethod(k, "defaultTintConfiguration", (void *)tcf_default); cx_add_cmethod(k, "monochromeTintConfiguration", (void *)tcf_default);
    cx_add_cmethod(k, "tintConfigurationWithPreferredColor:", (void *)tcf_with_color); cx_add_cmethod(k, "tintConfigurationWithFixedColor:", (void *)tcf_with_color);
    ADD(k, "baseTintColor", tcf_base); ADD(k, "equivalentContentTintColor", tcf_base); ADD(k, "adaptsToUserAccentColor", tcf_adapts);
    k = ensure(&NSToolTipManager_c, &NSToolTipManager_m, "NSToolTipManager", "NSObject");
    cx_add_cmethod(k, "sharedToolTipManager", (void *)ttm_shared); ADD(k, "abortToolTip", ttm_noop); ADD(k, "orderOutToolTip", ttm_noop);
    ADD(k, "orderOutToolTipImmediately:", ttm_order_out);
    k = ensure(&NSWritingToolsCoordinator_c, &NSWritingToolsCoordinator_m, "NSWritingToolsCoordinator", "NSObject");
    cx_add_cmethod(k, "isWritingToolsAvailable", (void *)wtc_available);
    PROP(k, "delegate", "setDelegate:", wtcDelegate); PROP(k, "preferredBehavior", "setPreferredBehavior:", wtcBehavior);
    PROP(k, "preferredResultOptions", "setPreferredResultOptions:", wtcResult);
    k = ensure(&NSAnimationManager_c, &NSAnimationManager_m, "NSAnimationManager", "NSObject");
    cx_add_cmethod(k, "sharedAnimationManager", (void *)am_shared); ADD(k, "removeAllAnimationsForTarget:", am_noop1);
    k = ensure(&NSSplitViewItemAccessoryViewController_c, &NSSplitViewItemAccessoryViewController_m, "NSSplitViewItemAccessoryViewController", "NSViewController");
    PROP(k, "automaticallyAppliesContentInsets", "setAutomaticallyAppliesContentInsets:", svaHidden);
    k = ensure(&NSSpeechRecognizer_c, &NSSpeechRecognizer_m, "NSSpeechRecognizer", "NSObject");
    ADD(k, "init", sr_init); PROP(k, "commands", "setCommands:", srCommands); PROP(k, "displayedCommandsTitle", "setDisplayedCommandsTitle:", srTitle);
    PROP(k, "delegate", "setDelegate:", srDelegate); PROP(k, "listensInForegroundOnly", "setListensInForegroundOnly:", srForeground);
    PROP(k, "blocksOtherRecognizers", "setBlocksOtherRecognizers:", srBlocks); ADD(k, "startListening", sr_start); ADD(k, "stopListening", sr_stop);
    ensure(&NSPathCell_c, &NSPathCell_m, "NSPathCell", kls("NSActionCell") ? "NSActionCell" : "NSObject");
    ensure(&NSFrameView_c, &NSFrameView_m, "NSFrameView", "NSView");
    ensure(&NSNextStepFrame_c, &NSNextStepFrame_m, "NSNextStepFrame", "NSFrameView");
    ensure(&NSFunctionRow_c, &NSFunctionRow_m, "NSFunctionRow", "NSObject");
    (void)g_svaView; (void)s_svaView; (void)g_svaAttr; (void)s_svaAttr; (void)g_tcfAccent; (void)s_tcfAccent;
}

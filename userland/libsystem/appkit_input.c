/*
 * mOS libSystem — entrada de texto e arrastar/soltar do AppKit, no formato que o Chromium usa
 * (content/app_shim_remote_cocoa/render_widget_host_view_cocoa.mm e ui/base/dragdrop):
 *
 *  - -[NSResponder interpretKeyEvents:]: o "key binding manager" do macOS. Tecla de edição/navegação vira
 *    doCommandBySelector: (deleteBackward:, insertNewline:, moveLeft:...); caractere imprimível vira
 *    insertText:replacementRange: (ou insertText:). Com ⌘ nada é interpretado (vai por performKeyEquivalent:).
 *  - NSTextInputContext: +currentInputContext, -initWithClient:, -handleEvent: (passa por
 *    interpretKeyEvents: no cliente), marked text descartável. Sem IME de composição por enquanto: o texto
 *    chega já confirmado, como num teclado US/ABNT2 sem teclas mortas.
 *  - -[NSView inputContext] para quem implementa NSTextInputClient.
 *  - Arrastar: NSDraggingItem, NSDraggingSession, -[NSView beginDraggingSessionWithItems:event:source:].
 *    Os itens vão para o pasteboard de arrasto ("Apple CFPasteboard drag"); como o servidor de janelas do mOS
 *    ainda não leva o arrasto a outra janela, a sessão termina na hora com NSDragOperationNone e a fonte
 *    recebe draggingSession:endedAtPoint:operation: (o Chromium libera o estado do arrasto nesse ponto).
 *  - registerForDraggedTypes:/unregisterDraggedTypes/registeredDraggedTypes guardam os tipos de verdade.
 */
#include "foundation.h"

void cx_arr_add(id a, id o);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
int strcmp(const char *a, const char *b);

static u64 ain_count(id a) { return a ? MSG0(u64, a, "count") : 0; }
static id ain_at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static BOOL ain_resp(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }
static id ain_cls(const char *n) { return (id)cx_class_named(n); }

#define NS_NOT_FOUND 0x7fffffffffffffffull
#define MOD_SHIFT (1ull << 17)
#define MOD_CTRL  (1ull << 18)
#define MOD_OPT   (1ull << 19)
#define MOD_CMD   (1ull << 20)

/* ---------------------------------------------------------------- interpretKeyEvents: */
static const char *key_command(u16 code, u64 mods) {
    int sh = (mods & MOD_SHIFT) != 0, opt = (mods & MOD_OPT) != 0;
    switch (code) {
    case 51: return opt ? "deleteWordBackward:" : "deleteBackward:";
    case 117: return opt ? "deleteWordForward:" : "deleteForward:";
    case 36: case 76: return "insertNewline:";
    case 48: return sh ? "insertBacktab:" : "insertTab:";
    case 53: return "cancelOperation:";
    case 123: return opt ? (sh ? "moveWordLeftAndModifySelection:" : "moveWordLeft:") : (sh ? "moveLeftAndModifySelection:" : "moveLeft:");
    case 124: return opt ? (sh ? "moveWordRightAndModifySelection:" : "moveWordRight:") : (sh ? "moveRightAndModifySelection:" : "moveRight:");
    case 125: return sh ? "moveDownAndModifySelection:" : "moveDown:";
    case 126: return sh ? "moveUpAndModifySelection:" : "moveUp:";
    case 115: return sh ? "moveToBeginningOfDocumentAndModifySelection:" : "scrollToBeginningOfDocument:";
    case 119: return sh ? "moveToEndOfDocumentAndModifySelection:" : "scrollToEndOfDocument:";
    case 116: return "scrollPageUp:";
    case 121: return "scrollPageDown:";
    }
    return 0;
}
/* atalhos de Ctrl do Emacs que o Cocoa oferece em todo campo de texto */
static const char *ctrl_command(char c) {
    switch (c) {
    case 'a': return "moveToBeginningOfParagraph:";
    case 'e': return "moveToEndOfParagraph:";
    case 'b': return "moveBackward:";
    case 'f': return "moveForward:";
    case 'p': return "moveUp:";
    case 'n': return "moveDown:";
    case 'd': return "deleteForward:";
    case 'h': return "deleteBackward:";
    case 'k': return "deleteToEndOfParagraph:";
    case 'o': return "insertNewlineIgnoringFieldEditor:";
    case 't': return "transpose:";
    case 'y': return "yank:";
    }
    return 0;
}
static void run_command(id self, const char *name) {
    SEL s = sel_registerName(name);
    if (ain_resp(self, "doCommandBySelector:")) MSG(void, self, "doCommandBySelector:", s);
    else if (cx_responds(object_getClass(self), s)) ((void (*)(id, SEL, id))objc_msgSend)(self, s, (id)0);
}
static void insert_text(id self, id text) {
    if (ain_resp(self, "insertText:replacementRange:"))
        ((void (*)(id, SEL, id, u64, u64))objc_msgSend)(self, sel_registerName("insertText:replacementRange:"), text, NS_NOT_FOUND, 0);
    else if (ain_resp(self, "insertText:")) MSG(void, self, "insertText:", text);
}
/* texto "digitável": sem controles nem a faixa de teclas de função do Cocoa (U+F700–U+F8FF) */
static int printable(const char *s) {
    if (!s || !*s) return 0;
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x20 || u[0] == 0x7f) return 0;
    if (u[0] == 0xEF && (u[1] == 0x9C || u[1] == 0x9D || u[1] == 0x9E || u[1] == 0x9F || u[1] == 0xA0 || u[1] == 0xA1 || u[1] == 0xA2 || u[1] == 0xA3)) return 0;
    return 1;
}
static void interpret_one(id self, id e) {
    if (!e) return;
    u64 mods = MSG0(u64, e, "modifierFlags");
    if (mods & MOD_CMD) return;
    u16 code = MSG0(u16, e, "keyCode");
    const char *cmd = key_command(code, mods);
    if (cmd) { run_command(self, cmd); return; }
    id chars = MSG0(id, e, "characters");
    const char *s = chars ? cx_cstr(chars) : 0;
    if (mods & MOD_CTRL) {
        id raw = ain_resp(e, "charactersIgnoringModifiers") ? MSG0(id, e, "charactersIgnoringModifiers") : chars;
        const char *r = raw ? cx_cstr(raw) : 0;
        char c = r && r[0] >= 'A' && r[0] <= 'Z' ? (char)(r[0] + 32) : (r ? r[0] : 0);
        const char *cc = ctrl_command(c);
        if (cc) { run_command(self, cc); return; }
    }
    if (printable(s)) insert_text(self, chars);
    else if (s && s[0] == '\r') run_command(self, "insertNewline:");
    else if (s && s[0] == '\t') run_command(self, "insertTab:");
    else if (s && (s[0] == 0x7f || s[0] == '\b')) run_command(self, "deleteBackward:");
    else if (s && s[0] == 0x1b) run_command(self, "cancelOperation:");
}
static void r_interpret(id self, SEL c, id events) {
    (void)c;
    u64 n = ain_count(events);
    for (u64 i = 0; i < n; i++) interpret_one(self, ain_at(events, i));
}
static void r_do_command(id self, SEL c, SEL s) {
    (void)c;
    if (s && cx_responds(object_getClass(self), s)) { ((void (*)(id, SEL, id))objc_msgSend)(self, s, (id)0); return; }
    id next = ain_resp(self, "nextResponder") ? MSG0(id, self, "nextResponder") : 0;
    if (next && ain_resp(next, "doCommandBySelector:")) MSG(void, next, "doCommandBySelector:", s);
}

/* ---------------------------------------------------------------- NSTextInputContext */
static cx_class_t NSTextInputContext_c __asm__("OBJC_CLASS_$_NSTextInputContext");
static cx_class_t NSTextInputContext_m __asm__("OBJC_METACLASS_$_NSTextInputContext");
typedef struct { void *isa; id client; id sources; } ticx_t;
static id ti_current;
static id ti_init_client(id self, SEL c, id client) { (void)c; ((ticx_t *)self)->client = client; return self; }
static id ti_client(id self, SEL c) { (void)c; return ((ticx_t *)self)->client; }
static BOOL ti_handle(id self, SEL c, id e) {
    (void)c;
    id cl = ((ticx_t *)self)->client;
    if (!cl || !e) return NO;
    u64 t = MSG0(u64, e, "type");
    if (t != 10) return NO;                              /* só NSEventTypeKeyDown */
    if (MSG0(u64, e, "modifierFlags") & MOD_CMD) return NO;
    id arr = cx_arr_new(0);
    cx_arr_add(arr, e);
    r_interpret(cl, 0, arr);
    objc_release(arr);
    return YES;
}
static void ti_discard(id self, SEL c) {
    (void)c;
    id cl = ((ticx_t *)self)->client;
    if (ain_resp(cl, "unmarkText")) MSG0(void, cl, "unmarkText");
}
static void ti_noop(id self, SEL c) { (void)self; (void)c; }
static void ti_activate(id self, SEL c) { (void)c; ti_current = self; }
static void ti_deactivate(id self, SEL c) { (void)c; if (ti_current == self) ti_current = 0; }
static id ti_current_ctx(id cls, SEL c) { (void)cls; (void)c; return ti_current; }
static id ti_source(id self, SEL c) { (void)self; (void)c; return cx_str("com.apple.keylayout.US"); }
static id ti_sources(id self, SEL c) {
    (void)c;
    ticx_t *t = (ticx_t *)self;
    if (!t->sources) { t->sources = objc_retain(cx_arr_new(0)); cx_arr_add(t->sources, cx_str("com.apple.keylayout.US")); }
    return t->sources;
}
static void ti_set_source(id self, SEL c, id s) { (void)self; (void)c; (void)s; }
static BOOL ti_accepts_glyph(id self, SEL c) { (void)self; (void)c; return NO; }

/* -[NSView inputContext]: um contexto por view que fala NSTextInputClient (guardado num pequeno mapa) */
#define TI_MAX 64
static id ti_views[TI_MAX], ti_ctxs[TI_MAX];
static id v_input_context(id self, SEL c) {
    (void)c;
    if (!ain_resp(self, "insertText:replacementRange:") && !ain_resp(self, "setMarkedText:selectedRange:replacementRange:")) return 0;
    for (int i = 0; i < TI_MAX; i++) if (ti_views[i] == self) return ti_ctxs[i];
    id ctx = ti_init_client(cx_alloc_instance(&NSTextInputContext_c), 0, self);
    for (int i = 0; i < TI_MAX; i++) if (!ti_views[i]) { ti_views[i] = self; ti_ctxs[i] = ctx; break; }
    return ctx;
}

/* ---------------------------------------------------------------- arrastar e soltar */
static cx_class_t NSDraggingItem_c __asm__("OBJC_CLASS_$_NSDraggingItem");
static cx_class_t NSDraggingItem_m __asm__("OBJC_METACLASS_$_NSDraggingItem");
static cx_class_t NSDraggingSession_c __asm__("OBJC_CLASS_$_NSDraggingSession");
static cx_class_t NSDraggingSession_m __asm__("OBJC_METACLASS_$_NSDraggingSession");
typedef struct { double x, y, w, h; } ain_rect_t;
typedef struct { void *isa; id writer; id contents; ain_rect_t frame; } dgitem_t;
typedef struct { void *isa; id pboard; i64 formation; BOOL animates; i64 seq; } dgsess_t;

static id di_init(id self, SEL c, id writer) { (void)c; ((dgitem_t *)self)->writer = objc_retain(writer); return self; }
static id di_item(id self, SEL c) { (void)c; return ((dgitem_t *)self)->writer; }
static void di_set_frame_contents(id self, SEL c, ain_rect_t r, id contents) {
    (void)c;
    dgitem_t *d = (dgitem_t *)self;
    d->frame = r;
    if (contents) d->contents = objc_retain(contents);
}
static void di_set_frame(id self, SEL c, ain_rect_t r) { (void)c; ((dgitem_t *)self)->frame = r; }
static ain_rect_t di_frame(id self, SEL c) { (void)c; return ((dgitem_t *)self)->frame; }
static id di_contents(id self, SEL c) { (void)c; return ((dgitem_t *)self)->contents; }

static id ds_pboard(id self, SEL c) { (void)c; return ((dgsess_t *)self)->pboard; }
static i64 ds_formation(id self, SEL c) { (void)c; return ((dgsess_t *)self)->formation; }
static void ds_set_formation(id self, SEL c, i64 f) { (void)c; ((dgsess_t *)self)->formation = f; }
static BOOL ds_animates(id self, SEL c) { (void)c; return ((dgsess_t *)self)->animates; }
static void ds_set_animates(id self, SEL c, BOOL b) { (void)c; ((dgsess_t *)self)->animates = b; }
static i64 ds_seq(id self, SEL c) { (void)c; return ((dgsess_t *)self)->seq; }

typedef struct { double x, y; } ain_pt_t;
static id v_begin_drag(id self, SEL c, id items, id event, id source) {
    (void)c;
    static i64 seq;
    id pb = MSG(id, ain_cls("NSPasteboard"), "pasteboardWithName:", cx_str("Apple CFPasteboard drag"));
    if (pb) MSG0(i64, pb, "clearContents");
    u64 n = ain_count(items);
    id writers = cx_arr_new(0);
    for (u64 i = 0; i < n; i++) {
        id it = ain_at(items, i);
        id w = it && cx_is_subclass(object_getClass(it), &NSDraggingItem_c) ? ((dgitem_t *)it)->writer : it;
        if (w) cx_arr_add(writers, w);
    }
    if (pb && ain_count(writers)) MSG(BOOL, pb, "writeObjects:", writers);
    objc_release(writers);
    dgsess_t *s = (dgsess_t *)cx_alloc_instance(&NSDraggingSession_c);
    s->pboard = pb;
    s->animates = YES;
    s->seq = ++seq;
    id sess = (id)s;
    ain_pt_t at = {0, 0};
    if (event && ain_resp(event, "locationInWindow"))
        at = ((ain_pt_t (*)(id, SEL))objc_msgSend)(event, sel_registerName("locationInWindow"));
    (void)self;
    if (ain_resp(source, "draggingSession:willBeginAtPoint:"))
        ((void (*)(id, SEL, id, ain_pt_t))objc_msgSend)(source, sel_registerName("draggingSession:willBeginAtPoint:"), sess, at);
    /* sem destino no servidor de janelas: o arrasto acaba onde começou, sem operação */
    if (ain_resp(source, "draggingSession:endedAtPoint:operation:"))
        ((void (*)(id, SEL, id, ain_pt_t, u64))objc_msgSend)(source, sel_registerName("draggingSession:endedAtPoint:operation:"), sess, at, 0);
    return objc_autorelease(sess);
}

/* tipos registrados para soltar: por view, num mapa pequeno */
#define DT_MAX 128
static id dt_views[DT_MAX], dt_types[DT_MAX];
static int dt_slot(id v, int make) {
    for (int i = 0; i < DT_MAX; i++) if (dt_views[i] == v) return i;
    if (!make) return -1;
    for (int i = 0; i < DT_MAX; i++) if (!dt_views[i]) { dt_views[i] = v; return i; }
    return -1;
}
static void v_register_drag(id self, SEL c, id types) {
    (void)c;
    int i = dt_slot(self, 1);
    if (i < 0) return;
    if (dt_types[i]) objc_release(dt_types[i]);
    dt_types[i] = types ? objc_retain(MSG0(id, types, "copy")) : 0;
}
static void v_unregister_drag(id self, SEL c) {
    (void)c;
    int i = dt_slot(self, 0);
    if (i < 0) return;
    if (dt_types[i]) objc_release(dt_types[i]);
    dt_types[i] = 0;
    dt_views[i] = 0;
}
static id v_registered_drag(id self, SEL c) {
    (void)c;
    int i = dt_slot(self, 0);
    return i >= 0 && dt_types[i] ? dt_types[i] : objc_autorelease(cx_arr_new(0));
}

static void add_if_missing(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}
void cx_appkit_input_init(void) {
    cx_class_t *resp = cx_class_named("NSResponder");
    cx_class_t *view = cx_class_named("NSView");
    cx_class_t *obj = cx_class_named("NSObject");
    add_if_missing(resp, "interpretKeyEvents:", (void *)r_interpret);
    add_if_missing(resp, "doCommandBySelector:", (void *)r_do_command);
    if (!cx_class_named("NSTextInputContext")) {
        cx_define_class(&NSTextInputContext_c, &NSTextInputContext_m, "NSTextInputContext", obj, 16);
        cx_add_cmethod(&NSTextInputContext_c, "currentInputContext", (void *)ti_current_ctx);
        cx_add_method(&NSTextInputContext_c, "initWithClient:", (void *)ti_init_client);
        cx_add_method(&NSTextInputContext_c, "client", (void *)ti_client);
        cx_add_method(&NSTextInputContext_c, "handleEvent:", (void *)ti_handle);
        cx_add_method(&NSTextInputContext_c, "discardMarkedText", (void *)ti_discard);
        cx_add_method(&NSTextInputContext_c, "invalidateCharacterCoordinates", (void *)ti_noop);
        cx_add_method(&NSTextInputContext_c, "activate", (void *)ti_activate);
        cx_add_method(&NSTextInputContext_c, "deactivate", (void *)ti_deactivate);
        cx_add_method(&NSTextInputContext_c, "selectedKeyboardInputSource", (void *)ti_source);
        cx_add_method(&NSTextInputContext_c, "setSelectedKeyboardInputSource:", (void *)ti_set_source);
        cx_add_method(&NSTextInputContext_c, "keyboardInputSources", (void *)ti_sources);
        cx_add_method(&NSTextInputContext_c, "allowedInputSourceLocales", (void *)ti_sources);
        cx_add_method(&NSTextInputContext_c, "acceptsGlyphInfo", (void *)ti_accepts_glyph);
    }
    add_if_missing(view, "inputContext", (void *)v_input_context);
    if (!cx_class_named("NSDraggingItem")) {
        cx_define_class(&NSDraggingItem_c, &NSDraggingItem_m, "NSDraggingItem", obj, (u32)(sizeof(dgitem_t) - 8));
        cx_add_method(&NSDraggingItem_c, "initWithPasteboardWriter:", (void *)di_init);
        cx_add_method(&NSDraggingItem_c, "item", (void *)di_item);
        cx_add_method(&NSDraggingItem_c, "setDraggingFrame:contents:", (void *)di_set_frame_contents);
        cx_add_method(&NSDraggingItem_c, "setDraggingFrame:", (void *)di_set_frame);
        cx_add_method(&NSDraggingItem_c, "draggingFrame", (void *)di_frame);
        cx_add_method(&NSDraggingItem_c, "imageComponents", (void *)di_contents);
    }
    if (!cx_class_named("NSDraggingSession")) {
        cx_define_class(&NSDraggingSession_c, &NSDraggingSession_m, "NSDraggingSession", obj, (u32)(sizeof(dgsess_t) - 8));
        cx_add_method(&NSDraggingSession_c, "draggingPasteboard", (void *)ds_pboard);
        cx_add_method(&NSDraggingSession_c, "draggingFormation", (void *)ds_formation);
        cx_add_method(&NSDraggingSession_c, "setDraggingFormation:", (void *)ds_set_formation);
        cx_add_method(&NSDraggingSession_c, "animatesToStartingPositionsOnCancelOrFail", (void *)ds_animates);
        cx_add_method(&NSDraggingSession_c, "setAnimatesToStartingPositionsOnCancelOrFail:", (void *)ds_set_animates);
        cx_add_method(&NSDraggingSession_c, "draggingSequenceNumber", (void *)ds_seq);
    }
    add_if_missing(view, "beginDraggingSessionWithItems:event:source:", (void *)v_begin_drag);
    /* substitui o "não faz nada" antigo: os tipos ficam guardados */
    if (view) {
        cx_add_method(view, "registerForDraggedTypes:", (void *)v_register_drag);
        cx_add_method(view, "unregisterDraggedTypes", (void *)v_unregister_drag);
        cx_add_method(view, "registeredDraggedTypes", (void *)v_registered_drag);
    }
}

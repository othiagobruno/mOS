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
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

void cx_arr_add(id a, id o);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
int strcmp(const char *a, const char *b);
void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
size_t strlen(const char *s);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
/* appkit.c */
typedef struct { double x, y, w, h; } NSRect_;
int cx_window_gid(id w);
double cx_window_content_h(id w);
NSRect_ cx_window_screen_frame(id w);
id cx_window_hit(id w, double x, double y);
BOOL cx_view_rect_in_window(id v, double *x, double *y, double *w, double *h);
void cx_native_insert_text(id v, const char *utf8);
/* appkit_ime.c */
BOOL cx_ime_key(id client, id event, int native);
void cx_ime_client_gone(id client);
const char *cx_ime_source_id(void);
void cx_ime_select_source_id(const char *sid);

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
    if (cx_ime_key(self, e, 0)) return;            /* método de entrada (japonês/chinês/coreano): composição */
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
cx_class_t NSTextInputContext_c __asm__("OBJC_CLASS_$_NSTextInputContext");
cx_class_t NSTextInputContext_m __asm__("OBJC_METACLASS_$_NSTextInputContext");
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
    cx_ime_client_gone(cl);                        /* a composição em andamento é descartada */
    if (ain_resp(cl, "unmarkText")) MSG0(void, cl, "unmarkText");
}
static void ti_noop(id self, SEL c) { (void)self; (void)c; }
static void ti_activate(id self, SEL c) { (void)c; ti_current = self; }
static void ti_deactivate(id self, SEL c) { (void)c; if (ti_current == self) ti_current = 0; }
static id ti_current_ctx(id cls, SEL c) { (void)cls; (void)c; return ti_current; }
static id ti_source(id self, SEL c) { (void)self; (void)c; return cx_str(cx_ime_source_id()); }
static id ti_sources(id self, SEL c) {
    (void)c;
    ticx_t *t = (ticx_t *)self;
    if (!t->sources) {
        t->sources = objc_retain(cx_arr_new(0));
        static const char *ids[] = {"com.apple.keylayout.Brazilian-ABNT2", "com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese",
                                    "com.apple.inputmethod.SCIM.ITABC", "com.apple.inputmethod.Korean.2SetKorean"};
        for (int i = 0; i < 4; i++) cx_arr_add(t->sources, cx_str(ids[i]));
    }
    return t->sources;
}
static void ti_set_source(id self, SEL c, id s) { (void)self; (void)c; if (s && cx_is_str(s)) cx_ime_select_source_id(cx_cstr(s)); }
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
cx_class_t NSDraggingItem_c __asm__("OBJC_CLASS_$_NSDraggingItem");
cx_class_t NSDraggingItem_m __asm__("OBJC_METACLASS_$_NSDraggingItem");
cx_class_t NSDraggingSession_c __asm__("OBJC_CLASS_$_NSDraggingSession");
cx_class_t NSDraggingSession_m __asm__("OBJC_METACLASS_$_NSDraggingSession");
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
typedef struct { double x, y; } ain_pt_t;

/* ---- conteúdo do arrasto <-> bytes (formato de GUI_DRAG em compat_gui.h) */
typedef struct { u8 *p; u32 n, cap; } dbuf_t;
static void db_put(dbuf_t *b, const void *d, u32 n) {
    if (b->n + n > b->cap) {
        u32 nc = b->cap ? b->cap : 256;
        while (nc < b->n + n) nc *= 2;
        u8 *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}
static void db_u32(dbuf_t *b, u32 v) { db_put(b, &v, 4); }
static id ain_plist_data(id plist) {
    id cls = ain_cls("NSPropertyListSerialization");
    if (!cls || !plist) return 0;
    return ((id (*)(id, SEL, id, u64, u64, id *))objc_msgSend)(cls, sel_registerName("dataWithPropertyList:format:options:error:"), plist, 100 /* XML */, 0, 0);
}
static id ain_plist_parse(const u8 *d, u32 n) {
    id cls = ain_cls("NSPropertyListSerialization");
    id data = objc_autorelease(CFDataCreate(0, d, (long)n));
    if (!cls || !data) return 0;
    return ((id (*)(id, SEL, id, u64, u64 *, id *))objc_msgSend)(cls, sel_registerName("propertyListWithData:options:format:error:"), data, 0, 0, 0);
}
static BOOL ain_is_data(id v) { return v && !cx_is_str(v) && ain_resp(v, "bytes") && ain_resp(v, "length"); }
static u8 *drag_serialize(id pb, u32 *len) {
    dbuf_t b = {0};
    db_u32(&b, 0);
    u32 count = 0;
    id types = pb ? MSG0(id, pb, "types") : 0;
    u64 nt = ain_count(types);
    for (u64 i = 0; i < nt; i++) {
        id t = ain_at(types, i);
        if (!cx_is_str(t)) continue;
        id v = MSG(id, pb, "propertyListForType:", t);
        if (!v) v = MSG(id, pb, "dataForType:", t);
        if (!v) continue;
        u32 kind;
        const void *bytes;
        u32 dlen;
        if (cx_is_str(v)) { kind = 'S'; bytes = cx_cstr(v); dlen = (u32)strlen((const char *)bytes); }
        else if (ain_is_data(v)) { kind = 'D'; bytes = MSG0(const void *, v, "bytes"); dlen = (u32)MSG0(u64, v, "length"); }
        else {
            id x = ain_plist_data(v);
            if (!ain_is_data(x)) continue;
            kind = 'P'; bytes = MSG0(const void *, x, "bytes"); dlen = (u32)MSG0(u64, x, "length");
        }
        const char *ts = cx_cstr(t);
        db_u32(&b, (u32)strlen(ts)); db_put(&b, ts, (u32)strlen(ts));
        db_u32(&b, kind);
        db_u32(&b, dlen); if (dlen) db_put(&b, bytes, dlen);
        count++;
    }
    if (b.p) memcpy(b.p, &count, 4);
    *len = b.n;
    return b.p;
}
static void drag_fill_pasteboard(id pb, const u8 *d, u32 n) {
    if (!pb) return;
    MSG0(i64, pb, "clearContents");
    if (n < 4) return;
    u32 count, o = 4;
    memcpy(&count, d, 4);
    for (u32 i = 0; i < count && o + 4 <= n; i++) {
        u32 tl, kind, dl;
        memcpy(&tl, d + o, 4); o += 4;
        if (tl > n - o) return;
        id type = cx_str_n((const char *)d + o, tl); o += tl;
        if (o + 8 > n) return;
        memcpy(&kind, d + o, 4); memcpy(&dl, d + o + 4, 4); o += 8;
        if (dl > n - o) return;
        const u8 *v = d + o; o += dl;
        if (kind == 'S') MSG(BOOL, pb, "setString:forType:", cx_str_n((const char *)v, dl), type);
        else if (kind == 'D') MSG(BOOL, pb, "setData:forType:", objc_autorelease(CFDataCreate(0, v, (long)dl)), type);
        else if (kind == 'P') { id pl = ain_plist_parse(v, dl); if (pl) MSG(BOOL, pb, "setPropertyList:forType:", pl, type); }
    }
}
static id drag_pasteboard(void) { return MSG(id, ain_cls("NSPasteboard"), "pasteboardWithName:", cx_str("Apple CFPasteboard drag")); }
static double screen_h(void) { gui_wininfo_t i = {1440, 876, 1, 1}; SYS1(GUI_SCREEN, &i); return i.h + 24; }
/* ponto do window server (tela, origem em cima) -> tela do Cocoa (origem embaixo) */
static ain_pt_t cocoa_screen_pt(int x, int y) { ain_pt_t p = {x, screen_h() - y}; return p; }

/* ---- fonte: a sessão de arrasto deste processo */
static struct { id session, source; i64 seq; } dsrc;
static id v_begin_drag(id self, SEL c, id items, id event, id source) {
    (void)c;
    static i64 lseq;
    id pb = drag_pasteboard();
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
    s->seq = ++lseq;
    id sess = (id)s;
    ain_pt_t at = {0, 0};
    if (event && ain_resp(event, "locationInWindow"))
        at = ((ain_pt_t (*)(id, SEL))objc_msgSend)(event, sel_registerName("locationInWindow"));
    id win = ain_resp(self, "window") ? MSG0(id, self, "window") : 0;
    NSRect_ wf = cx_window_screen_frame(win);
    at.x += wf.x; at.y += wf.y;                    /* willBegin/ended recebem coordenadas de tela */
    /* operações que a fonte permite fora do app (NSDraggingContextOutsideApplication = 0) */
    u64 mask = 1;                                 /* NSDragOperationCopy */
    SEL ms = sel_registerName("draggingSession:sourceOperationMaskForDraggingContext:");
    if (source && cx_responds(object_getClass(source), ms)) mask = ((u64 (*)(id, SEL, id, i64))objc_msgSend)(source, ms, sess, 0);
    u32 len = 0;
    u8 *payload = drag_serialize(pb, &len);
    int gid = cx_window_gid(win);
    long seq = gid && payload ? SYS5(GUI_DRAG, DG_BEGIN, gid, payload, len, mask) : -1;
    free(payload);
    if (ain_resp(source, "draggingSession:willBeginAtPoint:"))
        ((void (*)(id, SEL, id, ain_pt_t))objc_msgSend)(source, sel_registerName("draggingSession:willBeginAtPoint:"), sess, at);
    if (seq > 0) {
        /* o window server segue o cursor; o fim chega como GEV_DRAG_END (cx_drag_event) */
        if (dsrc.session) objc_release(dsrc.session);
        if (dsrc.source) objc_release(dsrc.source);
        dsrc.session = objc_retain(sess);
        dsrc.source = source ? objc_retain(source) : 0;
        dsrc.seq = seq;
        s->seq = seq;
        return objc_autorelease(sess);
    }
    /* sem janela no window server (ou kernel antigo): o arrasto acaba onde começou, sem operação */
    if (ain_resp(source, "draggingSession:endedAtPoint:operation:"))
        ((void (*)(id, SEL, id, ain_pt_t, u64))objc_msgSend)(source, sel_registerName("draggingSession:endedAtPoint:operation:"), sess, at, 0);
    return objc_autorelease(sess);
}

/* ---- destino: NSDraggingInfo entregue às views que registraram tipos */
cx_class_t NSDraggingInfoMOS_c __asm__("OBJC_CLASS_$_NSDraggingInfoMOS");
cx_class_t NSDraggingInfoMOS_m __asm__("OBJC_METACLASS_$_NSDraggingInfoMOS");
typedef struct {
    void *isa;
    id window, pboard, source;
    u64 mask;
    ain_pt_t loc;                 /* na janela, origem embaixo */
    i64 seq, valid;
    i64 formation;
    BOOL animates;
} dginfo_t;
static struct { id view; id info; i64 seq; u64 op; i64 loaded_seq; } ddst;
static id dgi_window(id self, SEL c) { (void)c; return ((dginfo_t *)self)->window; }
static id dgi_pboard(id self, SEL c) { (void)c; return ((dginfo_t *)self)->pboard; }
static id dgi_source(id self, SEL c) { (void)c; return ((dginfo_t *)self)->source; }
static u64 dgi_mask(id self, SEL c) { (void)c; return ((dginfo_t *)self)->mask; }
static ain_pt_t dgi_loc(id self, SEL c) { (void)c; return ((dginfo_t *)self)->loc; }
static i64 dgi_seq(id self, SEL c) { (void)c; return ((dginfo_t *)self)->seq; }
static id dgi_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static id dgi_nil1(id self, SEL c, id a) { (void)self; (void)c; (void)a; return 0; }
static void dgi_slide(id self, SEL c, ain_pt_t p) { (void)self; (void)c; (void)p; }
static i64 dgi_formation(id self, SEL c) { (void)c; return ((dginfo_t *)self)->formation; }
static void dgi_set_formation(id self, SEL c, i64 f) { (void)c; ((dginfo_t *)self)->formation = f; }
static BOOL dgi_animates(id self, SEL c) { (void)c; return ((dginfo_t *)self)->animates; }
static void dgi_set_animates(id self, SEL c, BOOL b) { (void)c; ((dginfo_t *)self)->animates = b; }
static i64 dgi_valid(id self, SEL c) { (void)c; return ((dginfo_t *)self)->valid; }
static void dgi_set_valid(id self, SEL c, i64 n) { (void)c; ((dginfo_t *)self)->valid = n; }
static i64 dgi_spring(id self, SEL c) { (void)self; (void)c; return 0; }
static void dgi_noop(id self, SEL c) { (void)self; (void)c; }
/* enumerateDraggingItemsWithOptions:forView:classes:searchOptions:usingBlock:: um item por objeto lido do
 * pasteboard (readObjectsForClasses:options:), cada um num NSDraggingItem */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id, i64, BOOL *); } ain_block_t;
static void dgi_enumerate(id self, SEL c, u64 opts, id view, id classes, id search, void *block) {
    (void)c; (void)opts; (void)view;
    ain_block_t *bl = block;
    if (!bl || !bl->invoke) return;
    id pb = ((dginfo_t *)self)->pboard;
    id objs = pb && classes ? MSG(id, pb, "readObjectsForClasses:options:", classes, search) : 0;
    u64 n = ain_count(objs);
    BOOL stop = NO;
    for (u64 i = 0; i < n && !stop; i++) {
        id it = objc_autorelease(di_init(cx_alloc_instance(&NSDraggingItem_c), 0, ain_at(objs, i)));
        bl->invoke(bl, it, (i64)i, &stop);
    }
}
static BOOL types_match(id registered, id offered) {
    u64 nr = ain_count(registered), no = ain_count(offered);
    for (u64 i = 0; i < nr; i++)
        for (u64 j = 0; j < no; j++)
            if (MSG(BOOL, ain_at(registered, i), "isEqual:", ain_at(offered, j))) return YES;
    return NO;
}
/* a view sob o cursor ou a primeira acima dela que aceita algum dos tipos oferecidos */
static id drop_target(id win, int x, int y, id offered) {
    for (id v = cx_window_hit(win, x, y); v; v = ain_resp(v, "superview") ? MSG0(id, v, "superview") : 0) {
        int i = dt_slot(v, 0);
        if (i >= 0 && dt_types[i] && types_match(dt_types[i], offered)) return v;
    }
    /* a própria janela (ou o delegate) também pode ser destino, como no Cocoa */
    if (ain_resp(win, "draggingEntered:")) return win;
    return 0;
}
static u64 one_op(u64 m) {
    static const u64 pref[] = {1 /* copy */, 16 /* move */, 2 /* link */, 4 /* generic */, 8 /* private */, 32 /* delete */};
    for (int i = 0; i < 6; i++) if (m & pref[i]) return pref[i];
    return 0;
}
static u64 call_op(id v, const char *sel, id info, u64 dflt) {
    if (!ain_resp(v, sel)) return dflt;
    return ((u64 (*)(id, SEL, id))objc_msgSend)(v, sel_registerName(sel), info);
}
static void call_void(id v, const char *sel, id info) { if (ain_resp(v, sel)) MSG(void, v, sel, info); }
static void drag_dest_leave(void) {
    if (ddst.view) call_void(ddst.view, "draggingExited:", ddst.info);
    ddst.view = 0;
    ddst.op = 0;
}
static void drag_dest_event(id win, const gui_event_t *g) {
    i64 seq = g->clicks;
    if (seq != ddst.seq) {                         /* sessão nova */
        if (ddst.info) objc_release(ddst.info);
        ddst.info = 0; ddst.view = 0; ddst.op = 0;
        ddst.seq = seq;
    }
    if (g->type == GEV_DRAG_EXIT) { drag_dest_leave(); return; }
    id pb = drag_pasteboard();
    /* o conteúdo vem do window server, a menos que a fonte seja este mesmo processo (o pasteboard já tem tudo) */
    if (ddst.loaded_seq != seq && seq != dsrc.seq) {
        long need = SYS4(GUI_DRAG, DG_DATA, 0, 0, 0);
        if (need > 0 && need <= (long)GUI_DRAG_MAX) {
            u8 *buf = malloc((size_t)need);
            if (buf && SYS4(GUI_DRAG, DG_DATA, buf, need, 0) == need) drag_fill_pasteboard(pb, buf, (u32)need);
            free(buf);
        }
        ddst.loaded_seq = seq;
    }
    if (!win) { if (g->type == GEV_DRAG_DROP) SYS4(GUI_DRAG, DG_FINISH, seq, 0, 0); return; }
    if (!ddst.info) ddst.info = cx_alloc_instance(&NSDraggingInfoMOS_c);
    dginfo_t *in = (dginfo_t *)ddst.info;
    in->window = win; in->pboard = pb; in->mask = (u64)(u32)g->key; in->seq = seq;
    in->source = seq == dsrc.seq ? dsrc.source : 0;
    in->loc.x = g->x; in->loc.y = cx_window_content_h(win) - g->y;
    id info = ddst.info;
    id offered = pb ? MSG0(id, pb, "types") : 0;
    id v = drop_target(win, g->x, g->y, offered);
    if (g->type == GEV_DRAG_DROP) {
        u64 done = 0;
        if (v && v != ddst.view) { drag_dest_leave(); ddst.view = v; ddst.op = call_op(v, "draggingEntered:", info, one_op(in->mask)); }
        if (ddst.view && ddst.op) {
            id d = ddst.view;
            BOOL ok = ain_resp(d, "prepareForDragOperation:") ? ((BOOL (*)(id, SEL, id))objc_msgSend)(d, sel_registerName("prepareForDragOperation:"), info) : YES;
            if (ok) ok = ain_resp(d, "performDragOperation:") ? ((BOOL (*)(id, SEL, id))objc_msgSend)(d, sel_registerName("performDragOperation:"), info) : NO;
            if (ok) { call_void(d, "concludeDragOperation:", info); done = one_op(ddst.op & (in->mask ? in->mask : ~0ull)); if (!done) done = ddst.op; }
            call_void(d, "draggingEnded:", info);
        }
        ddst.view = 0; ddst.op = 0;
        SYS4(GUI_DRAG, DG_FINISH, seq, done, 0);
        return;
    }
    if (v != ddst.view) {
        drag_dest_leave();
        ddst.view = v;
        ddst.op = v ? call_op(v, "draggingEntered:", info, one_op(in->mask)) : 0;
    } else if (v) ddst.op = call_op(v, "draggingUpdated:", info, ddst.op);
    SYS4(GUI_DRAG, DG_STATUS, seq, ddst.op, 0);
}
void cx_drag_event(id win, const gui_event_t *g) {
    if (g->type == GEV_DRAG_SRC_MOVE) {
        SEL s = sel_registerName("draggingSession:movedToPoint:");
        if (dsrc.session && g->clicks == dsrc.seq && dsrc.source && cx_responds(object_getClass(dsrc.source), s))
            ((void (*)(id, SEL, id, ain_pt_t))objc_msgSend)(dsrc.source, s, dsrc.session, cocoa_screen_pt(g->w, g->h));
        return;
    }
    if (g->type == GEV_DRAG_END) {
        if (!dsrc.session || g->clicks != dsrc.seq) return;
        id sess = dsrc.session, src = dsrc.source;
        dsrc.session = 0; dsrc.source = 0;
        SEL s = sel_registerName("draggingSession:endedAtPoint:operation:");
        if (src && cx_responds(object_getClass(src), s))
            ((void (*)(id, SEL, id, ain_pt_t, u64))objc_msgSend)(src, s, sess, cocoa_screen_pt(g->w, g->h), (u64)(u32)g->key);
        objc_release(sess);
        if (src) objc_release(src);
        return;
    }
    drag_dest_event(win, g);
}


static void add_if_missing(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}
void cx_vscode_cf_classes(void);
void cx_const_objs_init(void);
void cx_uttype_init(void);
void cx_foundation_extra_init(void);
void cx_tracking_init(void);
void cx_appkit_extra_init(void);
void cx_framework_classes_init(void);
void cx_gamecontroller_init(void);
void cx_corebluetooth_init(void) __attribute__((weak));
static void r_key_up_fwd(id self, SEL c, id e) { (void)c; id n = ain_resp(self, "nextResponder") ? MSG0(id, self, "nextResponder") : 0; if (n && n != self) MSG(void, n, "keyUp:", e); }
void cx_appkit_input_init(void) {
    cx_gamecontroller_init();   /* gamecontroller.c: GameController e CoreHaptics */
    if (cx_corebluetooth_init) cx_corebluetooth_init();   /* corebluetooth.c */
    cx_framework_classes_init(); /* vscode_fwclasses.c: classes de frameworks ausentes */
    cx_appkit_extra_init();     /* appkit_extra.c: painéis, barra de status, NSAnimation, NSParagraphStyle... */
    cx_tracking_init();         /* hover: NSTrackingArea e mouseEntered/Exited/Moved */
    cx_foundation_extra_init(); /* foundation_extra.c: NSMutableData, NSTimeZone, NSCalendar, NSInvocation... */
    cx_uttype_init();           /* vscode_media.c: classe UTType */
    cx_const_objs_init();       /* constobjs.c: NSConstantArray/NSConstantDictionary dos literais do clang */
    cx_vscode_cf_classes();     /* vscode_cf.c: classes internas de CF (locale, UUID, CharacterSet, BinaryHeap) e kCFNull */
    cx_class_t *resp = cx_class_named("NSResponder");
    cx_class_t *view = cx_class_named("NSView");
    cx_class_t *obj = cx_class_named("NSObject");
    add_if_missing(resp, "interpretKeyEvents:", (void *)r_interpret);
    add_if_missing(resp, "doCommandBySelector:", (void *)r_do_command);
    add_if_missing(resp, "keyUp:", (void *)r_key_up_fwd);   /* o window server agora entrega "tecla solta" */
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
    if (!cx_class_named("NSDraggingInfoMOS")) {
        cx_class_t *k = &NSDraggingInfoMOS_c;
        cx_define_class(k, &NSDraggingInfoMOS_m, "NSDraggingInfoMOS", obj, (u32)(sizeof(dginfo_t) - 8));
        cx_add_method(k, "draggingDestinationWindow", (void *)dgi_window);
        cx_add_method(k, "draggingPasteboard", (void *)dgi_pboard);
        cx_add_method(k, "draggingSource", (void *)dgi_source);
        cx_add_method(k, "draggingSourceOperationMask", (void *)dgi_mask);
        cx_add_method(k, "draggingLocation", (void *)dgi_loc);
        cx_add_method(k, "draggedImageLocation", (void *)dgi_loc);
        cx_add_method(k, "draggingSequenceNumber", (void *)dgi_seq);
        cx_add_method(k, "draggedImage", (void *)dgi_nil);
        cx_add_method(k, "slideDraggedImageTo:", (void *)dgi_slide);
        cx_add_method(k, "namesOfPromisedFilesDroppedAtDestination:", (void *)dgi_nil1);
        cx_add_method(k, "draggingFormation", (void *)dgi_formation);
        cx_add_method(k, "setDraggingFormation:", (void *)dgi_set_formation);
        cx_add_method(k, "animatesToDestination", (void *)dgi_animates);
        cx_add_method(k, "setAnimatesToDestination:", (void *)dgi_set_animates);
        cx_add_method(k, "numberOfValidItemsForDrop", (void *)dgi_valid);
        cx_add_method(k, "setNumberOfValidItemsForDrop:", (void *)dgi_set_valid);
        cx_add_method(k, "springLoadingHighlight", (void *)dgi_spring);
        cx_add_method(k, "resetSpringLoading", (void *)dgi_noop);
        cx_add_method(k, "enumerateDraggingItemsWithOptions:forView:classes:searchOptions:usingBlock:", (void *)dgi_enumerate);
    }
    /* substitui a versão que terminava o arrasto na hora */
    if (view) cx_add_method(view, "beginDraggingSessionWithItems:event:source:", (void *)v_begin_drag);
    /* substitui o "não faz nada" antigo: os tipos ficam guardados */
    if (view) {
        cx_add_method(view, "registerForDraggedTypes:", (void *)v_register_drag);
        cx_add_method(view, "unregisterDraggedTypes", (void *)v_unregister_drag);
        cx_add_method(view, "registeredDraggedTypes", (void *)v_registered_drag);
    }
}

/* ---------------------------------------------------------------- hover e NSTrackingArea */
/* view sob o cursor por janela (para mouseEntered:/mouseExited: no AppKit, appkit.c) */
#define HV_MAX 64
static id hv_win[HV_MAX], hv_view[HV_MAX];
id cx_hover_view(id w) { for (int i = 0; i < HV_MAX; i++) if (hv_win[i] == w) return hv_view[i]; return 0; }
void cx_set_hover_view(id w, id v) {
    for (int i = 0; i < HV_MAX; i++) if (hv_win[i] == w) { hv_view[i] = v; return; }
    for (int i = 0; i < HV_MAX; i++) if (!hv_win[i]) { hv_win[i] = w; hv_view[i] = v; return; }
}
cx_class_t NSTrackingArea_c __asm__("OBJC_CLASS_$_NSTrackingArea");
cx_class_t NSTrackingArea_m __asm__("OBJC_METACLASS_$_NSTrackingArea");
typedef struct { void *isa; ain_rect_t rect; u64 options; id owner; id info; } tarea_t;
static id ta_init(id self, SEL c, ain_rect_t r, u64 opts, id owner, id info) {
    (void)c;
    tarea_t *t = (tarea_t *)self;
    t->rect = r; t->options = opts; t->owner = owner; t->info = info ? objc_retain(info) : 0;
    return self;
}
static ain_rect_t ta_rect(id self, SEL c) { (void)c; return ((tarea_t *)self)->rect; }
static u64 ta_options(id self, SEL c) { (void)c; return ((tarea_t *)self)->options; }
static id ta_owner(id self, SEL c) { (void)c; return ((tarea_t *)self)->owner; }
static id ta_info(id self, SEL c) { (void)c; return ((tarea_t *)self)->info; }
/* áreas por view */
#define TA_MAX 256
static id ta_views[TA_MAX], ta_lists[TA_MAX];
static id ta_list(id v, int make) {
    for (int i = 0; i < TA_MAX; i++) if (ta_views[i] == v) return ta_lists[i];
    if (!make) return 0;
    for (int i = 0; i < TA_MAX; i++) if (!ta_views[i]) { ta_views[i] = v; ta_lists[i] = objc_retain(cx_arr_new(0)); return ta_lists[i]; }
    return 0;
}
static void v_add_ta(id self, SEL c, id a) { (void)c; id l = ta_list(self, 1); if (l && a && !MSG(BOOL, l, "containsObject:", a)) cx_arr_add(l, a); }
static void v_remove_ta(id self, SEL c, id a) { (void)c; id l = ta_list(self, 0); if (l && a) MSG(void, l, "removeObject:", a); }
static id v_tas(id self, SEL c) { (void)c; id l = ta_list(self, 0); return l ? l : objc_autorelease(cx_arr_new(0)); }
static void r_noop_event(id self, SEL c, id e) { (void)self; (void)c; (void)e; }
/* mouseEntered:/mouseExited:/mouseMoved: chegam à view; quem tem área com dono diferente (o Chromium usa
 * o próprio RenderWidgetHostViewCocoa como dono) recebe também */
static void v_forward_to_owners(id self, const char *sel, id e) {
    id l = ta_list(self, 0);
    u64 n = l ? MSG0(u64, l, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        tarea_t *t = (tarea_t *)MSG(id, l, "objectAtIndex:", i);
        if (t->owner && t->owner != self && ain_resp(t->owner, sel)) MSG(void, t->owner, sel, e);
    }
}
static IMP v_prev_entered, v_prev_exited, v_prev_moved;
static void v_entered(id self, SEL c, id e) { if (v_prev_entered) v_prev_entered(self, c, e); v_forward_to_owners(self, "mouseEntered:", e); }
static void v_exited(id self, SEL c, id e) { if (v_prev_exited) v_prev_exited(self, c, e); v_forward_to_owners(self, "mouseExited:", e); }
static void v_moved(id self, SEL c, id e) { if (v_prev_moved) v_prev_moved(self, c, e); v_forward_to_owners(self, "mouseMoved:", e); }
void cx_tracking_init(void) {
    cx_class_t *view = cx_class_named("NSView"), *resp = cx_class_named("NSResponder");
    if (!cx_class_named("NSTrackingArea")) {
        cx_define_class(&NSTrackingArea_c, &NSTrackingArea_m, "NSTrackingArea", cx_class_named("NSObject"), (u32)(sizeof(tarea_t) - 8));
        cx_add_method(&NSTrackingArea_c, "initWithRect:options:owner:userInfo:", (void *)ta_init);
        cx_add_method(&NSTrackingArea_c, "rect", (void *)ta_rect);
        cx_add_method(&NSTrackingArea_c, "options", (void *)ta_options);
        cx_add_method(&NSTrackingArea_c, "owner", (void *)ta_owner);
        cx_add_method(&NSTrackingArea_c, "userInfo", (void *)ta_info);
    }
    add_if_missing(resp, "mouseEntered:", (void *)r_noop_event);
    add_if_missing(resp, "mouseExited:", (void *)r_noop_event);
    add_if_missing(resp, "mouseMoved:", (void *)r_noop_event);
    if (view) {
        cx_add_method(view, "addTrackingArea:", (void *)v_add_ta);
        cx_add_method(view, "removeTrackingArea:", (void *)v_remove_ta);
        cx_add_method(view, "trackingAreas", (void *)v_tas);
        v_prev_entered = cx_lookup(view, sel_registerName("mouseEntered:"));
        v_prev_exited = cx_lookup(view, sel_registerName("mouseExited:"));
        v_prev_moved = cx_lookup(view, sel_registerName("mouseMoved:"));
        cx_add_method(view, "mouseEntered:", (void *)v_entered);
        cx_add_method(view, "mouseExited:", (void *)v_exited);
        cx_add_method(view, "mouseMoved:", (void *)v_moved);
    }
}

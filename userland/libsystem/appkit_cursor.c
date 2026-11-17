/*
 * mOS libSystem — NSCursor e os cursor rects do AppKit.
 *   +arrowCursor, IBeamCursor, pointingHandCursor, open/closedHand, crosshair, resize*, columnResize/rowResize (com e sem
 *   direções), frameResizeCursorFromPosition:inDirections:, zoomIn/zoomOut e dragCopy usam as formas do window server
 *   (GUI_WIN_FOCUS/GUI_FOCUS_CURSOR, kernel/compat/guisys.c). Proibido, atalho, menu de contexto, sumir e o I-beam
 *   vertical são imagens próprias (cursor_core.c); initWithImage:hotSpot: manda a imagem do app reduzida para 32x32
 *   (GUI_FOCUS_CURSOR_IMAGE).
 *   -set, -push, -pop, +pop, +currentCursor, +currentSystemCursor, +hide/+unhide (em pares), +setHiddenUntilMouseMoves:,
 *   setOnMouseEntered:/setOnMouseExited: com mouseEntered:/mouseExited:.
 *   NSView addCursorRect:cursor:, removeCursorRect:cursor:, discardCursorRects, resetCursorRects; NSWindow
 *   invalidateCursorRectsForView:, resetCursorRects, discardCursorRects, enable/disableCursorRects,
 *   areCursorRectsEnabled. O cursor do rect sob o mouse vale ao entrar nele e volta à seta ao sair (o appkit.c chama
 *   cx_cursor_moved a cada movimento sem botão). NSTextView e NSTextField editáveis ou selecionáveis põem o I-beam.
 * O cursor vale para todas as janelas do app; janelas novas recebem o atual no primeiro movimento sobre elas.
 * Registro: cx_appkit_cursor_init(), chamado do cx_appkit_input_init. Código original do mOS.
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"
#include "cursor_core.h"
#include "../../kernel/include/compat_gui.h"

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { P o; S s; } R;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id o, const void *k, id v, long policy);
id objc_getAssociatedObject(id o, const void *k);
int cx_window_gid(id w);
double cx_window_content_h(id w);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextGetData(void *c);
void CGContextDrawImage(void *c, R r, void *img);
void CGContextRelease(void *c);
size_t CGImageGetWidth(void *img);
size_t CGImageGetHeight(void *img);

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static int responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }

/* ---------------------------------------------------------------- estado de cada cursor */
/* form >= 0: forma do window server; -1: imagem em px (w x h, ponto quente hx, hy) */
typedef struct { int form; int w, h; double hx, hy; u32 px[CUR_PX * CUR_PX]; BOOL on_enter, on_exit; } cur_t;
static char k_state, k_image;
static cur_t *st(id c) {
    id d = c ? objc_getAssociatedObject(c, &k_state) : 0;
    if (!d && c) {
        d = MSG(id, (id)kls("NSMutableData"), "dataWithLength:", (u64)sizeof(cur_t));
        if (!d) return 0;
        objc_setAssociatedObject(c, &k_state, d, 1);
        ((cur_t *)MSG0(void *, d, "mutableBytes"))->form = GUI_CURSOR_ARROW;
    }
    return d ? (cur_t *)MSG0(void *, d, "mutableBytes") : 0;
}

/* ---------------------------------------------------------------- aplicar nas janelas */
static id g_current;            /* cursor atual (retido) */
static id g_stack;              /* pilha de push/pop (NSMutableArray) */
static int g_hidden;            /* +hide sem +unhide correspondente */
static BOOL g_hidden_until_move;
static long g_gen = 1;          /* muda a cada troca; cada janela guarda a geração que já recebeu */
static char k_win_gen;

static void apply_to(id w) {
    int gid = cx_window_gid(w);
    if (gid <= 0) return;
    if (g_hidden > 0 || g_hidden_until_move) SYS3(GUI_WIN_FOCUS, gid, GUI_FOCUS_CURSOR, GUI_CURSOR_HIDDEN);
    else {
        cur_t *c = st(g_current);
        if (c && c->form < 0) {
            gui_cursor_image_t *ci = malloc(sizeof *ci);
            if (!ci) return;
            memset(ci, 0, sizeof *ci);
            ci->w = (unsigned)c->w; ci->h = (unsigned)c->h;
            ci->hx = c->hx < 0 ? 0 : (unsigned)(c->hx + 0.5); ci->hy = c->hy < 0 ? 0 : (unsigned)(c->hy + 0.5);
            memcpy(ci->px, c->px, (usize)c->w * (usize)c->h * 4);
            SYS3(GUI_WIN_FOCUS, gid, GUI_FOCUS_CURSOR_IMAGE, ci);
            free(ci);
        } else SYS3(GUI_WIN_FOCUS, gid, GUI_FOCUS_CURSOR, c ? c->form : GUI_CURSOR_ARROW);
    }
    objc_setAssociatedObject(w, &k_win_gen, cx_num_i(g_gen), 1);
}
static void apply_all(void) {
    extern id NSApp;
    g_gen++;
    id ws = NSApp ? MSG0(id, NSApp, "windows") : 0;
    u64 n = ws ? MSG0(u64, ws, "count") : 0;
    for (u64 i = 0; i < n; i++) apply_to(MSG(id, ws, "objectAtIndex:", i));
}

/* ---------------------------------------------------------------- cursores do sistema */
enum { SC_ARROW, SC_IBEAM, SC_HAND, SC_CLOSED, SC_OPEN, SC_RLEFT, SC_RRIGHT, SC_RLR, SC_RUP, SC_RDOWN, SC_RUD, SC_CROSS,
       SC_POOF, SC_NOT_ALLOWED, SC_LINK, SC_COPY, SC_CONTEXT, SC_IBEAM_V, SC_ZIN, SC_ZOUT, SC_COL, SC_ROW, SC_RD, SC_RD2,
       SC_N };
static const int sc_form[SC_N] = {
    GUI_CURSOR_ARROW, GUI_CURSOR_IBEAM, GUI_CURSOR_HAND, GUI_CURSOR_GRABBING, GUI_CURSOR_GRAB, GUI_CURSOR_RESIZE_LEFT,
    GUI_CURSOR_RESIZE_RIGHT, GUI_CURSOR_RESIZE_H, GUI_CURSOR_RESIZE_UP, GUI_CURSOR_RESIZE_DOWN, GUI_CURSOR_RESIZE_V,
    GUI_CURSOR_CROSS, -CUR_POOF, -CUR_NOT_ALLOWED, -CUR_LINK, GUI_CURSOR_COPY, -CUR_CONTEXT, -CUR_IBEAM_VERTICAL,
    GUI_CURSOR_ZOOM_IN, GUI_CURSOR_ZOOM_OUT, GUI_CURSOR_RESIZE_COL, GUI_CURSOR_RESIZE_ROW, GUI_CURSOR_RESIZE_D,
    GUI_CURSOR_RESIZE_D2};
static id g_sys[SC_N];
static id sys_cursor(int k) {
    if (k < 0 || k >= SC_N) k = SC_ARROW;
    if (g_sys[k]) return g_sys[k];
    id c = MSG0(id, MSG0(id, (id)kls("NSCursor"), "alloc"), "init");
    cur_t *s = st(c);
    if (!s) return c;
    int f = sc_form[k], hx = 0, hy = 0;
    if (f >= 0) s->form = f;
    else if (cur_draw(-f, s->px, &hx, &hy)) { s->form = -1; s->w = s->h = CUR_PX; s->hx = hx; s->hy = hy; }
    g_sys[k] = c;
    return c;
}
#define SYSC(name, k) static id name(id cls, SEL c) { (void)cls; (void)c; return sys_cursor(k); }
SYSC(c_arrow, SC_ARROW) SYSC(c_ibeam, SC_IBEAM) SYSC(c_hand, SC_HAND) SYSC(c_closed, SC_CLOSED) SYSC(c_open, SC_OPEN)
SYSC(c_rleft, SC_RLEFT) SYSC(c_rright, SC_RRIGHT) SYSC(c_rlr, SC_RLR) SYSC(c_rup, SC_RUP) SYSC(c_rdown, SC_RDOWN)
SYSC(c_rud, SC_RUD) SYSC(c_cross, SC_CROSS) SYSC(c_poof, SC_POOF) SYSC(c_not, SC_NOT_ALLOWED) SYSC(c_link, SC_LINK)
SYSC(c_copy, SC_COPY) SYSC(c_context, SC_CONTEXT) SYSC(c_ibeam_v, SC_IBEAM_V) SYSC(c_zin, SC_ZIN) SYSC(c_zout, SC_ZOUT)
SYSC(c_col, SC_COL) SYSC(c_row, SC_ROW)

/* NSHorizontalDirections: Left 1, Right 2, All 3; NSVerticalDirections: Up 1, Down 2, All 3 */
static id c_col_dir(id cls, SEL c, u64 d) { (void)cls; (void)c; return sys_cursor(d == 1 ? SC_RLEFT : d == 2 ? SC_RRIGHT : SC_COL); }
static id c_row_dir(id cls, SEL c, u64 d) { (void)cls; (void)c; return sys_cursor(d == 1 ? SC_RUP : d == 2 ? SC_RDOWN : SC_ROW); }
/* NSCursorFrameResizePosition: Top 1, Left 2, Bottom 4, Right 8 (cantos combinam); Directions: Inward 1, Outward 2, All 3 */
static id c_frame(id cls, SEL c, u64 pos, u64 dir) {
    (void)cls; (void)c;
    BOOL top = (pos & 1) != 0, left = (pos & 2) != 0, bottom = (pos & 4) != 0, right = (pos & 8) != 0;
    BOOL in = (dir & 1) != 0, out = (dir & 2) != 0;
    if ((top || bottom) && (left || right)) return sys_cursor((top && left) || (bottom && right) ? SC_RD : SC_RD2);
    if (top) return sys_cursor(in && out ? SC_RUD : out ? SC_RUP : SC_RDOWN);
    if (bottom) return sys_cursor(in && out ? SC_RUD : out ? SC_RDOWN : SC_RUP);
    if (left) return sys_cursor(in && out ? SC_RLR : out ? SC_RLEFT : SC_RRIGHT);
    if (right) return sys_cursor(in && out ? SC_RLR : out ? SC_RRIGHT : SC_RLEFT);
    return sys_cursor(SC_ARROW);
}

/* ---------------------------------------------------------------- cursor com imagem */
static int image_pixels(id image, cur_t *s, double hx, double hy) {
    void *cg = image ? MSG(void *, image, "CGImageForProposedRect:context:hints:", (void *)0, (id)0, (id)0) : 0;
    if (!cg) return 0;
    S pts = ((S (*)(id, SEL))objc_msgSend)(image, sl("size"));
    long w = (long)CGImageGetWidth(cg), h = (long)CGImageGetHeight(cg);
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) return 0;
    void *space = CGColorSpaceCreateDeviceRGB();
    void *ctx = CGBitmapContextCreate(0, (size_t)w, (size_t)h, 8, (size_t)w * 4, space, 2u | (2u << 12));   /* ARGB pré-multiplicado */
    CGColorSpaceRelease(space);
    if (!ctx) return 0;
    CGContextDrawImage(ctx, (R){{0, 0}, {(double)w, (double)h}}, cg);
    u32 *src = CGBitmapContextGetData(ctx);
    int ok = 0;
    if (src) {
        u32 *np = malloc((usize)w * (usize)h * 4);
        if (np) {
            for (long i = 0; i < w * h; i++) {   /* desfaz a pré-multiplicação */
                u32 p = src[i], a = p >> 24;
                if (!a) { np[i] = 0; continue; }
                u32 r = ((p >> 16) & 255) * 255 / a, g = ((p >> 8) & 255) * 255 / a, b = (p & 255) * 255 / a;
                np[i] = (a << 24) | ((r > 255 ? 255 : r) << 16) | ((g > 255 ? 255 : g) << 8) | (b > 255 ? 255 : b);
            }
            /* o ponto quente vem em pontos, do canto superior esquerdo da imagem */
            double sx = pts.w > 0 ? w / pts.w : 1, sy = pts.h > 0 ? h / pts.h : 1;
            double px = hx * sx, py = hy * sy;
            int dw, dh;
            if (cur_fit(np, (int)w, (int)h, s->px, &dw, &dh, &px, &py)) { s->form = -1; s->w = dw; s->h = dh; s->hx = px; s->hy = py; ok = 1; }
            free(np);
        }
    }
    CGContextRelease(ctx);
    return ok;
}
static id c_init_image(id self, SEL c, id image, P hot) {
    (void)c;
    cur_t *s = st(self);
    if (!s) return self;
    objc_setAssociatedObject(self, &k_image, image, 1);
    s->hx = hot.x; s->hy = hot.y;
    if (!image_pixels(image, s, hot.x, hot.y)) s->form = GUI_CURSOR_ARROW;
    return self;
}
static id c_init_image_hints(id self, SEL c, id image, id fg, id bg, P hot) { (void)fg; (void)bg; return c_init_image(self, c, image, hot); }
static id c_image(id self, SEL c) {
    (void)c;
    id im = objc_getAssociatedObject(self, &k_image);
    if (im) return im;
    /* cursor do sistema: a imagem desenhada (as formas do window server saem como a seta, o I-beam etc. desenhados aqui) */
    cur_t *s = st(self);
    if (!s || s->form >= 0) return 0;
    void *space = CGColorSpaceCreateDeviceRGB();
    void *ctx = CGBitmapContextCreate(0, (size_t)s->w, (size_t)s->h, 8, (size_t)s->w * 4, space, 2u | (2u << 12));
    CGColorSpaceRelease(space);
    if (!ctx) return 0;
    u32 *d = CGBitmapContextGetData(ctx);
    for (int i = 0; d && i < s->w * s->h; i++) {   /* pré-multiplica para o contexto */
        u32 p = s->px[i], a = p >> 24;
        d[i] = (a << 24) | ((((p >> 16) & 255) * a / 255) << 16) | ((((p >> 8) & 255) * a / 255) << 8) | ((p & 255) * a / 255);
    }
    void *CGBitmapContextCreateImage(void *c);
    void CGImageRelease(void *i);
    void *cg = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!cg) return 0;
    im = ((id (*)(id, SEL, void *, S))objc_msgSend)(MSG0(id, (id)kls("NSImage"), "alloc"), sl("initWithCGImage:size:"), cg, (S){s->w, s->h});
    CGImageRelease(cg);
    if (im) { objc_setAssociatedObject(self, &k_image, im, 1); objc_release(im); }
    return im;
}
static P c_hot(id self, SEL c) {
    (void)c;
    cur_t *s = st(self);
    if (!s) return (P){0, 0};
    if (s->form >= 0) {   /* mesmos pontos quentes do window server (kernel/ui/wm.c) */
        switch (s->form) {
        case GUI_CURSOR_ARROW: case GUI_CURSOR_COPY: return (P){2, 1};
        case GUI_CURSOR_IBEAM: return (P){8, 11};
        case GUI_CURSOR_HAND: return (P){9, 1};
        case GUI_CURSOR_ZOOM_IN: case GUI_CURSOR_ZOOM_OUT: return (P){12, 12};
        case GUI_CURSOR_WAIT: return (P){14, 14};
        default: return (P){13, 13};
        }
    }
    return (P){s->hx, s->hy};
}

/* ---------------------------------------------------------------- set, pilha, ocultar */
static void set_current(id cur) {
    if (g_current != cur) { id old = g_current; g_current = cur ? objc_retain(cur) : 0; if (old) objc_release(old); }
    apply_all();
}
static void c_set(id self, SEL c) { (void)c; set_current(self); }
static id stack(void) {
    if (!g_stack) g_stack = MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
    return g_stack;
}
static void c_push(id self, SEL c) {
    (void)c;
    MSG(void, stack(), "addObject:", g_current ? g_current : sys_cursor(SC_ARROW));
    set_current(self);
}
static void c_pop_class(id cls, SEL c) {
    (void)cls; (void)c;
    id s = stack();
    u64 n = MSG0(u64, s, "count");
    if (!n) { set_current(sys_cursor(SC_ARROW)); return; }
    id top = objc_retain(MSG(id, s, "objectAtIndex:", n - 1));
    MSG(void, s, "removeLastObject");
    set_current(top);
    objc_release(top);
}
static void c_pop(id self, SEL c) { (void)self; c_pop_class(0, c); }
static id c_current(id cls, SEL c) { (void)cls; (void)c; return g_current ? g_current : sys_cursor(SC_ARROW); }
static void c_hide(id cls, SEL c) { (void)cls; (void)c; if (g_hidden++ == 0) apply_all(); }
static void c_unhide(id cls, SEL c) { (void)cls; (void)c; if (g_hidden > 0 && --g_hidden == 0) apply_all(); }
static void c_hidden_until_moves(id cls, SEL c, BOOL f) {
    (void)cls; (void)c;
    if (g_hidden_until_move == (f ? YES : NO)) return;
    g_hidden_until_move = f ? YES : NO;
    apply_all();
}
static void c_set_on_enter(id self, SEL c, BOOL f) { (void)c; cur_t *s = st(self); if (s) s->on_enter = f ? YES : NO; }
static BOOL c_on_enter(id self, SEL c) { (void)c; cur_t *s = st(self); return s && s->on_enter; }
static void c_set_on_exit(id self, SEL c, BOOL f) { (void)c; cur_t *s = st(self); if (s) s->on_exit = f ? YES : NO; }
static BOOL c_on_exit(id self, SEL c) { (void)c; cur_t *s = st(self); return s && s->on_exit; }
static void c_mouse_entered(id self, SEL c, id e) { (void)c; (void)e; cur_t *s = st(self); if (s && s->on_enter) set_current(self); }
static void c_mouse_exited(id self, SEL c, id e) { (void)c; (void)e; cur_t *s = st(self); if (s && s->on_exit) set_current(self); }

/* ---------------------------------------------------------------- cursor rects */
typedef struct { R r; } crect_t;
static char k_rects, k_rcurs, k_valid, k_disabled;
static void v_add_rect(id self, SEL c, R r, id cursor) {
    (void)c;
    if (!cursor || r.s.w <= 0 || r.s.h <= 0) return;
    id d = objc_getAssociatedObject(self, &k_rects), cs = objc_getAssociatedObject(self, &k_rcurs);
    if (!d) { d = MSG0(id, (id)kls("NSMutableData"), "data"); objc_setAssociatedObject(self, &k_rects, d, 1); }
    if (!cs) { cs = MSG0(id, (id)kls("NSMutableArray"), "array"); objc_setAssociatedObject(self, &k_rcurs, cs, 1); }
    crect_t e = {r};
    MSG(void, d, "appendBytes:length:", (const void *)&e, (u64)sizeof e);
    MSG(void, cs, "addObject:", cursor);
}
static void v_remove_rect(id self, SEL c, R r, id cursor) {
    (void)c;
    id d = objc_getAssociatedObject(self, &k_rects), cs = objc_getAssociatedObject(self, &k_rcurs);
    if (!d || !cs) return;
    crect_t *e = MSG0(void *, d, "mutableBytes");
    u64 n = MSG0(u64, d, "length") / sizeof(crect_t);
    for (u64 i = 0; i < n; i++)
        if (MSG(id, cs, "objectAtIndex:", i) == cursor && e[i].r.o.x == r.o.x && e[i].r.o.y == r.o.y && e[i].r.s.w == r.s.w &&
            e[i].r.s.h == r.s.h) {
            MSG(void, d, "replaceBytesInRange:withBytes:length:", (u64)(i * sizeof(crect_t)), (u64)sizeof(crect_t), (const void *)0, (u64)0);
            MSG(void, cs, "removeObjectAtIndex:", i);
            return;
        }
}
static void v_discard_rects(id self, SEL c) {
    (void)c;
    objc_setAssociatedObject(self, &k_rects, 0, 1);
    objc_setAssociatedObject(self, &k_rcurs, 0, 1);
    objc_setAssociatedObject(self, &k_valid, 0, 1);
}
static void v_reset_rects(id self, SEL c) { (void)self; (void)c; }   /* NSView: as subclasses põem os seus */
/* NSTextView / NSTextField: I-beam sobre a parte visível quando dá para selecionar ou editar */
static void text_reset_rects(id self, SEL c) {
    (void)c;
    BOOL ed = responds(self, "isEditable") && MSG0(BOOL, self, "isEditable");
    BOOL se = responds(self, "isSelectable") && MSG0(BOOL, self, "isSelectable");
    if (!ed && !se) return;
    R vis = {{0, 0}, {0, 0}};
    if (responds(self, "visibleRect")) vis = ((R (*)(id, SEL))objc_msgSend)(self, sl("visibleRect"));
    if (vis.s.w <= 0 || vis.s.h <= 0) vis = ((R (*)(id, SEL))objc_msgSend)(self, sl("bounds"));
    ((void (*)(id, SEL, R, id))objc_msgSend)(self, sl("addCursorRect:cursor:"), vis, sys_cursor(SC_IBEAM));
}
static void ensure_rects(id v) {
    if (objc_getAssociatedObject(v, &k_valid)) return;
    MSG0(void, v, "discardCursorRects");
    MSG0(void, v, "resetCursorRects");
    objc_setAssociatedObject(v, &k_valid, cx_num_i(1), 1);
}
static void w_invalidate_view(id self, SEL c, id v) { (void)self; (void)c; if (v) objc_setAssociatedObject(v, &k_valid, 0, 1); }
static void invalidate_tree(id v) {
    if (!v) return;
    objc_setAssociatedObject(v, &k_valid, 0, 1);
    id subs = MSG0(id, v, "subviews");
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    for (u64 i = 0; i < n; i++) invalidate_tree(MSG(id, subs, "objectAtIndex:", i));
}
static void w_reset_rects(id self, SEL c) { (void)c; invalidate_tree(MSG0(id, self, "contentView")); }
static void w_discard_rects(id self, SEL c) { (void)c; invalidate_tree(MSG0(id, self, "contentView")); }
static void w_enable_rects(id self, SEL c) { (void)c; objc_setAssociatedObject(self, &k_disabled, 0, 1); }
static void w_disable_rects(id self, SEL c) { (void)c; objc_setAssociatedObject(self, &k_disabled, cx_num_i(1), 1); }
static BOOL w_rects_enabled(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_disabled) == 0; }

/* rect sob o mouse: do view atingido para cima; o primeiro que contém o ponto ganha */
static id g_active_view;        /* fraco: só comparado */
static long g_active_idx = -1;
static id rect_cursor_at(id w, id hit, double x, double y, id *view_out, long *idx_out) {
    double h = cx_window_content_h(w);
    P base = {x, h - y};   /* coordenadas da janela: origem embaixo */
    for (id v = hit; v; v = MSG0(id, v, "superview")) {
        ensure_rects(v);
        id d = objc_getAssociatedObject(v, &k_rects), cs = objc_getAssociatedObject(v, &k_rcurs);
        if (!d || !cs) continue;
        P p = ((P (*)(id, SEL, P, id))objc_msgSend)(v, sl("convertPoint:fromView:"), base, (id)0);
        const crect_t *e = MSG0(const void *, d, "bytes");
        u64 n = MSG0(u64, d, "length") / sizeof(crect_t);
        for (u64 i = n; i-- > 0;) {   /* o último posto fica por cima */
            R r = e[i].r;
            if (p.x >= r.o.x && p.x < r.o.x + r.s.w && p.y >= r.o.y && p.y < r.o.y + r.s.h) {
                *view_out = v; *idx_out = (long)i;
                return MSG(id, cs, "objectAtIndex:", i);
            }
        }
    }
    *view_out = 0; *idx_out = -1;
    return 0;
}

/* appkit.c: movimento sem botão sobre a janela w, view atingido hit, (x, y) do conteúdo com origem em cima */
void cx_cursor_moved(id w, id hit, double x, double y) {
    if (g_hidden_until_move) { g_hidden_until_move = NO; apply_all(); }
    {   /* janela que ainda não recebeu o cursor atual (nova, ou o cursor mudou antes de ela existir) */
        id gen = objc_getAssociatedObject(w, &k_win_gen);
        if (!gen || MSG0(i64, gen, "longLongValue") != g_gen) apply_to(w);
    }
    if (objc_getAssociatedObject(w, &k_disabled)) return;
    id v = 0;
    long idx = -1;
    id cur = rect_cursor_at(w, hit, x, y, &v, &idx);
    if (v == g_active_view && idx == g_active_idx) return;
    BOOL had = g_active_view != 0;
    g_active_view = v; g_active_idx = idx;
    if (cur) set_current(cur);
    else if (had) set_current(sys_cursor(SC_ARROW));
}

void cx_appkit_cursor_init(void) {
    cx_class_t *k = kls("NSCursor");
    if (k) {
#define C(s, f) cx_add_cmethod(k, s, (void *)f)
#define I(s, f) cx_add_method(k, s, (void *)f)
        C("arrowCursor", c_arrow); C("IBeamCursor", c_ibeam); C("pointingHandCursor", c_hand);
        C("closedHandCursor", c_closed); C("openHandCursor", c_open); C("resizeLeftCursor", c_rleft);
        C("resizeRightCursor", c_rright); C("resizeLeftRightCursor", c_rlr); C("resizeUpCursor", c_rup);
        C("resizeDownCursor", c_rdown); C("resizeUpDownCursor", c_rud); C("crosshairCursor", c_cross);
        C("disappearingItemCursor", c_poof); C("operationNotAllowedCursor", c_not); C("dragLinkCursor", c_link);
        C("dragCopyCursor", c_copy); C("contextualMenuCursor", c_context); C("IBeamCursorForVerticalLayout", c_ibeam_v);
        C("zoomInCursor", c_zin); C("zoomOutCursor", c_zout); C("columnResizeCursor", c_col); C("rowResizeCursor", c_row);
        C("columnResizeCursorInDirections:", c_col_dir); C("rowResizeCursorInDirections:", c_row_dir);
        C("frameResizeCursorFromPosition:inDirections:", c_frame);
        C("currentCursor", c_current); C("currentSystemCursor", c_current); C("pop", c_pop_class);
        C("hide", c_hide); C("unhide", c_unhide); C("setHiddenUntilMouseMoves:", c_hidden_until_moves);
        I("initWithImage:hotSpot:", c_init_image); I("initWithImage:foregroundColorHint:backgroundColorHint:hotSpot:", c_init_image_hints);
        I("image", c_image); I("hotSpot", c_hot); I("set", c_set); I("push", c_push); I("pop", c_pop);
        I("setOnMouseEntered:", c_set_on_enter); I("isSetOnMouseEntered", c_on_enter);
        I("setOnMouseExited:", c_set_on_exit); I("isSetOnMouseExited", c_on_exit);
        I("mouseEntered:", c_mouse_entered); I("mouseExited:", c_mouse_exited);
#undef C
#undef I
    }
    cx_class_t *v = kls("NSView");
    if (v) {
        cx_add_method(v, "addCursorRect:cursor:", (void *)v_add_rect);
        cx_add_method(v, "removeCursorRect:cursor:", (void *)v_remove_rect);
        cx_add_method(v, "discardCursorRects", (void *)v_discard_rects);
        cx_add_method(v, "resetCursorRects", (void *)v_reset_rects);
    }
    cx_class_t *t = kls("NSTextView");
    if (t) cx_add_method(t, "resetCursorRects", (void *)text_reset_rects);
    t = kls("NSTextField");
    if (t) cx_add_method(t, "resetCursorRects", (void *)text_reset_rects);
    cx_class_t *w = kls("NSWindow");
    if (w) {
        cx_add_method(w, "invalidateCursorRectsForView:", (void *)w_invalidate_view);
        cx_add_method(w, "resetCursorRects", (void *)w_reset_rects);
        cx_add_method(w, "discardCursorRects", (void *)w_discard_rects);
        cx_add_method(w, "enableCursorRects", (void *)w_enable_rects);
        cx_add_method(w, "disableCursorRects", (void *)w_disable_rects);
        cx_add_method(w, "areCursorRectsEnabled", (void *)w_rects_enabled);
    }
}

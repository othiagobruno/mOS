/*
 * mOS libSystem — QuartzCore mínimo (CALayer, CATransaction, CAContext/CALayerHost) com composição
 * por software, para o conteúdo de janelas baseadas em CoreAnimation aparecer na tela do mOS
 * (porte do VS Code: o Chromium entrega os quadros como IOSurfaces em CALayers, docs/porte-vscode.md).
 *
 * Como funciona:
 *  - NSView ganha -wantsLayer/-setWantsLayer:/-layer/-setLayer:/-makeBackingLayer; a ligação
 *    view -> layer fica numa tabela deste arquivo (o estado das views é do appkit.c).
 *  - O appkit.c chama cx_layer_draw_hook para cada view ao desenhar a janela (fase 0 antes do
 *    desenho da view, fase 1 depois do drawRect:). Para uma view com layer, a árvore de layers é
 *    achatada num buffer ARGB do tamanho da área visível da view e mandada ao window server em
 *    faixas GC_IMAGE (cada uma < GUI_MAX_BATCH), depois de despachar o lote pendente do AppKit.
 *  - Qualquer mudança num layer (contents, frame, sublayers...) ou um IOSurfaceUnlock de escrita
 *    marca a view hospedeira com -setNeedsDisplay:, e o laço do AppKit redesenha a janela.
 *  - CAContext/CALayerHost: um layer publicado num CAContext (o processo da GPU do Chromium) é
 *    desenhado no lugar de cada CALayerHost com o mesmo contextId. No mesmo processo o host desenha
 *    a árvore viva. Entre processos o dono achata a árvore num buffer compartilhado (MACH_VM_CA_*)
 *    e o host do outro processo o mostra; o laço do AppKit redesenha quando a geração muda.
 *    As propriedades contextId/layer existem de verdade: o Chromium só usa o caminho remoto se
 *    class_getProperty as achar (RemoteLayerAPISupported).
 *
 * Suportado no desenho: bounds/position/anchorPoint, transform e sublayerTransform (com rotação), geometryFlipped,
 * sublayers por zPosition, hidden, opacity, masksToBounds, backgroundColor, borda, contents
 * (IOSurface, CGImage ou NSImage) com contentsRect/contentsScale/contentsGravity, cantos,
 * sombra, máscara e rotação. Sem animações:
 * toda mudança vale na hora (como com setDisableActions:YES, que é o que o Chromium usa).
 */
#include "foundation.h"
#include "libc.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *class_replaceMethod(cx_class_t *c, SEL s, void *imp, const char *types);
u64 mach_absolute_time(void);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *a, size_t len);
typedef u32 mach_port_t;
mach_port_t mach_task_self(void);
int mach_make_memory_entry_64(mach_port_t task, u64 *size, u64 offset, int perm, mach_port_t *handle, mach_port_t parent);
int mach_vm_map(mach_port_t task, u64 *addr, u64 size, u64 mask, int flags, mach_port_t object, u64 offset,
                int copy, int cur, int max, unsigned inherit);
int mach_port_deallocate(mach_port_t task, mach_port_t name);
int mach_port_construct(mach_port_t task, void *options, u64 context, mach_port_t *name);
BOOL class_addProperty(cx_class_t *c, const char *name, const void *attrs, u32 n);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { P o; S s; } R;
typedef struct { double m[16]; } T3;        /* CATransform3D (m11..m44, linha a linha) */

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(CALayer) CLASS(CATransaction) CLASS(CAContext) CLASS(CALayerHost) CLASS(CATransformLayer)
CLASS(CAMetalLayer) CLASS(CAOpenGLLayer) CLASS(CAEDRMetadata)
extern cx_class_t IOSurface_c __asm__("OBJC_CLASS_$_IOSurface");

/* ---- constantes (CFStringRef) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } qc_cfconst_t;
#define CFCONST(var, text) \
    static qc_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(kCAFilterLinear, "linear") CFCONST(kCAFilterNearest, "nearest") CFCONST(kCAFilterTrilinear, "trilinear")
CFCONST(kCAGravityCenter, "center") CFCONST(kCAGravityTop, "top") CFCONST(kCAGravityBottom, "bottom")
CFCONST(kCAGravityLeft, "left") CFCONST(kCAGravityRight, "right") CFCONST(kCAGravityTopLeft, "topLeft")
CFCONST(kCAGravityTopRight, "topRight") CFCONST(kCAGravityBottomLeft, "bottomLeft") CFCONST(kCAGravityBottomRight, "bottomRight")
CFCONST(kCAGravityResize, "resize") CFCONST(kCAGravityResizeAspect, "resizeAspect") CFCONST(kCAGravityResizeAspectFill, "resizeAspectFill")
CFCONST(kCATransactionDisableActions, "disableActions") CFCONST(kCATransactionAnimationDuration, "animationDuration")
CFCONST(kCATransactionCompletionBlock, "completionBlock") CFCONST(kCATransactionAnimationTimingFunction, "animationTimingFunction")
CFCONST(kCAContentsFormatRGBA8Uint, "RGBA8") CFCONST(kCAContentsFormatRGBA16Float, "RGBAh") CFCONST(kCAContentsFormatGray8Uint, "Gray8")
CFCONST(kCAMediaTimingFunctionLinear, "linear") CFCONST(kCAMediaTimingFunctionEaseIn, "easeIn")
CFCONST(kCAMediaTimingFunctionEaseOut, "easeOut") CFCONST(kCAMediaTimingFunctionEaseInEaseOut, "easeInEaseOut")
CFCONST(kCAMediaTimingFunctionDefault, "default")
CFCONST(kCAFillModeForwards, "forwards") CFCONST(kCAFillModeBackwards, "backwards") CFCONST(kCAFillModeBoth, "both") CFCONST(kCAFillModeRemoved, "removed")
CFCONST(kCAOnOrderIn, "onOrderIn") CFCONST(kCAOnOrderOut, "onOrderOut") CFCONST(kCATransition, "transition")
CFCONST(kCAContextCIFilterBehavior, "kCAContextCIFilterBehavior") CFCONST(kCAContextDisplayName, "kCAContextDisplayName")
CFCONST(kCAContextDisplayId, "kCAContextDisplayId") CFCONST(kCAContextIgnoresHitTest, "kCAContextIgnoresHitTest")

const T3 CATransform3DIdentity = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
T3 CATransform3DMakeTranslation(double x, double y, double z) { T3 t = CATransform3DIdentity; t.m[12] = x; t.m[13] = y; t.m[14] = z; return t; }
T3 CATransform3DMakeScale(double x, double y, double z) { T3 t = CATransform3DIdentity; t.m[0] = x; t.m[5] = y; t.m[10] = z; return t; }
T3 CATransform3DConcat(T3 a, T3 b) {
    T3 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    return r;
}
T3 CATransform3DTranslate(T3 t, double x, double y, double z) { return CATransform3DConcat(CATransform3DMakeTranslation(x, y, z), t); }
T3 CATransform3DScale(T3 t, double x, double y, double z) { return CATransform3DConcat(CATransform3DMakeScale(x, y, z), t); }
u8 CATransform3DIsIdentity(T3 t) { return !memcmp(&t, &CATransform3DIdentity, sizeof t); }
u8 CATransform3DEqualToTransform(T3 a, T3 b) { return !memcmp(&a, &b, sizeof a); }
typedef struct { double a, b, c, d, tx, ty; } AT;
T3 CATransform3DMakeAffineTransform(AT a) { T3 t = CATransform3DIdentity; t.m[0] = a.a; t.m[1] = a.b; t.m[4] = a.c; t.m[5] = a.d; t.m[12] = a.tx; t.m[13] = a.ty; return t; }
u8 CATransform3DIsAffine(T3 t) { return t.m[2] == 0 && t.m[3] == 0 && t.m[6] == 0 && t.m[7] == 0 && t.m[8] == 0 && t.m[9] == 0 && t.m[11] == 0 && t.m[14] == 0 && t.m[15] == 1; }
AT CATransform3DGetAffineTransform(T3 t) { AT a = {t.m[0], t.m[1], t.m[4], t.m[5], t.m[12], t.m[13]}; return a; }

/* mach_absolute_time está em nanossegundos (timebase 1/1) */
double CACurrentMediaTime(void) { return (double)mach_absolute_time() / 1e9; }

/* ================================================================ estado dos layers */
/* como no macOS, CALayer tem um único ponteiro de estado depois do isa (subclasses compiladas
 * contam com instance size pequeno e põem os ivars delas depois) */
#define LST(o) (*(lst_t **)((u8 *)(o) + 8))
typedef struct lst {
    R bounds;
    P pos, anchor;
    double z;
    T3 tf, stf;
    id contents;
    double cscale;
    R crect;
    id gravity;
    float opacity;
    BOOL hidden, masks, opaque, flipped, needs_display, host_layer;
    u32 autoresize;                  /* autoresizingMask */
    id bgc, borderc;                 /* CGColor (= NSColor do mOS) */
    double radius, border_w;
    double shadow_r, shadow_ox, shadow_oy, shadow_op;
    id shadowc;
    id *sub;
    u32 nsub, capsub;
    id super, delegate, name, mask;
    id host_view;                    /* view que este layer apoia (raiz) */
    u32 ctx_id;                      /* CALayerHost: contexto hospedado */
    id ctx;                          /* CAContext do qual este layer é a raiz */
    id actions, style, filters, minf, magf;
} lst_t;

typedef struct { cx_class_t *isa; double r, g, b, a; } qcolor_t;   /* = cgcolor_t / rgba_t */

static int rendering;                /* não marcar views durante o desenho */
static int publishing;               /* achatando um CAContext: não republicar no meio */

/* ---- diagnóstico: com /Sistema/Biblioteca/ca-log, linhas "[ca] ..." na saída de erro (que o
 * stderr-serial leva ao log serial) mostram o caminho do quadro: contents IOSurface, CAContext
 * publicado, CALayerHost ligado, quadro composto na janela (com a contagem de pixels visíveis). */
int access(const char *p, int m);
int cx_ca_diag(void) {
    static int on = -1;
    if (on < 0) on = access("/Sistema/Biblioteca/ca-log", 0) == 0;
    return on;
}
#define QLOG(...) do { if (cx_ca_diag()) { cx_fdprintf(2, "[ca] " __VA_ARGS__); cx_fdprintf(2, "\n"); } } while (0)
static int log_now(int *n) { ++*n; return *n <= 5 || *n % 120 == 0; }   /* os 5 primeiros e 1 a cada 120 */
static u32 count_visible(const u32 *px, size_t n) {   /* amostra: pixels com alfa != 0, de 7 em 7 */
    u32 c = 0;
    for (size_t i = 0; i < n; i += 7) if (px[i] >> 24) c++;
    return c * 7;
}
static volatile int surf_dirty;      /* IOSurfaceUnlock chegou durante o desenho */
static int tx_depth;                  /* CATransaction aberta: mudanças esperam o commit para publicar */
static void layer_dirty(id l);
static void note_ctx_id(u32 cid);
static void flush_dirty(void);
static void fence_fire(void);
void qc_mark_hosts(u32 cid, int depth);

static lst_t *ls(id l) {
    if (!LST(l)) {
        lst_t *s = calloc(1, sizeof *s);
        s->anchor.x = s->anchor.y = 0.5;
        s->opacity = 1;
        s->cscale = 1;
        s->crect.s.w = s->crect.s.h = 1;
        s->tf = s->stf = CATransform3DIdentity;
        LST(l) = s;
    }
    return LST(l);
}
static id keep(id *slot, id v) { if (v) objc_retain(v); id old = *slot; *slot = v; if (old) objc_release(old); return v; }

/* ---- CAContext: tabela de contextos por id */
typedef struct {
    cx_class_t *isa;
    u32 cid;
    id layer, cs;
    u8 *mem;                         /* quadro achatado publicado para os outros processos */
    u64 memlen;
    int fw, fh;
    u32 port;
} cactx_t;
#define MAX_CTX 64
static cactx_t *ctxs[MAX_CTX];
static u32 next_cid = 0x5000;

/* ---- ligação view -> layer */
typedef struct { id view, layer; BOOL wants; } vl_t;
static vl_t *vl;
static int nvl, capvl;
static vl_t *vl_find(id v, BOOL make) {
    for (int i = 0; i < nvl; i++) if (vl[i].view == v) return &vl[i];
    if (!make) return 0;
    if (nvl == capvl) { capvl = capvl ? capvl * 2 : 16; vl = realloc(vl, (size_t)capvl * sizeof *vl); }
    vl[nvl].view = v; vl[nvl].layer = 0; vl[nvl].wants = NO;
    return &vl[nvl++];
}

/* marca para redesenho a(s) view(s) que mostram este layer */
static void layer_dirty_depth(id l, int depth) {
    if (!l || rendering || depth > 8) return;
    id root = l;
    for (int i = 0; i < 256 && ls(root)->super; i++) root = ls(root)->super;
    lst_t *r = ls(root);
    if (r->host_view) MSG(void, r->host_view, "setNeedsDisplay:", (BOOL)YES);
    if (r->ctx) {
        u32 cid = ((cactx_t *)r->ctx)->cid;
        qc_mark_hosts(cid, depth + 1);             /* CALayerHost deste processo */
        note_ctx_id(cid);                          /* quadro para os outros processos */
    }
}
static void layer_dirty(id l) { layer_dirty_depth(l, 0); }

/* registro dos CALayerHost vivos (para achar quem mostra um contexto) */
static id *hosts;
static int nhosts, caphosts;
void qc_mark_hosts(u32 cid, int depth) {
    for (int i = 0; i < nhosts; i++) if (hosts[i] && ls(hosts[i])->ctx_id == cid) layer_dirty_depth(hosts[i], depth);
}
static void host_add(id h) {
    for (int i = 0; i < nhosts; i++) if (hosts[i] == h) return;
    if (nhosts == caphosts) { caphosts = caphosts ? caphosts * 2 : 8; hosts = realloc(hosts, (size_t)caphosts * sizeof(id)); }
    hosts[nhosts++] = h;
}
static void host_remove(id h) { for (int i = 0; i < nhosts; i++) if (hosts[i] == h) hosts[i] = hosts[--nhosts]; }

/* ================================================================ CALayer: métodos */
static id l_init(id self, SEL c) { (void)c; ls(self); return self; }
/* -initWithLayer: (cópia para camadas de apresentação): só a geometria e a aparência, sem sublayers */
static id l_init_with(id self, SEL c, id other) {
    (void)c;
    lst_t *s = ls(self);
    if (other && other != self) {
        lst_t *o = ls(other);
        s->bounds = o->bounds; s->pos = o->pos; s->anchor = o->anchor; s->z = o->z; s->tf = o->tf; s->stf = o->stf;
        s->cscale = o->cscale; s->crect = o->crect; s->opacity = o->opacity; s->hidden = o->hidden; s->masks = o->masks;
        s->opaque = o->opaque; s->flipped = o->flipped; s->radius = o->radius; s->border_w = o->border_w;
        s->shadow_r = o->shadow_r; s->shadow_ox = o->shadow_ox; s->shadow_oy = o->shadow_oy; s->shadow_op = o->shadow_op;
        keep(&s->contents, o->contents); keep(&s->gravity, o->gravity); keep(&s->bgc, o->bgc); keep(&s->borderc, o->borderc);
        keep(&s->shadowc, o->shadowc);
    }
    return self;
}
static id l_layer(id cls, SEL c) { (void)c; id l = MSG0(id, MSG0(id, cls, "alloc"), "init"); return objc_autorelease(l); }
static void l_remove_sub(id parent, id child);
static void l_dealloc(id self, SEL c) {
    lst_t *s = LST(self);
    if (s) {
        for (u32 i = 0; i < s->nsub; i++) { ls(s->sub[i])->super = 0; objc_release(s->sub[i]); }
        free(s->sub);
        id objs[] = {s->contents, s->gravity, s->bgc, s->borderc, s->shadowc, s->name, s->mask, s->actions, s->style, s->filters, s->minf, s->magf};
        for (u32 i = 0; i < sizeof objs / sizeof objs[0]; i++) if (objs[i]) objc_release(objs[i]);
        if (s->host_layer) host_remove(self);
        for (int i = 0; i < nvl; i++) if (vl[i].layer == self) vl[i].layer = 0;
        free(s);
        LST(self) = 0;
    }
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

/* autoresizingMask (como no macOS: -resizeWithOldSuperlayerSize: quando o superlayer muda de tamanho).
 * kCALayerMinXMargin 1, WidthSizable 2, MaxXMargin 4, MinYMargin 8, HeightSizable 16, MaxYMargin 32.
 * O DisplayCALayerTree do Chromium põe o layer invertido com tamanho 0 e Width|HeightSizable. */
static void set_size(id l, double w, double h);
static void resize_subs(id l, S old) {
    lst_t *s = ls(l);
    double dw = s->bounds.s.w - old.w, dh = s->bounds.s.h - old.h;
    if (dw == 0 && dh == 0) return;
    for (u32 i = 0; i < s->nsub; i++) {
        lst_t *k = ls(s->sub[i]);
        u32 m = k->autoresize;
        if (!m) continue;
        double x = k->pos.x - k->anchor.x * k->bounds.s.w, y = k->pos.y - k->anchor.y * k->bounds.s.h;
        double w = k->bounds.s.w, h = k->bounds.s.h;
        int hx = !!(m & 1) + !!(m & 2) + !!(m & 4), hy = !!(m & 8) + !!(m & 16) + !!(m & 32);
        if (hx) { double d = dw / hx; if (m & 1) x += d; if (m & 2) w += d; }
        if (hy) { double d = dh / hy; if (m & 8) y += d; if (m & 16) h += d; }
        if (w < 0) w = 0;
        if (h < 0) h = 0;
        set_size(s->sub[i], w, h);
        k->pos.x = x + k->anchor.x * w;
        k->pos.y = y + k->anchor.y * h;
    }
}
static void set_size(id l, double w, double h) {
    lst_t *s = ls(l);
    S old = s->bounds.s;
    s->bounds.s.w = w; s->bounds.s.h = h;
    resize_subs(l, old);
}
static R l_bounds(id self, SEL c) { (void)c; return ls(self)->bounds; }
static void l_set_bounds(id self, SEL c, R r) { (void)c; ls(self)->bounds.o = r.o; set_size(self, r.s.w, r.s.h); layer_dirty(self); }
static u32 l_autoresize(id self, SEL c) { (void)c; return ls(self)->autoresize; }
static void l_set_autoresize(id self, SEL c, u32 m) { (void)c; ls(self)->autoresize = m; }
static P l_position(id self, SEL c) { (void)c; return ls(self)->pos; }
static void l_set_position(id self, SEL c, P p) { (void)c; ls(self)->pos = p; layer_dirty(self); }
static P l_anchor(id self, SEL c) { (void)c; return ls(self)->anchor; }
static void l_set_anchor(id self, SEL c, P p) { (void)c; ls(self)->anchor = p; layer_dirty(self); }
static double l_z(id self, SEL c) { (void)c; return ls(self)->z; }
static void l_set_z(id self, SEL c, double z) { (void)c; ls(self)->z = z; layer_dirty(self); }
static R l_frame(id self, SEL c) {
    (void)c;
    lst_t *s = ls(self);
    R r = {{s->pos.x - s->anchor.x * s->bounds.s.w, s->pos.y - s->anchor.y * s->bounds.s.h}, s->bounds.s};
    return r;
}
static void l_set_frame(id self, SEL c, R r) {
    (void)c;
    lst_t *s = ls(self);
    set_size(self, r.s.w, r.s.h);
    s->pos.x = r.o.x + s->anchor.x * r.s.w;
    s->pos.y = r.o.y + s->anchor.y * r.s.h;
    layer_dirty(self);
}
static T3 l_transform(id self, SEL c) { (void)c; return ls(self)->tf; }
static void l_set_transform(id self, SEL c, T3 t) { (void)c; ls(self)->tf = t; layer_dirty(self); }
static T3 l_sub_transform(id self, SEL c) { (void)c; return ls(self)->stf; }
static void l_set_sub_transform(id self, SEL c, T3 t) { (void)c; ls(self)->stf = t; layer_dirty(self); }
static AT l_affine(id self, SEL c) { (void)c; return CATransform3DGetAffineTransform(ls(self)->tf); }
static void l_set_affine(id self, SEL c, AT a) { (void)c; ls(self)->tf = CATransform3DMakeAffineTransform(a); layer_dirty(self); }

static id l_contents(id self, SEL c) { (void)c; return ls(self)->contents; }
static void l_set_contents(id self, SEL c, id v) {
    (void)c;
    keep(&ls(self)->contents, v);
    layer_dirty(self);
    static int n;
    if (v && cx_ca_diag() && log_now(&n)) {
        extern u64 IOSurfaceGetWidth(id), IOSurfaceGetHeight(id);
        extern u32 IOSurfaceGetID(id);
        cx_class_t *k = object_getClass(v);
        if (cx_is_subclass(k, &IOSurface_c))
            QLOG("contents #%d: IOSurface id=%u %llux%llu no layer %p", n, IOSurfaceGetID(v), IOSurfaceGetWidth(v), IOSurfaceGetHeight(v), (void *)self);
        else QLOG("contents #%d: %s no layer %p", n, cx_class_name(k), (void *)self);
    }
}
static double l_cscale(id self, SEL c) { (void)c; return ls(self)->cscale; }
static void l_set_cscale(id self, SEL c, double v) { (void)c; ls(self)->cscale = v > 0 ? v : 1; layer_dirty(self); }
static R l_crect(id self, SEL c) { (void)c; return ls(self)->crect; }
static void l_set_crect(id self, SEL c, R r) { (void)c; ls(self)->crect = r; layer_dirty(self); }
static id l_gravity(id self, SEL c) { (void)c; return ls(self)->gravity ? ls(self)->gravity : (id)kCAGravityResize; }
static void l_set_gravity(id self, SEL c, id g) { (void)c; keep(&ls(self)->gravity, g); layer_dirty(self); }
static float l_opacity(id self, SEL c) { (void)c; return ls(self)->opacity; }
static void l_set_opacity(id self, SEL c, float o) { (void)c; ls(self)->opacity = o; layer_dirty(self); }
static BOOL l_hidden(id self, SEL c) { (void)c; return ls(self)->hidden; }
static void l_set_hidden(id self, SEL c, BOOL b) { (void)c; ls(self)->hidden = b; layer_dirty(self); }
static BOOL l_masks(id self, SEL c) { (void)c; return ls(self)->masks; }
static void l_set_masks(id self, SEL c, BOOL b) { (void)c; ls(self)->masks = b; layer_dirty(self); }
static BOOL l_opaque(id self, SEL c) { (void)c; return ls(self)->opaque; }
static void l_set_opaque(id self, SEL c, BOOL b) { (void)c; ls(self)->opaque = b; }
static BOOL l_flipped(id self, SEL c) { (void)c; return ls(self)->flipped; }
static void l_set_flipped(id self, SEL c, BOOL b) { (void)c; ls(self)->flipped = b; layer_dirty(self); }
static BOOL l_contents_flipped(id self, SEL c) {
    (void)c;
    BOOL f = NO;
    for (id l = self; l; l = ls(l)->super) if (ls(l)->flipped) f = !f;
    return f;
}
static id l_bg(id self, SEL c) { (void)c; return ls(self)->bgc; }
static void l_set_bg(id self, SEL c, id col) { (void)c; keep(&ls(self)->bgc, col); layer_dirty(self); }
static id l_borderc(id self, SEL c) { (void)c; return ls(self)->borderc; }
static void l_set_borderc(id self, SEL c, id col) { (void)c; keep(&ls(self)->borderc, col); layer_dirty(self); }
static double l_border_w(id self, SEL c) { (void)c; return ls(self)->border_w; }
static void l_set_border_w(id self, SEL c, double w) { (void)c; ls(self)->border_w = w; layer_dirty(self); }
static double l_radius(id self, SEL c) { (void)c; return ls(self)->radius; }
static void l_set_radius(id self, SEL c, double r) { (void)c; ls(self)->radius = r; layer_dirty(self); }
static double l_shadow_r(id self, SEL c) { (void)c; return ls(self)->shadow_r; }
static void l_set_shadow_r(id self, SEL c, double r) { (void)c; ls(self)->shadow_r = r; layer_dirty(self); }
static double l_shadow_op(id self, SEL c) { (void)c; return ls(self)->shadow_op; }
static void l_set_shadow_op(id self, SEL c, double o) { (void)c; ls(self)->shadow_op = o < 0 ? 0 : o > 1 ? 1 : o; layer_dirty(self); }
static S l_shadow_off(id self, SEL c) { (void)c; S o = {ls(self)->shadow_ox, ls(self)->shadow_oy}; return o; }
static void l_set_shadow_off(id self, SEL c, S o) { (void)c; ls(self)->shadow_ox = o.w; ls(self)->shadow_oy = o.h; layer_dirty(self); }
static id l_shadow_c(id self, SEL c) { (void)c; return ls(self)->shadowc; }
static void l_set_shadow_c(id self, SEL c, id col) { (void)c; keep(&ls(self)->shadowc, col); layer_dirty(self); }
static id l_delegate(id self, SEL c) { (void)c; return ls(self)->delegate; }
static void l_set_delegate(id self, SEL c, id d) { (void)c; ls(self)->delegate = d; }
static id l_name(id self, SEL c) { (void)c; return ls(self)->name; }
static void l_set_name(id self, SEL c, id n) { (void)c; keep(&ls(self)->name, n ? MSG0(id, n, "copy") : 0); if (n) objc_release(ls(self)->name); }
static id l_mask(id self, SEL c) { (void)c; return ls(self)->mask; }
static void l_set_mask(id self, SEL c, id m) { (void)c; keep(&ls(self)->mask, m); layer_dirty(self); }
static id l_actions(id self, SEL c) { (void)c; return ls(self)->actions; }
static void l_set_actions(id self, SEL c, id a) { (void)c; keep(&ls(self)->actions, a); }
static id l_style(id self, SEL c) { (void)c; return ls(self)->style; }
static void l_set_style(id self, SEL c, id a) { (void)c; keep(&ls(self)->style, a); }
static id l_filters(id self, SEL c) { (void)c; return ls(self)->filters; }
static void l_set_filters(id self, SEL c, id a) { (void)c; keep(&ls(self)->filters, a); }
static id l_minf(id self, SEL c) { (void)c; return ls(self)->minf ? ls(self)->minf : (id)kCAFilterLinear; }
static void l_set_minf(id self, SEL c, id a) { (void)c; keep(&ls(self)->minf, a); }
static id l_magf(id self, SEL c) { (void)c; return ls(self)->magf ? ls(self)->magf : (id)kCAFilterLinear; }
static void l_set_magf(id self, SEL c, id a) { (void)c; keep(&ls(self)->magf, a); }
static void l_noop_b(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; }
static void l_noop_u(id self, SEL c, u64 v) { (void)self; (void)c; (void)v; }
static void l_noop_d(id self, SEL c, double v) { (void)self; (void)c; (void)v; }
static void l_noop_o(id self, SEL c, id v) { (void)self; (void)c; (void)v; }
static void l_noop(id self, SEL c) { (void)self; (void)c; }
static id l_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static id l_nil_o(id self, SEL c, id k) { (void)self; (void)c; (void)k; return 0; }
static void l_add_anim(id self, SEL c, id a, id k) { (void)self; (void)c; (void)a; (void)k; }
static BOOL l_no(id self, SEL c) { (void)self; (void)c; return NO; }
static void l_set_needs_display(id self, SEL c) { (void)c; ls(self)->needs_display = YES; layer_dirty(self); }
static void l_set_needs_display_rect(id self, SEL c, R r) { (void)r; l_set_needs_display(self, c); }
static BOOL l_needs_display(id self, SEL c) { (void)c; return ls(self)->needs_display; }
/* -display: o delegate (que não seja a view dona, que desenha pelo drawRect:) preenche o layer */
static void l_display(id self, SEL c) {
    (void)c;
    lst_t *s = ls(self);
    s->needs_display = NO;
    id d = s->delegate;
    if (d && d != s->host_view && cx_responds(object_getClass(d), sel_registerName("displayLayer:"))) MSG(void, d, "displayLayer:", self);
}
static void l_display_if_needed(id self, SEL c) { if (ls(self)->needs_display) l_display(self, c); }
static void l_layout_if_needed(id self, SEL c) {
    (void)c;
    id d = ls(self)->delegate;
    if (d && cx_responds(object_getClass(d), sel_registerName("layoutSublayersOfLayer:"))) MSG(void, d, "layoutSublayersOfLayer:", self);
}

/* ---- sublayers */
static id l_super(id self, SEL c) { (void)c; return ls(self)->super; }
static id l_sublayers(id self, SEL c) {
    (void)c;
    lst_t *s = ls(self);
    if (!s->nsub) return 0;
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (u32 i = 0; i < s->nsub; i++) MSG(void, a, "addObject:", s->sub[i]);
    return a;
}
static void l_remove_sub(id parent, id child) {
    lst_t *p = ls(parent);
    for (u32 i = 0; i < p->nsub; i++)
        if (p->sub[i] == child) {
            memmove(p->sub + i, p->sub + i + 1, (p->nsub - i - 1) * sizeof(id));
            p->nsub--;
            ls(child)->super = 0;
            layer_dirty(parent);
            objc_release(child);
            return;
        }
}
static void l_insert_at(id self, SEL c, id sub, u32 idx) {
    (void)c;
    if (!sub || sub == self) return;
    objc_retain(sub);
    if (ls(sub)->super) l_remove_sub(ls(sub)->super, sub);
    lst_t *s = ls(self);
    if (s->nsub == s->capsub) { s->capsub = s->capsub ? s->capsub * 2 : 4; s->sub = realloc(s->sub, s->capsub * sizeof(id)); }
    if (idx > s->nsub) idx = s->nsub;
    memmove(s->sub + idx + 1, s->sub + idx, (s->nsub - idx) * sizeof(id));
    s->sub[idx] = sub;
    s->nsub++;
    ls(sub)->super = self;
    layer_dirty(self);
}
static void l_add_sub(id self, SEL c, id sub) { l_insert_at(self, c, sub, ls(self)->nsub); }
static u32 index_of(id self, id sib) { lst_t *s = ls(self); for (u32 i = 0; i < s->nsub; i++) if (s->sub[i] == sib) return i; return s->nsub; }
static void l_insert_idx(id self, SEL c, id sub, u32 idx) { l_insert_at(self, c, sub, idx); }
static void l_insert_below(id self, SEL c, id sub, id sib) { if (sub == sib) return; if (ls(sub)->super == self) l_remove_sub(self, sub); u32 i = sib ? index_of(self, sib) : 0; l_insert_at(self, c, sub, sib && i == ls(self)->nsub ? 0 : i); }
static void l_insert_above(id self, SEL c, id sub, id sib) { if (sub == sib) return; if (ls(sub)->super == self) l_remove_sub(self, sub); u32 i = sib ? index_of(self, sib) : ls(self)->nsub; l_insert_at(self, c, sub, i < ls(self)->nsub ? i + 1 : ls(self)->nsub); }
static void l_remove_from_super(id self, SEL c) { (void)c; if (ls(self)->super) l_remove_sub(ls(self)->super, self); }
static void l_replace(id self, SEL c, id old, id nw) {
    u32 i = index_of(self, old);
    if (i == ls(self)->nsub) return;
    l_remove_sub(self, old);
    if (nw) l_insert_at(self, c, nw, i);
}
static void l_set_sublayers(id self, SEL c, id arr) {
    lst_t *s = ls(self);
    while (s->nsub) l_remove_sub(self, s->sub[s->nsub - 1]);
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n; i++) l_add_sub(self, c, MSG(id, arr, "objectAtIndex:", i));
}
/* conversão de pontos entre layers (sem transformações): pelo deslocamento acumulado até a raiz */
static P origin_in_root(id l) {
    P o = {0, 0};
    for (; l && ls(l)->super; l = ls(l)->super) {
        lst_t *s = ls(l);
        o.x += s->pos.x - s->anchor.x * s->bounds.s.w - s->bounds.o.x;
        o.y += s->pos.y - s->anchor.y * s->bounds.s.h - s->bounds.o.y;
    }
    return o;
}
static P l_convert_to(id self, SEL c, P p, id to) {
    (void)c;
    P a = origin_in_root(self), b = origin_in_root(to);
    P r = {p.x + a.x - b.x, p.y + a.y - b.y};
    return r;
}
static P l_convert_from(id self, SEL c, P p, id from) { return l_convert_to(from, c, p, self); }
static R l_convert_rect_to(id self, SEL c, R r, id to) { r.o = l_convert_to(self, c, r.o, to); return r; }
static R l_convert_rect_from(id self, SEL c, R r, id from) { r.o = l_convert_to(from, c, r.o, self); return r; }
static id l_action_for_key(id self, SEL c, id k) { (void)self; (void)c; (void)k; return 0; }
static id l_default_action(id cls, SEL c, id k) { (void)cls; (void)c; (void)k; return 0; }
static id l_presentation(id self, SEL c) { (void)c; return self; }

/* ---- CALayerHost */
static u32 lh_cid(id self, SEL c) { (void)c; return ls(self)->ctx_id; }
static void lh_set_cid(id self, SEL c, u32 cid) {
    (void)c;
    lst_t *s = ls(self);
    s->ctx_id = cid; s->host_layer = YES; host_add(self); layer_dirty(self);
    QLOG("CALayerHost %p -> contextId %u", (void *)self, cid);
}

/* ---- CAContext */
/* O kernel dá um id único. Sem ele (harness), o contador local basta: só há um processo. */
static int ca_kernel = -1;
static long ca_vm(u32 op, u64 *a) {
    if (!ca_kernel) return -1;
    long r = cx_syscall(SYS_MACH_VM, (long)op, (long)a, 0, 0, 0);
    if (ca_kernel < 0) ca_kernel = r == 0 ? 1 : 0;
    return r;
}
static u32 ctx_alloc_id(void) {
    u64 a[8] = {0};
    if (ca_vm(MACH_VM_CA_ALLOC, a) == 0 && a[0]) return (u32)a[0];
    return __sync_fetch_and_add(&next_cid, 1);
}
static void ctx_release_pub(cactx_t *k) {
    if (!k || !k->mem) return;
    u64 a[8] = {k->cid, 0, 0, 0, 0, 0, 0, 0};
    ca_vm(MACH_VM_CA_DROP, a);
    munmap(k->mem, (size_t)k->memlen);
    k->mem = 0;
    k->memlen = 0;
    k->port = 0;
    k->fw = k->fh = 0;
}
static cactx_t *ctx_new(cx_class_t *cls) {
    cactx_t *k = (cactx_t *)cx_alloc_instance(cls);
    k->cid = ctx_alloc_id();
    for (int i = 0; i < MAX_CTX; i++) if (!ctxs[i]) { ctxs[i] = k; break; }
    return k;
}
static id ctx_with_conn(id cls, SEL c, u32 conn, id opts) { (void)c; (void)conn; (void)opts; return objc_autorelease((id)ctx_new((cx_class_t *)cls)); }
static id ctx_remote(id cls, SEL c, id opts) { (void)c; (void)opts; return objc_autorelease((id)ctx_new((cx_class_t *)cls)); }
static id ctx_local(id cls, SEL c) { (void)c; return objc_autorelease((id)ctx_new((cx_class_t *)cls)); }
static u32 ctx_id(id self, SEL c) { (void)c; return ((cactx_t *)self)->cid; }
static id ctx_layer(id self, SEL c) { (void)c; return ((cactx_t *)self)->layer; }
static void ctx_set_layer(id self, SEL c, id l) {
    (void)c;
    cactx_t *k = (cactx_t *)self;
    if (k->layer) ls(k->layer)->ctx = 0;
    keep(&k->layer, l);
    if (l) { ls(l)->ctx = self; layer_dirty(l); }
    qc_mark_hosts(k->cid, 0);
    QLOG("CAContext %u: layer %p", k->cid, (void *)l);
}
static void ctx_invalidate(id self, SEL c) {
    (void)c;
    cactx_t *k = (cactx_t *)self;
    ctx_release_pub(k);
    for (int i = 0; i < MAX_CTX; i++) if (ctxs[i] == k) ctxs[i] = 0;
    qc_mark_hosts(k->cid, 0);
}
static void ctx_dealloc(id self, SEL c) {
    ctx_invalidate(self, c);
    cactx_t *k = (cactx_t *)self;
    if (k->layer) { ls(k->layer)->ctx = 0; objc_release(k->layer); }
    if (k->cs) objc_release(k->cs);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static id ctx_cs(id self, SEL c) { (void)c; return ((cactx_t *)self)->cs; }
static void ctx_set_cs(id self, SEL c, id cs) { (void)c; keep(&((cactx_t *)self)->cs, cs); }
static id ctx_lookup_layer(u32 cid) { for (int i = 0; i < MAX_CTX; i++) if (ctxs[i] && ctxs[i]->cid == cid) return ctxs[i]->layer; return 0; }
/* CGSMainConnectionID: o Chromium passa a conexão ao CAContext; no mOS é só um número */
u32 CGSMainConnectionID(void) { return 1; }

/* ---- CATransaction: sem animações; commit roda os blocos de conclusão */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); } blk_t;
void *_Block_copy(const void *b);
void _Block_release(const void *b);
static BOOL tx_disable;
static double tx_duration;
static void *tx_blocks[32];
static int ntx_blocks;
static void tx_run_blocks(void) {
    while (ntx_blocks) {
        void *b = tx_blocks[--ntx_blocks];
        ((blk_t *)b)->invoke(b);
        _Block_release(b);
    }
}
static void tx_begin(id cls, SEL c) { (void)cls; (void)c; tx_depth++; }
static void tx_commit(id cls, SEL c) {
    (void)cls; (void)c;
    if (tx_depth > 0) tx_depth--;
    if (!tx_depth) { flush_dirty(); fence_fire(); tx_run_blocks(); }
}
static void tx_flush(id cls, SEL c) {
    (void)cls; (void)c;
    if (!tx_depth) { flush_dirty(); fence_fire(); tx_run_blocks(); }
}
static void tx_set_disable(id cls, SEL c, BOOL b) { (void)cls; (void)c; tx_disable = b; }
static BOOL tx_get_disable(id cls, SEL c) { (void)cls; (void)c; return tx_disable; }
static void tx_set_duration(id cls, SEL c, double d) { (void)cls; (void)c; tx_duration = d; }
static double tx_get_duration(id cls, SEL c) { (void)cls; (void)c; return tx_duration; }
static void tx_set_completion(id cls, SEL c, void *blk) {
    (void)cls; (void)c;
    if (!blk) return;
    if (!tx_depth) { ((blk_t *)blk)->invoke(blk); return; }
    if (ntx_blocks < 32) tx_blocks[ntx_blocks++] = _Block_copy(blk);
}
/* +addCommitHandler:forPhase: (macOS 11+): roda no commit da transação aberta. Sem transação explícita, a implícita do
 * Core Animation fecha no fim da volta do run loop — aqui, na próxima volta (fila principal do GCD). */
void dispatch_async(void *q, void *block);
void *dispatch_get_main_queue(void);
static void tx_add_commit_handler(id cls, SEL c, void *blk, long phase) {
    (void)cls; (void)c; (void)phase;
    if (!blk) return;
    if (tx_depth && ntx_blocks < 32) { tx_blocks[ntx_blocks++] = _Block_copy(blk); return; }
    dispatch_async(dispatch_get_main_queue(), blk);
}
static void tx_set_value(id cls, SEL c, id v, id k) {
    (void)cls; (void)c;
    if (k && !strcmp(cx_cstr(k), "disableActions")) tx_disable = v && MSG0(BOOL, v, "boolValue");
}
static id tx_value(id cls, SEL c, id k) { (void)cls; (void)c; (void)k; return 0; }
static void tx_lock(id cls, SEL c) { (void)cls; (void)c; }

/* ================================================================ composição por software */
/* sx = ax + bx*lx + cx*ly; sy = ay + cy*lx + by*ly. cx e cy são a rotação/cisalhamento. */
typedef struct { double ax, bx, ay, by, cx, cy; } map_t;
typedef struct { int x0, y0, x1, y1; } clip_t;

static u32 *cv;                         /* buffer ARGB pré-multiplicado da área visível da view */
static int cvx, cvy, cvw, cvh;
static size_t cvcap;
static BOOL touched;

static map_t child_map(map_t p, lst_t *c) {
    double w = c->bounds.s.w, h = c->bounds.s.h;
    double a = c->tf.m[0], b = c->tf.m[1], tc = c->tf.m[4], d = c->tf.m[5];
    double tx = c->tf.m[12], ty = c->tf.m[13];
    if (a == 0 && b == 0 && tc == 0 && d == 0) { a = 1; d = 1; }
    double yb = c->flipped ? -1 : 1;
    double vxk = -(c->bounds.o.x + c->anchor.x * w);
    double vyk = c->flipped ? (c->bounds.o.y + h - c->anchor.y * h) : -(c->bounds.o.y + c->anchor.y * h);
    double px0 = c->pos.x + tx + a * vxk + tc * vyk;
    double py0 = c->pos.y + ty + b * vxk + d * vyk;
    double al = a, cl = tc * yb, bl = b, dl = d * yb;
    map_t m;
    m.ax = p.ax + p.bx * px0 + p.cx * py0;
    m.ay = p.ay + p.cy * px0 + p.by * py0;
    m.bx = p.bx * al + p.cx * bl;
    m.cx = p.bx * cl + p.cx * dl;
    m.cy = p.cy * al + p.by * bl;
    m.by = p.cy * cl + p.by * dl;
    return m;
}
static map_t apply_stf(map_t p, T3 t) {
    double a = t.m[0], b = t.m[1], c = t.m[4], d = t.m[5], tx = t.m[12], ty = t.m[13];
    if (a == 0 && b == 0 && c == 0 && d == 0) { a = 1; d = 1; }
    map_t m;
    m.ax = p.ax + p.bx * tx + p.cx * ty;
    m.ay = p.ay + p.cy * tx + p.by * ty;
    m.bx = p.bx * a + p.cx * b;
    m.cx = p.bx * c + p.cx * d;
    m.cy = p.cy * a + p.by * b;
    m.by = p.cy * c + p.by * d;
    return m;
}
static int map_shear(map_t m) { return m.cx != 0 || m.cy != 0; }
static clip_t clip_and(clip_t a, double x0, double y0, double x1, double y1) {
    int ix0 = (int)(x0 + 0.5), iy0 = (int)(y0 + 0.5), ix1 = (int)(x1 + 0.5), iy1 = (int)(y1 + 0.5);
    if (ix0 > a.x0) a.x0 = ix0;
    if (iy0 > a.y0) a.y0 = iy0;
    if (ix1 < a.x1) a.x1 = ix1;
    if (iy1 < a.y1) a.y1 = iy1;
    return a;
}
static u32 premul(u32 c, float op) {
    u32 a = (u32)(((c >> 24) & 255) * op + 0.5f);
    u32 r = ((c >> 16) & 255) * a / 255, g = ((c >> 8) & 255) * a / 255, b = (c & 255) * a / 255;
    return a << 24 | r << 16 | g << 8 | b;
}
static u32 color_of(id col) {
    if (!col) return 0;
    qcolor_t *q = (qcolor_t *)col;
    double v[4] = {q->a, q->r, q->g, q->b};
    u32 o = 0;
    for (int i = 0; i < 4; i++) { double x = v[i] < 0 ? 0 : v[i] > 1 ? 1 : v[i]; o = o << 8 | (u32)(x * 255 + 0.5); }
    return o;
}
/* dst = src + dst * (1 - a_src), tudo pré-multiplicado */
static inline u32 over(u32 s, u32 d) {
    u32 a = s >> 24;
    if (a == 255) return s;
    if (a == 0) return d;
    u32 k = 255 - a;
    u32 rb = ((d & 0xFF00FF) * k + 0x800080) >> 8 & 0xFF00FF;
    u32 ag = ((d >> 8 & 0xFF00FF) * k + 0x800080) & 0xFF00FF00;
    return s + (rb | ag);
}
static inline u32 scale_px(u32 s, u32 k) {      /* s * k/255 (opacidade) */
    u32 rb = ((s & 0xFF00FF) * k + 0x800080) >> 8 & 0xFF00FF;
    u32 ag = ((s >> 8 & 0xFF00FF) * k + 0x800080) & 0xFF00FF00;
    return rb | ag;
}
/* canto arredondado: pilha, porque um layer filho pode recortar dentro do pai */
typedef struct { double x0, y0, x1, y1, r; } rr_t;
static rr_t rr_stk[8];
static int rr_n;
static int rr_outside(double x0, double y0, double x1, double y1, double rad, int x, int y) {
    if (rad < 0.5) return 0;
    double px = x + 0.5, py = y + 0.5, r = rad;
    double ix0 = x0 + r, iy0 = y0 + r, ix1 = x1 - r, iy1 = y1 - r;
    if (px >= x0 && px <= x1 && py >= iy0 && py <= iy1) return 0;
    if (py >= y0 && py <= y1 && px >= ix0 && px <= ix1) return 0;
    double dx = px < ix0 ? ix0 - px : px > ix1 ? px - ix1 : 0;
    double dy = py < iy0 ? iy0 - py : py > iy1 ? py - iy1 : 0;
    return dx * dx + dy * dy > r * r;
}
static int rr_out(int x, int y) {
    for (int i = 0; i < rr_n; i++) if (rr_outside(rr_stk[i].x0, rr_stk[i].y0, rr_stk[i].x1, rr_stk[i].y1, rr_stk[i].r, x, y)) return 1;
    return 0;
}
static void fill(clip_t k, double x0, double y0, double x1, double y1, u32 pc) {
    k = clip_and(k, x0, y0, x1, y1);
    if (k.x0 >= k.x1 || k.y0 >= k.y1 || !(pc >> 24)) return;
    touched = YES;
    for (int y = k.y0; y < k.y1; y++) {
        u32 *row = cv + (size_t)(y - cvy) * cvw - cvx;
        for (int x = k.x0; x < k.x1; x++) {
            if (rr_out(x, y)) continue;
            row[x] = over(pc, row[x]);
        }
    }
}

/* fonte de pixels de um contents */
typedef struct { const u8 *px; long bpr; int w, h; BOOL premul; } src_t;
static int ca_frame(u32 cid, src_t *s);
typedef struct { int w, h; u32 *px; double dw, dh; } qc_imgst_t;    /* = imgst_t do appkit.c (NSImage) */
static BOOL get_src(id contents, src_t *s) {
    if (!contents) return NO;
    cx_class_t *k = object_getClass(contents);
    if (cx_is_subclass(k, &IOSurface_c)) {
        extern void *IOSurfaceGetBaseAddress(id);
        extern u64 IOSurfaceGetWidth(id), IOSurfaceGetHeight(id), IOSurfaceGetBytesPerRow(id), IOSurfaceGetBytesPerElement(id);
        if (IOSurfaceGetBytesPerElement(contents) != 4) return NO;
        s->px = IOSurfaceGetBaseAddress(contents);
        s->bpr = (long)IOSurfaceGetBytesPerRow(contents);
        s->w = (int)IOSurfaceGetWidth(contents);
        s->h = (int)IOSurfaceGetHeight(contents);
        s->premul = YES;
        {   /* diagnóstico (ca-log): o que há dentro do IOSurface (o display do Chromium desenha nele; a janela do VS Code
             * continuava lisa mesmo com o renderer mandando quadros) — cores distintas numa grade de 16x16 e 3 amostras */
            static int n;
            if (s->px && cx_ca_diag() && log_now(&n)) {
                u32 seen[64]; int ns = 0;
                for (int gy = 0; gy < 16; gy++)
                    for (int gx = 0; gx < 16; gx++) {
                        u32 v = *(const u32 *)(s->px + (long)(gy * (s->h - 1) / 15) * s->bpr + (long)(gx * (s->w - 1) / 15) * 4);
                        int k2 = 0;
                        while (k2 < ns && seen[k2] != v) k2++;
                        if (k2 == ns && ns < 64) seen[ns++] = v;
                    }
                const u32 *p0 = (const u32 *)s->px;
                QLOG("IOSurface %dx%d (bpr %ld) desenhado #%d: %d cores na grade; (0,0)=%08x centro=%08x (40,100)=%08x", s->w, s->h,
                     s->bpr, n, ns, p0[0], *(const u32 *)(s->px + (long)(s->h / 2) * s->bpr + (long)(s->w / 2) * 4),
                     *(const u32 *)(s->px + 100L * s->bpr + 40L * 4));
            }
        }
        return s->px != 0;
    }
    extern int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul);
    int prem = 0;
    if (cx_cgimage_src(contents, &s->px, &s->bpr, &s->w, &s->h, &prem)) { s->premul = prem ? YES : NO; return YES; }
    cx_class_t *img = cx_class_named("NSImage");
    if (img && cx_is_subclass(k, img)) {
        qc_imgst_t *st = *(qc_imgst_t **)((u8 *)contents + 8);
        if (!st || !st->px || st->w <= 0 || st->h <= 0) return NO;
        s->px = (const u8 *)st->px; s->bpr = st->w * 4; s->w = st->w; s->h = st->h; s->premul = NO;
        return YES;
    }
    return NO;
}
static BOOL streq(id a, const char *b) { return a && !strcmp(cx_cstr(a), b); }

/* desenha a imagem (já recortada por contentsRect: sx0, sy0, sw, sh) no retângulo visual dx0..dx1 x dy0..dy1 */
static void blit(clip_t k, const src_t *s, double sx0, double sy0, double sw, double sh,
                 double dx0, double dy0, double dx1, double dy1, float op) {
    clip_t c = clip_and(k, dx0, dy0, dx1, dy1);
    if (c.x0 >= c.x1 || c.y0 >= c.y1 || sw <= 0 || sh <= 0) return;
    u32 ko = (u32)(op * 255 + 0.5f);
    if (!ko) return;
    touched = YES;
    double fx = sw / (dx1 - dx0), fy = sh / (dy1 - dy0);
    int linear = fx < 0.98 || fx > 1.02 || fy < 0.98 || fy > 1.02;
    for (int y = c.y0; y < c.y1; y++) {
        double sy = sy0 + (y + 0.5 - dy0) * fy;
        if (sy < 0) sy = 0;
        if (sy > s->h - 1) sy = s->h - 1;
        u32 *row = cv + (size_t)(y - cvy) * cvw - cvx;
        double u = sx0 + (c.x0 + 0.5 - dx0) * fx;
        for (int x = c.x0; x < c.x1; x++, u += fx) {
            if (rr_out(x, y)) continue;
            double sx = u;
            if (sx < 0) sx = 0;
            if (sx > s->w - 1) sx = s->w - 1;
            int x0 = (int)sx, y0 = (int)sy;
            u32 p;
            if (!linear) {
                p = ((const u32 *)(s->px + (long)y0 * s->bpr))[x0];
            } else {
                int x1 = x0 + 1 < s->w ? x0 + 1 : x0, y1 = y0 + 1 < s->h ? y0 + 1 : y0;
                u32 p00 = ((const u32 *)(s->px + (long)y0 * s->bpr))[x0];
                u32 p10 = ((const u32 *)(s->px + (long)y0 * s->bpr))[x1];
                u32 p01 = ((const u32 *)(s->px + (long)y1 * s->bpr))[x0];
                u32 p11 = ((const u32 *)(s->px + (long)y1 * s->bpr))[x1];
                if (!s->premul) { p00 = premul(p00, 1); p10 = premul(p10, 1); p01 = premul(p01, 1); p11 = premul(p11, 1); }
                double tx = sx - x0, ty = sy - y0;
                u32 o = 0;
                for (int ch = 0; ch < 4; ch++) {
                    double a = ((p00 >> (ch * 8)) & 255) * (1 - tx) + ((p10 >> (ch * 8)) & 255) * tx;
                    double b = ((p01 >> (ch * 8)) & 255) * (1 - tx) + ((p11 >> (ch * 8)) & 255) * tx;
                    u32 v = (u32)(a * (1 - ty) + b * ty + 0.5);
                    if (v > 255) v = 255;
                    o |= v << (ch * 8);
                }
                p = o;
            }
            if (!linear && !s->premul) p = premul(p, 1);
            if (ko != 255) p = scale_px(p, ko);
            row[x] = over(p, row[x]);
        }
    }
}

static void draw_contents(lst_t *L, clip_t k, double rx0, double ry0, double rx1, double ry1, BOOL yup, float op) {
    src_t s;
    if (!get_src(L->contents, &s)) return;
    /* contentsRect (unitário, origem no topo da imagem) */
    double sx0 = L->crect.o.x * s.w, sy0 = L->crect.o.y * s.h, sw = L->crect.s.w * s.w, sh = L->crect.s.h * s.h;
    double rw = rx1 - rx0, rh = ry1 - ry0;
    double iw = sw / L->cscale, ih = sh / L->cscale;          /* tamanho natural em pontos (= pixels no mOS) */
    id g = L->gravity;
    double dx0 = rx0, dy0 = ry0, dw = rw, dh = rh;
    if (!g || streq(g, "resize")) {
    } else if (streq(g, "resizeAspect") || streq(g, "resizeAspectFill")) {
        double f = rw / iw, f2 = rh / ih;
        f = streq(g, "resizeAspect") ? (f < f2 ? f : f2) : (f > f2 ? f : f2);
        dw = iw * f; dh = ih * f;
        dx0 = rx0 + (rw - dw) / 2; dy0 = ry0 + (rh - dh) / 2;
    } else {
        const char *n = cx_cstr(g);
        dw = iw; dh = ih;
        BOOL left = strstr(n, "eft") != 0 || !strcmp(n, "left"), right = strstr(n, "ight") != 0;
        BOOL top = !strncmp(n, "top", 3), bottom = !strncmp(n, "bottom", 6);
        dx0 = left ? rx0 : right ? rx1 - dw : rx0 + (rw - dw) / 2;
        /* "top" é o y máximo do layer: em cima na tela se y cresce para cima, embaixo se o layer é invertido */
        BOOL vis_top = yup ? top : bottom, vis_bottom = yup ? bottom : top;
        dy0 = vis_top ? ry0 : vis_bottom ? ry1 - dh : ry0 + (rh - dh) / 2;
    }
    blit(k, &s, sx0, sy0, sw, sh, dx0, dy0, dx0 + dw, dy0 + dh, op);
}

static int cmp_z(const void *a, const void *b) {
    double za = ls(*(id *)a)->z, zb = ls(*(id *)b)->z;
    return za < zb ? -1 : za > zb;
}
static void map_point(map_t m, double lx, double ly, double *sx, double *sy) {
    *sx = m.ax + m.bx * lx + m.cx * ly;
    *sy = m.ay + m.cy * lx + m.by * ly;
}
/* o buffer foi desenhado com `loc` (eixos retos). Cada pixel da tela volta à coordenada do layer e amostra. */
static void splat_affine(const u32 *src, int sw, int sh, map_t loc, map_t sc, clip_t k,
                         double lx0, double ly0, double lx1, double ly1) {
    if (!cv || !src || sw < 1 || sh < 1) return;
    double xs[4], ys[4];
    map_point(sc, lx0, ly0, &xs[0], &ys[0]);
    map_point(sc, lx1, ly0, &xs[1], &ys[1]);
    map_point(sc, lx0, ly1, &xs[2], &ys[2]);
    map_point(sc, lx1, ly1, &xs[3], &ys[3]);
    double minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < minx) minx = xs[i];
        if (xs[i] > maxx) maxx = xs[i];
        if (ys[i] < miny) miny = ys[i];
        if (ys[i] > maxy) maxy = ys[i];
    }
    clip_t c = clip_and(k, minx, miny, maxx, maxy);
    c = clip_and(c, cvx, cvy, cvx + cvw, cvy + cvh);
    if (c.x0 >= c.x1 || c.y0 >= c.y1) return;
    double det = sc.bx * sc.by - sc.cx * sc.cy;
    if (det < 1e-8 && det > -1e-8) return;
    double inv = 1.0 / det;
    touched = YES;
    for (int y = c.y0; y < c.y1; y++) {
        u32 *row = cv + (size_t)(y - cvy) * cvw - cvx;
        for (int x = c.x0; x < c.x1; x++) {
            double dx = x + 0.5 - sc.ax, dy = y + 0.5 - sc.ay;
            double lx = (sc.by * dx - sc.cx * dy) * inv;
            double ly = (-sc.cy * dx + sc.bx * dy) * inv;
            if (lx < lx0 || lx > lx1 || ly < ly0 || ly > ly1) continue;
            double bu = loc.ax + loc.bx * lx + loc.cx * ly;
            double bv = loc.ay + loc.cy * lx + loc.by * ly;
            if (bu < 0 || bv < 0 || bu >= sw || bv >= sh) continue;
            int x0 = (int)bu, y0 = (int)bv;
            int x1 = x0 + 1 < sw ? x0 + 1 : x0, y1 = y0 + 1 < sh ? y0 + 1 : y0;
            double tx = bu - x0, ty = bv - y0;
            u32 p00 = src[(size_t)y0 * sw + x0], p10 = src[(size_t)y0 * sw + x1];
            u32 p01 = src[(size_t)y1 * sw + x0], p11 = src[(size_t)y1 * sw + x1];
            u32 p = 0;
            for (int ch = 0; ch < 4; ch++) {
                double aa = ((p00 >> (ch * 8)) & 255) * (1 - tx) + ((p10 >> (ch * 8)) & 255) * tx;
                double bb = ((p01 >> (ch * 8)) & 255) * (1 - tx) + ((p11 >> (ch * 8)) & 255) * tx;
                u32 v = (u32)(aa * (1 - ty) + bb * ty + 0.5);
                if (v > 255) v = 255;
                p |= v << (ch * 8);
            }
            if (p >> 24) row[x] = over(p, row[x]);
        }
    }
}
static int xform_once, mask_once;
static void render(id l, map_t m, clip_t k, float op, int depth, BOOL skip_bg) {
    lst_t *L = ls(l);
    if (L->hidden || depth > 64) return;
    float op_in = op;
    op *= L->opacity;
    if (op <= 0.002f) return;
    if (L->needs_display) l_display(l, 0);
    if (xform_once) xform_once = 0;
    else if (map_shear(m) && L->bounds.s.w >= 1 && L->bounds.s.h >= 1 && cv) {
        int bw = (int)(L->bounds.s.w + 0.5), bh = (int)(L->bounds.s.h + 0.5);
        if (bw > 4096) bw = 4096;
        if (bh > 4096) bh = 4096;
        if (bw < 1) bw = 1;
        if (bh < 1) bh = 1;
        u32 *buf = calloc((size_t)bw * bh, 4);
        if (buf) {
            u32 *ocv = cv; int ocx = cvx, ocy = cvy, ocw = cvw, och = cvh; BOOL ot = touched;
            cv = buf; cvx = 0; cvy = 0; cvw = bw; cvh = bh; touched = NO;
            double sx = bw / L->bounds.s.w, sy = bh / L->bounds.s.h;
            map_t loc = {-L->bounds.o.x * sx, sx, 0, sy, 0, 0};
            if (m.by >= 0) loc.ay = -L->bounds.o.y * sy;
            else { loc.by = -sy; loc.ay = (L->bounds.o.y + L->bounds.s.h) * sy; }
            xform_once = 1;
            render(l, loc, (clip_t){0, 0, bw, bh}, op_in, depth, skip_bg);
            cv = ocv; cvx = ocx; cvy = ocy; cvw = ocw; cvh = och; touched = ot;
            splat_affine(buf, bw, bh, loc, m, k, L->bounds.o.x, L->bounds.o.y,
                         L->bounds.o.x + L->bounds.s.w, L->bounds.o.y + L->bounds.s.h);
            free(buf);
            return;
        }
    }
    if (mask_once) mask_once = 0;
    else if (L->mask && cv && cvw > 0 && cvh > 0) {
        size_t n = (size_t)cvw * cvh;
        u32 *tmp = calloc(n, 4), *mb = calloc(n, 4);
        if (tmp && mb) {
            u32 *ocv = cv; int ocx = cvx, ocy = cvy, ocw = cvw, och = cvh; BOOL ot = touched;
            cv = tmp; touched = NO;
            mask_once = 1;
            render(l, m, k, op_in, depth, skip_bg);
            cv = mb; touched = NO;
            render(L->mask, child_map(m, ls(L->mask)), k, 1, depth + 1, NO);
            cv = ocv; cvx = ocx; cvy = ocy; cvw = ocw; cvh = och; touched = ot;
            for (size_t i = 0; i < n; i++) {
                u32 a = mb[i] >> 24;
                if (!a || !(tmp[i] >> 24)) continue;
                u32 p = a == 255 ? tmp[i] : scale_px(tmp[i], a);
                u32 *px = ocv + i;
                *px = over(p, *px);
                touched = YES;
            }
            free(tmp); free(mb);
            return;
        }
        free(tmp); free(mb);
    }
    double xa = m.ax + m.bx * L->bounds.o.x, xb = m.ax + m.bx * (L->bounds.o.x + L->bounds.s.w);
    double ya = m.ay + m.by * L->bounds.o.y, yb = m.ay + m.by * (L->bounds.o.y + L->bounds.s.h);
    double rx0 = xa < xb ? xa : xb, rx1 = xa < xb ? xb : xa, ry0 = ya < yb ? ya : yb, ry1 = ya < yb ? yb : ya;
    if (L->shadow_op > 0.01 && L->shadow_r > 0.5) {
        double sc = m.bx < 0 ? -m.bx : m.bx;
        double rad = L->shadow_r * (sc > 0 ? sc : 1);
        double dx = L->shadow_ox * m.bx, dy = L->shadow_oy * m.by;
        u32 col = L->shadowc ? color_of(L->shadowc) : 0xFF000000u;
        for (int i = 3; i >= 1; i--) {
            double e = rad * i / 3.0;
            float a = (float)L->shadow_op * (float)(4 - i) / 6.f;
            fill(k, rx0 + dx - e, ry0 + dy - e, rx1 + dx + e, ry1 + dy + e, premul(col, op * a));
        }
    }
    int pushed = 0;
    if (L->radius > 0.5 && rr_n < 8) {
        double sc = m.bx < 0 ? -m.bx : m.bx;
        rr_stk[rr_n].x0 = rx0; rr_stk[rr_n].y0 = ry0; rr_stk[rr_n].x1 = rx1; rr_stk[rr_n].y1 = ry1;
        rr_stk[rr_n].r = L->radius * (sc > 0 ? sc : 1);
        rr_n++;
        pushed = 1;
    }
    if (!skip_bg && L->bgc) fill(k, rx0, ry0, rx1, ry1, premul(color_of(L->bgc), op));
    if (L->contents) draw_contents(L, k, rx0, ry0, rx1, ry1, m.by < 0, op);
    if (L->border_w > 0 && L->borderc) {
        u32 bc = premul(color_of(L->borderc), op);
        double b = L->border_w;
        fill(k, rx0, ry0, rx1, ry0 + b, bc);
        fill(k, rx0, ry1 - b, rx1, ry1, bc);
        fill(k, rx0, ry0 + b, rx0 + b, ry1 - b, bc);
        fill(k, rx1 - b, ry0 + b, rx1, ry1 - b, bc);
    }
    if (pushed && !L->masks) { rr_n--; pushed = 0; }
    clip_t kk = L->masks ? clip_and(k, rx0, ry0, rx1, ry1) : k;
    /* CALayerHost: árvore do mesmo processo, ou o quadro publicado por outro */
    if (L->host_layer && L->ctx_id) {
        id hl = ctx_lookup_layer(L->ctx_id);
        if (hl && hl != l) render(hl, child_map(m, ls(hl)), kk, op, depth + 1, NO);
        else if (!hl) {
            src_t rem;
            if (ca_frame(L->ctx_id, &rem)) blit(kk, &rem, 0, 0, rem.w, rem.h, rx0, ry0, rx1, ry1, op);
        }
    }
    if (L->nsub) {
        id tmp[64], *subs = L->nsub <= 64 ? tmp : malloc(L->nsub * sizeof(id));
        if (subs) {
            memcpy(subs, L->sub, L->nsub * sizeof(id));
            u32 n = L->nsub;
            BOOL any_z = NO;
            for (u32 i = 0; i < n; i++) if (ls(subs[i])->z != 0) any_z = YES;
            if (any_z) {                        /* ordem estável por zPosition */
                for (u32 i = 1; i < n; i++) for (u32 j = i; j > 0 && cmp_z(&subs[j - 1], &subs[j]) > 0; j--) { id t = subs[j]; subs[j] = subs[j - 1]; subs[j - 1] = t; }
            }
            map_t sm = apply_stf(m, L->stf);
            for (u32 i = 0; i < n; i++) render(subs[i], child_map(sm, ls(subs[i])), kk, op, depth + 1, NO);
            if (subs != tmp) free(subs);
        }
    }
    if (pushed && rr_n > 0) rr_n--;
}

/* manda o buffer à janela em faixas GC_IMAGE (ARGB não pré-multiplicado, como o kernel espera) */
extern int cx_gui_cur_gid;
extern void cx_gui_flush(void);
static u8 *strip;
static void send_canvas(void) {
    if (!touched || cvw <= 0 || cvh <= 0) return;
    cx_gui_flush();
    u32 maxrows = (GUI_MAX_BATCH - 64) / ((u32)cvw * 4);
    if (!maxrows) return;
    if (!strip) strip = malloc(GUI_MAX_BATCH);
    if (!strip) return;
    for (int y0 = 0; y0 < cvh; y0 += (int)maxrows) {
        int rows = cvh - y0 < (int)maxrows ? cvh - y0 : (int)maxrows;
        /* pula faixas totalmente transparentes */
        const u32 *src = cv + (size_t)y0 * cvw;
        size_t n = (size_t)rows * cvw, i = 0;
        while (i < n && !(src[i] >> 24)) i++;
        if (i == n) continue;
        u32 len = 8 + 16 + (u32)n * 4, op = GC_IMAGE;
        i32 iv[4] = {cvx, cvy + y0, cvw, rows};
        memcpy(strip, &op, 4);
        memcpy(strip + 4, &len, 4);
        memcpy(strip + 8, iv, 16);
        u32 *dst = (u32 *)(strip + 24);
        for (i = 0; i < n; i++) {
            u32 p = src[i], a = p >> 24;
            if (a == 255 || a == 0) { dst[i] = p; continue; }
            u32 r = ((p >> 16) & 255) * 255 / a, g = ((p >> 8) & 255) * 255 / a, b = (p & 255) * 255 / a;
            dst[i] = a << 24 | (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
        }
        SYS3(GUI_DRAW, cx_gui_cur_gid, strip, len);
    }
}

/* quadro publicado: página de cabeçalho + pixels BGRA pré-multiplicados, origem no canto superior esquerdo */
#define CA_FRAME_MAGIC 0x52464143u
typedef struct { u32 magic, gen; i32 w, h; } ca_hdr_t;

static void render_layer_to(id layer, u32 *px, int w, int h) {
    u32 *ocv = cv;
    int ocx = cvx, ocy = cvy, ocw = cvw, och = cvh;
    cv = px; cvx = 0; cvy = 0; cvw = w; cvh = h;
    lst_t *L = ls(layer);
    map_t m = {-L->bounds.o.x, 1, -L->bounds.o.y, 1};
    clip_t k = {0, 0, w, h};
    memset(px, 0, (size_t)w * h * 4);
    render(layer, m, k, 1, 0, NO);
    cv = ocv; cvx = ocx; cvy = ocy; cvw = ocw; cvh = och;
}
static void tree_extent(id l, int depth, double *w, double *h) {
    if (!l || depth > 32) return;
    lst_t *s = ls(l);
    if (s->bounds.s.w > *w) *w = s->bounds.s.w;
    if (s->bounds.s.h > *h) *h = s->bounds.s.h;
    for (u32 i = 0; i < s->nsub; i++) tree_extent(s->sub[i], depth + 1, w, h);
}
static void publish_context(cactx_t *k) {
    if (!k || !k->layer || publishing || ca_kernel == 0) return;
    lst_t *L = ls(k->layer);
    int w = (int)(L->bounds.s.w + 0.5), h = (int)(L->bounds.s.h + 0.5);
    if (w < 1 || h < 1) {
        double mw = 0, mh = 0;
        tree_extent(k->layer, 0, &mw, &mh);
        w = (int)(mw + 0.5);
        h = (int)(mh + 0.5);
    }
    if (w < 1 || h < 1) return;
    if (w > 8192) w = 8192;
    if (h > 8192) h = 8192;
    u64 pix = (u64)w * h * 4;
    u64 need = 4096 + ((pix + 0xFFF) & ~0xFFFULL);
    publishing = 1;
    rendering++;
    if (!k->mem || k->fw != w || k->fh != h || k->memlen < need) {
        u8 *mem = mmap(0, (size_t)need, 3, 0x1002, -1, 0);
        if (mem == (void *)-1 || !mem) { rendering--; publishing = 0; return; }
        memset(mem, 0, 4096);
        u64 size = need;
        mach_port_t port = 0;
        if (mach_make_memory_entry_64(mach_task_self(), &size, (u64)mem, 3, &port, 0) != 0 || !port) {
            munmap(mem, (size_t)need);
            rendering--;
            publishing = 0;
            return;
        }
        if (k->mem) munmap(k->mem, (size_t)k->memlen);
        k->mem = mem;
        k->memlen = need;
        k->fw = w;
        k->fh = h;
        k->port = port;
    }
    render_layer_to(k->layer, (u32 *)(k->mem + 4096), w, h);
    ca_hdr_t *hdr = (ca_hdr_t *)k->mem;
    hdr->magic = CA_FRAME_MAGIC;
    hdr->w = w;
    hdr->h = h;
    u64 a[8] = {k->cid, k->port, 0, 0, 0, 0, 0, 0};
    if (ca_vm(MACH_VM_CA_PUBLISH, a) == 0) hdr->gen = (u32)a[2];
    { static int n; if (cx_ca_diag() && log_now(&n)) QLOG("CAContext %u publicado #%d: %dx%d geração %u, ~%u pixels visíveis", k->cid, n, w, h, hdr->gen, count_visible((u32 *)(k->mem + 4096), (size_t)w * h)); }
    rendering--;
    publishing = 0;
    /* um IOSurfaceUnlock durante o achatamento pede mais um quadro, fora desta chamada */
    if (surf_dirty) { surf_dirty = 0; note_ctx_id(k->cid); }
}
static u32 dirty_cid[MAX_CTX];
static int ndirty;
static void note_ctx_id(u32 cid) {
    if (!cid || publishing || rendering) return;
    for (int i = 0; i < ndirty; i++) if (dirty_cid[i] == cid) goto queued;
    if (ndirty < MAX_CTX) dirty_cid[ndirty++] = cid;
queued:
    if (!tx_depth) flush_dirty();
}
static void flush_dirty(void) {
    if (publishing || !ndirty) return;
    u32 ids[MAX_CTX];
    int n = ndirty;
    ndirty = 0;
    memcpy(ids, dirty_cid, (size_t)n * sizeof ids[0]);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < MAX_CTX; j++)
            if (ctxs[j] && ctxs[j]->cid == ids[i]) publish_context(ctxs[j]);
}

/* o processo da janela mapeia o quadro do contexto uma vez e o relê quando a geração muda */
typedef struct { u32 cid, gen, port; u8 *base; u64 len; int w, h; } rview_t;
static rview_t rviews[MAX_CTX];
static int ca_frame(u32 cid, src_t *s) {
    u64 g[8] = {cid, 0, 0, 0, 0, 0, 0, 0};
    if (ca_vm(MACH_VM_CA_GEN, g) != 0 || !g[1]) return 0;
    rview_t *v = 0;
    for (int i = 0; i < MAX_CTX; i++) {
        if (rviews[i].cid == cid) { v = &rviews[i]; break; }
        if (!v && !rviews[i].cid) v = &rviews[i];
    }
    if (!v) return 0;
    if (!v->base || v->gen != (u32)g[1]) {
        int same = v->base && ((ca_hdr_t *)v->base)->magic == CA_FRAME_MAGIC && ((ca_hdr_t *)v->base)->gen == (u32)g[1]
                   && ((ca_hdr_t *)v->base)->w == v->w && ((ca_hdr_t *)v->base)->h == v->h;
        if (!same) {
            u64 b[8] = {cid, 0, 0, 0, 0, 0, 0, 0};
            if (ca_vm(MACH_VM_CA_LOOKUP, b) != 0 || !b[1]) return 0;
            if (v->base && v->port == (u32)b[1] && v->len == b[2]) mach_port_deallocate(mach_task_self(), (u32)b[1]);
            else {
                u64 addr = 0;
                if (mach_vm_map(mach_task_self(), &addr, b[2], 0, 1, (u32)b[1], 0, 0, 3, 3, 0) != 0 || !addr) {
                    mach_port_deallocate(mach_task_self(), (u32)b[1]);
                    return 0;
                }
                if (v->base) munmap(v->base, (size_t)v->len);
                v->base = (u8 *)addr;
                v->len = b[2];
                v->port = (u32)b[1];
            }
            ca_hdr_t *h = (ca_hdr_t *)v->base;
            if (h->magic != CA_FRAME_MAGIC || h->w < 1 || h->h < 1) return 0;
            v->w = h->w;
            v->h = h->h;
        }
        { static int n; if (cx_ca_diag() && log_now(&n)) QLOG("contexto remoto %u: quadro %dx%d geração %u (#%d)", cid, v->w, v->h, (u32)g[1], n); }
        v->cid = cid;
        v->gen = (u32)g[1];
    }
    s->px = v->base + 4096;
    s->bpr = (long)v->w * 4;
    s->w = v->w;
    s->h = v->h;
    s->premul = YES;
    return s->px != 0;
}

/* cerca do CAContext: o Chromium pede um direito de envio e o segura até o quadro seguinte.
 * O commit só precisa não travar; o handler (quando há) corre no fim da transação. */
static void *fence_blks[16];
static int nfence;
static void fence_fire(void) {
    while (nfence) {
        void *b = fence_blks[--nfence];
        ((blk_t *)b)->invoke(b);
        _Block_release(b);
    }
}
static u32 ctx_fence(id self, SEL c) {
    (void)self; (void)c;
    u32 opt[2] = {0x10, 0};                /* MPO_INSERT_SEND_RIGHT */
    mach_port_t p = 0;
    if (mach_port_construct(mach_task_self(), opt, 0, &p) != 0) return 0;
    return p;
}
static void ctx_set_fence(id self, SEL c, u32 port) { (void)self; (void)c; (void)port; }
static void ctx_set_fence_blk(id self, SEL c, u32 port, void *blk) {
    (void)self; (void)c; (void)port;
    if (!blk) return;
    if (!tx_depth) { ((blk_t *)blk)->invoke(blk); return; }
    if (nfence < 16) fence_blks[nfence++] = _Block_copy(blk);
}

void cx_ca_poll(void) {
    for (int i = 0; i < nhosts; i++) {
        id h = hosts[i];
        if (!h) continue;
        u32 cid = ls(h)->ctx_id;
        if (!cid || ctx_lookup_layer(cid)) continue;
        u64 a[8] = {cid, 0, 0, 0, 0, 0, 0, 0};
        if (ca_vm(MACH_VM_CA_GEN, a) != 0 || !a[1]) continue;
        int seen = 0;
        for (int j = 0; j < MAX_CTX; j++) if (rviews[j].cid == cid) { seen = rviews[j].gen == (u32)a[1]; break; }
        if (!seen) layer_dirty(h);
    }
}

/* diagnóstico (ca-log): árvore de layers */
static int qc_tree_has_surface(id l, int depth) {
    if (!l || depth > 16) return 0;
    lst_t *L = ls(l);
    if (L->contents && cx_is_subclass(object_getClass(L->contents), &IOSurface_c)) return 1;
    for (u32 i = 0; i < L->nsub; i++) if (qc_tree_has_surface(L->sub[i], depth + 1)) return 1;
    return 0;
}
static void qc_dump_tree(id l, int depth) {
    if (!l || depth > 10) return;
    lst_t *L = ls(l);
    extern u64 IOSurfaceGetWidth(id), IOSurfaceGetHeight(id);
    BOOL surf = L->contents && cx_is_subclass(object_getClass(L->contents), &IOSurface_c);
    QLOG("%*s%s %p bounds (%.0f,%.0f %.0fx%.0f) pos (%.1f,%.1f) anchor (%.2f,%.2f) escala (%.2f,%.2f) oculto %d op %.2f masks %d flip %d "
         "autoresize %u contents %s%s%llux%llu gravity %s cscale %.2f filhos %u",
         depth * 2, "", cx_class_name(object_getClass(l)), (void *)l, L->bounds.o.x, L->bounds.o.y, L->bounds.s.w, L->bounds.s.h,
         L->pos.x, L->pos.y, L->anchor.x, L->anchor.y, L->tf.m[0], L->tf.m[5], L->hidden, L->opacity, L->masks, L->flipped,
         L->autoresize, L->contents ? cx_class_name(object_getClass(L->contents)) : "-", surf ? " " : "",
         surf ? IOSurfaceGetWidth(L->contents) : 0ULL, surf ? IOSurfaceGetHeight(L->contents) : 0ULL,
         L->gravity ? cx_cstr(L->gravity) : "-", L->cscale, L->nsub);
    for (u32 i = 0; i < L->nsub; i++) qc_dump_tree(L->sub[i], depth + 1);
}

/* o gancho chamado pelo appkit.c (draw_view_tree) */
static void layer_draw_hook(id view, int phase, double tlx, double tly, double w, double h,
                            double cx0, double cy0, double cx1, double cy1) {
    vl_t *e = vl_find(view, NO);
    if (!e || !e->layer || cx_gui_cur_gid <= 0) return;
    id l = e->layer;
    lst_t *L = ls(l);
    rendering++;
    /* o layer raiz acompanha a view (bounds = tamanho da view, frame = bounds da view) */
    set_size(l, w, h);
    L->pos.x = L->anchor.x * w;
    L->pos.y = L->anchor.y * h;
    L->host_view = view;
    /* mapa da raiz: y para baixo se a view ou o layer forem invertidos, senão y para cima */
    BOOL yflip = MSG0(BOOL, view, "isFlipped") || L->flipped;
    map_t m = {tlx - L->bounds.o.x, 1, yflip ? tly - L->bounds.o.y : tly + h + L->bounds.o.y, yflip ? 1 : -1};
    clip_t k = {(int)(cx0 > tlx ? cx0 : tlx), (int)(cy0 > tly ? cy0 : tly),
                (int)(cx1 < tlx + w ? cx1 : tlx + w), (int)(cy1 < tly + h ? cy1 : tly + h)};
    if (k.x1 <= k.x0 || k.y1 <= k.y0) { rendering--; return; }
    if (phase == 0) {
        /* fundo do layer raiz, por baixo do que a view desenha */
        if (MSG0(BOOL, view, "wantsUpdateLayer") && cx_responds(object_getClass(view), sel_registerName("updateLayer"))) MSG0(void, view, "updateLayer");
        if (L->bgc && !L->hidden) {
            u32 col = color_of(L->bgc);
            cx_gui_flush();
            u8 b[8 + 20];
            u32 op = GC_FILL, len = sizeof b;
            i32 iv[4] = {k.x0, k.y0, k.x1 - k.x0, k.y1 - k.y0};
            memcpy(b, &op, 4); memcpy(b + 4, &len, 4); memcpy(b + 8, iv, 16); memcpy(b + 24, &col, 4);
            SYS3(GUI_DRAW, cx_gui_cur_gid, b, len);
        }
        rendering--;
        return;
    }
    cvx = k.x0; cvy = k.y0; cvw = k.x1 - k.x0; cvh = k.y1 - k.y0;
    size_t need = (size_t)cvw * cvh;
    if (need > cvcap) { free(cv); cv = malloc(need * 4); cvcap = cv ? need : 0; }
    if (!cv) { rendering--; return; }
    memset(cv, 0, need * 4);
    touched = NO;
    render(l, m, k, 1, 0, YES);
    {   /* contagem por janela: as primeiras 5 de cada uma (a janela principal não gasta a cota das outras, como os
         * diálogos do Chrome) e 1 a cada 120 */
        static int n, per[16];
        int g = cx_gui_cur_gid > 0 && cx_gui_cur_gid < 16 ? cx_gui_cur_gid : 0;
        ++n;
        if (cx_ca_diag() && (++per[g] <= 5 || n % 120 == 0))
            QLOG("quadro #%d na janela %d: view %s %dx%d em (%d,%d), ~%u pixels de layer visíveis", n, cx_gui_cur_gid, cx_class_name(object_getClass(view)), cvw, cvh, cvx, cvy, touched ? count_visible(cv, need) : 0);
    }
    {   /* diagnóstico (ca-log): a árvore de layers de uma view que tem um IOSurface embaixo, nas 2 primeiras vezes
         * (o IOSurface do display do Chromium nunca chegava a ser lido no desenho) */
        static int ndump;
        if (cx_ca_diag() && ndump < 2 && qc_tree_has_surface(l, 0)) { ndump++; QLOG("árvore de layers da view %s:", cx_class_name(object_getClass(view))); qc_dump_tree(l, 0); }
    }
    send_canvas();
    rendering--;
}

/* ================================================================ NSView com layer */
static void (*orig_set_wants)(id, SEL, BOOL);
static void (*orig_view_dealloc)(id, SEL);
static id v_make_backing(id self, SEL c) { (void)self; (void)c; return l_layer((id)&CALayer_c, 0); }
static void attach(id view, id layer) {
    vl_t *e = vl_find(view, YES);
    if (e->layer == layer) return;
    if (e->layer) { ls(e->layer)->host_view = 0; objc_release(e->layer); }
    e->layer = layer ? objc_retain(layer) : 0;
    if (layer) {
        lst_t *s = ls(layer);
        s->host_view = view;
        if (!s->delegate) s->delegate = view;
        if (MSG0(BOOL, view, "isFlipped")) s->flipped = YES;
    }
    MSG(void, view, "setNeedsDisplay:", (BOOL)YES);
}
static void v_set_layer(id self, SEL c, id layer) { (void)c; attach(self, layer); }
static id v_layer(id self, SEL c) { (void)c; vl_t *e = vl_find(self, NO); return e ? e->layer : 0; }
static BOOL v_wants(id self, SEL c) { (void)c; vl_t *e = vl_find(self, NO); return e && (e->wants || e->layer); }
static void v_set_wants(id self, SEL c, BOOL b) {
    if (orig_set_wants) orig_set_wants(self, c, b);
    vl_t *e = vl_find(self, b);
    if (!e) return;
    e->wants = b;
    if (b && !e->layer) {
        id l = MSG0(id, self, "makeBackingLayer");
        if (l) attach(self, l);
    }
}
static void v_dealloc(id self, SEL c) {
    for (int i = 0; i < nvl; i++)
        if (vl[i].view == self) {
            if (vl[i].layer) { lst_t *s = ls(vl[i].layer); s->host_view = 0; if (s->delegate == self) s->delegate = 0; objc_release(vl[i].layer); }
            vl[i] = vl[--nvl];
            break;
        }
    if (orig_view_dealloc) orig_view_dealloc(self, c);
    else { IMP sup = cx_lookup(cx_class_named("NSResponder"), c); if (sup) ((void (*)(id, SEL))sup)(self, c); }
}
static i64 v_policy(id self, SEL c) { (void)self; (void)c; return 0; }
static void v_set_policy(id self, SEL c, i64 p) { (void)self; (void)c; (void)p; }

/* IOSurfaceUnlock de escrita (em qualquer thread): marca as views com layer, que o laço do AppKit
 * redesenha no próximo giro (-setNeedsDisplay: só liga uma flag). O -run do appkit.c não passa por
 * cx_runloop_hook, então a marcação é feita aqui mesmo; o gancho do laço fica como reforço. */
void cx_iosurface_changed(id s) {
    (void)s;
    if (rendering) { surf_dirty = 1; return; }
    for (int i = 0; i < MAX_CTX; i++) if (ctxs[i] && ctxs[i]->layer) note_ctx_id(ctxs[i]->cid);
    for (int i = 0; i < nvl; i++) if (vl[i].layer) MSG(void, vl[i].view, "setNeedsDisplay:", (BOOL)YES);
}
static void (*prev_runloop_hook)(double until);
static void qc_runloop_hook(double until) {
    if (surf_dirty) {
        surf_dirty = 0;
        for (int i = 0; i < nvl; i++) if (vl[i].layer) MSG(void, vl[i].view, "setNeedsDisplay:", (BOOL)YES);
    }
    if (prev_runloop_hook) prev_runloop_hook(until);
}

/* ================================================================ registro */
#define LM(sel, fn) cx_add_method(&CALayer_c, sel, (void *)fn)
void cx_iosurface_init(void);
void cx_corevideo_init(void) __attribute__((weak));
extern void (*cx_layer_draw_hook)(id, int, double, double, double, double, double, double, double, double);
void cx_quartzcore_init(void) {
    static int done;
    if (done) return;
    done = 1;
#ifdef QC_SEM_GANCHO
    return;                                     /* só para comparar a saída do AppKit sem o QuartzCore */
#endif
    cx_iosurface_init();
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&CALayer_c, &CALayer_m, "CALayer", ob, 8);
    cx_add_cmethod(&CALayer_c, "layer", (void *)l_layer);
    cx_add_cmethod(&CALayer_c, "defaultActionForKey:", (void *)l_default_action);
    cx_add_cmethod(&CALayer_c, "needsDisplayForKey:", (void *)l_no);
    LM("init", l_init); LM("initWithLayer:", l_init_with); LM("dealloc", l_dealloc);
    LM("bounds", l_bounds); LM("setBounds:", l_set_bounds); LM("position", l_position); LM("setPosition:", l_set_position);
    LM("anchorPoint", l_anchor); LM("setAnchorPoint:", l_set_anchor); LM("zPosition", l_z); LM("setZPosition:", l_set_z);
    LM("frame", l_frame); LM("setFrame:", l_set_frame);
    LM("transform", l_transform); LM("setTransform:", l_set_transform);
    LM("sublayerTransform", l_sub_transform); LM("setSublayerTransform:", l_set_sub_transform);
    LM("affineTransform", l_affine); LM("setAffineTransform:", l_set_affine);
    LM("contents", l_contents); LM("setContents:", l_set_contents);
    LM("contentsScale", l_cscale); LM("setContentsScale:", l_set_cscale);
    LM("contentsRect", l_crect); LM("setContentsRect:", l_set_crect);
    LM("contentsGravity", l_gravity); LM("setContentsGravity:", l_set_gravity);
    LM("opacity", l_opacity); LM("setOpacity:", l_set_opacity);
    LM("isHidden", l_hidden); LM("hidden", l_hidden); LM("setHidden:", l_set_hidden);
    LM("masksToBounds", l_masks); LM("setMasksToBounds:", l_set_masks);
    LM("isOpaque", l_opaque); LM("opaque", l_opaque); LM("setOpaque:", l_set_opaque);
    LM("isGeometryFlipped", l_flipped); LM("geometryFlipped", l_flipped); LM("setGeometryFlipped:", l_set_flipped);
    LM("contentsAreFlipped", l_contents_flipped);
    LM("backgroundColor", l_bg); LM("setBackgroundColor:", l_set_bg);
    LM("borderColor", l_borderc); LM("setBorderColor:", l_set_borderc);
    LM("borderWidth", l_border_w); LM("setBorderWidth:", l_set_border_w);
    LM("cornerRadius", l_radius); LM("setCornerRadius:", l_set_radius);
    LM("shadowRadius", l_shadow_r); LM("setShadowRadius:", l_set_shadow_r);
    LM("shadowOpacity", l_shadow_op); LM("setShadowOpacity:", l_set_shadow_op);
    LM("shadowOffset", l_shadow_off); LM("setShadowOffset:", l_set_shadow_off);
    LM("shadowColor", l_shadow_c); LM("setShadowColor:", l_set_shadow_c);
    LM("delegate", l_delegate); LM("setDelegate:", l_set_delegate);
    LM("name", l_name); LM("setName:", l_set_name); LM("mask", l_mask); LM("setMask:", l_set_mask);
    LM("actions", l_actions); LM("setActions:", l_set_actions); LM("style", l_style); LM("setStyle:", l_set_style);
    LM("filters", l_filters); LM("setFilters:", l_set_filters); LM("setBackgroundFilters:", l_noop_o); LM("setCompositingFilter:", l_noop_o);
    LM("minificationFilter", l_minf); LM("setMinificationFilter:", l_set_minf);
    LM("magnificationFilter", l_magf); LM("setMagnificationFilter:", l_set_magf);
    LM("setContentsFormat:", l_noop_o); LM("setContentsCenter:", l_set_needs_display_rect);
    LM("setEdgeAntialiasingMask:", l_noop_u); LM("setAutoresizingMask:", l_set_autoresize); LM("autoresizingMask", l_autoresize); LM("setMaskedCorners:", l_noop_u);
    LM("setDrawsAsynchronously:", l_noop_b); LM("setAllowsGroupOpacity:", l_noop_b); LM("setAllowsEdgeAntialiasing:", l_noop_b);
    LM("setShouldRasterize:", l_noop_b); LM("setNeedsDisplayOnBoundsChange:", l_noop_b); LM("setDoubleSided:", l_noop_b);
    LM("setWantsExtendedDynamicRangeContent:", l_noop_b); LM("setContentsOpaque:", l_noop_b);
    LM("setRasterizationScale:", l_noop_d); LM("setMinificationFilterBias:", l_noop_d); LM("setLayoutManager:", l_noop_o);
    LM("setContentsMultiplyColor:", l_noop_o); LM("setEDRMetadata:", l_noop_o);
    LM("setNeedsDisplay", l_set_needs_display); LM("setNeedsDisplayInRect:", l_set_needs_display_rect);
    LM("setContentsChanged", l_set_needs_display);   /* privado: o Chromium avisa que o IOSurface mudou no lugar */
    LM("needsDisplay", l_needs_display); LM("display", l_display); LM("displayIfNeeded", l_display_if_needed);
    LM("setNeedsLayout", l_noop); LM("layoutIfNeeded", l_layout_if_needed); LM("layoutSublayers", l_layout_if_needed);
    LM("removeAllAnimations", l_noop); LM("removeAnimationForKey:", l_noop_o); LM("addAnimation:forKey:", l_add_anim);
    LM("animationForKey:", l_nil_o); LM("animationKeys", l_nil); LM("actionForKey:", l_action_for_key);
    LM("presentationLayer", l_presentation); LM("modelLayer", l_presentation);
    LM("superlayer", l_super); LM("sublayers", l_sublayers); LM("setSublayers:", l_set_sublayers);
    LM("addSublayer:", l_add_sub); LM("insertSublayer:atIndex:", l_insert_idx);
    LM("insertSublayer:below:", l_insert_below); LM("insertSublayer:above:", l_insert_above);
    LM("removeFromSuperlayer", l_remove_from_super); LM("replaceSublayer:with:", l_replace);
    LM("convertPoint:toLayer:", l_convert_to); LM("convertPoint:fromLayer:", l_convert_from);
    LM("convertRect:toLayer:", l_convert_rect_to); LM("convertRect:fromLayer:", l_convert_rect_from);

    cx_define_class(&CALayerHost_c, &CALayerHost_m, "CALayerHost", &CALayer_c, 0);
    cx_add_method(&CALayerHost_c, "contextId", (void *)lh_cid);
    cx_add_method(&CALayerHost_c, "setContextId:", (void *)lh_set_cid);
    cx_add_method(&CALayerHost_c, "setInheritsSecurity:", (void *)l_noop_b);
    cx_add_method(&CALayerHost_c, "setPreservesFlip:", (void *)l_noop_b);
    class_addProperty(&CALayerHost_c, "contextId", "TI,N", 4);
    cx_define_class(&CATransformLayer_c, &CATransformLayer_m, "CATransformLayer", &CALayer_c, 0);
    /* sem Metal nem OpenGL no mOS: as classes existem (o Electron tem subclasses), sem drawable */
    cx_define_class(&CAMetalLayer_c, &CAMetalLayer_m, "CAMetalLayer", &CALayer_c, 0);
    cx_add_method(&CAMetalLayer_c, "nextDrawable", (void *)l_nil);
    cx_add_method(&CAMetalLayer_c, "device", (void *)l_nil);
    cx_add_method(&CAMetalLayer_c, "setDevice:", (void *)l_noop_o);
    cx_add_method(&CAMetalLayer_c, "setPixelFormat:", (void *)l_noop_u);
    cx_add_method(&CAMetalLayer_c, "setFramebufferOnly:", (void *)l_noop_b);
    cx_add_method(&CAMetalLayer_c, "setPresentsWithTransaction:", (void *)l_noop_b);
    cx_add_method(&CAMetalLayer_c, "setColorspace:", (void *)l_noop_o);
    cx_define_class(&CAOpenGLLayer_c, &CAOpenGLLayer_m, "CAOpenGLLayer", &CALayer_c, 0);
    cx_define_class(&CAEDRMetadata_c, &CAEDRMetadata_m, "CAEDRMetadata", ob, 0);
    cx_add_cmethod(&CAEDRMetadata_c, "HDR10MetadataWithDisplayInfo:contentInfo:opticalOutputScale:", (void *)l_nil);
    cx_add_cmethod(&CAEDRMetadata_c, "HLGMetadata", (void *)l_nil);

    cx_define_class(&CATransaction_c, &CATransaction_m, "CATransaction", ob, 0);
    cx_add_cmethod(&CATransaction_c, "begin", (void *)tx_begin);
    cx_add_cmethod(&CATransaction_c, "commit", (void *)tx_commit);
    cx_add_cmethod(&CATransaction_c, "flush", (void *)tx_flush);
    cx_add_cmethod(&CATransaction_c, "lock", (void *)tx_lock);
    cx_add_cmethod(&CATransaction_c, "unlock", (void *)tx_lock);
    cx_add_cmethod(&CATransaction_c, "setDisableActions:", (void *)tx_set_disable);
    cx_add_cmethod(&CATransaction_c, "disableActions", (void *)tx_get_disable);
    cx_add_cmethod(&CATransaction_c, "setAnimationDuration:", (void *)tx_set_duration);
    cx_add_cmethod(&CATransaction_c, "animationDuration", (void *)tx_get_duration);
    cx_add_cmethod(&CATransaction_c, "setAnimationTimingFunction:", (void *)l_noop_o);
    cx_add_cmethod(&CATransaction_c, "setCompletionBlock:", (void *)tx_set_completion);
    cx_add_cmethod(&CATransaction_c, "addCommitHandler:forPhase:", (void *)tx_add_commit_handler);
    cx_add_cmethod(&CATransaction_c, "setValue:forKey:", (void *)tx_set_value);
    cx_add_cmethod(&CATransaction_c, "valueForKey:", (void *)tx_value);

    cx_define_class(&CAContext_c, &CAContext_m, "CAContext", ob, sizeof(cactx_t) - 8);
    cx_add_cmethod(&CAContext_c, "contextWithCGSConnection:options:", (void *)ctx_with_conn);
    cx_add_cmethod(&CAContext_c, "remoteContextWithOptions:", (void *)ctx_remote);
    cx_add_cmethod(&CAContext_c, "localContext", (void *)ctx_local);
    cx_add_method(&CAContext_c, "contextId", (void *)ctx_id);
    cx_add_method(&CAContext_c, "layer", (void *)ctx_layer);
    cx_add_method(&CAContext_c, "setLayer:", (void *)ctx_set_layer);
    cx_add_method(&CAContext_c, "invalidate", (void *)ctx_invalidate);
    cx_add_method(&CAContext_c, "dealloc", (void *)ctx_dealloc);
    cx_add_method(&CAContext_c, "colorSpace", (void *)ctx_cs);
    cx_add_method(&CAContext_c, "setColorSpace:", (void *)ctx_set_cs);
    cx_add_method(&CAContext_c, "createFencePort", (void *)ctx_fence);
    cx_add_method(&CAContext_c, "setFencePort:", (void *)ctx_set_fence);
    cx_add_method(&CAContext_c, "setFencePort:commitHandler:", (void *)ctx_set_fence_blk);
    class_addProperty(&CAContext_c, "contextId", "TI,R,N", 6);
    class_addProperty(&CAContext_c, "layer", "T@\"CALayer\",&,N", 15);

    cx_class_t *nv = cx_class_named("NSView");
    if (nv) {
        orig_set_wants = (void (*)(id, SEL, BOOL))class_replaceMethod(nv, sel_registerName("setWantsLayer:"), (void *)v_set_wants, "v@:c");
        orig_view_dealloc = (void (*)(id, SEL))class_replaceMethod(nv, sel_registerName("dealloc"), (void *)v_dealloc, "v@:");
        cx_add_method(nv, "wantsLayer", (void *)v_wants);
        cx_add_method(nv, "layer", (void *)v_layer);
        cx_add_method(nv, "setLayer:", (void *)v_set_layer);
        cx_add_method(nv, "makeBackingLayer", (void *)v_make_backing);
        cx_add_method(nv, "layerContentsRedrawPolicy", (void *)v_policy);
        cx_add_method(nv, "setLayerContentsRedrawPolicy:", (void *)v_set_policy);
        cx_add_method(nv, "setLayerContentsPlacement:", (void *)v_set_policy);
        cx_add_method(nv, "wantsUpdateLayer", (void *)l_no);
        cx_add_method(nv, "setCanDrawSubviewsIntoLayer:", (void *)l_noop_b);
    }
    cx_layer_draw_hook = layer_draw_hook;
    if (cx_runloop_hook != qc_runloop_hook) { prev_runloop_hook = cx_runloop_hook; cx_runloop_hook = qc_runloop_hook; }
    if (cx_corevideo_init) cx_corevideo_init();
}

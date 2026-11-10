/*
 * Host: CGL e NSOpenGL de userland/libsystem/appkit_nsopengl.c, com o runtime do harness do CoreText.
 *   zig cc -g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim \
 *     -Iuserland/libsystem/fsinc -Ikernel/include -Iuserland/libsystem \
 *     -c userland/libsystem/appkit_nsopengl.c -o /tmp/nsgl-lib.o
 *   zig cc -g -O1 -fno-sanitize=all -Iuserland/libsystem -w tools/nsopengl_host.c tools/ctline_host_stubs.c \
 *     /tmp/nsgl-lib.o userland/libsystem/objc_msg.S -lpthread -o /tmp/nsopengl && /tmp/nsopengl
 * O que o arquivo pede e o harness não tem (objetos associados, NSValue, NSView, NSColor, NSRectFill) está aqui.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "objc.h"

id cx_alloc_instance(cx_class_t *cls);
id objc_retain(id o);
void objc_release(id o);
void host_runtime_init(void);
void cx_appkit_nsopengl_init(void);
extern void *cx_msg_lookup(cx_class_t *cls, SEL sel);

int CGLChoosePixelFormat(const int *attrs, void **pix, int *npix);
int CGLDescribePixelFormat(void *pix, int vs, int attrib, int *value);
int CGLCreateContext(void *pix, void *share, void **ctx);
void *CGLGetCurrentContext(void);
int CGLSetCurrentContext(void *ctx);
int CGLLockContext(void *ctx);
int CGLUnlockContext(void *ctx);
int CGLSetParameter(void *ctx, int pname, const int *params);
int CGLGetParameter(void *ctx, int pname, int *params);
unsigned CGLGetContextRetainCount(void *ctx);
void *CGLRetainContext(void *ctx);
void CGLReleaseContext(void *ctx);
void *CGLGetPixelFormat(void *ctx);
int CGLFlushDrawable(void *ctx);
int CGLQueryRendererInfo(unsigned mask, void **rend, int *nrend);
int CGLDescribeRenderer(void *rend, int idx, int prop, int *value);
int CGLDestroyRendererInfo(void *rend);
const char *CGLErrorString(int err);

/* ---- objetos associados (tabela simples, política ignorada: retém se != 0) */
static struct { id o; const void *k; id v; } assoc[256];
void objc_setAssociatedObject(id o, const void *k, id v, unsigned long long pol) {
    for (int i = 0; i < 256; i++) if (assoc[i].o == o && assoc[i].k == k) { assoc[i].v = pol ? (v ? objc_retain(v) : 0) : v; if (!v) assoc[i].o = 0; return; }
    if (!v) return;
    for (int i = 0; i < 256; i++) if (!assoc[i].o) { assoc[i].o = o; assoc[i].k = k; assoc[i].v = pol ? objc_retain(v) : v; return; }
}
id objc_getAssociatedObject(id o, const void *k) {
    for (int i = 0; i < 256; i++) if (assoc[i].o == o && assoc[i].k == k) return assoc[i].v;
    return 0;
}
IMP class_getMethodImplementation(cx_class_t *c, SEL s) { return (IMP)cx_msg_lookup(c, s); }

/* ---- classes mínimas */
typedef struct { double x, y, w, h; } rect_t;
typedef struct { double w, h; } size_t2;
static cx_class_t Val_c, Val_m, View_c, View_m, Col_c, Col_m;
typedef struct { cx_class_t *isa; void *p; } val_t;
static id val_with(id cls, SEL c, void *p) { (void)c; val_t *v = (val_t *)cx_alloc_instance((cx_class_t *)cls); v->p = p; return (id)v; }
static void *val_ptr(id self, SEL c) { (void)c; return ((val_t *)self)->p; }
typedef struct { cx_class_t *isa; rect_t f; int needs; } view_t;
static id view_init(id self, SEL c, rect_t f) { (void)c; ((view_t *)self)->f = f; return self; }
static void view_set_size(id self, SEL c, size_t2 z) { (void)c; ((view_t *)self)->f.w = z.w; ((view_t *)self)->f.h = z.h; }
static rect_t view_bounds(id self, SEL c) { (void)c; rect_t r = {0, 0, ((view_t *)self)->f.w, ((view_t *)self)->f.h}; return r; }
static void view_needs(id self, SEL c, BOOL b) { (void)c; ((view_t *)self)->needs += b ? 1 : 0; }
static id col_white(id cls, SEL c, double w, double a) { (void)c; (void)w; (void)a; return cx_alloc_instance((cx_class_t *)cls); }
static void col_fill(id s, SEL c) { (void)s; (void)c; }
static int fills;
void NSRectFill(rect_t r) { (void)r; fills++; }

/* subclasse do "app": conta prepareOpenGL e reshape */
static cx_class_t AppGL_c, AppGL_m;
static int prepared, reshaped;
static void app_prepare(id s, SEL c) { (void)s; (void)c; prepared++; }
static void app_reshape(id s, SEL c) { (void)s; (void)c; reshaped++; }

static int fails;
static void expect(const char *name, int ok) { if (!ok) { printf("FALHOU %s\n", name); fails++; } else printf("ok %s\n", name); }

static void *other_thread(void *ctx) {
    long r = CGLSetCurrentContext(0);
    (void)r;
    return CGLGetCurrentContext() == 0 && ctx ? (void *)1 : 0;   /* current é por thread */
}

int main(void) {
    setvbuf(stdout, 0, _IONBF, 0);
    host_runtime_init();
    cx_define_class(&Val_c, &Val_m, "NSValue", cx_class_named("NSObject"), 8);
    cx_add_cmethod(&Val_c, "valueWithPointer:", (void *)val_with);
    cx_add_method(&Val_c, "pointerValue", (void *)val_ptr);
    cx_define_class(&View_c, &View_m, "NSView", cx_class_named("NSObject"), 40);
    cx_add_method(&View_c, "initWithFrame:", (void *)view_init);
    cx_add_method(&View_c, "setFrameSize:", (void *)view_set_size);
    cx_add_method(&View_c, "bounds", (void *)view_bounds);
    cx_add_method(&View_c, "setNeedsDisplay:", (void *)view_needs);
    cx_add_method(&View_c, "dealloc", (void *)col_fill);
    cx_define_class(&Col_c, &Col_m, "NSColor", cx_class_named("NSObject"), 8);
    cx_add_cmethod(&Col_c, "colorWithCalibratedWhite:alpha:", (void *)col_white);
    cx_add_method(&Col_c, "setFill", (void *)col_fill);
    cx_appkit_nsopengl_init();

    /* ---- CGL */
    {
        void *pix = 0;
        int n = 7, v = 0;
        expect("CGLChoosePixelFormat continua recusando", CGLChoosePixelFormat(0, &pix, &n) == 10002 && !pix && n == 0);
    }
    static const unsigned attrs[] = {5 /* DoubleBuffer */, 12 /* Depth */, 24, 13 /* Stencil */, 8, 99 /* Profile */, 0x3200, 0};
    id pf = MSG(id, MSG0(id, (id)cx_class_named("NSOpenGLPixelFormat"), "alloc"), "initWithAttributes:", attrs);
    void *pix = pf ? MSG0(void *, pf, "CGLPixelFormatObj") : 0;
    int v = -1;
    expect("pixel format válido", pf && pix);
    expect("profundidade pedida", CGLDescribePixelFormat(pix, 0, 12, &v) == 0 && v == 24);
    MSG(void, pf, "getValues:forAttribute:forVirtualScreen:", &v, (unsigned)99, (int)0);
    expect("getValues: perfil 3.2 core", v == 0x3200);
    MSG(void, pf, "getValues:forAttribute:forVirtualScreen:", &v, (unsigned)5, (int)0);
    expect("getValues: double buffer", v == 1);
    expect("uma tela virtual", MSG0(i64, pf, "numberOfVirtualScreens") == 1);
    expect("tela virtual inexistente recusada", CGLDescribePixelFormat(pix, 3, 12, &v) == 10008);

    void *ctx = 0, *bad = 0;
    expect("CGLCreateContext", CGLCreateContext(pix, 0, &ctx) == 0 && ctx);
    expect("CGLCreateContext sem pixel format", CGLCreateContext(0, 0, &bad) == 10002 && !bad);
    expect("pixel format do contexto", CGLGetPixelFormat(ctx) == pix);
    expect("vira current", CGLSetCurrentContext(ctx) == 0 && CGLGetCurrentContext() == ctx);
    {
        pthread_t t;
        void *r = 0;
        pthread_create(&t, 0, other_thread, ctx);
        pthread_join(t, &r);
        expect("current por thread", r == (void *)1 && CGLGetCurrentContext() == ctx);
    }
    expect("trava recursiva", CGLLockContext(ctx) == 0 && CGLLockContext(ctx) == 0 && CGLUnlockContext(ctx) == 0 && CGLUnlockContext(ctx) == 0);
    expect("destravar sem travar é erro", CGLUnlockContext(ctx) == 10016);
    {
        int one = 1, got = 0;
        expect("swap interval guardado", CGLSetParameter(ctx, 222, &one) == 0 && CGLGetParameter(ctx, 222, &got) == 0 && got == 1);
        expect("renderer é só leitura", CGLSetParameter(ctx, 309, &one) == 10008);
    }
    expect("retain/release", CGLRetainContext(ctx) == ctx && CGLGetContextRetainCount(ctx) == 2);
    CGLReleaseContext(ctx);
    CGLReleaseContext(ctx);
    expect("solto deixa de ser current", CGLGetCurrentContext() == 0);
    {
        void *ri = 0;
        int nr = 0, id_ = 0;
        expect("renderer de software", CGLQueryRendererInfo(1, &ri, &nr) == 0 && nr == 1 &&
               CGLDescribeRenderer(ri, 0, 70, &id_) == 0 && id_ == 0x00020400 && CGLDestroyRendererInfo(ri) == 0);
    }
    expect("CGLErrorString", !strcmp(CGLErrorString(10004), "invalid context"));

    /* ---- NSOpenGLContext */
    id gc = MSG(id, MSG0(id, (id)cx_class_named("NSOpenGLContext"), "alloc"), "initWithFormat:shareContext:", pf, (id)0);
    void *cgl = gc ? MSG0(void *, gc, "CGLContextObj") : 0;
    expect("NSOpenGLContext com CGLContextObj", gc && cgl);
    MSG0(void, gc, "makeCurrentContext");
    expect("makeCurrentContext / currentContext", MSG0(id, (id)cx_class_named("NSOpenGLContext"), "currentContext") == gc && CGLGetCurrentContext() == cgl);
    expect("pixelFormat do contexto", MSG0(id, gc, "pixelFormat") == pf);
    {
        int sw = 1, got = 0;
        MSG(void, gc, "setValues:forParameter:", &sw, (i64)222);
        MSG(void, gc, "getValues:forParameter:", &got, (i64)222);
        expect("setValues:forParameter:", got == 1);
    }
    MSG0(void, (id)cx_class_named("NSOpenGLContext"), "clearCurrentContext");
    expect("clearCurrentContext", CGLGetCurrentContext() == 0);
    id nilctx = MSG(id, MSG0(id, (id)cx_class_named("NSOpenGLContext"), "alloc"), "initWithFormat:shareContext:", (id)0, (id)0);
    expect("sem pixel format: nil", nilctx == 0);

    /* ---- NSOpenGLView */
    cx_define_class(&AppGL_c, &AppGL_m, "AppGLView", cx_class_named("NSOpenGLView"), 0);
    cx_add_method(&AppGL_c, "prepareOpenGL", (void *)app_prepare);
    cx_add_method(&AppGL_c, "reshape", (void *)app_reshape);
    rect_t fr = {0, 0, 320, 200};
    id view = MSG(id, cx_alloc_instance(&AppGL_c), "initWithFrame:", fr);
    id vc = view ? MSG0(id, view, "openGLContext") : 0;
    expect("view cria o contexto com o formato padrão", vc && MSG0(id, view, "pixelFormat") && MSG0(id, vc, "view") == view);
    expect("contexto da view é o mesmo", MSG0(id, view, "openGLContext") == vc);
    size_t2 z = {400, 300};
    MSG(void, view, "setFrameSize:", z);
    expect("prepareOpenGL uma vez e reshape", prepared == 1 && reshaped == 1);
    MSG(void, view, "drawRect:", fr);
    expect("drawRect: não repete prepareOpenGL e pinta", prepared == 1 && fills == 1);
    int before = ((view_t *)view)->needs;
    MSG0(void, vc, "flushBuffer");
    expect("flushBuffer pede redesenho da view", ((view_t *)view)->needs == before + 1);

    printf("nsopengl: %s (%d falhas)\n", fails ? "FALHOU" : "ok", fails);
    return fails ? 1 : 0;
}

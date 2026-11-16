/*
 * Host: CGL e NSOpenGL de userland/libsystem/appkit_nsopengl.c, com o runtime do harness do CoreText.
 *   zig cc -g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim \
 *     -Iuserland/libsystem/fsinc -Ikernel/include -Iuserland/libsystem \
 *     -c userland/libsystem/appkit_nsopengl.c -o /tmp/nsgl-lib.o
 *   for f in gl_ctx gl_buf gl_tex gl_fbo gl_raster gl_ffp gl_misc gl_eval gl_arbprog gl_debug gl_glsl gl_glsl_tr gl_glsl_pre corevideo mtltex mtlshader mtlexec mslspirv; do
 *     zig cc -g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \
 *       -Ikernel/include -Iuserland/libsystem -c userland/libsystem/$f.c -o /tmp/nsgl-$f.o; done
 *   zig cc -g -O1 -fno-sanitize=all -Iuserland/libsystem -w tools/nsopengl_host.c tools/ctline_host_stubs.c \
 *     /tmp/nsgl-*.o userland/libsystem/objc_msg.S -lpthread -lm -o /tmp/nsopengl && /tmp/nsopengl
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

/* IOSurface, NSNumber, NSDictionary e CALayer mínimos: o flushBuffer copia o quadro para o contents do layer */
typedef struct { cx_class_t *isa; long v; } num_t;
static cx_class_t Num_c, Num_m, Dict_c, Dict_m, Surf_c, Surf_m, Layer_c, Layer_m;
id cx_num_i(long long v) { num_t *n = (num_t *)cx_alloc_instance(&Num_c); n->v = (long)v; return (id)n; }
typedef struct { cx_class_t *isa; long w, h; } dict_t;
static id dict_make(id cls, SEL c, id *vals, id *keys, unsigned long long n) { (void)c; (void)keys; (void)n; dict_t *d = (dict_t *)cx_alloc_instance((cx_class_t *)cls); d->w = ((num_t *)vals[0])->v; d->h = ((num_t *)vals[1])->v; return (id)d; }
const void *kIOSurfaceWidth = "w", *kIOSurfaceHeight = "h", *kIOSurfaceBytesPerElement = "b", *kIOSurfacePixelFormat = "f";
typedef struct { cx_class_t *isa; long w, h; unsigned char *px; } surf_t;
void *IOSurfaceCreate(id props) { dict_t *d = (dict_t *)props; surf_t *s = (surf_t *)cx_alloc_instance(&Surf_c); s->w = d->w; s->h = d->h; s->px = calloc(1, (size_t)(d->w * d->h * 4)); return s; }
void *IOSurfaceGetBaseAddress(void *s) { return ((surf_t *)s)->px; }
void *IOSurfaceGetBaseAddressOfPlane(void *s, unsigned long long i) { (void)i; return ((surf_t *)s)->px; }
unsigned long long IOSurfaceGetBytesPerRow(void *s) { return (unsigned long long)((surf_t *)s)->w * 4; }
unsigned long long IOSurfaceGetBytesPerRowOfPlane(void *s, unsigned long long i) { (void)i; return IOSurfaceGetBytesPerRow(s); }
unsigned long long IOSurfaceGetWidth(void *s) { return (unsigned long long)((surf_t *)s)->w; }
unsigned long long IOSurfaceGetHeight(void *s) { return (unsigned long long)((surf_t *)s)->h; }
unsigned long long IOSurfaceGetAllocSize(void *s) { return IOSurfaceGetBytesPerRow(s) * IOSurfaceGetHeight(s); }
unsigned long long IOSurfaceGetWidthOfPlane(void *s, unsigned long long i) { (void)i; return IOSurfaceGetWidth(s); }
unsigned long long IOSurfaceGetHeightOfPlane(void *s, unsigned long long i) { (void)i; return IOSurfaceGetHeight(s); }
unsigned long long IOSurfaceGetPlaneCount(void *s) { (void)s; return 0; }
unsigned IOSurfaceGetPixelFormat(void *s) { (void)s; return 0x42475241; }
int IOSurfaceLock(void *s, unsigned o, unsigned *seed) { (void)s; (void)o; (void)seed; return 0; }
int IOSurfaceUnlock(void *s, unsigned o, unsigned *seed) { (void)s; (void)o; (void)seed; return 0; }
unsigned _dyld_image_count(void) { return 0; }
int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
void cx_metal_init(void) {}
/* o que o corevideo.c usa do IOSurface/CF e o harness não tem */
char cx_cfstr_isa[64] __asm__("__CFConstantStringClassReference");
const void *kIOSurfacePlaneElementHeight, *kIOSurfacePlaneBytesPerRow, *kIOSurfacePlaneOffset, *kIOSurfacePlaneSize,
    *kIOSurfacePlaneBytesPerElement, *kIOSurfacePlaneElementWidth, *kIOSurfacePlaneHeight, *kIOSurfacePlaneInfo, *kIOSurfacePlaneWidth;
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
void *CGColorSpaceCreateDeviceRGB(void) { return 0; }
int cx_ca_diag(void) { return 0; }
IMP cx_lookup(cx_class_t *c, SEL sel) { return (IMP)cx_msg_lookup(c, sel); }
unsigned long long mach_absolute_time(void) { return 0; }
const char *_dyld_get_image_name(unsigned i) { (void)i; return 0; }
typedef struct { cx_class_t *isa; id contents; } layer_t;
static void layer_set(id self, SEL c, id v) { (void)c; ((layer_t *)self)->contents = v; }
static void layer_opaque(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; }
static id the_layer;
static BOOL view_wants(id s, SEL c) { (void)s; (void)c; return the_layer != 0; }
static void view_set_wants(id s, SEL c, BOOL b) { (void)s; (void)c; if (b && !the_layer) the_layer = cx_alloc_instance(&Layer_c); }
static id view_layer(id s, SEL c) { (void)s; (void)c; return the_layer; }
static id view_window(id s, SEL c) { (void)s; (void)c; return 0; }
void glClearColor(float r, float g, float b, float a);
int CGLSetOffScreen(void *ctx, int w, int h, int rowbytes, void *base);
int CGLFlushDrawable(void *ctx);
void glClear(unsigned m);
const unsigned char *glGetString(unsigned n);

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
    cx_add_method(&View_c, "wantsLayer", (void *)view_wants);
    cx_add_method(&View_c, "setWantsLayer:", (void *)view_set_wants);
    cx_add_method(&View_c, "layer", (void *)view_layer);
    cx_add_method(&View_c, "window", (void *)view_window);
    cx_define_class(&Num_c, &Num_m, "NSNumber", cx_class_named("NSObject"), 8);
    cx_define_class(&Dict_c, &Dict_m, "NSDictionary", cx_class_named("NSObject"), 16);
    cx_add_cmethod(&Dict_c, "dictionaryWithObjects:forKeys:count:", (void *)dict_make);
    cx_define_class(&Surf_c, &Surf_m, "IOSurfaceHost", cx_class_named("NSObject"), 24);
    cx_define_class(&Layer_c, &Layer_m, "CALayerHost", cx_class_named("NSObject"), 8);
    cx_add_method(&Layer_c, "setContents:", (void *)layer_set);
    cx_add_method(&Layer_c, "setOpaque:", (void *)layer_opaque);
    cx_define_class(&Col_c, &Col_m, "NSColor", cx_class_named("NSObject"), 8);
    cx_add_cmethod(&Col_c, "colorWithCalibratedWhite:alpha:", (void *)col_white);
    cx_add_method(&Col_c, "setFill", (void *)col_fill);
    cx_appkit_nsopengl_init();

    /* ---- CGL */
    {
        void *pix = 0;
        int n = 7, v = 0;
        static const int a[] = {5, 12, 24, 0};
        expect("CGLChoosePixelFormat aceita fora do Chromium", CGLChoosePixelFormat(a, &pix, &n) == 0 && pix && n == 1);
        expect("profundidade do formato escolhido", CGLDescribePixelFormat(pix, 0, 12, &v) == 0 && v == 24);
        static const int bad[] = {99, 0x2100, 0};
        void *p2 = 0;
        expect("perfil inexistente recusado", CGLChoosePixelFormat(bad, &p2, &n) == 10002 && !p2);
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
    MSG0(void, vc, "makeCurrentContext");
    expect("contexto do perfil legado é GL 2.1", glGetString(0x1F02) && !strncmp((const char *)glGetString(0x1F02), "2.1", 3));
    glClearColor(0, 0, 1, 1);
    glClear(0x4000);
    MSG0(void, vc, "flushBuffer");
    layer_t *L = (layer_t *)the_layer;
    surf_t *S = L ? (surf_t *)L->contents : 0;
    expect("flushBuffer põe o quadro no layer da view", S && S->w == 400 && S->h == 300);
    expect("pixels BGRA do clear azul", S && S->px[0] == 255 && S->px[1] == 0 && S->px[2] == 0 && S->px[3] == 255);
    fills = 0;
    MSG(void, view, "drawRect:", fr);
    expect("depois do primeiro quadro o drawRect: não pinta por cima", fills == 0);
    /* contexto 3.2 core */
    void *cctx = 0;
    expect("contexto core", CGLCreateContext(pix, 0, &cctx) == 0 && cctx);
    CGLSetCurrentContext(cctx);
    expect("perfil core é GL 4.1", glGetString(0x1F02) && !strncmp((const char *)glGetString(0x1F02), "4.1", 3));
    {
        unsigned char buf[16 * 8 * 4];
        expect("CGLSetOffScreen", CGLSetOffScreen(cctx, 16, 8, 64, buf) == 0);
        glClearColor(1, 0, 0, 1);
        glClear(0x4000);
        CGLFlushDrawable(cctx);
        expect("offscreen recebe o quadro (BGRA)", buf[2] == 255 && buf[0] == 0 && buf[3] == 255);
    }
    /* opções globais do NSOpenGL/CGL */
    {
        extern void NSOpenGLSetOption(int, int), NSOpenGLGetOption(int, int *), NSOpenGLGetVersion(int *, int *);
        int v = -1, ma = 0, mi = 0;
        NSOpenGLGetOption(501, &v);
        expect("NSOpenGLGOFormatCacheSize padrão 5", v == 5);
        NSOpenGLSetOption(503, 1);
        NSOpenGLGetOption(503, &v);
        expect("NSOpenGLGORetainRenderers guardado", v == 1);
        NSOpenGLGetOption(77, &v);
        expect("opção inválida devolve 0", v == 0);
        NSOpenGLGetVersion(&ma, &mi);
        expect("NSOpenGLGetVersion", ma == 1 && mi >= 0);
    }
    /* CVOpenGLTextureCache: 2vuy (Cb Y0 Cr Y1) e 420v viram textura retangular BGRA */
    {
        extern cx_class_t CVPixelBuffer_c __asm__("OBJC_CLASS_$_CVPixelBuffer");
        extern cx_class_t CVPixelBuffer_m __asm__("OBJC_METACLASS_$_CVPixelBuffer");
        extern void cx_corevideo_gl_init(void);
        if (!CVPixelBuffer_c.vtable) cx_define_class(&CVPixelBuffer_c, &CVPixelBuffer_m, "CVPixelBuffer", cx_class_named("NSObject"), 160);
        cx_corevideo_gl_init();
        extern int CVPixelBufferCreateWithBytes(void *, unsigned long long, unsigned long long, unsigned, void *, unsigned long long, void *, void *, id, void **);
        extern int CVPixelBufferCreateWithPlanarBytes(void *, unsigned long long, unsigned long long, unsigned, void *, unsigned long long, unsigned long long, void **, unsigned long long *, unsigned long long *, unsigned long long *, void *, void *, id, void **);
        extern int CVOpenGLTextureCacheCreate(void *, id, void *, void *, id, void **);
        extern int CVOpenGLTextureCacheCreateTextureFromImage(void *, void *, void *, id, void **);
        extern unsigned CVOpenGLTextureGetName(void *), CVOpenGLTextureGetTarget(void *);
        void glBindTexture(unsigned t, unsigned n);
        void glGetTexImage(unsigned t, int l, unsigned f, unsigned ty, void *p);
        /* 2 pixels: Y=235 (branco) e Y=16 (preto) com croma neutro */
        static unsigned char yuv[4] = {128, 235, 128, 16};
        void *pb = 0, *cache = 0, *tex = 0;
        expect("CVPixelBufferCreateWithBytes 2vuy", CVPixelBufferCreateWithBytes(0, 2, 1, 0x32767579, yuv, 4, 0, 0, 0, &pb) == 0 && pb);
        expect("CVOpenGLTextureCacheCreate", CVOpenGLTextureCacheCreate(0, 0, cctx, pix, 0, &cache) == 0 && cache);
        expect("textura a partir do buffer", CVOpenGLTextureCacheCreateTextureFromImage(0, cache, pb, 0, &tex) == 0 && tex);
        unsigned name = CVOpenGLTextureGetName(tex);
        expect("textura retangular com nome", name != 0 && CVOpenGLTextureGetTarget(tex) == 0x84F5);
        unsigned char back[8] = {0};
        CGLSetCurrentContext(cctx);
        glBindTexture(0x84F5, name);
        glGetTexImage(0x84F5, 0, 0x80E1, 0x1401, back);
        expect("2vuy -> BGRA branco e preto", back[0] >= 250 && back[1] >= 250 && back[2] >= 250 && back[4] <= 3 && back[5] <= 3);
        /* 420v 2x2: Y de 128 e croma Cr alto (vermelho) */
        static unsigned char Y[4] = {128, 128, 128, 128}, CbCr[2] = {128, 240};
        void *planes[2] = {Y, CbCr};
        unsigned long long pw[2] = {2, 1}, ph[2] = {2, 1}, pr[2] = {2, 2};
        void *pb2 = 0, *tex2 = 0;
        expect("CVPixelBufferCreateWithPlanarBytes", CVPixelBufferCreateWithPlanarBytes(0, 2, 2, 0x34323076, 0, 0, 2, planes, pw, ph, pr, 0, 0, 0, &pb2) == 0 && pb2);
        expect("textura do 420v", CVOpenGLTextureCacheCreateTextureFromImage(0, cache, pb2, 0, &tex2) == 0 && tex2);
        CGLSetCurrentContext(cctx);
        glBindTexture(0x84F5, CVOpenGLTextureGetName(tex2));
        unsigned char b2[16] = {0};
        glGetTexImage(0x84F5, 0, 0x80E1, 0x1401, b2);
        expect("420v -> R alto, G baixo, B neutro", b2[2] > 200 && b2[1] < 60 && b2[0] > 110 && b2[0] < 150 && b2[12 + 2] > 200);
    }
    CGLSetCurrentContext(0);
    CGLReleaseContext(cctx);

    printf("nsopengl: %s (%d falhas)\n", fails ? "FALHOU" : "ok", fails);
    return fails ? 1 : 0;
}

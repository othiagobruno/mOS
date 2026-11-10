/*
 * mOS libSystem — NSOpenGL (NSOpenGLPixelFormat, NSOpenGLContext, NSOpenGLView) e a CGL que esses apps importam.
 * O mOS não tem OpenGL (nem por software: só Metal, em metal.c). Aqui o estado é real e coerente para o app
 * subir: o pixel format guarda os atributos pedidos e responde getValues/CGLDescribePixelFormat com eles; o
 * contexto conta referências, vira o current da thread (chave pthread), tranca e destranca de verdade, guarda os
 * parâmetros (swap interval, opacidade...) e a view. flushBuffer/CGLFlushDrawable não copiam pixel nenhum (não há
 * framebuffer de GL): só marcam a view para redesenho. A NSOpenGLView sem drawRect: do app pinta preto, como uma
 * superfície GL limpa.
 * CGLChoosePixelFormat continua recusando (kCGLBadPixelFormat): é por ela que o helper de GPU do Chromium desiste
 * do OpenGL e cai para o SwiftShader (metal_gate.c). O NSOpenGLPixelFormat monta o próprio CGLPixelFormatObj sem
 * passar por ela, e CGLCreateContext aceita esse objeto.
 * Registro: cx_appkit_nsopengl_init(), chamado no cx_appkit_input_init (appkit_input.c), junto do Postico.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
int pthread_key_create(unsigned long *k, void (*dtor)(void *));
void *pthread_getspecific(unsigned long k);
int pthread_setspecific(unsigned long k, const void *v);
void *pthread_self(void);

typedef struct { double x, y, w, h; } gl_rect_t;
void objc_msgSend_stret(void);   /* NSRect (32 bytes) volta por ponteiro oculto: MSG0 leria o isa do buffer */
#define MSGR(T, obj, sel) ((T (*)(id, SEL))objc_msgSend_stret)((id)(obj), sel_registerName(sel))
typedef struct { double w, h; } gl_size_t;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSOpenGLPixelFormat) K(NSOpenGLContext) K(NSOpenGLView)

/* ---- erros e atributos da CGL (CGLTypes.h); NSOpenGLPFA* tem os mesmos valores */
enum {
    kCGLNoError = 0, kCGLBadAttribute = 10000, kCGLBadProperty = 10001, kCGLBadPixelFormat = 10002,
    kCGLBadContext = 10004, kCGLBadValue = 10008, kCGLBadMatch = 10009, kCGLBadEnumeration = 10013,
    kCGLBadAddress = 10014, kCGLBadRendererInfo = 10012, kCGLBadState = 10016,
};
enum {
    PFA_AllRenderers = 1, PFA_TripleBuffer = 3, PFA_DoubleBuffer = 5, PFA_Stereo = 6, PFA_AuxBuffers = 7,
    PFA_ColorSize = 8, PFA_AlphaSize = 11, PFA_DepthSize = 12, PFA_StencilSize = 13, PFA_AccumSize = 14,
    PFA_MinimumPolicy = 51, PFA_MaximumPolicy = 52, PFA_OffScreen = 53, PFA_FullScreen = 54, PFA_SampleBuffers = 55,
    PFA_Samples = 56, PFA_AuxDepthStencil = 57, PFA_ColorFloat = 58, PFA_Multisample = 59, PFA_Supersample = 60,
    PFA_SampleAlpha = 61, PFA_RendererID = 70, PFA_SingleRenderer = 71, PFA_NoRecovery = 72, PFA_Accelerated = 73,
    PFA_ClosestPolicy = 74, PFA_Robust = 75, PFA_BackingStore = 76, PFA_MPSafe = 78, PFA_Window = 80,
    PFA_MultiScreen = 81, PFA_Compliant = 83, PFA_ScreenMask = 84, PFA_PixelBuffer = 90, PFA_RemotePixelBuffer = 91,
    PFA_AllowOfflineRenderers = 96, PFA_AcceleratedCompute = 97, PFA_OpenGLProfile = 99, PFA_VirtualScreenCount = 128,
    PFA_SupportsAutomaticGraphicsSwitching = 101,
};
#define GL_RENDERER_SOFT 0x00020400u          /* kCGLRendererGenericFloatID: o renderizador genérico da Apple */
#define GL_PROFILE_LEGACY 0x1000

/* atributo com valor depois dele na lista */
static int pfa_has_value(u32 a) {
    switch (a) {
    case PFA_AuxBuffers: case PFA_ColorSize: case PFA_AlphaSize: case PFA_DepthSize: case PFA_StencilSize:
    case PFA_AccumSize: case PFA_SampleBuffers: case PFA_Samples: case PFA_RendererID: case PFA_ScreenMask:
    case PFA_OpenGLProfile: case PFA_VirtualScreenCount:
        return 1;
    default:
        return 0;
    }
}

/* ================================================================ CGLPixelFormatObj */
#define PIX_MAGIC 0x43474C50u   /* 'CGLP' */
#define PIX_NATTR 160
typedef struct {
    u32 magic;
    int refs;
    int set[PIX_NATTR];          /* 1 = o app pediu o atributo */
    int val[PIX_NATTR];          /* valor (ou 1 para os booleanos pedidos) */
} cgl_pix_t;
static cgl_pix_t *pix_of(void *p) { cgl_pix_t *x = p; return x && x->magic == PIX_MAGIC ? x : 0; }
/* atributos terminados em 0, no formato de NSOpenGLPixelFormatAttribute/CGLPixelFormatAttribute (u32) */
static cgl_pix_t *pix_new(const u32 *attrs) {
    cgl_pix_t *p = calloc(1, sizeof *p);
    if (!p) return 0;
    p->magic = PIX_MAGIC;
    p->refs = 1;
    for (int i = 0; attrs && attrs[i] && i < 256; i++) {
        u32 a = attrs[i];
        int v = 1;
        if (pfa_has_value(a)) v = (int)attrs[++i];
        if (a < PIX_NATTR) { p->set[a] = 1; p->val[a] = v; }
        if (pfa_has_value(a) && !attrs[i]) break;   /* lista terminou no lugar do valor */
    }
    return p;
}
/* o que o renderizador de software tem: cor 32 (24 + alfa 8), profundidade e stencil como pedidos, 1 tela virtual */
static int pix_value(const cgl_pix_t *p, u32 a) {
    int asked = a < PIX_NATTR && p->set[a], v = asked ? p->val[a] : 0;
    switch (a) {
    case PFA_ColorSize: return asked && v > 0 ? (v > 32 ? 64 : 32) : 32;
    case PFA_AlphaSize: return asked ? (v > 8 ? 16 : 8) : 8;
    case PFA_DepthSize: return asked ? (v > 24 ? 32 : v > 16 ? 24 : v) : 0;
    case PFA_StencilSize: return asked ? (v > 0 ? 8 : 0) : 0;
    case PFA_Samples: case PFA_SampleBuffers: case PFA_AuxBuffers: case PFA_AccumSize: return asked ? v : 0;
    case PFA_RendererID: return GL_RENDERER_SOFT;
    case PFA_ScreenMask: return asked ? v : 1;
    case PFA_OpenGLProfile: return asked ? v : GL_PROFILE_LEGACY;
    case PFA_VirtualScreenCount: return 1;
    case PFA_Accelerated: case PFA_AcceleratedCompute: return 0;   /* software */
    case PFA_Window: case PFA_MPSafe: case PFA_Compliant: case PFA_SingleRenderer: case PFA_NoRecovery: return 1;
    default: return asked ? v : 0;
    }
}

int CGLChoosePixelFormat(const int *attrs, void **pix, int *npix) {
    (void)attrs;
    if (pix) *pix = 0;
    if (npix) *npix = 0;
    return kCGLBadPixelFormat;   /* sem OpenGL: o Chromium vai para o SwiftShader (ver o cabeçalho) */
}
int CGLDescribePixelFormat(void *pix, int vs, int attrib, int *value) {
    cgl_pix_t *p = pix_of(pix);
    if (!p) return kCGLBadPixelFormat;
    if (!value) return kCGLBadAddress;
    if (vs != 0) return kCGLBadValue;
    *value = pix_value(p, (u32)attrib);
    return kCGLNoError;
}
void *CGLRetainPixelFormat(void *pix) { cgl_pix_t *p = pix_of(pix); if (p) __atomic_add_fetch(&p->refs, 1, __ATOMIC_ACQ_REL); return pix; }
void CGLReleasePixelFormat(void *pix) {
    cgl_pix_t *p = pix_of(pix);
    if (p && __atomic_sub_fetch(&p->refs, 1, __ATOMIC_ACQ_REL) == 0) { p->magic = 0; free(p); }
}
int CGLDestroyPixelFormat(void *pix) { if (!pix_of(pix)) return kCGLBadPixelFormat; CGLReleasePixelFormat(pix); return kCGLNoError; }
unsigned CGLGetPixelFormatRetainCount(void *pix) { cgl_pix_t *p = pix_of(pix); return p ? (unsigned)p->refs : 0; }

/* ================================================================ CGLContextObj */
#define CTX_MAGIC 0x43474C43u   /* 'CGLC' */
#define CTX_NPARAM 512
typedef struct cgl_ctx {
    u32 magic;
    int refs;
    cgl_pix_t *pix;
    struct cgl_ctx *share;
    id view;                     /* NSView (fraca, como no Cocoa) */
    id owner;                    /* NSOpenGLContext que embrulha (fraco) */
    int params[CTX_NPARAM];      /* CGLContextParameter: só o primeiro valor de cada um */
    int enabled[CTX_NPARAM];     /* CGLContextEnable */
    int vscreen;
    void *lock_owner;
    int lock_depth;
    unsigned long flushes;
} cgl_ctx_t;
enum { kCGLCPSwapRectangle = 200, kCGLCPSwapInterval = 222, kCGLCPSurfaceOrder = 235, kCGLCPSurfaceOpacity = 236,
       kCGLCPSurfaceBackingSize = 304, kCGLCPSurfaceSurfaceVolatile = 306, kCGLCPHasDrawable = 314,
       kCGLCPCurrentRendererID = 309, kCGLCPGPUVertexProcessing = 310, kCGLCPGPUFragmentProcessing = 311 };
static cgl_ctx_t *ctx_of(void *c) { cgl_ctx_t *x = c; return x && x->magic == CTX_MAGIC ? x : 0; }

static unsigned long g_cur_key;
static int g_cur_ready;
static void cur_init(void) {
    if (__atomic_load_n(&g_cur_ready, __ATOMIC_ACQUIRE)) return;
    unsigned long k = 0;
    if (pthread_key_create(&k, 0) == 0) { g_cur_key = k; __atomic_store_n(&g_cur_ready, 1, __ATOMIC_RELEASE); }
}

int CGLCreateContext(void *pix, void *share, void **ctx) {
    cgl_pix_t *p = pix_of(pix);
    if (!ctx) return kCGLBadAddress;
    *ctx = 0;
    if (!p) return kCGLBadPixelFormat;
    if (share && !ctx_of(share)) return kCGLBadContext;
    cgl_ctx_t *c = calloc(1, sizeof *c);
    if (!c) return kCGLBadState;
    c->magic = CTX_MAGIC;
    c->refs = 1;
    c->pix = CGLRetainPixelFormat(p);
    c->share = share;
    c->params[kCGLCPSwapInterval] = 0;
    c->params[kCGLCPSurfaceOpacity] = 1;
    c->params[kCGLCPSurfaceOrder] = 1;
    c->params[kCGLCPCurrentRendererID] = (int)GL_RENDERER_SOFT;
    *ctx = c;
    return kCGLNoError;
}
void *CGLGetCurrentContext(void) { cur_init(); return g_cur_ready ? pthread_getspecific(g_cur_key) : 0; }
int CGLSetCurrentContext(void *ctx) {
    if (ctx && !ctx_of(ctx)) return kCGLBadContext;
    cur_init();
    if (!g_cur_ready) return kCGLBadState;
    pthread_setspecific(g_cur_key, ctx);
    return kCGLNoError;
}
static void ctx_free(cgl_ctx_t *c) {
    if (CGLGetCurrentContext() == c) CGLSetCurrentContext(0);
    CGLReleasePixelFormat(c->pix);
    c->magic = 0;
    free(c);
}
void *CGLRetainContext(void *ctx) { cgl_ctx_t *c = ctx_of(ctx); if (c) __atomic_add_fetch(&c->refs, 1, __ATOMIC_ACQ_REL); return ctx; }
void CGLReleaseContext(void *ctx) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (c && __atomic_sub_fetch(&c->refs, 1, __ATOMIC_ACQ_REL) == 0) ctx_free(c);
}
int CGLDestroyContext(void *ctx) { if (!ctx_of(ctx)) return kCGLBadContext; CGLReleaseContext(ctx); return kCGLNoError; }
unsigned CGLGetContextRetainCount(void *ctx) { cgl_ctx_t *c = ctx_of(ctx); return c ? (unsigned)c->refs : 0; }
void *CGLGetPixelFormat(void *ctx) { cgl_ctx_t *c = ctx_of(ctx); return c ? c->pix : 0; }
void *CGLGetShareGroup(void *ctx) { cgl_ctx_t *c = ctx_of(ctx); while (c && c->share && ctx_of(c->share)) c = c->share; return c; }
/* trava recursiva por thread, como a CGL */
int CGLLockContext(void *ctx) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    void *me = pthread_self();
    if (__atomic_load_n(&c->lock_owner, __ATOMIC_ACQUIRE) == me) { c->lock_depth++; return kCGLNoError; }
    for (;;) {
        void *expect = 0;
        if (__atomic_compare_exchange_n(&c->lock_owner, &expect, me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) break;
        __builtin_ia32_pause();
    }
    c->lock_depth = 1;
    return kCGLNoError;
}
int CGLUnlockContext(void *ctx) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    if (__atomic_load_n(&c->lock_owner, __ATOMIC_ACQUIRE) != pthread_self()) return kCGLBadState;
    if (--c->lock_depth == 0) __atomic_store_n(&c->lock_owner, (void *)0, __ATOMIC_RELEASE);
    return kCGLNoError;
}
int CGLSetParameter(void *ctx, int pname, const int *params) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    if (!params) return kCGLBadAddress;
    if (pname < 0 || pname >= CTX_NPARAM) return kCGLBadEnumeration;
    if (pname == kCGLCPCurrentRendererID || pname == kCGLCPHasDrawable) return kCGLBadValue;   /* só leitura */
    c->params[pname] = params[0];
    return kCGLNoError;
}
int CGLGetParameter(void *ctx, int pname, int *params) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    if (!params) return kCGLBadAddress;
    if (pname < 0 || pname >= CTX_NPARAM) return kCGLBadEnumeration;
    if (pname == kCGLCPHasDrawable) { params[0] = c->view != 0; return kCGLNoError; }
    if (pname == kCGLCPGPUVertexProcessing || pname == kCGLCPGPUFragmentProcessing) { params[0] = 0; return kCGLNoError; }
    params[0] = c->params[pname];
    return kCGLNoError;
}
int CGLEnable(void *ctx, int pname) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    if (pname < 0 || pname >= CTX_NPARAM) return kCGLBadEnumeration;
    c->enabled[pname] = 1;
    return kCGLNoError;
}
int CGLDisable(void *ctx, int pname) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    if (pname < 0 || pname >= CTX_NPARAM) return kCGLBadEnumeration;
    c->enabled[pname] = 0;
    return kCGLNoError;
}
int CGLIsEnabled(void *ctx, int pname, int *enable) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    if (!enable) return kCGLBadAddress;
    if (pname < 0 || pname >= CTX_NPARAM) return kCGLBadEnumeration;
    *enable = c->enabled[pname];
    return kCGLNoError;
}
int CGLSetVirtualScreen(void *ctx, int vs) { cgl_ctx_t *c = ctx_of(ctx); if (!c) return kCGLBadContext; if (vs != 0) return kCGLBadValue; c->vscreen = vs; return kCGLNoError; }
int CGLGetVirtualScreen(void *ctx, int *vs) { cgl_ctx_t *c = ctx_of(ctx); if (!c) return kCGLBadContext; if (!vs) return kCGLBadAddress; *vs = c->vscreen; return kCGLNoError; }
/* sem framebuffer de GL: o flush conta e pede redesenho da view */
int CGLFlushDrawable(void *ctx) {
    cgl_ctx_t *c = ctx_of(ctx);
    if (!c) return kCGLBadContext;
    c->flushes++;
    if (c->view) MSG(void, c->view, "setNeedsDisplay:", (BOOL)1);
    return kCGLNoError;
}
int CGLUpdateContext(void *ctx) { return ctx_of(ctx) ? kCGLNoError : kCGLBadContext; }
int CGLClearDrawable(void *ctx) { cgl_ctx_t *c = ctx_of(ctx); if (!c) return kCGLBadContext; c->view = 0; return kCGLNoError; }
int CGLSetOffScreen(void *ctx, int w, int h, int rowbytes, void *base) { (void)w; (void)h; (void)rowbytes; (void)base; return ctx_of(ctx) ? kCGLBadMatch : kCGLBadContext; }
int CGLSetFullScreen(void *ctx) { return ctx_of(ctx) ? kCGLBadMatch : kCGLBadContext; }
int CGLSetFullScreenOnDisplay(void *ctx, u32 mask) { (void)mask; return ctx_of(ctx) ? kCGLBadMatch : kCGLBadContext; }
/* IOSurface como textura exige GL de verdade */
int CGLTexImageIOSurface2D(void *ctx, u32 target, u32 ifmt, int w, int h, u32 fmt, u32 type, void *surf, u32 plane) {
    (void)target; (void)ifmt; (void)w; (void)h; (void)fmt; (void)type; (void)surf; (void)plane;
    return ctx_of(ctx) ? kCGLBadState : kCGLBadContext;
}
void CGLGetVersion(int *major, int *minor) { if (major) *major = 1; if (minor) *minor = 2; }
static int g_opts[16];
int CGLSetOption(int opt, int val) { if (opt < 0 || opt >= 16) return kCGLBadEnumeration; g_opts[opt] = val; return kCGLNoError; }
int CGLGetOption(int opt, int *val) { if (opt < 0 || opt >= 16) return kCGLBadEnumeration; if (!val) return kCGLBadAddress; *val = g_opts[opt]; return kCGLNoError; }
int CGLSetGlobalOption(int opt, const int *val) { return val ? CGLSetOption(opt, *val) : kCGLBadAddress; }
int CGLGetGlobalOption(int opt, int *val) { return CGLGetOption(opt, val); }
const char *CGLErrorString(int err) {
    switch (err) {
    case kCGLNoError: return "no error";
    case kCGLBadAttribute: return "invalid pixel format attribute";
    case kCGLBadProperty: return "invalid renderer property";
    case kCGLBadPixelFormat: return "invalid pixel format";
    case kCGLBadContext: return "invalid context";
    case kCGLBadValue: return "invalid numerical value";
    case kCGLBadMatch: return "invalid share context";
    case kCGLBadEnumeration: return "invalid enumerant";
    case kCGLBadAddress: return "invalid memory address";
    case kCGLBadRendererInfo: return "invalid renderer information";
    case kCGLBadState: return "invalid state";
    default: return "unknown error";
    }
}
/* informação de renderizador: um, o genérico de software */
#define RI_MAGIC 0x43474C52u
typedef struct { u32 magic; } cgl_ri_t;
int CGLQueryRendererInfo(u32 mask, void **rend, int *nrend) {
    (void)mask;
    if (!rend || !nrend) return kCGLBadAddress;
    cgl_ri_t *r = calloc(1, sizeof *r);
    if (!r) return kCGLBadState;
    r->magic = RI_MAGIC;
    *rend = r;
    *nrend = 1;
    return kCGLNoError;
}
int CGLDestroyRendererInfo(void *rend) { cgl_ri_t *r = rend; if (!r || r->magic != RI_MAGIC) return kCGLBadRendererInfo; r->magic = 0; free(r); return kCGLNoError; }
enum { kCGLRPOffScreen = 53, kCGLRPFullScreen = 54, kCGLRPRendererID = 70, kCGLRPAccelerated = 73, kCGLRPRobust = 75,
       kCGLRPBackingStore = 76, kCGLRPMPSafe = 78, kCGLRPWindow = 80, kCGLRPMultiScreen = 81, kCGLRPCompliant = 83,
       kCGLRPDisplayMask = 84, kCGLRPVideoMemoryMegabytes = 131, kCGLRPTextureMemoryMegabytes = 132,
       kCGLRPRendererCount = 128, kCGLRPOnline = 129, kCGLRPMajorGLVersion = 133 };
int CGLDescribeRenderer(void *rend, int idx, int prop, int *value) {
    cgl_ri_t *r = rend;
    if (!r || r->magic != RI_MAGIC) return kCGLBadRendererInfo;
    if (idx != 0) return kCGLBadValue;
    if (!value) return kCGLBadAddress;
    switch (prop) {
    case kCGLRPRendererID: *value = (int)GL_RENDERER_SOFT; break;
    case kCGLRPRendererCount: case kCGLRPOnline: case kCGLRPWindow: case kCGLRPMPSafe: case kCGLRPCompliant:
    case kCGLRPBackingStore: case kCGLRPDisplayMask: *value = 1; break;
    case kCGLRPMajorGLVersion: *value = 2; break;
    case kCGLRPVideoMemoryMegabytes: case kCGLRPTextureMemoryMegabytes: *value = 256; break;
    case kCGLRPAccelerated: case kCGLRPOffScreen: case kCGLRPFullScreen: case kCGLRPRobust: case kCGLRPMultiScreen: *value = 0; break;
    default: return kCGLBadProperty;
    }
    return kCGLNoError;
}

/* ================================================================ objetos ObjC: ponteiro C num valor associado */
static char gl_obj_key, gl_fmt_key;
static void *gl_get(id o) { id v = o ? objc_getAssociatedObject(o, &gl_obj_key) : 0; return v ? MSG0(void *, v, "pointerValue") : 0; }
static void gl_set(id o, void *p) {
    id v = p ? MSG(id, (id)cx_class_named("NSValue"), "valueWithPointer:", p) : 0;
    objc_setAssociatedObject(o, &gl_obj_key, v, 1);
}
static void super_dealloc(id self, const char *cls) {
    ((void (*)(id, SEL))class_getMethodImplementation(cx_class_named(cls), sel_registerName("dealloc")))(self, sel_registerName("dealloc"));
}

/* ---- NSOpenGLPixelFormat */
static id pf_init_attrs(id self, SEL c, const u32 *attrs) {
    (void)c;
    self = ((id (*)(id, SEL))class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("init")))(self, sel_registerName("init"));
    if (!self) return 0;
    cgl_pix_t *p = pix_new(attrs);
    if (!p) { objc_release(self); return 0; }
    gl_set(self, p);
    return self;
}
static id pf_init(id self, SEL c) {
    static const u32 dflt[] = {PFA_DoubleBuffer, PFA_ColorSize, 24, PFA_DepthSize, 16, 0};
    return pf_init_attrs(self, c, dflt);
}
static id pf_init_cgl(id self, SEL c, void *pix) {
    (void)c;
    if (!pix_of(pix)) { objc_release(self); return 0; }
    self = ((id (*)(id, SEL))class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("init")))(self, sel_registerName("init"));
    if (self) gl_set(self, CGLRetainPixelFormat(pix));
    return self;
}
static void *pf_cgl(id self, SEL c) { (void)c; return gl_get(self); }
static void pf_get_values(id self, SEL c, int *vals, u32 attr, int vs) {
    (void)c;
    if (vals) CGLDescribePixelFormat(gl_get(self), vs, (int)attr, vals);
}
static i64 pf_nscreens(id self, SEL c) { (void)c; int v = 0; CGLDescribePixelFormat(gl_get(self), 0, PFA_VirtualScreenCount, &v); return v; }
static void pf_dealloc(id self, SEL c) {
    (void)c;
    CGLReleasePixelFormat(gl_get(self));
    gl_set(self, 0);
    super_dealloc(self, "NSObject");
}

/* ---- NSOpenGLContext */
static id ctx_init_fmt(id self, SEL c, id fmt, id share) {
    (void)c;
    void *pix = fmt ? gl_get(fmt) : 0, *sh = share ? gl_get(share) : 0, *ctx = 0;
    if (CGLCreateContext(pix, sh, &ctx) != kCGLNoError) { objc_release(self); return 0; }
    self = ((id (*)(id, SEL))class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("init")))(self, sel_registerName("init"));
    if (!self) { CGLReleaseContext(ctx); return 0; }
    ctx_of(ctx)->owner = self;
    gl_set(self, ctx);
    objc_setAssociatedObject(self, &gl_fmt_key, fmt, 1);   /* pixelFormat retido */
    return self;
}
static id ctx_init_cgl(id self, SEL c, void *cgl) {
    (void)c;
    cgl_ctx_t *x = ctx_of(cgl);
    if (!x) { objc_release(self); return 0; }
    self = ((id (*)(id, SEL))class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("init")))(self, sel_registerName("init"));
    if (!self) return 0;
    CGLRetainContext(x);
    if (!x->owner) x->owner = self;
    gl_set(self, x);
    return self;
}
static void *ctx_cgl(id self, SEL c) { (void)c; return gl_get(self); }
static id ctx_pixel_format(id self, SEL c) {
    (void)c;
    id f = objc_getAssociatedObject(self, &gl_fmt_key);
    if (f) return f;
    cgl_ctx_t *x = ctx_of(gl_get(self));
    if (!x) return 0;
    f = MSG(id, MSG0(id, (id)&NSOpenGLPixelFormat_c, "alloc"), "initWithCGLPixelFormatObj:", (void *)x->pix);
    objc_setAssociatedObject(self, &gl_fmt_key, f, 1);
    objc_release(f);
    return f;
}
static void ctx_make_current(id self, SEL c) { (void)c; CGLSetCurrentContext(gl_get(self)); }
static id ctx_current(id cls, SEL c) {
    (void)cls; (void)c;
    cgl_ctx_t *x = ctx_of(CGLGetCurrentContext());
    return x ? x->owner : 0;
}
static void ctx_clear_current(id cls, SEL c) { (void)cls; (void)c; CGLSetCurrentContext(0); }
static void ctx_flush(id self, SEL c) { (void)c; CGLFlushDrawable(gl_get(self)); }
static void ctx_set_view(id self, SEL c, id v) {
    (void)c;
    cgl_ctx_t *x = ctx_of(gl_get(self));
    if (x) x->view = v;
}
static id ctx_view(id self, SEL c) { (void)c; cgl_ctx_t *x = ctx_of(gl_get(self)); return x ? x->view : 0; }
static void ctx_clear_drawable(id self, SEL c) { (void)c; CGLClearDrawable(gl_get(self)); }
static void ctx_update(id self, SEL c) { (void)c; CGLUpdateContext(gl_get(self)); }
static void ctx_set_values(id self, SEL c, const int *vals, i64 param) { (void)c; CGLSetParameter(gl_get(self), (int)param, vals); }
static void ctx_get_values(id self, SEL c, int *vals, i64 param) { (void)c; CGLGetParameter(gl_get(self), (int)param, vals); }
static void ctx_lock(id self, SEL c) { (void)c; CGLLockContext(gl_get(self)); }
static void ctx_unlock(id self, SEL c) { (void)c; CGLUnlockContext(gl_get(self)); }
static i64 ctx_vscreen(id self, SEL c) { (void)c; int v = 0; CGLGetVirtualScreen(gl_get(self), &v); return v; }
static void ctx_set_vscreen(id self, SEL c, i64 v) { (void)c; CGLSetVirtualScreen(gl_get(self), (int)v); }
static void ctx_dealloc(id self, SEL c) {
    (void)c;
    cgl_ctx_t *x = ctx_of(gl_get(self));
    if (x && x->owner == self) x->owner = 0;
    CGLReleaseContext(x);
    gl_set(self, 0);
    objc_setAssociatedObject(self, &gl_fmt_key, 0, 1);
    super_dealloc(self, "NSObject");
}

/* ---- NSOpenGLView: pixel format e contexto preguiçosos, prepareOpenGL uma vez antes do primeiro desenho */
static char glv_fmt_key, glv_ctx_key, glv_prep_key, glv_best_key;
static id glv_default_fmt(id cls, SEL c) {
    (void)cls; (void)c;
    return objc_autorelease(MSG0(id, MSG0(id, (id)&NSOpenGLPixelFormat_c, "alloc"), "init"));
}
static id glv_init_fmt(id self, SEL c, gl_rect_t f, id fmt) {
    (void)c;
    self = ((id (*)(id, SEL, gl_rect_t))class_getMethodImplementation(cx_class_named("NSView"), sel_registerName("initWithFrame:")))(self, sel_registerName("initWithFrame:"), f);
    if (self && fmt) objc_setAssociatedObject(self, &glv_fmt_key, fmt, 1);
    return self;
}
static id glv_init(id self, SEL c, gl_rect_t f) { (void)c; return glv_init_fmt(self, 0, f, 0); }
static id glv_fmt(id self, SEL c) {
    (void)c;
    id f = objc_getAssociatedObject(self, &glv_fmt_key);
    if (!f) {
        f = MSG0(id, (id)self->isa, "defaultPixelFormat");
        if (f) objc_setAssociatedObject(self, &glv_fmt_key, f, 1);
    }
    return f;
}
static void glv_set_fmt(id self, SEL c, id f) { (void)c; objc_setAssociatedObject(self, &glv_fmt_key, f, 1); }
static id glv_ctx(id self, SEL c) {
    (void)c;
    id x = objc_getAssociatedObject(self, &glv_ctx_key);
    if (!x) {
        x = MSG(id, MSG0(id, (id)&NSOpenGLContext_c, "alloc"), "initWithFormat:shareContext:", glv_fmt(self, 0), (id)0);
        if (!x) return 0;
        objc_setAssociatedObject(self, &glv_ctx_key, x, 1);
        objc_release(x);
        MSG(void, x, "setView:", self);
    }
    return x;
}
static void glv_set_ctx(id self, SEL c, id x) {
    (void)c;
    objc_setAssociatedObject(self, &glv_ctx_key, x, 1);
    if (x) MSG(void, x, "setView:", self);
}
static void glv_clear_ctx(id self, SEL c) {
    (void)c;
    id x = objc_getAssociatedObject(self, &glv_ctx_key);
    if (x) MSG0(void, x, "clearDrawable");
    objc_setAssociatedObject(self, &glv_ctx_key, 0, 1);
}
static void glv_noop(id self, SEL c) { (void)self; (void)c; }
static void glv_update(id self, SEL c) { (void)c; id x = objc_getAssociatedObject(self, &glv_ctx_key); if (x) MSG0(void, x, "update"); }
static void glv_prepare_once(id self) {
    if (objc_getAssociatedObject(self, &glv_prep_key)) return;
    objc_setAssociatedObject(self, &glv_prep_key, (id)cx_class_named("NSObject"), 0);   /* marca: já preparado */
    id x = glv_ctx(self, 0);
    if (x) MSG0(void, x, "makeCurrentContext");
    MSG0(void, self, "prepareOpenGL");
}
void NSRectFill(gl_rect_t r);
static void glv_draw(id self, SEL c, gl_rect_t r) {
    (void)c; (void)r;
    glv_prepare_once(self);
    id black = MSG(id, (id)cx_class_named("NSColor"), "colorWithCalibratedWhite:alpha:", 0.0, 1.0);
    if (black) MSG0(void, black, "setFill");
    NSRectFill(MSGR(gl_rect_t, self, "bounds"));
}
/* mudou o tamanho: o contexto se atualiza e o app recebe reshape (prepareOpenGL antes, uma vez) */
static void glv_set_frame_size(id self, SEL c, gl_size_t z) {
    (void)c;
    ((void (*)(id, SEL, gl_size_t))class_getMethodImplementation(cx_class_named("NSView"), sel_registerName("setFrameSize:")))(self, sel_registerName("setFrameSize:"), z);
    glv_prepare_once(self);
    glv_update(self, 0);
    MSG0(void, self, "reshape");
}
static BOOL glv_yes(id s, SEL c) { (void)s; (void)c; return 1; }
static BOOL glv_wants_best(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &glv_best_key) != 0; }
static void glv_set_wants_best(id self, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(self, &glv_best_key, b ? (id)cx_class_named("NSObject") : 0, 0); }

void cx_appkit_nsopengl_init(void) {
    cx_class_t *ob = cx_class_named("NSObject"), *nv = cx_class_named("NSView");
    if (!ob || !nv) return;
    if (!NSOpenGLPixelFormat_c.vtable) cx_define_class(&NSOpenGLPixelFormat_c, &NSOpenGLPixelFormat_m, "NSOpenGLPixelFormat", ob, 8);
    cx_add_method(&NSOpenGLPixelFormat_c, "init", (void *)pf_init);
    cx_add_method(&NSOpenGLPixelFormat_c, "initWithAttributes:", (void *)pf_init_attrs);
    cx_add_method(&NSOpenGLPixelFormat_c, "initWithCGLPixelFormatObj:", (void *)pf_init_cgl);
    cx_add_method(&NSOpenGLPixelFormat_c, "CGLPixelFormatObj", (void *)pf_cgl);
    cx_add_method(&NSOpenGLPixelFormat_c, "getValues:forAttribute:forVirtualScreen:", (void *)pf_get_values);
    cx_add_method(&NSOpenGLPixelFormat_c, "numberOfVirtualScreens", (void *)pf_nscreens);
    cx_add_method(&NSOpenGLPixelFormat_c, "dealloc", (void *)pf_dealloc);

    if (!NSOpenGLContext_c.vtable) cx_define_class(&NSOpenGLContext_c, &NSOpenGLContext_m, "NSOpenGLContext", ob, 8);
    cx_add_method(&NSOpenGLContext_c, "initWithFormat:shareContext:", (void *)ctx_init_fmt);
    cx_add_method(&NSOpenGLContext_c, "initWithCGLContextObj:", (void *)ctx_init_cgl);
    cx_add_method(&NSOpenGLContext_c, "CGLContextObj", (void *)ctx_cgl);
    cx_add_method(&NSOpenGLContext_c, "pixelFormat", (void *)ctx_pixel_format);
    cx_add_method(&NSOpenGLContext_c, "makeCurrentContext", (void *)ctx_make_current);
    cx_add_cmethod(&NSOpenGLContext_c, "currentContext", (void *)ctx_current);
    cx_add_cmethod(&NSOpenGLContext_c, "clearCurrentContext", (void *)ctx_clear_current);
    cx_add_method(&NSOpenGLContext_c, "flushBuffer", (void *)ctx_flush);
    cx_add_method(&NSOpenGLContext_c, "setView:", (void *)ctx_set_view);
    cx_add_method(&NSOpenGLContext_c, "view", (void *)ctx_view);
    cx_add_method(&NSOpenGLContext_c, "clearDrawable", (void *)ctx_clear_drawable);
    cx_add_method(&NSOpenGLContext_c, "update", (void *)ctx_update);
    cx_add_method(&NSOpenGLContext_c, "setValues:forParameter:", (void *)ctx_set_values);
    cx_add_method(&NSOpenGLContext_c, "getValues:forParameter:", (void *)ctx_get_values);
    cx_add_method(&NSOpenGLContext_c, "lock", (void *)ctx_lock);
    cx_add_method(&NSOpenGLContext_c, "unlock", (void *)ctx_unlock);
    cx_add_method(&NSOpenGLContext_c, "currentVirtualScreen", (void *)ctx_vscreen);
    cx_add_method(&NSOpenGLContext_c, "setCurrentVirtualScreen:", (void *)ctx_set_vscreen);
    cx_add_method(&NSOpenGLContext_c, "dealloc", (void *)ctx_dealloc);

    if (!NSOpenGLView_c.vtable) cx_define_class(&NSOpenGLView_c, &NSOpenGLView_m, "NSOpenGLView", nv, 8);
    cx_add_cmethod(&NSOpenGLView_c, "defaultPixelFormat", (void *)glv_default_fmt);
    cx_add_method(&NSOpenGLView_c, "initWithFrame:", (void *)glv_init);
    cx_add_method(&NSOpenGLView_c, "initWithFrame:pixelFormat:", (void *)glv_init_fmt);
    cx_add_method(&NSOpenGLView_c, "pixelFormat", (void *)glv_fmt);
    cx_add_method(&NSOpenGLView_c, "setPixelFormat:", (void *)glv_set_fmt);
    cx_add_method(&NSOpenGLView_c, "openGLContext", (void *)glv_ctx);
    cx_add_method(&NSOpenGLView_c, "setOpenGLContext:", (void *)glv_set_ctx);
    cx_add_method(&NSOpenGLView_c, "clearGLContext", (void *)glv_clear_ctx);
    cx_add_method(&NSOpenGLView_c, "prepareOpenGL", (void *)glv_noop);
    cx_add_method(&NSOpenGLView_c, "reshape", (void *)glv_noop);
    cx_add_method(&NSOpenGLView_c, "update", (void *)glv_update);
    cx_add_method(&NSOpenGLView_c, "drawRect:", (void *)glv_draw);
    cx_add_method(&NSOpenGLView_c, "setFrameSize:", (void *)glv_set_frame_size);
    cx_add_method(&NSOpenGLView_c, "isOpaque", (void *)glv_yes);
    cx_add_method(&NSOpenGLView_c, "wantsBestResolutionOpenGLSurface", (void *)glv_wants_best);
    cx_add_method(&NSOpenGLView_c, "setWantsBestResolutionOpenGLSurface:", (void *)glv_set_wants_best);
}

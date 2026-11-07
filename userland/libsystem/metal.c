/*
 * mOS libSystem — Metal por software (Metal.framework + CAMetalLayer + MetalKit/MTKView).
 *
 * O mOS não tem driver de GPU: o "MTLDevice" daqui roda tudo na CPU. Buffers e texturas são memória
 * comum (unified memory); os shaders são compilados do fonte MSL (newLibraryWithSource:, ou os .metal
 * do bundle em newDefaultLibrary) pelo compilador de mtlshader.c e interpretados por mtlexec.c; o
 * rasterizador, o compute e o blit ficam em metalexec.c. Os comandos são gravados nos encoders (com
 * uma cópia do estado a cada draw) e executados no -commit, em ordem; os handlers de conclusão rodam
 * logo depois. .metallib (AIR, LLVM bitcode) não é suportado: a lib devolve erro com a explicação.
 *
 * CAMetalLayer: os drawables são texturas sobre IOSurfaces BGRA; -present põe o IOSurface como
 * contents do layer, e o compositor do QuartzCore (quartzcore.c) o mostra na janela. Formatos que
 * não são BGRA8 são convertidos no present.
 *
 * Processos do Chromium/Electron continuam sem device (MTLCreateSystemDefaultDevice = nil) para não
 * trocarem o caminho de software (SwiftShader) pelo ANGLE/Metal; MOS_METAL=1 força, MOS_METAL=0 desliga.
 */
#include "metalint.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void *class_replaceMethod(cx_class_t *c, SEL s, void *imp, const char *types);
u64 mach_absolute_time(void);
u32 _dyld_image_count(void);
const char *_dyld_get_image_name(u32 i);
void *objc_msgSend_stret(void *ret, id self, SEL op, ...);
typedef id IOSurfaceRef;
IOSurfaceRef IOSurfaceCreate(id props);
void *IOSurfaceGetBaseAddress(IOSurfaceRef s);
void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef s, u64 i);
u64 IOSurfaceGetBytesPerRow(IOSurfaceRef s);
u64 IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef s, u64 i);
u64 IOSurfaceGetWidth(IOSurfaceRef s);
u64 IOSurfaceGetHeight(IOSurfaceRef s);
int IOSurfaceLock(IOSurfaceRef s, u32 opts, u32 *seed);
int IOSurfaceUnlock(IOSurfaceRef s, u32 opts, u32 *seed);
extern const void *kIOSurfaceWidth, *kIOSurfaceHeight, *kIOSurfaceBytesPerElement, *kIOSurfacePixelFormat, *kIOSurfaceBytesPerRow;
char *strstr(const char *h, const char *n);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);

typedef struct { double x, y; } CGP;
typedef struct { double w, h; } CGS;
typedef struct { CGP o; CGS s; } CGR;
typedef struct { u64 loc, len; } nsrange_t;
typedef struct { u64 size, align; } sizealign_t;
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
#define BLK1(b, a) ((void (*)(void *, id))((blk_t *)(b))->invoke)((b), (id)(a))
#define BLK2(b, a, c) ((void (*)(void *, id, id))((blk_t *)(b))->invoke)((b), (id)(a), (id)(c))

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
#define SCLASS(N) static cx_class_t N##_c, N##_m;
#define DEF(N, name, super, T) cx_define_class(&N##_c, &N##_m, name, super, (u32)(sizeof(T) - 8))
#define M(N, s, f) cx_add_method(&N##_c, s, (void *)(f))
#define CM(N, s, f) cx_add_cmethod(&N##_c, s, (void *)(f))

/* ---- descritores exportados */
CLASS(MTLTextureDescriptor) CLASS(MTLSamplerDescriptor) CLASS(MTLDepthStencilDescriptor) CLASS(MTLStencilDescriptor)
CLASS(MTLRenderPassDescriptor) CLASS(MTLRenderPassAttachmentDescriptor) CLASS(MTLRenderPassColorAttachmentDescriptor)
CLASS(MTLRenderPassDepthAttachmentDescriptor) CLASS(MTLRenderPassStencilAttachmentDescriptor)
CLASS(MTLRenderPassColorAttachmentDescriptorArray) CLASS(MTLRenderPipelineDescriptor)
CLASS(MTLRenderPipelineColorAttachmentDescriptor) CLASS(MTLRenderPipelineColorAttachmentDescriptorArray)
CLASS(MTLVertexDescriptor) CLASS(MTLVertexAttributeDescriptor) CLASS(MTLVertexAttributeDescriptorArray)
CLASS(MTLVertexBufferLayoutDescriptor) CLASS(MTLVertexBufferLayoutDescriptorArray) CLASS(MTLComputePipelineDescriptor)
CLASS(MTLPipelineBufferDescriptor) CLASS(MTLPipelineBufferDescriptorArray) CLASS(MTLCompileOptions)
CLASS(MTLFunctionConstantValues) CLASS(MTLFunctionDescriptor) CLASS(MTLHeapDescriptor) CLASS(MTLCommandBufferDescriptor)
CLASS(MTLComputePassDescriptor) CLASS(MTLBlitPassDescriptor) CLASS(MTLCaptureDescriptor) CLASS(MTLArgumentDescriptor)
CLASS(MTLIndirectCommandBufferDescriptor) CLASS(MTLStageInputOutputDescriptor) CLASS(MTLAttributeDescriptor)
CLASS(MTLAttributeDescriptorArray) CLASS(MTLBufferLayoutDescriptor) CLASS(MTLBufferLayoutDescriptorArray)
CLASS(MTLRenderPassSampleBufferAttachmentDescriptor) CLASS(MTLRenderPassSampleBufferAttachmentDescriptorArray)
CLASS(MTLComputePassSampleBufferAttachmentDescriptor) CLASS(MTLComputePassSampleBufferAttachmentDescriptorArray)
CLASS(MTLBlitPassSampleBufferAttachmentDescriptor) CLASS(MTLBlitPassSampleBufferAttachmentDescriptorArray)
CLASS(MTLCounterSampleBufferDescriptor) CLASS(MTLLinkedFunctions) CLASS(MTLBinaryArchiveDescriptor)
CLASS(MTLRasterizationRateMapDescriptor) CLASS(MTLResidencySetDescriptor) CLASS(MTLTileRenderPipelineDescriptor)
CLASS(MTLTileRenderPipelineColorAttachmentDescriptor) CLASS(MTLTileRenderPipelineColorAttachmentDescriptorArray)
CLASS(MTLRenderPassDescriptorArrayDummy)
/* objetos exportados */
CLASS(MTLCaptureManager) CLASS(MTLSharedEventListener) CLASS(MTLSharedEventHandle) CLASS(MTKView)
CLASS(MTLVertexAttribute) CLASS(MTLAttribute) CLASS(MTLArgument) CLASS(MTLRenderPipelineReflection)
CLASS(MTLComputePipelineReflection)
extern cx_class_t CAMetalLayer_c __asm__("OBJC_CLASS_$_CAMetalLayer");
extern cx_class_t CALayer_c __asm__("OBJC_CLASS_$_CALayer");

/* ---- objetos internos */
SCLASS(MObj) SCLASS(MDev) SCLASS(MBuf) SCLASS(MTex) SCLASS(MSamp) SCLASS(MDss) SCLASS(MLib) SCLASS(MFunc)
SCLASS(MRPipe) SCLASS(MCPipe) SCLASS(MQueue) SCLASS(MCB) SCLASS(MREnc) SCLASS(MCEnc) SCLASS(MBEnc) SCLASS(MFence)
SCLASS(MEvent) SCLASS(MSEvent) SCLASS(MHeap) SCLASS(MDrawable) SCLASS(MScope) SCLASS(MArch) SCLASS(MErr)

/* ================================================================ utilidades */
static id g_dev;
static volatile int g_gpu_lock;
static int g_presenting;
static void gpu_lock(void) { while (__sync_lock_test_and_set(&g_gpu_lock, 1)) SYS0(SYS_YIELD); }
static void gpu_unlock(void) { __sync_lock_release(&g_gpu_lock); }
static id ret_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static u64 ret_zero(id s, SEL c) { (void)s; (void)c; return 0; }
static u64 ret_one(id s, SEL c) { (void)s; (void)c; return 1; }
static BOOL ret_no(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL ret_yes(id s, SEL c) { (void)s; (void)c; return YES; }
static void noop(id s, SEL c) { (void)s; (void)c; }
static id ret_self(id s, SEL c) { (void)c; return s; }
static id ret_empty_arr(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static id ret_empty_dict(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)cx_class_named("NSDictionary"), "dictionary"); }
static double now_s(void) { return (double)mach_absolute_time() / 1e9; }
static void super_dealloc(id self, cx_class_t *super) {
    IMP sup = cx_lookup(super, sel_registerName("dealloc"));
    if (sup) ((void (*)(id, SEL))sup)(self, sel_registerName("dealloc"));
}
static id new_obj(cx_class_t *c) { mobj_t *o = (mobj_t *)cx_alloc_instance(c); o->dev = g_dev; return (id)o; }

/* base: label, device */
static id o_label(id s, SEL c) { (void)c; return ((mobj_t *)s)->label; }
static void o_set_label(id s, SEL c, id l) {
    (void)c;
    mobj_t *o = (mobj_t *)s;
    if (o->label == l) return;
    id n = l ? MSG0(id, l, "copy") : 0;
    if (o->label) objc_release(o->label);
    o->label = n;
}
static id o_device(id s, SEL c) { (void)c; return ((mobj_t *)s)->dev; }
static void o_dealloc(id s, SEL c) {
    (void)c;
    mobj_t *o = (mobj_t *)s;
    if (o->label) objc_release(o->label);
    o->label = 0;
    super_dealloc(s, cx_class_named("NSObject"));
}

/* NSError do Metal (domínio, código, descrição) */
typedef struct { cx_class_t *isa; id domain; i64 code; id desc; } merr_t;
static id e_desc(id s, SEL c) { (void)c; return ((merr_t *)s)->desc; }
static id e_domain(id s, SEL c) { (void)c; return ((merr_t *)s)->domain; }
static i64 e_code(id s, SEL c) { (void)c; return ((merr_t *)s)->code; }
static id e_info(id s, SEL c) {
    (void)c;
    merr_t *e = (merr_t *)s;
    id k = cx_str("NSLocalizedDescription");
    id d = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", e->desc, k);
    objc_release(k);
    return d;
}
static void e_dealloc(id s, SEL c) {
    merr_t *e = (merr_t *)s;
    if (e->domain) objc_release(e->domain);
    if (e->desc) objc_release(e->desc);
    (void)c;
    super_dealloc(s, cx_class_named("NSObject"));
}
static id mk_error(const char *domain, i64 code, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    extern int vsnprintf(char *b, size_t n, const char *f, va_list ap);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    merr_t *e = (merr_t *)cx_alloc_instance(&MErr_c);
    e->domain = cx_str(domain);
    e->code = code;
    e->desc = cx_str(buf);
    cx_fdprintf(2, "Metal: %s\n", buf);
    return objc_autorelease((id)e);
}
static void set_err(id *err, id e) { if (err) *err = e; }

/* ================================================================ descritores genéricos */
/* tipos: u (NSUInteger/enum), b (BOOL), f (float), d (double), o (objeto retido), w (objeto fraco),
 * O (objeto criado sob demanda), A (array indexado sob demanda), c (MTLClearColor), p (CGPoint/CGSize),
 * t (CATransform3D). Também usado pelo coreanimation.c (animações e camadas): cada classe tem a lista
 * completa das suas propriedades, guardadas depois das ivars da superclasse, com um u64 de "já
 * inicializado" na frente (instâncias criadas sem -init, como as do +[CALayer layer], recebem os
 * valores padrão no primeiro acesso). */
typedef struct {
    cx_class_t *cls, *real_super;   /* real_super: primeiro ancestral que não é descritor genérico */
    const pspec_t *ps;
    int np, nslots;
    u32 base;
    int off[64];
    SEL get[64], set[64], isget[64];
    cx_class_t *elem;        /* classes de array: elemento */
    void (*changed)(id self, const char *prop);
} dclass_t;
static dclass_t g_dc[160];
static int g_ndc;
typedef struct { cx_class_t *isa; cx_class_t *elem; id it[32]; } darr_t;

static dclass_t *dc_of(id o) {
    if (!o) return 0;
    for (cx_class_t *c = o->isa; c; c = c->super)
        for (int i = 0; i < g_ndc; i++) if (g_dc[i].cls == c) return &g_dc[i];
    return 0;
}
static int dprop(dclass_t *d, SEL s) {
    for (int i = 0; i < d->np; i++) if (d->get[i] == s || d->set[i] == s || d->isget[i] == s) return i;
    return -1;
}
static int dprop_name(dclass_t *d, const char *n) {
    for (int i = 0; i < d->np; i++) if (!strcmp(d->ps[i].name, n)) return i;
    return -1;
}
static u64 *dbase(id o, dclass_t *d) { return (u64 *)((u8 *)o + d->base); }
static void d_defaults(id self, dclass_t *d);
static u64 *dslot(id o, dclass_t *d, int i) {
    u64 *b = dbase(o, d);
    if (!b[0]) { b[0] = 1; d_defaults(o, d); }
    return &b[1 + d->off[i]];
}
static void dchanged(id o, dclass_t *d, int i) { if (d->changed) d->changed(o, d->ps[i].name); }
static id d_autoobj(cx_class_t *c) { return MSG0(id, MSG0(id, (id)c, "alloc"), "init"); }
static id darr_new(cx_class_t *arrcls) {
    darr_t *a = (darr_t *)cx_alloc_instance(arrcls);
    dclass_t *d = dc_of((id)a);
    a->elem = d ? d->elem : 0;
    return (id)a;
}
static u64 dg_u(id self, SEL c) {
    dclass_t *d = dc_of(self);
    int i = d ? dprop(d, c) : -1;
    if (i < 0) return 0;
    u64 *s = dslot(self, d, i);
    char t = d->ps[i].ty;
    if (t == 'O' && !*s) *s = (u64)d_autoobj(d->ps[i].cls);
    if (t == 'A' && !*s) *s = (u64)darr_new(d->ps[i].cls);
    return *s;
}
static float dg_f(id self, SEL c) { dclass_t *d = dc_of(self); int i = d ? dprop(d, c) : -1; float f = 0; if (i >= 0) memcpy(&f, dslot(self, d, i), 4); return f; }
static double dg_d(id self, SEL c) { dclass_t *d = dc_of(self); int i = d ? dprop(d, c) : -1; double f = 0; if (i >= 0) memcpy(&f, dslot(self, d, i), 8); return f; }
static mtl_cc_t dg_c(id self, SEL c) { dclass_t *d = dc_of(self); int i = d ? dprop(d, c) : -1; mtl_cc_t v = {0, 0, 0, 1}; if (i >= 0) memcpy(&v, dslot(self, d, i), sizeof v); return v; }
typedef struct { double x, y; } dpt_t;
typedef struct { double m[16]; } dt3_t;
static dpt_t dg_p(id self, SEL c) { dclass_t *d = dc_of(self); int i = d ? dprop(d, c) : -1; dpt_t v = {0, 0}; if (i >= 0) memcpy(&v, dslot(self, d, i), sizeof v); return v; }
static dt3_t dg_t(id self, SEL c) { dclass_t *d = dc_of(self); int i = d ? dprop(d, c) : -1; dt3_t v = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; if (i >= 0) memcpy(&v, dslot(self, d, i), sizeof v); return v; }
static void ds_u(id self, SEL c, u64 v) {
    dclass_t *d = dc_of(self);
    int i = d ? dprop(d, c) : -1;
    if (i < 0) return;
    u64 *s = dslot(self, d, i);
    char t = d->ps[i].ty;
    if (t == 'b') v = (u8)v != 0;
    else if (t == 'o' || t == 'O' || t == 'A') {
        id n = (id)v;
        if ((id)*s == n) return;
        if (n) objc_retain(n);
        if (*s) objc_release((id)*s);
    }
    *s = v;
    dchanged(self, d, i);
}
#define DSET(fn, T) static void fn(id self, SEL c, T v) { dclass_t *d = dc_of(self); int i = d ? dprop(d, c) : -1; if (i < 0) return; memcpy(dslot(self, d, i), &v, sizeof v); dchanged(self, d, i); }
DSET(ds_f, float) DSET(ds_d, double) DSET(ds_c, mtl_cc_t) DSET(ds_p, dpt_t) DSET(ds_t, dt3_t)

static void d_defaults(id self, dclass_t *d) {
    u64 *b = dbase(self, d);
    b[0] = 1;
    for (int i = 0; i < d->np; i++) {
        u64 *s = &b[1 + d->off[i]];
        const pspec_t *p = &d->ps[i];
        switch (p->ty) {
        case 'f': { float f = (float)p->dd; memcpy(s, &f, 4); break; }
        case 'd': memcpy(s, &p->dd, 8); break;
        case 'c': { mtl_cc_t v = {0, 0, 0, 1}; memcpy(s, &v, sizeof v); break; }
        case 't': { dt3_t v = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; memcpy(s, &v, sizeof v); break; }
        case 'p': { double v[2] = {p->dd, (double)(i64)p->def / 1000.0}; memcpy(s, v, sizeof v); break; }
        case 'u': case 'b': *s = p->def; break;
        }
    }
}
static id d_init(id self, SEL c) {
    dclass_t *d = dc_of(self);
    if (d && d->real_super) {
        IMP si = cx_lookup(d->real_super, c);
        if (si) self = ((id (*)(id, SEL))si)(self, c);
    }
    if (self && d && !dbase(self, d)[0]) d_defaults(self, d);
    return self;
}
static void d_release_all(id self, dclass_t *d) {
    u64 *b = dbase(self, d);
    if (!b[0]) return;
    for (int i = 0; i < d->np; i++) {
        char t = d->ps[i].ty;
        u64 *s = &b[1 + d->off[i]];
        if ((t == 'o' || t == 'O' || t == 'A') && *s) { objc_release((id)*s); *s = 0; }
    }
}
static void d_dealloc(id self, SEL c) {
    dclass_t *d = dc_of(self);
    if (d) d_release_all(self, d);
    super_dealloc(self, d && d->real_super ? d->real_super : cx_class_named("NSObject"));
    (void)c;
}
static id d_copy(id self, SEL c) {
    (void)c;
    dclass_t *d = dc_of(self);
    id n = cx_alloc_instance(self->isa);
    if (!d) return n;
    dslot(self, d, 0);
    memcpy(dbase(n, d), dbase(self, d), (u64)(d->nslots + 1) * 8);
    for (int i = 0; i < d->np; i++) {
        char t = d->ps[i].ty;
        u64 *s = &dbase(n, d)[1 + d->off[i]];
        if (!*s) continue;
        if (t == 'o') objc_retain((id)*s);
        else if (t == 'O' || t == 'A') *s = (u64)MSG0(id, (id)*s, "copy");
    }
    return n;
}
static id d_copyz(id self, SEL c, void *z) { (void)z; return d_copy(self, c); }
static void d_reset(id self, SEL c) {
    (void)c;
    dclass_t *d = dc_of(self);
    if (!d) return;
    d_release_all(self, d);
    memset(dbase(self, d), 0, (u64)(d->nslots + 1) * 8);
    d_defaults(self, d);
}
/* arrays de descritores */
static id da_get(id self, SEL c, u64 i) {
    (void)c;
    darr_t *a = (darr_t *)self;
    if (i >= 32) return 0;
    if (!a->it[i] && a->elem) a->it[i] = d_autoobj(a->elem);
    return a->it[i];
}
static void da_set(id self, SEL c, id o, u64 i) {
    (void)c;
    darr_t *a = (darr_t *)self;
    if (i >= 32) return;
    id n = o ? MSG0(id, o, "copy") : 0;
    if (a->it[i]) objc_release(a->it[i]);
    a->it[i] = n;
}
static void da_dealloc(id self, SEL c) {
    (void)c;
    darr_t *a = (darr_t *)self;
    for (int i = 0; i < 32; i++) if (a->it[i]) objc_release(a->it[i]);
    super_dealloc(self, cx_class_named("NSObject"));
}
static id da_copy(id self, SEL c) {
    (void)c;
    darr_t *a = (darr_t *)self, *n = (darr_t *)cx_alloc_instance(self->isa);
    n->elem = a->elem;
    for (int i = 0; i < 32; i++) if (a->it[i]) n->it[i] = MSG0(id, a->it[i], "copy");
    return (id)n;
}
static id da_copyz(id self, SEL c, void *z) { (void)z; return da_copy(self, c); }

static char *sel_buf(const char *pre, const char *name, const char *suf) {
    u64 n = strlen(pre) + strlen(name) + strlen(suf) + 1;
    char *s = malloc(n);
    strcpy(s, pre);
    u64 p = strlen(s);
    strcpy(s + p, name);
    if (*pre) s[p] = (char)toupper(s[p]);
    strcat(s, suf);
    return s;
}
void mtl_def_desc(cx_class_t *cls, cx_class_t *meta, const char *name, const pspec_t *ps, cx_class_t *super, void (*changed)(id, const char *)) {
    dclass_t *d = &g_dc[g_ndc++];
    if (!super) super = cx_class_named("NSObject");
    d->cls = cls;
    d->ps = ps;
    d->changed = changed;
    d->base = (cx_instance_size(super) + 7) & ~7u;
    cx_class_t *rs = super;
    for (int again = 1; again && rs;) {
        again = 0;
        for (int i = 0; i < g_ndc - 1; i++) if (g_dc[i].cls == rs) { rs = rs->super; again = 1; break; }
    }
    d->real_super = rs;
    int slots = 0, np = 0;
    for (; ps && ps[np].name; np++) {
        d->off[np] = slots;
        char t = ps[np].ty;
        slots += t == 'c' ? 4 : t == 'p' ? 2 : t == 't' ? 16 : 1;
    }
    d->np = np;
    d->nslots = slots;
    cx_define_class(cls, meta, name, super, (u32)((slots + 1) * 8 + (d->base - cx_instance_size(super))));
    for (int i = 0; i < np; i++) {
        const pspec_t *p = &ps[i];
        char t = p->ty;
        void *g = t == 'f' ? (void *)dg_f : t == 'd' ? (void *)dg_d : t == 'c' ? (void *)dg_c : t == 'p' ? (void *)dg_p : t == 't' ? (void *)dg_t : (void *)dg_u;
        void *s = t == 'f' ? (void *)ds_f : t == 'd' ? (void *)ds_d : t == 'c' ? (void *)ds_c : t == 'p' ? (void *)ds_p : t == 't' ? (void *)ds_t : (void *)ds_u;
        cx_add_method(cls, p->name, g);
        d->get[i] = sel_registerName(p->name);
        if (t == 'b') {
            char *is = sel_buf("is", p->name, "");
            cx_add_method(cls, is, g);
            d->isget[i] = sel_registerName(is);
        }
        if (t != 'A') {
            char *st = sel_buf("set", p->name, ":");
            cx_add_method(cls, st, s);
            d->set[i] = sel_registerName(st);
        }
    }
    cx_add_method(cls, "init", (void *)d_init);
    cx_add_method(cls, "dealloc", (void *)d_dealloc);
    cx_add_method(cls, "copy", (void *)d_copy);
    cx_add_method(cls, "copyWithZone:", (void *)d_copyz);
    cx_add_method(cls, "reset", (void *)d_reset);
}
static void def_desc(cx_class_t *cls, cx_class_t *meta, const char *name, const pspec_t *ps) { mtl_def_desc(cls, meta, name, ps, 0, 0); }
static void def_darr(cx_class_t *cls, cx_class_t *meta, const char *name, cx_class_t *elem) {
    dclass_t *d = &g_dc[g_ndc++];
    d->cls = cls;
    d->elem = elem;
    cx_define_class(cls, meta, name, cx_class_named("NSObject"), (u32)(sizeof(darr_t) - 8));
    cx_add_method(cls, "objectAtIndexedSubscript:", (void *)da_get);
    cx_add_method(cls, "setObject:atIndexedSubscript:", (void *)da_set);
    cx_add_method(cls, "dealloc", (void *)da_dealloc);
    cx_add_method(cls, "copy", (void *)da_copy);
    cx_add_method(cls, "copyWithZone:", (void *)da_copyz);
}
/* leitura dos descritores pelo runtime (também usada pelo coreanimation.c) */
u64 mtl_dget(id o, const char *name) { dclass_t *d = dc_of(o); int i = d ? dprop_name(d, name) : -1; return i >= 0 ? *dslot(o, d, i) : 0; }
double mtl_dgetd(id o, const char *name) { dclass_t *d = dc_of(o); int i = d ? dprop_name(d, name) : -1; double v = 0; if (i >= 0) memcpy(&v, dslot(o, d, i), 8); return v; }
float mtl_dgetf(id o, const char *name) { dclass_t *d = dc_of(o); int i = d ? dprop_name(d, name) : -1; float v = 0; if (i >= 0) memcpy(&v, dslot(o, d, i), 4); return v; }
void mtl_dgetv(id o, const char *name, double *out, int n) { dclass_t *d = dc_of(o); int i = d ? dprop_name(d, name) : -1; if (i >= 0) memcpy(out, dslot(o, d, i), (u64)n * 8); }
void mtl_dsetv(id o, const char *name, const double *in, int n) { dclass_t *d = dc_of(o); int i = d ? dprop_name(d, name) : -1; if (i >= 0) memcpy(dslot(o, d, i), in, (u64)n * 8); }
void mtl_dseto(id o, const char *name, id v) {
    dclass_t *d = dc_of(o);
    int i = d ? dprop_name(d, name) : -1;
    if (i < 0) return;
    u64 *s = dslot(o, d, i);
    if (v) objc_retain(v);
    if (*s) objc_release((id)*s);
    *s = (u64)v;
}
#define dget mtl_dget
#define dgetd mtl_dgetd
#define dgetf mtl_dgetf
static mtl_cc_t dgetc(id o, const char *name) { dclass_t *d = dc_of(o); int i = d ? dprop_name(d, name) : -1; mtl_cc_t v = {0, 0, 0, 1}; if (i >= 0) memcpy(&v, dslot(o, d, i), sizeof v); return v; }
static id darr_at(id o, const char *name, int idx) {
    darr_t *a = (darr_t *)dget(o, name);
    return a && idx >= 0 && idx < 32 ? a->it[idx] : 0;
}
static int is_desc(id o, cx_class_t *c) { return o && cx_is_subclass(o->isa, c); }

/* ---- listas de propriedades */
#define P_END {0, 0, 0, 0, 0}
static const pspec_t ps_texdesc[] = {
    {"textureType", 'u', 2}, {"pixelFormat", 'u', 80}, {"width", 'u', 1}, {"height", 'u', 1}, {"depth", 'u', 1},
    {"mipmapLevelCount", 'u', 1}, {"sampleCount", 'u', 1}, {"arrayLength", 'u', 1}, {"resourceOptions", 'u', 0},
    {"cpuCacheMode", 'u', 0}, {"storageMode", 'u', 0}, {"hazardTrackingMode", 'u', 0}, {"usage", 'u', 1},
    {"allowGPUOptimizedContents", 'b', 1}, {"compressionType", 'u', 0}, {"swizzle", 'u', 0x05040302}, {"placementSparsePageSize", 'u', 0}, P_END};
static const pspec_t ps_sampdesc[] = {
    {"minFilter", 'u'}, {"magFilter", 'u'}, {"mipFilter", 'u'}, {"maxAnisotropy", 'u', 1}, {"sAddressMode", 'u'}, {"tAddressMode", 'u'},
    {"rAddressMode", 'u'}, {"borderColor", 'u'}, {"normalizedCoordinates", 'b', 1}, {"lodMinClamp", 'f', 0, 0},
    {"lodMaxClamp", 'f', 0, 3.402823466e+38}, {"lodAverage", 'b'}, {"lodBias", 'f'}, {"compareFunction", 'u', 0},
    {"supportArgumentBuffers", 'b'}, {"reductionMode", 'u'}, {"label", 'o'}, P_END};
static const pspec_t ps_stencil[] = {
    {"stencilCompareFunction", 'u', 7}, {"stencilFailureOperation", 'u'}, {"depthFailureOperation", 'u'},
    {"depthStencilPassOperation", 'u'}, {"readMask", 'u', 0xffffffff}, {"writeMask", 'u', 0xffffffff}, P_END};
static pspec_t ps_dssdesc[] = {
    {"depthCompareFunction", 'u', 7}, {"depthWriteEnabled", 'b'}, {"frontFaceStencil", 'O'}, {"backFaceStencil", 'O'}, {"label", 'o'}, P_END};
#define ATT_BASE {"texture", 'o'}, {"level", 'u'}, {"slice", 'u'}, {"depthPlane", 'u'}, {"resolveTexture", 'o'}, \
    {"resolveLevel", 'u'}, {"resolveSlice", 'u'}, {"resolveDepthPlane", 'u'}, {"loadAction", 'u'}, {"storeAction", 'u', 1}, \
    {"storeActionOptions", 'u'}
static const pspec_t ps_att[] = {ATT_BASE, P_END};
static const pspec_t ps_catt[] = {ATT_BASE, {"clearColor", 'c'}, P_END};
static const pspec_t ps_datt[] = {ATT_BASE, {"clearDepth", 'd', 0, 1.0}, {"depthResolveFilter", 'u'}, P_END};
static const pspec_t ps_satt[] = {ATT_BASE, {"clearStencil", 'u'}, {"stencilResolveFilter", 'u'}, P_END};
static pspec_t ps_rpdesc[] = {
    {"colorAttachments", 'A'}, {"depthAttachment", 'O'}, {"stencilAttachment", 'O'}, {"visibilityResultBuffer", 'o'},
    {"renderTargetArrayLength", 'u'}, {"imageblockSampleLength", 'u'}, {"threadgroupMemoryLength", 'u'}, {"tileWidth", 'u'},
    {"tileHeight", 'u'}, {"defaultRasterSampleCount", 'u'}, {"renderTargetWidth", 'u'}, {"renderTargetHeight", 'u'},
    {"sampleBufferAttachments", 'A'}, {"rasterizationRateMap", 'o'}, {"supportColorAttachmentMapping", 'b'}, P_END};
static const pspec_t ps_rpca[] = {
    {"pixelFormat", 'u'}, {"blendingEnabled", 'b'}, {"sourceRGBBlendFactor", 'u', 1}, {"destinationRGBBlendFactor", 'u', 0},
    {"rgbBlendOperation", 'u'}, {"sourceAlphaBlendFactor", 'u', 1}, {"destinationAlphaBlendFactor", 'u', 0},
    {"alphaBlendOperation", 'u'}, {"writeMask", 'u', 0xF}, P_END};
static pspec_t ps_rpipedesc[] = {
    {"label", 'o'}, {"vertexFunction", 'o'}, {"fragmentFunction", 'o'}, {"vertexDescriptor", 'o'}, {"sampleCount", 'u', 1},
    {"rasterSampleCount", 'u', 1}, {"alphaToCoverageEnabled", 'b'}, {"alphaToOneEnabled", 'b'}, {"rasterizationEnabled", 'b', 1},
    {"maxVertexAmplificationCount", 'u', 1}, {"colorAttachments", 'A'}, {"depthAttachmentPixelFormat", 'u'},
    {"stencilAttachmentPixelFormat", 'u'}, {"inputPrimitiveTopology", 'u'}, {"supportIndirectCommandBuffers", 'b'},
    {"vertexBuffers", 'A'}, {"fragmentBuffers", 'A'}, {"binaryArchives", 'o'}, {"vertexPreloadedLibraries", 'o'},
    {"fragmentPreloadedLibraries", 'o'}, {"supportAddingVertexBinaryFunctions", 'b'}, {"supportAddingFragmentBinaryFunctions", 'b'},
    {"maxVertexCallStackDepth", 'u', 1}, {"maxFragmentCallStackDepth", 'u', 1}, {"vertexLinkedFunctions", 'O'},
    {"fragmentLinkedFunctions", 'O'}, {"shaderValidation", 'u'}, {"tessellationPartitionMode", 'u'},
    {"maxTessellationFactor", 'u', 16}, {"tessellationFactorScaleEnabled", 'b'}, {"tessellationFactorFormat", 'u'},
    {"tessellationControlPointIndexType", 'u'}, {"tessellationFactorStepFunction", 'u'}, {"tessellationOutputWindingOrder", 'u'}, P_END};
static pspec_t ps_vdesc[] = {{"attributes", 'A'}, {"layouts", 'A'}, P_END};
static const pspec_t ps_vattr[] = {{"format", 'u'}, {"offset", 'u'}, {"bufferIndex", 'u'}, P_END};
static const pspec_t ps_vlayout[] = {{"stride", 'u'}, {"stepFunction", 'u', 1}, {"stepRate", 'u', 1}, P_END};
static pspec_t ps_cpipedesc[] = {
    {"label", 'o'}, {"computeFunction", 'o'}, {"threadGroupSizeIsMultipleOfThreadExecutionWidth", 'b'},
    {"maxTotalThreadsPerThreadgroup", 'u'}, {"buffers", 'A'}, {"stageInputDescriptor", 'o'}, {"supportIndirectCommandBuffers", 'b'},
    {"binaryArchives", 'o'}, {"preloadedLibraries", 'o'}, {"linkedFunctions", 'O'}, {"supportAddingBinaryFunctions", 'b'},
    {"maxCallStackDepth", 'u', 1}, {"shaderValidation", 'u'}, P_END};
static const pspec_t ps_pbuf[] = {{"mutability", 'u'}, P_END};
static const pspec_t ps_copts[] = {
    {"fastMathEnabled", 'b', 1}, {"mathMode", 'u', 2}, {"mathFloatingPointFunctions", 'u'}, {"languageVersion", 'u', 0x30001},
    {"preprocessorMacros", 'o'}, {"libraryType", 'u'}, {"installName", 'o'}, {"libraries", 'o'}, {"preserveInvariance", 'b'},
    {"optimizationLevel", 'u'}, {"compileSymbolVisibility", 'u'}, {"allowReferencingUndefinedSymbols", 'b'},
    {"maxTotalThreadsPerThreadgroup", 'u'}, {"enableLogging", 'b'}, P_END};
static const pspec_t ps_heapdesc[] = {
    {"size", 'u'}, {"storageMode", 'u', 2}, {"cpuCacheMode", 'u'}, {"hazardTrackingMode", 'u'}, {"resourceOptions", 'u'},
    {"type", 'u'}, {"sparsePageSize", 'u'}, P_END};
static const pspec_t ps_cbdesc[] = {{"retainedReferences", 'b', 1}, {"errorOptions", 'u'}, {"logState", 'o'}, P_END};
static pspec_t ps_cpass[] = {{"dispatchType", 'u'}, {"sampleBufferAttachments", 'A'}, P_END};
static pspec_t ps_bpass[] = {{"sampleBufferAttachments", 'A'}, P_END};
static const pspec_t ps_capdesc[] = {{"captureObject", 'o'}, {"destination", 'u', 1}, {"outputURL", 'o'}, P_END};
static const pspec_t ps_fdesc[] = {{"name", 'o'}, {"specializedName", 'o'}, {"constantValues", 'o'}, {"options", 'u'}, {"binaryArchives", 'o'}, P_END};
static const pspec_t ps_argdesc[] = {{"dataType", 'u'}, {"index", 'u'}, {"arrayLength", 'u'}, {"access", 'u'}, {"textureType", 'u'}, {"constantBlockAlignment", 'u'}, P_END};
static const pspec_t ps_icbdesc[] = {{"commandTypes", 'u'}, {"inheritPipelineState", 'b'}, {"inheritBuffers", 'b'}, {"maxVertexBufferBindCount", 'u'},
                                     {"maxFragmentBufferBindCount", 'u'}, {"maxKernelBufferBindCount", 'u'}, P_END};
static pspec_t ps_siodesc[] = {{"attributes", 'A'}, {"layouts", 'A'}, {"indexType", 'u'}, {"indexBufferIndex", 'u'}, P_END};
static const pspec_t ps_sbatt[] = {{"sampleBuffer", 'o'}, {"startOfVertexSampleIndex", 'u'}, {"endOfVertexSampleIndex", 'u'},
                                   {"startOfFragmentSampleIndex", 'u'}, {"endOfFragmentSampleIndex", 'u'}, {"startOfEncoderSampleIndex", 'u'},
                                   {"endOfEncoderSampleIndex", 'u'}, P_END};
static const pspec_t ps_csbdesc[] = {{"counterSet", 'o'}, {"label", 'o'}, {"storageMode", 'u'}, {"sampleCount", 'u'}, P_END};
static const pspec_t ps_linked[] = {{"functions", 'o'}, {"binaryFunctions", 'o'}, {"groups", 'o'}, {"privateFunctions", 'o'}, P_END};
static const pspec_t ps_badesc[] = {{"url", 'o'}, P_END};
static const pspec_t ps_rrmdesc[] = {{"label", 'o'}, {"layerCount", 'u'}, P_END};
static const pspec_t ps_rsdesc[] = {{"label", 'o'}, {"initialCapacity", 'u'}, P_END};
static pspec_t ps_tiledesc[] = {{"label", 'o'}, {"tileFunction", 'o'}, {"rasterSampleCount", 'u', 1}, {"colorAttachments", 'A'},
                                {"threadgroupSizeMatchesTileSize", 'b'}, {"maxTotalThreadsPerThreadgroup", 'u'}, P_END};
static const pspec_t ps_tileca[] = {{"pixelFormat", 'u'}, P_END};
static const pspec_t ps_none[] = {P_END};

/* construtores de classe dos descritores */
#define TDS(o, n, v) (*dslot((o), dc_of(o), dprop_name(dc_of(o), (n))) = (u64)(v))
static id td_2d(id cls, SEL c, u64 fmt, u64 w, u64 h, BOOL mip) {
    (void)c;
    id d = d_autoobj((cx_class_t *)cls);
    TDS(d, "pixelFormat", fmt);
    TDS(d, "width", w);
    TDS(d, "height", h);
    if (mip) { u64 m = w > h ? w : h, n = 1; while (m > 1) { m >>= 1; n++; } TDS(d, "mipmapLevelCount", n); }
    return objc_autorelease(d);
}
static id td_cube(id cls, SEL c, u64 fmt, u64 size, BOOL mip) {
    id d = td_2d(cls, c, fmt, size, size, mip);
    TDS(d, "textureType", MSL_TEX_CUBE);
    return d;
}
static id td_buffer(id cls, SEL c, u64 fmt, u64 w, u64 opts, u64 usage) {
    id d = td_2d(cls, c, fmt, w, 1, NO);
    TDS(d, "textureType", MSL_TEX_BUFFER);
    TDS(d, "resourceOptions", opts);
    TDS(d, "usage", usage);
    return d;
}
static id dnew_ar(id cls, SEL c) { (void)c; return objc_autorelease(d_autoobj((cx_class_t *)cls)); }
static id rp_desc(id cls, SEL c) { return dnew_ar(cls, c); }
static void fcv_noop(id s, SEL c, const void *v, u64 t, u64 i) { (void)s; (void)c; (void)v; (void)t; (void)i; }

/* ================================================================ device */
typedef struct { MHDR; id name; } mdev_t;
static id dv_name(id s, SEL c) { (void)c; return ((mdev_t *)s)->name; }
static mtl_size_t dv_maxtpt(id s, SEL c) { (void)s; (void)c; mtl_size_t r = {1024, 1024, 64}; return r; }
static u64 dv_maxtgmem(id s, SEL c) { (void)s; (void)c; return 32768; }
static u64 dv_maxbuf(id s, SEL c) { (void)s; (void)c; return 1ull << 32; }
static u64 dv_wset(id s, SEL c) { (void)s; (void)c; return 1ull << 30; }
static u64 g_allocated;
static u64 dv_alloc(id s, SEL c) { (void)s; (void)c; return g_allocated; }
static u64 dv_regid(id s, SEL c) { (void)s; (void)c; return 0x6d4f53ull; }
static BOOL dv_family(id s, SEL c, i64 f) {
    (void)s; (void)c;
    /* Common1-3 e Mac2: o que o Metal por software sustenta */
    return f == 3001 || f == 3002 || f == 3003 || f == 2001 || f == 2002 || f == 1001;
}
static BOOL dv_fset(id s, SEL c, u64 f) { (void)s; (void)c; return f >= 10000 && f <= 10005; }
static BOOL dv_samples(id s, SEL c, u64 n) { (void)s; (void)c; return n == 1 || n == 4; }
static u64 dv_lin_align(id s, SEL c, u64 f) { (void)s; (void)c; (void)f; return 16; }
static u64 dv_tier(id s, SEL c) { (void)s; (void)c; return 0; }
static void dv_sample_ts(id s, SEL c, u64 *cpu, u64 *gpu) { (void)s; (void)c; u64 t = mach_absolute_time(); if (cpu) *cpu = t; if (gpu) *gpu = t; }
static id dv_arch(id s, SEL c) { (void)s; (void)c; return objc_autorelease(new_obj(&MArch_c)); }
static id ar_name(id s, SEL c) { (void)s; (void)c; return objc_autorelease(cx_str("mos-soft")); }

/* ---- buffers */
static mbuf_t *buf_new(u64 len, u64 opts, const void *init) {
    mbuf_t *b = (mbuf_t *)new_obj(&MBuf_c);
    u64 n = len ? len : 1;
    b->p = calloc(1, (n + 15) & ~15ull);
    if (!b->p) { objc_release((id)b); return 0; }
    if (init) memcpy(b->p, init, len);
    b->len = len;
    b->opts = opts;
    b->own = 1;
    g_allocated += n;
    return b;
}
static id dv_newbuf(id s, SEL c, u64 len, u64 opts) { (void)s; (void)c; return (id)buf_new(len, opts, 0); }
static id dv_newbuf_bytes(id s, SEL c, const void *p, u64 len, u64 opts) { (void)s; (void)c; return (id)buf_new(len, opts, p); }
static id dv_newbuf_nocopy(id s, SEL c, void *p, u64 len, u64 opts, void *blk) {
    (void)s; (void)c;
    mbuf_t *b = (mbuf_t *)new_obj(&MBuf_c);
    b->p = p;
    b->len = len;
    b->opts = opts;
    b->dealloc_blk = blk ? _Block_copy(blk) : 0;
    return (id)b;
}
static void buf_dealloc(id s, SEL c) {
    mbuf_t *b = (mbuf_t *)s;
    if (b->own) { free(b->p); g_allocated -= b->len ? b->len : 1; }
    else if (b->dealloc_blk) ((void (*)(void *, void *, u64))((blk_t *)b->dealloc_blk)->invoke)(b->dealloc_blk, b->p, b->len);
    if (b->heap) objc_release(b->heap);
    o_dealloc(s, c);
}
static void *bf_contents(id s, SEL c) { (void)c; return ((mbuf_t *)s)->p; }
static u64 bf_length(id s, SEL c) { (void)c; return ((mbuf_t *)s)->len; }
static u64 bf_gpuaddr(id s, SEL c) { (void)c; return (u64)((mbuf_t *)s)->p; }
static u64 bf_opts(id s, SEL c) { (void)c; return ((mbuf_t *)s)->opts; }
static u64 bf_storage(id s, SEL c) { (void)c; return (((mbuf_t *)s)->opts >> 4) & 0xf; }
static u64 bf_cache(id s, SEL c) { (void)c; return ((mbuf_t *)s)->opts & 0xf; }
static u64 bf_hazard(id s, SEL c) { (void)c; return (((mbuf_t *)s)->opts >> 8) & 3; }
static id bf_heap(id s, SEL c) { (void)c; return ((mbuf_t *)s)->heap; }
static u64 bf_purge(id s, SEL c, u64 st) { (void)s; (void)c; (void)st; return 2; /* NonVolatile */ }
static id bf_newtex(id s, SEL c, id desc, u64 off, u64 bpr);

/* ---- texturas */
static int tex_from_desc(mtex_t *t, id desc) {
    t->fmt = (int)dget(desc, "pixelFormat");
    t->type = (int)dget(desc, "textureType");
    t->w = dget(desc, "width");
    t->h = dget(desc, "height");
    t->d = dget(desc, "depth");
    t->levels = (int)dget(desc, "mipmapLevelCount");
    t->arrlen = (int)dget(desc, "arrayLength");
    t->samples = (int)dget(desc, "sampleCount");
    t->usage = (int)dget(desc, "usage");
    t->storage = (int)dget(desc, "storageMode");
    t->opts = dget(desc, "resourceOptions");
    t->swizzle = (u32)dget(desc, "swizzle");
    if (!t->w) t->w = 1;
    if (!t->h || t->type == MSL_TEX_1D || t->type == MSL_TEX_1D_ARRAY || t->type == MSL_TEX_BUFFER) t->h = t->type == MSL_TEX_BUFFER || t->type <= MSL_TEX_1D_ARRAY ? 1 : (t->h ? t->h : 1);
    if (!t->d || t->type != MSL_TEX_3D) t->d = 1;
    if (t->type == MSL_TEX_CUBE || t->type == MSL_TEX_CUBE_ARRAY) t->h = t->w;
    if (t->levels < 1) t->levels = 1;
    if (t->levels > 16) t->levels = 16;
    if (t->arrlen < 1 || (t->type != MSL_TEX_1D_ARRAY && t->type != MSL_TEX_2D_ARRAY && t->type != MSL_TEX_CUBE_ARRAY && t->type != MSL_TEX_2D_MS_ARRAY)) t->arrlen = 1;
    if (!mx_bpp(t->fmt)) return 0;
    return 1;
}
static id tex_new(id desc) {
    if (!desc) return 0;
    mtex_t *t = (mtex_t *)new_obj(&MTex_c);
    if (!tex_from_desc(t, desc)) {
        cx_fdprintf(2, "Metal: formato de pixel %d não suportado\n", t->fmt);
        objc_release((id)t);
        return 0;
    }
    mx_tex_layout(t, 0);
    u64 n = mx_tex_bytes(t);
    t->alloc = calloc(1, n + 16);
    if (!t->alloc) { objc_release((id)t); return 0; }
    t->t.data = t->alloc;
    g_allocated += n;
    return (id)t;
}
static id dv_newtex(id s, SEL c, id desc) { (void)s; (void)c; return tex_new(desc); }
static id dv_newtex_surf(id s, SEL c, id desc, id surf, u64 plane) {
    (void)s; (void)c;
    if (!desc || !surf) return 0;
    mtex_t *t = (mtex_t *)new_obj(&MTex_c);
    if (!tex_from_desc(t, desc)) { objc_release((id)t); return 0; }
    t->levels = 1;
    t->surf = objc_retain(surf);
    t->plane = (int)plane;
    u64 bpr = plane ? IOSurfaceGetBytesPerRowOfPlane(surf, plane) : IOSurfaceGetBytesPerRow(surf);
    mx_tex_layout(t, bpr);
    t->t.data = plane ? IOSurfaceGetBaseAddressOfPlane(surf, plane) : IOSurfaceGetBaseAddress(surf);
    return (id)t;
}
static id bf_newtex(id s, SEL c, id desc, u64 off, u64 bpr) {
    (void)c;
    mbuf_t *b = (mbuf_t *)s;
    mtex_t *t = (mtex_t *)new_obj(&MTex_c);
    if (!tex_from_desc(t, desc)) { objc_release((id)t); return 0; }
    t->levels = 1;
    t->buffer = objc_retain(s);
    t->boff = off;
    t->bbpr = bpr;
    mx_tex_layout(t, bpr);
    t->t.data = b->p + off;
    return (id)t;
}
static void tex_dealloc(id s, SEL c) {
    mtex_t *t = (mtex_t *)s;
    if (t->alloc) { free(t->alloc); g_allocated -= mx_tex_bytes(t); }
    if (t->surf) objc_release(t->surf);
    if (t->buffer) objc_release(t->buffer);
    if (t->parent) objc_release(t->parent);
    if (t->heap) objc_release(t->heap);
    o_dealloc(s, c);
}
#define TGET(fn, expr) static u64 fn(id s, SEL c) { (void)c; mtex_t *t = (mtex_t *)s; return (u64)(expr); }
TGET(tx_w, t->w) TGET(tx_h, t->h) TGET(tx_d, t->d) TGET(tx_fmt, t->fmt) TGET(tx_type, t->type) TGET(tx_levels, t->levels)
TGET(tx_arr, t->arrlen) TGET(tx_samples, t->samples ? t->samples : 1) TGET(tx_usage, t->usage) TGET(tx_storage, t->storage)
TGET(tx_iosurf, t->surf) TGET(tx_plane, t->plane) TGET(tx_buffer, t->buffer) TGET(tx_boff, t->boff) TGET(tx_bbpr, t->bbpr)
TGET(tx_parent, t->parent) TGET(tx_fbonly, t->fbonly) TGET(tx_swz, t->swizzle) TGET(tx_rid, (u64)t) TGET(tx_opts, t->opts)
TGET(tx_heap, t->heap)
static void tex_region_io(mtex_t *t, mtl_region_t r, u64 lvl, u64 slice, u8 *bytes, u64 bpr, u64 bpi, int write) {
    if (!t->t.data || lvl >= (u64)t->levels) return;
    int bpp = mx_bpp(t->fmt);
    u64 row = r.s.w * bpp;
    if (!bpr) bpr = row;
    if (!bpi) bpi = bpr * r.s.h;
    u64 lw = t->w >> lvl, lh = t->h >> lvl;
    if (!lw) lw = 1;
    if (!lh) lh = 1;
    if (r.o.x + r.s.w > lw || r.o.y + r.s.h > lh) return;
    u64 dd = r.s.d ? r.s.d : 1;
    for (u64 z = 0; z < dd; z++)
        for (u64 y = 0; y < r.s.h; y++) {
            int tz = t->type == MSL_TEX_3D ? (int)(r.o.z + z) : 0;
            int sl = t->type == MSL_TEX_3D ? (int)slice : (int)(slice + z);
            u8 *tp = mx_texel(&t->t, (int)r.o.x, (int)(r.o.y + y), tz, sl, (int)lvl);
            u8 *bp = bytes + z * bpi + y * bpr;
            if (write) memcpy(tp, bp, row); else memcpy(bp, tp, row);
        }
}
static void tx_replace(id s, SEL c, mtl_region_t r, u64 lvl, const void *b, u64 bpr) { (void)c; tex_region_io((mtex_t *)s, r, lvl, 0, (u8 *)b, bpr, 0, 1); }
static void tx_replace2(id s, SEL c, mtl_region_t r, u64 lvl, u64 slice, const void *b, u64 bpr, u64 bpi) { (void)c; tex_region_io((mtex_t *)s, r, lvl, slice, (u8 *)b, bpr, bpi, 1); }
static void tx_getbytes(id s, SEL c, void *b, u64 bpr, mtl_region_t r, u64 lvl) { (void)c; tex_region_io((mtex_t *)s, r, lvl, 0, b, bpr, 0, 0); }
static void tx_getbytes2(id s, SEL c, void *b, u64 bpr, u64 bpi, mtl_region_t r, u64 lvl, u64 slice) { (void)c; tex_region_io((mtex_t *)s, r, lvl, slice, b, bpr, bpi, 0); }
static id tex_view(mtex_t *p, int fmt, int type, int l0, int nl, int s0, int ns) {
    if (mx_bpp(fmt) != mx_bpp(p->fmt)) return 0;
    mtex_t *t = (mtex_t *)new_obj(&MTex_c);
    *t = *p;
    t->isa = &MTex_c;
    t->label = 0;
    t->alloc = 0;
    t->surf = p->surf ? objc_retain(p->surf) : 0;
    t->buffer = p->buffer ? objc_retain(p->buffer) : 0;
    t->heap = 0;
    t->parent = objc_retain((id)p);
    t->fmt = t->t.fmt = fmt;
    t->type = t->t.type = type;
    if (l0 < 0 || l0 >= p->levels) l0 = 0;
    if (nl < 1 || l0 + nl > p->levels) nl = p->levels - l0;
    if (s0 < 0 || s0 >= p->t.slices) s0 = 0;
    if (ns < 1 || s0 + ns > p->t.slices) ns = p->t.slices - s0;
    t->levels = t->t.levels = nl;
    t->w = p->w >> l0 ? p->w >> l0 : 1;
    t->h = p->h >> l0 ? p->h >> l0 : 1;
    t->d = p->d >> l0 ? p->d >> l0 : 1;
    t->t.w = (int)t->w;
    t->t.h = (int)t->h;
    t->t.d = (int)t->d;
    t->t.slices = ns;
    t->arrlen = type == MSL_TEX_CUBE || type == MSL_TEX_CUBE_ARRAY ? ns / 6 : ns;
    for (int i = 0; i < nl; i++) {
        t->t.loff[i] = p->t.loff[l0 + i] + (u64)s0 * p->t.lslice[l0 + i];
        t->t.lbpr[i] = p->t.lbpr[l0 + i];
        t->t.lslice[i] = p->t.lslice[l0 + i];
    }
    return (id)t;
}
static id tx_view(id s, SEL c, u64 fmt) { (void)c; mtex_t *p = (mtex_t *)s; return tex_view(p, (int)fmt, p->type, 0, p->levels, 0, p->t.slices); }
static id tx_view2(id s, SEL c, u64 fmt, u64 type, nsrange_t l, nsrange_t sl) { (void)c; return tex_view((mtex_t *)s, (int)fmt, (int)type, (int)l.loc, (int)l.len, (int)sl.loc, (int)sl.len); }
static id tx_view3(id s, SEL c, u64 fmt, u64 type, nsrange_t l, nsrange_t sl, u32 swz) { (void)swz; return tx_view2(s, c, fmt, type, l, sl); }

/* ---- samplers e depth/stencil */
static id dv_newsamp(id s, SEL c, id d) {
    (void)s; (void)c;
    msamp_t *m = (msamp_t *)new_obj(&MSamp_c);
    if (d) {
        m->s.minf = (int)dget(d, "minFilter");
        m->s.magf = (int)dget(d, "magFilter");
        m->s.mipf = (int)dget(d, "mipFilter");
        m->s.addr[0] = (int)dget(d, "sAddressMode");
        m->s.addr[1] = (int)dget(d, "tAddressMode");
        m->s.addr[2] = (int)dget(d, "rAddressMode");
        m->s.border = (int)dget(d, "borderColor");
        m->s.normalized = (int)dget(d, "normalizedCoordinates");
        m->s.compare = (int)dget(d, "compareFunction");
        m->s.lodmin = dgetf(d, "lodMinClamp");
        m->s.lodmax = dgetf(d, "lodMaxClamp");
        id l = (id)dget(d, "label");
        if (l) o_set_label((id)m, 0, l);
    } else { m->s.normalized = 1; m->s.lodmax = 3.4e38f; }
    return (id)m;
}
static void read_stencil(id sd, stencil_t *s) {
    s->cmp = 7; s->fail = s->dfail = s->pass = 0; s->rmask = s->wmask = 0xffffffff;
    if (!sd) return;
    s->cmp = (int)dget(sd, "stencilCompareFunction");
    s->fail = (int)dget(sd, "stencilFailureOperation");
    s->dfail = (int)dget(sd, "depthFailureOperation");
    s->pass = (int)dget(sd, "depthStencilPassOperation");
    s->rmask = (u32)dget(sd, "readMask");
    s->wmask = (u32)dget(sd, "writeMask");
}
static id dv_newdss(id s, SEL c, id d) {
    (void)s; (void)c;
    mdss_t *m = (mdss_t *)new_obj(&MDss_c);
    m->dcmp = 7;
    if (d) {
        m->dcmp = (int)dget(d, "depthCompareFunction");
        m->dwrite = (int)dget(d, "depthWriteEnabled");
        read_stencil((id)dget(d, "frontFaceStencil"), &m->front);
        read_stencil((id)dget(d, "backFaceStencil"), &m->back);
        stencil_t *f = &m->front, *b = &m->back;
        m->stencil_on = f->cmp != 7 || f->fail || f->dfail || f->pass || b->cmp != 7 || b->fail || b->dfail || b->pass;
        id l = (id)dget(d, "label");
        if (l) o_set_label((id)m, 0, l);
    } else { read_stencil(0, &m->front); read_stencil(0, &m->back); }
    return (id)m;
}

/* ---- bibliotecas e funções */
static char *g_incdir;
static char *read_file(const char *path, u64 *len) {
    int fd = open(path, 0);
    if (fd < 0) return 0;
    off_t n = lseek(fd, 0, 2);
    lseek(fd, 0, 0);
    if (n < 0) { close(fd); return 0; }
    char *b = malloc((u64)n + 1);
    if (!b) { close(fd); return 0; }
    ssize_t r = read(fd, b, (size_t)n);
    close(fd);
    if (r < 0) r = 0;
    b[r] = 0;
    if (len) *len = (u64)r;
    return b;
}
static char *inc_hook(const char *name, u64 *len) {
    if (!g_incdir) return 0;
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", g_incdir, name);
    return read_file(p, len);
}
static char *macros_str(id opts) {
    id d = opts && is_desc(opts, &MTLCompileOptions_c) ? (id)dget(opts, "preprocessorMacros") : 0;
    if (!d) return 0;
    id keys = MSG0(id, d, "allKeys");
    u64 n = MSG0(u64, keys, "count");
    u64 cap = 64, used = 0;
    char *s = malloc(cap);
    s[0] = 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id v = MSG(id, d, "objectForKey:", k);
        const char *ks = cx_cstr(MSG0(id, k, "description")), *vs = v ? cx_cstr(MSG0(id, v, "description")) : "1";
        u64 need = strlen(ks) + strlen(vs) + 3;
        if (used + need >= cap) { cap = (used + need) * 2; s = realloc(s, cap); }
        used += snprintf(s + used, cap - used, "%s=%s\n", ks, vs);
    }
    return s;
}
static id lib_from_source(const char *src, u64 n, id opts, id *err) {
    char e[1024];
    char *defs = macros_str(opts);
    msl_include_hook = inc_hook;
    gpu_lock();
    msl_lib_t *L = msl_compile(src, n, defs, e, sizeof e);
    gpu_unlock();
    free(defs);
    if (!L) { set_err(err, mk_error("MTLLibraryErrorDomain", 3, "erro de compilação do shader: %s", e)); return 0; }
    mlib_t *l = (mlib_t *)new_obj(&MLib_c);
    l->L = L;
    l->names = cx_arr_new(cx_class_named("NSMutableArray"));
    for (int i = 0; i < msl_fn_count(L); i++) {
        msl_fn_t *f = msl_fn_at(L, i);
        if (msl_fn_stage(f) != MSL_HELPER) { id s = cx_str(msl_fn_name(f)); cx_arr_add(l->names, s); objc_release(s); }
    }
    return (id)l;
}
static id dv_newlib_src(id s, SEL c, id src, id opts, id *err) {
    (void)s; (void)c;
    if (!src) { set_err(err, mk_error("MTLLibraryErrorDomain", 1, "fonte nulo")); return 0; }
    const char *p = cx_cstr(src);
    return lib_from_source(p, strlen(p), opts, err);
}
static void dv_newlib_src_blk(id s, SEL c, id src, id opts, void *blk) {
    id err = 0;
    id l = dv_newlib_src(s, c, src, opts, &err);
    if (blk) BLK2(blk, l, err);
    if (l) objc_release(l);
}
static int is_air(const char *p, u64 n) { return n >= 4 && p[0] == 'M' && p[1] == 'T' && p[2] == 'L' && p[3] == 'B'; }
static id lib_from_bytes(const char *p, u64 n, id *err) {
    if (is_air(p, n)) {
        set_err(err, mk_error("MTLLibraryErrorDomain", 2, "metallib (AIR) não é suportado pelo Metal por software do mOS; compile os shaders do fonte (.metal) com newLibraryWithSource:"));
        return 0;
    }
    return lib_from_source(p, n, 0, err);
}
static id dv_newlib_data(id s, SEL c, id data, id *err) {
    (void)s; (void)c;
    if (!data) return 0;
    const char *p = MSG0(const char *, data, "bytes");
    u64 n = MSG0(u64, data, "length");
    return lib_from_bytes(p, n, err);
}
static id dv_newlib_file(id s, SEL c, id path, id *err) {
    (void)s; (void)c;
    if (!path) return 0;
    u64 n = 0;
    char *b = read_file(cx_cstr(path), &n);
    if (!b) { set_err(err, mk_error("MTLLibraryErrorDomain", 6, "arquivo não encontrado: %s", cx_cstr(path))); return 0; }
    id l = lib_from_bytes(b, n, err);
    free(b);
    return l;
}
static id dv_newlib_url(id s, SEL c, id url, id *err) {
    id p = url ? MSG0(id, url, "path") : 0;
    return dv_newlib_file(s, c, p, err);
}
/* default.metallib: no mOS os .metal do bundle (Resources) são compilados do fonte */
static id lib_from_dir(const char *dir, id *err) {
    void *d = opendir(dir);
    if (!d) return 0;
    u64 cap = 4096, used = 0;
    char *all = malloc(cap);
    all[0] = 0;
    int found = 0, air = 0;
    void *e;
    while ((e = readdir(d))) {
        /* dirent do Darwin: d_ino(8) d_seekoff(8) d_reclen(2) d_namlen(2) d_type(1) d_name[] */
        const char *nm = (const char *)e + 21;
        u64 nl = strlen(nm);
        if (nl > 9 && !strcmp(nm + nl - 9, ".metallib")) air = 1;
        if (nl < 7 || strcmp(nm + nl - 6, ".metal")) continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, nm);
        u64 n = 0;
        char *b = read_file(p, &n);
        if (!b) continue;
        if (used + n + 2 >= cap) { cap = (used + n + 2) * 2; all = realloc(all, cap); }
        memcpy(all + used, b, n);
        used += n;
        all[used++] = '\n';
        all[used] = 0;
        free(b);
        found = 1;
    }
    closedir(d);
    id l = 0;
    if (found) {
        g_incdir = strdup(dir);
        l = lib_from_source(all, used, 0, err);
    } else if (air)
        set_err(err, mk_error("MTLLibraryErrorDomain", 2, "o bundle só tem default.metallib (AIR), que o Metal por software do mOS não executa; inclua os .metal em Resources"));
    free(all);
    return l;
}
static id dv_deflib_bundle(id s, SEL c, id bundle, id *err) {
    (void)s; (void)c;
    if (!bundle) bundle = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id rp = bundle ? MSG0(id, bundle, "resourcePath") : 0;
    id l = rp ? lib_from_dir(cx_cstr(rp), err) : 0;
    if (!l && rp && err && !*err) set_err(err, mk_error("MTLLibraryErrorDomain", 6, "nenhum .metal em %s", cx_cstr(rp)));
    return l;
}
static id dv_deflib(id s, SEL c) { return dv_deflib_bundle(s, c, 0, 0); }
static void lib_dealloc(id s, SEL c) {
    mlib_t *l = (mlib_t *)s;
    if (l->names) objc_release(l->names);
    /* as funções retêm a lib: quando ela morre, nenhuma função a usa */
    msl_lib_free(l->L);
    o_dealloc(s, c);
}
static id lb_names(id s, SEL c) { (void)c; return ((mlib_t *)s)->names; }
static id lb_newfn(id s, SEL c, id name) {
    (void)c;
    mlib_t *l = (mlib_t *)s;
    if (!name) return 0;
    msl_fn_t *f = msl_find(l->L, cx_cstr(name));
    if (!f) return 0;
    mfunc_t *fn = (mfunc_t *)new_obj(&MFunc_c);
    fn->lib = objc_retain(s);
    fn->fn = f;
    fn->name = MSG0(id, name, "copy");
    return (id)fn;
}
static id lb_newfn_cv(id s, SEL c, id name, id cv, id *err) {
    id f = lb_newfn(s, c, name);
    if (!f) set_err(err, mk_error("MTLLibraryErrorDomain", 5, "função '%s' não encontrada", name ? cx_cstr(name) : "?"));
    (void)cv;
    return f;
}
static void lb_newfn_cv_blk(id s, SEL c, id name, id cv, void *blk) {
    id err = 0;
    id f = lb_newfn_cv(s, c, name, cv, &err);
    if (blk) BLK2(blk, f, err);
    if (f) objc_release(f);
}
static id lb_newfn_desc(id s, SEL c, id d, id *err) { return lb_newfn_cv(s, c, d ? (id)dget(d, "name") : 0, 0, err); }
static u64 lb_type(id s, SEL c) { (void)s; (void)c; return 0; }
static void fn_dealloc(id s, SEL c) {
    mfunc_t *f = (mfunc_t *)s;
    if (f->name) objc_release(f->name);
    if (f->lib) objc_release(f->lib);
    o_dealloc(s, c);
}
static id fn_name(id s, SEL c) { (void)c; return ((mfunc_t *)s)->name; }
static u64 fn_type(id s, SEL c) { (void)c; int st = msl_fn_stage(((mfunc_t *)s)->fn); return st ? (u64)st : 5; }
static i64 fn_cpcount(id s, SEL c) { (void)s; (void)c; return -1; }

/* ---- pipelines */
static const char *fkey(const msl_field_t *f) { return f->user ? f->user : f->name; }
static id make_rpipe(id d, id *err) {
    mfunc_t *vf = (mfunc_t *)dget(d, "vertexFunction"), *ff = (mfunc_t *)dget(d, "fragmentFunction");
    if (!vf || msl_fn_stage(vf->fn) != MSL_VERTEX) { set_err(err, mk_error("MTLPipelineErrorDomain", 1, "vertexFunction ausente ou não é uma função vertex")); return 0; }
    if (ff && msl_fn_stage(ff->fn) != MSL_FRAGMENT) { set_err(err, mk_error("MTLPipelineErrorDomain", 1, "fragmentFunction não é uma função fragment")); return 0; }
    mrpipe_t *p = (mrpipe_t *)new_obj(&MRPipe_c);
    p->vfunc = objc_retain((id)vf);
    p->ffunc = ff ? objc_retain((id)ff) : 0;
    p->vfn = vf->fn;
    p->ffn = ff ? ff->fn : 0;
    p->dfmt = (int)dget(d, "depthAttachmentPixelFormat");
    p->sfmt = (int)dget(d, "stencilAttachmentPixelFormat");
    p->raster = (int)dget(d, "rasterizationEnabled");
    p->samples = (int)dget(d, "rasterSampleCount");
    p->a2c = (int)dget(d, "alphaToCoverageEnabled");
    for (int i = 0; i < 8; i++) {
        id ca = darr_at(d, "colorAttachments", i);
        cattach_t *a = &p->ca[i];
        if (!ca) { a->fmt = 0; a->srgb = 1; a->sa = 1; a->wmask = 0xF; continue; }
        a->fmt = (int)dget(ca, "pixelFormat");
        a->blend = (int)dget(ca, "blendingEnabled");
        a->srgb = (int)dget(ca, "sourceRGBBlendFactor");
        a->drgb = (int)dget(ca, "destinationRGBBlendFactor");
        a->rgbop = (int)dget(ca, "rgbBlendOperation");
        a->sa = (int)dget(ca, "sourceAlphaBlendFactor");
        a->da = (int)dget(ca, "destinationAlphaBlendFactor");
        a->aop = (int)dget(ca, "alphaBlendOperation");
        a->wmask = (u32)dget(ca, "writeMask");
    }
    id vd = (id)dget(d, "vertexDescriptor");
    if (vd) {
        p->has_vd = 1;
        for (int i = 0; i < 31; i++) {
            id a = darr_at(vd, "attributes", i), l = darr_at(vd, "layouts", i);
            if (a) { p->attr[i].fmt = (int)dget(a, "format"); p->attr[i].off = dget(a, "offset"); p->attr[i].buf = (int)dget(a, "bufferIndex"); }
            if (l) { p->lay[i].stride = dget(l, "stride"); p->lay[i].step = (int)dget(l, "stepFunction"); p->lay[i].rate = dget(l, "stepRate"); }
            else { p->lay[i].step = 1; p->lay[i].rate = 1; }
        }
    }
    p->nvout = msl_fn_out(p->vfn, &p->vout, &p->vout_size);
    p->nvin = msl_fn_stagein(p->vfn, &p->vin, &p->vin_size);
    p->pos_field = p->psize_field = -1;
    for (int i = 0; i < p->nvout; i++) {
        if (p->vout[i].attr == A_POSITION) p->pos_field = i;
        if (p->vout[i].attr == A_POINT_SIZE) p->psize_field = i;
    }
    if (p->raster && p->pos_field < 0) { objc_release((id)p); set_err(err, mk_error("MTLPipelineErrorDomain", 1, "a função vertex não devolve [[position]]")); return 0; }
    if (p->nvin && !vd) { objc_release((id)p); set_err(err, mk_error("MTLPipelineErrorDomain", 1, "a função vertex usa [[stage_in]] e o pipeline não tem vertexDescriptor")); return 0; }
    for (int i = 0; i < 8; i++) p->fcolor[i] = -1;
    p->fdepth = -1;
    if (p->ffn) {
        p->nfin = msl_fn_stagein(p->ffn, &p->fin, &p->fin_size);
        p->nfout = msl_fn_out(p->ffn, &p->fout, &p->fout_size);
        if (p->nfin > 64) p->nfin = 64;
        for (int i = 0; i < p->nfin; i++) {
            const msl_field_t *fi = &p->fin[i];
            p->fmap[i] = -1;
            p->finterp[i] = fi->interp;
            if (fi->attr == A_POSITION) { p->fmap[i] = -2; continue; }
            for (int j = 0; j < p->nvout; j++)
                if (!strcmp(fkey(fi), fkey(&p->vout[j]))) {
                    p->fmap[i] = j;
                    int vi = p->vout[j].interp;
                    if (vi == MSL_INTERP_FLAT || fi->interp == MSL_INTERP_FLAT) p->finterp[i] = MSL_INTERP_FLAT;
                    else if (vi == MSL_INTERP_NOPERSP) p->finterp[i] = MSL_INTERP_NOPERSP;
                    break;
                }
        }
        for (int i = 0; i < p->nfout; i++) {
            if (p->fout[i].attr == A_COLOR && p->fout[i].idx >= 0 && p->fout[i].idx < 8) p->fcolor[p->fout[i].idx] = i;
            if (p->fout[i].attr == A_DEPTH) p->fdepth = i;
        }
        p->discards = msl_fn_discards(p->ffn);
    }
    id l = (id)dget(d, "label");
    if (l) o_set_label((id)p, 0, l);
    return (id)p;
}
static id dv_newrp(id s, SEL c, id d, id *err) { (void)s; (void)c; return d ? make_rpipe(d, err) : 0; }
static id dv_newrp_refl(id s, SEL c, id d, u64 opts, id *refl, id *err) { (void)opts; if (refl) *refl = 0; return dv_newrp(s, c, d, err); }
static void dv_newrp_blk(id s, SEL c, id d, void *blk) { id err = 0; id p = dv_newrp(s, c, d, &err); if (blk) BLK2(blk, p, err); if (p) objc_release(p); }
static void dv_newrp_refl_blk(id s, SEL c, id d, u64 opts, void *blk) {
    (void)opts;
    id err = 0;
    id p = dv_newrp(s, c, d, &err);
    if (blk) ((void (*)(void *, id, id, id))((blk_t *)blk)->invoke)(blk, p, 0, err);
    if (p) objc_release(p);
}
static void rp_dealloc(id s, SEL c) {
    mrpipe_t *p = (mrpipe_t *)s;
    if (p->vfunc) objc_release(p->vfunc);
    if (p->ffunc) objc_release(p->ffunc);
    o_dealloc(s, c);
}
static u64 rp_maxthreads(id s, SEL c) { (void)s; (void)c; return 1024; }
static u64 rp_rid(id s, SEL c) { (void)c; return (u64)s; }

static id make_cpipe(mfunc_t *f, u64 maxt, id *err) {
    if (!f || msl_fn_stage(f->fn) != MSL_KERNEL) { set_err(err, mk_error("MTLPipelineErrorDomain", 1, "função de compute ausente ou não é kernel")); return 0; }
    mcpipe_t *p = (mcpipe_t *)new_obj(&MCPipe_c);
    p->func = objc_retain((id)f);
    p->fn = f->fn;
    p->maxthreads = msl_fn_barrier(f->fn) ? 256 : 1024;
    if (maxt && maxt < p->maxthreads) p->maxthreads = (u32)maxt;
    return (id)p;
}
static id dv_newcp(id s, SEL c, id f, id *err) { (void)s; (void)c; return make_cpipe((mfunc_t *)f, 0, err); }
static id dv_newcp_refl(id s, SEL c, id f, u64 o, id *refl, id *err) { (void)o; if (refl) *refl = 0; return dv_newcp(s, c, f, err); }
static void dv_newcp_blk(id s, SEL c, id f, void *blk) { id err = 0; id p = dv_newcp(s, c, f, &err); if (blk) BLK2(blk, p, err); if (p) objc_release(p); }
static void dv_newcp_refl_blk(id s, SEL c, id f, u64 o, void *blk) {
    (void)o;
    id err = 0;
    id p = dv_newcp(s, c, f, &err);
    if (blk) ((void (*)(void *, id, id, id))((blk_t *)blk)->invoke)(blk, p, 0, err);
    if (p) objc_release(p);
}
static id dv_newcp_desc(id s, SEL c, id d, u64 o, id *refl, id *err) {
    (void)s; (void)c; (void)o;
    if (refl) *refl = 0;
    return d ? make_cpipe((mfunc_t *)dget(d, "computeFunction"), dget(d, "maxTotalThreadsPerThreadgroup"), err) : 0;
}
static void cp_dealloc(id s, SEL c) { mcpipe_t *p = (mcpipe_t *)s; if (p->func) objc_release(p->func); o_dealloc(s, c); }
static u64 cp_maxthreads(id s, SEL c) { (void)c; return ((mcpipe_t *)s)->maxthreads; }
static u64 cp_width(id s, SEL c) { (void)s; (void)c; return 32; }
static u64 cp_tgmem(id s, SEL c) { (void)c; return msl_fn_tgstatic(((mcpipe_t *)s)->fn); }
static u64 cp_imgmem(id s, SEL c, mtl_size_t d) { (void)s; (void)c; (void)d; return 0; }

/* ---- heaps, fences, eventos */
typedef struct { MHDR; u64 size, used, opts; int type; } mheap_t;
static id dv_newheap(id s, SEL c, id d) {
    (void)s; (void)c;
    mheap_t *h = (mheap_t *)new_obj(&MHeap_c);
    if (d) {
        h->size = dget(d, "size");
        h->opts = dget(d, "cpuCacheMode") | dget(d, "storageMode") << 4 | dget(d, "hazardTrackingMode") << 8;
        h->type = (int)dget(d, "type");
    }
    return (id)h;
}
static id hp_newbuf(id s, SEL c, u64 len, u64 opts) {
    (void)c;
    mheap_t *h = (mheap_t *)s;
    if (h->used + len > h->size) return 0;
    mbuf_t *b = buf_new(len, opts, 0);
    if (!b) return 0;
    b->heap = objc_retain(s);
    h->used += (len + 255) & ~255ull;
    return (id)b;
}
static id hp_newbuf_off(id s, SEL c, u64 len, u64 opts, u64 off) { (void)off; return hp_newbuf(s, c, len, opts); }
static id hp_newtex(id s, SEL c, id d) {
    (void)c;
    mheap_t *h = (mheap_t *)s;
    mtex_t *t = (mtex_t *)tex_new(d);
    if (!t) return 0;
    u64 n = mx_tex_bytes(t);
    if (h->used + n > h->size) { objc_release((id)t); return 0; }
    h->used += (n + 255) & ~255ull;
    t->heap = objc_retain(s);
    return (id)t;
}
static id hp_newtex_off(id s, SEL c, id d, u64 off) { (void)off; return hp_newtex(s, c, d); }
static u64 hp_size(id s, SEL c) { (void)c; return ((mheap_t *)s)->size; }
static u64 hp_used(id s, SEL c) { (void)c; return ((mheap_t *)s)->used; }
static u64 hp_avail(id s, SEL c, u64 al) { (void)c; (void)al; mheap_t *h = (mheap_t *)s; return h->size > h->used ? h->size - h->used : 0; }
static u64 hp_type(id s, SEL c) { (void)c; return (u64)((mheap_t *)s)->type; }
static u64 hp_storage(id s, SEL c) { (void)c; return (((mheap_t *)s)->opts >> 4) & 0xf; }
static u64 hp_opts(id s, SEL c) { (void)c; return ((mheap_t *)s)->opts; }
static sizealign_t dv_heap_tex_sa(id s, SEL c, id d) {
    (void)s; (void)c;
    sizealign_t r = {0, 256};
    mtex_t tmp;
    memset(&tmp, 0, sizeof tmp);
    if (d && tex_from_desc(&tmp, d)) { mx_tex_layout(&tmp, 0); r.size = (mx_tex_bytes(&tmp) + 255) & ~255ull; }
    return r;
}
static sizealign_t dv_heap_buf_sa(id s, SEL c, u64 len, u64 opts) { (void)s; (void)c; (void)opts; sizealign_t r = {(len + 255) & ~255ull, 256}; return r; }

typedef struct sev_listener { u64 v; void *blk; id listener; struct sev_listener *next; } sevl_t;
typedef struct { MHDR; volatile u64 val; sevl_t *ls; } mevent_t;
static id dv_newfence(id s, SEL c) { (void)s; (void)c; return new_obj(&MFence_c); }
static id dv_newevent(id s, SEL c) { (void)s; (void)c; return new_obj(&MEvent_c); }
static id dv_newsevent(id s, SEL c) { (void)s; (void)c; return new_obj(&MSEvent_c); }
static id dv_newsevent_h(id s, SEL c, id h) { (void)h; return dv_newsevent(s, c); }
static u64 ev_get(id s, SEL c) { (void)c; return ((mevent_t *)s)->val; }
void mx_signal_event(id ev, u64 v) {
    if (!ev) return;
    mevent_t *e = (mevent_t *)ev;
    e->val = v;
    for (sevl_t **pl = &e->ls; *pl;) {
        sevl_t *l = *pl;
        if (e->val >= l->v) {
            *pl = l->next;
            ((void (*)(void *, id, u64))((blk_t *)l->blk)->invoke)(l->blk, ev, e->val);
            if (l->listener) objc_release(l->listener);
            free(l);
            continue;
        }
        pl = &l->next;
    }
}
static void ev_set(id s, SEL c, u64 v) { (void)c; mx_signal_event(s, v); }
static void ev_notify(id s, SEL c, id listener, u64 v, void *blk) {
    (void)c;
    if (!blk) return;
    sevl_t *l = calloc(1, sizeof *l);
    l->v = v;
    l->blk = _Block_copy(blk);
    l->listener = listener ? objc_retain(listener) : 0;
    mevent_t *e = (mevent_t *)s;
    l->next = e->ls;
    e->ls = l;
    if (e->val >= v) mx_signal_event(s, e->val);
}
static BOOL ev_wait(id s, SEL c, u64 v, u64 ms) { (void)c; (void)ms; return ((mevent_t *)s)->val >= v; }
static id ev_handle(id s, SEL c) { (void)s; (void)c; return objc_autorelease(MSG0(id, MSG0(id, (id)&MTLSharedEventHandle_c, "alloc"), "init")); }

/* ================================================================ fila e command buffers */
typedef struct ablk { struct ablk *next; u64 used, cap; u8 d[]; } ablk_t;
typedef struct mcb {
    MHDR;
    id queue;
    int status, retained;
    cmd_t **cmds;
    int n, cap;
    ablk_t *arena;
    id *ret;
    int nret, capret;
    void **sh, **ch;
    int nsh, nch, capsh, capch;
    id *pres;
    int npres, cappres;
    id enc;
    double t0, t1;
} mcb_t;
typedef struct menc {
    MHDR;
    mcb_t *cb;
    int kind, ended, rdirty, cdirty;
    rstate_t r;
    rstate_t *rsnap;
    cstate_t c;
    cstate_t *csnap;
} menc_t;
typedef struct { MHDR; u64 maxcb; } mqueue_t;

static void *cb_alloc(mcb_t *cb, u64 n) {
    n = (n + 15) & ~15ull;
    ablk_t *b = cb->arena;
    if (!b || b->used + n > b->cap) {
        u64 cap = n > 65536 ? n : 65536;
        b = malloc(sizeof *b + cap);
        if (!b) return 0;
        b->next = cb->arena;
        b->used = 0;
        b->cap = cap;
        cb->arena = b;
    }
    void *p = b->d + b->used;
    b->used += n;
    memset(p, 0, n);
    return p;
}
static void cb_retain(mcb_t *cb, id o) {
    if (!o) return;
    for (int i = cb->nret - 1; i >= 0 && i >= cb->nret - 8; i--) if (cb->ret[i] == o) return;
    if (cb->nret == cb->capret) { cb->capret = cb->capret ? cb->capret * 2 : 32; cb->ret = realloc(cb->ret, cb->capret * sizeof(id)); }
    cb->ret[cb->nret++] = objc_retain(o);
}
static cmd_t *cb_cmd(mcb_t *cb, int k) {
    cmd_t *c = cb_alloc(cb, sizeof *c);
    if (!c) return 0;
    c->k = k;
    if (cb->n == cb->cap) { cb->cap = cb->cap ? cb->cap * 2 : 64; cb->cmds = realloc(cb->cmds, cb->cap * sizeof(cmd_t *)); }
    cb->cmds[cb->n++] = c;
    return c;
}
static void ppush(void ***v, int *n, int *cap, void *x) {
    if (*n == *cap) { *cap = *cap ? *cap * 2 : 8; *v = realloc(*v, *cap * sizeof(void *)); }
    (*v)[(*n)++] = x;
}
static void cb_release_refs(mcb_t *cb) {
    for (int i = 0; i < cb->nret; i++) objc_release(cb->ret[i]);
    cb->nret = 0;
}
static void enc_end(menc_t *e);
static id q_cb(id s, SEL c) {
    (void)c;
    mcb_t *cb = (mcb_t *)new_obj(&MCB_c);
    cb->queue = objc_retain(s);
    cb->retained = 1;
    return objc_autorelease((id)cb);
}
static id q_cb_desc(id s, SEL c, id d) { (void)d; return q_cb(s, c); }
static id dv_newqueue(id s, SEL c) { (void)s; (void)c; return new_obj(&MQueue_c); }
static id dv_newqueue_n(id s, SEL c, u64 n) { (void)n; return dv_newqueue(s, c); }
static void cb_dealloc(id s, SEL c) {
    mcb_t *cb = (mcb_t *)s;
    if (cb->enc) { enc_end((menc_t *)cb->enc); objc_release(cb->enc); }
    cb_release_refs(cb);
    free(cb->ret);
    free(cb->cmds);
    for (ablk_t *b = cb->arena, *n; b; b = n) { n = b->next; free(b); }
    for (int i = 0; i < cb->npres; i++) objc_release(cb->pres[i]);
    free(cb->pres);
    free(cb->sh);
    free(cb->ch);
    if (cb->queue) objc_release(cb->queue);
    o_dealloc(s, c);
}
static u64 cb_status(id s, SEL c) { (void)c; return (u64)((mcb_t *)s)->status; }
static id cb_queue(id s, SEL c) { (void)c; return ((mcb_t *)s)->queue; }
static double cb_t0(id s, SEL c) { (void)c; return ((mcb_t *)s)->t0; }
static double cb_t1(id s, SEL c) { (void)c; return ((mcb_t *)s)->t1; }
static void cb_add_sched(id s, SEL c, void *b) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (!b) return;
    if (cb->status >= 3) { BLK1(b, s); return; }
    ppush(&cb->sh, &cb->nsh, &cb->capsh, _Block_copy(b));
}
static void cb_add_done(id s, SEL c, void *b) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (!b) return;
    if (cb->status >= 4) { BLK1(b, s); return; }
    ppush(&cb->ch, &cb->nch, &cb->capch, _Block_copy(b));
}
static void present_now(id drawable);
static void cb_present(id s, SEL c, id d) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (!d) return;
    ppush((void ***)&cb->pres, &cb->npres, &cb->cappres, objc_retain(d));
}
static void cb_present_t(id s, SEL c, id d, double t) { (void)t; cb_present(s, c, d); }
static void cb_enqueue(id s, SEL c) { (void)c; mcb_t *cb = (mcb_t *)s; if (cb->status < 1) cb->status = 1; }
static void cb_commit(id s, SEL c) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (cb->status >= 2) return;
    if (cb->enc) { enc_end((menc_t *)cb->enc); objc_release(cb->enc); cb->enc = 0; }
    cb->status = 2;
    objc_retain(s);
    gpu_lock();
    cb->t0 = now_s();
    for (int i = 0; i < cb->n; i++) mx_exec_cmd(cb->cmds[i]);
    cb->t1 = now_s();
    gpu_unlock();
    cb->status = 3;
    for (int i = 0; i < cb->nsh; i++) BLK1(cb->sh[i], s);
    for (int i = 0; i < cb->npres; i++) { present_now(cb->pres[i]); objc_release(cb->pres[i]); }
    cb->npres = 0;
    cb->status = 4;
    for (int i = 0; i < cb->nch; i++) BLK1(cb->ch[i], s);
    cb->nsh = cb->nch = 0;
    cb_release_refs(cb);
    objc_release(s);
}
static void cb_signal(id s, SEL c, id ev, u64 v) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (!ev) return;
    cmd_t *k = cb_cmd(cb, C_SIGNAL);
    if (!k) return;
    cb_retain(cb, ev);
    k->sig.ev = ev;
    k->sig.v = v;
}

/* ---- encoders */
static menc_t *enc_new(mcb_t *cb, cx_class_t *cls, int kind) {
    if (cb->enc) { enc_end((menc_t *)cb->enc); objc_release(cb->enc); cb->enc = 0; }
    menc_t *e = (menc_t *)new_obj(cls);
    e->cb = cb;
    e->kind = kind;
    e->rdirty = e->cdirty = 1;
    cb->enc = objc_retain((id)e);
    return e;
}
static void enc_end(menc_t *e) {
    if (e->ended) return;
    e->ended = 1;
    if (e->kind == 1) cb_cmd(e->cb, C_REND);
}
static void enc_endm(id s, SEL c) {
    (void)c;
    menc_t *e = (menc_t *)s;
    enc_end(e);
    if (e->cb && e->cb->enc == s) { e->cb->enc = 0; objc_release(s); }
}
static void read_att(mcb_t *cb, id a, attach_t *t, int kind) {
    if (!a) return;
    t->tex = (mtex_t *)dget(a, "texture");
    t->level = (int)dget(a, "level");
    t->slice = (int)dget(a, "slice");
    t->plane = (int)dget(a, "depthPlane");
    t->load = (int)dget(a, "loadAction");
    t->store = (int)dget(a, "storeAction");
    t->resolve = (mtex_t *)dget(a, "resolveTexture");
    t->rlevel = (int)dget(a, "resolveLevel");
    t->rslice = (int)dget(a, "resolveSlice");
    t->rplane = (int)dget(a, "resolveDepthPlane");
    if (kind == 0) t->cc = dgetc(a, "clearColor");
    if (kind == 1) t->cd = dgetd(a, "clearDepth");
    if (kind == 2) t->cs = (u32)dget(a, "clearStencil");
    cb_retain(cb, (id)t->tex);
    cb_retain(cb, (id)t->resolve);
}
static id cb_renc(id s, SEL c, id d) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (!d || cb->status >= 2) return 0;
    rpass_t *rp = cb_alloc(cb, sizeof *rp);
    for (int i = 0; i < 8; i++) read_att(cb, darr_at(d, "colorAttachments", i), &rp->c[i], 0);
    read_att(cb, (id)dget(d, "depthAttachment"), &rp->d, 1);
    read_att(cb, (id)dget(d, "stencilAttachment"), &rp->s, 2);
    rp->rtw = dget(d, "renderTargetWidth");
    rp->rth = dget(d, "renderTargetHeight");
    menc_t *e = enc_new(cb, &MREnc_c, 1);
    cmd_t *k = cb_cmd(cb, C_RBEGIN);
    if (k) k->rp = rp;
    e->r.sref_f = e->r.sref_b = 0;
    return objc_autorelease((id)e);
}
static id cb_cenc(id s, SEL c) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (cb->status >= 2) return 0;
    return objc_autorelease((id)enc_new(cb, &MCEnc_c, 2));
}
static id cb_cenc_t(id s, SEL c, u64 t) { (void)t; return cb_cenc(s, c); }
static id cb_cenc_d(id s, SEL c, id d) { (void)d; return cb_cenc(s, c); }
static id cb_benc(id s, SEL c) {
    (void)c;
    mcb_t *cb = (mcb_t *)s;
    if (cb->status >= 2) return 0;
    return objc_autorelease((id)enc_new(cb, &MBEnc_c, 3));
}
static id cb_benc_d(id s, SEL c, id d) { (void)d; return cb_benc(s, c); }
static void enc_dealloc(id s, SEL c) { o_dealloc(s, c); }

/* estado do render encoder */
#define RE(s) ((menc_t *)(s))
static void bind_set(menc_t *e, bind_t *b, id buf, u64 off) {
    cb_retain(e->cb, buf);
    b->obj = buf;
    b->off = off;
    b->bytes = 0;
}
static void bind_bytes(menc_t *e, bind_t *b, const void *p, u64 n) {
    void *cp = cb_alloc(e->cb, n + 16);
    if (cp && p) memcpy(cp, p, n);
    b->obj = 0;
    b->off = 0;
    b->bytes = cp;
}
static void re_pipe(id s, SEL c, id p) { (void)c; cb_retain(RE(s)->cb, p); RE(s)->r.pipe = (mrpipe_t *)p; RE(s)->rdirty = 1; }
static void re_vbuf(id s, SEL c, id b, u64 off, u64 i) { (void)c; if (i < 31) { bind_set(RE(s), &RE(s)->r.vb[i], b, off); RE(s)->rdirty = 1; } }
static void re_fbuf(id s, SEL c, id b, u64 off, u64 i) { (void)c; if (i < 31) { bind_set(RE(s), &RE(s)->r.fb[i], b, off); RE(s)->rdirty = 1; } }
static void re_vbufoff(id s, SEL c, u64 off, u64 i) { (void)c; if (i < 31) { RE(s)->r.vb[i].off = off; RE(s)->rdirty = 1; } }
static void re_fbufoff(id s, SEL c, u64 off, u64 i) { (void)c; if (i < 31) { RE(s)->r.fb[i].off = off; RE(s)->rdirty = 1; } }
static void re_vbufs(id s, SEL c, const id *b, const u64 *o, nsrange_t r) { for (u64 i = 0; i < r.len; i++) re_vbuf(s, c, b ? b[i] : 0, o ? o[i] : 0, r.loc + i); }
static void re_fbufs(id s, SEL c, const id *b, const u64 *o, nsrange_t r) { for (u64 i = 0; i < r.len; i++) re_fbuf(s, c, b ? b[i] : 0, o ? o[i] : 0, r.loc + i); }
static void re_vbytes(id s, SEL c, const void *p, u64 n, u64 i) { (void)c; if (i < 31) { bind_bytes(RE(s), &RE(s)->r.vb[i], p, n); RE(s)->rdirty = 1; } }
static void re_fbytes(id s, SEL c, const void *p, u64 n, u64 i) { (void)c; if (i < 31) { bind_bytes(RE(s), &RE(s)->r.fb[i], p, n); RE(s)->rdirty = 1; } }
static void re_vtex(id s, SEL c, id t, u64 i) { (void)c; if (i < 32) { cb_retain(RE(s)->cb, t); RE(s)->r.vt[i] = (mtex_t *)t; RE(s)->rdirty = 1; } }
static void re_ftex(id s, SEL c, id t, u64 i) { (void)c; if (i < 32) { cb_retain(RE(s)->cb, t); RE(s)->r.ft[i] = (mtex_t *)t; RE(s)->rdirty = 1; } }
static void re_vtexs(id s, SEL c, const id *t, nsrange_t r) { for (u64 i = 0; i < r.len; i++) re_vtex(s, c, t ? t[i] : 0, r.loc + i); }
static void re_ftexs(id s, SEL c, const id *t, nsrange_t r) { for (u64 i = 0; i < r.len; i++) re_ftex(s, c, t ? t[i] : 0, r.loc + i); }
static void re_vsamp(id s, SEL c, id t, u64 i) { (void)c; if (i < 16) { cb_retain(RE(s)->cb, t); RE(s)->r.vs[i] = (msamp_t *)t; RE(s)->rdirty = 1; } }
static void re_fsamp(id s, SEL c, id t, u64 i) { (void)c; if (i < 16) { cb_retain(RE(s)->cb, t); RE(s)->r.fs[i] = (msamp_t *)t; RE(s)->rdirty = 1; } }
static void re_vsamps(id s, SEL c, const id *t, nsrange_t r) { for (u64 i = 0; i < r.len; i++) re_vsamp(s, c, t ? t[i] : 0, r.loc + i); }
static void re_fsamps(id s, SEL c, const id *t, nsrange_t r) { for (u64 i = 0; i < r.len; i++) re_fsamp(s, c, t ? t[i] : 0, r.loc + i); }
static void re_vsamp_l(id s, SEL c, id t, float a, float b, u64 i) { (void)a; (void)b; re_vsamp(s, c, t, i); }
static void re_fsamp_l(id s, SEL c, id t, float a, float b, u64 i) { (void)a; (void)b; re_fsamp(s, c, t, i); }
static void re_viewport(id s, SEL c, mtl_vp_t v) { (void)c; RE(s)->r.vp = v; RE(s)->r.vp_set = 1; RE(s)->rdirty = 1; }
static void re_viewports(id s, SEL c, const mtl_vp_t *v, u64 n) { if (v && n) re_viewport(s, c, v[0]); }
static void re_scissor(id s, SEL c, mtl_sc_t r) { (void)c; RE(s)->r.sc = r; RE(s)->r.sc_set = 1; RE(s)->rdirty = 1; }
static void re_scissors(id s, SEL c, const mtl_sc_t *v, u64 n) { if (v && n) re_scissor(s, c, v[0]); }
static void re_wind(id s, SEL c, u64 w) { (void)c; RE(s)->r.wind = (int)w; RE(s)->rdirty = 1; }
static void re_cull(id s, SEL c, u64 m) { (void)c; RE(s)->r.cull = (int)m; RE(s)->rdirty = 1; }
static void re_fill(id s, SEL c, u64 m) { (void)c; RE(s)->r.fill = (int)m; RE(s)->rdirty = 1; }
static void re_dclip(id s, SEL c, u64 m) { (void)c; RE(s)->r.dclip = (int)m; RE(s)->rdirty = 1; }
static void re_dbias(id s, SEL c, float b, float sl, float cl) { (void)c; RE(s)->r.dbias = b * (1.0f / 16777216.0f); RE(s)->r.dslope = sl; RE(s)->r.dclamp = cl; RE(s)->rdirty = 1; }
static void re_dss(id s, SEL c, id d) { (void)c; cb_retain(RE(s)->cb, d); RE(s)->r.dss = (mdss_t *)d; RE(s)->rdirty = 1; }
static void re_sref(id s, SEL c, u32 v) { (void)c; RE(s)->r.sref_f = RE(s)->r.sref_b = v; RE(s)->rdirty = 1; }
static void re_sref2(id s, SEL c, u32 f, u32 b) { (void)c; RE(s)->r.sref_f = f; RE(s)->r.sref_b = b; RE(s)->rdirty = 1; }
static void re_blendc(id s, SEL c, float r, float g, float b, float a) { (void)c; float *v = RE(s)->r.bc; v[0] = r; v[1] = g; v[2] = b; v[3] = a; RE(s)->rdirty = 1; }
static rstate_t *re_snap(menc_t *e) {
    if (e->rdirty || !e->rsnap) {
        rstate_t *s = cb_alloc(e->cb, sizeof *s);
        if (!s) return 0;
        *s = e->r;
        e->rsnap = s;
        e->rdirty = 0;
    }
    return e->rsnap;
}
static void re_draw_full(id s, u64 prim, u64 start, u64 count, u64 inst, u64 binst) {
    menc_t *e = RE(s);
    if (e->ended) return;
    cmd_t *k = cb_cmd(e->cb, C_DRAW);
    if (!k) return;
    k->d.st = re_snap(e);
    k->d.prim = (int)prim;
    k->d.vstart = start;
    k->d.vcount = count;
    k->d.icount = inst;
    k->d.binst = binst;
}
static void re_draw(id s, SEL c, u64 p, u64 st, u64 n) { (void)c; re_draw_full(s, p, st, n, 1, 0); }
static void re_draw_i(id s, SEL c, u64 p, u64 st, u64 n, u64 i) { (void)c; re_draw_full(s, p, st, n, i, 0); }
static void re_draw_ib(id s, SEL c, u64 p, u64 st, u64 n, u64 i, u64 b) { (void)c; re_draw_full(s, p, st, n, i, b); }
static void re_drawidx_full(id s, u64 prim, u64 cnt, u64 type, id ib, u64 ioff, u64 inst, i64 bv, u64 bi) {
    menc_t *e = RE(s);
    if (e->ended || !ib) return;
    cmd_t *k = cb_cmd(e->cb, C_DRAW);
    if (!k) return;
    cb_retain(e->cb, ib);
    k->d.st = re_snap(e);
    k->d.prim = (int)prim;
    k->d.indexed = 1;
    k->d.icnt = cnt;
    k->d.itype = (int)type;
    k->d.ib = (mbuf_t *)ib;
    k->d.ioff = ioff;
    k->d.icount = inst;
    k->d.bvert = bv;
    k->d.binst = bi;
}
static void re_drawidx(id s, SEL c, u64 p, u64 n, u64 t, id ib, u64 off) { (void)c; re_drawidx_full(s, p, n, t, ib, off, 1, 0, 0); }
static void re_drawidx_i(id s, SEL c, u64 p, u64 n, u64 t, id ib, u64 off, u64 i) { (void)c; re_drawidx_full(s, p, n, t, ib, off, i, 0, 0); }
static void re_drawidx_ibb(id s, SEL c, u64 p, u64 n, u64 t, id ib, u64 off, u64 i, i64 bv, u64 bi) { (void)c; re_drawidx_full(s, p, n, t, ib, off, i, bv, bi); }
static void re_draw_ind(id s, SEL c, u64 p, id ind, u64 off) {
    (void)c;
    menc_t *e = RE(s);
    if (e->ended || !ind) return;
    cmd_t *k = cb_cmd(e->cb, C_DRAW);
    if (!k) return;
    cb_retain(e->cb, ind);
    k->d.st = re_snap(e);
    k->d.prim = (int)p;
    k->d.indirect = (mbuf_t *)ind;
    k->d.indoff = off;
}
static void re_drawidx_ind(id s, SEL c, u64 p, u64 t, id ib, u64 ioff, id ind, u64 off) {
    (void)c;
    menc_t *e = RE(s);
    if (e->ended || !ind || !ib) return;
    cmd_t *k = cb_cmd(e->cb, C_DRAW);
    if (!k) return;
    cb_retain(e->cb, ind);
    cb_retain(e->cb, ib);
    k->d.st = re_snap(e);
    k->d.prim = (int)p;
    k->d.indexed = 1;
    k->d.itype = (int)t;
    k->d.ib = (mbuf_t *)ib;
    k->d.ioff = ioff;
    k->d.indirect = (mbuf_t *)ind;
    k->d.indoff = off;
}

/* compute encoder */
static void ce_pipe(id s, SEL c, id p) { (void)c; cb_retain(RE(s)->cb, p); RE(s)->c.pipe = (mcpipe_t *)p; RE(s)->cdirty = 1; }
static void ce_buf(id s, SEL c, id b, u64 off, u64 i) { (void)c; if (i < 31) { bind_set(RE(s), &RE(s)->c.b[i], b, off); RE(s)->cdirty = 1; } }
static void ce_bufoff(id s, SEL c, u64 off, u64 i) { (void)c; if (i < 31) { RE(s)->c.b[i].off = off; RE(s)->cdirty = 1; } }
static void ce_bufs(id s, SEL c, const id *b, const u64 *o, nsrange_t r) { for (u64 i = 0; i < r.len; i++) ce_buf(s, c, b ? b[i] : 0, o ? o[i] : 0, r.loc + i); }
static void ce_bytes(id s, SEL c, const void *p, u64 n, u64 i) { (void)c; if (i < 31) { bind_bytes(RE(s), &RE(s)->c.b[i], p, n); RE(s)->cdirty = 1; } }
static void ce_tex(id s, SEL c, id t, u64 i) { (void)c; if (i < 32) { cb_retain(RE(s)->cb, t); RE(s)->c.t[i] = (mtex_t *)t; RE(s)->cdirty = 1; } }
static void ce_texs(id s, SEL c, const id *t, nsrange_t r) { for (u64 i = 0; i < r.len; i++) ce_tex(s, c, t ? t[i] : 0, r.loc + i); }
static void ce_samp(id s, SEL c, id t, u64 i) { (void)c; if (i < 16) { cb_retain(RE(s)->cb, t); RE(s)->c.s[i] = (msamp_t *)t; RE(s)->cdirty = 1; } }
static void ce_samps(id s, SEL c, const id *t, nsrange_t r) { for (u64 i = 0; i < r.len; i++) ce_samp(s, c, t ? t[i] : 0, r.loc + i); }
static void ce_samp_l(id s, SEL c, id t, float a, float b, u64 i) { (void)a; (void)b; ce_samp(s, c, t, i); }
static void ce_tgmem(id s, SEL c, u64 n, u64 i) { (void)c; if (i < 31) { RE(s)->c.tglen[i] = n; RE(s)->cdirty = 1; } }
static cstate_t *ce_snap(menc_t *e) {
    if (e->cdirty || !e->csnap) {
        cstate_t *s = cb_alloc(e->cb, sizeof *s);
        if (!s) return 0;
        *s = e->c;
        e->csnap = s;
        e->cdirty = 0;
    }
    return e->csnap;
}
static void ce_dispatch_k(id s, const u32 g[3], mtl_size_t t, int exact, const u32 grid[3], id ind, u64 off) {
    menc_t *e = RE(s);
    if (e->ended) return;
    cmd_t *k = cb_cmd(e->cb, C_DISPATCH);
    if (!k) return;
    k->c.st = ce_snap(e);
    for (int i = 0; i < 3; i++) { k->c.g[i] = g[i]; k->c.grid[i] = grid[i]; }
    k->c.t[0] = (u32)t.w; k->c.t[1] = (u32)t.h; k->c.t[2] = (u32)t.d;
    k->c.exact = exact;
    if (ind) { cb_retain(e->cb, ind); k->c.indirect = (mbuf_t *)ind; k->c.indoff = off; }
}
static void ce_dispatch_tg(id s, SEL c, mtl_size_t g, mtl_size_t t) {
    (void)c;
    u32 gg[3] = {(u32)g.w, (u32)g.h, (u32)g.d}, z[3] = {0, 0, 0};
    ce_dispatch_k(s, gg, t, 0, z, 0, 0);
}
static void ce_dispatch_th(id s, SEL c, mtl_size_t n, mtl_size_t t) {
    (void)c;
    if (!t.w) t.w = 1;
    if (!t.h) t.h = 1;
    if (!t.d) t.d = 1;
    u32 gg[3] = {(u32)((n.w + t.w - 1) / t.w), (u32)((n.h + t.h - 1) / t.h), (u32)((n.d + t.d - 1) / t.d)};
    u32 grid[3] = {(u32)n.w, (u32)n.h, (u32)n.d};
    ce_dispatch_k(s, gg, t, 1, grid, 0, 0);
}
static void ce_dispatch_ind(id s, SEL c, id ind, u64 off, mtl_size_t t) {
    (void)c;
    u32 z[3] = {0, 0, 0};
    ce_dispatch_k(s, z, t, 0, z, ind, off);
}
static u64 ce_dtype(id s, SEL c) { (void)s; (void)c; return 0; }

/* blit encoder */
static menc_t *be_ok(id s) { menc_t *e = RE(s); return e->ended ? 0 : e; }
static void be_bb(id s, SEL c, id sb, u64 so, id db, u64 dof, u64 n) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !sb || !db) return;
    cmd_t *k = cb_cmd(e->cb, C_COPY_BB);
    if (!k) return;
    cb_retain(e->cb, sb);
    cb_retain(e->cb, db);
    k->bb.sb = (mbuf_t *)sb; k->bb.db = (mbuf_t *)db; k->bb.so = so; k->bb.dof = dof; k->bb.n = n;
}
static void be_bt(id s, SEL c, id sb, u64 so, u64 bpr, u64 bpi, mtl_size_t sz, id t, u64 slice, u64 lvl, mtl_origin_t org) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !sb || !t) return;
    cmd_t *k = cb_cmd(e->cb, C_COPY_BT);
    if (!k) return;
    cb_retain(e->cb, sb);
    cb_retain(e->cb, t);
    k->bt.b = (mbuf_t *)sb; k->bt.off = so; k->bt.bpr = bpr; k->bt.bpi = bpi; k->bt.sz = sz; k->bt.t = (mtex_t *)t;
    k->bt.slice = slice; k->bt.level = lvl; k->bt.org = org;
}
static void be_bt_o(id s, SEL c, id sb, u64 so, u64 bpr, u64 bpi, mtl_size_t sz, id t, u64 slice, u64 lvl, mtl_origin_t org, u64 o) { (void)o; be_bt(s, c, sb, so, bpr, bpi, sz, t, slice, lvl, org); }
static void be_tb(id s, SEL c, id t, u64 slice, u64 lvl, mtl_origin_t org, mtl_size_t sz, id db, u64 off, u64 bpr, u64 bpi) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !db || !t) return;
    cmd_t *k = cb_cmd(e->cb, C_COPY_TB);
    if (!k) return;
    cb_retain(e->cb, db);
    cb_retain(e->cb, t);
    k->bt.b = (mbuf_t *)db; k->bt.off = off; k->bt.bpr = bpr; k->bt.bpi = bpi; k->bt.sz = sz; k->bt.t = (mtex_t *)t;
    k->bt.slice = slice; k->bt.level = lvl; k->bt.org = org;
}
static void be_tb_o(id s, SEL c, id t, u64 slice, u64 lvl, mtl_origin_t org, mtl_size_t sz, id db, u64 off, u64 bpr, u64 bpi, u64 o) { (void)o; be_tb(s, c, t, slice, lvl, org, sz, db, off, bpr, bpi); }
static void be_tt(id s, SEL c, id st, u64 ss, u64 sl, mtl_origin_t so, mtl_size_t sz, id dt, u64 ds, u64 dl, mtl_origin_t dor) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !st || !dt) return;
    cmd_t *k = cb_cmd(e->cb, C_COPY_TT);
    if (!k) return;
    cb_retain(e->cb, st);
    cb_retain(e->cb, dt);
    k->tt.s = (mtex_t *)st; k->tt.d = (mtex_t *)dt; k->tt.ss = ss; k->tt.sl = sl; k->tt.so = so; k->tt.sz = sz;
    k->tt.ds = ds; k->tt.dl = dl; k->tt.dorg = dor;
}
static void be_tt_all(id s, SEL c, id st, id dt) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !st || !dt) return;
    cmd_t *k = cb_cmd(e->cb, C_COPY_TT);
    if (!k) return;
    cb_retain(e->cb, st);
    cb_retain(e->cb, dt);
    k->tt.s = (mtex_t *)st; k->tt.d = (mtex_t *)dt; k->tt.whole = 1;
}
static void be_tt_n(id s, SEL c, id st, u64 ss, u64 sl, id dt, u64 ds, u64 dl, u64 ns, u64 nl) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !st || !dt) return;
    cmd_t *k = cb_cmd(e->cb, C_COPY_TT);
    if (!k) return;
    cb_retain(e->cb, st);
    cb_retain(e->cb, dt);
    k->tt.s = (mtex_t *)st; k->tt.d = (mtex_t *)dt; k->tt.ss = ss; k->tt.sl = sl; k->tt.ds = ds; k->tt.dl = dl;
    k->tt.nslices = ns; k->tt.nlevels = nl ? nl : 1;
}
static void be_fill(id s, SEL c, id b, nsrange_t r, u8 v) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !b) return;
    cmd_t *k = cb_cmd(e->cb, C_FILL);
    if (!k) return;
    cb_retain(e->cb, b);
    k->fill.b = (mbuf_t *)b; k->fill.off = r.loc; k->fill.n = r.len; k->fill.v = v;
}
static void be_mips(id s, SEL c, id t) {
    (void)c;
    menc_t *e = be_ok(s);
    if (!e || !t) return;
    cmd_t *k = cb_cmd(e->cb, C_MIPS);
    if (!k) return;
    cb_retain(e->cb, t);
    k->mips.t = (mtex_t *)t;
}

/* ================================================================ CAMetalLayer */
#define POOL 3
typedef struct {
    id device, colorspace;
    u64 fmt;
    CGS size;
    int size_set, fbonly, maxd, pwt, vsync, timeout, edr;
    id surf[POOL];
    id tex[POOL];             /* textura do drawable (sobre o IOSurface quando o formato é BGRA8) */
    int conv[POOL];
    u64 pw, ph, pfmt;
    int next, shown;
    u64 ids;
} mlayer_t;
static u32 g_ml_off;
static mlayer_t *ml(id l) {
    mlayer_t **pp = (mlayer_t **)((u8 *)l + g_ml_off);
    if (!*pp) {
        mlayer_t *m = calloc(1, sizeof *m);
        m->fmt = 80;
        m->fbonly = 1;
        m->maxd = 3;
        m->vsync = 1;
        m->timeout = 1;
        m->shown = -1;
        *pp = m;
    }
    return *pp;
}
static void (*orig_ml_dealloc)(id, SEL);
static void ml_pool_free(mlayer_t *m) {
    for (int i = 0; i < POOL; i++) {
        if (m->tex[i]) objc_release(m->tex[i]);
        if (m->surf[i]) objc_release(m->surf[i]);
        m->tex[i] = m->surf[i] = 0;
    }
    m->shown = -1;
}
static void ml_dealloc(id s, SEL c) {
    mlayer_t **pp = (mlayer_t **)((u8 *)s + g_ml_off);
    if (*pp) {
        ml_pool_free(*pp);
        if ((*pp)->device) objc_release((*pp)->device);
        if ((*pp)->colorspace) objc_release((*pp)->colorspace);
        free(*pp);
        *pp = 0;
    }
    if (orig_ml_dealloc) orig_ml_dealloc(s, c);
    else super_dealloc(s, &CALayer_c);
}
static id ml_device(id s, SEL c) { (void)c; return ml(s)->device; }
static void ml_set_device(id s, SEL c, id d) { (void)c; mlayer_t *m = ml(s); if (d) objc_retain(d); if (m->device) objc_release(m->device); m->device = d; }
static id ml_pref_device(id s, SEL c) { (void)s; (void)c; return g_dev; }
static u64 ml_fmt(id s, SEL c) { (void)c; return ml(s)->fmt; }
static void ml_set_fmt(id s, SEL c, u64 f) { (void)c; ml(s)->fmt = f; }
static BOOL ml_fbonly(id s, SEL c) { (void)c; return (BOOL)ml(s)->fbonly; }
static void ml_set_fbonly(id s, SEL c, BOOL b) { (void)c; ml(s)->fbonly = b; }
static BOOL ml_pwt(id s, SEL c) { (void)c; return (BOOL)ml(s)->pwt; }
static void ml_set_pwt(id s, SEL c, BOOL b) { (void)c; ml(s)->pwt = b; }
static BOOL ml_vsync(id s, SEL c) { (void)c; return (BOOL)ml(s)->vsync; }
static void ml_set_vsync(id s, SEL c, BOOL b) { (void)c; ml(s)->vsync = b; }
static BOOL ml_timeout(id s, SEL c) { (void)c; return (BOOL)ml(s)->timeout; }
static void ml_set_timeout(id s, SEL c, BOOL b) { (void)c; ml(s)->timeout = b; }
static BOOL ml_edr(id s, SEL c) { (void)c; return (BOOL)ml(s)->edr; }
static void ml_set_edr(id s, SEL c, BOOL b) { (void)c; ml(s)->edr = b; }
static u64 ml_maxd(id s, SEL c) { (void)c; return (u64)ml(s)->maxd; }
static void ml_set_maxd(id s, SEL c, u64 n) { (void)c; ml(s)->maxd = n < 2 ? 2 : n > 3 ? 3 : (int)n; }
static id ml_cs(id s, SEL c) { (void)c; return ml(s)->colorspace; }
static void ml_set_cs(id s, SEL c, id cs) { (void)c; mlayer_t *m = ml(s); if (cs) objc_retain(cs); if (m->colorspace) objc_release(m->colorspace); m->colorspace = cs; }
static CGS ml_auto_size(id s) {
    CGR b;
    ((void (*)(CGR *, id, SEL))objc_msgSend_stret)(&b, s, sel_registerName("bounds"));
    double sc = MSG0(double, s, "contentsScale");
    if (sc <= 0) sc = 1;
    CGS r = {b.s.w * sc, b.s.h * sc};
    return r;
}
static CGS ml_size(id s, SEL c) { (void)c; mlayer_t *m = ml(s); return m->size_set ? m->size : ml_auto_size(s); }
static void ml_set_size(id s, SEL c, CGS z) { (void)c; mlayer_t *m = ml(s); m->size = z; m->size_set = z.w > 0 && z.h > 0; }

typedef struct { MHDR; id layer, tex, surf; int slot, presented, conv; u64 did; void **ph; int nph, capph; double ptime; } mdraw_t;
static id make_surface(u64 w, u64 h) {
    id keys[4] = {(id)kIOSurfaceWidth, (id)kIOSurfaceHeight, (id)kIOSurfaceBytesPerElement, (id)kIOSurfacePixelFormat};
    id vals[4] = {cx_num_i((i64)w), cx_num_i((i64)h), cx_num_i(4), cx_num_i(0x42475241 /* 'BGRA' */)};
    id d = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObjects:forKeys:count:", vals, keys, (u64)4);
    for (int i = 0; i < 4; i++) objc_release(vals[i]);
    return IOSurfaceCreate(d);
}
static id ml_next(id s, SEL c) {
    (void)c;
    mlayer_t *m = ml(s);
    CGS z = m->size_set ? m->size : ml_auto_size(s);
    u64 w = z.w < 1 ? 1 : (u64)(z.w + 0.5), h = z.h < 1 ? 1 : (u64)(z.h + 0.5);
    if (w > 16384) w = 16384;
    if (h > 16384) h = 16384;
    if (w != m->pw || h != m->ph || m->fmt != m->pfmt) { ml_pool_free(m); m->pw = w; m->ph = h; m->pfmt = m->fmt; }
    int n = m->maxd < POOL ? m->maxd : POOL;
    int i = m->next % n;
    if (i == m->shown) i = (i + 1) % n;
    m->next = i + 1;
    if (!m->surf[i]) {
        m->surf[i] = make_surface(w, h);
        if (!m->surf[i]) return 0;
        id td = MSG(id, (id)&MTLTextureDescriptor_c, "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", m->fmt, w, h, (BOOL)NO);
        MSG(void, td, "setUsage:", (u64)0x5);
        int direct = m->fmt == 80 || m->fmt == 81;
        m->conv[i] = !direct;
        m->tex[i] = direct ? dv_newtex_surf(g_dev, 0, td, m->surf[i], 0) : tex_new(td);
        if (!m->tex[i]) { objc_release(m->surf[i]); m->surf[i] = 0; return 0; }
        ((mtex_t *)m->tex[i])->fbonly = m->fbonly;
    }
    mdraw_t *d = (mdraw_t *)new_obj(&MDrawable_c);
    d->layer = objc_retain(s);
    d->tex = objc_retain(m->tex[i]);
    d->surf = objc_retain(m->surf[i]);
    d->slot = i;
    d->conv = m->conv[i];
    d->did = ++m->ids;
    return objc_autorelease((id)d);
}
static void dr_dealloc(id s, SEL c) {
    mdraw_t *d = (mdraw_t *)s;
    if (d->layer) objc_release(d->layer);
    if (d->tex) objc_release(d->tex);
    if (d->surf) objc_release(d->surf);
    free(d->ph);
    o_dealloc(s, c);
}
static void present_now(id drawable) {
    mdraw_t *d = (mdraw_t *)drawable;
    if (!d || d->presented) return;
    d->presented = 1;
    mlayer_t *m = ml(d->layer);
    if (d->conv) mx_to_bgra8((mtex_t *)d->tex, IOSurfaceGetBaseAddress(d->surf), IOSurfaceGetBytesPerRow(d->surf));
    g_presenting++;
    IOSurfaceLock(d->surf, 0, 0);
    IOSurfaceUnlock(d->surf, 0, 0);
    MSG(void, d->layer, "setContents:", d->surf);
    g_presenting--;
    m->shown = d->slot;
    d->ptime = now_s();
    for (int i = 0; i < d->nph; i++) BLK1(d->ph[i], drawable);
    d->nph = 0;
}
static void dr_present(id s, SEL c) { (void)c; present_now(s); }
static void dr_present_t(id s, SEL c, double t) { (void)t; present_now(s); (void)c; }
static id dr_tex(id s, SEL c) { (void)c; return ((mdraw_t *)s)->tex; }
static id dr_layer(id s, SEL c) { (void)c; return ((mdraw_t *)s)->layer; }
static u64 dr_id(id s, SEL c) { (void)c; return ((mdraw_t *)s)->did; }
static double dr_ptime(id s, SEL c) { (void)c; return ((mdraw_t *)s)->ptime; }
static void dr_add_ph(id s, SEL c, void *b) {
    (void)c;
    mdraw_t *d = (mdraw_t *)s;
    if (!b) return;
    if (d->presented) { BLK1(b, s); return; }
    ppush(&d->ph, &d->nph, &d->capph, _Block_copy(b));
}

/* ================================================================ MTKView */
typedef struct {
    id device, layer, drawable, rpd, depth, msaa, timer;
    id delegate;              /* fraco */
    u64 cfmt, dfmt, samples, fps, dusage;
    mtl_cc_t cc;
    double cd;
    u32 cs;
    int paused, needs_display_mode, autoresize, pending, fbonly, pwt, drawing;
    CGS size;
} mtk_t;
static u32 g_mtk_off;
static IMP nsview_init_frame, nsview_init_coder, nsview_set_needs, nsview_dealloc;
static mtk_t *mk(id v) {
    mtk_t **pp = (mtk_t **)((u8 *)v + g_mtk_off);
    if (!*pp) {
        mtk_t *m = calloc(1, sizeof *m);
        m->cfmt = 80;
        m->samples = 1;
        m->fps = 60;
        m->cc.a = 1;
        m->cd = 1;
        m->autoresize = 1;
        m->fbonly = 1;
        m->dusage = 4;    /* RenderTarget */
        *pp = m;
    }
    return *pp;
}
static void mtk_tick(id self, SEL c, id timer);
static void mtk_setup(id self, id device) {
    mtk_t *m = mk(self);
    if (device && !m->device) m->device = objc_retain(device);
    MSG(void, self, "setWantsLayer:", (BOOL)YES);
    id l = MSG0(id, self, "layer");
    if (l && cx_is_subclass(l->isa, &CAMetalLayer_c)) {
        m->layer = objc_retain(l);
        ml_set_device(l, 0, m->device);
        ml(l)->fmt = m->cfmt;
    }
    if (!m->timer) {
        double iv = 1.0 / (double)(m->fps ? m->fps : 60);
        id t = MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", iv, self,
                   sel_registerName("_mosMetalTick:"), (id)0, (BOOL)YES);
        m->timer = t ? objc_retain(t) : 0;
    }
}
static id mtk_init_frame_dev(id self, SEL c, CGR f, id dev) {
    (void)c;
    self = nsview_init_frame ? ((id (*)(id, SEL, CGR))nsview_init_frame)(self, sel_registerName("initWithFrame:"), f) : self;
    if (self) mtk_setup(self, dev);
    return self;
}
static id mtk_init_frame(id self, SEL c, CGR f) { return mtk_init_frame_dev(self, c, f, 0); }
static id mtk_init_coder(id self, SEL c, id coder) {
    (void)c;
    self = nsview_init_coder ? ((id (*)(id, SEL, id))nsview_init_coder)(self, sel_registerName("initWithCoder:"), coder) : self;
    if (self) mtk_setup(self, 0);
    return self;
}
static id mtk_make_backing(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)&CAMetalLayer_c, "layer"); }
static void mtk_dealloc(id self, SEL c) {
    mtk_t **pp = (mtk_t **)((u8 *)self + g_mtk_off);
    mtk_t *m = *pp;
    if (m) {
        if (m->timer) { MSG0(void, m->timer, "invalidate"); objc_release(m->timer); }
        id *objs[] = {&m->device, &m->layer, &m->drawable, &m->rpd, &m->depth, &m->msaa};
        for (u64 i = 0; i < sizeof objs / sizeof *objs; i++) if (*objs[i]) objc_release(*objs[i]);
        free(m);
        *pp = 0;
    }
    if (nsview_dealloc) ((void (*)(id, SEL))nsview_dealloc)(self, c);
}
static id mtk_device(id s, SEL c) { (void)c; return mk(s)->device; }
static void mtk_set_device(id s, SEL c, id d) {
    (void)c;
    mtk_t *m = mk(s);
    if (d) objc_retain(d);
    if (m->device) objc_release(m->device);
    m->device = d;
    if (m->layer) ml_set_device(m->layer, 0, d);
}
static id mtk_delegate(id s, SEL c) { (void)c; return mk(s)->delegate; }
static void mtk_set_delegate(id s, SEL c, id d) { (void)c; mk(s)->delegate = d; }
static CGS mtk_backing_size(id s) {
    CGR b;
    ((void (*)(CGR *, id, SEL))objc_msgSend_stret)(&b, s, sel_registerName("bounds"));
    id w = MSG0(id, s, "window");
    double sc = w && cx_responds(w->isa, sel_registerName("backingScaleFactor")) ? MSG0(double, w, "backingScaleFactor") : 1;
    if (sc <= 0) sc = 1;
    CGS r = {b.s.w * sc, b.s.h * sc};
    return r;
}
static void mtk_sync_size(id s) {
    mtk_t *m = mk(s);
    if (!m->layer) return;
    CGS z = m->autoresize ? mtk_backing_size(s) : m->size;
    if (z.w < 1 || z.h < 1) return;
    if (z.w != m->size.w || z.h != m->size.h) {
        m->size = z;
        ml_set_size(m->layer, 0, z);
        if (m->delegate && cx_responds(m->delegate->isa, sel_registerName("mtkView:drawableSizeWillChange:")))
            ((void (*)(id, SEL, id, CGS))objc_msgSend)(m->delegate, sel_registerName("mtkView:drawableSizeWillChange:"), s, z);
    }
}
static id mtk_cur_drawable(id s, SEL c) {
    (void)c;
    mtk_t *m = mk(s);
    if (!m->layer) return 0;
    if (!m->drawable) {
        mtk_sync_size(s);
        id d = ml_next(m->layer, 0);
        m->drawable = d ? objc_retain(d) : 0;
    }
    return m->drawable;
}
static id mtk_tex(mtk_t *m, id *slot, u64 fmt, u64 w, u64 h, u64 samples, u64 usage) {
    if (*slot) {
        mtex_t *t = (mtex_t *)*slot;
        if (t->w == w && t->h == h && (u64)t->fmt == fmt && (u64)(t->samples ? t->samples : 1) == samples) return *slot;
        objc_release(*slot);
        *slot = 0;
    }
    (void)m;
    id td = MSG(id, (id)&MTLTextureDescriptor_c, "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", fmt, w, h, (BOOL)NO);
    MSG(void, td, "setUsage:", usage);
    MSG(void, td, "setStorageMode:", (u64)2);
    if (samples > 1) { MSG(void, td, "setTextureType:", (u64)MSL_TEX_2D_MS); MSG(void, td, "setSampleCount:", samples); }
    *slot = tex_new(td);
    return *slot;
}
static id mtk_rpd(id s, SEL c) {
    (void)c;
    mtk_t *m = mk(s);
    id d = mtk_cur_drawable(s, 0);
    if (!d) return 0;
    mtex_t *dt = (mtex_t *)((mdraw_t *)d)->tex;
    if (m->rpd) objc_release(m->rpd);
    m->rpd = d_autoobj(&MTLRenderPassDescriptor_c);
    id ca = da_get((id)dg_u(m->rpd, sel_registerName("colorAttachments")), 0, 0);
    if (m->samples > 1) {
        id ms = mtk_tex(m, &m->msaa, m->cfmt, dt->w, dt->h, m->samples, 4);
        ds_u(ca, sel_registerName("setTexture:"), (u64)ms);
        ds_u(ca, sel_registerName("setResolveTexture:"), (u64)dt);
        ds_u(ca, sel_registerName("setStoreAction:"), 2);
    } else {
        ds_u(ca, sel_registerName("setTexture:"), (u64)dt);
        ds_u(ca, sel_registerName("setStoreAction:"), 1);
    }
    ds_u(ca, sel_registerName("setLoadAction:"), 2);
    ds_c(ca, sel_registerName("setClearColor:"), m->cc);
    if (m->dfmt) {
        id dtx = mtk_tex(m, &m->depth, m->dfmt, dt->w, dt->h, m->samples, m->dusage);
        if (mx_isdepth((int)m->dfmt)) {
            id da = (id)dg_u(m->rpd, sel_registerName("depthAttachment"));
            ds_u(da, sel_registerName("setTexture:"), (u64)dtx);
            ds_u(da, sel_registerName("setLoadAction:"), 2);
            ds_u(da, sel_registerName("setStoreAction:"), 0);
            ds_d(da, sel_registerName("setClearDepth:"), m->cd);
        }
        if (mx_hasstencil((int)m->dfmt)) {
            id sa = (id)dg_u(m->rpd, sel_registerName("stencilAttachment"));
            ds_u(sa, sel_registerName("setTexture:"), (u64)dtx);
            ds_u(sa, sel_registerName("setLoadAction:"), 2);
            ds_u(sa, sel_registerName("setStoreAction:"), 0);
            ds_u(sa, sel_registerName("setClearStencil:"), m->cs);
        }
    }
    return m->rpd;
}
static void mtk_draw(id s, SEL c) {
    (void)c;
    mtk_t *m = mk(s);
    if (m->drawing) return;
    m->drawing = 1;
    mtk_sync_size(s);
    id dl = m->delegate;
    if (dl && cx_responds(dl->isa, sel_registerName("drawInMTKView:"))) MSG(void, dl, "drawInMTKView:", s);
    else {
        CGR b;
        ((void (*)(CGR *, id, SEL))objc_msgSend_stret)(&b, s, sel_registerName("bounds"));
        ((void (*)(id, SEL, CGR))objc_msgSend)(s, sel_registerName("drawRect:"), b);
    }
    if (m->drawable) { objc_release(m->drawable); m->drawable = 0; }
    if (m->rpd) { objc_release(m->rpd); m->rpd = 0; }
    m->pending = 0;
    m->drawing = 0;
}
static void mtk_tick(id self, SEL c, id timer) {
    (void)c; (void)timer;
    mtk_t *m = mk(self);
    if (!m->layer) return;
    if (m->needs_display_mode ? m->pending : !m->paused) mtk_draw(self, 0);
}
static void mtk_set_needs(id s, SEL c, BOOL b) {
    if (nsview_set_needs) ((void (*)(id, SEL, BOOL))nsview_set_needs)(s, c, b);
    mtk_t *m = mk(s);
    if (b && m->needs_display_mode && !g_presenting && !m->drawing) m->pending = 1;
}
static void mtk_draw_rect(id s, SEL c, CGR r) { (void)s; (void)c; (void)r; }
static void mtk_release_drawables(id s, SEL c) { (void)c; mtk_t *m = mk(s); if (m->layer) ml_pool_free(ml(m->layer)); }
#define MTK_U(get, set, field, extra) \
    static u64 get(id s, SEL c) { (void)c; return (u64)mk(s)->field; } \
    static void set(id s, SEL c, u64 v) { (void)c; mtk_t *m = mk(s); m->field = v; extra; }
MTK_U(mtk_cfmt, mtk_set_cfmt, cfmt, if (m->layer) ml(m->layer)->fmt = v)
MTK_U(mtk_dfmt, mtk_set_dfmt, dfmt, (void)0)
MTK_U(mtk_samples, mtk_set_samples, samples, if (!v) m->samples = 1)
MTK_U(mtk_dusage, mtk_set_dusage, dusage, (void)0)
static i64 mtk_fps(id s, SEL c) { (void)c; return (i64)mk(s)->fps; }
static void mtk_set_fps(id s, SEL c, i64 v) {
    (void)c;
    mtk_t *m = mk(s);
    m->fps = v > 0 ? (u64)v : 60;
    if (m->timer) {
        MSG0(void, m->timer, "invalidate");
        objc_release(m->timer);
        m->timer = 0;
        double iv = 1.0 / (double)m->fps;
        id t = MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", iv, s,
                   sel_registerName("_mosMetalTick:"), (id)0, (BOOL)YES);
        m->timer = t ? objc_retain(t) : 0;
    }
}
#define MTK_B(get, set, field) \
    static BOOL get(id s, SEL c) { (void)c; return (BOOL)mk(s)->field; } \
    static void set(id s, SEL c, BOOL v) { (void)c; mk(s)->field = v; }
MTK_B(mtk_paused, mtk_set_paused, paused)
MTK_B(mtk_needs_mode, mtk_set_needs_mode, needs_display_mode)
MTK_B(mtk_autoresize, mtk_set_autoresize, autoresize)
MTK_B(mtk_pwt, mtk_set_pwt, pwt)
static BOOL mtk_fbonly(id s, SEL c) { (void)c; return (BOOL)mk(s)->fbonly; }
static void mtk_set_fbonly(id s, SEL c, BOOL v) { (void)c; mtk_t *m = mk(s); m->fbonly = v; if (m->layer) ml(m->layer)->fbonly = v; }
static mtl_cc_t mtk_cc(id s, SEL c) { (void)c; return mk(s)->cc; }
static void mtk_set_cc(id s, SEL c, mtl_cc_t v) { (void)c; mk(s)->cc = v; }
static double mtk_cd(id s, SEL c) { (void)c; return mk(s)->cd; }
static void mtk_set_cd(id s, SEL c, double v) { (void)c; mk(s)->cd = v; }
static u32 mtk_cs(id s, SEL c) { (void)c; return mk(s)->cs; }
static void mtk_set_cs(id s, SEL c, u32 v) { (void)c; mk(s)->cs = v; }
static CGS mtk_size(id s, SEL c) { (void)c; mtk_t *m = mk(s); if (m->size.w < 1) mtk_sync_size(s); return m->size; }
static void mtk_set_size(id s, SEL c, CGS z) { (void)c; mtk_t *m = mk(s); m->size = z; if (m->layer) ml_set_size(m->layer, 0, z); }
static CGS mtk_pref_size(id s, SEL c) { (void)c; return mtk_backing_size(s); }
static id mtk_depth_tex(id s, SEL c) { (void)c; return mk(s)->depth; }
static id mtk_msaa_tex(id s, SEL c) { (void)c; return mk(s)->msaa; }
static id mtk_cspace(id s, SEL c) { (void)c; mtk_t *m = mk(s); return m->layer ? ml(m->layer)->colorspace : 0; }
static void mtk_set_cspace(id s, SEL c, id cs) { (void)c; mtk_t *m = mk(s); if (m->layer) ml_set_cs(m->layer, 0, cs); }

/* ================================================================ captura, reflexão */
static id cm_shared(id cls, SEL c) {
    (void)c;
    static id one;
    if (!one) one = MSG0(id, MSG0(id, cls, "alloc"), "init");
    return one;
}
static BOOL cm_start(id s, SEL c, id d, id *err) { (void)s; (void)c; (void)d; set_err(err, mk_error("MTLCaptureErrorDomain", 1, "captura de GPU não existe no Metal por software")); return NO; }
static id cm_scope(id s, SEL c, id d) { (void)s; (void)c; (void)d; return new_obj(&MScope_c); }

/* ================================================================ entrada */
void cx_metal_init(void);
static int metal_allowed(void) {
    const char *e = getenv("MOS_METAL");
    if (e && e[0]) return e[0] != '0';
    u32 n = _dyld_image_count();
    for (u32 i = 0; i < n; i++) {
        const char *nm = _dyld_get_image_name(i);
        if (nm && (strstr(nm, "Electron Framework") || strstr(nm, "Chromium Framework") || strstr(nm, "Chromium Embedded Framework") ||
                   strstr(nm, "Google Chrome Framework") || strstr(nm, "Microsoft Edge Framework")))
            return 0;
    }
    return 1;
}
/* um executável sem metadados de ObjC (C puro) não passa pelo boot do runtime, que é quem
 * inicializa a Foundation, o QuartzCore e este arquivo */
void cx_objc_boot(u64 sects, u64 n);
static void metal_boot(void) { if (!g_dev) { cx_objc_boot(0, 0); cx_metal_init(); } }
id MTLCreateSystemDefaultDevice(void) {
    metal_boot();
    if (!g_dev || !metal_allowed()) return 0;
    return objc_retain(g_dev);
}
id MTLCopyAllDevices(void) {
    metal_boot();
    id a = cx_arr_new(cx_class_named("NSMutableArray"));
    if (g_dev && metal_allowed()) cx_arr_add(a, g_dev);
    return a;
}
id MTLCopyAllDevicesWithObserver(id *observer, void *handler) { if (observer) *observer = 0; (void)handler; return MTLCopyAllDevices(); }
void MTLRemoveDeviceObserver(id o) { (void)o; }
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } mt_cfconst_t;
#define CFCONST(var, text) static mt_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
CFCONST(MTLDeviceWasAddedNotification, "MTLDeviceWasAddedNotification")
CFCONST(MTLDeviceRemovalRequestedNotification, "MTLDeviceRemovalRequestedNotification")
CFCONST(MTLDeviceWasRemovedNotification, "MTLDeviceWasRemovedNotification")
CFCONST(MTLCommonCounterSetTimestamp, "timestamp")
CFCONST(MTLCommonCounterTimestamp, "GPUTimestamp")
CFCONST(MTLCommonCounterSetStageUtilization, "stageutilization")
CFCONST(MTLCommonCounterSetStatistic, "statistic")
CFCONST(MTLLibraryErrorDomain, "MTLLibraryErrorDomain")
CFCONST(MTLCommandBufferErrorDomain, "MTLCommandBufferErrorDomain")
CFCONST(MTLCaptureErrorDomain, "MTLCaptureErrorDomain")
CFCONST(MTLCounterErrorDomain, "MTLCounterErrorDomain")
CFCONST(MTLBinaryArchiveDomain, "MTLBinaryArchiveDomain")
CFCONST(MTLDynamicLibraryDomain, "MTLDynamicLibraryDomain")
CFCONST(MTLIOErrorDomain, "MTLIOErrorDomain")
CFCONST(MTLLogStateErrorDomain, "MTLLogStateErrorDomain")
CFCONST(MTLCommandBufferEncoderInfoErrorKey, "MTLCommandBufferEncoderInfoErrorKey")
CFCONST(MTKTextureLoaderOptionSRGB, "MTKTextureLoaderOptionSRGB")
CFCONST(MTKTextureLoaderOptionGenerateMipmaps, "MTKTextureLoaderOptionGenerateMipmaps")
CFCONST(MTKTextureLoaderOptionTextureUsage, "MTKTextureLoaderOptionTextureUsage")
CFCONST(MTKTextureLoaderOptionTextureStorageMode, "MTKTextureLoaderOptionTextureStorageMode")
CFCONST(MTKTextureLoaderOptionOrigin, "MTKTextureLoaderOptionOrigin")
CFCONST(MTKTextureLoaderOriginTopLeft, "MTKTextureLoaderOriginTopLeft")
CFCONST(MTKTextureLoaderOriginBottomLeft, "MTKTextureLoaderOriginBottomLeft")
CFCONST(MTKTextureLoaderOriginFlippedVertically, "MTKTextureLoaderOriginFlippedVertically")

/* ================================================================ MTKTextureLoader */
#include "../../kernel/include/compat_gui.h"
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul);
CFCONST(MTKTextureLoaderOptionAllocateMipmaps, "MTKTextureLoaderOptionAllocateMipmaps")
CFCONST(MTKTextureLoaderOptionTextureCPUCacheMode, "MTKTextureLoaderOptionTextureCPUCacheMode")
CFCONST(MTKTextureLoaderOptionCubeLayout, "MTKTextureLoaderOptionCubeLayout")
CFCONST(MTKTextureLoaderOptionLoadAsArray, "MTKTextureLoaderOptionLoadAsArray")
CFCONST(MTKTextureLoaderCubeLayoutVertical, "MTKTextureLoaderCubeLayoutVertical")
CFCONST(MTKTextureLoaderErrorDomain, "MTKTextureLoaderErrorDomain")
CFCONST(MTKTextureLoaderErrorKey, "MTKTextureLoaderErrorKey")
CLASS(MTKTextureLoader)
typedef struct { cx_class_t *isa; id dev; } mtkl_t;
static int opt_bool(id opts, const void *key, int dflt) {
    id v = opts ? MSG(id, opts, "objectForKey:", (id)key) : 0;
    return v ? MSG0(BOOL, v, "boolValue") != 0 : dflt;
}
static u64 opt_u(id opts, const void *key, u64 dflt) {
    id v = opts ? MSG(id, opts, "objectForKey:", (id)key) : 0;
    return v ? MSG0(u64, v, "unsignedLongLongValue") : dflt;
}
/* pixels AARRGGBB (u32, = BGRA na memória) -> textura BGRA8 */
static id tex_from_argb(const u8 *px, long bpr, int w, int h, id opts, id *err) {
    if (!px || w <= 0 || h <= 0) { set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "imagem inválida")); return 0; }
    int srgb = opt_bool(opts, MTKTextureLoaderOptionSRGB, 1);
    int gen = opt_bool(opts, MTKTextureLoaderOptionGenerateMipmaps, 0);
    int mips = gen || opt_bool(opts, MTKTextureLoaderOptionAllocateMipmaps, 0);
    id org = opts ? MSG(id, opts, "objectForKey:", (id)MTKTextureLoaderOptionOrigin) : 0;
    int flip = org && (!strcmp(cx_cstr(org), "MTKTextureLoaderOriginBottomLeft") || !strcmp(cx_cstr(org), "MTKTextureLoaderOriginFlippedVertically"));
    id td = MSG(id, (id)&MTLTextureDescriptor_c, "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", (u64)(srgb ? 81 : 80), (u64)w, (u64)h, (BOOL)(mips ? YES : NO));
    MSG(void, td, "setUsage:", opt_u(opts, MTKTextureLoaderOptionTextureUsage, 1));
    MSG(void, td, "setStorageMode:", opt_u(opts, MTKTextureLoaderOptionTextureStorageMode, 0));
    mtex_t *t = (mtex_t *)tex_new(td);
    if (!t) { set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "sem memória para a textura")); return 0; }
    for (int y = 0; y < h; y++) memcpy(mx_texel(&t->t, 0, flip ? h - 1 - y : y, 0, 0, 0), px + (u64)y * bpr, (u64)w * 4);
    if (gen) { gpu_lock(); mx_gen_mips(t); gpu_unlock(); }
    return (id)t;
}
static id tex_from_bytes(const u8 *data, u64 n, id opts, id *err) {
    long need = data && n ? SYS4(GUI_DECODE_IMAGE, data, n, 0, 0) : 0;
    if (need <= 8) { set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "formato de imagem não reconhecido")); return 0; }
    u8 *out = malloc((size_t)need);
    if (!out || SYS4(GUI_DECODE_IMAGE, data, n, out, need) != need) { free(out); set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "falha ao decodificar a imagem")); return 0; }
    int w = (int)((u32 *)out)[0], h = (int)((u32 *)out)[1];
    id t = tex_from_argb(out + 8, (long)w * 4, w, h, opts, err);
    free(out);
    return t;
}
static id tl_init(id s, SEL c, id dev) { (void)c; ((mtkl_t *)s)->dev = dev ? objc_retain(dev) : 0; return s; }
static id tl_dev(id s, SEL c) { (void)c; return ((mtkl_t *)s)->dev; }
static void tl_dealloc(id s, SEL c) { (void)c; if (((mtkl_t *)s)->dev) objc_release(((mtkl_t *)s)->dev); super_dealloc(s, cx_class_named("NSObject")); }
static id tl_cgimage(id s, SEL c, void *img, id opts, id *err) {
    (void)s; (void)c;
    const u8 *px;
    long bpr;
    int w, h, pm;
    if (!img || !cx_cgimage_src(img, &px, &bpr, &w, &h, &pm)) { set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "CGImage inválida")); return 0; }
    return tex_from_argb(px, bpr, w, h, opts, err);
}
static id tl_data(id s, SEL c, id data, id opts, id *err) {
    (void)s; (void)c;
    if (!data) return 0;
    return tex_from_bytes(MSG0(const u8 *, data, "bytes"), MSG0(u64, data, "length"), opts, err);
}
static id tl_url(id s, SEL c, id url, id opts, id *err) {
    (void)s; (void)c;
    id p = url ? MSG0(id, url, "path") : 0;
    u64 n = 0;
    char *b = p ? read_file(cx_cstr(p), &n) : 0;
    if (!b) { set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "arquivo não encontrado: %s", p ? cx_cstr(p) : "?")); return 0; }
    id t = tex_from_bytes((const u8 *)b, n, opts, err);
    free(b);
    return t;
}
static id tl_name(id s, SEL c, id name, double scale, id bundle, id opts, id *err) {
    (void)s; (void)c; (void)scale;
    if (!bundle) bundle = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id rp = bundle ? MSG0(id, bundle, "resourcePath") : 0;
    static const char *ext[] = {"", ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga"};
    for (u64 i = 0; rp && name && i < sizeof ext / sizeof *ext; i++) {
        char p[1200];
        snprintf(p, sizeof p, "%s/%s%s", cx_cstr(rp), cx_cstr(name), ext[i]);
        u64 n = 0;
        char *b = read_file(p, &n);
        if (!b) continue;
        id t = tex_from_bytes((const u8 *)b, n, opts, err);
        free(b);
        if (t) return t;
    }
    set_err(err, mk_error("MTKTextureLoaderErrorDomain", 0, "imagem %s não encontrada no bundle", name ? cx_cstr(name) : "?"));
    return 0;
}
static id tl_name_gamut(id s, SEL c, id name, double scale, u64 gamut, id bundle, id opts, id *err) { (void)gamut; return tl_name(s, c, name, scale, bundle, opts, err); }
static id tl_urls(id s, SEL c, id urls, id opts, id *err) {
    id a = cx_arr_new(cx_class_named("NSMutableArray"));
    u64 n = urls ? MSG0(u64, urls, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id t = tl_url(s, c, MSG(id, urls, "objectAtIndex:", i), opts, err);
        if (!t) t = objc_retain(MSG0(id, (id)cx_class_named("NSNull"), "null"));
        cx_arr_add(a, t);
        objc_release(t);
    }
    return objc_autorelease(a);
}
static void tl_url_blk(id s, SEL c, id u, id o, void *blk) { id err = 0; id t = tl_url(s, c, u, o, &err); if (blk) BLK2(blk, t, err); if (t) objc_release(t); }
static void tl_data_blk(id s, SEL c, id d, id o, void *blk) { id err = 0; id t = tl_data(s, c, d, o, &err); if (blk) BLK2(blk, t, err); if (t) objc_release(t); }
static void tl_cgimage_blk(id s, SEL c, void *i, id o, void *blk) { id err = 0; id t = tl_cgimage(s, c, i, o, &err); if (blk) BLK2(blk, t, err); if (t) objc_release(t); }
static void tl_name_blk(id s, SEL c, id n, double sc, id b, id o, void *blk) { id err = 0; id t = tl_name(s, c, n, sc, b, o, &err); if (blk) BLK2(blk, t, err); if (t) objc_release(t); }

/* ================================================================ CVMetalTextureCache */
CFCONST(kCVMetalTextureUsage, "MetalTextureUsage") CFCONST(kCVMetalTextureStorageMode, "MetalTextureStorageMode")
CFCONST(kCVMetalTextureCacheMaximumTextureAgeKey, "MaximumTextureAge")
SCLASS(CVMTCache) SCLASS(CVMTex)
typedef struct { cx_class_t *isa; id dev; } cvmtc_t;
typedef struct { cx_class_t *isa; id tex, pb; } cvmt_t;
id CVPixelBufferGetIOSurface(id pb);
void *CVPixelBufferGetBaseAddress(id pb);
u64 CVPixelBufferGetBytesPerRow(id pb);
void *CVPixelBufferGetBaseAddressOfPlane(id pb, u64 i);
u64 CVPixelBufferGetBytesPerRowOfPlane(id pb, u64 i);
int CVMetalTextureCacheCreate(void *alloc, id cattrs, id dev, id tattrs, id *out) {
    (void)alloc; (void)cattrs; (void)tattrs;
    if (!out) return -6661;   /* kCVReturnInvalidArgument */
    cvmtc_t *c = (cvmtc_t *)cx_alloc_instance(&CVMTCache_c);
    c->dev = dev ? objc_retain(dev) : 0;
    *out = (id)c;
    return 0;
}
int CVMetalTextureCacheCreateTextureFromImage(void *alloc, id cache, id pb, id tattrs, u64 fmt, u64 w, u64 h, u64 plane, id *out) {
    (void)alloc;
    if (!cache || !pb || !out) return -6661;
    *out = 0;
    id td = MSG(id, (id)&MTLTextureDescriptor_c, "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", fmt, w, h, (BOOL)NO);
    id u = tattrs ? MSG(id, tattrs, "objectForKey:", (id)kCVMetalTextureUsage) : 0;
    MSG(void, td, "setUsage:", u ? MSG0(u64, u, "unsignedLongLongValue") : (u64)1);
    id surf = CVPixelBufferGetIOSurface(pb);
    id tex = 0;
    if (surf) tex = dv_newtex_surf(g_dev, 0, td, surf, plane);
    else {
        u8 *base = plane ? CVPixelBufferGetBaseAddressOfPlane(pb, plane) : CVPixelBufferGetBaseAddress(pb);
        u64 bpr = plane ? CVPixelBufferGetBytesPerRowOfPlane(pb, plane) : CVPixelBufferGetBytesPerRow(pb);
        if (base) {
            mtex_t *t = (mtex_t *)new_obj(&MTex_c);
            if (tex_from_desc(t, td)) { t->levels = 1; mx_tex_layout(t, bpr); t->t.data = base; tex = (id)t; }
            else objc_release((id)t);
        }
    }
    if (!tex) return -6683;   /* kCVReturnPixelBufferNotMetalCompatible */
    cvmt_t *m = (cvmt_t *)cx_alloc_instance(&CVMTex_c);
    m->tex = tex;
    m->pb = objc_retain(pb);
    *out = (id)m;
    return 0;
}
id CVMetalTextureGetTexture(id t) { return t ? ((cvmt_t *)t)->tex : 0; }
BOOL CVMetalTextureIsFlipped(id t) { (void)t; return NO; }
void CVMetalTextureGetCleanTexCoords(id t, float *ll, float *lr, float *ur, float *ul) {
    (void)t;
    if (ll) { ll[0] = 0; ll[1] = 1; }
    if (lr) { lr[0] = 1; lr[1] = 1; }
    if (ur) { ur[0] = 1; ur[1] = 0; }
    if (ul) { ul[0] = 0; ul[1] = 0; }
}
void CVMetalTextureCacheFlush(id c, u64 opts) { (void)c; (void)opts; }
u64 CVMetalTextureCacheGetTypeID(void) { return 0x2B7; }
u64 CVMetalTextureGetTypeID(void) { return 0x2B8; }
static void cvmtc_dealloc(id s, SEL c) { (void)c; if (((cvmtc_t *)s)->dev) objc_release(((cvmtc_t *)s)->dev); super_dealloc(s, cx_class_named("NSObject")); }
static void cvmt_dealloc(id s, SEL c) { (void)c; cvmt_t *m = (cvmt_t *)s; if (m->tex) objc_release(m->tex); if (m->pb) objc_release(m->pb); super_dealloc(s, cx_class_named("NSObject")); }
static void tl_register(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&MTKTextureLoader_c, &MTKTextureLoader_m, "MTKTextureLoader", ob, (u32)(sizeof(mtkl_t) - 8));
    M(MTKTextureLoader, "initWithDevice:", tl_init); M(MTKTextureLoader, "device", tl_dev); M(MTKTextureLoader, "dealloc", tl_dealloc);
    M(MTKTextureLoader, "newTextureWithCGImage:options:error:", tl_cgimage);
    M(MTKTextureLoader, "newTextureWithContentsOfURL:options:error:", tl_url);
    M(MTKTextureLoader, "newTextureWithData:options:error:", tl_data);
    M(MTKTextureLoader, "newTextureWithName:scaleFactor:bundle:options:error:", tl_name);
    M(MTKTextureLoader, "newTextureWithName:scaleFactor:displayGamut:bundle:options:error:", tl_name_gamut);
    M(MTKTextureLoader, "newTexturesWithContentsOfURLs:options:error:", tl_urls);
    M(MTKTextureLoader, "newTextureWithContentsOfURL:options:completionHandler:", tl_url_blk);
    M(MTKTextureLoader, "newTextureWithData:options:completionHandler:", tl_data_blk);
    M(MTKTextureLoader, "newTextureWithCGImage:options:completionHandler:", tl_cgimage_blk);
    M(MTKTextureLoader, "newTextureWithName:scaleFactor:bundle:options:completionHandler:", tl_name_blk);
    cx_define_class(&CVMTCache_c, &CVMTCache_m, "CVMetalTextureCache", ob, (u32)(sizeof(cvmtc_t) - 8));
    M(CVMTCache, "dealloc", cvmtc_dealloc);
    cx_define_class(&CVMTex_c, &CVMTex_m, "CVMetalTexture", ob, (u32)(sizeof(cvmt_t) - 8));
    M(CVMTex, "dealloc", cvmt_dealloc);
}

/* ================================================================ MetalPerformanceShaders (escala de imagem) */
CLASS(MPSKernel) CLASS(MPSUnaryImageKernel) CLASS(MPSImageScale) CLASS(MPSImageBilinearScale) CLASS(MPSImageLanczosScale)
typedef struct { MHDR; u64 opts; int has_t, lanczos; double t[4]; } mps_t;
BOOL MPSSupportsMTLDevice(id dev) { return dev && dev == g_dev; }
static id mps_init(id s, SEL c, id dev) {
    (void)c;
    mps_t *k = (mps_t *)s;
    k->dev = dev;
    k->lanczos = cx_is_subclass(s->isa, &MPSImageLanczosScale_c);
    return s;
}
static const double *mps_get_t(id s, SEL c) { (void)c; mps_t *k = (mps_t *)s; return k->has_t ? k->t : 0; }
static void mps_set_t(id s, SEL c, const double *t) { (void)c; mps_t *k = (mps_t *)s; k->has_t = t != 0; if (t) memcpy(k->t, t, sizeof k->t); }
static u64 mps_opts(id s, SEL c) { (void)c; return ((mps_t *)s)->opts; }
static void mps_set_opts(id s, SEL c, u64 o) { (void)c; ((mps_t *)s)->opts = o; }
static void mps_encode(id s, SEL c, id cb, id src, id dst) {
    (void)c;
    mcb_t *b = (mcb_t *)cb;
    mps_t *k = (mps_t *)s;
    if (!b || !src || !dst || b->status >= 2) return;
    if (b->enc) { enc_end((menc_t *)b->enc); objc_release(b->enc); b->enc = 0; }
    cmd_t *x = cb_cmd(b, C_SCALE);
    if (!x) return;
    cb_retain(b, src);
    cb_retain(b, dst);
    x->sc.s = (mtex_t *)src;
    x->sc.d = (mtex_t *)dst;
    x->sc.has_t = k->has_t;
    x->sc.lanczos = k->lanczos;
    memcpy(x->sc.t, k->t, sizeof x->sc.t);
}
static void mps_encode_inplace(id s, SEL c, id cb, id *tex, void *fallback) { (void)s; (void)c; (void)cb; (void)tex; (void)fallback; }
static BOOL mps_encode_inplace_b(id s, SEL c, id cb, id *tex, void *fallback) { (void)s; (void)c; (void)cb; (void)tex; (void)fallback; return NO; }
static void mps_register(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&MPSKernel_c, &MPSKernel_m, "MPSKernel", &MObj_c, (u32)(sizeof(mps_t) - sizeof(mobj_t)));
    M(MPSKernel, "initWithDevice:", mps_init); M(MPSKernel, "options", mps_opts); M(MPSKernel, "setOptions:", mps_set_opts);
    cx_define_class(&MPSUnaryImageKernel_c, &MPSUnaryImageKernel_m, "MPSUnaryImageKernel", &MPSKernel_c, 0);
    M(MPSUnaryImageKernel, "encodeToCommandBuffer:sourceTexture:destinationTexture:", mps_encode);
    M(MPSUnaryImageKernel, "encodeToCommandBuffer:inPlaceTexture:fallbackCopyAllocator:", mps_encode_inplace_b);
    cx_define_class(&MPSImageScale_c, &MPSImageScale_m, "MPSImageScale", &MPSUnaryImageKernel_c, 0);
    M(MPSImageScale, "scaleTransform", mps_get_t); M(MPSImageScale, "setScaleTransform:", mps_set_t);
    cx_define_class(&MPSImageBilinearScale_c, &MPSImageBilinearScale_m, "MPSImageBilinearScale", &MPSImageScale_c, 0);
    cx_define_class(&MPSImageLanczosScale_c, &MPSImageLanczosScale_m, "MPSImageLanczosScale", &MPSImageScale_c, 0);
    (void)ob;
    (void)mps_encode_inplace;
}

/* ---- registro */
static void def_obj(cx_class_t *c, cx_class_t *m, const char *name, u32 size) {
    cx_define_class(c, m, name, &MObj_c, size - 8);
    cx_add_method(c, "dealloc", (void *)o_dealloc);
}
#define DESC(N, ps) def_desc(&N##_c, &N##_m, #N, ps)
#define DARR(N, E) def_darr(&N##_c, &N##_m, #N, &E##_c)
static void desc_arr_class(pspec_t *ps, const char *name, cx_class_t *cls) {
    for (int i = 0; ps[i].name; i++) if (!strcmp(ps[i].name, name)) ps[i].cls = cls;
}
void cx_metal_init(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_class_t *ob = cx_class_named("NSObject");
    cx_class_t *nserr = cx_class_named("NSError");
    u32 ebase = cx_instance_size(nserr ? nserr : ob);
    cx_define_class(&MErr_c, &MErr_m, "MTLSoftError", nserr ? nserr : ob, sizeof(merr_t) > ebase ? (u32)(sizeof(merr_t) - ebase) : 0);
    M(MErr, "localizedDescription", e_desc); M(MErr, "description", e_desc); M(MErr, "domain", e_domain);
    M(MErr, "code", e_code); M(MErr, "userInfo", e_info); M(MErr, "dealloc", e_dealloc);
    M(MErr, "localizedFailureReason", e_desc);

    /* descritores (elementos antes dos arrays) */
    DESC(MTLTextureDescriptor, ps_texdesc);
    CM(MTLTextureDescriptor, "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", td_2d);
    CM(MTLTextureDescriptor, "textureCubeDescriptorWithPixelFormat:size:mipmapped:", td_cube);
    CM(MTLTextureDescriptor, "textureBufferDescriptorWithPixelFormat:width:resourceOptions:usage:", td_buffer);
    DESC(MTLSamplerDescriptor, ps_sampdesc);
    DESC(MTLStencilDescriptor, ps_stencil);
    for (int i = 0; ps_dssdesc[i].name; i++) if (ps_dssdesc[i].ty == 'O') ps_dssdesc[i].cls = &MTLStencilDescriptor_c;
    DESC(MTLDepthStencilDescriptor, ps_dssdesc);
    DESC(MTLRenderPassAttachmentDescriptor, ps_att);
    DESC(MTLRenderPassColorAttachmentDescriptor, ps_catt);
    MTLRenderPassColorAttachmentDescriptor_c.super = &MTLRenderPassAttachmentDescriptor_c;
    DESC(MTLRenderPassDepthAttachmentDescriptor, ps_datt);
    MTLRenderPassDepthAttachmentDescriptor_c.super = &MTLRenderPassAttachmentDescriptor_c;
    DESC(MTLRenderPassStencilAttachmentDescriptor, ps_satt);
    MTLRenderPassStencilAttachmentDescriptor_c.super = &MTLRenderPassAttachmentDescriptor_c;
    DARR(MTLRenderPassColorAttachmentDescriptorArray, MTLRenderPassColorAttachmentDescriptor);
    DESC(MTLRenderPassSampleBufferAttachmentDescriptor, ps_sbatt);
    DARR(MTLRenderPassSampleBufferAttachmentDescriptorArray, MTLRenderPassSampleBufferAttachmentDescriptor);
    DESC(MTLComputePassSampleBufferAttachmentDescriptor, ps_sbatt);
    DARR(MTLComputePassSampleBufferAttachmentDescriptorArray, MTLComputePassSampleBufferAttachmentDescriptor);
    DESC(MTLBlitPassSampleBufferAttachmentDescriptor, ps_sbatt);
    DARR(MTLBlitPassSampleBufferAttachmentDescriptorArray, MTLBlitPassSampleBufferAttachmentDescriptor);
    desc_arr_class(ps_rpdesc, "colorAttachments", &MTLRenderPassColorAttachmentDescriptorArray_c);
    desc_arr_class(ps_rpdesc, "depthAttachment", &MTLRenderPassDepthAttachmentDescriptor_c);
    desc_arr_class(ps_rpdesc, "stencilAttachment", &MTLRenderPassStencilAttachmentDescriptor_c);
    desc_arr_class(ps_rpdesc, "sampleBufferAttachments", &MTLRenderPassSampleBufferAttachmentDescriptorArray_c);
    DESC(MTLRenderPassDescriptor, ps_rpdesc);
    CM(MTLRenderPassDescriptor, "renderPassDescriptor", rp_desc);
    DESC(MTLRenderPipelineColorAttachmentDescriptor, ps_rpca);
    DARR(MTLRenderPipelineColorAttachmentDescriptorArray, MTLRenderPipelineColorAttachmentDescriptor);
    DESC(MTLPipelineBufferDescriptor, ps_pbuf);
    DARR(MTLPipelineBufferDescriptorArray, MTLPipelineBufferDescriptor);
    DESC(MTLLinkedFunctions, ps_linked);
    desc_arr_class(ps_rpipedesc, "colorAttachments", &MTLRenderPipelineColorAttachmentDescriptorArray_c);
    desc_arr_class(ps_rpipedesc, "vertexBuffers", &MTLPipelineBufferDescriptorArray_c);
    desc_arr_class(ps_rpipedesc, "fragmentBuffers", &MTLPipelineBufferDescriptorArray_c);
    desc_arr_class(ps_rpipedesc, "vertexLinkedFunctions", &MTLLinkedFunctions_c);
    desc_arr_class(ps_rpipedesc, "fragmentLinkedFunctions", &MTLLinkedFunctions_c);
    DESC(MTLRenderPipelineDescriptor, ps_rpipedesc);
    DESC(MTLVertexAttributeDescriptor, ps_vattr);
    DARR(MTLVertexAttributeDescriptorArray, MTLVertexAttributeDescriptor);
    DESC(MTLVertexBufferLayoutDescriptor, ps_vlayout);
    DARR(MTLVertexBufferLayoutDescriptorArray, MTLVertexBufferLayoutDescriptor);
    desc_arr_class(ps_vdesc, "attributes", &MTLVertexAttributeDescriptorArray_c);
    desc_arr_class(ps_vdesc, "layouts", &MTLVertexBufferLayoutDescriptorArray_c);
    DESC(MTLVertexDescriptor, ps_vdesc);
    CM(MTLVertexDescriptor, "vertexDescriptor", dnew_ar);
    desc_arr_class(ps_cpipedesc, "buffers", &MTLPipelineBufferDescriptorArray_c);
    desc_arr_class(ps_cpipedesc, "linkedFunctions", &MTLLinkedFunctions_c);
    DESC(MTLComputePipelineDescriptor, ps_cpipedesc);
    DESC(MTLCompileOptions, ps_copts);
    DESC(MTLFunctionConstantValues, ps_none);
    M(MTLFunctionConstantValues, "setConstantValue:type:atIndex:", fcv_noop);
    M(MTLFunctionConstantValues, "setConstantValue:type:withName:", fcv_noop);
    M(MTLFunctionConstantValues, "setConstantValues:type:withRange:", fcv_noop);
    DESC(MTLFunctionDescriptor, ps_fdesc);
    CM(MTLFunctionDescriptor, "functionDescriptor", dnew_ar);
    DESC(MTLHeapDescriptor, ps_heapdesc);
    DESC(MTLCommandBufferDescriptor, ps_cbdesc);
    desc_arr_class(ps_cpass, "sampleBufferAttachments", &MTLComputePassSampleBufferAttachmentDescriptorArray_c);
    DESC(MTLComputePassDescriptor, ps_cpass);
    CM(MTLComputePassDescriptor, "computePassDescriptor", dnew_ar);
    desc_arr_class(ps_bpass, "sampleBufferAttachments", &MTLBlitPassSampleBufferAttachmentDescriptorArray_c);
    DESC(MTLBlitPassDescriptor, ps_bpass);
    CM(MTLBlitPassDescriptor, "blitPassDescriptor", dnew_ar);
    DESC(MTLCaptureDescriptor, ps_capdesc);
    DESC(MTLArgumentDescriptor, ps_argdesc);
    CM(MTLArgumentDescriptor, "argumentDescriptor", dnew_ar);
    DESC(MTLIndirectCommandBufferDescriptor, ps_icbdesc);
    DESC(MTLAttributeDescriptor, ps_vattr);
    DARR(MTLAttributeDescriptorArray, MTLAttributeDescriptor);
    DESC(MTLBufferLayoutDescriptor, ps_vlayout);
    DARR(MTLBufferLayoutDescriptorArray, MTLBufferLayoutDescriptor);
    desc_arr_class(ps_siodesc, "attributes", &MTLAttributeDescriptorArray_c);
    desc_arr_class(ps_siodesc, "layouts", &MTLBufferLayoutDescriptorArray_c);
    DESC(MTLStageInputOutputDescriptor, ps_siodesc);
    CM(MTLStageInputOutputDescriptor, "stageInputOutputDescriptor", dnew_ar);
    DESC(MTLCounterSampleBufferDescriptor, ps_csbdesc);
    DESC(MTLBinaryArchiveDescriptor, ps_badesc);
    DESC(MTLRasterizationRateMapDescriptor, ps_rrmdesc);
    DESC(MTLResidencySetDescriptor, ps_rsdesc);
    DESC(MTLTileRenderPipelineColorAttachmentDescriptor, ps_tileca);
    DARR(MTLTileRenderPipelineColorAttachmentDescriptorArray, MTLTileRenderPipelineColorAttachmentDescriptor);
    desc_arr_class(ps_tiledesc, "colorAttachments", &MTLTileRenderPipelineColorAttachmentDescriptorArray_c);
    DESC(MTLTileRenderPipelineDescriptor, ps_tiledesc);
    DESC(MTLRenderPassDescriptorArrayDummy, ps_none);
    /* reflexão (vazios) */
    DESC(MTLVertexAttribute, ps_none);
    DESC(MTLAttribute, ps_none);
    DESC(MTLArgument, ps_none);
    DESC(MTLRenderPipelineReflection, ps_none);
    DESC(MTLComputePipelineReflection, ps_none);
    DESC(MTLSharedEventListener, ps_none);
    M(MTLSharedEventListener, "initWithDispatchQueue:", ret_self);
    DESC(MTLSharedEventHandle, ps_none);
    M(MTLSharedEventHandle, "label", ret_nil);
    cx_define_class(&MTLCaptureManager_c, &MTLCaptureManager_m, "MTLCaptureManager", ob, 0);
    CM(MTLCaptureManager, "sharedCaptureManager", cm_shared);
    M(MTLCaptureManager, "startCaptureWithDescriptor:error:", cm_start);
    M(MTLCaptureManager, "startCaptureWithDevice:", noop);
    M(MTLCaptureManager, "startCaptureWithCommandQueue:", noop);
    M(MTLCaptureManager, "startCaptureWithScope:", noop);
    M(MTLCaptureManager, "stopCapture", noop);
    M(MTLCaptureManager, "isCapturing", ret_no);
    M(MTLCaptureManager, "supportsDestination:", ret_no);
    M(MTLCaptureManager, "newCaptureScopeWithDevice:", cm_scope);
    M(MTLCaptureManager, "newCaptureScopeWithCommandQueue:", cm_scope);
    M(MTLCaptureManager, "defaultCaptureScope", ret_nil);
    M(MTLCaptureManager, "setDefaultCaptureScope:", noop);

    /* objetos */
    cx_define_class(&MObj_c, &MObj_m, "MTLSoftObject", ob, (u32)(sizeof(mobj_t) - 8));
    M(MObj, "label", o_label); M(MObj, "setLabel:", o_set_label); M(MObj, "device", o_device); M(MObj, "dealloc", o_dealloc);
    M(MObj, "pushDebugGroup:", noop); M(MObj, "popDebugGroup", noop); M(MObj, "insertDebugSignpost:", noop);
    M(MObj, "useResource:usage:", noop); M(MObj, "useResource:usage:stages:", noop); M(MObj, "useResources:count:usage:", noop);
    M(MObj, "useResources:count:usage:stages:", noop); M(MObj, "useHeap:", noop); M(MObj, "useHeaps:count:", noop);
    M(MObj, "useHeap:stages:", noop); M(MObj, "useHeaps:count:stages:", noop);
    M(MObj, "updateFence:", noop); M(MObj, "waitForFence:", noop); M(MObj, "updateFence:afterStages:", noop);
    M(MObj, "waitForFence:beforeStages:", noop); M(MObj, "memoryBarrierWithScope:", noop);
    M(MObj, "memoryBarrierWithResources:count:", noop); M(MObj, "memoryBarrierWithScope:afterStages:beforeStages:", noop);
    M(MObj, "memoryBarrierWithResources:count:afterStages:beforeStages:", noop); M(MObj, "textureBarrier", noop);
    M(MObj, "makeAliasable", noop); M(MObj, "isAliasable", ret_no); M(MObj, "setPurgeableState:", bf_purge);
    M(MObj, "addDebugMarker:range:", noop); M(MObj, "removeAllDebugMarkers", noop); M(MObj, "insertDebugCaptureBoundary", noop);
    M(MObj, "addResidencySet:", noop); M(MObj, "removeResidencySet:", noop); M(MObj, "addResidencySets:count:", noop);
    M(MObj, "removeResidencySets:count:", noop); M(MObj, "useResidencySet:", noop); M(MObj, "useResidencySets:count:", noop);
    M(MObj, "sampleCountersInBuffer:atSampleIndex:withBarrier:", noop); M(MObj, "beginScope", noop); M(MObj, "endScope", noop);

    def_obj(&MDev_c, &MDev_m, "MTLSoftDevice", sizeof(mdev_t));
    M(MDev, "name", dv_name); M(MDev, "maxThreadsPerThreadgroup", dv_maxtpt); M(MDev, "maxThreadgroupMemoryLength", dv_maxtgmem);
    M(MDev, "maxBufferLength", dv_maxbuf); M(MDev, "recommendedMaxWorkingSetSize", dv_wset); M(MDev, "currentAllocatedSize", dv_alloc);
    M(MDev, "registryID", dv_regid); M(MDev, "isLowPower", ret_no); M(MDev, "isHeadless", ret_no); M(MDev, "isRemovable", ret_no);
    M(MDev, "hasUnifiedMemory", ret_yes); M(MDev, "location", ret_zero); M(MDev, "locationNumber", ret_zero);
    M(MDev, "maxTransferRate", ret_zero); M(MDev, "peerGroupID", ret_zero); M(MDev, "peerIndex", ret_zero); M(MDev, "peerCount", ret_one);
    M(MDev, "supportsFamily:", dv_family); M(MDev, "supportsFeatureSet:", dv_fset); M(MDev, "supportsTextureSampleCount:", dv_samples);
    M(MDev, "argumentBuffersSupport", dv_tier); M(MDev, "readWriteTextureSupport", ret_one);
    M(MDev, "areRasterOrderGroupsSupported", ret_no); M(MDev, "areProgrammableSamplePositionsSupported", ret_no);
    M(MDev, "isDepth24Stencil8PixelFormatSupported", ret_yes); M(MDev, "supports32BitFloatFiltering", ret_yes);
    M(MDev, "supports32BitMSAA", ret_yes); M(MDev, "supportsBCTextureCompression", ret_no); M(MDev, "supportsRaytracing", ret_no);
    M(MDev, "supportsDynamicLibraries", ret_no); M(MDev, "supportsFunctionPointers", ret_no); M(MDev, "supportsRenderDynamicLibraries", ret_no);
    M(MDev, "supportsPullModelInterpolation", ret_no); M(MDev, "supportsQueryTextureLOD", ret_no); M(MDev, "supportsShaderBarycentricCoordinates", ret_no);
    M(MDev, "areBarycentricCoordsSupported", ret_no); M(MDev, "supportsPrimitiveMotionBlur", ret_no); M(MDev, "supportsRaytracingFromRender", ret_no);
    M(MDev, "supportsFunctionPointersFromRender", ret_no); M(MDev, "supportsVertexAmplificationCount:", ret_no);
    M(MDev, "supportsCounterSampling:", ret_no); M(MDev, "supportsRasterizationRateMapWithLayerCount:", ret_no);
    M(MDev, "maxArgumentBufferSamplerCount", ret_zero); M(MDev, "sparseTileSizeInBytes", ret_zero); M(MDev, "maximumConcurrentCompilationTaskCount", ret_one);
    M(MDev, "shouldMaximizeConcurrentCompilation", ret_no); M(MDev, "setShouldMaximizeConcurrentCompilation:", noop);
    M(MDev, "minimumLinearTextureAlignmentForPixelFormat:", dv_lin_align); M(MDev, "minimumTextureBufferAlignmentForPixelFormat:", dv_lin_align);
    M(MDev, "counterSets", ret_empty_arr); M(MDev, "sampleTimestamps:gpuTimestamp:", dv_sample_ts); M(MDev, "architecture", dv_arch);
    M(MDev, "getDefaultSamplePositions:count:", noop);
    M(MDev, "newBufferWithLength:options:", dv_newbuf); M(MDev, "newBufferWithBytes:length:options:", dv_newbuf_bytes);
    M(MDev, "newBufferWithBytesNoCopy:length:options:deallocator:", dv_newbuf_nocopy);
    M(MDev, "newTextureWithDescriptor:", dv_newtex); M(MDev, "newTextureWithDescriptor:iosurface:plane:", dv_newtex_surf);
    M(MDev, "newSharedTextureWithDescriptor:", dv_newtex);
    M(MDev, "newSamplerStateWithDescriptor:", dv_newsamp); M(MDev, "newDepthStencilStateWithDescriptor:", dv_newdss);
    M(MDev, "newCommandQueue", dv_newqueue); M(MDev, "newCommandQueueWithMaxCommandBufferCount:", dv_newqueue_n);
    M(MDev, "newCommandQueueWithDescriptor:", dv_newqueue_n);
    M(MDev, "newLibraryWithSource:options:error:", dv_newlib_src); M(MDev, "newLibraryWithSource:options:completionHandler:", dv_newlib_src_blk);
    M(MDev, "newDefaultLibrary", dv_deflib); M(MDev, "newDefaultLibraryWithBundle:error:", dv_deflib_bundle);
    M(MDev, "newLibraryWithURL:error:", dv_newlib_url); M(MDev, "newLibraryWithFile:error:", dv_newlib_file);
    M(MDev, "newLibraryWithData:error:", dv_newlib_data);
    M(MDev, "newRenderPipelineStateWithDescriptor:error:", dv_newrp);
    M(MDev, "newRenderPipelineStateWithDescriptor:options:reflection:error:", dv_newrp_refl);
    M(MDev, "newRenderPipelineStateWithDescriptor:completionHandler:", dv_newrp_blk);
    M(MDev, "newRenderPipelineStateWithDescriptor:options:completionHandler:", dv_newrp_refl_blk);
    M(MDev, "newComputePipelineStateWithFunction:error:", dv_newcp);
    M(MDev, "newComputePipelineStateWithFunction:options:reflection:error:", dv_newcp_refl);
    M(MDev, "newComputePipelineStateWithFunction:completionHandler:", dv_newcp_blk);
    M(MDev, "newComputePipelineStateWithFunction:options:completionHandler:", dv_newcp_refl_blk);
    M(MDev, "newComputePipelineStateWithDescriptor:options:reflection:error:", dv_newcp_desc);
    M(MDev, "newHeapWithDescriptor:", dv_newheap);
    M(MDev, "heapTextureSizeAndAlignWithDescriptor:", dv_heap_tex_sa); M(MDev, "heapBufferSizeAndAlignWithLength:options:", dv_heap_buf_sa);
    M(MDev, "newFence", dv_newfence); M(MDev, "newEvent", dv_newevent); M(MDev, "newSharedEvent", dv_newsevent);
    M(MDev, "newSharedEventWithHandle:", dv_newsevent_h);
    M(MDev, "newIndirectCommandBufferWithDescriptor:maxCommandCount:options:", ret_nil);
    M(MDev, "newCounterSampleBufferWithDescriptor:error:", ret_nil); M(MDev, "newArgumentEncoderWithArguments:", ret_nil);
    M(MDev, "newBinaryArchiveWithDescriptor:error:", ret_nil); M(MDev, "newRasterizationRateMapWithDescriptor:", ret_nil);
    M(MDev, "newDynamicLibrary:error:", ret_nil); M(MDev, "newDynamicLibraryWithURL:error:", ret_nil);
    M(MDev, "newResidencySetWithDescriptor:error:", ret_nil); M(MDev, "newLogStateWithDescriptor:error:", ret_nil);
    def_obj(&MArch_c, &MArch_m, "MTLSoftArchitecture", sizeof(mobj_t));
    M(MArch, "name", ar_name);

    def_obj(&MBuf_c, &MBuf_m, "MTLSoftBuffer", sizeof(mbuf_t));
    M(MBuf, "dealloc", buf_dealloc); M(MBuf, "contents", bf_contents); M(MBuf, "length", bf_length); M(MBuf, "allocatedSize", bf_length);
    M(MBuf, "gpuAddress", bf_gpuaddr); M(MBuf, "resourceOptions", bf_opts); M(MBuf, "storageMode", bf_storage);
    M(MBuf, "cpuCacheMode", bf_cache); M(MBuf, "hazardTrackingMode", bf_hazard); M(MBuf, "heap", bf_heap); M(MBuf, "heapOffset", ret_zero);
    M(MBuf, "didModifyRange:", noop); M(MBuf, "newTextureWithDescriptor:offset:bytesPerRow:", bf_newtex);
    M(MBuf, "newRemoteBufferViewForDevice:", ret_nil); M(MBuf, "remoteStorageBuffer", ret_nil);

    def_obj(&MTex_c, &MTex_m, "MTLSoftTexture", sizeof(mtex_t));
    M(MTex, "dealloc", tex_dealloc); M(MTex, "width", tx_w); M(MTex, "height", tx_h); M(MTex, "depth", tx_d);
    M(MTex, "pixelFormat", tx_fmt); M(MTex, "textureType", tx_type); M(MTex, "mipmapLevelCount", tx_levels);
    M(MTex, "arrayLength", tx_arr); M(MTex, "sampleCount", tx_samples); M(MTex, "usage", tx_usage); M(MTex, "storageMode", tx_storage);
    M(MTex, "iosurface", tx_iosurf); M(MTex, "iosurfacePlane", tx_plane); M(MTex, "buffer", tx_buffer); M(MTex, "bufferOffset", tx_boff);
    M(MTex, "bufferBytesPerRow", tx_bbpr); M(MTex, "parentTexture", tx_parent); M(MTex, "rootResource", tx_parent);
    M(MTex, "isFramebufferOnly", tx_fbonly); M(MTex, "framebufferOnly", tx_fbonly); M(MTex, "swizzle", tx_swz);
    M(MTex, "gpuResourceID", tx_rid); M(MTex, "resourceOptions", tx_opts); M(MTex, "heap", tx_heap); M(MTex, "allowGPUOptimizedContents", ret_yes);
    M(MTex, "isShareable", ret_no); M(MTex, "isSparse", ret_no); M(MTex, "compressionType", ret_zero); M(MTex, "cpuCacheMode", ret_zero);
    M(MTex, "hazardTrackingMode", ret_zero); M(MTex, "parentRelativeLevel", ret_zero); M(MTex, "parentRelativeSlice", ret_zero);
    M(MTex, "firstMipmapInTail", ret_zero); M(MTex, "tailSizeInBytes", ret_zero); M(MTex, "allocatedSize", ret_zero);
    M(MTex, "replaceRegion:mipmapLevel:withBytes:bytesPerRow:", tx_replace);
    M(MTex, "replaceRegion:mipmapLevel:slice:withBytes:bytesPerRow:bytesPerImage:", tx_replace2);
    M(MTex, "getBytes:bytesPerRow:fromRegion:mipmapLevel:", tx_getbytes);
    M(MTex, "getBytes:bytesPerRow:bytesPerImage:fromRegion:mipmapLevel:slice:", tx_getbytes2);
    M(MTex, "newTextureViewWithPixelFormat:", tx_view); M(MTex, "newTextureViewWithPixelFormat:textureType:levels:slices:", tx_view2);
    M(MTex, "newTextureViewWithPixelFormat:textureType:levels:slices:swizzle:", tx_view3);
    M(MTex, "newSharedTextureHandle", ret_nil); M(MTex, "newRemoteTextureViewForDevice:", ret_nil);

    def_obj(&MSamp_c, &MSamp_m, "MTLSoftSamplerState", sizeof(msamp_t));
    M(MSamp, "gpuResourceID", rp_rid);
    def_obj(&MDss_c, &MDss_m, "MTLSoftDepthStencilState", sizeof(mdss_t));
    def_obj(&MLib_c, &MLib_m, "MTLSoftLibrary", sizeof(mlib_t));
    M(MLib, "dealloc", lib_dealloc); M(MLib, "functionNames", lb_names); M(MLib, "newFunctionWithName:", lb_newfn);
    M(MLib, "newFunctionWithName:constantValues:error:", lb_newfn_cv);
    M(MLib, "newFunctionWithName:constantValues:completionHandler:", lb_newfn_cv_blk);
    M(MLib, "newFunctionWithDescriptor:error:", lb_newfn_desc); M(MLib, "newIntersectionFunctionWithDescriptor:error:", lb_newfn_desc);
    M(MLib, "type", lb_type); M(MLib, "installName", ret_nil);
    def_obj(&MFunc_c, &MFunc_m, "MTLSoftFunction", sizeof(mfunc_t));
    M(MFunc, "dealloc", fn_dealloc); M(MFunc, "name", fn_name); M(MFunc, "functionType", fn_type);
    M(MFunc, "vertexAttributes", ret_empty_arr); M(MFunc, "stageInputAttributes", ret_empty_arr);
    M(MFunc, "functionConstantsDictionary", ret_empty_dict); M(MFunc, "patchType", ret_zero);
    M(MFunc, "patchControlPointCount", fn_cpcount); M(MFunc, "options", ret_zero);
    M(MFunc, "newArgumentEncoderWithBufferIndex:", ret_nil); M(MFunc, "newArgumentEncoderWithBufferIndex:reflection:", ret_nil);
    def_obj(&MRPipe_c, &MRPipe_m, "MTLSoftRenderPipelineState", sizeof(mrpipe_t));
    M(MRPipe, "dealloc", rp_dealloc); M(MRPipe, "maxTotalThreadsPerThreadgroup", rp_maxthreads);
    M(MRPipe, "supportIndirectCommandBuffers", ret_no); M(MRPipe, "gpuResourceID", rp_rid);
    M(MRPipe, "threadgroupSizeMatchesTileSize", ret_no); M(MRPipe, "imageblockSampleLength", ret_zero);
    def_obj(&MCPipe_c, &MCPipe_m, "MTLSoftComputePipelineState", sizeof(mcpipe_t));
    M(MCPipe, "dealloc", cp_dealloc); M(MCPipe, "maxTotalThreadsPerThreadgroup", cp_maxthreads);
    M(MCPipe, "threadExecutionWidth", cp_width); M(MCPipe, "staticThreadgroupMemoryLength", cp_tgmem);
    M(MCPipe, "imageblockMemoryLengthForDimensions:", cp_imgmem); M(MCPipe, "supportIndirectCommandBuffers", ret_no);
    M(MCPipe, "gpuResourceID", rp_rid);
    def_obj(&MHeap_c, &MHeap_m, "MTLSoftHeap", sizeof(mheap_t));
    M(MHeap, "newBufferWithLength:options:", hp_newbuf); M(MHeap, "newBufferWithLength:options:offset:", hp_newbuf_off);
    M(MHeap, "newTextureWithDescriptor:", hp_newtex); M(MHeap, "newTextureWithDescriptor:offset:", hp_newtex_off);
    M(MHeap, "size", hp_size); M(MHeap, "usedSize", hp_used); M(MHeap, "currentAllocatedSize", hp_size);
    M(MHeap, "maxAvailableSizeWithAlignment:", hp_avail); M(MHeap, "type", hp_type); M(MHeap, "storageMode", hp_storage);
    M(MHeap, "cpuCacheMode", ret_zero); M(MHeap, "hazardTrackingMode", ret_zero); M(MHeap, "resourceOptions", hp_opts);
    def_obj(&MFence_c, &MFence_m, "MTLSoftFence", sizeof(mobj_t));
    def_obj(&MEvent_c, &MEvent_m, "MTLSoftEvent", sizeof(mevent_t));
    M(MEvent, "signaledValue", ev_get); M(MEvent, "setSignaledValue:", ev_set);
    cx_define_class(&MSEvent_c, &MSEvent_m, "MTLSoftSharedEvent", &MEvent_c, 0);
    M(MSEvent, "notifyListener:atValue:block:", ev_notify); M(MSEvent, "newSharedEventHandle", ev_handle);
    M(MSEvent, "waitUntilSignaledValue:timeoutMS:", ev_wait);
    def_obj(&MScope_c, &MScope_m, "MTLSoftCaptureScope", sizeof(mobj_t));

    def_obj(&MQueue_c, &MQueue_m, "MTLSoftCommandQueue", sizeof(mqueue_t));
    M(MQueue, "commandBuffer", q_cb); M(MQueue, "commandBufferWithUnretainedReferences", q_cb);
    M(MQueue, "commandBufferWithDescriptor:", q_cb_desc);
    def_obj(&MCB_c, &MCB_m, "MTLSoftCommandBuffer", sizeof(mcb_t));
    M(MCB, "dealloc", cb_dealloc); M(MCB, "status", cb_status); M(MCB, "commandQueue", cb_queue); M(MCB, "error", ret_nil);
    M(MCB, "retainedReferences", ret_yes); M(MCB, "errorOptions", ret_zero); M(MCB, "logs", ret_empty_arr);
    M(MCB, "kernelStartTime", cb_t0); M(MCB, "kernelEndTime", cb_t1); M(MCB, "GPUStartTime", cb_t0); M(MCB, "GPUEndTime", cb_t1);
    M(MCB, "enqueue", cb_enqueue); M(MCB, "commit", cb_commit); M(MCB, "waitUntilScheduled", noop); M(MCB, "waitUntilCompleted", noop);
    M(MCB, "addScheduledHandler:", cb_add_sched); M(MCB, "addCompletedHandler:", cb_add_done);
    M(MCB, "presentDrawable:", cb_present); M(MCB, "presentDrawable:atTime:", cb_present_t);
    M(MCB, "presentDrawable:afterMinimumDuration:", cb_present_t);
    M(MCB, "encodeSignalEvent:value:", cb_signal); M(MCB, "encodeWaitForEvent:value:", noop);
    M(MCB, "renderCommandEncoderWithDescriptor:", cb_renc); M(MCB, "computeCommandEncoder", cb_cenc);
    M(MCB, "computeCommandEncoderWithDispatchType:", cb_cenc_t); M(MCB, "computeCommandEncoderWithDescriptor:", cb_cenc_d);
    M(MCB, "blitCommandEncoder", cb_benc); M(MCB, "blitCommandEncoderWithDescriptor:", cb_benc_d);
    M(MCB, "parallelRenderCommandEncoderWithDescriptor:", ret_nil); M(MCB, "resourceStateCommandEncoder", ret_nil);
    M(MCB, "accelerationStructureCommandEncoder", ret_nil);

    def_obj(&MREnc_c, &MREnc_m, "MTLSoftRenderCommandEncoder", sizeof(menc_t));
    M(MREnc, "dealloc", enc_dealloc); M(MREnc, "endEncoding", enc_endm);
    M(MREnc, "setRenderPipelineState:", re_pipe);
    M(MREnc, "setVertexBuffer:offset:atIndex:", re_vbuf); M(MREnc, "setFragmentBuffer:offset:atIndex:", re_fbuf);
    M(MREnc, "setVertexBufferOffset:atIndex:", re_vbufoff); M(MREnc, "setFragmentBufferOffset:atIndex:", re_fbufoff);
    M(MREnc, "setVertexBuffers:offsets:withRange:", re_vbufs); M(MREnc, "setFragmentBuffers:offsets:withRange:", re_fbufs);
    M(MREnc, "setVertexBytes:length:atIndex:", re_vbytes); M(MREnc, "setFragmentBytes:length:atIndex:", re_fbytes);
    M(MREnc, "setVertexTexture:atIndex:", re_vtex); M(MREnc, "setFragmentTexture:atIndex:", re_ftex);
    M(MREnc, "setVertexTextures:withRange:", re_vtexs); M(MREnc, "setFragmentTextures:withRange:", re_ftexs);
    M(MREnc, "setVertexSamplerState:atIndex:", re_vsamp); M(MREnc, "setFragmentSamplerState:atIndex:", re_fsamp);
    M(MREnc, "setVertexSamplerStates:withRange:", re_vsamps); M(MREnc, "setFragmentSamplerStates:withRange:", re_fsamps);
    M(MREnc, "setVertexSamplerState:lodMinClamp:lodMaxClamp:atIndex:", re_vsamp_l);
    M(MREnc, "setFragmentSamplerState:lodMinClamp:lodMaxClamp:atIndex:", re_fsamp_l);
    M(MREnc, "setViewport:", re_viewport); M(MREnc, "setViewports:count:", re_viewports);
    M(MREnc, "setScissorRect:", re_scissor); M(MREnc, "setScissorRects:count:", re_scissors);
    M(MREnc, "setFrontFacingWinding:", re_wind); M(MREnc, "setCullMode:", re_cull); M(MREnc, "setTriangleFillMode:", re_fill);
    M(MREnc, "setDepthClipMode:", re_dclip); M(MREnc, "setDepthBias:slopeScale:clamp:", re_dbias);
    M(MREnc, "setDepthStencilState:", re_dss); M(MREnc, "setStencilReferenceValue:", re_sref);
    M(MREnc, "setStencilFrontReferenceValue:backReferenceValue:", re_sref2);
    M(MREnc, "setBlendColorRed:green:blue:alpha:", re_blendc); M(MREnc, "setVisibilityResultMode:offset:", noop);
    M(MREnc, "setColorStoreAction:atIndex:", noop); M(MREnc, "setDepthStoreAction:", noop); M(MREnc, "setStencilStoreAction:", noop);
    M(MREnc, "drawPrimitives:vertexStart:vertexCount:", re_draw);
    M(MREnc, "drawPrimitives:vertexStart:vertexCount:instanceCount:", re_draw_i);
    M(MREnc, "drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:", re_draw_ib);
    M(MREnc, "drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:", re_drawidx);
    M(MREnc, "drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:", re_drawidx_i);
    M(MREnc, "drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:", re_drawidx_ibb);
    M(MREnc, "drawPrimitives:indirectBuffer:indirectBufferOffset:", re_draw_ind);
    M(MREnc, "drawIndexedPrimitives:indexType:indexBuffer:indexBufferOffset:indirectBuffer:indirectBufferOffset:", re_drawidx_ind);
    def_obj(&MCEnc_c, &MCEnc_m, "MTLSoftComputeCommandEncoder", sizeof(menc_t));
    M(MCEnc, "dealloc", enc_dealloc); M(MCEnc, "endEncoding", enc_endm); M(MCEnc, "dispatchType", ce_dtype);
    M(MCEnc, "setComputePipelineState:", ce_pipe); M(MCEnc, "setBuffer:offset:atIndex:", ce_buf);
    M(MCEnc, "setBufferOffset:atIndex:", ce_bufoff); M(MCEnc, "setBuffers:offsets:withRange:", ce_bufs);
    M(MCEnc, "setBytes:length:atIndex:", ce_bytes); M(MCEnc, "setTexture:atIndex:", ce_tex); M(MCEnc, "setTextures:withRange:", ce_texs);
    M(MCEnc, "setSamplerState:atIndex:", ce_samp); M(MCEnc, "setSamplerStates:withRange:", ce_samps);
    M(MCEnc, "setSamplerState:lodMinClamp:lodMaxClamp:atIndex:", ce_samp_l);
    M(MCEnc, "setThreadgroupMemoryLength:atIndex:", ce_tgmem); M(MCEnc, "setImageblockWidth:height:", noop);
    M(MCEnc, "setStageInRegion:", noop);
    M(MCEnc, "dispatchThreadgroups:threadsPerThreadgroup:", ce_dispatch_tg);
    M(MCEnc, "dispatchThreads:threadsPerThreadgroup:", ce_dispatch_th);
    M(MCEnc, "dispatchThreadgroupsWithIndirectBuffer:indirectBufferOffset:threadsPerThreadgroup:", ce_dispatch_ind);
    def_obj(&MBEnc_c, &MBEnc_m, "MTLSoftBlitCommandEncoder", sizeof(menc_t));
    M(MBEnc, "dealloc", enc_dealloc); M(MBEnc, "endEncoding", enc_endm);
    M(MBEnc, "copyFromBuffer:sourceOffset:toBuffer:destinationOffset:size:", be_bb);
    M(MBEnc, "copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:", be_bt);
    M(MBEnc, "copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:options:", be_bt_o);
    M(MBEnc, "copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:", be_tb);
    M(MBEnc, "copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:options:", be_tb_o);
    M(MBEnc, "copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:", be_tt);
    M(MBEnc, "copyFromTexture:toTexture:", be_tt_all);
    M(MBEnc, "copyFromTexture:sourceSlice:sourceLevel:toTexture:destinationSlice:destinationLevel:sliceCount:levelCount:", be_tt_n);
    M(MBEnc, "fillBuffer:range:value:", be_fill); M(MBEnc, "generateMipmapsForTexture:", be_mips);
    M(MBEnc, "synchronizeResource:", noop); M(MBEnc, "synchronizeTexture:slice:level:", noop);
    M(MBEnc, "optimizeContentsForGPUAccess:", noop); M(MBEnc, "optimizeContentsForCPUAccess:", noop);
    M(MBEnc, "optimizeContentsForGPUAccess:slice:level:", noop); M(MBEnc, "optimizeContentsForCPUAccess:slice:level:", noop);
    M(MBEnc, "resetCommandsInBuffer:withRange:", noop); M(MBEnc, "copyIndirectCommandBuffer:sourceRange:destination:destinationIndex:", noop);
    M(MBEnc, "optimizeIndirectCommandBuffer:withRange:", noop); M(MBEnc, "resolveCounters:inRange:destinationBuffer:destinationOffset:", noop);
    M(MBEnc, "getTextureAccessCounters:region:mipLevel:slice:resetCounters:countersBuffer:countersBufferOffset:", noop);
    M(MBEnc, "resetTextureAccessCounters:region:mipLevel:slice:", noop);

    def_obj(&MDrawable_c, &MDrawable_m, "MTLSoftDrawable", sizeof(mdraw_t));
    M(MDrawable, "dealloc", dr_dealloc); M(MDrawable, "present", dr_present); M(MDrawable, "presentAtTime:", dr_present_t);
    M(MDrawable, "presentAfterMinimumDuration:", dr_present_t); M(MDrawable, "texture", dr_tex); M(MDrawable, "layer", dr_layer);
    M(MDrawable, "drawableID", dr_id); M(MDrawable, "presentedTime", dr_ptime); M(MDrawable, "addPresentedHandler:", dr_add_ph);

    /* CAMetalLayer (classe definida pelo quartzcore.c): estado num ponteiro depois das ivars do CALayer */
    cx_rw_t *rw = (cx_rw_t *)CAMetalLayer_c.vtable;
    if (rw && rw->magic == RW_MAGIC) {
        g_ml_off = (rw->size + 7) & ~7u;
        rw->size = g_ml_off + 8;
#define LMR(sel, f) class_replaceMethod(&CAMetalLayer_c, sel_registerName(sel), (void *)(f), "@@:")
        orig_ml_dealloc = (void (*)(id, SEL))cx_lookup(&CALayer_c, sel_registerName("dealloc"));
        LMR("dealloc", ml_dealloc); LMR("device", ml_device); LMR("setDevice:", ml_set_device); LMR("preferredDevice", ml_pref_device);
        LMR("pixelFormat", ml_fmt); LMR("setPixelFormat:", ml_set_fmt); LMR("framebufferOnly", ml_fbonly); LMR("setFramebufferOnly:", ml_set_fbonly);
        LMR("presentsWithTransaction", ml_pwt); LMR("setPresentsWithTransaction:", ml_set_pwt);
        LMR("displaySyncEnabled", ml_vsync); LMR("setDisplaySyncEnabled:", ml_set_vsync);
        LMR("allowsNextDrawableTimeout", ml_timeout); LMR("setAllowsNextDrawableTimeout:", ml_set_timeout);
        LMR("wantsExtendedDynamicRangeContent", ml_edr); LMR("setWantsExtendedDynamicRangeContent:", ml_set_edr);
        LMR("maximumDrawableCount", ml_maxd); LMR("setMaximumDrawableCount:", ml_set_maxd);
        LMR("colorspace", ml_cs); LMR("setColorspace:", ml_set_cs); LMR("drawableSize", ml_size); LMR("setDrawableSize:", ml_set_size);
        LMR("nextDrawable", ml_next); LMR("EDRMetadata", ret_nil); LMR("setEDRMetadata:", noop);
        LMR("developerHUDProperties", ret_nil); LMR("setDeveloperHUDProperties:", noop);
#undef LMR
    }

    /* MTKView (MetalKit) */
    cx_class_t *nv = cx_class_named("NSView");
    if (nv) {
        cx_define_class(&MTKView_c, &MTKView_m, "MTKView", nv, 8);
        g_mtk_off = cx_instance_size(&MTKView_c) - 8;
        nsview_init_frame = cx_lookup(nv, sel_registerName("initWithFrame:"));
        nsview_init_coder = cx_lookup(nv, sel_registerName("initWithCoder:"));
        nsview_set_needs = cx_lookup(nv, sel_registerName("setNeedsDisplay:"));
        nsview_dealloc = cx_lookup(nv, sel_registerName("dealloc"));
        M(MTKView, "initWithFrame:device:", mtk_init_frame_dev); M(MTKView, "initWithFrame:", mtk_init_frame);
        M(MTKView, "initWithCoder:", mtk_init_coder); M(MTKView, "makeBackingLayer", mtk_make_backing); M(MTKView, "dealloc", mtk_dealloc);
        M(MTKView, "device", mtk_device); M(MTKView, "setDevice:", mtk_set_device); M(MTKView, "preferredDevice", ml_pref_device);
        M(MTKView, "delegate", mtk_delegate); M(MTKView, "setDelegate:", mtk_set_delegate);
        M(MTKView, "currentDrawable", mtk_cur_drawable); M(MTKView, "currentRenderPassDescriptor", mtk_rpd);
        M(MTKView, "colorPixelFormat", mtk_cfmt); M(MTKView, "setColorPixelFormat:", mtk_set_cfmt);
        M(MTKView, "depthStencilPixelFormat", mtk_dfmt); M(MTKView, "setDepthStencilPixelFormat:", mtk_set_dfmt);
        M(MTKView, "depthStencilAttachmentTextureUsage", mtk_dusage); M(MTKView, "setDepthStencilAttachmentTextureUsage:", mtk_set_dusage);
        M(MTKView, "sampleCount", mtk_samples); M(MTKView, "setSampleCount:", mtk_set_samples);
        M(MTKView, "clearColor", mtk_cc); M(MTKView, "setClearColor:", mtk_set_cc);
        M(MTKView, "clearDepth", mtk_cd); M(MTKView, "setClearDepth:", mtk_set_cd);
        M(MTKView, "clearStencil", mtk_cs); M(MTKView, "setClearStencil:", mtk_set_cs);
        M(MTKView, "preferredFramesPerSecond", mtk_fps); M(MTKView, "setPreferredFramesPerSecond:", mtk_set_fps);
        M(MTKView, "isPaused", mtk_paused); M(MTKView, "paused", mtk_paused); M(MTKView, "setPaused:", mtk_set_paused);
        M(MTKView, "enableSetNeedsDisplay", mtk_needs_mode); M(MTKView, "setEnableSetNeedsDisplay:", mtk_set_needs_mode);
        M(MTKView, "autoResizeDrawable", mtk_autoresize); M(MTKView, "setAutoResizeDrawable:", mtk_set_autoresize);
        M(MTKView, "presentsWithTransaction", mtk_pwt); M(MTKView, "setPresentsWithTransaction:", mtk_set_pwt);
        M(MTKView, "framebufferOnly", mtk_fbonly); M(MTKView, "setFramebufferOnly:", mtk_set_fbonly);
        M(MTKView, "drawableSize", mtk_size); M(MTKView, "setDrawableSize:", mtk_set_size);
        M(MTKView, "preferredDrawableSize", mtk_pref_size); M(MTKView, "depthStencilTexture", mtk_depth_tex);
        M(MTKView, "multisampleColorTexture", mtk_msaa_tex); M(MTKView, "colorspace", mtk_cspace); M(MTKView, "setColorspace:", mtk_set_cspace);
        M(MTKView, "draw", mtk_draw); M(MTKView, "drawRect:", mtk_draw_rect); M(MTKView, "releaseDrawables", mtk_release_drawables);
        M(MTKView, "setNeedsDisplay:", mtk_set_needs); M(MTKView, "_mosMetalTick:", mtk_tick);
        M(MTKView, "wantsUpdateLayer", ret_yes); M(MTKView, "isOpaque", ret_yes);
    }

    tl_register();
    mps_register();
    /* resto do QuartzCore (animações, CAShapeLayer, CAGradientLayer, CATextLayer, CADisplayLink...) */
    { void cx_coreanimation_init(void); cx_coreanimation_init(); }

    /* o device */
    mdev_t *d = (mdev_t *)cx_alloc_instance(&MDev_c);
    d->name = cx_str("mOS Software GPU");
    d->dev = (id)d;
    g_dev = (id)d;
}

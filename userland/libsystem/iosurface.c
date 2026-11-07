/*
 * mOS libSystem — IOSurface (porte do VS Code: o Chromium entrega os quadros da janela em IOSurfaces).
 *
 * Um IOSurface aqui é um objeto Objective-C da classe "IOSurface" (como no macOS, IOSurfaceRef é
 * ponte gratuita com ela; CFRetain/CFRelease funcionam) com um bloco de pixels mapeado com mmap.
 * Cada surface tem um ID global (IOSurfaceGetID/IOSurfaceLookup) e pode ganhar uma porta Mach
 * (IOSurfaceCreateMachPort/IOSurfaceLookupFromMachPort).
 *
 * Entre processos (IPC Mach do kernel, kernel/proc/machipc.c): a memória do surface é uma página de
 * cabeçalho (surf_hdr_t: tamanho, formato, planos) seguida dos pixels. IOSurfaceCreateMachPort cria
 * uma ENTRADA DE MEMÓRIA (mach_make_memory_entry_64) sobre esse bloco e põe o ID no contexto da
 * porta (com o pid); o Chromium manda essa porta ao outro processo num mach_msg, e lá
 * IOSurfaceLookupFromMachPort mapeia a entrada (mach_vm_map), lê o cabeçalho e monta um IOSurface
 * que enxerga os MESMOS pixels. No mesmo processo a porta só leva de volta ao objeto (pelo contexto).
 * Sem IPC no kernel (harness machotest), a porta é um nome local (PORT_LOCAL | id).
 * O QuartzCore (quartzcore.c) lê os pixels com IOSurfaceGetBaseAddress e é avisado a cada
 * IOSurfaceUnlock de escrita (cx_iosurface_changed) para redesenhar as janelas que o mostram.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *a, size_t len);
typedef u32 mach_port_t;
mach_port_t mach_task_self(void);
int mach_make_memory_entry_64(mach_port_t task, u64 *size, u64 offset, int perm, mach_port_t *handle, mach_port_t parent);
int mach_vm_map(mach_port_t task, u64 *addr, u64 size, u64 mask, int flags, mach_port_t object, u64 offset,
                int copy, int cur, int max, unsigned inherit);
int mach_port_set_context(mach_port_t task, mach_port_t name, u64 context);
int mach_port_get_context(mach_port_t task, mach_port_t name, u64 *context);
#define PORT_LOCAL 0x7F000000u                  /* nomes locais (sem IPC): PORT_LOCAL | id */
/* contexto da porta: marca (16 bits) | pid de quem criou (16) | id (32); o contexto é visto por todos os processos */
#define CTX_MAGIC 0x105Full
static u64 port_ctx(u32 sid) { return CTX_MAGIC << 48 | (u64)(getpid() & 0xFFFF) << 32 | sid; }

#define KERN_SUCCESS 0
#define KERN_INVALID_ARGUMENT 4
#define FMT(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))

typedef struct { u32 w, h, bpe, ew, eh; u64 bpr, off, size; } plane_t;
typedef struct iosurf {
    cx_class_t *isa;
    u32 sid;                    /* IOSurfaceID */
    u32 port;                   /* porta Mach (0 = ainda não criada) */
    u32 w, h, bpe, fmt, nplanes;
    u64 bpr, alloc;
    u8 *base;                   /* pixels = mem + 4096 */
    u8 *mem;                    /* bloco mapeado: cabeçalho + pixels */
    u64 memlen;
    plane_t pl[4];
    volatile i32 use, locks;
    volatile u32 seed;
    id values;                  /* IOSurfaceSetValue/CopyValue */
    int purgeable;
} iosurf_t;

cx_class_t IOSurface_c __asm__("OBJC_CLASS_$_IOSurface");
cx_class_t IOSurface_m __asm__("OBJC_METACLASS_$_IOSurface");

/* ---- chaves do dicionário de propriedades (CFStringRef constantes) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } surf_cfconst_t;
#define CFCONST(var, text) \
    static surf_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(kIOSurfaceWidth, "IOSurfaceWidth")
CFCONST(kIOSurfaceHeight, "IOSurfaceHeight")
CFCONST(kIOSurfaceBytesPerRow, "IOSurfaceBytesPerRow")
CFCONST(kIOSurfaceBytesPerElement, "IOSurfaceBytesPerElement")
CFCONST(kIOSurfaceElementWidth, "IOSurfaceElementWidth")
CFCONST(kIOSurfaceElementHeight, "IOSurfaceElementHeight")
CFCONST(kIOSurfaceOffset, "IOSurfaceOffset")
CFCONST(kIOSurfacePixelFormat, "IOSurfacePixelFormat")
CFCONST(kIOSurfaceAllocSize, "IOSurfaceAllocSize")
CFCONST(kIOSurfaceIsGlobal, "IOSurfaceIsGlobal")
CFCONST(kIOSurfaceCacheMode, "IOSurfaceCacheMode")
CFCONST(kIOSurfaceName, "IOSurfaceName")
CFCONST(kIOSurfaceColorSpace, "IOSurfaceColorSpace")
CFCONST(kIOSurfacePlaneInfo, "IOSurfacePlaneInfo")
CFCONST(kIOSurfacePlaneWidth, "IOSurfacePlaneWidth")
CFCONST(kIOSurfacePlaneHeight, "IOSurfacePlaneHeight")
CFCONST(kIOSurfacePlaneBytesPerRow, "IOSurfacePlaneBytesPerRow")
CFCONST(kIOSurfacePlaneBytesPerElement, "IOSurfacePlaneBytesPerElement")
CFCONST(kIOSurfacePlaneElementWidth, "IOSurfacePlaneElementWidth")
CFCONST(kIOSurfacePlaneElementHeight, "IOSurfacePlaneElementHeight")
CFCONST(kIOSurfacePlaneOffset, "IOSurfacePlaneOffset")
CFCONST(kIOSurfacePlaneSize, "IOSurfacePlaneSize")
CFCONST(kIOSurfacePlaneBase, "IOSurfacePlaneBase")

/* ---- tabela global de surfaces (ID e porta -> objeto), protegida por trava simples */
#define MAX_SURF 1024
static iosurf_t *table[MAX_SURF];
static volatile int tlock;
static u32 next_sid = 1;
static void T_lock(void) { while (__sync_lock_test_and_set(&tlock, 1)) SYS0(SYS_YIELD); }
static void T_unlock(void) { __sync_lock_release(&tlock); }

/* ---- memória: uma página de cabeçalho + os pixels, num bloco que pode virar entrada de memória Mach */
#define HDR_MAGIC 0x46534F49u                   /* "IOSF" */
typedef struct { u32 magic, sid, w, h, bpe, fmt, nplanes, pad; u64 bpr, alloc; plane_t pl[4]; } surf_hdr_t;
static BOOL surf_mem_alloc(iosurf_t *s) {
    u64 len = 4096 + s->alloc;
    void *p = mmap(0, (size_t)len, 3 /* READ|WRITE */, 0x1002 /* MAP_ANON|MAP_PRIVATE */, -1, 0);
    if (p == (void *)-1 || !p) return NO;
    memset(p, 0, (size_t)len);
    s->mem = p; s->memlen = len; s->base = (u8 *)p + 4096;
    return YES;
}
static void surf_write_hdr(iosurf_t *s) {
    surf_hdr_t *h = (surf_hdr_t *)s->mem;
    h->magic = HDR_MAGIC; h->sid = s->sid; h->w = s->w; h->h = s->h; h->bpe = s->bpe; h->fmt = s->fmt;
    h->nplanes = s->nplanes; h->bpr = s->bpr; h->alloc = s->alloc;
    memcpy(h->pl, s->pl, sizeof h->pl);
}
static void surf_mem_free(iosurf_t *s) { if (s->mem) munmap(s->mem, (size_t)s->memlen); s->mem = s->base = 0; }

/* ---- leitura do dicionário de propriedades */
static id dget(id d, const void *k) { return d ? MSG(id, d, "objectForKey:", (id)k) : 0; }
static i64 dnum(id d, const void *k, i64 dflt) { id v = dget(d, k); return v ? MSG0(i64, v, "longLongValue") : dflt; }

/* bytes por pixel quando o app não informa, a partir do formato (FourCC) */
static u32 fmt_bpe(u32 f) {
    switch (f) {
    case FMT('R', 'G', 'h', 'A'): return 8;                        /* half float RGBA */
    case FMT('L', '0', '0', '8'): case FMT('L', '0', '0', 'h'): return f == FMT('L', '0', '0', 'h') ? 2 : 1;
    case FMT('2', 'C', '0', '8'): return 2;
    case FMT('4', '2', '0', 'v'): case FMT('4', '2', '0', 'f'): return 1;  /* NV12 (Y do primeiro plano) */
    case FMT('x', '4', '2', '0'): return 2;                        /* P010 */
    default: return 4;                                              /* 'BGRA', 'l10r', 'RGBA'... */
    }
}
static u64 align64(u64 v) { return (v + 63) & ~63ull; }

static iosurf_t *surf_new(void) {
    iosurf_t *s = (iosurf_t *)cx_alloc_instance(&IOSurface_c);
    return s;
}
static BOOL surf_register(iosurf_t *s) {
    T_lock();
    for (int i = 0; i < MAX_SURF; i++) if (!table[i]) {
        table[i] = s;
        if (!s->sid) s->sid = next_sid++;
        T_unlock();
        return YES;
    }
    T_unlock();
    return NO;
}

typedef iosurf_t *IOSurfaceRef;

IOSurfaceRef IOSurfaceCreate(id props) {
    i64 w = dnum(props, kIOSurfaceWidth, 0), h = dnum(props, kIOSurfaceHeight, 0);
    {   /* diagnóstico (ca-log): quem cria superfícies e de que tamanho (o caminho do quadro do Chromium) */
        extern int cx_ca_diag(void);
        static int ncr;
        if (cx_ca_diag() && (++ncr <= 8 || ncr % 120 == 0))
            cx_fdprintf(2, "[ca] IOSurfaceCreate %lldx%lld formato %08llx (#%d)\n", w, h, (long long)dnum(props, kIOSurfacePixelFormat, 0), ncr);
    }
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return 0;
    iosurf_t *s = surf_new();
    if (!s) return 0;
    s->w = (u32)w; s->h = (u32)h;
    s->fmt = (u32)dnum(props, kIOSurfacePixelFormat, FMT('B', 'G', 'R', 'A'));
    id planes = dget(props, kIOSurfacePlaneInfo);
    u64 np = planes ? MSG0(u64, planes, "count") : 0;
    if (np > 4) np = 4;
    if (np) {
        /* multiplanar (NV12 do vídeo): cada plano com tamanho e deslocamento próprios */
        u64 off = 0;
        for (u64 i = 0; i < np; i++) {
            id pd = MSG(id, planes, "objectAtIndex:", i);
            plane_t *p = &s->pl[i];
            p->w = (u32)dnum(pd, kIOSurfacePlaneWidth, w);
            p->h = (u32)dnum(pd, kIOSurfacePlaneHeight, h);
            p->bpe = (u32)dnum(pd, kIOSurfacePlaneBytesPerElement, 1);
            p->ew = (u32)dnum(pd, kIOSurfacePlaneElementWidth, 1);
            p->eh = (u32)dnum(pd, kIOSurfacePlaneElementHeight, 1);
            p->bpr = (u64)dnum(pd, kIOSurfacePlaneBytesPerRow, (i64)align64((u64)p->w * p->bpe));
            p->off = (u64)dnum(pd, kIOSurfacePlaneOffset, (i64)off);
            p->size = (u64)dnum(pd, kIOSurfacePlaneSize, (i64)(p->bpr * p->h));
            if (p->off + p->size > off) off = p->off + p->size;
        }
        s->nplanes = (u32)np;
        s->bpe = s->pl[0].bpe;
        s->bpr = s->pl[0].bpr;
        s->alloc = off;
    } else {
        s->bpe = (u32)dnum(props, kIOSurfaceBytesPerElement, fmt_bpe(s->fmt));
        s->bpr = (u64)dnum(props, kIOSurfaceBytesPerRow, (i64)align64((u64)s->w * s->bpe));
        s->alloc = s->bpr * s->h;
    }
    u64 asz = (u64)dnum(props, kIOSurfaceAllocSize, 0);
    if (asz > s->alloc) s->alloc = asz;
    s->alloc = (s->alloc + 4095) & ~4095ull;
    if (!surf_mem_alloc(s) || !surf_register(s)) { objc_release((id)s); return 0; }
    surf_write_hdr(s);
    return s;
}

/* processo receptor: monta um IOSurface sobre um bloco (cabeçalho + pixels) mapeado de outro processo */
static iosurf_t *surf_import(u8 *mem, u64 memlen) {
    surf_hdr_t *h = (surf_hdr_t *)mem;
    if (h->magic != HDR_MAGIC || 4096 + h->alloc > memlen) return 0;
    iosurf_t *s = surf_new();
    if (!s) return 0;
    s->w = h->w; s->h = h->h; s->bpe = h->bpe; s->fmt = h->fmt; s->nplanes = h->nplanes > 4 ? 4 : h->nplanes;
    s->bpr = h->bpr; s->alloc = h->alloc;
    memcpy(s->pl, h->pl, sizeof s->pl);
    s->mem = mem; s->memlen = memlen; s->base = mem + 4096;
    if (!surf_register(s)) { s->mem = 0; objc_release((id)s); return 0; }   /* ID novo, deste processo */
    return s;
}

static void surf_dealloc(id self, SEL c) {
    iosurf_t *s = (iosurf_t *)self;
    T_lock();
    for (int i = 0; i < MAX_SURF; i++) if (table[i] == s) table[i] = 0;
    T_unlock();
    surf_mem_free(s);
    if (s->values) objc_release(s->values);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static u64 surf_typeid(id self, SEL c) { (void)self; (void)c; extern u64 IOSurfaceGetTypeID(void); return IOSurfaceGetTypeID(); }

u64 IOSurfaceGetTypeID(void) { return 0x2B0; }
u32 IOSurfaceGetID(IOSurfaceRef s) { return s ? s->sid : 0; }

/* Lookup: devolve uma referência nova (+1), como no macOS */
static iosurf_t *find(u32 sid, u32 port) {
    iosurf_t *r = 0;
    T_lock();
    for (int i = 0; i < MAX_SURF && !r; i++) {
        iosurf_t *s = table[i];
        if (s && ((sid && s->sid == sid) || (port && s->port == port))) r = s;
    }
    if (r) objc_retain((id)r);
    T_unlock();
    return r;
}
IOSurfaceRef IOSurfaceLookup(u32 sid) { return sid ? find(sid, 0) : 0; }

/* Cada chamada devolve um direito novo (o chamador o desaloca, como no macOS): uma entrada de memória
 * sobre o bloco do surface, com CTX_MAGIC|id no contexto. Sem IPC no kernel: PORT_LOCAL | id. */
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef s) {
    if (!s) return 0;
    {
        extern int cx_ca_diag(void);
        static int nmp;
        if (cx_ca_diag() && (++nmp <= 5 || nmp % 120 == 0)) cx_fdprintf(2, "[ca] IOSurfaceCreateMachPort id %u %ux%u (#%d)\n", s->sid, s->w, s->h, nmp);
    }
    u64 size = s->memlen;
    mach_port_t p = 0;
    if (s->mem && mach_make_memory_entry_64(mach_task_self(), &size, (u64)s->mem, 3 /* READ|WRITE */, &p, 0) == KERN_SUCCESS && p) {
        mach_port_set_context(mach_task_self(), p, port_ctx(s->sid));
        return p;
    }
    return PORT_LOCAL | s->sid;
}
IOSurfaceRef IOSurfaceLookupFromMachPort(mach_port_t port) {
    if (!port) return 0;
    if ((port & 0xFF000000u) == PORT_LOCAL) return find(port & 0xFFFFFFu, 0);
    u64 ctx = 0;
    if (mach_port_get_context(mach_task_self(), port, &ctx) == KERN_SUCCESS && ctx >> 32 == port_ctx(0) >> 32) {
        iosurf_t *s = find((u32)ctx, 0);        /* mesmo processo (o contexto vale para quem criou a porta) */
        if (s) return s;
    }
    /* outro processo: mapeia a página de cabeçalho, lê o tamanho e mapeia o bloco inteiro */
    u64 a = 0;
    if (mach_vm_map(mach_task_self(), &a, 4096, 0, 1 /* VM_FLAGS_ANYWHERE */, port, 0, 0, 3, 3, 0) != KERN_SUCCESS || !a) return 0;
    surf_hdr_t h = *(surf_hdr_t *)a;
    munmap((void *)a, 4096);
    if (h.magic != HDR_MAGIC) return 0;
    u64 len = 4096 + h.alloc, b = 0;
    if (mach_vm_map(mach_task_self(), &b, len, 0, 1, port, 0, 0, 3, 3, 0) != KERN_SUCCESS || !b) return 0;
    iosurf_t *s = surf_import((u8 *)b, len);
    if (!s) munmap((void *)b, (size_t)len);
    extern int cx_ca_diag(void);
    static int nimp;
    if (s && cx_ca_diag() && (++nimp <= 5 || nimp % 120 == 0))
        cx_fdprintf(2, "[ca] IOSurface de outro processo (porta %u, pid %u): %ux%u -> id local %u (#%d)\n",
                    port, (u32)(ctx >> 32) & 0xFFFF, s->w, s->h, s->sid, nimp);
    return s;
}
/* variantes por XPC: o mOS não tem XPC */
void *IOSurfaceCreateXPCObject(IOSurfaceRef s) { (void)s; return 0; }
IOSurfaceRef IOSurfaceLookupFromXPCObject(void *x) { (void)x; return 0; }

int IOSurfaceLock(IOSurfaceRef s, u32 opts, u32 *seed) {
    if (!s) return KERN_INVALID_ARGUMENT;
    (void)opts;
    __sync_fetch_and_add(&s->locks, 1);
    if (seed) *seed = s->seed;
    return KERN_SUCCESS;
}
void cx_iosurface_changed(IOSurfaceRef s) __attribute__((weak));
int IOSurfaceUnlock(IOSurfaceRef s, u32 opts, u32 *seed) {
    if (!s) return KERN_INVALID_ARGUMENT;
    __sync_fetch_and_sub(&s->locks, 1);
    if (!(opts & 1)) {                          /* kIOSurfaceLockReadOnly = 1: escrita muda o seed */
        __sync_fetch_and_add(&s->seed, 1);
        if (cx_iosurface_changed) cx_iosurface_changed(s);
    }
    if (seed) *seed = s->seed;
    return KERN_SUCCESS;
}
u32 IOSurfaceGetSeed(IOSurfaceRef s) { return s ? s->seed : 0; }

void *IOSurfaceGetBaseAddress(IOSurfaceRef s) { return s ? s->base : 0; }
u64 IOSurfaceGetWidth(IOSurfaceRef s) { return s ? s->w : 0; }
u64 IOSurfaceGetHeight(IOSurfaceRef s) { return s ? s->h : 0; }
u64 IOSurfaceGetBytesPerRow(IOSurfaceRef s) { return s ? s->bpr : 0; }
u64 IOSurfaceGetBytesPerElement(IOSurfaceRef s) { return s ? s->bpe : 0; }
u64 IOSurfaceGetElementWidth(IOSurfaceRef s) { (void)s; return 1; }
u64 IOSurfaceGetElementHeight(IOSurfaceRef s) { (void)s; return 1; }
u64 IOSurfaceGetAllocSize(IOSurfaceRef s) { return s ? s->alloc : 0; }
u32 IOSurfaceGetPixelFormat(IOSurfaceRef s) { return s ? s->fmt : 0; }
u64 IOSurfaceGetPlaneCount(IOSurfaceRef s) { return s ? s->nplanes : 0; }

/* plano i; num surface de um plano só, o plano 0 é o surface inteiro */
static plane_t one_plane(IOSurfaceRef s, u64 i, BOOL *ok) {
    plane_t p = {0};
    *ok = NO;
    if (!s) return p;
    if (s->nplanes) { if (i >= s->nplanes) return p; *ok = YES; return s->pl[i]; }
    if (i) return p;
    p.w = s->w; p.h = s->h; p.bpe = s->bpe; p.ew = p.eh = 1; p.bpr = s->bpr; p.off = 0; p.size = s->bpr * s->h;
    *ok = YES;
    return p;
}
void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? s->base + p.off : 0; }
u64 IOSurfaceGetWidthOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? p.w : 0; }
u64 IOSurfaceGetHeightOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? p.h : 0; }
u64 IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? p.bpr : 0; }
u64 IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? p.bpe : 0; }
u64 IOSurfaceGetElementWidthOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? p.ew : 0; }
u64 IOSurfaceGetElementHeightOfPlane(IOSurfaceRef s, u64 i) { BOOL ok; plane_t p = one_plane(s, i, &ok); return ok ? p.eh : 0; }

/* contagem de uso: o Chromium marca surfaces "em uso" pela janela para não reciclá-los */
void IOSurfaceIncrementUseCount(IOSurfaceRef s) { if (s) __sync_fetch_and_add(&s->use, 1); }
void IOSurfaceDecrementUseCount(IOSurfaceRef s) { if (s && s->use > 0) __sync_fetch_and_sub(&s->use, 1); }
i32 IOSurfaceGetUseCount(IOSurfaceRef s) { return s ? s->use : 0; }
u8 IOSurfaceIsInUse(IOSurfaceRef s) { return s && s->use > 0; }

/* sem pressão de memória no mOS: o conteúdo nunca é descartado */
int IOSurfaceSetPurgeable(IOSurfaceRef s, u32 state, u32 *old) {
    if (!s) return KERN_INVALID_ARGUMENT;
    if (old) *old = (u32)s->purgeable;
    if (state != 3 /* kIOSurfacePurgeableKeepCurrent */) s->purgeable = (int)state;
    return KERN_SUCCESS;
}

void IOSurfaceSetValue(IOSurfaceRef s, id key, id val) {
    if (!s || !key) return;
    if (!s->values) s->values = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    if (val) MSG(void, s->values, "setObject:forKey:", val, key);
    else MSG(void, s->values, "removeObjectForKey:", key);
}
id IOSurfaceCopyValue(IOSurfaceRef s, id key) {
    id v = s && s->values && key ? MSG(id, s->values, "objectForKey:", key) : 0;
    return v ? objc_retain(v) : 0;
}
void IOSurfaceRemoveValue(IOSurfaceRef s, id key) { if (s && s->values && key) MSG(void, s->values, "removeObjectForKey:", key); }
void IOSurfaceSetValues(IOSurfaceRef s, id dict) {
    if (!s || !dict) return;
    nsdict_t *d = (nsdict_t *)dict;
    for (u64 i = 0; i < d->n; i++) IOSurfaceSetValue(s, d->keys[i], d->vals[i]);
}
id IOSurfaceCopyAllValues(IOSurfaceRef s) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (s && s->values) { nsdict_t *v = (nsdict_t *)s->values; for (u64 i = 0; i < v->n; i++) MSG(void, d, "setObject:forKey:", v->vals[i], v->keys[i]); }
    return objc_retain(d);
}

/* alinhamento: bytes por linha em múltiplos de 64 (como o macOS em Intel), o resto como veio */
u64 IOSurfaceAlignProperty(id key, u64 v) {
    const char *k = key ? cx_cstr(key) : "";
    if (strstr(k, "BytesPerRow")) return align64(v);
    if (strstr(k, "Offset") || strstr(k, "Size")) return (v + 4095) & ~4095ull;
    return v;
}
u64 IOSurfaceGetPropertyMaximum(id key) { (void)key; return 16384; }
u64 IOSurfaceGetPropertyAlignment(id key) {
    const char *k = key ? cx_cstr(key) : "";
    return strstr(k, "BytesPerRow") ? 64 : 1;
}

/* ObjC: [IOSurface ...] mínimo (width/height/baseAddress) para quem usa a classe */
static i64 o_w(id self, SEL c) { (void)c; return ((iosurf_t *)self)->w; }
static i64 o_h(id self, SEL c) { (void)c; return ((iosurf_t *)self)->h; }
static void *o_base(id self, SEL c) { (void)c; return ((iosurf_t *)self)->base; }
static i64 o_bpr(id self, SEL c) { (void)c; return (i64)((iosurf_t *)self)->bpr; }
static id o_init_props(id self, SEL c, id props) {
    (void)c;
    objc_release(self);
    return (id)IOSurfaceCreate(props);
}

void cx_iosurface_init(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_define_class(&IOSurface_c, &IOSurface_m, "IOSurface", cx_class_named("NSObject"), sizeof(iosurf_t) - 8);
    cx_add_method(&IOSurface_c, "dealloc", (void *)surf_dealloc);
    cx_add_method(&IOSurface_c, "_cfTypeID", (void *)surf_typeid);
    cx_add_method(&IOSurface_c, "width", (void *)o_w);
    cx_add_method(&IOSurface_c, "height", (void *)o_h);
    cx_add_method(&IOSurface_c, "bytesPerRow", (void *)o_bpr);
    cx_add_method(&IOSurface_c, "baseAddress", (void *)o_base);
    cx_add_method(&IOSurface_c, "initWithProperties:", (void *)o_init_props);
}

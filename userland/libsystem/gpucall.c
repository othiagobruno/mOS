/* Cliente da ABI de GPU (docs/gpu-abi.md) com backend de CPU.
 *
 * O header público é o do kernel. Se ele ainda não foi mergeado, as
 * estruturas abaixo repetem a revisão 1 só para este arquivo compilar.
 * Não há outro header de ABI.
 */
#include "gpucall.h"
#include "libc.h"

#if defined(__has_include)
#  if __has_include("../../kernel/include/mos_gpu.h")
#    include "../../kernel/include/mos_gpu.h"
#    define MOS_GPU_FROM_KERNEL 1
#  endif
#endif

#ifndef MOS_GPU_ABI
#define MOS_GPU_ABI 1u
enum {
    MOS_GPU_INFO = 1,
    MOS_GPU_CTX_CREATE,
    MOS_GPU_CTX_DESTROY,
    MOS_GPU_BO_CREATE,
    MOS_GPU_BO_DESTROY,
    MOS_GPU_BO_MAP,
    MOS_GPU_BO_UNMAP,
    MOS_GPU_BO_EXPORT,
    MOS_GPU_BO_IMPORT,
    MOS_GPU_SUBMIT,
    MOS_GPU_FENCE_CREATE,
    MOS_GPU_FENCE_WAIT,
    MOS_GPU_FENCE_SIGNAL,
    MOS_GPU_FENCE_DESTROY,
    MOS_GPU_IOSURF_CREATE,
    MOS_GPU_IOSURF_LOOKUP,
    MOS_GPU_IOSURF_EXPORT,
    MOS_GPU_IOSURF_DESTROY,
    MOS_GPU_KMS_GET,
    MOS_GPU_KMS_SET
};
#define MOS_GPU_CAP_VIRGL  (1u << 0)
#define MOS_GPU_CAP_VENUS  (1u << 1)
#define MOS_GPU_CAP_KMS    (1u << 2)
#define MOS_GPU_CAP_IOSURF (1u << 3)
#define MOS_GPU_CTX_VIRGL 1u
#define MOS_GPU_CTX_VENUS 2u
#define MOS_GPU_BO_READ   (1u << 0)
#define MOS_GPU_BO_WRITE  (1u << 1)
#define MOS_GPU_BO_SHARED (1u << 2)
#define MOS_GPU_BO_GPU    (1u << 3)
#define MOS_GPU_SUBMIT_3D (1u << 0)
#define MOS_GPU_FENCE_SHARED (1u << 0)
#define MOS_GPU_FMT_BGRA8 1u
#define MOS_GPU_FMT_RGBA8 2u
#define MOS_GPU_CONN_INTERNAL 0u
#define MOS_GPU_CONN_EXTERNAL 1u
typedef struct {
    unsigned abi, caps, vendor, device;
    char name[32];
} mos_gpu_info_t;
typedef struct { unsigned ctx, kind, flags; } mos_gpu_ctx_t;
typedef struct {
    unsigned handle, flags, ctx, token;
    unsigned long long size;
} mos_gpu_bo_t;
typedef struct {
    unsigned handle, pad;
    unsigned long long addr, len;
} mos_gpu_map_t;
typedef struct {
    unsigned ctx, cmd_bytes, fence, flags;
    unsigned long long cmds;
} mos_gpu_submit_t;
typedef struct { unsigned fence, timeout_ms, flags, seq; } mos_gpu_fence_t;
typedef struct {
    unsigned id, bo, width, height, stride, format, token, pad;
} mos_gpu_iosurface_t;
typedef struct {
    unsigned connector, width, height, stride, format, bo;
} mos_gpu_kms_t;
#endif

#ifndef SYS_GPU
#define SYS_GPU 61
#endif

#define MOS_GPU_EINVAL 22
#define MOS_GPU_ENOMEM 12
#define MOS_GPU_EBUSY  16
#define MOS_GPU_ENOENT 2
#define MOS_GPU_ETIMEDOUT 60

#define GPU_SLOTS 48

typedef struct {
    int live;
    unsigned handle, flags, ctx;
    unsigned long long size;
    void *raw, *mem;
    int maps;
    unsigned token;
} cpu_bo_t;
typedef struct { int live; unsigned id, kind; } cpu_ctx_t;
typedef struct { int live; unsigned id, seq, flags; } cpu_fn_t;
typedef struct {
    int live;
    unsigned id, bo, width, height, stride, format, token;
} cpu_sf_t;

static cpu_bo_t g_bo[GPU_SLOTS];
static cpu_ctx_t g_ctx[GPU_SLOTS];
static cpu_fn_t g_fn[GPU_SLOTS];
static cpu_sf_t g_sf[GPU_SLOTS];
static unsigned g_tok = 1, g_sid = 1;
static int g_probed;
#if defined(MOS_GPU_FROM_KERNEL)
static int g_kernel;
#endif
static unsigned g_caps;

static int fail(int e) { return -e; }

static cpu_bo_t *bo_get(unsigned h) {
    if (!h || h > GPU_SLOTS || !g_bo[h - 1].live) return 0;
    return &g_bo[h - 1];
}
static cpu_ctx_t *ctx_get(unsigned h) {
    if (!h || h > GPU_SLOTS || !g_ctx[h - 1].live) return 0;
    return &g_ctx[h - 1];
}
static cpu_fn_t *fn_get(unsigned h) {
    if (!h || h > GPU_SLOTS || !g_fn[h - 1].live) return 0;
    return &g_fn[h - 1];
}

static int cpu_info(mos_gpu_info_t *s) {
    memset(s, 0, sizeof *s);
    s->abi = MOS_GPU_ABI;
    memcpy(s->name, "cpu", 4);
    return 0;
}
static int cpu_ctx_create(mos_gpu_ctx_t *s) {
    if (s->kind != MOS_GPU_CTX_VIRGL && s->kind != MOS_GPU_CTX_VENUS) return fail(MOS_GPU_EINVAL);
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (!g_ctx[i].live) {
            g_ctx[i].live = 1;
            g_ctx[i].id = i + 1;
            g_ctx[i].kind = s->kind;
            s->ctx = i + 1;
            return 0;
        }
    return fail(MOS_GPU_ENOMEM);
}
static int cpu_ctx_destroy(mos_gpu_ctx_t *s) {
    cpu_ctx_t *c = ctx_get(s->ctx);
    if (!c) return fail(MOS_GPU_ENOENT);
    c->live = 0;
    return 0;
}
static int cpu_bo_create(mos_gpu_bo_t *s) {
    if (!s->size || (s->size & 4095ull)) return fail(MOS_GPU_EINVAL);
    if (s->ctx && !ctx_get(s->ctx)) return fail(MOS_GPU_ENOENT);
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (!g_bo[i].live) {
            void *raw = calloc(1, (size_t)s->size + 4096u);
            if (!raw) return fail(MOS_GPU_ENOMEM);
            unsigned long long a = ((unsigned long long)(unsigned long)raw + 4095ull) & ~4095ull;
            g_bo[i].live = 1;
            g_bo[i].handle = i + 1;
            g_bo[i].flags = s->flags;
            g_bo[i].ctx = s->ctx;
            g_bo[i].size = s->size;
            g_bo[i].raw = raw;
            g_bo[i].mem = (void *)(unsigned long)a;
            s->handle = i + 1;
            s->token = 0;
            return 0;
        }
    return fail(MOS_GPU_ENOMEM);
}
static int cpu_bo_destroy(mos_gpu_bo_t *s) {
    cpu_bo_t *b = bo_get(s->handle);
    if (!b) return fail(MOS_GPU_ENOENT);
    if (b->maps) return fail(MOS_GPU_EBUSY);
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (g_sf[i].live && g_sf[i].bo == s->handle) return fail(MOS_GPU_EBUSY);
    free(b->raw);
    memset(b, 0, sizeof *b);
    return 0;
}
static int cpu_bo_map(mos_gpu_map_t *s) {
    cpu_bo_t *b = bo_get(s->handle);
    if (!b) return fail(MOS_GPU_ENOENT);
    b->maps++;
    s->addr = (unsigned long long)(unsigned long)b->mem;
    s->len = b->size;
    return 0;
}
static int cpu_bo_unmap(mos_gpu_map_t *s) {
    cpu_bo_t *b = bo_get(s->handle);
    if (!b || !b->maps) return fail(MOS_GPU_ENOENT);
    if (s->addr && s->addr != (unsigned long long)(unsigned long)b->mem) return fail(MOS_GPU_EINVAL);
    b->maps--;
    return 0;
}
static int cpu_bo_export(mos_gpu_bo_t *s) {
    cpu_bo_t *b = bo_get(s->handle);
    if (!b) return fail(MOS_GPU_ENOENT);
    if (!(b->flags & MOS_GPU_BO_SHARED)) return fail(MOS_GPU_EINVAL);
    if (!b->token) b->token = g_tok++;
    s->token = b->token;
    s->size = b->size;
    s->flags = b->flags;
    return 0;
}
static int cpu_bo_import(mos_gpu_bo_t *s) {
    if (!s->token) return fail(MOS_GPU_EINVAL);
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (g_bo[i].live && g_bo[i].token == s->token) {
            s->handle = g_bo[i].handle;
            s->size = g_bo[i].size;
            s->flags = g_bo[i].flags;
            return 0;
        }
    return fail(MOS_GPU_ENOENT);
}
static int cpu_submit(mos_gpu_submit_t *s) {
    if (!s->cmd_bytes || !s->cmds) return fail(MOS_GPU_EINVAL);
    if (!ctx_get(s->ctx)) return fail(MOS_GPU_ENOENT);
    /* O lote é virgl ou venus. O CPU não executa esses bytes: o rasterizador
     * do Metal continua no interpretador. Sinalizar a fence marca o lote
     * como aceito, que é o que a ABI pede no retorno da syscall. */
    if (s->fence) {
        cpu_fn_t *f = fn_get(s->fence);
        if (!f) return fail(MOS_GPU_ENOENT);
        f->seq++;
    }
    return 0;
}
static int cpu_fence_create(mos_gpu_fence_t *s) {
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (!g_fn[i].live) {
            g_fn[i].live = 1;
            g_fn[i].id = i + 1;
            g_fn[i].flags = s->flags;
            g_fn[i].seq = 0;
            s->fence = i + 1;
            s->seq = 0;
            return 0;
        }
    return fail(MOS_GPU_ENOMEM);
}
static int cpu_fence_wait(mos_gpu_fence_t *s) {
    cpu_fn_t *f = fn_get(s->fence);
    if (!f) return fail(MOS_GPU_ENOENT);
    if (!f->seq) return fail(MOS_GPU_ETIMEDOUT);
    s->seq = f->seq;
    return 0;
}
static int cpu_fence_signal(mos_gpu_fence_t *s) {
    cpu_fn_t *f = fn_get(s->fence);
    if (!f) return fail(MOS_GPU_ENOENT);
    f->seq++;
    s->seq = f->seq;
    return 0;
}
static int cpu_fence_destroy(mos_gpu_fence_t *s) {
    cpu_fn_t *f = fn_get(s->fence);
    if (!f) return fail(MOS_GPU_ENOENT);
    memset(f, 0, sizeof *f);
    return 0;
}
static int cpu_iosurf_create(mos_gpu_iosurface_t *s) {
    cpu_bo_t *b = bo_get(s->bo);
    if (!b) return fail(MOS_GPU_ENOENT);
    if (!s->width || !s->height || !s->stride) return fail(MOS_GPU_EINVAL);
    if (s->format != MOS_GPU_FMT_BGRA8 && s->format != MOS_GPU_FMT_RGBA8) return fail(MOS_GPU_EINVAL);
    if ((unsigned long long)s->stride * s->height > b->size) return fail(MOS_GPU_EINVAL);
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (!g_sf[i].live) {
            g_sf[i].live = 1;
            g_sf[i].id = g_sid++;
            g_sf[i].bo = s->bo;
            g_sf[i].width = s->width;
            g_sf[i].height = s->height;
            g_sf[i].stride = s->stride;
            g_sf[i].format = s->format;
            s->id = g_sf[i].id;
            s->token = 0;
            return 0;
        }
    return fail(MOS_GPU_ENOMEM);
}
static cpu_sf_t *sf_by_id(unsigned id) {
    for (unsigned i = 0; i < GPU_SLOTS; i++)
        if (g_sf[i].live && g_sf[i].id == id) return &g_sf[i];
    return 0;
}
static int cpu_iosurf_lookup(mos_gpu_iosurface_t *s) {
    cpu_sf_t *sf = 0;
    if (s->token) {
        for (unsigned i = 0; i < GPU_SLOTS; i++)
            if (g_sf[i].live && g_sf[i].token == s->token) sf = &g_sf[i];
    } else sf = sf_by_id(s->id);
    if (!sf) return fail(MOS_GPU_ENOENT);
    s->id = sf->id;
    s->bo = sf->bo;
    s->width = sf->width;
    s->height = sf->height;
    s->stride = sf->stride;
    s->format = sf->format;
    s->token = sf->token;
    return 0;
}
static int cpu_iosurf_export(mos_gpu_iosurface_t *s) {
    cpu_sf_t *sf = sf_by_id(s->id);
    if (!sf) return fail(MOS_GPU_ENOENT);
    if (!sf->token) sf->token = g_tok++;
    s->token = sf->token;
    s->bo = sf->bo;
    return 0;
}
static int cpu_iosurf_destroy(mos_gpu_iosurface_t *s) {
    cpu_sf_t *sf = sf_by_id(s->id);
    if (!sf) return fail(MOS_GPU_ENOENT);
    memset(sf, 0, sizeof *sf);
    return 0;
}
static int cpu_kms(mos_gpu_kms_t *s, int set) {
    (void)set;
    if (s->connector != MOS_GPU_CONN_INTERNAL && s->connector != MOS_GPU_CONN_EXTERNAL)
        return fail(MOS_GPU_EINVAL);
    /* Sem conector de verdade no processo. A ABI devolve ENOENT. */
    return fail(MOS_GPU_ENOENT);
}

static int cpu_op(unsigned op, void *st) {
    switch (op) {
    case MOS_GPU_INFO: return cpu_info(st);
    case MOS_GPU_CTX_CREATE: return cpu_ctx_create(st);
    case MOS_GPU_CTX_DESTROY: return cpu_ctx_destroy(st);
    case MOS_GPU_BO_CREATE: return cpu_bo_create(st);
    case MOS_GPU_BO_DESTROY: return cpu_bo_destroy(st);
    case MOS_GPU_BO_MAP: return cpu_bo_map(st);
    case MOS_GPU_BO_UNMAP: return cpu_bo_unmap(st);
    case MOS_GPU_BO_EXPORT: return cpu_bo_export(st);
    case MOS_GPU_BO_IMPORT: return cpu_bo_import(st);
    case MOS_GPU_SUBMIT: return cpu_submit(st);
    case MOS_GPU_FENCE_CREATE: return cpu_fence_create(st);
    case MOS_GPU_FENCE_WAIT: return cpu_fence_wait(st);
    case MOS_GPU_FENCE_SIGNAL: return cpu_fence_signal(st);
    case MOS_GPU_FENCE_DESTROY: return cpu_fence_destroy(st);
    case MOS_GPU_IOSURF_CREATE: return cpu_iosurf_create(st);
    case MOS_GPU_IOSURF_LOOKUP: return cpu_iosurf_lookup(st);
    case MOS_GPU_IOSURF_EXPORT: return cpu_iosurf_export(st);
    case MOS_GPU_IOSURF_DESTROY: return cpu_iosurf_destroy(st);
    case MOS_GPU_KMS_GET: return cpu_kms(st, 0);
    case MOS_GPU_KMS_SET: return cpu_kms(st, 1);
    default: return fail(MOS_GPU_EINVAL);
    }
}

static void probe(void) {
    if (g_probed) return;
    g_probed = 1;
#if defined(MOS_GPU_FROM_KERNEL)
    mos_gpu_info_t info;
    memset(&info, 0, sizeof info);
    long r = cx_syscall(SYS_GPU, MOS_GPU_INFO, (long)&info, (long)sizeof info, 0, 0);
    if (r == 0) {
        g_kernel = 1;
        g_caps = info.caps;
    }
#else
    (void)0;
#endif
}

void mos_gpu_boot(void) { probe(); }
unsigned mos_gpu_caps(void) {
    probe();
    return g_caps;
}

int mos_gpu_op(unsigned op, void *st, unsigned n) {
    if (!st || !n) return fail(MOS_GPU_EINVAL);
    probe();
#if defined(MOS_GPU_FROM_KERNEL)
    if (g_kernel) {
        long r = cx_syscall(SYS_GPU, op, (long)st, (long)n, 0, 0);
        if (r == 0) return 0;
        /* Sem implementação no kernel: o CPU cobre contexto, BO, submit
         * (aceite sem executar virgl), fence e IOSurface. */
        if (r != -MOS_GPU_EINVAL && r != -38) return (int)r;
    }
#else
    (void)n;
#endif
    return cpu_op(op, st);
}

static unsigned g_pr_bo, g_pr_sf, g_pr_w, g_pr_h, g_pr_stride;

unsigned mos_gpu_present_bgra(const void *px, unsigned w, unsigned h, unsigned stride) {
    if (!px || !w || !h || stride < w * 4u) return 0;
    unsigned long long bytes = (unsigned long long)stride * h;
    unsigned long long size = (bytes + 4095ull) & ~4095ull;
    if (g_pr_sf && (g_pr_w != w || g_pr_h != h || g_pr_stride != stride)) {
        mos_gpu_iosurface_t old;
        memset(&old, 0, sizeof old);
        old.id = g_pr_sf;
        mos_gpu_op(MOS_GPU_IOSURF_DESTROY, &old, sizeof old);
        mos_gpu_bo_t bd;
        memset(&bd, 0, sizeof bd);
        bd.handle = g_pr_bo;
        mos_gpu_op(MOS_GPU_BO_DESTROY, &bd, sizeof bd);
        g_pr_sf = g_pr_bo = 0;
    }
    if (!g_pr_bo) {
        mos_gpu_bo_t bo;
        memset(&bo, 0, sizeof bo);
        bo.size = size;
        bo.flags = MOS_GPU_BO_READ | MOS_GPU_BO_WRITE | MOS_GPU_BO_SHARED;
        if (mos_gpu_op(MOS_GPU_BO_CREATE, &bo, sizeof bo)) return 0;
        g_pr_bo = bo.handle;
    }
    mos_gpu_map_t map;
    memset(&map, 0, sizeof map);
    map.handle = g_pr_bo;
    if (mos_gpu_op(MOS_GPU_BO_MAP, &map, sizeof map)) return 0;
    for (unsigned y = 0; y < h; y++)
        memcpy((u8 *)(unsigned long)map.addr + (unsigned long long)y * stride,
               (const u8 *)px + (unsigned long long)y * stride, w * 4u);
    mos_gpu_op(MOS_GPU_BO_UNMAP, &map, sizeof map);
    if (!g_pr_sf) {
        mos_gpu_iosurface_t sf;
        memset(&sf, 0, sizeof sf);
        sf.bo = g_pr_bo;
        sf.width = w;
        sf.height = h;
        sf.stride = stride;
        sf.format = MOS_GPU_FMT_BGRA8;
        if (mos_gpu_op(MOS_GPU_IOSURF_CREATE, &sf, sizeof sf)) return 0;
        g_pr_sf = sf.id;
        g_pr_w = w;
        g_pr_h = h;
        g_pr_stride = stride;
    }
    return g_pr_sf;
}

void mos_gpu_present_layer(const void *px, unsigned w, unsigned h, unsigned stride) {
    if (!(mos_gpu_caps() & MOS_GPU_CAP_IOSURF)) return;
    (void)mos_gpu_present_bgra(px, w, h, stride);
}

#ifdef MOS_GPU_HOST_TEST
int mos_gpu_host_check(void) {
    int bad = 0;
    mos_gpu_boot();
    if (mos_gpu_caps() != 0) bad++;
    mos_gpu_info_t info;
    memset(&info, 0, sizeof info);
    if (mos_gpu_op(MOS_GPU_INFO, &info, sizeof info) || info.abi != 1 || info.name[0] != 'c') bad++;
    mos_gpu_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.kind = MOS_GPU_CTX_VIRGL;
    if (mos_gpu_op(MOS_GPU_CTX_CREATE, &ctx, sizeof ctx) || !ctx.ctx) bad++;
    mos_gpu_bo_t bo;
    memset(&bo, 0, sizeof bo);
    bo.size = 4096;
    bo.flags = MOS_GPU_BO_READ | MOS_GPU_BO_WRITE | MOS_GPU_BO_SHARED;
    bo.ctx = ctx.ctx;
    if (mos_gpu_op(MOS_GPU_BO_CREATE, &bo, sizeof bo)) bad++;
    mos_gpu_map_t map;
    memset(&map, 0, sizeof map);
    map.handle = bo.handle;
    if (mos_gpu_op(MOS_GPU_BO_MAP, &map, sizeof map) || map.len != 4096) bad++;
    ((unsigned *)(unsigned long)map.addr)[0] = 0x11223344u;
    if (mos_gpu_op(MOS_GPU_BO_UNMAP, &map, sizeof map)) bad++;
    memset(&map, 0, sizeof map);
    map.handle = bo.handle;
    if (mos_gpu_op(MOS_GPU_BO_MAP, &map, sizeof map)) bad++;
    if (((unsigned *)(unsigned long)map.addr)[0] != 0x11223344u) bad++;
    mos_gpu_op(MOS_GPU_BO_UNMAP, &map, sizeof map);
    mos_gpu_bo_t ex;
    memset(&ex, 0, sizeof ex);
    ex.handle = bo.handle;
    if (mos_gpu_op(MOS_GPU_BO_EXPORT, &ex, sizeof ex) || !ex.token) bad++;
    mos_gpu_bo_t im;
    memset(&im, 0, sizeof im);
    im.token = ex.token;
    if (mos_gpu_op(MOS_GPU_BO_IMPORT, &im, sizeof im) || im.handle != bo.handle) bad++;
    mos_gpu_iosurface_t sf;
    memset(&sf, 0, sizeof sf);
    sf.bo = bo.handle;
    sf.width = 4;
    sf.height = 4;
    sf.stride = 16;
    sf.format = MOS_GPU_FMT_BGRA8;
    if (mos_gpu_op(MOS_GPU_IOSURF_CREATE, &sf, sizeof sf) || !sf.id) bad++;
    if (mos_gpu_op(MOS_GPU_BO_DESTROY, &bo, sizeof bo) != -16) bad++;
    mos_gpu_iosurface_t look;
    memset(&look, 0, sizeof look);
    look.id = sf.id;
    if (mos_gpu_op(MOS_GPU_IOSURF_LOOKUP, &look, sizeof look) || look.bo != bo.handle) bad++;
    if (mos_gpu_op(MOS_GPU_IOSURF_DESTROY, &sf, sizeof sf)) bad++;
    if (mos_gpu_op(MOS_GPU_BO_DESTROY, &bo, sizeof bo)) bad++;
    mos_gpu_fence_t fnc;
    memset(&fnc, 0, sizeof fnc);
    if (mos_gpu_op(MOS_GPU_FENCE_CREATE, &fnc, sizeof fnc)) bad++;
    fnc.timeout_ms = 0;
    if (mos_gpu_op(MOS_GPU_FENCE_WAIT, &fnc, sizeof fnc) == 0) bad++;
    if (mos_gpu_op(MOS_GPU_FENCE_SIGNAL, &fnc, sizeof fnc) || fnc.seq != 1) bad++;
    if (mos_gpu_op(MOS_GPU_FENCE_WAIT, &fnc, sizeof fnc) || fnc.seq != 1) bad++;
    unsigned pix[4] = {0xff112233u, 0, 0, 0};
    unsigned id = mos_gpu_present_bgra(pix, 1, 1, 16);
    if (!id) bad++;
    memset(&look, 0, sizeof look);
    look.id = id;
    if (mos_gpu_op(MOS_GPU_IOSURF_LOOKUP, &look, sizeof look) || look.format != MOS_GPU_FMT_BGRA8) bad++;
    mos_gpu_submit_t sub;
    memset(&sub, 0, sizeof sub);
    sub.ctx = ctx.ctx;
    if (mos_gpu_op(MOS_GPU_SUBMIT, &sub, sizeof sub) == 0) bad++;
    unsigned cmd = 1;
    sub.cmds = (unsigned long long)(unsigned long)&cmd;
    sub.cmd_bytes = 4;
    sub.fence = fnc.fence;
    sub.flags = MOS_GPU_SUBMIT_3D;
    if (mos_gpu_op(MOS_GPU_SUBMIT, &sub, sizeof sub)) bad++;
    if (mos_gpu_op(MOS_GPU_FENCE_WAIT, &fnc, sizeof fnc) || fnc.seq != 2) bad++;
    mos_gpu_kms_t kms;
    memset(&kms, 0, sizeof kms);
    if (mos_gpu_op(MOS_GPU_KMS_GET, &kms, sizeof kms) == 0) bad++;
    if (mos_gpu_op(MOS_GPU_CTX_DESTROY, &ctx, sizeof ctx)) bad++;
    return bad;
}
#endif

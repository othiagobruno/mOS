/* Serviço da ABI em kernel/include/mos_gpu.h. O virtio-gpu 3D e o KMS entram por baixo. */
#ifdef MOS_GPU_HOST
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long usize;
#define MOS_EINVAL 22
#define MOS_ENOMEM 12
#define MOS_EFAULT 14
#define MOS_EBUSY 16
#define MOS_ENOENT 2
#define MOS_ENXIO 6
#define MOS_EIO 5
#define MOS_EAGAIN 35
typedef struct { int pid; u64 mmap_next; } process_t;
typedef struct { int state, wait_pid; u64 futex_va, wake_at, wait_status; } thread_t;
static void *dma_alloc(usize n, u64 *phys) {
    void *p = malloc(n);
    if (phys) *phys = (u64)(uintptr_t)p;
    if (p) memset(p, 0, n);
    return p;
}
static void pmm_free(u64 p, u64 pages) { (void)pages; free((void *)(uintptr_t)p); }
#define kprintf(...) ((void)0)
static u32 host_caps;
void gpu_host_caps(u32 c) { host_caps = c; }
#else
#include "kernel.h"
#include "syscall.h"
#include "proc.h"
#include "gpu.h"
#endif

#define GPU_N 64
#define GPU_CMD_MAX 400u

typedef struct { int used, pid; u32 id, kind; } ctx_t;
typedef struct {
    int used, pid, mapped, ios;
    u32 handle, ctx, flags, token;
    u64 size, phys, map;
} bo_t;
typedef struct { int used, pid; u32 id, seq, flags; } fen_t;
typedef struct {
    int used, pid;
    u32 id, token, bo, width, height, stride, format;
} io_t;

static ctx_t ctxs[GPU_N];
static bo_t bos[GPU_N];
static fen_t fens[GPU_N];
static io_t ios[GPU_N];
static mos_gpu_kms_t kms[2];
static u32 seq_id = 1;

static u32 caps_now(void) {
#ifdef MOS_GPU_HOST
    return host_caps;
#else
    u32 c = virtio_gpu_caps() | MOS_GPU_CAP_IOSURF;
    if (gpu_kms_available()) c |= MOS_GPU_CAP_KMS;
    return c;
#endif
}

static int rd(process_t *p, void *dst, u64 user, u64 len, u64 need) {
    if (len < need || !user) return -MOS_EINVAL;
#ifdef MOS_GPU_HOST
    (void)p;
    memcpy(dst, (void *)(uintptr_t)user, (usize)need);
    return 0;
#else
    return uvm_copy_in(p, dst, user, (usize)need) ? 0 : -MOS_EFAULT;
#endif
}
static int wr(process_t *p, u64 user, const void *src, u64 n) {
#ifdef MOS_GPU_HOST
    (void)p;
    memcpy((void *)(uintptr_t)user, src, (usize)n);
    return 0;
#else
    return uvm_copy_out(p, user, src, (usize)n) ? 0 : -MOS_EFAULT;
#endif
}

static ctx_t *ctx_of(int pid, u32 id) {
    for (int i = 0; i < GPU_N; i++) if (ctxs[i].used && ctxs[i].pid == pid && ctxs[i].id == id) return &ctxs[i];
    return 0;
}
static bo_t *bo_of(int pid, u32 id) {
    for (int i = 0; i < GPU_N; i++) if (bos[i].used && bos[i].pid == pid && bos[i].handle == id) return &bos[i];
    return 0;
}
static fen_t *fen_of(int pid, u32 id) {
    for (int i = 0; i < GPU_N; i++) if (fens[i].used && fens[i].pid == pid && fens[i].id == id) return &fens[i];
    return 0;
}

#ifndef MOS_GPU_HOST
static void wake_fence(int pid, u32 id, i64 rax, u32 seq) {
    for (thread_t *w = sched_threads(); w; w = w->next) {
        if (w->state != TH_WAITING || w->wait_pid != -12 || w->futex_va != id || !w->proc || w->proc->pid != pid) continue;
        if (rax == 0 && w->wait_status) uvm_copy_out(w->proc, w->wait_status + 12, &seq, 4);
        ((int_frame_t *)w->frame)->rax = (u64)rax;
        w->wake_at = 0;
        w->futex_va = 0;
        w->state = TH_READY;
    }
}
#endif

static void signal_fence(int pid, u32 id) {
    fen_t *f = fen_of(pid, id);
    if (!f) return;
    f->seq++;
#ifndef MOS_GPU_HOST
    wake_fence(pid, id, 0, f->seq);
#endif
}

i64 gpu_syscall(process_t *p, u64 op, u64 user, u64 len, thread_t *self, int *blocked) {
    if (blocked) *blocked = 0;
    if (!p) return -MOS_EINVAL;
    int pid = p->pid;
    switch ((u32)op) {
    case MOS_GPU_INFO: {
        mos_gpu_info_t n;
        memset(&n, 0, sizeof n);
        n.abi = MOS_GPU_ABI;
        n.caps = caps_now();
        strncpy(n.name, n.caps & (MOS_GPU_CAP_VIRGL | MOS_GPU_CAP_VENUS) ? "virtio-gpu" : "framebuffer", 31);
#ifndef MOS_GPU_HOST
        if (gpu_count()) {
            const gpu_info_t *g = gpu_get(0);
            n.vendor = g->vendor;
            n.device = g->device;
        }
#endif
        if (len < sizeof n) return -MOS_EINVAL;
        return wr(p, user, &n, sizeof n);
    }
    case MOS_GPU_CTX_CREATE: {
        mos_gpu_ctx_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        if (in.kind != MOS_GPU_CTX_VIRGL && in.kind != MOS_GPU_CTX_VENUS) return -MOS_EINVAL;
        if (in.kind == MOS_GPU_CTX_VIRGL && !(caps_now() & MOS_GPU_CAP_VIRGL)) return -MOS_ENXIO;
        if (in.kind == MOS_GPU_CTX_VENUS && !(caps_now() & MOS_GPU_CAP_VENUS)) return -MOS_ENXIO;
        ctx_t *c = 0;
        for (int i = 0; i < GPU_N; i++) if (!ctxs[i].used) { c = &ctxs[i]; break; }
        if (!c) return -MOS_ENOMEM;
        u32 id = seq_id++;
#ifndef MOS_GPU_HOST
        e = virtio_gpu_ctx(id, in.kind, 1);
        if (e) return e;
#endif
        c->used = 1; c->pid = pid; c->id = id; c->kind = in.kind;
        in.ctx = id;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_CTX_DESTROY: {
        mos_gpu_ctx_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        ctx_t *c = ctx_of(pid, in.ctx);
        if (!c) return -MOS_ENOENT;
#ifndef MOS_GPU_HOST
        virtio_gpu_ctx(c->id, c->kind, 0);
#endif
        c->used = 0;
        return 0;
    }
    case MOS_GPU_BO_CREATE: {
        mos_gpu_bo_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        if (!in.size || (in.size & 0xFFF) || in.size > (16ull << 20)) return -MOS_EINVAL;
        if (in.ctx && !ctx_of(pid, in.ctx)) return -MOS_ENOENT;
        bo_t *b = 0;
        for (int i = 0; i < GPU_N; i++) if (!bos[i].used) { b = &bos[i]; break; }
        if (!b) return -MOS_ENOMEM;
        u64 phys = 0;
        if (!dma_alloc((usize)in.size, &phys)) return -MOS_ENOMEM;
        u32 id = seq_id++;
#ifndef MOS_GPU_HOST
        if (in.ctx) {
            e = virtio_gpu_bo(in.ctx, id, in.size, 1);
            if (e) { pmm_free(phys, in.size / 4096); return e; }
        }
#endif
        memset(b, 0, sizeof *b);
        b->used = 1; b->pid = pid; b->handle = id; b->ctx = in.ctx; b->flags = in.flags; b->size = in.size; b->phys = phys;
        in.handle = id;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_BO_DESTROY: {
        mos_gpu_bo_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        bo_t *b = bo_of(pid, in.handle);
        if (!b) return -MOS_ENOENT;
        if (b->mapped || b->ios) return -MOS_EBUSY;
        int sharers = 0;
        for (int i = 0; i < GPU_N; i++) if (bos[i].used && bos[i].phys == b->phys) sharers++;
#ifndef MOS_GPU_HOST
        if (b->ctx) virtio_gpu_bo(b->ctx, b->handle, b->size, 0);
#endif
        if (sharers == 1) pmm_free(b->phys, b->size / 4096);
        b->used = 0;
        return 0;
    }
    case MOS_GPU_BO_MAP: {
        mos_gpu_map_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        bo_t *b = bo_of(pid, in.handle);
        if (!b) return -MOS_ENOENT;
        if (!(b->flags & (MOS_GPU_BO_READ | MOS_GPU_BO_WRITE))) return -MOS_EINVAL;
        if (b->mapped) return -MOS_EBUSY;
#ifdef MOS_GPU_HOST
        b->map = b->phys;
#else
        if (!p->mmap_next) p->mmap_next = USER_MMAP_BASE;
        u64 va = (p->mmap_next + 0xFFF) & ~0xFFFULL;
        if (!gpu_user_map(p, va, b->phys, b->size, !!(b->flags & MOS_GPU_BO_WRITE))) return -MOS_ENOMEM;
        p->mmap_next = va + b->size + 4096;
        b->map = va;
#endif
        b->mapped = 1;
        in.addr = b->map;
        in.len = b->size;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_BO_UNMAP: {
        mos_gpu_map_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        bo_t *b = bo_of(pid, in.handle);
        if (!b || !b->mapped) return -MOS_ENOENT;
#ifndef MOS_GPU_HOST
        gpu_user_unmap(p, b->map, b->size);
#endif
        b->mapped = 0;
        b->map = 0;
        return 0;
    }
    case MOS_GPU_BO_EXPORT: {
        mos_gpu_bo_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        bo_t *b = bo_of(pid, in.handle);
        if (!b) return -MOS_ENOENT;
        if (!(b->flags & MOS_GPU_BO_SHARED)) return -MOS_EINVAL;
        if (!b->token) b->token = seq_id++;
        in.token = b->token;
        in.size = b->size;
        in.flags = b->flags;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_BO_IMPORT: {
        mos_gpu_bo_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        bo_t *src = 0;
        for (int i = 0; i < GPU_N; i++) if (bos[i].used && bos[i].token && bos[i].token == in.token) { src = &bos[i]; break; }
        if (!src) return -MOS_ENOENT;
        bo_t *b = 0;
        for (int i = 0; i < GPU_N; i++) if (!bos[i].used) { b = &bos[i]; break; }
        if (!b) return -MOS_ENOMEM;
        memset(b, 0, sizeof *b);
        b->used = 1; b->pid = pid; b->handle = seq_id++; b->flags = src->flags; b->token = src->token;
        b->size = src->size; b->phys = src->phys; b->ctx = 0;
        in.handle = b->handle;
        in.size = b->size;
        in.flags = b->flags;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_SUBMIT: {
        mos_gpu_submit_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        ctx_t *c = ctx_of(pid, in.ctx);
        if (!c) return -MOS_ENOENT;
        if (!in.cmd_bytes || in.cmd_bytes > GPU_CMD_MAX || !(in.flags & MOS_GPU_SUBMIT_3D)) return -MOS_EINVAL;
        if (in.fence && !fen_of(pid, in.fence)) return -MOS_ENOENT;
        u8 tmp[GPU_CMD_MAX];
        e = rd(p, tmp, in.cmds, in.cmd_bytes, in.cmd_bytes);
        if (e) return e;
#ifndef MOS_GPU_HOST
        e = virtio_gpu_submit(c->id, tmp, in.cmd_bytes, in.fence);
        if (e) return e;
#else
        (void)c;
        (void)tmp;
#endif
        if (in.fence) signal_fence(pid, in.fence);
        return 0;
    }
    case MOS_GPU_FENCE_CREATE: {
        mos_gpu_fence_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        fen_t *f = 0;
        for (int i = 0; i < GPU_N; i++) if (!fens[i].used) { f = &fens[i]; break; }
        if (!f) return -MOS_ENOMEM;
        memset(f, 0, sizeof *f);
        f->used = 1; f->pid = pid; f->id = seq_id++; f->flags = in.flags;
        in.fence = f->id;
        in.seq = 0;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_FENCE_SIGNAL: {
        mos_gpu_fence_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        if (!fen_of(pid, in.fence)) return -MOS_ENOENT;
        signal_fence(pid, in.fence);
        in.seq = fen_of(pid, in.fence)->seq;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_FENCE_WAIT: {
        mos_gpu_fence_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        fen_t *f = fen_of(pid, in.fence);
        if (!f) return -MOS_ENOENT;
        if (f->seq) {
            in.seq = f->seq;
            return wr(p, user, &in, sizeof in);
        }
#ifdef MOS_GPU_HOST
        return -MOS_EAGAIN;
#else
        if (!in.timeout_ms || !self) return -MOS_EAGAIN;
        self->wait_pid = -12;
        self->futex_va = f->id;
        self->wait_status = user;
        self->wake_at = in.timeout_ms == ~0u ? 0 : timer_ticks() + in.timeout_ms;
        self->state = TH_WAITING;
        if (blocked) *blocked = 1;
        return 0;
#endif
    }
    case MOS_GPU_FENCE_DESTROY: {
        mos_gpu_fence_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        fen_t *f = fen_of(pid, in.fence);
        if (!f) return -MOS_ENOENT;
#ifndef MOS_GPU_HOST
        wake_fence(pid, f->id, -MOS_ENOENT, f->seq);
#endif
        f->used = 0;
        return 0;
    }
    case MOS_GPU_IOSURF_CREATE: {
        mos_gpu_iosurface_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        bo_t *b = bo_of(pid, in.bo);
        if (!b) return -MOS_ENOENT;
        if (!in.width || !in.height || in.stride < in.width * 4) return -MOS_EINVAL;
        if (in.format != MOS_GPU_FMT_BGRA8 && in.format != MOS_GPU_FMT_RGBA8) return -MOS_EINVAL;
        if ((u64)in.stride * in.height > b->size) return -MOS_EINVAL;
        io_t *s = 0;
        for (int i = 0; i < GPU_N; i++) if (!ios[i].used) { s = &ios[i]; break; }
        if (!s) return -MOS_ENOMEM;
        memset(s, 0, sizeof *s);
        s->used = 1; s->pid = pid; s->id = seq_id++; s->bo = b->handle;
        s->width = in.width; s->height = in.height; s->stride = in.stride; s->format = in.format;
        b->ios++;
        in.id = s->id;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_IOSURF_EXPORT: {
        mos_gpu_iosurface_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        io_t *s = 0;
        for (int i = 0; i < GPU_N; i++) if (ios[i].used && ios[i].pid == pid && ios[i].id == in.id) { s = &ios[i]; break; }
        if (!s) return -MOS_ENOENT;
        if (!s->token) s->token = seq_id++;
        in.token = s->token;
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_IOSURF_LOOKUP: {
        mos_gpu_iosurface_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        io_t *s = 0;
        for (int i = 0; i < GPU_N; i++) {
            if (!ios[i].used) continue;
            if (in.token && ios[i].token == in.token) { s = &ios[i]; break; }
            if (!in.token && ios[i].pid == pid && ios[i].id == in.id) { s = &ios[i]; break; }
        }
        if (!s) return -MOS_ENOENT;
        in.id = s->id; in.width = s->width; in.height = s->height; in.stride = s->stride; in.format = s->format; in.token = s->token;
        if (s->pid == pid) in.bo = s->bo;
        else {
            bo_t *src = 0;
            for (int i = 0; i < GPU_N; i++) if (bos[i].used && bos[i].pid == s->pid && bos[i].handle == s->bo) { src = &bos[i]; break; }
            if (!src) return -MOS_ENOENT;
            if (!src->token) src->token = seq_id++;
            bo_t *b = 0;
            for (int i = 0; i < GPU_N; i++) if (!bos[i].used) { b = &bos[i]; break; }
            if (!b) return -MOS_ENOMEM;
            memset(b, 0, sizeof *b);
            b->used = 1; b->pid = pid; b->handle = seq_id++; b->flags = src->flags | MOS_GPU_BO_SHARED;
            b->token = src->token; b->size = src->size; b->phys = src->phys;
            in.bo = b->handle;
        }
        return wr(p, user, &in, sizeof in);
    }
    case MOS_GPU_IOSURF_DESTROY: {
        mos_gpu_iosurface_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        for (int i = 0; i < GPU_N; i++) {
            if (!ios[i].used || ios[i].pid != pid || ios[i].id != in.id) continue;
            bo_t *b = bo_of(pid, ios[i].bo);
            if (b && b->ios) b->ios--;
            ios[i].used = 0;
            return 0;
        }
        return -MOS_ENOENT;
    }
    case MOS_GPU_KMS_GET: {
        mos_gpu_kms_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        if (in.connector > 1) return -MOS_EINVAL;
        if (!(caps_now() & MOS_GPU_CAP_KMS) && !kms[in.connector].width) return -MOS_ENOENT;
        in = kms[in.connector];
        in.connector = in.connector;
        return wr(p, user, &kms[in.connector ? 1 : 0], sizeof(mos_gpu_kms_t));
    }
    case MOS_GPU_KMS_SET: {
        mos_gpu_kms_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        if (in.connector > 1) return -MOS_EINVAL;
        if (!(caps_now() & MOS_GPU_CAP_KMS)) return -MOS_ENOENT;
        u64 phys = 0;
        if (in.bo) {
            bo_t *b = bo_of(pid, in.bo);
            if (!b) return -MOS_ENOENT;
            phys = b->phys;
        }
#ifndef MOS_GPU_HOST
        e = gpu_kms_set(in.connector, in.width, in.height, in.stride, in.format, phys);
        if (e) return e;
#else
        (void)phys;
        if (!in.width || !in.height || in.stride < in.width * 4) return -MOS_EINVAL;
#endif
        kms[in.connector] = in;
        return 0;
    }
    default:
        return -MOS_EINVAL;
    }
}

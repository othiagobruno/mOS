/* Serviço da ABI em kernel/include/mos_gpu.h. O virtio-gpu 3D e o KMS entram por baixo. */
#ifdef MOS_GPU_HOST
#include <stdbool.h>
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
#define MOS_ETIMEDOUT 60
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
#define sched_wake(t) ((t)->state = TH_READY)
static u32 host_caps;
void gpu_host_caps(u32 c) { host_caps = c; }
#else
#include "kernel.h"
#include "syscall.h"
#include "proc.h"
#include "gpu.h"
#include "amdgpu/amdgpu.h"
#endif

/* BOs MOS_GPU_BO_GPU na VRAM da AMD (dev/amdgpu): alocador da VRAM, mapa de dispositivo WC no processo
 * (gpu_user_map_io, sem refcount). Sem GPU AMD o BO fica na RAM, como antes. */
#define GPU_VRAM_BO_MAX (64ull << 20)
#ifndef MOS_GPU_HOST
static bool vram_avail(void) { return amdgpu_main() != NULL; }
static u64 vram_get(u64 n) {
    u64 ph = amdgpu_vram_alloc(amdgpu_main(), n);
    if (ph) memset((void *)(usize)ph, 0, (usize)n);
    return ph;
}
static bool vram_is(u64 ph, u64 n) { return amdgpu_vram_owns(amdgpu_main(), ph, n); }
static void vram_put(u64 ph, u64 n) { amdgpu_vram_free(amdgpu_main(), ph, n); }
#else
static bool vram_avail(void) { return false; }
static u64 vram_get(u64 n) { (void)n; return 0; }
static bool vram_is(u64 ph, u64 n) { (void)ph; (void)n; return false; }
static void vram_put(u64 ph, u64 n) { (void)ph; (void)n; }
#endif
/* revisão 3: GPU AMD nativa (dev/amdgpu/abi.c). No host do teste da ABI não há AMD. */
#ifndef MOS_GPU_HOST
static bool amd_on(void) { return amdgpu_abi_available(); }
#else
static bool amd_on(void) { return false; }
static int amdgpu_abi_bo_map(int pid, u64 phys, u64 size, bool vram, bool exec, u64 *va) { (void)pid; (void)phys; (void)size; (void)vram; (void)exec; (void)va; return -MOS_EINVAL; }
static void amdgpu_abi_bo_unmap(int pid, u64 va, u64 size) { (void)pid; (void)va; (void)size; }
static bool amdgpu_abi_done(u64 t) { (void)t; return true; }
static void amdgpu_abi_proc_exit(int pid) { (void)pid; }
#endif

#define GPU_N 64
#define GPU_CMD_MAX 400u

typedef struct { int used, pid; u32 id, kind; } ctx_t;
typedef struct {
    int used, pid, mapped, ios;
    u32 handle, ctx, flags, token;
    u64 size, phys, map;
    u64 gpu_va, ticket;   /* revisão 3: VA na VM do processo e o último lote que usou o BO */
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
    return host_caps | MOS_GPU_CAP_IOSURF;
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
        sched_wake(w);
    }
}
#else
enum { TH_READY = 0, TH_WAITING = 3, HOST_NW = 4 };
static thread_t *host_ws[HOST_NW];
static int host_wp[HOST_NW];
static i64 host_rax[HOST_NW];
static i64 host_wake_rax;
static thread_t *host_done_th[HOST_NW];
static i64 host_done_rax[HOST_NW];
static void host_drop(thread_t *t, i64 rax) {
    for (int i = 0; i < HOST_NW; i++) if (host_ws[i] == t) {
        host_rax[i] = rax;
        host_wake_rax = rax;
        sched_wake(t);
        t->wait_pid = 0;
        t->futex_va = 0;
        t->wake_at = 0;
        host_ws[i] = 0;
    }
    for (int i = 0; i < HOST_NW; i++) if (!host_done_th[i] || host_done_th[i] == t) {
        host_done_th[i] = t;
        host_done_rax[i] = rax;
        break;
    }
}
static void wake_fence(int pid, u32 id, i64 rax, u32 seq) {
    for (int i = 0; i < HOST_NW; i++) {
        thread_t *t = host_ws[i];
        if (!t || host_wp[i] != pid || t->wait_pid != -12 || t->futex_va != id) continue;
        if (rax == 0 && t->wait_status) memcpy((void *)(uintptr_t)(t->wait_status + 12), &seq, 4);
        host_drop(t, rax);
    }
}
void gpu_host_fence_expire(void) {
    for (int i = 0; i < HOST_NW; i++) {
        thread_t *t = host_ws[i];
        if (!t || t->wait_pid != -12 || !t->wake_at) continue;
        host_drop(t, -MOS_ETIMEDOUT);
    }
}
i64 gpu_host_wake_rax(void) { return host_wake_rax; }
i64 gpu_host_waiter_rax(thread_t *t) {
    for (int i = 0; i < HOST_NW; i++) if (host_done_th[i] == t) return host_done_rax[i];
    return 0;
}
#endif

static void signal_fence(int pid, u32 id);
void gpu_fence_from_irq(int pid, unsigned id) { signal_fence(pid, (u32)id); }

#ifdef MOS_GPU_HOST
static struct { int pid; u32 id; int live; } host_q[8];
void gpu_host_irq(void) {
    for (int i = 0; i < 8; i++) if (host_q[i].live) {
        int pid = host_q[i].pid;
        u32 id = host_q[i].id;
        host_q[i].live = 0;
        gpu_fence_from_irq(pid, id);
    }
}
static int host_post(int pid, u32 id) {
    for (int i = 0; i < 8; i++) if (!host_q[i].live) {
        host_q[i].pid = pid; host_q[i].id = id; host_q[i].live = 1; return 0;
    }
    return -MOS_EBUSY;
}
#endif

static void signal_fence(int pid, u32 id) {
    fen_t *f = fen_of(pid, id);
    if (!f) return;
    f->seq++;
    wake_fence(pid, id, 0, f->seq);
}

static int batch_u32(const u8 *b, u32 n, u32 off, u32 *out) {
    if (off + 4 > n) return -1;
    memcpy(out, b + off, 4);
    return 0;
}

static int batch_ok(int pid, u32 ctx_kind, const u8 *b, u32 n) {
    mos_gpu_batch_hdr_t h;
    u32 handles[MOS_GPU_BATCH_MAX_BO];
    u32 cmd_at[MOS_GPU_BATCH_MAX_CMD], cmd_sz[MOS_GPU_BATCH_MAX_CMD];
    u32 off, i, pos;
    if (n < sizeof h || (n & 3)) return -MOS_EINVAL;
    memcpy(&h, b, sizeof h);
    if (h.version != MOS_GPU_BATCH_VER || h.kind != ctx_kind || h.bytes != n || h.pad0 || h.pad1) return -MOS_EINVAL;
    if (!h.ncmd || h.ncmd > MOS_GPU_BATCH_MAX_CMD || h.nbo > MOS_GPU_BATCH_MAX_BO || h.nreloc > MOS_GPU_BATCH_MAX_RELOC)
        return -MOS_EINVAL;
    off = (u32)sizeof h;
    if (off + h.nbo * 4u + h.nreloc * (u32)sizeof(mos_gpu_reloc_t) > n) return -MOS_EINVAL;
    for (i = 0; i < h.nbo; i++) {
        if (batch_u32(b, n, off + i * 4u, &handles[i]) || !bo_of(pid, handles[i])) return -MOS_ENOENT;
    }
    off += h.nbo * 4u;
    pos = off + h.nreloc * (u32)sizeof(mos_gpu_reloc_t);
    for (i = 0; i < h.ncmd; i++) {
        mos_gpu_cmd_hdr_t c;
        u32 expect;
        if (pos + sizeof c > n) return -MOS_EINVAL;
        memcpy(&c, b + pos, sizeof c);
        if (c.bytes < sizeof c || (c.bytes & 3) || pos + c.bytes > n) return -MOS_EINVAL;
        if (c.op < MOS_GPU_CMD_BIND || c.op > MOS_GPU_CMD_DISPATCH) return -MOS_EINVAL;
        if (ctx_kind == MOS_GPU_CTX_VIRGL) {
            expect = c.op == MOS_GPU_CMD_BIND ? MOS_GPU_VIRGL_SET_SHADER_BUFFERS :
                     c.op == MOS_GPU_CMD_SHADER ? MOS_GPU_VIRGL_BIND_SHADER : MOS_GPU_VIRGL_LAUNCH_GRID;
            if (c.venus || c.virgl != expect) return -MOS_EINVAL;
        } else {
            expect = c.op == MOS_GPU_CMD_BIND ? MOS_GPU_VENUS_BIND :
                     c.op == MOS_GPU_CMD_SHADER ? MOS_GPU_VENUS_SHADER : MOS_GPU_VENUS_DISPATCH;
            if (c.virgl || c.venus != expect) return -MOS_EINVAL;
        }
        if (c.op == MOS_GPU_CMD_BIND) {
            mos_gpu_bind_t bd;
            bo_t *bo;
            if (c.bytes < sizeof c + sizeof bd) return -MOS_EINVAL;
            memcpy(&bd, b + pos + sizeof c, sizeof bd);
            if (bd.slot > 31 || bd.bo_index >= h.nbo || !bd.range) return -MOS_EINVAL;
            bo = bo_of(pid, handles[bd.bo_index]);
            if (!bo || (u64)bd.offset + bd.range > bo->size) return bo ? -MOS_EINVAL : -MOS_ENOENT;
        } else if (c.op == MOS_GPU_CMD_SHADER) {
            mos_gpu_shader_t sh;
            bo_t *bo;
            if (c.bytes < sizeof c + sizeof sh) return -MOS_EINVAL;
            memcpy(&sh, b + pos + sizeof c, sizeof sh);
            if (sh.bo_index >= h.nbo || !sh.length || !sh.local_x || !sh.local_y || !sh.local_z) return -MOS_EINVAL;
            bo = bo_of(pid, handles[sh.bo_index]);
            if (!bo || (u64)sh.offset + sh.length > bo->size) return bo ? -MOS_EINVAL : -MOS_ENOENT;
        } else {
            mos_gpu_dispatch_t d;
            if (c.bytes < sizeof c + sizeof d) return -MOS_EINVAL;
            memcpy(&d, b + pos + sizeof c, sizeof d);
            if (!d.x || !d.y || !d.z || d.x > 65535 || d.y > 65535 || d.z > 65535) return -MOS_EINVAL;
        }
        cmd_at[i] = pos;
        cmd_sz[i] = c.bytes;
        pos += c.bytes;
    }
    if (pos != n) return -MOS_EINVAL;
    for (i = 0; i < h.nreloc; i++) {
        mos_gpu_reloc_t r;
        u32 word;
        bo_t *bo;
        memcpy(&r, b + off + i * sizeof r, sizeof r);
        if (r.cmd >= h.ncmd || r.bo_index >= h.nbo) return -MOS_EINVAL;
        if ((r.off & 3) || r.off < sizeof(mos_gpu_cmd_hdr_t) || r.off + 4 > cmd_sz[r.cmd]) return -MOS_EINVAL;
        memcpy(&word, b + cmd_at[r.cmd] + r.off, 4);
        if (word != r.bo_index) return -MOS_EINVAL;
        bo = bo_of(pid, handles[r.bo_index]);
        if (!bo || r.bo_off >= bo->size) return bo ? -MOS_EINVAL : -MOS_ENOENT;
    }
    return 0;
}

static int put32(u8 *o, u32 *n, u32 cap, u32 v) {
    if (!o || *n + 4 > cap) return -MOS_EINVAL;
    memcpy(o + *n, &v, 4);
    *n += 4;
    return 0;
}

int gpu_batch_translate(int pid, u32 kind, const void *src, u32 n, void *dst, u32 cap, u32 *out_n) {
    const u8 *b = src;
    u8 *o = dst;
    mos_gpu_batch_hdr_t h;
    u32 handles[MOS_GPU_BATCH_MAX_BO];
    u32 off, pos, i, on = 0, lx = 1, ly = 1, lz = 1;
    int e = batch_ok(pid, kind, b, n);
    if (e) return e;
    memcpy(&h, b, sizeof h);
    off = (u32)sizeof h;
    for (i = 0; i < h.nbo; i++) memcpy(&handles[i], b + off + i * 4u, 4);
    pos = off + h.nbo * 4u + h.nreloc * (u32)sizeof(mos_gpu_reloc_t);
    for (i = 0; i < h.ncmd; i++) {
        mos_gpu_cmd_hdr_t c;
        memcpy(&c, b + pos, sizeof c);
        if (c.op == MOS_GPU_CMD_BIND) {
            mos_gpu_bind_t bd;
            u32 res;
            memcpy(&bd, b + pos + sizeof c, sizeof bd);
            res = handles[bd.bo_index];
            if (kind == MOS_GPU_CTX_VIRGL) {
                if (put32(o, &on, cap, 32u | (6u << 16)) || put32(o, &on, cap, 3) || put32(o, &on, cap, bd.slot) ||
                    put32(o, &on, cap, 1) || put32(o, &on, cap, res) || put32(o, &on, cap, bd.offset) || put32(o, &on, cap, bd.range))
                    return -MOS_EINVAL;
            } else if (put32(o, &on, cap, 5) || put32(o, &on, cap, MOS_GPU_VENUS_BIND) || put32(o, &on, cap, bd.slot) ||
                       put32(o, &on, cap, res) || put32(o, &on, cap, bd.offset) || put32(o, &on, cap, bd.range))
                return -MOS_EINVAL;
        } else if (c.op == MOS_GPU_CMD_SHADER) {
            mos_gpu_shader_t sh;
            u32 res;
            memcpy(&sh, b + pos + sizeof c, sizeof sh);
            res = handles[sh.bo_index];
            lx = sh.local_x; ly = sh.local_y; lz = sh.local_z;
            if (kind == MOS_GPU_CTX_VIRGL) {
                if (put32(o, &on, cap, 31u | (2u << 16)) || put32(o, &on, cap, res) || put32(o, &on, cap, 3)) return -MOS_EINVAL;
            } else if (put32(o, &on, cap, 7) || put32(o, &on, cap, MOS_GPU_VENUS_SHADER) || put32(o, &on, cap, res) ||
                       put32(o, &on, cap, sh.offset) || put32(o, &on, cap, sh.length) || put32(o, &on, cap, lx) ||
                       put32(o, &on, cap, ly) || put32(o, &on, cap, lz))
                return -MOS_EINVAL;
        } else {
            mos_gpu_dispatch_t d;
            memcpy(&d, b + pos + sizeof c, sizeof d);
            if (kind == MOS_GPU_CTX_VIRGL) {
                if (put32(o, &on, cap, 35u | (8u << 16)) || put32(o, &on, cap, lx) || put32(o, &on, cap, ly) || put32(o, &on, cap, lz) ||
                    put32(o, &on, cap, d.x) || put32(o, &on, cap, d.y) || put32(o, &on, cap, d.z) || put32(o, &on, cap, 0) || put32(o, &on, cap, 0))
                    return -MOS_EINVAL;
            } else if (put32(o, &on, cap, 4) || put32(o, &on, cap, MOS_GPU_VENUS_DISPATCH) || put32(o, &on, cap, d.x) ||
                       put32(o, &on, cap, d.y) || put32(o, &on, cap, d.z))
                return -MOS_EINVAL;
        }
        pos += c.bytes;
    }
    if (out_n) *out_n = on;
    return 0;
}

#ifndef MOS_GPU_HOST
/* revisão 3: lote AMD (docs/gpu-abi.md). Os handles viram {VA, tamanho, memória}; dev/amdgpu/abi.c copia, valida
 * e envia; a fence sinaliza pelo EOP. */
static int amd_submit(process_t *p, ctx_t *c, const mos_gpu_submit_t *in) {
    int pid = p->pid;
    if (c->kind != MOS_GPU_CTX_AMD || !amd_on()) return -MOS_EINVAL;
    if (in->cmd_bytes < sizeof(mos_gpu_amd_batch_t) || in->cmd_bytes > MOS_GPU_AMD_BATCH_MAX) return -MOS_EINVAL;
    if (in->fence && !fen_of(pid, in->fence)) return -MOS_ENOENT;
    static u8 buf[MOS_GPU_AMD_BATCH_MAX];
    int e = rd(p, buf, in->cmds, in->cmd_bytes, in->cmd_bytes);
    if (e) return e;
    mos_gpu_amd_batch_t h;
    memcpy(&h, buf, sizeof h);
    if (h.version != MOS_GPU_AMD_BATCH_VER || !h.nib || h.nib > 16 || h.nbo > 256 || h.pad) return -MOS_EINVAL;
    u64 need = sizeof h + (u64)h.nib * sizeof(mos_gpu_amd_ib_t) + (u64)h.nbo * 4;
    if (need != in->cmd_bytes) return -MOS_EINVAL;
    static amd_abi_bo_t ab[256];
    static bo_t *used[256];
    const u8 *hp = buf + sizeof h + (u64)h.nib * sizeof(mos_gpu_amd_ib_t);
    for (u32 i = 0; i < h.nbo; i++) {
        u32 hd;
        memcpy(&hd, hp + 4 * i, 4);
        bo_t *b = bo_of(pid, hd);
        if (!b || !b->gpu_va) return -MOS_EINVAL;
        ab[i].va = b->gpu_va; ab[i].size = b->size; ab[i].phys = b->phys; ab[i].vram = vram_is(b->phys, b->size);
        used[i] = b;
    }
    amd_abi_ib_t ibs[16];
    u32 ring = 0;
    for (u32 k = 0; k < h.nib; k++) {
        mos_gpu_amd_ib_t ib;
        memcpy(&ib, buf + sizeof h + k * sizeof ib, sizeof ib);
        bo_t *b = bo_of(pid, ib.bo);
        if (!b || !b->gpu_va || ib.ring > MOS_GPU_AMD_RING_COMPUTE || (k && ib.ring != ring) || !ib.size_dw ||
            (u64)(ib.offset_dw + (u64)ib.size_dw) * 4 > b->size)
            return -MOS_EINVAL;
        bool listed = false;
        for (u32 i = 0; i < h.nbo; i++) if (used[i] == b) listed = true;
        if (!listed) return -MOS_EINVAL;                       /* o BO do IB também vai na lista */
        ring = ib.ring;
        ibs[k].va = b->gpu_va + 4ull * ib.offset_dw;
        ibs[k].ndw = ib.size_dw;
    }
    u64 ticket = 0;
    e = amdgpu_abi_submit(pid, ring, ibs, h.nib, ab, h.nbo, in->fence, &ticket);
    if (e) return e;
    for (u32 i = 0; i < h.nbo; i++) used[i]->ticket = ticket;   /* residente até o lote passar */
    return 0;
}
#else
static int amd_submit(process_t *p, ctx_t *c, const mos_gpu_submit_t *in) { (void)p; (void)c; (void)in; return -MOS_EINVAL; }
#endif

/* Saída (ou exec) do processo: a GPU termina o que é dele, a VM some, e os BOs, contextos, fences e superfícies
 * voltam. Os mapas de usuário saem antes de o espaço de endereços ser desmontado. */
void gpu_proc_exit(process_t *p) {
    if (!p) return;
    int pid = p->pid;
    amdgpu_abi_proc_exit(pid);
    for (int i = 0; i < GPU_N; i++) if (ios[i].used && ios[i].pid == pid) ios[i].used = 0;
    for (int i = 0; i < GPU_N; i++) {
        bo_t *b = &bos[i];
        if (!b->used || b->pid != pid) continue;
#ifndef MOS_GPU_HOST
        if (b->mapped) { if (vram_is(b->phys, b->size)) gpu_user_unmap_io(p, b->map, b->size); else gpu_user_unmap(p, b->map, b->size); }
        if (b->ctx && !b->gpu_va && (caps_now() & (MOS_GPU_CAP_VIRGL | MOS_GPU_CAP_VENUS))) virtio_gpu_bo(b->ctx, b->handle, b->size, 0);
#endif
        int sharers = 0;
        for (int j = 0; j < GPU_N; j++) if (bos[j].used && bos[j].phys == b->phys) sharers++;
        if (sharers == 1) { if (vram_is(b->phys, b->size)) vram_put(b->phys, b->size); else pmm_free(b->phys, b->size / 4096); }
        b->used = 0;
    }
    for (int i = 0; i < GPU_N; i++) if (ctxs[i].used && ctxs[i].pid == pid) {
#ifndef MOS_GPU_HOST
        if (ctxs[i].kind != MOS_GPU_CTX_AMD) virtio_gpu_ctx(ctxs[i].id, ctxs[i].kind, 0);
#endif
        ctxs[i].used = 0;
    }
    for (int i = 0; i < GPU_N; i++) if (fens[i].used && fens[i].pid == pid) fens[i].used = 0;
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
        if (amd_on()) {
            n.caps |= MOS_GPU_CAP_AMD;
            memset(n.name, 0, sizeof n.name);
            strncpy(n.name, "amdgpu-gfx9", 31);
            amdgpu_abi_info(&n.amd_family, &n.num_cu, &n.vram_bytes, &n.gtt_bytes);
        }
#endif
#ifndef MOS_GPU_HOST
        if (gpu_count()) {
            const gpu_info_t *g = gpu_get(0);
            n.vendor = g->vendor;
            n.device = g->device;
        }
#endif
        if (len < MOS_GPU_INFO_V2_SIZE) return -MOS_EINVAL;
        return wr(p, user, &n, len < sizeof n ? MOS_GPU_INFO_V2_SIZE : sizeof n);   /* a revisão 2 recebe só até name */
    }
    case MOS_GPU_CTX_CREATE: {
        mos_gpu_ctx_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        if (in.kind != MOS_GPU_CTX_VIRGL && in.kind != MOS_GPU_CTX_VENUS && !(in.kind == MOS_GPU_CTX_AMD && amd_on())) return -MOS_EINVAL;
        ctx_t *c = 0;
        for (int i = 0; i < GPU_N; i++) if (!ctxs[i].used) { c = &ctxs[i]; break; }
        if (!c) return -MOS_ENOMEM;
        u32 id = seq_id++;
#ifndef MOS_GPU_HOST
        if ((in.kind == MOS_GPU_CTX_VIRGL && (caps_now() & MOS_GPU_CAP_VIRGL)) ||
            (in.kind == MOS_GPU_CTX_VENUS && (caps_now() & MOS_GPU_CAP_VENUS))) {
            e = virtio_gpu_ctx(id, in.kind, 1);
            if (e) return e;
        }
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
        if (c->kind != MOS_GPU_CTX_AMD) virtio_gpu_ctx(c->id, c->kind, 0);
#endif
        c->used = 0;
        return 0;
    }
    case MOS_GPU_BO_CREATE: {
        mos_gpu_bo_t in;
        memset(&in, 0, sizeof in);
        int e = rd(p, &in, user, len, len >= sizeof in ? sizeof in : MOS_GPU_BO_V2_SIZE);
        if (e) return e;
        bool in_vram = (in.flags & MOS_GPU_BO_GPU) && vram_avail();
        if (!in.size || (in.size & 0xFFF) || in.size > (in_vram ? GPU_VRAM_BO_MAX : (16ull << 20))) return -MOS_EINVAL;
        if (in.ctx && !ctx_of(pid, in.ctx)) return -MOS_ENOENT;
        bo_t *b = 0;
        for (int i = 0; i < GPU_N; i++) if (!bos[i].used) { b = &bos[i]; break; }
        if (!b) return -MOS_ENOMEM;
        u64 phys = 0;
        if (in_vram) { if (!(phys = vram_get(in.size))) return -MOS_ENOMEM; }
        else if (!dma_alloc((usize)in.size, &phys)) return -MOS_ENOMEM;
        u32 id = seq_id++;
#ifndef MOS_GPU_HOST
        if (in.ctx && ctx_of(pid, in.ctx)->kind != MOS_GPU_CTX_AMD && (caps_now() & (MOS_GPU_CAP_VIRGL | MOS_GPU_CAP_VENUS))) {
            e = virtio_gpu_bo(in.ctx, id, in.size, 1);
            if (e) { if (in_vram) vram_put(phys, in.size); else pmm_free(phys, in.size / 4096); return e; }
        }
#endif
        in.gpu_va = 0;
        if (in.ctx && ctx_of(pid, in.ctx)->kind == MOS_GPU_CTX_AMD) {   /* VA na VM do processo */
            e = amdgpu_abi_bo_map(pid, phys, in.size, in_vram, !!(in.flags & MOS_GPU_BO_EXEC), &in.gpu_va);
            if (e) { if (in_vram) vram_put(phys, in.size); else pmm_free(phys, in.size / 4096); return e; }
        }
        memset(b, 0, sizeof *b);
        b->used = 1; b->pid = pid; b->handle = id; b->ctx = in.ctx; b->flags = in.flags; b->size = in.size; b->phys = phys;
        b->gpu_va = in.gpu_va;
        in.handle = id;
        return wr(p, user, &in, len >= sizeof in ? sizeof in : MOS_GPU_BO_V2_SIZE);
    }
    case MOS_GPU_BO_DESTROY: {
        mos_gpu_bo_t in;
        int e = rd(p, &in, user, len, 4);
        if (e) return e;
        bo_t *b = bo_of(pid, in.handle);
        if (!b) return -MOS_ENOENT;
        if (b->mapped || b->ios || !amdgpu_abi_done(b->ticket)) return -MOS_EBUSY;
        if (b->gpu_va) amdgpu_abi_bo_unmap(pid, b->gpu_va, b->size);
        int sharers = 0;
        for (int i = 0; i < GPU_N; i++) if (bos[i].used && bos[i].phys == b->phys) sharers++;
#ifndef MOS_GPU_HOST
        if (b->ctx && !b->gpu_va) virtio_gpu_bo(b->ctx, b->handle, b->size, 0);
#endif
        if (sharers == 1) {
            if (vram_is(b->phys, b->size)) vram_put(b->phys, b->size);
            else pmm_free(b->phys, b->size / 4096);
        }
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
        bool ok = vram_is(b->phys, b->size) ? gpu_user_map_io(p, va, b->phys, b->size, !!(b->flags & MOS_GPU_BO_WRITE))
                                            : gpu_user_map(p, va, b->phys, b->size, !!(b->flags & MOS_GPU_BO_WRITE));
        if (!ok) return -MOS_ENOMEM;
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
        if (vram_is(b->phys, b->size)) gpu_user_unmap_io(p, b->map, b->size);
        else gpu_user_unmap(p, b->map, b->size);
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
        memset(&in, 0, sizeof in);
        int e = rd(p, &in, user, len, len >= sizeof in ? sizeof in : MOS_GPU_BO_V2_SIZE);
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
        in.gpu_va = 0;
        if (in.ctx && ctx_of(pid, in.ctx) && ctx_of(pid, in.ctx)->kind == MOS_GPU_CTX_AMD) {
            e = amdgpu_abi_bo_map(pid, b->phys, b->size, vram_is(b->phys, b->size), !!(b->flags & MOS_GPU_BO_EXEC), &in.gpu_va);
            if (e) { b->used = 0; return e; }
            b->ctx = in.ctx;
        }
        b->gpu_va = in.gpu_va;
        in.handle = b->handle;
        in.size = b->size;
        in.flags = b->flags;
        return wr(p, user, &in, len >= sizeof in ? sizeof in : MOS_GPU_BO_V2_SIZE);
    }
    case MOS_GPU_SUBMIT: {
        mos_gpu_submit_t in;
        int e = rd(p, &in, user, len, sizeof in);
        if (e) return e;
        ctx_t *c = ctx_of(pid, in.ctx);
        if (!c) return -MOS_ENOENT;
        if (in.flags & MOS_GPU_SUBMIT_AMD) return amd_submit(p, c, &in);
        if (!in.cmd_bytes || in.cmd_bytes > GPU_CMD_MAX || !(in.flags & MOS_GPU_SUBMIT_3D)) return -MOS_EINVAL;
        if (in.fence && !fen_of(pid, in.fence)) return -MOS_ENOENT;
        u8 tmp[GPU_CMD_MAX];
        e = rd(p, tmp, in.cmds, in.cmd_bytes, in.cmd_bytes);
        if (e) return e;
        e = batch_ok(pid, c->kind, tmp, in.cmd_bytes);
        if (e) return e;
        {
            u8 dev[GPU_CMD_MAX];
            u32 dn = 0;
            e = gpu_batch_translate(pid, c->kind, tmp, in.cmd_bytes, dev, sizeof dev, &dn);
            if (e) return e;
#ifndef MOS_GPU_HOST
            {
                int hw = (c->kind == MOS_GPU_CTX_VENUS && (caps_now() & MOS_GPU_CAP_VENUS)) ||
                         (c->kind == MOS_GPU_CTX_VIRGL && (caps_now() & MOS_GPU_CAP_VIRGL));
                if (hw) {
                    e = virtio_gpu_submit(c->id, dev, dn, pid, in.fence);
                    if (e) return e;
                    return 0; /* fence no IRQ */
                }
            }
#else
            if (in.fence && ((c->kind == MOS_GPU_CTX_VENUS && (caps_now() & MOS_GPU_CAP_VENUS)) ||
                             (c->kind == MOS_GPU_CTX_VIRGL && (caps_now() & MOS_GPU_CAP_VIRGL)))) {
                e = host_post(pid, in.fence);
                if (e) return e;
                (void)dev; (void)dn;
                return 0; /* gpu_host_irq sinaliza */
            }
            (void)dev;
            (void)dn;
#endif
        }
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
        if (!in.timeout_ms || !self) return -MOS_EAGAIN;
#ifdef MOS_GPU_HOST
        {
            int slot = -1;
            for (int i = 0; i < HOST_NW; i++) if (!host_ws[i]) { slot = i; break; }
            if (slot < 0) return -MOS_EBUSY;
            self->wait_pid = -12;
            self->futex_va = f->id;
            self->wait_status = user;
            self->wake_at = in.timeout_ms == ~0u ? 0 : 1;
            self->state = TH_WAITING;
            host_ws[slot] = self;
            host_wp[slot] = pid;
        }
        if (blocked) *blocked = 1;
        return 0;
#else
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
        wake_fence(pid, f->id, -MOS_ENOENT, f->seq);
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
    case MOS_GPU_DISPLAY_INFO:
    case MOS_GPU_DISPLAY_BRIGHTNESS: {
#ifndef MOS_GPU_HOST
        static mos_gpu_display_t dsp;
        int e = rd(p, &dsp, user, len, sizeof dsp);
        if (e) return e;
        amdgpu_t *a = amdgpu_main();
        if (!a) return -MOS_ENOENT;
        if (!a->ddc_mask) amdgpu_ddc_probe(a);
        int line = -1;
        for (u32 d = 0, k = 0; d < AMD_NUM_DDC; d++)
            if (a->ddc_mask >> d & 1) { if (k == dsp.index) { line = (int)d; break; } k++; }
        if (line < 0) return -MOS_ENOENT;
        dsp.connector = (unsigned)line + 1;
        if ((u32)op == MOS_GPU_DISPLAY_INFO) {
            int n = amdgpu_ddc_read_edid(a, (u32)line, a->ddc_edid[line], sizeof a->ddc_edid[line]);   /* ao vivo */
            if (n > 0) a->ddc_edid_len[line] = (u32)n;
            dsp.edid_len = a->ddc_edid_len[line];
            memcpy(dsp.edid, a->ddc_edid[line], dsp.edid_len);
        } else {
            u16 cur = 0, max = 0;
            if (dsp.flags & MOS_GPU_DISPLAY_SET) {
                e = amdgpu_ddcci_get_vcp(a, (u32)line, 0x10, &cur, &max);
                if (e) return e;
                if (dsp.brightness > max) dsp.brightness = max;
                e = amdgpu_ddcci_set_vcp(a, (u32)line, 0x10, (u16)dsp.brightness);
                if (e) return e;
            }
            e = amdgpu_ddcci_get_vcp(a, (u32)line, 0x10, &cur, &max);
            if (e) return e;
            dsp.brightness = cur;
            dsp.brightness_max = max;
        }
        return wr(p, user, &dsp, sizeof dsp);
#else
        return -MOS_ENOENT;
#endif
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

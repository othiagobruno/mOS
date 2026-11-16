/* mOS — lado AMD da ABI de GPU revisão 3 (docs/gpu-abi.md): uma VM de GPU por processo, VA fixo por BO,
 * envio de lotes de IBs validados (pm4check.c) e fences que sinalizam pelo EOP. kernel/dev/gpu_abi.c cuida dos
 * handles e chama estas funções. Testado no host com o CP/MEC simulados (tools/amdgpu_hw_host.c).
 *
 * Os IBs são copiados para uma área do kernel antes da validação (o BO do processo continua gravável pela CPU
 * e pela GPU: validar no lugar abriria a corrida "valida e depois troca"). Essa área (pool) é por processo,
 * fica na VRAM e entra na VM do processo só de leitura num VA reservado; os INDIRECT_BUFFER de IB2 são
 * reescritos para a cópia. */
#include "amdgpu.h"
#include "regs.h"
#include "pm4check.h"
#ifndef MOS_HOST_TEST
#include "gpu.h"
#include "syscall.h"
#else
void gpu_fence_from_irq(int pid, unsigned fence);
#endif

#define POOL_BYTES   (1024u * 1024u)
#define POOL_VA      0x00007FFF00000000ull      /* reservado: os BOs nunca recebem VA aqui */
#define VA_FIRST     0x0000000100000000ull
#define VA_LIMIT     0x00007FF000000000ull
#define NPROC        15
#define NPEND        128

typedef struct {
    int       pid;                   /* 0 = livre */
    amd_vm_t *vm;
    u64       va_next;
    u64       pool;                  /* endereço no BAR0 */
    u32       pool_head;             /* bytes */
    u64       pool_ticket[POOL_BYTES / 4096];   /* último envio que usou cada página do pool */
    bool      lost;                  /* falha de página da GPU: os envios seguintes dão -MOS_EIO */
} proc_vm_t;

typedef struct { int pid; u32 fence; u64 ticket; } pend_t;

static proc_vm_t procs[NPROC];
static pend_t pend[NPEND];

bool amdgpu_abi_available(void) { amdgpu_t *a = amdgpu_main(); return a && a->gfx_ok; }

static proc_vm_t *pv_of(int pid, bool create) {
    for (int i = 0; i < NPROC; i++) if (procs[i].pid == pid && pid) return &procs[i];
    if (!create || !pid) return NULL;
    amdgpu_t *a = amdgpu_main();
    if (!a || !a->gfx_ok) return NULL;
    proc_vm_t *pv = NULL;
    for (int i = 0; i < NPROC; i++) if (!procs[i].pid) { pv = &procs[i]; break; }
    if (!pv) return NULL;
    amd_vm_t *vm = amdgpu_vm_create(a);
    if (!vm) return NULL;
    u64 pool = amdgpu_vram_alloc(a, POOL_BYTES);
    if (!pool || amdgpu_vm_map(a, vm, POOL_VA, pool, POOL_BYTES, AMD_VM_VRAM)) {
        if (pool) amdgpu_vram_free(a, pool, POOL_BYTES);
        amdgpu_vm_destroy(a, vm);
        return NULL;
    }
#ifndef MOS_HOST_TEST
    vmm_map_wc(a->fb_phys + (pool - a->bar0), POOL_BYTES);
#endif
    memset(pv, 0, sizeof *pv);
    pv->pid = pid; pv->vm = vm; pv->va_next = VA_FIRST; pv->pool = pool;
    return pv;
}

static u32 ring_done(amdgpu_t *a, u32 ring) { return ring ? amdgpu_compute_done(a) : amdgpu_gfx_done(a); }

bool amdgpu_abi_done(u64 ticket) {
    amdgpu_t *a = amdgpu_main();
    if (!a || !ticket) return true;
    return (i32)(ring_done(a, (u32)(ticket >> 32) & 1) - (u32)ticket) >= 0;
}

int amdgpu_abi_bo_map(int pid, u64 phys, u64 size, bool vram, bool exec, u64 *va) {
    amdgpu_t *a = amdgpu_main();
    proc_vm_t *pv = pv_of(pid, true);
    if (!a || !pv) return -MOS_ENOMEM;
    if (pv->va_next + size > VA_LIMIT) return -MOS_ENOMEM;
    u64 v = pv->va_next;
    int e = amdgpu_vm_map(a, pv->vm, v, phys, size, (vram ? AMD_VM_VRAM : 0) | AMD_VM_WRITE | (exec ? AMD_VM_EXEC : 0));
    if (e) return e;
    pv->va_next = v + size + 65536;           /* um buraco de 64 KiB entre BOs: estouro vira falta, não lixo do vizinho */
    *va = v;
    return 0;
}

void amdgpu_abi_bo_unmap(int pid, u64 va, u64 size) {
    amdgpu_t *a = amdgpu_main();
    proc_vm_t *pv = pv_of(pid, false);
    if (a && pv && va) amdgpu_vm_unmap(a, pv->vm, va, size);
}

/* espaço no pool para n bytes (alinhado a 256), esperando os envios antigos que ainda leem a área */
static i64 pool_take(amdgpu_t *a, proc_vm_t *pv, u32 n) {
    n = (n + 255) & ~255u;
    if (n > POOL_BYTES) return -1;
    if (pv->pool_head + n > POOL_BYTES) pv->pool_head = 0;
    u32 off = pv->pool_head;
    for (u32 pg = off / 4096; pg <= (off + n - 1) / 4096; pg++) {
        u64 t = pv->pool_ticket[pg];
        if (t && !amdgpu_abi_done(t)) {
            bool ok = (t >> 32 & 1) ? amdgpu_compute_wait(a, (u32)t) : amdgpu_gfx_wait(a, (u32)t);
            if (!ok) return -1;
        }
    }
    pv->pool_head = off + n;
    return off;
}

static const amd_abi_bo_t *bo_at(const amd_abi_bo_t *bos, u32 nbo, u64 va, u64 bytes) {
    for (u32 i = 0; i < nbo; i++) if (va >= bos[i].va && va + bytes <= bos[i].va + bos[i].size) return &bos[i];
    return NULL;
}

static const u32 *cpu_of(amdgpu_t *a, const amd_abi_bo_t *b, u64 va) {
    u64 off = va - b->va;
    return b->vram ? (const u32 *)((const u8 *)amdgpu_vram_cpu(a, b->phys) + off) : (const u32 *)(usize)(b->phys + off);
}

int amdgpu_abi_submit(int pid, u32 ring, const amd_abi_ib_t *ibs, u32 nib, const amd_abi_bo_t *bos, u32 nbo, u32 fence, u64 *ticket_out) {
    amdgpu_t *a = amdgpu_main();
    if (!a || !a->gfx_ok || ring > 1 || (ring == 1 && !a->comp_ok) || !nib || nib > 16 || nbo > 256) return -MOS_EINVAL;
    proc_vm_t *pv = pv_of(pid, true);
    if (!pv) return -MOS_ENOMEM;
    if (pv->lost) return -MOS_EIO;
    amd_pm4_bo_t pb[256];
    for (u32 i = 0; i < nbo; i++) { pb[i].va = bos[i].va; pb[i].size = bos[i].size; }
    u64 ticket = 0;
    for (u32 k = 0; k < nib; k++) {
        u32 r_off[1 + AMD_PM4_MAX_IB2], r_len[1 + AMD_PM4_MAX_IB2], nr = 0, total = 0;
        const amd_abi_bo_t *b = bo_at(bos, nbo, ibs[k].va, 4ull * ibs[k].ndw);
        if (!b || !ibs[k].ndw || ibs[k].ndw > AMD_PM4_MAX_DW || (ibs[k].va & 3)) return -MOS_EINVAL;
        /* cópia do IB1 para o pool e validação da cópia */
        if (4ull * ibs[k].ndw > POOL_BYTES / 2) return -MOS_EINVAL;
        i64 off = pool_take(a, pv, 4 * ibs[k].ndw);
        if (off < 0) return -MOS_ENOMEM;
        r_off[nr] = (u32)off; r_len[nr++] = 4 * ibs[k].ndw; total = 4 * ibs[k].ndw;
        u32 *copy = (u32 *)((u8 *)amdgpu_vram_cpu(a, pv->pool) + off);
        memcpy(copy, cpu_of(a, b, ibs[k].va), 4ull * ibs[k].ndw);
        amd_pm4_ctx_t c;
        memset(&c, 0, sizeof c);
        c.bo = pb; c.nbo = nbo;
        int e = amd_pm4_check(&c, copy, ibs[k].ndw, false);
        if (e) { kprintf("amdgpu: IB do pid %d recusado: %s (dword %u)\n", pid, amd_pm4_error(e), c.bad_dw); return -MOS_EINVAL; }
        /* cada IB2: copia, valida e aponta o INDIRECT_BUFFER da cópia para ela */
        u32 n2 = 0;
        for (u32 p = 0; p < ibs[k].ndw && n2 < c.nib2;) {
            u32 h = copy[p];
            if (h >> 30 != 3) { p++; continue; }
            u32 op = (h >> 8) & 0xFF, n = (h >> 16) & 0x3FFF;
            if (op == PM4C_NOP && n == 0x3FFF) { p++; continue; }
            if (op == PM4C_INDIRECT_BUFFER) {
                const amd_abi_bo_t *b2 = bo_at(bos, nbo, c.ib2[n2].va, 4ull * c.ib2[n2].ndw);
                total += 4 * c.ib2[n2].ndw;
                if (!b2 || total > POOL_BYTES / 2) return -MOS_EINVAL;     /* o lote inteiro cabe em meio pool */
                i64 o2 = pool_take(a, pv, 4 * c.ib2[n2].ndw);
                if (o2 < 0) return -MOS_ENOMEM;
                r_off[nr] = (u32)o2; r_len[nr++] = 4 * c.ib2[n2].ndw;
                u32 *c2 = (u32 *)((u8 *)amdgpu_vram_cpu(a, pv->pool) + o2);
                memcpy(c2, cpu_of(a, b2, c.ib2[n2].va), 4ull * c.ib2[n2].ndw);
                amd_pm4_ctx_t c2x;
                memset(&c2x, 0, sizeof c2x);
                c2x.bo = pb; c2x.nbo = nbo;
                e = amd_pm4_check(&c2x, c2, c.ib2[n2].ndw, true);
                if (e) { kprintf("amdgpu: IB2 do pid %d recusado: %s\n", pid, amd_pm4_error(e)); return -MOS_EINVAL; }
                u64 nva = POOL_VA + (u64)o2;
                copy[p + 1] = (u32)nva;
                copy[p + 2] = (u32)(nva >> 32);
                n2++;
            }
            p += n + 2;
        }
        u64 iva = POOL_VA + (u64)off;
        u32 seq = ring ? amdgpu_compute_submit_vm(a, pv->vm, iva, ibs[k].ndw) : amdgpu_gfx_submit_vm(a, pv->vm, iva, ibs[k].ndw);
        if (!seq) return -MOS_EIO;
        ticket = (u64)ring << 32 | seq;
        for (u32 r = 0; r < nr; r++)                              /* o pool só volta depois deste envio */
            for (u32 pg = r_off[r] / 4096; pg <= (r_off[r] + r_len[r] - 1) / 4096; pg++) pv->pool_ticket[pg] = ticket;
    }
    if (ticket_out) *ticket_out = ticket;
    if (fence) {
        pend_t *f = NULL;
        for (int i = 0; i < NPEND; i++) if (!pend[i].pid) { f = &pend[i]; break; }
        if (!f) {                                                /* lista cheia: espera este lote e sinaliza já */
            if (ring ? !amdgpu_compute_wait(a, (u32)ticket) : !amdgpu_gfx_wait(a, (u32)ticket)) return -MOS_EIO;
            gpu_fence_from_irq(pid, fence);
            return 0;
        }
        f->pid = pid; f->fence = fence; f->ticket = ticket;
        amdgpu_abi_poll();
    }
    return 0;
}

/* chamado depois do EOP (irq.c) e por quem espera: sinaliza as fences cujos lotes terminaram */
void amdgpu_abi_poll(void) {
    for (int i = 0; i < NPEND; i++)
        if (pend[i].pid && amdgpu_abi_done(pend[i].ticket)) {
            int pid = pend[i].pid; u32 fence = pend[i].fence;
            pend[i].pid = 0;
            gpu_fence_from_irq(pid, fence);
        }
}

/* falha de página da GPU no VMID: o processo dono perde o contexto */
void amdgpu_abi_vm_fault(u32 vmid) {
    for (int i = 0; i < NPROC; i++)
        if (procs[i].pid && procs[i].vm && procs[i].vm->vmid == vmid && !procs[i].lost) {
            procs[i].lost = true;
            kprintf("amdgpu: falha de página da GPU no VMID %u (pid %d): contexto perdido\n", vmid, procs[i].pid);
        }
}

bool amdgpu_abi_lost(int pid) { proc_vm_t *pv = pv_of(pid, false); return pv && pv->lost; }

/* saída do processo: espera o que ainda está na GPU, destrói a VM (as tabelas e o pool voltam) */
void amdgpu_abi_proc_exit(int pid) {
    amdgpu_t *a = amdgpu_main();
    for (int i = 0; i < NPEND; i++) if (pend[i].pid == pid) pend[i].pid = 0;
    proc_vm_t *pv = pv_of(pid, false);
    if (!a || !pv) return;
    for (u32 pg = 0; pg < POOL_BYTES / 4096; pg++) {
        u64 t = pv->pool_ticket[pg];
        if (t && !amdgpu_abi_done(t)) { if (t >> 32 & 1) amdgpu_compute_wait(a, (u32)t); else amdgpu_gfx_wait(a, (u32)t); }
    }
    amdgpu_vm_destroy(a, pv->vm);
    amdgpu_vram_free(a, pv->pool, POOL_BYTES);
    memset(pv, 0, sizeof *pv);
}

/* resposta do INFO: família, CUs e memória */
void amdgpu_abi_info(u32 *family, u32 *num_cu, u64 *vram, u64 *gtt) {
    amdgpu_t *a = amdgpu_main();
    *family = 9; *num_cu = a ? a->cu_count : 0;
    *vram = a ? a->vram_size : 0;
    *gtt = a ? a->gart_size : 0;
}

/* mOS — memória virtual de GPU por processo na família Raven (GMC 9): um VMID (1..15) por espaço, tabelas de 4
 * níveis (PDB2 -> PDB1 -> PDB0 -> PTB, 512 entradas de 8 bytes cada, 48 bits de VA) na VRAM, PTEs para a VRAM
 * (endereço físico do carve-out) e para a RAM (SYSTEM + SNOOPED), invalidação por VMID nas duas hubs.
 * Ver amdgpu.h. Testado no host contra o CP simulado, que percorre as tabelas (tools/amdgpu_hw_host.c).
 *
 * Formato (gmc_v9_0.c, MIT): PDE = endereço físico da tabela seguinte | VALID; PTE = endereço físico da página |
 * VALID | SYSTEM? | SNOOPED? | EXEC | READ | WRITE, MTYPE NC. A base do contexto também é física | VALID.
 * O VMID fica com o espaço da criação até a destruição: não há troca de VMID entre envios. */
#include "amdgpu.h"
#include "regs.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define WMB()       ((void)0)
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define WMB()       __asm__ volatile("sfence" ::: "memory")
#endif

#define TBL_PER_CHUNK  16u                   /* tabelas de 4 KiB por bloco de 64 KiB da VRAM */
#define CHUNK_BYTES    (64u * 1024u)

static u32 gfxhub_vm(u32 r) { return GFXHUB_VM(r); }

static u64 tbl_pa(const amdgpu_t *a, u64 bar) { return a->fb_phys + (bar - a->bar0); }

/* uma tabela zerada de 4 KiB; devolve o endereço no BAR0 ou 0 */
static u64 tbl_alloc(amdgpu_t *a, amd_vm_t *vm) {
    for (u32 c = 0; c < vm->nchunks; c++)
        for (u32 i = 0; i < TBL_PER_CHUNK; i++)
            if (!(vm->used[c] >> i & 1)) {
                vm->used[c] |= 1u << i;
                u64 t = vm->chunk[c] + (u64)i * 4096;
                memset(amdgpu_vram_cpu(a, t), 0, 4096);
                vm->tables++;
                return t;
            }
    if (vm->nchunks >= AMD_VM_MAX_CHUNKS) return 0;
    u64 c = amdgpu_vram_alloc(a, CHUNK_BYTES);
    if (!c) return 0;
#ifndef MOS_HOST_TEST
    vmm_map_wc(a->fb_phys + (c - a->bar0), CHUNK_BYTES);
#endif
    vm->chunk[vm->nchunks] = c;
    vm->used[vm->nchunks] = 1;
    vm->nchunks++;
    memset(amdgpu_vram_cpu(a, c), 0, 4096);
    vm->tables++;
    return c;
}

/* tabela apontada pela entrada (ou nova, se create); devolve o ponteiro da CPU ou NULL */
static u64 *next_level(amdgpu_t *a, amd_vm_t *vm, u64 *e, bool create) {
    if (*e & AMD_PDE_VALID) {
        u64 pa = *e & AMD_PTE_ADDR_MASK;
        return (u64 *)amdgpu_vram_cpu(a, a->bar0 + (pa - a->fb_phys));
    }
    if (!create) return NULL;
    u64 t = tbl_alloc(a, vm);
    if (!t) return NULL;
    *e = tbl_pa(a, t) | AMD_PDE_VALID;
    return (u64 *)amdgpu_vram_cpu(a, t);
}

static u64 *pte_of(amdgpu_t *a, amd_vm_t *vm, u64 va, bool create) {
    u64 *t = (u64 *)amdgpu_vram_cpu(a, vm->root);
    for (int lvl = 3; lvl >= 1; lvl--) {
        t = next_level(a, vm, &t[(va >> (12 + 9 * lvl)) & 511], create);
        if (!t) return NULL;
    }
    return &t[(va >> 12) & 511];
}

amd_vm_t *amdgpu_vm_create(amdgpu_t *a) {
    if (!a || !a->gmc_ok || !a->gfxhub_ok) return NULL;
    u32 vmid = 0;
    for (u32 v = 1; v < 16; v++) if (!(a->vmid_used >> v & 1)) { vmid = v; break; }
    if (!vmid) { kprintf("amdgpu: os 15 VMIDs estão em uso\n"); return NULL; }
    amd_vm_t *vm = &a->vms[vmid];
    memset(vm, 0, sizeof *vm);
    vm->root = tbl_alloc(a, vm);
    if (!vm->root) { kprintf("amdgpu: sem VRAM para a tabela da VM\n"); return NULL; }
    vm->vmid = vmid;
    a->vmid_used |= 1u << vmid;
    u64 base = tbl_pa(a, vm->root) | AMD_PDE_VALID;
    amdgpu_gfxoff_get(a);                        /* a GFXHUB é do GC */
    for (int hub = 0; hub < 2; hub++) {
        WR(a, hub ? gfxhub_vm(R_VM_CTX_PT_BASE_LO(vmid)) : R_VM_CTX_PT_BASE_LO(vmid), (u32)base);
        WR(a, hub ? gfxhub_vm(R_VM_CTX_PT_BASE_HI(vmid)) : R_VM_CTX_PT_BASE_HI(vmid), (u32)(base >> 32));
    }
    amdgpu_gfxoff_put(a);
    amdgpu_tlb_flush(a, 1u << vmid);
    return vm;
}

void amdgpu_vm_destroy(amdgpu_t *a, amd_vm_t *vm) {
    if (!a || !vm || !vm->vmid) return;
    u32 vmid = vm->vmid;
    amdgpu_gfxoff_get(a);
    for (int hub = 0; hub < 2; hub++) {
        WR(a, hub ? gfxhub_vm(R_VM_CTX_PT_BASE_LO(vmid)) : R_VM_CTX_PT_BASE_LO(vmid), 0);
        WR(a, hub ? gfxhub_vm(R_VM_CTX_PT_BASE_HI(vmid)) : R_VM_CTX_PT_BASE_HI(vmid), 0);
    }
    amdgpu_gfxoff_put(a);
    amdgpu_tlb_flush(a, 1u << vmid);
    for (u32 c = 0; c < vm->nchunks; c++) amdgpu_vram_free(a, vm->chunk[c], CHUNK_BYTES);
    a->vmid_used &= ~(1u << vmid);
    memset(vm, 0, sizeof *vm);
}

/* liga [va, va+size) a memória contígua a partir de phys: com AMD_VM_VRAM, phys é um endereço no BAR0 (VRAM);
 * sem, é o endereço físico da RAM. 0 ou -MOS_*. Em caso de erro desfaz o que mapeou. */
int amdgpu_vm_map(amdgpu_t *a, amd_vm_t *vm, u64 va, u64 phys, u64 size, u32 flags) {
    if (!a || !vm || !vm->vmid || !size || ((va | phys | size) & 0xFFF) || va + size > (AMD_VM_MAX_PFN << 12) || va + size < va)
        return -MOS_EINVAL;
    if ((flags & AMD_VM_VRAM) && !amdgpu_vram_owns(a, phys, size)) return -MOS_EINVAL;
    for (u64 o = 0; o < size; o += 4096) {          /* nada já mapeado na faixa */
        u64 *p = pte_of(a, vm, va + o, false);
        if (p && (*p & AMD_PTE_VALID)) return -MOS_EINVAL;
    }
    u64 f = AMD_PTE_VALID | AMD_PTE_READ;
    if (flags & AMD_VM_WRITE) f |= AMD_PTE_WRITE;
    if (flags & AMD_VM_EXEC) f |= AMD_PTE_EXEC;
    if (!(flags & AMD_VM_VRAM)) f |= AMD_PTE_SYSTEM | AMD_PTE_SNOOPED;
    for (u64 o = 0; o < size; o += 4096) {
        u64 *p = pte_of(a, vm, va + o, true);
        if (!p) { amdgpu_vm_unmap(a, vm, va, o); return -MOS_ENOMEM; }
        u64 pa = (flags & AMD_VM_VRAM) ? tbl_pa(a, phys + o) : phys + o;
        *p = (pa & AMD_PTE_ADDR_MASK) | f;
        vm->pages++;
    }
    WMB();
    amdgpu_tlb_flush(a, 1u << vm->vmid);
    return 0;
}

void amdgpu_vm_unmap(amdgpu_t *a, amd_vm_t *vm, u64 va, u64 size) {
    if (!a || !vm || !vm->vmid || (va & 0xFFF)) return;
    for (u64 o = 0; o < size; o += 4096) {
        u64 *p = pte_of(a, vm, va + o, false);
        if (p && (*p & AMD_PTE_VALID)) { *p = 0; vm->pages--; }
    }
    WMB();
    amdgpu_tlb_flush(a, 1u << vm->vmid);
}

/* tradução feita pela CPU (para conferir e para o kernel ler um endereço do processo) */
u64 amdgpu_vm_lookup(amdgpu_t *a, amd_vm_t *vm, u64 va, u64 *pte_out) {
    if (!a || !vm || !vm->vmid) return 0;
    u64 *p = pte_of(a, vm, va & ~0xFFFull, false);
    if (!p || !(*p & AMD_PTE_VALID)) return 0;
    if (pte_out) *pte_out = *p;
    return (*p & AMD_PTE_ADDR_MASK) + (va & 0xFFF);
}

/* mOS — fila de compute da GPU AMD família Raven (GC 9.1): microcódigo do MEC (código na VRAM pelo IC_BASE,
 * tabela de saltos por registrador), MQD na VRAM, HQD do MEC1/pipe 0/fila 0 programada direto nos
 * registradores (a mesma sequência que o amdgpu usa para a KIQ e o KFD para as filas de usuário), doorbell de
 * 64 bits no BAR2, IBs e fences RELEASE_MEM com EOP no IH (ME 1). Ver amdgpu.h. Testado no host contra um MEC
 * simulado (tools/amdgpu_hw_host.c).
 *
 * A fila de compute não anda pela escrita do WPTR no registrador (o anel GFX anda): o CP só busca quando o
 * doorbell da fila é tocado. Por isso a abertura de doorbells do NBIO e a faixa do MEC são ligadas aqui. */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define DB64(a, i, v) ((void)(a), amd_host_doorbell(i, v))
#define CPU_RELAX() ((void)0)
#define WMB()       ((void)0)
#define UDELAY(us)  ((void)(us))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define DB64(a, i, v) (*(volatile u64 *)((a)->db + (usize)(i) * 4) = (v))
#define CPU_RELAX() __asm__ volatile("pause")
#define WMB()       __asm__ volatile("sfence" ::: "memory")
#define UDELAY(us)  do { for (u32 _i = 0; _i < (u32)(us); _i++) io_wait(); } while (0)
#endif

#define SPIN_MAX     2000000u
#define USEC_TIMEOUT 100000u
#define DB_INDEX     (AMD_DB_MEC_RING0 << 1)   /* em dwords */

/* organização do bloco auxiliar de 64 KiB na VRAM */
#define AUX_MQD   0u
#define AUX_EOP   4096u
#define AUX_WB    8192u
#define AUX_IB    12288u

static void select_queue(amdgpu_t *a, u32 me, u32 pipe, u32 queue) {
    WR(a, R_GRBM_GFX_CNTL, pipe | me << GRBM_GFX_CNTL_MEID_SHIFT | queue << 8);
}

static void compute_enable(amdgpu_t *a, bool on) {
    WR(a, R_CP_MEC_CNTL, on ? 0 : CP_MEC_RESET_ALL);
    UDELAY(50);
}

static bool load_mec(amdgpu_t *a) {
    char name[40];
    snprintf(name, sizeof name, "%s_mec.bin", amd_raven_fw_prefix(a->variant));
    u64 len = 0;
    const u8 *fw = amdgpu_fw_load(name, &len);
    amd_ucode_t u;
    int e = fw ? amd_ucode_parse(fw, (usize)len, UC_KIND_GFX, &u) : -1;
    if (e) { kprintf("amdgpu: firmware %s %s\n", name, fw ? amd_ucode_error(e) : "não encontrado"); return false; }
    if (!u.jt_size) { kprintf("amdgpu: %s sem tabela de saltos\n", name); return false; }
    u64 bytes = ((u64)u.ucode_size_bytes + 0xFFFF) & ~0xFFFFull;
    bool again = a->mec_phys && a->mec_bytes == bytes;
    u64 code = again ? a->mec_phys : amdgpu_vram_alloc(a, bytes);
    if (!code) { kprintf("amdgpu: sem VRAM para o microcódigo do MEC\n"); return false; }
#ifndef MOS_HOST_TEST
    if (!again) vmm_map_wc(a->fb_phys + (code - a->bar0), bytes);
#endif
    memcpy(amdgpu_vram_cpu(a, code), u.ucode, u.ucode_size_bytes);
    WMB();
    a->mec_phys = code; a->mec_bytes = bytes;
    compute_enable(a, false);
    u64 mc = amdgpu_vram_to_mc(a, code);
    WR(a, R_CP_CPC_IC_BASE_CNTL, 0);                 /* VMID 0, cache normal */
    WR(a, R_CP_CPC_IC_BASE_LO, (u32)mc & 0xFFFFF000u);
    WR(a, R_CP_CPC_IC_BASE_HI, (u32)(mc >> 32));
    WR(a, R_CP_MEC_ME1_UCODE_ADDR, u.jt_offset);
    for (u32 i = 0; i < u.jt_size; i++) WR(a, R_CP_MEC_ME1_UCODE_DATA, amd_le32((const u8 *)(u.ucode + u.jt_offset + i)));
    WR(a, R_CP_MEC_ME1_UCODE_ADDR, u.ucode_version);
    a->mec_fw_version = u.ucode_version;
    return true;
}

/* MQD da fila (gfx_v9_0_mqd_init) e os registradores da HQD (gfx_v9_0_kiq_init_register) */
static bool hqd_init(amdgpu_t *a) {
    u32 *m = amdgpu_vram_cpu(a, a->caux_phys + AUX_MQD);
    u64 mqd = a->mqd_mc, eop = amdgpu_vram_to_mc(a, a->caux_phys + AUX_EOP);
    memset(m, 0, MQD_DW * 4);
    memset(amdgpu_vram_cpu(a, a->caux_phys + AUX_EOP), 0, AMD_MEC_HPD_BYTES);
    select_queue(a, 1, 0, 0);

    m[MQD_header] = 0xC0310800u;
    m[MQD_pipelinestat_enable] = 1;
    m[MQD_thread_mgmt_se0] = m[MQD_thread_mgmt_se1] = m[MQD_thread_mgmt_se2] = m[MQD_thread_mgmt_se3] = 0xFFFFFFFFu;
    for (u32 i = 0; i < 4; i++) m[MQD_thread_mgmt_se4 + i] = 0xFFFFFFFFu;
    m[MQD_misc_reserved] = 3;
    m[MQD_dyn_cu_mask_lo] = (u32)(mqd + MQD_DYN_CU_MASK_BYTES);
    m[MQD_dyn_cu_mask_hi] = (u32)((mqd + MQD_DYN_CU_MASK_BYTES) >> 32);
    m[MQD_eop_base_lo] = (u32)(eop >> 8);
    m[MQD_eop_base_hi] = (u32)(eop >> 40);
    u32 order = 0;
    while ((1u << order) < AMD_MEC_HPD_BYTES / 4) order++;
    m[MQD_eop_control] = (RR(a, R_CP_HQD_EOP_CONTROL) & ~CP_HQD_EOP_SIZE_MASK) | (order - 1);
    u32 db = RR(a, R_CP_HQD_PQ_DOORBELL_CONTROL) & ~(CP_HQD_DB_OFFSET_MASK | CP_HQD_DB_SOURCE | CP_HQD_DB_HIT);
    m[MQD_pq_doorbell_control] = db | DB_INDEX << CP_HQD_DB_OFFSET_SHIFT | CP_HQD_DB_EN;
    m[MQD_mqd_base_lo] = (u32)mqd & 0xFFFFFFFCu;
    m[MQD_mqd_base_hi] = (u32)(mqd >> 32);
    m[MQD_mqd_control] = RR(a, R_CP_MQD_CONTROL) & ~0xFu;           /* VMID 0 */
    m[MQD_pq_base_lo] = (u32)(a->cring_mc >> 8);
    m[MQD_pq_base_hi] = (u32)(a->cring_mc >> 40);
    u32 qorder = 0;
    while ((1u << qorder) < AMD_COMP_RING_BYTES / 4) qorder++;
    u32 pq = RR(a, R_CP_HQD_PQ_CONTROL) & ~(CP_HQD_PQ_QUEUE_SIZE_MASK | CP_HQD_PQ_RPTR_BLOCK_MASK | CP_HQD_PQ_UNORD_DISPATCH | CP_HQD_PQ_ROQ_IB_FLIP);
    m[MQD_pq_control] = pq | (qorder - 1) | (10u - 1) << CP_HQD_PQ_RPTR_BLOCK_SHIFT | CP_HQD_PQ_PRIV_STATE | CP_HQD_PQ_KMD_QUEUE;
    m[MQD_rptr_report_lo] = (u32)a->cwb_mc & 0xFFFFFFFCu;
    m[MQD_rptr_report_hi] = (u32)(a->cwb_mc >> 32) & 0xFFFFu;
    m[MQD_wptr_poll_lo] = (u32)(a->cwb_mc + 8) & 0xFFFFFFFCu;
    m[MQD_wptr_poll_hi] = (u32)((a->cwb_mc + 8) >> 32) & 0xFFFFu;
    m[MQD_hqd_vmid] = 0;
    m[MQD_persistent_state] = (RR(a, R_CP_HQD_PERSISTENT_STATE) & ~CP_HQD_PRELOAD_MASK) | 0x53u << 8;
    m[MQD_ib_control] = (RR(a, R_CP_HQD_IB_CONTROL) & ~CP_HQD_MIN_IB_AVAIL_MASK) | 3u << 20;
    m[MQD_quantum] = (RR(a, R_CP_HQD_QUANTUM) & ~CP_HQD_QUANTUM_MASK) | 1u | 1u << 4 | 1u << 8;
    m[MQD_hqd_active] = 1;
    WMB();

    /* registradores: tira a fila do ar se a UEFI ou um boot anterior a deixou ativa */
    WR(a, R_CP_PQ_WPTR_POLL_CNTL, RR(a, R_CP_PQ_WPTR_POLL_CNTL) & ~CP_PQ_WPTR_POLL_EN);
    WR(a, R_CP_HQD_EOP_BASE_ADDR, m[MQD_eop_base_lo]);
    WR(a, R_CP_HQD_EOP_BASE_ADDR_HI, m[MQD_eop_base_hi]);
    WR(a, R_CP_HQD_EOP_CONTROL, m[MQD_eop_control]);
    WR(a, R_CP_HQD_PQ_DOORBELL_CONTROL, m[MQD_pq_doorbell_control]);
    if (RR(a, R_CP_HQD_ACTIVE) & 1) {
        WR(a, R_CP_HQD_DEQUEUE_REQUEST, 1);
        for (u32 j = 0; j < USEC_TIMEOUT && (RR(a, R_CP_HQD_ACTIVE) & 1); j++) UDELAY(1);
        if (RR(a, R_CP_HQD_ACTIVE) & 1) { kprintf("amdgpu: a fila de compute não saiu do ar\n"); select_queue(a, 0, 0, 0); return false; }
        WR(a, R_CP_HQD_DEQUEUE_REQUEST, 0);
        WR(a, R_CP_HQD_PQ_RPTR, 0);
        WR(a, R_CP_HQD_PQ_WPTR_LO, 0);
        WR(a, R_CP_HQD_PQ_WPTR_HI, 0);
    }
    WR(a, R_CP_MQD_BASE_ADDR, m[MQD_mqd_base_lo]);
    WR(a, R_CP_MQD_BASE_ADDR_HI, m[MQD_mqd_base_hi]);
    WR(a, R_CP_MQD_CONTROL, m[MQD_mqd_control]);
    WR(a, R_CP_HQD_PQ_BASE, m[MQD_pq_base_lo]);
    WR(a, R_CP_HQD_PQ_BASE_HI, m[MQD_pq_base_hi]);
    WR(a, R_CP_HQD_PQ_CONTROL, m[MQD_pq_control]);
    WR(a, R_CP_HQD_PQ_RPTR_REPORT_ADDR, m[MQD_rptr_report_lo]);
    WR(a, R_CP_HQD_PQ_RPTR_REPORT_ADDR_HI, m[MQD_rptr_report_hi]);
    WR(a, R_CP_HQD_PQ_WPTR_POLL_ADDR, m[MQD_wptr_poll_lo]);
    WR(a, R_CP_HQD_PQ_WPTR_POLL_ADDR_HI, m[MQD_wptr_poll_hi]);
    WR(a, R_CP_MEC_DOORBELL_RANGE_LOWER, (AMD_DB_KIQ * 2) << 2);
    WR(a, R_CP_MEC_DOORBELL_RANGE_UPPER, (AMD_DB_USERQ_END * 2) << 2);
    WR(a, R_CP_HQD_PQ_DOORBELL_CONTROL, m[MQD_pq_doorbell_control]);
    WR(a, R_CP_HQD_PQ_WPTR_LO, 0);
    WR(a, R_CP_HQD_PQ_WPTR_HI, 0);
    WR(a, R_CP_HQD_VMID, 0);
    WR(a, R_CP_HQD_PERSISTENT_STATE, m[MQD_persistent_state]);
    WR(a, R_CP_HQD_ACTIVE, 1);
    WR(a, R_CP_PQ_STATUS, RR(a, R_CP_PQ_STATUS) | CP_PQ_STATUS_DOORBELL_EN);
    select_queue(a, 0, 0, 0);
    return true;
}

/* ------------------------------------------------------------ anel */
static u32 crptr(amdgpu_t *a) { return a->cwb_cpu[0] & (a->cring_dw - 1); }

static bool croom(amdgpu_t *a, u32 ndw) {
    for (u32 n = 0; n < SPIN_MAX; n++) {
        u32 used = ((u32)a->cwptr - crptr(a)) & (a->cring_dw - 1);
        if (a->cring_dw - used > ndw + 256 + 8) return true;
        if (!a->msi_on && !(n & 1023)) amdgpu_irq_process(a);
        CPU_RELAX();
    }
    a->comp_timeouts++;
    return false;
}

static void cput(amdgpu_t *a, u32 v) { a->cring_cpu[(u32)a->cwptr & (a->cring_dw - 1)] = v; a->cwptr++; }

static void ccommit(amdgpu_t *a) {
    while (a->cwptr & 0xFF) cput(a, PM4_NOP_1DW);
    WMB();
    a->cwb_cpu[2] = (u32)a->cwptr;                   /* cópia do wptr na página de poll */
    a->cwb_cpu[3] = (u32)(a->cwptr >> 32);
    WMB();
    DB64(a, DB_INDEX, a->cwptr);
}

static u32 cfence(amdgpu_t *a) {
    u32 seq = ++a->comp_seq;
    u64 addr = a->cwb_mc + 16;
    cput(a, PM4_TYPE3(PM4_RELEASE_MEM, 6));
    cput(a, PM4_EOP_TCL1 | PM4_EOP_TC | PM4_EOP_TC_MD | PM4_EOP_TC_WB | PM4_EV_CACHE_FLUSH_INV_TS | PM4_EVENT_INDEX(5));
    cput(a, PM4_DATA_SEL_32 | PM4_INT_SEL_WAIT);
    cput(a, (u32)addr);
    cput(a, (u32)(addr >> 32));
    cput(a, seq);
    cput(a, 0);
    cput(a, 0);
    return seq;
}

static bool cwait(amdgpu_t *a, u32 seq) {
    for (u32 n = 0; n < SPIN_MAX * 4; n++) {
        if ((i32)(a->cwb_cpu[4] - seq) >= 0) return true;
        if (!a->msi_on && !(n & 1023)) amdgpu_irq_process(a);
        CPU_RELAX();
    }
    a->comp_timeouts++;
    return false;
}

static u32 csubmit(amdgpu_t *a, u64 ib_mc, u32 ndw, u32 vmid) {
    if (!ndw || ndw > 0xFFFFFu || (ib_mc & 3) || !croom(a, 4 + 8)) return 0;
    cput(a, PM4_TYPE3(PM4_INDIRECT_BUFFER, 2));
    cput(a, (u32)ib_mc);
    cput(a, (u32)(ib_mc >> 32) & 0xFFFFu);
    cput(a, PM4_IB_VALID | ndw | vmid << 24);
    u32 seq = cfence(a);
    ccommit(a);
    a->comp_ibs++;
    return seq;
}

u32 amdgpu_compute_submit(amdgpu_t *a, u64 ib_mc, u32 ndw) { return a && a->comp_ok ? csubmit(a, ib_mc, ndw, 0) : 0; }
u32 amdgpu_compute_submit_vm(amdgpu_t *a, amd_vm_t *vm, u64 ib_va, u32 ndw) {
    return a && a->comp_ok && vm && vm->vmid ? csubmit(a, ib_va, ndw, vm->vmid) : 0;
}
bool amdgpu_compute_wait(amdgpu_t *a, u32 seq) { return a && a->comp_ok && cwait(a, seq); }
u32 amdgpu_compute_done(amdgpu_t *a) { return a && a->comp_ok ? a->cwb_cpu[4] : 0; }

/* ------------------------------------------------------------ entrada */
static void release(amdgpu_t *a, u64 ring, u64 aux) {
    if (ring) amdgpu_vram_free(a, ring, AMD_COMP_RING_BYTES);
    if (aux) amdgpu_vram_free(a, aux, 64 * 1024);
    if (a->mec_phys) { amdgpu_vram_free(a, a->mec_phys, a->mec_bytes); a->mec_phys = 0; }
    a->cring_phys = a->caux_phys = 0;
    a->cring_cpu = a->cwb_cpu = NULL;
}

bool amdgpu_compute_init(amdgpu_t *a) {
    if (!a || !a->gfx_ok) return false;
    a->db_phys = pci_bar_addr(a->pci, 2);
    if (!a->db_phys) { kprintf("amdgpu: sem BAR2 de doorbells\n"); return false; }
#ifndef MOS_HOST_TEST
    vmm_map_mmio(a->db_phys, 64 * 1024);
    a->db = (volatile u8 *)(usize)a->db_phys;
#endif
    WR(a, R_RCC_DOORBELL_APER_EN, RR(a, R_RCC_DOORBELL_APER_EN) | DOORBELL_APER_EN);

    bool again = a->cring_phys != 0;         /* volta do S3 */
    u64 ring = again ? a->cring_phys : amdgpu_vram_alloc(a, AMD_COMP_RING_BYTES), aux = again ? a->caux_phys : amdgpu_vram_alloc(a, 64 * 1024);
    if (!ring || !aux) { kprintf("amdgpu: sem VRAM para a fila de compute\n"); release(a, ring, aux); return false; }
#ifndef MOS_HOST_TEST
    if (!again) {
        vmm_map_wc(a->fb_phys + (ring - a->bar0), AMD_COMP_RING_BYTES);
        vmm_map_wc(a->fb_phys + (aux - a->bar0), 64 * 1024);
    }
#endif
    a->cring_phys = ring; a->cring_cpu = amdgpu_vram_cpu(a, ring); a->cring_mc = amdgpu_vram_to_mc(a, ring);
    a->cring_dw = AMD_COMP_RING_BYTES / 4; a->cwptr = 0;
    a->caux_phys = aux; a->mqd_mc = amdgpu_vram_to_mc(a, aux + AUX_MQD);
    a->cwb_cpu = amdgpu_vram_cpu(a, aux + AUX_WB); a->cwb_mc = amdgpu_vram_to_mc(a, aux + AUX_WB);
    memset((void *)a->cring_cpu, 0, AMD_COMP_RING_BYTES);
    if (!again) memset((void *)a->cwb_cpu, 0, 4096);
    else { a->cwb_cpu[0] = 0; a->cwb_cpu[4] = a->comp_seq; }

    bool ok = load_mec(a);
    if (ok) { compute_enable(a, true); ok = hqd_init(a); }
    if (ok) {
        WR(a, R_CP_ME1_PIPE0_INT_CNTL, RR(a, R_CP_ME1_PIPE0_INT_CNTL) | CP_INT_TIME_STAMP);
        /* teste: um IB que escreve na página de writeback, com a fence */
        u32 *ib = amdgpu_vram_cpu(a, aux + AUX_IB);
        u64 dst = a->cwb_mc + 32;
        a->cwb_cpu[8] = 0xCAFEDEADu;
        ib[0] = PM4_TYPE3(PM4_WRITE_DATA, 3);
        ib[1] = PM4_WD_DST_MEM | PM4_WR_CONFIRM;
        ib[2] = (u32)dst; ib[3] = (u32)(dst >> 32);
        ib[4] = 0xC0FFEE00u;
        WMB();
        u32 seq = csubmit(a, amdgpu_vram_to_mc(a, aux + AUX_IB), 5, 0);
        ok = seq && cwait(a, seq) && a->cwb_cpu[8] == 0xC0FFEE00u;
        if (!ok) kprintf("amdgpu: a fila de compute não executou o IB (writeback 0x%x, fence %u de %u)\n", a->cwb_cpu[8], a->cwb_cpu[4], seq);
    }
    if (!ok) {
        compute_enable(a, false);
        release(a, ring, aux);
        return false;
    }
    a->comp_ok = true;
    kprintf("amdgpu: fila de compute no ar (MEC 0x%x, doorbell %u no BAR2)\n", a->mec_fw_version, DB_INDEX);
    return true;
}

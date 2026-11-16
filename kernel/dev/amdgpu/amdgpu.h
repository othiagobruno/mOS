/* mOS — driver nativo da GPU AMD da família Raven (Raven, Picasso, Raven2; DCN 1.0, GC 9.1).
 *
 * O que este módulo faz no hardware (kernel/dev/amdgpu/amdgpu.c):
 *   - mapeia os registradores (BAR5), lê o rev_id do NBIO e escolhe a variante e o firmware;
 *   - lê onde a VRAM está no espaço da GPU (MMHUB) e o que a UEFI deixou ligado no display: cada OTG
 *     (timing completo) e cada HUBP (superfície, pitch, formato, viewport), e confere que a superfície
 *     ligada é o framebuffer do GOP;
 *   - vsync pelo OTG (posição vertical, bit de vblank e contador de quadros), com a taxa medida no boot;
 *   - troca de quadro (page flip) no vblank para uma superfície linear BGRA/RGBA na VRAM;
 *   - alocador de VRAM para os BOs MOS_GPU_BO_GPU da ABI;
 *   - presença de monitor por HPD.
 * A troca de resolução com PLL, o PHY/DIG, o SMU, a GMC com páginas, os rings e o 3D estão em
 * docs/gpu-amd.md (fases F3..F9). */
#pragma once
#include "amdgpu_base.h"
#include "edid.h"
#include "regs.h"

#ifdef MOS_HOST_TEST
#include "amdgpu_shim.h"      /* tools/: pci_dev_t e as funções do kernel que o teste simula */
#else
#include "kernel.h"
#endif

typedef struct {
    bool on;                  /* OTG_MASTER_EN */
    bool interlace;
    u16  htotal, hactive, hfront, hsync, hback;
    u16  vtotal, vactive, vfront, vsync, vback;
    bool hpos, vpos;          /* polaridade positiva */
} amd_otg_timing_t;

typedef struct {
    bool on;                  /* não está em blank nem desligado */
    u32  format;              /* SURFACE_PIXEL_FORMAT */
    u32  sw_mode;             /* 0 = linear */
    u32  pitch_px;
    u32  vp_x, vp_y, vp_w, vp_h;
    u64  addr;                /* endereço MC (espaço da GPU) */
    bool rgba;                /* crossbar trocado: memória em R,G,B,A */
} amd_plane_t;

#define AMD_GART_PAGES       65536u          /* 256 MiB de janela da GART (tabela de 512 KiB na VRAM) */
#define AMD_SDMA_RING_BYTES  (64u * 1024u)
#define AMD_VRAM_CHUNK  (64u * 1024u)
#define AMD_VRAM_CHUNKS 4096u            /* 256 MiB visíveis pela CPU */

#define AMD_VM_MAX_CHUNKS 64u            /* 64 x 16 tabelas de 4 KiB = até 2 GiB mapeados por processo */
typedef struct {
    u32           vmid;               /* 1..15; 0 = livre */
    u64           root;               /* PDB2, endereço no BAR0 */
    u64           chunk[AMD_VM_MAX_CHUNKS];
    u16           used[AMD_VM_MAX_CHUNKS];
    u32           nchunks, tables;
    u64           pages;
} amd_vm_t;

typedef struct {
    bool          ready;
    pci_dev_t    *pci;
    volatile u8  *mmio;
    u64           mmio_phys, bar0;
    u8            pci_rev;
    u32           rev_id;                /* do NBIO */
    int           variant;               /* AMD_RAVEN, AMD_PICASSO, AMD_RAVEN2 (ucode.h) */
    char          rlc_name[32];
    u64           fb_start, fb_top;      /* faixa da VRAM no espaço MC */
    u64           fb_phys;               /* endereço físico da VRAM (MC_VM_FB_OFFSET) */
    u64           vram_size;
    amd_otg_timing_t otg[AMD_NUM_OTG];
    amd_plane_t   plane[AMD_NUM_OTG];
    int           pipe;                  /* pipe que mostra o framebuffer do GOP; -1 = nenhum */
    bool          uefi_match;            /* a superfície do pipe é exatamente o framebuffer do GOP */
    u64           uefi_mc;               /* endereço MC do framebuffer do GOP */
    u32           uefi_pitch_px;
    bool          uefi_rgba;
    u32           hpd_mask;              /* bit i = HPD i com monitor */
    u32           refresh_mhz;           /* medida no boot (milésimos de Hz); 0 = não mediu */
    bool          vsync_ok;
    u64           last_vblank_ms;
    u64           flips, flip_timeouts;
    /* buffer duplo da tela (amdgpu_present): [0] = framebuffer do GOP, [1] = VRAM alocada */
    bool          scr_on, scr_failed;
    u64           scr_phys[2];
    u32           scr_stride;             /* bytes */
    int           scr_front;
    int           prev_x, prev_y, prev_w, prev_h;   /* área copiada no quadro anterior (coordenadas da saída) */
    u64           presents;
    /* motores (engine.c), ligados depois do VFS por amdgpu_engines_init */
    bool          engines_tried, smu_ok, gmc_ok, sdma_ok;
    u32           smu_version, sdma_fw_version;
    u64           gart_start, gart_size, gart_table, dummy_page;
    u64          *gart_cpu;
    u8            gart_used[AMD_GART_PAGES / 8];
    u64           ring_phys, ring_mc, wb_phys, wb_mc;
    volatile u32 *ring_cpu, *wb_cpu;      /* wb[0] = rptr (bytes), wb[4] = última fence, wb[8] = teste */
    u32           ring_dw, ring_wptr, sdma_seq;
    u64           sdma_timeouts, gmc_timeouts;
    /* interrupções (irq.c) */
    bool          ih_ok, msi_on;
    u64           ih_phys, ih_wb_phys;
    volatile u32 *ih_cpu, *ih_wb;
    u32           ih_dw, ih_rptr;          /* rptr em bytes */
    u64           irqs, ih_entries, ih_overflows, sdma_traps, hpd_events, ih_unknown;
    volatile u32  hpd_changed;             /* bit i = HPD i mudou desde a última leitura (amdgpu_hpd_take) */
    /* motor 3D (gfx.c): GFXHUB, RLC, CP (PFP/CE/ME) e o anel GFX 0 */
    bool          gfxhub_ok, gfx_ok;
    u32           pfp_fw_version, ce_fw_version, me_fw_version, rlc_fw_version;
    u32           gb_addr_config, cu_mask, cu_count;   /* CUs ativos do SE0/SH0 (a família Raven tem 1 SE e 1 SH) */
    u64           csb_phys, csb_mc; u32 csb_dw;        /* preâmbulo de clear state para o RLC */
    u64           gring_phys, gring_mc, gwb_phys, gwb_mc;
    volatile u32 *gring_cpu, *gwb_cpu;   /* gwb[0] = rptr (dwords), gwb[4] = última fence, gwb[8] = teste */
    u32           gring_dw, gring_wptr, gfx_seq;
    u64           gfx_timeouts, gfx_eops, gfx_faults, gfx_ibs, gfx_idle_irqs;
    /* compute (compute.c): MEC1 pipe 0 fila 0, doorbells no BAR2 */
    bool          comp_ok;
    u32           mec_fw_version;
    u64           db_phys;
    volatile u8  *db;
    u64           mec_phys, mec_bytes, caux_phys, cring_phys, cring_mc, cwb_mc, mqd_mc;
    volatile u32 *cring_cpu, *cwb_cpu;   /* cwb[0] = rptr (dwords), cwb[2..3] = wptr, cwb[4] = fence, cwb[8] = teste */
    u32           cring_dw, comp_seq;
    u64           cwptr;                 /* dwords, cresce sem dar a volta (o CP aplica a máscara) */
    u64           comp_timeouts, comp_eops, comp_ibs;
    /* VMs dos processos (vm.c): índice = VMID */
    u32           vmid_used;
    u32           vm_fault_status;
    u64           vm_faults;
    amd_vm_t      vms[16];
    /* VRAM livre: blocos de 64 KiB a partir do começo do BAR0 */
    u32           vram_lo, vram_hi;      /* faixa alocável, em blocos */
    u8            vram_used[AMD_VRAM_CHUNKS / 8];
} amdgpu_t;

/* uefi_fb: endereço físico do framebuffer do GOP; w, h, pitch_px do GOP. false = não é uma Raven ou o
 * hardware não respondeu (fica o framebuffer da UEFI, como antes). */
bool       amdgpu_probe(amdgpu_t *a, pci_dev_t *d, u64 uefi_fb, u32 w, u32 h, u32 pitch_px);
amdgpu_t  *amdgpu_main(void);           /* o da tela, NULL se não houver */
void       amdgpu_set_main(amdgpu_t *a);

/* Timing do OTG <-> modo. Lógica pura (as mesmas fórmulas do programador de timing do DCN 1.0). */
void       amd_otg_decode(const u32 regs[8], amd_otg_timing_t *t);  /* H_TOTAL, H_BLANK, H_SYNC, H_SYNC_CNTL, V_TOTAL, V_BLANK, V_SYNC, V_SYNC_CNTL */
void       amd_otg_encode(const edid_mode_t *m, u32 regs[8]);
void       amd_otg_to_mode(const amd_otg_timing_t *t, edid_mode_t *m);  /* sem pclk */

/* espera o começo do próximo vblank do pipe da tela (limite por leituras: serve com IRQ desligada) */
void       amdgpu_vblank_wait(amdgpu_t *a);
u32        amdgpu_frame_count(amdgpu_t *a);
bool       amdgpu_in_vblank(amdgpu_t *a);

/* Mostra a superfície linear em surf_phys (endereço físico no BAR0) no próximo vblank. w, h = viewport
 * (igual à área ativa do pipe), stride em bytes (múltiplo de 256), format MOS_GPU_FMT_*. surf_phys 0 volta
 * ao framebuffer do GOP. wait: espera a troca acontecer. 0 ou -MOS_*. */
int        amdgpu_flip(amdgpu_t *a, u64 surf_phys, u32 w, u32 h, u32 stride, u32 format, bool wait);

/* Apresentação da tela do compositor com buffer duplo na VRAM e troca no vblank (sem tearing e sem espera
 * ativa). px: superfície w x h (stride = w) em RAM; (ox, oy) = canto da saída 0 nela; (x, y, rw, rh) = área
 * suja. false = não dá (outro tamanho, sem VRAM, GOP fora do plano): quem chama copia para o GOP. */
bool       amdgpu_present(amdgpu_t *a, const u32 *px, int w, int h, int ox, int oy, int x, int y, int rw, int rh);
/* a troca anterior já aconteceu (o buffer de trás pode ser escrito) */
bool       amdgpu_flip_done(amdgpu_t *a);

/* VRAM para BOs: endereço físico (no BAR0) alinhado a 64 KiB, ou 0. */
u64        amdgpu_vram_alloc(amdgpu_t *a, u64 size);
void       amdgpu_vram_free(amdgpu_t *a, u64 phys, u64 size);
bool       amdgpu_vram_owns(const amdgpu_t *a, u64 phys, u64 size);
u64        amdgpu_vram_free_bytes(const amdgpu_t *a);

/* ------------------------------------------------------------ motores (engine.c) */
/* SMU 10, GMC (aberturas e GART do contexto 0) e SDMA 4.1. Precisa do VFS (firmware em
 * /Sistema/Firmware/amdgpu). true = SDMA no ar; o display continua de pé se algo falhar. */
bool       amdgpu_engines_init(amdgpu_t *a);
const u8  *amdgpu_fw_load(const char *name, u64 *len);
int        amdgpu_smu_send(amdgpu_t *a, u32 msg, u32 arg, u32 *out);
/* RAM -> janela da GART (páginas contíguas a partir de phys). Endereço MC ou 0. */
u64        amdgpu_gart_map(amdgpu_t *a, u64 phys, u32 npages, bool writable);
void       amdgpu_gart_unmap(amdgpu_t *a, u64 mc, u32 npages);
u64        amdgpu_vram_to_mc(const amdgpu_t *a, u64 bar_phys);
/* SDMA: devolvem o número da fence (0 = não enviou); amdgpu_sdma_wait espera por ela */
u32        amdgpu_sdma_copy(amdgpu_t *a, u64 dst_mc, u64 src_mc, u64 bytes);
u32        amdgpu_sdma_copy_rect(amdgpu_t *a, u64 dst_mc, u32 dst_pitch, u64 src_mc, u32 src_pitch, u32 row_bytes, u32 rows);
u32        amdgpu_sdma_fill(amdgpu_t *a, u64 dst_mc, u32 value, u64 bytes);
bool       amdgpu_sdma_wait(amdgpu_t *a, u32 seq);
u32        amdgpu_sdma_done(amdgpu_t *a);

void      *amdgpu_vram_cpu(const amdgpu_t *a, u64 bar_phys);
/* invalida as TLBs das duas hubs para os VMIDs da máscara (bit n = VMID n) */
void       amdgpu_tlb_flush(amdgpu_t *a, u32 vmid_mask);
void       amdgpu_vm_fault_ack(amdgpu_t *a, int hub);
/* programa reg = (reg & ~and_mask) | (or_mask & and_mask); and_mask 0xFFFFFFFF escreve or_mask direto */
void       amdgpu_golden(amdgpu_t *a, u32 reg, u32 and_mask, u32 or_mask);

/* ------------------------------------------------------------ motor 3D (gfx.c) */
/* GFX 9.1: golden registers, RLC, CP (PFP, CE, ME) e o anel GFX 0 com o preâmbulo de clear state.
 * Chamado por amdgpu_engines_init depois da GART. true = o anel passou no teste (registrador e IB). */
bool       amdgpu_gfx_init(amdgpu_t *a);
/* Executa um IB (PM4) de ndw dwords no endereço MC ib_mc (VMID 0: VRAM ou GART do kernel). Devolve o número
 * da fence (0 = não enviou). As VMs por processo entram com a F9 (docs/gpu-amd.md). */
u32        amdgpu_gfx_submit(amdgpu_t *a, u64 ib_mc, u32 ndw);
bool       amdgpu_gfx_wait(amdgpu_t *a, u32 seq);
u32        amdgpu_gfx_done(amdgpu_t *a);
/* VM por processo (vm.c). flags AMD_VM_*. phys: endereço no BAR0 com AMD_VM_VRAM, físico da RAM sem. */
#define AMD_VM_VRAM  0x1u
#define AMD_VM_WRITE 0x2u
#define AMD_VM_EXEC  0x4u
amd_vm_t  *amdgpu_vm_create(amdgpu_t *a);
void       amdgpu_vm_destroy(amdgpu_t *a, amd_vm_t *vm);
int        amdgpu_vm_map(amdgpu_t *a, amd_vm_t *vm, u64 va, u64 phys, u64 size, u32 flags);
void       amdgpu_vm_unmap(amdgpu_t *a, amd_vm_t *vm, u64 va, u64 size);
u64        amdgpu_vm_lookup(amdgpu_t *a, amd_vm_t *vm, u64 va, u64 *pte_out);
/* IB no espaço da VM (VA do processo), no anel GFX ou na fila de compute */
u32        amdgpu_gfx_submit_vm(amdgpu_t *a, amd_vm_t *vm, u64 ib_va, u32 ndw);
u32        amdgpu_compute_submit_vm(amdgpu_t *a, amd_vm_t *vm, u64 ib_va, u32 ndw);

/* ABI de GPU revisão 3 (abi.c): VM por processo, VA por BO, lotes de IBs validados, fences pelo EOP */
typedef struct { u64 va; u32 ndw; } amd_abi_ib_t;
typedef struct { u64 va, size, phys; bool vram; } amd_abi_bo_t;   /* phys: BAR0 (VRAM) ou físico da RAM */
bool       amdgpu_abi_available(void);
int        amdgpu_abi_bo_map(int pid, u64 phys, u64 size, bool vram, bool exec, u64 *va);
void       amdgpu_abi_bo_unmap(int pid, u64 va, u64 size);
/* ring 0 = GFX, 1 = compute. fence != 0: gpu_fence_from_irq(pid, fence) quando o lote terminar. ticket_out:
 * o lote, para amdgpu_abi_done (BO ocupado até lá). 0 ou -MOS_* */
int        amdgpu_abi_submit(int pid, u32 ring, const amd_abi_ib_t *ibs, u32 nib, const amd_abi_bo_t *bos, u32 nbo, u32 fence, u64 *ticket_out);
bool       amdgpu_abi_done(u64 ticket);
void       amdgpu_abi_poll(void);
void       amdgpu_abi_vm_fault(u32 vmid);
bool       amdgpu_abi_lost(int pid);
void       amdgpu_abi_proc_exit(int pid);
void       amdgpu_abi_info(u32 *family, u32 *num_cu, u64 *vram, u64 *gtt);

/* Compute: MEC1 com a HQD do pipe 0, fila 0, MQD na VRAM e doorbell no BAR2. Chamado por amdgpu_gfx_init
 * depois do anel GFX. Mesma convenção de IB e fence do anel GFX. */
bool       amdgpu_compute_init(amdgpu_t *a);
u32        amdgpu_compute_submit(amdgpu_t *a, u64 ib_mc, u32 ndw);
bool       amdgpu_compute_wait(amdgpu_t *a, u32 seq);
u32        amdgpu_compute_done(amdgpu_t *a);
/* Monta o preâmbulo de clear state (CSB) em buf (NULL = só conta). Devolve o tamanho em dwords. Lógica pura. */
u32        amdgpu_gfx_csb(u32 *buf);

/* ------------------------------------------------------------ interrupções (irq.c) */
/* Anel do IH na RAM (endereço de barramento, com snoop), MSI no vetor compartilhado, trap do SDMA e hot-plug.
 * Chamado por amdgpu_engines_init antes do SDMA. */
bool       amdgpu_ih_init(amdgpu_t *a);
/* esvazia o anel; devolve quantas entradas tratou (IRQ ou polling) */
u32        amdgpu_irq_process(amdgpu_t *a);
/* HPDs que mudaram desde a última chamada (e zera) */
u32        amdgpu_hpd_take(amdgpu_t *a);

/* MOS_GPU_FMT_* (mos_gpu.h) */
#ifndef MOS_GPU_FMT_BGRA8
#define MOS_GPU_FMT_BGRA8 1u
#define MOS_GPU_FMT_RGBA8 2u
#endif

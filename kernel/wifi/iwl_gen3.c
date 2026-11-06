/*
 * mOS — Intel Wi-Fi família AX210/"So" (AX203, AX210, AX211): carga do firmware pelo "context info
 * gen3" e espera da notificação ALIVE. Reconstruído a partir do desenho do iwlwifi do Linux
 * (iwl-context-info-gen3.h, trans-gen2, rx/tx "gen3"). Ainda NÃO validado em hardware: cada passo
 * grava diagnóstico no log do kernel para a próxima rodada de teste.
 *
 * Memória entregue à placa (DMA):
 *   context info gen3 -> prph_info (estágio de boot), prph_scratch (controle + tabela DRAM das
 *   seções LMAC/UMAC/paginação), anel de comandos (TFDs de 256 bytes), anéis de RX (livres/usados)
 *   e o IML (carregador de imagem) do arquivo .ucode.
 */
#include "iwl.h"

/* ---- registradores */
#define CSR_CTXT_INFO_BOOT_CTRL 0x000
#define CSR_AUTO_FUNC_BOOT_ENA  (1u << 1)
#define CSR_CTXT_INFO_ADDR      0x118
#define CSR_IML_DATA_ADDR       0x120
#define CSR_IML_SIZE_ADDR       0x128
#define CSR_IML_RESP_ADDR       0x12C
#define CSR_GP_CNTRL_AUTO_FUNC_INIT (1u << 7)
#define CSR_GP_CNTRL_MAC_ACCESS_REQ (1u << 3)
#define CSR_DBG_HPET_MEM_REG    0x240
#define CSR_GIO_CHICKEN_L1A_NO_L0S_RX (1u << 23)
#define CSR_GIO_CHICKEN_DIS_L0S_EXIT  (1u << 29)
#define HBUS_TARG_PRPH_WADDR    0x444
#define HBUS_TARG_PRPH_RADDR    0x448
#define HBUS_TARG_PRPH_WDAT     0x44C
#define HBUS_TARG_PRPH_RDAT     0x450
#define HBUS_TARG_WRPTR         0x460
#define FIRST_RX_QUEUE          512
#define UREG_CPU_INIT_RUN       0xA05C44
#define SB_CPU_1_STATUS         0xA01E30
#define SB_CPU_2_STATUS         0xA01E34
#define UREG_UMAC_CURRENT_PC    0xA05C18
#define UREG_LMAC1_CURRENT_PC   0xA05C1C

#define CPU1_CPU2_SEPARATOR 0xFFFFCCCC
#define PAGING_SEPARATOR    0xAAAABBBB
#define MAX_DRAM            64
#define CMDQ_SIZE           32         /* entradas no anel de comandos */
#define RXQ_SIZE            64         /* buffers de recepção (4 KiB) */

#pragma pack(push, 1)
typedef struct { u16 tb_len; u64 addr; } tfh_tb_t;
typedef struct { u16 num_tbs; tfh_tb_t tbs[25]; u32 pad; } tfh_tfd_t;                    /* 256 bytes */
typedef struct { u16 rbid; u16 rsv[3]; u64 addr; } rx_td_t;                              /* 16 bytes */
typedef struct { u32 rsv1; u16 rbid; u8 flags; u8 rsv2[25]; } rx_cd_t;                   /* 32 bytes */
typedef struct {
    u16 mac_id, version, size, rsv;                     /* version */
    u32 control_flags, rsv2;                            /* control */
    u64 pnvm_base; u32 pnvm_size, rsv3;                 /* pnvm_cfg */
    u64 hwm_base; u32 hwm_size, debug_token;            /* hwm_cfg */
    u64 free_rbd_addr; u32 rsv4;                        /* rbd_cfg */
    u64 uefi_base; u32 uefi_size, rsv5;                 /* reduce power / uefi cfg */
    u32 step_cfg[2];
} prph_ctrl_cfg_t;
typedef struct {
    prph_ctrl_cfg_t ctrl;
    u32 fseq_override, step_analog;
    u32 rsv[8];
    u64 umac_img[MAX_DRAM], lmac_img[MAX_DRAM], virtual_img[MAX_DRAM];
} prph_scratch_t;
typedef struct { u32 boot_stage_mirror, ipc_status_mirror, sleep_notif, rsv; } prph_info_t;
typedef struct {
    u16 version, size; u32 config;
    u64 prph_info_base, cr_head_idx_arr, tr_tail_idx_arr, cr_tail_idx_arr, tr_head_idx_arr;
    u16 cr_idx_arr_size, tr_idx_arr_size;
    u64 mtr_base, mcr_base;
    u16 mtr_size, mcr_size, mtr_doorbell_vec, mcr_doorbell_vec, mtr_msi_vec, mcr_msi_vec;
    u8 mtr_opt_header, mtr_opt_footer, mcr_opt_header, mcr_opt_footer;
    u16 msg_rings_ctrl_flags, prph_info_msi_vec;
    u64 prph_scratch_base; u32 prph_scratch_size, rsv;
} ctxt_gen3_t;
typedef struct { u8 cmd, group; u16 seq, len; u8 rsv, version; } cmd_hdr_wide_t;
#pragma pack(pop)

typedef struct { u8 *v; u64 p; } dma_t;
static dma_t dalloc(usize n) { dma_t d; d.v = dma_alloc(n, &d.p); return d; }

static volatile u8 *R;
static inline u32 rd(u32 o) { return mmio_r32(R, o); }
static inline void wr(u32 o, u32 v) { mmio_w32(R, o, v); }
static inline void wr64(u32 o, u64 v) { wr(o, (u32)v); wr(o + 4, (u32)(v >> 32)); }

static bool grab_nic(void) {
    wr(CSR_GP_CNTRL, rd(CSR_GP_CNTRL) | CSR_GP_CNTRL_MAC_ACCESS_REQ);
    u64 end = timer_ticks() + 50;
    while (!(rd(CSR_GP_CNTRL) & CSR_GP_CNTRL_MAC_CLOCK_READY)) if (timer_ticks() > end) return false;
    return true;
}
static void release_nic(void) { wr(CSR_GP_CNTRL, rd(CSR_GP_CNTRL) & ~CSR_GP_CNTRL_MAC_ACCESS_REQ); }
static u32 prph_rd(u32 a) { wr(HBUS_TARG_PRPH_RADDR, (a & 0x00FFFFFF) | (3u << 24)); return rd(HBUS_TARG_PRPH_RDAT); }
static void prph_wr(u32 a, u32 v) { wr(HBUS_TARG_PRPH_WADDR, (a & 0x00FFFFFF) | (3u << 24)); wr(HBUS_TARG_PRPH_WDAT, v); }

static struct {
    dma_t ctxt, info, scratch, iml, cmdq, cmdbuf, rx_free, rx_used, rx_stts, rb[RXQ_SIZE];
    dma_t sec[3][MAX_DRAM];
    int nsec[3];
    u16 rx_read;
    u16 cmd_write;
    bool alive;
} g;

static void dump_state(const char *when) {
    prph_info_t *pi = (prph_info_t *)g.info.v;
    u32 c1 = 0, c2 = 0, upc = 0, lpc = 0;
    if (grab_nic()) { c1 = prph_rd(SB_CPU_1_STATUS); c2 = prph_rd(SB_CPU_2_STATUS); upc = prph_rd(UREG_UMAC_CURRENT_PC); lpc = prph_rd(UREG_LMAC1_CURRENT_PC); release_nic(); }
    kprintf("iwl: [%s] boot_stage 0x%08x ipc 0x%08x | CSR_INT 0x%08x GP_CNTRL 0x%08x IML_RESP 0x%08x | CPU1 0x%x CPU2 0x%x PC umac 0x%x lmac 0x%x | rb_stts %u\n",
            when, pi ? pi->boot_stage_mirror : 0, pi ? pi->ipc_status_mirror : 0, rd(CSR_INT), rd(CSR_GP_CNTRL), rd(CSR_IML_RESP_ADDR),
            c1, c2, upc, lpc, g.rx_stts.v ? *(volatile u16 *)g.rx_stts.v : 0);
}

static int ilog2(u32 v) { int r = 0; while (v >>= 1) r++; return r; }

/* processa pacotes recebidos (notificações e respostas); devolve true se viu o ALIVE */
static void rx_poll(void) {
    volatile u16 *stts = (volatile u16 *)g.rx_stts.v;
    u16 closed = *stts & 0x0FFF;
    while (g.rx_read != closed) {
        rx_cd_t *cd = (rx_cd_t *)(g.rx_used.v + (g.rx_read % RXQ_SIZE) * sizeof(rx_cd_t));
        u16 id = cd->rbid & 0x0FFF;
        if (id < RXQ_SIZE) {
            u8 *pkt = g.rb[id].v;
            u32 lnf = *(u32 *)pkt;
            u8 cmd = pkt[4], grp = pkt[5];
            kprintf("iwl: RX rb %u: grupo 0x%02x cmd 0x%02x, %u bytes\n", id, grp, cmd, lnf & 0x3FFF);
            if (cmd == 0x01 && grp == 0x00) {                          /* UCODE_ALIVE_NTFY */
                u16 status = *(u16 *)(pkt + 8);
                kprintf("iwl: ALIVE status 0x%04x %s\n", status, status == 0xCAFE ? "(ok)" : "(FALHA)");
                g.alive = status == 0xCAFE;
            }
            /* devolve o buffer à placa */
            rx_td_t *td = (rx_td_t *)g.rx_free.v + (g.rx_read % RXQ_SIZE);
            td->rbid = id;
            td->addr = g.rb[id].p;
        }
        g.rx_read = (u16)((g.rx_read + 1) & 0x0FFF);
    }
}

bool iwl_gen3_load(volatile u8 *regs, const iwl_fw_t *fw) {
    R = regs;
    memset(&g, 0, sizeof g);
    /* ---- separa as seções: LMAC | separador | UMAC | separador de paginação | paginação */
    int part = 0;
    for (int i = 0; i < fw->n_rt; i++) {
        const iwl_sec_t *s = &fw->rt[i];
        if (s->offset == CPU1_CPU2_SEPARATOR) { part = 1; continue; }
        if (s->offset == PAGING_SEPARATOR) { part = 2; continue; }
        if (g.nsec[part] >= MAX_DRAM) continue;
        dma_t d = dalloc(s->len);
        if (!d.v) { kprintf("iwl: sem memória para a seção %d\n", i); return false; }
        memcpy(d.v, s->data, s->len);
        g.sec[part][g.nsec[part]++] = d;
    }
    kprintf("iwl: seções na DRAM: %d LMAC, %d UMAC, %d de paginação\n", g.nsec[0], g.nsec[1], g.nsec[2]);
    if (!g.nsec[0] || !g.nsec[1] || !fw->iml_len) { kprintf("iwl: firmware sem LMAC/UMAC/IML\n"); return false; }

    /* ---- RX: anel de livres (descritores de transferência), anel de usados (conclusões), status */
    g.rx_free = dalloc(RXQ_SIZE * sizeof(rx_td_t));
    g.rx_used = dalloc(RXQ_SIZE * sizeof(rx_cd_t));
    g.rx_stts = dalloc(4096);
    for (int i = 0; i < RXQ_SIZE; i++) {
        g.rb[i] = dalloc(4096);
        rx_td_t *td = (rx_td_t *)g.rx_free.v + i;
        td->rbid = (u16)i;
        td->addr = g.rb[i].p;
    }
    /* ---- anel de comandos (TFDs) e buffers dos comandos */
    g.cmdq = dalloc(CMDQ_SIZE * sizeof(tfh_tfd_t));
    g.cmdbuf = dalloc(CMDQ_SIZE * 512);
    /* ---- prph_info, prph_scratch, context info, IML */
    g.info = dalloc(4096);
    g.scratch = dalloc(sizeof(prph_scratch_t));
    g.ctxt = dalloc(sizeof(ctxt_gen3_t));
    g.iml = dalloc(fw->iml_len);
    if (!g.rx_free.v || !g.rx_used.v || !g.cmdq.v || !g.info.v || !g.scratch.v || !g.ctxt.v || !g.iml.v) { kprintf("iwl: sem memória DMA\n"); return false; }
    memcpy(g.iml.v, fw->iml, fw->iml_len);

    prph_scratch_t *ps = (prph_scratch_t *)g.scratch.v;
    ps->ctrl.mac_id = (u16)rd(CSR_HW_REV);
    ps->ctrl.version = 0;
    ps->ctrl.size = sizeof(prph_scratch_t) / 4;
    ps->ctrl.control_flags = (1u << 16) /* RB 4K */ | (1u << 17) /* MTR mode */ | (3u << 18) /* MTR 256B */;
    ps->ctrl.free_rbd_addr = g.rx_free.p;
    for (int i = 0; i < g.nsec[0]; i++) ps->lmac_img[i] = g.sec[0][i].p;
    for (int i = 0; i < g.nsec[1]; i++) ps->umac_img[i] = g.sec[1][i].p;
    for (int i = 0; i < g.nsec[2]; i++) ps->virtual_img[i] = g.sec[2][i].p;

    ctxt_gen3_t *ci = (ctxt_gen3_t *)g.ctxt.v;
    ci->prph_info_base = g.info.p;
    ci->prph_scratch_base = g.scratch.p;
    ci->prph_scratch_size = sizeof(prph_scratch_t);
    ci->cr_head_idx_arr = g.rx_stts.p;
    ci->tr_tail_idx_arr = g.info.p + 2048;
    ci->cr_tail_idx_arr = g.info.p + 3072;
    ci->mtr_base = g.cmdq.p;
    ci->mcr_base = g.rx_used.p;
    ci->mtr_size = (u16)(ilog2(CMDQ_SIZE) - 3);
    ci->mcr_size = (u16)ilog2(RXQ_SIZE);

    /* ---- "APM": acorda o MAC */
    wr(CSR_GIO_CHICKEN_BITS, rd(CSR_GIO_CHICKEN_BITS) | CSR_GIO_CHICKEN_L1A_NO_L0S_RX | CSR_GIO_CHICKEN_DIS_L0S_EXIT);
    wr(CSR_DBG_HPET_MEM_REG, rd(CSR_DBG_HPET_MEM_REG) | 0xFFFF0000);
    wr(CSR_GP_CNTRL, rd(CSR_GP_CNTRL) | CSR_GP_CNTRL_INIT_DONE);
    u64 end = timer_ticks() + 100;
    while (!(rd(CSR_GP_CNTRL) & CSR_GP_CNTRL_MAC_CLOCK_READY)) if (timer_ticks() > end) { kprintf("iwl: relógio do MAC não ficou pronto\n"); return false; }
    wr(CSR_INT_MASK, 0);                                   /* sem interrupções: tudo por polling */
    wr(CSR_INT, 0xFFFFFFFF);
    dump_state("antes da carga");

    /* ---- entrega o context info e o IML e manda iniciar */
    wr64(CSR_CTXT_INFO_ADDR, g.ctxt.p);
    wr64(CSR_IML_DATA_ADDR, g.iml.p);
    wr(CSR_IML_SIZE_ADDR, fw->iml_len);
    wr(CSR_CTXT_INFO_BOOT_CTRL, rd(CSR_CTXT_INFO_BOOT_CTRL) | CSR_AUTO_FUNC_BOOT_ENA);
    wr(CSR_GP_CNTRL, rd(CSR_GP_CNTRL) | CSR_GP_CNTRL_AUTO_FUNC_INIT);
    /* libera os buffers de RX para a placa (ponteiro de escrita múltiplo de 8) */
    wr(HBUS_TARG_WRPTR, (RXQ_SIZE - 8) | (FIRST_RX_QUEUE << 16));

    u64 t0 = timer_ticks(), last_dump = t0;
    while (!g.alive && timer_ticks() - t0 < 3000) {
        rx_poll();
        if (timer_ticks() - last_dump >= 500) { last_dump = timer_ticks(); dump_state("esperando ALIVE"); }
        __asm__ volatile("pause");
    }
    dump_state(g.alive ? "ALIVE recebido" : "sem ALIVE");
    if (!g.alive) {
        kprintf("iwl: a placa não confirmou o firmware em 3 s — reiniciando a placa (mande este log)\n");
        wr(CSR_RESET, CSR_RESET_REG_FLAG_SW_RESET);
        return false;
    }
    kprintf("iwl: firmware rodando em %llu ms (etapa 3 concluída)\n", timer_ticks() - t0);
    return true;
}

/*
 * mOS — driver do RTL8821CE (PCIe 10ec:c821), o Wi-Fi do PC do Thiago.
 *
 * softMAC: a pilha 802.11 do mOS (kernel/wifi/ieee80211.c) monta os quadros, cifra com CCMP e faz o
 * handshake; este driver liga o chip, carrega o firmware 24.11, configura MAC/BB/RF pelas tabelas da
 * Realtek, troca de canal (2,4 e 5 GHz, 20/40/80 MHz), põe a potência por taxa, move os quadros pelos
 * anéis de DMA do PCIe (polling, sem interrupção) e fala com o firmware (H2C/C2H) para a adaptação de
 * taxa, o estado da conexão, a calibração IQK e a coexistência com o Bluetooth do mesmo combo.
 *
 * Ordem dos passos, endereços e valores: driver rtw88 da Realtek (GPL-2.0 OR BSD-3-Clause; o mOS usa a
 * opção BSD-3-Clause, aviso em rtw8821c_table.c). A estrutura é própria: sem mac80211, sem IRQ.
 * O mesmo arquivo compila no teste de host (tools/rtw88_host.c) contra um chip simulado.
 */
#include "rtw.h"
#include "wifi.h"
#ifndef MOS_HOST_TEST
#include "vfs.h"
#endif

/* ============================================================ registradores */
#define R_SYS_FUNC_EN   0x0002
#define R_SYS_PW_CTRL   0x0004
#define R_SYS_CLK_CTRL  0x0008
#define R_RSV_CTRL      0x001C
#define R_RF_CTRL       0x001F
#define R_AFE_XTAL      0x0024
#define R_AFE_PLL       0x0028
#define R_EFUSE_CTRL    0x0030
#define R_LDO_EFUSE     0x0034
#define R_GPIO_MUXCFG   0x0040
#define R_LED_CFG       0x004C
#define R_PAD_CTRL1     0x0064
#define R_SYS_SDIO_CTRL 0x0070
#define R_HCI_OPT_CTRL  0x0074
#define R_MCUFW_CTRL    0x0080
#define R_WIFI_BT_INFO  0x00AA
#define R_WLRF1         0x00EC
#define R_SYS_CFG1      0x00F0
#define R_CR            0x0100
#define R_TXDMA_PQ_MAP  0x010C
#define R_TRXFF_BNDY    0x0114
#define R_RXFF_BNDY     0x011C
#define R_C2HEVT        0x01A0
#define R_HMETFR        0x01CC
#define R_HMEBOX0       0x01D0
#define R_HMEBOX0_EX    0x01F0
#define R_FIFOPAGE_CTRL_2 0x0204
#define R_AUTO_LLT      0x0208
#define R_TXDMA_OFFSET_CHK 0x020C
#define R_TXDMA_STATUS  0x0210
#define R_RQPN_CTRL_2   0x022C
#define R_FIFOPAGE_INFO 0x0230    /* _1 .. _5 a cada 4 */
#define R_H2C_HEAD      0x0244
#define R_H2C_TAIL      0x0248
#define R_H2C_READ_ADDR 0x024C
#define R_H2C_INFO      0x0254
#define R_PCI_CTRL      0x0300
#define R_FWHW_TXQ_CTRL 0x0420
#define R_BCNQ_BDNY     0x0424
#define R_BCNQ1_BDNY    0x0456
#define R_INIRTS_RATE   0x0480
#define R_DATA_SC       0x0483
#define R_CCK_CHECK     0x0454
#define R_AMPDU_MAX_TIME 0x0455
#define R_TX_HANG_CTRL  0x045E
#define R_QUEUE_CTRL    0x04C6
#define R_PROT_MODE     0x04C8
#define R_BAR_MODE      0x04CC
#define R_PRECNT_CTRL   0x04E5
#define R_EDCA_VO       0x0500
#define R_EDCA_VI       0x0504
#define R_EDCA_BE       0x0508
#define R_EDCA_BK       0x050C
#define R_PIFS          0x0512
#define R_SIFS          0x0514
#define R_SLOT          0x051B
#define R_TX_PTCL_CTRL  0x0520
#define R_TXPAUSE       0x0522
#define R_TBTT_PROHIBIT 0x0540
#define R_RD_NAV_NXT    0x0544
#define R_BCN_CTRL      0x0550
#define R_DRVERLYINT    0x0558
#define R_BCNDMATIM     0x0559
#define R_USTIME_TSF    0x055C
#define R_RXTSF_OFF_CCK 0x055E
#define R_TIMER0_SRC    0x05B4
#define R_TCR           0x0604
#define R_RCR           0x0608
#define R_RX_PKT_LIMIT  0x060C
#define R_RX_DRVINFO_SZ 0x060F
#define R_MACID         0x0610
#define R_BSSID         0x0618
#define R_USTIME_EDCA   0x0638
#define R_ACKTO_CCK     0x0639
#define R_WMAC_TRXPTCL  0x0668
#define R_WMAC_TRXPTCL_H 0x066C
#define R_RXFLTMAP0     0x06A0
#define R_RXFLTMAP1     0x06A2
#define R_RXFLTMAP2     0x06A4
#define R_AID           0x06A8
#define R_BT_COEX_TABLE0 0x06C0
#define R_BT_COEX_TABLE1 0x06C4
#define R_BT_COEX_BRK   0x06C8
#define R_BT_COEX_TABLE_H 0x06CC
#define R_SND_PTCL_CTRL 0x0718
#define R_BT_COEX_V2    0x0762
#define R_BT_STAT_CTRL  0x0778
#define R_BT_TDMA_TIME  0x0790
#define R_WMAC_OPTION   0x07D0
#define R_CPU_DMEM_CON  0x1080
#define R_H2C_PKT_READADDR 0x10D0
#define R_H2C_PKT_WRITEADDR 0x10D4
#define R_FW_DBG7       0x10FC
#define R_CR_EXT        0x1100
#define R_DDMA_CH0SA    0x1200
#define R_DDMA_CH0DA    0x1204
#define R_DDMA_CH0CTRL  0x1208
#define R_H2CQ_CSR      0x1330
#define R_FAST_EDCA_VOVI 0x1448
#define R_FAST_EDCA_BEBK 0x144C
#define R_LTECOEX_CTRL  0x1700
#define R_LTECOEX_WDATA 0x1704
#define R_LTECOEX_RDATA 0x1708
/* BB */
#define R_RXPSEL        0x0808
#define R_RXCCAMSK      0x0814
#define R_CLKTRK        0x0860
#define R_ADCCLK        0x08AC
#define R_ADC160        0x08C4
#define R_ADC40         0x08C8
#define R_CHFIR         0x08F0
#define R_ACBB0         0x0948
#define R_ACBBRXFIR     0x094C
#define R_RXSB          0x0A00
#define R_CCA_FLTR      0x0A20
#define R_TXSF2         0x0A24
#define R_TXSF6         0x0A28
#define R_CCK0_FAREPORT 0x0A2C
#define R_ENTXCCK       0x0A80
#define R_ENRXCCA       0x0A84
#define R_CSRATIO       0x0AAA
#define R_TXFILTER      0x0AAC
#define R_IGI_A         0x0C50
#define R_TXSCALE_A     0x0C1C
#define R_TXDFIR        0x0C20
#define R_SIPI_A        0x0C90
#define R_RFE_CTRL8     0x0CB4
#define R_RFECTL        0x0CB8
#define R_TXAGC_A       0x1D00
#define R_IQKFAILMSK    0x1BF0
#define R_RF_BASE_A     0x2800
/* PCIe: buffer descriptors */
#define R_TXBD_DESA_BCNQ  0x308
#define R_TXBD_DESA_MGMTQ 0x310
#define R_TXBD_DESA_VOQ   0x318
#define R_TXBD_DESA_VIQ   0x320
#define R_TXBD_DESA_BEQ   0x328
#define R_TXBD_DESA_BKQ   0x330
#define R_RXBD_DESA_MPDUQ 0x338
#define R_TXBD_DESA_HI0Q  0x340
#define R_TXBD_NUM_MGMTQ  0x380
#define R_RXBD_NUM_MPDUQ  0x382
#define R_TXBD_BCN_WORK   0x383
#define R_TXBD_NUM_VOQ    0x384
#define R_TXBD_NUM_VIQ    0x386
#define R_TXBD_NUM_BEQ    0x388
#define R_TXBD_NUM_BKQ    0x38A
#define R_TXBD_NUM_HI0Q   0x38C
#define R_TXBD_RWPTR_CLR  0x39C
#define R_TXBD_IDX_VOQ    0x3A0
#define R_TXBD_IDX_VIQ    0x3A4
#define R_TXBD_IDX_BEQ    0x3A8
#define R_TXBD_IDX_BKQ    0x3AC
#define R_TXBD_IDX_MGMTQ  0x3B0
#define R_RXBD_IDX_MPDUQ  0x3B4
#define R_TXBD_IDX_HI0Q   0x3B8
#define R_TXBD_DESA_H2CQ  0x1320
#define R_TXBD_NUM_H2CQ   0x1328
#define R_TXBD_IDX_H2CQ   0x132C
#define R_HIMR0           0x0B0
#define R_HIMR1           0x0B8
#define R_HIMR3           0x10B8
#define R_RPWM            0x03D9
#define R_CPWM            0x03DA

#define B(n) (1u << (n))
/* RCR */
#define RCR_APP_FCS   B(31)
#define RCR_APP_MIC   B(30)
#define RCR_APP_ICV   B(29)
#define RCR_APP_PHYSTS B(28)
#define RCR_VHT_DACK  B(26)
#define RCR_PKTCTL_DLEN B(20)
#define RCR_HTC_LOC   B(14)
#define RCR_CBSSID_BCN B(7)
#define RCR_CBSSID_DATA B(6)
#define RCR_AB        B(3)
#define RCR_AM        B(2)
#define RCR_APM       B(1)

/* firmware (MCUFW_CTRL) */
#define FW_DL_EN      B(0)
#define FW_IMEM_DW_OK B(3)
#define FW_IMEM_CK_OK B(4)
#define FW_DMEM_DW_OK B(5)
#define FW_DMEM_CK_OK B(6)
#define FW_CHKSUM_OK  (B(4) | B(6))
#define FW_DW_RDY     B(14)
#define FW_INIT_RDY   B(15)
#define FW_READY      (FW_INIT_RDY | FW_DW_RDY | FW_IMEM_DW_OK | FW_DMEM_DW_OK | FW_CHKSUM_OK)
#define FW_READY_MASK (0xFFFF & ~(B(12) | B(13)))
#define DDMA_OWN      B(31)
#define DDMA_CHKSUM_EN B(29)
#define DDMA_CHKSUM_STS B(27)
#define DDMA_RST_CHKSUM B(25)
#define DDMA_CHKSUM_CONT B(24)
#define OCP_TXBUF     0x18780000u
#define OCP_DMEM      0x00200000u

/* fila de transmissão do hardware */
enum { Q_BK, Q_BE, Q_VI, Q_VO, Q_MGMT, Q_HI0, Q_H2C, Q_BCN, Q_N };
#define QSEL_BEACON 16
#define QSEL_HIGH   17
#define QSEL_MGMT   18
#define QSEL_H2C    19

#define TXDESC  48
#define RXDESC  24
#define TXBUF   (TXDESC + 2400)
#define BCNBUF  (TXDESC + 0x1000)                   /* fila de beacon: pedaços de 4 KiB do firmware */
#define RXBUF   (11454 + 24)
#define RX_N    64
#define PAGES   512                          /* 64 KiB de FIFO de TX em páginas de 128 B */
#define RSVD_DRV 8
#define RSVD_H2C_EXTRA 24
#define RSVD_H2C_STATIC 8
#define RSVD_H2CQ 8
#define RSVD_FW_TXBUF 4

/* ============================================================ estado */
typedef struct { u8 *desc; u64 desc_pa; u8 **buf; u64 *buf_pa; u32 len, wp, rp; } ring_t;

typedef struct {
    rtw_io_t io;
    volatile u8 *mmio;
    pci_dev_t *pci;
    wifi_radio_t radio;
    rtw_fw_t fw;
    rtw_efuse_t ef;
    rtw_cond_t cond;
    rtw_pwr_t pwr;
    ring_t tx[Q_N], rx;
    u8 cut;
    u16 rsvd_boundary, rsvd_fw_txbuf, rsvd_h2cq;
    u32 ch_param[3];
    int band, primary, center, bw;      /* band 0 = 2,4 GHz, 1 = 5 GHz */
    u8 h2c_box;
    u16 h2c_seq;
    u32 rcr;
    u16 rx_tag;
    bool up, assoc, ps_on, ps_broken;
    wifi_bss_t bss;
    u8 rate_id, ra_bw;
    u32 last_watchdog;
    u8 tx_rate;                          /* última taxa que o firmware reportou */
    int rssi;
    u8 igi;
    u8 scbd;
} rtw_t;

static rtw_t D;

/* ---- acesso */
static inline u8 r8(u32 a) { return D.io.r8(&D.io, a); }
static inline u16 r16(u32 a) { return D.io.r16(&D.io, a); }
static inline u32 r32(u32 a) { return D.io.r32(&D.io, a); }
static inline void w8(u32 a, u8 v) { D.io.w8(&D.io, a, v); }
static inline void w16(u32 a, u16 v) { D.io.w16(&D.io, a, v); }
static inline void w32(u32 a, u32 v) { D.io.w32(&D.io, a, v); }
static inline void udelay(u32 us) { D.io.udelay(&D.io, us); }
static void w8m(u32 a, u8 m, u8 v) { u8 s = (u8)rtw_ffs(m); w8(a, (u8)((r8(a) & ~m) | ((v << s) & m))); }
static void w32m(u32 a, u32 m, u32 v) { rtw_w32_mask(&D.io, a, m, v); }
static u32 r32m(u32 a, u32 m) { return rtw_r32_mask(&D.io, a, m); }
static void set8(u32 a, u8 b) { rtw_w8_set(&D.io, a, b); }
static void clr8(u32 a, u8 b) { rtw_w8_clr(&D.io, a, b); }
static void set32(u32 a, u32 b) { rtw_w32_set(&D.io, a, b); }
static void clr32(u32 a, u32 b) { rtw_w32_clr(&D.io, a, b); }

static bool ready32(u32 a, u32 m, u32 want) {              /* como check_hw_ready: até 10 ms */
    for (int i = 0; i < 1000; i++) { if (r32m(a, m) == want) return true; udelay(10); }
    return false;
}

/* ---- memória para DMA: o chip só endereça 32 bits */
#ifdef MOS_HOST_TEST
void *rtw_host_dma(usize n, u64 *pa);
#define DMA_ALLOC(n, pa) rtw_host_dma(n, pa)
#else
#define DMA_ALLOC(n, pa) dma_alloc32(n, pa)
#endif

/* ---- RF: leitura direta (0x2800 + 4·reg), escrita pela interface serial SIPI (0xC90) */
static u32 rf_read(u32 reg, u32 mask) { return r32m(R_RF_BASE_A + ((reg & 0xFF) << 2), mask & 0xFFFFF); }
static void rf_write(u32 reg, u32 mask, u32 data) {
    reg &= 0xFF; mask &= 0xFFFFF;
    if (mask != 0xFFFFF) {
        u32 old = rf_read(reg, 0xFFFFF);
        data = (old & ~mask) | ((data << rtw_ffs(mask)) & mask);
    }
    w32(R_SIPI_A, ((reg << 20) | (data & 0xFFFFF)) & 0x0FFFFFFF);
    udelay(13);
}

/* ---- acesso indireto ao bloco de coexistência (0x1700) */
static bool lte_read(u16 off, u32 *v) {
    if (!ready32(R_LTECOEX_CTRL, B(29), 1)) return false;
    w32(R_LTECOEX_CTRL, 0x800F0000u | off);
    *v = r32(R_LTECOEX_RDATA);
    return true;
}
static bool lte_write(u16 off, u32 v) {
    if (!ready32(R_LTECOEX_CTRL, B(29), 1)) return false;
    w32(R_LTECOEX_WDATA, v);
    w32(R_LTECOEX_CTRL, 0xC00F0000u | off);
    return true;
}
static void lte_write_mask(u16 off, u32 mask, u32 val) {
    u32 t = 0;
    if (!lte_read(off, &t)) return;
    t = (t & ~mask) | ((val << rtw_ffs(mask)) & mask);
    lte_write(off, t);
}

/* ============================================================ anéis de DMA do PCIe */
static const u32 q_desa[Q_N] = {R_TXBD_DESA_BKQ, R_TXBD_DESA_BEQ, R_TXBD_DESA_VIQ, R_TXBD_DESA_VOQ,
                                R_TXBD_DESA_MGMTQ, R_TXBD_DESA_HI0Q, R_TXBD_DESA_H2CQ, R_TXBD_DESA_BCNQ};
static const u32 q_num[Q_N] = {R_TXBD_NUM_BKQ, R_TXBD_NUM_BEQ, R_TXBD_NUM_VIQ, R_TXBD_NUM_VOQ,
                               R_TXBD_NUM_MGMTQ, R_TXBD_NUM_HI0Q, R_TXBD_NUM_H2CQ, 0};
static const u32 q_idx[Q_N] = {R_TXBD_IDX_BKQ, R_TXBD_IDX_BEQ, R_TXBD_IDX_VIQ, R_TXBD_IDX_VOQ,
                               R_TXBD_IDX_MGMTQ, R_TXBD_IDX_HI0Q, R_TXBD_IDX_H2CQ, 0};
static const u32 q_len[Q_N] = {4, 64, 4, 4, 32, 16, 16, 1};

static bool ring_alloc(ring_t *r, u32 n, u32 desc_sz, u32 buf_sz) {
    r->len = n;
    r->desc = DMA_ALLOC(n * desc_sz, &r->desc_pa);
    r->buf = kmalloc(n * sizeof(u8 *));
    r->buf_pa = kmalloc(n * sizeof(u64));
    if (!r->desc || !r->buf || !r->buf_pa) return false;
    for (u32 i = 0; i < n; i++) {
        r->buf[i] = DMA_ALLOC(buf_sz, &r->buf_pa[i]);
        if (!r->buf[i]) return false;
#ifndef MOS_HOST_TEST
        if (r->buf_pa[i] + buf_sz > 0x100000000ull) return false;
#endif
    }
    return true;
}

static void rx_desc_reset(u32 i) {
    u8 *d = D.rx.desc + 8 * i;
    memset(d, 0, 8);
    d[0] = (u8)RXBUF; d[1] = (u8)(RXBUF >> 8);
    u32 pa = (u32)D.rx.buf_pa[i];
    d[4] = (u8)pa; d[5] = (u8)(pa >> 8); d[6] = (u8)(pa >> 16); d[7] = (u8)(pa >> 24);
}

/* rtw_pci_setup: endereços dos anéis, índices zerados, DMA e etiqueta de RX reiniciados */
static void hci_setup(void) {
    w8(R_PCI_CTRL + 3, (u8)(r8(R_PCI_CTRL + 3) | 0xF7));
    w32(R_TXBD_DESA_BCNQ, (u32)D.tx[Q_BCN].desc_pa);
    for (int q = 0; q < Q_BCN; q++) {
        D.tx[q].wp = D.tx[q].rp = 0;
        w16(q_num[q], (u16)(D.tx[q].len & 0xFFF));
        w32(q_desa[q], (u32)D.tx[q].desc_pa);
    }
    D.rx.wp = D.rx.rp = 0;
    for (u32 i = 0; i < D.rx.len; i++) rx_desc_reset(i);
    w16(R_RXBD_NUM_MPDUQ, (u16)(D.rx.len & 0xFFF));
    w32(R_RXBD_DESA_MPDUQ, (u32)D.rx.desc_pa);
    w32(R_TXBD_RWPTR_CLR, 0xFFFFFFFF);
    set32(R_H2CQ_CSR, B(16) | B(8));
    set32(R_PCI_CTRL, B(20) | B(15));                 /* reinicia o DMA e liga a etiqueta de RX */
    D.rx_tag = 0;
}

typedef struct {
    u16 size; u8 offset, qsel, rate, rate_id, macid, bw, seq_sel;
    bool bmc, use_rate, dis_fb, dis_qselseq, en_hwseq, sgi, ldpc, stbc, ampdu, more;
    u8 ampdu_factor, ampdu_density;
    u16 seq;
} txinfo_t;

static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static u32 get32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

/* descritor de TX de 48 bytes (rtw_tx_fill_tx_desc + checksum das 8 primeiras palavras de 16 bits... 16 no 8821C) */
static void fill_txdesc(u8 *d, const txinfo_t *t) {
    memset(d, 0, TXDESC);
    put32(d + 0, t->size | (u32)t->offset << 16 | (u32)t->bmc << 24 | 1u << 26 | (u32)t->dis_qselseq << 31);
    put32(d + 4, t->macid | (u32)(t->qsel & 0x1F) << 8 | (u32)(t->rate_id & 0x1F) << 16 | (u32)t->more << 29);
    put32(d + 8, (u32)t->ampdu << 12 | (u32)(t->ampdu_density & 7) << 20);
    put32(d + 12, (u32)(t->seq_sel & 3) << 6 | (u32)t->use_rate << 8 | (u32)t->dis_fb << 10 | (u32)(t->ampdu_factor & 0x1F) << 17);
    put32(d + 16, t->rate & 0x7F);
    put32(d + 20, (u32)t->sgi << 4 | (u32)(t->bw & 3) << 5 | (u32)t->ldpc << 7 | (u32)t->stbc << 8);
    put32(d + 32, (u32)t->en_hwseq << 15);
    put32(d + 36, (u32)(t->seq & 0xFFF) << 12);
    u16 ck = 0;
    for (int i = 0; i < 16; i++) ck ^= (u16)(d[2 * i] | d[2 * i + 1] << 8);
    d[28] = (u8)ck; d[29] = (u8)(ck >> 8);
}

static int avail(const ring_t *r) { return r->rp > r->wp ? (int)(r->rp - r->wp - 1) : (int)(r->len - r->wp + r->rp - 1); }

/* lê do hardware até onde a fila andou e libera as entradas */
static void tx_reclaim(int q) {
    if (q == Q_BCN) return;
    u32 hw = (r32(q_idx[q]) >> 16) & 0xFFF;
    D.tx[q].rp = hw % D.tx[q].len;
}

/* põe um pacote na fila q: [descritor de 48 B][dados], dois segmentos no buffer descriptor */
static bool tx_queue(int q, const txinfo_t *t, const u8 *data, u16 len) {
    ring_t *r = &D.tx[q];
    if (len + TXDESC > (q == Q_BCN ? BCNBUF : TXBUF)) { kprintf("rtw88: quadro de %u bytes grande demais para a fila %d\n", len, q); return false; }
    u32 slot;
    if (q == Q_BCN) slot = 0;
    else {
        tx_reclaim(q);
        if (!avail(r)) return false;
        slot = r->wp;
    }
    u8 *b = r->buf[slot];
    txinfo_t ti = *t;
    ti.size = len;
    ti.offset = TXDESC;
    fill_txdesc(b, &ti);
    memcpy(b + TXDESC, data, len);
    u8 *bd = r->desc + 16 * slot;
    memset(bd, 0, 16);
    u32 total = (u32)len + TXDESC;
    u16 psb = (u16)((total - 1) / 128 + 1);
    if (q == Q_BCN) psb |= 1u << 15;
    u32 pa = (u32)r->buf_pa[slot];
    bd[0] = TXDESC; bd[1] = 0; bd[2] = (u8)psb; bd[3] = (u8)(psb >> 8); put32(bd + 4, pa);
    bd[8] = (u8)len; bd[9] = (u8)(len >> 8); put32(bd + 12, pa + TXDESC);
    __sync_synchronize();
    if (q == Q_BCN) return true;
    r->wp = (r->wp + 1) % r->len;
    w16(q_idx[q], (u16)(r->wp & 0xFFF));
    return true;
}

/* ============================================================ H2C */
static bool h2c_cmd(const u8 h[8]) {
    u8 box = D.h2c_box;
    bool free = false;
    for (int i = 0; i < 30; i++) { if (!((r8(R_HMETFR) >> box) & 1)) { free = true; break; } udelay(100); }
    if (!free) { kprintf("rtw88: caixa H2C %u ocupada, comando 0x%02x perdido\n", box, h[0]); return false; }
    w32(R_HMEBOX0_EX + 4 * box, get32(h + 4));
    w32(R_HMEBOX0 + 4 * box, get32(h));
    D.h2c_box = (u8)((box + 1) & 3);
    return true;
}

#define H2C_PKT 32
static void h2c_pkt_hdr(u8 *p, u8 sub, u16 total) {
    memset(p, 0, H2C_PKT);
    put32(p, 0x01 | 0xFFu << 8 | (u32)sub << 16);
    put32(p + 4, total | (u32)D.h2c_seq++ << 16);
}
static bool h2c_pkt(const u8 *p) {
    txinfo_t t = {0};
    t.qsel = QSEL_H2C;
    return tx_queue(Q_H2C, &t, p, H2C_PKT);
}

static void fw_general_info(void) {
    u8 p[H2C_PKT];
    h2c_pkt_hdr(p, 0x0D, 8 + 4);
    p[10] = (u8)(D.rsvd_fw_txbuf - D.rsvd_boundary);
    h2c_pkt(p);
}
static void fw_phydm_info(void) {
    u8 p[H2C_PKT];
    h2c_pkt_hdr(p, 0x11, 8 + 8);
    p[8] = D.ef.rfe;
    p[9] = 1;                                   /* FW_RF_1T1R */
    p[10] = D.cut;
    p[11] = 1 | 1 << 4;                         /* antenas RX e TX: caminho A */
    h2c_pkt(p);
}
static void fw_iqk(bool segment) {
    u8 p[H2C_PKT];
    h2c_pkt_hdr(p, 0x0E, 8 + 1);
    p[8] = (u8)(segment ? 2 : 0);
    h2c_pkt(p);
}
static void fw_media_status(bool connect) {
    u8 h[8] = {0x01, (u8)(connect ? 1 : 0), 0 /* macid */, 0, 0, 0, 0, 0};
    h2c_cmd(h);
}
static void fw_ra_info(u32 mask, bool reset) {
    u8 h[8] = {0};
    u32 w = 0x40 | 0u << 8 | (u32)(D.rate_id & 0x1F) << 16 | (u32)(D.bss.sgi80 || D.bss.sgi40 || D.bss.sgi20) << 23 |
            (u32)(D.ra_bw & 3) << 24 | (u32)D.bss.ldpc << 26 | (u32)(!reset) << 27 | (u32)(D.bss.vht ? 1 : 0) << 28 | 1u << 30;
    put32(h, w);
    put32(h + 4, mask);
    h2c_cmd(h);
}
static void fw_rssi(int rssi_pct) {
    u8 h[8] = {0x42, 0, 0, (u8)rssi_pct, 0, 0, 0, 0};
    h2c_cmd(h);
}
/* canal do Wi-Fi para o BT pular no AFH (2,4 GHz) ou só saber onde estamos (5 GHz) */
static void fw_wl_ch_info(bool connected) {
    u8 link = 0, ch = 0, bw = 0;
    if (connected && !(D.ef.share_ant && D.center <= 14)) {
        ch = (u8)D.center;
        if (D.center <= 14) { link = 1; bw = D.bw == 1 ? 0x36 : 0x24; }
        else bw = (u8)D.bw;
    }
    u8 h[8] = {0x66, link, ch, bw, 0, 0, 0, 0};
    h2c_cmd(h);
}

/* ============================================================ energia e firmware */
static int mac_pre_system_cfg(void) {
    w8(R_RSV_CTRL, 0);
    set32(R_HCI_OPT_CTRL, B(8));                               /* USB_SUS_DIS no PCIe */
    set32(R_PAD_CTRL1, B(29) | B(28));                         /* PAPE e LNAON: WL/BT */
    clr32(R_LED_CFG, B(25) | B(26));
    set32(R_GPIO_MUXCFG, B(2));                                /* WLRFE 4/5 */
    clr8(R_SYS_FUNC_EN, (u8)(B(0) | B(1)));
    clr8(R_RF_CTRL, (u8)(B(0) | B(1) | B(2)));
    clr32(R_WLRF1, B(24) | B(25) | B(26));
    return 0;
}

static int mac_power_switch(bool on) {
    if (r16(R_MCUFW_CTRL) == 0xC078) {                       /* firmware ainda roda: acorda-o antes */
        u8 rp = r8(R_RPWM);
        w8(R_RPWM, (u8)((rp ^ 0x80) & 0x80));
    }
    bool cur = r8(R_CR) != 0xEA;
    if (on == cur) return 1;                                 /* já está no estado pedido */
    return rtw_pwr_seq(&D.io, on ? rtw8821c_pwr_on : rtw8821c_pwr_off, D.cut, 50) ? -1 : 0;
}

static int mac_power_on(void) {
    mac_pre_system_cfg();
    int r = mac_power_switch(true);
    if (r == 1) {                                            /* ligado de antes (UEFI/outro SO): desliga e liga */
        mac_power_switch(false);
        mac_pre_system_cfg();
        r = mac_power_switch(true);
    }
    if (r) return -1;
    set32(R_CPU_DMEM_CON, B(16) | B(8));                       /* WL_PLATFORM_RST, DDMA_EN */
    set8(R_SYS_FUNC_EN + 1, 0xD8);
    w8(R_CR_EXT + 3, (u8)((r8(R_CR_EXT + 3) & 0xF0) | 0x0C));
    u32 t = r32(R_MCUFW_CTRL);
    if (t & B(20)) { w32(R_MCUFW_CTRL, t & ~B(20)); clr32(R_GPIO_MUXCFG, B(19)); }   /* sem boot por flash */
    return 0;
}

static void wlan_cpu(bool on) {
    if (on) { set8(R_RSV_CTRL + 1, 1); set8(R_SYS_FUNC_EN + 1, 4); }
    else { clr8(R_SYS_FUNC_EN + 1, 4); clr8(R_RSV_CTRL + 1, 1); }
}

/* página reservada pela fila de beacon: o pacote vai para o buffer de TX do chip na página 0 */
static bool write_rsvd_page(u16 pg, const u8 *data, u32 size) {
    u8 bcn = r8(R_BCN_CTRL);
    w16(R_FIFOPAGE_CTRL_2, (u16)((pg & 0xFFF) | B(15)));
    u8 cr1 = r8(R_CR + 1);
    w8(R_CR + 1, (u8)(cr1 | 1));                             /* ENSWBCN */
    w8(R_BCN_CTRL, (u8)((bcn & ~B(3)) | B(4)));
    u8 txq = r8(R_FWHW_TXQ_CTRL + 2);
    w8(R_FWHW_TXQ_CTRL + 2, (u8)(txq & ~(B(22) >> 16)));
    txinfo_t t = {0};
    t.qsel = QSEL_BEACON;
    t.bmc = size > 4 && (data[4] & 1);
    t.dis_qselseq = t.en_hwseq = true;
    t.use_rate = t.dis_fb = true;
    t.rate = D.band ? 0x04 : 0x00;
    t.rate_id = D.band ? 7 : 8;
    bool ok = tx_queue(Q_BCN, &t, data, (u16)size);
    if (ok) {
        w8(R_TXBD_BCN_WORK, (u8)(r8(R_TXBD_BCN_WORK) | B(4)));
        ok = ready32(R_FIFOPAGE_CTRL_2, B(15), 1);
        if (!ok) kprintf("rtw88: página reservada não ficou válida\n");
    }
    w16(R_FIFOPAGE_CTRL_2, (u16)(D.rsvd_boundary | B(15)));
    w8(R_BCN_CTRL, bcn);
    w8(R_FWHW_TXQ_CTRL + 2, txq);
    w8(R_CR + 1, cr1);
    return ok;
}

static bool fw_to_mem(const u8 *data, u32 dst, u32 size) {
    set32(R_DDMA_CH0CTRL, DDMA_RST_CHKSUM);
    bool first = true;
    for (u32 off = 0; off < size;) {
        u32 n = size - off > 0x1000 ? 0x1000 : size - off;
        if (!write_rsvd_page(0, data + off, n)) return false;
        if (!ready32(R_DDMA_CH0CTRL, DDMA_OWN, 0)) return false;
        u32 ctrl = DDMA_CHKSUM_EN | DDMA_OWN | (n & 0x3FFFF) | (first ? 0 : DDMA_CHKSUM_CONT);
        w32(R_DDMA_CH0SA, OCP_TXBUF + TXDESC);
        w32(R_DDMA_CH0DA, dst + off);
        w32(R_DDMA_CH0CTRL, ctrl);
        if (!ready32(R_DDMA_CH0CTRL, DDMA_OWN, 0)) { kprintf("rtw88: DDMA do firmware não terminou\n"); return false; }
        first = false;
        off += n;
    }
    u8 fc = r8(R_MCUFW_CTRL);
    bool bad = r32(R_DDMA_CH0CTRL) & DDMA_CHKSUM_STS;
    if (dst < OCP_DMEM) fc = (u8)(bad ? ((fc | FW_IMEM_DW_OK) & ~FW_IMEM_CK_OK) : (fc | FW_IMEM_DW_OK | FW_IMEM_CK_OK));
    else fc = (u8)(bad ? ((fc | FW_DMEM_DW_OK) & ~FW_DMEM_CK_OK) : (fc | FW_DMEM_DW_OK | FW_DMEM_CK_OK));
    w8(R_MCUFW_CTRL, fc);
    if (bad) kprintf("rtw88: checksum do firmware errado (destino 0x%x)\n", dst);
    return !bad;
}

static bool download_fw(void) {
    u32 lte = 0;
    if (!lte_read(0x38, &lte)) { kprintf("rtw88: bloco de coexistência não responde\n"); return false; }
    wlan_cpu(false);
    /* salva e prepara: só a fila alta, 0x200 páginas para ela, beacon desligado */
    u8 pq1 = r8(R_TXDMA_PQ_MAP + 1), cr = r8(R_CR), bcn = r8(R_BCN_CTRL);
    u16 fp1 = r16(R_FIFOPAGE_INFO);
    u32 rq2 = r32(R_RQPN_CTRL_2) | B(31);
    w8(R_TXDMA_PQ_MAP + 1, 3 << 6);
    w8(R_CR, (u8)(B(0) | B(2)));
    w32(R_H2CQ_CSR, B(31));
    w16(R_FIFOPAGE_INFO, 0x200);
    w32(R_RQPN_CTRL_2, rq2);
    w8(R_BCN_CTRL, (u8)((bcn & ~B(3)) | B(4)));
    /* reinicia a plataforma da CPU do Wi-Fi */
    clr8(R_CPU_DMEM_CON + 2, 1); clr8(R_SYS_CLK_CTRL + 1, B(6));
    set8(R_CPU_DMEM_CON + 2, 1); set8(R_SYS_CLK_CTRL + 1, B(6));
    w16(R_MCUFW_CTRL, (u16)((r16(R_MCUFW_CTRL) & 0x3800) | FW_DL_EN));
    bool ok = fw_to_mem(D.fw.dmem, D.fw.dmem_addr, D.fw.dmem_size) &&
              fw_to_mem(D.fw.imem, D.fw.imem_addr, D.fw.imem_size) &&
              (!D.fw.emem_size || fw_to_mem(D.fw.emem, D.fw.emem_addr, D.fw.emem_size));
    /* restaura */
    w8(R_TXDMA_PQ_MAP + 1, pq1); w8(R_CR, cr); w32(R_H2CQ_CSR, B(31));
    w16(R_FIFOPAGE_INFO, fp1); w32(R_RQPN_CTRL_2, rq2); w8(R_BCN_CTRL, bcn);
    if (!ok) { clr8(R_MCUFW_CTRL, FW_DL_EN); set8(R_SYS_FUNC_EN + 1, 4); return false; }
    w32(R_TXDMA_STATUS, B(2));
    u16 fc = r16(R_MCUFW_CTRL);
    if ((fc & FW_CHKSUM_OK) == FW_CHKSUM_OK) w16(R_MCUFW_CTRL, (u16)((fc | FW_DW_RDY) & ~FW_DL_EN));
    wlan_cpu(true);
    lte_write(0x38, lte);
    if (!ready32(R_MCUFW_CTRL, FW_READY_MASK, FW_READY)) {
        u32 key = r32(R_FW_DBG7) & 0xF;
        kprintf("rtw88: firmware não ficou pronto (MCUFW_CTRL 0x%04x%s)\n", r16(R_MCUFW_CTRL), key == 3 ? ", chave inválida" : "");
        clr8(R_MCUFW_CTRL, FW_DL_EN); set8(R_SYS_FUNC_EN + 1, 4);
        return false;
    }
    hci_setup();
    D.h2c_box = 0; D.h2c_seq = 0;
    return true;
}

/* ============================================================ eFuse */
static bool read_efuse(void) {
    static u8 phys[RTW_EFUSE_PHYS], log[RTW_EFUSE_LOG];
    w32m(R_LDO_EFUSE, B(8) | B(9), 0);                       /* banco do Wi-Fi */
    w8(R_LDO_EFUSE + 3, (u8)(r8(R_LDO_EFUSE + 3) & ~B(7)));  /* desliga o LDO de 2,5 V */
    u32 ctl = r32(R_EFUSE_CTRL);
    for (u32 a = 0; a < RTW_EFUSE_PHYS; a++) {
        ctl &= ~(0xFFu | (0x3FFu << 8));
        ctl |= (a & 0x3FF) << 8;
        w32(R_EFUSE_CTRL, ctl & ~B(31));
        int n = 0;
        do { udelay(1); ctl = r32(R_EFUSE_CTRL); } while (!(ctl & B(31)) && ++n < 100000);
        if (!(ctl & B(31))) { kprintf("rtw88: eFuse não respondeu no byte %u\n", a); return false; }
        phys[a] = (u8)ctl;
    }
    if (!rtw_efuse_logical(phys, log)) { kprintf("rtw88: mapa do eFuse inválido\n"); return false; }
    rtw8821c_efuse_parse(log, &D.ef);
    return true;
}

/* ============================================================ MAC */
static int init_trx(void) {
    /* filas: VO/VI normal, BE/BK baixa, MGMT extra, HI alta (rqpn_table[1] do 8821C) */
    u16 map = 2 << 4 | 2 << 6 | 1 << 8 | 1 << 10 | 0 << 12 | 3 << 14;
    w16(R_TXDMA_PQ_MAP, map);
    w8(R_CR, 0);
    w8(R_CR, 0xFF);                                          /* HCI/TX/RX DMA, protocolo, agendador, MAC TX/RX */
    w32(R_H2CQ_CSR, B(31));
    /* páginas: 52 reservadas no fim da FIFO */
    u16 rsvd = RSVD_DRV + RSVD_H2C_EXTRA + RSVD_H2C_STATIC + RSVD_H2CQ + RSVD_FW_TXBUF;
    D.rsvd_boundary = PAGES - rsvd;
    D.rsvd_fw_txbuf = PAGES - RSVD_FW_TXBUF;
    D.rsvd_h2cq = (u16)(D.rsvd_fw_txbuf - RSVD_H2CQ);
    u16 hq = 16, nq = 16, lq = 16, exq = 14, gap = 1;
    u16 pub = (u16)(D.rsvd_boundary - hq - nq - lq - exq - gap);
    w16(R_FIFOPAGE_INFO + 0, hq); w16(R_FIFOPAGE_INFO + 4, lq); w16(R_FIFOPAGE_INFO + 8, nq);
    w16(R_FIFOPAGE_INFO + 12, exq); w16(R_FIFOPAGE_INFO + 16, pub);
    set32(R_RQPN_CTRL_2, B(31));
    w16(R_FIFOPAGE_CTRL_2, D.rsvd_boundary);
    set8(R_FWHW_TXQ_CTRL + 2, (u8)(B(20) >> 16));
    w16(R_BCNQ_BDNY, D.rsvd_boundary);
    w16(R_FIFOPAGE_CTRL_2 + 2, D.rsvd_boundary);
    w16(R_BCNQ1_BDNY, D.rsvd_boundary);
    w32(R_RXFF_BNDY, 16384 - 256 - 1);
    set8(R_AUTO_LLT, 1);
    if (!ready32(R_AUTO_LLT, 1, 0)) { kprintf("rtw88: LLT não inicializou\n"); return -1; }
    w8(R_CR + 3, 0);
    /* fila de H2C nas páginas reservadas */
    u32 h2cq = (u32)D.rsvd_h2cq << 7, h2cs = RSVD_H2CQ << 7;
    w32(R_H2C_HEAD, (r32(R_H2C_HEAD) & 0xFFFC0000) | h2cq);
    w32(R_H2C_READ_ADDR, (r32(R_H2C_READ_ADDR) & 0xFFFC0000) | h2cq);
    w32(R_H2C_TAIL, (r32(R_H2C_TAIL) & 0xFFFC0000) | (h2cq + h2cs));
    w8(R_H2C_INFO, (u8)((r8(R_H2C_INFO) & 0xFC) | 1));
    w8(R_H2C_INFO, (u8)((r8(R_H2C_INFO) & 0xFB) | 4));
    w8(R_TXDMA_OFFSET_CHK + 1, (u8)((r8(R_TXDMA_OFFSET_CHK + 1) & 0x7F) | 0x80));
    u32 wp = r32(R_H2C_PKT_WRITEADDR) & 0x3FFFF, rp = r32(R_H2C_PKT_READADDR) & 0x3FFFF;
    u32 free = wp >= rp ? h2cs - (wp - rp) : rp - wp;
    if (free != h2cs) { kprintf("rtw88: fila de H2C inconsistente (%u de %u livres)\n", free, h2cs); return -1; }
    return 0;
}

static void mac_init_8821c(void) {
    w8(R_AMPDU_MAX_TIME, 0x70);
    set8(R_TX_HANG_CTRL, B(2));
    u16 pre = 0x1E4 | B(11);
    w8(R_PRECNT_CTRL, (u8)pre); w8(R_PRECNT_CTRL + 1, (u8)(pre >> 8));
    w32(R_PROT_MODE, 0xFF | 0x08 << 8 | 0x20u << 16 | 0x20u << 24);
    w16(R_BAR_MODE + 2, 0x01 | 0x08 << 8);
    w8(R_FAST_EDCA_VOVI, 6); w8(R_FAST_EDCA_VOVI + 2, 6);
    w8(R_FAST_EDCA_BEBK, 6); w8(R_FAST_EDCA_BEBK + 2, 6);
    set8(R_INIRTS_RATE, B(5));
    clr8(R_TIMER0_SRC, (u8)(B(4) | B(5) | B(6)));
    w16(R_TXPAUSE, 0);
    w8(R_SLOT, 0x09);
    w8(R_PIFS, 0x19);
    w32(R_SIFS, 0x0A | 0x0E << 8 | 0x10u << 16 | 0x10u << 24);
    w16(R_EDCA_VO + 2, 0x186);
    w16(R_EDCA_VI + 2, 0x3BC);
    w32(R_RD_NAV_NXT, 0x05 | 0x1Bu << 16);
    w16(R_RXTSF_OFF_CCK, 0x30 | 0x30 << 8);
    set8(R_BCN_CTRL, B(3));
    w32(R_TBTT_PROHIBIT, 0x04 | 0x64 << 8);
    w8(R_DRVERLYINT, 0x04);
    w8(R_BCNDMATIM, 0x02);
    clr8(R_TX_PTCL_CTRL + 1, (u8)(B(12) >> 8));
    w16(R_RXFLTMAP0, 0xFFFF);
    w16(R_RXFLTMAP1, 0x0FFF);
    w16(R_RXFLTMAP2, 0xFFFF);
    w32(R_RCR, 0xE400220E);
    w8(R_RX_PKT_LIMIT, 12288 >> 9);
    w8(R_TCR + 2, 0x30);
    w8(R_TCR + 1, 0x30);
    w8(R_ACKTO_CCK, 0x40);
    set8(R_WMAC_TRXPTCL_H, B(1));
    set8(R_SND_PTCL_CTRL, B(6));
    w32(R_WMAC_OPTION + 8, 0xB0810041);
    w8(R_WMAC_OPTION + 4, 0x98);
}

static int mac_init(void) {
    if (init_trx()) return -1;
    mac_init_8821c();
    /* informação de PHY anexada a cada quadro recebido */
    w8(R_RX_DRVINFO_SZ, 4);
    w8(R_TRXFF_BNDY + 1, (u8)((r8(R_TRXFF_BNDY + 1) & 0xF0) | 0xF));
    set32(R_RCR, RCR_APP_PHYSTS);
    clr32(R_WMAC_OPTION + 4, B(8) | B(9));
    return 0;
}

/* ============================================================ PHY */
static void cfg_mac(void *c, u32 a, u32 d) { (void)c; w8(a, (u8)d); }
static void cfg_agc(void *c, u32 a, u32 d) { (void)c; w32(a, d); }
static void cfg_bb(void *c, u32 a, u32 d) {
    (void)c;
    switch (a) {
    case 0xFE: udelay(50000); break;
    case 0xFD: udelay(5000); break;
    case 0xFC: udelay(1000); break;
    case 0xFB: udelay(50); break;
    case 0xFA: udelay(5); break;
    case 0xF9: udelay(1); break;
    default: w32(a, d); break;
    }
}
static void cfg_rf(void *c, u32 a, u32 d) {
    (void)c;
    if (a == 0xFFE) udelay(50000);
    else if (a == 0xFE) udelay(100);
    else { rf_write(a, 0xFFFFF, d); udelay(1); }
}

static void phy_set_param(void) {
    u8 v = (u8)(r8(R_SYS_FUNC_EN) | B(6));
    w8(R_SYS_FUNC_EN, v);
    v |= B(0) | B(1); w8(R_SYS_FUNC_EN, v);
    v &= (u8)~(B(0) | B(1)); w8(R_SYS_FUNC_EN, v);
    v |= B(0) | B(1); w8(R_SYS_FUNC_EN, v);
    w8(R_RF_CTRL, 7); udelay(10);
    w8(R_WLRF1 + 3, 7); udelay(10);
    clr32(R_RXPSEL, B(28) | B(29));
    rtw_parse_cond_table(&rtw8821c_mac, &D.cond, cfg_mac, NULL);
    rtw_parse_cond_table(&rtw8821c_bb, &D.cond, cfg_bb, NULL);
    rtw_parse_cond_table(&rtw8821c_agc, &D.cond, cfg_agc, NULL);
    if (D.ef.rfe == 2 || D.ef.rfe == 4) rtw_parse_cond_table(&rtw8821c_agc_btg_type2, &D.cond, cfg_agc, NULL);
    rtw_parse_cond_table(&rtw8821c_rf_a, &D.cond, cfg_rf, NULL);
    u8 xt = D.ef.xtal & 0x3F;
    w32m(R_AFE_XTAL, 0x7E000000, xt);
    w32m(R_AFE_PLL, 0x7E, xt);
    w32m(R_CCK0_FAREPORT, B(18) | B(22), 0);
    set32(R_RXPSEL, B(28) | B(29));
    D.ch_param[0] = r32(R_TXSF2);
    D.ch_param[1] = r32(R_TXSF6);
    D.ch_param[2] = r32(R_TXFILTER);
    D.igi = (u8)(r32(R_IGI_A) & 0x7F);
    w32(0x1C94, 0xAFFFAFFF);
}

/* ============================================================ coexistência com o Bluetooth
 * O combo do Thiago é RFE 0: duas antenas, chave DPDT, Wi-Fi em WLG ("não compartilhada"). Em 5 GHz o
 * Wi-Fi fica com a antena dele e o BT com a dele, sem divisão de tempo; em 2,4 GHz a mesma antena separada
 * roda em "free run" com o PTA cuidando das colisões. Antena compartilhada (RFE 2/4/7...): o BT ganha a
 * prioridade pela tabela do PTA. */
enum { POS_BT, POS_WLG, POS_WLA, POS_WLG_BT };
static void coex_ant_switch(int pos) {
    bool polarity = D.ef.rfe == 3 || D.ef.rfe == 4 || D.ef.rfe == 11 || D.ef.rfe == 12;
    bool wlg_at_btg = D.ef.rfe == 2 || D.ef.rfe == 10 || D.ef.rfe == 7 || D.ef.rfe == 15 || D.ef.rfe == 4 || D.ef.rfe == 12;
    if (D.ef.rfe == 5 || D.ef.rfe == 6 || D.ef.rfe == 13 || D.ef.rfe == 14) return;   /* sem chave de antena */
    if (wlg_at_btg) pos = polarity ? POS_WLA : POS_WLG_BT;
    clr32(R_LED_CFG, B(23));
    set32(R_LED_CFG, B(24));
    w8m(R_RFE_CTRL8, 0xFF, 0x77);                           /* DPDT pelos pinos RFE 8/9 */
    u8 val;
    if (pos == POS_WLG_BT) val = (D.ef.rfe != 4 && D.ef.rfe != 2) ? 3 : (polarity ? 1 : 2);
    else if (pos == POS_WLG) val = polarity ? 1 : 2;
    else val = polarity ? 2 : 1;
    w32m(R_RFE_CTRL8, 0xF0000000, val);
    set8(0x67, (u8)(B(5) | B(4)));                            /* controle pelo Wi-Fi */
}

static void coex_scbd(u16 bits, bool set) {
    u16 v = (u16)(D.scbd | 2);
    if (set) v |= bits; else v &= (u16)~bits;
    if (v == D.scbd) return;
    D.scbd = (u8)v;
    w16(R_WIFI_BT_INFO, (u16)(v | B(15)));
}

static void coex_gnt(int bt, int wl) {                     /* 0 = PTA, 1 = SW baixo, 3 = SW alto */
    lte_write_mask(0x38, 0xC000, (u32)bt); lte_write_mask(0x38, 0x0C00, (u32)bt);
    lte_write_mask(0x38, 0x3000, (u32)wl); lte_write_mask(0x38, 0x0300, (u32)wl);
}

static void coex_table(u32 bt, u32 wl) {
    w32(R_BT_COEX_TABLE0, bt);
    w32(R_BT_COEX_TABLE1, wl);
    w32(R_BT_COEX_BRK, 0xF0FFFFFF);
}

static void coex_init(void) {
    set8(R_SYS_FUNC_EN, (u8)(B(0) | B(1)));                   /* BB ligada para escrever 0x948 */
    /* power-on: caminho com o BT, dono = BT */
    clr8(R_SYS_SDIO_CTRL + 3, (u8)(B(26) >> 24));
    coex_ant_switch(POS_BT);
    w8(0xFF1A, 0);
    clr32(R_PAD_CTRL1, B(20)); clr32(R_PAD_CTRL1, B(24));
    clr32(R_GPIO_MUXCFG, B(19)); clr32(R_PAD_CTRL1, B(15));
    clr32(R_SYS_SDIO_CTRL, B(18)); clr32(R_SYS_SDIO_CTRL, B(27));
    /* init do hardware de coex (rtw8821c_coex_cfg_init) */
    set8(R_BCN_CTRL, B(3));
    w8m(R_BT_TDMA_TIME, 0x3F, 5);
    w8(R_BT_STAT_CTRL, 1);
    set32(R_GPIO_MUXCFG, B(5));
    set32(R_GPIO_MUXCFG, B(9));
    set8(R_QUEUE_CTRL, B(4));
    clr8(R_QUEUE_CTRL, B(5));
    w16(R_BT_COEX_V2, (u16)(r16(R_BT_COEX_V2) | B(12)));
    w8m(R_BT_COEX_TABLE_H + 3, B(3), 1);
    /* respostas, beacon e fila de beacon do Wi-Fi com prioridade alta no PTA */
    w8m(R_BT_COEX_TABLE_H + 0, B(3), 1);
    w8m(R_BT_COEX_TABLE_H + 0, B(4), 1);
    w8m(R_BT_COEX_TABLE_H + 3, B(3), 1);
    /* INIT: dono = Wi-Fi, GNT_BT alto até a primeira troca de canal */
    coex_gnt(3, 1);
    set8(R_SYS_SDIO_CTRL + 3, (u8)(B(26) >> 24));
    coex_ant_switch(POS_BT);
    coex_scbd(1 | 2, true);                                 /* Wi-Fi ativo e ligado */
    if (D.ef.share_ant) coex_table(0x55555555, 0x55555555); /* tabela 1 de antena compartilhada */
    else coex_table(0xFFFF55FF, 0xFAFAFAFA);                /* tabela 101 de antena separada */
    u8 h[8] = {0x61, 1, 0, 0, 0, 0, 0, 0};                  /* pergunta o estado do BT ao firmware */
    h2c_cmd(h);
}

static void coex_band(int band) {
    if (band == 1) {
        coex_gnt(0, 3);                                     /* BT por PTA, Wi-Fi sempre liberado */
        set8(R_SYS_SDIO_CTRL + 3, (u8)(B(26) >> 24));
        coex_ant_switch(POS_WLA);
        coex_table(0xFFFFFFFF, 0xFFFFFFFF);                 /* 5 GHz: nenhum conflito de rádio */
    } else if (!D.ef.share_ant) {
        coex_gnt(0, 3);
        set8(R_SYS_SDIO_CTRL + 3, (u8)(B(26) >> 24));
        coex_ant_switch(POS_WLG_BT);
        coex_table(0xFFFFFFFF, 0xFFFFFFFF);                 /* antenas separadas: free run */
    } else {
        coex_gnt(0, 0);
        set8(R_SYS_SDIO_CTRL + 3, (u8)(B(26) >> 24));
        clr32(R_LED_CFG, B(23)); set32(R_LED_CFG, B(24));
        w8m(R_RFE_CTRL8, 0xFF, 0x66);                       /* chave pelo PTA */
        w32m(R_RFE_CTRL8, 0xF0000000, 2);
        set8(0x67, (u8)(B(5) | B(4)));
        coex_table(0x55555555, 0x5A5A5A5A);
    }
    fw_wl_ch_info(D.assoc);
}

/* ============================================================ canal */
static void switch_rf_set(int set) {           /* 0 BTG, 1 WLG, 2 WLA */
    set32(0x1080, B(16));
    set32(0x0000, B(26));
    u32 reg = r32(R_RFECTL);
    if (set == 0) { reg |= B(16); reg &= ~(B(18) | B(20) | B(22) | B(21) | B(23)); w32m(R_ENRXCCA, 0x00FF0000, 0x0E); w32m(R_ENTXCCK, 0xFFFF, 0xFC84); }
    else if (set == 1) { reg |= B(20) | B(22) | B(21); reg &= ~(B(16) | B(18) | B(23)); w32m(R_ENRXCCA, 0x00FF0000, 0x12); w32m(R_ENTXCCK, 0xFFFF, 0x7532); }
    else { reg |= B(20) | B(22) | B(23); reg &= ~(B(16) | B(18) | B(21)); }
    w32(R_RFECTL, reg);
}

static void set_channel_rf(int ch, int bw) {
    u32 r18 = rf_read(0x18, 0xFFFFF);
    r18 &= ~((B(16) | B(9) | B(8)) | 0xFF | (B(18) | B(17)) | (B(11) | B(10)));
    r18 |= ch <= 14 ? 0 : (B(16) | B(8));
    r18 |= (u32)ch & 0xFF;
    if (ch >= 100 && ch <= 140) r18 |= B(17);
    else if (ch > 140) r18 |= B(18);
    r18 |= bw == 2 ? B(10) : bw == 1 ? B(11) : (B(11) | B(10));
    if (ch <= 14) {
        switch_rf_set(D.ef.rfe_btg ? 0 : 1);
        rf_write(0xDF, B(6), 1);
        rf_write(0x64, 0xF, 0xF);
    } else {
        switch_rf_set(2);
        rf_write(0xDF, B(6), 0);
    }
    rf_write(0x18, 0xFFFFF, r18);
    rf_write(0xB8, B(19), 0);
    rf_write(0xB8, B(19), 1);
}

static void set_channel_bb(int ch, int bw, int sc) {
    if (ch <= 14) {
        w32m(R_RXPSEL, B(28), 1);
        w32m(R_CCK_CHECK, B(7), 0);
        w32m(R_ENTXCCK, B(18), 0);
        w32m(R_RXCCAMSK, 0xFC00, 15);
        w32m(R_TXSCALE_A, 0xF00, 0);
        w32m(R_CLKTRK, 0x1FFE0000, 0x96A);
        if (ch == 14) { w32(R_TXSF2, 0x0000B81C); w32m(R_TXSF6, 0xFFFF, 0); w32(R_TXFILTER, 0x00003667); }
        else { w32(R_TXSF2, D.ch_param[0]); w32m(R_TXSF6, 0xFFFF, D.ch_param[1] & 0xFFFF); w32(R_TXFILTER, D.ch_param[2]); }
    } else {
        w32m(R_ENTXCCK, B(18), 1);
        w32m(R_CCK_CHECK, B(7), 1);
        w32m(R_RXPSEL, B(28), 0);
        w32m(R_RXCCAMSK, 0xFC00, 15);
        if (ch <= 64) w32m(R_TXSCALE_A, 0xF00, 1);
        else if (ch <= 144) w32m(R_TXSCALE_A, 0xF00, 2);
        else w32m(R_TXSCALE_A, 0xF00, 3);
        if (ch <= 48) w32m(R_CLKTRK, 0x1FFE0000, 0x494);
        else if (ch <= 64) w32m(R_CLKTRK, 0x1FFE0000, 0x453);
        else if (ch <= 116) w32m(R_CLKTRK, 0x1FFE0000, 0x452);
        else w32m(R_CLKTRK, 0x1FFE0000, 0x412);
    }
    u32 v = r32(R_ADCCLK);
    if (bw == 0) { v = (v & 0xFFCFFC00) | 0x10010000; }
    else if (bw == 1) {
        if (sc == 1) set32(R_RXSB, B(4)); else clr32(R_RXSB, B(4));
        v = (v & 0xFF3FF300) | 0x20020000 | (u32)(sc & 0xF) << 2 | 1;
    } else v = (v & 0xFCFFCF00) | 0x40040000 | (u32)(sc & 0xF) << 2 | 2;
    w32(R_ADCCLK, v);
    w32m(R_ADC160, B(30), 1);
}

static void set_channel_mac(int ch, int bw, int sc) {
    int sc40 = 0;
    if (bw == 2) sc40 = (sc == 1 || sc == 3) ? 9 : 10;
    w8(R_DATA_SC, (u8)((sc & 0xF) | (sc40 & 0xF) << 4));
    u32 v = r32(R_WMAC_TRXPTCL) & ~(B(7) | B(8));
    if (bw == 2) v |= B(8); else if (bw == 1) v |= B(7);
    w32(R_WMAC_TRXPTCL, v);
    v = r32(0x0024) & ~(B(20) | B(21));
    w32(0x0024, v);                                          /* relógio do MAC: 80 MHz */
    w8(R_USTIME_TSF, 80);
    w8(R_USTIME_EDCA, 80);
    u8 c = (u8)(r8(R_CCK_CHECK) & ~B(7));
    if (ch > 14) c |= B(7);
    w8(R_CCK_CHECK, c);
}

static void set_channel_rxdfir(int bw) {
    w32m(R_ACBB0, B(29) | B(28), 2);
    if (bw == 1) { w32m(R_ACBBRXFIR, B(29) | B(28), 2); w32m(R_TXDFIR, B(31), 0); w32m(R_CHFIR, B(31), 0); }
    else if (bw == 2) { w32m(R_ACBBRXFIR, B(29) | B(28), 1); w32m(R_TXDFIR, B(31), 0); w32m(R_CHFIR, B(31), 1); }
    else { w32m(R_ACBBRXFIR, B(29) | B(28), 2); w32m(R_TXDFIR, B(31), 1); w32m(R_CHFIR, B(31), 0); }
}

static void set_tx_power(void) {
    u32 acc = 0;
    for (int s = 0; s < RTW_RS_NUM; s++) {
        if (s == RTW_RS_HT2 || s == RTW_RS_VHT2) continue;
        for (int k = 0; k < rtw_rate_sec_n[s]; k++) {
            u8 rate = rtw_rate_sec[s][k];
            u8 idx = rtw_pwr_index(&D.pwr, D.ef.txpwr, D.band, D.bw, rate, D.center, D.primary);
            if (D.band && s == RTW_RS_CCK) idx = 0;
            acc |= (u32)idx << ((rate & 3) * 8);
            if ((rate & 3) == 3 || rate == 0x35) { w32(R_TXAGC_A + (rate & 0xFC), acc); acc = 0; }
        }
    }
}

/* índice do subcanal primário dentro da largura (RTW_SC_*): 1 acima, 2 abaixo, 3 o mais alto, 4 o mais baixo */
static int primary_sc(int primary, int center, int bw) {
    if (bw == 0) return 0;
    int d = (primary - center) * 5;                          /* MHz */
    if (bw == 1) return d > 0 ? 1 : 2;
    if (d > 0) return d == 10 ? 1 : 3;
    return d == -10 ? 2 : 4;
}

static void iqk(void) {
    fw_iqk(D.assoc);
    for (int i = 0; i < 300; i++) {
        if (rf_read(0x08, 0xFFFFF) == 0xABCDE) break;
        udelay(20000);
    }
    rf_write(0x08, 0xFFFFF, 0);
}

static bool set_chan(int primary, int center, int bw) {
    if (!D.up) return false;
    if (bw && (center <= 0)) bw = 0;
    if (!bw) center = primary;
    int sc = primary_sc(primary, center, bw);
    int band = primary > 14;
    bool band_change = band != D.band;
    D.band = band; D.primary = primary; D.center = center; D.bw = bw;
    set_channel_bb(center, bw, sc);
    w32m(R_TXSCALE_A, 0xFFE00000, ((u32[]){0x200, 0x16A, 0x101, 0x0B6})[((center <= 14 ? D.ef.swing_2g : D.ef.swing_5g) > 9 ? 0 : (center <= 14 ? D.ef.swing_2g : D.ef.swing_5g)) / 3]);
    set_channel_mac(center, bw, sc);
    set_channel_rf(center, bw);
    set_channel_rxdfir(bw);
    if (band_change || !D.assoc) coex_band(band);
    set_tx_power();
    return true;
}

static bool r_set_channel(wifi_radio_t *r, int ch) { (void)r; return set_chan(ch, ch, 0); }
static bool r_set_chan(wifi_radio_t *r, int p, int c, int bw) { (void)r; return set_chan(p, c, bw); }

/* ============================================================ adaptação de taxa e associação */
static void update_ra(bool reset) {
    u32 mask;
    bool b5 = D.band == 1;
    u32 legacy = D.bss.legacy ? D.bss.legacy : (b5 ? 0xFF0 : 0xFFF);
    if (b5) legacy &= 0xFF0;
    if (D.bss.vht) {
        int cap = D.bss.vht_mcs_map & 3;
        u32 v = cap == 2 ? 0x3FF : cap == 1 ? 0x1FF : 0xFF;
        mask = (legacy & 0xFF0) | v << 12;
        D.rate_id = b5 ? 10 : 11;                           /* ARFR1_AC_1SS / ARFR2_AC_2G_1SS */
        if (!b5) mask |= legacy & 0xF;
    } else if (D.bss.ht) {
        mask = legacy | (u32)D.bss.ht_mcs << 12;
        D.rate_id = b5 ? 5 : (D.bss.bw == 1 ? 1 : 3);       /* GN_N1SS / BGN_40M_1SS / BGN_20M_1SS */
    } else {
        mask = legacy;
        D.rate_id = b5 ? 7 : ((legacy & 0xFF0) ? 6 : 8);    /* G / BG / B */
    }
    D.ra_bw = (u8)(D.bss.vht ? D.bss.bw : D.bss.ht ? (D.bss.bw ? 1 : 0) : 0);
    fw_ra_info(mask, reset);
}

static void r_bss(wifi_radio_t *r, const wifi_bss_t *b) {
    (void)r;
    if (!D.up) return;
    if (!b) {
        if (!D.assoc) return;
        D.assoc = false;
        fw_media_status(false);
        w32m(0x0100, 0x30000, 0);                            /* tipo de rede: sem enlace */
        for (int i = 0; i < 6; i++) w8(R_BSSID + i, 0);
        D.rcr &= ~(RCR_CBSSID_DATA | RCR_CBSSID_BCN);
        w32(R_RCR, D.rcr);
        fw_wl_ch_info(false);
        coex_scbd(1 << 6, false);
        return;
    }
    D.bss = *b;
    for (int i = 0; i < 6; i++) w8(R_BSSID + i, b->bssid[i]);
    w16(R_AID, (u16)(b->aid & 0x7FF));
    w32m(0x0100, 0x30000, 2);                                /* infra, associado */
    D.rcr |= RCR_CBSSID_DATA;                                /* dados só da nossa BSS */
    w32(R_RCR, D.rcr);
    bool was = D.assoc;
    D.assoc = true;
    fw_media_status(true);
    update_ra(!was);
    fw_wl_ch_info(true);
    coex_scbd(1 << 6, true);
    iqk();                                                   /* calibração com o canal da rede */
}

/* ============================================================ TX e RX */
static int q_for(const u8 *f) {
    u8 type = (f[0] >> 2) & 3;
    if (type == 0 || type == 1) return Q_MGMT;
    if (f[4] & 1) return Q_HI0;
    return Q_BE;
}

static bool r_tx(wifi_radio_t *r, const u8 *f, u16 len) {
    (void)r;
    if (!D.up || len < 24) return false;
    txinfo_t t = {0};
    int q = q_for(f);
    t.bmc = f[4] & 1;
    u8 type = (f[0] >> 2) & 3, sub = f[0] >> 4;
    bool nullfunc = type == 2 && (sub == 4 || sub == 12);
    t.seq = (u16)((f[22] | f[23] << 8) >> 4);
    if (q == Q_MGMT || nullfunc || !D.assoc || t.bmc) {
        /* gestão, null, broadcast ou antes de associar: taxa básica fixa */
        t.use_rate = t.dis_fb = true;
        t.rate = D.band ? 0x04 : 0x00;
        t.rate_id = D.band ? 7 : 8;
        t.dis_qselseq = t.en_hwseq = q == Q_MGMT;
        t.qsel = q == Q_MGMT ? QSEL_MGMT : q == Q_HI0 ? QSEL_HIGH : 0;
        t.more = q == Q_HI0;
    } else {
        /* dados unicast: o firmware escolhe a taxa pela máscara do RA */
        t.rate = D.bss.vht ? 0x35 : D.bss.ht ? 0x13 : 0x0B;
        t.rate_id = D.rate_id;
        t.bw = D.ra_bw;
        t.sgi = D.bss.bw == 2 ? D.bss.sgi80 : D.bss.bw == 1 ? D.bss.sgi40 : D.bss.sgi20;
        t.ldpc = D.bss.ldpc;
        u8 tid = (f[0] & 0x80) ? (f[24] & 7) : 0;            /* QoS data: TID */
        t.qsel = tid;
    }
    return tx_queue(q, &t, f, len);
}

/* potência da PHY -> RSSI em dBm (OFDM: PWDB - 110; CCK: ganho do LNA e do VGA) */
static int phy_rssi(const u8 *ps, u8 rate) {
    if (rate <= 0x03) {
        static const i8 lna0[8] = {22, 8, -6, -22, -31, -40, -46, -52};
        static const i8 lna1[16] = {10, 6, 2, -2, -6, -10, -14, -17, -20, -24, -28, -31, -34, -37, -40, -44};
        u32 w3 = get32(ps + 12);
        u8 vga = (u8)((w3 >> 8) & 0x1F);
        u8 lna = (u8)(((w3 >> 23) & 1) << 3 | ((w3 >> 13) & 7));
        int g = D.ef.rfe == 0 ? (lna < 8 ? lna0[lna] : -120) : lna1[lna & 15];
        return g - 2 * vga;
    }
    return (int)ps[1] - 110;
}

static void c2h(const u8 *p, int len) {
    if (len < 2) return;
    if (p[0] == 0x0C && len >= 3) {                          /* RA report: taxa atual */
        D.tx_rate = p[2] & 0x7F;
    } else if (p[0] == 0x09) {                               /* estado do BT: registra para o log de coex */
        /* o firmware mantém a tabela; nada a fazer aqui além de saber que o BT está vivo */
    }
}

static int r_rx(wifi_radio_t *r, u8 *f, u16 max, int *rssi) {
    (void)r;
    if (!D.up) return 0;
    for (;;) {
        u32 hw = (r32(R_RXBD_IDX_MPDUQ) >> 16) & 0xFFF;
        if (hw == D.rx.rp) return 0;
        u32 i = D.rx.rp;
        const u8 *bd = D.rx.desc + 8 * i;
        u16 tag = (u16)(bd[2] | bd[3] << 8);
        if (tag != D.rx_tag) kprintf("rtw88: etiqueta de RX %u, esperada %u (barramento PCIe atrasado)\n", tag, D.rx_tag);
        D.rx_tag = (u16)((D.rx_tag + 1) % 8192);
        __sync_synchronize();
        const u8 *d = D.rx.buf[i];
        u32 w0 = get32(d), w2 = get32(d + 8), w3 = get32(d + 12);
        u32 plen = w0 & 0x3FFF, drv = ((w0 >> 16) & 0xF) * 8, shift = (w0 >> 24) & 3;
        bool crc = w0 & B(14), icv = w0 & B(15), is_c2h = w2 & B(28), physt = w0 & B(26);
        u8 rate = (u8)(w3 & 0x7F);
        int n = 0;
        const u8 *pl = d + RXDESC + drv + shift;
        if (RXDESC + drv + shift + plen <= RXBUF && drv <= 32) {
            if (is_c2h) c2h(pl, (int)plen);
            else if (!crc && !icv && plen > 4 + 10) {
                n = (int)plen - 4;                           /* tira o FCS */
                if (n > max) n = max;
                memcpy(f, pl, (usize)n);
                if (physt && drv) { *rssi = phy_rssi(d + RXDESC + shift, rate); D.rssi = *rssi; }
            }
        }
        rx_desc_reset(i);
        D.rx.rp = (D.rx.rp + 1) % D.rx.len;
        w16(R_RXBD_IDX_MPDUQ, (u16)D.rx.rp);
        if (n > 0) return n;
    }
}

/* ============================================================ economia de energia (LPS do firmware, sem PG) */
static bool r_powersave(wifi_radio_t *r, bool on) {
    (void)r;
    if (!D.up || !D.assoc || D.ps_broken || on == D.ps_on) return on == D.ps_on;
    u8 h[8] = {0x20, 0, 0, 0, 0, 0, 0, 0};
    if (on) {
        h[1] = 1;                                            /* modo LPS */
        h[2] = 1 | 2 << 4;                                   /* rlbm 1, smart_ps 2 */
        h[3] = 1;                                            /* acorda a cada beacon */
        h[5] = 0x00;                                         /* RF desligado entre beacons */
        h2c_cmd(h);
        D.ps_on = true;
        return true;
    }
    h[3] = 1;
    h[5] = 0x0C;                                             /* tudo ligado */
    h2c_cmd(h);
    for (int i = 0; i < 5; i++) {                            /* o firmware avisa o AP e restaura o TCR */
        if (!(r32(R_TCR) & B(7))) { D.ps_on = false; return true; }
        udelay(20000);
    }
    clr32(R_TCR, B(7));
    D.ps_on = false;
    D.ps_broken = true;                                      /* o mesmo defeito do Linux neste chip: desliga o LPS */
    kprintf("rtw88: o firmware não saiu do LPS; economia de energia desligada nesta sessão\n");
    return false;
}

/* ============================================================ vigia (a cada 2 s) */
static void dig(void) {
    /* ganho inicial a partir do RSSI do AP: mais forte -> IGI maior (menos sensível a ruído) */
    if (!D.assoc) return;
    int rssi_pct = D.rssi + 100;
    if (rssi_pct < 0) rssi_pct = 0;
    if (rssi_pct > 100) rssi_pct = 100;
    int igi = rssi_pct - 10;
    if (igi > 0x3E) igi = 0x3E;
    if (igi < 0x1C) igi = 0x1C;                              /* dig_min do 8821C */
    if (igi != D.igi) { D.igi = (u8)igi; w32m(R_IGI_A, 0x7F, (u32)igi); }
    fw_rssi(rssi_pct);
}

static void r_poll(wifi_radio_t *r) {
    (void)r;
    if (!D.up) return;
    u32 now = (u32)timer_ticks();
    if (now - D.last_watchdog < 2000) return;
    D.last_watchdog = now;
    dig();
}

/* ============================================================ subida */
static bool chip_start(void) {
    hci_setup();
    if (mac_power_on()) { kprintf("rtw88: sequência de energia falhou\n"); return false; }
    if (!download_fw()) return false;
    if (mac_init()) return false;
    phy_set_param();
    fw_general_info();
    fw_phydm_info();
    coex_init();
    D.rcr = RCR_APP_FCS | RCR_APP_MIC | RCR_APP_ICV | RCR_PKTCTL_DLEN | RCR_HTC_LOC | RCR_APP_PHYSTS |
            RCR_AB | RCR_AM | RCR_APM | RCR_VHT_DACK;
    w32(R_RCR, D.rcr);
    for (int i = 0; i < 6; i++) w8(R_MACID + i, D.ef.mac[i]);
    w32m(0x0100, 0x30000, 0);
    w32(R_HIMR0, 0); w32(R_HIMR1, 0); w32(R_HIMR3, 0);       /* polling: interrupções mascaradas */
    return true;
}

/* sobe o chip: firmware já validado em D.fw; devolve false com o motivo no log */
bool rtw8821c_attach(rtw_io_t *io, pci_dev_t *pci) {
    D.io = *io;
    D.pci = pci;
    for (int q = 0; q < Q_N; q++)
        if (!ring_alloc(&D.tx[q], q_len[q], 16, q == Q_BCN ? BCNBUF : TXBUF)) { kprintf("rtw88: sem memória abaixo de 4 GiB para os anéis\n"); return false; }
    if (!ring_alloc(&D.rx, RX_N, 8, RXBUF)) { kprintf("rtw88: sem memória abaixo de 4 GiB para RX\n"); return false; }
    u32 cfg = r32(R_SYS_CFG1);
    D.cut = (u8)((cfg >> 12) & 0xF);
    kprintf("rtw88: RTL8821C corte %c, %s\n", 'A' + D.cut, (cfg & B(27)) ? "2T2R (inesperado)" : "1T1R");
    /* 1) liga o MAC e o firmware só para ler o eFuse */
    hci_setup();
    if (mac_power_on()) { kprintf("rtw88: sequência de energia falhou\n"); return false; }
    w8(R_C2HEVT, 0xFD);
    if (!download_fw()) return false;
    if (!read_efuse()) return false;
    w8(R_C2HEVT, 0);
    mac_power_switch(false);
    D.cond.rfe = D.ef.rfe; D.cond.intf = RTW_INTF_PCIE; D.cond.pkg = D.ef.pkg; D.cond.cut = D.cut;
    bool mac_ok = false;
    for (int i = 0; i < 6; i++) if (D.ef.mac[i] != 0xFF && D.ef.mac[i] != 0) mac_ok = true;
    if (!mac_ok || (D.ef.mac[0] & 1)) {
        for (int i = 0; i < 6; i++) D.ef.mac[i] = (u8)rand32();
        D.ef.mac[0] = (u8)((D.ef.mac[0] & 0xFE) | 2);
        kprintf("rtw88: eFuse sem MAC válido; usando um aleatório local\n");
    }
    kprintf("rtw88: eFuse: RFE %u (%s), cristal 0x%02x, país %c%c, BT %s\n", D.ef.rfe, D.ef.rfe_btg ? "BTG" : "WLG",
            D.ef.xtal, D.ef.country[0] > 32 ? D.ef.country[0] : '?', D.ef.country[1] > 32 ? D.ef.country[1] : '?',
            D.ef.share_ant ? "na mesma antena" : "em antena separada");
    rtw_pwr_init(&D.pwr, &rtw8821c_bb_pg_type0, &rtw8821c_txpwr_lmt_type0, RTW_REGD_FCC);
    /* 2) sobe de vez */
    if (!chip_start()) return false;
    D.up = true;
    D.band = -1;
    set_chan(1, 1, 0);
    memcpy(D.radio.mac, D.ef.mac, 6);
    D.radio.name = "Realtek RTL8821CE";
    D.radio.priv = &D;
    D.radio.set_channel = r_set_channel;
    D.radio.set_chan = r_set_chan;
    D.radio.tx = r_tx;
    D.radio.rx = r_rx;
    D.radio.poll = r_poll;
    D.radio.bss = r_bss;
    D.radio.powersave = r_powersave;
    wifi_register_radio(&D.radio);
    return true;
}

#ifndef MOS_HOST_TEST
/* ============================================================ PCI */
static volatile u8 *mm;
static u8 m_r8(rtw_io_t *io, u32 a) { (void)io; return mmio_r8(mm, a); }
static u16 m_r16(rtw_io_t *io, u32 a) { (void)io; return mmio_r16(mm, a); }
static u32 m_r32(rtw_io_t *io, u32 a) { (void)io; return mmio_r32(mm, a); }
static void m_w8(rtw_io_t *io, u32 a, u8 v) { (void)io; mmio_w8(mm, a, v); }
static void m_w16(rtw_io_t *io, u32 a, u16 v) { (void)io; mmio_w16(mm, a, v); }
static void m_w32(rtw_io_t *io, u32 a, u32 v) { (void)io; mmio_w32(mm, a, v); }
static void m_udelay(rtw_io_t *io, u32 us) {
    (void)io;
    if (us >= 1000) { sleep_ms((us + 999) / 1000); return; }
    u64 t0 = hpet_ns();
    if (t0) { while (hpet_ns() - t0 < (u64)us * 1000) __asm__ volatile("pause"); return; }
    u64 end = timer_ticks() + 1;                             /* sem HPET: arredonda para 1 ms */
    while (timer_ticks() < end) __asm__ volatile("pause");
}

/* PCIe: desliga o timeout de conclusão (DEVCTL2 bit 4), como o driver da Realtek faz no 8821CE */
static void pcie_disable_comp_timeout(pci_dev_t *d) {
    u8 p = (u8)(pci_read(d, 0x34) & 0xFC);
    for (int i = 0; p && i < 48; i++) {
        u32 h = pci_read(d, p);
        if ((h & 0xFF) == 0x10) {                            /* capability do PCI Express */
            u32 v = pci_read(d, (u8)(p + 0x28));
            pci_write(d, (u8)(p + 0x28), v | B(4));
            return;
        }
        p = (u8)((h >> 8) & 0xFC);
    }
}

bool wifi_rtw_probe(void) {
    pci_dev_t *d = pci_find(0x10EC, 0xC821);
    if (!d) return false;
    kprintf("rtw88: RTL8821CE (10ec:c821) em %02x:%02x.%x\n", d->bus, d->dev, d->fn);
    vnode_t *v = vfs_lookup("/Sistema/Firmware/rtw88/rtw8821c_fw.bin");
    char err[96];
    if (!v || !v->data) { kprintf("rtw88: firmware rtw88/rtw8821c_fw.bin não encontrado em /Sistema/Firmware\n"); return false; }
    if (!rtw_fw_parse(v->data, (u32)v->size, &D.fw, err, sizeof err)) { kprintf("rtw88: firmware inválido: %s\n", err); return false; }
    kprintf("rtw88: firmware %u.%u\n", D.fw.version, D.fw.sub);
    u64 base = pci_bar_addr(d, 2);
    if (!base) { kprintf("rtw88: sem BAR2\n"); return false; }
    pci_write(d, 0x04, pci_read(d, 0x04) | 0x02);            /* memória */
    pci_enable_bus_master(d);
    pcie_disable_comp_timeout(d);
    vmm_map_mmio(base, 0x10000);
    mm = (volatile u8 *)base;
    if (mmio_r32(mm, R_SYS_CFG1) == 0xFFFFFFFF) { kprintf("rtw88: a placa não responde (MMIO 0xFFFFFFFF)\n"); return false; }
    rtw_io_t io = {0, m_r8, m_r16, m_r32, m_w8, m_w16, m_w32, m_udelay};
    if (!rtw8821c_attach(&io, d)) { kprintf("rtw88: o Wi-Fi não subiu (detalhes acima)\n"); return false; }
    kprintf("rtw88: pronto\n");
    return true;
}
#endif

/* para o teste de host */
#ifdef MOS_HOST_TEST
void rtw8821c_set_fw(const rtw_fw_t *fw) { D.fw = *fw; }
const rtw_efuse_t *rtw8821c_efuse(void) { return &D.ef; }
int rtw8821c_state(int *band, int *primary, int *center, int *bw) { *band = D.band; *primary = D.primary; *center = D.center; *bw = D.bw; return D.up; }
bool rtw8821c_ps_broken(void) { return D.ps_broken; }
#endif

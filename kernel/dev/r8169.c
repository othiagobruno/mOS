/* mOS — driver Realtek RTL8111/8168 (10EC:8168, 10EC:8161) e RTL810x (10EC:8136) por MMIO, com polling.
 *
 * Anéis de descritores de 16 bytes em memória DMA (identidade), sem interrupções, como o e1000.
 * A família vem do XID de TxConfig e decide o RxConfig e os passos extras dos chips 8168G/H/EP
 * (tirar do modo OOB, abrir o "RXDV gate", tamanho dos FIFOs por ERI).
 * Referência de comportamento: drivers/net/ethernet/realtek/r8169_main.c do Linux (rtl_hw_start_8168h_1,
 * rtl_hw_init_8168g, rtl_init_rxcfg, rtl_set_rx_mode, rtl_eri_write, r8168_mac_ocp_write,
 * r8168_phy_ocp_write). Nada foi copiado: só os endereços e a ordem dos passos.
 *
 * Fica de fora, de propósito:
 *  - firmware de PHY (rtl_nic/rtl8168h-2.fw): é opcional no Linux (só corrige afinação) e aqui não é usado;
 *  - tabela de EPHY (afinação do PCIe), ajustes de EEE e os demais MAC OCP de rtl_hw_start_8168h_1:
 *    o enlace PCIe já está de pé quando lemos os registradores, e um valor errado ali o derrubaria;
 *  - o MAC de reserva em 0x19E0 (MAC0_BKP): fora da janela de 4 KiB que nem todo BAR cobre. */
#ifdef MOS_HOST_TEST
#include "r8169_shim.h"
#else
#include "kernel.h"
#include "kernel/net/netpriv.h"
#endif

/* ---------- registradores ---------- */
#define R_IDR0       0x00
#define R_MAR0       0x08
#define R_TXDESC_LO  0x20
#define R_TXDESC_HI  0x24
#define R_CHIPCMD    0x37
#define R_TXPOLL     0x38
#define R_INTRMASK   0x3C
#define R_INTRSTATUS 0x3E
#define R_TXCONFIG   0x40
#define R_RXCONFIG   0x44
#define R_CFG9346    0x50
#define R_CONFIG2    0x53
#define R_CONFIG5    0x56
#define R_PHYAR      0x60
#define R_PHYSTATUS  0x6C
#define R_PMCH       0x6F
#define R_ERIDR      0x70
#define R_ERIAR      0x74
#define R_OCPDR      0xB0     /* MAC OCP (8168G+) */
#define R_GPHY_OCP   0xB8     /* PHY por OCP (8168G+) */
#define R_MCU        0xD3
#define R_RXMAXSIZE  0xDA
#define R_CPLUSCMD   0xE0
#define R_INTRMITIG  0xE2
#define R_RXDESC_LO  0xE4
#define R_RXDESC_HI  0xE8
#define R_MAXTXPKT   0xEC
#define R_MISC       0xF0

#define CMD_RE       0x08
#define CMD_TE       0x04
#define CMD_RESET    0x10
#define TXPOLL_NPQ   0x40
#define CFG_UNLOCK   0xC0
#define CFG_LOCK     0x00
#define PHY_LINK     0x02
#define CFG2_CLKREQ  0x80
#define CFG5_ASPM    0x01
#define MCU_OOB      0x80     /* NOW_IS_OOB: o firmware (UEFI/BMC) ainda é dono do MAC */
#define MCU_LL_RDY   0x02     /* LINK_LIST_RDY */
#define MISC_RXDV_GATE (1u << 19)
#define CPLUS_RXCSUM (1u << 5)
#define CPLUS_RXVLAN (1u << 6)

/* RxConfig */
#define RXC_MYPHYS   0x02
#define RXC_MCAST    0x04
#define RXC_BCAST    0x08
#define RXC_ACCEPT   0x3F
#define RXC_DMA_UNL  (7u << 8)
#define RXC_EARLYOFF (1u << 11)
#define RXC_FIFO_NONE (7u << 13)
#define RXC_MULTI    (1u << 14)
#define RXC_128INT   (1u << 15)

/* TxConfig */
#define TXC_DMA_UNL  (7u << 8)
#define TXC_IFG96    (3u << 24)
#define TXC_AUTOFIFO (1u << 7)

/* descritores */
#define D_OWN        (1u << 31)
#define D_EOR        (1u << 30)
#define D_FS         (1u << 29)
#define D_LS         (1u << 28)
#define D_RXRES      (1u << 21)
#define D_LEN        0x3FFFu

#define RX_DESC   256
#define TX_DESC   64
#define BUF_SIZE  2048
#define FRAME_MAX 1792       /* mesmo teto dos vizinhos; RxMaxSize segura o resto no chip */

typedef struct { u32 opts1, opts2, addr_lo, addr_hi; } r8169_desc_t;

/* família pelo XID (TxConfig bits 30:26 e 22:20): RxConfig e passos de inicialização */
enum { RXK_FIFO, RXK_128, RXK_MULTI, RXK_G };
#define F_EVL  1    /* 8168E-VL em diante: FIFO automático no TX, MaxTxPacketSize "early" */
#define F_G    2    /* 8168G/H/EP: modo OOB, RXDV gate, FIFOs por ERI, PHY por OCP */

typedef struct { u16 mask, xid; const char *name; u8 rxk, flags; } r8169_model_t;

static const r8169_model_t models[] = {
    {0x7cf, 0x541, "RTL8168H/8111H",   RXK_G, F_EVL | F_G},
    {0x7cf, 0x502, "RTL8168EP/8111EP", RXK_G, F_EVL | F_G},
    {0x7cf, 0x5c8, "RTL8411B",         RXK_G, F_EVL | F_G},
    {0x7cf, 0x509, "RTL8168GU/8111GU", RXK_G, F_EVL | F_G},
    {0x7cf, 0x4c1, "RTL8168G/8111G",   RXK_G, F_EVL | F_G},
    {0x7cf, 0x4c0, "RTL8168G/8111G",   RXK_G, F_EVL | F_G},
    {0x7c8, 0x488, "RTL8411",          RXK_MULTI, F_EVL},
    {0x7cf, 0x481, "RTL8168F/8111F",   RXK_MULTI, F_EVL},
    {0x7cf, 0x480, "RTL8168F/8111F",   RXK_MULTI, F_EVL},
    {0x7c8, 0x2c8, "RTL8168E-VL/8111E-VL", RXK_MULTI, F_EVL},
    {0x7cf, 0x2c1, "RTL8168E/8111E",   RXK_128, 0},
    {0x7c8, 0x2c0, "RTL8168E/8111E",   RXK_128, 0},
    {0x7cf, 0x281, "RTL8168D/8111D",   RXK_128, 0},
    {0x7c8, 0x280, "RTL8168D/8111D",   RXK_128, 0},
    {0x7cf, 0x28a, "RTL8168DP/8111DP", RXK_128, 0},
    {0x7cf, 0x28b, "RTL8168DP/8111DP", RXK_128, 0},
    {0x7c8, 0x3c8, "RTL8168C/8111C",   RXK_MULTI, 0},
    {0x7c8, 0x3c0, "RTL8168C/8111C",   RXK_MULTI, 0},
    {0x7c8, 0x380, "RTL8168B/8111B",   RXK_FIFO, 0},
    {0x7c8, 0x300, "RTL8168B/8111B",   RXK_FIFO, 0},
    {0x7c8, 0x448, "RTL8106E",         RXK_128, 0},
    {0x7c8, 0x440, "RTL8402",          RXK_128, F_EVL},
    {0x7cf, 0x409, "RTL8105E",         RXK_128, 0},
    {0x7c8, 0x408, "RTL8105E",         RXK_128, 0},
    {0x7c8, 0x348, "RTL8101E/8102E",   RXK_FIFO, 0},
    {0x7c8, 0x248, "RTL8101E/8102E",   RXK_FIFO, 0},
    {0x7c8, 0x340, "RTL8101E",         RXK_FIFO, 0},
    {0x7cf, 0x240, "RTL8101E",         RXK_FIFO, 0},
};
static const r8169_model_t model_unknown = {0, 0, "Realtek (XID desconhecido)", RXK_128, 0};

static struct {
    volatile u8 *mmio;
    const r8169_model_t *model;
    u16 xid;
    volatile r8169_desc_t *rx, *tx;
    u8 *rxbuf[RX_DESC];
    u8 *txbuf[TX_DESC];
    u32 rx_cur, tx_cur;
    char name[64];
} R;

static netdev_t r8169_dev;

static inline u8   reg_r8(u32 o)  { return mmio_r8(R.mmio, o); }
static inline u16  reg_r16(u32 o) { return mmio_r16(R.mmio, o); }
static inline u32  reg_r32(u32 o) { return mmio_r32(R.mmio, o); }
static inline void reg_w8(u32 o, u8 v)   { mmio_w8(R.mmio, o, v); }
static inline void reg_w16(u32 o, u16 v) { mmio_w16(R.mmio, o, v); }
static inline void reg_w32(u32 o, u32 v) { mmio_w32(R.mmio, o, v); }
static inline void barrier(void) { __asm__ volatile("" ::: "memory"); }

/* espera um bit de um registrador de 32 bits chegar a `want`; cada leitura MMIO custa ~1 µs no PCIe,
 * então 20000 leituras cobrem com folga os 10 ms que o Linux dá ao ERI/EPHY */
static bool wait32(u32 reg, u32 bit, bool want) {
    for (int i = 0; i < 20000; i++)
        if (!!(reg_r32(reg) & bit) == want) return true;
    return false;
}

/* o mesmo para registradores de 8 bits (MCU em 0xD3 não está alinhado a 4) */
static bool wait8(u32 reg, u8 bit) {
    for (int i = 0; i < 20000; i++)
        if (reg_r8(reg) & bit) return true;
    return false;
}

/* ---------- ERI (rtl_eri_write / rtl_eri_read): janela de registradores internos do MAC */
#define ERIAR_FLAG  0x80000000u
#define ERIAR_MASK_1111 (0xFu << 12)

static bool eri_write(u32 addr, u32 val) {
    reg_w32(R_ERIDR, val);
    reg_w32(R_ERIAR, ERIAR_FLAG | ERIAR_MASK_1111 | addr);   /* tipo EXGMAC = 0 */
    if (wait32(R_ERIAR, ERIAR_FLAG, false)) return true;
    kprintf("r8169: ERI 0x%x: escrita sem resposta\n", addr);
    return false;
}

static bool eri_read(u32 addr, u32 *val) {
    reg_w32(R_ERIAR, ERIAR_MASK_1111 | addr);
    if (!wait32(R_ERIAR, ERIAR_FLAG, true)) { kprintf("r8169: ERI 0x%x: leitura sem resposta\n", addr); return false; }
    *val = reg_r32(R_ERIDR);
    return true;
}

static void eri_modify(u32 addr, u32 clear, u32 set) {
    u32 v;
    if (eri_read(addr, &v)) eri_write(addr, (v & ~clear) | set);
}

/* ---------- MAC OCP (r8168_mac_ocp_write/read): síncrono, sem bit de espera */
static void mac_ocp_write(u32 reg, u16 v) { reg_w32(R_OCPDR, 0x80000000u | (reg << 15) | v); }
static u16 mac_ocp_read(u32 reg) { reg_w32(R_OCPDR, reg << 15); return (u16)reg_r32(R_OCPDR); }
static void mac_ocp_modify(u32 reg, u16 clear, u16 set) { mac_ocp_write(reg, (u16)((mac_ocp_read(reg) & ~clear) | set)); }

/* ---------- PHY: OCP nos 8168G+ (registrador padrão r em 0xA400 + 2r), PHYAR (MDIO) nos anteriores */
static bool phy_read(u32 reg, u16 *v) {
    if (R.model->flags & F_G) {
        reg_w32(R_GPHY_OCP, (0xA400u + reg * 2) << 15);
        if (!wait32(R_GPHY_OCP, 0x80000000u, true)) return false;
        *v = (u16)reg_r32(R_GPHY_OCP);
    } else {
        reg_w32(R_PHYAR, (reg & 0x1F) << 16);
        if (!wait32(R_PHYAR, 0x80000000u, true)) return false;
        *v = (u16)reg_r32(R_PHYAR);
    }
    return true;
}

static bool phy_write(u32 reg, u16 v) {
    if (R.model->flags & F_G) {
        reg_w32(R_GPHY_OCP, 0x80000000u | ((0xA400u + reg * 2) << 15) | v);
        return wait32(R_GPHY_OCP, 0x80000000u, false);
    }
    reg_w32(R_PHYAR, 0x80000000u | ((reg & 0x1F) << 16) | v);
    return wait32(R_PHYAR, 0x80000000u, false);
}

/* ---------- netdev ---------- */
static bool r8169_link_up(void) { return R.mmio && (reg_r8(R_PHYSTATUS) & PHY_LINK); }

static bool r8169_send(const void *frame, u16 len) {
    if (!R.mmio || len == 0 || len > FRAME_MAX) return false;
    u32 i = R.tx_cur;
    volatile r8169_desc_t *d = &R.tx[i];
    if (d->opts1 & D_OWN) return false;              /* anel cheio: o NIC ainda não mandou este */
    memcpy(R.txbuf[i], frame, len);
    if (len < 60) { memset(R.txbuf[i] + len, 0, 60 - len); len = 60; }
    d->opts2 = 0;
    barrier();
    d->opts1 = D_OWN | D_FS | D_LS | (i == TX_DESC - 1 ? D_EOR : 0) | len;
    barrier();
    R.tx_cur = (i + 1) % TX_DESC;
    reg_w8(R_TXPOLL, TXPOLL_NPQ);
    return true;
}

static void rx_give_back(u32 i) {
    volatile r8169_desc_t *d = &R.rx[i];
    d->opts2 = 0;
    barrier();
    d->opts1 = D_OWN | (i == RX_DESC - 1 ? D_EOR : 0) | BUF_SIZE;
}

static int r8169_recv(void *frame, u16 max) {
    if (!R.mmio) return 0;
    u16 st = reg_r16(R_INTRSTATUS);
    if (st) reg_w16(R_INTRSTATUS, st);   /* polling: só limpa (RDU/FIFO cheio voltam sozinhos com descritores livres) */
    for (int n = 0; n < RX_DESC; n++) {
        u32 i = R.rx_cur;
        u32 o = R.rx[i].opts1;
        if (o & D_OWN) return 0;
        barrier();
        int len = (int)(o & D_LEN) - 4;               /* sem o CRC */
        bool ok = (o & (D_FS | D_LS)) == (D_FS | D_LS) && !(o & D_RXRES) && len > 0 && len <= max;
        if (ok) memcpy(frame, R.rxbuf[i], (usize)len);
        rx_give_back(i);
        R.rx_cur = (i + 1) % RX_DESC;
        if (ok) return len;
        /* com erro, fragmentado (maior que o buffer) ou grande demais para quem chama: descarta e segue */
    }
    return 0;
}

/* ---------- inicialização ---------- */
static bool setup_rings(void) {
    u64 rxa, txa;
    R.rx = dma_alloc(sizeof(r8169_desc_t) * RX_DESC, &rxa);   /* páginas: alinhamento de 256 garantido */
    R.tx = dma_alloc(sizeof(r8169_desc_t) * TX_DESC, &txa);
    u8 *rxpool = dma_alloc(BUF_SIZE * RX_DESC, NULL);
    u8 *txpool = dma_alloc(BUF_SIZE * TX_DESC, NULL);
    if (!R.rx || !R.tx || !rxpool || !txpool || (rxa & 0xFF) || (txa & 0xFF)) return false;
    for (u32 i = 0; i < RX_DESC; i++) {
        R.rxbuf[i] = rxpool + i * BUF_SIZE;
        u64 a = (u64)(uptr)R.rxbuf[i];
        R.rx[i].addr_lo = (u32)a;
        R.rx[i].addr_hi = (u32)(a >> 32);
        rx_give_back(i);
    }
    for (u32 i = 0; i < TX_DESC; i++) {
        R.txbuf[i] = txpool + i * BUF_SIZE;
        u64 a = (u64)(uptr)R.txbuf[i];
        R.tx[i].addr_lo = (u32)a;
        R.tx[i].addr_hi = (u32)(a >> 32);
        R.tx[i].opts1 = i == TX_DESC - 1 ? D_EOR : 0;   /* livre (sem Own) */
    }
    barrier();
    R.rx_cur = R.tx_cur = 0;
    /* o alto antes do baixo, como o Linux (rtl_set_rx_tx_desc_registers) */
    reg_w32(R_TXDESC_HI, (u32)(txa >> 32));
    reg_w32(R_TXDESC_LO, (u32)txa);
    reg_w32(R_RXDESC_HI, (u32)(rxa >> 32));
    reg_w32(R_RXDESC_LO, (u32)rxa);
    return true;
}

static const r8169_model_t *model_of(u16 xid) {
    for (u32 k = 0; k < ARRAY_LEN(models); k++)
        if ((xid & models[k].mask) == models[k].xid) return &models[k];
    return &model_unknown;
}

/* primeiro BAR de memória (o 8168 tem I/O no BAR0 e o MMIO de 64 bits no BAR2) */
static int find_mem_bar(pci_dev_t *pd, u64 *addr) {
    for (int i = 0; i < 6; i++) {
        u32 b = pd->bar[i];
        if (!b || (b & 1)) continue;
        u64 a = pci_bar_addr(pd, i);
        if (a) { *addr = a; return i; }
        if (((b >> 1) & 3) == 2) i++;   /* metade alta de um BAR de 64 bits */
    }
    return -1;
}

/* ASPM e CLKREQ desligados no Link Control da capacidade PCIe: com polling, o enlace entrando em L1
 * entre leituras só traz latência e, em alguns 8168, perda de quadros */
static void pcie_aspm_off(pci_dev_t *pd) {
    if (!(pci_read(pd, 0x04) & (1u << 20))) return;   /* sem lista de capacidades */
    u8 p = (u8)(pci_read(pd, 0x34) & 0xFC);
    for (int n = 0; p && n < 48; n++) {
        u32 h = pci_read(pd, p);
        if ((h & 0xFF) == 0x10) {
            u32 lc = pci_read(pd, (u8)(p + 0x10));
            if (lc & 0x103) pci_write(pd, (u8)(p + 0x10), lc & ~0x103u & 0xFFFFu);  /* não reescreve o Link Status (RW1C) */
            return;
        }
        p = (u8)((h >> 8) & 0xFC);
    }
}

/* rtl_hw_init_8168g: tira o MAC do modo OOB (firmware/UEFI) e espera a lista de FIFO compartilhada */
static void hw_init_8168g(void) {
    reg_w32(R_MISC, reg_r32(R_MISC) | MISC_RXDV_GATE);
    reg_w8(R_CHIPCMD, reg_r8(R_CHIPCMD) & ~(CMD_TE | CMD_RE));
    sleep_ms(1);
    reg_w8(R_MCU, reg_r8(R_MCU) & ~MCU_OOB);
    mac_ocp_modify(0xE8DE, 1u << 14, 0);
    if (!wait8(R_MCU, MCU_LL_RDY)) kprintf("r8169: FIFO compartilhado não ficou pronto (1)\n");
    mac_ocp_modify(0xE8DE, 0, 1u << 15);
    if (!wait8(R_MCU, MCU_LL_RDY)) kprintf("r8169: FIFO compartilhado não ficou pronto (2)\n");
}

static bool mac_valid(const u8 *m) {
    bool zero = true, ones = true;
    for (int i = 0; i < 6; i++) { if (m[i]) zero = false; if (m[i] != 0xFF) ones = false; }
    return !zero && !ones && !(m[0] & 1);
}

bool r8169_probe(void) {
    pci_dev_t *pd = NULL;
    for (int i = 0; i < pci_count() && !pd; i++) {
        pci_dev_t *d = pci_get(i);
        if (d->vendor == 0x10EC && (d->device == 0x8168 || d->device == 0x8161 || d->device == 0x8136)) pd = d;
    }
    if (!pd) return false;

    u64 bar = 0;
    int bi = find_mem_bar(pd, &bar);
    if (bi < 0) { kprintf("r8169: nenhum BAR de memória\n"); return false; }
    vmm_map_mmio(bar, 4096);
    R.mmio = (volatile u8 *)(uptr)bar;
    pci_enable_bus_master(pd);
    pci_write(pd, 0x04, pci_read(pd, 0x04) | 0x06 | (1u << 10));   /* memória + bus master, INTx desligado */

    R.xid = (u16)((reg_r32(R_TXCONFIG) >> 20) & 0xFCF);
    R.model = model_of(R.xid);
    kprintf("r8169: %s (XID 0x%03x) %04x:%04x em %02x:%02x.%x, MMIO BAR%d 0x%llx\n", R.model->name, R.xid,
            pd->vendor, pd->device, pd->bus, pd->dev, pd->fn, bi, bar);
    if (R.xid == 0xFCF) { kprintf("r8169: registradores ilegíveis (0xFFFFFFFF)\n"); R.mmio = NULL; return false; }

    reg_w16(R_INTRMASK, 0);
    reg_w16(R_INTRSTATUS, 0xFFFF);
    if (R.model->flags & F_G) {
        hw_init_8168g();
        reg_w8(R_PMCH, reg_r8(R_PMCH) | 0xC0);   /* PLL ligado (rtl_pll_power_up) */
    } else {
        reg_w8(R_CHIPCMD, reg_r8(R_CHIPCMD) & ~(CMD_TE | CMD_RE));
    }

    reg_w8(R_CHIPCMD, CMD_RESET);
    int t = 0;
    for (; t < 100 && (reg_r8(R_CHIPCMD) & CMD_RESET); t++) sleep_ms(1);
    if (reg_r8(R_CHIPCMD) & CMD_RESET) { kprintf("r8169: reset não terminou\n"); R.mmio = NULL; return false; }

    u8 mac[6];
    for (int i = 0; i < 6; i++) mac[i] = reg_r8(R_IDR0 + i);
    bool own_mac = !mac_valid(mac);
    if (own_mac) {
        u32 r = rand32();
        mac[0] = 0x02; mac[1] = 0x6D; mac[2] = 0x4F;
        mac[3] = (u8)r; mac[4] = (u8)(r >> 8); mac[5] = (u8)(r >> 16);
        kprintf("r8169: MAC não encontrado, usando endereço local\n");
    }

    reg_w8(R_CFG9346, CFG_UNLOCK);
    if (own_mac) {
        reg_w32(R_IDR0 + 4, mac[4] | (mac[5] << 8));
        reg_w32(R_IDR0, mac[0] | (mac[1] << 8) | (mac[2] << 16) | ((u32)mac[3] << 24));
    }
    /* ASPM e CLKREQ desligados antes de mexer no resto (rtl_hw_aspm_clkreq_enable(false)) */
    reg_w8(R_CONFIG5, reg_r8(R_CONFIG5) & ~CFG5_ASPM);
    reg_w8(R_CONFIG2, reg_r8(R_CONFIG2) & ~CFG2_CLKREQ);
    pcie_aspm_off(pd);

    reg_w8(R_MAXTXPKT, (R.model->flags & F_EVL) ? 0x27 : 0x3F);
    reg_w16(R_CPLUSCMD, reg_r16(R_CPLUSCMD) & ~(CPLUS_RXCSUM | CPLUS_RXVLAN));   /* sem checksum nem VLAN no RX */
    if (R.model->flags & F_G) {
        /* o mínimo de rtl_hw_start_8168g/8168h_1 para RX e TX: FIFOs, filtro de pacotes reiniciado, RXDV gate aberto */
        eri_write(0xC8, (0x08u << 16) | 0x02);
        eri_write(0xE8, (0x10u << 16) | 0x06);
        eri_modify(0xDC, 1, 0);
        eri_modify(0xDC, 0, 1);
        reg_w32(R_MISC, reg_r32(R_MISC) & ~MISC_RXDV_GATE);
    }
    reg_w16(R_INTRMITIG, 0);
    reg_w16(R_RXMAXSIZE, BUF_SIZE);

    if (!setup_rings()) {
        reg_w8(R_CFG9346, CFG_LOCK);
        kprintf("r8169: sem memória DMA\n");
        R.mmio = NULL;
        return false;
    }
    reg_w8(R_CFG9346, CFG_LOCK);
    (void)reg_r8(R_CHIPCMD);   /* commit das escritas postadas antes de ligar */

    reg_w8(R_CHIPCMD, CMD_TE | CMD_RE);
    u32 rxc = R.model->rxk == RXK_FIFO ? RXC_FIFO_NONE | RXC_DMA_UNL
            : R.model->rxk == RXK_MULTI ? RXC_128INT | RXC_MULTI | RXC_DMA_UNL
            : R.model->rxk == RXK_G ? RXC_128INT | RXC_MULTI | RXC_DMA_UNL | RXC_EARLYOFF
            : RXC_128INT | RXC_DMA_UNL;
    reg_w32(R_RXCONFIG, rxc | RXC_BCAST | RXC_MCAST | RXC_MYPHYS);
    reg_w32(R_TXCONFIG, TXC_IFG96 | TXC_DMA_UNL | ((R.model->flags & F_EVL) ? TXC_AUTOFIFO : 0));
    /* todo multicast passa (o IPv6 precisa de 33:33:…); o filtro fino é em software, como no e1000 */
    reg_w32(R_MAR0, 0xFFFFFFFF);
    reg_w32(R_MAR0 + 4, 0xFFFFFFFF);

    /* PHY desligado (BMCR.PDOWN) ou sem autonegociação: liga e reinicia a negociação */
    u16 bmcr;
    if (phy_read(0, &bmcr)) {
        if ((bmcr & 0x0800) || !(bmcr & 0x1000)) phy_write(0, 0x1000 | 0x0200);
    } else {
        kprintf("r8169: PHY não respondeu\n");
    }

    snprintf(R.name, sizeof R.name, "Realtek %s (r8169)", R.model->name);
    r8169_dev.name = R.name;
    r8169_dev.send = r8169_send;
    r8169_dev.recv = r8169_recv;
    r8169_dev.link_up = r8169_link_up;
    memcpy(r8169_dev.mac, mac, 6);
    kprintf("r8169: MAC %02x:%02x:%02x:%02x:%02x:%02x, reset em %d ms, link %s\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], t, r8169_link_up() ? "ativo" : "aguardando");
    net_register_device(&r8169_dev);
    return true;
}

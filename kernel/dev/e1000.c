/* mOS — driver Intel PRO/1000 (8254x, 8257x/82583 "e1000e", I217/I218/I219 "PCH")
 * Usa descritores legados, anéis de RX/TX em memória DMA (identidade) e polling — sem interrupções. */
#include "kernel.h"
#include "kernel/net/netpriv.h"

/* ---------- registradores ---------- */
#define REG_CTRL    0x0000
#define REG_STATUS  0x0008
#define REG_EECD    0x0010
#define REG_EERD    0x0014
#define REG_CTRL_EXT 0x0018
#define REG_ICR     0x00C0
#define REG_IMS     0x00D0
#define REG_IMC     0x00D8
#define REG_RCTL    0x0100
#define REG_TCTL    0x0400
#define REG_TIPG    0x0410
#define REG_RDBAL   0x2800
#define REG_RDBAH   0x2804
#define REG_RDLEN   0x2808
#define REG_RDH     0x2810
#define REG_RDT     0x2818
#define REG_RDTR    0x2820
#define REG_RXDCTL  0x2828
#define REG_RADV    0x282C
#define REG_TDBAL   0x3800
#define REG_TDBAH   0x3804
#define REG_TDLEN   0x3808
#define REG_TDH     0x3810
#define REG_TDT     0x3818
#define REG_TXDCTL  0x3828
#define REG_MTA     0x5200
#define REG_RAL0    0x5400
#define REG_RAH0    0x5404

#define CTRL_FD      (1u << 0)
#define CTRL_LRST    (1u << 3)
#define CTRL_ASDE    (1u << 5)
#define CTRL_SLU     (1u << 6)
#define CTRL_ILOS    (1u << 7)
#define CTRL_RST     (1u << 26)
#define CTRL_VME     (1u << 30)
#define CTRL_PHY_RST (1u << 31)

#define STATUS_LU    (1u << 1)

#define RCTL_EN      (1u << 1)
#define RCTL_SBP     (1u << 2)
#define RCTL_UPE     (1u << 3)
#define RCTL_MPE     (1u << 4)
#define RCTL_LPE     (1u << 5)
#define RCTL_BAM     (1u << 15)
#define RCTL_BSIZE_2048 (0u << 16)
#define RCTL_SECRC   (1u << 26)

#define TCTL_EN      (1u << 1)
#define TCTL_PSP     (1u << 3)
#define TCTL_CT(x)   ((u32)(x) << 4)
#define TCTL_COLD(x) ((u32)(x) << 12)
#define TCTL_RTLC    (1u << 24)

#define TXCMD_EOP    (1u << 0)
#define TXCMD_IFCS   (1u << 1)
#define TXCMD_RS     (1u << 3)
#define TXSTA_DD     (1u << 0)
#define RXSTA_DD     (1u << 0)
#define RXSTA_EOP    (1u << 1)

#define RX_DESC   128
#define TX_DESC   64
#define BUF_SIZE  2048

typedef struct PACKED {
    u64 addr;
    u16 length;
    u16 csum;
    u8  status;
    u8  errors;
    u16 special;
} rx_desc_t;

typedef struct PACKED {
    u64 addr;
    u16 length;
    u8  cso;
    u8  cmd;
    u8  status;
    u8  css;
    u16 special;
} tx_desc_t;

/* família do controlador: muda o formato do EERD e alguns bits de inicialização */
enum { FAM_8254X, FAM_8254X_EERD2, FAM_E1000E, FAM_PCH };

typedef struct { u16 id; u8 fam; const char *name; } e1000_model_t;

static const e1000_model_t models[] = {
    {0x1000, FAM_8254X, "Intel 82542"},
    {0x1001, FAM_8254X, "Intel 82543GC"},
    {0x1004, FAM_8254X, "Intel 82543GC"},
    {0x1008, FAM_8254X, "Intel 82544EI"},
    {0x100C, FAM_8254X, "Intel 82544GC"},
    {0x100E, FAM_8254X, "Intel 82540EM"},
    {0x100F, FAM_8254X, "Intel 82545EM"},
    {0x1010, FAM_8254X, "Intel 82546EB"},
    {0x1011, FAM_8254X, "Intel 82545EM"},
    {0x1015, FAM_8254X, "Intel 82540EM"},
    {0x1016, FAM_8254X, "Intel 82540EP"},
    {0x1017, FAM_8254X, "Intel 82540EP"},
    {0x101E, FAM_8254X, "Intel 82540EP"},
    {0x1026, FAM_8254X, "Intel 82545GM"},
    {0x1027, FAM_8254X, "Intel 82545GM"},
    {0x1028, FAM_8254X, "Intel 82545GM"},
    {0x1079, FAM_8254X, "Intel 82546GB"},
    {0x1019, FAM_8254X_EERD2, "Intel 82547EI"},
    {0x1075, FAM_8254X_EERD2, "Intel 82547GI"},
    {0x1076, FAM_8254X_EERD2, "Intel 82541GI"},
    {0x1077, FAM_8254X_EERD2, "Intel 82541GI"},
    {0x1078, FAM_8254X_EERD2, "Intel 82541ER"},
    {0x107C, FAM_8254X_EERD2, "Intel 82541PI"},
    {0x105E, FAM_E1000E, "Intel 82571EB"},
    {0x105F, FAM_E1000E, "Intel 82571EB"},
    {0x107D, FAM_E1000E, "Intel 82572EI"},
    {0x107E, FAM_E1000E, "Intel 82572EI"},
    {0x10B9, FAM_E1000E, "Intel 82572EI"},
    {0x108B, FAM_E1000E, "Intel 82573V"},
    {0x108C, FAM_E1000E, "Intel 82573E"},
    {0x109A, FAM_E1000E, "Intel 82573L"},
    {0x10D3, FAM_E1000E, "Intel 82574L"},
    {0x10F6, FAM_E1000E, "Intel 82574LA"},
    {0x150C, FAM_E1000E, "Intel 82583V"},
    {0x10EA, FAM_PCH, "Intel 82577LM"},
    {0x10EB, FAM_PCH, "Intel 82577LC"},
    {0x10EF, FAM_PCH, "Intel 82578DM"},
    {0x10F0, FAM_PCH, "Intel 82578DC"},
    {0x1502, FAM_PCH, "Intel 82579LM"},
    {0x1503, FAM_PCH, "Intel 82579V"},
    {0x153A, FAM_PCH, "Intel I217-LM"},
    {0x153B, FAM_PCH, "Intel I217-V"},
    {0x155A, FAM_PCH, "Intel I218-LM"},
    {0x1559, FAM_PCH, "Intel I218-V"},
    {0x15A0, FAM_PCH, "Intel I218-LM"},
    {0x15A1, FAM_PCH, "Intel I218-V"},
    {0x15A2, FAM_PCH, "Intel I218-LM"},
    {0x15A3, FAM_PCH, "Intel I218-V"},
    {0x156F, FAM_PCH, "Intel I219-LM"},
    {0x1570, FAM_PCH, "Intel I219-V"},
    {0x15B7, FAM_PCH, "Intel I219-LM"},
    {0x15B8, FAM_PCH, "Intel I219-V"},
    {0x15B9, FAM_PCH, "Intel I219-LM"},
    {0x15BB, FAM_PCH, "Intel I219-LM"},
    {0x15BC, FAM_PCH, "Intel I219-V"},
    {0x15BD, FAM_PCH, "Intel I219-LM"},
    {0x15BE, FAM_PCH, "Intel I219-V"},
    {0x15D6, FAM_PCH, "Intel I219-V"},
    {0x15D7, FAM_PCH, "Intel I219-LM"},
    {0x15D8, FAM_PCH, "Intel I219-V"},
    {0x15E3, FAM_PCH, "Intel I219-LM"},
    {0x15DF, FAM_PCH, "Intel I219-LM"},
    {0x15E0, FAM_PCH, "Intel I219-V"},
    {0x15E1, FAM_PCH, "Intel I219-LM"},
    {0x15E2, FAM_PCH, "Intel I219-V"},
    {0x0D4C, FAM_PCH, "Intel I219-LM"},
    {0x0D4D, FAM_PCH, "Intel I219-V"},
    {0x0D4E, FAM_PCH, "Intel I219-LM"},
    {0x0D4F, FAM_PCH, "Intel I219-V"},
    {0x0D53, FAM_PCH, "Intel I219-LM"},
    {0x0D55, FAM_PCH, "Intel I219-V"},
    {0x15FB, FAM_PCH, "Intel I219-LM"},
    {0x15FC, FAM_PCH, "Intel I219-V"},
    {0x1A1C, FAM_PCH, "Intel I219-LM"},
    {0x1A1D, FAM_PCH, "Intel I219-V"},
    {0x1A1E, FAM_PCH, "Intel I219-LM"},
    {0x1A1F, FAM_PCH, "Intel I219-V"},
};

static struct {
    volatile u8 *mmio;
    const e1000_model_t *model;
    rx_desc_t *rx;
    tx_desc_t *tx;
    u8 *rxbuf[RX_DESC];
    u8 *txbuf[TX_DESC];
    u32 rx_cur, tx_cur;
    char name[48];
} E;

static netdev_t e1000_dev;

static inline u32 rd(u32 off) { return mmio_r32(E.mmio, off); }
static inline void wr(u32 off, u32 v) { mmio_w32(E.mmio, off, v); }
static inline void barrier(void) { __asm__ volatile("" ::: "memory"); }

/* lê uma palavra de 16 bits da EEPROM via EERD; devolve false em timeout */
static bool eeprom_read(u8 addr, u16 *out) {
    u32 shift, done;
    if (E.model->fam == FAM_8254X) { shift = 8; done = 1u << 4; }
    else { shift = 2; done = 1u << 1; }
    wr(REG_EERD, ((u32)addr << shift) | 1u);
    for (int i = 0; i < 100000; i++) {
        u32 v = rd(REG_EERD);
        if (v & done) { *out = (u16)(v >> 16); return true; }
    }
    return false;
}

static bool mac_valid(const u8 *m) {
    bool zero = true, ones = true;
    for (int i = 0; i < 6; i++) { if (m[i]) zero = false; if (m[i] != 0xFF) ones = false; }
    return !zero && !ones && !(m[0] & 1);
}

static bool read_mac_ral(u8 *mac) {
    u32 lo = rd(REG_RAL0), hi = rd(REG_RAH0);
    if (!(hi & (1u << 31))) return false;
    for (int i = 0; i < 4; i++) mac[i] = (u8)(lo >> (i * 8));
    mac[4] = (u8)hi; mac[5] = (u8)(hi >> 8);
    return mac_valid(mac);
}

static bool read_mac_eeprom(u8 *mac) {
    if (E.model->fam == FAM_PCH) return false;  /* NVM em flash SPI: depende do firmware ter programado RAL */
    for (int i = 0; i < 3; i++) {
        u16 w;
        if (!eeprom_read((u8)i, &w)) return false;
        mac[i * 2] = (u8)w; mac[i * 2 + 1] = (u8)(w >> 8);
    }
    return mac_valid(mac);
}

static bool e1000_link_up(void) { return E.mmio && (rd(REG_STATUS) & STATUS_LU); }

static bool e1000_send(const void *frame, u16 len) {
    if (!E.mmio || len == 0 || len > BUF_SIZE) return false;
    tx_desc_t *d = &E.tx[E.tx_cur];
    if (!(d->status & TXSTA_DD)) return false;     /* anel cheio */
    memcpy(E.txbuf[E.tx_cur], frame, len);
    d->addr = (u64)(uptr)E.txbuf[E.tx_cur];
    d->length = len;
    d->cso = 0; d->css = 0; d->special = 0;
    d->cmd = TXCMD_EOP | TXCMD_IFCS | TXCMD_RS;
    d->status = 0;
    barrier();
    E.tx_cur = (E.tx_cur + 1) % TX_DESC;
    wr(REG_TDT, E.tx_cur);
    return true;
}

static int e1000_recv(void *frame, u16 max) {
    if (!E.mmio) return 0;
    for (;;) {
        rx_desc_t *d = &E.rx[E.rx_cur];
        u8 st = *(volatile u8 *)&d->status;
        if (!(st & RXSTA_DD)) return 0;
        barrier();
        int len = d->length;
        bool ok = (st & RXSTA_EOP) && d->errors == 0 && len > 0 && len <= max;
        if (ok) memcpy(frame, E.rxbuf[E.rx_cur], len);
        d->status = 0;
        d->errors = 0;
        barrier();
        u32 done = E.rx_cur;
        E.rx_cur = (E.rx_cur + 1) % RX_DESC;
        wr(REG_RDT, done);   /* devolve o descritor ao hardware */
        if (ok) return len;
        /* quadro inválido/fragmentado: descarta e tenta o próximo */
    }
}

static bool setup_rings(void) {
    u64 phys;
    E.rx = dma_alloc(sizeof(rx_desc_t) * RX_DESC, &phys);
    E.tx = dma_alloc(sizeof(tx_desc_t) * TX_DESC, NULL);
    u8 *rxpool = dma_alloc(BUF_SIZE * RX_DESC, NULL);
    u8 *txpool = dma_alloc(BUF_SIZE * TX_DESC, NULL);
    if (!E.rx || !E.tx || !rxpool || !txpool) return false;
    for (int i = 0; i < RX_DESC; i++) {
        E.rxbuf[i] = rxpool + i * BUF_SIZE;
        E.rx[i].addr = (u64)(uptr)E.rxbuf[i];
        E.rx[i].status = 0;
    }
    for (int i = 0; i < TX_DESC; i++) {
        E.txbuf[i] = txpool + i * BUF_SIZE;
        E.tx[i].addr = (u64)(uptr)E.txbuf[i];
        E.tx[i].status = TXSTA_DD;     /* livre */
    }

    /* recepção */
    u64 rxa = (u64)(uptr)E.rx, txa = (u64)(uptr)E.tx;
    wr(REG_RDBAL, (u32)rxa);
    wr(REG_RDBAH, (u32)(rxa >> 32));
    wr(REG_RDLEN, sizeof(rx_desc_t) * RX_DESC);
    wr(REG_RDH, 0);
    wr(REG_RDT, RX_DESC - 1);
    wr(REG_RDTR, 0);
    E.rx_cur = 0;

    /* transmissão */
    wr(REG_TDBAL, (u32)txa);
    wr(REG_TDBAH, (u32)(txa >> 32));
    wr(REG_TDLEN, sizeof(tx_desc_t) * TX_DESC);
    wr(REG_TDH, 0);
    wr(REG_TDT, 0);
    E.tx_cur = 0;

    if (E.model->fam == FAM_E1000E || E.model->fam == FAM_PCH) {
        /* bit 22 de TXDCTL deve ser 1 nestes controladores; GRAN = descritores */
        wr(REG_TXDCTL, rd(REG_TXDCTL) | (1u << 22) | (1u << 24));
        wr(REG_RXDCTL, rd(REG_RXDCTL) | (1u << 24));
    }

    wr(REG_TIPG, 10u | (8u << 10) | (6u << 20));
    wr(REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT(0x0F) | TCTL_COLD(0x40) | TCTL_RTLC);
    wr(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_MPE | RCTL_BSIZE_2048 | RCTL_SECRC);   /* MPE: multicast do IPv6 (NDP) */
    return true;
}

bool e1000_probe(void) {
    pci_dev_t *pd = NULL;
    for (int i = 0; i < pci_count() && !pd; i++) {
        pci_dev_t *d = pci_get(i);
        if (d->vendor != 0x8086) continue;
        for (u32 k = 0; k < ARRAY_LEN(models); k++)
            if (models[k].id == d->device) { pd = d; E.model = &models[k]; break; }
    }
    if (!pd) return false;

    u64 bar = pci_bar_addr(pd, 0);
    if (!bar || (pd->bar[0] & 1)) { kprintf("e1000: BAR0 inválido\n"); return false; }
    vmm_map_mmio(bar, 128 * 1024);
    E.mmio = (volatile u8 *)(uptr)bar;
    pci_enable_bus_master(pd);
    /* sem interrupções: o driver é consultado por polling */
    pci_write(pd, 0x04, pci_read(pd, 0x04) | (1u << 10));

    /* MAC programado pelo firmware (antes do reset, pois o reset pode limpar RAL nas PCH) */
    u8 mac[6];
    bool have_mac = read_mac_ral(mac);

    wr(REG_IMC, 0xFFFFFFFF);
    wr(REG_RCTL, 0);
    wr(REG_TCTL, 0);
    (void)rd(REG_STATUS);
    sleep_ms(2);
    wr(REG_CTRL, rd(REG_CTRL) | CTRL_RST);
    sleep_ms(E.model->fam == FAM_PCH ? 20 : 5);
    for (int i = 0; i < 1000 && (rd(REG_CTRL) & CTRL_RST); i++) sleep_ms(1);
    wr(REG_IMC, 0xFFFFFFFF);
    (void)rd(REG_ICR);

    if (!have_mac) have_mac = read_mac_ral(mac);
    if (!have_mac) have_mac = read_mac_eeprom(mac);
    if (!have_mac) {
        /* sem MAC utilizável: gera um endereço localmente administrado */
        u32 r = rand32();
        mac[0] = 0x02; mac[1] = 0x6D; mac[2] = 0x4F;
        mac[3] = (u8)r; mac[4] = (u8)(r >> 8); mac[5] = (u8)(r >> 16);
        kprintf("e1000: MAC não encontrado, usando endereço local\n");
    }
    wr(REG_RAL0, mac[0] | (mac[1] << 8) | (mac[2] << 16) | ((u32)mac[3] << 24));
    wr(REG_RAH0, mac[4] | (mac[5] << 8) | (1u << 31));

    u32 ctrl = rd(REG_CTRL);
    ctrl &= ~(CTRL_LRST | CTRL_PHY_RST | CTRL_ILOS | CTRL_VME);
    ctrl |= CTRL_SLU;
    if (E.model->fam == FAM_8254X || E.model->fam == FAM_8254X_EERD2) ctrl |= CTRL_ASDE;
    wr(REG_CTRL, ctrl);

    for (int i = 0; i < 128; i++) wr(REG_MTA + i * 4, 0);

    if (!setup_rings()) { kprintf("e1000: sem memória DMA\n"); E.mmio = NULL; return false; }

    snprintf(E.name, sizeof E.name, "%s (e1000)", E.model->name);
    e1000_dev.name = E.name;
    e1000_dev.send = e1000_send;
    e1000_dev.recv = e1000_recv;
    e1000_dev.link_up = e1000_link_up;
    memcpy(e1000_dev.mac, mac, 6);
    kprintf("e1000: %s %04x em %02x:%02x.%x, MMIO 0x%llx, MAC %02x:%02x:%02x:%02x:%02x:%02x, link %s\n",
            E.model->name, pd->device, pd->bus, pd->dev, pd->fn, bar,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], e1000_link_up() ? "ativo" : "aguardando");
    net_register_device(&e1000_dev);
    return true;
}

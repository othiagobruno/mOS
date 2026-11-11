/* mOS — teste de host do driver r8169 (kernel/dev/r8169.c) contra um RTL8168H simulado.
 * O "chip" é um arquivo de registradores de 4 KiB com os efeitos colaterais que o driver usa
 * (reset que se apaga sozinho, ERI/OCP/PHY com bit de flag, IntrStatus W1C, TxPoll); a config PCI
 * tem I/O no BAR0, MMIO de 64 bits no BAR2 e outro BAR de memória no BAR4 (tabela MSI-X).
 * zig cc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/r8169_host.c kernel/dev/r8169.c */
#include "r8169_shim.h"
#include <stdarg.h>

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------- chip simulado ---------- */
static u8 regs[4096] __attribute__((aligned(4096)));
static u8 msix[4096] __attribute__((aligned(4096)));
static u32 eri[0x400];
static u16 macocp[0x10000], phy[0x10000];
static int reset_reads, resets, txpolls, foreign, locked_cfg_writes, unlocked_seen;
static u8 last_txpoll;

static u32 r32(u32 o) { u32 v; memcpy(&v, regs + o, 4); return v; }
static void s32(u32 o, u32 v) { memcpy(regs + o, &v, 4); }
static u16 r16(u32 o) { u16 v; memcpy(&v, regs + o, 2); return v; }

static bool ours(volatile void *b, u32 off, int n) {
    /* fora do BAR2, além da janela ou desalinhado (um MMIO de 32 bits em 0xD3 não existe no chip) */
    if ((const void *)b != (const void *)regs || off + n > sizeof regs || off % n) { foreign++; return false; }
    return true;
}

static void after_read(u32 off) {
    if (off == 0x37 && reset_reads > 0 && --reset_reads == 0) regs[0x37] &= (u8)~0x10;
}

/* efeitos colaterais de uma escrita (o valor já foi gravado, exceto onde o chip faz outra coisa) */
static void on_write(u32 off, u32 v, int n, u32 old) {
    bool cfg = off == 0x53 || off == 0x56 || off == 0x00 || off == 0x04 || off == 0x20 || off == 0x24
            || off == 0xE4 || off == 0xE8;
    if (cfg && regs[0x50] != 0xC0) locked_cfg_writes++;
    switch (off) {
    case 0x50: if (v == 0xC0) unlocked_seen++; break;
    case 0x37: if (v & 0x10) { resets++; reset_reads = 3; } break;
    case 0x38: txpolls++; last_txpoll = (u8)v; regs[0x38] = 0; break;
    case 0x3E: { u16 st = (u16)(old & ~v); memcpy(regs + 0x3E, &st, 2); break; }
    case 0x40: s32(0x40, (old & 0x7CF00000u) | (v & ~0x7CF00000u)); break;   /* XID é só leitura */
    case 0x74: {
        u32 a = (v & 0xFFF) / 4;
        if (v & 0x80000000u) { eri[a] = r32(0x70); s32(0x74, v & ~0x80000000u); }
        else { s32(0x70, eri[a]); s32(0x74, v | 0x80000000u); }
        break;
    }
    case 0xB0: {
        u32 reg = (v >> 15) & 0xFFFF;
        if (v & 0x80000000u) macocp[reg] = (u16)v; else s32(0xB0, (v & 0xFFFF0000u) | macocp[reg]);
        break;
    }
    case 0xB8: {
        u32 reg = (v >> 15) & 0xFFFF;
        if (v & 0x80000000u) { phy[reg] = (u16)v; s32(0xB8, v & ~0x80000000u); }
        else s32(0xB8, 0x80000000u | (v & 0x7FFF0000u) | phy[reg]);
        break;
    }
    }
    (void)n;
}

u8 mmio_r8(volatile void *b, u32 o) { if (!ours(b, o, 1)) return 0xFF; u8 v = regs[o]; after_read(o); return v; }
u16 mmio_r16(volatile void *b, u32 o) { if (!ours(b, o, 2)) return 0xFFFF; return r16(o); }
u32 mmio_r32(volatile void *b, u32 o) { if (!ours(b, o, 4)) return 0xFFFFFFFF; return r32(o); }
void mmio_w8(volatile void *b, u32 o, u8 v) { if (!ours(b, o, 1)) return; u32 old = regs[o]; regs[o] = v; on_write(o, v, 1, old); }
void mmio_w16(volatile void *b, u32 o, u16 v) { if (!ours(b, o, 2)) return; u32 old = r16(o); memcpy(regs + o, &v, 2); on_write(o, v, 2, old); }
void mmio_w32(volatile void *b, u32 o, u32 v) { if (!ours(b, o, 4)) return; u32 old = r32(o); s32(o, v); on_write(o, v, 4, old); }

/* ---------- PCI simulado ---------- */
static pci_dev_t devs[2];
static int ndevs;
static u8 cfgsp[256];
static u64 mapped_at;
static int maps;

int pci_count(void) { return ndevs; }
pci_dev_t *pci_get(int i) { return &devs[i]; }
u32 pci_read(pci_dev_t *d, u8 off) { (void)d; u32 v; memcpy(&v, cfgsp + (off & 0xFC), 4); return v; }
void pci_write(pci_dev_t *d, u8 off, u32 v) { (void)d; memcpy(cfgsp + (off & 0xFC), &v, 4); }
void pci_enable_bus_master(pci_dev_t *d) { pci_write(d, 0x04, (pci_read(d, 0x04) | 0x07) & ~(1u << 10)); }
u64 pci_bar_addr(pci_dev_t *d, int i) {   /* igual a kernel/dev/pci.c */
    u32 b = d->bar[i];
    if (b & 1) return b & ~3u;
    u64 a = b & ~15u;
    if (((b >> 1) & 3) == 2 && i < 5) a |= (u64)d->bar[i + 1] << 32;
    return a;
}

void vmm_map_mmio(u64 phys, u64 size) { mapped_at = phys; maps++; (void)size; }
void *dma_alloc(usize bytes, u64 *phys) {
    usize n = (bytes + 4095) & ~(usize)4095;
    void *p = aligned_alloc(4096, n);
    if (!p) return NULL;
    memset(p, 0, n);
    if (phys) *phys = (u64)(uptr)p;
    return p;
}
void sleep_ms(u64 ms) { (void)ms; }
u32 rand32(void) { return 0x123456; }

static char logbuf[8192];
static usize loglen;
void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(logbuf + loglen, sizeof logbuf - loglen, fmt, ap);
    va_end(ap);
    if (n > 0) loglen = loglen + (usize)n < sizeof logbuf ? loglen + (usize)n : sizeof logbuf - 1;
    if (getenv("R8169_VERBOSE")) fputs(logbuf + loglen - (n > 0 ? (usize)n : 0), stdout);
}

static netdev_t *registered;
void net_register_device(netdev_t *d) { registered = d; }

/* ---------- auxiliares do "NIC" ---------- */
typedef struct { u32 opts1, opts2, lo, hi; } desc_t;
#define OWN (1u << 31)
#define EOR (1u << 30)
#define FS  (1u << 29)
#define LS  (1u << 28)
#define RES (1u << 21)

static desc_t *ring_at(u32 lo_reg, u32 hi_reg) { return (desc_t *)(uptr)(((u64)r32(hi_reg) << 32) | r32(lo_reg)); }
static u8 *buf_of(const desc_t *d) { return (u8 *)(uptr)(((u64)d->hi << 32) | d->lo); }

/* o NIC entrega um quadro (com 4 bytes de CRC) no descritor i e devolve-o ao driver */
static void nic_rx(desc_t *rx, int i, const u8 *data, u32 len, u32 extra) {
    memcpy(buf_of(&rx[i]), data, len);
    memset(buf_of(&rx[i]) + len, 0xCC, 4);
    rx[i].opts1 = (rx[i].opts1 & EOR) | FS | LS | extra | (len + 4);
}

static void setup_chip(void) {
    memset(regs, 0, sizeof regs);
    const u8 mac[6] = {0x00, 0xE0, 0x4C, 0x68, 0x01, 0x23};
    memcpy(regs, mac, 6);
    s32(0x40, 0x541u << 20);       /* XID do 8168H */
    regs[0xD3] = 0x80 | 0x02;      /* MCU: OOB ligado, lista ligada pronta */
    s32(0xF0, 1u << 19);           /* RXDV gate fechado, como o firmware deixa */
    regs[0x53] = 0x80; regs[0x56] = 0x01;   /* CLKREQ e ASPM ligados */
    regs[0x37] = 0x0C;
    phy[0xA400] = 0x0800 | 0x1000; /* BMCR: PHY desligado */

    memset(cfgsp, 0, sizeof cfgsp);
    u32 cmd = 0x00100000u;         /* status: lista de capacidades */
    memcpy(cfgsp + 0x04, &cmd, 4);
    cfgsp[0x34] = 0x40;
    cfgsp[0x40] = 0x01; cfgsp[0x41] = 0x70;          /* PM -> PCIe */
    cfgsp[0x70] = 0x10; cfgsp[0x71] = 0x00;          /* PCIe, fim da lista */
    u32 lc = 0x10110143u;                            /* Link Control: ASPM L0s|L1, CLKREQ, RCB */
    memcpy(cfgsp + 0x80, &lc, 4);

    memset(devs, 0, sizeof devs);
    pci_dev_t *d = &devs[0];
    d->vendor = 0x10EC; d->device = 0x8168; d->bus = 2; d->dev = 0; d->fn = 0;
    u64 a = (u64)(uptr)regs, m = (u64)(uptr)msix;
    d->bar[0] = 0xE001;                              /* I/O */
    d->bar[1] = 0;
    d->bar[2] = (u32)a | 0x4;  d->bar[3] = (u32)(a >> 32);   /* MMIO de 64 bits */
    d->bar[4] = (u32)m | 0xC;  d->bar[5] = (u32)(m >> 32);   /* 64 bits prefetchável (MSI-X) */
    ndevs = 1;
}

int main(void) {
    /* sem placa: probe recusa sem tocar em nada */
    ndevs = 0;
    CHECK(!r8169_probe(), "probe sem placa devolveu true");
    CHECK(maps == 0, "mapeou MMIO sem placa");

    setup_chip();
    CHECK(r8169_probe(), "probe falhou");
    CHECK(registered != NULL, "netdev não registrado");
    if (!registered) {   /* o resto depende do netdev */
        printf("r8169_host: %d/%d ok (parou no probe)\n", pass, total);
        return 1;
    }
    CHECK(maps == 1 && mapped_at == (u64)(uptr)regs, "mapeou 0x%llx em vez do BAR2", mapped_at);
    CHECK(foreign == 0, "%d acessos fora do BAR2", foreign);
    CHECK(strstr(logbuf, "XID 0x541") && strstr(logbuf, "RTL8168H") && strstr(logbuf, "BAR2"), "log sem XID/família/BAR: %s", logbuf);
    CHECK(registered && strstr(registered->name, "8168H"), "nome: %s", registered ? registered->name : "?");
    u32 cmd = pci_read(&devs[0], 0x04);
    CHECK((cmd & 0x06) == 0x06 && (cmd & (1u << 10)), "comando PCI 0x%x", cmd);
    CHECK(resets == 1 && reset_reads == 0 && !(regs[0x37] & 0x10), "reset: %d resets, %d leituras pendentes", resets, reset_reads);
    const u8 mac[6] = {0x00, 0xE0, 0x4C, 0x68, 0x01, 0x23};
    CHECK(registered && memcmp(registered->mac, mac, 6) == 0, "MAC lido errado");
    CHECK(r32(0x08) == 0xFFFFFFFF && r32(0x0C) == 0xFFFFFFFF, "MAR0..7 = %08x %08x", r32(0x08), r32(0x0C));
    CHECK(r16(0x3C) == 0, "IntrMask 0x%x", r16(0x3C));
    CHECK(regs[0x37] == 0x0C, "ChipCmd 0x%x", regs[0x37]);
    u32 rxc = r32(0x44);
    CHECK((rxc & 0x3F) == 0x0E && (rxc & (1u << 15)) && (rxc & (1u << 14)) && (rxc & (7u << 8)) == (7u << 8) && (rxc & (1u << 11)),
          "RxConfig 0x%x", rxc);
    u32 txc = r32(0x40);
    CHECK((txc & (3u << 24)) == (3u << 24) && (txc & (7u << 8)) == (7u << 8) && ((txc >> 20) & 0xFCF) == 0x541, "TxConfig 0x%x", txc);
    CHECK(r16(0xDA) == 2048, "RxMaxSize %u", r16(0xDA));
    CHECK(!(r16(0xE0) & (1u << 5)), "RxChkSum ligado");
    CHECK(unlocked_seen >= 1 && locked_cfg_writes == 0 && regs[0x50] == 0x00, "Cfg9346: %d destravas, %d escritas travadas, final 0x%x",
          unlocked_seen, locked_cfg_writes, regs[0x50]);
    CHECK(!(regs[0x53] & 0x80) && !(regs[0x56] & 0x01), "CLKREQ/ASPM do chip ainda ligados");
    u32 lc; memcpy(&lc, cfgsp + 0x80, 4);
    CHECK((lc & 0x103) == 0 && (lc & 0x40), "Link Control 0x%x", lc);
    CHECK(!(r32(0xF0) & (1u << 19)) && !(regs[0xD3] & 0x80), "RXDV gate 0x%x / MCU 0x%x", r32(0xF0), regs[0xD3]);
    CHECK(eri[0xC8 / 4] == 0x00080002 && eri[0xE8 / 4] == 0x00100006 && (eri[0xDC / 4] & 1), "ERI FIFO %x %x %x",
          eri[0xC8 / 4], eri[0xE8 / 4], eri[0xDC / 4]);
    CHECK(phy[0xA400] == 0x1200, "BMCR 0x%x (PHY não foi ligado)", phy[0xA400]);

    desc_t *tx = ring_at(0x20, 0x24), *rx = ring_at(0xE4, 0xE8);
    CHECK(tx && rx && !((uptr)tx & 0xFF) && !((uptr)rx & 0xFF), "anéis desalinhados ou ausentes");
    bool rx_ok = true;
    for (int i = 0; i < 256; i++)
        if (rx[i].opts1 != (OWN | (i == 255 ? EOR : 0) | 2048) || !buf_of(&rx[i])) rx_ok = false;
    CHECK(rx_ok, "anel RX inicial");
    CHECK(!(tx[0].opts1 & OWN) && (tx[63].opts1 & EOR) && !(tx[62].opts1 & EOR), "anel TX inicial");

    /* ---- envio ---- */
    u8 small[20];
    for (int i = 0; i < 20; i++) small[i] = (u8)(0xA0 + i);
    memset(buf_of(&tx[0]), 0x55, 64);   /* lixo que o pad tem de apagar */
    CHECK(registered->send(small, 20), "send falhou");
    CHECK(tx[0].opts1 == (OWN | FS | LS | 60), "opts1 TX 0x%x", tx[0].opts1);
    u8 zeros[40] = {0};
    CHECK(memcmp(buf_of(&tx[0]), small, 20) == 0 && memcmp(buf_of(&tx[0]) + 20, zeros, 40) == 0, "pad a 60");
    CHECK(txpolls == 1 && last_txpoll == 0x40, "TxPoll %d vezes, valor 0x%x", txpolls, last_txpoll);
    CHECK(!registered->send(small, 0) && !registered->send(small, 4000), "aceitou tamanho inválido");

    u8 big[1500];
    for (int i = 0; i < 1500; i++) big[i] = (u8)i;
    bool fill = true;
    for (int i = 1; i < 64; i++) fill &= registered->send(big, 1500);
    CHECK(fill, "não encheu o anel TX");
    CHECK(tx[63].opts1 == (OWN | EOR | FS | LS | 1500) && memcmp(buf_of(&tx[63]), big, 1500) == 0, "último TX 0x%x", tx[63].opts1);
    CHECK(!registered->send(big, 100), "anel cheio aceitou envio");
    tx[0].opts1 &= ~OWN;                 /* o NIC terminou o primeiro */
    CHECK(registered->send(big, 100) && tx[0].opts1 == (OWN | FS | LS | 100), "wrap do TX: 0x%x", tx[0].opts1);
    CHECK(txpolls == 65, "TxPoll %d", txpolls);

    /* ---- recepção ---- */
    u8 got[2048];
    CHECK(registered->recv(got, sizeof got) == 0, "recv com anel vazio");
    u8 f1[100];
    for (int i = 0; i < 100; i++) f1[i] = (u8)(i * 3);
    u16 st = 0x0015; memcpy(regs + 0x3E, &st, 2);
    nic_rx(rx, 0, f1, 100, 0);
    int n = registered->recv(got, sizeof got);
    CHECK(n == 100 && memcmp(got, f1, 100) == 0, "recv leu %d", n);
    CHECK(rx[0].opts1 == (OWN | 2048), "descritor RX não voltou ao NIC: 0x%x", rx[0].opts1);
    CHECK(r16(0x3E) == 0, "IntrStatus não foi limpo: 0x%x", r16(0x3E));

    nic_rx(rx, 1, big, 200, RES);        /* com erro */
    u8 f2[64];
    memset(f2, 0x77, sizeof f2);
    nic_rx(rx, 2, f2, 64, 0);
    n = registered->recv(got, sizeof got);
    CHECK(n == 64 && memcmp(got, f2, 64) == 0, "quadro com erro não foi descartado (%d)", n);
    CHECK(rx[1].opts1 == (OWN | 2048) && rx[2].opts1 == (OWN | 2048), "descritores descartados não voltaram");

    nic_rx(rx, 3, big, 1500, 0);         /* maior que o buffer de quem chama */
    n = registered->recv(got, 1000);
    CHECK(n == 0 && rx[3].opts1 == (OWN | 2048), "quadro grande demais: %d", n);
    nic_rx(rx, 4, big, 300, 0);
    rx[4].opts1 &= ~LS;                  /* fragmento */
    CHECK(registered->recv(got, sizeof got) == 0 && rx[4].opts1 == (OWN | 2048), "fragmento aceito");

    /* avança até o fim do anel e confere o wrap */
    bool loop_ok = true;
    for (int i = 5; i < 256; i++) {
        u8 b[60];
        memset(b, (u8)i, sizeof b);
        nic_rx(rx, i, b, 60, 0);
        if (registered->recv(got, sizeof got) != 60 || got[0] != (u8)i) loop_ok = false;
    }
    CHECK(loop_ok, "recepção sequencial até o fim do anel");
    CHECK(rx[255].opts1 == (OWN | EOR | 2048), "EOR perdido no último RX: 0x%x", rx[255].opts1);
    nic_rx(rx, 0, f1, 80, 0);
    n = registered->recv(got, sizeof got);
    CHECK(n == 80 && memcmp(got, f1, 80) == 0, "wrap do RX leu %d", n);

    /* ---- link ---- */
    regs[0x6C] = 0;
    CHECK(!registered->link_up(), "link sem PHYstatus");
    regs[0x6C] = 0x02;
    CHECK(registered->link_up(), "link com PHYstatus bit 1");
    CHECK(foreign == 0, "%d acessos fora do BAR2 no fim", foreign);

    printf("r8169_host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}

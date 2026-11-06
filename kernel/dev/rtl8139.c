/* mOS — driver Realtek RTL8139 (10EC:8139) por I/O, com polling */
#include "kernel.h"
#include "kernel/net/netpriv.h"

#define R_IDR0    0x00
#define R_TSD0    0x10
#define R_TSAD0   0x20
#define R_RBSTART 0x30
#define R_CR      0x37
#define R_CAPR    0x38
#define R_IMR     0x3C
#define R_ISR     0x3E
#define R_TCR     0x40
#define R_RCR     0x44
#define R_CONFIG1 0x52
#define R_MSR     0x58

#define CR_BUFE   0x01
#define CR_TE     0x04
#define CR_RE     0x08
#define CR_RST    0x10

#define TSD_OWN   (1u << 13)
#define TSD_TOK   (1u << 15)

#define RX_LEN    (32 * 1024)
#define RX_ALLOC  (RX_LEN + 16 + 2048)
#define TX_SLOTS  4
#define TX_SIZE   2048
/* aceita físico, multicast, broadcast; WRAP; buffer de 32K+16; DMA sem limite */
#define RCR_VAL   (0x0Eu | (1u << 7) | (2u << 11) | (7u << 8))

static struct {
    u16 io;
    u8 *rx;
    u8 *tx[TX_SLOTS];
    bool busy[TX_SLOTS];
    u32 tx_cur;
    u32 rx_off;
} R;

static netdev_t rtl_dev;

static bool rtl_link_up(void) { return R.io && !(inb(R.io + R_MSR) & 0x04); }

static bool rtl_send(const void *frame, u16 len) {
    if (!R.io || len > 1792) return false;
    u32 i = R.tx_cur;
    u16 tsd = (u16)(R.io + R_TSD0 + i * 4);
    if (R.busy[i] && !(inl(tsd) & TSD_OWN)) return false;   /* ainda transmitindo */
    memcpy(R.tx[i], frame, len);
    if (len < 60) { memset(R.tx[i] + len, 0, 60 - len); len = 60; }
    outl(R.io + R_TSAD0 + i * 4, (u32)(uptr)R.tx[i]);
    outl(tsd, len);   /* OWN=0 inicia a transmissão; limiar de FIFO mínimo */
    R.busy[i] = true;
    R.tx_cur = (i + 1) % TX_SLOTS;
    return true;
}

static void rx_reset(void) {
    outb(R.io + R_CR, CR_TE);
    outb(R.io + R_CR, CR_TE | CR_RE);
    outl(R.io + R_RCR, RCR_VAL);
    R.rx_off = 0;
    outw(R.io + R_CAPR, (u16)(R.rx_off - 16));
}

static int rtl_recv(void *frame, u16 max) {
    if (!R.io) return 0;
    u16 isr = inw(R.io + R_ISR);
    if (isr) outw(R.io + R_ISR, isr);
    if (isr & 0x0010) { rx_reset(); return 0; }    /* RX overflow */
    if (inb(R.io + R_CR) & CR_BUFE) return 0;
    u8 *h = R.rx + R.rx_off;
    u16 status = (u16)(h[0] | (h[1] << 8));
    u16 len = (u16)(h[2] | (h[3] << 8));
    if (!(status & 1) || len < 64 || len > 1792 + 4) { rx_reset(); return 0; }
    int n = len - 4;   /* sem CRC */
    if (n > max) n = 0;
    else memcpy(frame, h + 4, n);
    R.rx_off = (R.rx_off + len + 4 + 3) & ~3u;
    if (R.rx_off >= RX_LEN) R.rx_off -= RX_LEN;
    outw(R.io + R_CAPR, (u16)(R.rx_off - 16));
    return n;
}

bool rtl8139_probe(void) {
    pci_dev_t *pd = pci_find(0x10EC, 0x8139);
    if (!pd || !(pd->bar[0] & 1)) return false;
    R.io = (u16)pci_bar_addr(pd, 0);
    pci_enable_bus_master(pd);
    pci_write(pd, 0x04, pci_read(pd, 0x04) | (1u << 10));

    u64 phys;
    R.rx = dma_alloc(RX_ALLOC, &phys);
    u8 *txp = dma_alloc(TX_SIZE * TX_SLOTS, NULL);
    if (!R.rx || !txp || phys + RX_ALLOC > 0xFFFFFFFFull || (u64)(uptr)txp + TX_SIZE * TX_SLOTS > 0xFFFFFFFFull) {
        kprintf("rtl8139: memória DMA abaixo de 4 GiB indisponível\n");
        R.io = 0;
        return false;
    }
    for (int i = 0; i < TX_SLOTS; i++) R.tx[i] = txp + i * TX_SIZE;

    outb(R.io + R_CONFIG1, 0);
    outb(R.io + R_CR, CR_RST);
    for (int i = 0; i < 1000 && (inb(R.io + R_CR) & CR_RST); i++) sleep_ms(1);
    outl(R.io + R_RBSTART, (u32)phys);
    outw(R.io + R_IMR, 0);
    outw(R.io + R_ISR, 0xFFFF);
    outl(R.io + R_TCR, 0x03000700);   /* IFG normal, DMA burst 2048 */
    outb(R.io + R_CR, CR_TE | CR_RE);
    /* aceita físico, multicast, broadcast; WRAP; buffer de 32K+16 */
    outl(R.io + R_RCR, RCR_VAL);
    R.rx_off = 0;

    for (int i = 0; i < 6; i++) rtl_dev.mac[i] = inb(R.io + R_IDR0 + i);
    rtl_dev.name = "Realtek RTL8139 (rtl8139)";
    rtl_dev.send = rtl_send;
    rtl_dev.recv = rtl_recv;
    rtl_dev.link_up = rtl_link_up;
    u8 *m = rtl_dev.mac;
    kprintf("rtl8139: I/O 0x%x, MAC %02x:%02x:%02x:%02x:%02x:%02x\n", R.io, m[0], m[1], m[2], m[3], m[4], m[5]);
    net_register_device(&rtl_dev);
    return true;
}

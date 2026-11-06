/* mOS — driver AC'97 (Intel ICH 8086:2415 e compatíveis, classe PCI 04/01)
 *
 * Fallback quando não há HD Audio. Usa as portas de E/S: NAM (BAR0, mixer do
 * codec) e NABM (BAR1, bus master). Saída PCM em 48 kHz/16 bits/estéreo com
 * as 32 entradas da BDL formando um anel; o LVI é empurrado para trás do
 * índice atual a cada serviço, então o DMA nunca para. Sem interrupções.
 */
#include "kernel.h"

void snd_render(i16 *dst, u32 frames);
bool snd_late_pending(void);
void snd_render_late(i16 *dst, u32 frames);
void snd_late_done(void);

/* NAM */
#define NAM_RESET     0x00
#define NAM_MASTER    0x02
#define NAM_HEADPHONE 0x04
#define NAM_PCMOUT    0x18
#define NAM_EXTID     0x28
#define NAM_EXTCTL    0x2A
#define NAM_FRONTRATE 0x2C
#define NAM_VID1      0x7C
#define NAM_VID2      0x7E
/* NABM: caixa de saída PCM em 0x10 */
#define PO_BDBAR 0x10
#define PO_CIV   0x14
#define PO_LVI   0x15
#define PO_SR    0x16
#define PO_PICB  0x18
#define PO_CR    0x1B
#define GLOB_CNT 0x2C
#define GLOB_STA 0x30

#define NBD        32
#define SEG_FRAMES 2048
#define RING_FRAMES (NBD * SEG_FRAMES)  /* 65536 quadros = 1,37 s */
#define RING_MASK   (RING_FRAMES - 1)
#define HALF_RING   (RING_FRAMES / 2)
#define WRITE_AHEAD 9600                 /* 200 ms (inicial) */
#define AHEAD_STEP  4800                 /* +100 ms por underrun */
#define MAX_AHEAD   28800                /* até 600 ms */
#define LATE_GAP    480                  /* sons novos: 10 ms à frente do DMA */
#define RESYNC_GAP  960

static struct {
    u16  nam, nabm;
    i16 *ring;
    u32 *bdl;
    u32  wpos;
    bool running, primed;
    u32  underruns, ahead_target;
} A;

static void spin_ms(u32 ms) {
    u64 end = timer_ticks() + ms + 1;
    u32 s = 0;
    while (timer_ticks() < end && ++s < 20000000u) __asm__ volatile("pause");
}

static u32 play_pos(void) {
    u8 civ = inb(A.nabm + PO_CIV) & 31;
    u16 picb = inw(A.nabm + PO_PICB);          /* amostras de 16 bits restantes no buffer atual */
    u32 done = SEG_FRAMES - MIN((u32)picb / 2, (u32)SEG_FRAMES);
    return (civ * SEG_FRAMES + done) & RING_MASK;
}

static void ring_zero(u32 at, u32 n) {
    while (n) {
        u32 c = MIN(n, RING_FRAMES - at);
        memset(A.ring + at * 2, 0, c * 4);
        at = (at + c) & RING_MASK;
        n -= c;
    }
}

void ac97_service(void) {
    if (!A.running) return;
    u8 civ = inb(A.nabm + PO_CIV) & 31;
    outb(A.nabm + PO_LVI, (civ + NBD - 1) & 31);  /* nunca alcança o último válido */
    u16 sr = inw(A.nabm + PO_SR);
    if (sr & 0x1C) outw(A.nabm + PO_SR, sr & 0x1C);
    if (sr & 1) outb(A.nabm + PO_CR, 0x01);       /* DMA parado (DCH): reinicia */

    u32 play = play_pos();
    u32 ahead = (A.wpos - play) & RING_MASK;
    if (ahead > HALF_RING) {
        if (A.primed) {
            A.ahead_target = MIN(A.ahead_target + AHEAD_STEP, MAX_AHEAD);
            if (A.underruns++ < 8) kprintf("AC97: underrun (escrita %u, DMA %u); folga -> %u ms\n", A.wpos, play, A.ahead_target / 48);
        }
        A.wpos = (play + RESYNC_GAP) & RING_MASK;
        ahead = RESYNC_GAP;
    }
    A.primed = true;
    if (snd_late_pending()) {
        if (ahead > LATE_GAP) {
            u32 at = (play + LATE_GAP) & RING_MASK, n = ahead - LATE_GAP;
            while (n) {
                u32 c = MIN(n, RING_FRAMES - at);
                snd_render_late(A.ring + at * 2, c);
                at = (at + c) & RING_MASK;
                n -= c;
            }
        }
        snd_late_done();
    }
    if (ahead >= A.ahead_target) return;
    u32 todo = A.ahead_target - ahead;
    while (todo) {
        u32 n = MIN(todo, RING_FRAMES - A.wpos);
        snd_render(A.ring + A.wpos * 2, n);
        ring_zero((A.wpos + HALF_RING) & RING_MASK, n);
        A.wpos = (A.wpos + n) & RING_MASK;
        todo -= n;
    }
}

void ac97_set_mute(bool mute) {
    if (!A.running) return;
    outw(A.nam + NAM_MASTER, mute ? 0x8000 : 0x0000);
    outw(A.nam + NAM_HEADPHONE, mute ? 0x8000 : 0x0000);
}

bool ac97_probe(char *name, usize n) {
    pci_dev_t *d = pci_find_class(0x04, 0x01, 0);
    if (!d) return false;
    if (!(d->bar[0] & 1) || !(d->bar[1] & 1)) { kprintf("AC97: BARs de E/S ausentes\n"); return false; }
    memset(&A, 0, sizeof A);
    A.nam = (u16)pci_bar_addr(d, 0);
    A.nabm = (u16)pci_bar_addr(d, 1);
    pci_enable_bus_master(d);
    kprintf("AC97: controlador %04x:%04x, NAM 0x%x, NABM 0x%x\n", d->vendor, d->device, A.nam, A.nabm);

    /* cold reset desativado (bit 1), sem interrupções; espera o codec primário */
    outl(A.nabm + GLOB_CNT, 0x00000002);
    u64 end = timer_ticks() + 300;
    u32 s = 0;
    while (!(inl(A.nabm + GLOB_STA) & (1u << 8)) && timer_ticks() < end && ++s < 5000000u) __asm__ volatile("pause");
    if (!(inl(A.nabm + GLOB_STA) & (1u << 8))) { kprintf("AC97: codec não ficou pronto\n"); return false; }

    outw(A.nam + NAM_RESET, 1);
    spin_ms(20);
    u32 vid = ((u32)inw(A.nam + NAM_VID1) << 16) | inw(A.nam + NAM_VID2);
    outw(A.nam + NAM_MASTER, 0x0000);              /* 0 dB, sem mudo */
    outw(A.nam + NAM_HEADPHONE, 0x0000);
    outw(A.nam + NAM_PCMOUT, 0x0808);              /* 0 dB */
    u16 ext = inw(A.nam + NAM_EXTID);
    if (ext & 1) {                                 /* VRA: fixa 48 kHz explicitamente */
        outw(A.nam + NAM_EXTCTL, inw(A.nam + NAM_EXTCTL) | 1);
        outw(A.nam + NAM_FRONTRATE, 48000);
    }

    u64 rphys, bphys;
    A.ring = dma_alloc(RING_FRAMES * 4, &rphys);
    A.bdl = dma_alloc(NBD * 8, &bphys);
    if (!A.ring || !A.bdl || (rphys >> 32) || (bphys >> 32)) return false;
    for (int i = 0; i < NBD; i++) {
        A.bdl[i * 2] = (u32)(rphys + (u64)i * SEG_FRAMES * 4);
        A.bdl[i * 2 + 1] = SEG_FRAMES * 2;          /* amostras de 16 bits; sem IOC/BUP */
    }
    outb(A.nabm + PO_CR, 0x02);                    /* reset dos registradores da caixa */
    end = timer_ticks() + 20;
    while ((inb(A.nabm + PO_CR) & 0x02) && timer_ticks() < end) __asm__ volatile("pause");
    outl(A.nabm + PO_BDBAR, (u32)bphys);
    outb(A.nabm + PO_LVI, NBD - 1);
    outw(A.nabm + PO_SR, 0x1C);
    outb(A.nabm + PO_CR, 0x01);                    /* RPBM: em execução */
    A.running = true;
    A.wpos = 0;
    A.ahead_target = WRITE_AHEAD;
    kprintf("AC97: codec %04x:%04x, saída PCM 48 kHz 16 bits estéreo em execução (%d x %d quadros)\n",
            vid >> 16, vid & 0xFFFF, NBD, SEG_FRAMES);
    snprintf(name, n, "AC'97 (codec %04x:%04x)", vid >> 16, vid & 0xFFFF);
    return true;
}

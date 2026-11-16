/* mOS — áudio HDMI/DisplayPort da GPU AMD (controlador HDA 1002:15de, codec ATI 1002:aa01)
 *
 * Controlador próprio, separado do analógico (hda.c, 1022:15e3). Este controlador não faz snoop do cache
 * de forma confiável (o Linux o marca "NS" e desliga o snoop no reg. 0x42), então todo buffer de DMA é
 * escrito com clflush depois e o RIRB é invalidado antes de ler.
 *
 * Codec: pares conversor/pino digitais (0x02/0x03, 0x04/0x05, ...). O pino com monitor (pin sense bit 31)
 * e ELD válida (bit 30) recebe o stream; a ELD (verbo F2F) dá o nome do monitor e os formatos LPCM. O
 * áudio sai em 48 kHz, 16 bits, 2 canais, o formato básico que todo receptor HDMI aceita.
 *
 * Dados: quando o analógico roda, o HDMI copia do anel dele só os quadros finais (que já passaram do
 * ponto em que o mixer ainda soma sons atrasados) — mesma mistura nas duas saídas. Sem analógico, o HDMI
 * pede as amostras direto ao mixer.
 */
#include "kernel.h"

void snd_render(i16 *dst, u32 frames);

#define GCAP 0x00
#define GCTL 0x08
#define STATESTS 0x0E
#define INTCTL 0x20
#define CORBLBASE 0x40
#define CORBUBASE 0x44
#define CORBWP 0x48
#define CORBRP 0x4A
#define CORBCTL 0x4C
#define CORBSIZE 0x4E
#define RIRBLBASE 0x50
#define RIRBUBASE 0x54
#define RIRBWP 0x58
#define RINTCNT 0x5A
#define RIRBCTL 0x5C
#define RIRBSTS 0x5D
#define RIRBSIZE 0x5E
#define DPLBASE 0x70
#define SD_BASE 0x80
#define SD_CTL 0x00
#define SD_STS 0x03
#define SD_LPIB 0x04
#define SD_CBL 0x08
#define SD_LVI 0x0C
#define SD_FMT 0x12
#define SD_BDPL 0x18
#define SD_BDPU 0x1C

#define NBDL 4
#define SEG 8192
#define RING (NBDL * SEG)              /* 32768 quadros = 0,68 s */
#define RMASK (RING - 1)
#define AHEAD 2400                     /* 50 ms à frente do DMA */
#define TAG 3
#define FMT 0x0011                     /* 48 kHz, 16 bits, 2 canais */

static struct {
    pci_dev_t *pci;
    volatile u8 *mm;
    u16 gcap;
    u32 *corb, *rirb;
    u16 corb_n, rirb_n, corb_wp, rirb_rp;
    bool dead;
    int cad;
    u8 cvt, pin;
    u32 sd;
    i16 *ring;
    u32 *bdl;
    u32 wpos;              /* próxima escrita no anel HDMI */
    u32 src;               /* próximo quadro do anel analógico a copiar */
    bool src_sync;
    bool running, primary, present;
    char monitor[20];
    u32 underruns;
} M;

static inline u8 r8(u32 o) { return mmio_r8(M.mm, o); }
static inline u16 r16(u32 o) { return mmio_r16(M.mm, o); }
static inline u32 r32(u32 o) { return mmio_r32(M.mm, o); }
static inline void w8(u32 o, u8 v) { mmio_w8(M.mm, o, v); }
static inline void w16(u32 o, u16 v) { mmio_w16(M.mm, o, v); }
static inline void w32(u32 o, u32 v) { mmio_w32(M.mm, o, v); }

static void flush(const void *p, usize n) {
    const u8 *a = (const u8 *)((uptr)p & ~63ull), *e = (const u8 *)p + n;
    for (; a < e; a += 64) __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}

static bool wait_reg(u32 off, int width, u32 mask, u32 val, u32 ms) {
    u64 end = timer_ticks() + ms + 1;
    for (u32 spins = 0;; spins++) {
        u32 v = width == 8 ? r8(off) : width == 16 ? r16(off) : r32(off);
        if ((v & mask) == val) return true;
        if (timer_ticks() > end || spins > 4000000u) return false;
        __asm__ volatile("pause");
    }
}
static void delay_ms(u32 ms) { u64 e = timer_ticks() + ms + 1; for (u32 s = 0; timer_ticks() < e && s < 20000000u; s++) __asm__ volatile("pause"); }

static u32 cmd(u32 c) {
    if (M.dead) return 0;
    u16 wp = (u16)((M.corb_wp + 1) % M.corb_n);
    M.corb[wp] = c;
    flush(&M.corb[wp], 4);
    w16(CORBWP, wp);
    M.corb_wp = wp;
    u64 end = timer_ticks() + 25;
    for (u32 spins = 0;; spins++) {
        u16 rwp = r16(RIRBWP) & 0xFF;
        while (M.rirb_rp != rwp % M.rirb_n) {
            M.rirb_rp = (u16)((M.rirb_rp + 1) % M.rirb_n);
            flush(&M.rirb[M.rirb_rp * 2], 8);         /* invalida antes de ler o que o DMA escreveu */
            u32 r = M.rirb[M.rirb_rp * 2], ex = M.rirb[M.rirb_rp * 2 + 1];
            if (ex & 0x10) continue;
            w8(RIRBSTS, 0x05);
            return r;
        }
        if (timer_ticks() > end || spins > 2000000u) { kprintf("HDMI: codec não respondeu (0x%08x)\n", c); M.dead = true; return 0; }
        __asm__ volatile("pause");
    }
}
static u32 verb(u8 nid, u32 v, u8 d) { return cmd((u32)M.cad << 28 | (u32)(nid & 0x7F) << 20 | (v & 0xFFF) << 8 | d); }
static u32 verb4(u8 nid, u32 v, u16 d) { return cmd((u32)M.cad << 28 | (u32)(nid & 0x7F) << 20 | (v & 0xF) << 16 | d); }
static u32 param(u8 nid, u8 p) { return verb(nid, 0xF00, p); }

static bool ctl_reset(void) {
    int ns = ((M.gcap >> 12) & 15) + ((M.gcap >> 8) & 15) + ((M.gcap >> 3) & 31);
    for (int i = 0; i < ns && i < 30; i++) w8(SD_BASE + i * 0x20 + SD_CTL, 0);
    w8(CORBCTL, 0); w8(RIRBCTL, 0);
    w32(INTCTL, 0); w32(DPLBASE, 0);
    w32(GCTL, r32(GCTL) & ~1u);
    if (!wait_reg(GCTL, 32, 1, 0, 100)) return false;
    delay_ms(1);
    w32(GCTL, r32(GCTL) | 1u);
    if (!wait_reg(GCTL, 32, 1, 1, 100)) return false;
    delay_ms(2);
    return true;
}

static bool corb_setup(void) {
    u64 phys;
    u8 *mem = dma_alloc(4096, &phys);
    if (!mem || (phys >> 32 && !(M.gcap & 1))) return false;
    M.corb = (u32 *)mem; M.rirb = (u32 *)(mem + 1024);
    flush(mem, 4096);
    u8 cs = r8(CORBSIZE), rs = r8(RIRBSIZE);
    u8 csel = (cs & 0x40) ? 2 : (cs & 0x20) ? 1 : 0, rsel = (rs & 0x40) ? 2 : (rs & 0x20) ? 1 : 0;
    M.corb_n = csel == 2 ? 256 : csel == 1 ? 16 : 2;
    M.rirb_n = rsel == 2 ? 256 : rsel == 1 ? 16 : 2;
    w8(CORBSIZE, (u8)((cs & ~3) | csel)); w8(RIRBSIZE, (u8)((rs & ~3) | rsel));
    w32(CORBLBASE, (u32)phys); w32(CORBUBASE, (u32)(phys >> 32));
    w32(RIRBLBASE, (u32)(phys + 1024)); w32(RIRBUBASE, (u32)((phys + 1024) >> 32));
    w16(CORBRP, 0x8000); wait_reg(CORBRP, 16, 0x8000, 0x8000, 5);
    w16(CORBRP, 0); wait_reg(CORBRP, 16, 0x8000, 0, 5);
    w16(CORBWP, 0); M.corb_wp = 0;
    w16(RIRBWP, 0x8000); M.rirb_rp = 0;
    w16(RINTCNT, 1); w8(RIRBSTS, 0x05);
    w8(CORBCTL, 0x02); w8(RIRBCTL, 0x03);
    return wait_reg(CORBCTL, 8, 2, 2, 10);
}

/* ELD (CEA-861 / HDA 7.3.3.36): tamanho pelo parâmetro de pino, bytes pelo verbo F2F */
static void read_eld(u8 pin) {
    M.monitor[0] = 0;
    u32 size = verb(pin, 0xF2E, 0x08) & 0xFF;          /* GET_HDMI_DIP_SIZE, índice 8 = ELD */
    if (!size || size > 256) size = 256;
    u8 eld[256];
    u32 lim = size;
    for (u32 i = 0; i < lim; i++) {
        u32 r = verb(pin, 0xF2F, (u8)i);
        if (!(r & 0x80000000u)) { size = i; break; }   /* bit 31 = byte válido */
        eld[i] = (u8)r;
        size = i + 1;
    }
    if (size < 20) return;
    u32 baseline = (u32)eld[2] * 4;                     /* bloco base em palavras de 4 bytes */
    u32 mnl = eld[4] & 0x1F, sad = (eld[5] >> 4) & 15;
    if (4 + baseline > size || 20 + mnl > size) return;
    u32 n = mnl < sizeof M.monitor - 1 ? mnl : sizeof M.monitor - 1;
    for (u32 i = 0; i < n; i++) { u8 c = eld[20 + i]; M.monitor[i] = (char)(c >= 32 && c < 127 ? c : '?'); }
    M.monitor[n] = 0;
    kprintf("HDMI: ELD de %u bytes, monitor \"%s\", %u descritores de áudio\n", size, M.monitor, sad);
}

/* escolhe o par conversor/pino com monitor e liga o caminho digital */
static bool codec_setup(void) {
    u32 nodes = param(0, 0x04);
    u8 afg = 0;
    for (int i = 0; i < (int)(nodes & 0xFF); i++)
        if ((param((u8)(((nodes >> 16) & 0xFF) + i), 0x05) & 0xFF) == 1) { afg = (u8)(((nodes >> 16) & 0xFF) + i); break; }
    if (!afg) return false;
    verb(afg, 0x705, 0);                               /* AFG em D0 */
    delay_ms(10);
    u32 wn = param(afg, 0x04);
    u8 ws = (u8)((wn >> 16) & 0xFF), wc = (u8)(wn & 0xFF);
    u8 best = 0, any = 0;
    for (int n = ws; n < ws + wc && n < 128; n++) {
        u32 caps = param((u8)n, 0x09);
        if (((caps >> 20) & 0xF) != 4 || !(caps & (1u << 9))) continue;   /* pino digital */
        u32 pcap = param((u8)n, 0x0C);
        if (!(pcap & (1u << 4))) continue;                                  /* capaz de saída */
        if (!any) any = (u8)n;
        verb((u8)n, 0x709, 0);                                              /* dispara o pin sense */
        u32 sense = verb((u8)n, 0xF09, 0);
        if ((sense & 0x80000000u) && (sense & 0x40000000u)) { best = (u8)n; break; }
        if ((sense & 0x80000000u) && !best) best = (u8)n;
    }
    M.present = best != 0;
    u8 pin = best ? best : any;
    if (!pin) return false;
    u32 cl = verb(pin, 0xF02, 0);                                           /* primeira entrada da lista */
    u8 cvt = (u8)(cl & 0x7F);
    if (!cvt) return false;
    M.pin = pin; M.cvt = cvt;
    verb(cvt, 0x705, 0); verb(pin, 0x705, 0);
    verb(pin, 0x701, 0);
    verb4(cvt, 0x2, FMT);
    verb(cvt, 0x72D, 1);                                                    /* 2 canais no conversor */
    verb(cvt, 0x70D, 0x01);                                                 /* DigEn, PCM (não é "non-audio") */
    verb(cvt, 0x706, TAG << 4);
    verb(pin, 0x707, 0x40);                                                 /* saída do pino */
    if (param(pin, 0x09) & (1u << 2)) verb4(pin, 0x3, 0xB000);              /* amp de saída sem mudo */
    verb(pin, 0x771, 0x00);                                                 /* ATI: alocação de canais FL/FR */
    if (M.present) read_eld(pin);
    kprintf("HDMI: pino 0x%02x (%s), conversor 0x%02x\n", pin, M.present ? "monitor presente" : "sem monitor", cvt);
    return !M.dead;
}

static bool stream_setup(void) {
    int iss = (M.gcap >> 8) & 15, oss = (M.gcap >> 12) & 15;
    if (!oss) return false;
    M.sd = SD_BASE + (u32)iss * 0x20;
    u64 rp, bp;
    M.ring = dma_alloc(RING * 4, &rp);
    M.bdl = dma_alloc(4096, &bp);
    if (!M.ring || !M.bdl || (((rp | bp) >> 32) && !(M.gcap & 1))) return false;
    u32 sd = M.sd;
    w8(sd + SD_CTL, 0); wait_reg(sd + SD_CTL, 8, 2, 0, 10);
    w8(sd + SD_CTL, 1); wait_reg(sd + SD_CTL, 8, 1, 1, 10);
    delay_ms(1);
    w8(sd + SD_CTL, 0); wait_reg(sd + SD_CTL, 8, 1, 0, 10);
    w8(sd + SD_STS, 0x1C);
    for (int i = 0; i < NBDL; i++) {
        u64 a = rp + (u64)i * SEG * 4;
        M.bdl[i * 4] = (u32)a; M.bdl[i * 4 + 1] = (u32)(a >> 32); M.bdl[i * 4 + 2] = SEG * 4; M.bdl[i * 4 + 3] = 0;
    }
    flush(M.bdl, 64);
    flush(M.ring, RING * 4);
    w32(sd + SD_BDPL, (u32)bp); w32(sd + SD_BDPU, (u32)(bp >> 32));
    w32(sd + SD_CBL, RING * 4);
    w16(sd + SD_LVI, NBDL - 1);
    w16(sd + SD_FMT, FMT);
    w8(sd + SD_CTL + 2, (u8)((TAG << 4) | (r8(sd + SD_CTL + 2) & 0x0F)));
    w8(sd + SD_CTL, 0x02);
    if (!wait_reg(sd + SD_CTL, 8, 2, 2, 10)) return false;
    M.wpos = AHEAD;
    M.running = true;
    return true;
}

bool hdmi_audio_probe(bool primary) {
    pci_dev_t *d = pci_find(0x1002, 0x15de);
    for (int i = 0; !d; i++) {   /* outras GPUs AMD: qualquer HDA 1002 */
        pci_dev_t *c = pci_find_class(0x04, 0x03, i);
        if (!c) break;
        if (c->vendor == 0x1002) d = c;
    }
    if (!d) return false;
    memset(&M, 0, sizeof M);
    M.pci = d;
    M.primary = primary;
    u64 bar = pci_bar_addr(d, 0);
    if (!bar || (d->bar[0] & 1)) return false;
    vmm_map_mmio(bar, 0x4000);
    M.mm = (volatile u8 *)(uptr)bar;
    pci_enable_bus_master(d);
    u32 v = pci_read(d, 0x40);
    pci_write(d, 0x40, v & ~(7u << 16));               /* reg. 0x42: snoop desligado (o DMA lê a RAM; o kernel faz clflush) */
    M.gcap = r16(GCAP);
    if (M.gcap == 0xFFFF || !ctl_reset()) { kprintf("HDMI: controlador não respondeu\n"); return false; }
    u16 sts = 0;
    for (int i = 0; i < 10 && !sts; i++) { delay_ms(5); sts = r16(STATESTS) & 0x7FFF; }
    w16(STATESTS, sts);
    if (!sts || !corb_setup()) { kprintf("HDMI: sem codec\n"); return false; }
    for (int c = 0; c < 15; c++) {
        if (!(sts & (1u << c))) continue;
        M.cad = c; M.dead = false;
        if (codec_setup() && stream_setup()) {
            kprintf("HDMI: áudio em %02x:%02x.%x, 48 kHz/16 bits/estéreo, %s\n", d->bus, d->dev, d->fn,
                    primary ? "saída principal" : "espelho do analógico");
            return true;
        }
    }
    return false;
}

static u32 play_pos(void) { u32 l = r32(M.sd + SD_LPIB); return (l % (RING * 4)) / 4; }

static void put(const i16 *src, u32 n) {
    while (n) {
        u32 c = MIN(n, RING - M.wpos);
        memcpy(M.ring + M.wpos * 2, src, c * 4);
        flush(M.ring + M.wpos * 2, c * 4);
        M.wpos = (M.wpos + c) & RMASK;
        src += c * 2; n -= c;
    }
}

/* chamado pelo hda.c: quadros [M.src, final) do anel analógico já estão definitivos */
void hdmi_audio_mirror(const i16 *ring, u32 ring_frames, u32 final) {
    if (!M.running || M.primary) return;
    u32 mask = ring_frames - 1;
    u32 play = play_pos();
    u32 ahead = (M.wpos - play) & RMASK;
    if (!M.src_sync || ahead > RING / 2) {               /* começo ou o DMA passou: recomeça 50 ms à frente */
        if (M.src_sync && M.underruns++ < 4) kprintf("HDMI: underrun; ressincronizando\n");
        M.src = final; M.src_sync = true;
        M.wpos = (play + AHEAD) & RMASK;
        return;
    }
    u32 avail = (final - M.src) & mask;
    if (avail > ring_frames / 2) { M.src = final; return; }
    if (ahead + avail > RING / 2) avail = RING / 2 - ahead;   /* não passa do meio anel */
    while (avail) {
        u32 c = MIN(avail, ring_frames - M.src);
        put(ring + M.src * 2, c);
        M.src = (M.src + c) & mask;
        avail -= c;
    }
}

/* sem o analógico: o HDMI é a saída e pede amostras ao mixer */
void hdmi_audio_service(void) {
    if (!M.running || !M.primary) return;
    u8 st = r8(M.sd + SD_STS);
    if (st & 0x1C) w8(M.sd + SD_STS, st & 0x1C);
    u32 play = play_pos(), ahead = (M.wpos - play) & RMASK;
    if (ahead > RING / 2) { M.wpos = (play + 480) & RMASK; ahead = 480; }
    static i16 tmp[2048 * 2];
    while (ahead < AHEAD * 2) {
        u32 n = MIN((u32)2048, AHEAD * 2 - ahead);
        snd_render(tmp, n);
        put(tmp, n);
        ahead += n;
    }
}

void hdmi_audio_set_mute(bool mute) {
    if (!M.running) return;
    M.dead = false;
    if (param(M.pin, 0x09) & (1u << 2)) verb4(M.pin, 0x3, mute ? 0xB080 : 0xB000);
}

bool hdmi_audio_present(void) { return M.running && M.present; }
const char *hdmi_audio_name(void) { return M.monitor[0] ? M.monitor : "HDMI/DisplayPort"; }

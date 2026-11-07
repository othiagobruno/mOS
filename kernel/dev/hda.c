/* mOS — driver Intel High Definition Audio (classe PCI 04/03)
 *
 * Fluxo: reset do controlador (CRST) -> CORB/RIRB (ou interface imediata
 * ICOI/ICII se o anel de comandos não responder) -> enumeração dos codecs
 * (STATESTS) -> varredura da árvore de widgets do grupo de função de áudio
 * (AFG) -> escolha de pinos de saída (line-out / alto-falante / fone) pelas
 * configurações padrão, procurando um conversor de saída (DAC) pelas listas
 * de conexão -> liga energia, desmuta amplificadores, seleciona conexões ->
 * stream de saída 48 kHz/16 bits/estéreo com BDL cíclica.
 *
 * Captura (microfone, senão entrada de linha): o pino de entrada é a fonte,
 * então o caminho segue quem lista cada widget até um conversor de entrada
 * (ADC), sem reprogramar widgets que já estão no caminho de saída. O stream
 * de entrada (tag 2) grava no próprio anel; hda_service copia o que o DMA
 * já escreveu para o mixer (snd_capture). Sem pino ou sem stream de entrada
 * a saída segue sozinha.
 *
 * Sem interrupções: o laço principal chama hda_service(), que lê a posição
 * do DMA (LPIB) e mantém 200-600 ms de áudio à frente (a folga cresce se o
 * laço principal atrasar), pedindo amostras ao mixer (snd_render em
 * sound.c); sons novos são somados ao trecho já escrito a partir de ~10 ms à
 * frente do DMA (snd_render_late), então a latência continua baixa mesmo com
 * buffer grande. Uma faixa de meio anel à frente da escrita
 * é mantida em silêncio, então se o laço principal travar o DMA toca
 * silêncio em vez de repetir áudio velho.
 *
 * Todas as esperas de hardware têm tempo limite: sem dispositivo (ou com um
 * dispositivo que não responde) o boot simplesmente segue sem áudio.
 */
#include "kernel.h"

void snd_render(i16 *dst, u32 frames); /* sound.c: mixer (estéreo intercalado) */
bool snd_late_pending(void);
void snd_render_late(i16 *dst, u32 frames);
void snd_late_done(void);
void snd_capture(const i16 *stereo, u32 frames); /* sound.c: anel da entrada */

/* ---------------------------------------------------------------- registradores */
#define GCAP      0x00
#define VMIN      0x02
#define VMAJ      0x03
#define GCTL      0x08
#define STATESTS  0x0E
#define INTCTL    0x20
#define INTSTS    0x24
#define CORBLBASE 0x40
#define CORBUBASE 0x44
#define CORBWP    0x48
#define CORBRP    0x4A
#define CORBCTL   0x4C
#define CORBSTS   0x4D
#define CORBSIZE  0x4E
#define RIRBLBASE 0x50
#define RIRBUBASE 0x54
#define RIRBWP    0x58
#define RINTCNT   0x5A
#define RIRBCTL   0x5C
#define RIRBSTS   0x5D
#define RIRBSIZE  0x5E
#define ICOI      0x60
#define ICII      0x64
#define ICIS      0x68
#define DPLBASE   0x70
#define DPUBASE   0x74
#define SD_BASE   0x80
/* descritor de stream (offsets relativos) */
#define SD_CTL    0x00 /* 3 bytes */
#define SD_STS    0x03
#define SD_LPIB   0x04
#define SD_CBL    0x08
#define SD_LVI    0x0C
#define SD_FIFOS  0x10
#define SD_FMT    0x12
#define SD_BDPL   0x18
#define SD_BDPU   0x1C

/* verbos de codec */
#define V_GET_PARAM    0xF00
#define V_GET_CONSEL   0xF01
#define V_SET_CONSEL   0x701
#define V_GET_CONLIST  0xF02
#define V_SET_POWER    0x705
#define V_SET_STREAM   0x706
#define V_SET_PINCTL   0x707
#define V_SET_EAPD     0x70C
#define V_GET_CFGDEF   0xF1C
#define V_GET_SENSE    0xF09
#define V4_SET_FMT     0x2
#define V4_SET_AMP     0x3
/* parâmetros */
#define P_VENDOR    0x00
#define P_REV       0x02
#define P_NODES     0x04
#define P_FGTYPE    0x05
#define P_WCAPS     0x09
#define P_PCM       0x0A
#define P_PINCAP    0x0C
#define P_INAMP     0x0D
#define P_CONLEN    0x0E
#define P_OUTAMP    0x12
/* tipos de widget */
enum { WT_OUT = 0, WT_IN, WT_MIX, WT_SEL, WT_PIN, WT_POWER, WT_KNOB, WT_BEEP, WT_VENDOR = 0xF };

/* ---------------------------------------------------------------- buffer de áudio */
#define NBDL        4
#define SEG_FRAMES  16384
#define RING_FRAMES (NBDL * SEG_FRAMES)   /* 65536 quadros = 1,37 s (256 KiB) */
#define RING_MASK   (RING_FRAMES - 1)
#define RING_BYTES  (RING_FRAMES * 4)
#define HALF_RING   (RING_FRAMES / 2)
#define WRITE_AHEAD 9600                  /* 200 ms à frente do DMA (inicial) */
#define AHEAD_STEP  4800                  /* +100 ms a cada underrun... */
#define MAX_AHEAD   28800                 /* ...até 600 ms (< meio anel) */
#define LATE_GAP    480                   /* sons novos entram 10 ms à frente do DMA */
#define RESYNC_GAP  480                   /* 10 ms de margem após um underrun */
#define STREAM_TAG     1
#define STREAM_TAG_IN  2
#define FMT_48K_16_2   0x0011             /* base 48 kHz, 16 bits, 2 canais */
#define FMT_48K_16_1   0x0010             /* o mesmo, 1 canal */

#define MAX_NID   128
#define MAX_CONN  16
#define MAX_DACS  4
#define MAX_PATH  8

typedef struct {
    u32 caps, pincap, cfg;
    u8  type, nconn;
    u8  conn[MAX_CONN];
} widget_t;

static struct {
    pci_dev_t    *pci;
    volatile u8  *mm;
    u16           gcap;
    /* comandos */
    u32          *corb;
    u32          *rirb;
    u32           corb_n, rirb_n;
    u16           corb_wp, rirb_rp;
    bool          immediate, dead;
    /* codec */
    int           cad;
    u32           codec_id, codec_rev;
    u8            afg, wstart, wcount;
    widget_t      w[MAX_NID];
    u8            dacs[MAX_DACS];
    int           ndacs;
    u8            outpins[8];
    int           noutpins;
    u8            on_play[MAX_NID];       /* widgets do caminho de saída: a captura não os mexe */
    /* stream de entrada */
    u32           in_sd;
    i16          *in_ring;
    u32          *in_bdl;
    u32           in_rpos, in_first;
    bool          in_running, in_saw, in_mono;
    const char   *in_name;
    /* stream */
    u32           sd;
    i16          *ring;
    u32          *bdl;
    u32          *posbuf;
    u32           wpos;
    u32           underruns, ahead_target;
    u32           first_lpib;
    bool          running, saw_motion, primed;
} H;

/* ---------------------------------------------------------------- MMIO / tempo */
static inline u8  r8(u32 o)  { return mmio_r8(H.mm, o); }
static inline u16 r16(u32 o) { return mmio_r16(H.mm, o); }
static inline u32 r32(u32 o) { return mmio_r32(H.mm, o); }
static inline void w8(u32 o, u8 v)   { mmio_w8(H.mm, o, v); }
static inline void w16(u32 o, u16 v) { mmio_w16(H.mm, o, v); }
static inline void w32(u32 o, u32 v) { mmio_w32(H.mm, o, v); }
static inline void cpu_relax(void) { __asm__ volatile("pause" ::: "memory"); }

/* espera ocupada com dois limites: relógio (ms) e número de voltas
 * (caso as interrupções estejam desligadas e o relógio não ande) */
static void delay_ms(u32 ms) {
    u64 end = timer_ticks() + ms + 1;
    u32 spins = 0;
    while (timer_ticks() < end && ++spins < 20000000u) cpu_relax();
}

static bool wait_reg(u32 off, int width, u32 mask, u32 val, u32 ms) {
    u64 end = timer_ticks() + ms + 1;
    u32 spins = 0;
    for (;;) {
        u32 v = width == 8 ? r8(off) : width == 16 ? r16(off) : r32(off);
        if ((v & mask) == val) return true;
        if (timer_ticks() > end || ++spins > 4000000u) return false;
        cpu_relax();
    }
}

/* ---------------------------------------------------------------- comandos */
static bool imm_cmd(u32 cmd, u32 *resp) {
    if (!wait_reg(ICIS, 16, 1, 0, 10)) return false;      /* ICB livre */
    w16(ICIS, 2);                                         /* limpa IRV (RW1C) */
    w32(ICOI, cmd);
    w16(ICIS, 1);                                         /* ICB: envia */
    if (!wait_reg(ICIS, 16, 3, 2, 20)) return false;      /* ICB=0, IRV=1 */
    *resp = r32(ICII);
    w16(ICIS, 2);
    return true;
}

static bool corb_cmd(u32 cmd, u32 *resp) {
    u16 wp = (H.corb_wp + 1) % H.corb_n;
    H.corb[wp] = cmd;
    __asm__ volatile("sfence" ::: "memory");
    w16(CORBWP, wp);
    H.corb_wp = wp;
    u64 end = timer_ticks() + 25;
    u32 spins = 0;
    for (;;) {
        u16 rwp = r16(RIRBWP) & 0xFF;
        while (H.rirb_rp != rwp % H.rirb_n) {
            H.rirb_rp = (H.rirb_rp + 1) % H.rirb_n;
            u32 r = H.rirb[H.rirb_rp * 2], ex = H.rirb[H.rirb_rp * 2 + 1];
            if (ex & 0x10) continue;                      /* resposta não solicitada */
            w8(RIRBSTS, 0x05);
            *resp = r;
            return true;
        }
        if (timer_ticks() > end || ++spins > 2000000u) return false;
        cpu_relax();
    }
}

static void corb_stop(void) {
    w8(CORBCTL, 0);
    w8(RIRBCTL, 0);
    wait_reg(CORBCTL, 8, 2, 0, 10);
    wait_reg(RIRBCTL, 8, 2, 0, 10);
}

static u32 hda_cmd(u32 cmd) {
    u32 r = 0;
    if (H.dead) return 0;
    if (!H.immediate) {
        if (corb_cmd(cmd, &r)) return r;
        kprintf("HDA: CORB/RIRB não respondeu (cmd 0x%08x); usando interface imediata\n", cmd);
        corb_stop();
        H.immediate = true;
    }
    if (imm_cmd(cmd, &r)) return r;
    kprintf("HDA: codec não respondeu ao comando 0x%08x\n", cmd);
    H.dead = true;
    return 0;
}

static u32 verb(u8 nid, u32 v, u8 data) {
    return hda_cmd(((u32)H.cad << 28) | ((u32)(nid & 0x7F) << 20) | ((v & 0xFFF) << 8) | data);
}
static u32 verb4(u8 nid, u32 v, u16 data) {
    return hda_cmd(((u32)H.cad << 28) | ((u32)(nid & 0x7F) << 20) | ((v & 0xF) << 16) | data);
}
static u32 param(u8 nid, u8 p) { return verb(nid, V_GET_PARAM, p); }

/* ---------------------------------------------------------------- controlador */
static bool controller_reset(void) {
    /* para DMA de streams e do anel de comandos antes do reset */
    int nstreams = ((H.gcap >> 12) & 15) + ((H.gcap >> 8) & 15) + ((H.gcap >> 3) & 31);
    for (int i = 0; i < nstreams && i < 30; i++) w8(SD_BASE + i * 0x20 + SD_CTL, 0);
    corb_stop();
    w32(INTCTL, 0);
    w32(DPLBASE, 0);

    w32(GCTL, r32(GCTL) & ~1u);                           /* entra em reset */
    if (!wait_reg(GCTL, 32, 1, 0, 100)) { kprintf("HDA: CRST não baixou\n"); return false; }
    delay_ms(1);
    w32(GCTL, r32(GCTL) | 1u);                            /* sai do reset */
    if (!wait_reg(GCTL, 32, 1, 1, 100)) { kprintf("HDA: CRST não subiu\n"); return false; }
    delay_ms(2);                                          /* >= 521 us para os codecs se anunciarem */
    w32(GCTL, r32(GCTL) | (1u << 8));                     /* aceita respostas não solicitadas (descartadas) */
    return true;
}

static bool corb_setup(void) {
    u64 phys;
    u8 *mem = dma_alloc(4096, &phys);                     /* CORB 1 KiB + RIRB 2 KiB na mesma página */
    if (!mem) return false;
    if (phys >> 32 && !(H.gcap & 1)) { kprintf("HDA: controlador sem 64 bits e DMA acima de 4 GiB\n"); return false; }
    H.corb = (u32 *)mem;
    H.rirb = (u32 *)(mem + 1024);

    u8 cs = r8(CORBSIZE), rs = r8(RIRBSIZE);
    u8 csel = (cs & 0x40) ? 2 : (cs & 0x20) ? 1 : 0;
    u8 rsel = (rs & 0x40) ? 2 : (rs & 0x20) ? 1 : 0;
    H.corb_n = csel == 2 ? 256 : csel == 1 ? 16 : 2;
    H.rirb_n = rsel == 2 ? 256 : rsel == 1 ? 16 : 2;
    w8(CORBSIZE, (cs & ~3) | csel);
    w8(RIRBSIZE, (rs & ~3) | rsel);

    w32(CORBLBASE, (u32)phys);
    w32(CORBUBASE, (u32)(phys >> 32));
    w32(RIRBLBASE, (u32)(phys + 1024));
    w32(RIRBUBASE, (u32)((phys + 1024) >> 32));

    /* reset do ponteiro de leitura do CORB (alguns controladores não ecoam o bit: ignora) */
    w16(CORBRP, 0x8000);
    wait_reg(CORBRP, 16, 0x8000, 0x8000, 5);
    w16(CORBRP, 0);
    wait_reg(CORBRP, 16, 0x8000, 0, 5);
    w16(CORBWP, 0);
    H.corb_wp = 0;

    w16(RIRBWP, 0x8000);                                  /* reset do ponteiro de escrita do RIRB */
    H.rirb_rp = 0;
    w16(RINTCNT, 1);
    w8(RIRBSTS, 0x05);

    w8(CORBCTL, 0x02);                                    /* CORB DMA em execução */
    w8(RIRBCTL, 0x03);                                    /* RIRB DMA + RINTCTL (só o bit de status: INTCTL=0) */
    wait_reg(CORBCTL, 8, 2, 2, 10);
    return true;
}

/* ---------------------------------------------------------------- árvore de widgets */
static const char *dev_name(u32 cfg) {
    static const char *n[16] = {"line-out", "alto-falante", "fone", "CD", "S/PDIF", "digital", "modem",
                                "fone-modem", "line-in", "aux", "microfone", "telefonia", "S/PDIF-in",
                                "digital-in", "reservado", "outro"};
    return n[(cfg >> 20) & 15];
}

static void read_conn_list(u8 nid) {
    widget_t *w = &H.w[nid];
    w->nconn = 0;
    if (!(w->caps & (1u << 8))) return;                   /* sem lista de conexões */
    u32 cl = param(nid, P_CONLEN);
    bool lng = cl & 0x80;
    int len = cl & 0x7F;
    int per = lng ? 2 : 4, bits = lng ? 16 : 8;
    u32 prev = 0;
    for (int i = 0; i < len && w->nconn < MAX_CONN; i += per) {
        u32 r = verb(nid, V_GET_CONLIST, (u8)i);
        for (int j = 0; j < per && i + j < len && w->nconn < MAX_CONN; j++) {
            u32 e = (r >> (j * bits)) & ((1u << bits) - 1);
            bool range = e & (1u << (bits - 1));
            u32 id = e & ((1u << (bits - 1)) - 1);
            if (range && prev && id > prev) {
                for (u32 k = prev + 1; k <= id && w->nconn < MAX_CONN; k++) w->conn[w->nconn++] = (u8)k;
            } else w->conn[w->nconn++] = (u8)id;
            prev = id;
        }
    }
}

static bool is_analog_dac(u8 nid) {
    widget_t *w = &H.w[nid];
    return w->type == WT_OUT && !(w->caps & (1u << 9)); /* bit 9: digital */
}

/* busca em profundidade de um pino até um DAC; path[0]=pino ... path[n-1]=DAC,
 * idx[i] = índice da conexão usada em path[i]. want=0: qualquer DAC. */
static int dfs(u8 nid, int depth, u8 want, u8 *path, u8 *idx, u8 *visited) {
    if (nid >= MAX_NID || depth >= MAX_PATH || visited[nid]) return 0;
    widget_t *w = &H.w[nid];
    path[depth] = nid;
    if (depth > 0 && is_analog_dac(nid)) return (!want || nid == want) ? depth + 1 : 0;
    if (depth > 0 && w->type != WT_MIX && w->type != WT_SEL) return 0; /* só atravessa misturadores/seletores */
    visited[nid] = 1;
    /* prefere a conexão atualmente selecionada (o firmware costuma deixar algo sensato) */
    int cur = (w->type != WT_MIX && w->nconn > 1) ? (int)(verb(nid, V_GET_CONSEL, 0) & 0xFF) : 0;
    for (int k = 0; k < w->nconn; k++) {
        int i = (k == 0) ? cur : (k <= cur ? k - 1 : k);
        if (i >= w->nconn) continue;
        idx[depth] = (u8)i;
        int n = dfs(w->conn[i], depth + 1, want, path, idx, visited);
        if (n) { visited[nid] = 0; return n; }
    }
    visited[nid] = 0;
    return 0;
}

static u32 amp_caps(u8 nid, bool out) {
    widget_t *w = &H.w[nid];
    u8 src = (w->caps & (1u << 3)) ? nid : H.afg;         /* AmpParOvr */
    return param(src, out ? P_OUTAMP : P_INAMP);
}

/* ganho de 0 dB (offset), limitado ao número de passos; o volume é feito no mixer */
static u16 amp_gain(u32 caps) {
    u32 steps = (caps >> 8) & 0x7F, off = caps & 0x7F;
    return (u16)(off > steps ? steps : off);
}

/* ganho db acima de 0 dB (o offset), limitado ao último passo. O campo de
 * passo é em unidades de 0,25 dB e o 0 do hardware vale 0,25 dB. */
static u16 amp_boost(u32 caps, int db) {
    u32 steps = (caps >> 8) & 0x7F, off = caps & 0x7F;
    u32 q = ((caps >> 16) & 0x7F) + 1;
    u32 g = off + (u32)db * 4 / q;
    if (g > steps) g = steps;
    return (u16)g;
}

static void power_up(u8 nid) {
    if (H.w[nid].caps & (1u << 10)) verb(nid, V_SET_POWER, 0); /* D0 */
}

static void setup_path(u8 *path, u8 *idx, int n) {
    for (int i = 0; i < n; i++) {
        u8 nid = path[i];
        widget_t *w = &H.w[nid];
        H.on_play[nid] = 1;
        power_up(nid);
        if (w->caps & (1u << 2)) {                        /* amplificador de saída: desmuta, 0 dB */
            u32 c = amp_caps(nid, true);
            verb4(nid, V4_SET_AMP, 0xB000 | amp_gain(c));
        }
        if (i == n - 1) break;                            /* DAC: sem entradas */
        if (w->type == WT_MIX) {
            /* misturador: desmuta só a entrada do caminho (evita loopback de microfone etc.) */
            if (w->caps & (1u << 1)) {
                u32 c = amp_caps(nid, false);
                for (int k = 0; k < w->nconn; k++)
                    verb4(nid, V4_SET_AMP, 0x7000 | (k << 8) | (k == idx[i] ? amp_gain(c) : 0x80));
            }
        } else {
            if (w->nconn > 1) verb(nid, V_SET_CONSEL, idx[i]);
            if ((w->caps & (1u << 1)) && w->type != WT_PIN) {
                u32 c = amp_caps(nid, false);
                verb4(nid, V4_SET_AMP, 0x7000 | (idx[i] << 8) | amp_gain(c));
            }
        }
    }
    /* pino: habilita saída (+ amplificador de fone se aplicável) e EAPD */
    u8 pin = path[0];
    widget_t *p = &H.w[pin];
    u8 ctl = 0x40;
    if (((p->cfg >> 20) & 15) == 2 && (p->pincap & (1u << 3))) ctl |= 0x80;
    verb(pin, V_SET_PINCTL, ctl);
    if (p->pincap & (1u << 16)) verb(pin, V_SET_EAPD, 0x02);
}

static void add_dac(u8 d) {
    for (int i = 0; i < H.ndacs; i++) if (H.dacs[i] == d) return;
    if (H.ndacs < MAX_DACS) H.dacs[H.ndacs++] = d;
}

static int pin_rank(widget_t *w, bool strict) {
    u32 cfg = w->cfg;
    u32 conn = cfg >> 30, dev = (cfg >> 20) & 15;
    if (!(w->pincap & (1u << 4))) return -1;              /* não é capaz de saída */
    if (w->caps & (1u << 9)) return -1;                   /* pino digital (HDMI/DP/S-PDIF) */
    if (!strict) return 1;
    if (conn == 1) return -1;                             /* sem conexão física */
    if (dev == 0) return 3;                               /* line-out */
    if (dev == 1) return 2;                               /* alto-falante */
    if (dev == 2) return 1;                               /* fone */
    return -1;
}

/* escolhe pinos e DACs; devolve o número de pinos configurados */
static int build_paths(bool strict) {
    H.ndacs = 0; H.noutpins = 0;
    memset(H.on_play, 0, sizeof H.on_play);
    for (int rank = 3; rank >= 1; rank--) {
        for (int n = H.wstart; n < H.wstart + H.wcount && n < MAX_NID; n++) {
            widget_t *w = &H.w[n];
            if (w->type != WT_PIN || pin_rank(w, strict) != (strict ? rank : 1)) continue;
            if (!strict && rank != 3) continue;
            u8 path[MAX_PATH], idx[MAX_PATH], visited[MAX_NID];
            memset(visited, 0, sizeof visited);
            /* tenta primeiro um DAC já escolhido, para compartilhar */
            int len = 0;
            for (int d = 0; d < H.ndacs && !len; d++) len = dfs((u8)n, 0, H.dacs[d], path, idx, visited);
            if (!len) len = dfs((u8)n, 0, 0, path, idx, visited);
            if (!len) { kprintf("HDA:   pino 0x%02x (%s): nenhum DAC alcançável\n", n, dev_name(w->cfg)); continue; }
            setup_path(path, idx, len);
            add_dac(path[len - 1]);
            if (H.noutpins < 8) H.outpins[H.noutpins++] = (u8)n;
            char buf[96];
            int o = 0;
            for (int i = 0; i < len && o < (int)sizeof buf - 8; i++)
                o += snprintf(buf + o, sizeof buf - o, i ? " -> 0x%02x" : "0x%02x", path[i]);
            kprintf("HDA:   pino 0x%02x %s/%s (cfg 0x%08x): %s (DAC)\n", n, dev_name(w->cfg),
                    (w->cfg >> 30) == 0 ? "conector" : (w->cfg >> 30) == 2 ? "fixo" : "ambos", w->cfg, buf);
        }
    }
    return H.noutpins;
}

static bool codec_probe(int cad) {
    H.cad = cad;
    H.dead = false;
    u32 vid = param(0, P_VENDOR);
    if (H.dead || vid == 0 || vid == 0xFFFFFFFF) return false;
    H.codec_id = vid;
    H.codec_rev = param(0, P_REV);
    u32 nodes = param(0, P_NODES);
    int fs = (nodes >> 16) & 0xFF, fc = nodes & 0xFF;
    H.afg = 0;
    for (int i = 0; i < fc; i++) {
        u32 t = param((u8)(fs + i), P_FGTYPE) & 0xFF;
        if (t == 1) { H.afg = (u8)(fs + i); break; }      /* 1 = áudio, 2 = modem */
    }
    kprintf("HDA: codec %d: %04x:%04x rev %08x, AFG %s0x%02x\n", cad, vid >> 16, vid & 0xFFFF, H.codec_rev,
            H.afg ? "" : "ausente ", H.afg);
    if (!H.afg) return false;

    verb(H.afg, V_SET_POWER, 0);                          /* AFG em D0 */
    delay_ms(10);
    u32 wn = param(H.afg, P_NODES);
    H.wstart = (wn >> 16) & 0xFF;
    H.wcount = wn & 0xFF;
    if (!H.wcount || H.wstart >= MAX_NID) return false;
    if (H.wstart + H.wcount > MAX_NID) H.wcount = MAX_NID - H.wstart;
    memset(H.w, 0, sizeof H.w);
    int ndac = 0, npin = 0;
    for (int n = H.wstart; n < H.wstart + H.wcount; n++) {
        widget_t *w = &H.w[n];
        w->caps = param((u8)n, P_WCAPS);
        w->type = (w->caps >> 20) & 0xF;
        if (w->type == WT_PIN) {
            w->pincap = param((u8)n, P_PINCAP);
            w->cfg = verb((u8)n, V_GET_CFGDEF, 0);
            npin++;
        }
        if (is_analog_dac((u8)n)) ndac++;
        read_conn_list((u8)n);
        if (H.dead) return false;
    }
    kprintf("HDA: AFG 0x%02x: widgets 0x%02x-0x%02x, %d DAC(s), %d pino(s)\n", H.afg, H.wstart,
            H.wstart + H.wcount - 1, ndac, npin);
    if (!ndac) return false;
    for (int n = H.wstart; n < H.wstart + H.wcount; n++) power_up((u8)n);
    if (!build_paths(true)) {
        kprintf("HDA: configurações padrão sem saída utilizável; tentando qualquer pino de saída\n");
        if (!build_paths(false)) return false;
    }
    return H.ndacs > 0;
}

/* ---------------------------------------------------------------- captura */
static bool is_analog_adc(u8 nid) {
    widget_t *w = &H.w[nid];
    return w->type == WT_IN && !(w->caps & (1u << 9));
}

static const char *input_label(u32 cfg) {
    u32 dev = (cfg >> 20) & 15, conn = cfg >> 30;
    if (dev == 10 && conn == 2) return "Microfone interno";
    if (dev == 10) return "Microfone";
    if (dev == 8) return "Entrada de linha";
    if (dev == 9) return "Entrada auxiliar";
    return "Entrada";
}

/* maior = preferido. Jack com presença detectada passa na frente do microfone interno. */
static int pin_in_rank(u8 nid) {
    widget_t *w = &H.w[nid];
    if (w->type != WT_PIN) return -1;
    u32 conn = w->cfg >> 30, dev = (w->cfg >> 20) & 15;
    if (!(w->pincap & (1u << 5))) return -1;              /* não é capaz de entrada */
    if (w->caps & (1u << 9)) return -1;                   /* pino digital */
    if (conn == 1) return -1;                             /* sem conexão física */
    int rank = dev == 10 ? 4 : dev == 8 ? 3 : dev == 9 ? 2 : -1;
    if (rank < 0) return -1;
    if (dev == 10 && conn == 2) rank = 5;                 /* microfone de placa */
    if (conn != 2 && (w->pincap & (1u << 2))) {
        bool dead = H.dead;
        u32 s = verb(nid, V_GET_SENSE, 0);
        if (H.dead && !dead) H.dead = false;              /* pino sem sentido: ignora a presença */
        else if (s & 0x80000000u) rank += 3;              /* plugado agora */
    }
    return rank;
}

/* quem tem `src` na lista de conexões: o sinal sai de src e entra nesse widget.
 * idx = índice dessa conexão. */
static int fed_by(u8 src, u8 *dsts, u8 *idxs, int max) {
    int n = 0;
    for (int i = H.wstart; i < H.wstart + H.wcount && i < MAX_NID; i++) {
        widget_t *w = &H.w[i];
        for (int k = 0; k < w->nconn; k++) {
            if (w->conn[k] != src) continue;
            if (n < max) { dsts[n] = (u8)i; idxs[n] = (u8)k; n++; }
            break;
        }
    }
    return n;
}

/* path[0] = pino, path[n-1] = ADC. idx[i] = conexão de path[i+1] que seleciona path[i]. */
static int dfs_cap(u8 nid, int depth, u8 *path, u8 *idx, u8 *visited) {
    if (nid >= MAX_NID || depth >= MAX_PATH || visited[nid]) return 0;
    path[depth] = nid;
    if (depth > 0 && is_analog_adc(nid)) return depth + 1;
    if (depth > 0 && (H.on_play[nid] || H.w[nid].type == WT_OUT)) return 0;
    visited[nid] = 1;
    u8 dsts[MAX_CONN], ixs[MAX_CONN];
    int n = fed_by(nid, dsts, ixs, MAX_CONN);
    for (int i = 0; i < n; i++) {
        idx[depth] = ixs[i];
        int len = dfs_cap(dsts[i], depth + 1, path, idx, visited);
        if (len) { visited[nid] = 0; return len; }
    }
    visited[nid] = 0;
    return 0;
}

static bool adc_pcm_ok(u8 adc) {
    bool dead = H.dead;
    u32 pcm = (H.w[adc].caps & (1u << 4)) ? param(adc, P_PCM) : param(H.afg, P_PCM);
    if (H.dead && !dead) { H.dead = false; return true; } /* leitura falhou: tenta assim mesmo */
    if (!pcm || pcm == 0xFFFFFFFFu) return true;
    return (pcm & (1u << 6)) && (pcm & (1u << 17));       /* 48 kHz e 16 bits */
}

static u8 mic_vref(widget_t *p) {
    if (p->pincap & (1u << 11)) return 4;                 /* 80% */
    if (p->pincap & (1u << 12)) return 5;                 /* 100% */
    if (p->pincap & (1u << 9)) return 1;                  /* 50% */
    return 0;
}

static void setup_capture_path(u8 *path, u8 *idx, int n) {
    for (int i = 0; i < n; i++) power_up(path[i]);
    u8 pin = path[0];
    widget_t *p = &H.w[pin];
    u32 dev = (p->cfg >> 20) & 15;
    u8 ctl = 0x20;                                        /* habilita entrada */
    if (dev == 10) ctl |= mic_vref(p);                    /* polarização de eletreto */
    verb(pin, V_SET_PINCTL, ctl);

    bool boosted = false;
    if (p->caps & (1u << 1)) {                            /* boost no próprio pino, se houver */
        u32 c = amp_caps(pin, false);
        verb4(pin, V4_SET_AMP, 0x7000 | amp_boost(c, 20));
        boosted = true;
    }
    for (int i = 0; i < n - 1; i++) {
        u8 dst = path[i + 1];
        widget_t *w = &H.w[dst];
        u8 sel = idx[i];
        bool boost = !boosted;                            /* +20 dB no primeiro estágio que tiver amplificador */
        if (w->nconn > 1 && w->type != WT_MIX) verb(dst, V_SET_CONSEL, sel);
        if (w->caps & (1u << 1)) {
            u32 c = amp_caps(dst, false);
            u16 g = boost ? amp_boost(c, 20) : amp_gain(c);
            if (w->type == WT_MIX) {
                for (int k = 0; k < w->nconn; k++)
                    verb4(dst, V4_SET_AMP, 0x7000 | (k << 8) | (k == sel ? g : 0x80));
            } else verb4(dst, V4_SET_AMP, 0x7000 | (sel << 8) | g);
            if (boost) boosted = true;
        }
        if (i + 1 == n - 1 && (w->caps & (1u << 2))) {    /* saída do ADC em 0 dB */
            u32 c = amp_caps(dst, true);
            verb4(dst, V4_SET_AMP, 0xB000 | amp_gain(c));
        }
    }
}

static u32 pick_input_sd(void) {
    int iss = (H.gcap >> 8) & 15, oss = (H.gcap >> 12) & 15, bss = (H.gcap >> 3) & 31;
    if (iss) return SD_BASE;                              /* primeiro stream de entrada */
    u32 b0 = SD_BASE + (u32)(iss + oss) * 0x20;           /* primeiro bidirecional */
    if (bss && b0 != H.sd) return b0;
    if (bss > 1 && b0 + 0x20 != H.sd) return b0 + 0x20;
    return 0;
}

static void emit_capture(u32 at, u32 n) {
    if (!H.in_mono) { snd_capture(H.in_ring + at * 2, n); return; }
    static i16 tmp[128 * 2];
    while (n) {
        u32 c = MIN(n, 128);
        for (u32 i = 0; i < c; i++) {
            i16 s = H.in_ring[at + i];
            tmp[i * 2] = tmp[i * 2 + 1] = s;
        }
        snd_capture(tmp, c);
        at += c;
        n -= c;
    }
}

static void capture_service(void) {
    if (!H.in_running) return;
    u32 sd = H.in_sd;
    u8 st = r8(sd + SD_STS);
    if (st & 0x1C) w8(sd + SD_STS, st & 0x1C);
    u32 bpf = H.in_mono ? 2u : 4u;
    u32 ring_bytes = RING_FRAMES * bpf;
    u32 lpib_reg = r32(sd + SD_LPIB);
    __asm__ volatile("lfence" ::: "memory");
    /* o buffer de posição (já ligado pela saída) acompanha a captura melhor
     * que o LPIB em vários controladores Intel; cai para o LPIB se vier zerado */
    u32 sdi = (sd - SD_BASE) / 0x20;
    u32 pos = H.posbuf ? ((volatile u32 *)H.posbuf)[sdi * 2] : 0;
    u32 lpib = (pos && pos < ring_bytes) ? pos : lpib_reg;
    if (lpib >= ring_bytes) lpib %= ring_bytes;
    u32 cap = (lpib / bpf) & RING_MASK;
    if (!H.in_saw && lpib != H.in_first) {
        H.in_saw = true;
        kprintf("HDA: DMA de entrada ativo (LPIB %u -> %u)\n", H.in_first, lpib);
    }
    u32 avail = (cap - H.in_rpos) & RING_MASK;
    if (avail <= 48) return;
    if (avail > HALF_RING) {                              /* o DMA deu a volta: fica com o trecho recente */
        H.in_rpos = (cap - 480) & RING_MASK;
        avail = 480;
        static int nov;
        if (nov++ < 4) kprintf("HDA: overrun da captura; descartando áudio antigo\n");
    }
    u32 todo = avail - 48;                                /* não lê o quadro que o DMA ainda escreve */
    while (todo) {
        u32 c = MIN(todo, RING_FRAMES - H.in_rpos);
        emit_capture(H.in_rpos, c);
        H.in_rpos = (H.in_rpos + c) & RING_MASK;
        todo -= c;
    }
}

static bool capture_setup(void) {
    H.in_running = false;
    typedef struct { u8 nid; int rank; } cand_t;
    cand_t cands[16];
    int nc = 0;
    for (int n = H.wstart; n < H.wstart + H.wcount && n < MAX_NID; n++) {
        int rank = pin_in_rank((u8)n);
        if (rank < 0 || nc >= 16) continue;
        int i = nc++;
        while (i > 0 && cands[i - 1].rank < rank) { cands[i] = cands[i - 1]; i--; }
        cands[i].nid = (u8)n;
        cands[i].rank = rank;
    }
    if (!nc) { kprintf("HDA: nenhum pino de entrada (microfone ou linha)\n"); return false; }

    u8 path[MAX_PATH], idx[MAX_PATH];
    int len = 0;
    for (int i = 0; i < nc && !len; i++) {
        u8 visited[MAX_NID];
        memset(visited, 0, sizeof visited);
        len = dfs_cap(cands[i].nid, 0, path, idx, visited);
        if (!len) {
            kprintf("HDA:   pino 0x%02x (%s): nenhum ADC alcançável\n", cands[i].nid, dev_name(H.w[cands[i].nid].cfg));
            continue;
        }
        if (!adc_pcm_ok(path[len - 1])) {
            kprintf("HDA:   ADC 0x%02x sem PCM 48 kHz/16 bits\n", path[len - 1]);
            len = 0;
        }
    }
    if (!len) return false;

    u32 sd = pick_input_sd();
    if (!sd) { kprintf("HDA: controlador sem stream de entrada livre\n"); return false; }

    bool dead = H.dead;
    setup_capture_path(path, idx, len);
    if (H.dead && !dead) {
        kprintf("HDA: captura abortada (codec parou de responder)\n");
        H.dead = false;
        return false;
    }
    u8 adc = path[len - 1];
    u32 caps = H.w[adc].caps;
    u32 ch = ((((caps >> 13) & 7) << 1) | (caps & 1)) + 1;
    bool mono = ch < 2;
    u16 fmt = mono ? FMT_48K_16_1 : FMT_48K_16_2;
    verb4(adc, V4_SET_FMT, fmt);
    verb(adc, V_SET_STREAM, (STREAM_TAG_IN << 4));
    if (H.dead) { H.dead = false; return false; }

    u32 bpf = mono ? 2u : 4u;
    u64 rphys, bphys;
    i16 *ring = dma_alloc(RING_FRAMES * bpf, &rphys);
    u32 *bdl = dma_alloc(4096, &bphys);
    if (!ring || !bdl) return false;
    if (((rphys | bphys) >> 32) && !(H.gcap & 1)) return false;

    w8(sd + SD_CTL, 0);
    wait_reg(sd + SD_CTL, 8, 2, 0, 10);
    w8(sd + SD_CTL, 1);
    wait_reg(sd + SD_CTL, 8, 1, 1, 10);
    delay_ms(1);
    w8(sd + SD_CTL, 0);
    w8(sd + SD_STS, 0x1C);
    for (int i = 0; i < NBDL; i++) {
        u64 a = rphys + (u64)i * SEG_FRAMES * bpf;
        bdl[i * 4 + 0] = (u32)a;
        bdl[i * 4 + 1] = (u32)(a >> 32);
        bdl[i * 4 + 2] = SEG_FRAMES * bpf;
        bdl[i * 4 + 3] = 0;
    }
    w32(sd + SD_BDPL, (u32)bphys);
    w32(sd + SD_BDPU, (u32)(bphys >> 32));
    w32(sd + SD_CBL, RING_FRAMES * bpf);
    w16(sd + SD_LVI, NBDL - 1);
    w16(sd + SD_FMT, fmt);
    w8(sd + SD_CTL + 2, (STREAM_TAG_IN << 4) | (r8(sd + SD_CTL + 2) & 0x0F));
    w8(sd + SD_CTL, 0x02);
    if (!wait_reg(sd + SD_CTL, 8, 2, 2, 10)) { kprintf("HDA: stream de entrada não iniciou\n"); w8(sd + SD_CTL, 0); return false; }

    H.in_sd = sd;
    H.in_ring = ring;
    H.in_bdl = bdl;
    H.in_rpos = 0;
    H.in_first = r32(sd + SD_LPIB);
    H.in_mono = mono;
    H.in_name = input_label(H.w[path[0]].cfg);
    H.in_running = true;
    char buf[96];
    int o = 0;
    for (int i = 0; i < len && o < (int)sizeof buf - 8; i++)
        o += snprintf(buf + o, sizeof buf - o, i ? " -> 0x%02x" : "0x%02x", path[i]);
    kprintf("HDA:   captura %s, pino 0x%02x: %s (ADC%s)\n", H.in_name, path[0], buf, mono ? ", mono" : "");
    kprintf("HDA: stream %d de entrada (SD @0x%02x): 48 kHz, 16 bits, %s\n", STREAM_TAG_IN, sd, mono ? "mono" : "estéreo");
    return true;
}

/* ---------------------------------------------------------------- stream de saída */
static bool stream_setup(void) {
    int iss = (H.gcap >> 8) & 15, oss = (H.gcap >> 12) & 15, bss = (H.gcap >> 3) & 31;
    if (oss) H.sd = SD_BASE + iss * 0x20;                 /* primeiro stream de saída */
    else if (bss) H.sd = SD_BASE + (iss + oss) * 0x20;    /* bidirecional */
    else { kprintf("HDA: controlador sem streams de saída\n"); return false; }

    u64 rphys, bphys, pphys;
    H.ring = dma_alloc(RING_BYTES, &rphys);
    H.bdl = dma_alloc(4096, &bphys);
    H.posbuf = dma_alloc(4096, &pphys);
    if (!H.ring || !H.bdl || !H.posbuf) return false;
    if (((rphys | bphys | pphys) >> 32) && !(H.gcap & 1)) return false;

    u32 sd = H.sd;
    w8(sd + SD_CTL, 0);
    wait_reg(sd + SD_CTL, 8, 2, 0, 10);
    w8(sd + SD_CTL, 1);                                   /* SRST */
    wait_reg(sd + SD_CTL, 8, 1, 1, 10);
    delay_ms(1);
    w8(sd + SD_CTL, 0);
    if (!wait_reg(sd + SD_CTL, 8, 1, 0, 10)) kprintf("HDA: reset do stream não concluiu (seguindo)\n");
    w8(sd + SD_STS, 0x1C);                                /* limpa BCIS/FIFOE/DESE */

    for (int i = 0; i < NBDL; i++) {
        u64 a = rphys + (u64)i * SEG_FRAMES * 4;
        H.bdl[i * 4 + 0] = (u32)a;
        H.bdl[i * 4 + 1] = (u32)(a >> 32);
        H.bdl[i * 4 + 2] = SEG_FRAMES * 4;
        H.bdl[i * 4 + 3] = 0;                             /* sem IOC: operamos por polling */
    }
    w32(sd + SD_BDPL, (u32)bphys);
    w32(sd + SD_BDPU, (u32)(bphys >> 32));
    w32(sd + SD_CBL, RING_BYTES);
    w16(sd + SD_LVI, NBDL - 1);
    w16(sd + SD_FMT, FMT_48K_16_2);
    w8(sd + SD_CTL + 2, (STREAM_TAG << 4) | (r8(sd + SD_CTL + 2) & 0x0F)); /* tag do stream, prioridade normal */

    w32(DPUBASE, (u32)(pphys >> 32));
    w32(DPLBASE, (u32)pphys | 1);                         /* buffer de posição do DMA */

    /* conversores: mesmo stream em todos os DACs escolhidos (fone + alto-falante) */
    for (int i = 0; i < H.ndacs; i++) {
        verb4(H.dacs[i], V4_SET_FMT, FMT_48K_16_2);
        verb(H.dacs[i], V_SET_STREAM, (STREAM_TAG << 4) | 0);
    }
    if (H.dead) return false;

    w8(sd + SD_CTL, 0x02);                                /* RUN, sem interrupções */
    if (!wait_reg(sd + SD_CTL, 8, 2, 2, 10)) { kprintf("HDA: stream não iniciou\n"); return false; }
    H.wpos = 0;
    H.ahead_target = WRITE_AHEAD;
    H.first_lpib = r32(sd + SD_LPIB);
    H.running = true;
    kprintf("HDA: stream %d (SD @0x%02x) em execução: 48 kHz, 16 bits, estéreo, %d x %d quadros, FIFO %u bytes\n",
            STREAM_TAG, sd, NBDL, SEG_FRAMES, r16(sd + SD_FIFOS) + 1);
    return true;
}

/* ---------------------------------------------------------------- API para sound.c */
static void ring_zero(u32 at, u32 n) {
    while (n) {
        u32 c = MIN(n, RING_FRAMES - at);
        memset(H.ring + at * 2, 0, c * 4);
        at = (at + c) & RING_MASK;
        n -= c;
    }
}

void hda_service(void) {
    if (!H.running) return;
    capture_service();
    u32 sd = H.sd;
    u8 st = r8(sd + SD_STS);
    if (st & 0x18) {                                      /* erro de FIFO / descritor */
        static int nerr;
        if (nerr++ < 4) kprintf("HDA: status do stream 0x%02x\n", st);
        w8(sd + SD_STS, st & 0x1C);
    } else if (st & 0x04) w8(sd + SD_STS, 0x04);

    u32 lpib = r32(sd + SD_LPIB);
    if (lpib >= RING_BYTES) lpib %= RING_BYTES;
    u32 play = lpib / 4;
    if (!H.saw_motion && lpib != H.first_lpib) {
        H.saw_motion = true;
        kprintf("HDA: DMA de saída ativo (LPIB %u -> %u, posbuf %u)\n", H.first_lpib, lpib, H.posbuf[(sd - SD_BASE) / 0x20 * 2]);
    }

    u32 ahead = (H.wpos - play) & RING_MASK;
    if (ahead > HALF_RING) {                              /* o DMA passou da escrita: ressincroniza */
        if (H.primed) {                                   /* laço principal lento: aumenta a folga */
            H.ahead_target = MIN(H.ahead_target + AHEAD_STEP, MAX_AHEAD);
            if (H.underruns++ < 8)
                kprintf("HDA: underrun (escrita %u, DMA %u); folga -> %u ms\n", H.wpos, play, H.ahead_target / 48);
        }
        H.wpos = (play + RESYNC_GAP) & RING_MASK;
        ahead = RESYNC_GAP;
    }
    H.primed = true;
    if (snd_late_pending()) {                             /* sons novos: entram já, sobre o que foi escrito */
        if (ahead > LATE_GAP) {
            u32 at = (play + LATE_GAP) & RING_MASK, n = ahead - LATE_GAP;
            while (n) {
                u32 c = MIN(n, RING_FRAMES - at);
                snd_render_late(H.ring + at * 2, c);
                at = (at + c) & RING_MASK;
                n -= c;
            }
        }
        snd_late_done();
    }
    if (ahead >= H.ahead_target) return;
    u32 todo = H.ahead_target - ahead;
    while (todo) {
        u32 n = MIN(todo, RING_FRAMES - H.wpos);
        snd_render(H.ring + H.wpos * 2, n);
        ring_zero((H.wpos + HALF_RING) & RING_MASK, n);   /* mantém o futuro distante em silêncio */
        H.wpos = (H.wpos + n) & RING_MASK;
        todo -= n;
    }
    __asm__ volatile("sfence" ::: "memory");
}

void hda_set_mute(bool mute) {
    if (!H.running) return;
    H.dead = false;
    for (int i = 0; i < H.noutpins; i++) {
        u8 pin = H.outpins[i];
        if (H.w[pin].caps & (1u << 2)) {
            u32 c = amp_caps(pin, true);
            verb4(pin, V4_SET_AMP, 0xB000 | (mute ? 0x80 : amp_gain(c)));
        }
    }
    for (int i = 0; i < H.ndacs; i++) {
        u8 d = H.dacs[i];
        if (H.w[d].caps & (1u << 2)) {
            u32 c = amp_caps(d, true);
            verb4(d, V4_SET_AMP, 0xB000 | (mute ? 0x80 : amp_gain(c)));
        }
    }
}

static const char *ctrl_vendor(u16 v) {
    switch (v) {
    case 0x8086: return "Intel";
    case 0x1022: case 0x1002: return "AMD";
    case 0x10DE: return "NVIDIA";
    case 0x1106: return "VIA";
    case 0x10B9: return "ULi";
    case 0x1039: return "SiS";
    default: return "";
    }
}

static bool probe_controller(pci_dev_t *d) {
    memset(&H, 0, sizeof H);
    H.pci = d;
    u64 bar = pci_bar_addr(d, 0);
    if (!bar || (d->bar[0] & 1)) { kprintf("HDA: BAR0 inválido\n"); return false; }
    vmm_map_mmio(bar, 0x4000);
    H.mm = (volatile u8 *)(uptr)bar;
    pci_enable_bus_master(d);
    if (d->vendor == 0x8086) {                            /* TCSEL = TC0 (como o Linux faz) */
        u32 v = pci_read(d, 0x44);
        pci_write(d, 0x44, v & ~7u);
    }
    H.gcap = r16(GCAP);
    if (H.gcap == 0xFFFF) { kprintf("HDA: controlador não responde no MMIO\n"); return false; }
    kprintf("HDA: controlador %04x:%04x em %02x:%02x.%x, MMIO 0x%llx, HDA %u.%u, GCAP 0x%04x (%u entrada, %u saída, %u bidir, %s)\n",
            d->vendor, d->device, d->bus, d->dev, d->fn, bar, r8(VMAJ), r8(VMIN), H.gcap,
            (H.gcap >> 8) & 15, (H.gcap >> 12) & 15, (H.gcap >> 3) & 31, (H.gcap & 1) ? "64 bits" : "32 bits");
    if (!controller_reset()) return false;

    u16 sts = r16(STATESTS) & 0x7FFF;
    for (int i = 0; i < 10 && !sts; i++) { delay_ms(5); sts = r16(STATESTS) & 0x7FFF; }
    w16(STATESTS, sts);
    kprintf("HDA: codecs presentes (STATESTS) 0x%04x\n", sts);
    if (!sts) return false;

    if (!corb_setup()) H.immediate = true;
    for (int cad = 0; cad < 15; cad++) {
        if (!(sts & (1u << cad))) continue;
        if (codec_probe(cad)) {
            if (!stream_setup()) return false;
            capture_setup();                              /* a saída não depende da captura */
            return true;
        }
    }
    return false;
}

bool hda_input_available(void) { return H.in_running; }
const char *hda_input_name(void) { return H.in_name ? H.in_name : "Nenhuma entrada"; }

bool hda_probe(char *name, usize n) {
    for (int i = 0;; i++) {
        pci_dev_t *d = pci_find_class(0x04, 0x03, i);
        if (!d) break;
        if (probe_controller(d)) {
            const char *v = ctrl_vendor(d->vendor);
            snprintf(name, n, "Áudio HD%s%s (codec %04x:%04x)", *v ? " " : "", v, H.codec_id >> 16, H.codec_id & 0xFFFF);
            return true;
        }
        /* deixa o controlador quieto se não serviu */
        if (H.mm) { corb_stop(); w32(DPLBASE, 0); }
        memset(&H, 0, sizeof H);
    }
    return false;
}

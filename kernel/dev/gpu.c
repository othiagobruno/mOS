/*
 * mOS — GPUs Intel (integradas, Gen7 a Xe2) e AMD: fase 1, somente leitura.
 *
 * Só lê o espaço de configuração PCI e, na Intel, registradores de vídeo (pipe e plano primário),
 * que estão ligados porque o firmware está usando a tela. Nenhuma escrita: sem risco de travar a GPU.
 * Tudo vai para o log do kernel (app Console) com o prefixo "gpu:", para conferir em máquinas reais.
 *
 * Registradores de vídeo (iguais de Haswell a Tiger Lake/Alder Lake para pipe e plano 1):
 *   PIPE_CONF  0x70008 + pipe*0x1000   bit 31 = pipe ligado
 *   PIPE_SRC   0x6001C + pipe*0x1000   (largura-1) << 16 | (altura-1)
 *   PLANE_CTL  0x70180 + pipe*0x1000   bit 31 = plano ligado
 *   PLANE_STRIDE 0x70188               Gen9+: unidades de 64 bytes; antes: bytes
 *   PLANE_SURF 0x7019C                 endereço da superfície na GTT global
 * Config PCI: GGC 0x50 (memória roubada e GTT), BDSM 0x5C (Gen11+: 0xC0, 64 bits).
 */
#include "gpu.h"
#include "syscall.h"

#define MAX_GPUS 4

static gpu_info_t gpus[MAX_GPUS];
static int ngpus;
static char summary[128] = "Framebuffer UEFI (nenhuma GPU Intel ou AMD reconhecida)";

/* ------------------------------------------------------------ tabelas de família */
typedef struct { u8 hi; int gen; const char *name, *arch; } intel_fam_t;
/* pelo byte alto do ID do dispositivo: cobre quase todos os modelos de cada família */
static const intel_fam_t intel_fams[] = {
    {0x04, 7, "Haswell", "Gen7.5"}, {0x0A, 7, "Haswell", "Gen7.5"}, {0x0C, 7, "Haswell", "Gen7.5"}, {0x0D, 7, "Haswell", "Gen7.5"},
    {0x16, 8, "Broadwell", "Gen8"}, {0x22, 8, "Cherry View", "Gen8"},
    {0x19, 9, "Skylake", "Gen9"}, {0x59, 9, "Kaby Lake", "Gen9.5"}, {0x87, 9, "Amber/Comet Lake", "Gen9.5"},
    {0x3E, 9, "Coffee Lake", "Gen9.5"}, {0x9B, 9, "Comet Lake", "Gen9.5"},
    {0x5A, 9, "Apollo Lake", "Gen9"}, {0x31, 9, "Gemini Lake", "Gen9"},
    {0x8A, 11, "Ice Lake", "Gen11"}, {0x4E, 11, "Jasper Lake", "Gen11"}, {0x45, 11, "Elkhart Lake", "Gen11"},
    {0x9A, 12, "Tiger Lake", "Gen12 (Xe-LP)"}, {0x4C, 12, "Rocket Lake", "Gen12 (Xe-LP)"},
    {0x46, 12, "Alder Lake", "Gen12 (Xe-LP)"}, {0xA7, 12, "Raptor Lake", "Gen12 (Xe-LP)"},
    {0x56, 12, "Arc Alchemist (DG2)", "Xe-HPG"}, {0x7D, 12, "Meteor/Arrow Lake", "Xe-LPG"},
    {0x64, 20, "Lunar Lake", "Xe2"}, {0xE2, 20, "Battlemage", "Xe2"},
};

typedef struct { u16 id; const char *name, *arch; } amd_id_t;
static const amd_id_t amd_ids[] = {   /* GPUs integradas (APUs) mais comuns */
    {0x15DD, "Raven Ridge/Picasso", "Vega (GFX9)"}, {0x15D8, "Picasso", "Vega (GFX9)"},
    {0x1636, "Renoir", "Vega (GFX9)"}, {0x1638, "Cezanne", "Vega (GFX9)"}, {0x164C, "Lucienne", "Vega (GFX9)"},
    {0x15E7, "Barcelo", "Vega (GFX9)"}, {0x1681, "Rembrandt", "RDNA2 (GFX10.3)"}, {0x164E, "Raphael", "RDNA2 (GFX10.3)"},
    {0x1506, "Mendocino", "RDNA2 (GFX10.3)"}, {0x163F, "Van Gogh", "RDNA2 (GFX10.3)"},
    {0x15BF, "Phoenix", "RDNA3 (GFX11)"}, {0x15C8, "Phoenix 2", "RDNA3 (GFX11)"}, {0x150E, "Strix Point", "RDNA3.5 (GFX11.5)"},
};
typedef struct { u8 hi; const char *name, *arch; } amd_fam_t;
static const amd_fam_t amd_fams[] = {   /* placas dedicadas, pelo byte alto */
    {0x67, "Polaris/GCN", "GCN (GFX8)"}, {0x69, "Polaris/Vega", "GCN (GFX8/9)"}, {0x6F, "Polaris", "GCN (GFX8)"},
    {0x68, "Vega", "Vega (GFX9)"}, {0x66, "Vega 20/GCN", "Vega (GFX9)"},
    {0x73, "Navi 1x/2x", "RDNA1/2 (GFX10)"}, {0x74, "Navi 3x", "RDNA3 (GFX11)"}, {0x75, "Navi 4x", "RDNA4 (GFX12)"},
};

/* ------------------------------------------------------------ Intel */
static volatile u8 *intel_regs;
static inline u32 ir(u32 off) { return mmio_r32(intel_regs, off); }

static void intel_probe(gpu_info_t *g) {
    pci_dev_t *d = g->pci;
    g->maker = "Intel";
    g->family = "GPU Intel desconhecida";
    g->arch = "?";
    g->guessed = true;
    for (u32 i = 0; i < ARRAY_LEN(intel_fams); i++)
        if (intel_fams[i].hi == (d->device >> 8)) { g->gen = intel_fams[i].gen; g->family = intel_fams[i].name; g->arch = intel_fams[i].arch; g->guessed = false; break; }
    g->mmio = pci_bar_addr(d, 0);
    g->aperture = pci_bar_addr(d, 2);
    g->pipe = -1;

    /* memória roubada (stolen) e tamanho da GTT, pelo GGC */
    u32 ggc = pci_read(d, 0x50) & 0xFFFF;
    if (g->gen >= 8) {
        u32 gms = ggc >> 8, ggms = (ggc >> 6) & 3;
        g->stolen_mb = gms < 0xF0 ? gms * 32 : (gms - 0xF0 + 1) * 4;
        g->gtt_mb = ggms ? 1u << ggms : 0;
    } else if (g->gen == 7) {
        g->stolen_mb = ((ggc >> 3) & 0x1F) * 32;
        g->gtt_mb = (ggc >> 8) & 3;
    }
    if (g->gen >= 11) g->stolen_base = (pci_read(d, 0xC0) & 0xFFF00000u) | (u64)pci_read(d, 0xC4) << 32;
    else if (g->gen) g->stolen_base = pci_read(d, 0x5C) & 0xFFF00000u;

    kprintf("gpu: Intel %04x rev %02x — %s, %s%s; registradores 0x%llx, abertura 0x%llx\n", d->device,
            pci_read(d, 0x08) & 0xFF, g->family, g->arch, g->guessed ? " (família não reconhecida)" : "", g->mmio, g->aperture);
    kprintf("gpu:   memória roubada %u MiB em 0x%llx, GTT %u MiB\n", g->stolen_mb, g->stolen_base, g->gtt_mb);

    if (!g->mmio || g->gen < 7 || (g->mmio >> 48)) return;
    if (!(pci_read(d, 0x04) & 2)) { kprintf("gpu:   decodificação de memória desligada, sem ler registradores\n"); return; }
    vmm_map_mmio(g->mmio, 0x80000);                   /* só a faixa de vídeo (até 0x7FFFF) */
    intel_regs = (volatile u8 *)g->mmio;

    u64 fb = (u64)g_boot->fb.fb;
    for (int p = 0; p < 3; p++) {
        u32 conf = ir(0x70008 + p * 0x1000);
        if (conf == 0xFFFFFFFFu || !(conf >> 31)) continue;
        u32 src = ir(0x6001C + p * 0x1000), ctl = ir(0x70180 + p * 0x1000);
        u32 str = ir(0x70188 + p * 0x1000), surf = ir(0x7019C + p * 0x1000) & ~0xFFFu;
        u32 w = ((src >> 16) & 0x1FFF) + 1, h = (src & 0x1FFF) + 1;
        u32 stride = g->gen >= 9 ? (str & 0xFFF) * 64 : str & ~63u;
        bool plane_on = ctl >> 31;
        bool match = plane_on && g->aperture && fb >= g->aperture && fb - g->aperture == surf;
        kprintf("gpu:   pipe %c ligado: %ux%u, plano 1 %s, stride %u bytes, superfície GTT 0x%x%s\n", 'A' + p, w, h,
                plane_on ? "ligado" : "desligado", stride, surf, match ? " = framebuffer do UEFI" : "");
        if (g->pipe < 0 || match) {
            g->pipe = p; g->mode_w = w; g->mode_h = h; g->stride = stride; g->surf_match = match;
        }
    }
    if (g->pipe < 0) kprintf("gpu:   nenhum pipe ligado encontrado (registradores de vídeo inacessíveis?)\n");
}

/* ------------------------------------------------------------ portas de vídeo (fase 3, só leitura)
 *
 * 1) O que o firmware ligou: TRANS_DDI_FUNC_CTL de cada transcoder (A 0x60400, B 0x61400, C 0x62400,
 *    eDP 0x6F400 até Gen11): bit 31 ligado; porta em 30:28 (Gen12+: 30:27, porta+1); modo em 26:24
 *    (0 HDMI, 1 DVI, 2 DP, 3 DP MST).
 * 2) O que está plugado: a linha de presença (HPD) de cada porta, em registradores que mudam por geração:
 *    - Gen7.5/8 (chipset LPT/WPT):     SDEISR 0xC4000 bits 21/22/23 = portas B/C/D
 *    - Gen9 (SPT/KBP/CNP/CMP):         os mesmos + bit 24 = A, bit 25 = E
 *    - Apollo/Gemini Lake (sem PCH):   GEN8_DE_PORT_ISR 0x44440 bits 3/4/5 = A/B/C
 *    - Gen11/12 (ICP/TGP/ADP):         SDEISR bits 16.. = DDI A.., bits 24.. = USB-C 1..; no norte,
 *                                      GEN11_DE_HPD_ISR 0x44470 bits 16.. (DP alt) e 0.. (Thunderbolt)
 *    A porta A é o painel interno (eDP) na prática, e não conta como monitor externo.
 * Ligar uma porta (PLL, buffers, treino de link, EDID) exige escrever na GPU: fica para quando der para
 * testar em hardware. Até lá, um monitor plugado e apagado ganha pelo menos um aviso claro. */
static char notice[160];

static void port_label(gpu_info_t *g, int port, char *out, usize n) {
    bool icl = g->gen == 11 && g->family[0] == 'I';   /* Ice Lake: portas C.. são USB-C 1.. */
    if (g->gen == 12 && port >= 3) snprintf(out, n, "USB-C %d", port - 2);
    else if (icl && port >= 2) snprintf(out, n, "USB-C %d", port - 1);
    else snprintf(out, n, "DDI %c", 'A' + port);
}

static void plugged(gpu_info_t *g, const char *what) {
    g->ext_plugged++;
    usize l = strlen(g->ports);
    snprintf(g->ports + l, sizeof g->ports - l, "%s%s", l ? ", " : "", what);
}

static void intel_ports(gpu_info_t *g) {
    if (!intel_regs || g->gen < 7 || g->gen >= 20) return;
    /* 1) transcoders ligados pelo firmware */
    static const u32 trans[4] = {0x60400, 0x61400, 0x62400, 0x6F400};
    static const char *mode_name[8] = {"HDMI", "DVI", "DP", "DP MST", "FDI", "?", "?", "?"};
    for (int t = 0; t < 4; t++) {
        if (t == 3 && g->gen >= 12) continue;          /* Gen12: sem transcoder eDP dedicado */
        u32 v = ir(trans[t]);
        if (v == 0xFFFFFFFFu || !(v >> 31)) continue;
        int port = g->gen >= 12 ? (int)((v >> 27) & 0xF) - 1 : (int)((v >> 28) & 7);
        bool internal = t == 3 || port == 0;
        char pn[16];
        port_label(g, port < 0 ? 0 : port, pn, sizeof pn);
        kprintf("gpu:   transcoder %s ligado pelo firmware: %s na %s%s\n", t == 3 ? "eDP" : (const char *[]){"A", "B", "C"}[t],
                t == 3 ? "eDP" : mode_name[(v >> 24) & 7], pn, internal ? " (tela interna)" : " (monitor externo)");
        if (!internal) g->ext_driven++;
    }

    /* 2) linhas de presença */
    bool atom = g->device >> 8 == 0x5A || g->device >> 8 == 0x31;
    char pn[24];
    if (atom) {
        u32 isr = ir(0x44440);
        for (int p = 1; p < 3; p++) if (isr >> (3 + p) & 1) { snprintf(pn, sizeof pn, "DDI %c", 'A' + p); plugged(g, pn); }
    } else if (g->gen >= 11) {
        u32 sde = ir(0xC4000), north = ir(0x44470);
        if (sde == 0xFFFFFFFFu) sde = 0;
        if (north == 0xFFFFFFFFu) north = 0;
        for (int i = 1; i < 4; i++) if (sde >> (16 + i) & 1) { snprintf(pn, sizeof pn, "HDMI/DP %c", 'A' + i); plugged(g, pn); }
        for (int i = 0; i < 6; i++)
            if ((sde >> (24 + i) & 1) || (north >> (16 + i) & 1) || (north >> i & 1)) { snprintf(pn, sizeof pn, "USB-C %d", i + 1); plugged(g, pn); }
    } else {
        u32 sde = ir(0xC4000);
        if (sde == 0xFFFFFFFFu) sde = 0;
        static const struct { u8 bit; char port; } spt[] = {{21, 'B'}, {22, 'C'}, {23, 'D'}, {25, 'E'}};
        for (u32 i = 0; i < ARRAY_LEN(spt); i++) {
            if (spt[i].port == 'E' && g->gen < 9) continue;
            if (sde >> spt[i].bit & 1) { snprintf(pn, sizeof pn, "HDMI/DP %c", spt[i].port); plugged(g, pn); }
        }
    }
    kprintf("gpu:   monitores externos com sinal de presença: %s\n", g->ext_plugged ? g->ports : "nenhum");

    if (g->ext_driven)
        snprintf(notice, sizeof notice, "O firmware mostra a mesma imagem no monitor externo (espelho). Estender a área de trabalho para ele ainda precisa do driver de vídeo da %s.", g->family);
    else if (g->ext_plugged)
        snprintf(notice, sizeof notice, "Monitor detectado em %s, mas o mOS ainda não liga portas de vídeo nesta GPU (%s). Ele fica apagado por enquanto.", g->ports, g->family);
    if (notice[0]) kprintf("gpu: %s\n", notice);
}

const char *gpu_display_notice(void) { return notice; }

/* ------------------------------------------------------------ vsync (fase 2, ainda só leitura)
 * Espera o retorno vertical (vblank) antes de copiar o quadro para o framebuffer: a cópia começa no
 * topo da tela junto com a varredura e anda mais rápido que ela, então não aparece tearing.
 *   PIPEDSL       0x70000 + pipe*0x1000   linha sendo varrida (>= altura ativa = dentro do vblank)
 *   PIPE_FRMCOUNT 0x70040 + pipe*0x1000   contador de quadros
 * No boot mede a taxa de atualização; se os registradores não fizerem sentido, o vsync fica desligado. */
static int vs_pipe = -1;
static u32 vs_lines, vs_hz;
static u64 vs_last_ms;
static bool vs_on;

static inline u32 vs_scanline(void) { return ir(0x70000 + vs_pipe * 0x1000) & 0x1FFF; }
static inline u32 vs_frame(void) { return ir(0x70040 + vs_pipe * 0x1000); }

static void vsync_init(gpu_info_t *g) {
    if (!intel_regs || g->pipe < 0 || !g->display || !g->mode_h) return;
    vs_pipe = g->pipe;
    vs_lines = g->mode_h;
    u32 f0 = vs_frame();
    u64 t0 = timer_ticks();
    sleep_ms(100);
    u32 f1 = vs_frame();
    u64 dt = timer_ticks() - t0;
    vs_hz = dt ? (u32)((u64)(f1 - f0) * 1000 / dt) : 0;
    u32 lo = ~0u, hi = 0;
    for (int i = 0; i < 4000; i++) { u32 s = vs_scanline(); lo = MIN(lo, s); hi = MAX(hi, s); }
    vs_on = vs_hz >= 20 && vs_hz <= 250 && hi > lo && hi < vs_lines + 400;
    kprintf("gpu: vsync %s (pipe %c: %u Hz medidos, linhas %u..%u de %u visíveis)\n", vs_on ? "ligado" : "desligado: leituras sem sentido",
            'A' + vs_pipe, vs_hz, lo, hi, vs_lines);
}

void gpu_vsync_wait(void) {
    if (!vs_on) return;
    u64 now = timer_ticks();
    if (vs_last_ms && now - vs_last_ms < 5) return;  /* mesma composição: os outros retângulos vão juntos */
    u32 f0 = vs_frame();
    /* limite por número de leituras, não por tempo: funciona até com interrupções desligadas (pânico) */
    for (u32 i = 0; i < 200000; i++) {
        if (vs_scanline() >= vs_lines || vs_frame() != f0) break;
        __asm__ volatile("pause");
    }
    vs_last_ms = timer_ticks();
}

bool gpu_vsync_active(void) { return vs_on; }
u32 gpu_refresh_hz(void) { return vs_hz; }

/* ------------------------------------------------------------ AMD */
static void amd_probe(gpu_info_t *g) {
    pci_dev_t *d = g->pci;
    g->maker = "AMD";
    g->family = "GPU AMD desconhecida";
    g->arch = "?";
    g->guessed = true;
    for (u32 i = 0; i < ARRAY_LEN(amd_ids); i++)
        if (amd_ids[i].id == d->device) { g->family = amd_ids[i].name; g->arch = amd_ids[i].arch; g->guessed = false; break; }
    if (g->guessed)
        for (u32 i = 0; i < ARRAY_LEN(amd_fams); i++)
            if (amd_fams[i].hi == (d->device >> 8)) { g->family = amd_fams[i].name; g->arch = amd_fams[i].arch; break; }
    g->aperture = pci_bar_addr(d, 0);                 /* VRAM visível pela CPU */
    g->mmio = pci_bar_addr(d, 5);                     /* registradores (GCN em diante) */
    g->pipe = -1;
    kprintf("gpu: AMD %04x rev %02x — %s, %s%s; VRAM visível 0x%llx, registradores 0x%llx\n", d->device,
            pci_read(d, 0x08) & 0xFF, g->family, g->arch, g->guessed ? " (estimada pelo ID)" : "", g->aperture, g->mmio);
}

/* ------------------------------------------------------------ comum */
void gpu_init(void) {
    for (int i = 0; i < pci_count() && ngpus < MAX_GPUS; i++) {
        pci_dev_t *d = pci_get(i);
        if (d->class_ != 3) continue;
        bool virtio = d->vendor == 0x1AF4 && d->device == 0x1050;
        if (d->vendor != 0x8086 && d->vendor != 0x1002 && !virtio) {
            kprintf("gpu: adaptador %04x:%04x sem driver (só o framebuffer do UEFI)\n", d->vendor, d->device);
            continue;
        }
        gpu_info_t *g = &gpus[ngpus++];
        memset(g, 0, sizeof *g);
        g->pci = d; g->vendor = d->vendor; g->device = d->device;
        if (virtio) { if (!virtio_gpu_probe(g)) g->arch = "2D (falhou: só framebuffer)"; }
        else if (d->vendor == 0x8086) intel_probe(g);
        else amd_probe(g);
    }
    if (!ngpus) { kprintf("gpu: nenhuma GPU Intel, AMD ou virtio\n"); return; }

    /* quem mostra o framebuffer do UEFI: a GPU cuja abertura é a mais próxima abaixo dele */
    u64 fb = (u64)g_boot->fb.fb, best = ~0ull;
    gpu_info_t *owner = NULL;
    for (int i = 0; i < ngpus; i++) {
        gpu_info_t *g = &gpus[i];
        if (g->surf_match) { owner = g; break; }
        if (g->aperture && fb >= g->aperture && fb - g->aperture < best) { best = fb - g->aperture; owner = g; }
    }
    if (owner) owner->display = true;
    if (owner && owner->vendor == 0x8086) { vsync_init(owner); intel_ports(owner); }
    gpu_info_t *m = owner ? owner : &gpus[0];
    snprintf(summary, sizeof summary, "%s %s (%s)%s", m->maker, m->family, m->arch,
             ngpus > 1 ? (owner ? " + outra GPU" : " + outras GPUs") : "");
    kprintf("gpu: %d GPU(s); a tela do UEFI está na %s %s\n", ngpus, owner ? owner->maker : "?", owner ? owner->family : "(não identificada)");
}

int gpu_kms_available(void) {
    for (int i = 0; i < ngpus; i++) {
        if (gpus[i].vendor == 0x8086 && gpus[i].gen >= 9 && gpus[i].pipe >= 0 && intel_regs) return 1;
        if (gpus[i].vendor == 0x1002 && gpus[i].aperture) return 1;
    }
    return 0;
}

/* Intel Gen9+: o plano já está ligado pelo firmware. Troca a superfície (deslocamento na abertura)
 * e o tamanho do pipe. AMD: o modo fica registrado; a superfície precisa cair na abertura da VRAM.
 * Sem programar PLL nem treino de link. */
int gpu_kms_set(unsigned connector, unsigned w, unsigned h, unsigned stride, unsigned format, unsigned long long surf_phys) {
    if (!w || !h || stride < w * 4 || (format != MOS_GPU_FMT_BGRA8 && format != MOS_GPU_FMT_RGBA8)) return -MOS_EINVAL;
    gpu_info_t *g = NULL;
    for (int i = 0; i < ngpus; i++) if (gpus[i].display) { g = &gpus[i]; break; }
    if (!g && ngpus) g = &gpus[0];
    if (!g) return -MOS_ENOENT;
    if (connector == MOS_GPU_CONN_EXTERNAL) return -MOS_ENOENT;
    if (g->vendor == 0x8086 && g->gen >= 9 && intel_regs && g->pipe >= 0) {
        u32 pipe = (u32)g->pipe;
        u32 surf = 0;
        if (surf_phys) {
            if (!g->aperture || surf_phys < g->aperture) return -MOS_EINVAL;
            surf = (u32)(surf_phys - g->aperture);
        } else surf = ir(0x7019C + pipe * 0x1000) & ~0xFFFu;
        mmio_w32(intel_regs, 0x6001C + pipe * 0x1000, ((w - 1) << 16) | (h - 1));
        mmio_w32(intel_regs, 0x70188 + pipe * 0x1000, (stride / 64) & 0xFFF);
        mmio_w32(intel_regs, 0x7019C + pipe * 0x1000, surf);
        u32 ctl = ir(0x70180 + pipe * 0x1000);
        mmio_w32(intel_regs, 0x70180 + pipe * 0x1000, ctl | 0x80000000u);
        u32 conf = ir(0x70008 + pipe * 0x1000);
        mmio_w32(intel_regs, 0x70008 + pipe * 0x1000, conf | 0x80000000u);
        g->mode_w = w; g->mode_h = h; g->stride = stride;
        kprintf("gpu: modo Intel pipe %c agora %ux%u stride %u\n", 'A' + (int)pipe, w, h, stride);
        return 0;
    }
    if (g->vendor == 0x1002 && g->aperture) {
        if (surf_phys && (surf_phys < g->aperture)) return -MOS_EINVAL;
        g->mode_w = w; g->mode_h = h; g->stride = stride;
        kprintf("gpu: modo AMD registrado %ux%u stride %u (sem PLL)\n", w, h, stride);
        return 0;
    }
    return -MOS_ENOENT;
}

int gpu_count(void) { return ngpus; }
const gpu_info_t *gpu_get(int i) { return i >= 0 && i < ngpus ? &gpus[i] : NULL; }
const char *gpu_summary(void) { return summary; }

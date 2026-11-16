/* mOS — teste de host de kernel/dev/amdgpu/amdgpu.c contra um Picasso simulado.
 * O "chip" é o arquivo de registradores do BAR5 (512 KiB) com os efeitos que o driver usa: o OTG varre
 * linhas a cada leitura da posição e conta quadros, a trava de atualização responde no bit de status, e o
 * endereço novo do plano só vale (EARLIEST_INUSE) no vblank depois de soltar a trava.
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu -I tools tools/amdgpu_hw_host.c \
 *       kernel/dev/amdgpu/amdgpu.c kernel/dev/amdgpu/edid.c kernel/dev/amdgpu/ucode.c -o /tmp/amdgpu_hw && /tmp/amdgpu_hw */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#include <stdarg.h>
#include <stdlib.h>

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ chip simulado */
static u32 regs[AMD_MMIO_SIZE / 4];
static u64 now_ms;
static u32 vline, frames;
static bool locked, armed;
static u32 pending_lo, pending_hi;
static bool verbose;
static int sim_otg = 0;

static u32 vtotal_of(void) { return (regs[R_OTG_V_TOTAL(sim_otg) / 4] & OTG_TIMING_MASK) + 1; }
static u32 vbs_of(void) { return regs[R_OTG_V_BLANK_START_END(sim_otg) / 4] & OTG_TIMING_MASK; }

static void scan(u32 lines) {
    u32 vt = vtotal_of();
    for (u32 k = 0; k < lines; k++) {
        vline++;
        if (vline == vbs_of() && armed && !locked) {        /* troca no começo do vblank */
            regs[R_DCSURF_EARLIEST_INUSE(sim_otg) / 4] = pending_lo;
            regs[R_DCSURF_EARLIEST_INUSE_HI(sim_otg) / 4] = pending_hi;
            regs[R_DCSURF_FLIP_CONTROL(sim_otg) / 4] &= ~FLIP_PENDING;
            armed = false;
        }
        if (vline >= vt) { vline = 0; frames = (frames + 1) & OTG_FRAME_COUNT_MASK; }
    }
}

u32 amd_host_rreg(u32 off) {
    if (off >= AMD_MMIO_SIZE) { printf("FALHOU: leitura fora do BAR5 0x%x\n", off); total++; return 0xFFFFFFFFu; }
    if (off == R_OTG_STATUS_POSITION(sim_otg)) { scan(7); return vline; }
    if (off == R_OTG_STATUS(sim_otg)) { scan(3); return vline >= vbs_of() ? OTG_V_BLANK : 0; }
    if (off == R_OTG_STATUS_FRAME_COUNT(sim_otg)) { scan(1); return frames; }
    if (off == R_OTG_MASTER_UPDATE_LOCK(sim_otg)) return locked ? (OTG_UPDATE_LOCK | OTG_UPDATE_LOCK_STATUS) : 0;
    if (off == R_DCSURF_FLIP_CONTROL(sim_otg)) scan(11);
    return regs[off / 4];
}

void amd_host_wreg(u32 off, u32 v) {
    if (off >= AMD_MMIO_SIZE) { printf("FALHOU: escrita fora do BAR5 0x%x\n", off); total++; return; }
    if (off == R_OTG_MASTER_UPDATE_LOCK(sim_otg)) { locked = v & OTG_UPDATE_LOCK; return; }
    regs[off / 4] = v;
    if (off == R_DCSURF_PRIMARY_ADDR(sim_otg)) {
        pending_lo = v; pending_hi = regs[R_DCSURF_PRIMARY_ADDR_HI(sim_otg) / 4];
        armed = true;
        regs[R_DCSURF_FLIP_CONTROL(sim_otg) / 4] |= FLIP_PENDING;
        if (!locked) printf("aviso: endereço escrito sem a trava\n");
    }
}

static u16 pci_cmd = 0x0006;
static u8 pci_rev = 0x75;
u32 pci_read(pci_dev_t *d, u8 off) {
    (void)d;
    if (off == 0x04) return pci_cmd;
    if (off == 0x08) return 0x03000000u | pci_rev;
    return 0;
}
u64 pci_bar_addr(pci_dev_t *d, int bar) {
    (void)d;
    return bar == 0 ? 0xE0000000ull : bar == 5 ? 0xFCC00000ull : 0;
}
/* VRAM simulada: os 256 MiB do BAR0 (alocados sob demanda) */
static u8 *vram;
static u64 wc_lo, wc_len;
void *amd_host_phys(u64 phys) {
    if (!vram) vram = calloc(1, 256u << 20);
    if (phys < 0xE0000000ull || phys >= 0xE0000000ull + (256ull << 20)) { printf("FALHOU: físico fora da VRAM 0x%llx\n", phys); total++; return vram; }
    return vram + (phys - 0xE0000000ull);
}
void vmm_map_wc(u64 phys, u64 size) { wc_lo = phys; wc_len = size; }
u64 timer_ticks(void) { return now_ms++; }
void sleep_ms(u64 ms) {
    /* 60 Hz em 5120x1440 (vtotal 1485): 89100 linhas por segundo */
    u64 lines = ms * 89100 / 1000;
    scan((u32)lines);
    now_ms += ms;
}
void kprintf(const char *fmt, ...) {
    if (!verbose) return;
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

/* estado deixado pela UEFI no PC do Thiago: 5120x1440@60 no OTG0, plano linear no começo da VRAM */
static const u64 FB_MC = 0xF400000000ull;
static void boot_state(const edid_mode_t *m, u32 rev_id) {
    memset(regs, 0, sizeof regs);
    vline = 0; frames = 1000; locked = false; armed = false;
    regs[R_RCC_DEV0_EPF0_STRAP0 / 4] = rev_id << STRAP0_ATI_REV_ID_SHIFT | 0x1234;
    regs[R_MC_VM_FB_LOCATION_BASE / 4] = 0xF400;
    regs[R_MC_VM_FB_LOCATION_TOP / 4] = 0xF40F;      /* 256 MiB */
    regs[R_MC_VM_FB_OFFSET / 4] = 0x3F0;             /* física 0x3F0000000 */
    u32 t[8];
    amd_otg_encode(m, t);
    regs[R_OTG_H_TOTAL(0) / 4] = t[0]; regs[R_OTG_H_BLANK_START_END(0) / 4] = t[1];
    regs[R_OTG_H_SYNC_A(0) / 4] = t[2]; regs[R_OTG_H_SYNC_A_CNTL(0) / 4] = t[3];
    regs[R_OTG_V_TOTAL(0) / 4] = t[4]; regs[R_OTG_V_BLANK_START_END(0) / 4] = t[5];
    regs[R_OTG_V_SYNC_A(0) / 4] = t[6]; regs[R_OTG_V_SYNC_A_CNTL(0) / 4] = t[7];
    regs[R_OTG_CONTROL(0) / 4] = OTG_MASTER_EN;
    regs[R_DCSURF_SURFACE_CONFIG(0) / 4] = SURFACE_FMT_ARGB8888;
    regs[R_DCSURF_PRI_VIEWPORT_DIM(0) / 4] = m->hactive | (u32)m->vactive << 16;
    regs[R_DCSURF_SURFACE_PITCH(0) / 4] = m->hactive - 1;
    regs[R_DCSURF_PRIMARY_ADDR(0) / 4] = (u32)FB_MC;
    regs[R_DCSURF_PRIMARY_ADDR_HI(0) / 4] = (u32)(FB_MC >> 32);
    regs[R_DCSURF_EARLIEST_INUSE(0) / 4] = (u32)FB_MC;
    regs[R_DCSURF_EARLIEST_INUSE_HI(0) / 4] = (u32)(FB_MC >> 32);
    regs[R_HUBPRET_CONTROL(0) / 4] = 2u << XBAR_CB_B_SHIFT | 3u << XBAR_CR_R_SHIFT | 0x5;
    for (int i = 1; i < AMD_NUM_OTG; i++) regs[R_DCHUBP_CNTL(i) / 4] = HUBP_BLANK_EN;
    regs[R_DC_HPD_INT_STATUS(4) / 4] = DC_HPD_SENSE;  /* HDMI */
}

static edid_mode_t mode_5120(void) {
    edid_mode_t m = {0};
    m.pclk_khz = 481140; m.hactive = 5120; m.hfront = 88; m.hsync = 44; m.hback = 148;
    m.vactive = 1440; m.vfront = 3; m.vsync = 5; m.vback = 37; m.flags = EDID_MODE_HPOS;
    return m;
}

static void test_otg_roundtrip(void) {
    FILE *f = fopen("tools/testdata/amdgpu/edid-hdmi-thiago.bin", "rb");
    u8 b[384];
    CHECK(f && fread(b, 1, 384, f) == 384, "EDID");
    if (f) fclose(f);
    edid_info_t e;
    edid_parse(b, 384, &e);
    int ok = 0;
    for (int i = 0; i < e.nmodes; i++) {
        const edid_mode_t *m = &e.modes[i];
        if (m->flags & EDID_MODE_INTERLACE) continue;
        u32 r[8];
        amd_otg_encode(m, r);
        amd_otg_timing_t t;
        amd_otg_decode(r, &t);
        edid_mode_t back;
        amd_otg_to_mode(&t, &back);
        bool same = back.hactive == m->hactive && back.hfront == m->hfront && back.hsync == m->hsync && back.hback == m->hback &&
                    back.vactive == m->vactive && back.vfront == m->vfront && back.vsync == m->vsync && back.vback == m->vback &&
                    (back.flags & (EDID_MODE_HPOS | EDID_MODE_VPOS)) == (m->flags & (EDID_MODE_HPOS | EDID_MODE_VPOS));
        CHECK(same, "OTG ida e volta %ux%u", m->hactive, m->vactive);
        ok += same;
    }
    CHECK(ok >= 25, "%d modos no OTG", ok);
    /* registradores como o programador de timing do DCN grava para 1920x1080@60 (CTA VIC 16) */
    edid_mode_t m;
    edid_vic_lookup(16, &m);
    u32 r[8];
    amd_otg_encode(&m, r);
    CHECK(r[0] == 2199 && r[1] == (2112u | 192u << 16) && r[2] == 44u << 16 && r[3] == 0 &&
          r[4] == 1124 && r[5] == (1121u | 41u << 16) && r[6] == 5u << 16 && r[7] == 0, "VIC 16 no OTG");
}

static void test_probe_and_vsync(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    amdgpu_t a;
    CHECK(amdgpu_probe(&a, &d, 0xE0000000ull, 5120, 1440, 5120), "probe");
    CHECK(a.variant == AMD_PICASSO && !strcmp(a.rlc_name, "picasso_rlc.bin"), "variante %d rlc %s", a.variant, a.rlc_name);
    CHECK(a.fb_start == FB_MC && a.vram_size == (256ull << 20) && a.fb_phys == 0x3F0000000ull, "VRAM");
    CHECK(a.pipe == 0 && a.uefi_match && a.uefi_mc == FB_MC && a.uefi_pitch_px == 5120 && !a.uefi_rgba, "pipe da tela");
    CHECK(a.otg[0].hactive == 5120 && a.otg[0].vactive == 1440 && a.otg[0].htotal == 5400 && a.otg[0].vtotal == 1485 &&
          a.otg[0].hfront == 88 && a.otg[0].hsync == 44 && a.otg[0].hpos && !a.otg[0].vpos, "timing lido");
    CHECK(!a.otg[1].on && !a.plane[1].on, "OTG1 desligado");
    CHECK(a.hpd_mask == 0x10, "HPD 0x%x", a.hpd_mask);
    CHECK(a.vsync_ok && a.refresh_mhz >= 59000 && a.refresh_mhz <= 61000, "taxa %u", a.refresh_mhz);
    /* GOP ocupa 5120*1440*4 = 28,1 MiB: a VRAM livre começa em 29 MiB e para 16 MiB antes do topo */
    CHECK(a.vram_lo == 29 * 16 && a.vram_hi == 240 * 16, "faixa de VRAM %u..%u", a.vram_lo, a.vram_hi);
    amdgpu_set_main(&a);
    CHECK(amdgpu_main() == &a, "main");

    for (int k = 0; k < 20; k++) {
        a.last_vblank_ms = 0;
        vline = (u32)(k * 97) % 1485;
        amdgpu_vblank_wait(&a);
        CHECK(vline >= 1482 || vline < 16, "vblank_wait parou na linha %u", vline);
    }
    /* chamada repetida na mesma composição (menos de 5 ms) não espera de novo */
    a.last_vblank_ms = now_ms;
    vline = 10;
    amdgpu_vblank_wait(&a);
    CHECK(vline == 10, "segunda espera na mesma composição andou para %u", vline);

    /* o rev_id >= 8 é Raven2 mesmo com o ID 15D8; registradores mortos recusam */
    boot_state(&m, 9);
    amdgpu_t b;
    CHECK(amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120) && b.variant == AMD_RAVEN2 && !strcmp(b.rlc_name, "raven2_rlc.bin"), "Raven2");
    regs[R_RCC_DEV0_EPF0_STRAP0 / 4] = 0xFFFFFFFFu;
    CHECK(!amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120), "registradores mortos");
    pci_cmd = 0;
    boot_state(&m, 1);
    CHECK(!amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120), "decodificação desligada");
    pci_cmd = 6;
    d.device = 0x1636;
    CHECK(!amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120), "Renoir não é Raven");
    /* GOP fora do plano ligado: pipe achado, mas sem match e sem voltar ao GOP */
    d.device = 0x15D8;
    boot_state(&m, 1);
    CHECK(amdgpu_probe(&b, &d, 0xE1000000ull, 5120, 1440, 5120) && b.pipe == 0 && !b.uefi_match, "sem match");
    CHECK(amdgpu_flip(&b, 0, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_ENOENT, "sem GOP para voltar");
}

static void test_flip_and_vram(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    amdgpu_t a;
    amdgpu_probe(&a, &d, 0xE0000000ull, 5120, 1440, 5120);
    u64 sz = 5120ull * 4 * 1440;
    u64 b1 = amdgpu_vram_alloc(&a, sz), b2 = amdgpu_vram_alloc(&a, sz);
    CHECK(b1 == 0xE0000000ull + (29ull << 20) && b2 == b1 + ((sz + 0xFFFF) & ~0xFFFFull), "alocação 0x%llx 0x%llx", b1, b2);
    CHECK(amdgpu_vram_owns(&a, b1, sz) && !amdgpu_vram_owns(&a, 0xE0000000ull, 4096), "dono");
    u64 before = amdgpu_vram_free_bytes(&a);

    CHECK(amdgpu_flip(&a, b1, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true) == 0, "flip para b1");
    u64 mc1 = FB_MC + (b1 - 0xE0000000ull);
    CHECK(regs[R_DCSURF_PRIMARY_ADDR(0) / 4] == (u32)mc1 && regs[R_DCSURF_PRIMARY_ADDR_HI(0) / 4] == (u32)(mc1 >> 32), "endereço MC");
    CHECK(regs[R_DCSURF_EARLIEST_INUSE(0) / 4] == (u32)mc1 && !(regs[R_DCSURF_FLIP_CONTROL(0) / 4] & FLIP_PENDING), "troca aconteceu");
    CHECK(!locked, "trava solta");
    CHECK((regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) == 5119 && (regs[R_DCSURF_FLIP_CONTROL(0) / 4] & FLIP_TYPE_IMMEDIATE) == 0, "pitch e tipo");
    CHECK((regs[R_HUBPRET_CONTROL(0) / 4] & 0xF) == 0x5, "crossbar preservou os outros bits");

    CHECK(amdgpu_flip(&a, b2, 5120, 1440, 5376 * 4, MOS_GPU_FMT_RGBA8, true) == 0, "flip RGBA com pitch maior");
    CHECK((regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) == 5375, "pitch 5376");
    u32 xb = regs[R_HUBPRET_CONTROL(0) / 4];
    CHECK(((xb >> XBAR_CB_B_SHIFT) & 3) == 3 && ((xb >> XBAR_CR_R_SHIFT) & 3) == 2, "crossbar RGBA");
    CHECK(a.flips == 2 && a.flip_timeouts == 0, "contadores");

    CHECK(amdgpu_flip(&a, b1, 1920, 1080, 1920 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "outro tamanho");
    CHECK(amdgpu_flip(&a, b1, 5120, 1440, 5120 * 4 + 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "stride desalinhado");
    CHECK(amdgpu_flip(&a, b1 + 4, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "endereço desalinhado");
    CHECK(amdgpu_flip(&a, 0xD0000000ull, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "fora da VRAM");
    CHECK(amdgpu_flip(&a, 0xE0000000ull + (250ull << 20), 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "passa do fim");
    CHECK(amdgpu_flip(&a, b1, 5120, 1440, 5120 * 4, 9, false) == -MOS_EINVAL, "formato");

    CHECK(amdgpu_flip(&a, 0, 0, 0, 0, MOS_GPU_FMT_BGRA8, true) == 0 && regs[R_DCSURF_PRIMARY_ADDR(0) / 4] == (u32)FB_MC &&
          (regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) == 5119, "volta ao GOP");
    xb = regs[R_HUBPRET_CONTROL(0) / 4];
    CHECK(((xb >> XBAR_CB_B_SHIFT) & 3) == 2 && ((xb >> XBAR_CR_R_SHIFT) & 3) == 3, "crossbar do GOP");

    amdgpu_vram_free(&a, b1, sz);
    CHECK(amdgpu_vram_free_bytes(&a) == before + ((sz + 0xFFFF) & ~0xFFFFull), "free");
    u64 b3 = amdgpu_vram_alloc(&a, 4096);
    CHECK(b3 == b1, "reuso do buraco");
    CHECK(!amdgpu_vram_alloc(&a, 1ull << 30), "pedido maior que a VRAM");
    amdgpu_vram_free(&a, 0xE0000000ull, 4096);                 /* GOP não é do alocador: ignora */
    int n = 0;
    while (amdgpu_vram_alloc(&a, 1u << 20)) n++;
    CHECK(n > 150 && amdgpu_vram_free_bytes(&a) < (1u << 20), "esgota a VRAM (%d MiB)", n);

    /* troca que nunca acontece (OTG parado): timeout sem travar */
    armed = false;
    regs[R_OTG_V_BLANK_START_END(0) / 4] = 0x7FFF;              /* vblank nunca chega na simulação */
    int r = amdgpu_flip(&a, b2, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true);
    CHECK(r == -MOS_ETIMEDOUT && a.flip_timeouts == 1, "timeout %d", r);
}

static u32 *scr;
static void paint(int sw, int x, int y, int w, int h, u32 v) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) scr[(usize)(y + j) * sw + x + i] = v;
}
/* o que o monitor mostra agora: o buffer cujo endereço MC está em uso */
static bool shown_equals(const amdgpu_t *a, int sw, int ox, int oy, int w, int h) {
    u64 mc = (u64)regs[R_DCSURF_EARLIEST_INUSE_HI(0) / 4] << 32 | regs[R_DCSURF_EARLIEST_INUSE(0) / 4];
    u8 *base = amd_host_phys(0xE0000000ull + (mc - a->fb_start));
    u32 stride = (regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) * 4 + 4;
    for (int j = 0; j < h; j++)
        if (memcmp(base + (usize)j * stride, scr + (usize)(oy + j) * sw + ox, (usize)w * 4)) return false;
    return true;
}

static void test_present(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    amdgpu_t a;
    amdgpu_probe(&a, &d, 0xE0000000ull, 5120, 1440, 5120);
    CHECK(wc_lo == 0xE0000000ull + (29ull << 20) && wc_len == (211ull << 20), "WC 0x%llx +%llu", wc_lo, wc_len);
    /* tela de 7680x1440 com a saída 0 em x=2560 (arranjo com outro monitor à esquerda) */
    int sw = 7680, sh = 1440, ox = 2560;
    scr = calloc((usize)sw * sh, 4);
    paint(sw, 0, 0, sw, sh, 0x00336699);
    CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, 0, 0, sw, sh) && a.scr_on && a.scr_front == 1, "primeiro present");
    scan(1485);
    CHECK(shown_equals(&a, sw, ox, 0, 5120, 1440), "quadro 1 na tela");
    u32 vfree = (u32)(amdgpu_vram_free_bytes(&a) >> 20);
    CHECK(vfree == 211 - 29, "buffer de trás ocupa 29 MiB (%u livres)", vfree);
    /* série de áreas sujas: a tela tem de ficar sempre igual à superfície */
    srand(7);
    for (int k = 0; k < 40; k++) {
        int x = rand() % sw, y = rand() % sh, w = 1 + rand() % 900, h = 1 + rand() % 500;
        if (x + w > sw) w = sw - x;
        if (y + h > sh) h = sh - y;
        paint(sw, x, y, w, h, (u32)rand());
        CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, x, y, w, h), "present %d", k);
        scan(1485);
        CHECK(shown_equals(&a, sw, ox, 0, 5120, 1440), "tela diferente da superfície no quadro %d", k);
    }
    CHECK(a.flip_timeouts == 0 && a.presents >= 20, "presents %llu", a.presents);
    /* área só no outro monitor: nada muda nesta saída */
    u64 flips = a.flips;
    paint(sw, 10, 10, 100, 100, 0xFF);
    CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, 10, 10, 100, 100) && a.flips == flips, "fora da saída");
    /* superfície menor que a saída: recusa */
    CHECK(!amdgpu_present(&a, scr, 1920, 1080, 0, 0, 0, 0, 10, 10), "superfície menor");
    /* troca travada: desliga o buffer duplo, devolve o GOP com o conteúdo atual */
    regs[R_OTG_V_BLANK_START_END(0) / 4] = 0x7FFF;
    paint(sw, ox, 0, 64, 64, 0x123456);
    CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, ox, 0, 64, 64), "present antes do travamento");
    paint(sw, ox + 100, 0, 64, 64, 0x654321);
    bool r = amdgpu_present(&a, scr, sw, sh, ox, 0, ox + 100, 0, 64, 64);
    CHECK(!r && a.scr_failed && !a.scr_on && a.scr_front == 0, "desliga no travamento");
    CHECK(regs[R_DCSURF_PRIMARY_ADDR(0) / 4] == (u32)FB_MC, "voltou ao GOP");
    CHECK(amdgpu_vram_free_bytes(&a) == 211ull << 20, "buffer de trás devolvido");
    CHECK(!amdgpu_present(&a, scr, sw, sh, ox, 0, 0, 0, 1, 1), "fica desligado");
    /* o GOP tem o quadro que estava na frente (sem a última área, que fica com a cópia direta de quem chamou) */
    u8 *gop = amd_host_phys(0xE0000000ull);
    CHECK(!memcmp(gop, scr + ox, 64 * 4), "GOP recebeu a frente");
    free(scr);
}

int main(int argc, char **argv) {
    verbose = argc > 1 && argv[1][0] == '-';
    test_otg_roundtrip();
    test_probe_and_vsync();
    test_flip_and_vram();
    test_present();
    printf("amdgpu-hw-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}

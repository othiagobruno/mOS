/* mOS — teste de host de kernel/dev/amdgpu/amdgpu.c contra um Picasso simulado.
 * O "chip" é o arquivo de registradores do BAR5 (512 KiB) com os efeitos que o driver usa: o OTG varre
 * linhas a cada leitura da posição e conta quadros, a trava de atualização responde no bit de status, e o
 * endereço novo do plano só vale (EARLIEST_INUSE) no vblank depois de soltar a trava.
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu -I tools tools/amdgpu_hw_host.c kernel/dev/amdgpu/engine.c \
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

#define CARVE 0x3F0000000ull     /* MC_VM_FB_OFFSET do boot_state */
/* ---------- motores simulados: SMU, MMHUB e SDMA que executa os pacotes ---------- */
static bool smu_alive = true, smu_sdma_on, sdma_dead;
static u32 ucode_words, ucode_bad, ucode_loaded_ver;
static const u32 *ucode_ref; static u32 ucode_ref_dw;
static u64 sdma_packets, sdma_faults;
static u32 sim_fb_lo(void) { return regs[R_MC_VM_FB_LOCATION_BASE / 4]; }
static u8 *mc_ptr(u64 mc, bool write) {
    u64 fb0 = (u64)sim_fb_lo() << 24, fb1 = ((u64)regs[R_MC_VM_FB_LOCATION_TOP / 4] << 24) | 0xFFFFFF;
    u64 lo = (u64)regs[R_MC_VM_SYS_APERTURE_LOW / 4] << 18, hi = ((u64)regs[R_MC_VM_SYS_APERTURE_HIGH / 4] << 18) | 0x3FFFF;
    if (mc >= fb0 && mc <= fb1) {
        if (mc < lo || mc > hi) { sdma_faults++; return NULL; }       /* fora da abertura do sistema */
        return amd_host_phys(CARVE + (mc - fb0));
    }
    u64 gs = ((u64)regs[R_VM_CTX0_PT_START_HI / 4] << 44) | ((u64)regs[R_VM_CTX0_PT_START_LO / 4] << 12);
    u64 ge = ((u64)regs[R_VM_CTX0_PT_END_HI / 4] << 44) | ((u64)regs[R_VM_CTX0_PT_END_LO / 4] << 12) | 0xFFF;
    if (!(regs[R_VM_CONTEXT0_CNTL / 4] & CTX0_ENABLE) || mc < gs || mc > ge) { sdma_faults++; return NULL; }
    u64 pt = ((u64)regs[R_VM_CTX0_PT_BASE_HI / 4] << 32) | regs[R_VM_CTX0_PT_BASE_LO / 4];
    u64 *tbl = amd_host_phys(CARVE + (pt - fb0));
    u64 pte = tbl[(mc - gs) >> 12];
    if (!(pte & AMD_PTE_VALID) || !(pte & AMD_PTE_SYSTEM) || (write && !(pte & AMD_PTE_WRITE))) { sdma_faults++; return NULL; }
    return (u8 *)(uintptr_t)((pte & AMD_PTE_ADDR_MASK) + (mc & 0xFFF));
}
static void mc_xfer(u64 dst, u64 src, u64 n, bool fill, u32 val) {
    while (n) {
        u64 step = 4096 - (dst & 0xFFF);
        if (!fill && 4096 - (src & 0xFFF) < step) step = 4096 - (src & 0xFFF);
        if (step > n) step = n;
        u8 *d = mc_ptr(dst, true), *s = fill ? NULL : mc_ptr(src, false);
        if (!d || (!fill && !s)) return;
        if (fill) for (u64 i = 0; i < step; i += 4) memcpy(d + i, &val, 4);
        else memmove(d, s, step);
        dst += step; src += step; n -= step;
    }
}
static void sdma_run(void) {
    u32 rb = regs[R_SDMA_GFX_RB_CNTL / 4];
    if (sdma_dead || !smu_sdma_on || (regs[R_SDMA_F32_CNTL / 4] & SDMA_F32_HALT) || !(rb & SDMA_RB_ENABLE) ||
        !ucode_loaded_ver || ucode_bad) return;
    u32 dw = 1u << ((rb & SDMA_RB_SIZE_MASK) >> SDMA_RB_SIZE_SHIFT);
    u64 base = ((u64)regs[R_SDMA_GFX_RB_BASE_HI / 4] << 40) | ((u64)regs[R_SDMA_GFX_RB_BASE / 4] << 8);
    u32 *ring = (u32 *)mc_ptr(base, false);
    if (!ring) return;
    u32 rp = regs[R_SDMA_GFX_RB_RPTR / 4] >> 2, wp = regs[R_SDMA_GFX_RB_WPTR / 4] >> 2;
    #define RD(k) ring[(rp + (k)) & (dw - 1)]
    while (rp != wp) {
        u32 h = RD(0), op = h & 0xFF;
        u32 len = 1;
        if (op == SDMA_OP_NOP) len = 1;
        else if (op == SDMA_OP_COPY) { mc_xfer(((u64)RD(6) << 32) | RD(5), ((u64)RD(4) << 32) | RD(3), (u64)RD(1) + 1, false, 0); len = 7; }
        else if (op == SDMA_OP_CONST_FILL) { mc_xfer(((u64)RD(2) << 32) | RD(1), 0, (u64)RD(4) + 1, true, RD(3)); len = 5; }
        else if (op == SDMA_OP_FENCE) { u8 *d = mc_ptr(((u64)RD(2) << 32) | RD(1), true); u32 v = RD(3); if (d) memcpy(d, &v, 4); len = 4; }
        else if (op == SDMA_OP_WRITE) {
            u32 cnt = RD(3) + 1; u64 a = ((u64)RD(2) << 32) | RD(1);
            for (u32 k = 0; k < cnt; k++) { u8 *d = mc_ptr(a + 4 * k, true); u32 v = RD(4 + k); if (d) memcpy(d, &v, 4); }
            len = 4 + cnt;
        } else if (op == SDMA_OP_TRAP) len = 2;
        else { printf("FALHOU: pacote SDMA desconhecido 0x%x\n", h); total++; break; }
        sdma_packets++;
        rp = (rp + len) & (dw - 1);
    }
    #undef RD
    regs[R_SDMA_GFX_RB_RPTR / 4] = rp << 2;
    if (rb & SDMA_RPTR_WB_ENABLE) {
        u64 wa = ((u64)regs[R_SDMA_GFX_RB_RPTR_ADDR_HI / 4] << 32) | regs[R_SDMA_GFX_RB_RPTR_ADDR_LO / 4];
        u8 *d = mc_ptr(wa, true); u32 v = rp << 2;
        if (d) memcpy(d, &v, 4);
    }
}
static bool engine_wreg(u32 off, u32 v) {
    if (off == R_MP1_C2PMSG_66) {
        if (!smu_alive) return true;
        regs[R_MP1_C2PMSG_90 / 4] = SMU_RESP_OK;
        if (v == SMU_MSG_GET_VERSION) regs[R_MP1_C2PMSG_82 / 4] = 0x41E4D;
        else if (v == SMU_MSG_POWER_UP_SDMA) smu_sdma_on = true;
        else if (v == SMU_MSG_POWER_DOWN_SDMA) smu_sdma_on = false;
        else regs[R_MP1_C2PMSG_90 / 4] = 0xFF;                        /* mensagem desconhecida */
        regs[R_MP1_C2PMSG_66 / 4] = v;
        return true;
    }
    if (off == R_VM_INV_ENG17_REQ) { regs[off / 4] = v; regs[R_VM_INV_ENG17_ACK / 4] |= v & 0xFFFF; return true; }
    if (off == R_SDMA_UCODE_ADDR) {
        if (v == 0) { ucode_words = 0; ucode_bad = 0; ucode_loaded_ver = 0; }
        else if (ucode_words == ucode_ref_dw) ucode_loaded_ver = v;
        regs[off / 4] = v; return true;
    }
    if (off == R_SDMA_UCODE_DATA) {
        if (ucode_words >= ucode_ref_dw || v != ucode_ref[ucode_words]) ucode_bad++;
        ucode_words++; return true;
    }
    if (off == R_SDMA_GFX_RB_WPTR) { regs[off / 4] = v; sdma_run(); return true; }
    if (off == R_SDMA_GFX_RB_RPTR) { regs[off / 4] = v; return true; }
    return false;
}

u32 amd_host_rreg(u32 off) {
    if (off == R_VM_INV_ENG17_SEM) return 1;
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
    if (engine_wreg(off, v)) return;
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
/* VRAM pelo BAR0 (0xE0000000) ou pelo carve-out (MC_VM_FB_OFFSET = 0x3F0000000); o resto é RAM do teste */

void *amd_host_phys(u64 phys) {
    if (!vram) vram = calloc(1, 256u << 20);
    if (phys >= 0xE0000000ull && phys < 0xE0000000ull + (256ull << 20)) return vram + (phys - 0xE0000000ull);
    if (phys >= CARVE && phys < CARVE + (256ull << 20)) return vram + (phys - CARVE);
    return (void *)(uintptr_t)phys;
}
const u8 *amd_host_fw(const char *name, u64 *len) {
    static u8 *cache[8]; static char names[8][32]; static u64 lens[8];
    for (int i = 0; i < 8; i++) if (cache[i] && !strcmp(names[i], name)) { *len = lens[i]; return cache[i]; }
    char path[96];
    snprintf(path, sizeof path, "firmware/amdgpu/%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = aligned_alloc(16, ((usize)n + 15) & ~(usize)15);
    if (fread(b, 1, (usize)n, f) != (usize)n) { fclose(f); return NULL; }
    fclose(f);
    for (int i = 0; i < 8; i++) if (!cache[i]) { cache[i] = b; snprintf(names[i], 32, "%s", name); lens[i] = (u64)n; break; }
    *len = (u64)n;
    return b;
}
u64 amd_host_ram_page(void) { return (u64)(uintptr_t)aligned_alloc(4096, 4096); }
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
    regs[R_MP1_C2PMSG_90 / 4] = SMU_RESP_OK;          /* a UEFI/o SMU deixam a última resposta (1) */
    regs[R_SDMA_F32_CNTL / 4] = SDMA_F32_HALT;
    regs[R_MC_VM_MX_L1_TLB_CNTL / 4] = 0x2501;        /* valores de reset da MMHUB 1.0 */
    regs[R_VM_L2_CNTL / 4] = 0x00080602;
    regs[R_VM_CONTEXT0_CNTL / 4] = 0x007FFE80;
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

static amdgpu_t eng;      /* grande (bitmap da GART): fora da pilha */
static bool engines_up(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    if (!amdgpu_probe(&eng, &d, 0xE0000000ull, 5120, 1440, 5120)) return false;
    u64 n;
    const u8 *fw = amd_host_fw("picasso_sdma.bin", &n);
    amd_ucode_t uc;
    if (!fw || amd_ucode_parse(fw, (usize)n, UC_KIND_SDMA, &uc)) return false;
    ucode_ref = uc.ucode; ucode_ref_dw = uc.ucode_dwords;
    smu_sdma_on = false; ucode_loaded_ver = 0; sdma_faults = 0;
    return amdgpu_engines_init(&eng);
}
static u8 *bar_ptr(u64 bar) { return amd_host_phys(bar); }

static void test_engines(void) {
    smu_alive = true; sdma_dead = false;
    CHECK(engines_up(), "motores");
    amdgpu_t *a = &eng;
    CHECK(a->smu_ok && a->smu_version == 0x41E4D && smu_sdma_on, "SMU");
    CHECK(a->gmc_ok && a->sdma_ok && ucode_loaded_ver == 0xA9 && !ucode_bad && ucode_words == ucode_ref_dw, "microcódigo %u/%u", ucode_words, ucode_ref_dw);
    CHECK(regs[R_MC_VM_SYS_APERTURE_LOW / 4] == (u32)(FB_MC >> 18) && regs[R_MC_VM_SYS_APERTURE_HIGH / 4] == (u32)((FB_MC + (256ull << 20) - 1) >> 18), "abertura do sistema");
    CHECK((regs[R_VM_CONTEXT0_CNTL / 4] & (CTX0_ENABLE | CTX0_DEPTH_MASK)) == CTX0_ENABLE, "contexto 0 com tabela plana");
    CHECK((regs[R_MC_VM_MX_L1_TLB_CNTL / 4] & (L1_TLB_ENABLE | L1_TLB_SYS_APERTURE_UNMAPPED | L1_TLB_MTYPE_MASK)) == (L1_TLB_ENABLE | L1_TLB_MTYPE_UC), "TLB L1");
    CHECK(a->gart_start == 0 && a->gart_size == (256ull << 20), "GART em MC 0");
    CHECK((regs[R_SDMA_GFX_RB_CNTL / 4] & (SDMA_RB_ENABLE | SDMA_RPTR_WB_ENABLE)) == (SDMA_RB_ENABLE | SDMA_RPTR_WB_ENABLE) &&
          ((regs[R_SDMA_GFX_RB_CNTL / 4] & SDMA_RB_SIZE_MASK) >> SDMA_RB_SIZE_SHIFT) == 14, "anel de 16 Ki dwords");
    CHECK((regs[R_SDMA_GB_ADDR_CONFIG / 4] & 0x0018773F) == 2 && (regs[R_SDMA_POWER_CNTL / 4] & 0xFC3FFFFF) == 0x40000051, "golden");
    CHECK(a->wb_cpu[8] == 0xDEADBEEF && sdma_faults == 0, "teste do anel");

    /* preenchimento e cópia na VRAM (9 MiB: passa do limite de 4 MiB por pacote) */
    u64 b1 = amdgpu_vram_alloc(a, 9u << 20), b2 = amdgpu_vram_alloc(a, 9u << 20);
    u32 s = amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1), 0x5A5A1234u, 9u << 20);
    CHECK(s && amdgpu_sdma_wait(a, s), "fill");
    u32 *p1 = (u32 *)bar_ptr(b1);
    CHECK(p1[0] == 0x5A5A1234u && p1[(9u << 18) - 1] == 0x5A5A1234u, "fill na VRAM");
    for (u32 i = 0; i < (9u << 18); i += 977) p1[i] = i * 2654435761u;
    s = amdgpu_sdma_copy(a, amdgpu_vram_to_mc(a, b2), amdgpu_vram_to_mc(a, b1), 9u << 20);
    CHECK(s && amdgpu_sdma_wait(a, s) && !memcmp(bar_ptr(b1), bar_ptr(b2), 9u << 20), "cópia VRAM -> VRAM");

    /* GART: RAM do sistema <-> VRAM */
    u8 *ram = aligned_alloc(4096, 64 * 4096);
    for (u32 i = 0; i < 64 * 4096; i++) ram[i] = (u8)(i * 7 + 3);
    u64 g = amdgpu_gart_map(a, (u64)(uintptr_t)ram, 64, true);
    CHECK(g == 0 && a->gart_cpu[0] && (a->gart_cpu[0] & AMD_PTE_ADDR_MASK) == (u64)(uintptr_t)ram, "GART map");
    u64 g2 = amdgpu_gart_map(a, (u64)(uintptr_t)ram, 1, false);
    CHECK(g2 == 64 * 4096ull, "segunda faixa logo depois");
    s = amdgpu_sdma_copy(a, amdgpu_vram_to_mc(a, b1), g, 64 * 4096);
    CHECK(s && amdgpu_sdma_wait(a, s) && !memcmp(bar_ptr(b1), ram, 64 * 4096), "RAM -> VRAM pela GART");
    memset(bar_ptr(b2), 0xC3, 64 * 4096);
    s = amdgpu_sdma_copy(a, g + 4096, amdgpu_vram_to_mc(a, b2), 8192);
    CHECK(s && amdgpu_sdma_wait(a, s) && ram[4096] == 0xC3 && ram[3 * 4096 - 1] == 0xC3 && ram[4095] != 0xC3 && ram[3 * 4096] != 0xC3, "VRAM -> RAM");
    u64 f0 = sdma_faults;
    s = amdgpu_sdma_copy(a, g2, amdgpu_vram_to_mc(a, b2), 16);
    CHECK(s && amdgpu_sdma_wait(a, s) && sdma_faults == f0 + 1, "página só de leitura recusa escrita");
    amdgpu_gart_unmap(a, g, 64);
    CHECK(a->gart_cpu[0] == 0 && a->gart_cpu[63] == 0 && a->gart_cpu[64], "unmap");
    f0 = sdma_faults;
    s = amdgpu_sdma_copy(a, amdgpu_vram_to_mc(a, b1), g, 4096);
    CHECK(s && amdgpu_sdma_wait(a, s) && sdma_faults == f0 + 1, "página desligada dá falta");
    CHECK(amdgpu_gart_map(a, (u64)(uintptr_t)ram, 64, true) == 0, "faixa reaproveitada");

    /* retângulo: superfície do compositor (7680 px) -> buffer da tela (5120 px) */
    u32 sw = 7680, rows = 300, rw = 1000, ox = 2560 + 37, oy = 11;
    u32 *surf = aligned_alloc(4096, (usize)sw * 4 * 320);
    for (u32 i = 0; i < sw * 320; i++) surf[i] = i ^ 0xA5A5A5A5u;
    u32 sp = (u32)(((usize)sw * 4 * 320 + 4095) / 4096);
    u64 gs = amdgpu_gart_map(a, (u64)(uintptr_t)surf, sp, false);
    CHECK(gs, "GART da superfície");
    s = amdgpu_sdma_copy_rect(a, amdgpu_vram_to_mc(a, b2) + (u64)oy * 5120 * 4 + 37 * 4, 5120 * 4,
                              gs + (u64)oy * sw * 4 + ox * 4, sw * 4, rw * 4, rows);
    CHECK(s && amdgpu_sdma_wait(a, s), "copy_rect");
    bool ok = true;
    u32 *dst = (u32 *)bar_ptr(b2);
    for (u32 r = 0; r < rows && ok; r++) ok = !memcmp(dst + (usize)(oy + r) * 5120 + 37, surf + (usize)(oy + r) * sw + ox, rw * 4);
    CHECK(ok && dst[(usize)oy * 5120 + 36] == 0xC3C3C3C3u, "retângulo no lugar certo");

    /* volta do anel: 6000 pacotes (mais de 5 voltas de 16 Ki dwords) */
    u32 last = 0;
    for (u32 k = 0; k < 6000; k++) last = amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1) + (k % 1024) * 4, k, 4);
    CHECK(last && amdgpu_sdma_wait(a, last) && amdgpu_sdma_done(a) == last && p1[5999 % 1024] == 5999 && a->sdma_timeouts == 0, "anel deu a volta");
    CHECK(sdma_faults == 2, "só as duas faltas provocadas (%llu)", sdma_faults);
    CHECK(!amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1) + 2, 0, 4) && !amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1), 0, 6), "fill desalinhado");
    CHECK(!amdgpu_engines_init(a) || a->sdma_ok, "segunda chamada não refaz");

    /* falhas: SMU mudo, SDMA morto */
    smu_alive = false;
    CHECK(!engines_up() && !eng.smu_ok && !eng.sdma_ok, "SMU mudo");
    smu_alive = true; sdma_dead = true;
    CHECK(!engines_up() && eng.gmc_ok && !eng.sdma_ok && (regs[R_SDMA_F32_CNTL / 4] & SDMA_F32_HALT), "SDMA morto fica parado");
    CHECK(!amdgpu_sdma_copy(&eng, 0, 0, 4), "sem SDMA não envia");
    sdma_dead = false;
    free(ram); free(surf);
}

int main(int argc, char **argv) {
    verbose = argc > 1 && argv[1][0] == '-';
    setvbuf(stdout, NULL, _IONBF, 0);
    test_otg_roundtrip();
    test_probe_and_vsync();
    test_flip_and_vram();
    test_present();
    test_engines();
    printf("amdgpu-hw-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}

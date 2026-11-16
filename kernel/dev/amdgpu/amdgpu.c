/* mOS — driver da GPU AMD família Raven: registradores, estado do display, vsync, page flip, VRAM.
 * Ver amdgpu.h. Testado no host contra um modelo de registradores (tools/amdgpu_host.c). */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define PHYS_PTR(p) ((u8 *)amd_host_phys(p))
#else
#define PHYS_PTR(p) ((u8 *)(usize)(p))     /* mapa de identidade do kernel */
void vmm_map_wc(u64 phys, u64 size);        /* mm/memory.c */
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#endif

static amdgpu_t *g_main;
amdgpu_t *amdgpu_main(void) { return g_main && g_main->ready ? g_main : NULL; }
void amdgpu_set_main(amdgpu_t *a) { g_main = a; }

/* ------------------------------------------------------------ timing do OTG (lógica pura) */
void amd_otg_decode(const u32 r[8], amd_otg_timing_t *t) {
    u32 ht = (r[0] & OTG_TIMING_MASK) + 1;
    u32 hbs = r[1] & OTG_TIMING_MASK, hbe = (r[1] >> 16) & OTG_TIMING_MASK;
    u32 hss = r[2] & OTG_TIMING_MASK, hse = (r[2] >> 16) & OTG_TIMING_MASK;
    u32 vt = (r[4] & OTG_TIMING_MASK) + 1;
    u32 vbs = r[5] & OTG_TIMING_MASK, vbe = (r[5] >> 16) & OTG_TIMING_MASK;
    u32 vss = r[6] & OTG_TIMING_MASK, vse = (r[6] >> 16) & OTG_TIMING_MASK;
    t->htotal = (u16)ht;
    t->hactive = (u16)(hbs > hbe ? hbs - hbe : 0);
    t->hfront = (u16)(ht > hbs ? ht - hbs : 0);
    t->hsync = (u16)(hse > hss ? hse - hss : 0);
    t->hback = (u16)(ht > (u32)t->hactive + t->hfront + t->hsync ? ht - t->hactive - t->hfront - t->hsync : 0);
    t->vtotal = (u16)vt;
    t->vactive = (u16)(vbs > vbe ? vbs - vbe : 0);
    t->vfront = (u16)(vt > vbs ? vt - vbs : 0);
    t->vsync = (u16)(vse > vss ? vse - vss : 0);
    t->vback = (u16)(vt > (u32)t->vactive + t->vfront + t->vsync ? vt - t->vactive - t->vfront - t->vsync : 0);
    t->hpos = !(r[3] & OTG_SYNC_POL_NEG);
    t->vpos = !(r[7] & OTG_SYNC_POL_NEG);
}

void amd_otg_encode(const edid_mode_t *m, u32 r[8]) {
    u32 ht = edid_htotal(m), vt = edid_vtotal(m);
    u32 hbs = ht - m->hfront, hbe = hbs - m->hactive;
    u32 vbs = vt - m->vfront, vbe = vbs - m->vactive;
    r[0] = (ht - 1) & OTG_TIMING_MASK;
    r[1] = (hbs & OTG_TIMING_MASK) | (hbe & OTG_TIMING_MASK) << 16;
    r[2] = (u32)(m->hsync & OTG_TIMING_MASK) << 16;
    r[3] = (m->flags & EDID_MODE_HPOS) ? 0 : OTG_SYNC_POL_NEG;
    r[4] = (vt - 1) & OTG_TIMING_MASK;
    r[5] = (vbs & OTG_TIMING_MASK) | (vbe & OTG_TIMING_MASK) << 16;
    r[6] = (u32)(m->vsync & OTG_TIMING_MASK) << 16;
    r[7] = (m->flags & EDID_MODE_VPOS) ? 0 : OTG_SYNC_POL_NEG;
}

void amd_otg_to_mode(const amd_otg_timing_t *t, edid_mode_t *m) {
    memset(m, 0, sizeof *m);
    m->hactive = t->hactive; m->hfront = t->hfront; m->hsync = t->hsync; m->hback = t->hback;
    m->vactive = t->vactive; m->vfront = t->vfront; m->vsync = t->vsync; m->vback = t->vback;
    if (t->hpos) m->flags |= EDID_MODE_HPOS;
    if (t->vpos) m->flags |= EDID_MODE_VPOS;
    if (t->interlace) m->flags |= EDID_MODE_INTERLACE;
}

/* ------------------------------------------------------------ leitura do estado */
static void read_otg(amdgpu_t *a, int i) {
    amd_otg_timing_t *t = &a->otg[i];
    memset(t, 0, sizeof *t);
    u32 ctl = RR(a, R_OTG_CONTROL(i));
    if (ctl == 0xFFFFFFFFu || !(ctl & OTG_MASTER_EN)) return;
    u32 r[8] = {
        RR(a, R_OTG_H_TOTAL(i)), RR(a, R_OTG_H_BLANK_START_END(i)), RR(a, R_OTG_H_SYNC_A(i)), RR(a, R_OTG_H_SYNC_A_CNTL(i)),
        RR(a, R_OTG_V_TOTAL(i)), RR(a, R_OTG_V_BLANK_START_END(i)), RR(a, R_OTG_V_SYNC_A(i)), RR(a, R_OTG_V_SYNC_A_CNTL(i)),
    };
    amd_otg_decode(r, t);
    t->interlace = RR(a, R_OTG_INTERLACE_CONTROL(i)) & 1;
    t->on = t->hactive && t->vactive;
}

static void read_plane(amdgpu_t *a, int i) {
    amd_plane_t *p = &a->plane[i];
    memset(p, 0, sizeof *p);
    u32 cntl = RR(a, R_DCHUBP_CNTL(i));
    if (cntl == 0xFFFFFFFFu || (cntl & (HUBP_BLANK_EN | HUBP_DISABLE))) return;
    p->format = RR(a, R_DCSURF_SURFACE_CONFIG(i)) & SURFACE_PIXEL_FORMAT_MASK;
    p->sw_mode = RR(a, R_DCSURF_TILING_CONFIG(i)) & TILING_SW_MODE_MASK;
    p->pitch_px = (RR(a, R_DCSURF_SURFACE_PITCH(i)) & PITCH_MASK) + 1;
    u32 vs = RR(a, R_DCSURF_PRI_VIEWPORT_START(i)), vd = RR(a, R_DCSURF_PRI_VIEWPORT_DIM(i));
    p->vp_x = vs & VIEWPORT_W_MASK; p->vp_y = (vs >> VIEWPORT_H_SHIFT) & VIEWPORT_W_MASK;
    p->vp_w = vd & VIEWPORT_W_MASK; p->vp_h = (vd >> VIEWPORT_H_SHIFT) & VIEWPORT_W_MASK;
    p->addr = (u64)(RR(a, R_DCSURF_PRIMARY_ADDR_HI(i)) & 0xFFFF) << 32 | RR(a, R_DCSURF_PRIMARY_ADDR(i));
    u32 xb = RR(a, R_HUBPRET_CONTROL(i));
    p->rgba = ((xb >> XBAR_CB_B_SHIFT) & 3) == 3 && ((xb >> XBAR_CR_R_SHIFT) & 3) == 2;
    p->on = p->vp_w && p->vp_h;
}

static u32 measure_refresh(amdgpu_t *a) {
    int i = a->pipe;
    u32 f0 = RR(a, R_OTG_STATUS_FRAME_COUNT(i)) & OTG_FRAME_COUNT_MASK;
    u64 t0 = timer_ticks();
    sleep_ms(100);
    u32 f1 = RR(a, R_OTG_STATUS_FRAME_COUNT(i)) & OTG_FRAME_COUNT_MASK;
    u64 dt = timer_ticks() - t0;
    u32 frames = (f1 - f0) & OTG_FRAME_COUNT_MASK;
    return dt ? (u32)((u64)frames * 1000000ull / dt) : 0;
}

bool amdgpu_probe(amdgpu_t *a, pci_dev_t *d, u64 uefi_fb, u32 w, u32 h, u32 pitch_px) {
    memset(a, 0, sizeof *a);
    a->pci = d;
    a->pipe = -1;
    if (d->vendor != 0x1002 || (d->device != 0x15D8 && d->device != 0x15DD)) return false;
    a->pci_rev = (u8)(pci_read(d, 0x08) & 0xFF);
    a->bar0 = pci_bar_addr(d, 0);
    a->mmio_phys = pci_bar_addr(d, 5);
    if (!a->mmio_phys || !a->bar0) { kprintf("amdgpu: BAR0/BAR5 sem endereço\n"); return false; }
    if (!(pci_read(d, 0x04) & 2)) { kprintf("amdgpu: decodificação de memória desligada\n"); return false; }
#ifdef MOS_HOST_TEST
    a->mmio = NULL;
#else
    vmm_map_mmio(a->mmio_phys, AMD_MMIO_SIZE);
    a->mmio = (volatile u8 *)a->mmio_phys;
#endif
    u32 strap = RR(a, R_RCC_DEV0_EPF0_STRAP0);
    if (strap == 0xFFFFFFFFu) { kprintf("amdgpu: registradores não respondem (0xFFFFFFFF)\n"); return false; }
    a->rev_id = (strap & STRAP0_ATI_REV_ID_MASK) >> STRAP0_ATI_REV_ID_SHIFT;
    a->variant = amd_raven_variant(d->device, a->rev_id);
    amd_raven_rlc_name(a->variant, a->pci_rev, 0, a->rlc_name, sizeof a->rlc_name);

    a->fb_start = (u64)(RR(a, R_MC_VM_FB_LOCATION_BASE) & MC_FB_MASK) << 24;
    a->fb_top = ((u64)(RR(a, R_MC_VM_FB_LOCATION_TOP) & MC_FB_MASK) << 24) | 0xFFFFFF;
    a->fb_phys = (u64)(RR(a, R_MC_VM_FB_OFFSET) & MC_FB_MASK) << 24;
    a->vram_size = a->fb_top > a->fb_start ? a->fb_top - a->fb_start + 1 : 0;
    kprintf("amdgpu: %s (rev PCI %02x, rev_id %u), RLC %s; VRAM %llu MiB em MC 0x%llx, física 0x%llx\n",
            amd_raven_fw_prefix(a->variant), a->pci_rev, a->rev_id, a->rlc_name, a->vram_size >> 20, a->fb_start, a->fb_phys);
    if (!a->vram_size || a->vram_size > (4ull << 30)) { kprintf("amdgpu: faixa de VRAM sem sentido\n"); return false; }

    /* display: OTGs e planos; o pipe da tela é o plano que aponta para o framebuffer do GOP */
    a->uefi_mc = uefi_fb >= a->bar0 ? a->fb_start + (uefi_fb - a->bar0) : 0;
    for (int i = 0; i < AMD_NUM_OTG; i++) {
        read_otg(a, i);
        read_plane(a, i);
        amd_otg_timing_t *t = &a->otg[i];
        amd_plane_t *p = &a->plane[i];
        if (!t->on) continue;
        bool match = p->on && p->addr == a->uefi_mc && p->vp_w == w && p->vp_h == h;
        kprintf("amdgpu:   OTG%d %ux%u total %ux%u%s; plano %s %ux%u pitch %u fmt %u sw %u MC 0x%llx%s\n", i,
                t->hactive, t->vactive, t->htotal, t->vtotal, t->interlace ? " entrelaçado" : "",
                p->on ? "ligado" : "desligado", p->vp_w, p->vp_h, p->pitch_px, p->format, p->sw_mode, p->addr,
                match ? " = framebuffer do GOP" : "");
        if (match && (a->pipe < 0 || !a->uefi_match)) { a->pipe = i; a->uefi_match = true; }
        else if (a->pipe < 0 && p->on) a->pipe = i;
    }
    if (a->pipe >= 0) {
        a->uefi_pitch_px = a->uefi_match ? a->plane[a->pipe].pitch_px : pitch_px;
        a->uefi_rgba = a->plane[a->pipe].rgba;
    }
    for (int i = 0; i < AMD_NUM_HPD; i++)
        if (RR(a, R_DC_HPD_INT_STATUS(i)) & DC_HPD_SENSE) a->hpd_mask |= 1u << i;

    /* VRAM alocável: depois do framebuffer do GOP; 16 MiB no topo ficam para o firmware (DMCU/PSP/stolen) */
    u64 visible = a->vram_size < (u64)AMD_VRAM_CHUNKS * AMD_VRAM_CHUNK ? a->vram_size : (u64)AMD_VRAM_CHUNKS * AMD_VRAM_CHUNK;
    u64 fb_end = uefi_fb >= a->bar0 ? uefi_fb - a->bar0 + (u64)pitch_px * 4 * h : 0;
    u64 lo = (fb_end + (1u << 20) - 1) & ~(u64)((1u << 20) - 1);
    u64 hi = visible > (16u << 20) ? visible - (16u << 20) : 0;
    a->vram_lo = (u32)(lo / AMD_VRAM_CHUNK);
    a->vram_hi = hi > lo ? (u32)(hi / AMD_VRAM_CHUNK) : a->vram_lo;

    /* a VRAM alocável passa a write-combining no mapa do kernel (o GOP já é); o resto do BAR fica UC */
    if (a->vram_hi > a->vram_lo) vmm_map_wc(a->bar0 + (u64)a->vram_lo * AMD_VRAM_CHUNK, (u64)(a->vram_hi - a->vram_lo) * AMD_VRAM_CHUNK);

    a->ready = true;
    if (a->pipe >= 0 && a->otg[a->pipe].on) {
        a->refresh_mhz = measure_refresh(a);
        u32 hz = a->refresh_mhz / 1000;
        a->vsync_ok = hz >= 20 && hz <= 250;
        kprintf("amdgpu: tela no OTG%d%s, %u.%03u Hz medidos, vsync %s; monitores por HPD 0x%x; VRAM livre %llu MiB\n",
                a->pipe, a->uefi_match ? " (framebuffer do GOP)" : "", a->refresh_mhz / 1000, a->refresh_mhz % 1000,
                a->vsync_ok ? "ligado" : "desligado", a->hpd_mask, amdgpu_vram_free_bytes(a) >> 20);
    } else {
        kprintf("amdgpu: nenhum OTG ligado mostra a tela; vsync desligado\n");
    }
    return true;
}

/* ------------------------------------------------------------ vsync */
bool amdgpu_in_vblank(amdgpu_t *a) {
    return a->pipe >= 0 && (RR(a, R_OTG_STATUS(a->pipe)) & OTG_V_BLANK);
}

u32 amdgpu_frame_count(amdgpu_t *a) {
    return a->pipe >= 0 ? RR(a, R_OTG_STATUS_FRAME_COUNT(a->pipe)) & OTG_FRAME_COUNT_MASK : 0;
}

void amdgpu_vblank_wait(amdgpu_t *a) {
    if (!a || !a->ready || !a->vsync_ok || a->pipe < 0) return;
    u64 now = timer_ticks();
    if (a->last_vblank_ms && now - a->last_vblank_ms < 5) return;   /* mesma composição */
    int i = a->pipe;
    u32 f0 = RR(a, R_OTG_STATUS_FRAME_COUNT(i)) & OTG_FRAME_COUNT_MASK;
    u32 vbs = (u32)a->otg[i].vtotal - a->otg[i].vfront;   /* começo do blank vertical */
    for (u32 n = 0; n < 400000; n++) {
        if (RR(a, R_OTG_STATUS(i)) & OTG_V_BLANK) break;
        u32 pos = RR(a, R_OTG_STATUS_POSITION(i)) & OTG_VERT_COUNT_MASK;
        if (pos >= vbs) break;
        if ((RR(a, R_OTG_STATUS_FRAME_COUNT(i)) & OTG_FRAME_COUNT_MASK) != f0) break;
#ifndef MOS_HOST_TEST
        __asm__ volatile("pause");
#endif
    }
    a->last_vblank_ms = timer_ticks();
}

/* ------------------------------------------------------------ page flip */
static void otg_lock(amdgpu_t *a, int i, bool on) {
    WR(a, R_OTG_MASTER_UPDATE_LOCK(i), on ? OTG_UPDATE_LOCK : 0);
    if (!on) return;
    for (u32 n = 0; n < 100000; n++)
        if (RR(a, R_OTG_MASTER_UPDATE_LOCK(i)) & OTG_UPDATE_LOCK_STATUS) return;
}

int amdgpu_flip(amdgpu_t *a, u64 surf_phys, u32 w, u32 h, u32 stride, u32 format, bool wait) {
    if (!a || !a->ready || a->pipe < 0) return -MOS_ENOENT;
    int i = a->pipe;
    const amd_otg_timing_t *t = &a->otg[i];
    if (format != MOS_GPU_FMT_BGRA8 && format != MOS_GPU_FMT_RGBA8) return -MOS_EINVAL;
    u64 mc;
    u32 pitch_px;
    bool rgba;
    if (!surf_phys) {
        if (!a->uefi_match) return -MOS_ENOENT;
        mc = a->uefi_mc; pitch_px = a->uefi_pitch_px; rgba = a->uefi_rgba;
        w = t->hactive; h = t->vactive;
    } else {
        if (w != t->hactive || h != t->vactive) return -MOS_EINVAL;   /* outro tamanho = troca de modo */
        if (!stride || (stride & 255) || stride < w * 4 || stride / 4 > PITCH_MASK + 1) return -MOS_EINVAL;
        if ((surf_phys & 255) || surf_phys < a->bar0) return -MOS_EINVAL;
        u64 off = surf_phys - a->bar0, bytes = (u64)stride * h;
        if (off + bytes > a->vram_size || off + bytes > (u64)AMD_VRAM_CHUNKS * AMD_VRAM_CHUNK) return -MOS_EINVAL;
        mc = a->fb_start + off; pitch_px = stride / 4; rgba = format == MOS_GPU_FMT_RGBA8;
    }
    otg_lock(a, i, true);
    WR(a, R_DCSURF_TILING_CONFIG(i), RR(a, R_DCSURF_TILING_CONFIG(i)) & ~TILING_SW_MODE_MASK);
    WR(a, R_DCSURF_SURFACE_CONFIG(i), (RR(a, R_DCSURF_SURFACE_CONFIG(i)) & ~SURFACE_PIXEL_FORMAT_MASK) | SURFACE_FMT_ARGB8888);
    WR(a, R_DCSURF_SURFACE_PITCH(i), (RR(a, R_DCSURF_SURFACE_PITCH(i)) & ~PITCH_MASK) | ((pitch_px - 1) & PITCH_MASK));
    u32 xb = RR(a, R_HUBPRET_CONTROL(i)) & ~XBAR_MASK;
    xb |= rgba ? (3u << XBAR_CB_B_SHIFT | 2u << XBAR_CR_R_SHIFT) : (2u << XBAR_CB_B_SHIFT | 3u << XBAR_CR_R_SHIFT);
    WR(a, R_HUBPRET_CONTROL(i), xb);
    WR(a, R_DCSURF_FLIP_CONTROL(i), RR(a, R_DCSURF_FLIP_CONTROL(i)) & ~FLIP_TYPE_IMMEDIATE);   /* no vblank */
    WR(a, R_DCSURF_PRIMARY_ADDR_HI(i), (u32)(mc >> 32) & 0xFFFF);
    WR(a, R_DCSURF_PRIMARY_ADDR(i), (u32)mc);                       /* o endereço baixo arma a troca */
    otg_lock(a, i, false);
    a->plane[i].addr = mc; a->plane[i].pitch_px = pitch_px; a->plane[i].rgba = rgba;
    a->plane[i].format = SURFACE_FMT_ARGB8888; a->plane[i].sw_mode = 0;
    a->flips++;
    if (!wait) return 0;
    for (u32 n = 0; n < 2000000; n++) {
        u32 fc = RR(a, R_DCSURF_FLIP_CONTROL(i));
        u64 inuse = (u64)(RR(a, R_DCSURF_EARLIEST_INUSE_HI(i)) & 0xFFFF) << 32 | RR(a, R_DCSURF_EARLIEST_INUSE(i));
        if (!(fc & FLIP_PENDING) && inuse == mc) return 0;
#ifndef MOS_HOST_TEST
        __asm__ volatile("pause");
#endif
    }
    a->flip_timeouts++;
    return -MOS_ETIMEDOUT;
}

bool amdgpu_flip_done(amdgpu_t *a) {
    if (!a || !a->ready || a->pipe < 0) return true;
    int i = a->pipe;
    u64 mc = a->plane[i].addr;
    u64 inuse = (u64)(RR(a, R_DCSURF_EARLIEST_INUSE_HI(i)) & 0xFFFF) << 32 | RR(a, R_DCSURF_EARLIEST_INUSE(i));
    return !(RR(a, R_DCSURF_FLIP_CONTROL(i)) & FLIP_PENDING) && inuse == mc;
}

/* ------------------------------------------------------------ buffer duplo da tela */
static void copy_rect(amdgpu_t *a, int buf, const u32 *px, int sw, int ox, int oy, int x, int y, int w, int h) {
    u8 *dst = PHYS_PTR(a->scr_phys[buf]);
    for (int j = 0; j < h; j++)
        memcpy(dst + (usize)(y + j) * a->scr_stride + (usize)x * 4, px + (usize)(oy + y + j) * sw + ox + x, (usize)w * 4);
}

static void scr_disable(amdgpu_t *a, const char *why) {
    if (a->scr_on && a->scr_front != 0) {                        /* o GOP está um quadro atrás: recebe a frente */
        u32 rows = a->otg[a->pipe].vactive, bytes = (u32)a->otg[a->pipe].hactive * 4;
        for (u32 j = 0; j < rows; j++)
            memcpy(PHYS_PTR(a->scr_phys[0]) + (usize)j * a->scr_stride, PHYS_PTR(a->scr_phys[1]) + (usize)j * a->scr_stride, bytes);
        amdgpu_flip(a, 0, 0, 0, 0, MOS_GPU_FMT_BGRA8, true);
        a->scr_front = 0;
    }
    if (a->scr_phys[1]) amdgpu_vram_free(a, a->scr_phys[1], (u64)a->scr_stride * a->otg[a->pipe].vactive);
    a->scr_phys[1] = 0;
    a->scr_on = false;
    a->scr_failed = true;
    kprintf("amdgpu: buffer duplo desligado (%s); a tela volta à cópia direta no GOP\n", why);
}

bool amdgpu_present(amdgpu_t *a, const u32 *px, int sw, int sh, int ox, int oy, int x, int y, int rw, int rh) {
    if (!a || !a->ready || a->scr_failed || a->pipe < 0 || !a->uefi_match || !px) return false;
    const amd_otg_timing_t *t = &a->otg[a->pipe];
    int ow = t->hactive, oh = t->vactive;
    if (ox < 0 || oy < 0 || ox + ow > sw || oy + oh > sh) return false;
    /* área suja na saída */
    int x0 = MAX(x - ox, 0), y0 = MAX(y - oy, 0), x1 = MIN(x + rw - ox, ow), y1 = MIN(y + rh - oy, oh);
    if (!a->scr_on) {
        u32 stride = a->uefi_pitch_px * 4;
        if (stride & 255) { a->scr_failed = true; kprintf("amdgpu: pitch do GOP %u não alinhado a 256: sem buffer duplo\n", stride); return false; }
        u64 b1 = amdgpu_vram_alloc(a, (u64)stride * oh);
        if (!b1) { a->scr_failed = true; kprintf("amdgpu: sem VRAM para o buffer duplo\n"); return false; }
        a->scr_phys[0] = a->bar0 + (a->uefi_mc - a->fb_start);
        a->scr_phys[1] = b1;
        a->scr_stride = stride;
        a->scr_front = 0;
        a->scr_on = true;
        copy_rect(a, 1, px, sw, ox, oy, 0, 0, ow, oh);           /* quadro inteiro no buffer novo */
        a->prev_x = 0; a->prev_y = 0; a->prev_w = ow; a->prev_h = oh;
        if (amdgpu_flip(a, b1, (u32)ow, (u32)oh, stride, MOS_GPU_FMT_BGRA8, false)) { scr_disable(a, "troca recusada"); return false; }
        a->scr_front = 1;
        a->presents++;
        kprintf("amdgpu: buffer duplo ligado (%dx%d, VRAM 0x%llx)\n", ow, oh, b1);
        return true;
    }
    if (x1 <= x0 || y1 <= y0) return true;                       /* nada desta saída mudou */
    /* o buffer de trás só pode ser escrito depois que a troca anterior aconteceu */
    u32 n = 0;
    while (!amdgpu_flip_done(a) && n < 2000000) {
        n++;
#ifndef MOS_HOST_TEST
        __asm__ volatile("pause");
#endif
    }
    if (!amdgpu_flip_done(a)) { a->flip_timeouts++; scr_disable(a, "a troca não terminou"); return false; }
    int back = 1 - a->scr_front;
    /* o de trás está um quadro atrasado: recebe a área deste quadro e a do anterior */
    int ux0 = MIN(x0, a->prev_x), uy0 = MIN(y0, a->prev_y);
    int ux1 = MAX(x1, a->prev_x + a->prev_w), uy1 = MAX(y1, a->prev_y + a->prev_h);
    copy_rect(a, back, px, sw, ox, oy, ux0, uy0, ux1 - ux0, uy1 - uy0);
    a->prev_x = x0; a->prev_y = y0; a->prev_w = x1 - x0; a->prev_h = y1 - y0;
    if (amdgpu_flip(a, a->scr_phys[back], (u32)ow, (u32)oh, a->scr_stride, MOS_GPU_FMT_BGRA8, false)) { scr_disable(a, "troca recusada"); return false; }
    a->scr_front = back;
    a->presents++;
    return true;
}

/* ------------------------------------------------------------ VRAM */
static bool chunk_used(const amdgpu_t *a, u32 c) { return a->vram_used[c >> 3] >> (c & 7) & 1; }
static void chunk_set(amdgpu_t *a, u32 c, bool on) {
    if (on) a->vram_used[c >> 3] |= (u8)(1u << (c & 7));
    else a->vram_used[c >> 3] &= (u8)~(1u << (c & 7));
}

u64 amdgpu_vram_alloc(amdgpu_t *a, u64 size) {
    if (!a || !a->ready || !size) return 0;
    u32 n = (u32)((size + AMD_VRAM_CHUNK - 1) / AMD_VRAM_CHUNK);
    for (u32 c = a->vram_lo; c + n <= a->vram_hi;) {
        u32 k = 0;
        while (k < n && !chunk_used(a, c + k)) k++;
        if (k == n) {
            for (u32 j = 0; j < n; j++) chunk_set(a, c + j, true);
            return a->bar0 + (u64)c * AMD_VRAM_CHUNK;
        }
        c += k + 1;
    }
    return 0;
}

bool amdgpu_vram_owns(const amdgpu_t *a, u64 phys, u64 size) {
    if (!a || !a->ready || phys < a->bar0 || !size) return false;
    u64 off = phys - a->bar0;
    return off % AMD_VRAM_CHUNK == 0 && off / AMD_VRAM_CHUNK >= a->vram_lo &&
           (off + size + AMD_VRAM_CHUNK - 1) / AMD_VRAM_CHUNK <= a->vram_hi;
}

void amdgpu_vram_free(amdgpu_t *a, u64 phys, u64 size) {
    if (!amdgpu_vram_owns(a, phys, size)) return;
    u32 c = (u32)((phys - a->bar0) / AMD_VRAM_CHUNK), n = (u32)((size + AMD_VRAM_CHUNK - 1) / AMD_VRAM_CHUNK);
    for (u32 j = 0; j < n; j++) chunk_set(a, c + j, false);
}

u64 amdgpu_vram_free_bytes(const amdgpu_t *a) {
    u64 n = 0;
    for (u32 c = a->vram_lo; c < a->vram_hi; c++) if (!chunk_used(a, c)) n++;
    return n * AMD_VRAM_CHUNK;
}

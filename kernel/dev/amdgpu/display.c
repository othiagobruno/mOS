/* mOS — telas da GPU AMD no kernel: escolha do modo pelo EDID, inicialização do DCN (dcn10_init_hw), troca de
 * modo de um monitor mantendo os outros, e a volta do S3 (a saída que a UEFI deixou ou a última troca de modo).
 * Usa modeset.c (sequência), dcn.c (pipes) e edid.c (modos). */
#include "amdgpu.h"
#include "regs.h"
#include "dcn_regs.h"
#include "edid.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define PHYS_PTR(p) ((u8 *)amd_host_phys(p))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define PHYS_PTR(p) ((u8 *)(usize)(p))
#endif

#define FV_(s, m, v)     ((((u32)(v)) << (s)) & (m))
#define FV(f, v)         FV_(f, v)
#define FG_(x, s, m)     (((x) & (m)) >> (s))
#define FG(x, f)         FG_(x, f)
#define UPD_(a, r, s, m, v) upd(a, r, m, ((u32)(v) << (s)) & (m))
#define UPD(a, r, f, v)  UPD_(a, r, f, v)
static void upd(amdgpu_t *a, u32 r, u32 mask, u32 bits) { WR(a, r, (RR(a, r) & ~mask) | bits); }

/* ------------------------------------------------------------ escolha do modo (lógica pura) */
/* O modo w x h do EDID: com refresh_mhz = 0, o preferido se tiver esse tamanho, senão a maior taxa; com taxa, a mais
 * próxima. Fica de fora o que o sink ou o PHY não levam: entrelaçado, só 4:2:0, TMDS acima do limite (HDMI) e
 * relógio acima do teto do DCN (max_pclk_khz). -1 se nenhum serve. */
int amd_pick_mode(const edid_info_t *e, u32 w, u32 h, u32 refresh_mhz, u32 max_tmds_khz, u32 max_pclk_khz) {
    int best = -1;
    u32 best_d = 0xFFFFFFFFu, best_r = 0;
    for (int i = 0; e && i < e->nmodes; i++) {
        const edid_mode_t *m = &e->modes[i];
        if (m->hactive != w || m->vactive != h) continue;
        if (m->flags & (EDID_MODE_INTERLACE | EDID_MODE_Y420_ONLY)) continue;
        if (max_tmds_khz && m->pclk_khz > max_tmds_khz) continue;
        if (max_pclk_khz && m->pclk_khz > max_pclk_khz) continue;
        u32 r = edid_refresh_mhz(m);
        if (!refresh_mhz) {
            if (i == e->preferred) return i;
            if (best < 0 || r > best_r) { best = i; best_r = r; }
        } else {
            u32 d = r > refresh_mhz ? r - refresh_mhz : refresh_mhz - r;
            if (d < best_d) { best = i; best_d = d; }
        }
    }
    return best;
}

static void mode_to_timing(const edid_mode_t *m, amd_timing_t *t) {
    *t = (amd_timing_t){ .h_total = edid_htotal(m), .h_active = m->hactive, .h_front_porch = m->hfront, .h_sync_width = m->hsync,
        .v_total = edid_vtotal(m), .v_active = m->vactive, .v_front_porch = m->vfront, .v_sync_width = m->vsync,
        .hsync_pos = (m->flags & EDID_MODE_HPOS) != 0, .vsync_pos = (m->flags & EDID_MODE_VPOS) != 0, .pix_clk_khz = m->pclk_khz };
}

/* ------------------------------------------------------------ dcn10_init_hw */
static bool dig_enabled(amdgpu_t *a, u32 phy) {
    return FG(RR(a, D_DIG_DIG_BE_EN_CNTL(phy)), F_DIG_DIG_BE_EN_CNTL__DIG_ENABLE) != 0;
}

void amdgpu_dcn_init_hw(amdgpu_t *a) {
    /* dcn10_disable_vga */
    const u32 vga[4] = { D_D1VGA_CONTROL, D_D2VGA_CONTROL, D_D3VGA_CONTROL, D_D4VGA_CONTROL };
    bool any = false;
    for (int i = 0; i < 4; i++) any |= FG(RR(a, vga[i]), F_D1VGA_CONTROL__D1VGA_MODE_ENABLE) != 0;
    if (any) {
        for (int i = 0; i < 4; i++) WR(a, vga[i], 0);
        UPD(a, D_VGA_TEST_CONTROL, F_VGA_TEST_CONTROL__VGA_TEST_ENABLE, 1);
        UPD(a, D_VGA_TEST_CONTROL, F_VGA_TEST_CONTROL__VGA_TEST_RENDER_START, 1);
    }
    /* dcn10_bios_golden_init: EnableDispPowerGating v2.1, INIT no pipe 0 e DISABLE em todos */
    u32 npipes = a->bw_soc.num_pipes ? a->bw_soc.num_pipes : 4;
    if (a->vbios_ok) {
        u32 ps[8] = { 0 };
        ps[0] = 0 | 7u << 8;                                 /* ATOM_CRTC1, ATOM_INIT */
        amd_atom_execute(&a->atom, ATOM_CMD_ENABLE_DISP_PG, ps, 8);
        for (u32 i = 0; i < npipes; i++) { memset(ps, 0, sizeof ps); ps[0] = i; amd_atom_execute(&a->atom, ATOM_CMD_ENABLE_DISP_PG, ps, 8); }
        /* dcn10_link_encoder_hw_init: transmissor INIT em cada saída */
        for (u32 c = 0; c < a->vbios.nconn; c++) {
            const amd_conn_t *k = &a->vbios.conn[c];
            if (k->transmitter < 0) continue;
            memset(ps, 0, sizeof ps);
            /* phyid, ATOM_TRANSMITTER_ACTION_INIT, modo DVI (sinal indefinido no hw_init; eDP = DP), 4 vias */
            ps[0] = (u32)(k->transmitter * 2 + k->link) | 7u << 8 | (k->type == AMD_CONN_EDP ? 0u : 2u) << 16 | 4u << 24;
            ps[2] = (u32)(k->hpd >= 0 ? k->hpd + 1 : 0) | (u32)(k->object & 0xFF) << 16;
            amd_atom_execute(&a->atom, ATOM_CMD_DIG1_TRANSMITTER, ps, 8);
        }
    }
    /* dcn10_enable_power_gating_plane(true) */
    const u32 dom[8] = { D_DOMAIN0_PG_CONFIG, D_DOMAIN1_PG_CONFIG, D_DOMAIN2_PG_CONFIG, D_DOMAIN3_PG_CONFIG,
                         D_DOMAIN4_PG_CONFIG, D_DOMAIN5_PG_CONFIG, D_DOMAIN6_PG_CONFIG, D_DOMAIN7_PG_CONFIG };
    for (int d = 0; d < 8; d++) UPD(a, dom[d], F_DOMAIN0_PG_CONFIG__DOMAIN0_POWER_FORCEON, 0);
    /* dcn10_init_pipes: árvore MPC vazia (mpc1_mpc_init), pipes e OTGs desligados */
    for (u32 m = 0; m < AMD_DCN_PIPES; m++) {
        WR(a, D_MPCC_MPCC_TOP_SEL(m), 0xF);
        WR(a, D_MPCC_MPCC_BOT_SEL(m), 0xF);
        WR(a, D_MPCC_MPCC_OPP_ID(m), 0xF);
        WR(a, D_MPCC_MPCC_UPDATE_LOCK_SEL(m), 0xF);
    }
    for (u32 o = 0; o < AMD_DCN_OTGS; o++) UPD(a, D_MPC_OUT_MUX(o), F_MPC_OUT_MUX__MPC_OUT_MUX, 0xF);
    for (u32 p = 0; p < npipes; p++) amdgpu_dcn_disable_pipe(a, p);
    for (u32 o = 0; o < AMD_DCN_OTGS; o++)
        if (FG(RR(a, D_OTG_OTG_CONTROL(o)), F_OTG_OTG_CONTROL__OTG_MASTER_EN)) amdgpu_dcn_disable_otg(a, (int)o);
    /* stutter permitido e DCFCLK com clock gating */
    upd(a, D_DCHUBBUB_ARB_DRAM_STATE_CNTL, FV(F_DCHUBBUB_ARB_DRAM_STATE_CNTL__DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_ENABLE, ~0u), 0);
    UPD(a, D_DCFCLK_CNTL, F_DCFCLK_CNTL__DCFCLK_GATE_DIS, 0);
    amdgpu_audio_init(a);                                    /* dce_aud_hw_init */
    for (int o = 0; o < AMD_DCN_OTGS; o++) { a->disp[o].on = false; a->disp[o].audio_ep = -1; }
    a->dcn_inited = true;
}

/* ------------------------------------------------------------ pedido de modo para uma saída */
static int conn_of_ddc(const amdgpu_t *a, int ddc) {
    for (u32 c = 0; c < a->vbios.nconn; c++) if (a->vbios.conn[c].ddc == ddc) return (int)c;
    return -1;
}

static bool edid_of(amdgpu_t *a, u32 conn, edid_info_t *e) {
    int ddc = a->vbios.conn[conn].ddc;
    if (ddc < 0 || ddc >= AMD_NUM_DDC || !a->ddc_edid_len[ddc]) return false;
    return edid_parse(a->ddc_edid[ddc], a->ddc_edid_len[ddc], e);
}

/* política do amdgpu para a saída: 8 bpc, YCbCr 4:4:4 no HDMI quando o sink aceita, AVI com o VIC e o aspecto */
static void fill_req(amdgpu_t *a, u32 conn, const edid_info_t *e, const edid_mode_t *m, amd_mode_req_t *r) {
    memset(r, 0, sizeof *r);
    r->conn = conn;
    mode_to_timing(m, &r->t);
    r->bpc = 8;
    bool hdmi = a->vbios.conn[conn].type == AMD_CONN_HDMI;
    r->ycbcr444 = hdmi && e && e->hdmi && e->ycbcr444;
    r->vic = m->vic;
    r->aspect = edid_vic_aspect(m->vic);
    r->rgb_quant_selectable = e && e->rgb_quant_selectable;
    r->lte340_scramble = hdmi && e && e->lte_340_scramble;
    r->format = AMD_FMT_ARGB8888;
    r->sw_mode = 0;
}

/* a tela do compositor é a saída cuja superfície é o buffer 0 do amdgpu_present (uefi_mc): pipe, divisão e timing */
static void retarget_main(amdgpu_t *a) {
    for (int o = 0; o < AMD_DCN_OTGS; o++) {
        const amd_disp_t *d = &a->disp[o];
        if (!d->on || d->req.addr != a->uefi_mc) continue;
        const amd_timing_t *m = &d->req.t;
        a->otg[o] = (amd_otg_timing_t){ .on = true, .interlace = false, .htotal = (u16)m->h_total, .hactive = (u16)m->h_active,
            .hfront = (u16)m->h_front_porch, .hsync = (u16)m->h_sync_width,
            .hback = (u16)(m->h_total - m->h_active - m->h_front_porch - m->h_sync_width),
            .vtotal = (u16)m->v_total, .vactive = (u16)m->v_active, .vfront = (u16)m->v_front_porch, .vsync = (u16)m->v_sync_width,
            .vback = (u16)(m->v_total - m->v_active - m->v_front_porch - m->v_sync_width), .hpos = m->hsync_pos, .vpos = m->vsync_pos };
        a->pipe = o;
        a->split_mask = 0;
        for (u32 i = 0; i < d->dcn.npipes; i++) {
            u32 p = d->dcn.pipe[i];
            a->split_mask |= 1u << p;
            a->plane[p] = (amd_plane_t){ .on = true, .format = 8, .sw_mode = 0, .pitch_px = d->req.pitch_px,
                .vp_x = d->bw.viewport_x[i], .vp_y = 0, .vp_w = d->bw.viewport_width[i], .vp_h = m->v_active,
                .addr = a->uefi_mc, .rgba = d->req.format == AMD_FMT_ABGR8888 };
        }
        a->refresh_mhz = (u32)((u64)m->pix_clk_khz * 1000000ull / ((u64)m->h_total * m->v_total));
        return;
    }
}

/* a saída nova passa a ser a do compositor: o buffer 0 do amdgpu_present é a superfície dela */
static void adopt_main(amdgpu_t *a, u64 buf0_mc, u32 pitch_px) {
    if (a->scr_phys[1] && a->pipe >= 0) amdgpu_vram_free(a, a->scr_phys[1], (u64)a->scr_stride * a->otg[a->pipe].vactive);
    a->scr_phys[1] = 0;
    a->scr_on = false;
    a->scr_failed = false;
    a->scr_front = 0;
    a->uefi_mc = buf0_mc;
    a->uefi_pitch_px = pitch_px;
    a->uefi_rgba = false;
    a->uefi_match = true;
    retarget_main(a);
}

/* Muda uma saída para w x h (taxa em milésimos de Hz; 0 = a preferida) e mantém as outras telas. A superfície nova
 * fica preta até o próximo quadro. main: esta saída passa a ser a do compositor. */
int amdgpu_display_set(amdgpu_t *a, u32 conn, u32 w, u32 h, u32 refresh_mhz, bool main) {
    if (!a || !a->ready || !a->vbios_ok || conn >= a->vbios.nconn || !w || !h) return -MOS_EINVAL;
    static edid_info_t e;
    if (!edid_of(a, conn, &e)) return -MOS_ENOENT;
    bool hdmi = a->vbios.conn[conn].type == AMD_CONN_HDMI;
    u32 tmds = hdmi ? edid_sink_max_tmds_khz(&e) : 0;
    if (hdmi && (!tmds || tmds > 600000)) tmds = 600000;
    int mi = amd_pick_mode(&e, w, h, refresh_mhz, tmds, 0);
    if (mi < 0) return -MOS_ERANGE;
    amd_mode_req_t req[AMD_MAX_STREAMS];
    u32 n = 0;
    for (int o = 0; o < AMD_DCN_OTGS; o++)
        if (a->disp[o].on && a->disp[o].req.conn != conn) req[n++] = a->disp[o].req;
    if (n >= AMD_MAX_STREAMS) return -MOS_ERANGE;
    fill_req(a, conn, &e, &e.modes[mi], &req[n]);
    u32 pitch = ((w * 4 + 255) & ~255u) / 4;
    u64 bytes = (u64)pitch * 4 * h;
    u64 buf = amdgpu_vram_alloc(a, bytes);
    if (!buf) return -MOS_ENOMEM;
    memset(PHYS_PTR(a->fb_phys + (buf - a->bar0)), 0, (usize)bytes);
    req[n].addr = amdgpu_vram_to_mc(a, buf);
    req[n].pitch_px = pitch;
    n++;
    if (!a->dcn_inited) amdgpu_dcn_init_hw(a);
    int r = amdgpu_modeset(a, req, n);
    if (r) { amdgpu_vram_free(a, buf, bytes); return r; }
    /* os buffers das outras telas continuam; o desta saída troca */
    bool was_main = a->disp_buf[conn] && amdgpu_vram_to_mc(a, a->disp_buf[conn]) == a->uefi_mc;
    if (main || was_main) adopt_main(a, req[n - 1].addr, pitch);
    else retarget_main(a);
    if (a->disp_buf[conn]) amdgpu_vram_free(a, a->disp_buf[conn], a->disp_buf_bytes[conn]);
    a->disp_buf[conn] = buf;
    a->disp_buf_bytes[conn] = bytes;
    kprintf("amdgpu: %s %ux%u a %u.%03u Hz (%u kHz)%s\n", hdmi ? "HDMI" : "DP", w, h,
            edid_refresh_mhz(&e.modes[mi]) / 1000, edid_refresh_mhz(&e.modes[mi]) % 1000, e.modes[mi].pclk_khz,
            req[n - 1].ycbcr444 ? ", YCbCr 4:4:4" : "");
    return 0;
}

/* ------------------------------------------------------------ S3 */
/* Antes do S3: se o mOS ainda não trocou o modo, guarda a saída que a UEFI ligou (conector pelo DIG que lê o OTG
 * da tela, timing do OTG, relógio pelo EDID) para religá-la igual na volta. */
void amdgpu_display_save(amdgpu_t *a) {
    if (!a || !a->ready) return;
    a->boot_saved = false;
    for (int o = 0; o < AMD_DCN_OTGS; o++) if (a->disp[o].on) return;     /* o modeset já guarda o estado */
    if (a->pipe < 0 || !a->vbios_ok || !a->uefi_match) return;
    for (u32 c = 0; c < a->vbios.nconn; c++) {
        const amd_conn_t *k = &a->vbios.conn[c];
        if (k->transmitter < 0) continue;
        u32 phy = (u32)(k->transmitter * 2 + k->link);
        if (!dig_enabled(a, phy)) continue;
        if (FG(RR(a, D_DIG_DIG_FE_CNTL(phy)), F_DIG_DIG_FE_CNTL__DIG_SOURCE_SELECT) != (u32)a->pipe) continue;
        static edid_info_t e;
        bool have = edid_of(a, c, &e);
        edid_mode_t m;
        amd_otg_to_mode(&a->otg[a->pipe], &m);
        /* o relógio exato vem do modo igual do EDID; sem ele, da taxa medida no boot */
        m.pclk_khz = 0;
        for (int i = 0; have && i < e.nmodes; i++)
            if (e.modes[i].hactive == m.hactive && e.modes[i].vactive == m.vactive && edid_htotal(&e.modes[i]) == edid_htotal(&m) &&
                edid_vtotal(&e.modes[i]) == edid_vtotal(&m)) { m.pclk_khz = e.modes[i].pclk_khz; m.vic = e.modes[i].vic; break; }
        if (!m.pclk_khz && a->refresh_mhz)
            m.pclk_khz = (u32)((u64)edid_htotal(&m) * edid_vtotal(&m) * a->refresh_mhz / 1000000ull);
        if (!m.pclk_khz) return;
        fill_req(a, c, have ? &e : NULL, &m, &a->boot_req);
        a->boot_req.addr = a->uefi_mc;
        a->boot_req.pitch_px = a->uefi_pitch_px;
        a->boot_req.format = a->uefi_rgba ? AMD_FMT_ABGR8888 : AMD_FMT_ARGB8888;
        a->boot_saved = true;
        return;
    }
}

/* Na volta do S3, com SMU e GMC de pé: DCN do zero e as telas de antes */
void amdgpu_display_restore(amdgpu_t *a) {
    if (!a || !a->ready || !a->vbios_ok) return;          /* sem as tabelas ATOM não há como religar a saída */
    amd_mode_req_t req[AMD_MAX_STREAMS];
    u32 n = 0;
    for (int o = 0; o < AMD_DCN_OTGS; o++) if (a->disp[o].on) req[n++] = a->disp[o].req;
    if (!n && a->boot_saved) req[n++] = a->boot_req;
    amdgpu_dcn_init_hw(a);
    if (!n) return;
    int r = amdgpu_modeset(a, req, n);
    if (r) { kprintf("amdgpu: as telas não voltaram do S3 (%d)\n", r); return; }
    retarget_main(a);                                       /* a tela do compositor continua sendo a mesma saída */
    a->scr_on = false;                                      /* o buffer 1 do present é refeito no próximo quadro */
    kprintf("amdgpu: %u tela(s) de volta do S3\n", n);
}

int amdgpu_display_conn_of_ddc(amdgpu_t *a, int ddc) { return a ? conn_of_ddc(a, ddc) : -1; }

/* saída da tela do compositor: a da troca de modo ou, antes dela, a que a UEFI ligou (DIG que lê o OTG da tela) */
int amdgpu_display_main_conn(amdgpu_t *a) {
    if (!a || !a->ready || !a->vbios_ok || a->pipe < 0) return -1;
    if (a->disp[a->pipe].on) return (int)a->disp[a->pipe].req.conn;
    for (u32 c = 0; c < a->vbios.nconn; c++) {
        const amd_conn_t *k = &a->vbios.conn[c];
        if (k->transmitter < 0) continue;
        u32 phy = (u32)(k->transmitter * 2 + k->link);
        if (dig_enabled(a, phy) && FG(RR(a, D_DIG_DIG_FE_CNTL(phy)), F_DIG_DIG_FE_CNTL__DIG_SOURCE_SELECT) == (u32)a->pipe) return (int)c;
    }
    return -1;
}

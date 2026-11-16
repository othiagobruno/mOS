/* mOS — EDID, CTA-861 e DisplayID (kernel/dev/amdgpu/edid.h). Lógica pura, testada no host.
 * Referências públicas: VESA E-EDID 1.4, VESA DMT 1.13, VESA CVT 1.2, CTA-861-H, HDMI 1.4b/2.1,
 * VESA DisplayID 1.3 e 2.0. */
#include "edid.h"

/* ------------------------------------------------------------ tabelas */
typedef struct { u8 vic; u32 pclk; u16 ha, hf, hs, hb, va, vf, vs, vb; u8 fl; } vic_t;
#define P (EDID_MODE_HPOS | EDID_MODE_VPOS)
#define I EDID_MODE_INTERLACE
static const vic_t vic_tab[] = {
    {1,   25175,  640,  16,  96,  48,  480,  10, 2, 33, 0},
    {2,   27000,  720,  16,  62,  60,  480,   9, 6, 30, 0},
    {3,   27000,  720,  16,  62,  60,  480,   9, 6, 30, 0},
    {4,   74250, 1280, 110,  40, 220,  720,   5, 5, 20, P},
    {5,   74250, 1920,  88,  44, 148, 1080,   2, 5, 15, P | I},
    {6,   27000, 1440,  38, 124, 114,  480,   4, 3, 15, I},
    {7,   27000, 1440,  38, 124, 114,  480,   4, 3, 15, I},
    {16, 148500, 1920,  88,  44, 148, 1080,   4, 5, 36, P},
    {17,  27000,  720,  12,  64,  68,  576,   5, 5, 39, 0},
    {18,  27000,  720,  12,  64,  68,  576,   5, 5, 39, 0},
    {19,  74250, 1280, 440,  40, 220,  720,   5, 5, 20, P},
    {20,  74250, 1920, 528,  44, 148, 1080,   2, 5, 15, P | I},
    {31, 148500, 1920, 528,  44, 148, 1080,   4, 5, 36, P},
    {32,  74250, 1920, 638,  44, 148, 1080,   4, 5, 36, P},
    {33,  74250, 1920, 528,  44, 148, 1080,   4, 5, 36, P},
    {34,  74250, 1920,  88,  44, 148, 1080,   4, 5, 36, P},
    {63, 297000, 1920,  88,  44, 148, 1080,   4, 5, 36, P},
    {64, 297000, 1920, 528,  44, 148, 1080,   4, 5, 36, P},
    {93, 297000, 3840, 1276, 88, 296, 2160,   8, 10, 72, P},
    {94, 297000, 3840, 1056, 88, 296, 2160,   8, 10, 72, P},
    {95, 297000, 3840, 176,  88, 296, 2160,   8, 10, 72, P},
    {96, 594000, 3840, 1056, 88, 296, 2160,   8, 10, 72, P},
    {97, 594000, 3840, 176,  88, 296, 2160,   8, 10, 72, P},
    {98, 297000, 4096, 1020, 88, 296, 2160,   8, 10, 72, P},
    {99, 297000, 4096, 968,  88, 128, 2160,   8, 10, 72, P},
    {100, 297000, 4096, 88,  88, 128, 2160,   8, 10, 72, P},
    {101, 594000, 4096, 968, 88, 128, 2160,   8, 10, 72, P},
    {102, 594000, 4096, 88,  88, 128, 2160,   8, 10, 72, P},
};

typedef struct { u16 ha, va, hz; u32 pclk; u16 hf, hs, hb, vf, vs, vb; u8 fl; } dmt_t;
#define HP EDID_MODE_HPOS
#define VP EDID_MODE_VPOS
static const dmt_t dmt_tab[] = {
    {640,  480, 60,  25175, 16,  96,  48,  10, 2, 33, 0},
    {640,  480, 67,  30240, 64,  64,  96,   3, 3, 39, 0},
    {640,  480, 72,  31500, 24,  40, 128,   9, 3, 28, 0},
    {640,  480, 75,  31500, 16,  64, 120,   1, 3, 16, 0},
    {720,  400, 70,  28322, 18, 108,  54,  12, 2, 35, VP},
    {800,  600, 56,  36000, 24,  72, 128,   1, 2, 22, HP | VP},
    {800,  600, 60,  40000, 40, 128,  88,   1, 4, 23, HP | VP},
    {800,  600, 72,  50000, 56, 120,  64,  37, 6, 23, HP | VP},
    {800,  600, 75,  49500, 16,  80, 160,   1, 3, 21, HP | VP},
    {832,  624, 75,  57284, 32,  64, 224,   1, 3, 39, 0},
    {1024, 768, 60,  65000, 24, 136, 160,   3, 6, 29, 0},
    {1024, 768, 70,  75000, 24, 136, 144,   3, 6, 29, 0},
    {1024, 768, 75,  78750, 16,  96, 176,   1, 3, 28, HP | VP},
    {1152, 864, 75, 108000, 64, 128, 256,   1, 3, 32, HP | VP},
    {1280, 720, 60,  74250, 110, 40, 220,   5, 5, 20, HP | VP},
    {1280, 800, 60,  83500, 72, 128, 200,   3, 6, 22, VP},
    {1280, 960, 60, 108000, 96, 112, 312,   1, 3, 36, HP | VP},
    {1280, 1024, 60, 108000, 48, 112, 248,  1, 3, 38, HP | VP},
    {1280, 1024, 75, 135000, 16, 144, 248,  1, 3, 38, HP | VP},
    {1360, 768, 60,  85500, 64, 112, 256,   3, 6, 18, HP | VP},
    {1366, 768, 60,  85500, 70, 143, 213,   3, 3, 24, HP | VP},
    {1400, 1050, 60, 121750, 88, 144, 232,  3, 4, 32, VP},
    {1440, 900, 60, 106500, 80, 152, 232,   3, 6, 25, VP},
    {1600, 900, 60, 108000, 24,  80,  96,   1, 3, 96, HP | VP},
    {1600, 1200, 60, 162000, 64, 192, 304,  1, 3, 46, HP | VP},
    {1680, 1050, 60, 146250, 104, 176, 280, 3, 6, 30, VP},
    {1920, 1080, 60, 148500, 88,  44, 148,  4, 5, 36, HP | VP},
    {1920, 1200, 60, 154000, 48,  32,  80,  3, 6, 26, HP},
};
#undef P
#undef I
#undef HP
#undef VP

bool edid_vic_lookup(u8 vic, edid_mode_t *m) {
    for (usize i = 0; i < ARRAY_LEN(vic_tab); i++) {
        const vic_t *v = &vic_tab[i];
        if (v->vic != vic) continue;
        memset(m, 0, sizeof *m);
        m->pclk_khz = v->pclk;
        m->hactive = v->ha; m->hfront = v->hf; m->hsync = v->hs; m->hback = v->hb;
        m->vactive = v->va; m->vfront = v->vf; m->vsync = v->vs; m->vback = v->vb;
        m->flags = v->fl; m->source = EDID_SRC_VIC; m->vic = vic;
        return true;
    }
    return false;
}

bool edid_dmt_lookup(u32 w, u32 h, u32 hz, edid_mode_t *m) {
    for (usize i = 0; i < ARRAY_LEN(dmt_tab); i++) {
        const dmt_t *d = &dmt_tab[i];
        if (d->ha != w || d->va != h || d->hz != hz) continue;
        memset(m, 0, sizeof *m);
        m->pclk_khz = d->pclk;
        m->hactive = d->ha; m->hfront = d->hf; m->hsync = d->hs; m->hback = d->hb;
        m->vactive = d->va; m->vfront = d->vf; m->vsync = d->vs; m->vback = d->vb;
        m->flags = d->fl; m->source = EDID_SRC_EST;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ CVT 1.2 (aritmética inteira, picossegundos) */
static u32 cvt_vsync_for_aspect(u32 w, u32 h) {
    if (w * 3 == h * 4) return 4;
    if (w * 9 == h * 16) return 5;
    if (w * 10 == h * 16) return 6;
    if (w * 4 == h * 5 || w * 9 == h * 15) return 7;
    return 10;
}

bool edid_cvt(u32 w, u32 h, u32 hz, bool reduced, edid_mode_t *m) {
    if (w < 320 || h < 200 || w > 16384 || h > 8192 || hz < 24 || hz > 360) return false;
    w &= ~7u;
    u32 vsync = cvt_vsync_for_aspect(w, h);
    u64 frame_ps = 1000000000000ull / hz;
    memset(m, 0, sizeof *m);
    m->hactive = (u16)w; m->vactive = (u16)h; m->vsync = (u16)vsync;
    if (!reduced) {
        const u64 min_vsync_bp = 550000000ull;     /* 550 us */
        u64 hp = (frame_ps - min_vsync_bp) / (h + 3);
        u32 vsbp = (u32)(min_vsync_bp / hp) + 1;
        if (vsbp < vsync + 6) vsbp = vsync + 6;
        i64 duty = 30000 - (i64)(300 * hp / 1000000); /* milésimos de % */
        if (duty < 20000) duty = 20000;
        u32 hblank = (u32)(((u64)w * (u64)duty / (u64)(100000 - duty)) / 16) * 16;
        u32 htotal = w + hblank;
        u32 hsync = (htotal * 8 / 100) / 8 * 8;
        u64 pclk = (u64)htotal * 1000000000ull / hp;  /* kHz */
        m->pclk_khz = (u32)(pclk / 250 * 250);
        m->hback = (u16)(hblank / 2);
        m->hsync = (u16)hsync;
        m->hfront = (u16)(hblank - hblank / 2 - hsync);
        m->vfront = 3;
        m->vback = (u16)(vsbp - vsync);
        m->flags = EDID_MODE_VPOS;
    } else {
        const u64 min_vblank = 460000000ull;       /* 460 us */
        u64 hp = (frame_ps - min_vblank) / h;
        u32 vbi = (u32)(min_vblank / hp) + 1;
        if (vbi < 3 + vsync + 6) vbi = 3 + vsync + 6;
        u32 htotal = w + 160, vtotal = h + vbi;
        u64 pclk_hz = (u64)hz * vtotal * htotal;
        m->pclk_khz = (u32)(pclk_hz / 250000 * 250);
        m->hfront = 48; m->hsync = 32; m->hback = 80;
        m->vfront = 3; m->vback = (u16)(vbi - 3 - vsync);
        m->flags = EDID_MODE_HPOS;
    }
    m->source = EDID_SRC_STD;
    return true;
}

/* ------------------------------------------------------------ montagem da lista */
static bool same_timing(const edid_mode_t *a, const edid_mode_t *b) {
    return a->pclk_khz == b->pclk_khz && a->hactive == b->hactive && a->vactive == b->vactive &&
           a->hfront == b->hfront && a->hsync == b->hsync && a->hback == b->hback &&
           a->vfront == b->vfront && a->vsync == b->vsync && a->vback == b->vback &&
           (a->flags & EDID_MODE_INTERLACE) == (b->flags & EDID_MODE_INTERLACE);
}

static int add_mode(edid_info_t *e, const edid_mode_t *m) {
    if (!m->pclk_khz || !m->hactive || !m->vactive) return -1;
    for (int i = 0; i < e->nmodes; i++)
        if (same_timing(&e->modes[i], m)) {
            e->modes[i].flags |= m->flags & (EDID_MODE_PREFERRED | EDID_MODE_NATIVE | EDID_MODE_Y420_CAP);
            if (e->modes[i].flags & EDID_MODE_Y420_ONLY && !(m->flags & EDID_MODE_Y420_ONLY))
                e->modes[i].flags &= (u16)~EDID_MODE_Y420_ONLY;
            if (!e->modes[i].vic) e->modes[i].vic = m->vic;
            return i;
        }
    if (e->nmodes >= EDID_MAX_MODES) return -1;
    e->modes[e->nmodes] = *m;
    return e->nmodes++;
}

/* Descritor detalhado de 18 bytes. Devolve false se for descritor de exibição (pclk 0). */
static bool parse_dtd(const u8 *d, edid_mode_t *m) {
    u16 pclk = amd_le16(d);
    if (!pclk) return false;
    memset(m, 0, sizeof *m);
    m->pclk_khz = (u32)pclk * 10;
    u32 ha = d[2] | (d[4] & 0xF0) << 4, hbl = d[3] | (d[4] & 0x0F) << 8;
    u32 va = d[5] | (d[7] & 0xF0) << 4, vbl = d[6] | (d[7] & 0x0F) << 8;
    u32 hfp = d[8] | (d[11] & 0xC0) << 2, hsw = d[9] | (d[11] & 0x30) << 4;
    u32 vfp = (d[10] >> 4) | (d[11] & 0x0C) << 2, vsw = (d[10] & 0x0F) | (d[11] & 0x03) << 4;
    if (!ha || !va || hfp + hsw > hbl || vfp + vsw > vbl) return false;
    bool interlace = d[17] & 0x80;
    m->hactive = (u16)ha; m->hfront = (u16)hfp; m->hsync = (u16)hsw; m->hback = (u16)(hbl - hfp - hsw);
    if (interlace) va *= 2;                    /* o DTD entrelaçado descreve um campo */
    m->vactive = (u16)va; m->vfront = (u16)vfp; m->vsync = (u16)vsw; m->vback = (u16)(vbl - vfp - vsw);
    if (interlace) m->flags |= EDID_MODE_INTERLACE;
    if ((d[17] & 0x18) == 0x18) {              /* sincronismo digital separado */
        if (d[17] & 0x04) m->flags |= EDID_MODE_VPOS;
        if (d[17] & 0x02) m->flags |= EDID_MODE_HPOS;
    }
    m->source = EDID_SRC_DTD;
    return true;
}

static void copy_text(char *out, const u8 *s) {   /* 13 bytes terminados em 0x0A e preenchidos com espaços */
    int n = 0;
    for (; n < 13 && s[n] != 0x0A && s[n]; n++) out[n] = (char)(s[n] >= 0x20 && s[n] < 0x7F ? s[n] : '?');
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}

static bool checksum_ok(const u8 *b) {
    u8 s = 0;
    for (int i = 0; i < 128; i++) s = (u8)(s + b[i]);
    return s == 0;
}

/* ------------------------------------------------------------ bloco base */
static void parse_std_timing(edid_info_t *e, u8 b0, u8 b1) {
    if (b0 == 0x01 && b1 == 0x01) return;
    if (b0 == 0x00) return;
    u32 w = (b0 + 31u) * 8, h;
    switch (b1 >> 6) {
    case 0: h = (e->version == 1 && e->revision < 3) ? w : w * 10 / 16; break;
    case 1: h = w * 3 / 4; break;
    case 2: h = w * 4 / 5; break;
    default: h = w * 9 / 16; break;
    }
    u32 hz = (b1 & 0x3F) + 60u;
    edid_mode_t m;
    if (edid_dmt_lookup(w, h, hz, &m) || edid_cvt(w, h, hz, false, &m)) {
        m.source = EDID_SRC_STD;
        add_mode(e, &m);
    }
}

static void parse_est(edid_info_t *e, const u8 *b) {
    static const struct { u8 byte, bit; u16 w, h, hz; } est[] = {
        {0, 7, 720, 400, 70}, {0, 5, 640, 480, 60}, {0, 4, 640, 480, 67}, {0, 3, 640, 480, 72},
        {0, 2, 640, 480, 75}, {0, 1, 800, 600, 56}, {0, 0, 800, 600, 60}, {1, 7, 800, 600, 72},
        {1, 6, 800, 600, 75}, {1, 5, 832, 624, 75}, {1, 3, 1024, 768, 60}, {1, 2, 1024, 768, 70},
        {1, 1, 1024, 768, 75}, {1, 0, 1280, 1024, 75}, {2, 7, 1152, 864, 75},
    };
    for (usize i = 0; i < ARRAY_LEN(est); i++) {
        if (!(b[est[i].byte] >> est[i].bit & 1)) continue;
        edid_mode_t m;
        if (edid_dmt_lookup(est[i].w, est[i].h, est[i].hz, &m)) add_mode(e, &m);
    }
}

static void parse_base(edid_info_t *e, const u8 *b) {
    u16 v = (u16)(b[8] << 8 | b[9]);
    e->vendor[0] = (char)('A' - 1 + ((v >> 10) & 0x1F));
    e->vendor[1] = (char)('A' - 1 + ((v >> 5) & 0x1F));
    e->vendor[2] = (char)('A' - 1 + (v & 0x1F));
    e->vendor[3] = 0;
    e->product = amd_le16(b + 10);
    e->serial = amd_le32(b + 12);
    e->week = b[16]; e->year_off = b[17];
    e->version = b[18]; e->revision = b[19];
    e->digital = b[20] & 0x80;
    if (e->digital && e->version == 1 && e->revision >= 4) {
        static const u8 bpc[8] = {0, 6, 8, 10, 12, 14, 16, 0};
        e->bpc = bpc[(b[20] >> 4) & 7];
    }
    e->width_cm = b[21]; e->height_cm = b[22];
    e->ext_count = b[126];

    int first_dtd = -1;
    for (int i = 0; i < 4; i++) {
        const u8 *d = b + 54 + i * 18;
        edid_mode_t m;
        if (parse_dtd(d, &m)) {
            int idx = add_mode(e, &m);
            if (first_dtd < 0) first_dtd = idx;
            continue;
        }
        if (d[0] || d[1] || d[2]) continue;
        switch (d[3]) {
        case 0xFC: copy_text(e->name, d + 5); break;
        case 0xFF: copy_text(e->serial_str, d + 5); break;
        case 0xFD: {
            u8 off = d[4];
            e->vmin_hz = (u16)(d[5] + ((off & 0x01) ? 255 : 0));
            e->vmax_hz = (u16)(d[6] + ((off & 0x02) ? 255 : 0));
            e->hmin_khz = (u16)(d[7] + ((off & 0x04) ? 255 : 0));
            e->hmax_khz = (u16)(d[8] + ((off & 0x08) ? 255 : 0));
            e->max_pclk_khz = (u32)d[9] * 10000;
            break;
        }
        case 0xFA:                              /* mais timings padrão */
            for (int k = 0; k < 6; k++) parse_std_timing(e, d[5 + 2 * k], d[6 + 2 * k]);
            break;
        default: break;
        }
    }
    /* EDID 1.4: o primeiro DTD é sempre o preferido; 1.3: só com o bit 1 do byte 24 */
    if (first_dtd >= 0 && ((b[24] & 0x02) || (e->version == 1 && e->revision >= 4))) {
        e->modes[first_dtd].flags |= EDID_MODE_PREFERRED;
        e->preferred = first_dtd;
    }
    parse_est(e, b + 35);
    for (int i = 0; i < 8; i++) parse_std_timing(e, b[38 + 2 * i], b[39 + 2 * i]);
}

/* ------------------------------------------------------------ CTA-861 */
static void add_vic(edid_info_t *e, u8 vic, bool native, bool y420_only, bool y420_cap) {
    edid_mode_t m;
    if (!edid_vic_lookup(vic, &m)) return;
    if (native) m.flags |= EDID_MODE_NATIVE;
    if (y420_only) m.flags |= EDID_MODE_Y420_ONLY;
    if (y420_cap) m.flags |= EDID_MODE_Y420_CAP;
    add_mode(e, &m);
}

static void parse_hdmi_vsdb(edid_info_t *e, const u8 *p, u32 len) {
    e->hdmi = true;
    if (len >= 5) e->hdmi_phys_addr = (u16)(p[3] << 8 | p[4]);
    if (len >= 6) e->hdmi_dc = p[5] & 0x78;
    if (len >= 7 && p[6]) e->hdmi_max_tmds_khz = (u32)p[6] * 5000;
    if (len < 8) return;
    u8 f = p[7];
    u32 i = 8;
    if (f & 0x80) i += 2;                       /* latência de vídeo/áudio */
    if (f & 0x40) i += 2;                       /* latência entrelaçada */
    if (!(f & 0x20) || i + 1 >= len) return;    /* sem HDMI_Video_present */
    u32 vic_len = p[i + 1] >> 5;
    i += 2;
    static const u8 hdmi_vic[5] = {0, 95, 94, 93, 98};
    for (u32 k = 0; k < vic_len && i + k < len; k++) {
        u8 hv = p[i + k];
        if (hv >= 1 && hv <= 4) add_vic(e, hdmi_vic[hv], false, false, false);
    }
}

static void parse_hf_vsdb(edid_info_t *e, const u8 *p, u32 len) {
    e->hf = true;
    if (len < 6) return;
    if (p[4]) e->hf_max_tmds_khz = (u32)p[4] * 5000;
    e->scdc_present = p[5] & 0x80;
    e->scdc_rr = p[5] & 0x40;
    e->lte_340_scramble = p[5] & 0x08;
    if (len >= 7) e->max_frl_rate = p[6] >> 4;
}

static void parse_cta(edid_info_t *e, const u8 *b) {
    e->has_cta = true;
    u8 rev = b[1], dtd_off = b[2];
    if (rev >= 2) {
        e->underscan = b[3] & 0x80;
        e->basic_audio = b[3] & 0x40;
        e->ycbcr444 = b[3] & 0x20;
        e->ycbcr422 = b[3] & 0x10;
    }
    if (dtd_off > 127 || (dtd_off && dtd_off < 4)) return;
    /* primeira passada: SVDs (para o mapa de 4:2:0 que vem depois) */
    u8 svd[64]; u32 nsvd = 0;
    const u8 *y420_map = NULL; u32 y420_map_len = 0; bool y420_all = false;
    u32 end = dtd_off ? dtd_off : 4;
    for (u32 i = 4; i < end;) {
        u8 tag = b[i] >> 5, len = b[i] & 0x1F;
        const u8 *p = b + i + 1;
        if (i + 1 + len > end) break;
        if (tag == 2)
            for (u32 k = 0; k < len && nsvd < ARRAY_LEN(svd); k++) svd[nsvd++] = p[k];
        else if (tag == 3 && len >= 3) {
            u32 oui = amd_le24(p);
            if (oui == 0x000C03) parse_hdmi_vsdb(e, p, len);
            else if (oui == 0xC45DD8) parse_hf_vsdb(e, p, len);
            else if (oui == 0x00001A && len >= 7 && p[5] && p[6] > p[5]) { e->vrr_min_hz = p[5]; e->vrr_max_hz = p[6]; }
        } else if (tag == 7 && len >= 1) {
            if (p[0] == 0x0F) { y420_map = p + 1; y420_map_len = len - 1u; y420_all = len == 1; }
            else if (p[0] == 0x0E)              /* VDB só 4:2:0 */
                for (u32 k = 1; k < len; k++) add_vic(e, p[k], false, true, false);
            else if (p[0] == 0x79 && len >= 4 && amd_le24(p + 1) == 0xC45DD8)
                parse_hf_vsdb(e, p + 1, len - 1u);  /* HF-SCDB: mesmo formato a partir do OUI */
        }
        i += 1u + len;
    }
    for (u32 k = 0; k < nsvd; k++) {
        u8 v = svd[k];
        bool native = false;
        if (v >= 129 && v <= 192) { native = true; v &= 0x7F; }
        bool cap = y420_all || (y420_map && k / 8 < y420_map_len && (y420_map[k / 8] >> (k % 8) & 1));
        add_vic(e, v, native, false, cap);
    }
    if (!dtd_off) return;
    for (u32 o = dtd_off; o + 18 <= 127; o += 18) {
        edid_mode_t m;
        if (!parse_dtd(b + o, &m)) break;
        add_mode(e, &m);
    }
}

/* ------------------------------------------------------------ DisplayID */
static void parse_did_timing(edid_info_t *e, const u8 *t, u32 unit_khz, int *pref) {
    edid_mode_t m;
    memset(&m, 0, sizeof m);
    m.pclk_khz = (amd_le24(t) + 1) * unit_khz;
    u8 opt = t[3];
    u32 ha = amd_le16(t + 4) + 1u, hbl = amd_le16(t + 6) + 1u;
    u32 hfp = (amd_le16(t + 8) & 0x7FFF) + 1u, hsw = amd_le16(t + 10) + 1u;
    u32 va = amd_le16(t + 12) + 1u, vbl = amd_le16(t + 14) + 1u;
    u32 vfp = (amd_le16(t + 16) & 0x7FFF) + 1u, vsw = amd_le16(t + 18) + 1u;
    if (hfp + hsw > hbl || vfp + vsw > vbl) return;
    m.hactive = (u16)ha; m.hfront = (u16)hfp; m.hsync = (u16)hsw; m.hback = (u16)(hbl - hfp - hsw);
    m.vactive = (u16)va; m.vfront = (u16)vfp; m.vsync = (u16)vsw; m.vback = (u16)(vbl - vfp - vsw);
    if (t[9] & 0x80) m.flags |= EDID_MODE_HPOS;
    if (t[17] & 0x80) m.flags |= EDID_MODE_VPOS;
    if (opt & 0x10) m.flags |= EDID_MODE_INTERLACE;
    if (opt & 0x80) m.flags |= EDID_MODE_PREFERRED;
    m.source = EDID_SRC_DISPLAYID;
    int idx = add_mode(e, &m);
    if (idx >= 0 && (opt & 0x80) && *pref < 0) *pref = idx;
}

static void parse_displayid(edid_info_t *e, const u8 *b) {
    const u8 *s = b + 1;                        /* seção começa depois da tag 0x70 */
    u8 ver = s[0];
    u32 bytes = s[1];
    if (bytes > 121) return;                    /* 5 de cabeçalho + 1 de checksum dentro de 127 */
    u8 sum = 0;
    for (u32 i = 0; i < bytes + 5; i++) sum = (u8)(sum + s[i]);
    if (sum) return;
    int pref = -1;
    for (u32 i = 4; i + 3 <= 4 + bytes;) {
        u8 tag = s[i], len = s[i + 2];
        if (!tag && !len) break;
        if (i + 3 + len > 4 + bytes) break;
        const u8 *p = s + i + 3;
        bool v1 = ver < 0x20;
        if ((v1 && tag == 0x03) || (!v1 && tag == 0x22))
            for (u32 k = 0; k + 20 <= len; k += 20) parse_did_timing(e, p + k, v1 ? 10 : 1, &pref);
        i += 3u + len;
    }
    /* timing preferido do DisplayID descreve o painel nativo: vale mais que o primeiro DTD do bloco base */
    if (pref >= 0) {
        if (e->preferred >= 0 && e->preferred != pref) e->modes[e->preferred].flags &= (u16)~EDID_MODE_PREFERRED;
        e->preferred = pref;
    }
}

/* ------------------------------------------------------------ entrada */
bool edid_parse(const u8 *edid, usize len, edid_info_t *out) {
    static const u8 hdr[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    memset(out, 0, sizeof *out);
    out->preferred = -1;
    if (!edid || len < 128 || memcmp(edid, hdr, 8) || !checksum_ok(edid)) return false;
    out->valid = true;
    parse_base(out, edid);
    out->blocks_parsed = 1;
    usize nblk = len / 128;
    if (nblk > (usize)out->ext_count + 1) nblk = (usize)out->ext_count + 1;
    for (usize k = 1; k < nblk; k++) {
        const u8 *b = edid + k * 128;
        if (!checksum_ok(b)) continue;
        if (b[0] == 0x02) parse_cta(out, b);
        else if (b[0] == 0x70) parse_displayid(out, b);
        else continue;
        out->blocks_parsed++;
    }
    if (out->preferred < 0 && out->nmodes) out->preferred = 0;
    return true;
}

u32 edid_sink_max_tmds_khz(const edid_info_t *e) {
    if (e->hf) return e->hf_max_tmds_khz ? e->hf_max_tmds_khz : 340000;
    if (e->hdmi_max_tmds_khz) return e->hdmi_max_tmds_khz;
    if (e->max_pclk_khz) return e->max_pclk_khz;
    return 165000;
}

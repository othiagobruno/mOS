/* mOS — teste de host do driver AMD (kernel/dev/amdgpu): EDID/CTA/DisplayID com o EDID real do monitor
 * do Thiago (tools/testdata/amdgpu/edid-hdmi-thiago.bin) e os cabeçalhos dos blobs reais de firmware/amdgpu.
 * Rodar da raiz do repositório:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu tools/amdgpu_host.c \
 *       kernel/dev/amdgpu/edid.c kernel/dev/amdgpu/ucode.c -o /tmp/amdgpu_host && /tmp/amdgpu_host */
#include "edid.h"
#include "ucode.h"
#include <stdlib.h>

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static u8 *load(const char *path, usize *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("FALHOU: não abriu %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = aligned_alloc(16, ((usize)n + 15) & ~(usize)15);
    if (fread(b, 1, (usize)n, f) != (usize)n) { printf("FALHOU: leitura %s\n", path); exit(1); }
    fclose(f);
    *len = (usize)n;
    return b;
}

static int find(const edid_info_t *e, u32 w, u32 h, u32 hz) {
    for (int i = 0; i < e->nmodes; i++) {
        u32 r = edid_refresh_mhz(&e->modes[i]);
        if (e->modes[i].hactive == w && e->modes[i].vactive == h && r + 600 >= hz * 1000 && r <= hz * 1000 + 600) return i;
    }
    return -1;
}

static void test_edid_thiago(void) {
    usize n;
    u8 *b = load("tools/testdata/amdgpu/edid-hdmi-thiago.bin", &n);
    edid_info_t e;
    CHECK(edid_parse(b, n, &e), "EDID real não passou");
    CHECK(e.blocks_parsed == 3 && e.ext_count == 2, "blocos %u ext %u", e.blocks_parsed, e.ext_count);
    CHECK(!strcmp(e.name, "PG-CFCFS49U"), "nome '%s'", e.name);
    CHECK(!strcmp(e.serial_str, "123456789000"), "serial '%s'", e.serial_str);
    CHECK(!strcmp(e.vendor, "LVT") && e.product == 0x2022, "vendor %s produto %04x", e.vendor, e.product);
    CHECK(e.version == 1 && e.revision == 3 && e.digital, "versão %u.%u", e.version, e.revision);
    CHECK(e.width_cm == 120 && e.height_cm == 34, "tamanho %ux%u", e.width_cm, e.height_cm);
    CHECK(e.vmin_hz == 48 && e.vmax_hz == 144 && e.hmin_khz == 30 && e.hmax_khz == 160 && e.max_pclk_khz == 1200000,
          "faixa %u-%u Hz %u-%u kHz %u", e.vmin_hz, e.vmax_hz, e.hmin_khz, e.hmax_khz, e.max_pclk_khz);
    CHECK(e.has_cta && e.hdmi && e.basic_audio && e.ycbcr444 && e.ycbcr422 && e.underscan, "flags CTA");
    CHECK(e.hdmi_phys_addr == 0x1000, "endereço físico %04x", e.hdmi_phys_addr);
    CHECK(e.hdmi_dc == 0x38, "deep color %02x", e.hdmi_dc);
    CHECK(e.hf && e.scdc_present && e.scdc_rr && e.max_frl_rate == 3, "HF-VSDB scdc %d rr %d frl %u", e.scdc_present, e.scdc_rr, e.max_frl_rate);
    CHECK(e.hf_max_tmds_khz == 1275000 && edid_sink_max_tmds_khz(&e) == 1275000, "TMDS HF %u", e.hf_max_tmds_khz);
    CHECK(e.vrr_min_hz == 48 && e.vrr_max_hz == 144, "FreeSync %u-%u", e.vrr_min_hz, e.vrr_max_hz);

    /* preferido: o timing nativo do DisplayID, 5120x1440 a 60 Hz (o Linux também escolhe este) */
    CHECK(e.preferred >= 0, "sem preferido");
    const edid_mode_t *p = &e.modes[e.preferred];
    CHECK(p->hactive == 5120 && p->vactive == 1440 && p->pclk_khz == 481140 && p->source == EDID_SRC_DISPLAYID,
          "preferido %ux%u %u kHz src %u", p->hactive, p->vactive, p->pclk_khz, p->source);
    CHECK(edid_htotal(p) == 5400 && edid_vtotal(p) == 1485 && p->hfront == 88 && p->hsync == 44 && p->vfront == 3 && p->vsync == 5,
          "timing do preferido");
    CHECK(edid_refresh_mhz(p) == 60000, "taxa do preferido %u", edid_refresh_mhz(p));
    int nprefs = 0;
    for (int i = 0; i < e.nmodes; i++) if (e.modes[i].flags & EDID_MODE_PREFERRED) nprefs++;
    CHECK(nprefs == 1, "%d modos preferidos", nprefs);

    /* os modos que o Linux lista para este monitor */
    static const struct { u16 w, h, hz; } want[] = {
        {5120, 1440, 60}, {5120, 1440, 120}, {5120, 1440, 144}, {2560, 1440, 60}, {2560, 1440, 120},
        {3840, 2160, 30}, {3840, 2160, 25}, {1920, 1080, 60}, {1920, 1080, 50}, {1920, 1080, 120},
        {1280, 720, 60}, {1440, 900, 60}, {1600, 900, 60}, {1680, 1050, 60}, {1280, 1024, 60},
        {1280, 1024, 75}, {1024, 768, 60}, {1024, 768, 75}, {1152, 864, 75}, {832, 624, 75},
        {800, 600, 60}, {800, 600, 75}, {640, 480, 60}, {720, 400, 70}, {720, 480, 60}, {720, 576, 50},
    };
    for (usize i = 0; i < ARRAY_LEN(want); i++)
        CHECK(find(&e, want[i].w, want[i].h, want[i].hz) >= 0, "falta %ux%u@%u", want[i].w, want[i].h, want[i].hz);
    int m120 = find(&e, 5120, 1440, 120);
    CHECK(m120 >= 0 && e.modes[m120].pclk_khz == 962280 && e.modes[m120].hfront == 48, "5120x1440@120");
    int m2k = find(&e, 2560, 1440, 120);
    CHECK(m2k >= 0 && e.modes[m2k].pclk_khz == 497760 && e.modes[m2k].source == EDID_SRC_DTD, "2560x1440@120 do CTA");
    int i1080i = -1;
    for (int i = 0; i < e.nmodes; i++) if (e.modes[i].vic == 5) i1080i = i;
    CHECK(i1080i >= 0 && (e.modes[i1080i].flags & EDID_MODE_INTERLACE) && edid_refresh_mhz(&e.modes[i1080i]) == 60000, "1080i60");
    int v4 = -1;
    for (int i = 0; i < e.nmodes; i++) if (e.modes[i].vic == 4) v4 = i;
    CHECK(v4 >= 0 && (e.modes[v4].flags & EDID_MODE_Y420_CAP), "VIC 4 no mapa 4:2:0");
    int v95 = -1;
    for (int i = 0; i < e.nmodes; i++) if (e.modes[i].vic == 95) v95 = i;
    CHECK(v95 >= 0 && e.modes[v95].pclk_khz == 297000, "HDMI VIC 1 -> VIC 95");
    for (int i = 0; i < e.nmodes; i++)
        for (int j = i + 1; j < e.nmodes; j++)
            CHECK(!(e.modes[i].pclk_khz == e.modes[j].pclk_khz && e.modes[i].hactive == e.modes[j].hactive &&
                    e.modes[i].vactive == e.modes[j].vactive && edid_htotal(&e.modes[i]) == edid_htotal(&e.modes[j]) &&
                    edid_vtotal(&e.modes[i]) == edid_vtotal(&e.modes[j]) && e.modes[i].hsync == e.modes[j].hsync &&
                    e.modes[i].hfront == e.modes[j].hfront && e.modes[i].vfront == e.modes[j].vfront &&
                    (e.modes[i].flags & EDID_MODE_INTERLACE) == (e.modes[j].flags & EDID_MODE_INTERLACE)),
                  "modo duplicado %d/%d", i, j);
    printf("edid: %d modos; preferido %ux%u@%u.%03u\n", e.nmodes, p->hactive, p->vactive,
           edid_refresh_mhz(p) / 1000, edid_refresh_mhz(p) % 1000);

    /* bloco corrompido: o base ruim recusa tudo; extensão ruim é ignorada */
    u8 bad[384];
    memcpy(bad, b, 384);
    bad[0x90] ^= 1;
    edid_info_t e2;
    CHECK(edid_parse(bad, 384, &e2) && !e2.has_cta && e2.blocks_parsed == 2, "CTA com checksum errado devia ser ignorado");
    bad[0x20] ^= 1;
    CHECK(!edid_parse(bad, 384, &e2) && !e2.valid, "base com checksum errado devia falhar");
    CHECK(!edid_parse(b, 100, &e2), "EDID curto");
    /* só o bloco base: preferido vira o primeiro DTD 2560x1440@60 (bit 1 do byte 24) */
    CHECK(edid_parse(b, 128, &e2) && e2.preferred >= 0 && e2.modes[e2.preferred].hactive == 2560 &&
          e2.modes[e2.preferred].pclk_khz == 242350 && !e2.has_cta, "só base");
    CHECK(edid_sink_max_tmds_khz(&e2) == 1200000, "TMDS pela faixa %u", edid_sink_max_tmds_khz(&e2));
    free(b);
}

static void test_edid_synth(void) {
    /* EDID 1.4 sintético: DTD 1920x1200 RB, DisplayID 2.0 com timing tipo VII 3840x2160@60 (kHz) */
    u8 b[256];
    memset(b, 0, sizeof b);
    static const u8 hdr[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    memcpy(b, hdr, 8);
    b[8] = 0x4C; b[9] = 0x2D;               /* "SAM" */
    b[18] = 1; b[19] = 4; b[20] = 0xB5;     /* digital, 10 bpc, DP */
    b[24] = 0x00;                           /* 1.4: primeiro DTD é preferido mesmo sem o bit */
    b[126] = 1;
    u8 *d = b + 54;                         /* 1920x1200@60 RB: 154000 kHz, hbl 160, vbl 35 */
    d[0] = 0x28; d[1] = 0x3C; d[2] = 0x80; d[3] = 0xA0; d[4] = 0x70; d[5] = 0xB0; d[6] = 0x23; d[7] = 0x40;
    d[8] = 0x30; d[9] = 0x20; d[10] = 0x36; d[11] = 0x00; d[17] = 0x1A;
    for (int i = 0; i < 8; i++) { b[38 + 2 * i] = 1; b[39 + 2 * i] = 1; }
    for (int k = 1; k < 4; k++) { u8 *x = b + 54 + 18 * k; x[3] = 0x10; }
    u8 s = 0; for (int i = 0; i < 127; i++) s = (u8)(s + b[i]); b[127] = (u8)(0x100 - s);

    u8 *x = b + 128;
    x[0] = 0x70; x[1] = 0x20; x[2] = 23; x[3] = 0; x[4] = 0;
    u8 *blk = x + 5;
    blk[0] = 0x22; blk[1] = 0; blk[2] = 20;
    u8 *t = blk + 3;                        /* 3840x2160@60: 522614 kHz, ht 3920, vt 2222 */
    u32 pk = 522614 - 1; t[0] = pk & 0xFF; t[1] = pk >> 8 & 0xFF; t[2] = pk >> 16;
    t[3] = 0x80;
    u16 v;
    v = 3840 - 1; t[4] = v & 0xFF; t[5] = v >> 8;
    v = 80 - 1;   t[6] = v & 0xFF; t[7] = v >> 8;
    v = 8 - 1;    t[8] = v & 0xFF; t[9] = (v >> 8) | 0x80;
    v = 32 - 1;   t[10] = v & 0xFF; t[11] = v >> 8;
    v = 2160 - 1; t[12] = v & 0xFF; t[13] = v >> 8;
    v = 62 - 1;   t[14] = v & 0xFF; t[15] = v >> 8;
    v = 48 - 1;   t[16] = v & 0xFF; t[17] = v >> 8;
    v = 8 - 1;    t[18] = v & 0xFF; t[19] = v >> 8;
    s = 0; for (int i = 1; i < 1 + 5 + 23; i++) s = (u8)(s + x[i]); x[1 + 4 + 23] = (u8)(0x100 - s);
    s = 0; for (int i = 0; i < 127; i++) s = (u8)(s + x[i]); x[127] = (u8)(0x100 - s);

    edid_info_t e;
    CHECK(edid_parse(b, 256, &e), "sintético");
    CHECK(!strcmp(e.vendor, "SAM") && e.bpc == 10, "vendor %s bpc %u", e.vendor, e.bpc);
    int a = find(&e, 1920, 1200, 60), k = find(&e, 3840, 2160, 60);
    CHECK(a >= 0 && e.modes[a].pclk_khz == 154000 && e.modes[a].hback == 80 && (e.modes[a].flags & EDID_MODE_HPOS) &&
          !(e.modes[a].flags & EDID_MODE_VPOS), "DTD 1920x1200 RB");
    CHECK(k >= 0 && e.modes[k].pclk_khz == 522614 && e.modes[k].hback == 40 && e.modes[k].vback == 6 &&
          (e.modes[k].flags & EDID_MODE_HPOS), "DisplayID 2.0 tipo VII");
    CHECK(e.preferred == k, "preferido do DisplayID");
    CHECK(edid_sink_max_tmds_khz(&e) == 165000, "sem limite declarado: DVI single-link");
}

static void test_cvt(void) {
    edid_mode_t m;
    CHECK(edid_cvt(1920, 1080, 60, false, &m) && m.pclk_khz == 173000 && edid_htotal(&m) == 2576 && edid_vtotal(&m) == 1120 &&
          m.hsync == 200 && m.hfront == 128 && m.vsync == 5 && m.vfront == 3, "CVT 1920x1080@60 %u %u %u hs %u hf %u",
          m.pclk_khz, edid_htotal(&m), edid_vtotal(&m), m.hsync, m.hfront);
    CHECK(edid_cvt(1920, 1080, 60, true, &m) && m.pclk_khz == 138500 && edid_htotal(&m) == 2080 && edid_vtotal(&m) == 1111,
          "CVT-RB 1920x1080@60 %u %u %u", m.pclk_khz, edid_htotal(&m), edid_vtotal(&m));
    CHECK(edid_cvt(2560, 1440, 60, true, &m) && m.pclk_khz == 241500 && edid_vtotal(&m) == 1481,
          "CVT-RB 2560x1440@60 %u %u", m.pclk_khz, edid_vtotal(&m));
    CHECK(edid_cvt(1024, 768, 60, false, &m) && m.pclk_khz == 63500 && edid_htotal(&m) == 1328 && m.hsync == 104 && m.hfront == 48 && edid_vtotal(&m) == 798 && m.vsync == 4,
          "CVT 1024x768@60 %u %u %u", m.pclk_khz, edid_htotal(&m), edid_vtotal(&m));
    CHECK(!edid_cvt(100, 100, 60, false, &m), "CVT fora da norma");
    CHECK(edid_dmt_lookup(1680, 1050, 60, &m) && m.pclk_khz == 146250, "DMT 1680x1050");
    CHECK(edid_vic_lookup(97, &m) && m.pclk_khz == 594000 && edid_refresh_mhz(&m) == 60000, "VIC 97");
    CHECK(!edid_vic_lookup(200, &m), "VIC inexistente");
}

static void test_fw(const char *prefix, u32 rlc_ver, u32 rlc_minor, u32 num_cu) {
    char path[128];
    static const struct { const char *n; int kind; } fw[] = {
        {"pfp", UC_KIND_GFX}, {"me", UC_KIND_GFX}, {"ce", UC_KIND_GFX}, {"mec", UC_KIND_GFX},
        {"rlc", UC_KIND_RLC}, {"sdma", UC_KIND_SDMA}, {"gpu_info", UC_KIND_GPU_INFO},
    };
    for (usize i = 0; i < ARRAY_LEN(fw); i++) {
        snprintf(path, sizeof path, "firmware/amdgpu/%s_%s.bin", prefix, fw[i].n);
        usize n;
        u8 *b = load(path, &n);
        amd_ucode_t u;
        int r = amd_ucode_parse(b, n, fw[i].kind, &u);
        CHECK(r == 0, "%s: %s", path, amd_ucode_error(r));
        if (r) { free(b); continue; }
        if (fw[i].kind == UC_KIND_GFX) {
            CHECK(u.ip_major == 9 && u.feature_version == 54 && u.jt_size == (fw[i].n[1] == 'e' && fw[i].n[0] == 'm' && fw[i].n[2] == 'c' ? 224u : 96u) && u.jt_offset + u.jt_size <= u.ucode_dwords,
                  "%s: ip %u.%u feature %u jt %u+%u de %u", path, u.ip_major, u.ip_minor, u.feature_version, u.jt_offset, u.jt_size, u.ucode_dwords);
        } else if (fw[i].kind == UC_KIND_SDMA) {
            CHECK(u.ip_major == 4 && u.ip_minor == 1 && u.ucode_version == 0xA9 && u.feature_version == 41 && u.jt_offset == 4096 && u.jt_size == 64,
                  "%s: sdma", path);
        } else if (fw[i].kind == UC_KIND_RLC) {
            CHECK(u.ucode_version == rlc_ver && u.hdr_minor == rlc_minor, "%s: rlc 0x%x v2.%u", path, u.ucode_version, u.hdr_minor);
            CHECK(u.rlc_reg_list_format_dwords && u.rlc_reg_list_dwords && u.rlc_srl_cntl_bytes && u.rlc_srl_gpm_bytes && u.rlc_srl_srm_bytes,
                  "%s: listas do rlc", path);
            CHECK((const u8 *)u.rlc_srl_srm + u.rlc_srl_srm_bytes <= b + n, "%s: srm fora", path);
        } else {
            CHECK(u.gi_num_se == 1 && u.gi_num_cu_per_sh == num_cu && u.gi_wave_size == 64 && u.gi_lds_size == 64,
                  "%s: gpu_info se %u cu %u", path, u.gi_num_se, u.gi_num_cu_per_sh);
        }
        /* cabeçalho truncado e tamanho mentiroso */
        CHECK(amd_ucode_parse(b, 20, fw[i].kind, &u) != 0, "%s: curto", path);
        CHECK(amd_ucode_parse(b, n - 4, fw[i].kind, &u) != 0, "%s: tamanho declarado > arquivo", path);
        CHECK(amd_ucode_parse(b, n, 99, &u) != 0, "tipo inválido");
        free(b);
    }
}

static void test_picasso_rlc_fields(void) {
    usize n;
    u8 *b = load("firmware/amdgpu/picasso_rlc.bin", &n);
    amd_ucode_t u;
    CHECK(amd_ucode_parse(b, n, UC_KIND_RLC, &u) == 0, "picasso rlc");
    CHECK(u.rlc_reg_restore_list_size == 95 && u.rlc_reg_list_format_start == 96 && u.rlc_reg_list_format_separate_start == 166 &&
          u.rlc_starting_offsets_start == 186 && u.rlc_reg_list_format_dwords == 65 && u.rlc_reg_list_dwords == 2416 &&
          u.rlc_direct_reg_list_length == 34 && u.rlc_srl_cntl_bytes == 608 && u.rlc_srl_gpm_bytes == 1312 && u.rlc_srl_srm_bytes == 10144,
          "campos do RLC Picasso");
    CHECK(u.rlc_srl_srm == b + 28996, "offset da lista srm");
    free(b);
}

static void test_variant(void) {
    char nm[32];
    CHECK(amd_raven_variant(0x15D8, 1) == AMD_PICASSO, "15d8 rev 1");
    CHECK(amd_raven_variant(0x15D8, 8) == AMD_RAVEN2, "15d8 rev 8");
    CHECK(amd_raven_variant(0x15DD, 0) == AMD_RAVEN, "15dd");
    CHECK(amd_raven_variant(0x15DD, 9) == AMD_RAVEN2, "15dd rev 9");
    amd_raven_rlc_name(AMD_PICASSO, 0x75, 0x41e4d, nm, sizeof nm);
    CHECK(!strcmp(nm, "picasso_rlc.bin"), "PC do Thiago (rev 0x75, FP5): %s", nm);
    amd_raven_rlc_name(AMD_PICASSO, 0xC9, 0, nm, sizeof nm);
    CHECK(!strcmp(nm, "picasso_rlc_am4.bin"), "AM4: %s", nm);
    amd_raven_rlc_name(AMD_PICASSO, 0xE0, 0, nm, sizeof nm);
    CHECK(!strcmp(nm, "picasso_rlc.bin"), "rev 0xE0: %s", nm);
    amd_raven_rlc_name(AMD_RAVEN, 0xC4, 0x41e2b, nm, sizeof nm);
    CHECK(!strcmp(nm, "raven_kicker_rlc.bin"), "kicker: %s", nm);
    amd_raven_rlc_name(AMD_RAVEN2, 0xC3, 0x41e2b, nm, sizeof nm);
    CHECK(!strcmp(nm, "raven2_rlc.bin"), "raven2: %s", nm);
}

int main(void) {
    test_edid_thiago();
    test_edid_synth();
    test_cvt();
    test_fw("picasso", 0x6F, 1, 11);
    test_fw("raven2", 0x49, 1, 3);
    test_fw("raven", 0x213, 1, 11);
    test_picasso_rlc_fields();
    test_variant();
    printf("amdgpu-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}

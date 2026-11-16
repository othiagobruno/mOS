/* mOS — EDID 1.3/1.4, extensões CTA-861 e DisplayID 1.x/2.x: lista de modos de um monitor.
 *
 * Entrada: os blocos de 128 bytes lidos pelo DDC (HDMI/DVI) ou pelo AUX (DP). Saída: identificação,
 * capacidades HDMI (TMDS máximo, SCDC, scrambling, cor profunda) e os modos com o timing completo.
 * Lógica pura: sem hardware, testada no host com o EDID real do monitor do Thiago. */
#pragma once
#include "amdgpu_base.h"

#define EDID_MAX_MODES 64

enum {
    EDID_SRC_DTD = 1,       /* descritor detalhado do bloco base ou do CTA */
    EDID_SRC_DISPLAYID,     /* DisplayID tipo I (1.x) ou tipo VII (2.x) */
    EDID_SRC_VIC,           /* código de vídeo CTA-861 (SVD) */
    EDID_SRC_EST,           /* timings estabelecidos (DMT) */
    EDID_SRC_STD,           /* timings padrão (DMT ou CVT) */
};

#define EDID_MODE_HPOS      (1u << 0)   /* hsync positivo */
#define EDID_MODE_VPOS      (1u << 1)
#define EDID_MODE_INTERLACE (1u << 2)
#define EDID_MODE_PREFERRED (1u << 3)
#define EDID_MODE_NATIVE    (1u << 4)   /* VIC marcado como nativo no SVD */
#define EDID_MODE_Y420_ONLY (1u << 5)   /* só em YCbCr 4:2:0 (não serve para RGB) */
#define EDID_MODE_Y420_CAP  (1u << 6)   /* também aceita YCbCr 4:2:0 (mapa Y420CMDB) */

typedef struct {
    u32 pclk_khz;
    u16 hactive, hfront, hsync, hback;
    u16 vactive, vfront, vsync, vback;
    u16 flags;              /* EDID_MODE_* */
    u8  source;             /* EDID_SRC_* */
    u8  vic;                /* 0 se não veio de um VIC */
} edid_mode_t;

static inline u32 edid_htotal(const edid_mode_t *m) { return (u32)m->hactive + m->hfront + m->hsync + m->hback; }
static inline u32 edid_vtotal(const edid_mode_t *m) { return (u32)m->vactive + m->vfront + m->vsync + m->vback; }
/* taxa em milésimos de Hz (60000 = 60 Hz). Entrelaçado: vactive é o quadro e os blanks são por campo;
 * o quadro tem 2 campos e meia linha a mais, e a taxa é a de campos. */
static inline u32 edid_refresh_mhz(const edid_mode_t *m) {
    u64 vt = edid_vtotal(m), num = (u64)m->pclk_khz * 1000000ull;
    if (m->flags & EDID_MODE_INTERLACE) { vt = (u64)m->vactive + 2ull * (m->vfront + m->vsync + m->vback) + 1; num *= 2; }
    u64 t = (u64)edid_htotal(m) * vt;
    return t ? (u32)((num + t / 2) / t) : 0;
}

typedef struct {
    bool  valid;
    u8    version, revision;
    char  vendor[4];        /* PNP de 3 letras */
    u16   product;
    u32   serial;
    u8    week, year_off;   /* ano = 1990 + year_off */
    char  name[14];
    char  serial_str[14];
    bool  digital;
    u8    bpc;              /* bits por cor do bloco base (0 = indefinido) */
    u16   width_cm, height_cm;
    /* faixa (descritor 0xFD) */
    u16   vmin_hz, vmax_hz;
    u16   hmin_khz, hmax_khz;
    u32   max_pclk_khz;     /* 0 = sem limite declarado */
    /* CTA-861 */
    bool  has_cta;
    bool  hdmi;             /* VSDB HDMI 1.x (OUI 00-0C-03) presente: o sink aceita HDMI, não só DVI */
    bool  basic_audio, ycbcr444, ycbcr422, underscan;
    u16   hdmi_phys_addr;
    u32   hdmi_max_tmds_khz; /* do VSDB 1.x; 0 = não declarado */
    u8    hdmi_dc;          /* bits DC_30/36/48 do VSDB */
    /* HDMI Forum (HF-VSDB / HF-SCDB, OUI C4-5D-D8) */
    bool  hf;
    u32   hf_max_tmds_khz;  /* 0 = não declarado (vale 340 MHz) */
    bool  scdc_present, scdc_rr, lte_340_scramble;
    u8    max_frl_rate;
    /* FreeSync (VSDB da AMD, OUI 00-00-1A): faixa de taxa variável; 0 = sem VRR */
    u8    vrr_min_hz, vrr_max_hz;
    /* modos */
    int         nmodes;
    edid_mode_t modes[EDID_MAX_MODES];
    int         preferred;  /* índice em modes[], -1 se nenhum */
    u8          ext_count;
    u8          blocks_parsed;
} edid_info_t;

/* len = bytes disponíveis (128 * blocos). Blocos com checksum errado são ignorados (o base inválido
 * zera tudo e devolve false). */
bool edid_parse(const u8 *edid, usize len, edid_info_t *out);

/* TMDS máximo que o sink aceita em kHz: HF-VSDB > VSDB 1.x > faixa > 165 MHz (DVI single-link). */
u32  edid_sink_max_tmds_khz(const edid_info_t *e);

/* Timings CVT 1.2 (normal ou blanking reduzido v1). Devolve false para parâmetros fora da norma. */
bool edid_cvt(u32 w, u32 h, u32 hz, bool reduced, edid_mode_t *m);
/* Tabela DMT (VESA) e VIC (CTA-861). */
bool edid_dmt_lookup(u32 w, u32 h, u32 hz, edid_mode_t *m);
bool edid_vic_lookup(u8 vic, edid_mode_t *m);

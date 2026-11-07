/*
 * mOS libSystem — execução dos comandos do Metal por software: formatos de pixel (leitura/escrita de
 * texels, sRGB, half, 10:10:10:2, 11:11:10), amostragem (nearest/linear, mipmaps, endereçamento,
 * cubemaps), rasterizador de triângulos/linhas/pontos (recorte no espaço de recorte, viewport,
 * scissor, culling, regra top-left, interpolação com correção de perspectiva, flat e
 * no_perspective, teste de profundidade e stencil, blending com todos os fatores e operações,
 * máscara de escrita), compute (threadgroups, threadgroup memory) e blit.
 */
#include "metalint.h"

double pow(double, double), floor(double), sqrt(double), round(double), fabs(double), ldexp(double, int), ceil(double);
u16 msl_f2h(float f);
float msl_h2f(u16 h);

/* ================================================================ formatos */
enum { PK_NONE = 0, PK_UN8, PK_SN8, PK_U8, PK_S8, PK_UN16, PK_SN16, PK_U16, PK_S16, PK_HALF, PK_U32, PK_S32, PK_F32,
       PK_RGB10A2, PK_RGB10A2U, PK_RG11B10F, PK_RGB9E5, PK_B5G6R5, PK_A1BGR5, PK_ABGR4, PK_BGR5A1,
       PK_D16, PK_D32F, PK_ST8, PK_D32FS8, PK_D24S8, PK_X32S8, PK_X24S8, PK_BGRA10XR, PK_BGR10XR };
enum { PF_BGRA = 1, PF_SRGB = 2, PF_ALPHA = 4 };
typedef struct { u16 fmt; u8 bpp, nc, kind, fl; } pf_t;
static const pf_t pfs[] = {
    {1, 1, 1, PK_UN8, PF_ALPHA},
    {10, 1, 1, PK_UN8, 0}, {11, 1, 1, PK_UN8, PF_SRGB}, {12, 1, 1, PK_SN8, 0}, {13, 1, 1, PK_U8, 0}, {14, 1, 1, PK_S8, 0},
    {20, 2, 1, PK_UN16, 0}, {22, 2, 1, PK_SN16, 0}, {23, 2, 1, PK_U16, 0}, {24, 2, 1, PK_S16, 0}, {25, 2, 1, PK_HALF, 0},
    {30, 2, 2, PK_UN8, 0}, {31, 2, 2, PK_UN8, PF_SRGB}, {32, 2, 2, PK_SN8, 0}, {33, 2, 2, PK_U8, 0}, {34, 2, 2, PK_S8, 0},
    {40, 2, 3, PK_B5G6R5, 0}, {41, 2, 4, PK_A1BGR5, 0}, {42, 2, 4, PK_ABGR4, 0}, {43, 2, 4, PK_BGR5A1, 0},
    {53, 4, 1, PK_U32, 0}, {54, 4, 1, PK_S32, 0}, {55, 4, 1, PK_F32, 0},
    {60, 4, 2, PK_UN16, 0}, {62, 4, 2, PK_SN16, 0}, {63, 4, 2, PK_U16, 0}, {64, 4, 2, PK_S16, 0}, {65, 4, 2, PK_HALF, 0},
    {70, 4, 4, PK_UN8, 0}, {71, 4, 4, PK_UN8, PF_SRGB}, {72, 4, 4, PK_SN8, 0}, {73, 4, 4, PK_U8, 0}, {74, 4, 4, PK_S8, 0},
    {80, 4, 4, PK_UN8, PF_BGRA}, {81, 4, 4, PK_UN8, PF_BGRA | PF_SRGB},
    {90, 4, 4, PK_RGB10A2, 0}, {91, 4, 4, PK_RGB10A2U, 0}, {92, 4, 3, PK_RG11B10F, 0}, {93, 4, 3, PK_RGB9E5, 0},
    {94, 4, 4, PK_RGB10A2, PF_BGRA},
    {103, 8, 2, PK_U32, 0}, {104, 8, 2, PK_S32, 0}, {105, 8, 2, PK_F32, 0},
    {110, 8, 4, PK_UN16, 0}, {112, 8, 4, PK_SN16, 0}, {113, 8, 4, PK_U16, 0}, {114, 8, 4, PK_S16, 0}, {115, 8, 4, PK_HALF, 0},
    {123, 16, 4, PK_U32, 0}, {124, 16, 4, PK_S32, 0}, {125, 16, 4, PK_F32, 0},
    {250, 2, 1, PK_D16, 0}, {252, 4, 1, PK_D32F, 0}, {253, 1, 1, PK_ST8, 0}, {255, 4, 1, PK_D24S8, 0},
    {260, 8, 1, PK_D32FS8, 0}, {261, 8, 1, PK_X32S8, 0}, {262, 4, 1, PK_X24S8, 0},
    {552, 8, 4, PK_BGRA10XR, PF_BGRA}, {553, 8, 4, PK_BGRA10XR, PF_BGRA | PF_SRGB}, {554, 4, 3, PK_BGR10XR, PF_BGRA},
    {555, 4, 3, PK_BGR10XR, PF_BGRA | PF_SRGB},
    {0, 0, 0, 0, 0}};
static const pf_t *pf(int fmt) {
    static const pf_t *cache[600];
    static int built;
    if (!built) { for (int i = 0; pfs[i].fmt; i++) cache[pfs[i].fmt] = &pfs[i]; built = 1; }
    return fmt > 0 && fmt < 600 ? cache[fmt] : 0;
}
int mx_bpp(int fmt) { const pf_t *p = pf(fmt); return p ? p->bpp : 0; }
int mx_isdepth(int fmt) { return fmt == 250 || fmt == 252 || fmt == 255 || fmt == 260; }
int mx_hasstencil(int fmt) { return fmt == 253 || fmt == 255 || fmt == 260 || fmt == 261 || fmt == 262; }
int msl_fmt_isint(int fmt) {
    const pf_t *p = pf(fmt);
    if (!p) return 0;
    int k = p->kind;
    return k == PK_U8 || k == PK_S8 || k == PK_U16 || k == PK_S16 || k == PK_U32 || k == PK_S32 || k == PK_RGB10A2U ||
           k == PK_ST8 || k == PK_X32S8 || k == PK_X24S8;
}

/* sRGB */
static float srgb_dec[256];
static u8 srgb_enc[4097];
static void srgb_init(void) {
    static int done;
    if (done) return;
    for (int i = 0; i < 256; i++) { double s = i / 255.0; srgb_dec[i] = (float)(s <= 0.04045 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4)); }
    for (int i = 0; i <= 4096; i++) {
        double l = i / 4096.0, s = l <= 0.0031308 ? l * 12.92 : 1.055 * pow(l, 1 / 2.4) - 0.055;
        srgb_enc[i] = (u8)(s * 255 + 0.5);
    }
    done = 1;
}
static double cl01(double v) { return v != v ? 0 : v < 0 ? 0 : v > 1 ? 1 : v; }
static double clm11(double v) { return v != v ? 0 : v < -1 ? -1 : v > 1 ? 1 : v; }
static float uf_dec(u32 v, int mbits) {   /* float sem sinal de 10/11 bits */
    u32 e = v >> mbits, m = v & ((1u << mbits) - 1);
    if (!e) return (float)ldexp((double)m, -14 - mbits);
    if (e == 31) return m ? __builtin_nanf("") : __builtin_inff();
    return (float)ldexp(1.0 + (double)m / (1 << mbits), (int)e - 15);
}
static u32 uf_enc(double f, int mbits) {
    if (!(f > 0)) return 0;
    u16 h = msl_f2h((float)f);   /* half: 5 bits de expoente, 10 de mantissa */
    u32 e = (h >> 10) & 0x1f, m = h & 0x3ff;
    return e << mbits | (m >> (10 - mbits));
}

u8 *mx_texel(const msl_tex_t *t, int x, int y, int z, int slice, int lvl) {
    const pf_t *p = pf(t->fmt);
    int bpp = p ? p->bpp : 4;
    return t->data + t->loff[lvl] + (u64)slice * t->lslice[lvl] + ((u64)z * (u64)(t->h >> lvl ? t->h >> lvl : 1) + (u64)y) * t->lbpr[lvl] + (u64)x * bpp;
}
static void decode(const pf_t *p, const u8 *q, double o[4]) {
    o[0] = o[1] = o[2] = 0;
    o[3] = 1;
    int n = p->nc;
    switch (p->kind) {
    case PK_UN8:
        if (p->fl & PF_ALPHA) { o[3] = q[0] / 255.0; break; }
        for (int i = 0; i < n; i++) o[i] = (p->fl & PF_SRGB) && i < 3 ? srgb_dec[q[i]] : q[i] / 255.0;
        break;
    case PK_SN8: for (int i = 0; i < n; i++) o[i] = clm11((i8)q[i] / 127.0); break;
    case PK_U8: for (int i = 0; i < n; i++) o[i] = q[i]; break;
    case PK_S8: for (int i = 0; i < n; i++) o[i] = (i8)q[i]; break;
    case PK_UN16: for (int i = 0; i < n; i++) o[i] = ((const u16 *)q)[i] / 65535.0; break;
    case PK_SN16: for (int i = 0; i < n; i++) o[i] = clm11(((const i16 *)q)[i] / 32767.0); break;
    case PK_U16: for (int i = 0; i < n; i++) o[i] = ((const u16 *)q)[i]; break;
    case PK_S16: for (int i = 0; i < n; i++) o[i] = ((const i16 *)q)[i]; break;
    case PK_HALF: for (int i = 0; i < n; i++) o[i] = msl_h2f(((const u16 *)q)[i]); break;
    case PK_U32: for (int i = 0; i < n; i++) o[i] = ((const u32 *)q)[i]; break;
    case PK_S32: for (int i = 0; i < n; i++) o[i] = ((const i32 *)q)[i]; break;
    case PK_F32: for (int i = 0; i < n; i++) o[i] = ((const float *)q)[i]; break;
    case PK_RGB10A2: case PK_RGB10A2U: {
        u32 v = *(const u32 *)q;
        double s = p->kind == PK_RGB10A2 ? 1023.0 : 1.0, sa = p->kind == PK_RGB10A2 ? 3.0 : 1.0;
        o[0] = (v & 1023) / s; o[1] = ((v >> 10) & 1023) / s; o[2] = ((v >> 20) & 1023) / s; o[3] = (v >> 30) / sa;
        break;
    }
    case PK_RG11B10F: { u32 v = *(const u32 *)q; o[0] = uf_dec(v & 0x7ff, 6); o[1] = uf_dec((v >> 11) & 0x7ff, 6); o[2] = uf_dec(v >> 22, 5); break; }
    case PK_RGB9E5: {
        u32 v = *(const u32 *)q;
        double sc = ldexp(1.0, (int)(v >> 27) - 15 - 9);
        o[0] = (v & 511) * sc; o[1] = ((v >> 9) & 511) * sc; o[2] = ((v >> 18) & 511) * sc;
        break;
    }
    case PK_B5G6R5: { u16 v = *(const u16 *)q; o[0] = (v >> 11) / 31.0; o[1] = ((v >> 5) & 63) / 63.0; o[2] = (v & 31) / 31.0; break; }
    case PK_A1BGR5: { u16 v = *(const u16 *)q; o[3] = v >> 15; o[2] = ((v >> 10) & 31) / 31.0; o[1] = ((v >> 5) & 31) / 31.0; o[0] = (v & 31) / 31.0; break; }
    case PK_ABGR4: { u16 v = *(const u16 *)q; o[3] = (v >> 12) / 15.0; o[2] = ((v >> 8) & 15) / 15.0; o[1] = ((v >> 4) & 15) / 15.0; o[0] = (v & 15) / 15.0; break; }
    case PK_BGR5A1: { u16 v = *(const u16 *)q; o[2] = (v >> 11) / 31.0; o[1] = ((v >> 6) & 31) / 31.0; o[0] = ((v >> 1) & 31) / 31.0; o[3] = v & 1; break; }
    case PK_D16: o[0] = *(const u16 *)q / 65535.0; break;
    case PK_D32F: case PK_D32FS8: o[0] = *(const float *)q; break;
    case PK_D24S8: o[0] = (*(const u32 *)q & 0xffffff) / 16777215.0; break;
    case PK_ST8: o[0] = q[0]; break;
    case PK_X32S8: o[0] = q[4]; break;
    case PK_X24S8: o[0] = q[3]; break;
    case PK_BGRA10XR: for (int i = 0; i < 4; i++) o[i] = (((const u16 *)q)[i] >> 6) / 510.0 - 0.75294; break;
    case PK_BGR10XR: { u32 v = *(const u32 *)q; for (int i = 0; i < 3; i++) o[i] = ((v >> (i * 10)) & 1023) / 510.0 - 0.75294; break; }
    }
    if (p->fl & PF_BGRA) { double t = o[0]; o[0] = o[2]; o[2] = t; }
}
static void encode(const pf_t *p, u8 *q, const double in[4]) {
    double v[4] = {in[0], in[1], in[2], in[3]};
    if (p->fl & PF_BGRA) { double t = v[0]; v[0] = v[2]; v[2] = t; }
    int n = p->nc;
    switch (p->kind) {
    case PK_UN8:
        if (p->fl & PF_ALPHA) { q[0] = (u8)(cl01(v[3]) * 255 + 0.5); break; }
        for (int i = 0; i < n; i++) q[i] = (p->fl & PF_SRGB) && i < 3 ? srgb_enc[(int)(cl01(v[i]) * 4096 + 0.5)] : (u8)(cl01(v[i]) * 255 + 0.5);
        break;
    case PK_SN8: for (int i = 0; i < n; i++) q[i] = (u8)(i8)round(clm11(v[i]) * 127); break;
    case PK_U8: for (int i = 0; i < n; i++) q[i] = (u8)(v[i] < 0 ? 0 : v[i] > 255 ? 255 : v[i]); break;
    case PK_S8: for (int i = 0; i < n; i++) q[i] = (u8)(i8)(v[i] < -128 ? -128 : v[i] > 127 ? 127 : v[i]); break;
    case PK_UN16: for (int i = 0; i < n; i++) ((u16 *)q)[i] = (u16)(cl01(v[i]) * 65535 + 0.5); break;
    case PK_SN16: for (int i = 0; i < n; i++) ((i16 *)q)[i] = (i16)round(clm11(v[i]) * 32767); break;
    case PK_U16: for (int i = 0; i < n; i++) ((u16 *)q)[i] = (u16)(v[i] < 0 ? 0 : v[i] > 65535 ? 65535 : v[i]); break;
    case PK_S16: for (int i = 0; i < n; i++) ((i16 *)q)[i] = (i16)(v[i] < -32768 ? -32768 : v[i] > 32767 ? 32767 : v[i]); break;
    case PK_HALF: for (int i = 0; i < n; i++) ((u16 *)q)[i] = msl_f2h((float)v[i]); break;
    case PK_U32: for (int i = 0; i < n; i++) ((u32 *)q)[i] = (u32)(v[i] < 0 ? 0 : v[i] > 4294967295.0 ? 4294967295.0 : v[i]); break;
    case PK_S32: for (int i = 0; i < n; i++) ((i32 *)q)[i] = (i32)(v[i] < -2147483648.0 ? -2147483648.0 : v[i] > 2147483647.0 ? 2147483647.0 : v[i]); break;
    case PK_F32: for (int i = 0; i < n; i++) ((float *)q)[i] = (float)v[i]; break;
    case PK_RGB10A2:
        *(u32 *)q = (u32)(cl01(v[0]) * 1023 + 0.5) | (u32)(cl01(v[1]) * 1023 + 0.5) << 10 | (u32)(cl01(v[2]) * 1023 + 0.5) << 20 | (u32)(cl01(v[3]) * 3 + 0.5) << 30;
        break;
    case PK_RGB10A2U: *(u32 *)q = ((u32)v[0] & 1023) | ((u32)v[1] & 1023) << 10 | ((u32)v[2] & 1023) << 20 | ((u32)v[3] & 3) << 30; break;
    case PK_RG11B10F: *(u32 *)q = uf_enc(v[0], 6) | uf_enc(v[1], 6) << 11 | uf_enc(v[2], 5) << 22; break;
    case PK_RGB9E5: {
        double mx = v[0] > v[1] ? v[0] : v[1];
        if (v[2] > mx) mx = v[2];
        int e = 0;
        if (mx > 0) { e = (int)floor(__builtin_log2(mx)) + 1; if (e < -15) e = -15; if (e > 16) e = 16; }
        double sc = ldexp(1.0, 9 - e);
        u32 r = (u32)(cl01(v[0] * sc / 512.0) * 511 + 0.5), g = (u32)(cl01(v[1] * sc / 512.0) * 511 + 0.5), b = (u32)(cl01(v[2] * sc / 512.0) * 511 + 0.5);
        *(u32 *)q = r | g << 9 | b << 18 | (u32)(e + 15) << 27;
        break;
    }
    case PK_B5G6R5: *(u16 *)q = (u16)((u32)(cl01(v[0]) * 31 + 0.5) << 11 | (u32)(cl01(v[1]) * 63 + 0.5) << 5 | (u32)(cl01(v[2]) * 31 + 0.5)); break;
    case PK_A1BGR5: *(u16 *)q = (u16)((u32)(cl01(v[3]) + 0.5) << 15 | (u32)(cl01(v[2]) * 31 + 0.5) << 10 | (u32)(cl01(v[1]) * 31 + 0.5) << 5 | (u32)(cl01(v[0]) * 31 + 0.5)); break;
    case PK_ABGR4: *(u16 *)q = (u16)((u32)(cl01(v[3]) * 15 + 0.5) << 12 | (u32)(cl01(v[2]) * 15 + 0.5) << 8 | (u32)(cl01(v[1]) * 15 + 0.5) << 4 | (u32)(cl01(v[0]) * 15 + 0.5)); break;
    case PK_BGR5A1: *(u16 *)q = (u16)((u32)(cl01(v[2]) * 31 + 0.5) << 11 | (u32)(cl01(v[1]) * 31 + 0.5) << 6 | (u32)(cl01(v[0]) * 31 + 0.5) << 1 | (u32)(cl01(v[3]) + 0.5)); break;
    case PK_D16: *(u16 *)q = (u16)(cl01(v[0]) * 65535 + 0.5); break;
    case PK_D32F: case PK_D32FS8: *(float *)q = (float)v[0]; break;
    case PK_D24S8: *(u32 *)q = (*(u32 *)q & 0xff000000u) | (u32)(cl01(v[0]) * 16777215 + 0.5); break;
    case PK_ST8: q[0] = (u8)v[0]; break;
    case PK_X32S8: q[4] = (u8)v[0]; break;
    case PK_X24S8: q[3] = (u8)v[0]; break;
    case PK_BGRA10XR: for (int i = 0; i < 4; i++) { double x = (v[i] + 0.75294) * 510; ((u16 *)q)[i] = (u16)((u32)(x < 0 ? 0 : x > 1023 ? 1023 : x) << 6); } break;
    case PK_BGR10XR: { u32 o = 0; for (int i = 0; i < 3; i++) { double x = (v[i] + 0.75294) * 510; o |= (u32)(x < 0 ? 0 : x > 1023 ? 1023 : x) << (i * 10); } *(u32 *)q = o; break; }
    }
}
void msl_tex_load(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, double out[4]) {
    const pf_t *p = pf(t->fmt);
    if (!p || !t->data) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
    srgb_init();
    decode(p, mx_texel(t, x, y, z, slice, lvl), out);
}
void msl_tex_store(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, const double in[4]) {
    const pf_t *p = pf(t->fmt);
    if (!p || !t->data) return;
    srgb_init();
    encode(p, mx_texel(t, x, y, z, slice, lvl), in);
}
/* estêncil e profundidade crus */
static u32 st_get(const msl_tex_t *t, int x, int y, int slice, int lvl) {
    u8 *q = mx_texel(t, x, y, 0, slice, lvl);
    switch (t->fmt) {
    case 253: return q[0];
    case 255: case 262: return q[3];
    case 260: case 261: return q[4];
    }
    return 0;
}
static void st_set(const msl_tex_t *t, int x, int y, int slice, int lvl, u32 v) {
    u8 *q = mx_texel(t, x, y, 0, slice, lvl);
    switch (t->fmt) {
    case 253: q[0] = (u8)v; break;
    case 255: case 262: q[3] = (u8)v; break;
    case 260: case 261: q[4] = (u8)v; break;
    }
}

void mx_tex_layout(mtex_t *m, u64 bpr0) {
    msl_tex_t *t = &m->t;
    int bpp = mx_bpp(m->fmt);
    t->fmt = m->fmt;
    t->type = m->type;
    t->w = (int)m->w;
    t->h = (int)m->h;
    t->d = (int)m->d;
    t->levels = m->levels;
    int faces = m->type == MSL_TEX_CUBE || m->type == MSL_TEX_CUBE_ARRAY ? 6 : 1;
    t->slices = (m->arrlen ? m->arrlen : 1) * faces;
    u64 off = 0;
    for (int l = 0; l < m->levels && l < 16; l++) {
        u64 w = m->w >> l, h = m->h >> l, d = m->d >> l;
        if (!w) w = 1;
        if (!h) h = 1;
        if (!d) d = 1;
        u64 bpr = l == 0 && bpr0 ? bpr0 : w * bpp;
        t->loff[l] = off;
        t->lbpr[l] = bpr;
        t->lslice[l] = bpr * h * d;
        off += t->lslice[l] * t->slices;
    }
}
/* tamanho total de uma textura com o layout acima */
u64 mx_tex_bytes(mtex_t *m) {
    msl_tex_t *t = &m->t;
    int l = m->levels - 1;
    if (l < 0) return 0;
    return t->loff[l] + t->lslice[l] * t->slices;
}

/* ================================================================ amostragem */
static int addr_i(int mode, int i, int n, int *border) {
    if (n <= 0) return 0;
    switch (mode) {
    case 1: if (i < 0) i = -i - 1; return i >= n ? n - 1 : i;
    case 2: i %= n; return i < 0 ? i + n : i;
    case 3: { int m = i % (2 * n); if (m < 0) m += 2 * n; return m < n ? m : 2 * n - 1 - m; }
    case 4: case 5: if (i < 0 || i >= n) { *border = 1; return 0; } return i;
    default: return i < 0 ? 0 : i >= n ? n - 1 : i;
    }
}
static void border_col(const msl_samp_t *s, double o[4]) {
    o[0] = o[1] = o[2] = s->border == 2 ? 1 : 0;
    o[3] = s->border ? 1 : 0;
    if (s->addr[0] == 4) o[3] = 0;
}
static void fetch(const msl_tex_t *t, const msl_samp_t *s, int x, int y, int z, int slice, int lvl, double o[4]) {
    int w = t->w >> lvl, h = t->h >> lvl, d = t->d >> lvl, b = 0;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (d < 1) d = 1;
    x = addr_i(s->addr[0], x, w, &b);
    y = addr_i(s->addr[1], y, h, &b);
    z = addr_i(s->addr[2], z, d, &b);
    if (b) { border_col(s, o); return; }
    msl_tex_load(t, x, y, z, slice, lvl, o);
}
static void sample_lvl(const msl_tex_t *t, const msl_samp_t *s, const double *c, int nc, int slice, int lvl, int lin, double o[4]) {
    int w = t->w >> lvl, h = t->h >> lvl, d = t->d >> lvl;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (d < 1) d = 1;
    double u = c[0], v = nc > 1 ? c[1] : 0, r = nc > 2 && t->type == MSL_TEX_3D ? c[2] : 0;
    if (s->normalized) { u *= w; v *= h; r *= d; }
    else if (lvl) { u /= (1 << lvl); v /= (1 << lvl); }
    if (!lin) {
        fetch(t, s, (int)floor(u), nc > 1 ? (int)floor(v) : 0, t->type == MSL_TEX_3D ? (int)floor(r) : 0, slice, lvl, o);
        return;
    }
    double x = u - 0.5, y = v - 0.5, zz = r - 0.5;
    int x0 = (int)floor(x), y0 = (int)floor(y), z0 = (int)floor(zz);
    double fx = x - x0, fy = y - y0, fz = zz - z0;
    if (nc < 2) { y0 = 0; fy = 0; }
    int is3d = t->type == MSL_TEX_3D;
    if (!is3d) { z0 = 0; fz = 0; }
    for (int i = 0; i < 4; i++) o[i] = 0;
    for (int dz = 0; dz < (is3d ? 2 : 1); dz++)
        for (int dy = 0; dy < (nc > 1 ? 2 : 1); dy++)
            for (int dx = 0; dx < 2; dx++) {
                double wgt = (dx ? fx : 1 - fx) * (nc > 1 ? (dy ? fy : 1 - fy) : 1) * (is3d ? (dz ? fz : 1 - fz) : 1);
                if (wgt == 0) continue;
                double tx[4];
                fetch(t, s, x0 + dx, y0 + dy, z0 + dz, slice, lvl, tx);
                for (int i = 0; i < 4; i++) o[i] += wgt * tx[i];
            }
}
void msl_tex_sample(const msl_tex_t *t, const msl_samp_t *s, const double *coord, int ncoord, int slice, double lod, double out[4]) {
    double c[3] = {coord[0], ncoord > 1 ? coord[1] : 0, ncoord > 2 ? coord[2] : 0};
    if (!t->data) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
    int nc = ncoord;
    if (t->type == MSL_TEX_CUBE || t->type == MSL_TEX_CUBE_ARRAY) {
        double x = c[0], y = c[1], z = c[2], ax = fabs(x), ay = fabs(y), az = fabs(z), sc, tc, ma;
        int face;
        if (ax >= ay && ax >= az) { face = x >= 0 ? 0 : 1; sc = x >= 0 ? -z : z; tc = -y; ma = ax; }
        else if (ay >= az) { face = y >= 0 ? 2 : 3; sc = x; tc = y >= 0 ? z : -z; ma = ay; }
        else { face = z >= 0 ? 4 : 5; sc = z >= 0 ? x : -x; tc = -y; ma = az; }
        if (ma == 0) ma = 1;
        c[0] = (sc / ma + 1) * 0.5;
        c[1] = (tc / ma + 1) * 0.5;
        slice = slice * 6 + face;
        nc = 2;
        msl_samp_t s2 = *s;
        s2.addr[0] = s2.addr[1] = 0;
        s2.normalized = 1;
        s = &s2;
        /* segue com a cópia local */
        double l = lod < s->lodmin ? s->lodmin : lod > s->lodmax ? s->lodmax : lod;
        int lin = l > 0 ? s->minf : s->magf;
        int lv = s->mipf == 0 ? 0 : (int)(l + 0.5);
        if (lv >= t->levels) lv = t->levels - 1;
        if (lv < 0) lv = 0;
        sample_lvl(t, s, c, nc, slice, lv, lin, out);
        return;
    }
    if (slice >= t->slices) slice = t->slices - 1;
    if (slice < 0) slice = 0;
    double l = lod < s->lodmin ? s->lodmin : lod > s->lodmax ? s->lodmax : lod;
    if (l < 0) l = 0;
    int lin = l > 0 ? s->minf : s->magf;
    if (s->mipf == 0 || t->levels <= 1) { sample_lvl(t, s, c, nc, slice, 0, lin, out); return; }
    if (l > t->levels - 1) l = t->levels - 1;
    if (s->mipf == 1) { sample_lvl(t, s, c, nc, slice, (int)(l + 0.5), lin, out); return; }
    int l0 = (int)floor(l), l1 = l0 + 1 < t->levels ? l0 + 1 : l0;
    double f = l - l0, a[4], b[4];
    sample_lvl(t, s, c, nc, slice, l0, lin, a);
    if (f == 0 || l1 == l0) { for (int i = 0; i < 4; i++) out[i] = a[i]; return; }
    sample_lvl(t, s, c, nc, slice, l1, lin, b);
    for (int i = 0; i < 4; i++) out[i] = a[i] + (b[i] - a[i]) * f;
}

/* conversão para BGRA8 (CAMetalLayer com formato que o compositor não lê direto) */
void mx_to_bgra8(mtex_t *src, u8 *dst, u64 dbpr) {
    const pf_t *bg = pf(80);
    srgb_init();
    for (u64 y = 0; y < src->h; y++)
        for (u64 x = 0; x < src->w; x++) {
            double v[4];
            msl_tex_load(&src->t, (int)x, (int)y, 0, 0, 0, v);
            if ((src->fmt == 115 || src->fmt == 125 || src->fmt == 552 || src->fmt == 554)) {
                /* conteúdo linear: codifica em sRGB para a tela */
                for (int i = 0; i < 3; i++) { double l = cl01(v[i]); v[i] = l <= 0.0031308 ? l * 12.92 : 1.055 * pow(l, 1 / 2.4) - 0.055; }
            }
            encode(bg, dst + y * dbpr + x * 4, v);
        }
}

/* ================================================================ utilidades de blobs */
static void get_field(const u8 *p, int scal, int n, double *v) {
    for (int i = 0; i < n; i++) {
        switch (scal) {
        case MSL_FLOAT: v[i] = ((const float *)p)[i]; break;
        case MSL_HALF: v[i] = msl_h2f(((const u16 *)p)[i]); break;
        case MSL_INT: v[i] = ((const i32 *)p)[i]; break;
        case MSL_UINT: v[i] = ((const u32 *)p)[i]; break;
        case MSL_SHORT: v[i] = ((const i16 *)p)[i]; break;
        case MSL_USHORT: v[i] = ((const u16 *)p)[i]; break;
        case MSL_CHAR: v[i] = ((const i8 *)p)[i]; break;
        case MSL_UCHAR: case MSL_BOOL: v[i] = p[i]; break;
        case MSL_LONG: v[i] = (double)((const i64 *)p)[i]; break;
        case MSL_ULONG: v[i] = (double)((const u64 *)p)[i]; break;
        default: v[i] = 0;
        }
    }
}
static void put_field(u8 *p, int scal, int n, const double *v) {
    for (int i = 0; i < n; i++) {
        double x = v[i];
        switch (scal) {
        case MSL_FLOAT: ((float *)p)[i] = (float)x; break;
        case MSL_HALF: ((u16 *)p)[i] = msl_f2h((float)x); break;
        case MSL_INT: ((i32 *)p)[i] = (i32)x; break;
        case MSL_UINT: ((u32 *)p)[i] = (u32)(i64)x; break;
        case MSL_SHORT: ((i16 *)p)[i] = (i16)x; break;
        case MSL_USHORT: ((u16 *)p)[i] = (u16)(i64)x; break;
        case MSL_CHAR: ((i8 *)p)[i] = (i8)x; break;
        case MSL_UCHAR: ((u8 *)p)[i] = (u8)(i64)x; break;
        case MSL_BOOL: p[i] = x != 0; break;
        case MSL_LONG: ((i64 *)p)[i] = (i64)x; break;
        case MSL_ULONG: ((u64 *)p)[i] = (u64)x; break;
        }
    }
}
/* componente i de um campo com matriz/float3: os vetores de 3 têm 4 slots */
static int fstride(const msl_field_t *f) { return f->n == 3 && f->size >= 4 * (f->scal == MSL_HALF ? 2 : 4) ? 4 : f->n; }

/* formatos de vértice -> double[4] */
static void vfetch(int fmt, const u8 *q, double o[4]) {
    o[0] = o[1] = o[2] = 0;
    o[3] = 1;
    static const struct { u8 kind, n; } vf[] = {
        {0, 0}, {PK_U8, 2}, {PK_U8, 3}, {PK_U8, 4}, {PK_S8, 2}, {PK_S8, 3}, {PK_S8, 4}, {PK_UN8, 2}, {PK_UN8, 3}, {PK_UN8, 4},
        {PK_SN8, 2}, {PK_SN8, 3}, {PK_SN8, 4}, {PK_U16, 2}, {PK_U16, 3}, {PK_U16, 4}, {PK_S16, 2}, {PK_S16, 3}, {PK_S16, 4},
        {PK_UN16, 2}, {PK_UN16, 3}, {PK_UN16, 4}, {PK_SN16, 2}, {PK_SN16, 3}, {PK_SN16, 4}, {PK_HALF, 2}, {PK_HALF, 3},
        {PK_HALF, 4}, {PK_F32, 1}, {PK_F32, 2}, {PK_F32, 3}, {PK_F32, 4}, {PK_S32, 1}, {PK_S32, 2}, {PK_S32, 3}, {PK_S32, 4},
        {PK_U32, 1}, {PK_U32, 2}, {PK_U32, 3}, {PK_U32, 4}, {PK_RGB10A2, 4}, {PK_RGB10A2, 4}, {PK_UN8, 4}, {0, 0}, {0, 0},
        {PK_U8, 1}, {PK_S8, 1}, {PK_UN8, 1}, {PK_SN8, 1}, {PK_U16, 1}, {PK_S16, 1}, {PK_UN16, 1}, {PK_SN16, 1}, {PK_HALF, 1},
        {PK_UN8, 4}};
    if (fmt <= 0 || fmt > 54) return;
    pf_t p = {0, 0, vf[fmt].n, vf[fmt].kind, (u8)(fmt == 42 ? PF_BGRA : 0)};
    if (fmt == 40) {   /* Int1010102Normalized */
        u32 v = *(const u32 *)q;
        for (int i = 0; i < 3; i++) { i32 c = (i32)(v << (22 - i * 10)) >> 22; o[i] = clm11(c / 511.0); }
        o[3] = clm11(((i32)v >> 30) / 1.0);
        return;
    }
    decode(&p, q, o);
}

/* ================================================================ render */
typedef struct {
    double x, y, z, w;        /* recorte */
    u8 *blob;
} cv_t;                       /* vértice no espaço de recorte */
typedef struct {
    double x, y, z, iw;       /* tela */
    u8 *blob;
} sv_t;

typedef struct {
    rpass_t *rp;
    rstate_t *st;
    mrpipe_t *pp;
    int W, H;
    msl_tex_t *ct[8];
    int cl[8], cs[8], cz[8];
    msl_tex_t *dt, *stt;
    int dl, ds, sl, ss;
    u8 *fin;                  /* blob do stage_in do fragmento */
    u8 *fout;
    msl_ctx_t fc;
    int x0, y0, x1, y1;       /* recorte final (scissor/alvo) */
    double vx, vy, vw, vh, zn, zf;
    int early;                /* teste de profundidade antes do shader */
    u32 prim_id;
} rctx_t;

static int cmpf(int f, double a, double b) {
    switch (f) {
    case 0: return 0;
    case 1: return a < b;
    case 2: return a == b;
    case 3: return a <= b;
    case 4: return a > b;
    case 5: return a != b;
    case 6: return a >= b;
    default: return 1;
    }
}
static u32 stencil_op(int op, u32 cur, u32 ref) {
    switch (op) {
    case 1: return 0;
    case 2: return ref;
    case 3: return cur < 255 ? cur + 1 : 255;
    case 4: return cur > 0 ? cur - 1 : 0;
    case 5: return ~cur & 255;
    case 6: return (cur + 1) & 255;
    case 7: return (cur - 1) & 255;
    default: return cur;
    }
}
static double bfac(int f, int i, const double *s, const double *d, const float *bc) {
    switch (f) {
    case 0: return 0;
    case 1: return 1;
    case 2: case 15: return s[i];
    case 3: case 16: return 1 - s[i];
    case 4: case 17: return s[3];
    case 5: case 18: return 1 - s[3];
    case 6: return d[i];
    case 7: return 1 - d[i];
    case 8: return d[3];
    case 9: return 1 - d[3];
    case 10: return i < 3 ? (s[3] < 1 - d[3] ? s[3] : 1 - d[3]) : 1;
    case 11: return bc[i];
    case 12: return 1 - bc[i];
    case 13: return bc[3];
    case 14: return 1 - bc[3];
    }
    return 1;
}
static double bop(int op, double s, double fs, double d, double fd) {
    switch (op) {
    case 1: return s * fs - d * fd;
    case 2: return d * fd - s * fs;
    case 3: return s < d ? s : d;
    case 4: return s > d ? s : d;
    default: return s * fs + d * fd;
    }
}

/* um fragmento: l[] = baricêntricas na tela, v[] = vértices */
static void fragment(rctx_t *R, int px, int py, const double l[3], sv_t *const v[3], int front, const float *pcoord) {
    mrpipe_t *pp = R->pp;
    rstate_t *st = R->st;
    double z = l[0] * v[0]->z + l[1] * v[1]->z + l[2] * v[2]->z + st->dbias;
    if (st->dclip) z = z < 0 ? 0 : z > 1 ? 1 : z;
    double iw = l[0] * v[0]->iw + l[1] * v[1]->iw + l[2] * v[2]->iw;
    mdss_t *dss = st->dss;
    int dtest = R->dt && dss && (dss->dcmp != 7 || dss->dwrite);
    double dcur = 0;
    if (R->dt) { double t[4]; msl_tex_load(R->dt, px, py, 0, R->ds, R->dl, t); dcur = t[0]; }
    /* estêncil */
    int stest = R->stt && dss && dss->stencil_on;
    stencil_t *sd = dss ? (front ? &dss->front : &dss->back) : 0;
    u32 sref = front ? st->sref_f : st->sref_b, scur = 0;
    if (stest) {
        scur = st_get(R->stt, px, py, R->ss, R->sl);
        if (!cmpf(sd->cmp, (double)(sref & sd->rmask), (double)(scur & sd->rmask))) {
            u32 nv = stencil_op(sd->fail, scur, sref);
            st_set(R->stt, px, py, R->ss, R->sl, (scur & ~sd->wmask) | (nv & sd->wmask));
            return;
        }
    }
    if (R->early && dtest && !cmpf(dss->dcmp, z, dcur)) {
        if (stest) { u32 nv = stencil_op(sd->dfail, scur, sref); st_set(R->stt, px, py, R->ss, R->sl, (scur & ~sd->wmask) | (nv & sd->wmask)); }
        return;
    }
    /* stage_in do fragmento */
    msl_fn_t *ff = pp->ffn;
    if (ff) {
        double w[3];
        double pw = iw != 0 ? 1 / iw : 0;
        for (int i = 0; i < 3; i++) w[i] = l[i] * v[i]->iw * pw;
        for (int i = 0; i < pp->nfin; i++) {
            const msl_field_t *fi = &pp->fin[i];
            if (!fi->n) continue;
            int m = pp->fmap[i];
            u8 *dst = R->fin + fi->off;
            double out[16] = {0};
            if (m == -2) {
                out[0] = px + 0.5; out[1] = py + 0.5; out[2] = z; out[3] = iw;
            } else if (m >= 0) {
                const msl_field_t *vo = &pp->vout[m];
                int n = vo->n < fi->n ? vo->n : fi->n;
                if (n > 16) n = 16;
                int vs = fstride(vo);
                double a[16], b[16], c[16];
                if (vo->n > 4) {          /* matriz: por coluna */
                    get_field(v[0]->blob + vo->off, vo->scal, vo->size / (vo->scal == MSL_HALF ? 2 : 4), a);
                    get_field(v[1]->blob + vo->off, vo->scal, vo->size / (vo->scal == MSL_HALF ? 2 : 4), b);
                    get_field(v[2]->blob + vo->off, vo->scal, vo->size / (vo->scal == MSL_HALF ? 2 : 4), c);
                } else {
                    get_field(v[0]->blob + vo->off, vo->scal, vs, a);
                    get_field(v[1]->blob + vo->off, vo->scal, vs, b);
                    get_field(v[2]->blob + vo->off, vo->scal, vs, c);
                }
                int interp = pp->finterp[i];
                int isint = vo->scal != MSL_FLOAT && vo->scal != MSL_HALF;
                int cnt = vo->n > 4 ? vo->size / (vo->scal == MSL_HALF ? 2 : 4) : n;
                for (int k = 0; k < cnt && k < 16; k++) {
                    if (interp == MSL_INTERP_FLAT || isint) out[k] = a[k];
                    else if (interp == MSL_INTERP_NOPERSP) out[k] = l[0] * a[k] + l[1] * b[k] + l[2] * c[k];
                    else out[k] = w[0] * a[k] + w[1] * b[k] + w[2] * c[k];
                }
                if (vo->n > 4) { put_field(dst, fi->scal, cnt < fi->size / 4 ? cnt : fi->size / (fi->scal == MSL_HALF ? 2 : 4), out); continue; }
                put_field(dst, fi->scal, n, out);
                continue;
            }
            put_field(dst, fi->scal, fi->n > 16 ? 16 : fi->n, out);
        }
        msl_ctx_t *c = &R->fc;
        c->stage_in = R->fin;
        c->fragpos[0] = (float)(px + 0.5);
        c->fragpos[1] = (float)(py + 0.5);
        c->fragpos[2] = (float)z;
        c->fragpos[3] = (float)iw;
        c->front_facing = front;
        c->discarded = 0;
        c->primitive_id = R->prim_id;
        if (pcoord) { c->point_coord[0] = pcoord[0]; c->point_coord[1] = pcoord[1]; }
        memset(R->fout, 0, pp->fout_size + 16);
        msl_run(ff, c, R->fout);
        if (c->discarded) return;
        if (pp->fdepth >= 0) {
            double dv;
            get_field(R->fout + pp->fout[pp->fdepth].off, pp->fout[pp->fdepth].scal, 1, &dv);
            z = dv;
        }
    }
    if (!R->early && dtest && !cmpf(dss->dcmp, z, dcur)) {
        if (stest) { u32 nv = stencil_op(sd->dfail, scur, sref); st_set(R->stt, px, py, R->ss, R->sl, (scur & ~sd->wmask) | (nv & sd->wmask)); }
        return;
    }
    if (stest) { u32 nv = stencil_op(sd->pass, scur, sref); st_set(R->stt, px, py, R->ss, R->sl, (scur & ~sd->wmask) | (nv & sd->wmask)); }
    if (R->dt && dss && dss->dwrite) { double t[4] = {z, 0, 0, 0}; msl_tex_store(R->dt, px, py, 0, R->ds, R->dl, t); }
    if (!ff) return;
    for (int i = 0; i < 8; i++) {
        if (!R->ct[i] || pp->fcolor[i] < 0) continue;
        cattach_t *ca = &pp->ca[i];
        if (!ca->wmask) continue;
        const msl_field_t *fo = &pp->fout[pp->fcolor[i]];
        double s[4] = {0, 0, 0, 1};
        get_field(R->fout + fo->off, fo->scal, fo->n > 4 ? 4 : fo->n, s);
        msl_tex_t *t = R->ct[i];
        int isint = msl_fmt_isint(t->fmt);
        double d[4];
        int needd = (ca->blend && !isint) || ca->wmask != 0xF;
        if (needd) msl_tex_load(t, px, py, R->cz[i], R->cs[i], R->cl[i], d);
        if (ca->blend && !isint) {
            int fl = t->fmt != 115 && t->fmt != 125 && t->fmt != 25 && t->fmt != 65 && t->fmt != 55 && t->fmt != 105 && t->fmt != 92;
            if (fl) for (int k = 0; k < 4; k++) s[k] = cl01(s[k]);
            double o[4];
            for (int k = 0; k < 3; k++) o[k] = bop(ca->rgbop, s[k], bfac(ca->srgb, k, s, d, st->bc), d[k], bfac(ca->drgb, k, s, d, st->bc));
            o[3] = bop(ca->aop, s[3], bfac(ca->sa, 3, s, d, st->bc), d[3], bfac(ca->da, 3, s, d, st->bc));
            for (int k = 0; k < 4; k++) s[k] = o[k];
        }
        if (ca->wmask != 0xF) {
            if (!(ca->wmask & 8)) s[0] = d[0];
            if (!(ca->wmask & 4)) s[1] = d[1];
            if (!(ca->wmask & 2)) s[2] = d[2];
            if (!(ca->wmask & 1)) s[3] = d[3];
        }
        msl_tex_store(t, px, py, R->cz[i], R->cs[i], R->cl[i], s);
    }
}

static sv_t to_screen(rctx_t *R, const cv_t *c) {
    sv_t s;
    double iw = c->w != 0 ? 1 / c->w : 0;
    s.x = R->vx + (c->x * iw + 1) * 0.5 * R->vw;
    s.y = R->vy + (1 - c->y * iw) * 0.5 * R->vh;
    s.z = R->zn + c->z * iw * (R->zf - R->zn);
    s.iw = iw;
    s.blob = c->blob;
    return s;
}

static void raster_line(rctx_t *R, sv_t *a, sv_t *b) {
    double dx = b->x - a->x, dy = b->y - a->y;
    double len = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
    int steps = (int)(len + 0.5);
    if (steps < 1) steps = 1;
    sv_t *vv[3] = {a, b, b};
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        int px = (int)floor(a->x + dx * t), py = (int)floor(a->y + dy * t);
        if (i == steps && steps > 1) break;  /* último pixel fica para o segmento seguinte */
        if (px < R->x0 || py < R->y0 || px >= R->x1 || py >= R->y1) continue;
        double l[3] = {1 - t, t, 0};
        fragment(R, px, py, l, vv, 1, 0);
    }
}

static void raster_tri(rctx_t *R, sv_t *v0, sv_t *v1, sv_t *v2) {
    rstate_t *st = R->st;
    double area = (v1->x - v0->x) * (v2->y - v0->y) - (v2->x - v0->x) * (v1->y - v0->y);
    if (area == 0 || area != area) return;
    /* área > 0 = sentido horário na tela (y para baixo) */
    int cw = area > 0;
    int front = st->wind == 0 ? cw : !cw;
    if ((st->cull == 1 && front) || (st->cull == 2 && !front)) return;
    if (st->fill == 1) { raster_line(R, v0, v1); raster_line(R, v1, v2); raster_line(R, v2, v0); return; }
    sv_t *v[3] = {v0, v1, v2};
    if (area < 0) { v[1] = v2; v[2] = v1; area = -area; }
    double minx = v[0]->x, maxx = v[0]->x, miny = v[0]->y, maxy = v[0]->y;
    for (int i = 1; i < 3; i++) {
        if (v[i]->x < minx) minx = v[i]->x;
        if (v[i]->x > maxx) maxx = v[i]->x;
        if (v[i]->y < miny) miny = v[i]->y;
        if (v[i]->y > maxy) maxy = v[i]->y;
    }
    int x0 = (int)floor(minx), x1 = (int)floor(maxx) + 1, y0 = (int)floor(miny), y1 = (int)floor(maxy) + 1;
    if (x0 < R->x0) x0 = R->x0;
    if (y0 < R->y0) y0 = R->y0;
    if (x1 > R->x1) x1 = R->x1;
    if (y1 > R->y1) y1 = R->y1;
    if (x0 >= x1 || y0 >= y1) return;
    /* arestas e (a->b): E = (bx-ax)(py-ay) - (by-ay)(px-ax); positivo dentro */
    double ex[3], ey[3];
    int own[3];
    for (int i = 0; i < 3; i++) {
        sv_t *a = v[(i + 1) % 3], *b = v[(i + 2) % 3];
        ex[i] = b->x - a->x;
        ey[i] = b->y - a->y;
        own[i] = ey[i] > 0 || (ey[i] == 0 && ex[i] < 0);
    }
    double inv = 1 / area;
    sv_t *vo[3] = {v0, v1, v2};
    for (int py = y0; py < y1; py++) {
        double cy = py + 0.5;
        for (int px = x0; px < x1; px++) {
            double cx = px + 0.5, l[3];
            int in = 1;
            for (int i = 0; i < 3; i++) {
                sv_t *a = v[(i + 1) % 3];
                double e = ex[i] * (cy - a->y) - ey[i] * (cx - a->x);
                if (e < 0 || (e == 0 && !own[i])) { in = 0; break; }
                l[i] = e * inv;
            }
            if (!in) continue;
            /* volta para a ordem original dos vértices (o flat usa o primeiro) */
            double lo[3];
            if (v[1] == v0 || v[1] == v1) { lo[0] = l[0]; lo[1] = l[1]; lo[2] = l[2]; }
            else { lo[0] = l[0]; lo[1] = l[2]; lo[2] = l[1]; }
            fragment(R, px, py, lo, vo, front, 0);
        }
    }
}

/* recorte de um polígono contra um plano: d = a·(x,y,z,w) */
typedef struct { cv_t v[24]; int n; } poly_t;
static double plane_d(const cv_t *c, int p) {
    switch (p) {
    case 0: return c->z;                            /* z >= 0 */
    case 1: return c->w - c->z;                     /* z <= w */
    case 2: return c->w * 4 + c->x;                 /* faixa de proteção: x >= -4w */
    case 3: return c->w * 4 - c->x;
    case 4: return c->w * 4 + c->y;
    case 5: return c->w * 4 - c->y;
    case 6: return c->w - 1e-6;                     /* w > 0 */
    }
    return 1;
}
static void lerp_blob(mrpipe_t *pp, u8 *dst, const u8 *a, const u8 *b, double t) {
    memcpy(dst, a, pp->vout_size);
    for (int i = 0; i < pp->nvout; i++) {
        const msl_field_t *f = &pp->vout[i];
        if (f->scal != MSL_FLOAT && f->scal != MSL_HALF) continue;
        int cnt = f->size / (f->scal == MSL_HALF ? 2 : 4);
        double x[16], y[16];
        if (cnt > 16) cnt = 16;
        get_field(a + f->off, f->scal, cnt, x);
        get_field(b + f->off, f->scal, cnt, y);
        for (int k = 0; k < cnt; k++) x[k] += (y[k] - x[k]) * t;
        put_field(dst + f->off, f->scal, cnt, x);
    }
}
typedef struct { u8 *mem; u64 used, cap; } scratch_t;
static u8 *scr_alloc(scratch_t *s, u64 n) {
    if (s->used + n > s->cap) return 0;
    u8 *p = s->mem + s->used;
    s->used += n;
    return p;
}
static void clip_and_draw(rctx_t *R, cv_t *a, cv_t *b, cv_t *c, scratch_t *scr) {
    int need = 0;
    cv_t *in[3] = {a, b, c};
    for (int i = 0; i < 3 && !need; i++)
        for (int p = 0; p < 7; p++) {
            if (R->st->dclip && p <= 1) continue;
            if (plane_d(in[i], p) < 0) { need = 1; break; }
        }
    if (!need) {
        sv_t s0 = to_screen(R, a), s1 = to_screen(R, b), s2 = to_screen(R, c);
        raster_tri(R, &s0, &s1, &s2);
        return;
    }
    poly_t P, Q;
    P.n = 3;
    P.v[0] = *a; P.v[1] = *b; P.v[2] = *c;
    u64 mark = scr->used;
    for (int p = 0; p < 7; p++) {
        if (R->st->dclip && p <= 1) continue;     /* depthClipMode = clamp */
        Q.n = 0;
        for (int i = 0; i < P.n; i++) {
            cv_t *u = &P.v[i], *w = &P.v[(i + 1) % P.n];
            double du = plane_d(u, p), dw = plane_d(w, p);
            if (du >= 0 && Q.n < 24) Q.v[Q.n++] = *u;
            if ((du >= 0) != (dw >= 0) && Q.n < 24) {
                double t = du / (du - dw);
                cv_t nv;
                nv.x = u->x + (w->x - u->x) * t;
                nv.y = u->y + (w->y - u->y) * t;
                nv.z = u->z + (w->z - u->z) * t;
                nv.w = u->w + (w->w - u->w) * t;
                nv.blob = scr_alloc(scr, R->pp->vout_size + 16);
                if (!nv.blob) { scr->used = mark; return; }
                lerp_blob(R->pp, nv.blob, u->blob, w->blob, t);
                /* a posição dentro do blob também acompanha */
                Q.v[Q.n++] = nv;
            }
        }
        P = Q;
        if (P.n < 3) { scr->used = mark; return; }
    }
    sv_t sv[24];
    for (int i = 0; i < P.n; i++) sv[i] = to_screen(R, &P.v[i]);
    for (int i = 1; i + 1 < P.n; i++) raster_tri(R, &sv[0], &sv[i], &sv[i + 1]);
    scr->used = mark;
}
static int clip_point_in(const cv_t *c) { return c->w > 0 && c->z >= 0 && c->z <= c->w && c->x >= -c->w * 4 && c->x <= c->w * 4 && c->y >= -c->w * 4 && c->y <= c->w * 4; }

static u8 *bind_ptr(const bind_t *b) {
    if (b->bytes) return b->bytes;
    if (!b->obj) return 0;
    return ((mbuf_t *)b->obj)->p + b->off;
}

static void begin_pass(rpass_t *rp) {
    for (int i = 0; i < 8; i++) {
        attach_t *a = &rp->c[i];
        if (!a->tex || a->load != 2) continue;
        msl_tex_t *t = &a->tex->t;
        int w = t->w >> a->level, h = t->h >> a->level;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        double cc[4] = {a->cc.r, a->cc.g, a->cc.b, a->cc.a};
        int bpp = mx_bpp(t->fmt);
        u8 px[16];
        srgb_init();
        encode(pf(t->fmt), px, cc);
        for (int y = 0; y < h; y++) {
            u8 *row = mx_texel(t, 0, y, a->plane, a->slice, a->level);
            for (int x = 0; x < w; x++) memcpy(row + x * bpp, px, bpp);
        }
    }
    if (rp->d.tex && rp->d.load == 2) {
        msl_tex_t *t = &rp->d.tex->t;
        int w = t->w >> rp->d.level, h = t->h >> rp->d.level;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        double cd[4] = {rp->d.cd, 0, 0, 0};
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) msl_tex_store(t, x, y, 0, rp->d.slice, rp->d.level, cd);
    }
    if (rp->s.tex && rp->s.load == 2 && mx_hasstencil(rp->s.tex->fmt)) {
        msl_tex_t *t = &rp->s.tex->t;
        int w = t->w >> rp->s.level, h = t->h >> rp->s.level;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) st_set(t, x, y, rp->s.slice, rp->s.level, rp->s.cs);
    }
}
static void copy_level(mtex_t *s, int sl, int ss, mtex_t *d, int dl, int ds);
static void end_pass(rpass_t *rp) {
    for (int i = 0; i < 8; i++) {
        attach_t *a = &rp->c[i];
        if (a->tex && a->resolve && (a->store == 2 || a->store == 3) && a->resolve != a->tex)
            copy_level(a->tex, a->level, a->slice, a->resolve, a->rlevel, a->rslice);
    }
}

static rpass_t *g_rp;           /* passada de render corrente (entre C_RBEGIN e C_REND) */

static void draw(cmd_t *c) {
    rpass_t *rp = g_rp;
    rstate_t *st = c->d.st;
    mrpipe_t *pp = st->pipe;
    if (!rp || !pp || !pp->vfn) return;
    rctx_t R;
    memset(&R, 0, sizeof R);
    R.rp = rp;
    R.st = st;
    R.pp = pp;
    int W = rp->rtw ? (int)rp->rtw : 0x7fffffff, H = rp->rth ? (int)rp->rth : 0x7fffffff;
    for (int i = 0; i < 8; i++) {
        attach_t *a = &rp->c[i];
        if (!a->tex) continue;
        R.ct[i] = &a->tex->t;
        R.cl[i] = a->level;
        R.cs[i] = a->slice;
        R.cz[i] = a->plane;
        int w = a->tex->t.w >> a->level, h = a->tex->t.h >> a->level;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        if (w < W) W = w;
        if (h < H) H = h;
    }
    if (rp->d.tex) {
        R.dt = &rp->d.tex->t; R.dl = rp->d.level; R.ds = rp->d.slice;
        int w = R.dt->w >> R.dl, h = R.dt->h >> R.dl;
        if (w < W) W = w > 0 ? w : 1;
        if (h < H) H = h > 0 ? h : 1;
        if (!mx_isdepth(rp->d.tex->fmt)) R.dt = 0;
    }
    if (rp->s.tex && mx_hasstencil(rp->s.tex->fmt)) { R.stt = &rp->s.tex->t; R.sl = rp->s.level; R.ss = rp->s.slice; }
    if (W == 0x7fffffff) W = 1;
    if (H == 0x7fffffff) H = 1;
    R.W = W;
    R.H = H;
    if (st->vp_set) { R.vx = st->vp.x; R.vy = st->vp.y; R.vw = st->vp.w; R.vh = st->vp.h; R.zn = st->vp.zn; R.zf = st->vp.zf; }
    else { R.vw = W; R.vh = H; R.zn = 0; R.zf = 1; }
    R.x0 = 0; R.y0 = 0; R.x1 = W; R.y1 = H;
    if (st->sc_set) {
        if ((int)st->sc.x > R.x0) R.x0 = (int)st->sc.x;
        if ((int)st->sc.y > R.y0) R.y0 = (int)st->sc.y;
        if ((int)(st->sc.x + st->sc.w) < R.x1) R.x1 = (int)(st->sc.x + st->sc.w);
        if ((int)(st->sc.y + st->sc.h) < R.y1) R.y1 = (int)(st->sc.y + st->sc.h);
    }
    if (R.x0 >= R.x1 || R.y0 >= R.y1 || !pp->raster) return;
    R.early = pp->fdepth < 0 && !pp->discards;
    /* contexto do fragmento */
    for (int i = 0; i < 31; i++) R.fc.buf[i] = bind_ptr(&st->fb[i]);
    for (int i = 0; i < 32; i++) R.fc.tex[i] = st->ft[i] ? &st->ft[i]->t : 0;
    for (int i = 0; i < 16; i++) R.fc.samp[i] = st->fs[i] ? &st->fs[i]->s : 0;
    R.fin = calloc(1, pp->fin_size + 64);
    R.fout = calloc(1, pp->fout_size + 64);
    /* contexto do vértice */
    msl_ctx_t vc;
    memset(&vc, 0, sizeof vc);
    for (int i = 0; i < 31; i++) vc.buf[i] = bind_ptr(&st->vb[i]);
    for (int i = 0; i < 32; i++) vc.tex[i] = st->vt[i] ? &st->vt[i]->t : 0;
    for (int i = 0; i < 16; i++) vc.samp[i] = st->vs[i] ? &st->vs[i]->s : 0;
    u8 *vin = calloc(1, pp->vin_size + 64);
    vc.stage_in = pp->vin_size ? vin : 0;

    u64 count = c->d.indexed ? c->d.icnt : c->d.vcount, icount = c->d.icount ? c->d.icount : 1;
    u8 *ip = 0;
    int isz = c->d.itype ? 4 : 2;
    if (c->d.indexed) { if (!c->d.ib) goto out; ip = c->d.ib->p + c->d.ioff; }
    if (!count) goto out;
    {
        int osz = pp->vout_size + 16;
        u8 *blobs = malloc((u64)osz * count);
        cv_t *cvs = malloc(sizeof(cv_t) * count);
        u32 *ids = malloc(sizeof(u32) * count);
        scratch_t scr = {malloc(osz * 512), 0, (u64)osz * 512};
        if (!blobs || !cvs || !ids || !scr.mem) { free(blobs); free(cvs); free(ids); free(scr.mem); goto out; }
        const msl_field_t *pos = pp->pos_field >= 0 ? &pp->vout[pp->pos_field] : 0;
        for (u64 inst = 0; inst < icount; inst++) {
            u32 iid = (u32)(inst + c->d.binst);
            /* sombreamento dos vértices (com cache por índice dentro da instância) */
            for (u64 i = 0; i < count; i++) {
                u32 id;
                if (ip) {
                    id = isz == 4 ? ((u32 *)ip)[i] : ((u16 *)ip)[i];
                    ids[i] = id;
                    u32 restart = isz == 4 ? 0xffffffffu : 0xffffu;
                    if (id == restart) { cvs[i].blob = 0; continue; }
                    id = (u32)((i64)id + c->d.bvert);
                    /* reaproveita um vértice já sombreado com o mesmo índice próximo */
                    int hit = -1;
                    for (u64 j = i > 16 ? i - 16 : 0; j < i; j++) if (ids[j] == ids[i] && cvs[j].blob) { hit = (int)j; break; }
                    if (hit >= 0) { cvs[i] = cvs[hit]; continue; }
                } else id = (u32)(c->d.vstart + i);
                vc.vid = id;
                vc.iid = iid;
                vc.base_vertex = (u32)c->d.bvert;
                vc.base_instance = (u32)c->d.binst;
                if (pp->vin_size) {
                    memset(vin, 0, pp->vin_size);
                    for (int f = 0; f < pp->nvin; f++) {
                        const msl_field_t *fl = &pp->vin[f];
                        if (fl->attr != A_ATTRIBUTE || fl->idx < 0 || fl->idx > 30 || !fl->n) continue;
                        vattr_t *a = &pp->attr[fl->idx];
                        if (!a->fmt || a->buf < 0 || a->buf > 30) continue;
                        vlayout_t *lay = &pp->lay[a->buf];
                        u64 e = lay->step == 0 ? 0 : lay->step == 2 ? (lay->rate ? iid / lay->rate : 0) : id;
                        u8 *bp = vc.buf[a->buf];
                        if (!bp) continue;
                        double v[4];
                        vfetch(a->fmt, bp + e * lay->stride + a->off, v);
                        put_field(vin + fl->off, fl->scal, fl->n > 4 ? 4 : fl->n, v);
                    }
                }
                u8 *bl = blobs + (u64)osz * i;
                memset(bl, 0, osz);
                vc.discarded = 0;
                msl_run(pp->vfn, &vc, bl);
                cv_t *cv = &cvs[i];
                cv->blob = bl;
                if (pos) {
                    double p4[4] = {0, 0, 0, 1};
                    get_field(bl + pos->off, pos->scal, 4, p4);
                    cv->x = p4[0]; cv->y = p4[1]; cv->z = p4[2]; cv->w = p4[3];
                } else { cv->x = cv->y = cv->z = 0; cv->w = 1; }
            }
            /* montagem das primitivas */
            int prim = c->d.prim;
            R.prim_id = 0;
            if (prim == 3) {
                for (u64 i = 0; i + 2 < count; i += 3) {
                    if (!cvs[i].blob || !cvs[i + 1].blob || !cvs[i + 2].blob) continue;
                    clip_and_draw(&R, &cvs[i], &cvs[i + 1], &cvs[i + 2], &scr);
                    R.prim_id++;
                }
            } else if (prim == 4) {
                u64 s = 0;
                for (u64 i = 0; i < count; i++) {
                    if (!cvs[i].blob) { s = i + 1; continue; }
                    if (i - s < 2) continue;
                    int odd = (int)((i - s) & 1);
                    cv_t *a = &cvs[i - 2], *b = &cvs[i - 1], *cc = &cvs[i];
                    if (odd) clip_and_draw(&R, b, a, cc, &scr);
                    else clip_and_draw(&R, a, b, cc, &scr);
                    R.prim_id++;
                }
            } else if (prim == 1 || prim == 2) {
                u64 step = prim == 1 ? 2 : 1;
                for (u64 i = 0; i + 1 < count; i += step) {
                    if (!cvs[i].blob || !cvs[i + 1].blob) continue;
                    if (!clip_point_in(&cvs[i]) || !clip_point_in(&cvs[i + 1])) {
                        if (cvs[i].w <= 0 || cvs[i + 1].w <= 0) continue;
                    }
                    sv_t a = to_screen(&R, &cvs[i]), b = to_screen(&R, &cvs[i + 1]);
                    raster_line(&R, &a, &b);
                    R.prim_id++;
                }
            } else if (prim == 0) {
                const msl_field_t *ps = pp->psize_field >= 0 ? &pp->vout[pp->psize_field] : 0;
                for (u64 i = 0; i < count; i++) {
                    if (!cvs[i].blob || !clip_point_in(&cvs[i])) continue;
                    sv_t a = to_screen(&R, &cvs[i]);
                    double size = 1;
                    if (ps) get_field(cvs[i].blob + ps->off, ps->scal, 1, &size);
                    if (size < 1) size = 1;
                    int px0 = (int)floor(a.x - size / 2 + 0.5), py0 = (int)floor(a.y - size / 2 + 0.5);
                    int n = (int)(size + 0.5);
                    sv_t *vv[3] = {&a, &a, &a};
                    double l[3] = {1, 0, 0};
                    for (int y = py0; y < py0 + n; y++)
                        for (int x = px0; x < px0 + n; x++) {
                            if (x < R.x0 || y < R.y0 || x >= R.x1 || y >= R.y1) continue;
                            float pc[2] = {(float)((x + 0.5 - (a.x - size / 2)) / size), (float)((y + 0.5 - (a.y - size / 2)) / size)};
                            fragment(&R, x, y, l, vv, 1, pc);
                        }
                    R.prim_id++;
                }
            }
        }
        free(blobs);
        free(cvs);
        free(ids);
        free(scr.mem);
    }
out:
    free(vin);
    free(R.fin);
    free(R.fout);
}

/* ================================================================ compute */
static void dispatch(cmd_t *c) {
    cstate_t *st = c->c.st;
    mcpipe_t *cp = st->pipe;
    if (!cp || !cp->fn) return;
    u32 g[3] = {c->c.g[0], c->c.g[1], c->c.g[2]}, t[3] = {c->c.t[0], c->c.t[1], c->c.t[2]};
    if (c->c.indirect) { u32 *a = (u32 *)(c->c.indirect->p + c->c.indoff); g[0] = a[0]; g[1] = a[1]; g[2] = a[2]; }
    for (int i = 0; i < 3; i++) if (!t[i]) t[i] = 1;
    u32 grid[3];
    for (int i = 0; i < 3; i++) grid[i] = c->c.exact ? c->c.grid[i] : g[i] * t[i];
    msl_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    for (int i = 0; i < 31; i++) ctx.buf[i] = bind_ptr(&st->b[i]);
    for (int i = 0; i < 32; i++) ctx.tex[i] = st->t[i] ? &st->t[i]->t : 0;
    for (int i = 0; i < 16; i++) ctx.samp[i] = st->s[i] ? &st->s[i]->s : 0;
    for (int i = 0; i < 31; i++) if (st->tglen[i]) ctx.tgbuf[i] = calloc(1, st->tglen[i] + 16);
    for (int i = 0; i < 3; i++) { ctx.tpg[i] = grid[i]; ctx.tgpg[i] = g[i]; }
    u32 tg[3];
    for (tg[2] = 0; tg[2] < g[2]; tg[2]++)
        for (tg[1] = 0; tg[1] < g[1]; tg[1]++)
            for (tg[0] = 0; tg[0] < g[0]; tg[0]++) {
                for (int i = 0; i < 31; i++) if (ctx.tgbuf[i]) memset(ctx.tgbuf[i], 0, st->tglen[i]);
                msl_run_group(cp->fn, &ctx, tg, t, grid, c->c.exact);
            }
    for (int i = 0; i < 31; i++) free(ctx.tgbuf[i]);
}

/* ================================================================ blit */
static void copy_level(mtex_t *s, int sl, int ss, mtex_t *d, int dl, int ds) {
    int w = s->t.w >> sl, h = s->t.h >> sl;
    int dw = d->t.w >> dl, dh = d->t.h >> dl;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (dw < w) w = dw;
    if (dh < h) h = dh;
    int sb = mx_bpp(s->fmt), db = mx_bpp(d->fmt);
    for (int y = 0; y < h; y++) {
        if (sb == db) memcpy(mx_texel(&d->t, 0, y, 0, ds, dl), mx_texel(&s->t, 0, y, 0, ss, sl), (u64)w * sb);
        else for (int x = 0; x < w; x++) { double v[4]; msl_tex_load(&s->t, x, y, 0, ss, sl, v); msl_tex_store(&d->t, x, y, 0, ds, dl, v); }
    }
}
static void blit_tt(cmd_t *c) {
    mtex_t *s = c->tt.s, *d = c->tt.d;
    if (c->tt.whole) {
        int nl = s->levels < d->levels ? s->levels : d->levels, ns = s->t.slices < d->t.slices ? s->t.slices : d->t.slices;
        for (int l = 0; l < nl; l++) for (int sl = 0; sl < ns; sl++) copy_level(s, l, sl, d, l, sl);
        return;
    }
    if (c->tt.nlevels) {
        for (u64 l = 0; l < c->tt.nlevels; l++)
            for (u64 sl = 0; sl < c->tt.nslices; sl++) copy_level(s, (int)(c->tt.sl + l), (int)(c->tt.ss + sl), d, (int)(c->tt.dl + l), (int)(c->tt.ds + sl));
        return;
    }
    int sb = mx_bpp(s->fmt), db = mx_bpp(d->fmt);
    for (u64 z = 0; z < c->tt.sz.d; z++)
        for (u64 y = 0; y < c->tt.sz.h; y++) {
            int sy = (int)(c->tt.so.y + y), dy = (int)(c->tt.dorg.y + y), sz = (int)(c->tt.so.z + z), dz = (int)(c->tt.dorg.z + z);
            if (sy >= (s->t.h >> c->tt.sl) && s->t.h > 1) break;
            if (sb == db) {
                memcpy(mx_texel(&d->t, (int)c->tt.dorg.x, dy, dz, (int)c->tt.ds, (int)c->tt.dl),
                       mx_texel(&s->t, (int)c->tt.so.x, sy, sz, (int)c->tt.ss, (int)c->tt.sl), c->tt.sz.w * sb);
            } else
                for (u64 x = 0; x < c->tt.sz.w; x++) {
                    double v[4];
                    msl_tex_load(&s->t, (int)(c->tt.so.x + x), sy, sz, (int)c->tt.ss, (int)c->tt.sl, v);
                    msl_tex_store(&d->t, (int)(c->tt.dorg.x + x), dy, dz, (int)c->tt.ds, (int)c->tt.dl, v);
                }
        }
}
static void blit_bt(cmd_t *c, int to_tex) {
    mtex_t *t = c->bt.t;
    mbuf_t *b = c->bt.b;
    if (!t || !b) return;
    int bpp = mx_bpp(t->fmt);
    u64 row = c->bt.sz.w * bpp;
    u64 bpi = c->bt.bpi ? c->bt.bpi : c->bt.bpr * c->bt.sz.h;
    for (u64 z = 0; z < (c->bt.sz.d ? c->bt.sz.d : 1); z++)
        for (u64 y = 0; y < c->bt.sz.h; y++) {
            u8 *bp = b->p + c->bt.off + z * bpi + y * c->bt.bpr;
            int tz = t->type == MSL_TEX_3D ? (int)(c->bt.org.z + z) : 0;
            int sl = t->type == MSL_TEX_3D ? (int)c->bt.slice : (int)(c->bt.slice + z);
            u8 *tp = mx_texel(&t->t, (int)c->bt.org.x, (int)(c->bt.org.y + y), tz, sl, (int)c->bt.level);
            if (bp + row > b->p + b->len) return;
            if (to_tex) memcpy(tp, bp, row); else memcpy(bp, tp, row);
        }
}
void mx_gen_mips(mtex_t *m) {
    for (int l = 1; l < m->levels; l++)
        for (int s = 0; s < m->t.slices; s++) {
            int w = m->t.w >> l, h = m->t.h >> l, pw = m->t.w >> (l - 1), ph = m->t.h >> (l - 1);
            if (w < 1) w = 1;
            if (h < 1) h = 1;
            if (pw < 1) pw = 1;
            if (ph < 1) ph = 1;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    double acc[4] = {0, 0, 0, 0}, v[4];
                    int n = 0;
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++) {
                            int sx = x * 2 + dx, sy = y * 2 + dy;
                            if (sx >= pw || sy >= ph) continue;
                            msl_tex_load(&m->t, sx, sy, 0, s, l - 1, v);
                            for (int i = 0; i < 4; i++) acc[i] += v[i];
                            n++;
                        }
                    if (n) for (int i = 0; i < 4; i++) acc[i] /= n;
                    msl_tex_store(&m->t, x, y, 0, s, l, acc);
                }
        }
}

/* MPSImageBilinearScale/LanczosScale: destino[x,y] = fonte[(x - tx)/sx, (y - ty)/sy]; na redução cada
 * pixel do destino é a média de uma grade de amostras bilineares (filtro de caixa sobre a área coberta) */
static void scale_tex(cmd_t *c) {
    mtex_t *s = c->sc.s, *d = c->sc.d;
    if (!s || !d || !s->t.data || !d->t.data) return;
    double sx = c->sc.has_t ? c->sc.t[0] : (double)d->w / (double)s->w, sy = c->sc.has_t ? c->sc.t[1] : (double)d->h / (double)s->h;
    double tx = c->sc.has_t ? c->sc.t[2] : 0, ty = c->sc.has_t ? c->sc.t[3] : 0;
    if (sx == 0 || sy == 0) return;
    int kx = sx < 1 ? (int)ceil(1 / sx) : 1, ky = sy < 1 ? (int)ceil(1 / sy) : 1;
    if (kx > 8) kx = 8;
    if (ky > 8) ky = 8;
    if (c->sc.lanczos) { if (kx < 2 && sx < 1) kx = 2; if (ky < 2 && sy < 1) ky = 2; }
    msl_samp_t sm;
    memset(&sm, 0, sizeof sm);
    sm.minf = sm.magf = 1;
    sm.lodmax = 0;
    for (int y = 0; y < (int)d->h; y++)
        for (int x = 0; x < (int)d->w; x++) {
            double acc[4] = {0, 0, 0, 0}, v[4];
            for (int j = 0; j < ky; j++)
                for (int i = 0; i < kx; i++) {
                    double dx = x + (i + 0.5) / kx, dy = y + (j + 0.5) / ky;
                    double cc[2] = {(dx - tx) / sx, (dy - ty) / sy};
                    msl_tex_sample(&s->t, &sm, cc, 2, 0, 0, v);
                    for (int k = 0; k < 4; k++) acc[k] += v[k];
                }
            for (int k = 0; k < 4; k++) acc[k] /= kx * ky;
            msl_tex_store(&d->t, x, y, 0, 0, 0, acc);
        }
}

void mx_exec_cmd(cmd_t *c) {
    switch (c->k) {
    case C_RBEGIN: g_rp = c->rp; begin_pass(c->rp); break;
    case C_DRAW: {
        if (c->d.indirect) {
            u32 *a = (u32 *)(c->d.indirect->p + c->d.indoff);
            cmd_t d = *c;
            if (c->d.indexed) {
                d.d.icnt = a[0]; d.d.icount = a[1]; d.d.ioff = c->d.ioff + (u64)a[2] * (c->d.itype ? 4 : 2);
                d.d.bvert = (i32)a[3]; d.d.binst = a[4];
            } else { d.d.vcount = a[0]; d.d.icount = a[1]; d.d.vstart = a[2]; d.d.binst = a[3]; }
            d.d.indirect = 0;
            draw(&d);
        } else draw(c);
        break;
    }
    case C_REND: if (g_rp) end_pass(g_rp); g_rp = 0; break;
    case C_DISPATCH: dispatch(c); break;
    case C_COPY_BB:
        if (c->bb.sb && c->bb.db && c->bb.so + c->bb.n <= c->bb.sb->len && c->bb.dof + c->bb.n <= c->bb.db->len)
            memmove(c->bb.db->p + c->bb.dof, c->bb.sb->p + c->bb.so, c->bb.n);
        break;
    case C_COPY_BT: blit_bt(c, 1); break;
    case C_COPY_TB: blit_bt(c, 0); break;
    case C_COPY_TT: blit_tt(c); break;
    case C_FILL: if (c->fill.b && c->fill.off + c->fill.n <= c->fill.b->len) memset(c->fill.b->p + c->fill.off, c->fill.v, c->fill.n); break;
    case C_MIPS: if (c->mips.t) mx_gen_mips(c->mips.t); break;
    case C_SIGNAL: mx_signal_event(c->sig.ev, c->sig.v); break;
    case C_SCALE: scale_tex(c); break;
    }
}

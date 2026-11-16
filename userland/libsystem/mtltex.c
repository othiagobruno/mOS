/*
 * mOS libSystem — formatos de pixel (leitura/escrita de texels, sRGB, half, 10:10:10:2, 11:11:10, 9:9:9:5,
 * depth/stencil) e amostragem (nearest/linear, mipmaps, endereçamento, cubemaps). Usado pelo Metal por
 * software (metalexec.c) e pelo OpenGL por software (gl.c, glraster.c).
 */
#include "mtltex.h"
#include "libc.h"

double pow(double, double), floor(double), sqrt(double), round(double), fabs(double), ldexp(double, int), ceil(double);
u16 msl_f2h(float f);
float msl_h2f(u16 h);

/* ================================================================ formatos */
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
const pf_t *mx_pf(int fmt) {
    static const pf_t *cache[600];
    static int built;
    if (!built) { for (int i = 0; pfs[i].fmt; i++) cache[pfs[i].fmt] = &pfs[i]; built = 1; }
    return fmt > 0 && fmt < 600 ? cache[fmt] : 0;
}
int mx_bpp(int fmt) { const pf_t *p = mx_pf(fmt); return p ? p->bpp : 0; }
int mx_isdepth(int fmt) { return fmt == 250 || fmt == 252 || fmt == 255 || fmt == 260; }
int mx_hasstencil(int fmt) { return fmt == 253 || fmt == 255 || fmt == 260 || fmt == 261 || fmt == 262; }
int msl_fmt_isint(int fmt) {
    const pf_t *p = mx_pf(fmt);
    if (!p) return 0;
    int k = p->kind;
    return k == PK_U8 || k == PK_S8 || k == PK_U16 || k == PK_S16 || k == PK_U32 || k == PK_S32 || k == PK_RGB10A2U ||
           k == PK_ST8 || k == PK_X32S8 || k == PK_X24S8;
}

/* sRGB */
static float srgb_dec[256];
static u8 srgb_enc[4097];
void mx_srgb_init(void) {
    static int done;
    if (done) return;
    for (int i = 0; i < 256; i++) { double s = i / 255.0; srgb_dec[i] = (float)(s <= 0.04045 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4)); }
    for (int i = 0; i <= 4096; i++) {
        double l = i / 4096.0, s = l <= 0.0031308 ? l * 12.92 : 1.055 * pow(l, 1 / 2.4) - 0.055;
        srgb_enc[i] = (u8)(s * 255 + 0.5);
    }
    done = 1;
}
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
    const pf_t *p = mx_pf(t->fmt);
    int bpp = p ? p->bpp : 4;
    return t->data + t->loff[lvl] + (u64)slice * t->lslice[lvl] + ((u64)z * (u64)(t->h >> lvl ? t->h >> lvl : 1) + (u64)y) * t->lbpr[lvl] + (u64)x * bpp;
}
void mx_decode(const pf_t *p, const u8 *q, double o[4]) {
    o[0] = o[1] = o[2] = 0;
    o[3] = 1;
    int n = p->nc;
    switch (p->kind) {
    case PK_UN8:
        if (p->fl & PF_ALPHA) { o[3] = q[0] / 255.0; break; }
        for (int i = 0; i < n; i++) o[i] = (p->fl & PF_SRGB) && i < 3 ? srgb_dec[q[i]] : q[i] / 255.0;
        break;
    case PK_SN8: for (int i = 0; i < n; i++) o[i] = mx_clm11((i8)q[i] / 127.0); break;
    case PK_U8: for (int i = 0; i < n; i++) o[i] = q[i]; break;
    case PK_S8: for (int i = 0; i < n; i++) o[i] = (i8)q[i]; break;
    case PK_UN16: for (int i = 0; i < n; i++) o[i] = ((const u16 *)q)[i] / 65535.0; break;
    case PK_SN16: for (int i = 0; i < n; i++) o[i] = mx_clm11(((const i16 *)q)[i] / 32767.0); break;
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
void mx_encode(const pf_t *p, u8 *q, const double in[4]) {
    double v[4] = {in[0], in[1], in[2], in[3]};
    if (p->fl & PF_BGRA) { double t = v[0]; v[0] = v[2]; v[2] = t; }
    int n = p->nc;
    switch (p->kind) {
    case PK_UN8:
        if (p->fl & PF_ALPHA) { q[0] = (u8)(mx_cl01(v[3]) * 255 + 0.5); break; }
        for (int i = 0; i < n; i++) q[i] = (p->fl & PF_SRGB) && i < 3 ? srgb_enc[(int)(mx_cl01(v[i]) * 4096 + 0.5)] : (u8)(mx_cl01(v[i]) * 255 + 0.5);
        break;
    case PK_SN8: for (int i = 0; i < n; i++) q[i] = (u8)(i8)round(mx_clm11(v[i]) * 127); break;
    case PK_U8: for (int i = 0; i < n; i++) q[i] = (u8)(v[i] < 0 ? 0 : v[i] > 255 ? 255 : v[i]); break;
    case PK_S8: for (int i = 0; i < n; i++) q[i] = (u8)(i8)(v[i] < -128 ? -128 : v[i] > 127 ? 127 : v[i]); break;
    case PK_UN16: for (int i = 0; i < n; i++) ((u16 *)q)[i] = (u16)(mx_cl01(v[i]) * 65535 + 0.5); break;
    case PK_SN16: for (int i = 0; i < n; i++) ((i16 *)q)[i] = (i16)round(mx_clm11(v[i]) * 32767); break;
    case PK_U16: for (int i = 0; i < n; i++) ((u16 *)q)[i] = (u16)(v[i] < 0 ? 0 : v[i] > 65535 ? 65535 : v[i]); break;
    case PK_S16: for (int i = 0; i < n; i++) ((i16 *)q)[i] = (i16)(v[i] < -32768 ? -32768 : v[i] > 32767 ? 32767 : v[i]); break;
    case PK_HALF: for (int i = 0; i < n; i++) ((u16 *)q)[i] = msl_f2h((float)v[i]); break;
    case PK_U32: for (int i = 0; i < n; i++) ((u32 *)q)[i] = (u32)(v[i] < 0 ? 0 : v[i] > 4294967295.0 ? 4294967295.0 : v[i]); break;
    case PK_S32: for (int i = 0; i < n; i++) ((i32 *)q)[i] = (i32)(v[i] < -2147483648.0 ? -2147483648.0 : v[i] > 2147483647.0 ? 2147483647.0 : v[i]); break;
    case PK_F32: for (int i = 0; i < n; i++) ((float *)q)[i] = (float)v[i]; break;
    case PK_RGB10A2:
        *(u32 *)q = (u32)(mx_cl01(v[0]) * 1023 + 0.5) | (u32)(mx_cl01(v[1]) * 1023 + 0.5) << 10 | (u32)(mx_cl01(v[2]) * 1023 + 0.5) << 20 | (u32)(mx_cl01(v[3]) * 3 + 0.5) << 30;
        break;
    case PK_RGB10A2U: *(u32 *)q = ((u32)v[0] & 1023) | ((u32)v[1] & 1023) << 10 | ((u32)v[2] & 1023) << 20 | ((u32)v[3] & 3) << 30; break;
    case PK_RG11B10F: *(u32 *)q = uf_enc(v[0], 6) | uf_enc(v[1], 6) << 11 | uf_enc(v[2], 5) << 22; break;
    case PK_RGB9E5: {
        double mx = v[0] > v[1] ? v[0] : v[1];
        if (v[2] > mx) mx = v[2];
        int e = 0;
        if (mx > 0) { e = (int)floor(__builtin_log2(mx)) + 1; if (e < -15) e = -15; if (e > 16) e = 16; }
        double sc = ldexp(1.0, 9 - e);
        u32 r = (u32)(mx_cl01(v[0] * sc / 512.0) * 511 + 0.5), g = (u32)(mx_cl01(v[1] * sc / 512.0) * 511 + 0.5), b = (u32)(mx_cl01(v[2] * sc / 512.0) * 511 + 0.5);
        *(u32 *)q = r | g << 9 | b << 18 | (u32)(e + 15) << 27;
        break;
    }
    case PK_B5G6R5: *(u16 *)q = (u16)((u32)(mx_cl01(v[0]) * 31 + 0.5) << 11 | (u32)(mx_cl01(v[1]) * 63 + 0.5) << 5 | (u32)(mx_cl01(v[2]) * 31 + 0.5)); break;
    case PK_A1BGR5: *(u16 *)q = (u16)((u32)(mx_cl01(v[3]) + 0.5) << 15 | (u32)(mx_cl01(v[2]) * 31 + 0.5) << 10 | (u32)(mx_cl01(v[1]) * 31 + 0.5) << 5 | (u32)(mx_cl01(v[0]) * 31 + 0.5)); break;
    case PK_ABGR4: *(u16 *)q = (u16)((u32)(mx_cl01(v[3]) * 15 + 0.5) << 12 | (u32)(mx_cl01(v[2]) * 15 + 0.5) << 8 | (u32)(mx_cl01(v[1]) * 15 + 0.5) << 4 | (u32)(mx_cl01(v[0]) * 15 + 0.5)); break;
    case PK_BGR5A1: *(u16 *)q = (u16)((u32)(mx_cl01(v[2]) * 31 + 0.5) << 11 | (u32)(mx_cl01(v[1]) * 31 + 0.5) << 6 | (u32)(mx_cl01(v[0]) * 31 + 0.5) << 1 | (u32)(mx_cl01(v[3]) + 0.5)); break;
    case PK_D16: *(u16 *)q = (u16)(mx_cl01(v[0]) * 65535 + 0.5); break;
    case PK_D32F: case PK_D32FS8: *(float *)q = (float)v[0]; break;
    case PK_D24S8: *(u32 *)q = (*(u32 *)q & 0xff000000u) | (u32)(mx_cl01(v[0]) * 16777215 + 0.5); break;
    case PK_ST8: q[0] = (u8)v[0]; break;
    case PK_X32S8: q[4] = (u8)v[0]; break;
    case PK_X24S8: q[3] = (u8)v[0]; break;
    case PK_BGRA10XR: for (int i = 0; i < 4; i++) { double x = (v[i] + 0.75294) * 510; ((u16 *)q)[i] = (u16)((u32)(x < 0 ? 0 : x > 1023 ? 1023 : x) << 6); } break;
    case PK_BGR10XR: { u32 o = 0; for (int i = 0; i < 3; i++) { double x = (v[i] + 0.75294) * 510; o |= (u32)(x < 0 ? 0 : x > 1023 ? 1023 : x) << (i * 10); } *(u32 *)q = o; break; }
    }
}
void msl_tex_load(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, double out[4]) {
    const pf_t *p = mx_pf(t->fmt);
    if (!p || !t->data) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
    mx_srgb_init();
    mx_decode(p, mx_texel(t, x, y, z, slice, lvl), out);
}
void msl_tex_store(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, const double in[4]) {
    const pf_t *p = mx_pf(t->fmt);
    if (!p || !t->data) return;
    mx_srgb_init();
    mx_encode(p, mx_texel(t, x, y, z, slice, lvl), in);
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


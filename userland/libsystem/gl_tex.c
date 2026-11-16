/*
 * mOS libSystem — OpenGL por software: texturas (1D, 2D, 3D, cube, rect, arrays, buffer, multisample com
 * uma amostra), transferência de pixels em todos os formatos e tipos do GL 4.1 + compat (luminância,
 * alfa, intensidade, BGRA, tipos empacotados, depth/stencil), S3TC (DXT1/3/5) e RGTC descomprimidos no
 * envio, parâmetros, objetos sampler, mipmaps e a montagem do msl_tex_t que o amostrador do Metal lê.
 * Linha 0 de cada imagem é a de baixo (t = 0), como o GL guarda.
 */
#include "gl_int.h"
#include "gl_proto.h"

void gl_buf_ref(gl_buf_t *b);
void gl_buf_unref(gl_buf_t *b);

/* ================================================================ formatos internos */
typedef struct { GLenum ifmt; int fmt; GLenum base; u32 swz; } ifmt_t;
#define SW(r, g, b, a) ((u32)(r) | (u32)(g) << 8 | (u32)(b) << 16 | (u32)(a) << 24)
enum { Z0 = 0, O1 = 1, R_ = 2, G_ = 3, B_ = 4, A_ = 5 };
static const ifmt_t ifmts[] = {
    {GL_RGBA, 70, GL_RGBA, 0}, {GL_RGBA8, 70, GL_RGBA, 0}, {GL_RGB, 70, GL_RGB, 0}, {GL_RGB8, 70, GL_RGB, 0},
    {GL_RGBA2, 70, GL_RGBA, 0}, {GL_RGBA4, 70, GL_RGBA, 0}, {GL_RGB5_A1, 70, GL_RGBA, 0}, {GL_RGBA12, 110, GL_RGBA, 0},
    {GL_RGBA16, 110, GL_RGBA, 0}, {GL_R3_G3_B2, 70, GL_RGB, 0}, {GL_RGB4, 70, GL_RGB, 0}, {GL_RGB5, 70, GL_RGB, 0},
    {GL_RGB565, 70, GL_RGB, 0}, {GL_RGB10, 90, GL_RGB, 0}, {GL_RGB12, 110, GL_RGB, 0}, {GL_RGB16, 110, GL_RGB, 0},
    {GL_RGB10_A2, 90, GL_RGBA, 0}, {GL_RGB10_A2UI, 91, GL_RGBA_INTEGER, 0},
    {GL_SRGB, 71, GL_RGB, 0}, {GL_SRGB8, 71, GL_RGB, 0}, {GL_SRGB_ALPHA, 71, GL_RGBA, 0}, {GL_SRGB8_ALPHA8, 71, GL_RGBA, 0},
    {GL_SLUMINANCE, 11, GL_LUMINANCE, SW(R_, R_, R_, O1)}, {GL_SLUMINANCE8, 11, GL_LUMINANCE, SW(R_, R_, R_, O1)},
    {GL_SLUMINANCE_ALPHA, 31, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {GL_SLUMINANCE8_ALPHA8, 31, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)},
    {GL_RED, 10, GL_RED, 0}, {GL_R8, 10, GL_RED, 0}, {GL_R16, 20, GL_RED, 0}, {GL_RG, 30, GL_RG, 0}, {GL_RG8, 30, GL_RG, 0},
    {GL_RG16, 60, GL_RG, 0}, {GL_R8_SNORM, 12, GL_RED, 0}, {GL_RG8_SNORM, 32, GL_RG, 0}, {GL_RGB8_SNORM, 72, GL_RGB, 0},
    {GL_RGBA8_SNORM, 72, GL_RGBA, 0}, {GL_R16_SNORM, 22, GL_RED, 0}, {GL_RG16_SNORM, 62, GL_RG, 0},
    {GL_RGB16_SNORM, 112, GL_RGB, 0}, {GL_RGBA16_SNORM, 112, GL_RGBA, 0},
    {GL_R16F, 25, GL_RED, 0}, {GL_RG16F, 65, GL_RG, 0}, {GL_RGB16F, 115, GL_RGB, 0}, {GL_RGBA16F, 115, GL_RGBA, 0},
    {GL_R32F, 55, GL_RED, 0}, {GL_RG32F, 105, GL_RG, 0}, {GL_RGB32F, 125, GL_RGB, 0}, {GL_RGBA32F, 125, GL_RGBA, 0},
    {GL_R11F_G11F_B10F, 92, GL_RGB, 0}, {GL_RGB9_E5, 93, GL_RGB, 0},
    {GL_R8I, 14, GL_RED_INTEGER, 0}, {GL_R8UI, 13, GL_RED_INTEGER, 0}, {GL_R16I, 24, GL_RED_INTEGER, 0},
    {GL_R16UI, 23, GL_RED_INTEGER, 0}, {GL_R32I, 54, GL_RED_INTEGER, 0}, {GL_R32UI, 53, GL_RED_INTEGER, 0},
    {GL_RG8I, 34, GL_RG_INTEGER, 0}, {GL_RG8UI, 33, GL_RG_INTEGER, 0}, {GL_RG16I, 64, GL_RG_INTEGER, 0},
    {GL_RG16UI, 63, GL_RG_INTEGER, 0}, {GL_RG32I, 104, GL_RG_INTEGER, 0}, {GL_RG32UI, 103, GL_RG_INTEGER, 0},
    {GL_RGB8I, 74, GL_RGB_INTEGER, 0}, {GL_RGB8UI, 73, GL_RGB_INTEGER, 0}, {GL_RGB16I, 114, GL_RGB_INTEGER, 0},
    {GL_RGB16UI, 113, GL_RGB_INTEGER, 0}, {GL_RGB32I, 124, GL_RGB_INTEGER, 0}, {GL_RGB32UI, 123, GL_RGB_INTEGER, 0},
    {GL_RGBA8I, 74, GL_RGBA_INTEGER, 0}, {GL_RGBA8UI, 73, GL_RGBA_INTEGER, 0}, {GL_RGBA16I, 114, GL_RGBA_INTEGER, 0},
    {GL_RGBA16UI, 113, GL_RGBA_INTEGER, 0}, {GL_RGBA32I, 124, GL_RGBA_INTEGER, 0}, {GL_RGBA32UI, 123, GL_RGBA_INTEGER, 0},
    {GL_DEPTH_COMPONENT, 252, GL_DEPTH_COMPONENT, 0}, {GL_DEPTH_COMPONENT16, 250, GL_DEPTH_COMPONENT, 0},
    {GL_DEPTH_COMPONENT24, 252, GL_DEPTH_COMPONENT, 0}, {GL_DEPTH_COMPONENT32, 252, GL_DEPTH_COMPONENT, 0},
    {GL_DEPTH_COMPONENT32F, 252, GL_DEPTH_COMPONENT, 0}, {GL_DEPTH_STENCIL, 260, GL_DEPTH_STENCIL, 0},
    {GL_DEPTH24_STENCIL8, 260, GL_DEPTH_STENCIL, 0}, {GL_DEPTH32F_STENCIL8, 260, GL_DEPTH_STENCIL, 0},
    {GL_STENCIL_INDEX8, 253, GL_STENCIL_INDEX, 0}, {GL_STENCIL_INDEX, 253, GL_STENCIL_INDEX, 0},
    /* compat */
    {GL_ALPHA, 10, GL_ALPHA, SW(Z0, Z0, Z0, R_)}, {GL_ALPHA4, 10, GL_ALPHA, SW(Z0, Z0, Z0, R_)}, {GL_ALPHA8, 10, GL_ALPHA, SW(Z0, Z0, Z0, R_)},
    {GL_ALPHA12, 20, GL_ALPHA, SW(Z0, Z0, Z0, R_)}, {GL_ALPHA16, 20, GL_ALPHA, SW(Z0, Z0, Z0, R_)},
    {GL_LUMINANCE, 10, GL_LUMINANCE, SW(R_, R_, R_, O1)}, {GL_LUMINANCE4, 10, GL_LUMINANCE, SW(R_, R_, R_, O1)},
    {GL_LUMINANCE8, 10, GL_LUMINANCE, SW(R_, R_, R_, O1)}, {GL_LUMINANCE12, 20, GL_LUMINANCE, SW(R_, R_, R_, O1)},
    {GL_LUMINANCE16, 20, GL_LUMINANCE, SW(R_, R_, R_, O1)},
    {GL_LUMINANCE_ALPHA, 30, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {GL_LUMINANCE4_ALPHA4, 30, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)},
    {GL_LUMINANCE6_ALPHA2, 30, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {GL_LUMINANCE8_ALPHA8, 30, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)},
    {GL_LUMINANCE12_ALPHA4, 60, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {GL_LUMINANCE12_ALPHA12, 60, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)},
    {GL_LUMINANCE16_ALPHA16, 60, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)},
    {GL_INTENSITY, 10, GL_INTENSITY, SW(R_, R_, R_, R_)}, {GL_INTENSITY4, 10, GL_INTENSITY, SW(R_, R_, R_, R_)},
    {GL_INTENSITY8, 10, GL_INTENSITY, SW(R_, R_, R_, R_)}, {GL_INTENSITY12, 20, GL_INTENSITY, SW(R_, R_, R_, R_)},
    {GL_INTENSITY16, 20, GL_INTENSITY, SW(R_, R_, R_, R_)},
    {GL_RGBA16F_ARB, 115, GL_RGBA, 0}, {GL_RGB16F_ARB, 115, GL_RGB, 0}, {GL_RGBA32F_ARB, 125, GL_RGBA, 0}, {GL_RGB32F_ARB, 125, GL_RGB, 0},
    {GL_ALPHA16F_ARB, 25, GL_ALPHA, SW(Z0, Z0, Z0, R_)}, {GL_ALPHA32F_ARB, 55, GL_ALPHA, SW(Z0, Z0, Z0, R_)},
    {GL_LUMINANCE16F_ARB, 25, GL_LUMINANCE, SW(R_, R_, R_, O1)}, {GL_LUMINANCE32F_ARB, 55, GL_LUMINANCE, SW(R_, R_, R_, O1)},
    {GL_LUMINANCE_ALPHA16F_ARB, 65, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {GL_LUMINANCE_ALPHA32F_ARB, 105, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)},
    {GL_INTENSITY16F_ARB, 25, GL_INTENSITY, SW(R_, R_, R_, R_)}, {GL_INTENSITY32F_ARB, 55, GL_INTENSITY, SW(R_, R_, R_, R_)},
    {GL_RGB_422_APPLE, 70, GL_RGB, 0}, {GL_YCBCR_422_APPLE, 70, GL_RGB, 0},
    {1, 10, GL_LUMINANCE, SW(R_, R_, R_, O1)}, {2, 30, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {3, 70, GL_RGB, 0}, {4, 70, GL_RGBA, 0},
    /* comprimidos (descomprimidos no envio) */
    {GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 70, GL_RGB, 0}, {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 70, GL_RGBA, 0},
    {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 70, GL_RGBA, 0}, {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 70, GL_RGBA, 0},
    {GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 71, GL_RGB, 0}, {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 71, GL_RGBA, 0},
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 71, GL_RGBA, 0}, {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 71, GL_RGBA, 0},
    {GL_COMPRESSED_RED_RGTC1, 10, GL_RED, 0}, {GL_COMPRESSED_SIGNED_RED_RGTC1, 12, GL_RED, 0},
    {GL_COMPRESSED_RG_RGTC2, 30, GL_RG, 0}, {GL_COMPRESSED_SIGNED_RG_RGTC2, 32, GL_RG, 0},
    {GL_COMPRESSED_RGB, 70, GL_RGB, 0}, {GL_COMPRESSED_RGBA, 70, GL_RGBA, 0}, {GL_COMPRESSED_RED, 10, GL_RED, 0},
    {GL_COMPRESSED_RG, 30, GL_RG, 0}, {GL_COMPRESSED_SRGB, 71, GL_RGB, 0}, {GL_COMPRESSED_SRGB_ALPHA, 71, GL_RGBA, 0},
    {GL_COMPRESSED_ALPHA, 10, GL_ALPHA, SW(Z0, Z0, Z0, R_)}, {GL_COMPRESSED_LUMINANCE, 10, GL_LUMINANCE, SW(R_, R_, R_, O1)},
    {GL_COMPRESSED_LUMINANCE_ALPHA, 30, GL_LUMINANCE_ALPHA, SW(R_, R_, R_, G_)}, {GL_COMPRESSED_INTENSITY, 10, GL_INTENSITY, SW(R_, R_, R_, R_)},
    {0, 0, 0, 0}};
static const ifmt_t *ifmt_find(GLenum f) { for (int i = 0; ifmts[i].ifmt; i++) if (ifmts[i].ifmt == f) return &ifmts[i]; return 0; }
int gl_ifmt_to_mtl(GLenum ifmt, int *ok) { const ifmt_t *f = ifmt_find(ifmt); if (ok) *ok = f != 0; return f ? f->fmt : 70; }
GLenum gl_ifmt_base(GLenum ifmt) { const ifmt_t *f = ifmt_find(ifmt); return f ? f->base : GL_RGBA; }
static int is_compressed(GLenum f) {
    return (f >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && f <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) ||
           (f >= GL_COMPRESSED_SRGB_S3TC_DXT1_EXT && f <= GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT) ||
           (f >= GL_COMPRESSED_RED_RGTC1 && f <= GL_COMPRESSED_SIGNED_RG_RGTC2);
}
static int block_bytes(GLenum f) {
    return f == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || f == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || f == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
                   f == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT || f == GL_COMPRESSED_RED_RGTC1 || f == GL_COMPRESSED_SIGNED_RED_RGTC1
               ? 8 : 16;
}

/* ================================================================ transferência de pixels */
static int fmt_comps(GLenum f) {
    switch (f) {
    case GL_RED: case GL_GREEN: case GL_BLUE: case GL_ALPHA: case GL_LUMINANCE: case GL_INTENSITY: case GL_RED_INTEGER:
    case GL_GREEN_INTEGER: case GL_BLUE_INTEGER: case GL_ALPHA_INTEGER: case GL_DEPTH_COMPONENT: case GL_STENCIL_INDEX:
    case GL_COLOR_INDEX: case GL_LUMINANCE_INTEGER_EXT:
        return 1;
    case GL_RG: case GL_RG_INTEGER: case GL_LUMINANCE_ALPHA: case GL_DEPTH_STENCIL: case GL_LUMINANCE_ALPHA_INTEGER_EXT: return 2;
    case GL_RGB: case GL_BGR: case GL_RGB_INTEGER: case GL_BGR_INTEGER: case GL_YCBCR_422_APPLE: case GL_RGB_422_APPLE: return 3;
    case GL_RGBA: case GL_BGRA: case GL_RGBA_INTEGER: case GL_BGRA_INTEGER: case GL_ABGR_EXT: return 4;
    }
    return 0;
}
static int type_size(GLenum t) {
    switch (t) {
    case GL_UNSIGNED_BYTE: case GL_BYTE: case GL_UNSIGNED_BYTE_3_3_2: case GL_UNSIGNED_BYTE_2_3_3_REV: return 1;
    case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: case GL_UNSIGNED_SHORT_5_6_5: case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4: case GL_UNSIGNED_SHORT_4_4_4_4_REV: case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV: case GL_UNSIGNED_SHORT_8_8_APPLE: case GL_UNSIGNED_SHORT_8_8_REV_APPLE: return 2;
    case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT: case GL_UNSIGNED_INT_8_8_8_8: case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2: case GL_UNSIGNED_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_24_8:
    case GL_UNSIGNED_INT_10F_11F_11F_REV: case GL_UNSIGNED_INT_5_9_9_9_REV: return 4;
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV: return 8;
    case GL_BITMAP: return 1;
    }
    return 0;
}
static int packed_type(GLenum t) { return t != GL_UNSIGNED_BYTE && t != GL_BYTE && t != GL_UNSIGNED_SHORT && t != GL_SHORT && t != GL_UNSIGNED_INT && t != GL_INT && t != GL_FLOAT && t != GL_HALF_FLOAT; }
/* bytes por pixel do lado do cliente */
static int px_bytes(GLenum f, GLenum t) {
    if (t == GL_UNSIGNED_SHORT_8_8_APPLE || t == GL_UNSIGNED_SHORT_8_8_REV_APPLE) return 2;
    return packed_type(t) ? type_size(t) : type_size(t) * fmt_comps(f);
}
/* lê os componentes do pixel em ordem de memória */
static void read_comps(const u8 *p, GLenum t, int n, int swap, double *c, u32 *raw) {
    for (int i = 0; i < n; i++) {
        switch (t) {
        case GL_UNSIGNED_BYTE: c[i] = p[i] / 255.0; raw[i] = p[i]; break;
        case GL_BYTE: c[i] = mx_clm11((i8)p[i] / 127.0); raw[i] = (u32)(i32)(i8)p[i]; break;
        case GL_UNSIGNED_SHORT: { u16 v; memcpy(&v, p + 2 * i, 2); if (swap) v = (u16)(v >> 8 | v << 8); c[i] = v / 65535.0; raw[i] = v; break; }
        case GL_SHORT: { u16 v; memcpy(&v, p + 2 * i, 2); if (swap) v = (u16)(v >> 8 | v << 8); c[i] = mx_clm11((i16)v / 32767.0); raw[i] = (u32)(i32)(i16)v; break; }
        case GL_UNSIGNED_INT: { u32 v; memcpy(&v, p + 4 * i, 4); if (swap) v = __builtin_bswap32(v); c[i] = v / 4294967295.0; raw[i] = v; break; }
        case GL_INT: { u32 v; memcpy(&v, p + 4 * i, 4); if (swap) v = __builtin_bswap32(v); c[i] = mx_clm11((i32)v / 2147483647.0); raw[i] = v; break; }
        case GL_FLOAT: { u32 v; memcpy(&v, p + 4 * i, 4); if (swap) v = __builtin_bswap32(v); float f; memcpy(&f, &v, 4); c[i] = f; raw[i] = (u32)(i32)f; break; }
        case GL_HALF_FLOAT: { u16 v; memcpy(&v, p + 2 * i, 2); if (swap) v = (u16)(v >> 8 | v << 8); c[i] = msl_h2f(v); raw[i] = (u32)(i32)c[i]; break; }
        }
    }
}
typedef struct { int n; u8 sh[4], bits[4]; } pk_t;
static int packed_layout(GLenum t, pk_t *k) {
    /* componentes na ordem em que o formato os nomeia; sh = deslocamento a partir do bit 0 */
    static const struct { GLenum t; int n; u8 sh[4], bits[4]; } L[] = {
        {GL_UNSIGNED_BYTE_3_3_2, 3, {5, 2, 0}, {3, 3, 2}}, {GL_UNSIGNED_BYTE_2_3_3_REV, 3, {0, 3, 6}, {3, 3, 2}},
        {GL_UNSIGNED_SHORT_5_6_5, 3, {11, 5, 0}, {5, 6, 5}}, {GL_UNSIGNED_SHORT_5_6_5_REV, 3, {0, 5, 11}, {5, 6, 5}},
        {GL_UNSIGNED_SHORT_4_4_4_4, 4, {12, 8, 4, 0}, {4, 4, 4, 4}}, {GL_UNSIGNED_SHORT_4_4_4_4_REV, 4, {0, 4, 8, 12}, {4, 4, 4, 4}},
        {GL_UNSIGNED_SHORT_5_5_5_1, 4, {11, 6, 1, 0}, {5, 5, 5, 1}}, {GL_UNSIGNED_SHORT_1_5_5_5_REV, 4, {0, 5, 10, 15}, {5, 5, 5, 1}},
        {GL_UNSIGNED_INT_8_8_8_8, 4, {24, 16, 8, 0}, {8, 8, 8, 8}}, {GL_UNSIGNED_INT_8_8_8_8_REV, 4, {0, 8, 16, 24}, {8, 8, 8, 8}},
        {GL_UNSIGNED_INT_10_10_10_2, 4, {22, 12, 2, 0}, {10, 10, 10, 2}}, {GL_UNSIGNED_INT_2_10_10_10_REV, 4, {0, 10, 20, 30}, {10, 10, 10, 2}},
        {0, 0, {0}, {0}}};
    for (int i = 0; L[i].t; i++)
        if (L[i].t == t) { k->n = L[i].n; memcpy(k->sh, L[i].sh, 4); memcpy(k->bits, L[i].bits, 4); return 1; }
    return 0;
}
/* um pixel do cliente -> rgba (double) + inteiros crus + profundidade/estêncil */
static void unpack_px(const u8 *p, GLenum f, GLenum t, int swap, double o[4], u32 ri[4], double *depth, u32 *stencil) {
    double c[4] = {0, 0, 0, 1};
    u32 raw[4] = {0, 0, 0, 1};
    int n = fmt_comps(f);
    if (t == GL_UNSIGNED_INT_24_8) {
        u32 v; memcpy(&v, p, 4); if (swap) v = __builtin_bswap32(v);
        *depth = (v >> 8) / 16777215.0; *stencil = v & 0xff;
        return;
    }
    if (t == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
        float d; u32 s; memcpy(&d, p, 4); memcpy(&s, p + 4, 4);
        *depth = d; *stencil = s & 0xff;
        return;
    }
    if (t == GL_UNSIGNED_INT_10F_11F_11F_REV) { mx_decode(mx_pf(92), p, c); n = 3; for (int i = 0; i < 3; i++) raw[i] = (u32)c[i]; goto place; }
    if (t == GL_UNSIGNED_INT_5_9_9_9_REV) { mx_decode(mx_pf(93), p, c); n = 3; for (int i = 0; i < 3; i++) raw[i] = (u32)c[i]; goto place; }
    if (t == GL_UNSIGNED_SHORT_8_8_APPLE || t == GL_UNSIGNED_SHORT_8_8_REV_APPLE) {
        /* YCbCr 4:2:2 (APPLE_ycbcr_422): par de pixels Cb Y0 Cr Y1; aqui um pixel com o Y dele */
        u16 v; memcpy(&v, p, 2);
        double y = (t == GL_UNSIGNED_SHORT_8_8_APPLE ? (v >> 8) : (v & 0xff)) / 255.0;
        o[0] = o[1] = o[2] = y; o[3] = 1;
        return;
    }
    pk_t k;
    if (packed_layout(t, &k)) {
        u32 v = 0;
        int sz = type_size(t);
        if (sz == 1) v = p[0];
        else if (sz == 2) { u16 h; memcpy(&h, p, 2); if (swap) h = (u16)(h >> 8 | h << 8); v = h; }
        else { memcpy(&v, p, 4); if (swap) v = __builtin_bswap32(v); }
        n = k.n;
        for (int i = 0; i < k.n; i++) { u32 m = (1u << k.bits[i]) - 1; raw[i] = (v >> k.sh[i]) & m; c[i] = raw[i] / (double)m; }
    } else read_comps(p, t, n, swap, c, raw);
place:
    o[0] = o[1] = o[2] = 0; o[3] = 1;
    ri[0] = ri[1] = ri[2] = 0; ri[3] = 1;
#define PUT(i, j) do { o[i] = c[j]; ri[i] = raw[j]; } while (0)
    switch (f) {
    case GL_RED: case GL_RED_INTEGER: case GL_LUMINANCE: case GL_INTENSITY: case GL_LUMINANCE_INTEGER_EXT: PUT(0, 0); break;
    case GL_GREEN: case GL_GREEN_INTEGER: PUT(1, 0); break;
    case GL_BLUE: case GL_BLUE_INTEGER: PUT(2, 0); break;
    case GL_ALPHA: case GL_ALPHA_INTEGER: PUT(3, 0); break;
    case GL_RG: case GL_RG_INTEGER: PUT(0, 0); PUT(1, 1); break;
    case GL_LUMINANCE_ALPHA: case GL_LUMINANCE_ALPHA_INTEGER_EXT: PUT(0, 0); PUT(3, 1); break;
    case GL_RGB: case GL_RGB_INTEGER: case GL_YCBCR_422_APPLE: case GL_RGB_422_APPLE: PUT(0, 0); PUT(1, 1); PUT(2, 2); break;
    case GL_BGR: case GL_BGR_INTEGER: PUT(2, 0); PUT(1, 1); PUT(0, 2); break;
    case GL_RGBA: case GL_RGBA_INTEGER: PUT(0, 0); PUT(1, 1); PUT(2, 2); PUT(3, 3); break;
    case GL_BGRA: case GL_BGRA_INTEGER: PUT(2, 0); PUT(1, 1); PUT(0, 2); PUT(3, 3); break;
    case GL_ABGR_EXT: PUT(3, 0); PUT(2, 1); PUT(1, 2); PUT(0, 3); break;
    case GL_DEPTH_COMPONENT: *depth = c[0]; break;
    case GL_STENCIL_INDEX: *stencil = raw[0]; break;
    case GL_DEPTH_STENCIL: *depth = c[0]; *stencil = raw[1]; break;
    }
#undef PUT
}
static int fmt_isint(GLenum f) { return f == GL_RED_INTEGER || f == GL_GREEN_INTEGER || f == GL_BLUE_INTEGER || f == GL_ALPHA_INTEGER || f == GL_RG_INTEGER || f == GL_RGB_INTEGER || f == GL_BGR_INTEGER || f == GL_RGBA_INTEGER || f == GL_BGRA_INTEGER || f == GL_LUMINANCE_INTEGER_EXT || f == GL_LUMINANCE_ALPHA_INTEGER_EXT; }
/* grava um texel no formato Metal de destino: o pixel para a camada do formato base
 * (luminância/alfa/intensidade vivem no R/RG e o swizzle reconstrói) */
static void store_texel(int dfmt, GLenum base, u8 *dst, const double c[4], const u32 ri[4], int isint, double depth, u32 stencil, int has_ds) {
    const pf_t *p = mx_pf(dfmt);
    if (!p) return;
    if (dfmt == 260) { float d = (float)mx_cl01(depth); memcpy(dst, &d, 4); dst[4] = (u8)stencil; memset(dst + 5, 0, 3); return; }
    if (dfmt == 252 || dfmt == 250) { double v[4] = {has_ds ? depth : c[0], 0, 0, 1}; mx_encode(p, dst, v); return; }
    if (dfmt == 253) { dst[0] = (u8)stencil; return; }
    double v[4] = {c[0], c[1], c[2], c[3]};
    if (isint) for (int i = 0; i < 4; i++) v[i] = msl_fmt_isint(dfmt) ? (double)(i32)ri[i] : v[i];
    /* inteiro sem sinal: u32 sem virar negativo */
    if (isint && (p->kind == PK_U8 || p->kind == PK_U16 || p->kind == PK_U32)) for (int i = 0; i < 4; i++) v[i] = (double)ri[i];
    switch (base) {
    case GL_ALPHA: v[0] = c[3]; break;
    case GL_LUMINANCE_ALPHA: v[1] = c[3]; break;
    case GL_INTENSITY: case GL_LUMINANCE: break;
    case GL_RGB: case GL_RGB_INTEGER: v[3] = isint ? 1 : 1; break;
    }
    mx_encode(p, dst, v);
}
static i64 row_stride(gl_ctx_t *C, int pack, int w, GLenum f, GLenum t) {
    int row = pack ? C->pk_row : C->up_row, al = pack ? C->pk_align : C->up_align;
    int len = row > 0 ? row : w;
    if (t == GL_BITMAP) { i64 b = (len + 7) / 8; return (b + al - 1) / al * al; }
    i64 bpp = px_bytes(f, t), b = bpp * len;
    int csz = packed_type(t) ? bpp : type_size(t);
    if (csz >= al) return b;
    return (b + al - 1) / al * al;
}
/* envia w*h*d pixels do cliente (src) para a imagem dst no formato dfmt com o formato base dado */
static void unpack_image(gl_ctx_t *C, int w, int h, int d, GLenum f, GLenum t, const u8 *src, int dfmt, GLenum base, u8 *dst) {
    int bpp = mx_bpp(dfmt);
    i64 rs = row_stride(C, 0, w, f, t);
    i64 rows_img = C->up_imgh > 0 ? C->up_imgh : h;
    i64 is = rs * rows_img;
    i64 pb = px_bytes(f, t);
    src += C->up_skimg * is + C->up_skrow * rs + C->up_skpix * pb;
    int isint = fmt_isint(f);
    int has_ds = f == GL_DEPTH_COMPONENT || f == GL_DEPTH_STENCIL || f == GL_STENCIL_INDEX;
    for (int z = 0; z < d; z++)
        for (int y = 0; y < h; y++) {
            const u8 *sp = src + z * is + y * rs;
            u8 *dp = dst + (((i64)z * h + y) * w) * bpp;
            for (int x = 0; x < w; x++) {
                double c[4]; u32 ri[4]; double dep = 0; u32 st = 0;
                if (t == GL_BITMAP) {
                    int bit = C->up_lsb ? (sp[x >> 3] >> (x & 7)) & 1 : (sp[x >> 3] >> (7 - (x & 7))) & 1;
                    c[0] = c[1] = c[2] = c[3] = bit; ri[0] = ri[1] = ri[2] = ri[3] = (u32)bit; st = (u32)bit;
                } else unpack_px(sp + x * pb, f, t, C->up_swap, c, ri, &dep, &st);
                store_texel(dfmt, base, dp + x * bpp, c, ri, isint, dep, st, has_ds);
            }
        }
}
void gl_unpack_pixels(gl_ctx_t *C, int w, int h, int d, GLenum format, GLenum type, const void *src, int dfmt, u8 *dst, int base) {
    unpack_image(C, w, h, d, format, type, src, dfmt, (GLenum)base, dst);
}
/* texel -> pixel do cliente */
static void write_comps(u8 *p, GLenum t, int n, const double *c, const u32 *ri, int isint, int swap) {
    for (int i = 0; i < n; i++) {
        double v = c[i];
        switch (t) {
        case GL_UNSIGNED_BYTE: p[i] = isint ? (u8)ri[i] : (u8)(mx_cl01(v) * 255 + 0.5); break;
        case GL_BYTE: p[i] = isint ? (u8)ri[i] : (u8)(i8)(mx_clm11(v) * 127 + (v < 0 ? -0.5 : 0.5)); break;
        case GL_UNSIGNED_SHORT: { u16 x = isint ? (u16)ri[i] : (u16)(mx_cl01(v) * 65535 + 0.5); if (swap) x = (u16)(x >> 8 | x << 8); memcpy(p + 2 * i, &x, 2); break; }
        case GL_SHORT: { u16 x = isint ? (u16)ri[i] : (u16)(i16)(mx_clm11(v) * 32767 + (v < 0 ? -0.5 : 0.5)); if (swap) x = (u16)(x >> 8 | x << 8); memcpy(p + 2 * i, &x, 2); break; }
        case GL_UNSIGNED_INT: { u32 x = isint ? ri[i] : (u32)(mx_cl01(v) * 4294967295.0 + 0.5); if (swap) x = __builtin_bswap32(x); memcpy(p + 4 * i, &x, 4); break; }
        case GL_INT: { u32 x = isint ? ri[i] : (u32)(i32)(mx_clm11(v) * 2147483647.0); if (swap) x = __builtin_bswap32(x); memcpy(p + 4 * i, &x, 4); break; }
        case GL_FLOAT: { float f = isint ? (float)(i32)ri[i] : (float)v; u32 x; memcpy(&x, &f, 4); if (swap) x = __builtin_bswap32(x); memcpy(p + 4 * i, &x, 4); break; }
        case GL_HALF_FLOAT: { u16 x = msl_f2h((float)v); if (swap) x = (u16)(x >> 8 | x << 8); memcpy(p + 2 * i, &x, 2); break; }
        }
    }
}
static void pack_px(u8 *p, GLenum f, GLenum t, const double rgba[4], const u32 ri[4], int isint, double depth, u32 stencil, int swap) {
    if (t == GL_UNSIGNED_INT_24_8) { u32 v = (u32)(mx_cl01(depth) * 16777215 + 0.5) << 8 | (stencil & 0xff); if (f == GL_STENCIL_INDEX) v = stencil; if (swap) v = __builtin_bswap32(v); memcpy(p, &v, 4); return; }
    if (t == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) { float d = (float)depth; u32 s = stencil & 0xff; memcpy(p, &d, 4); memcpy(p + 4, &s, 4); return; }
    double c[4];
    u32 r[4];
    int n = fmt_comps(f);
    switch (f) {
#define G(i, j) do { c[i] = rgba[j]; r[i] = ri[j]; } while (0)
    case GL_RED: case GL_RED_INTEGER: G(0, 0); break;
    case GL_GREEN: case GL_GREEN_INTEGER: G(0, 1); break;
    case GL_BLUE: case GL_BLUE_INTEGER: G(0, 2); break;
    case GL_ALPHA: case GL_ALPHA_INTEGER: G(0, 3); break;
    case GL_LUMINANCE: case GL_LUMINANCE_INTEGER_EXT: c[0] = rgba[0] + rgba[1] + rgba[2]; if (c[0] > 1) c[0] = 1; r[0] = ri[0]; break;
    case GL_LUMINANCE_ALPHA: case GL_LUMINANCE_ALPHA_INTEGER_EXT: c[0] = rgba[0] + rgba[1] + rgba[2]; if (c[0] > 1) c[0] = 1; r[0] = ri[0]; G(1, 3); break;
    case GL_RG: case GL_RG_INTEGER: G(0, 0); G(1, 1); break;
    case GL_RGB: case GL_RGB_INTEGER: G(0, 0); G(1, 1); G(2, 2); break;
    case GL_BGR: case GL_BGR_INTEGER: G(0, 2); G(1, 1); G(2, 0); break;
    case GL_RGBA: case GL_RGBA_INTEGER: G(0, 0); G(1, 1); G(2, 2); G(3, 3); break;
    case GL_BGRA: case GL_BGRA_INTEGER: G(0, 2); G(1, 1); G(2, 0); G(3, 3); break;
    case GL_ABGR_EXT: G(0, 3); G(1, 2); G(2, 1); G(3, 0); break;
    case GL_DEPTH_COMPONENT: c[0] = depth; r[0] = (u32)depth; break;
    case GL_STENCIL_INDEX: c[0] = stencil; r[0] = stencil; isint = 1; break;
    case GL_DEPTH_STENCIL: c[0] = depth; r[1] = stencil; break;
    default: return;
#undef G
    }
    if (t == GL_UNSIGNED_INT_10F_11F_11F_REV) { double v[4] = {c[0], c[1], c[2], 1}; mx_encode(mx_pf(92), p, v); return; }
    if (t == GL_UNSIGNED_INT_5_9_9_9_REV) { double v[4] = {c[0], c[1], c[2], 1}; mx_encode(mx_pf(93), p, v); return; }
    pk_t k;
    if (packed_layout(t, &k)) {
        u32 v = 0;
        for (int i = 0; i < k.n; i++) { u32 m = (1u << k.bits[i]) - 1; u32 q = isint ? (r[i] & m) : (u32)(mx_cl01(c[i]) * m + 0.5); v |= q << k.sh[i]; }
        int sz = type_size(t);
        if (sz == 1) p[0] = (u8)v;
        else if (sz == 2) { u16 h = (u16)v; if (swap) h = (u16)(h >> 8 | h << 8); memcpy(p, &h, 2); }
        else { if (swap) v = __builtin_bswap32(v); memcpy(p, &v, 4); }
        return;
    }
    write_comps(p, t, n, c, r, isint, swap);
}
/* lê w*h de uma imagem (camada layer, nível lvl) para o cliente; ybottom: a linha 0 da textura é a de baixo */
void gl_pack_pixels(gl_ctx_t *C, const msl_tex_t *t, int lvl, int layer, int x0, int y0, int w, int h, GLenum f, GLenum ty, void *dst, int ybottom) {
    (void)ybottom;
    i64 rs = row_stride(C, 1, w, f, ty);
    i64 pb = px_bytes(f, ty);
    u8 *base = (u8 *)dst + C->pk_skrow * rs + C->pk_skpix * pb;
    int isint = fmt_isint(f) || msl_fmt_isint(t->fmt);
    int lw = t->w >> lvl, lh = t->h >> lvl;
    if (lw < 1) lw = 1;
    if (lh < 1) lh = 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int sx = x0 + x, sy = y0 + y;
            if (sx < 0 || sy < 0 || sx >= lw || sy >= lh) continue;
            double c[4] = {0, 0, 0, 1};
            u32 ri[4] = {0, 0, 0, 1};
            double dep = 0;
            u32 st = 0;
            const u8 *q = mx_texel(t, sx, sy, t->type == MSL_TEX_3D ? layer : 0, t->type == MSL_TEX_3D ? 0 : layer, lvl);
            if (t->fmt == 260) { float d; memcpy(&d, q, 4); dep = d; st = q[4]; }
            else if (t->fmt == 253) st = q[0];
            else {
                msl_tex_load(t, sx, sy, t->type == MSL_TEX_3D ? layer : 0, t->type == MSL_TEX_3D ? 0 : layer, lvl, c);
                msl_tex_swizzle(t, c);
                dep = c[0];
                for (int i = 0; i < 4; i++) ri[i] = (u32)(i64)c[i];
            }
            pack_px(base + y * rs + x * pb, f, ty, c, ri, isint, dep, st, C->pk_swap);
        }
}

/* ================================================================ descompressão (S3TC/RGTC) */
static void dxt_color(const u8 *b, u8 out[16][4], int dxt1) {
    u16 c0 = (u16)(b[0] | b[1] << 8), c1 = (u16)(b[2] | b[3] << 8);
    u8 pal[4][4];
    for (int i = 0; i < 2; i++) {
        u16 c = i ? c1 : c0;
        pal[i][0] = (u8)(((c >> 11) & 31) * 255 / 31);
        pal[i][1] = (u8)(((c >> 5) & 63) * 255 / 63);
        pal[i][2] = (u8)((c & 31) * 255 / 31);
        pal[i][3] = 255;
    }
    if (c0 > c1 || !dxt1) {
        for (int k = 0; k < 3; k++) { pal[2][k] = (u8)((2 * pal[0][k] + pal[1][k]) / 3); pal[3][k] = (u8)((pal[0][k] + 2 * pal[1][k]) / 3); }
        pal[2][3] = pal[3][3] = 255;
    } else {
        for (int k = 0; k < 3; k++) { pal[2][k] = (u8)((pal[0][k] + pal[1][k]) / 2); pal[3][k] = 0; }
        pal[2][3] = 255;
        pal[3][3] = 0;
    }
    u32 bits = (u32)b[4] | (u32)b[5] << 8 | (u32)b[6] << 16 | (u32)b[7] << 24;
    for (int i = 0; i < 16; i++) memcpy(out[i], pal[(bits >> (2 * i)) & 3], 4);
}
static void rgtc_chan(const u8 *b, int sgn, double out[16]) {
    double e0 = sgn ? ((i8)b[0] < -127 ? -127 : (i8)b[0]) / 127.0 : b[0] / 255.0;
    double e1 = sgn ? ((i8)b[1] < -127 ? -127 : (i8)b[1]) / 127.0 : b[1] / 255.0;
    double pal[8];
    pal[0] = e0;
    pal[1] = e1;
    int gt = sgn ? (i8)b[0] > (i8)b[1] : b[0] > b[1];
    if (gt) for (int i = 1; i < 7; i++) pal[i + 1] = ((7 - i) * e0 + i * e1) / 7;
    else {
        for (int i = 1; i < 5; i++) pal[i + 1] = ((5 - i) * e0 + i * e1) / 5;
        pal[6] = sgn ? -1 : 0;
        pal[7] = 1;
    }
    u64 bits = 0;
    for (int i = 0; i < 6; i++) bits |= (u64)b[2 + i] << (8 * i);
    for (int i = 0; i < 16; i++) out[i] = pal[(bits >> (3 * i)) & 7];
}
/* descomprime uma imagem de blocos 4x4 para dfmt (70/71 RGBA8, 10/12 R, 30/32 RG) */
static void decompress(GLenum f, int w, int h, int d, const u8 *src, u8 *dst, int dfmt) {
    int bw = (w + 3) / 4, bh = (h + 3) / 4, bb = block_bytes(f), bpp = mx_bpp(dfmt);
    const pf_t *p = mx_pf(dfmt);
    for (int z = 0; z < d; z++)
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) {
                const u8 *b = src + (((i64)z * bh + by) * bw + bx) * bb;
                double px[16][4];
                if (f == GL_COMPRESSED_RED_RGTC1 || f == GL_COMPRESSED_SIGNED_RED_RGTC1) {
                    double r[16];
                    rgtc_chan(b, f == GL_COMPRESSED_SIGNED_RED_RGTC1, r);
                    for (int i = 0; i < 16; i++) { px[i][0] = r[i]; px[i][1] = px[i][2] = 0; px[i][3] = 1; }
                } else if (f == GL_COMPRESSED_RG_RGTC2 || f == GL_COMPRESSED_SIGNED_RG_RGTC2) {
                    double r[16], g[16];
                    int s = f == GL_COMPRESSED_SIGNED_RG_RGTC2;
                    rgtc_chan(b, s, r);
                    rgtc_chan(b + 8, s, g);
                    for (int i = 0; i < 16; i++) { px[i][0] = r[i]; px[i][1] = g[i]; px[i][2] = 0; px[i][3] = 1; }
                } else {
                    u8 c[16][4];
                    int dxt1 = bb == 8;
                    dxt_color(b + (dxt1 ? 0 : 8), c, dxt1);
                    int noalpha = f == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || f == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
                    for (int i = 0; i < 16; i++) {
                        for (int k = 0; k < 3; k++) px[i][k] = c[i][k] / 255.0;
                        px[i][3] = noalpha ? 1 : c[i][3] / 255.0;
                    }
                    if (f == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT || f == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT)
                        for (int i = 0; i < 16; i++) px[i][3] = ((b[i / 2] >> (4 * (i & 1))) & 15) / 15.0;
                    if (f == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT || f == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT) {
                        double a[16];
                        rgtc_chan(b, 0, a);
                        for (int i = 0; i < 16; i++) px[i][3] = a[i];
                    }
                    if (dfmt == 71) for (int i = 0; i < 16; i++) for (int k = 0; k < 3; k++) {
                        /* o bloco sRGB guarda valores codificados: decodifica para o encode sRGB voltar ao mesmo byte */
                        double s = px[i][k];
                        px[i][k] = s <= 0.04045 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4);
                    }
                }
                for (int i = 0; i < 16; i++) {
                    int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                    if (x >= w || y >= h) continue;
                    mx_encode(p, dst + (((i64)z * h + y) * w + x) * bpp, px[i]);
                }
            }
}

/* ================================================================ objetos de textura */
gl_tex_t *gl_tex_new(u32 name, GLenum target) {
    gl_tex_t *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->name = name;
    t->target = target;
    int rect = target == GL_TEXTURE_RECTANGLE;
    t->min_f = rect ? GL_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
    t->mag_f = GL_LINEAR;
    t->wrap[0] = t->wrap[1] = t->wrap[2] = rect ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    t->cmp_mode = GL_NONE;
    t->cmp_func = GL_LEQUAL;
    t->min_lod = -1000;
    t->max_lod = 1000;
    t->max_level = 1000;
    t->aniso = 1;
    t->swz[0] = GL_RED; t->swz[1] = GL_GREEN; t->swz[2] = GL_BLUE; t->swz[3] = GL_ALPHA;
    t->depth_mode = GL_LUMINANCE;
    t->dirty = 1;
    t->refs = 1;
    return t;
}
void gl_tex_free(gl_tex_t *t) {
    if (!t) return;
    for (int i = 0; i < 16; i++) { free(t->lv[i].data); free(t->lv[i].cdata); }
    free(t->packed);
    gl_buf_unref(t->tbo);
    free(t);
}
static gl_tex_t *tex_obj(gl_ctx_t *C, GLuint name, GLenum target) {
    gl_tex_t *t = gl_names_get(&C->sh->texs, name);
    if (!t && name) {
        t = gl_tex_new(name, target);
        if (t) gl_names_set(&C->sh->texs, name, t);
    }
    return t;
}
void glGenTextures(GLsizei n, GLuint *out) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->sh->texs); }
void gl_fbo_detach_tex(gl_ctx_t *C, GLuint name);
void glDeleteTextures(GLsizei n, const GLuint *names) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < n; i++) {
        if (!names[i]) continue;
        gl_tex_t *t = gl_names_get(&C->sh->texs, names[i]);
        gl_names_del(&C->sh->texs, names[i]);
        if (!t) continue;
        for (int u = 0; u < GLX_UNITS; u++)
            for (int k = 0; k < 11; k++) if (C->tex[u][k] == t) C->tex[u][k] = C->tex_default[k];
        gl_fbo_detach_tex(C, names[i]);
        gl_tex_free(t);
    }
}
GLboolean glIsTexture(GLuint name) { GLC(0); return gl_names_get(&C->sh->texs, name) != 0; }
void glBindTexture(GLenum target, GLuint name) {
    GLC();
    int ti = gl_tgt_index(target);
    if (ti < 0 || target == GL_PROXY_TEXTURE_2D) GLERR(GL_INVALID_ENUM, );
    if (C->dl_cur) { u32 a[2] = {target, name}; extern int gl_dl_record(gl_ctx_t *, int, const void *, int); if (gl_dl_record(C, 4, a, sizeof a)) return; }
    if (!name) { C->tex[C->unit][ti] = C->tex_default[ti]; return; }
    if (C->profile_core && !gl_names_is(&C->sh->texs, name)) GLERR(GL_INVALID_OPERATION, );
    gl_tex_t *t = tex_obj(C, name, target);
    if (!t) GLERR(GL_OUT_OF_MEMORY, );
    if (t->target != target) {
        if (t->lv[0].data || t->immutable || t->tbo) GLERR(GL_INVALID_OPERATION, );
        t->target = target;
        if (target == GL_TEXTURE_RECTANGLE) { t->min_f = GL_LINEAR; t->wrap[0] = t->wrap[1] = t->wrap[2] = GL_CLAMP_TO_EDGE; }
    }
    C->tex[C->unit][ti] = t;
}
void glActiveTexture(GLenum u) { GLC(); if (u < GL_TEXTURE0 || u >= GL_TEXTURE0 + GLX_UNITS) GLERR(GL_INVALID_ENUM, ); C->unit = (int)(u - GL_TEXTURE0); }
void glActiveTextureARB(GLenum u) { glActiveTexture(u); }
void glBindTextureEXT(GLenum t, GLuint n) { glBindTexture(t, n); }
void glGenTexturesEXT(GLsizei n, GLuint *t) { glGenTextures(n, t); }
void glDeleteTexturesEXT(GLsizei n, const GLuint *t) { glDeleteTextures(n, t); }
GLboolean glAreTexturesResident(GLsizei n, const GLuint *t, GLboolean *res) { for (int i = 0; i < n; i++) if (res) res[i] = 1; (void)t; return 1; }
void glPrioritizeTextures(GLsizei n, const GLuint *t, const GLfloat *p) { (void)n; (void)t; (void)p; }
void glTextureRangeAPPLE(GLenum t, GLsizei len, const void *p) { (void)t; (void)len; (void)p; }
void glGetTexParameterPointervAPPLE(GLenum t, GLenum p, void **v) { (void)t; (void)p; if (v) *v = 0; }

/* face do cube / camada a partir do alvo da imagem */
static int face_of(GLenum t) { return t >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && t <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ? (int)(t - GL_TEXTURE_CUBE_MAP_POSITIVE_X) : -1; }
static int is_proxy(GLenum t) {
    return t == GL_PROXY_TEXTURE_1D || t == GL_PROXY_TEXTURE_2D || t == GL_PROXY_TEXTURE_3D || t == GL_PROXY_TEXTURE_CUBE_MAP ||
           t == GL_PROXY_TEXTURE_RECTANGLE || t == GL_PROXY_TEXTURE_1D_ARRAY || t == GL_PROXY_TEXTURE_2D_ARRAY ||
           t == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY || t == GL_PROXY_TEXTURE_2D_MULTISAMPLE || t == GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY;
}
static gl_tex_t g_proxy[11];
static gl_tex_t *tex_for_image(gl_ctx_t *C, GLenum target) {
    int ti = gl_tgt_index(target);
    if (ti < 0) { gl_err(C, GL_INVALID_ENUM); return 0; }
    if (is_proxy(target)) { gl_tex_t *p = &g_proxy[ti]; p->target = target; return p; }
    if (ti == 3 && face_of(target) < 0 && target != GL_TEXTURE_CUBE_MAP) { gl_err(C, GL_INVALID_ENUM); return 0; }
    return C->tex[C->unit][ti];
}
/* define o nível: w, h, d (camadas para arrays, 6 para cube, profundidade no 3D) */
static gl_level_t *level_alloc(gl_ctx_t *C, gl_tex_t *t, int lvl, GLenum ifmt, int w, int h, int d) {
    if (lvl < 0 || lvl >= 16) { gl_err(C, GL_INVALID_VALUE); return 0; }
    int ok;
    int fmt = gl_ifmt_to_mtl(ifmt, &ok);
    if (!ok) { gl_err(C, GL_INVALID_VALUE); return 0; }
    if (w < 0 || h < 0 || d < 0 || w > 16384 || h > 16384 || d > 2048) { gl_err(C, GL_INVALID_VALUE); return 0; }
    gl_level_t *L = &t->lv[lvl];
    i64 bytes = (i64)w * h * d * mx_bpp(fmt);
    if (is_proxy(t->target)) { L->w = w; L->h = h; L->d = d; L->ifmt = ifmt; L->fmt = fmt; return L; }
    if (L->w != w || L->h != h || L->d != d || L->fmt != fmt || !L->data) {
        free(L->data);
        L->data = bytes ? calloc(1, (size_t)bytes) : 0;
        if (bytes && !L->data) { gl_err(C, GL_OUT_OF_MEMORY); return 0; }
    }
    free(L->cdata);
    L->cdata = 0;
    L->csize = 0;
    L->w = w; L->h = h; L->d = d;
    L->ifmt = ifmt;
    L->fmt = fmt;
    t->dirty = 1;
    return L;
}
static const u8 *unpack_src(gl_ctx_t *C, const void *pixels) {
    if (!C->b_unpack) return pixels;
    if (!C->b_unpack->data) return 0;
    return C->b_unpack->data + (uintptr_t)pixels;
}
static int fmt_type_ok(gl_ctx_t *C, GLenum f, GLenum t) {
    if (!fmt_comps(f)) { gl_err(C, GL_INVALID_ENUM); return 0; }
    if (!type_size(t) && t != GL_UNSIGNED_SHORT_8_8_APPLE && t != GL_UNSIGNED_SHORT_8_8_REV_APPLE) { gl_err(C, GL_INVALID_ENUM); return 0; }
    pk_t k;
    if (packed_layout(t, &k) && k.n != fmt_comps(f) && !(f == GL_BGRA && k.n == 4) && !(f == GL_BGR && k.n == 3)) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    return 1;
}
/* envia uma região de pixels para (lvl, x, y, z) da textura; z é a camada/face/fatia */
static void upload(gl_ctx_t *C, gl_tex_t *t, int lvl, int x, int y, int z, int w, int h, int d, GLenum f, GLenum ty, const void *px) {
    gl_level_t *L = &t->lv[lvl];
    if (!px && !C->b_unpack) return;
    const u8 *src = unpack_src(C, px);
    if (!src) return;
    int bpp = mx_bpp(L->fmt);
    GLenum base = gl_ifmt_base(L->ifmt);
    u8 *tmp = malloc((size_t)((i64)w * h * d * bpp + 1));
    if (!tmp) { gl_err(C, GL_OUT_OF_MEMORY); return; }
    unpack_image(C, w, h, d, f, ty, src, L->fmt, base, tmp);
    for (int k = 0; k < d; k++)
        for (int r = 0; r < h; r++)
            memcpy(L->data + ((((i64)(z + k) * L->h) + y + r) * L->w + x) * bpp, tmp + (((i64)k * h + r) * w) * bpp, (size_t)w * bpp);
    free(tmp);
    t->dirty = 1;
}
static void gen_mips_if(gl_ctx_t *C, gl_tex_t *t, int lvl);
void glTexImage3D(GLenum target, GLint lvl, GLint ifmt, GLsizei w, GLsizei h, GLsizei d, GLint border, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    if (target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_CUBE_MAP_ARRAY &&
        target != GL_PROXY_TEXTURE_3D && target != GL_PROXY_TEXTURE_2D_ARRAY && target != GL_PROXY_TEXTURE_CUBE_MAP_ARRAY) GLERR(GL_INVALID_ENUM, );
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    if (t->immutable) GLERR(GL_INVALID_OPERATION, );
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY && d % 6) GLERR(GL_INVALID_VALUE, );
    gl_level_t *L = level_alloc(C, t, lvl, (GLenum)ifmt, w, h, d);
    if (!L || is_proxy(target)) return;
    upload(C, t, lvl, 0, 0, 0, w, h, d, f, ty, px);
    gen_mips_if(C, t, lvl);
}
void glTexImage2D(GLenum target, GLint lvl, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    int face = face_of(target);
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE && target != GL_TEXTURE_1D_ARRAY && face < 0 &&
        target != GL_PROXY_TEXTURE_2D && target != GL_PROXY_TEXTURE_RECTANGLE && target != GL_PROXY_TEXTURE_1D_ARRAY &&
        target != GL_PROXY_TEXTURE_CUBE_MAP) GLERR(GL_INVALID_ENUM, );
    if (target == GL_TEXTURE_RECTANGLE && lvl) GLERR(GL_INVALID_VALUE, );
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    if (t->immutable) GLERR(GL_INVALID_OPERATION, );
    if (face >= 0 || target == GL_PROXY_TEXTURE_CUBE_MAP) {
        if (w != h) GLERR(GL_INVALID_VALUE, );
        gl_level_t *L = &t->lv[lvl < 16 && lvl >= 0 ? lvl : 0];
        int ok;
        int fmt = gl_ifmt_to_mtl((GLenum)ifmt, &ok);
        if (!ok) GLERR(GL_INVALID_VALUE, );
        /* as 6 faces dividem o nível; trocar o tamanho ou formato zera as outras */
        if (L->w != w || L->fmt != fmt || L->d != 6 || !L->data) { if (!level_alloc(C, t, lvl, (GLenum)ifmt, w, h, 6)) return; }
        else { L->ifmt = (GLenum)ifmt; t->dirty = 1; }
        if (target == GL_PROXY_TEXTURE_CUBE_MAP) return;
        upload(C, t, lvl, 0, 0, face, w, h, 1, f, ty, px);
        gen_mips_if(C, t, lvl);
        return;
    }
    gl_level_t *L = level_alloc(C, t, lvl, (GLenum)ifmt, w, h, 1);
    if (!L || is_proxy(target)) return;
    upload(C, t, lvl, 0, 0, 0, w, h, 1, f, ty, px);
    gen_mips_if(C, t, lvl);
}
void glTexImage1D(GLenum target, GLint lvl, GLint ifmt, GLsizei w, GLint border, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    if (target != GL_TEXTURE_1D && target != GL_PROXY_TEXTURE_1D) GLERR(GL_INVALID_ENUM, );
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    if (t->immutable) GLERR(GL_INVALID_OPERATION, );
    gl_level_t *L = level_alloc(C, t, lvl, (GLenum)ifmt, w, 1, 1);
    if (!L || is_proxy(target)) return;
    upload(C, t, lvl, 0, 0, 0, w, 1, 1, f, ty, px);
    gen_mips_if(C, t, lvl);
}
static int sub_ok(gl_ctx_t *C, gl_tex_t *t, int lvl, int x, int y, int z, int w, int h, int d) {
    if (!t) return 0;
    if (lvl < 0 || lvl >= 16) { gl_err(C, GL_INVALID_VALUE); return 0; }
    gl_level_t *L = &t->lv[lvl];
    if (!L->data && (w && h && d)) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    if (x < 0 || y < 0 || z < 0 || w < 0 || h < 0 || d < 0 || x + w > L->w || y + h > L->h || z + d > L->d) { gl_err(C, GL_INVALID_VALUE); return 0; }
    return 1;
}
void glTexSubImage3D(GLenum target, GLint lvl, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    if (!sub_ok(C, t, lvl, x, y, z, w, h, d)) return;
    upload(C, t, lvl, x, y, z, w, h, d, f, ty, px);
    gen_mips_if(C, t, lvl);
}
void glTexSubImage2D(GLenum target, GLint lvl, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    int face = face_of(target);
    if (!sub_ok(C, t, lvl, x, y, face < 0 ? 0 : face, w, h, 1)) return;
    upload(C, t, lvl, x, y, face < 0 ? 0 : face, w, h, 1, f, ty, px);
    gen_mips_if(C, t, lvl);
}
void glTexSubImage1D(GLenum target, GLint lvl, GLint x, GLsizei w, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    if (!sub_ok(C, t, lvl, x, 0, 0, w, 1, 1)) return;
    upload(C, t, lvl, x, 0, 0, w, 1, 1, f, ty, px);
    gen_mips_if(C, t, lvl);
}
/* ---- comprimidas */
static void cupload(gl_ctx_t *C, gl_tex_t *t, int lvl, GLenum f, int x, int y, int z, int w, int h, int d, GLsizei size, const void *data, int whole) {
    gl_level_t *L = &t->lv[lvl];
    const u8 *src = unpack_src(C, data);
    if (!src) return;
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    i64 need = (i64)bw * bh * d * block_bytes(f);
    if (size < need) { gl_err(C, GL_INVALID_VALUE); return; }
    int bpp = mx_bpp(L->fmt);
    u8 *tmp = malloc((size_t)((i64)w * h * d * bpp + 1));
    if (!tmp) { gl_err(C, GL_OUT_OF_MEMORY); return; }
    decompress(f, w, h, d, src, tmp, L->fmt);
    for (int k = 0; k < d; k++)
        for (int r = 0; r < h; r++)
            memcpy(L->data + ((((i64)(z + k) * L->h) + y + r) * L->w + x) * bpp, tmp + (((i64)k * h + r) * w) * bpp, (size_t)w * bpp);
    free(tmp);
    if (whole) {
        free(L->cdata);
        L->cdata = malloc((size_t)size);
        if (L->cdata) { memcpy(L->cdata, src, (size_t)size); L->csize = size; }
    } else { free(L->cdata); L->cdata = 0; L->csize = 0; }
    t->dirty = 1;
}
void glCompressedTexImage2D(GLenum target, GLint lvl, GLenum ifmt, GLsizei w, GLsizei h, GLint border, GLsizei size, const void *data) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    if (!is_compressed(ifmt)) { glTexImage2D(target, lvl, (GLint)ifmt, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0); return; }
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    int face = face_of(target);
    if (face >= 0) {
        gl_level_t *L = &t->lv[lvl < 16 && lvl >= 0 ? lvl : 0];
        int fmt = gl_ifmt_to_mtl(ifmt, 0);
        if (L->w != w || L->fmt != fmt || L->d != 6 || !L->data) { if (!level_alloc(C, t, lvl, ifmt, w, h, 6)) return; }
        cupload(C, t, lvl, ifmt, 0, 0, face, w, h, 1, size, data, 0);
        return;
    }
    if (!level_alloc(C, t, lvl, ifmt, w, h, 1) || is_proxy(target)) return;
    cupload(C, t, lvl, ifmt, 0, 0, 0, w, h, 1, size, data, 1);
}
void glCompressedTexImage3D(GLenum target, GLint lvl, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d, GLint border, GLsizei size, const void *data) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    if (!is_compressed(ifmt)) GLERR(GL_INVALID_ENUM, );
    gl_tex_t *t = tex_for_image(C, target);
    if (!t || !level_alloc(C, t, lvl, ifmt, w, h, d) || is_proxy(target)) return;
    cupload(C, t, lvl, ifmt, 0, 0, 0, w, h, d, size, data, 1);
}
void glCompressedTexImage1D(GLenum target, GLint lvl, GLenum ifmt, GLsizei w, GLint border, GLsizei size, const void *data) {
    (void)target; (void)lvl; (void)ifmt; (void)w; (void)border; (void)size; (void)data;
    GLC();
    GLERR(GL_INVALID_ENUM, );   /* nenhum formato comprimido do GL 4.1 é 1D */
}
void glCompressedTexSubImage2D(GLenum target, GLint lvl, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLsizei size, const void *data) {
    GLC();
    gl_tex_t *t = tex_for_image(C, target);
    int face = face_of(target);
    if (!sub_ok(C, t, lvl, x, y, face < 0 ? 0 : face, w, h, 1)) return;
    if ((x & 3) || (y & 3)) GLERR(GL_INVALID_OPERATION, );
    cupload(C, t, lvl, f, x, y, face < 0 ? 0 : face, w, h, 1, size, data, 0);
}
void glCompressedTexSubImage3D(GLenum target, GLint lvl, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum f, GLsizei size, const void *data) {
    GLC();
    gl_tex_t *t = tex_for_image(C, target);
    if (!sub_ok(C, t, lvl, x, y, z, w, h, d)) return;
    if ((x & 3) || (y & 3)) GLERR(GL_INVALID_OPERATION, );
    cupload(C, t, lvl, f, x, y, z, w, h, d, size, data, 0);
}
void glCompressedTexSubImage1D(GLenum target, GLint lvl, GLint x, GLsizei w, GLenum f, GLsizei size, const void *data) {
    (void)target; (void)lvl; (void)x; (void)w; (void)f; (void)size; (void)data;
    GLC();
    GLERR(GL_INVALID_ENUM, );
}
void glGetCompressedTexImage(GLenum target, GLint lvl, void *img) {
    GLC();
    gl_tex_t *t = tex_for_image(C, target);
    if (!t || lvl < 0 || lvl >= 16) GLERR(GL_INVALID_VALUE, );
    gl_level_t *L = &t->lv[lvl];
    if (!L->cdata) GLERR(GL_INVALID_OPERATION, );
    u8 *dst = C->b_pack ? (C->b_pack->data ? C->b_pack->data + (uintptr_t)img : 0) : img;
    if (dst) memcpy(dst, L->cdata, (size_t)L->csize);
}
void glCompressedTexImage2DARB(GLenum t, GLint l, GLenum f, GLsizei w, GLsizei h, GLint b, GLsizei s, const void *d) { glCompressedTexImage2D(t, l, f, w, h, b, s, d); }
void glCompressedTexImage3DARB(GLenum t, GLint l, GLenum f, GLsizei w, GLsizei h, GLsizei dd, GLint b, GLsizei s, const void *d) { glCompressedTexImage3D(t, l, f, w, h, dd, b, s, d); }
void glCompressedTexImage1DARB(GLenum t, GLint l, GLenum f, GLsizei w, GLint b, GLsizei s, const void *d) { glCompressedTexImage1D(t, l, f, w, b, s, d); }
void glCompressedTexSubImage2DARB(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLsizei s, const void *d) { glCompressedTexSubImage2D(t, l, x, y, w, h, f, s, d); }
void glCompressedTexSubImage3DARB(GLenum t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei dd, GLenum f, GLsizei s, const void *d) { glCompressedTexSubImage3D(t, l, x, y, z, w, h, dd, f, s, d); }
void glCompressedTexSubImage1DARB(GLenum t, GLint l, GLint x, GLsizei w, GLenum f, GLsizei s, const void *d) { glCompressedTexSubImage1D(t, l, x, w, f, s, d); }
void glGetCompressedTexImageARB(GLenum t, GLint l, void *img) { glGetCompressedTexImage(t, l, img); }

/* ---- armazenamento imutável (ARB_texture_storage) */
static void storage(gl_ctx_t *C, GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d) {
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    if (!t->name && !is_proxy(target)) GLERR(GL_INVALID_OPERATION, );
    if (t->immutable) GLERR(GL_INVALID_OPERATION, );
    if (levels < 1 || w < 1 || h < 1 || d < 1) GLERR(GL_INVALID_VALUE, );
    int cube = target == GL_TEXTURE_CUBE_MAP;
    int is3d = target == GL_TEXTURE_3D;
    for (int l = 0; l < levels && l < 16; l++) {
        int lw = w >> l, lh = h >> l, ld = is3d ? d >> l : d;
        if (lw < 1) lw = 1;
        if (lh < 1) lh = 1;
        if (ld < 1) ld = 1;
        if (target == GL_TEXTURE_1D_ARRAY) lh = h;
        if (!level_alloc(C, t, l, ifmt, lw, lh, cube ? 6 : ld)) return;
    }
    if (is_proxy(target)) return;
    t->immutable = 1;
    t->imm_levels = levels;
}
void glTexStorage1D(GLenum t, GLsizei l, GLenum f, GLsizei w) { GLC(); storage(C, t, l, f, w, 1, 1); }
void glTexStorage2D(GLenum t, GLsizei l, GLenum f, GLsizei w, GLsizei h) { GLC(); storage(C, t, l, f, w, h, 1); }
void glTexStorage3D(GLenum t, GLsizei l, GLenum f, GLsizei w, GLsizei h, GLsizei d) { GLC(); storage(C, t, l, f, w, h, d); }
void glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLboolean fixed) {
    GLC();
    (void)fixed;
    if (samples < 1 || samples > 8) GLERR(GL_INVALID_VALUE, );
    gl_tex_t *t = tex_for_image(C, target);
    if (t) level_alloc(C, t, 0, ifmt, w, h, 1);   /* uma amostra por pixel: o resolve é cópia */
}
void glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d, GLboolean fixed) {
    GLC();
    (void)fixed;
    if (samples < 1 || samples > 8) GLERR(GL_INVALID_VALUE, );
    gl_tex_t *t = tex_for_image(C, target);
    if (t) level_alloc(C, t, 0, ifmt, w, h, d);
}
void glTexBuffer(GLenum target, GLenum ifmt, GLuint buf) {
    GLC();
    if (target != GL_TEXTURE_BUFFER) GLERR(GL_INVALID_ENUM, );
    gl_tex_t *t = C->tex[C->unit][7];
    if (!t || !t->name) GLERR(GL_INVALID_OPERATION, );
    gl_buf_t *b = buf ? gl_names_get(&C->sh->bufs, buf) : 0;
    if (buf && !b) GLERR(GL_INVALID_OPERATION, );
    gl_buf_ref(b);
    gl_buf_unref(t->tbo);
    t->tbo = b;
    t->tbo_fmt = ifmt;
    t->dirty = 1;
}
void glTexBufferARB(GLenum t, GLenum f, GLuint b) { glTexBuffer(t, f, b); }
void glTexBufferEXT(GLenum t, GLenum f, GLuint b) { glTexBuffer(t, f, b); }

/* ---- mipmaps */
static void box_down(const gl_level_t *s, gl_level_t *d, int is3d) {
    int bpp = mx_bpp(s->fmt);
    const pf_t *p = mx_pf(s->fmt);
    int layers = is3d ? 1 : s->d;
    for (int k = 0; k < (is3d ? d->d : layers); k++)
        for (int y = 0; y < d->h; y++)
            for (int x = 0; x < d->w; x++) {
                double acc[4] = {0, 0, 0, 0};
                int cnt = 0;
                for (int dz = 0; dz < (is3d && s->d > 1 ? 2 : 1); dz++)
                    for (int dy = 0; dy < (s->h > 1 ? 2 : 1); dy++)
                        for (int dx = 0; dx < (s->w > 1 ? 2 : 1); dx++) {
                            int sx = x * 2 + dx, sy = y * 2 + dy, sz = is3d ? k * 2 + dz : k;
                            if (sx >= s->w) sx = s->w - 1;
                            if (sy >= s->h) sy = s->h - 1;
                            if (sz >= s->d) sz = s->d - 1;
                            double v[4];
                            mx_decode(p, s->data + (((i64)sz * s->h + sy) * s->w + sx) * bpp, v);
                            for (int i = 0; i < 4; i++) acc[i] += v[i];
                            cnt++;
                        }
                for (int i = 0; i < 4; i++) acc[i] /= cnt;
                mx_encode(p, d->data + (((i64)k * d->h + y) * d->w + x) * bpp, acc);
            }
}
static void gen_mips(gl_ctx_t *C, gl_tex_t *t) {
    int b = t->base_level < 16 ? t->base_level : 0;
    gl_level_t *L0 = &t->lv[b];
    if (!L0->data) return;
    mx_srgb_init();
    int is3d = t->target == GL_TEXTURE_3D;
    int arr1d = t->target == GL_TEXTURE_1D_ARRAY;
    int top = t->immutable ? t->imm_levels - 1 : 15;
    if (t->max_level < top) top = t->max_level;
    for (int l = b + 1; l <= top && l < 16; l++) {
        gl_level_t *s = &t->lv[l - 1];
        if (s->w == 1 && (s->h == 1 || arr1d) && (!is3d || s->d == 1)) break;
        int w = s->w > 1 ? s->w / 2 : 1, h = arr1d ? s->h : (s->h > 1 ? s->h / 2 : 1), d = is3d ? (s->d > 1 ? s->d / 2 : 1) : s->d;
        gl_level_t *dl = t->immutable ? &t->lv[l] : level_alloc(C, t, l, L0->ifmt, w, h, d);
        if (!dl || !dl->data) return;
        if (arr1d) {
            /* 1D array: as camadas são as linhas */
            gl_level_t s2 = *s, d2 = *dl;
            s2.d = s->h; s2.h = 1; d2.d = dl->h; d2.h = 1;
            box_down(&s2, &d2, 0);
        } else box_down(s, dl, is3d);
    }
    t->dirty = 1;
}
static void gen_mips_if(gl_ctx_t *C, gl_tex_t *t, int lvl) { if (t->gen_mipmap && lvl == t->base_level) gen_mips(C, t); }
void glGenerateMipmap(GLenum target) {
    GLC();
    int ti = gl_tgt_index(target);
    if (ti < 0 || ti == 4 || ti == 7 || ti == 8 || ti == 9) GLERR(GL_INVALID_ENUM, );
    gen_mips(C, C->tex[C->unit][ti]);
}
void glGenerateMipmapEXT(GLenum t) { glGenerateMipmap(t); }

/* ================================================================ parâmetros */
static int wrap_ok(GLenum w) { return w == GL_REPEAT || w == GL_CLAMP_TO_EDGE || w == GL_CLAMP_TO_BORDER || w == GL_MIRRORED_REPEAT || w == GL_CLAMP || w == GL_MIRROR_CLAMP_TO_EDGE || w == GL_MIRROR_CLAMP_EXT || w == GL_MIRROR_CLAMP_TO_BORDER_EXT; }
static int filt_ok(GLenum f, int min) { return f == GL_NEAREST || f == GL_LINEAR || (min && (f == GL_NEAREST_MIPMAP_NEAREST || f == GL_LINEAR_MIPMAP_NEAREST || f == GL_NEAREST_MIPMAP_LINEAR || f == GL_LINEAR_MIPMAP_LINEAR)); }
static int swz_ok(GLenum s) { return s == GL_RED || s == GL_GREEN || s == GL_BLUE || s == GL_ALPHA || s == GL_ZERO || s == GL_ONE; }
/* parâmetro comum a textura e sampler; devolve 0 se o nome não é de amostragem */
static int samp_param(gl_ctx_t *C, GLenum p, const double *v, GLenum *min_f, GLenum *mag_f, GLenum *wrap, GLenum *cmode, GLenum *cfunc,
                      float *minl, float *maxl, float *bias, float *border, float *aniso) {
    GLenum e = (GLenum)v[0];
    switch (p) {
    case GL_TEXTURE_MIN_FILTER: if (!filt_ok(e, 1)) { gl_err(C, GL_INVALID_ENUM); return 1; } *min_f = e; return 1;
    case GL_TEXTURE_MAG_FILTER: if (!filt_ok(e, 0)) { gl_err(C, GL_INVALID_ENUM); return 1; } *mag_f = e; return 1;
    case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T: case GL_TEXTURE_WRAP_R:
        if (!wrap_ok(e)) { gl_err(C, GL_INVALID_ENUM); return 1; }
        wrap[p == GL_TEXTURE_WRAP_S ? 0 : p == GL_TEXTURE_WRAP_T ? 1 : 2] = e;
        return 1;
    case GL_TEXTURE_COMPARE_MODE: if (e != GL_NONE && e != GL_COMPARE_REF_TO_TEXTURE) { gl_err(C, GL_INVALID_ENUM); return 1; } *cmode = e; return 1;
    case GL_TEXTURE_COMPARE_FUNC: if (e < GL_NEVER || e > GL_ALWAYS) { gl_err(C, GL_INVALID_ENUM); return 1; } *cfunc = e; return 1;
    case GL_TEXTURE_MIN_LOD: *minl = (float)v[0]; return 1;
    case GL_TEXTURE_MAX_LOD: *maxl = (float)v[0]; return 1;
    case GL_TEXTURE_LOD_BIAS: *bias = (float)v[0]; return 1;
    case GL_TEXTURE_BORDER_COLOR: for (int i = 0; i < 4; i++) border[i] = (float)v[i]; return 1;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT: if (v[0] < 1) { gl_err(C, GL_INVALID_VALUE); return 1; } *aniso = (float)v[0]; return 1;
    case GL_TEXTURE_SRGB_DECODE_EXT: return 1;
    }
    return 0;
}
static void tex_param(gl_ctx_t *C, GLenum target, GLenum p, const double *v, int n) {
    (void)n;
    gl_tex_t *t = gl_tex_bound(C, target);
    if (!t) GLERR(GL_INVALID_ENUM, );
    if (samp_param(C, p, v, &t->min_f, &t->mag_f, t->wrap, &t->cmp_mode, &t->cmp_func, &t->min_lod, &t->max_lod, &t->lod_bias, t->border, &t->aniso)) {
        if (target == GL_TEXTURE_RECTANGLE && p == GL_TEXTURE_MIN_FILTER && t->min_f != GL_NEAREST && t->min_f != GL_LINEAR) { t->min_f = GL_LINEAR; gl_err(C, GL_INVALID_ENUM); }
        t->dirty = 1;
        return;
    }
    GLenum e = (GLenum)v[0];
    switch (p) {
    case GL_TEXTURE_BASE_LEVEL: if (v[0] < 0) GLERR(GL_INVALID_VALUE, ); t->base_level = (int)v[0]; break;
    case GL_TEXTURE_MAX_LEVEL: if (v[0] < 0) GLERR(GL_INVALID_VALUE, ); t->max_level = (int)v[0]; break;
    case GL_TEXTURE_SWIZZLE_R: case GL_TEXTURE_SWIZZLE_G: case GL_TEXTURE_SWIZZLE_B: case GL_TEXTURE_SWIZZLE_A:
        if (!swz_ok(e)) GLERR(GL_INVALID_ENUM, );
        t->swz[p - GL_TEXTURE_SWIZZLE_R] = e;
        break;
    case GL_TEXTURE_SWIZZLE_RGBA:
        for (int i = 0; i < 4; i++) if (!swz_ok((GLenum)v[i])) GLERR(GL_INVALID_ENUM, );
        for (int i = 0; i < 4; i++) t->swz[i] = (GLenum)v[i];
        break;
    case GL_DEPTH_TEXTURE_MODE: if (e != GL_LUMINANCE && e != GL_INTENSITY && e != GL_ALPHA && e != GL_RED) GLERR(GL_INVALID_ENUM, ); t->depth_mode = e; break;
    case GL_GENERATE_MIPMAP: t->gen_mipmap = v[0] != 0; break;
    case GL_TEXTURE_PRIORITY: break;
    case GL_TEXTURE_STORAGE_HINT_APPLE: break;
    default: GLERR(GL_INVALID_ENUM, );
    }
    t->dirty = 1;
}
void glTexParameterf(GLenum t, GLenum p, GLfloat v) { GLC(); double d[4] = {v, 0, 0, 0}; tex_param(C, t, p, d, 1); }
void glTexParameteri(GLenum t, GLenum p, GLint v) { GLC(); double d[4] = {v, 0, 0, 0}; tex_param(C, t, p, d, 1); }
void glTexParameterfv(GLenum t, GLenum p, const GLfloat *v) { GLC(); double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR || p == GL_TEXTURE_SWIZZLE_RGBA) for (int i = 1; i < 4; i++) d[i] = v[i]; tex_param(C, t, p, d, 4); }
void glTexParameteriv(GLenum t, GLenum p, const GLint *v) {
    GLC();
    double d[4] = {v[0], 0, 0, 0};
    if (p == GL_TEXTURE_BORDER_COLOR) for (int i = 0; i < 4; i++) d[i] = v[i] >= 0 ? v[i] / 2147483647.0 : v[i] / 2147483648.0;
    else if (p == GL_TEXTURE_SWIZZLE_RGBA) for (int i = 1; i < 4; i++) d[i] = v[i];
    tex_param(C, t, p, d, 4);
}
void glTexParameterIiv(GLenum t, GLenum p, const GLint *v) { GLC(); double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR || p == GL_TEXTURE_SWIZZLE_RGBA) for (int i = 1; i < 4; i++) d[i] = v[i]; tex_param(C, t, p, d, 4); }
void glTexParameterIuiv(GLenum t, GLenum p, const GLuint *v) { GLC(); double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR || p == GL_TEXTURE_SWIZZLE_RGBA) for (int i = 1; i < 4; i++) d[i] = v[i]; tex_param(C, t, p, d, 4); }
void glTexParameterIivEXT(GLenum t, GLenum p, const GLint *v) { glTexParameterIiv(t, p, v); }
void glTexParameterIuivEXT(GLenum t, GLenum p, const GLuint *v) { glTexParameterIuiv(t, p, v); }
static int tex_get(gl_ctx_t *C, gl_tex_t *t, GLenum p, double *o) {
    switch (p) {
    case GL_TEXTURE_MIN_FILTER: o[0] = t->min_f; return 1;
    case GL_TEXTURE_MAG_FILTER: o[0] = t->mag_f; return 1;
    case GL_TEXTURE_WRAP_S: o[0] = t->wrap[0]; return 1;
    case GL_TEXTURE_WRAP_T: o[0] = t->wrap[1]; return 1;
    case GL_TEXTURE_WRAP_R: o[0] = t->wrap[2]; return 1;
    case GL_TEXTURE_COMPARE_MODE: o[0] = t->cmp_mode; return 1;
    case GL_TEXTURE_COMPARE_FUNC: o[0] = t->cmp_func; return 1;
    case GL_TEXTURE_MIN_LOD: o[0] = t->min_lod; return 1;
    case GL_TEXTURE_MAX_LOD: o[0] = t->max_lod; return 1;
    case GL_TEXTURE_LOD_BIAS: o[0] = t->lod_bias; return 1;
    case GL_TEXTURE_BORDER_COLOR: for (int i = 0; i < 4; i++) o[i] = t->border[i]; return 4;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT: o[0] = t->aniso; return 1;
    case GL_TEXTURE_BASE_LEVEL: o[0] = t->base_level; return 1;
    case GL_TEXTURE_MAX_LEVEL: o[0] = t->max_level; return 1;
    case GL_TEXTURE_SWIZZLE_R: case GL_TEXTURE_SWIZZLE_G: case GL_TEXTURE_SWIZZLE_B: case GL_TEXTURE_SWIZZLE_A: o[0] = t->swz[p - GL_TEXTURE_SWIZZLE_R]; return 1;
    case GL_TEXTURE_SWIZZLE_RGBA: for (int i = 0; i < 4; i++) o[i] = t->swz[i]; return 4;
    case GL_DEPTH_TEXTURE_MODE: o[0] = t->depth_mode; return 1;
    case GL_GENERATE_MIPMAP: o[0] = t->gen_mipmap; return 1;
    case GL_TEXTURE_IMMUTABLE_FORMAT: o[0] = t->immutable; return 1;
    case GL_TEXTURE_IMMUTABLE_LEVELS: o[0] = t->imm_levels; return 1;
    case GL_TEXTURE_PRIORITY: o[0] = 1; return 1;
    case GL_TEXTURE_RESIDENT: o[0] = 1; return 1;
    }
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
void glGetTexParameterfv(GLenum target, GLenum p, GLfloat *v) { GLC(); gl_tex_t *t = gl_tex_bound(C, target); if (!t) GLERR(GL_INVALID_ENUM, ); double o[4]; int n = tex_get(C, t, p, o); for (int i = 0; i < n; i++) v[i] = (float)o[i]; }
void glGetTexParameteriv(GLenum target, GLenum p, GLint *v) {
    GLC();
    gl_tex_t *t = gl_tex_bound(C, target);
    if (!t) GLERR(GL_INVALID_ENUM, );
    double o[4];
    int n = tex_get(C, t, p, o);
    for (int i = 0; i < n; i++) v[i] = p == GL_TEXTURE_BORDER_COLOR ? (GLint)(mx_clm11(o[i]) * 2147483647.0) : (GLint)o[i];
}
void glGetTexParameterIiv(GLenum target, GLenum p, GLint *v) { GLC(); gl_tex_t *t = gl_tex_bound(C, target); if (!t) GLERR(GL_INVALID_ENUM, ); double o[4]; int n = tex_get(C, t, p, o); for (int i = 0; i < n; i++) v[i] = (GLint)o[i]; }
void glGetTexParameterIuiv(GLenum target, GLenum p, GLuint *v) { GLC(); gl_tex_t *t = gl_tex_bound(C, target); if (!t) GLERR(GL_INVALID_ENUM, ); double o[4]; int n = tex_get(C, t, p, o); for (int i = 0; i < n; i++) v[i] = (GLuint)o[i]; }
void glGetTexParameterIivEXT(GLenum t, GLenum p, GLint *v) { glGetTexParameterIiv(t, p, v); }
void glGetTexParameterIuivEXT(GLenum t, GLenum p, GLuint *v) { glGetTexParameterIuiv(t, p, v); }
static int bits_of(int fmt, int comp) {
    const pf_t *p = mx_pf(fmt);
    if (!p) return 0;
    if (comp >= p->nc && comp < 3) return 0;
    switch (p->kind) {
    case PK_UN8: case PK_SN8: case PK_U8: case PK_S8: return (p->fl & PF_ALPHA) ? (comp == 3 ? 8 : 0) : (comp == 3 && p->nc < 4 ? 0 : 8);
    case PK_UN16: case PK_SN16: case PK_U16: case PK_S16: case PK_HALF: return comp == 3 && p->nc < 4 ? 0 : 16;
    case PK_U32: case PK_S32: case PK_F32: return comp == 3 && p->nc < 4 ? 0 : 32;
    case PK_RGB10A2: case PK_RGB10A2U: return comp == 3 ? 2 : 10;
    case PK_RG11B10F: return comp == 3 ? 0 : comp == 2 ? 10 : 11;
    case PK_RGB9E5: return comp == 3 ? 0 : 9;
    }
    return 0;
}
static int level_get(gl_ctx_t *C, GLenum target, int lvl, GLenum p, int *o) {
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return 0;
    if (lvl < 0 || lvl >= 16) { gl_err(C, GL_INVALID_VALUE); return 0; }
    gl_level_t *L = &t->lv[lvl];
    int has = L->w > 0, fmt = L->fmt;
    GLenum base = gl_ifmt_base(L->ifmt);
    int lum = base == GL_LUMINANCE || base == GL_LUMINANCE_ALPHA, inten = base == GL_INTENSITY, alpha = base == GL_ALPHA;
    switch (p) {
    case GL_TEXTURE_WIDTH: *o = L->w; return 1;
    case GL_TEXTURE_HEIGHT: *o = L->h; return 1;
    case GL_TEXTURE_DEPTH: *o = target == GL_TEXTURE_CUBE_MAP || face_of(target) >= 0 ? 1 : L->d; return 1;
    case GL_TEXTURE_INTERNAL_FORMAT: *o = has ? (int)L->ifmt : 1; return 1;
    case GL_TEXTURE_BORDER: *o = 0; return 1;
    case GL_TEXTURE_RED_SIZE: *o = has && !lum && !inten && !alpha ? bits_of(fmt, 0) : 0; return 1;
    case GL_TEXTURE_GREEN_SIZE: *o = has && !lum && !inten && !alpha ? bits_of(fmt, 1) : 0; return 1;
    case GL_TEXTURE_BLUE_SIZE: *o = has && !lum && !inten && !alpha ? bits_of(fmt, 2) : 0; return 1;
    case GL_TEXTURE_ALPHA_SIZE: *o = !has ? 0 : alpha ? bits_of(fmt, 0) : base == GL_LUMINANCE_ALPHA ? bits_of(fmt, 1) : lum || inten ? 0 : bits_of(fmt, 3); return 1;
    case GL_TEXTURE_LUMINANCE_SIZE: *o = has && lum ? bits_of(fmt, 0) : 0; return 1;
    case GL_TEXTURE_INTENSITY_SIZE: *o = has && inten ? bits_of(fmt, 0) : 0; return 1;
    case GL_TEXTURE_DEPTH_SIZE: *o = !has ? 0 : fmt == 250 ? 16 : fmt == 252 || fmt == 260 ? (L->ifmt == GL_DEPTH_COMPONENT24 || L->ifmt == GL_DEPTH24_STENCIL8 ? 24 : 32) : 0; return 1;
    case GL_TEXTURE_STENCIL_SIZE: *o = has && (fmt == 260 || fmt == 253) ? 8 : 0; return 1;
    case GL_TEXTURE_SHARED_SIZE: *o = has && fmt == 93 ? 5 : 0; return 1;
    case GL_TEXTURE_COMPRESSED: *o = has && is_compressed(L->ifmt); return 1;
    case GL_TEXTURE_COMPRESSED_IMAGE_SIZE: *o = (int)L->csize; if (!L->cdata) { gl_err(C, GL_INVALID_OPERATION); } return 1;
    case GL_TEXTURE_SAMPLES: *o = 0; return 1;
    case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS: *o = 1; return 1;
    case GL_TEXTURE_BUFFER_DATA_STORE_BINDING: *o = t->tbo ? (int)t->tbo->name : 0; return 1;
    case GL_TEXTURE_RED_TYPE: case GL_TEXTURE_GREEN_TYPE: case GL_TEXTURE_BLUE_TYPE: case GL_TEXTURE_ALPHA_TYPE:
    case GL_TEXTURE_LUMINANCE_TYPE: case GL_TEXTURE_INTENSITY_TYPE: case GL_TEXTURE_DEPTH_TYPE: {
        const pf_t *pp = mx_pf(fmt);
        if (!has || !pp) { *o = GL_NONE; return 1; }
        int k = pp->kind;
        *o = k == PK_F32 || k == PK_HALF || k == PK_RG11B10F || k == PK_RGB9E5 || k == PK_D32F || k == PK_D32FS8 ? GL_FLOAT :
             k == PK_SN8 || k == PK_SN16 ? GL_SIGNED_NORMALIZED : k == PK_U8 || k == PK_U16 || k == PK_U32 || k == PK_RGB10A2U ? GL_UNSIGNED_INT :
             k == PK_S8 || k == PK_S16 || k == PK_S32 ? GL_INT : GL_UNSIGNED_NORMALIZED;
        return 1;
    }
    }
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
void glGetTexLevelParameteriv(GLenum target, GLint lvl, GLenum p, GLint *v) { GLC(); int o; if (level_get(C, target, lvl, p, &o) && v) *v = o; }
void glGetTexLevelParameterfv(GLenum target, GLint lvl, GLenum p, GLfloat *v) { GLC(); int o; if (level_get(C, target, lvl, p, &o) && v) *v = (float)o; }
void glGetTexImage(GLenum target, GLint lvl, GLenum f, GLenum ty, void *px) {
    GLC();
    if (!fmt_type_ok(C, f, ty)) return;
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    if (lvl < 0 || lvl >= 16) GLERR(GL_INVALID_VALUE, );
    gl_level_t *L = &t->lv[lvl];
    if (!L->data) return;
    u8 *dst = C->b_pack ? (C->b_pack->data ? C->b_pack->data + (uintptr_t)px : 0) : px;
    if (!dst) GLERR(GL_INVALID_OPERATION, );
    int face = face_of(target);
    int z0 = face >= 0 ? face : 0, nz = face >= 0 ? 1 : L->d;
    msl_tex_t m = {0};
    m.data = L->data; m.fmt = L->fmt; m.type = MSL_TEX_2D_ARRAY; m.w = L->w; m.h = L->h; m.d = 1; m.levels = 1; m.slices = L->d;
    m.lbpr[0] = (u64)L->w * mx_bpp(L->fmt); m.lslice[0] = m.lbpr[0] * L->h;
    const ifmt_t *fi = ifmt_find(L->ifmt);
    m.swizzle = fi ? fi->swz : 0;
    i64 rs = row_stride(C, 1, L->w, f, ty), is = rs * (C->pk_imgh > 0 ? C->pk_imgh : L->h);
    for (int z = 0; z < nz; z++) gl_pack_pixels(C, &m, 0, z0 + z, 0, 0, L->w, L->h, f, ty, dst + is * z, 1);
}

/* ================================================================ samplers */
void glGenSamplers(GLsizei n, GLuint *out) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < n; i++) {
        u32 name = gl_names_gen(&C->sh->samps);
        gl_samp_t *s = calloc(1, sizeof *s);
        if (!s) GLERR(GL_OUT_OF_MEMORY, );
        s->name = name;
        s->min_f = GL_NEAREST_MIPMAP_LINEAR; s->mag_f = GL_LINEAR;
        s->wrap[0] = s->wrap[1] = s->wrap[2] = GL_REPEAT;
        s->cmp_mode = GL_NONE; s->cmp_func = GL_LEQUAL;
        s->min_lod = -1000; s->max_lod = 1000; s->aniso = 1;
        gl_names_set(&C->sh->samps, name, s);
        out[i] = name;
    }
}
void glDeleteSamplers(GLsizei n, const GLuint *names) {
    GLC();
    for (int i = 0; i < n; i++) {
        gl_samp_t *s = gl_names_get(&C->sh->samps, names[i]);
        if (!s) continue;
        for (int u = 0; u < GLX_UNITS; u++) if (C->samp[u] == s) C->samp[u] = 0;
        gl_names_del(&C->sh->samps, names[i]);
        free(s);
    }
}
GLboolean glIsSampler(GLuint name) { GLC(0); return gl_names_get(&C->sh->samps, name) != 0; }
void glBindSampler(GLuint unit, GLuint name) {
    GLC();
    if (unit >= GLX_UNITS) GLERR(GL_INVALID_VALUE, );
    gl_samp_t *s = name ? gl_names_get(&C->sh->samps, name) : 0;
    if (name && !s) GLERR(GL_INVALID_OPERATION, );
    C->samp[unit] = s;
}
static void sp_set(GLuint name, GLenum p, const double *v) {
    GLC();
    gl_samp_t *s = gl_names_get(&C->sh->samps, name);
    if (!s) GLERR(GL_INVALID_OPERATION, );
    if (!samp_param(C, p, v, &s->min_f, &s->mag_f, s->wrap, &s->cmp_mode, &s->cmp_func, &s->min_lod, &s->max_lod, &s->lod_bias, s->border, &s->aniso)) GLERR(GL_INVALID_ENUM, );
}
void glSamplerParameteri(GLuint s, GLenum p, GLint v) { double d[4] = {v, 0, 0, 0}; sp_set(s, p, d); }
void glSamplerParameterf(GLuint s, GLenum p, GLfloat v) { double d[4] = {v, 0, 0, 0}; sp_set(s, p, d); }
void glSamplerParameteriv(GLuint s, GLenum p, const GLint *v) { double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR) for (int i = 0; i < 4; i++) d[i] = v[i] / 2147483647.0; sp_set(s, p, d); }
void glSamplerParameterfv(GLuint s, GLenum p, const GLfloat *v) { double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR) for (int i = 1; i < 4; i++) d[i] = v[i]; sp_set(s, p, d); }
void glSamplerParameterIiv(GLuint s, GLenum p, const GLint *v) { double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR) for (int i = 1; i < 4; i++) d[i] = v[i]; sp_set(s, p, d); }
void glSamplerParameterIuiv(GLuint s, GLenum p, const GLuint *v) { double d[4] = {v[0], 0, 0, 0}; if (p == GL_TEXTURE_BORDER_COLOR) for (int i = 1; i < 4; i++) d[i] = v[i]; sp_set(s, p, d); }
static int sp_get(GLuint name, GLenum p, double *o) {
    GLC(0);
    gl_samp_t *s = gl_names_get(&C->sh->samps, name);
    if (!s) GLERR(GL_INVALID_OPERATION, 0);
    switch (p) {
    case GL_TEXTURE_MIN_FILTER: o[0] = s->min_f; return 1;
    case GL_TEXTURE_MAG_FILTER: o[0] = s->mag_f; return 1;
    case GL_TEXTURE_WRAP_S: o[0] = s->wrap[0]; return 1;
    case GL_TEXTURE_WRAP_T: o[0] = s->wrap[1]; return 1;
    case GL_TEXTURE_WRAP_R: o[0] = s->wrap[2]; return 1;
    case GL_TEXTURE_COMPARE_MODE: o[0] = s->cmp_mode; return 1;
    case GL_TEXTURE_COMPARE_FUNC: o[0] = s->cmp_func; return 1;
    case GL_TEXTURE_MIN_LOD: o[0] = s->min_lod; return 1;
    case GL_TEXTURE_MAX_LOD: o[0] = s->max_lod; return 1;
    case GL_TEXTURE_LOD_BIAS: o[0] = s->lod_bias; return 1;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT: o[0] = s->aniso; return 1;
    case GL_TEXTURE_BORDER_COLOR: for (int i = 0; i < 4; i++) o[i] = s->border[i]; return 4;
    }
    GLERR(GL_INVALID_ENUM, 0);
}
void glGetSamplerParameteriv(GLuint s, GLenum p, GLint *v) { double o[4]; int n = sp_get(s, p, o); for (int i = 0; i < n; i++) v[i] = (GLint)o[i]; }
void glGetSamplerParameterfv(GLuint s, GLenum p, GLfloat *v) { double o[4]; int n = sp_get(s, p, o); for (int i = 0; i < n; i++) v[i] = (float)o[i]; }
void glGetSamplerParameterIiv(GLuint s, GLenum p, GLint *v) { glGetSamplerParameteriv(s, p, v); }
void glGetSamplerParameterIuiv(GLuint s, GLenum p, GLuint *v) { double o[4]; int n = sp_get(s, p, o); for (int i = 0; i < n; i++) v[i] = (GLuint)o[i]; }

/* ================================================================ montagem para o amostrador */
static int addr_of(GLenum w) {
    switch (w) {
    case GL_CLAMP_TO_EDGE: case GL_CLAMP: return 0;
    case GL_MIRROR_CLAMP_TO_EDGE: case GL_MIRROR_CLAMP_EXT: case GL_MIRROR_CLAMP_TO_BORDER_EXT: return 1;
    case GL_REPEAT: return 2;
    case GL_MIRRORED_REPEAT: return 3;
    case GL_CLAMP_TO_BORDER: return 5;
    }
    return 2;
}
void gl_tex_sampler(gl_ctx_t *C, gl_tex_t *t, gl_samp_t *s, msl_samp_t *o) {
    (void)C;
    GLenum minf = s ? s->min_f : t->min_f, magf = s ? s->mag_f : t->mag_f;
    const GLenum *wr = s ? s->wrap : t->wrap;
    memset(o, 0, sizeof *o);
    o->magf = magf == GL_LINEAR;
    o->minf = minf == GL_LINEAR || minf == GL_LINEAR_MIPMAP_NEAREST || minf == GL_LINEAR_MIPMAP_LINEAR;
    o->mipf = minf == GL_NEAREST_MIPMAP_NEAREST || minf == GL_LINEAR_MIPMAP_NEAREST ? 1 : minf == GL_NEAREST_MIPMAP_LINEAR || minf == GL_LINEAR_MIPMAP_LINEAR ? 2 : 0;
    for (int i = 0; i < 3; i++) o->addr[i] = addr_of(wr[i]);
    o->normalized = t->target != GL_TEXTURE_RECTANGLE;
    GLenum cm = s ? s->cmp_mode : t->cmp_mode, cf = s ? s->cmp_func : t->cmp_func;
    o->compare = cm == GL_COMPARE_REF_TO_TEXTURE ? (int)(cf - GL_NEVER) : 7;
    if (cm != GL_COMPARE_REF_TO_TEXTURE) o->compare = 7;
    float minl = s ? s->min_lod : t->min_lod, maxl = s ? s->max_lod : t->max_lod;
    /* a lod é relativa ao nível base montado */
    o->lodmin = minl < 0 ? 0 : minl;
    o->lodmax = maxl;
    o->lodbias = (s ? s->lod_bias : 0) + t->lod_bias;
    o->aniso = s ? s->aniso : t->aniso;
    if (o->aniso > 16) o->aniso = 16;   /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */
    o->bcustom = 1;
    const float *b = s ? s->border : t->border;
    for (int i = 0; i < 4; i++) o->bcol[i] = b[i];
}
static u32 swz_code(GLenum s) { return s == GL_ZERO ? 0 : s == GL_ONE ? 1 : s == GL_RED ? 2 : s == GL_GREEN ? 3 : s == GL_BLUE ? 4 : 5; }
/* compõe o swizzle do formato base com o do usuário */
static u32 swz_compose(u32 base, const GLenum *user, int depth, GLenum dmode, int core) {
    u32 b[4];
    for (int i = 0; i < 4; i++) b[i] = base ? (base >> (8 * i)) & 0xff : (u32)(2 + i);
    if (depth && !core) {
        /* GL_DEPTH_TEXTURE_MODE (compat) */
        if (dmode == GL_LUMINANCE) { b[0] = b[1] = b[2] = 2; b[3] = 1; }
        else if (dmode == GL_INTENSITY) b[0] = b[1] = b[2] = b[3] = 2;
        else if (dmode == GL_ALPHA) { b[0] = b[1] = b[2] = 0; b[3] = 2; }
        else { b[0] = 2; b[1] = b[2] = 0; b[3] = 1; }
    } else if (depth) { b[0] = 2; b[1] = b[2] = 0; b[3] = 1; }
    u32 r = 0;
    for (int i = 0; i < 4; i++) {
        u32 u = swz_code(user[i]);
        u32 v = u <= 1 ? u : b[u - 2];
        r |= v << (8 * i);
    }
    return r == 0x05040302u ? 0 : r;
}
static msl_tex_t g_empty_tex;
/* monta (ou remonta) o msl_tex_t; devolve 1 se a textura está completa */
int gl_tex_complete(gl_ctx_t *C, gl_tex_t *t) {
    if (!t) return 0;
    if (!t->dirty) return t->complete;
    t->dirty = 0;
    t->complete = 0;
    free(t->packed);
    t->packed = 0;
    memset(&t->mt, 0, sizeof t->mt);
    if (t->target == GL_TEXTURE_BUFFER) {
        if (!t->tbo || !t->tbo->data) return 0;
        int fmt = gl_ifmt_to_mtl(t->tbo_fmt, 0);
        int bpp = mx_bpp(fmt);
        i64 n = t->tbo->size / (bpp ? bpp : 4);
        if (n > 134217728) n = 134217728;
        t->mt.data = t->tbo->data;
        t->mt.fmt = fmt;
        t->mt.type = MSL_TEX_BUFFER;
        t->mt.w = (int)n; t->mt.h = 1; t->mt.d = 1; t->mt.levels = 1; t->mt.slices = 1;
        t->mt.lbpr[0] = (u64)n * bpp;
        t->mt.lslice[0] = t->mt.lbpr[0];
        t->complete = 1;
        t->dirty = 1;   /* o conteúdo do buffer pode mudar a qualquer hora */
        return 1;
    }
    int b = t->base_level;
    if (b < 0 || b >= 16) return 0;
    gl_level_t *L0 = &t->lv[b];
    if (!L0->data || L0->w <= 0) return 0;
    int mip = t->min_f != GL_NEAREST && t->min_f != GL_LINEAR;
    int is3d = t->target == GL_TEXTURE_3D, arr1d = t->target == GL_TEXTURE_1D_ARRAY;
    int top = b;
    if (mip) {
        int maxl = t->immutable ? t->imm_levels - 1 : 15;
        if (t->max_level < maxl) maxl = t->max_level;
        int w = L0->w, h = L0->h, d = L0->d;
        for (int l = b + 1; l <= maxl && l < 16; l++) {
            if (w == 1 && (h == 1 || arr1d) && (!is3d || d == 1)) break;
            w = w > 1 ? w / 2 : 1;
            if (!arr1d) h = h > 1 ? h / 2 : 1;
            if (is3d) d = d > 1 ? d / 2 : 1;
            gl_level_t *L = &t->lv[l];
            if (!L->data || L->w != w || L->h != h || L->d != d || L->fmt != L0->fmt) {
                /* incompleta: o GL amostra como (0,0,0,1) */
                if (!t->immutable) return 0;
                break;
            }
            top = l;
        }
    }
    int nl = top - b + 1;
    int bpp = mx_bpp(L0->fmt);
    msl_tex_t *m = &t->mt;
    m->fmt = L0->fmt;
    m->levels = nl;
    switch (t->target) {
    case GL_TEXTURE_1D: m->type = MSL_TEX_1D; break;
    case GL_TEXTURE_1D_ARRAY: m->type = MSL_TEX_1D_ARRAY; break;
    case GL_TEXTURE_3D: m->type = MSL_TEX_3D; break;
    case GL_TEXTURE_CUBE_MAP: m->type = MSL_TEX_CUBE; break;
    case GL_TEXTURE_2D_ARRAY: case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: m->type = MSL_TEX_2D_ARRAY; break;
    case GL_TEXTURE_CUBE_MAP_ARRAY: m->type = MSL_TEX_CUBE_ARRAY; break;
    default: m->type = MSL_TEX_2D;
    }
    m->w = L0->w;
    m->h = arr1d ? 1 : L0->h;
    m->d = is3d ? L0->d : 1;
    m->slices = is3d ? 1 : arr1d ? L0->h : L0->d;
    if (t->target == GL_TEXTURE_CUBE_MAP && L0->d != 6) return 0;
    /* níveis num bloco só, no layout do mx_texel */
    u64 off = 0;
    for (int i = 0; i < nl; i++) {
        gl_level_t *L = &t->lv[b + i];
        int lh = arr1d ? 1 : L->h, ld = is3d ? L->d : 1;
        m->loff[i] = off;
        m->lbpr[i] = (u64)L->w * bpp;
        m->lslice[i] = m->lbpr[i] * lh * ld;
        off += m->lslice[i] * m->slices;
    }
    if (nl == 1) m->data = L0->data;     /* um nível: lê direto, sem cópia */
    else {
        t->packed = malloc((size_t)off);
        if (!t->packed) return 0;
        for (int i = 0; i < nl; i++) memcpy(t->packed + m->loff[i], t->lv[b + i].data, (size_t)(m->lslice[i] * m->slices));
        m->data = t->packed;
    }
    const ifmt_t *fi = ifmt_find(L0->ifmt);
    m->swizzle = swz_compose(fi ? fi->swz : 0, t->swz, mx_isdepth(L0->fmt), t->depth_mode, C->profile_core);
    t->complete = 1;
    return 1;
}
msl_tex_t *gl_tex_mt(gl_ctx_t *C, gl_tex_t *t) { return gl_tex_complete(C, t) ? &t->mt : &g_empty_tex; }
/* uma imagem (nível, camada) como msl_tex_t de 1 nível, para FBO e cópias */
int gl_tex_image(gl_tex_t *t, int lvl, int layer, msl_tex_t *m, int *nlayers) {
    if (!t || lvl < 0 || lvl >= 16) return 0;
    gl_level_t *L = &t->lv[lvl];
    if (!L->data) return 0;
    memset(m, 0, sizeof *m);
    int arr1d = t->target == GL_TEXTURE_1D_ARRAY, is3d = t->target == GL_TEXTURE_3D;
    int bpp = mx_bpp(L->fmt);
    m->fmt = L->fmt;
    m->type = MSL_TEX_2D;
    m->w = L->w;
    m->h = arr1d ? 1 : L->h;
    m->d = 1;
    m->levels = 1;
    m->slices = 1;
    m->lbpr[0] = (u64)L->w * bpp;
    m->lslice[0] = m->lbpr[0] * m->h;
    int nl = arr1d ? L->h : L->d;
    if (nlayers) *nlayers = nl;
    if (layer < 0) layer = 0;
    if (layer >= nl) return 0;
    m->data = L->data + (u64)layer * (arr1d ? m->lbpr[0] : m->lslice[0]);
    (void)is3d;
    t->dirty = 1;   /* quem pede a imagem vai escrever nela */
    return 1;
}
void gl_tex_touch(gl_tex_t *t) { if (t) t->dirty = 1; }

/* ---- cópias do framebuffer de leitura para a textura */
int gl_read_source(gl_ctx_t *C, msl_tex_t **src);
static void copy_fb(gl_ctx_t *C, gl_tex_t *t, int lvl, int dx, int dy, int dz, int x, int y, int w, int h) {
    msl_tex_t *s;
    if (!gl_read_source(C, &s) || !s) GLERR(GL_INVALID_OPERATION, );
    gl_level_t *L = &t->lv[lvl];
    int bpp = mx_bpp(L->fmt);
    const pf_t *dp = mx_pf(L->fmt);
    GLenum base = gl_ifmt_base(L->ifmt);
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            int sx = x + c, sy = y + r;
            double v[4] = {0, 0, 0, 1};
            if (sx >= 0 && sy >= 0 && sx < s->w && sy < s->h) msl_tex_load(s, sx, sy, 0, 0, 0, v);
            if (base == GL_ALPHA) v[0] = v[3];
            else if (base == GL_LUMINANCE_ALPHA) v[1] = v[3];
            else if (base == GL_RGB) v[3] = 1;
            if (L->fmt == 260) { float d = (float)v[0]; u8 *q = L->data + ((((i64)dz * L->h) + dy + r) * L->w + dx + c) * bpp; memcpy(q, &d, 4); continue; }
            mx_encode(dp, L->data + ((((i64)dz * L->h) + dy + r) * L->w + dx + c) * bpp, v);
        }
    t->dirty = 1;
}
void glCopyTexImage2D(GLenum target, GLint lvl, GLenum ifmt, GLint x, GLint y, GLsizei w, GLsizei h, GLint border) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    gl_tex_t *t = tex_for_image(C, target);
    if (!t) return;
    int face = face_of(target);
    if (!level_alloc(C, t, lvl, ifmt, w, h, face >= 0 ? 6 : 1)) return;
    copy_fb(C, t, lvl, 0, 0, face < 0 ? 0 : face, x, y, w, h);
    gen_mips_if(C, t, lvl);
}
void glCopyTexImage1D(GLenum target, GLint lvl, GLenum ifmt, GLint x, GLint y, GLsizei w, GLint border) {
    GLC();
    if (border) GLERR(GL_INVALID_VALUE, );
    gl_tex_t *t = tex_for_image(C, target);
    if (!t || !level_alloc(C, t, lvl, ifmt, w, 1, 1)) return;
    copy_fb(C, t, lvl, 0, 0, 0, x, y, w, 1);
}
void glCopyTexSubImage2D(GLenum target, GLint lvl, GLint dx, GLint dy, GLint x, GLint y, GLsizei w, GLsizei h) {
    GLC();
    gl_tex_t *t = tex_for_image(C, target);
    int face = face_of(target);
    if (!sub_ok(C, t, lvl, dx, dy, face < 0 ? 0 : face, w, h, 1)) return;
    copy_fb(C, t, lvl, dx, dy, face < 0 ? 0 : face, x, y, w, h);
    gen_mips_if(C, t, lvl);
}
void glCopyTexSubImage1D(GLenum target, GLint lvl, GLint dx, GLint x, GLint y, GLsizei w) {
    GLC();
    gl_tex_t *t = tex_for_image(C, target);
    if (!sub_ok(C, t, lvl, dx, 0, 0, w, 1, 1)) return;
    copy_fb(C, t, lvl, dx, 0, 0, x, y, w, 1);
}
void glCopyTexSubImage3D(GLenum target, GLint lvl, GLint dx, GLint dy, GLint dz, GLint x, GLint y, GLsizei w, GLsizei h) {
    GLC();
    gl_tex_t *t = tex_for_image(C, target);
    if (!sub_ok(C, t, lvl, dx, dy, dz, w, h, 1)) return;
    copy_fb(C, t, lvl, dx, dy, dz, x, y, w, h);
}

/* CGLTexImageIOSurface2D: o plano do IOSurface vira o nível 0 da textura ligada ao alvo (cópia; a linha 0 da
 * memória é t = 0, como no macOS) */
int gl_tex_from_surface(gl_ctx_t *C, u32 target, u32 ifmt, int w, int h, u32 fmt, u32 type, const u8 *base, u64 bpr) {
    if (!C) return 0;
    gl_ctx_make_current(C);
    gl_tex_t *t = tex_for_image(C, target);
    if (!t || !t->name) return 0;
    if (!fmt_type_ok(C, fmt, type)) return 0;
    gl_level_t *L = level_alloc(C, t, 0, ifmt, w, h, 1);
    if (!L) return 0;
    int sr = C->up_row, sa = C->up_align, ssr = C->up_skrow, ssp = C->up_skpix, sih = C->up_imgh, ssi = C->up_skimg;
    gl_buf_t *pb = C->b_unpack;
    i64 pbpp = px_bytes(fmt, type);
    C->up_row = pbpp ? (int)(bpr / (u64)pbpp) : 0;
    C->up_align = 1;
    C->up_skrow = C->up_skpix = C->up_imgh = C->up_skimg = 0;
    C->b_unpack = 0;
    unpack_image(C, w, h, 1, fmt, type, base, L->fmt, gl_ifmt_base(ifmt), L->data);
    C->up_row = sr; C->up_align = sa; C->up_skrow = ssr; C->up_skpix = ssp; C->up_imgh = sih; C->up_skimg = ssi;
    C->b_unpack = pb;
    t->dirty = 1;
    return 1;
}

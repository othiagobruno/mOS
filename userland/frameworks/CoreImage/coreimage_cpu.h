/*
 * mOS — pixels do CoreImage (CPU).
 * px é 0xAARRGGBB, alfa reto, o mesmo layout do ImageIO e do CGImage do mOS.
 * A origem do extent é em pixels, eixo y para cima, como no Core Image.
 * MIT.
 */
#ifndef MOS_COREIMAGE_CPU_H
#define MOS_COREIMAGE_CPU_H
#include <stddef.h>
#include <stdint.h>

typedef struct mos_ci mos_ci;

/* Códigos dos kCIFormat* exportados pelo framework. O app liga o símbolo;
 * estes números são os que o render desta versão aceita. */
enum {
    MOS_CI_ARGB8 = 0,
    MOS_CI_BGRA8 = 1,
    MOS_CI_RGBA8 = 2,
    MOS_CI_ABGR8 = 3,
    MOS_CI_RGBAh = 4                /* 4 × half float (binary16), R G B A, pré-multiplicado */
};

mos_ci *mos_ci_bitmap(const uint32_t *px, int w, int h, double ox, double oy);
mos_ci *mos_ci_crop(mos_ci *im, double x, double y, double w, double h);
mos_ci *mos_ci_affine(mos_ci *im, double a, double b, double c, double d, double tx, double ty);
mos_ci *mos_ci_blur(mos_ci *im, double radius);
mos_ci *mos_ci_color(mos_ci *im, double saturation, double brightness, double contrast);
mos_ci *mos_ci_over(mos_ci *fg, mos_ci *bg);
mos_ci *mos_ci_lanczos(mos_ci *im, double scale, double aspect);
void mos_ci_retain(mos_ci *im);
void mos_ci_release(mos_ci *im);

/* Operações por pixel (mos_ci_pixop). Parâmetros:
 * MATRIX: 20 (R, G, B, A e bias, 4 cada); EXPOSURE: ev; MONOCHROME: r, g, b, intensidade; SEPIA: intensidade;
 * HUE: ângulo (rad); GAMMA: potência; CLAMP_COMPONENTS: mín (4), máx (4); POSTERIZE: níveis; VIBRANCE: quantidade;
 * FALSE_COLOR: cor0 (4), cor1 (4); COLOR_CONTROLS: saturação, brilho, contraste. */
enum {
    MOS_CI_OP_MATRIX = 1, MOS_CI_OP_EXPOSURE, MOS_CI_OP_INVERT, MOS_CI_OP_MONOCHROME, MOS_CI_OP_SEPIA, MOS_CI_OP_HUE,
    MOS_CI_OP_GAMMA, MOS_CI_OP_CLAMP_COMPONENTS, MOS_CI_OP_POSTERIZE, MOS_CI_OP_MASK_TO_ALPHA, MOS_CI_OP_VIBRANCE,
    MOS_CI_OP_MIN_COMPONENT, MOS_CI_OP_MAX_COMPONENT, MOS_CI_OP_FALSE_COLOR, MOS_CI_OP_COLOR_CONTROLS, MOS_CI_OP_PHOTO_MONO
};
/* Misturas (mos_ci_blend): fg = inputImage, bg = inputBackgroundImage. Separáveis do W3C sobre source-over, Porter-Duff,
 * máscaras (mask = inputMaskImage) e o realce (UNSHARP/SHARPEN_LUMA: bg = a imagem desfocada, param = intensidade). */
enum {
    MOS_CI_BL_MULTIPLY = 1, MOS_CI_BL_SCREEN, MOS_CI_BL_OVERLAY, MOS_CI_BL_HARD_LIGHT, MOS_CI_BL_SOFT_LIGHT, MOS_CI_BL_DARKEN,
    MOS_CI_BL_LIGHTEN, MOS_CI_BL_DIFFERENCE, MOS_CI_BL_EXCLUSION, MOS_CI_BL_COLOR_DODGE, MOS_CI_BL_COLOR_BURN,
    MOS_CI_BL_SOURCE_IN, MOS_CI_BL_SOURCE_OUT, MOS_CI_BL_SOURCE_ATOP, MOS_CI_BL_ADDITION, MOS_CI_BL_MULTIPLY_COMPOSITING,
    MOS_CI_BL_MINIMUM, MOS_CI_BL_MAXIMUM, MOS_CI_BL_MASK, MOS_CI_BL_ALPHA_MASK, MOS_CI_BL_UNSHARP, MOS_CI_BL_SHARPEN_LUMA
};
/* Geradores (extent infinito). CONSTANT: cor (4); LINEAR/SMOOTH_LINEAR: p0 (2), p1 (2), c0 (4), c1 (4);
 * RADIAL: centro (2), r0, r1, c0 (4), c1 (4); CHECKER: centro (2), c0 (4), c1 (4), largura, nitidez. */
enum { MOS_CI_GEN_CONSTANT = 1, MOS_CI_GEN_LINEAR, MOS_CI_GEN_SMOOTH_LINEAR, MOS_CI_GEN_RADIAL, MOS_CI_GEN_CHECKER };
mos_ci *mos_ci_pixop(mos_ci *im, int op, const double *p, int np);
mos_ci *mos_ci_blend(mos_ci *fg, mos_ci *bg, mos_ci *mask, int mode, double param);
mos_ci *mos_ci_generator(int kind, const double *p, int np);
mos_ci *mos_ci_clamp(mos_ci *im);                 /* imageByClampingToExtent: borda repetida, extent infinito */
mos_ci *mos_ci_box_blur(mos_ci *im, double radius);
mos_ci *mos_ci_pixellate(mos_ci *im, double cx, double cy, double scale);
int mos_ci_is_infinite(const mos_ci *im);

/* 1 se o extent existe; o infinito sai como CGRectInfinite. */
int mos_ci_extent(const mos_ci *im, double *x, double *y, double *w, double *h);

/* Rasteriza o retângulo inteiro [x,x+w) × [y,y+h) em px (w*h, 0xAARRGGBB). 0 = ok. */
int mos_ci_render(mos_ci *im, double x, double y, int w, int h, uint32_t *px);

/* Escreve o mesmo retângulo no formato MOS_CI_*. 0 = ok. */
int mos_ci_render_format(mos_ci *im, double x, double y, int w, int h, int format, void *bytes, long row_bytes);

/* PNG ou JPEG por mos_img_decode, extern do libSystem (imageio.c). NULL se não for esses tipos. */
mos_ci *mos_ci_from_encoded(const uint8_t *data, size_t n);

/* CIContext. cache=0 recalcula sempre. Um espaço de cor estrangeiro não é convertido. */
typedef struct {
    int cache;
    mos_ci *im;
    double x, y;
    int w, h;
    uint32_t *px;
    int renders;
} mos_ci_ctx;
void mos_ci_ctx_init(mos_ci_ctx *c, int cache);
int mos_ci_ctx_render(mos_ci_ctx *c, mos_ci *im, double x, double y, int w, int h, uint32_t *px);
int mos_ci_ctx_renders(const mos_ci_ctx *c);
void mos_ci_ctx_free(mos_ci_ctx *c);
int mos_ci_colorspace_ok(int foreign_space);

/* O mesmo despacho de -[CIFilter outputImage]. crop=1 usa tx,ty,a,b como retângulo. */
mos_ci *mos_ci_apply(const char *name, mos_ci *input, mos_ci *background,
                     double radius, double saturation, double brightness, double contrast,
                     double scale, double aspect,
                     double a, double b, double c, double d, double tx, double ty, int crop);

#endif

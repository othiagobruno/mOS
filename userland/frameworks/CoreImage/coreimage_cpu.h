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
    MOS_CI_ABGR8 = 3
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

/* 1 se o extent é finito. */
int mos_ci_extent(const mos_ci *im, double *x, double *y, double *w, double *h);

/* Rasteriza o retângulo inteiro [x,x+w) × [y,y+h) em px (w*h, 0xAARRGGBB). 0 = ok. */
int mos_ci_render(mos_ci *im, double x, double y, int w, int h, uint32_t *px);

/* Escreve o mesmo retângulo no formato MOS_CI_*. 0 = ok. */
int mos_ci_render_format(mos_ci *im, double x, double y, int w, int h, int format, void *bytes, long row_bytes);

/* PNG ou JPEG por mos_img_decode, extern do libSystem (grok_imageio.c). NULL se não for esses tipos. */
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

/* Rasterizador do CGBitmapContext (coregraphics.c), sem runtime: polígonos com cobertura
 * (nonzero e even-odd), traço convertido em polígonos e gradientes. tools/cg_raster_test.c
 * chama as mesmas funções. */
#ifndef MOS_CG_RASTER_H
#define MOS_CG_RASTER_H
#include <stdint.h>

typedef struct { double x, y; } cgr_pt;

/* Contornos: pts em sequência; sub[i] é o índice do primeiro ponto do contorno i, closed[i] se fecha. */
typedef struct {
    cgr_pt *pts;
    int n, cap;
    int *sub;
    uint8_t *closed;
    int nsub, subcap;
} cgr_poly;

void cgr_init(cgr_poly *p);
void cgr_free(cgr_poly *p);
void cgr_clear(cgr_poly *p);
void cgr_move(cgr_poly *p, double x, double y);
void cgr_line(cgr_poly *p, double x, double y);
void cgr_close(cgr_poly *p);
/* curvas a partir do ponto atual, achatadas com erro máximo tol (mesma unidade dos pontos) */
void cgr_quad(cgr_poly *p, double cx, double cy, double x, double y, double tol);
void cgr_cubic(cgr_poly *p, double c1x, double c1y, double c2x, double c2y, double x, double y, double tol);
/* arco de círculo do ângulo a0 a a1 (radianos), sentido horário se cw; liga do ponto atual com reta */
void cgr_arc(cgr_poly *p, double cx, double cy, double r, double a0, double a1, int cw, double tol);
/* aplica x' = a*x + c*y + tx, y' = b*x + d*y + ty */
void cgr_transform(cgr_poly *p, double a, double b, double c, double d, double tx, double ty);
int cgr_bbox(const cgr_poly *p, double *x0, double *y0, double *x1, double *y1);

/* Cobertura por linha: emit(ctx, y, x0, x1, cov) com cov[0..x1-x0) de 0 a 255.
 * Só o retângulo [cx0,cx1) × [cy0,cy1) é varrido. evenodd = 0 é a regra nonzero. */
typedef void (*cgr_emit_fn)(void *ctx, int y, int x0, int x1, const uint8_t *cov);
int cgr_fill(const cgr_poly *p, int evenodd, int cx0, int cy0, int cx1, int cy1, cgr_emit_fn emit, void *ctx);

/* Traço: cap 0 butt, 1 round, 2 square; join 0 miter, 1 round, 2 bevel. dash com ndash > 0 alterna
 * traço/espaço a partir de phase. out recebe polígonos de mesma orientação, para preencher com nonzero. */
typedef struct {
    double width, miter, phase;
    const double *dash;
    int ndash, cap, join;
    double tol;
} cgr_stroke_t;
void cgr_stroke(const cgr_poly *in, const cgr_stroke_t *st, cgr_poly *out);
/* só o tracejado: contornos abertos com os pedaços de traço */
void cgr_dash(const cgr_poly *in, const double *dash, int ndash, double phase, cgr_poly *out);

/* Gradiente: n paradas com locais crescentes e cor RGBA não pré-multiplicada (0..1). */
typedef struct { int n; const double *loc; const double *rgba; } cgr_grad;
/* cor pré-multiplicada em ARGB 8888 no ponto t (0..1, já limitado) */
uint32_t cgr_grad_at(const cgr_grad *g, double t);
/* t do gradiente linear de s a e; 0 se fora (flags: 1 antes do início, 2 depois do fim) */
int cgr_linear_t(double px, double py, double sx, double sy, double ex, double ey, int flags, double *t);
/* t do gradiente radial entre (c0, r0) e (c1, r1), o maior t com raio ≥ 0 */
int cgr_radial_t(double px, double py, double c0x, double c0y, double r0, double c1x, double c1y, double r1, int flags, double *t);

#endif

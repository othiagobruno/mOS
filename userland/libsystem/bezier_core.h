/* mOS — geometria de caminhos (NSBezierPath e o path de janela do CGContext em appkit.c), sem libc.
 * Elementos como no Cocoa (move, line, curve cúbica, close); achatamento adaptativo; preenchimento não-zero ou par-ímpar
 * num único polígono para o window server (que só tem a regra não-zero); contenção de ponto; caixas; transformação
 * afim; inversão; arcos; tracejado. O alocador vem de quem usa (bz_alloc/bz_free).
 * Teste de host: userland/tests-appkit/bezier_host.c. */
#ifndef BEZIER_CORE_H
#define BEZIER_CORE_H

enum { BZ_MOVE = 0, BZ_LINE = 1, BZ_CURVE = 2, BZ_CLOSE = 3 };
typedef struct { int kind; double p[6]; } bz_el;          /* curve: c1, c2, fim; move/line: p[0..1] */
typedef struct { bz_el *el; int n, cap; } bz_path;
typedef struct { double x, y, w, h; } bz_rect;

void *bz_alloc(unsigned long n);                            /* fornecidos por quem usa */
void *bz_realloc(void *p, unsigned long n);
void bz_free(void *p);

void bz_clear(bz_path *p);
void bz_release(bz_path *p);
int bz_copy(bz_path *dst, const bz_path *src);
int bz_move(bz_path *p, double x, double y);
int bz_line(bz_path *p, double x, double y);              /* sem ponto atual: vira move */
int bz_curve(bz_path *p, double c1x, double c1y, double c2x, double c2y, double x, double y);
int bz_close(bz_path *p);
int bz_current(const bz_path *p, double *x, double *y);   /* 0 se vazio */
int bz_append(bz_path *p, const bz_path *q);
void bz_rect_path(bz_path *p, bz_rect r);
void bz_oval(bz_path *p, bz_rect r);
void bz_rrect(bz_path *p, bz_rect r, double rx, double ry);
/* arco de círculo (graus, como o NSBezierPath): liga ao ponto atual com uma reta, ou começa nele */
void bz_arc(bz_path *p, double cx, double cy, double r, double a0, double a1, int clockwise);
/* arco tangente às retas atual→p1 e p1→p2 (appendBezierPathWithArcFromPoint:toPoint:radius:) */
void bz_arc_to(bz_path *p, double x1, double y1, double x2, double y2, double r);
void bz_transform(bz_path *p, const double m[6]);         /* x' = a x + c y + tx, y' = b x + d y + ty: m = a b c d tx ty */
int bz_reverse(bz_path *dst, const bz_path *src);
bz_rect bz_control_bounds(const bz_path *p);
bz_rect bz_bounds(const bz_path *p);                      /* da curva achatada */

/* achatamento: anéis de pontos (x,y em double); ring_start[i] e ring_closed[i]. Devolve o número de anéis ou -1 */
typedef struct { double *xy; int n, cap; int *start; unsigned char *closed; int nr, rcap; } bz_flat;
int bz_flatten(const bz_path *p, double tol, bz_flat *out);
void bz_flat_release(bz_flat *f);
int bz_contains(const bz_path *p, double x, double y, int evenodd);
/* um só polígono para preencher com a regra não-zero: anéis orientados conforme a regra pedida e ligados por pontes
 * de ida e volta. out recebe pares float; devolve o número de pontos (ou -1) */
int bz_fill_polygon(const bz_flat *f, int evenodd, float **out);
/* tracejado: corta os anéis em pedaços; para cada pedaço chama emit(ctx, pontos, n) */
void bz_dash(const bz_flat *f, const double *pattern, int count, double phase,
             void (*emit)(void *ctx, const double *xy, int n), void *ctx);
#endif

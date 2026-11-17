/* mOS — NSGradient sem libc: cores com posições, interpolação e as faixas que o desenho preenche.
 * Linear: faixas perpendiculares ao eixo, recortadas pela forma (qualquer polígono: o recorte é feito com a faixa,
 * que é convexa, como janela). Radial: anéis entre dois círculos. Teste: userland/tests-appkit/gradient_host.c. */
#ifndef GRADIENT_CORE_H
#define GRADIENT_CORE_H

typedef struct { double r, g, b, a; } gc_color;
typedef struct { int n; gc_color c[64]; double loc[64]; } gc_stops;

/* ordena por posição e limita a [0,1]; devolve o número de paradas */
int gc_normalize(gc_stops *s);
gc_color gc_color_at(const gc_stops *s, double t);
/* recorta o polígono (subject, n pontos) pela janela convexa (clip, m pontos); out com até cap pontos */
int gc_clip(const double *subject, int n, const double *clip, int m, double *out, int cap);
/* faixa k de steps entre p0 e p1 (eixo), larga o bastante para cobrir a caixa bbox (x, y, w, h): 4 pontos */
void gc_band(double x0, double y0, double x1, double y1, int k, int steps, const double bbox[4], double quad[8]);
#endif

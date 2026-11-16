/* mOS libSystem — simulação das partículas do CAEmitterLayer, sem runtime (o desenho fica no quartzcore.c).
 * Teste de host: tools/caemit_host.c.
 *
 * Por célula: birthRate * birthRate do layer partículas por segundo; vida lifetime ± lifetimeRange (vezes a do layer);
 * velocidade velocity ± velocityRange (vezes a do layer) na direção emissionLongitude ± emissionRange/2 (radianos, 0 =
 * +x, y para cima como no macOS); aceleração x/y; escala scale ± scaleRange com scaleSpeed por segundo; giro spin ±
 * spinRange (vezes o do layer); cor color ± range por canal com speed por segundo. Nascimento no emitterShape
 * (ponto, linha, retângulo, círculo; cubo e esfera como retângulo e círculo no plano) segundo o emitterMode (points:
 * nos vértices; outline: no contorno; surface/volume: na área). */
#pragma once

enum { CAEMIT_POINT = 0, CAEMIT_LINE, CAEMIT_RECT, CAEMIT_CIRCLE };
enum { CAEMIT_POINTS = 0, CAEMIT_OUTLINE, CAEMIT_SURFACE };

typedef struct {
    double birth, life, lifer;
    double lon, range;
    double vel, velr, ax, ay;
    double scale, scaler, scales;
    double spin, spinr;
    double col[4], colr[4], cols[4];   /* r g b a */
} caemit_cell_t;
typedef struct {
    int shape, mode;
    double px, py, w, h;               /* emitterPosition e emitterSize */
    double birth, life, vel, scale, spin;   /* multiplicadores do layer */
} caemit_layer_t;
typedef struct { float x, y, vx, vy, age, life, scale, rot, spin, c[4], cs[4], ss; int cell; } caemit_p_t;
#define CAEMIT_MAX_CELLS 32
#define CAEMIT_MAX 20000
typedef struct {
    caemit_p_t *p;
    int n, cap;
    double acc[CAEMIT_MAX_CELLS];
    unsigned long long rng;
} caemit_t;

void caemit_seed(caemit_t *s, unsigned seed);
void caemit_step(caemit_t *s, const caemit_layer_t *L, const caemit_cell_t *c, int nc, double dt);
void caemit_free(caemit_t *s);

/* mOS — teste de host da simulação de partículas do CAEmitterLayer (userland/libsystem/caemit.c).
 *   gcc -O1 -I userland/libsystem tools/caemit_host.c userland/libsystem/caemit.c -lm -o /tmp/caemit && /tmp/caemit */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "caemit.h"
static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static caemit_cell_t cell(void) {
    caemit_cell_t c;
    memset(&c, 0, sizeof c);
    c.birth = 10; c.life = 1; c.vel = 100; c.scale = 1;
    c.col[0] = c.col[1] = c.col[2] = c.col[3] = 1;
    return c;
}
static caemit_layer_t layer(void) {
    caemit_layer_t L;
    memset(&L, 0, sizeof L);
    L.px = 50; L.py = 20; L.birth = L.life = L.vel = L.scale = L.spin = 1;
    return L;
}

int main(void) {
    caemit_t s;
    memset(&s, 0, sizeof s);
    caemit_seed(&s, 7);
    caemit_cell_t c = cell();
    caemit_layer_t L = layer();
    /* 10 por segundo, vida de 1 s: em regime, 10 vivas */
    for (int i = 0; i < 50; i++) caemit_step(&s, &L, &c, 1, 0.1);
    CHECK(s.n >= 10 && s.n <= 11, "regime: 10 partículas vivas, mais a que morre neste passo (%d)", s.n);
    /* direção 0 rad, velocidade 100: x = 50 + 100 * idade, y parado */
    int okm = 1;
    for (int i = 0; i < s.n; i++) if (fabs(s.p[i].x - (50 + 100 * s.p[i].age)) > 1e-3 || fabs(s.p[i].y - 20) > 1e-3) okm = 0;
    CHECK(okm, "movimento retilíneo a 100 pontos/s");
    /* aceleração em y (para cima) e faixa de vida */
    caemit_free(&s);
    caemit_seed(&s, 7);
    c.ay = -50; c.lifer = 0.5; c.vel = 0;
    for (int i = 0; i < 10; i++) caemit_step(&s, &L, &c, 1, 0.1);
    int oka = 1, okl = 1;
    for (int i = 0; i < s.n; i++) {
        /* integração semi-implícita por passo: v depois x */
        double t = s.p[i].age, n = round(t / 0.1), want = 20 - 50 * 0.1 * 0.1 * n * (n + 1) / 2;
        if (fabs(s.p[i].y - want) > 1e-2) oka = 0;
        if (s.p[i].life < 0.5 - 1e-6 || s.p[i].life > 1.5 + 1e-6) okl = 0;
    }
    CHECK(oka && s.n > 0, "yAcceleration puxa para baixo (%d partículas)", s.n);
    CHECK(okl, "lifetime ± lifetimeRange");
    /* cone de emissão: longitude pi/2 (para cima), faixa pi/2 -> ângulo dentro de [pi/4, 3pi/4] */
    caemit_free(&s);
    caemit_seed(&s, 3);
    c = cell();
    c.lon = M_PI / 2; c.range = M_PI / 2; c.birth = 200;
    caemit_step(&s, &L, &c, 1, 0.5);
    int okc = s.n == 100;
    for (int i = 0; i < s.n; i++) { double a = atan2(s.p[i].vy, s.p[i].vx); if (a < M_PI / 4 - 1e-6 || a > 3 * M_PI / 4 + 1e-6) okc = 0; }
    CHECK(okc, "emissionLongitude ± emissionRange/2 (%d)", s.n);
    /* formas: linha de 40 (x em 30..70), contorno de retângulo, área de círculo, pontos de linha */
    caemit_free(&s);
    caemit_seed(&s, 5);
    L.shape = CAEMIT_LINE; L.mode = CAEMIT_SURFACE; L.w = 40;
    c = cell(); c.vel = 0; c.birth = 1000;
    caemit_step(&s, &L, &c, 1, 0.1);
    int okline = s.n == 100;
    double mn = 1e9, mx = -1e9;
    for (int i = 0; i < s.n; i++) { if (s.p[i].x < mn) mn = s.p[i].x; if (s.p[i].x > mx) mx = s.p[i].x; if (s.p[i].x < 30 || s.p[i].x > 70 || s.p[i].y != 20) okline = 0; }
    CHECK(okline && mx - mn > 30, "linha: nascimento ao longo de 40 pontos (%g..%g)", mn, mx);
    caemit_free(&s);
    caemit_seed(&s, 5);
    L.shape = CAEMIT_RECT; L.mode = CAEMIT_OUTLINE; L.w = 40; L.h = 20;
    caemit_step(&s, &L, &c, 1, 0.1);
    int okr = s.n == 100;
    for (int i = 0; i < s.n; i++) {
        double dx = fabs(s.p[i].x - 50), dy = fabs(s.p[i].y - 20);
        if (!((fabs(dx - 20) < 1e-3 && dy <= 10 + 1e-3) || (fabs(dy - 10) < 1e-3 && dx <= 20 + 1e-3))) okr = 0;
    }
    CHECK(okr, "retângulo outline: só no contorno");
    caemit_free(&s);
    caemit_seed(&s, 5);
    L.shape = CAEMIT_CIRCLE; L.mode = CAEMIT_SURFACE; L.w = 40;
    caemit_step(&s, &L, &c, 1, 0.1);
    int okci = s.n == 100;
    for (int i = 0; i < s.n; i++) if (hypot(s.p[i].x - 50, s.p[i].y - 20) > 20 + 1e-3) okci = 0;
    CHECK(okci, "círculo surface: dentro do raio 20");
    /* cor com velocidade e escala com scaleSpeed; multiplicadores do layer */
    caemit_free(&s);
    caemit_seed(&s, 9);
    L = layer();
    c = cell(); c.birth = 10; c.cols[3] = -0.5; c.scales = 2; c.spin = 1;
    L.birth = 2; L.spin = 3;
    caemit_step(&s, &L, &c, 1, 0.1);   /* nascem 2 */
    CHECK(s.n == 2, "birthRate do layer multiplica (%d)", s.n);
    caemit_step(&s, &L, &c, 1, 0.4);
    int okcol = 1;
    for (int i = 0; i < s.n; i++) if (s.p[i].age > 0.39 && (fabs(s.p[i].c[3] - 0.8) > 1e-5 || fabs(s.p[i].scale - 1.8) > 1e-5 || fabs(s.p[i].rot - 1.2) > 1e-5)) okcol = 0;
    CHECK(okcol, "alphaSpeed, scaleSpeed e spin * spin do layer");
    /* determinístico pela semente */
    caemit_t a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    caemit_seed(&a, 42); caemit_seed(&b, 42);
    c = cell(); c.velr = 50; c.range = 1;
    for (int i = 0; i < 5; i++) { caemit_step(&a, &L, &c, 1, 0.1); caemit_step(&b, &L, &c, 1, 0.1); }
    CHECK(a.n == b.n && a.n > 0 && !memcmp(a.p, b.p, sizeof *a.p * (size_t)a.n), "mesma semente, mesmas partículas");
    caemit_free(&a); caemit_free(&b); caemit_free(&s);
    printf("caemit: %d/%d ok\n", pass, total);
    return pass != total;
}

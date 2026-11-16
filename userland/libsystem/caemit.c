/* mOS libSystem — simulação das partículas do CAEmitterLayer: ver caemit.h. */
#include "caemit.h"

void *realloc(void *p, unsigned long n);
void free(void *p);
double sin(double), cos(double), sqrt(double);

static double rnd01(caemit_t *s) {       /* xorshift64*: determinístico pela semente do layer */
    unsigned long long x = s->rng ? s->rng : 0x9E3779B97F4A7C15ull;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    s->rng = x;
    return (double)((x * 2685821657736338717ull) >> 11) / 9007199254740992.0;
}
static double rndpm(caemit_t *s) { return rnd01(s) * 2 - 1; }   /* -1..1 */
void caemit_seed(caemit_t *s, unsigned seed) { s->rng = 0x9E3779B97F4A7C15ull ^ ((unsigned long long)seed * 0x100000001B3ull); }
static double cl01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* ponto de nascimento na forma do emissor */
static void spawn_pos(caemit_t *s, const caemit_layer_t *L, double *x, double *y) {
    double hw = L->w / 2, hh = L->h / 2;
    *x = L->px; *y = L->py;
    switch (L->shape) {
    case CAEMIT_LINE:
        if (L->mode == CAEMIT_POINTS) *x += rnd01(s) < 0.5 ? -hw : hw;
        else *x += rndpm(s) * hw;
        break;
    case CAEMIT_RECT:
        if (L->mode == CAEMIT_POINTS) { *x += rnd01(s) < 0.5 ? -hw : hw; *y += rnd01(s) < 0.5 ? -hh : hh; }
        else if (L->mode == CAEMIT_OUTLINE) {
            double per = 2 * (L->w + L->h), t = rnd01(s) * (per > 0 ? per : 1);
            if (t < L->w) { *x += -hw + t; *y -= hh; }
            else if ((t -= L->w) < L->h) { *x += hw; *y += -hh + t; }
            else if ((t -= L->h) < L->w) { *x += hw - t; *y += hh; }
            else { t -= L->w; *x -= hw; *y += hh - t; }
        } else { *x += rndpm(s) * hw; *y += rndpm(s) * hh; }
        break;
    case CAEMIT_CIRCLE: {
        if (L->mode == CAEMIT_POINTS) break;    /* o centro */
        double a = rnd01(s) * 6.283185307179586, r = hw;
        if (L->mode == CAEMIT_SURFACE) r = hw * sqrt(rnd01(s));   /* uniforme na área */
        *x += cos(a) * r; *y += sin(a) * r;
        break;
    }
    default: break;
    }
}

void caemit_step(caemit_t *s, const caemit_layer_t *L, const caemit_cell_t *c, int nc, double dt) {
    if (dt < 0) dt = 0;
    if (dt > 1) dt = 1;   /* depois de uma pausa longa não nasce uma rajada */
    if (nc > CAEMIT_MAX_CELLS) nc = CAEMIT_MAX_CELLS;
    /* envelhece, move e tira as mortas */
    int w = 0;
    for (int i = 0; i < s->n; i++) {
        caemit_p_t p = s->p[i];
        p.age += (float)dt;
        if (p.age >= p.life || p.cell >= nc) continue;
        const caemit_cell_t *k = &c[p.cell];
        p.vx += (float)(k->ax * dt);
        p.vy += (float)(k->ay * dt);
        p.x += (float)(p.vx * dt);
        p.y += (float)(p.vy * dt);
        p.scale += (float)(p.ss * dt);
        if (p.scale < 0) p.scale = 0;
        p.rot += (float)(p.spin * dt);
        for (int j = 0; j < 4; j++) p.c[j] = (float)cl01(p.c[j] + p.cs[j] * dt);
        s->p[w++] = p;
    }
    s->n = w;
    /* nascimentos */
    for (int ci = 0; ci < nc; ci++) {
        const caemit_cell_t *k = &c[ci];
        double rate = k->birth * L->birth;
        if (rate <= 0) { s->acc[ci] = 0; continue; }
        s->acc[ci] += rate * dt;
        while (s->acc[ci] >= 1) {
            s->acc[ci] -= 1;
            if (s->n >= CAEMIT_MAX) continue;
            if (s->n == s->cap) {
                int nc2 = s->cap ? s->cap * 2 : 256;
                caemit_p_t *np = realloc(s->p, sizeof *np * (unsigned long)nc2);
                if (!np) return;
                s->p = np; s->cap = nc2;
            }
            caemit_p_t p;
            double x, y;
            spawn_pos(s, L, &x, &y);
            double life = (k->life + rndpm(s) * k->lifer) * L->life;
            if (life <= 0) continue;
            double v = (k->vel + rndpm(s) * k->velr) * L->vel, a = k->lon + rndpm(s) * k->range / 2;
            p.x = (float)x; p.y = (float)y;
            p.vx = (float)(cos(a) * v); p.vy = (float)(sin(a) * v);
            p.age = 0;
            p.life = (float)life;
            p.scale = (float)((k->scale + rndpm(s) * k->scaler) * L->scale);
            if (p.scale < 0) p.scale = 0;
            p.ss = (float)k->scales;
            p.rot = 0;
            p.spin = (float)((k->spin + rndpm(s) * k->spinr) * L->spin);
            for (int j = 0; j < 4; j++) { p.c[j] = (float)cl01(k->col[j] + rndpm(s) * k->colr[j]); p.cs[j] = (float)k->cols[j]; }
            p.cell = ci;
            s->p[s->n++] = p;
        }
    }
}
void caemit_free(caemit_t *s) { free(s->p); s->p = 0; s->n = s->cap = 0; }

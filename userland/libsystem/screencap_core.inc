/*
 * Quadro da captura contínua (SCStream e CGDisplayStream).
 * O intervalo 0 vale 16667 µs (60 Hz). Sem permissão a leitura não acontece:
 * cx_cap_allowed é o cx_tcc_preflight no libSystem, e o teste de host troca os dois ganchos.
 */
#ifndef CX_CAP_CORE
#define CX_CAP_CORE

typedef struct {
    long long interval_us;
    long long next_us;
    int w, h;
    unsigned *px;
    int cap;
} cx_cap_t;

int cx_cap_allowed(void);
int cx_cap_read(int x, int y, int w, int h, unsigned *dst, int dst_px, int *ow, int *oh);

static long long cx_cap_interval(long long us) { return us > 0 ? us : 16667; }

/* 1 = quadro novo em c->px. 0 = intervalo ainda não venceu. -1 = sem permissão (o dispositivo não é lido). */
static int cx_cap_take(cx_cap_t *c, long long now_us, int x, int y, int w, int h) __attribute__((unused));
static int cx_cap_take(cx_cap_t *c, long long now_us, int x, int y, int w, int h) {
    if (!c || !c->px || c->cap <= 0) return -1;
    if (!cx_cap_allowed()) return -1;
    long long iv = cx_cap_interval(c->interval_us);
    if (c->next_us && now_us < c->next_us) return 0;
    int ow = 0, oh = 0;
    if (!cx_cap_read(x, y, w, h, c->px, c->cap, &ow, &oh) || ow <= 0 || oh <= 0) return 0;
    c->w = ow;
    c->h = oh;
    c->next_us = now_us + iv;
    return 1;
}
#endif

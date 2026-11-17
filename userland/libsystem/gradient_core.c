/* mOS — núcleo do NSGradient (ver gradient_core.h). Código original do mOS. */
#include "gradient_core.h"

int gc_normalize(gc_stops *s) {
    if (s->n > 64) s->n = 64;
    for (int i = 0; i < s->n; i++) { if (s->loc[i] < 0) s->loc[i] = 0; if (s->loc[i] > 1) s->loc[i] = 1; }
    for (int i = 1; i < s->n; i++)   /* inserção: estável para posições iguais */
        for (int j = i; j > 0 && s->loc[j - 1] > s->loc[j]; j--) {
            double t = s->loc[j]; s->loc[j] = s->loc[j - 1]; s->loc[j - 1] = t;
            gc_color c = s->c[j]; s->c[j] = s->c[j - 1]; s->c[j - 1] = c;
        }
    return s->n;
}
gc_color gc_color_at(const gc_stops *s, double t) {
    gc_color z = {0, 0, 0, 0};
    if (s->n <= 0) return z;
    if (t <= s->loc[0]) return s->c[0];
    if (t >= s->loc[s->n - 1]) return s->c[s->n - 1];
    for (int i = 1; i < s->n; i++)
        if (t <= s->loc[i]) {
            double span = s->loc[i] - s->loc[i - 1], f = span > 0 ? (t - s->loc[i - 1]) / span : 1;
            gc_color a = s->c[i - 1], b = s->c[i];
            gc_color r = {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f, a.a + (b.a - a.a) * f};
            return r;
        }
    return s->c[s->n - 1];
}
/* Sutherland–Hodgman: a janela tem que ser convexa (a faixa é); o polígono recortado pode ser qualquer um */
int gc_clip(const double *subject, int n, const double *clip, int m, double *out, int cap) {
    double buf_a[2 * 1024], buf_b[2 * 1024];
    if (n > 1024 || cap < 3) return 0;
    for (int i = 0; i < n * 2; i++) buf_a[i] = subject[i];
    double *in = buf_a, *o = buf_b;
    int cnt = n;
    /* orientação da janela: os pontos "dentro" ficam do lado do centro */
    double area = 0;
    for (int i = 0; i < m; i++) { int j = (i + 1) % m; area += clip[i * 2] * clip[j * 2 + 1] - clip[j * 2] * clip[i * 2 + 1]; }
    double sgn = area >= 0 ? 1 : -1;
    for (int e = 0; e < m && cnt > 0; e++) {
        double ax = clip[e * 2], ay = clip[e * 2 + 1], bx = clip[((e + 1) % m) * 2], by = clip[((e + 1) % m) * 2 + 1];
        int k = 0;
        for (int i = 0; i < cnt; i++) {
            double px = in[i * 2], py = in[i * 2 + 1], qx = in[((i + 1) % cnt) * 2], qy = in[((i + 1) % cnt) * 2 + 1];
            double dp = sgn * ((bx - ax) * (py - ay) - (by - ay) * (px - ax)), dq = sgn * ((bx - ax) * (qy - ay) - (by - ay) * (qx - ax));
            int pin = dp >= 0, qin = dq >= 0;
            if (pin && k < 1023) { o[k * 2] = px; o[k * 2 + 1] = py; k++; }
            if (pin != qin && k < 1023) {
                double t = dp / (dp - dq);
                o[k * 2] = px + (qx - px) * t; o[k * 2 + 1] = py + (qy - py) * t; k++;
            }
        }
        double *tmp = in; in = o; o = tmp;
        cnt = k;
    }
    if (cnt > cap) cnt = cap;
    for (int i = 0; i < cnt * 2; i++) out[i] = in[i];
    return cnt;
}
void gc_band(double x0, double y0, double x1, double y1, int k, int steps, const double bbox[4], double quad[8]) {
    double dx = x1 - x0, dy = y1 - y0, len = __builtin_sqrt(dx * dx + dy * dy);
    if (len <= 0) { dx = 1; dy = 0; len = 1; }
    double ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    /* a faixa vai além do eixo nas pontas para cobrir o que fica antes de p0 e depois de p1 */
    double diag = __builtin_sqrt(bbox[2] * bbox[2] + bbox[3] * bbox[3]) + len + 2;
    double t0 = (double)k / steps * len, t1 = (double)(k + 1) / steps * len;
    if (k == 0) t0 -= diag;
    if (k == steps - 1) t1 += diag;
    double cx = bbox[0] + bbox[2] / 2, cy = bbox[1] + bbox[3] / 2;
    double off = (cx - x0) * nx + (cy - y0) * ny;   /* centra a faixa na caixa */
    double ax = x0 + nx * off, ay = y0 + ny * off;
    double p0x = ax + ux * t0, p0y = ay + uy * t0, p1x = ax + ux * t1, p1y = ay + uy * t1;
    quad[0] = p0x - nx * diag; quad[1] = p0y - ny * diag;
    quad[2] = p1x - nx * diag; quad[3] = p1y - ny * diag;
    quad[4] = p1x + nx * diag; quad[5] = p1y + ny * diag;
    quad[6] = p0x + nx * diag; quad[7] = p0y + ny * diag;
}

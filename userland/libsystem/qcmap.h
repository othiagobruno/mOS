/* mOS libSystem — matemática da composição do QuartzCore (quartzcore.c), sem runtime: o mapa de um layer para a tela
 * (homografia 3x3, que cobre perspectiva e rotação em x/y) e as instâncias do CAReplicatorLayer.
 * Teste de host: tools/qcmap_host.c.
 *
 * Cada layer é um plano; o Core Animation multiplica as matrizes 4x4 (convenção de vetor-linha: v' = v * M) e
 * achata no plano do pai. Para pontos (x, y, 0, 1) do plano, a 4x4 vira a 3x3 [[m11 m21 m41] [m12 m22 m42]
 * [m14 m24 m44]] em vetor-coluna, e a composição entre layers é produto de 3x3. */
#pragma once

/* sx = (ax + bx*lx + cx*ly) / W; sy = (ay + cy*lx + by*ly) / W; W = 1 + wd + wx*lx + wy*ly.
 * Com wx = wy = wd = 0 é afim (o caso comum; um map_t zerado nos três campos novos continua afim). */
typedef struct { double ax, bx, ay, by, cx, cy, wx, wy, wd; } map_t;

static inline void qc_to_mat(map_t m, double *h) {
    h[0] = m.bx; h[1] = m.cx; h[2] = m.ax;
    h[3] = m.cy; h[4] = m.by; h[5] = m.ay;
    h[6] = m.wx; h[7] = m.wy; h[8] = 1 + m.wd;
}
static inline map_t qc_from_mat(const double *h) {
    double s = h[8] != 0 ? h[8] : 1;   /* normaliza W = 1 em (0, 0) quando dá */
    map_t m = {h[2] / s, h[0] / s, h[5] / s, h[4] / s, h[1] / s, h[3] / s, h[6] / s, h[7] / s, 0};
    return m;
}
static inline void qc_mat_mul(double *r, const double *a, const double *b) {
    double o[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) o[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
    for (int i = 0; i < 9; i++) r[i] = o[i];
}
static inline int qc_mat_inv(double *r, const double *m) {
    double c0 = m[4] * m[8] - m[5] * m[7], c1 = m[5] * m[6] - m[3] * m[8], c2 = m[3] * m[7] - m[4] * m[6];
    double det = m[0] * c0 + m[1] * c1 + m[2] * c2;
    if (det < 1e-12 && det > -1e-12) return 0;
    double d = 1 / det;
    r[0] = c0 * d; r[1] = (m[2] * m[7] - m[1] * m[8]) * d; r[2] = (m[1] * m[5] - m[2] * m[4]) * d;
    r[3] = c1 * d; r[4] = (m[0] * m[8] - m[2] * m[6]) * d; r[5] = (m[2] * m[3] - m[0] * m[5]) * d;
    r[6] = c2 * d; r[7] = (m[1] * m[6] - m[0] * m[7]) * d; r[8] = (m[0] * m[4] - m[1] * m[3]) * d;
    return 1;
}
/* CATransform3D (16 doubles, m11..m44 linha a linha) restrita ao plano z = 0, como 3x3 de vetor-coluna */
static inline void qc_proj4(const double *t, double *h) {
    if (t[0] == 0 && t[1] == 0 && t[4] == 0 && t[5] == 0 && t[15] == 0) { h[0] = 1; h[1] = 0; h[2] = 0; h[3] = 0; h[4] = 1; h[5] = 0; h[6] = 0; h[7] = 0; h[8] = 1; return; }
    h[0] = t[0]; h[1] = t[4]; h[2] = t[12];
    h[3] = t[1]; h[4] = t[5]; h[5] = t[13];
    h[6] = t[3]; h[7] = t[7]; h[8] = t[15];
}
static inline int qc_is_affine(map_t m) { return m.wx == 0 && m.wy == 0 && m.wd == 0; }
static inline int qc_map_point(map_t m, double lx, double ly, double *sx, double *sy) {
    double w = 1 + m.wd + m.wx * lx + m.wy * ly;
    if (w <= 1e-9) return 0;          /* atrás do observador */
    *sx = (m.ax + m.bx * lx + m.cx * ly) / w;
    *sy = (m.ay + m.cy * lx + m.by * ly) / w;
    return 1;
}
/* composição com uma transformação (CATransform3D inteira, projetada no plano) aplicada no espaço local */
static inline map_t qc_apply(map_t p, const double *t) {
    double a[9], b[9];
    qc_to_mat(p, a);
    qc_proj4(t, b);
    qc_mat_mul(a, a, b);
    return qc_from_mat(a);
}
/* a mesma transformação em volta de um ponto (cx, cy) do espaço local (sublayerTransform em volta do âncora) */
static inline map_t qc_apply_at(map_t p, const double *t, double cx, double cy) {
    double to[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, cx, cy, 0, 1}, back[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -cx, -cy, 0, 1};
    return qc_apply(qc_apply(qc_apply(p, to), t), back);
}
/* r = a * b (convenção do Core Animation: aplica a, depois b) */
static inline void qc_t3_mul(double *r, const double *a, const double *b) {
    double o[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) o[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
    for (int i = 0; i < 16; i++) r[i] = o[i];
}
static inline void qc_t3_id(double *r) { for (int i = 0; i < 16; i++) r[i] = i % 5 == 0; }
static inline void qc_t3_copy(double *r, const double *t) {
    int zero = t[0] == 0 && t[1] == 0 && t[4] == 0 && t[5] == 0 && t[15] == 0;   /* nunca inicializada: identidade */
    if (zero) qc_t3_id(r); else for (int i = 0; i < 16; i++) r[i] = t[i];
}
/* t em volta do ponto (cx, cy): T(-c) * t * T(c) */
static inline void qc_t3_about(double *r, const double *t, double cx, double cy) {
    double a[16], b[16], tt[16];
    qc_t3_id(a); a[12] = -cx; a[13] = -cy;
    qc_t3_id(b); b[12] = cx; b[13] = cy;
    qc_t3_copy(tt, t);
    qc_t3_mul(r, a, tt);
    qc_t3_mul(r, r, b);
}
/* mapa de um sublayer sobre o mapa do pai. A composição é em 4x4 e só no fim vai para o plano do pai (é aí que a
 * profundidade de um transform 3D encontra a perspectiva do sublayerTransform):
 *   v * U * transform * T(posição) * post
 * U leva o ponto do layer ao âncora (u = (lx + vxk, yb*ly + vyk), yb = -1 com a geometria invertida); post é o que
 * o pai aplica nos sublayers (instância do replicador e sublayerTransform, já em volta do âncora); 0 = nada. */
static inline map_t qc_child(map_t parent, const double *tf, double posx, double posy, double vxk, double vyk, double yb, const double *post) {
    double m[16], u[16], tp[16], h[9], g[9];
    qc_t3_id(u); u[5] = yb; u[12] = vxk; u[13] = vyk;
    qc_t3_copy(tp, tf);
    qc_t3_mul(m, u, tp);
    qc_t3_id(tp); tp[12] = posx; tp[13] = posy;
    qc_t3_mul(m, m, tp);
    if (post) qc_t3_mul(m, m, post);
    qc_proj4(m, g);
    qc_to_mat(parent, h);
    qc_mat_mul(h, h, g);
    return qc_from_mat(h);
}
/* cor da instância i: instanceColor + i * deslocamento, em 0..1 */
static inline double qc_replica_chan(double base, double off, unsigned long long i) {
    double v = base + off * (double)i;
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

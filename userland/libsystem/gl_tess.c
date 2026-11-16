/*
 * mOS libSystem — OpenGL por software: o gerador de primitivos da tesselação (GL 4.0, seção 11.2.2).
 * Recebe o domínio (triângulos, quads ou isolinhas), o espaçamento (equal, fractional_even, fractional_odd),
 * a ordem dos vértices (cw/ccw), point_mode e os níveis externos/internos; devolve as coordenadas (u, v, w)
 * de cada vértice (gl_TessCoord) e os índices dos primitivos.
 *
 * Arestas: com espaçamento fracionário, o nível f vira n (próximo par/ímpar); a aresta tem n-2 segmentos de
 * comprimento 1/f e dois menores, iguais, no meio (simétricos, então dois patches vizinhos com o mesmo nível
 * dividem a aresta igual nos dois sentidos). Triângulos: anéis concêntricos (o anel k tem os cantos onde as
 * perpendiculares pelos pontos de subdivisão k se cruzam: d = 2/3 t_k em baricêntricas), costurados aresta
 * por aresta. Quads: o anel externo costurado na grade interna regular. Isolinhas: outer[0] linhas
 * (sempre equal) de outer[1] segmentos.
 */
#include "gl_int.h"

typedef struct { float u, v, w; } tess_pt_t;
#define TMAX 64

/* subdivisão de [0,1] para o nível f; t recebe n+1 parâmetros; devolve n */
static int subdiv(double f, GLenum sp, double *t) {
    int n;
    if (f != f) f = 1;
    if (sp == GL_FRACTIONAL_EVEN) {
        if (f < 2) f = 2;
        if (f > TMAX) f = TMAX;
        n = (int)ceil(f);
        if (n & 1) n++;
    } else if (sp == GL_FRACTIONAL_ODD) {
        if (f < 1) f = 1;
        if (f > TMAX - 1) f = TMAX - 1;
        n = (int)ceil(f);
        if (!(n & 1)) n++;
    } else {
        if (f < 1) f = 1;
        if (f > TMAX) f = TMAX;
        n = (int)ceil(f);
    }
    if (sp == GL_EQUAL || (double)n == f || n < 3) {
        for (int i = 0; i <= n; i++) t[i] = (double)i / n;
        return n;
    }
    /* n-2 segmentos de 1/f e dois curtos de (1 - (n-2)/f)/2, no meio */
    double l = 1.0 / f, s = (1 - (n - 2) * l) / 2;
    double seg[TMAX + 2];
    int k = 0;
    if (!(n & 1)) {
        for (int i = 0; i < (n - 2) / 2; i++) seg[k++] = l;
        seg[k++] = s; seg[k++] = s;
        for (int i = 0; i < (n - 2) / 2; i++) seg[k++] = l;
    } else {
        for (int i = 0; i < (n - 3) / 2; i++) seg[k++] = l;
        seg[k++] = s; seg[k++] = l; seg[k++] = s;
        for (int i = 0; i < (n - 3) / 2; i++) seg[k++] = l;
    }
    t[0] = 0;
    for (int i = 0; i < n; i++) t[i + 1] = t[i] + seg[i];
    t[n] = 1;
    return n;
}
/* número de segmentos de um nível (para os casos especiais de nível 1) */
static int levels_n(double f, GLenum sp) { double t[TMAX + 2]; return subdiv(f, sp, t); }

/* ---- saída com vértices únicos (hash pelos bits de u, v) */
typedef struct {
    tess_pt_t *p; int np, cap;
    u32 *ix; int ni, icap;
    int *hash; int hcap;
    int cw, pv;
} out_t;
static u32 hbits(float u, float v) { u32 a, b; memcpy(&a, &u, 4); memcpy(&b, &v, 4); return a * 2654435761u ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2)); }
static int vtx(out_t *O, double u, double v, double w) {
    float fu = (float)u, fv = (float)v, fw = (float)w;
    if (fu == 0) fu = 0;   /* -0 */
    if (fv == 0) fv = 0;
    u32 h = hbits(fu, fv) & (u32)(O->hcap - 1);
    for (;;) {
        int k = O->hash[h];
        if (k < 0) break;
        if (O->p[k].u == fu && O->p[k].v == fv) return k;
        h = (h + 1) & (u32)(O->hcap - 1);
    }
    if (O->np == O->cap) {
        int nc = O->cap ? O->cap * 2 : 256;
        tess_pt_t *np = realloc(O->p, sizeof *np * (size_t)nc);
        if (!np) return 0;
        O->p = np; O->cap = nc;
    }
    O->p[O->np] = (tess_pt_t){fu, fv, fw};
    O->hash[h] = O->np;
    return O->np++;
}
static void idx(out_t *O, u32 i) {
    if (O->ni == O->icap) {
        int nc = O->icap ? O->icap * 2 : 768;
        u32 *n = realloc(O->ix, sizeof *n * (size_t)nc);
        if (!n) return;
        O->ix = n; O->icap = nc;
    }
    O->ix[O->ni++] = i;
}
/* triângulo na ordem pedida (ccw ou cw no plano (u, v)) */
static void tri(out_t *O, int a, int b, int c) {
    if (a == b || b == c || a == c) return;
    tess_pt_t *A = &O->p[a], *B = &O->p[b], *Cc = &O->p[c];
    double ar = ((double)B->u - A->u) * ((double)Cc->v - A->v) - ((double)Cc->u - A->u) * ((double)B->v - A->v);
    if (ar == 0) return;
    if ((ar > 0) == !!O->cw) { int t = b; b = c; c = t; }
    idx(O, (u32)a); idx(O, (u32)b); idx(O, (u32)c);
}
/* costura duas arestas: P (np+1 pontos, parâmetros pa) por fora e Q (nq+1 pontos, parâmetros pb) por dentro */
static void stitch(out_t *O, const int *P, const double *pa, int np, const int *Q, const double *pb, int nq) {
    int i = 0, j = 0;
    while (i < np || j < nq) {
        if (j >= nq || (i < np && (pa[i] + pa[i + 1]) <= (pb[j] + pb[j + 1]))) { tri(O, P[i], P[i + 1], Q[j]); i++; }
        else { tri(O, P[i], Q[j + 1], Q[j]); j++; }
    }
}

/* ---- triângulos */
static void gen_tri(out_t *O, GLenum sp, const float *outer, const float *inner) {
    double to[3][TMAX + 2], ti[TMAX + 2];
    int no[3];
    for (int e = 0; e < 3; e++) no[e] = subdiv(outer[e], sp, to[e]);
    double fi = inner[0];
    int ni = levels_n(fi, sp);
    if (ni == 1) {
        if (no[0] == 1 && no[1] == 1 && no[2] == 1) {
            int a = vtx(O, 1, 0, 0), b = vtx(O, 0, 1, 0), c = vtx(O, 0, 0, 1);
            tri(O, a, b, c);
            return;
        }
        fi = 1.0001;   /* nível interno 1 com externo > 1 vale como 1 + epsilon */
    }
    ni = subdiv(fi, sp, ti);
    /* anel externo: A (u) -> B (v) é a aresta w = 0 (outer[2]); B -> C (w) é u = 0 (outer[0]); C -> A é v = 0 (outer[1]) */
    static const double corner[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    static const int edge_level[3] = {2, 0, 1};
    int ring_prev[3][TMAX + 2];
    double par_prev[3][TMAX + 2];
    int nprev[3];
    for (int e = 0; e < 3; e++) {
        const double *P = corner[e], *Q = corner[(e + 1) % 3];
        int L = edge_level[e], n = no[L];
        for (int i = 0; i <= n; i++) {
            double t = to[L][i];
            ring_prev[e][i] = vtx(O, P[0] + (Q[0] - P[0]) * t, P[1] + (Q[1] - P[1]) * t, P[2] + (Q[2] - P[2]) * t);
            par_prev[e][i] = t;
        }
        nprev[e] = n;
    }
    /* anéis internos */
    for (int k = 1; 2 * k <= ni; k++) {
        double d = 2.0 / 3.0 * ti[k];
        int m = ni - 2 * k;
        double cr[3][3] = {{1 - 2 * d, d, d}, {d, 1 - 2 * d, d}, {d, d, 1 - 2 * d}};
        if (m == 0) cr[0][0] = cr[0][1] = cr[0][2] = cr[1][0] = cr[1][1] = cr[1][2] = cr[2][0] = cr[2][1] = cr[2][2] = 1.0 / 3.0;
        int ring[3][TMAX + 2];
        double par[3][TMAX + 2];
        double span = ti[ni - k] - ti[k];
        for (int e = 0; e < 3; e++) {
            const double *P = cr[e], *Q = cr[(e + 1) % 3];
            for (int j = 0; j <= m; j++) {
                double s = m ? (ti[k + j] - ti[k]) / (span > 0 ? span : 1) : 0;
                ring[e][j] = vtx(O, P[0] + (Q[0] - P[0]) * s, P[1] + (Q[1] - P[1]) * s, P[2] + (Q[2] - P[2]) * s);
                par[e][j] = m ? s : 0.5;
            }
        }
        for (int e = 0; e < 3; e++) stitch(O, ring_prev[e], par_prev[e], nprev[e], ring[e], par[e], m);
        if (m == 1) tri(O, ring[0][0], ring[1][0], ring[2][0]);   /* nível ímpar: o triângulo do meio */
        for (int e = 0; e < 3; e++) { memcpy(ring_prev[e], ring[e], sizeof(int) * (size_t)(m + 1)); memcpy(par_prev[e], par[e], sizeof(double) * (size_t)(m + 1)); nprev[e] = m; }
        if (m <= 1) break;
    }
}

/* ---- quads */
static void gen_quad(out_t *O, GLenum sp, const float *outer, const float *inner) {
    double to[4][TMAX + 2], tu[TMAX + 2], tv[TMAX + 2];
    int no[4];
    for (int e = 0; e < 4; e++) no[e] = subdiv(outer[e], sp, to[e]);
    double fu = inner[0], fv = inner[1];
    int nu = levels_n(fu, sp), nv = levels_n(fv, sp);
    if (nu == 1 && nv == 1 && no[0] == 1 && no[1] == 1 && no[2] == 1 && no[3] == 1) {
        int a = vtx(O, 0, 0, 0), b = vtx(O, 1, 0, 0), c = vtx(O, 1, 1, 0), d = vtx(O, 0, 1, 0);
        tri(O, a, b, c);
        tri(O, a, c, d);
        return;
    }
    if (nu == 1) fu = 1.0001;
    if (nv == 1) fv = 1.0001;
    nu = subdiv(fu, sp, tu);
    nv = subdiv(fv, sp, tv);
    /* anel externo: (0,0) -> (1,0) é v = 0 (outer[1]); -> (1,1) u = 1 (outer[2]); -> (0,1) v = 1 (outer[3]); -> (0,0) u = 0 (outer[0]) */
    static const double corner[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    static const int edge_level[4] = {1, 2, 3, 0};
    for (int e = 0; e < 4; e++) {
        const double *P = corner[e], *Q = corner[(e + 1) % 4];
        int L = edge_level[e], n = no[L];
        int outv[TMAX + 2], inv_[TMAX + 2];
        double pa[TMAX + 2], pb[TMAX + 2];
        for (int i = 0; i <= n; i++) {
            double t = to[L][i];
            outv[i] = vtx(O, P[0] + (Q[0] - P[0]) * t, P[1] + (Q[1] - P[1]) * t, 0);
            pa[i] = t;
        }
        /* aresta correspondente do anel interno (pontos da grade), no mesmo sentido de percurso */
        int horiz = e == 0 || e == 2, m = horiz ? nu - 2 : nv - 2;
        double span = horiz ? tu[nu - 1] - tu[1] : tv[nv - 1] - tv[1];
        for (int j = 0; j <= m; j++) {
            double x, y, s;
            if (e == 0) { x = tu[1 + j]; y = tv[1]; s = x - tu[1]; }
            else if (e == 1) { x = tu[nu - 1]; y = tv[1 + j]; s = y - tv[1]; }
            else if (e == 2) { x = tu[nu - 1 - j]; y = tv[nv - 1]; s = tu[nu - 1] - x; }
            else { x = tu[1]; y = tv[nv - 1 - j]; s = tv[nv - 1] - y; }
            inv_[j] = vtx(O, x, y, 0);
            pb[j] = m && span > 0 ? s / span : 0.5;
        }
        stitch(O, outv, pa, n, inv_, pb, m);
    }
    /* grade interna */
    for (int j = 1; j + 1 <= nv - 1; j++)
        for (int i = 1; i + 1 <= nu - 1; i++) {
            int a = vtx(O, tu[i], tv[j], 0), b = vtx(O, tu[i + 1], tv[j], 0), c = vtx(O, tu[i + 1], tv[j + 1], 0), d = vtx(O, tu[i], tv[j + 1], 0);
            tri(O, a, b, c);
            tri(O, a, c, d);
        }
}

/* ---- isolinhas */
static void gen_iso(out_t *O, GLenum sp, const float *outer) {
    double tl[TMAX + 2], ts[TMAX + 2];
    int nl = subdiv(outer[0], GL_EQUAL, tl);
    int ns = subdiv(outer[1], sp, ts);
    for (int j = 0; j < nl; j++) {
        double v = tl[j];
        int prev = vtx(O, ts[0], v, 0);
        for (int i = 1; i <= ns; i++) {
            int cur = vtx(O, ts[i], v, 0);
            idx(O, (u32)prev); idx(O, (u32)cur);
            prev = cur;
        }
    }
}

/* devolve 0 se o patch é descartado (nível externo <= 0 ou NaN); *pv = vértices por primitivo (3, 2 ou 1) */
int gl_tess_gen(GLenum mode, GLenum sp, int cw, int point, const float outer[4], const float inner[2],
                tess_pt_t **pts, int *npts, u32 **ix, int *nix, int *pv) {
    int ne = mode == GL_TRIANGLES ? 3 : mode == GL_QUADS ? 4 : 2;
    for (int i = 0; i < ne; i++) if (!(outer[i] > 0)) return 0;
    out_t O = {0};
    O.cw = cw;
    O.hcap = 16384;
    O.hash = malloc(sizeof(int) * (size_t)O.hcap);
    if (!O.hash) return 0;
    for (int i = 0; i < O.hcap; i++) O.hash[i] = -1;
    if (mode == GL_TRIANGLES) gen_tri(&O, sp, outer, inner);
    else if (mode == GL_QUADS) gen_quad(&O, sp, outer, inner);
    else gen_iso(&O, sp, outer);
    free(O.hash);
    *pv = mode == GL_ISOLINES ? 2 : 3;
    if (point) {
        /* point_mode: cada vértice distinto uma vez */
        free(O.ix);
        O.ix = malloc(sizeof(u32) * (size_t)(O.np ? O.np : 1));
        if (O.ix) for (int i = 0; i < O.np; i++) O.ix[i] = (u32)i;
        O.ni = O.ix ? O.np : 0;
        *pv = 1;
    }
    *pts = O.p;
    *npts = O.np;
    *ix = O.ix;
    *nix = O.ni;
    return 1;
}

/*
 * mOS libSystem — OpenGL por software (compat): avaliadores e buffer de acumulação.
 *   glMap1/glMap2 (todos os alvos), glMapGrid, glEvalCoord, glEvalPoint, glEvalMesh e glGetMap: polinômios de
 *   Bernstein (de Casteljau) sobre os pontos de controle; o vértice gerado leva cor, normal, índice e coordenada
 *   de textura dos mapas ligados sem mudar os valores correntes; GL_AUTO_NORMAL calcula a normal pelas derivadas
 *   parciais do mapa de vértice 2D.
 *   glAccum (GL_ACCUM, LOAD, RETURN, MULT, ADD) sobre um buffer RGBA float do tamanho do framebuffer, e o
 *   glClear com GL_ACCUM_BUFFER_BIT.
 */
#include "gl_int.h"
#include "gl_proto.h"

/* ================================================================ mapas */
#define NMAP 9   /* VERTEX_3, VERTEX_4, INDEX, COLOR_4, NORMAL, TEXTURE_COORD_1..4 */
typedef struct { int on, order, k; double u1, u2; float *pts; } map1_t;
typedef struct { int uorder, vorder, k; double u1, u2, v1, v2; float *pts; } map2_t;
typedef struct {
    map1_t m1[NMAP];
    map2_t m2[NMAP];
    int g1n; double g1u1, g1u2;
    int g2un, g2vn; double g2u1, g2u2, g2v1, g2v2;
} ev_t;
static ev_t *ev(gl_ctx_t *C) {
    if (!C->eval_state) {
        ev_t *e = calloc(1, sizeof *e);
        if (!e) return 0;
        /* valores iniciais da especificação: grade 1, domínio [0, 1], mapas de ordem 1 com o ponto padrão */
        e->g1n = 1; e->g1u2 = 1;
        e->g2un = e->g2vn = 1; e->g2u2 = e->g2v2 = 1;
        C->eval_state = e;
    }
    return C->eval_state;
}
static int map_index(GLenum t, int two, int *k) {
    GLenum base = two ? GL_MAP2_COLOR_4 : GL_MAP1_COLOR_4;
    static const struct { int off, k; } tab[] = {
        /* em ordem de enum: COLOR_4, INDEX, NORMAL, TEXTURE_COORD_1..4, VERTEX_3, VERTEX_4 */
        {3, 4}, {2, 1}, {4, 3}, {5, 1}, {6, 2}, {7, 3}, {8, 4}, {0, 3}, {1, 4}};
    if (t < base || t > base + 8) return -1;
    *k = tab[t - base].k;
    return tab[t - base].off;
}
static const GLenum MAP1_ENUM[NMAP] = {GL_MAP1_VERTEX_3, GL_MAP1_VERTEX_4, GL_MAP1_INDEX, GL_MAP1_COLOR_4, GL_MAP1_NORMAL,
                                       GL_MAP1_TEXTURE_COORD_1, GL_MAP1_TEXTURE_COORD_2, GL_MAP1_TEXTURE_COORD_3, GL_MAP1_TEXTURE_COORD_4};
static const GLenum MAP2_ENUM[NMAP] = {GL_MAP2_VERTEX_3, GL_MAP2_VERTEX_4, GL_MAP2_INDEX, GL_MAP2_COLOR_4, GL_MAP2_NORMAL,
                                       GL_MAP2_TEXTURE_COORD_1, GL_MAP2_TEXTURE_COORD_2, GL_MAP2_TEXTURE_COORD_3, GL_MAP2_TEXTURE_COORD_4};
static void map1(GLenum target, double u1, double u2, int stride, int order, const float *pf, const double *pd) {
    GLC();
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    int k, i = map_index(target, 0, &k);
    if (i < 0) GLERR(GL_INVALID_ENUM, );
    if (u1 == u2 || stride < k || order < 1 || order > 8) GLERR(GL_INVALID_VALUE, );
    ev_t *E = ev(C);
    if (!E) GLERR(GL_OUT_OF_MEMORY, );
    float *p = malloc(sizeof(float) * (size_t)(order * k));
    if (!p) GLERR(GL_OUT_OF_MEMORY, );
    for (int a = 0; a < order; a++)
        for (int c = 0; c < k; c++) p[a * k + c] = pf ? pf[a * stride + c] : (float)pd[a * stride + c];
    map1_t *m = &E->m1[i];
    free(m->pts);
    m->pts = p;
    m->order = order;
    m->k = k;
    m->u1 = u1;
    m->u2 = u2;
}
void glMap1f(GLenum t, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *p) { map1(t, u1, u2, stride, order, p, 0); }
void glMap1d(GLenum t, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *p) { map1(t, u1, u2, stride, order, 0, p); }
static void map2(GLenum target, double u1, double u2, int us, int uo, double v1, double v2, int vs, int vo, const float *pf, const double *pd) {
    GLC();
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    int k, i = map_index(target, 1, &k);
    if (i < 0) GLERR(GL_INVALID_ENUM, );
    if (u1 == u2 || v1 == v2 || us < k || vs < k || uo < 1 || uo > 8 || vo < 1 || vo > 8) GLERR(GL_INVALID_VALUE, );
    ev_t *E = ev(C);
    if (!E) GLERR(GL_OUT_OF_MEMORY, );
    float *p = malloc(sizeof(float) * (size_t)(uo * vo * k));
    if (!p) GLERR(GL_OUT_OF_MEMORY, );
    for (int a = 0; a < uo; a++)
        for (int b = 0; b < vo; b++)
            for (int c = 0; c < k; c++) p[(a * vo + b) * k + c] = pf ? pf[a * us + b * vs + c] : (float)pd[a * us + b * vs + c];
    map2_t *m = &E->m2[i];
    free(m->pts);
    m->pts = p;
    m->uorder = uo;
    m->vorder = vo;
    m->k = k;
    m->u1 = u1; m->u2 = u2; m->v1 = v1; m->v2 = v2;
}
void glMap2f(GLenum t, GLfloat u1, GLfloat u2, GLint us, GLint uo, GLfloat v1, GLfloat v2, GLint vs, GLint vo, const GLfloat *p) { map2(t, u1, u2, us, uo, v1, v2, vs, vo, p, 0); }
void glMap2d(GLenum t, GLdouble u1, GLdouble u2, GLint us, GLint uo, GLdouble v1, GLdouble v2, GLint vs, GLint vo, const GLdouble *p) { map2(t, u1, u2, us, uo, v1, v2, vs, vo, 0, p); }

/* de Casteljau num conjunto de n pontos de k componentes (com derivada opcional) */
static void casteljau(const float *pts, int n, int k, int step, double t, double *out, double *deriv) {
    double tmp[8][4];
    for (int i = 0; i < n; i++) for (int c = 0; c < k; c++) tmp[i][c] = pts[i * step + c];
    for (int r = 1; r < n; r++) {
        if (r == n - 1 && deriv) for (int c = 0; c < k; c++) deriv[c] = (n - 1) * (tmp[1][c] - tmp[0][c]);
        for (int i = 0; i < n - r; i++) for (int c = 0; c < k; c++) tmp[i][c] = (1 - t) * tmp[i][c] + t * tmp[i + 1][c];
    }
    if (n == 1 && deriv) for (int c = 0; c < k; c++) deriv[c] = 0;
    for (int c = 0; c < k; c++) out[c] = tmp[0][c];
}
static void eval1(const map1_t *m, double u, double *out) {
    double t = (u - m->u1) / (m->u2 - m->u1);
    casteljau(m->pts, m->order, m->k, m->k, t, out, 0);
}
/* avalia o mapa 2D em (u, v); du/dv: derivadas parciais (no domínio do mapa) */
static void eval2(const map2_t *m, double u, double v, double *out, double *du, double *dv) {
    double s = (u - m->u1) / (m->u2 - m->u1), t = (v - m->v1) / (m->v2 - m->v1);
    double col[8][4], dcol[8][4];
    /* primeiro na direção v para cada linha u, depois em u */
    for (int a = 0; a < m->uorder; a++) casteljau(m->pts + (i64)a * m->vorder * m->k, m->vorder, m->k, m->k, t, col[a], dcol[a]);
    float cf[32], df[32];
    for (int a = 0; a < m->uorder; a++) for (int c = 0; c < m->k; c++) { cf[a * m->k + c] = (float)col[a][c]; df[a * m->k + c] = (float)dcol[a][c]; }
    casteljau(cf, m->uorder, m->k, m->k, s, out, du);
    if (dv) { double d[4]; casteljau(df, m->uorder, m->k, m->k, s, d, 0); for (int c = 0; c < m->k; c++) dv[c] = d[c] / (m->v2 - m->v1); }
    if (du) for (int c = 0; c < m->k; c++) du[c] /= (m->u2 - m->u1);
}
static int map_on(gl_ctx_t *C, int two, int i) { return gl_is_on(C, two ? MAP2_ENUM[i] : MAP1_ENUM[i]); }
/* gera um vértice com os mapas ligados; os valores correntes ficam como estavam */
static void emit(gl_ctx_t *C, int two, double u, double v) {
    ev_t *E = ev(C);
    if (!E) return;
    float scol[4], snrm[3], stc[4];
    memcpy(scol, C->cur_col, 16);
    memcpy(snrm, C->cur_nrm, 12);
    memcpy(stc, C->cur_tc[0], 16);
    double o[4], du[4], dv[4];
    if (map_on(C, two, 3) && (two ? E->m2[3].pts : E->m1[3].pts)) {
        if (two) eval2(&E->m2[3], u, v, o, 0, 0); else eval1(&E->m1[3], u, o);
        glColor4f((float)o[0], (float)o[1], (float)o[2], (float)o[3]);
    }
    for (int t = 8; t >= 5; t--)
        if (map_on(C, two, t) && (two ? E->m2[t].pts : E->m1[t].pts)) {
            double tc[4] = {0, 0, 0, 1};
            if (two) eval2(&E->m2[t], u, v, tc, 0, 0); else eval1(&E->m1[t], u, tc);
            int n = t - 4;
            glTexCoord4f((float)tc[0], n > 1 ? (float)tc[1] : 0, n > 2 ? (float)tc[2] : 0, n > 3 ? (float)tc[3] : 1);
            break;
        }
    int vi = map_on(C, two, 1) && (two ? E->m2[1].pts : E->m1[1].pts) ? 1 : map_on(C, two, 0) && (two ? E->m2[0].pts : E->m1[0].pts) ? 0 : -1;
    int nrm_done = 0;
    if (map_on(C, two, 4) && (two ? E->m2[4].pts : E->m1[4].pts)) {
        if (two) eval2(&E->m2[4], u, v, o, 0, 0); else eval1(&E->m1[4], u, o);
        glNormal3f((float)o[0], (float)o[1], (float)o[2]);
        nrm_done = 1;
    }
    if (vi >= 0) {
        if (two) {
            eval2(&E->m2[vi], u, v, o, du, dv);
            if (!nrm_done && gl_is_on(C, GL_AUTO_NORMAL)) {
                /* normal = dP/du x dP/dv (com w, pela regra do quociente) */
                double pu[3], pv[3];
                if (vi == 1 && o[3] != 0) {
                    for (int c = 0; c < 3; c++) { pu[c] = (du[c] * o[3] - o[c] * du[3]) / (o[3] * o[3]); pv[c] = (dv[c] * o[3] - o[c] * dv[3]) / (o[3] * o[3]); }
                } else for (int c = 0; c < 3; c++) { pu[c] = du[c]; pv[c] = dv[c]; }
                double n[3] = {pu[1] * pv[2] - pu[2] * pv[1], pu[2] * pv[0] - pu[0] * pv[2], pu[0] * pv[1] - pu[1] * pv[0]};
                double l = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (l > 0) glNormal3f((float)(n[0] / l), (float)(n[1] / l), (float)(n[2] / l));
            }
        } else eval1(&E->m1[vi], u, o);
        if (vi == 1) glVertex4f((float)o[0], (float)o[1], (float)o[2], (float)o[3]);
        else glVertex3f((float)o[0], (float)o[1], (float)o[2]);
    }
    memcpy(C->cur_col, scol, 16);
    memcpy(C->cur_nrm, snrm, 12);
    memcpy(C->cur_tc[0], stc, 16);
}
void glEvalCoord1d(GLdouble u) { GLC(); emit(C, 0, u, 0); }
void glEvalCoord1f(GLfloat u) { glEvalCoord1d(u); }
void glEvalCoord1dv(const GLdouble *u) { glEvalCoord1d(u[0]); }
void glEvalCoord1fv(const GLfloat *u) { glEvalCoord1d(u[0]); }
void glEvalCoord2d(GLdouble u, GLdouble v) { GLC(); emit(C, 1, u, v); }
void glEvalCoord2f(GLfloat u, GLfloat v) { glEvalCoord2d(u, v); }
void glEvalCoord2dv(const GLdouble *u) { glEvalCoord2d(u[0], u[1]); }
void glEvalCoord2fv(const GLfloat *u) { glEvalCoord2d(u[0], u[1]); }
void glMapGrid1d(GLint n, GLdouble u1, GLdouble u2) { GLC(); if (n <= 0) GLERR(GL_INVALID_VALUE, ); ev_t *E = ev(C); if (!E) return; E->g1n = n; E->g1u1 = u1; E->g1u2 = u2; }
void glMapGrid1f(GLint n, GLfloat u1, GLfloat u2) { glMapGrid1d(n, u1, u2); }
void glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) {
    GLC();
    if (un <= 0 || vn <= 0) GLERR(GL_INVALID_VALUE, );
    ev_t *E = ev(C);
    if (!E) return;
    E->g2un = un; E->g2u1 = u1; E->g2u2 = u2; E->g2vn = vn; E->g2v1 = v1; E->g2v2 = v2;
}
void glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) { glMapGrid2d(un, u1, u2, vn, v1, v2); }
static double gu1(ev_t *E, int i) { return i == E->g1n ? E->g1u2 : E->g1u1 + i * (E->g1u2 - E->g1u1) / E->g1n; }
static double gu2(ev_t *E, int i) { return i == E->g2un ? E->g2u2 : E->g2u1 + i * (E->g2u2 - E->g2u1) / E->g2un; }
static double gv2(ev_t *E, int j) { return j == E->g2vn ? E->g2v2 : E->g2v1 + j * (E->g2v2 - E->g2v1) / E->g2vn; }
void glEvalPoint1(GLint i) { GLC(); ev_t *E = ev(C); if (E) emit(C, 0, gu1(E, i), 0); }
void glEvalPoint2(GLint i, GLint j) { GLC(); ev_t *E = ev(C); if (E) emit(C, 1, gu2(E, i), gv2(E, j)); }
void glEvalMesh1(GLenum mode, GLint i1, GLint i2) {
    GLC();
    if (mode != GL_POINT && mode != GL_LINE) GLERR(GL_INVALID_ENUM, );
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    ev_t *E = ev(C);
    if (!E) return;
    glBegin(mode == GL_POINT ? GL_POINTS : GL_LINE_STRIP);
    for (int i = i1; i <= i2; i++) emit(C, 0, gu1(E, i), 0);
    glEnd();
}
void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {
    GLC();
    if (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL) GLERR(GL_INVALID_ENUM, );
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    ev_t *E = ev(C);
    if (!E) return;
    if (mode == GL_FILL) {
        for (int j = j1; j < j2; j++) {
            glBegin(GL_QUAD_STRIP);
            for (int i = i1; i <= i2; i++) { emit(C, 1, gu2(E, i), gv2(E, j)); emit(C, 1, gu2(E, i), gv2(E, j + 1)); }
            glEnd();
        }
    } else if (mode == GL_LINE) {
        for (int j = j1; j <= j2; j++) { glBegin(GL_LINE_STRIP); for (int i = i1; i <= i2; i++) emit(C, 1, gu2(E, i), gv2(E, j)); glEnd(); }
        for (int i = i1; i <= i2; i++) { glBegin(GL_LINE_STRIP); for (int j = j1; j <= j2; j++) emit(C, 1, gu2(E, i), gv2(E, j)); glEnd(); }
    } else {
        glBegin(GL_POINTS);
        for (int j = j1; j <= j2; j++) for (int i = i1; i <= i2; i++) emit(C, 1, gu2(E, i), gv2(E, j));
        glEnd();
    }
}
/* glGetMap: GL_COEFF, GL_ORDER, GL_DOMAIN */
static int get_map(gl_ctx_t *C, GLenum target, GLenum q, double *out) {
    ev_t *E = ev(C);
    if (!E) return 0;
    int k, i = map_index(target, 0, &k), two = 0;
    if (i < 0) { i = map_index(target, 1, &k); two = 1; }
    if (i < 0) { gl_err(C, GL_INVALID_ENUM); return 0; }
    if (!two) {
        map1_t *m = &E->m1[i];
        if (q == GL_ORDER) { out[0] = m->pts ? m->order : 1; return 1; }
        if (q == GL_DOMAIN) { out[0] = m->pts ? m->u1 : 0; out[1] = m->pts ? m->u2 : 1; return 2; }
        if (q == GL_COEFF) { if (!m->pts) return 0; for (int a = 0; a < m->order * m->k; a++) out[a] = m->pts[a]; return m->order * m->k; }
    } else {
        map2_t *m = &E->m2[i];
        if (q == GL_ORDER) { out[0] = m->pts ? m->uorder : 1; out[1] = m->pts ? m->vorder : 1; return 2; }
        if (q == GL_DOMAIN) { out[0] = m->pts ? m->u1 : 0; out[1] = m->pts ? m->u2 : 1; out[2] = m->pts ? m->v1 : 0; out[3] = m->pts ? m->v2 : 1; return 4; }
        if (q == GL_COEFF) { if (!m->pts) return 0; for (int a = 0; a < m->uorder * m->vorder * m->k; a++) out[a] = m->pts[a]; return m->uorder * m->vorder * m->k; }
    }
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
void glGetMapdv(GLenum t, GLenum q, GLdouble *v) { GLC(); double o[256]; int n = get_map(C, t, q, o); for (int i = 0; i < n; i++) v[i] = o[i]; }
void glGetMapfv(GLenum t, GLenum q, GLfloat *v) { GLC(); double o[256]; int n = get_map(C, t, q, o); for (int i = 0; i < n; i++) v[i] = (float)o[i]; }
void glGetMapiv(GLenum t, GLenum q, GLint *v) { GLC(); double o[256]; int n = get_map(C, t, q, o); for (int i = 0; i < n; i++) v[i] = (GLint)(o[i] < 0 ? o[i] - 0.5 : o[i] + 0.5); }
void gl_eval_free(gl_ctx_t *C) {
    ev_t *E = C->eval_state;
    if (E) { for (int i = 0; i < NMAP; i++) { free(E->m1[i].pts); free(E->m2[i].pts); } free(E); C->eval_state = 0; }
    free(C->accum);
    C->accum = 0;
}

/* ================================================================ acumulação */
static int accum_ready(gl_ctx_t *C) {
    if (C->accum && C->accum_w == C->fb_w && C->accum_h == C->fb_h) return 1;
    free(C->accum);
    C->accum = calloc((size_t)C->fb_w * (size_t)C->fb_h, 16);
    C->accum_w = C->fb_w;
    C->accum_h = C->fb_h;
    return C->accum != 0;
}
static void accum_rect(gl_ctx_t *C, int *x0, int *y0, int *x1, int *y1) {
    *x0 = 0; *y0 = 0; *x1 = C->fb_w; *y1 = C->fb_h;
    if (gl_is_on(C, GL_SCISSOR_TEST)) {
        if (C->sc[0] > *x0) *x0 = C->sc[0];
        if (C->sc[1] > *y0) *y0 = C->sc[1];
        if (C->sc[0] + C->sc[2] < *x1) *x1 = C->sc[0] + C->sc[2];
        if (C->sc[1] + C->sc[3] < *y1) *y1 = C->sc[1] + C->sc[3];
    }
}
void gl_accum_clear(gl_ctx_t *C) {
    if (C->draw_fbo || !accum_ready(C)) return;
    int x0, y0, x1, y1;
    accum_rect(C, &x0, &y0, &x1, &y1);
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) memcpy(C->accum + ((i64)y * C->accum_w + x) * 4, C->clear_accum, 16);
}
int gl_read_source(gl_ctx_t *C, msl_tex_t **src);
void glAccum(GLenum op, GLfloat value) {
    GLC();
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    if (op != GL_ACCUM && op != GL_LOAD && op != GL_RETURN && op != GL_MULT && op != GL_ADD) GLERR(GL_INVALID_ENUM, );
    if (C->profile_core || C->draw_fbo || C->read_fbo) GLERR(GL_INVALID_OPERATION, );   /* só o framebuffer padrão tem acumulação */
    if (!accum_ready(C)) GLERR(GL_OUT_OF_MEMORY, );
    int x0, y0, x1, y1;
    accum_rect(C, &x0, &y0, &x1, &y1);
    msl_tex_t *src = 0;
    if (op == GL_ACCUM || op == GL_LOAD) { if (!gl_read_source(C, &src) || !src) GLERR(GL_INVALID_OPERATION, ); }
    gl_target_t T;
    if (op == GL_RETURN && !gl_resolve_target(C, 1, &T)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float *a = C->accum + ((i64)y * C->accum_w + x) * 4;
            double c[4];
            switch (op) {
            case GL_ACCUM: msl_tex_load(src, x, y, 0, 0, 0, c); for (int k = 0; k < 4; k++) a[k] += (float)(c[k] * value); break;
            case GL_LOAD: msl_tex_load(src, x, y, 0, 0, 0, c); for (int k = 0; k < 4; k++) a[k] = (float)(c[k] * value); break;
            case GL_MULT: for (int k = 0; k < 4; k++) a[k] *= value; break;
            case GL_ADD: for (int k = 0; k < 4; k++) a[k] += value; break;
            case GL_RETURN:
                for (int i = 0; i < T.ncolor; i++) {
                    msl_tex_t *t = T.color[i];
                    if (!t) continue;
                    double o[4];
                    msl_tex_load(t, x, y, 0, 0, 0, o);
                    for (int k = 0; k < 4; k++) if (C->cmask[i][k]) o[k] = mx_cl01(a[k] * value);
                    msl_tex_store(t, x, y, 0, 0, 0, o);
                }
                break;
            }
        }
}

/*
 * mOS libSystem — OpenGL por software: o pipeline de desenho.
 *   busca de índices (8/16/32 bits, primitive restart, base vertex) -> vertex shader (GLSL ou fixed
 *   function, com cache de vértices) -> montagem (pontos, linhas, strips, loops, triângulos, strips,
 *   fans, quads, quad strips, polígonos e as formas com adjacência) -> transform feedback ->
 *   recorte no espaço de recorte (−w..w, depth clamp, gl_ClipDistance / glClipPlane) -> divisão
 *   e viewport -> culling, polygon mode e polygon offset -> rasterização (regra top-left, centro do
 *   pixel, interpolação com correção de perspectiva, flat com o vértice provocante, noperspective)
 *   -> fragment shader -> scissor, alpha test, stencil, depth, blending (fatores, equações, dual
 *   source), logic op, sRGB e máscaras.
 * Também: modo de seleção e feedback (compat) e as contagens das queries.
 */
#include "gl_int.h"
#include "gl_proto.h"

/* ================================================================ estado do draw */
typedef struct {
    gl_ctx_t *C;
    gl_target_t *T;
    gl_stage_t *S;
    int depth_on, stencil_on, blend_any, scis, cull_on, early;
    int poff_fill, poff_line, poff_point;
    int nclip;
    u32 clip_mask;
    int x0, y0, x1, y1;             /* janela de escrita (alvo ∩ scissor) */
    int flat_last;                  /* vértice provocante: último */
    float flat_v[GLX_VAR][4];       /* valores flat do primitivo atual */
    int front;
    u32 prim;
    int srgb_on;
    int sel;                        /* GL_SELECT / GL_FEEDBACK */
} rs_t;

/* ================================================================ testes e escrita de pixel */
static int cmpf(GLenum f, double a, double b) {
    switch (f) {
    case GL_NEVER: return 0;
    case GL_LESS: return a < b;
    case GL_EQUAL: return a == b;
    case GL_LEQUAL: return a <= b;
    case GL_GREATER: return a > b;
    case GL_NOTEQUAL: return a != b;
    case GL_GEQUAL: return a >= b;
    }
    return 1;
}
static u8 *stencil_ptr(msl_tex_t *t, int x, int y) {
    if (!t) return 0;
    u8 *q = mx_texel(t, x, y, 0, 0, 0);
    return t->fmt == 260 ? q + 4 : t->fmt == 253 ? q : 0;
}
static double depth_get(msl_tex_t *t, int x, int y) {
    u8 *q = mx_texel(t, x, y, 0, 0, 0);
    if (t->fmt == 250) { u16 v; memcpy(&v, q, 2); return v / 65535.0; }
    float f;
    memcpy(&f, q, 4);
    return f;
}
static void depth_put(msl_tex_t *t, int x, int y, double d) {
    u8 *q = mx_texel(t, x, y, 0, 0, 0);
    if (t->fmt == 250) { u16 v = (u16)(mx_cl01(d) * 65535 + 0.5); memcpy(q, &v, 2); return; }
    float f = (float)d;
    memcpy(q, &f, 4);
}
static u32 sop(GLenum op, u32 cur, u32 ref) {
    switch (op) {
    case GL_KEEP: return cur;
    case GL_ZERO: return 0;
    case GL_REPLACE: return ref;
    case GL_INCR: return cur < 255 ? cur + 1 : 255;
    case GL_DECR: return cur > 0 ? cur - 1 : 0;
    case GL_INVERT: return ~cur & 255;
    case GL_INCR_WRAP: return (cur + 1) & 255;
    case GL_DECR_WRAP: return (cur - 1) & 255;
    }
    return cur;
}
static void stencil_apply(gl_ctx_t *C, u8 *sp, int face, GLenum op) {
    if (!sp) return;
    u32 cur = *sp, ref = (u32)C->sref[face] & 255, m = C->swmask[face] & 255;
    u32 nv = sop(op, cur, ref);
    *sp = (u8)((cur & ~m) | (nv & m));
}
/* 0: descartado; 1: passou */
static int ds_test(rs_t *R, int x, int y, double z) {
    gl_ctx_t *C = R->C;
    gl_target_t *T = R->T;
    int face = R->front ? 0 : 1;
    u8 *sp = R->stencil_on ? stencil_ptr(T->stencil, x, y) : 0;
    if (sp) {
        u32 ref = (u32)C->sref[face] & 255, m = C->svmask[face] & 255;
        if (!cmpf(C->sfunc[face], ref & m, *sp & m)) { stencil_apply(C, sp, face, C->sfail[face]); return 0; }
    }
    if (R->depth_on && T->depth) {
        double cur = depth_get(T->depth, x, y);
        if (!cmpf(C->dfunc, z, cur)) { if (sp) stencil_apply(C, sp, face, C->szfail[face]); return 0; }
        if (C->dmask) depth_put(T->depth, x, y, z);
    }
    if (sp) stencil_apply(C, sp, face, C->szpass[face]);
    C->samples_passed++;
    return 1;
}
static double bfac(GLenum f, int i, const double *s, const double *s1, const double *d, const float *bc) {
    switch (f) {
    case GL_ZERO: return 0;
    case GL_ONE: return 1;
    case GL_SRC_COLOR: return s[i];
    case GL_ONE_MINUS_SRC_COLOR: return 1 - s[i];
    case GL_DST_COLOR: return d[i];
    case GL_ONE_MINUS_DST_COLOR: return 1 - d[i];
    case GL_SRC_ALPHA: return s[3];
    case GL_ONE_MINUS_SRC_ALPHA: return 1 - s[3];
    case GL_DST_ALPHA: return d[3];
    case GL_ONE_MINUS_DST_ALPHA: return 1 - d[3];
    case GL_CONSTANT_COLOR: return bc[i];
    case GL_ONE_MINUS_CONSTANT_COLOR: return 1 - bc[i];
    case GL_CONSTANT_ALPHA: return bc[3];
    case GL_ONE_MINUS_CONSTANT_ALPHA: return 1 - bc[3];
    case GL_SRC_ALPHA_SATURATE: return i < 3 ? (s[3] < 1 - d[3] ? s[3] : 1 - d[3]) : 1;
    case GL_SRC1_COLOR: return s1[i];
    case GL_ONE_MINUS_SRC1_COLOR: return 1 - s1[i];
    case GL_SRC1_ALPHA: return s1[3];
    case GL_ONE_MINUS_SRC1_ALPHA: return 1 - s1[3];
    }
    return 1;
}
static double beq(GLenum e, double s, double fs, double d, double fd) {
    switch (e) {
    case GL_FUNC_SUBTRACT: return s * fs - d * fd;
    case GL_FUNC_REVERSE_SUBTRACT: return d * fd - s * fs;
    case GL_MIN: return s < d ? s : d;
    case GL_MAX: return s > d ? s : d;
    }
    return s * fs + d * fd;
}
static u32 logic(GLenum op, u32 s, u32 d) {
    switch (op) {
    case GL_CLEAR: return 0;
    case GL_AND: return s & d;
    case GL_AND_REVERSE: return s & ~d;
    case GL_COPY: return s;
    case GL_AND_INVERTED: return ~s & d;
    case GL_NOOP: return d;
    case GL_XOR: return s ^ d;
    case GL_OR: return s | d;
    case GL_NOR: return ~(s | d);
    case GL_EQUIV: return ~(s ^ d);
    case GL_INVERT: return ~d;
    case GL_OR_REVERSE: return s | ~d;
    case GL_COPY_INVERTED: return ~s;
    case GL_OR_INVERTED: return ~s | d;
    case GL_NAND: return ~(s & d);
    case GL_SET: return ~0u;
    }
    return s;
}
/* formato de escrita: sem GL_FRAMEBUFFER_SRGB o sRGB grava o valor sem conversão */
static const pf_t *write_pf(int fmt, int srgb_on) {
    if (srgb_on) return mx_pf(fmt);
    switch (fmt) {
    case 71: return mx_pf(70);
    case 81: return mx_pf(80);
    case 11: return mx_pf(10);
    case 31: return mx_pf(30);
    case 553: return mx_pf(552);
    case 555: return mx_pf(554);
    }
    return mx_pf(fmt);
}
static void color_out(rs_t *R, int x, int y, gl_frag_t *f) {
    gl_ctx_t *C = R->C;
    gl_target_t *T = R->T;
    int logic_on = gl_is_on(C, GL_COLOR_LOGIC_OP);
    for (int i = 0; i < T->ncolor; i++) {
        msl_tex_t *t = T->color[i];
        if (!t || !t->data) continue;
        const u8 *m = C->cmask[i];
        if (!m[0] && !m[1] && !m[2] && !m[3]) continue;
        const pf_t *p = write_pf(t->fmt, R->srgb_on);
        if (!p) continue;
        u8 *q = mx_texel(t, x, y, 0, 0, 0);
        double d[4];
        mx_decode(p, q, d);
        double s[4];
        if (T->isint[i]) {
            for (int k = 0; k < 4; k++) s[k] = f->icolor[i][k];
            if (p->kind == PK_U8 || p->kind == PK_U16 || p->kind == PK_U32 || p->kind == PK_RGB10A2U) for (int k = 0; k < 4; k++) s[k] = (double)(u32)f->icolor[i][k];
        } else {
            int fixed = !(p->kind == PK_F32 || p->kind == PK_HALF || p->kind == PK_RG11B10F || p->kind == PK_RGB9E5);
            for (int k = 0; k < 4; k++) s[k] = fixed ? mx_cl01(f->color[i][k]) : f->color[i][k];
            if (p->kind == PK_SN8 || p->kind == PK_SN16) for (int k = 0; k < 4; k++) s[k] = mx_clm11(f->color[i][k]);
            if (C->blend_on[i] && !logic_on) {
                double s1[4];
                for (int k = 0; k < 4; k++) s1[k] = fixed ? mx_cl01(f->color[1][k]) : f->color[1][k];
                double bc[4];
                float bcf[4];
                for (int k = 0; k < 4; k++) { bc[k] = fixed ? mx_cl01(C->bcolor[k]) : C->bcolor[k]; bcf[k] = (float)bc[k]; }
                double o[4];
                for (int k = 0; k < 3; k++)
                    o[k] = beq(C->beq_rgb[i], s[k], bfac(C->bsrc_rgb[i], k, s, s1, d, bcf), d[k], bfac(C->bdst_rgb[i], k, s, s1, d, bcf));
                o[3] = beq(C->beq_a[i], s[3], bfac(C->bsrc_a[i], 3, s, s1, d, bcf), d[3], bfac(C->bdst_a[i], 3, s, s1, d, bcf));
                for (int k = 0; k < 4; k++) s[k] = fixed ? mx_cl01(o[k]) : o[k];
            }
            if (logic_on && p->kind == PK_UN8) {
                for (int k = 0; k < 4; k++) {
                    u32 si = (u32)(mx_cl01(s[k]) * 255 + 0.5), di = (u32)(mx_cl01(d[k]) * 255 + 0.5);
                    s[k] = (logic(C->logic_op, si, di) & 255) / 255.0;
                }
            }
        }
        for (int k = 0; k < 4; k++) if (!m[k]) s[k] = d[k];
        mx_encode(p, q, s);
    }
}
/* fragmento já com profundidade interpolada em f->coord[2] */
static void frag(rs_t *R, int x, int y, gl_frag_t *f) {
    gl_ctx_t *C = R->C;
    gl_stage_t *S = R->S;
    double z = f->coord[2];
    if (!gl_is_on(C, GL_DEPTH_CLAMP) && (z < 0 || z > 1)) z = mx_cl01(z);
    f->coord[2] = (float)z;
    if (R->early) {
        if (!ds_test(R, x, y, z)) return;
        f->discard = 0;
        S->fs(C, f);
        if (f->discard) return;
        color_out(R, x, y, f);
        return;
    }
    f->discard = 0;
    f->depth = f->coord[2];
    S->fs(C, f);
    if (f->discard) return;
    if (S->alpha_test && gl_is_on(C, GL_ALPHA_TEST) && !cmpf(C->alpha_func, f->color[0][3], C->alpha_ref)) return;
    if (gl_is_on(C, GL_SAMPLE_ALPHA_TO_COVERAGE) && f->color[0][3] <= 0) return;
    z = S->writes_depth ? f->depth : f->coord[2];
    if (S->writes_depth) z = mx_cl01(z);
    if (!ds_test(R, x, y, z)) return;
    color_out(R, x, y, f);
}
void gl_pixel_write(gl_ctx_t *C, gl_target_t *T, int x, int y, gl_frag_t *f) {
    /* glDrawPixels/glBitmap: fragmento pronto (sem fragment shader) passa pelos mesmos testes */
    rs_t R = {0};
    R.C = C; R.T = T; R.S = &C->st;
    R.depth_on = gl_is_on(C, GL_DEPTH_TEST) && T->depth;
    R.stencil_on = gl_is_on(C, GL_STENCIL_TEST) && T->stencil;
    R.front = 1;
    R.srgb_on = gl_is_on(C, GL_FRAMEBUFFER_SRGB);
    if (x < 0 || y < 0 || x >= T->w || y >= T->h) return;
    if (gl_is_on(C, GL_SCISSOR_TEST) && (x < C->sc[0] || y < C->sc[1] || x >= C->sc[0] + C->sc[2] || y >= C->sc[1] + C->sc[3])) return;
    if (gl_is_on(C, GL_ALPHA_TEST) && !cmpf(C->alpha_func, f->color[0][3], C->alpha_ref)) return;
    if (!ds_test(&R, x, y, f->coord[2])) return;
    color_out(&R, x, y, f);
}

/* ================================================================ interpolação */
typedef struct { float x, y, z, iw; } wv_t;
static void interp_frag(rs_t *R, gl_frag_t *f, gl_vtx_t *const v[3], double b0, double b1, double b2) {
    gl_stage_t *S = R->S;
    double p0 = b0 * v[0]->win[3], p1 = b1 * v[1]->win[3], p2 = b2 * v[2]->win[3];
    double sum = p0 + p1 + p2;
    if (sum == 0) sum = 1e-30;
    double q0 = p0 / sum, q1 = p1 / sum, q2 = p2 / sum;
    f->coord[2] = (float)(b0 * v[0]->win[2] + b1 * v[1]->win[2] + b2 * v[2]->win[2]);
    f->coord[3] = (float)(b0 * v[0]->win[3] + b1 * v[1]->win[3] + b2 * v[2]->win[3]);
    for (int s = 0; s < S->nvar; s++) {
        int mode = S->interp[s];
        if (mode == MSL_INTERP_FLAT) { for (int k = 0; k < 4; k++) f->v[s][k] = R->flat_v[s][k]; continue; }
        double w0 = mode == MSL_INTERP_NOPERSP ? b0 : q0, w1 = mode == MSL_INTERP_NOPERSP ? b1 : q1, w2 = mode == MSL_INTERP_NOPERSP ? b2 : q2;
        for (int k = 0; k < 4; k++) f->v[s][k] = (float)(w0 * v[0]->v[s][k] + w1 * v[1]->v[s][k] + w2 * v[2]->v[s][k]);
    }
}
/* valor de um slot em (px, py) qualquer, para as derivadas */
static void interp_at(gl_vtx_t *const v[3], double A, const double e[3][3], double px, double py, int slot, int mode, float out[4]) {
    double b[3];
    for (int i = 0; i < 3; i++) b[i] = (e[i][0] * px + e[i][1] * py + e[i][2]) / A;
    double p[3], sum = 0;
    for (int i = 0; i < 3; i++) { p[i] = mode == MSL_INTERP_NOPERSP ? b[i] : b[i] * v[i]->win[3]; sum += p[i]; }
    if (sum == 0) sum = 1e-30;
    for (int k = 0; k < 4; k++) out[k] = (float)((p[0] * v[0]->v[slot][k] + p[1] * v[1]->v[slot][k] + p[2] * v[2]->v[slot][k]) / sum);
}

/* ================================================================ triângulo */
static void set_flat(rs_t *R, gl_vtx_t *pv) { for (int s = 0; s < R->S->nvar; s++) for (int k = 0; k < 4; k++) R->flat_v[s][k] = pv->v[s][k]; }
static int top_left(double ex, double ey) { return ey < 0 || (ey == 0 && ex < 0); }   /* y para cima, sentido anti-horário */
static void raster_tri(rs_t *R, gl_vtx_t *a, gl_vtx_t *b, gl_vtx_t *c, double zoff) {
    gl_vtx_t *v[3] = {a, b, c};
    double x[3], y[3];
    for (int i = 0; i < 3; i++) { x[i] = v[i]->win[0]; y[i] = v[i]->win[1]; }
    double A = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (A == 0 || A != A) return;
    if (A < 0) {
        /* orientação positiva para as funções de aresta */
        gl_vtx_t *t = v[1]; v[1] = v[2]; v[2] = t;
        double tx = x[1]; x[1] = x[2]; x[2] = tx;
        double ty = y[1]; y[1] = y[2]; y[2] = ty;
        A = -A;
    }
    /* e_i(p) = área oposta ao vértice i */
    double e[3][3];
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3, k = (i + 2) % 3;
        e[i][0] = y[j] - y[k];
        e[i][1] = x[k] - x[j];
        e[i][2] = x[j] * y[k] - x[k] * y[j];
    }
    int bias[3];
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3, k = (i + 2) % 3;
        bias[i] = top_left(x[k] - x[j], y[k] - y[j]) ? 0 : 1;
    }
    double minx = x[0], maxx = x[0], miny = y[0], maxy = y[0];
    for (int i = 1; i < 3; i++) {
        if (x[i] < minx) minx = x[i];
        if (x[i] > maxx) maxx = x[i];
        if (y[i] < miny) miny = y[i];
        if (y[i] > maxy) maxy = y[i];
    }
    int ix0 = (int)floor(minx), ix1 = (int)ceil(maxx), iy0 = (int)floor(miny), iy1 = (int)ceil(maxy);
    if (ix0 < R->x0) ix0 = R->x0;
    if (iy0 < R->y0) iy0 = R->y0;
    if (ix1 > R->x1) ix1 = R->x1;
    if (iy1 > R->y1) iy1 = R->y1;
    if (ix0 >= ix1 || iy0 >= iy1) return;
    gl_stage_t *S = R->S;
    gl_frag_t f;
    f.front = R->front;
    f.prim_id = R->prim;
    f.pcoord[0] = f.pcoord[1] = 0;
    for (int py = iy0; py < iy1; py++) {
        double cy = py + 0.5;
        for (int px = ix0; px < ix1; px++) {
            double cx = px + 0.5;
            double w0 = e[0][0] * cx + e[0][1] * cy + e[0][2];
            double w1 = e[1][0] * cx + e[1][1] * cy + e[1][2];
            double w2 = e[2][0] * cx + e[2][1] * cy + e[2][2];
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            if ((w0 == 0 && bias[0]) || (w1 == 0 && bias[1]) || (w2 == 0 && bias[2])) continue;
            double b0 = w0 / A, b1 = w1 / A, b2 = w2 / A;
            f.coord[0] = (float)cx;
            f.coord[1] = (float)cy;
            interp_frag(R, &f, v, b0, b1, b2);
            f.coord[2] = (float)(f.coord[2] + zoff);
            if (S->dmask) {
                for (int s = 0; s < S->nvar; s++) {
                    if (!(S->dmask >> s & 1)) continue;
                    float a1[4], a2[4];
                    interp_at(v, A, e, cx + 1, cy, s, S->interp[s], a1);
                    interp_at(v, A, e, cx, cy + 1, s, S->interp[s], a2);
                    for (int k = 0; k < 4; k++) { f.dx[s][k] = a1[k] - f.v[s][k]; f.dy[s][k] = a2[k] - f.v[s][k]; }
                }
            }
            frag(R, px, py, &f);
        }
    }
}

/* ================================================================ linha e ponto */
static void raster_line(rs_t *R, gl_vtx_t *a, gl_vtx_t *b, double zoff, int last_pixel) {
    gl_ctx_t *C = R->C;
    gl_stage_t *S = R->S;
    double x0 = a->win[0], y0 = a->win[1], x1 = b->win[0], y1 = b->win[1];
    double dx = x1 - x0, dy = y1 - y0;
    int xmajor = fabs(dx) >= fabs(dy);
    double len = xmajor ? fabs(dx) : fabs(dy);
    int wdt = (int)(C->line_w + 0.5);
    if (wdt < 1) wdt = 1;
    /* pontos de amostra no eixo maior: centros de pixel entre as extremidades (regra do diamante aproximada) */
    double s0 = xmajor ? x0 : y0, s1 = xmajor ? x1 : y1;
    int dir = s1 >= s0 ? 1 : -1;
    int i0 = (int)floor(s0 + 0.5 * dir * 0 + (dir > 0 ? 0.5 : -0.5) * 0), i1;
    /* primeiro centro c com c >= s0 (dir > 0) */
    if (dir > 0) { i0 = (int)ceil(s0 - 0.5); i1 = (int)ceil(s1 - 0.5); if (last_pixel) i1++; }
    else { i0 = (int)floor(s0 - 0.5); i1 = (int)floor(s1 - 0.5); if (last_pixel) i1--; }
    gl_frag_t f;
    f.front = 1;
    f.prim_id = R->prim;
    f.pcoord[0] = f.pcoord[1] = 0;
    for (int i = i0; dir > 0 ? i < i1 : i > i1; i += dir) {
        double c = i + 0.5;
        double t = len > 0 ? (c - s0) / (s1 - s0) : 0;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        double minor = xmajor ? y0 + dy * t : x0 + dx * t;
        /* correção de perspectiva no parâmetro */
        double iw = a->win[3] * (1 - t) + b->win[3] * t;
        double tp = iw != 0 ? b->win[3] * t / iw : t;
        f.coord[2] = (float)(a->win[2] * (1 - t) + b->win[2] * t + zoff);
        f.coord[3] = (float)iw;
        for (int s = 0; s < S->nvar; s++) {
            int mode = S->interp[s];
            if (mode == MSL_INTERP_FLAT) { for (int k = 0; k < 4; k++) f.v[s][k] = R->flat_v[s][k]; continue; }
            double u = mode == MSL_INTERP_NOPERSP ? t : tp;
            for (int k = 0; k < 4; k++) f.v[s][k] = (float)(a->v[s][k] * (1 - u) + b->v[s][k] * u);
        }
        if (S->dmask) for (int s = 0; s < S->nvar; s++) if (S->dmask >> s & 1) for (int k = 0; k < 4; k++) {
            double dv = (b->v[s][k] - a->v[s][k]) / (len > 0 ? len : 1);
            f.dx[s][k] = (float)(xmajor ? dv : 0);
            f.dy[s][k] = (float)(xmajor ? 0 : dv);
        }
        int m0 = (int)floor(minor - (wdt - 1) / 2.0);
        for (int w = 0; w < wdt; w++) {
            int mm = m0 + w;
            int px = xmajor ? i : mm, py = xmajor ? mm : i;
            if (px < R->x0 || py < R->y0 || px >= R->x1 || py >= R->y1) continue;
            f.coord[0] = px + 0.5f;
            f.coord[1] = py + 0.5f;
            frag(R, px, py, &f);
        }
    }
}
static void raster_point(rs_t *R, gl_vtx_t *a, double zoff) {
    gl_ctx_t *C = R->C;
    gl_stage_t *S = R->S;
    double size = a->psize;
    if (size < 1) size = 1;
    if (size > 1024) size = 1024;
    double cx = a->win[0], cy = a->win[1];
    int x0 = (int)floor(cx - size / 2 + 0.5), y0 = (int)floor(cy - size / 2 + 0.5);
    int n = (int)(size + 0.5);
    if (n < 1) n = 1;
    gl_frag_t f;
    f.front = 1;
    f.prim_id = R->prim;
    set_flat(R, a);
    for (int s = 0; s < S->nvar; s++) for (int k = 0; k < 4; k++) { f.v[s][k] = a->v[s][k]; f.dx[s][k] = f.dy[s][k] = 0; }
    for (int py = y0; py < y0 + n; py++)
        for (int px = x0; px < x0 + n; px++) {
            if (px < R->x0 || py < R->y0 || px >= R->x1 || py >= R->y1) continue;
            f.coord[0] = px + 0.5f;
            f.coord[1] = py + 0.5f;
            f.coord[2] = (float)(a->win[2] + zoff);
            f.coord[3] = a->win[3];
            f.pcoord[0] = (float)(0.5 + (px + 0.5 - cx) / size);
            double t = (py + 0.5 - cy) / size;
            f.pcoord[1] = (float)(C->point_origin == GL_UPPER_LEFT ? 0.5 - t : 0.5 + t);
            if (S->point_sprite) {
                /* fixed function: GL_COORD_REPLACE troca a coordenada de textura da unidade */
                for (int u = 0; u < 8; u++) if (C->te[u].coord_replace && 4 + u < S->nvar) { f.v[4 + u][0] = f.pcoord[0]; f.v[4 + u][1] = f.pcoord[1]; f.v[4 + u][2] = 0; f.v[4 + u][3] = 1; }
            }
            for (int s = 0; s < S->nvar; s++) if (S->interp[s] == MSL_INTERP_FLAT) for (int k = 0; k < 4; k++) f.v[s][k] = R->flat_v[s][k];
            frag(R, px, py, &f);
        }
}

/* ================================================================ recorte */
#define MAXPOLY 24
static void lerp_v(gl_vtx_t *o, const gl_vtx_t *a, const gl_vtx_t *b, double t, int nvar, int nclip) {
    for (int k = 0; k < 4; k++) o->clip[k] = (float)(a->clip[k] + (b->clip[k] - a->clip[k]) * t);
    for (int s = 0; s < nvar; s++) for (int k = 0; k < 4; k++) o->v[s][k] = (float)(a->v[s][k] + (b->v[s][k] - a->v[s][k]) * t);
    for (int c = 0; c < nclip; c++) o->cd[c] = (float)(a->cd[c] + (b->cd[c] - a->cd[c]) * t);
    o->psize = a->psize;
    o->edge = a->edge;
}
static double plane_d(const gl_vtx_t *v, int p, int depth_clamp) {
    const float *c = v->clip;
    switch (p) {
    case 0: return c[3] + c[0];
    case 1: return c[3] - c[0];
    case 2: return c[3] + c[1];
    case 3: return c[3] - c[1];
    case 4: return depth_clamp ? 1 : c[3] + c[2];
    case 5: return depth_clamp ? 1 : c[3] - c[2];
    }
    return v->cd[p - 6];
}
static int clip_poly(rs_t *R, gl_vtx_t *in, int n, gl_vtx_t *tmp, gl_vtx_t **out) {
    int dc = gl_is_on(R->C, GL_DEPTH_CLAMP);
    gl_vtx_t *cur = in, *nxt = tmp;
    int nvar = R->S->nvar, nclip = R->nclip;
    for (int p = 0; p < 6 + GLX_CLIP; p++) {
        if (p >= 6 && !(R->clip_mask >> (p - 6) & 1)) continue;
        int all_in = 1;
        for (int i = 0; i < n; i++) if (plane_d(&cur[i], p, dc) < 0) { all_in = 0; break; }
        if (all_in) continue;
        int m = 0;
        for (int i = 0; i < n && m < MAXPOLY - 1; i++) {
            gl_vtx_t *a = &cur[i], *b = &cur[(i + 1) % n];
            double da = plane_d(a, p, dc), db = plane_d(b, p, dc);
            if (da >= 0) nxt[m++] = *a;
            if ((da >= 0) != (db >= 0) && m < MAXPOLY) {
                double t = da / (da - db);
                /* interpola sempre da aresta de dentro para fora para dar o mesmo vértice nos dois triângulos vizinhos */
                if (da >= 0) lerp_v(&nxt[m], a, b, t, nvar, nclip);
                else lerp_v(&nxt[m], b, a, 1 - t, nvar, nclip);
                nxt[m].edge = da >= 0 ? a->edge : 0;
                m++;
            }
        }
        n = m;
        gl_vtx_t *sw = cur; cur = nxt; nxt = sw;
        if (n < 3) return 0;
    }
    *out = cur;
    return n;
}
static void to_window(rs_t *R, gl_vtx_t *v) {
    gl_ctx_t *C = R->C;
    double w = v->clip[3];
    double iw = w != 0 ? 1.0 / w : 0;
    double nx = v->clip[0] * iw, ny = v->clip[1] * iw, nz = v->clip[2] * iw;
    v->win[0] = (float)((nx + 1) * 0.5 * C->vp[2] + C->vp[0]);
    v->win[1] = (float)((ny + 1) * 0.5 * C->vp[3] + C->vp[1]);
    v->win[2] = (float)(nz * 0.5 * (C->dr[1] - C->dr[0]) + (C->dr[1] + C->dr[0]) * 0.5);
    v->win[3] = (float)iw;
}

/* ---- seleção e feedback (compat) */
static void sel_hit(gl_ctx_t *C, gl_vtx_t **v, int n) {
    for (int i = 0; i < n; i++) {
        float z = v[i]->win[2];
        if (!C->sel_hit || z < C->sel_min) C->sel_min = z;
        if (!C->sel_hit || z > C->sel_max) C->sel_max = z;
        C->sel_hit = 1;
    }
}
static void fb_put(gl_ctx_t *C, float v) { if (C->fb_buf && C->fb_n < C->fb_size) C->fb_buf[C->fb_n] = v; C->fb_n++; }
static void fb_vertex(gl_ctx_t *C, gl_vtx_t *v) {
    GLenum t = C->fb_type;
    fb_put(C, v->win[0]);
    fb_put(C, v->win[1]);
    if (t != GL_2D) fb_put(C, v->win[2]);
    if (t == GL_4D_COLOR_TEXTURE) fb_put(C, v->win[3] ? 1.0f / v->win[3] : 0);
    if (t == GL_3D_COLOR || t == GL_3D_COLOR_TEXTURE || t == GL_4D_COLOR_TEXTURE) for (int k = 0; k < 4; k++) fb_put(C, v->v[0][k]);
    if (t == GL_3D_COLOR_TEXTURE || t == GL_4D_COLOR_TEXTURE) for (int k = 0; k < 4; k++) fb_put(C, v->v[4][k]);
}
static void feedback(gl_ctx_t *C, gl_vtx_t **v, int n) {
    fb_put(C, n == 1 ? GL_POINT_TOKEN : n == 2 ? GL_LINE_TOKEN : GL_POLYGON_TOKEN);
    if (n > 2) fb_put(C, (float)n);
    for (int i = 0; i < n; i++) fb_vertex(C, v[i]);
}

/* ---- primitivos depois da montagem */
static double poly_offset(rs_t *R, gl_vtx_t **v) {
    gl_ctx_t *C = R->C;
    double x1 = v[1]->win[0] - v[0]->win[0], y1 = v[1]->win[1] - v[0]->win[1], z1 = v[1]->win[2] - v[0]->win[2];
    double x2 = v[2]->win[0] - v[0]->win[0], y2 = v[2]->win[1] - v[0]->win[1], z2 = v[2]->win[2] - v[0]->win[2];
    double a = x1 * y2 - x2 * y1;
    double m = 0;
    if (a != 0) {
        double dzdx = (z1 * y2 - z2 * y1) / a, dzdy = (x1 * z2 - x2 * z1) / a;
        m = fabs(dzdx) > fabs(dzdy) ? fabs(dzdx) : fabs(dzdy);
    }
    double o = m * C->poff_factor + C->poff_units * (1.0 / 16777216.0);
    if (C->poff_clamp > 0 && o > C->poff_clamp) o = C->poff_clamp;
    if (C->poff_clamp < 0 && o < C->poff_clamp) o = C->poff_clamp;
    return o;
}
static void tri_post(rs_t *R, gl_vtx_t **pv, int n, gl_vtx_t *provoke) {
    gl_ctx_t *C = R->C;
    for (int i = 0; i < n; i++) to_window(R, pv[i]);
    double area = 0;
    for (int i = 0; i < n; i++) {
        gl_vtx_t *a = pv[i], *b = pv[(i + 1) % n];
        area += (double)a->win[0] * b->win[1] - (double)b->win[0] * a->win[1];
    }
    int ccw = area > 0;
    R->front = C->front_face == GL_CCW ? ccw : !ccw;
    if (R->cull_on && (C->cull_mode == GL_FRONT_AND_BACK || (C->cull_mode == GL_FRONT && R->front) || (C->cull_mode == GL_BACK && !R->front))) return;
    if (R->sel == 1) { sel_hit(C, pv, n); return; }
    if (R->sel == 2) { feedback(C, pv, n); return; }
    set_flat(R, provoke);
    if (R->S->two_side && !R->front) {
        /* fixed function com luz dos dois lados: slots 2..3 têm as cores do verso */
        for (int i = 0; i < n; i++) for (int k = 0; k < 4; k++) { pv[i]->v[0][k] = pv[i]->v[2][k]; pv[i]->v[1][k] = pv[i]->v[3][k]; }
        for (int k = 0; k < 4; k++) { R->flat_v[0][k] = provoke->v[2][k]; R->flat_v[1][k] = provoke->v[3][k]; }
    }
    GLenum mode = C->pmode[R->front ? 0 : 1];
    if (mode == GL_FILL) {
        double zo = R->poff_fill && n >= 3 ? poly_offset(R, pv) : 0;
        for (int i = 1; i + 1 < n; i++) raster_tri(R, pv[0], pv[i], pv[i + 1], zo);
    } else if (mode == GL_LINE) {
        double zo = R->poff_line && n >= 3 ? poly_offset(R, pv) : 0;
        for (int i = 0; i < n; i++) if (pv[i]->edge) raster_line(R, pv[i], pv[(i + 1) % n], zo, 1);
    } else {
        double zo = R->poff_point && n >= 3 ? poly_offset(R, pv) : 0;
        for (int i = 0; i < n; i++) if (pv[i]->edge) { float ps = pv[i]->psize; pv[i]->psize = C->point_s; raster_point(R, pv[i], zo); pv[i]->psize = ps; }
    }
}
static void emit_tri(rs_t *R, gl_vtx_t *a, gl_vtx_t *b, gl_vtx_t *c, gl_vtx_t *provoke) {
    R->C->prims_generated++;
    R->prim++;
    gl_vtx_t in[MAXPOLY], tmp[MAXPOLY];
    in[0] = *a; in[1] = *b; in[2] = *c;
    gl_vtx_t pk = *provoke;
    gl_vtx_t *out;
    int n = clip_poly(R, in, 3, tmp, &out);
    if (n < 3) return;
    gl_vtx_t *pv[MAXPOLY];
    for (int i = 0; i < n; i++) pv[i] = &out[i];
    tri_post(R, pv, n, &pk);
}
static void emit_line(rs_t *R, gl_vtx_t *a, gl_vtx_t *b, gl_vtx_t *provoke, int last) {
    gl_ctx_t *C = R->C;
    C->prims_generated++;
    R->prim++;
    gl_vtx_t va = *a, vb = *b;
    double t0 = 0, t1 = 1;
    int dc = gl_is_on(C, GL_DEPTH_CLAMP);
    for (int p = 0; p < 6 + GLX_CLIP; p++) {
        if (p >= 6 && !(R->clip_mask >> (p - 6) & 1)) continue;
        double da = plane_d(a, p, dc), db = plane_d(b, p, dc);
        if (da < 0 && db < 0) return;
        if (da < 0) { double t = da / (da - db); if (t > t0) t0 = t; }
        else if (db < 0) { double t = da / (da - db); if (t < t1) t1 = t; }
    }
    if (t0 > t1) return;
    int nvar = R->S->nvar, nclip = R->nclip;
    if (t0 > 0) lerp_v(&va, a, b, t0, nvar, nclip);
    if (t1 < 1) { lerp_v(&vb, a, b, t1, nvar, nclip); last = 0; }
    to_window(R, &va);
    to_window(R, &vb);
    gl_vtx_t *pv[2] = {&va, &vb};
    if (R->sel == 1) { sel_hit(C, pv, 2); return; }
    if (R->sel == 2) { feedback(C, pv, 2); return; }
    R->front = 1;
    set_flat(R, provoke);
    raster_line(R, &va, &vb, 0, last);
}
static void emit_point(rs_t *R, gl_vtx_t *a) {
    gl_ctx_t *C = R->C;
    C->prims_generated++;
    R->prim++;
    int dc = gl_is_on(C, GL_DEPTH_CLAMP);
    for (int p = 0; p < 6 + GLX_CLIP; p++) {
        if (p >= 6 && !(R->clip_mask >> (p - 6) & 1)) continue;
        if (plane_d(a, p, dc) < 0) return;
    }
    gl_vtx_t v = *a;
    to_window(R, &v);
    gl_vtx_t *pv[1] = {&v};
    if (R->sel == 1) { sel_hit(C, pv, 1); return; }
    if (R->sel == 2) { feedback(C, pv, 1); return; }
    R->front = 1;
    raster_point(R, &v, 0);
}

/* ================================================================ transform feedback */
static GLenum xfb_base(GLenum mode) {
    switch (mode) {
    case GL_POINTS: return GL_POINTS;
    case GL_LINES: case GL_LINE_STRIP: case GL_LINE_LOOP: case GL_LINES_ADJACENCY: case GL_LINE_STRIP_ADJACENCY: return GL_LINES;
    }
    return GL_TRIANGLES;
}
int gl_prog_xfb_layout(gl_ctx_t *C, int *interleaved, int *n, int *comps);
static void xfb_write(gl_ctx_t *C, gl_vtx_t **v, int n) {
    gl_xfb_t *X = C->xfb;
    if (!X->active || X->paused) return;
    int inter = 1, nv = 0, comps[64];
    if (!gl_prog_xfb_layout(C, &inter, &nv, comps)) return;
    int total = 0;
    for (int i = 0; i < nv; i++) total += comps[i];
    /* cabe o primitivo inteiro? */
    if (inter) {
        gl_buf_t *b = X->buf[0];
        if (!b || !b->data) return;
        i64 lim = X->size[0] ? X->off[0] + X->size[0] : b->size;
        if (lim > b->size) lim = b->size;
        i64 need = (i64)total * 4 * n;
        i64 pos = X->off[0] + (i64)X->written * total * 4;
        if (pos + need > lim) return;
        for (int k = 0; k < n; k++) memcpy(b->data + pos + (i64)k * total * 4, v[k]->xfb, (size_t)total * 4);
    } else {
        int base = 0;
        for (int i = 0; i < nv; i++) {
            gl_buf_t *b = X->buf[i];
            if (!b || !b->data) return;
            i64 lim = X->size[i] ? X->off[i] + X->size[i] : b->size;
            if (lim > b->size) lim = b->size;
            if (X->off[i] + ((i64)X->written + n) * comps[i] * 4 > lim) return;
            (void)base;
        }
        for (int i = 0; i < nv; i++) {
            gl_buf_t *b = X->buf[i];
            for (int k = 0; k < n; k++) memcpy(b->data + X->off[i] + ((i64)X->written + k) * comps[i] * 4, v[k]->xfb + base, (size_t)comps[i] * 4);
            base += comps[i];
        }
    }
    X->written += (u64)n;
    C->prims_written++;
}

/* ================================================================ vértices e montagem */
#define VCACHE 64
typedef struct { gl_vtx_t v; u32 key; int ok; } vc_t;
typedef struct {
    rs_t *R;
    vc_t *cache;
    u32 iid;
    GLenum mode, xmode;
    int capture;
} asm_t;
static gl_vtx_t *vtx(asm_t *A, u32 e) {
    vc_t *c = &A->cache[e % VCACHE];
    if (c->ok && c->key == e) return &c->v;
    gl_ctx_t *C = A->R->C;
    c->v.edge = 1;
    c->v.nxfb = 0;
    A->R->S->vs(C, e, A->iid, &c->v);
    if (!c->v.edge) c->v.edge = 0;
    c->key = e;
    c->ok = 1;
    return &c->v;
}
/* o primitivo pode usar vértices que o cache vai trocar: copia para a pilha */
static void prim3(asm_t *A, u32 e0, u32 e1, u32 e2, int pvi) {
    gl_vtx_t a = *vtx(A, e0), b = *vtx(A, e1), c = *vtx(A, e2);
    gl_vtx_t *pv = pvi == 0 ? &a : pvi == 1 ? &b : &c;
    if (A->capture) { gl_vtx_t *v[3] = {&a, &b, &c}; xfb_write(A->R->C, v, 3); }
    if (A->R->sel >= 0) emit_tri(A->R, &a, &b, &c, pv);
}
static void prim2(asm_t *A, u32 e0, u32 e1, int pvi, int last) {
    gl_vtx_t a = *vtx(A, e0), b = *vtx(A, e1);
    if (A->capture) { gl_vtx_t *v[2] = {&a, &b}; xfb_write(A->R->C, v, 2); }
    if (A->R->sel >= 0) emit_line(A->R, &a, &b, pvi ? &b : &a, last);
}
static void prim1(asm_t *A, u32 e0) {
    gl_vtx_t a = *vtx(A, e0);
    if (A->capture) { gl_vtx_t *v[1] = {&a}; xfb_write(A->R->C, v, 1); }
    if (A->R->sel >= 0) emit_point(A->R, &a);
}
/* polígono / quad: arestas internas não aparecem no polygon mode GL_LINE */
static void prim_poly(asm_t *A, const u32 *e, int n, int pvi) {
    gl_vtx_t in[MAXPOLY], tmp[MAXPOLY];
    if (n > MAXPOLY - 8) n = MAXPOLY - 8;
    for (int i = 0; i < n; i++) in[i] = *vtx(A, e[i]);
    gl_ctx_t *C = A->R->C;
    if (A->capture) for (int i = 1; i + 1 < n; i++) { gl_vtx_t *v[3] = {&in[0], &in[i], &in[i + 1]}; xfb_write(C, v, 3); }
    if (A->R->sel < 0) return;
    C->prims_generated++;
    A->R->prim++;
    gl_vtx_t pk = in[pvi < n ? pvi : n - 1];
    gl_vtx_t *out;
    int m = clip_poly(A->R, in, n, tmp, &out);
    if (m < 3) return;
    gl_vtx_t *pv[MAXPOLY];
    for (int i = 0; i < m; i++) pv[i] = &out[i];
    tri_post(A->R, pv, m, &pk);
}
/* ---- geometry shader */
static rs_t *g_rs;   /* draw em andamento (o raster é síncrono) */
void gl_raster_gs_emit(gl_ctx_t *C, GLenum outprim, gl_vtx_t *v, const u8 *cut, int n) {
    rs_t *R = g_rs;
    (void)C;
    if (!R || R->sel < 0) return;
    int start = 0;
    for (int i = 0; i <= n; i++) {
        if (i < n && !(cut[i] && i > start)) continue;
        /* segmento [start, i) */
        int len = i - start;
        gl_vtx_t *s = v + start;
        if (outprim == GL_POINTS) for (int k = 0; k < len; k++) emit_point(R, &s[k]);
        else if (outprim == GL_LINE_STRIP) for (int k = 0; k + 1 < len; k++) { gl_vtx_t a = s[k], b = s[k + 1]; emit_line(R, &a, &b, &b, 0); }
        else for (int k = 2; k < len; k++) {
            gl_vtx_t a = s[k - 2], b = s[k - 1], c = s[k];
            if (k & 1) emit_tri(R, &b, &a, &c, &c); else emit_tri(R, &a, &b, &c, &c);
        }
        start = i;
    }
}
static void gs_prim(asm_t *A, const u32 *e, int k) {
    gl_vtx_t vs[6];
    gl_vtx_t *pv[6];
    for (int i = 0; i < k; i++) { vs[i] = *vtx(A, e[i]); pv[i] = &vs[i]; }
    A->R->C->prims_generated++;
    A->R->S->gs(A->R->C, pv, k, A->R->prim++);
}
static void gs_assemble(asm_t *A, const u32 *e, i64 n) {
    u32 q[6];
    switch (A->mode) {
    case GL_POINTS: for (i64 i = 0; i < n; i++) gs_prim(A, &e[i], 1); break;
    case GL_LINES: for (i64 i = 0; i + 1 < n; i += 2) gs_prim(A, &e[i], 2); break;
    case GL_LINE_STRIP: for (i64 i = 0; i + 1 < n; i++) gs_prim(A, &e[i], 2); break;
    case GL_LINE_LOOP:
        for (i64 i = 0; i + 1 < n; i++) gs_prim(A, &e[i], 2);
        if (n >= 2) { q[0] = e[n - 1]; q[1] = e[0]; gs_prim(A, q, 2); }
        break;
    case GL_TRIANGLES: for (i64 i = 0; i + 2 < n; i += 3) gs_prim(A, &e[i], 3); break;
    case GL_TRIANGLE_STRIP:
        for (i64 i = 0; i + 2 < n; i++) {
            if (i & 1) { q[0] = e[i + 1]; q[1] = e[i]; q[2] = e[i + 2]; } else { q[0] = e[i]; q[1] = e[i + 1]; q[2] = e[i + 2]; }
            gs_prim(A, q, 3);
        }
        break;
    case GL_TRIANGLE_FAN: for (i64 i = 1; i + 1 < n; i++) { q[0] = e[0]; q[1] = e[i]; q[2] = e[i + 1]; gs_prim(A, q, 3); } break;
    case GL_LINES_ADJACENCY: for (i64 i = 0; i + 3 < n; i += 4) gs_prim(A, &e[i], 4); break;
    case GL_LINE_STRIP_ADJACENCY: for (i64 i = 0; i + 3 < n; i++) gs_prim(A, &e[i], 4); break;
    case GL_TRIANGLES_ADJACENCY: for (i64 i = 0; i + 5 < n; i += 6) gs_prim(A, &e[i], 6); break;
    case GL_TRIANGLE_STRIP_ADJACENCY: {
        /* tabela da especificação (seção 10.1.13): vértices primários 2i, 2i+2, 2i+4 e as adjacências */
        i64 nt = (n - 4) / 2;
        for (i64 i = 0; i < nt; i++) {
            u32 v0, v1, v2, a0, a1, a2;
            if (i & 1) { v0 = e[2 * i + 2]; v1 = e[2 * i]; v2 = e[2 * i + 4]; }
            else { v0 = e[2 * i]; v1 = e[2 * i + 2]; v2 = e[2 * i + 4]; }
            if (nt == 1) { a0 = e[1]; a1 = e[5]; a2 = e[3]; }
            else if (i == 0) { a0 = e[1]; a1 = e[6]; a2 = e[3]; }
            else if (i == nt - 1) { a0 = (i & 1) ? e[2 * i + 3] : e[2 * i - 2]; a1 = e[2 * i + 5]; a2 = (i & 1) ? e[2 * i - 2] : e[2 * i + 3]; }
            else { a0 = (i & 1) ? e[2 * i + 6] : e[2 * i - 2]; a1 = (i & 1) ? e[2 * i - 2] : e[2 * i + 6]; a2 = e[2 * i + 3]; }
            q[0] = v0; q[1] = a0; q[2] = v1; q[3] = a1; q[4] = v2; q[5] = a2;
            gs_prim(A, q, 6);
        }
        break;
    }
    }
}
/* percorre a sequência de elementos (já sem os índices de reinício) */
static void assemble(asm_t *A, const u32 *e, i64 n) {
    rs_t *R = A->R;
    if (R->S->gs) { gs_assemble(A, e, n); return; }
    int last = R->flat_last;
    switch (A->mode) {
    case GL_POINTS: for (i64 i = 0; i < n; i++) prim1(A, e[i]); break;
    case GL_LINES: for (i64 i = 0; i + 1 < n; i += 2) prim2(A, e[i], e[i + 1], last, 0); break;
    case GL_LINE_STRIP: for (i64 i = 0; i + 1 < n; i++) prim2(A, e[i], e[i + 1], last, 0); break;
    case GL_LINE_LOOP:
        for (i64 i = 0; i + 1 < n; i++) prim2(A, e[i], e[i + 1], last, 0);
        if (n >= 2) prim2(A, e[n - 1], e[0], last, 0);
        break;
    case GL_TRIANGLES: for (i64 i = 0; i + 2 < n; i += 3) prim3(A, e[i], e[i + 1], e[i + 2], last ? 2 : 0); break;
    case GL_TRIANGLE_STRIP:
        for (i64 i = 0; i + 2 < n; i++) {
            if (i & 1) prim3(A, e[i + 1], e[i], e[i + 2], last ? 2 : 1);
            else prim3(A, e[i], e[i + 1], e[i + 2], last ? 2 : 0);
        }
        break;
    case GL_TRIANGLE_FAN: for (i64 i = 1; i + 1 < n; i++) prim3(A, e[0], e[i], e[i + 1], last ? 2 : 1); break;
    case GL_QUADS:
        for (i64 i = 0; i + 3 < n; i += 4) { u32 q[4] = {e[i], e[i + 1], e[i + 2], e[i + 3]}; prim_poly(A, q, 4, 3); }
        break;
    case GL_QUAD_STRIP:
        for (i64 i = 0; i + 3 < n; i += 2) { u32 q[4] = {e[i], e[i + 1], e[i + 3], e[i + 2]}; prim_poly(A, q, 4, 2); }
        break;
    case GL_POLYGON: {
        if (n < 3) break;
        u32 q[MAXPOLY];
        if (n <= MAXPOLY - 8) { for (i64 i = 0; i < n; i++) q[i] = e[i]; prim_poly(A, q, (int)n, 0); }
        else for (i64 i = 1; i + 1 < n; i++) prim3(A, e[0], e[i], e[i + 1], 0);
        break;
    }
    case GL_LINES_ADJACENCY: for (i64 i = 0; i + 3 < n; i += 4) prim2(A, e[i + 1], e[i + 2], last, 0); break;
    case GL_LINE_STRIP_ADJACENCY: for (i64 i = 1; i + 2 < n; i++) prim2(A, e[i], e[i + 1], last, 0); break;
    case GL_TRIANGLES_ADJACENCY: for (i64 i = 0; i + 5 < n; i += 6) prim3(A, e[i], e[i + 2], e[i + 4], last ? 2 : 0); break;
    case GL_TRIANGLE_STRIP_ADJACENCY:
        for (i64 i = 0; 2 * i + 4 < n; i++) {
            if (i & 1) prim3(A, e[2 * i + 2], e[2 * i], e[2 * i + 4], last ? 2 : 0);
            else prim3(A, e[2 * i], e[2 * i + 2], e[2 * i + 4], last ? 2 : 0);
        }
        break;
    }
}
static int mode_ok(GLenum m) { return m <= GL_POLYGON || (m >= GL_LINES_ADJACENCY && m <= GL_TRIANGLE_STRIP_ADJACENCY) || m == GL_PATCHES; }
static u32 read_index(const u8 *p, GLenum t, i64 i) {
    if (t == GL_UNSIGNED_BYTE) return p[i];
    if (t == GL_UNSIGNED_SHORT) { u16 v; memcpy(&v, p + 2 * i, 2); return v; }
    u32 v;
    memcpy(&v, p + 4 * i, 4);
    return v;
}
void gl_query_tick_time(gl_ctx_t *C);
void gl_draw(gl_ctx_t *C, GLenum mode, i64 first, i64 count, GLenum itype, const void *indices, i64 inst, i64 binst, i64 bvert) {
    (void)binst;
    if (!mode_ok(mode)) { gl_err(C, GL_INVALID_ENUM); return; }
    if (count < 0 || inst < 0 || first < 0) { gl_err(C, GL_INVALID_VALUE); return; }
    if (C->in_begin) { gl_err(C, GL_INVALID_OPERATION); return; }
    if (mode == GL_PATCHES) { gl_err(C, GL_INVALID_OPERATION); return; }   /* sem shader de tesselação ligado */
    if (C->xfb->active && !C->xfb->paused && xfb_base(mode) != C->xfb->prim) { gl_err(C, GL_INVALID_OPERATION); return; }
    if (!count || !inst) return;
    { extern int gl_cond_skip(void); if (gl_cond_skip()) return; }
    if (!gl_resolve_target(C, 1, &C->tgt)) { gl_err(C, GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    if (!gl_stage_setup(C)) return;
    if (C->st.gs) {
        /* o modo do draw tem de ser da família do primitivo de entrada do GS */
        GLenum fam = mode == GL_POINTS ? GL_POINTS : mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP ? GL_LINES :
                     mode == GL_LINES_ADJACENCY || mode == GL_LINE_STRIP_ADJACENCY ? GL_LINES_ADJACENCY :
                     mode == GL_TRIANGLES_ADJACENCY || mode == GL_TRIANGLE_STRIP_ADJACENCY ? GL_TRIANGLES_ADJACENCY :
                     mode == GL_TRIANGLES || mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN ? GL_TRIANGLES : 0;
        if (fam != C->st.gs_in) { gl_err(C, GL_INVALID_OPERATION); return; }
    }
    const u8 *ip = 0;
    if (itype) {
        int isz = itype == GL_UNSIGNED_BYTE ? 1 : itype == GL_UNSIGNED_SHORT ? 2 : itype == GL_UNSIGNED_INT ? 4 : 0;
        if (!isz) { gl_err(C, GL_INVALID_ENUM); return; }
        gl_buf_t *eb = C->vao->ebo;
        if (eb) {
            i64 off = (i64)(uintptr_t)indices;
            if (!eb->data || off < 0 || off + count * isz > eb->size) { gl_err(C, GL_INVALID_OPERATION); return; }
            ip = eb->data + off;
        } else {
            if (C->profile_core) { gl_err(C, GL_INVALID_OPERATION); return; }
            ip = indices;
            if (!ip) return;
        }
    }
    rs_t R = {0};
    gl_target_t *T = &C->tgt;
    R.C = C;
    R.T = T;
    R.S = &C->st;
    R.depth_on = gl_is_on(C, GL_DEPTH_TEST) && T->depth;
    R.stencil_on = gl_is_on(C, GL_STENCIL_TEST) && T->stencil;
    R.cull_on = gl_is_on(C, GL_CULL_FACE);
    R.poff_fill = gl_is_on(C, GL_POLYGON_OFFSET_FILL);
    R.poff_line = gl_is_on(C, GL_POLYGON_OFFSET_LINE);
    R.poff_point = gl_is_on(C, GL_POLYGON_OFFSET_POINT);
    R.flat_last = C->provoke == GL_LAST_VERTEX_CONVENTION;
    R.srgb_on = gl_is_on(C, GL_FRAMEBUFFER_SRGB);
    R.nclip = C->st.nclip;
    for (int i = 0; i < GLX_CLIP; i++) if (gl_is_on(C, GL_CLIP_DISTANCE0 + i)) R.clip_mask |= 1u << i;
    R.early = !C->st.writes_depth && !C->st.discards && !(C->st.alpha_test && gl_is_on(C, GL_ALPHA_TEST)) && !gl_is_on(C, GL_SAMPLE_ALPHA_TO_COVERAGE);
    R.x0 = 0; R.y0 = 0; R.x1 = T->w; R.y1 = T->h;
    if (gl_is_on(C, GL_SCISSOR_TEST)) {
        if (C->sc[0] > R.x0) R.x0 = C->sc[0];
        if (C->sc[1] > R.y0) R.y0 = C->sc[1];
        if (C->sc[0] + C->sc[2] < R.x1) R.x1 = C->sc[0] + C->sc[2];
        if (C->sc[1] + C->sc[3] < R.y1) R.y1 = C->sc[1] + C->sc[3];
    }
    R.sel = C->render_mode == GL_SELECT ? 1 : C->render_mode == GL_FEEDBACK ? 2 : 0;
    if (gl_is_on(C, GL_RASTERIZER_DISCARD)) R.sel = -1;
    /* quads/polígonos em compat usam o último vértice como provocante (quads) e o primeiro (polígono) */
    vc_t *cache = calloc(VCACHE, sizeof *cache);
    u32 *seq = malloc(sizeof(u32) * (size_t)(count > 4096 ? 4096 : count) + 64);
    if (!cache || !seq) { free(cache); free(seq); gl_err(C, GL_OUT_OF_MEMORY); return; }
    asm_t A = {&R, cache, 0, mode, xfb_base(mode), C->xfb->active && !C->xfb->paused && !C->st.gs};
    rs_t *prev_rs = g_rs;
    g_rs = &R;
    int restart = itype && gl_is_on(C, GL_PRIMITIVE_RESTART);
    u32 ridx = C->restart_idx;
    for (i64 k = 0; k < inst; k++) {
        A.iid = (u32)k;
        for (int i = 0; i < VCACHE; i++) cache[i].ok = 0;
        R.prim = 0;
        /* a sequência vai em blocos; strips/fans/loops/polígonos precisam dela inteira entre reinícios */
        int whole = mode != GL_POINTS && mode != GL_LINES && mode != GL_TRIANGLES && mode != GL_QUADS && mode != GL_LINES_ADJACENCY && mode != GL_TRIANGLES_ADJACENCY;
        u32 *buf = seq;
        i64 cap = count > 4096 ? 4096 : count, n = 0;
        int unit = mode == GL_LINES ? 2 : mode == GL_TRIANGLES ? 3 : mode == GL_QUADS || mode == GL_LINES_ADJACENCY ? 4 : mode == GL_TRIANGLES_ADJACENCY ? 6 : 1;
        for (i64 i = 0; i < count; i++) {
            u32 e;
            if (ip) {
                u32 raw = read_index(ip, itype, i);
                if (restart && raw == ridx) { assemble(&A, buf, n); n = 0; continue; }
                e = (u32)((i64)raw + bvert);
            } else e = (u32)(first + i);
            if (n == cap) {
                if (whole) {
                    cap *= 2;
                    u32 *nb = realloc(buf, sizeof(u32) * (size_t)cap + 64);
                    if (!nb) break;
                    buf = nb;
                } else {
                    i64 keep = n - n % unit;
                    assemble(&A, buf, keep);
                    memmove(buf, buf + keep, sizeof(u32) * (size_t)(n - keep));
                    n -= keep;
                }
            }
            buf[n++] = e;
        }
        assemble(&A, buf, n);
        seq = buf;
    }
    free(seq);
    free(cache);
    g_rs = prev_rs;
}

/* ================================================================ chamadas de desenho */
void gl_ffp_flush_begin(gl_ctx_t *C);
void glDrawArrays(GLenum mode, GLint first, GLsizei count) { GLC(); gl_draw(C, mode, first, count, 0, 0, 1, 0, 0); }
void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei inst) { GLC(); gl_draw(C, mode, first, count, 0, 0, inst, 0, 0); }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *idx) { GLC(); gl_draw(C, mode, 0, count, type, idx, 1, 0, 0); }
void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *idx, GLsizei inst) { GLC(); gl_draw(C, mode, 0, count, type, idx, inst, 0, 0); }
void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *idx, GLint bv) { GLC(); gl_draw(C, mode, 0, count, type, idx, 1, 0, bv); }
void glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *idx, GLsizei inst, GLint bv) { GLC(); gl_draw(C, mode, 0, count, type, idx, inst, 0, bv); }
void glDrawRangeElements(GLenum mode, GLuint s, GLuint e, GLsizei count, GLenum type, const void *idx) {
    GLC();
    if (e < s) GLERR(GL_INVALID_VALUE, );
    gl_draw(C, mode, 0, count, type, idx, 1, 0, 0);
}
void glDrawRangeElementsBaseVertex(GLenum mode, GLuint s, GLuint e, GLsizei count, GLenum type, const void *idx, GLint bv) {
    GLC();
    if (e < s) GLERR(GL_INVALID_VALUE, );
    gl_draw(C, mode, 0, count, type, idx, 1, 0, bv);
}
void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei n) { for (int i = 0; i < n; i++) glDrawArrays(mode, first[i], count[i]); }
void glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const *idx, GLsizei n) { for (int i = 0; i < n; i++) glDrawElements(mode, count[i], type, idx[i]); }
void glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const void *const *idx, GLsizei n, const GLint *bv) { for (int i = 0; i < n; i++) glDrawElementsBaseVertex(mode, count[i], type, idx[i], bv[i]); }
void glMultiDrawArraysEXT(GLenum mode, const GLint *first, const GLsizei *count, GLsizei n) { glMultiDrawArrays(mode, first, count, n); }
void glMultiDrawElementsEXT(GLenum mode, const GLsizei *count, GLenum type, const void *const *idx, GLsizei n) { glMultiDrawElements(mode, count, type, idx, n); }
void glDrawArraysInstancedARB(GLenum m, GLint f, GLsizei c, GLsizei p) { glDrawArraysInstanced(m, f, c, p); }
void glDrawElementsInstancedARB(GLenum m, GLsizei c, GLenum t, const void *i, GLsizei p) { glDrawElementsInstanced(m, c, t, i, p); }
void glDrawArraysInstancedEXT(GLenum m, GLint f, GLsizei c, GLsizei p) { glDrawArraysInstanced(m, f, c, p); }
void glDrawElementsInstancedEXT(GLenum m, GLsizei c, GLenum t, const void *i, GLsizei p) { glDrawElementsInstanced(m, c, t, i, p); }
void glDrawRangeElementsEXT(GLenum m, GLuint s, GLuint e, GLsizei c, GLenum t, const void *i) { glDrawRangeElements(m, s, e, c, t, i); }
typedef struct { u32 count, inst, first, base; } dai_t;
typedef struct { u32 count, inst, first; i32 bv; u32 base; } dei_t;
void glDrawArraysIndirect(GLenum mode, const void *ind) {
    GLC();
    gl_buf_t *b = C->b_indirect;
    if (!b || !b->data) GLERR(GL_INVALID_OPERATION, );
    i64 off = (i64)(uintptr_t)ind;
    if (off < 0 || off + (i64)sizeof(dai_t) > b->size || (off & 3)) GLERR(GL_INVALID_OPERATION, );
    dai_t d;
    memcpy(&d, b->data + off, sizeof d);
    gl_draw(C, mode, d.first, d.count, 0, 0, d.inst, d.base, 0);
}
void glDrawElementsIndirect(GLenum mode, GLenum type, const void *ind) {
    GLC();
    gl_buf_t *b = C->b_indirect;
    if (!b || !b->data) GLERR(GL_INVALID_OPERATION, );
    i64 off = (i64)(uintptr_t)ind;
    if (off < 0 || off + (i64)sizeof(dei_t) > b->size || (off & 3)) GLERR(GL_INVALID_OPERATION, );
    dei_t d;
    memcpy(&d, b->data + off, sizeof d);
    int isz = type == GL_UNSIGNED_BYTE ? 1 : type == GL_UNSIGNED_SHORT ? 2 : 4;
    gl_draw(C, mode, 0, d.count, type, (const void *)(uintptr_t)((u64)d.first * isz), d.inst, d.base, d.bv);
}
/* APPLE_element_array */
static const void *g_apple_elems;
static GLenum g_apple_etype;
void glElementPointerAPPLE(GLenum type, const void *p) { g_apple_etype = type; g_apple_elems = p; }
void glDrawElementArrayAPPLE(GLenum mode, GLint first, GLsizei count) {
    int isz = g_apple_etype == GL_UNSIGNED_BYTE ? 1 : g_apple_etype == GL_UNSIGNED_SHORT ? 2 : 4;
    glDrawElements(mode, count, g_apple_etype, (const u8 *)g_apple_elems + (i64)first * isz);
}
void glDrawRangeElementArrayAPPLE(GLenum mode, GLuint s, GLuint e, GLint first, GLsizei count) { (void)s; (void)e; glDrawElementArrayAPPLE(mode, first, count); }
void glMultiDrawElementArrayAPPLE(GLenum mode, const GLint *first, const GLsizei *count, GLsizei n) { for (int i = 0; i < n; i++) glDrawElementArrayAPPLE(mode, first[i], count[i]); }
void glMultiDrawRangeElementArrayAPPLE(GLenum mode, GLuint s, GLuint e, const GLint *first, const GLsizei *count, GLsizei n) { (void)s; (void)e; glMultiDrawElementArrayAPPLE(mode, first, count, n); }

/*
 * mOS libSystem — OpenGL por software: o fixed function do perfil de compatibilidade (GL 2.1).
 *   matrizes e pilhas, glBegin/glEnd e os valores correntes, arrays de cliente, glInterleavedArrays,
 *   luz (8 luzes, material, color material, dois lados, especular separado), texgen, matrizes de textura,
 *   planos de recorte, tamanho de ponto com atenuação, texenv (REPLACE, MODULATE, DECAL, BLEND, ADD,
 *   COMBINE com os operandos e as escalas), soma de cores, fog, raster pos / window pos, glDrawPixels,
 *   glBitmap, glCopyPixels, glRect, seleção (glSelectBuffer/glRenderMode/nomes), feedback e listas de exibição.
 * Slots de varying do fixed function: 0 cor primária, 1 secundária, 2/3 cores do verso, 4..11 texturas
 * 0..7, 12 coordenada de fog.
 */
#include "gl_int.h"
#include "gl_proto.h"

int gl_attr_fetch(gl_ctx_t *C, const gl_attr_t *a, u32 e, float out[4], i32 iout[4]);
void gl_buf_ref(gl_buf_t *b);
void gl_buf_unref(gl_buf_t *b);
msl_tex_t *gl_tex_mt(gl_ctx_t *C, gl_tex_t *t);
int gl_unit_on(gl_ctx_t *C, int unit, GLenum cap);
float log2f_(float);

enum { S_COL0 = 0, S_COL1 = 1, S_BCOL0 = 2, S_BCOL1 = 3, S_TEX0 = 4, S_FOG = 12, S_NFFP = 13 };

/* ================================================================ matrizes */
void gl_mat_mul(float *r, const float *a, const float *b) {
    float t[16];
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += (double)a[k * 4 + rr] * b[c * 4 + k];
            t[c * 4 + rr] = (float)s;
        }
    memcpy(r, t, sizeof t);
}
void gl_mat_vec(float *r, const float *m, const float *v) {
    float t[4];
    for (int i = 0; i < 4; i++) t[i] = m[i] * v[0] + m[4 + i] * v[1] + m[8 + i] * v[2] + m[12 + i] * v[3];
    memcpy(r, t, sizeof t);
}
int gl_mat_inv(float *out, const float *m) {
    double inv[16], det;
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0) { for (int i = 0; i < 16; i++) out[i] = i % 5 == 0; return 0; }
    for (int i = 0; i < 16; i++) out[i] = (float)(inv[i] / det);
    return 1;
}
static void ident(float *m) { for (int i = 0; i < 16; i++) m[i] = i % 5 == 0; }
mstack_t *gl_cur_stack(gl_ctx_t *C) {
    switch (C->mmode) {
    case GL_PROJECTION: return &C->proj;
    case GL_TEXTURE: return &C->texm[C->unit < 8 ? C->unit : 7];
    case GL_COLOR: return &C->colm;
    }
    return &C->mv;
}
static float *top(gl_ctx_t *C) { mstack_t *s = gl_cur_stack(C); return s->s[s->top].m; }
void gl_ffp_init(gl_ctx_t *C) {
    C->mmode = GL_MODELVIEW;
    C->mv.max = 32; C->proj.max = 4; C->colm.max = 4;
    ident(C->mv.s[0].m); ident(C->proj.s[0].m); ident(C->colm.s[0].m);
    for (int i = 0; i < 8; i++) { C->texm[i].max = 4; ident(C->texm[i].s[0].m); }
    C->cur_col[0] = C->cur_col[1] = C->cur_col[2] = C->cur_col[3] = 1;
    C->cur_col2[3] = 1;
    C->cur_nrm[2] = 1;
    for (int i = 0; i < 8; i++) C->cur_tc[i][3] = 1;
    for (int i = 0; i < GLX_ATTR; i++) { C->cur_attr[i][3] = 1; C->cur_attri[i][3] = 1; }
    C->cur_edge = 1;
    C->lm_amb[0] = C->lm_amb[1] = C->lm_amb[2] = 0.2f; C->lm_amb[3] = 1;
    for (int i = 0; i < GLX_LIGHTS; i++) {
        gl_light_t *l = &C->light[i];
        l->amb[3] = 1;
        if (i == 0) { l->dif[0] = l->dif[1] = l->dif[2] = l->dif[3] = 1; l->spec[0] = l->spec[1] = l->spec[2] = l->spec[3] = 1; }
        else { l->dif[3] = 1; l->spec[3] = 1; }
        l->pos[2] = 1;
        l->spot_dir[2] = -1;
        l->spot_cut = 180;
        l->att[0] = 1;
    }
    for (int f = 0; f < 2; f++) {
        gl_mat_t *m = &C->mat[f];
        m->amb[0] = m->amb[1] = m->amb[2] = 0.2f; m->amb[3] = 1;
        m->dif[0] = m->dif[1] = m->dif[2] = 0.8f; m->dif[3] = 1;
        m->spec[3] = 1; m->emi[3] = 1;
        m->ci[1] = m->ci[2] = 1;
    }
    C->cm_face = GL_FRONT_AND_BACK;
    C->cm_mode = GL_AMBIENT_AND_DIFFUSE;
    C->shade = GL_SMOOTH;
    C->alpha_func = GL_ALWAYS;
    C->fog_mode = GL_EXP; C->fog_d = 1; C->fog_e = 1; C->fog_src = GL_FRAGMENT_DEPTH;
    for (int u = 0; u < 8; u++) {
        gl_texenv_t *t = &C->te[u];
        t->mode = GL_MODULATE;
        t->comb_rgb = t->comb_a = GL_MODULATE;
        t->src[0][0] = t->src[1][0] = GL_TEXTURE;
        t->src[0][1] = t->src[1][1] = GL_PREVIOUS;
        t->src[0][2] = t->src[1][2] = GL_CONSTANT;
        t->op[0][0] = t->op[0][1] = GL_SRC_COLOR; t->op[0][2] = GL_SRC_ALPHA;
        t->op[1][0] = t->op[1][1] = t->op[1][2] = GL_SRC_ALPHA;
        t->scale_rgb = t->scale_a = 1;
        for (int k = 0; k < 4; k++) t->gen_mode[k] = GL_EYE_LINEAR;
        t->gen_obj[0][0] = t->gen_eye[0][0] = 1;
        t->gen_obj[1][1] = t->gen_eye[1][1] = 1;
    }
    C->raster_valid = 1;
    C->raster_pos[3] = 1;
    C->raster_col[0] = C->raster_col[1] = C->raster_col[2] = C->raster_col[3] = 1;
    C->raster_tc[3] = 1;
    for (int i = 0; i < 4; i++) C->pmap_scale[i] = 1;
    gl_vao_defaults(C->vao);
}
void gl_vao_defaults(gl_vao_t *v) {
    for (int i = 0; i < GLX_ATTR; i++) { v->a[i].size = 4; v->a[i].type = GL_FLOAT; }
    v->ca_vertex.size = 4; v->ca_vertex.type = GL_FLOAT;
    v->ca_normal.size = 3; v->ca_normal.type = GL_FLOAT;
    v->ca_color.size = 4; v->ca_color.type = GL_FLOAT;
    v->ca_color2.size = 3; v->ca_color2.type = GL_FLOAT;
    v->ca_fog.size = 1; v->ca_fog.type = GL_FLOAT;
    v->ca_index.size = 1; v->ca_index.type = GL_FLOAT;
    v->ca_edge.size = 1; v->ca_edge.type = GL_UNSIGNED_BYTE;
    for (int i = 0; i < 8; i++) { v->ca_tc[i].size = 4; v->ca_tc[i].type = GL_FLOAT; }
}

/* ================================================================ listas de exibição */
enum {
    DL_ENABLE = 1, DL_DISABLE, DL_VIEWPORT, DL_BINDTEX, DL_CLEAR, DL_BEGIN, DL_END, DL_VERTEX, DL_COLOR, DL_NORMAL, DL_TEXCOORD,
    DL_COLOR2, DL_FOG, DL_EDGE, DL_MMODE, DL_LOADM, DL_MULTM, DL_PUSHM, DL_POPM, DL_MATERIAL, DL_LIGHT, DL_LMODEL, DL_SHADE,
    DL_ALPHAF, DL_BLENDF, DL_DEPTHF, DL_DEPTHM, DL_COLORM, DL_LINEW, DL_POINTS, DL_PMODE, DL_CULL, DL_FRONT, DL_TEXENV,
    DL_TEXPARAM, DL_FOGP, DL_CALL, DL_RASTER, DL_BITMAP, DL_DRAWPIX, DL_POFF, DL_SCISSOR, DL_COLORMAT, DL_TEXGEN, DL_CLIPP,
    DL_PUSHA, DL_POPA, DL_ATTR, DL_STENCILF, DL_STENCILO, DL_ACTIVE, DL_CLEARCOL, DL_CLEARDEPTH, DL_LSTIPPLE, DL_PSTIPPLE, DL_IDENT,
    DL_LOADNAME, DL_PUSHNAME, DL_POPNAME, DL_BLENDCOL, DL_BLENDEQ, DL_USEPROG, DL_LISTBASE, DL_ZOOM, DL_HINT,
};
struct gl_dlcmd {
    gl_dlcmd_t *next;
    int op, n;
    union { float f[20]; u32 u[20]; i32 i[20]; } a;
    void *blob;
    i64 blen;
};
int gl_dl_record(gl_ctx_t *C, int op, const void *args, int nbytes) {
    gl_dlist_t *L = C->dl_cur;
    if (!L || C->dl_exec) return 0;
    gl_dlcmd_t *c = calloc(1, sizeof *c);
    if (!c) return 1;
    c->op = op;
    if (nbytes > (int)sizeof c->a) nbytes = sizeof c->a;
    if (args && nbytes) memcpy(&c->a, args, (size_t)nbytes);
    c->n = nbytes / 4;
    if (L->tail) L->tail->next = c; else L->head = c;
    L->tail = c;
    return C->dl_mode == GL_COMPILE;
}
static int dl_blob(gl_ctx_t *C, int op, const void *args, int nbytes, const void *blob, i64 blen) {
    if (!C->dl_cur || C->dl_exec) return 0;
    int r = gl_dl_record(C, op, args, nbytes);
    gl_dlcmd_t *c = C->dl_cur->tail;
    if (c && blob && blen > 0) { c->blob = malloc((size_t)blen); if (c->blob) { memcpy(c->blob, blob, (size_t)blen); c->blen = blen; } }
    return r;
}
#define REC(op, ...) do { if (C->dl_cur && !C->dl_exec) { float _a[] = {__VA_ARGS__}; if (gl_dl_record(C, op, _a, sizeof _a)) return; } } while (0)
#define RECU(op, ...) do { if (C->dl_cur && !C->dl_exec) { u32 _a[] = {__VA_ARGS__}; if (gl_dl_record(C, op, _a, sizeof _a)) return; } } while (0)
static void dl_free(gl_dlist_t *L) {
    for (gl_dlcmd_t *c = L->head, *n; c; c = n) { n = c->next; free(c->blob); free(c); }
    L->head = L->tail = 0;
}
GLuint glGenLists(GLsizei range) {
    GLC(0);
    if (range < 0) GLERR(GL_INVALID_VALUE, 0);
    if (!range) return 0;
    /* faixa contígua livre */
    gl_names_t *t = &C->sh->lists;
    for (u32 base = 1;; base++) {
        int ok = 1;
        for (u32 i = 0; i < (u32)range; i++) if (gl_names_is(t, base + i)) { ok = 0; base += i; break; }
        if (ok) { for (u32 i = 0; i < (u32)range; i++) gl_names_reserve(t, base + i); return base; }
    }
}
void glDeleteLists(GLuint list, GLsizei range) {
    GLC();
    if (range < 0) GLERR(GL_INVALID_VALUE, );
    for (u32 i = 0; i < (u32)range; i++) {
        gl_dlist_t *L = gl_names_get(&C->sh->lists, list + i);
        if (L) { dl_free(L); free(L); }
        gl_names_del(&C->sh->lists, list + i);
    }
}
GLboolean glIsList(GLuint list) { GLC(0); return gl_names_get(&C->sh->lists, list) != 0; }
void glNewList(GLuint list, GLenum mode) {
    GLC();
    if (!list) GLERR(GL_INVALID_VALUE, );
    if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) GLERR(GL_INVALID_ENUM, );
    if (C->dl_cur || C->in_begin) GLERR(GL_INVALID_OPERATION, );
    gl_dlist_t *L = calloc(1, sizeof *L);
    if (!L) GLERR(GL_OUT_OF_MEMORY, );
    L->name = list;
    C->dl_cur = L;
    C->dl_mode = mode;
}
void glEndList(void) {
    GLC();
    if (!C->dl_cur) GLERR(GL_INVALID_OPERATION, );
    gl_dlist_t *L = C->dl_cur;
    C->dl_cur = 0;
    gl_dlist_t *old = gl_names_get(&C->sh->lists, L->name);
    if (old) { dl_free(old); free(old); }
    gl_names_set(&C->sh->lists, L->name, L);
}
static void dl_exec(gl_ctx_t *C, gl_dlist_t *L, int depth);
void glCallList(GLuint list) {
    GLC();
    RECU(DL_CALL, list);
    gl_dlist_t *L = gl_names_get(&C->sh->lists, list);
    if (L) dl_exec(C, L, 0);
}
void glCallLists(GLsizei n, GLenum type, const void *lists) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    const u8 *p = lists;
    for (int i = 0; i < n; i++) {
        u32 v;
        switch (type) {
        case GL_BYTE: v = (u32)(i32)((const i8 *)p)[i]; break;
        case GL_UNSIGNED_BYTE: v = p[i]; break;
        case GL_SHORT: v = (u32)(i32)((const i16 *)p)[i]; break;
        case GL_UNSIGNED_SHORT: v = ((const u16 *)p)[i]; break;
        case GL_INT: case GL_UNSIGNED_INT: v = ((const u32 *)p)[i]; break;
        case GL_FLOAT: v = (u32)((const float *)p)[i]; break;
        case GL_2_BYTES: v = (u32)p[2 * i] << 8 | p[2 * i + 1]; break;
        case GL_3_BYTES: v = (u32)p[3 * i] << 16 | (u32)p[3 * i + 1] << 8 | p[3 * i + 2]; break;
        case GL_4_BYTES: v = (u32)p[4 * i] << 24 | (u32)p[4 * i + 1] << 16 | (u32)p[4 * i + 2] << 8 | p[4 * i + 3]; break;
        default: GLERR(GL_INVALID_ENUM, );
        }
        glCallList(v + C->list_base);
    }
}
void glListBase(GLuint base) { GLC(); RECU(DL_LISTBASE, base); C->list_base = base; }

/* ================================================================ matrizes (API) */
void glMatrixMode(GLenum m) {
    GLC();
    RECU(DL_MMODE, m);
    if (m != GL_MODELVIEW && m != GL_PROJECTION && m != GL_TEXTURE && m != GL_COLOR) GLERR(GL_INVALID_ENUM, );
    C->mmode = m;
}
void glLoadIdentity(void) { GLC(); RECU(DL_IDENT, 0); ident(top(C)); }
static void loadm(gl_ctx_t *C, const float *m) {
    if (C->dl_cur && !C->dl_exec && gl_dl_record(C, DL_LOADM, m, 64)) return;
    memcpy(top(C), m, 64);
}
static void multm(gl_ctx_t *C, const float *m) {
    if (C->dl_cur && !C->dl_exec && gl_dl_record(C, DL_MULTM, m, 64)) return;
    gl_mat_mul(top(C), top(C), m);
}
void glLoadMatrixf(const GLfloat *m) { GLC(); loadm(C, m); }
void glLoadMatrixd(const GLdouble *m) { GLC(); float f[16]; for (int i = 0; i < 16; i++) f[i] = (float)m[i]; loadm(C, f); }
void glMultMatrixf(const GLfloat *m) { GLC(); multm(C, m); }
void glMultMatrixd(const GLdouble *m) { GLC(); float f[16]; for (int i = 0; i < 16; i++) f[i] = (float)m[i]; multm(C, f); }
static void transp(float *o, const float *m) { for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) o[i * 4 + j] = m[j * 4 + i]; }
void glLoadTransposeMatrixf(const GLfloat *m) { GLC(); float t[16]; transp(t, m); loadm(C, t); }
void glLoadTransposeMatrixd(const GLdouble *m) { GLC(); float f[16], t[16]; for (int i = 0; i < 16; i++) f[i] = (float)m[i]; transp(t, f); loadm(C, t); }
void glMultTransposeMatrixf(const GLfloat *m) { GLC(); float t[16]; transp(t, m); multm(C, t); }
void glMultTransposeMatrixd(const GLdouble *m) { GLC(); float f[16], t[16]; for (int i = 0; i < 16; i++) f[i] = (float)m[i]; transp(t, f); multm(C, t); }
void glLoadTransposeMatrixfARB(const GLfloat *m) { glLoadTransposeMatrixf(m); }
void glLoadTransposeMatrixdARB(const GLdouble *m) { glLoadTransposeMatrixd(m); }
void glMultTransposeMatrixfARB(const GLfloat *m) { glMultTransposeMatrixf(m); }
void glMultTransposeMatrixdARB(const GLdouble *m) { glMultTransposeMatrixd(m); }
void glPushMatrix(void) {
    GLC();
    RECU(DL_PUSHM, 0);
    mstack_t *s = gl_cur_stack(C);
    if (s->top + 1 >= s->max) GLERR(GL_STACK_OVERFLOW, );
    s->s[s->top + 1] = s->s[s->top];
    s->top++;
}
void glPopMatrix(void) {
    GLC();
    RECU(DL_POPM, 0);
    mstack_t *s = gl_cur_stack(C);
    if (s->top == 0) GLERR(GL_STACK_UNDERFLOW, );
    s->top--;
}
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { GLC(); float m[16]; ident(m); m[12] = x; m[13] = y; m[14] = z; multm(C, m); }
void glTranslated(GLdouble x, GLdouble y, GLdouble z) { glTranslatef((float)x, (float)y, (float)z); }
void glScalef(GLfloat x, GLfloat y, GLfloat z) { GLC(); float m[16]; ident(m); m[0] = x; m[5] = y; m[10] = z; multm(C, m); }
void glScaled(GLdouble x, GLdouble y, GLdouble z) { glScalef((float)x, (float)y, (float)z); }
void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) {
    GLC();
    double l = sqrt((double)x * x + (double)y * y + (double)z * z);
    if (l == 0) return;
    double c = cos(a * 3.14159265358979323846 / 180), s = sin(a * 3.14159265358979323846 / 180), X = x / l, Y = y / l, Z = z / l, t = 1 - c;
    float m[16] = {(float)(X * X * t + c), (float)(Y * X * t + Z * s), (float)(X * Z * t - Y * s), 0,
                   (float)(X * Y * t - Z * s), (float)(Y * Y * t + c), (float)(Y * Z * t + X * s), 0,
                   (float)(X * Z * t + Y * s), (float)(Y * Z * t - X * s), (float)(Z * Z * t + c), 0, 0, 0, 0, 1};
    multm(C, m);
}
void glRotated(GLdouble a, GLdouble x, GLdouble y, GLdouble z) { glRotatef((float)a, (float)x, (float)y, (float)z); }
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    GLC();
    if (l == r || b == t || n == f) GLERR(GL_INVALID_VALUE, );
    float m[16] = {(float)(2 / (r - l)), 0, 0, 0, 0, (float)(2 / (t - b)), 0, 0, 0, 0, (float)(-2 / (f - n)), 0,
                   (float)(-(r + l) / (r - l)), (float)(-(t + b) / (t - b)), (float)(-(f + n) / (f - n)), 1};
    multm(C, m);
}
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    GLC();
    if (n <= 0 || f <= 0 || l == r || b == t || n == f) GLERR(GL_INVALID_VALUE, );
    float m[16] = {(float)(2 * n / (r - l)), 0, 0, 0, 0, (float)(2 * n / (t - b)), 0, 0,
                   (float)((r + l) / (r - l)), (float)((t + b) / (t - b)), (float)(-(f + n) / (f - n)), -1,
                   0, 0, (float)(-2 * f * n / (f - n)), 0};
    multm(C, m);
}

/* ================================================================ valores correntes e glBegin/glEnd */
static void imv_push(gl_ctx_t *C, const float *pos) {
    if (C->nimv == C->capimv) {
        u32 nc = C->capimv ? C->capimv * 2 : 256;
        gl_imv_t *n = realloc(C->imv, nc * sizeof *n);
        if (!n) { gl_err(C, GL_OUT_OF_MEMORY); return; }
        C->imv = n;
        C->capimv = nc;
    }
    gl_imv_t *v = &C->imv[C->nimv++];
    memcpy(v->pos, pos, 16);
    memcpy(v->col, C->cur_col, 16);
    memcpy(v->col2, C->cur_col2, 16);
    memcpy(v->nrm, C->cur_nrm, 12);
    memcpy(v->tc, C->cur_tc, sizeof v->tc);
    v->fog = C->cur_fog;
    memcpy(v->attr, C->cur_attr, sizeof v->attr);
    v->edge = C->cur_edge;
}
void gl_ffp_attr0(gl_ctx_t *C, const float *v) { imv_push(C, v); }
static int g_imm_draw;    /* 1 enquanto o glEnd desenha a lista do glBegin */
int gl_ffp_imm(void) { return g_imm_draw; }
void glBegin(GLenum mode) {
    GLC();
    RECU(DL_BEGIN, mode);
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    if (mode > GL_POLYGON) GLERR(GL_INVALID_ENUM, );
    C->in_begin = 1;
    C->begin_mode = mode;
    C->nimv = 0;
}
void glEnd(void) {
    GLC();
    RECU(DL_END, 0);
    if (!C->in_begin) GLERR(GL_INVALID_OPERATION, );
    C->in_begin = 0;
    if (!C->nimv) return;
    g_imm_draw = 1;
    gl_draw(C, C->begin_mode, 0, C->nimv, 0, 0, 1, 0, 0);
    g_imm_draw = 0;
}
static void vtx4(gl_ctx_t *C, float x, float y, float z, float w) {
    if (C->dl_cur && !C->dl_exec) { float a[4] = {x, y, z, w}; if (gl_dl_record(C, DL_VERTEX, a, 16)) return; }
    if (!C->in_begin) return;   /* fora do glBegin o glVertex não tem efeito */
    float p[4] = {x, y, z, w};
    imv_push(C, p);
}
#define V4(x, y, z, w) do { GLC(); vtx4(C, (float)(x), (float)(y), (float)(z), (float)(w)); } while (0)
void glVertex2f(GLfloat x, GLfloat y) { V4(x, y, 0, 1); }
void glVertex2d(GLdouble x, GLdouble y) { V4(x, y, 0, 1); }
void glVertex2i(GLint x, GLint y) { V4(x, y, 0, 1); }
void glVertex2s(GLshort x, GLshort y) { V4(x, y, 0, 1); }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { V4(x, y, z, 1); }
void glVertex3d(GLdouble x, GLdouble y, GLdouble z) { V4(x, y, z, 1); }
void glVertex3i(GLint x, GLint y, GLint z) { V4(x, y, z, 1); }
void glVertex3s(GLshort x, GLshort y, GLshort z) { V4(x, y, z, 1); }
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { V4(x, y, z, w); }
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { V4(x, y, z, w); }
void glVertex4i(GLint x, GLint y, GLint z, GLint w) { V4(x, y, z, w); }
void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) { V4(x, y, z, w); }
void glVertex2fv(const GLfloat *v) { V4(v[0], v[1], 0, 1); }
void glVertex2dv(const GLdouble *v) { V4(v[0], v[1], 0, 1); }
void glVertex2iv(const GLint *v) { V4(v[0], v[1], 0, 1); }
void glVertex2sv(const GLshort *v) { V4(v[0], v[1], 0, 1); }
void glVertex3fv(const GLfloat *v) { V4(v[0], v[1], v[2], 1); }
void glVertex3dv(const GLdouble *v) { V4(v[0], v[1], v[2], 1); }
void glVertex3iv(const GLint *v) { V4(v[0], v[1], v[2], 1); }
void glVertex3sv(const GLshort *v) { V4(v[0], v[1], v[2], 1); }
void glVertex4fv(const GLfloat *v) { V4(v[0], v[1], v[2], v[3]); }
void glVertex4dv(const GLdouble *v) { V4(v[0], v[1], v[2], v[3]); }
void glVertex4iv(const GLint *v) { V4(v[0], v[1], v[2], v[3]); }
void glVertex4sv(const GLshort *v) { V4(v[0], v[1], v[2], v[3]); }
static void color4(gl_ctx_t *C, float r, float g, float b, float a) {
    REC(DL_COLOR, r, g, b, a);
    C->cur_col[0] = r; C->cur_col[1] = g; C->cur_col[2] = b; C->cur_col[3] = a;
    /* color material: o material acompanha a cor corrente */
    if (gl_is_on(C, GL_COLOR_MATERIAL)) {
        for (int f = 0; f < 2; f++) {
            if (C->cm_face != GL_FRONT_AND_BACK && (f == 0) != (C->cm_face == GL_FRONT)) continue;
            gl_mat_t *m = &C->mat[f];
            switch (C->cm_mode) {
            case GL_AMBIENT: memcpy(m->amb, C->cur_col, 16); break;
            case GL_DIFFUSE: memcpy(m->dif, C->cur_col, 16); break;
            case GL_SPECULAR: memcpy(m->spec, C->cur_col, 16); break;
            case GL_EMISSION: memcpy(m->emi, C->cur_col, 16); break;
            default: memcpy(m->amb, C->cur_col, 16); memcpy(m->dif, C->cur_col, 16);
            }
        }
    }
}
#define C4(r, g, b, a) do { GLC(); color4(C, (float)(r), (float)(g), (float)(b), (float)(a)); } while (0)
#define NB_(x) ((float)(x) / 127.0f)
#define NUB(x) ((float)(x) / 255.0f)
#define NS_(x) ((float)(x) / 32767.0f)
#define NUS(x) ((float)(x) / 65535.0f)
#define NI_(x) ((float)((double)(x) / 2147483647.0))
#define NUI(x) ((float)((double)(x) / 4294967295.0))
void glColor3f(GLfloat r, GLfloat g, GLfloat b) { C4(r, g, b, 1); }
void glColor3d(GLdouble r, GLdouble g, GLdouble b) { C4(r, g, b, 1); }
void glColor3b(GLbyte r, GLbyte g, GLbyte b) { C4(NB_(r), NB_(g), NB_(b), 1); }
void glColor3ub(GLubyte r, GLubyte g, GLubyte b) { C4(NUB(r), NUB(g), NUB(b), 1); }
void glColor3s(GLshort r, GLshort g, GLshort b) { C4(NS_(r), NS_(g), NS_(b), 1); }
void glColor3us(GLushort r, GLushort g, GLushort b) { C4(NUS(r), NUS(g), NUS(b), 1); }
void glColor3i(GLint r, GLint g, GLint b) { C4(NI_(r), NI_(g), NI_(b), 1); }
void glColor3ui(GLuint r, GLuint g, GLuint b) { C4(NUI(r), NUI(g), NUI(b), 1); }
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { C4(r, g, b, a); }
void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) { C4(r, g, b, a); }
void glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a) { C4(NB_(r), NB_(g), NB_(b), NB_(a)); }
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { C4(NUB(r), NUB(g), NUB(b), NUB(a)); }
void glColor4s(GLshort r, GLshort g, GLshort b, GLshort a) { C4(NS_(r), NS_(g), NS_(b), NS_(a)); }
void glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { C4(NUS(r), NUS(g), NUS(b), NUS(a)); }
void glColor4i(GLint r, GLint g, GLint b, GLint a) { C4(NI_(r), NI_(g), NI_(b), NI_(a)); }
void glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a) { C4(NUI(r), NUI(g), NUI(b), NUI(a)); }
void glColor3fv(const GLfloat *v) { C4(v[0], v[1], v[2], 1); }
void glColor3dv(const GLdouble *v) { C4(v[0], v[1], v[2], 1); }
void glColor3bv(const GLbyte *v) { C4(NB_(v[0]), NB_(v[1]), NB_(v[2]), 1); }
void glColor3ubv(const GLubyte *v) { C4(NUB(v[0]), NUB(v[1]), NUB(v[2]), 1); }
void glColor3sv(const GLshort *v) { C4(NS_(v[0]), NS_(v[1]), NS_(v[2]), 1); }
void glColor3usv(const GLushort *v) { C4(NUS(v[0]), NUS(v[1]), NUS(v[2]), 1); }
void glColor3iv(const GLint *v) { C4(NI_(v[0]), NI_(v[1]), NI_(v[2]), 1); }
void glColor3uiv(const GLuint *v) { C4(NUI(v[0]), NUI(v[1]), NUI(v[2]), 1); }
void glColor4fv(const GLfloat *v) { C4(v[0], v[1], v[2], v[3]); }
void glColor4dv(const GLdouble *v) { C4(v[0], v[1], v[2], v[3]); }
void glColor4bv(const GLbyte *v) { C4(NB_(v[0]), NB_(v[1]), NB_(v[2]), NB_(v[3])); }
void glColor4ubv(const GLubyte *v) { C4(NUB(v[0]), NUB(v[1]), NUB(v[2]), NUB(v[3])); }
void glColor4sv(const GLshort *v) { C4(NS_(v[0]), NS_(v[1]), NS_(v[2]), NS_(v[3])); }
void glColor4usv(const GLushort *v) { C4(NUS(v[0]), NUS(v[1]), NUS(v[2]), NUS(v[3])); }
void glColor4iv(const GLint *v) { C4(NI_(v[0]), NI_(v[1]), NI_(v[2]), NI_(v[3])); }
void glColor4uiv(const GLuint *v) { C4(NUI(v[0]), NUI(v[1]), NUI(v[2]), NUI(v[3])); }
static void color2(gl_ctx_t *C, float r, float g, float b) { REC(DL_COLOR2, r, g, b); C->cur_col2[0] = r; C->cur_col2[1] = g; C->cur_col2[2] = b; C->cur_col2[3] = 1; }
#define C2(r, g, b) do { GLC(); color2(C, (float)(r), (float)(g), (float)(b)); } while (0)
void glSecondaryColor3f(GLfloat r, GLfloat g, GLfloat b) { C2(r, g, b); }
void glSecondaryColor3d(GLdouble r, GLdouble g, GLdouble b) { C2(r, g, b); }
void glSecondaryColor3b(GLbyte r, GLbyte g, GLbyte b) { C2(NB_(r), NB_(g), NB_(b)); }
void glSecondaryColor3ub(GLubyte r, GLubyte g, GLubyte b) { C2(NUB(r), NUB(g), NUB(b)); }
void glSecondaryColor3s(GLshort r, GLshort g, GLshort b) { C2(NS_(r), NS_(g), NS_(b)); }
void glSecondaryColor3us(GLushort r, GLushort g, GLushort b) { C2(NUS(r), NUS(g), NUS(b)); }
void glSecondaryColor3i(GLint r, GLint g, GLint b) { C2(NI_(r), NI_(g), NI_(b)); }
void glSecondaryColor3ui(GLuint r, GLuint g, GLuint b) { C2(NUI(r), NUI(g), NUI(b)); }
void glSecondaryColor3fv(const GLfloat *v) { C2(v[0], v[1], v[2]); }
void glSecondaryColor3dv(const GLdouble *v) { C2(v[0], v[1], v[2]); }
void glSecondaryColor3bv(const GLbyte *v) { C2(NB_(v[0]), NB_(v[1]), NB_(v[2])); }
void glSecondaryColor3ubv(const GLubyte *v) { C2(NUB(v[0]), NUB(v[1]), NUB(v[2])); }
void glSecondaryColor3sv(const GLshort *v) { C2(NS_(v[0]), NS_(v[1]), NS_(v[2])); }
void glSecondaryColor3usv(const GLushort *v) { C2(NUS(v[0]), NUS(v[1]), NUS(v[2])); }
void glSecondaryColor3iv(const GLint *v) { C2(NI_(v[0]), NI_(v[1]), NI_(v[2])); }
void glSecondaryColor3uiv(const GLuint *v) { C2(NUI(v[0]), NUI(v[1]), NUI(v[2])); }
void glSecondaryColor3fEXT(GLfloat r, GLfloat g, GLfloat b) { C2(r, g, b); }
void glSecondaryColor3dEXT(GLdouble r, GLdouble g, GLdouble b) { C2(r, g, b); }
void glSecondaryColor3bEXT(GLbyte r, GLbyte g, GLbyte b) { C2(NB_(r), NB_(g), NB_(b)); }
void glSecondaryColor3ubEXT(GLubyte r, GLubyte g, GLubyte b) { C2(NUB(r), NUB(g), NUB(b)); }
void glSecondaryColor3sEXT(GLshort r, GLshort g, GLshort b) { C2(NS_(r), NS_(g), NS_(b)); }
void glSecondaryColor3usEXT(GLushort r, GLushort g, GLushort b) { C2(NUS(r), NUS(g), NUS(b)); }
void glSecondaryColor3iEXT(GLint r, GLint g, GLint b) { C2(NI_(r), NI_(g), NI_(b)); }
void glSecondaryColor3uiEXT(GLuint r, GLuint g, GLuint b) { C2(NUI(r), NUI(g), NUI(b)); }
void glSecondaryColor3fvEXT(const GLfloat *v) { C2(v[0], v[1], v[2]); }
void glSecondaryColor3dvEXT(const GLdouble *v) { C2(v[0], v[1], v[2]); }
void glSecondaryColor3bvEXT(const GLbyte *v) { C2(NB_(v[0]), NB_(v[1]), NB_(v[2])); }
void glSecondaryColor3ubvEXT(const GLubyte *v) { C2(NUB(v[0]), NUB(v[1]), NUB(v[2])); }
void glSecondaryColor3svEXT(const GLshort *v) { C2(NS_(v[0]), NS_(v[1]), NS_(v[2])); }
void glSecondaryColor3usvEXT(const GLushort *v) { C2(NUS(v[0]), NUS(v[1]), NUS(v[2])); }
void glSecondaryColor3ivEXT(const GLint *v) { C2(NI_(v[0]), NI_(v[1]), NI_(v[2])); }
void glSecondaryColor3uivEXT(const GLuint *v) { C2(NUI(v[0]), NUI(v[1]), NUI(v[2])); }
static void normal3(gl_ctx_t *C, float x, float y, float z) { REC(DL_NORMAL, x, y, z); C->cur_nrm[0] = x; C->cur_nrm[1] = y; C->cur_nrm[2] = z; }
#define N3(x, y, z) do { GLC(); normal3(C, (float)(x), (float)(y), (float)(z)); } while (0)
void glNormal3f(GLfloat x, GLfloat y, GLfloat z) { N3(x, y, z); }
void glNormal3d(GLdouble x, GLdouble y, GLdouble z) { N3(x, y, z); }
void glNormal3b(GLbyte x, GLbyte y, GLbyte z) { N3(NB_(x), NB_(y), NB_(z)); }
void glNormal3s(GLshort x, GLshort y, GLshort z) { N3(NS_(x), NS_(y), NS_(z)); }
void glNormal3i(GLint x, GLint y, GLint z) { N3(NI_(x), NI_(y), NI_(z)); }
void glNormal3fv(const GLfloat *v) { N3(v[0], v[1], v[2]); }
void glNormal3dv(const GLdouble *v) { N3(v[0], v[1], v[2]); }
void glNormal3bv(const GLbyte *v) { N3(NB_(v[0]), NB_(v[1]), NB_(v[2])); }
void glNormal3sv(const GLshort *v) { N3(NS_(v[0]), NS_(v[1]), NS_(v[2])); }
void glNormal3iv(const GLint *v) { N3(NI_(v[0]), NI_(v[1]), NI_(v[2])); }
static void texc(gl_ctx_t *C, int u, float s, float t, float r, float q) {
    if (C->dl_cur && !C->dl_exec) { float a[5] = {(float)u, s, t, r, q}; if (gl_dl_record(C, DL_TEXCOORD, a, 20)) return; }
    if (u < 0 || u >= 8) { gl_err(C, GL_INVALID_ENUM); return; }
    C->cur_tc[u][0] = s; C->cur_tc[u][1] = t; C->cur_tc[u][2] = r; C->cur_tc[u][3] = q;
}
#define T4(u, s, t, r, q) do { GLC(); texc(C, u, (float)(s), (float)(t), (float)(r), (float)(q)); } while (0)
void glTexCoord1f(GLfloat s) { T4(0, s, 0, 0, 1); }
void glTexCoord1d(GLdouble s) { T4(0, s, 0, 0, 1); }
void glTexCoord1i(GLint s) { T4(0, s, 0, 0, 1); }
void glTexCoord1s(GLshort s) { T4(0, s, 0, 0, 1); }
void glTexCoord2f(GLfloat s, GLfloat t) { T4(0, s, t, 0, 1); }
void glTexCoord2d(GLdouble s, GLdouble t) { T4(0, s, t, 0, 1); }
void glTexCoord2i(GLint s, GLint t) { T4(0, s, t, 0, 1); }
void glTexCoord2s(GLshort s, GLshort t) { T4(0, s, t, 0, 1); }
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) { T4(0, s, t, r, 1); }
void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) { T4(0, s, t, r, 1); }
void glTexCoord3i(GLint s, GLint t, GLint r) { T4(0, s, t, r, 1); }
void glTexCoord3s(GLshort s, GLshort t, GLshort r) { T4(0, s, t, r, 1); }
void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) { T4(0, s, t, r, q); }
void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) { T4(0, s, t, r, q); }
void glTexCoord4i(GLint s, GLint t, GLint r, GLint q) { T4(0, s, t, r, q); }
void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) { T4(0, s, t, r, q); }
void glTexCoord1fv(const GLfloat *v) { T4(0, v[0], 0, 0, 1); }
void glTexCoord1dv(const GLdouble *v) { T4(0, v[0], 0, 0, 1); }
void glTexCoord1iv(const GLint *v) { T4(0, v[0], 0, 0, 1); }
void glTexCoord1sv(const GLshort *v) { T4(0, v[0], 0, 0, 1); }
void glTexCoord2fv(const GLfloat *v) { T4(0, v[0], v[1], 0, 1); }
void glTexCoord2dv(const GLdouble *v) { T4(0, v[0], v[1], 0, 1); }
void glTexCoord2iv(const GLint *v) { T4(0, v[0], v[1], 0, 1); }
void glTexCoord2sv(const GLshort *v) { T4(0, v[0], v[1], 0, 1); }
void glTexCoord3fv(const GLfloat *v) { T4(0, v[0], v[1], v[2], 1); }
void glTexCoord3dv(const GLdouble *v) { T4(0, v[0], v[1], v[2], 1); }
void glTexCoord3iv(const GLint *v) { T4(0, v[0], v[1], v[2], 1); }
void glTexCoord3sv(const GLshort *v) { T4(0, v[0], v[1], v[2], 1); }
void glTexCoord4fv(const GLfloat *v) { T4(0, v[0], v[1], v[2], v[3]); }
void glTexCoord4dv(const GLdouble *v) { T4(0, v[0], v[1], v[2], v[3]); }
void glTexCoord4iv(const GLint *v) { T4(0, v[0], v[1], v[2], v[3]); }
void glTexCoord4sv(const GLshort *v) { T4(0, v[0], v[1], v[2], v[3]); }
#define MT(u) ((int)((u) - GL_TEXTURE0))
void glMultiTexCoord1f(GLenum u, GLfloat s) { T4(MT(u), s, 0, 0, 1); }
void glMultiTexCoord1d(GLenum u, GLdouble s) { T4(MT(u), s, 0, 0, 1); }
void glMultiTexCoord1i(GLenum u, GLint s) { T4(MT(u), s, 0, 0, 1); }
void glMultiTexCoord1s(GLenum u, GLshort s) { T4(MT(u), s, 0, 0, 1); }
void glMultiTexCoord2f(GLenum u, GLfloat s, GLfloat t) { T4(MT(u), s, t, 0, 1); }
void glMultiTexCoord2d(GLenum u, GLdouble s, GLdouble t) { T4(MT(u), s, t, 0, 1); }
void glMultiTexCoord2i(GLenum u, GLint s, GLint t) { T4(MT(u), s, t, 0, 1); }
void glMultiTexCoord2s(GLenum u, GLshort s, GLshort t) { T4(MT(u), s, t, 0, 1); }
void glMultiTexCoord3f(GLenum u, GLfloat s, GLfloat t, GLfloat r) { T4(MT(u), s, t, r, 1); }
void glMultiTexCoord3d(GLenum u, GLdouble s, GLdouble t, GLdouble r) { T4(MT(u), s, t, r, 1); }
void glMultiTexCoord3i(GLenum u, GLint s, GLint t, GLint r) { T4(MT(u), s, t, r, 1); }
void glMultiTexCoord3s(GLenum u, GLshort s, GLshort t, GLshort r) { T4(MT(u), s, t, r, 1); }
void glMultiTexCoord4f(GLenum u, GLfloat s, GLfloat t, GLfloat r, GLfloat q) { T4(MT(u), s, t, r, q); }
void glMultiTexCoord4d(GLenum u, GLdouble s, GLdouble t, GLdouble r, GLdouble q) { T4(MT(u), s, t, r, q); }
void glMultiTexCoord4i(GLenum u, GLint s, GLint t, GLint r, GLint q) { T4(MT(u), s, t, r, q); }
void glMultiTexCoord4s(GLenum u, GLshort s, GLshort t, GLshort r, GLshort q) { T4(MT(u), s, t, r, q); }
void glMultiTexCoord1fv(GLenum u, const GLfloat *v) { T4(MT(u), v[0], 0, 0, 1); }
void glMultiTexCoord1dv(GLenum u, const GLdouble *v) { T4(MT(u), v[0], 0, 0, 1); }
void glMultiTexCoord1iv(GLenum u, const GLint *v) { T4(MT(u), v[0], 0, 0, 1); }
void glMultiTexCoord1sv(GLenum u, const GLshort *v) { T4(MT(u), v[0], 0, 0, 1); }
void glMultiTexCoord2fv(GLenum u, const GLfloat *v) { T4(MT(u), v[0], v[1], 0, 1); }
void glMultiTexCoord2dv(GLenum u, const GLdouble *v) { T4(MT(u), v[0], v[1], 0, 1); }
void glMultiTexCoord2iv(GLenum u, const GLint *v) { T4(MT(u), v[0], v[1], 0, 1); }
void glMultiTexCoord2sv(GLenum u, const GLshort *v) { T4(MT(u), v[0], v[1], 0, 1); }
void glMultiTexCoord3fv(GLenum u, const GLfloat *v) { T4(MT(u), v[0], v[1], v[2], 1); }
void glMultiTexCoord3dv(GLenum u, const GLdouble *v) { T4(MT(u), v[0], v[1], v[2], 1); }
void glMultiTexCoord3iv(GLenum u, const GLint *v) { T4(MT(u), v[0], v[1], v[2], 1); }
void glMultiTexCoord3sv(GLenum u, const GLshort *v) { T4(MT(u), v[0], v[1], v[2], 1); }
void glMultiTexCoord4fv(GLenum u, const GLfloat *v) { T4(MT(u), v[0], v[1], v[2], v[3]); }
void glMultiTexCoord4dv(GLenum u, const GLdouble *v) { T4(MT(u), v[0], v[1], v[2], v[3]); }
void glMultiTexCoord4iv(GLenum u, const GLint *v) { T4(MT(u), v[0], v[1], v[2], v[3]); }
void glMultiTexCoord4sv(GLenum u, const GLshort *v) { T4(MT(u), v[0], v[1], v[2], v[3]); }
void glMultiTexCoord1fARB(GLenum u, GLfloat s) { glMultiTexCoord1f(u, s); }
void glMultiTexCoord1dARB(GLenum u, GLdouble s) { glMultiTexCoord1d(u, s); }
void glMultiTexCoord1iARB(GLenum u, GLint s) { glMultiTexCoord1i(u, s); }
void glMultiTexCoord1sARB(GLenum u, GLshort s) { glMultiTexCoord1s(u, s); }
void glMultiTexCoord2fARB(GLenum u, GLfloat s, GLfloat t) { glMultiTexCoord2f(u, s, t); }
void glMultiTexCoord2dARB(GLenum u, GLdouble s, GLdouble t) { glMultiTexCoord2d(u, s, t); }
void glMultiTexCoord2iARB(GLenum u, GLint s, GLint t) { glMultiTexCoord2i(u, s, t); }
void glMultiTexCoord2sARB(GLenum u, GLshort s, GLshort t) { glMultiTexCoord2s(u, s, t); }
void glMultiTexCoord3fARB(GLenum u, GLfloat s, GLfloat t, GLfloat r) { glMultiTexCoord3f(u, s, t, r); }
void glMultiTexCoord3dARB(GLenum u, GLdouble s, GLdouble t, GLdouble r) { glMultiTexCoord3d(u, s, t, r); }
void glMultiTexCoord3iARB(GLenum u, GLint s, GLint t, GLint r) { glMultiTexCoord3i(u, s, t, r); }
void glMultiTexCoord3sARB(GLenum u, GLshort s, GLshort t, GLshort r) { glMultiTexCoord3s(u, s, t, r); }
void glMultiTexCoord4fARB(GLenum u, GLfloat s, GLfloat t, GLfloat r, GLfloat q) { glMultiTexCoord4f(u, s, t, r, q); }
void glMultiTexCoord4dARB(GLenum u, GLdouble s, GLdouble t, GLdouble r, GLdouble q) { glMultiTexCoord4d(u, s, t, r, q); }
void glMultiTexCoord4iARB(GLenum u, GLint s, GLint t, GLint r, GLint q) { glMultiTexCoord4i(u, s, t, r, q); }
void glMultiTexCoord4sARB(GLenum u, GLshort s, GLshort t, GLshort r, GLshort q) { glMultiTexCoord4s(u, s, t, r, q); }
void glMultiTexCoord1fvARB(GLenum u, const GLfloat *v) { glMultiTexCoord1fv(u, v); }
void glMultiTexCoord1dvARB(GLenum u, const GLdouble *v) { glMultiTexCoord1dv(u, v); }
void glMultiTexCoord1ivARB(GLenum u, const GLint *v) { glMultiTexCoord1iv(u, v); }
void glMultiTexCoord1svARB(GLenum u, const GLshort *v) { glMultiTexCoord1sv(u, v); }
void glMultiTexCoord2fvARB(GLenum u, const GLfloat *v) { glMultiTexCoord2fv(u, v); }
void glMultiTexCoord2dvARB(GLenum u, const GLdouble *v) { glMultiTexCoord2dv(u, v); }
void glMultiTexCoord2ivARB(GLenum u, const GLint *v) { glMultiTexCoord2iv(u, v); }
void glMultiTexCoord2svARB(GLenum u, const GLshort *v) { glMultiTexCoord2sv(u, v); }
void glMultiTexCoord3fvARB(GLenum u, const GLfloat *v) { glMultiTexCoord3fv(u, v); }
void glMultiTexCoord3dvARB(GLenum u, const GLdouble *v) { glMultiTexCoord3dv(u, v); }
void glMultiTexCoord3ivARB(GLenum u, const GLint *v) { glMultiTexCoord3iv(u, v); }
void glMultiTexCoord3svARB(GLenum u, const GLshort *v) { glMultiTexCoord3sv(u, v); }
void glMultiTexCoord4fvARB(GLenum u, const GLfloat *v) { glMultiTexCoord4fv(u, v); }
void glMultiTexCoord4dvARB(GLenum u, const GLdouble *v) { glMultiTexCoord4dv(u, v); }
void glMultiTexCoord4ivARB(GLenum u, const GLint *v) { glMultiTexCoord4iv(u, v); }
void glMultiTexCoord4svARB(GLenum u, const GLshort *v) { glMultiTexCoord4sv(u, v); }
static void fogc(gl_ctx_t *C, float f) { REC(DL_FOG, f); C->cur_fog = f; }
void glFogCoordf(GLfloat f) { GLC(); fogc(C, f); }
void glFogCoordd(GLdouble f) { GLC(); fogc(C, (float)f); }
void glFogCoordfv(const GLfloat *f) { GLC(); fogc(C, f[0]); }
void glFogCoorddv(const GLdouble *f) { GLC(); fogc(C, (float)f[0]); }
void glFogCoordfEXT(GLfloat f) { glFogCoordf(f); }
void glFogCoorddEXT(GLdouble f) { glFogCoordd(f); }
void glFogCoordfvEXT(const GLfloat *f) { glFogCoordfv(f); }
void glFogCoorddvEXT(const GLdouble *f) { glFogCoorddv(f); }
void glEdgeFlag(GLboolean f) { GLC(); RECU(DL_EDGE, f); C->cur_edge = f != 0; }
void glEdgeFlagv(const GLboolean *f) { glEdgeFlag(f[0]); }
void glIndexf(GLfloat c) { (void)c; }
void glIndexd(GLdouble c) { (void)c; }
void glIndexi(GLint c) { (void)c; }
void glIndexs(GLshort c) { (void)c; }
void glIndexub(GLubyte c) { (void)c; }
void glIndexfv(const GLfloat *c) { (void)c; }
void glIndexdv(const GLdouble *c) { (void)c; }
void glIndexiv(const GLint *c) { (void)c; }
void glIndexsv(const GLshort *c) { (void)c; }
void glIndexubv(const GLubyte *c) { (void)c; }
void glIndexMask(GLuint m) { (void)m; }
void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    glBegin(GL_POLYGON);
    glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
    glEnd();
}
void glRectd(GLdouble a, GLdouble b, GLdouble c, GLdouble d) { glRectf((float)a, (float)b, (float)c, (float)d); }
void glRecti(GLint a, GLint b, GLint c, GLint d) { glRectf((float)a, (float)b, (float)c, (float)d); }
void glRects(GLshort a, GLshort b, GLshort c, GLshort d) { glRectf(a, b, c, d); }
void glRectfv(const GLfloat *a, const GLfloat *b) { glRectf(a[0], a[1], b[0], b[1]); }
void glRectdv(const GLdouble *a, const GLdouble *b) { glRectd(a[0], a[1], b[0], b[1]); }
void glRectiv(const GLint *a, const GLint *b) { glRecti(a[0], a[1], b[0], b[1]); }
void glRectsv(const GLshort *a, const GLshort *b) { glRects(a[0], a[1], b[0], b[1]); }

/* ================================================================ arrays de cliente */
static void ca_set(gl_ctx_t *C, gl_attr_t *a, int size, GLenum type, int norm, GLsizei stride, const void *p) {
    if (stride < 0) { gl_err(C, GL_INVALID_VALUE); return; }
    a->size = size;
    a->type = type;
    a->normalized = norm;
    a->stride = stride;
    a->ptr = p;
    gl_buf_t *b = C->b_array;
    if (a->buf != b) { gl_buf_ref(b); gl_buf_unref(a->buf); a->buf = b; }
}
void glVertexPointer(GLint s, GLenum t, GLsizei st, const void *p) { GLC(); if (s < 2 || s > 4) GLERR(GL_INVALID_VALUE, ); ca_set(C, &C->vao->ca_vertex, s, t, 0, st, p); }
void glNormalPointer(GLenum t, GLsizei st, const void *p) { GLC(); ca_set(C, &C->vao->ca_normal, 3, t, t != GL_FLOAT && t != GL_DOUBLE, st, p); }
void glColorPointer(GLint s, GLenum t, GLsizei st, const void *p) { GLC(); if ((s < 3 || s > 4) && s != GL_BGRA) GLERR(GL_INVALID_VALUE, ); ca_set(C, &C->vao->ca_color, s, t, t != GL_FLOAT && t != GL_DOUBLE && t != GL_HALF_FLOAT, st, p); }
void glSecondaryColorPointer(GLint s, GLenum t, GLsizei st, const void *p) { GLC(); ca_set(C, &C->vao->ca_color2, s, t, t != GL_FLOAT && t != GL_DOUBLE, st, p); }
void glSecondaryColorPointerEXT(GLint s, GLenum t, GLsizei st, const void *p) { glSecondaryColorPointer(s, t, st, p); }
void glFogCoordPointer(GLenum t, GLsizei st, const void *p) { GLC(); ca_set(C, &C->vao->ca_fog, 1, t, 0, st, p); }
void glFogCoordPointerEXT(GLenum t, GLsizei st, const void *p) { glFogCoordPointer(t, st, p); }
void glTexCoordPointer(GLint s, GLenum t, GLsizei st, const void *p) { GLC(); if (s < 1 || s > 4) GLERR(GL_INVALID_VALUE, ); ca_set(C, &C->vao->ca_tc[C->client_unit], s, t, 0, st, p); }
void glIndexPointer(GLenum t, GLsizei st, const void *p) { GLC(); ca_set(C, &C->vao->ca_index, 1, t, 0, st, p); }
void glEdgeFlagPointer(GLsizei st, const void *p) { GLC(); ca_set(C, &C->vao->ca_edge, 1, GL_UNSIGNED_BYTE, 0, st, p); }
void glClientActiveTexture(GLenum u) { GLC(); if (u < GL_TEXTURE0 || u >= GL_TEXTURE0 + 8) GLERR(GL_INVALID_ENUM, ); C->client_unit = (int)(u - GL_TEXTURE0); }
void glClientActiveTextureARB(GLenum u) { glClientActiveTexture(u); }
static gl_attr_t *ca_of(gl_ctx_t *C, GLenum a) {
    switch (a) {
    case GL_VERTEX_ARRAY: return &C->vao->ca_vertex;
    case GL_NORMAL_ARRAY: return &C->vao->ca_normal;
    case GL_COLOR_ARRAY: return &C->vao->ca_color;
    case GL_SECONDARY_COLOR_ARRAY: return &C->vao->ca_color2;
    case GL_FOG_COORD_ARRAY: return &C->vao->ca_fog;
    case GL_TEXTURE_COORD_ARRAY: return &C->vao->ca_tc[C->client_unit];
    case GL_INDEX_ARRAY: return &C->vao->ca_index;
    case GL_EDGE_FLAG_ARRAY: return &C->vao->ca_edge;
    }
    return 0;
}
void glEnableClientState(GLenum a) { GLC(); gl_attr_t *x = ca_of(C, a); if (!x) GLERR(GL_INVALID_ENUM, ); x->enabled = 1; }
void glDisableClientState(GLenum a) { GLC(); gl_attr_t *x = ca_of(C, a); if (!x) GLERR(GL_INVALID_ENUM, ); x->enabled = 0; }
void glInterleavedArrays(GLenum f, GLsizei stride, const void *ptr) {
    GLC();
    int et = 0, ec = 0, en = 0, st = 0, sc = 0, sv = 0, tc = 0, pc = 0, pn = 0, pv = 0, s;
    GLenum ctype = GL_FLOAT;
    int fs = 4;
    switch (f) {
    case GL_V2F: sv = 2; pv = 0; s = 2 * fs; break;
    case GL_V3F: sv = 3; pv = 0; s = 3 * fs; break;
    case GL_C4UB_V2F: ec = 1; sc = 4; sv = 2; ctype = GL_UNSIGNED_BYTE; pc = 0; pv = 4; s = 4 + 2 * fs; break;
    case GL_C4UB_V3F: ec = 1; sc = 4; sv = 3; ctype = GL_UNSIGNED_BYTE; pc = 0; pv = 4; s = 4 + 3 * fs; break;
    case GL_C3F_V3F: ec = 1; sc = 3; sv = 3; pc = 0; pv = 3 * fs; s = 6 * fs; break;
    case GL_N3F_V3F: en = 1; sv = 3; pn = 0; pv = 3 * fs; s = 6 * fs; break;
    case GL_C4F_N3F_V3F: ec = 1; en = 1; sc = 4; sv = 3; pc = 0; pn = 4 * fs; pv = 7 * fs; s = 10 * fs; break;
    case GL_T2F_V3F: et = 1; st = 2; sv = 3; tc = 0; pv = 2 * fs; s = 5 * fs; break;
    case GL_T4F_V4F: et = 1; st = 4; sv = 4; tc = 0; pv = 4 * fs; s = 8 * fs; break;
    case GL_T2F_C4UB_V3F: et = 1; ec = 1; st = 2; sc = 4; sv = 3; ctype = GL_UNSIGNED_BYTE; tc = 0; pc = 2 * fs; pv = pc + 4; s = pv + 3 * fs; break;
    case GL_T2F_C3F_V3F: et = 1; ec = 1; st = 2; sc = 3; sv = 3; tc = 0; pc = 2 * fs; pv = 5 * fs; s = 8 * fs; break;
    case GL_T2F_N3F_V3F: et = 1; en = 1; st = 2; sv = 3; tc = 0; pn = 2 * fs; pv = 5 * fs; s = 8 * fs; break;
    case GL_T2F_C4F_N3F_V3F: et = 1; ec = 1; en = 1; st = 2; sc = 4; sv = 3; tc = 0; pc = 2 * fs; pn = 6 * fs; pv = 9 * fs; s = 12 * fs; break;
    case GL_T4F_C4F_N3F_V4F: et = 1; ec = 1; en = 1; st = 4; sc = 4; sv = 4; tc = 0; pc = 4 * fs; pn = 8 * fs; pv = 11 * fs; s = 15 * fs; break;
    default: GLERR(GL_INVALID_ENUM, );
    }
    if (!stride) stride = s;
    const u8 *p = ptr;
    C->vao->ca_edge.enabled = C->vao->ca_index.enabled = C->vao->ca_fog.enabled = C->vao->ca_color2.enabled = 0;
    gl_attr_t *t = &C->vao->ca_tc[C->client_unit];
    t->enabled = et;
    if (et) ca_set(C, t, st, GL_FLOAT, 0, stride, p + tc);
    C->vao->ca_color.enabled = ec;
    if (ec) ca_set(C, &C->vao->ca_color, sc, ctype, ctype != GL_FLOAT, stride, p + pc);
    C->vao->ca_normal.enabled = en;
    if (en) ca_set(C, &C->vao->ca_normal, 3, GL_FLOAT, 0, stride, p + pn);
    C->vao->ca_vertex.enabled = 1;
    ca_set(C, &C->vao->ca_vertex, sv, GL_FLOAT, 0, stride, p + pv);
}
void glArrayElement(GLint i) {
    GLC();
    float v[4];
    i32 iv[4];
    if (C->vao->ca_normal.enabled && gl_attr_fetch(C, &C->vao->ca_normal, (u32)i, v, iv)) memcpy(C->cur_nrm, v, 12);
    if (C->vao->ca_color.enabled && gl_attr_fetch(C, &C->vao->ca_color, (u32)i, v, iv)) color4(C, v[0], v[1], v[2], C->vao->ca_color.size == 3 ? 1 : v[3]);
    if (C->vao->ca_color2.enabled && gl_attr_fetch(C, &C->vao->ca_color2, (u32)i, v, iv)) memcpy(C->cur_col2, v, 12);
    if (C->vao->ca_fog.enabled && gl_attr_fetch(C, &C->vao->ca_fog, (u32)i, v, iv)) C->cur_fog = v[0];
    for (int u = 0; u < 8; u++) if (C->vao->ca_tc[u].enabled && gl_attr_fetch(C, &C->vao->ca_tc[u], (u32)i, v, iv)) memcpy(C->cur_tc[u], v, 16);
    if (C->vao->ca_edge.enabled && gl_attr_fetch(C, &C->vao->ca_edge, (u32)i, v, iv)) C->cur_edge = v[0] != 0;
    for (int a = 1; a < GLX_ATTR; a++) if (C->vao->a[a].enabled && gl_attr_fetch(C, &C->vao->a[a], (u32)i, v, iv)) { memcpy(C->cur_attr[a], v, 16); memcpy(C->cur_attri[a], iv, 16); }
    if (C->vao->ca_vertex.enabled && gl_attr_fetch(C, &C->vao->ca_vertex, (u32)i, v, iv)) vtx4(C, v[0], v[1], C->vao->ca_vertex.size > 2 ? v[2] : 0, C->vao->ca_vertex.size > 3 ? v[3] : 1);
    else if (C->vao->a[0].enabled && gl_attr_fetch(C, &C->vao->a[0], (u32)i, v, iv)) vtx4(C, v[0], v[1], v[2], v[3]);
}

/* ---- busca de um atributo do fixed function (ou dos embutidos do GLSL compat) para o elemento e */
enum { FA_POS, FA_NRM, FA_COL, FA_COL2, FA_FOG, FA_TC0 };
void gl_ffp_fetch(gl_ctx_t *C, u32 e, int what, float out[4]) {
    float d[4] = {0, 0, 0, 1};
    i32 iv[4];
    if (g_imm_draw && e < C->nimv) {
        gl_imv_t *v = &C->imv[e];
        switch (what) {
        case FA_POS: memcpy(out, v->pos, 16); return;
        case FA_NRM: memcpy(out, v->nrm, 12); out[3] = 0; return;
        case FA_COL: memcpy(out, v->col, 16); return;
        case FA_COL2: memcpy(out, v->col2, 16); return;
        case FA_FOG: out[0] = v->fog; out[1] = out[2] = 0; out[3] = 1; return;
        default: memcpy(out, v->tc[what - FA_TC0], 16); return;
        }
    }
    gl_attr_t *a = 0;
    const float *cur = 0;
    switch (what) {
    case FA_POS: a = C->vao->ca_vertex.enabled ? &C->vao->ca_vertex : C->vao->a[0].enabled ? &C->vao->a[0] : 0; cur = d; break;
    case FA_NRM: a = &C->vao->ca_normal; cur = C->cur_nrm; break;
    case FA_COL: a = &C->vao->ca_color; cur = C->cur_col; break;
    case FA_COL2: a = &C->vao->ca_color2; cur = C->cur_col2; break;
    case FA_FOG: a = &C->vao->ca_fog; cur = &C->cur_fog; break;
    default: a = &C->vao->ca_tc[what - FA_TC0]; cur = C->cur_tc[what - FA_TC0]; break;
    }
    if (a && a->enabled && gl_attr_fetch(C, a, e, d, iv)) {
        if (what == FA_POS) { if (a->size < 3) d[2] = 0; if (a->size < 4) d[3] = 1; }
        if (what == FA_COL && a->size == 3) d[3] = 1;
        memcpy(out, d, 16);
        return;
    }
    if (what == FA_NRM) { memcpy(out, cur, 12); out[3] = 0; return; }
    if (what == FA_FOG) { out[0] = cur[0]; out[1] = out[2] = 0; out[3] = 1; return; }
    memcpy(out, cur, 16);
}
void gl_imm_attr(gl_ctx_t *C, u32 e, int a, float out[4], i32 iout[4]) {
    if (g_imm_draw && e < C->nimv) {
        memcpy(out, a == 0 ? C->imv[e].pos : C->imv[e].attr[a], 16);
        for (int k = 0; k < 4; k++) iout[k] = (i32)out[k];
        return;
    }
    memcpy(out, C->cur_attr[a], 16);
    memcpy(iout, C->cur_attri[a], 16);
}

/* ================================================================ luz, material, fog, texenv (API) */
static int light_n(gl_ctx_t *C, GLenum l) { if (l < GL_LIGHT0 || l > GL_LIGHT7) { gl_err(C, GL_INVALID_ENUM); return -1; } return (int)(l - GL_LIGHT0); }
static void lightv(gl_ctx_t *C, GLenum l, GLenum p, const float *v) {
    if (C->dl_cur && !C->dl_exec) { float a[6] = {(float)l, (float)p, v[0], p == GL_SPOT_DIRECTION || p == GL_POSITION || p == GL_AMBIENT || p == GL_DIFFUSE || p == GL_SPECULAR ? v[1] : 0, 0, 0}; if (p != GL_SPOT_EXPONENT && p != GL_SPOT_CUTOFF && p != GL_CONSTANT_ATTENUATION && p != GL_LINEAR_ATTENUATION && p != GL_QUADRATIC_ATTENUATION) { a[3] = v[1]; a[4] = v[2]; a[5] = p == GL_SPOT_DIRECTION ? 0 : v[3]; } if (gl_dl_record(C, DL_LIGHT, a, sizeof a)) return; }
    int n = light_n(C, l);
    if (n < 0) return;
    gl_light_t *L = &C->light[n];
    const float *mv = C->mv.s[C->mv.top].m;
    switch (p) {
    case GL_AMBIENT: memcpy(L->amb, v, 16); break;
    case GL_DIFFUSE: memcpy(L->dif, v, 16); break;
    case GL_SPECULAR: memcpy(L->spec, v, 16); break;
    case GL_POSITION: gl_mat_vec(L->pos, mv, v); break;     /* guardada no espaço do olho */
    case GL_SPOT_DIRECTION: { float d[4] = {v[0], v[1], v[2], 0}, o[4]; gl_mat_vec(o, mv, d); memcpy(L->spot_dir, o, 12); break; }
    case GL_SPOT_EXPONENT: if (v[0] < 0 || v[0] > 128) GLERR(GL_INVALID_VALUE, ); L->spot_exp = v[0]; break;
    case GL_SPOT_CUTOFF: if ((v[0] < 0 || v[0] > 90) && v[0] != 180) GLERR(GL_INVALID_VALUE, ); L->spot_cut = v[0]; break;
    case GL_CONSTANT_ATTENUATION: L->att[0] = v[0]; break;
    case GL_LINEAR_ATTENUATION: L->att[1] = v[0]; break;
    case GL_QUADRATIC_ATTENUATION: L->att[2] = v[0]; break;
    default: GLERR(GL_INVALID_ENUM, );
    }
}
void glLightfv(GLenum l, GLenum p, const GLfloat *v) { GLC(); lightv(C, l, p, v); }
void glLightf(GLenum l, GLenum p, GLfloat v) { GLC(); float a[4] = {v, 0, 0, 0}; lightv(C, l, p, a); }
void glLightiv(GLenum l, GLenum p, const GLint *v) {
    GLC();
    float a[4];
    int col = p == GL_AMBIENT || p == GL_DIFFUSE || p == GL_SPECULAR;
    for (int i = 0; i < 4; i++) a[i] = col ? NI_(v[i]) : (float)v[i];
    if (p == GL_SPOT_EXPONENT || p == GL_SPOT_CUTOFF || p == GL_CONSTANT_ATTENUATION || p == GL_LINEAR_ATTENUATION || p == GL_QUADRATIC_ATTENUATION) a[1] = a[2] = a[3] = 0;
    lightv(C, l, p, a);
}
void glLighti(GLenum l, GLenum p, GLint v) { glLightf(l, p, (float)v); }
void glGetLightfv(GLenum l, GLenum p, GLfloat *v) {
    GLC();
    int n = light_n(C, l);
    if (n < 0) return;
    gl_light_t *L = &C->light[n];
    switch (p) {
    case GL_AMBIENT: memcpy(v, L->amb, 16); return;
    case GL_DIFFUSE: memcpy(v, L->dif, 16); return;
    case GL_SPECULAR: memcpy(v, L->spec, 16); return;
    case GL_POSITION: memcpy(v, L->pos, 16); return;
    case GL_SPOT_DIRECTION: memcpy(v, L->spot_dir, 12); return;
    case GL_SPOT_EXPONENT: v[0] = L->spot_exp; return;
    case GL_SPOT_CUTOFF: v[0] = L->spot_cut; return;
    case GL_CONSTANT_ATTENUATION: v[0] = L->att[0]; return;
    case GL_LINEAR_ATTENUATION: v[0] = L->att[1]; return;
    case GL_QUADRATIC_ATTENUATION: v[0] = L->att[2]; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glGetLightiv(GLenum l, GLenum p, GLint *v) { float f[4] = {0}; glGetLightfv(l, p, f); int n = p == GL_SPOT_DIRECTION ? 3 : (p == GL_AMBIENT || p == GL_DIFFUSE || p == GL_SPECULAR || p == GL_POSITION) ? 4 : 1; for (int i = 0; i < n; i++) v[i] = (GLint)f[i]; }
static void lmodel(gl_ctx_t *C, GLenum p, const float *v) {
    if (C->dl_cur && !C->dl_exec) { float a[5] = {(float)p, v[0], p == GL_LIGHT_MODEL_AMBIENT ? v[1] : 0, p == GL_LIGHT_MODEL_AMBIENT ? v[2] : 0, p == GL_LIGHT_MODEL_AMBIENT ? v[3] : 0}; if (gl_dl_record(C, DL_LMODEL, a, sizeof a)) return; }
    switch (p) {
    case GL_LIGHT_MODEL_AMBIENT: memcpy(C->lm_amb, v, 16); return;
    case GL_LIGHT_MODEL_LOCAL_VIEWER: C->lm_local = v[0] != 0; return;
    case GL_LIGHT_MODEL_TWO_SIDE: C->lm_two = v[0] != 0; return;
    case GL_LIGHT_MODEL_COLOR_CONTROL: C->lm_sep_spec = (GLenum)v[0] == GL_SEPARATE_SPECULAR_COLOR; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glLightModelfv(GLenum p, const GLfloat *v) { GLC(); lmodel(C, p, v); }
void glLightModelf(GLenum p, GLfloat v) { GLC(); float a[4] = {v, 0, 0, 0}; lmodel(C, p, a); }
void glLightModeliv(GLenum p, const GLint *v) { GLC(); float a[4]; for (int i = 0; i < 4; i++) a[i] = p == GL_LIGHT_MODEL_AMBIENT ? NI_(v[i]) : (float)v[i]; if (p != GL_LIGHT_MODEL_AMBIENT) a[1] = a[2] = a[3] = 0; lmodel(C, p, a); }
void glLightModeli(GLenum p, GLint v) { glLightModelf(p, (float)v); }
static void material(gl_ctx_t *C, GLenum face, GLenum p, const float *v) {
    if (C->dl_cur && !C->dl_exec) { float a[6] = {(float)face, (float)p, v[0], p == GL_SHININESS ? 0 : v[1], p == GL_SHININESS ? 0 : v[2], p == GL_SHININESS ? 0 : v[3]}; if (gl_dl_record(C, DL_MATERIAL, a, sizeof a)) return; }
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) GLERR(GL_INVALID_ENUM, );
    for (int f = 0; f < 2; f++) {
        if (face != GL_FRONT_AND_BACK && (f == 0) != (face == GL_FRONT)) continue;
        gl_mat_t *m = &C->mat[f];
        switch (p) {
        case GL_AMBIENT: memcpy(m->amb, v, 16); break;
        case GL_DIFFUSE: memcpy(m->dif, v, 16); break;
        case GL_SPECULAR: memcpy(m->spec, v, 16); break;
        case GL_EMISSION: memcpy(m->emi, v, 16); break;
        case GL_SHININESS: if (v[0] < 0 || v[0] > 128) GLERR(GL_INVALID_VALUE, ); m->shin = v[0]; break;
        case GL_AMBIENT_AND_DIFFUSE: memcpy(m->amb, v, 16); memcpy(m->dif, v, 16); break;
        case GL_COLOR_INDEXES: m->ci[0] = (int)v[0]; m->ci[1] = (int)v[1]; m->ci[2] = (int)v[2]; break;
        default: GLERR(GL_INVALID_ENUM, );
        }
    }
}
void glMaterialfv(GLenum f, GLenum p, const GLfloat *v) { GLC(); material(C, f, p, v); }
void glMaterialf(GLenum f, GLenum p, GLfloat v) { GLC(); float a[4] = {v, 0, 0, 0}; material(C, f, p, a); }
void glMaterialiv(GLenum f, GLenum p, const GLint *v) { GLC(); float a[4]; for (int i = 0; i < 4; i++) a[i] = p == GL_SHININESS || p == GL_COLOR_INDEXES ? (float)v[i] : NI_(v[i]); if (p == GL_SHININESS) a[1] = a[2] = a[3] = 0; material(C, f, p, a); }
void glMateriali(GLenum f, GLenum p, GLint v) { glMaterialf(f, p, (float)v); }
void glGetMaterialfv(GLenum face, GLenum p, GLfloat *v) {
    GLC();
    gl_mat_t *m = &C->mat[face == GL_BACK ? 1 : 0];
    switch (p) {
    case GL_AMBIENT: memcpy(v, m->amb, 16); return;
    case GL_DIFFUSE: memcpy(v, m->dif, 16); return;
    case GL_SPECULAR: memcpy(v, m->spec, 16); return;
    case GL_EMISSION: memcpy(v, m->emi, 16); return;
    case GL_SHININESS: v[0] = m->shin; return;
    case GL_COLOR_INDEXES: v[0] = (float)m->ci[0]; v[1] = (float)m->ci[1]; v[2] = (float)m->ci[2]; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glGetMaterialiv(GLenum face, GLenum p, GLint *v) { float f[4] = {0}; glGetMaterialfv(face, p, f); int n = p == GL_SHININESS ? 1 : p == GL_COLOR_INDEXES ? 3 : 4; for (int i = 0; i < n; i++) v[i] = (GLint)f[i]; }
void glColorMaterial(GLenum face, GLenum mode) {
    GLC();
    RECU(DL_COLORMAT, face, mode);
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) GLERR(GL_INVALID_ENUM, );
    if (mode != GL_EMISSION && mode != GL_AMBIENT && mode != GL_DIFFUSE && mode != GL_SPECULAR && mode != GL_AMBIENT_AND_DIFFUSE) GLERR(GL_INVALID_ENUM, );
    C->cm_face = face;
    C->cm_mode = mode;
}
void glShadeModel(GLenum m) { GLC(); RECU(DL_SHADE, m); if (m != GL_FLAT && m != GL_SMOOTH) GLERR(GL_INVALID_ENUM, ); C->shade = m; }
void glAlphaFunc(GLenum f, GLfloat ref) { GLC(); if (C->dl_cur && !C->dl_exec) { float a[2] = {(float)f, ref}; if (gl_dl_record(C, DL_ALPHAF, a, 8)) return; } if (f < GL_NEVER || f > GL_ALWAYS) GLERR(GL_INVALID_ENUM, ); C->alpha_func = f; C->alpha_ref = ref < 0 ? 0 : ref > 1 ? 1 : ref; }
static void fogv(gl_ctx_t *C, GLenum p, const float *v) {
    if (C->dl_cur && !C->dl_exec) { float a[5] = {(float)p, v[0], p == GL_FOG_COLOR ? v[1] : 0, p == GL_FOG_COLOR ? v[2] : 0, p == GL_FOG_COLOR ? v[3] : 0}; if (gl_dl_record(C, DL_FOGP, a, sizeof a)) return; }
    switch (p) {
    case GL_FOG_MODE: { GLenum m = (GLenum)v[0]; if (m != GL_LINEAR && m != GL_EXP && m != GL_EXP2) GLERR(GL_INVALID_ENUM, ); C->fog_mode = m; return; }
    case GL_FOG_DENSITY: if (v[0] < 0) GLERR(GL_INVALID_VALUE, ); C->fog_d = v[0]; return;
    case GL_FOG_START: C->fog_s = v[0]; return;
    case GL_FOG_END: C->fog_e = v[0]; return;
    case GL_FOG_COLOR: memcpy(C->fog_c, v, 16); return;
    case GL_FOG_INDEX: C->fog_index = v[0]; return;
    case GL_FOG_COORD_SRC: C->fog_src = (GLenum)v[0]; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glFogfv(GLenum p, const GLfloat *v) { GLC(); fogv(C, p, v); }
void glFogf(GLenum p, GLfloat v) { GLC(); float a[4] = {v, 0, 0, 0}; fogv(C, p, a); }
void glFogiv(GLenum p, const GLint *v) { GLC(); float a[4]; for (int i = 0; i < 4; i++) a[i] = p == GL_FOG_COLOR ? NI_(v[i]) : (float)v[i]; if (p != GL_FOG_COLOR) a[1] = a[2] = a[3] = 0; fogv(C, p, a); }
void glFogi(GLenum p, GLint v) { glFogf(p, (float)v); }
static int env_src_ok(GLenum s) { return s == GL_TEXTURE || s == GL_CONSTANT || s == GL_PRIMARY_COLOR || s == GL_PREVIOUS || (s >= GL_TEXTURE0 && s < GL_TEXTURE0 + 8) || s == GL_ZERO || s == GL_ONE; }
static void texenv(gl_ctx_t *C, GLenum target, GLenum p, const float *v) {
    if (C->dl_cur && !C->dl_exec) { float a[6] = {(float)target, (float)p, v[0], p == GL_TEXTURE_ENV_COLOR ? v[1] : 0, p == GL_TEXTURE_ENV_COLOR ? v[2] : 0, p == GL_TEXTURE_ENV_COLOR ? v[3] : 0}; if (gl_dl_record(C, DL_TEXENV, a, sizeof a)) return; }
    int u = C->unit < 8 ? C->unit : 7;
    gl_texenv_t *t = &C->te[u];
    GLenum e = (GLenum)v[0];
    if (target == GL_TEXTURE_FILTER_CONTROL) { if (p == GL_TEXTURE_LOD_BIAS) { t->lod_bias = v[0]; return; } GLERR(GL_INVALID_ENUM, ); }
    if (target == GL_POINT_SPRITE) { if (p == GL_COORD_REPLACE) { t->coord_replace = v[0] != 0; return; } GLERR(GL_INVALID_ENUM, ); }
    if (target != GL_TEXTURE_ENV) GLERR(GL_INVALID_ENUM, );
    switch (p) {
    case GL_TEXTURE_ENV_MODE:
        if (e != GL_MODULATE && e != GL_DECAL && e != GL_BLEND && e != GL_REPLACE && e != GL_ADD && e != GL_COMBINE) GLERR(GL_INVALID_ENUM, );
        t->mode = e;
        return;
    case GL_TEXTURE_ENV_COLOR: memcpy(t->color, v, 16); return;
    case GL_COMBINE_RGB: t->comb_rgb = e; return;
    case GL_COMBINE_ALPHA: t->comb_a = e; return;
    case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB: if (!env_src_ok(e)) GLERR(GL_INVALID_ENUM, ); t->src[0][p - GL_SRC0_RGB] = e; return;
    case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA: if (!env_src_ok(e)) GLERR(GL_INVALID_ENUM, ); t->src[1][p - GL_SRC0_ALPHA] = e; return;
    case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB: t->op[0][p - GL_OPERAND0_RGB] = e; return;
    case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA: t->op[1][p - GL_OPERAND0_ALPHA] = e; return;
    case GL_RGB_SCALE: if (v[0] != 1 && v[0] != 2 && v[0] != 4) GLERR(GL_INVALID_VALUE, ); t->scale_rgb = v[0]; return;
    case GL_ALPHA_SCALE: if (v[0] != 1 && v[0] != 2 && v[0] != 4) GLERR(GL_INVALID_VALUE, ); t->scale_a = v[0]; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glTexEnvfv(GLenum t, GLenum p, const GLfloat *v) { GLC(); texenv(C, t, p, v); }
void glTexEnvf(GLenum t, GLenum p, GLfloat v) { GLC(); float a[4] = {v, 0, 0, 0}; texenv(C, t, p, a); }
void glTexEnviv(GLenum t, GLenum p, const GLint *v) { GLC(); float a[4]; for (int i = 0; i < 4; i++) a[i] = p == GL_TEXTURE_ENV_COLOR ? NI_(v[i]) : (float)v[i]; if (p != GL_TEXTURE_ENV_COLOR) a[1] = a[2] = a[3] = 0; texenv(C, t, p, a); }
void glTexEnvi(GLenum t, GLenum p, GLint v) { glTexEnvf(t, p, (float)v); }
void glGetTexEnvfv(GLenum target, GLenum p, GLfloat *v) {
    GLC();
    gl_texenv_t *t = &C->te[C->unit < 8 ? C->unit : 7];
    if (target == GL_TEXTURE_FILTER_CONTROL) { v[0] = t->lod_bias; return; }
    if (target == GL_POINT_SPRITE) { v[0] = (float)t->coord_replace; return; }
    switch (p) {
    case GL_TEXTURE_ENV_MODE: v[0] = (float)t->mode; return;
    case GL_TEXTURE_ENV_COLOR: memcpy(v, t->color, 16); return;
    case GL_COMBINE_RGB: v[0] = (float)t->comb_rgb; return;
    case GL_COMBINE_ALPHA: v[0] = (float)t->comb_a; return;
    case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB: v[0] = (float)t->src[0][p - GL_SRC0_RGB]; return;
    case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA: v[0] = (float)t->src[1][p - GL_SRC0_ALPHA]; return;
    case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB: v[0] = (float)t->op[0][p - GL_OPERAND0_RGB]; return;
    case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA: v[0] = (float)t->op[1][p - GL_OPERAND0_ALPHA]; return;
    case GL_RGB_SCALE: v[0] = t->scale_rgb; return;
    case GL_ALPHA_SCALE: v[0] = t->scale_a; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glGetTexEnviv(GLenum t, GLenum p, GLint *v) { float f[4] = {0}; glGetTexEnvfv(t, p, f); int n = p == GL_TEXTURE_ENV_COLOR ? 4 : 1; for (int i = 0; i < n; i++) v[i] = p == GL_TEXTURE_ENV_COLOR ? (GLint)(f[i] * 2147483647.0) : (GLint)f[i]; }
static void texgen(gl_ctx_t *C, GLenum coord, GLenum p, const double *v) {
    if (C->dl_cur && !C->dl_exec) { float a[6] = {(float)coord, (float)p, (float)v[0], p == GL_TEXTURE_GEN_MODE ? 0 : (float)v[1], p == GL_TEXTURE_GEN_MODE ? 0 : (float)v[2], p == GL_TEXTURE_GEN_MODE ? 0 : (float)v[3]}; if (gl_dl_record(C, DL_TEXGEN, a, sizeof a)) return; }
    int c = coord == GL_S ? 0 : coord == GL_T ? 1 : coord == GL_R ? 2 : coord == GL_Q ? 3 : -1;
    if (c < 0) GLERR(GL_INVALID_ENUM, );
    gl_texenv_t *t = &C->te[C->unit < 8 ? C->unit : 7];
    switch (p) {
    case GL_TEXTURE_GEN_MODE: {
        GLenum m = (GLenum)v[0];
        if (m != GL_OBJECT_LINEAR && m != GL_EYE_LINEAR && m != GL_SPHERE_MAP && m != GL_NORMAL_MAP && m != GL_REFLECTION_MAP) GLERR(GL_INVALID_ENUM, );
        if (m == GL_SPHERE_MAP && c >= 2) GLERR(GL_INVALID_ENUM, );
        t->gen_mode[c] = m;
        return;
    }
    case GL_OBJECT_PLANE: for (int i = 0; i < 4; i++) t->gen_obj[c][i] = (float)v[i]; return;
    case GL_EYE_PLANE: {
        /* plano do olho: p * inversa(MV) */
        float inv[16], pl[4];
        gl_mat_inv(inv, C->mv.s[C->mv.top].m);
        for (int j = 0; j < 4; j++) pl[j] = (float)(v[0] * inv[j * 4 + 0] + v[1] * inv[j * 4 + 1] + v[2] * inv[j * 4 + 2] + v[3] * inv[j * 4 + 3]);
        memcpy(t->gen_eye[c], pl, 16);
        return;
    }
    }
    GLERR(GL_INVALID_ENUM, );
}
void glTexGendv(GLenum c, GLenum p, const GLdouble *v) { GLC(); texgen(C, c, p, v); }
void glTexGenfv(GLenum c, GLenum p, const GLfloat *v) { GLC(); double d[4] = {v[0], p == GL_TEXTURE_GEN_MODE ? 0 : v[1], p == GL_TEXTURE_GEN_MODE ? 0 : v[2], p == GL_TEXTURE_GEN_MODE ? 0 : v[3]}; texgen(C, c, p, d); }
void glTexGeniv(GLenum c, GLenum p, const GLint *v) { GLC(); double d[4] = {v[0], p == GL_TEXTURE_GEN_MODE ? 0 : v[1], p == GL_TEXTURE_GEN_MODE ? 0 : v[2], p == GL_TEXTURE_GEN_MODE ? 0 : v[3]}; texgen(C, c, p, d); }
void glTexGend(GLenum c, GLenum p, GLdouble v) { GLC(); double d[4] = {v, 0, 0, 0}; texgen(C, c, p, d); }
void glTexGenf(GLenum c, GLenum p, GLfloat v) { glTexGend(c, p, v); }
void glTexGeni(GLenum c, GLenum p, GLint v) { glTexGend(c, p, v); }
void glGetTexGendv(GLenum coord, GLenum p, GLdouble *v) {
    GLC();
    int c = coord == GL_S ? 0 : coord == GL_T ? 1 : coord == GL_R ? 2 : coord == GL_Q ? 3 : -1;
    if (c < 0) GLERR(GL_INVALID_ENUM, );
    gl_texenv_t *t = &C->te[C->unit < 8 ? C->unit : 7];
    if (p == GL_TEXTURE_GEN_MODE) { v[0] = t->gen_mode[c]; return; }
    const float *s = p == GL_OBJECT_PLANE ? t->gen_obj[c] : p == GL_EYE_PLANE ? t->gen_eye[c] : 0;
    if (!s) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < 4; i++) v[i] = s[i];
}
void glGetTexGenfv(GLenum c, GLenum p, GLfloat *v) { double d[4] = {0}; glGetTexGendv(c, p, d); int n = p == GL_TEXTURE_GEN_MODE ? 1 : 4; for (int i = 0; i < n; i++) v[i] = (float)d[i]; }
void glGetTexGeniv(GLenum c, GLenum p, GLint *v) { double d[4] = {0}; glGetTexGendv(c, p, d); int n = p == GL_TEXTURE_GEN_MODE ? 1 : 4; for (int i = 0; i < n; i++) v[i] = (GLint)d[i]; }
void glClipPlane(GLenum plane, const GLdouble *eq) {
    GLC();
    if (C->dl_cur && !C->dl_exec) { float a[5] = {(float)plane, (float)eq[0], (float)eq[1], (float)eq[2], (float)eq[3]}; if (gl_dl_record(C, DL_CLIPP, a, sizeof a)) return; }
    int p = (int)(plane - GL_CLIP_PLANE0);
    if (p < 0 || p >= GLX_CLIP) GLERR(GL_INVALID_ENUM, );
    float inv[16];
    gl_mat_inv(inv, C->mv.s[C->mv.top].m);
    for (int j = 0; j < 4; j++) C->clip_plane[p][j] = eq[0] * inv[j * 4 + 0] + eq[1] * inv[j * 4 + 1] + eq[2] * inv[j * 4 + 2] + eq[3] * inv[j * 4 + 3];
}
void glGetClipPlane(GLenum plane, GLdouble *eq) {
    GLC();
    int p = (int)(plane - GL_CLIP_PLANE0);
    if (p < 0 || p >= GLX_CLIP) GLERR(GL_INVALID_ENUM, );
    for (int j = 0; j < 4; j++) eq[j] = C->clip_plane[p][j];
}
void glLineStipple(GLint f, GLushort p) { GLC(); RECU(DL_LSTIPPLE, (u32)f, p); C->ls_factor = f < 1 ? 1 : f > 256 ? 256 : f; C->ls_pattern = p; }
/* máscara 32x32 de 1 bit, lida e escrita com os parâmetros de unpack/pack (alinhamento, ROW_LENGTH, SKIP_*, LSB_FIRST, PBO) */
static int pstip_unpack(gl_ctx_t *C, const GLubyte *m, u32 rows[32]) {
    const u8 *src = m;
    if (C->b_unpack) src = C->b_unpack->data ? C->b_unpack->data + (uintptr_t)m : 0;
    if (!src) return 0;
    int rl = C->up_row > 0 ? C->up_row : 32;
    i64 rs = ((rl + 7) / 8 + C->up_align - 1) / C->up_align * C->up_align;
    for (int y = 0; y < 32; y++) {
        const u8 *r = src + (i64)(y + C->up_skrow) * rs;
        u32 v = 0;
        for (int x = 0; x < 32; x++) {
            int c = x + C->up_skpix;
            int bit = C->up_lsb ? (r[c >> 3] >> (c & 7)) & 1 : (r[c >> 3] >> (7 - (c & 7))) & 1;
            v |= (u32)bit << x;
        }
        rows[y] = v;
    }
    return 1;
}
void glPolygonStipple(const GLubyte *m) {
    GLC();
    u32 rows[32];
    if (!pstip_unpack(C, m, rows)) return;
    if (C->dl_cur && !C->dl_exec) { u32 z = 0; if (dl_blob(C, DL_PSTIPPLE, &z, sizeof z, rows, sizeof rows)) return; }
    memcpy(C->pstipple, rows, sizeof rows);
}
void glGetPolygonStipple(GLubyte *m) {
    GLC();
    u8 *dst = m;
    if (C->b_pack) dst = C->b_pack->data ? C->b_pack->data + (uintptr_t)m : 0;
    if (!dst) return;
    int rl = C->pk_row > 0 ? C->pk_row : 32;
    i64 rs = ((rl + 7) / 8 + C->pk_align - 1) / C->pk_align * C->pk_align;
    for (int y = 0; y < 32; y++) {
        u8 *r = dst + (i64)(y + C->pk_skrow) * rs;
        for (int x = 0; x < 32; x++) {
            int c = x + C->pk_skpix, b = C->pk_lsb ? (c & 7) : 7 - (c & 7);
            if (C->pstipple[y] >> x & 1) r[c >> 3] |= (u8)(1 << b); else r[c >> 3] &= (u8)~(1 << b);
        }
    }
}

/* ================================================================ vertex shader do fixed function */
static void nrm3(float *v) { double l = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]); if (l > 0) for (int i = 0; i < 3; i++) v[i] = (float)(v[i] / l); }
static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
typedef struct { float mvp[16], nm[9], mv[16]; int lighting, nlights, normalize, rescale; float rescale_f; int texgen_any, tex_on[8]; int fog_on; } ffp_t;
typedef struct ftex ftex_t;
struct ftex { msl_tex_t *mt; msl_samp_t s; int nc; GLenum base; int size[3]; int on; };
typedef struct { ffp_t F; struct ftex *T; } ffps_t;
#define g_ffp (*(ffp_t *)ffp_state(C))
static void *ffp_state(gl_ctx_t *C) {
    if (!C->ffp_state) {
        ffps_t *p = calloc(1, sizeof *p + 8 * sizeof(struct ftex));
        if (p) p->T = (struct ftex *)(p + 1);
        C->ffp_state = p;
    }
    return C->ffp_state;
}
static void light_vertex(gl_ctx_t *C, const float *eye, float *n, int face, const float *col, float out0[4], float out1[4]) {
    gl_mat_t m = C->mat[face];
    if (gl_is_on(C, GL_COLOR_MATERIAL) && (C->cm_face == GL_FRONT_AND_BACK || (face == 0) == (C->cm_face == GL_FRONT))) {
        switch (C->cm_mode) {
        case GL_AMBIENT: memcpy(m.amb, col, 16); break;
        case GL_DIFFUSE: memcpy(m.dif, col, 16); break;
        case GL_SPECULAR: memcpy(m.spec, col, 16); break;
        case GL_EMISSION: memcpy(m.emi, col, 16); break;
        default: memcpy(m.amb, col, 16); memcpy(m.dif, col, 16);
        }
    }
    float c[3], s[3] = {0, 0, 0};
    for (int k = 0; k < 3; k++) c[k] = m.emi[k] + m.amb[k] * C->lm_amb[k];
    float V[3] = {0, 0, 1};
    if (C->lm_local) { V[0] = -eye[0]; V[1] = -eye[1]; V[2] = -eye[2]; nrm3(V); }
    for (int i = 0; i < GLX_LIGHTS; i++) {
        gl_light_t *L = &C->light[i];
        if (!L->on) continue;
        float Ld[3];
        double att = 1;
        if (L->pos[3] != 0) {
            for (int k = 0; k < 3; k++) Ld[k] = L->pos[k] / L->pos[3] - eye[k];
            double d = sqrt((double)dot3(Ld, Ld));
            att = 1.0 / (L->att[0] + L->att[1] * d + L->att[2] * d * d);
            nrm3(Ld);
            if (L->spot_cut != 180) {
                float sd[3] = {L->spot_dir[0], L->spot_dir[1], L->spot_dir[2]};
                nrm3(sd);
                float mLd[3] = {-Ld[0], -Ld[1], -Ld[2]};
                double sdot = dot3(mLd, sd);
                if (sdot < cos(L->spot_cut * 3.14159265358979323846 / 180)) att = 0;
                else att *= pow(sdot > 0 ? sdot : 0, L->spot_exp);
            }
        } else { memcpy(Ld, L->pos, 12); nrm3(Ld); }
        double nl = dot3(n, Ld);
        if (nl < 0) nl = 0;
        float H[3] = {Ld[0] + V[0], Ld[1] + V[1], Ld[2] + V[2]};
        nrm3(H);
        double nh = dot3(n, H);
        double sp = nl > 0 && nh > 0 ? pow(nh, m.shin) : 0;
        for (int k = 0; k < 3; k++) {
            c[k] += (float)(att * (m.amb[k] * L->amb[k] + nl * m.dif[k] * L->dif[k]));
            float spec = (float)(att * sp * m.spec[k] * L->spec[k]);
            if (C->lm_sep_spec) s[k] += spec; else c[k] += spec;
        }
    }
    for (int k = 0; k < 3; k++) { out0[k] = c[k] < 0 ? 0 : c[k] > 1 ? 1 : c[k]; out1[k] = s[k] < 0 ? 0 : s[k] > 1 ? 1 : s[k]; }
    out0[3] = m.dif[3] < 0 ? 0 : m.dif[3] > 1 ? 1 : m.dif[3];
    out1[3] = 1;
}
static void ffp_vs(gl_ctx_t *C, u32 e, u32 iid, gl_vtx_t *o) {
    (void)iid;
    ffp_t *F = &g_ffp;
    float pos[4], nrm[4], col[4], col2[4];
    gl_ffp_fetch(C, e, FA_POS, pos);
    gl_mat_vec(o->clip, F->mvp, pos);
    float eye[4];
    gl_mat_vec(eye, F->mv, pos);
    float n[3] = {0, 0, 1};
    int need_n = F->lighting || F->texgen_any;
    if (need_n) {
        gl_ffp_fetch(C, e, FA_NRM, nrm);
        for (int i = 0; i < 3; i++) n[i] = F->nm[i] * nrm[0] + F->nm[3 + i] * nrm[1] + F->nm[6 + i] * nrm[2];
        if (F->rescale) for (int i = 0; i < 3; i++) n[i] *= F->rescale_f;
        if (F->normalize) nrm3(n);
    }
    gl_ffp_fetch(C, e, FA_COL, col);
    if (F->lighting) {
        float ec[3] = {eye[3] ? eye[0] / eye[3] : eye[0], eye[3] ? eye[1] / eye[3] : eye[1], eye[3] ? eye[2] / eye[3] : eye[2]};
        light_vertex(C, ec, n, 0, col, o->v[S_COL0], o->v[S_COL1]);
        if (C->lm_two) { float bn[3] = {-n[0], -n[1], -n[2]}; light_vertex(C, ec, bn, 1, col, o->v[S_BCOL0], o->v[S_BCOL1]); }
    } else {
        gl_ffp_fetch(C, e, FA_COL2, col2);
        memcpy(o->v[S_COL0], col, 16);
        memcpy(o->v[S_COL1], col2, 16);
        o->v[S_COL1][3] = 1;
    }
    for (int u = 0; u < 8; u++) {
        if (!F->tex_on[u]) continue;
        float tc[4];
        gl_ffp_fetch(C, e, FA_TC0 + u, tc);
        gl_texenv_t *t = &C->te[u];
        float r[3] = {0, 0, 0}, sm[2] = {0, 0};
        int refl_done = 0;
        for (int c = 0; c < 4; c++) {
            if (!gl_unit_on(C, u, GL_TEXTURE_GEN_S + c)) continue;
            switch (t->gen_mode[c]) {
            case GL_OBJECT_LINEAR: tc[c] = t->gen_obj[c][0] * pos[0] + t->gen_obj[c][1] * pos[1] + t->gen_obj[c][2] * pos[2] + t->gen_obj[c][3] * pos[3]; break;
            case GL_EYE_LINEAR: tc[c] = t->gen_eye[c][0] * eye[0] + t->gen_eye[c][1] * eye[1] + t->gen_eye[c][2] * eye[2] + t->gen_eye[c][3] * eye[3]; break;
            case GL_NORMAL_MAP: tc[c] = c < 3 ? n[c] : tc[c]; break;
            case GL_SPHERE_MAP: case GL_REFLECTION_MAP:
                if (!refl_done) {
                    float u3[3] = {eye[0], eye[1], eye[2]};
                    nrm3(u3);
                    float d = dot3(n, u3);
                    for (int k = 0; k < 3; k++) r[k] = u3[k] - 2 * n[k] * d;
                    double m = 2 * sqrt((double)r[0] * r[0] + (double)r[1] * r[1] + (r[2] + 1.0) * (r[2] + 1.0));
                    sm[0] = (float)(r[0] / (m ? m : 1) + 0.5);
                    sm[1] = (float)(r[1] / (m ? m : 1) + 0.5);
                    refl_done = 1;
                }
                tc[c] = t->gen_mode[c] == GL_SPHERE_MAP ? sm[c] : c < 3 ? r[c] : tc[c];
                break;
            }
        }
        gl_mat_vec(o->v[S_TEX0 + u], C->texm[u].s[C->texm[u].top].m, tc);
    }
    if (F->fog_on) {
        float fc[4];
        if (C->fog_src == GL_FOG_COORD) gl_ffp_fetch(C, e, FA_FOG, fc);
        else fc[0] = (float)fabs(eye[2] / (eye[3] ? eye[3] : 1));
        o->v[S_FOG][0] = fc[0];
    }
    for (int p = 0; p < GLX_CLIP; p++) o->cd[p] = (float)(C->clip_plane[p][0] * eye[0] + C->clip_plane[p][1] * eye[1] + C->clip_plane[p][2] * eye[2] + C->clip_plane[p][3] * eye[3]);
    o->psize = C->point_s;
    if (C->point_att[1] != 0 || C->point_att[2] != 0 || C->point_att[0] != 1) {
        double d = fabs(eye[2]);
        double a = C->point_att[0] + C->point_att[1] * d + C->point_att[2] * d * d;
        o->psize = (float)(C->point_s * sqrt(a > 0 ? 1 / a : 1));
    }
    if (o->psize < C->point_min) o->psize = C->point_min;
    if (o->psize > C->point_max) o->psize = C->point_max;
    float ef[4];
    i32 iv[4];
    o->edge = 1;
    if (g_imm_draw && e < C->nimv) o->edge = C->imv[e].edge;
    else if (C->vao->ca_edge.enabled && gl_attr_fetch(C, &C->vao->ca_edge, e, ef, iv)) o->edge = ef[0] != 0;
}

/* ================================================================ fragment shader do fixed function */
#define g_ftex (((ffps_t *)ffp_state(C))->T)
static GLenum tex_base_of(gl_tex_t *t) { int b = t->base_level >= 0 && t->base_level < 16 ? t->base_level : 0; return gl_ifmt_base(t->lv[b].ifmt); }
static void env_arg(gl_ctx_t *C, int u, GLenum src, GLenum op, int alpha, const float *tex, const float *prev, const float *prim, const float texu[8][4], float o[4]) {
    const float *s;
    static const float zero[4] = {0, 0, 0, 0}, one[4] = {1, 1, 1, 1};
    switch (src) {
    case GL_TEXTURE: s = tex; break;
    case GL_CONSTANT: s = C->te[u].color; break;
    case GL_PRIMARY_COLOR: s = prim; break;
    case GL_PREVIOUS: s = prev; break;
    case GL_ZERO: s = zero; break;
    case GL_ONE: s = one; break;
    default: s = src >= GL_TEXTURE0 && src < GL_TEXTURE0 + 8 ? texu[src - GL_TEXTURE0] : tex;
    }
    for (int k = 0; k < 4; k++) {
        float v;
        switch (op) {
        case GL_SRC_COLOR: v = s[k]; break;
        case GL_ONE_MINUS_SRC_COLOR: v = 1 - s[k]; break;
        case GL_SRC_ALPHA: v = s[3]; break;
        case GL_ONE_MINUS_SRC_ALPHA: v = 1 - s[3]; break;
        default: v = s[k];
        }
        o[k] = v;
    }
    (void)alpha;
}
static float combine(GLenum f, int k, const float a[3][4]) {
    switch (f) {
    case GL_REPLACE: return a[0][k];
    case GL_MODULATE: return a[0][k] * a[1][k];
    case GL_ADD: return a[0][k] + a[1][k];
    case GL_ADD_SIGNED: return a[0][k] + a[1][k] - 0.5f;
    case GL_INTERPOLATE: return a[0][k] * a[2][k] + a[1][k] * (1 - a[2][k]);
    case GL_SUBTRACT: return a[0][k] - a[1][k];
    case GL_DOT3_RGB: case GL_DOT3_RGBA: return 4 * ((a[0][0] - 0.5f) * (a[1][0] - 0.5f) + (a[0][1] - 0.5f) * (a[1][1] - 0.5f) + (a[0][2] - 0.5f) * (a[1][2] - 0.5f));
    }
    return a[0][k];
}
static float cl(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static void ffp_fs(gl_ctx_t *C, gl_frag_t *f) {
    float prim[4], prev[4], texu[8][4];
    memcpy(prim, f->v[S_COL0], 16);
    memcpy(prev, prim, 16);
    for (int u = 0; u < 8; u++) {
        ftex_t *T = &g_ftex[u];
        if (!T->on) continue;
        float *tc = f->v[S_TEX0 + u];
        double q = tc[3] != 0 ? tc[3] : 1;
        double c[3] = {tc[0] / q, tc[1] / q, tc[2] / q};
        if (T->mt->type == MSL_TEX_CUBE) { c[0] = tc[0]; c[1] = tc[1]; c[2] = tc[2]; }
        /* lod pela derivada da coordenada em texels */
        double lod = 0;
        if (T->mt->levels > 1 || T->s.minf != T->s.magf) {
            double ax = 0, ay = 0;
            for (int k = 0; k < T->nc && k < 3; k++) {
                double sz = T->s.normalized ? T->size[k] : 1;
                double dx = f->dx[S_TEX0 + u][k] / q * sz, dy = f->dy[S_TEX0 + u][k] / q * sz;
                ax += dx * dx;
                ay += dy * dy;
            }
            double rho2 = ax > ay ? ax : ay;
            lod = rho2 > 0 ? 0.5 * __builtin_log2(rho2) : -1000;
            lod += C->te[u].lod_bias + T->s.lodbias;
        }
        double out[4];
        if (T->s.compare != 7 && mx_isdepth(T->mt->fmt)) {
            /* ARB_shadow: r comparado com a profundidade; o swizzle depois aplica o GL_DEPTH_TEXTURE_MODE */
            msl_samp_t s2 = T->s;
            s2.compare = 7;
            msl_tex_sample(T->mt, &s2, c, 2, 0, lod, out);
            double r = c[2] > 1 ? 1 : c[2], d = out[0];
            int pass;
            switch (T->s.compare) {
            case 0: pass = 0; break;
            case 1: pass = r < d; break;
            case 2: pass = r == d; break;
            case 3: pass = r <= d; break;
            case 4: pass = r > d; break;
            case 5: pass = r != d; break;
            case 6: pass = r >= d; break;
            default: pass = 1;
            }
            out[0] = pass; out[1] = out[2] = 0; out[3] = 1;
        } else msl_tex_sample(T->mt, &T->s, c, T->nc, 0, lod, out);
        msl_tex_swizzle(T->mt, out);
        for (int k = 0; k < 4; k++) texu[u][k] = (float)out[k];
    }
    for (int u = 0; u < 8; u++) {
        ftex_t *T = &g_ftex[u];
        if (!T->on) continue;
        gl_texenv_t *E = &C->te[u];
        float *t = texu[u], r[4];
        GLenum b = T->base;
        int has_rgb = b != GL_ALPHA, has_a = b == GL_ALPHA || b == GL_LUMINANCE_ALPHA || b == GL_RGBA || b == GL_INTENSITY;
        switch (E->mode) {
        case GL_REPLACE:
            for (int k = 0; k < 3; k++) r[k] = has_rgb ? t[k] : prev[k];
            r[3] = has_a ? t[3] : prev[3];
            break;
        case GL_MODULATE:
            for (int k = 0; k < 3; k++) r[k] = has_rgb ? prev[k] * t[k] : prev[k];
            r[3] = has_a ? prev[3] * t[3] : prev[3];
            break;
        case GL_DECAL:
            if (b == GL_RGBA) for (int k = 0; k < 3; k++) r[k] = prev[k] * (1 - t[3]) + t[k] * t[3];
            else for (int k = 0; k < 3; k++) r[k] = has_rgb ? t[k] : prev[k];
            r[3] = prev[3];
            break;
        case GL_BLEND:
            for (int k = 0; k < 3; k++) r[k] = has_rgb ? prev[k] * (1 - t[k]) + E->color[k] * t[k] : prev[k];
            r[3] = b == GL_INTENSITY ? prev[3] * (1 - t[3]) + E->color[3] * t[3] : has_a ? prev[3] * t[3] : prev[3];
            break;
        case GL_ADD:
            for (int k = 0; k < 3; k++) r[k] = has_rgb ? prev[k] + t[k] : prev[k];
            r[3] = b == GL_INTENSITY ? prev[3] + t[3] : has_a ? prev[3] * t[3] : prev[3];
            break;
        default: {   /* GL_COMBINE */
            float a[3][4], aa[3][4];
            for (int i = 0; i < 3; i++) {
                env_arg(C, u, E->src[0][i], E->op[0][i], 0, t, prev, prim, texu, a[i]);
                env_arg(C, u, E->src[1][i], E->op[1][i], 1, t, prev, prim, texu, aa[i]);
                for (int k = 0; k < 4; k++) a[i][k] = a[i][k];
                aa[i][3] = aa[i][3];
            }
            for (int k = 0; k < 3; k++) r[k] = combine(E->comb_rgb, k, a) * E->scale_rgb;
            float al[3][4];
            for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) al[i][k] = aa[i][3];
            r[3] = E->comb_rgb == GL_DOT3_RGBA ? r[0] : combine(E->comb_a, 3, al) * E->scale_a;
        }
        }
        for (int k = 0; k < 4; k++) prev[k] = cl(r[k]);
    }
    /* soma de cores */
    if (gl_is_on(C, GL_COLOR_SUM) || (gl_is_on(C, GL_LIGHTING) && C->lm_sep_spec))
        for (int k = 0; k < 3; k++) prev[k] = cl(prev[k] + f->v[S_COL1][k]);
    if (g_ffp.fog_on) {
        double z = f->v[S_FOG][0], ff;
        if (C->fog_mode == GL_LINEAR) ff = C->fog_e != C->fog_s ? (C->fog_e - z) / (C->fog_e - C->fog_s) : 1;
        else if (C->fog_mode == GL_EXP) ff = exp(-C->fog_d * z);
        else ff = exp(-(C->fog_d * z) * (C->fog_d * z));
        ff = ff < 0 ? 0 : ff > 1 ? 1 : ff;
        for (int k = 0; k < 3; k++) prev[k] = (float)(ff * prev[k] + (1 - ff) * C->fog_c[k]);
    }
    for (int i = 0; i < GLX_DRAW; i++) memcpy(f->color[i], prev, 16);
}
/* monta o estágio do fixed function para o draw atual */
void gl_ffp_stage(gl_ctx_t *C) {
    ffp_t *F = &g_ffp;
    const float *mv = C->mv.s[C->mv.top].m, *pj = C->proj.s[C->proj.top].m;
    memcpy(F->mv, mv, 64);
    gl_mat_mul(F->mvp, pj, mv);
    float inv[16];
    gl_mat_inv(inv, mv);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) F->nm[i * 3 + j] = inv[j * 4 + i];   /* transposta da inversa */
    F->lighting = gl_is_on(C, GL_LIGHTING);
    F->normalize = gl_is_on(C, GL_NORMALIZE);
    F->rescale = gl_is_on(C, GL_RESCALE_NORMAL) && !F->normalize;
    if (F->rescale) { double s = sqrt((double)inv[8] * inv[8] + (double)inv[9] * inv[9] + (double)inv[10] * inv[10]); F->rescale_f = (float)(s ? 1 / s : 1); }
    F->fog_on = gl_is_on(C, GL_FOG);
    F->texgen_any = 0;
    gl_stage_t *S = &C->st;
    memset(S, 0, sizeof *S);
    S->nvar = S_NFFP;
    for (int i = 0; i < S_NFFP; i++) S->interp[i] = MSL_INTERP_PERSP;
    if (C->shade == GL_FLAT) for (int i = 0; i < 4; i++) S->interp[i] = MSL_INTERP_FLAT;
    S->vs = ffp_vs;
    S->fs = ffp_fs;
    S->alpha_test = 1;
    S->two_side = F->lighting && C->lm_two;
    S->point_sprite = gl_is_on(C, GL_POINT_SPRITE);
    S->nclip = GLX_CLIP;
    for (int u = 0; u < 8; u++) {
        ftex_t *T = &g_ftex[u];
        T->on = 0;
        F->tex_on[u] = 0;
        static const GLenum order[5] = {GL_TEXTURE_CUBE_MAP, GL_TEXTURE_3D, GL_TEXTURE_RECTANGLE, GL_TEXTURE_2D, GL_TEXTURE_1D};
        static const int ti[5] = {3, 2, 4, 1, 0};
        for (int k = 0; k < 5; k++) {
            if (!gl_unit_on(C, u, order[k])) continue;
            gl_tex_t *t = C->tex[u][ti[k]];
            if (!t) break;
            msl_tex_t *mt = gl_tex_mt(C, t);
            if (!mt->data) break;   /* incompleta: a unidade fica desligada */
            T->mt = mt;
            gl_tex_sampler(C, t, C->samp[u], &T->s);
            T->nc = k == 0 || k == 1 ? 3 : k == 4 ? 1 : 2;
            T->base = tex_base_of(t);
            T->size[0] = mt->w; T->size[1] = mt->h; T->size[2] = mt->d;
            T->on = 1;
            F->tex_on[u] = 1;
            S->dmask |= 1u << (S_TEX0 + u);
            break;
        }
        for (int c = 0; c < 4; c++) if (gl_unit_on(C, u, GL_TEXTURE_GEN_S + c)) { F->texgen_any = 1; if (!F->tex_on[u]) { /* nada a amostrar */ } }
    }
}

/* ================================================================ raster pos, pixels e bitmap */
static void raster_pos(gl_ctx_t *C, float x, float y, float z, float w) {
    REC(DL_RASTER, x, y, z, w);
    float p[4] = {x, y, z, w}, eye[4], clip[4];
    gl_mat_vec(eye, C->mv.s[C->mv.top].m, p);
    gl_mat_vec(clip, C->proj.s[C->proj.top].m, eye);
    C->raster_valid = !(clip[0] < -clip[3] || clip[0] > clip[3] || clip[1] < -clip[3] || clip[1] > clip[3] || clip[2] < -clip[3] || clip[2] > clip[3]) && clip[3] > 0;
    if (!C->raster_valid) return;
    double iw = 1.0 / clip[3];
    C->raster_pos[0] = (float)((clip[0] * iw + 1) * 0.5 * C->vp[2] + C->vp[0]);
    C->raster_pos[1] = (float)((clip[1] * iw + 1) * 0.5 * C->vp[3] + C->vp[1]);
    C->raster_pos[2] = (float)((clip[2] * iw) * 0.5 * (C->dr[1] - C->dr[0]) + (C->dr[1] + C->dr[0]) * 0.5);
    C->raster_pos[3] = (float)fabs(eye[2]);
    memcpy(C->raster_col, C->cur_col, 16);
    memcpy(C->raster_tc, C->cur_tc[0], 16);
}
#define RP(x, y, z, w) do { GLC(); raster_pos(C, (float)(x), (float)(y), (float)(z), (float)(w)); } while (0)
void glRasterPos2f(GLfloat x, GLfloat y) { RP(x, y, 0, 1); }
void glRasterPos2d(GLdouble x, GLdouble y) { RP(x, y, 0, 1); }
void glRasterPos2i(GLint x, GLint y) { RP(x, y, 0, 1); }
void glRasterPos2s(GLshort x, GLshort y) { RP(x, y, 0, 1); }
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) { RP(x, y, z, 1); }
void glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) { RP(x, y, z, 1); }
void glRasterPos3i(GLint x, GLint y, GLint z) { RP(x, y, z, 1); }
void glRasterPos3s(GLshort x, GLshort y, GLshort z) { RP(x, y, z, 1); }
void glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { RP(x, y, z, w); }
void glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { RP(x, y, z, w); }
void glRasterPos4i(GLint x, GLint y, GLint z, GLint w) { RP(x, y, z, w); }
void glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) { RP(x, y, z, w); }
void glRasterPos2fv(const GLfloat *v) { RP(v[0], v[1], 0, 1); }
void glRasterPos2dv(const GLdouble *v) { RP(v[0], v[1], 0, 1); }
void glRasterPos2iv(const GLint *v) { RP(v[0], v[1], 0, 1); }
void glRasterPos2sv(const GLshort *v) { RP(v[0], v[1], 0, 1); }
void glRasterPos3fv(const GLfloat *v) { RP(v[0], v[1], v[2], 1); }
void glRasterPos3dv(const GLdouble *v) { RP(v[0], v[1], v[2], 1); }
void glRasterPos3iv(const GLint *v) { RP(v[0], v[1], v[2], 1); }
void glRasterPos3sv(const GLshort *v) { RP(v[0], v[1], v[2], 1); }
void glRasterPos4fv(const GLfloat *v) { RP(v[0], v[1], v[2], v[3]); }
void glRasterPos4dv(const GLdouble *v) { RP(v[0], v[1], v[2], v[3]); }
void glRasterPos4iv(const GLint *v) { RP(v[0], v[1], v[2], v[3]); }
void glRasterPos4sv(const GLshort *v) { RP(v[0], v[1], v[2], v[3]); }
static void window_pos(gl_ctx_t *C, float x, float y, float z) {
    C->raster_pos[0] = x; C->raster_pos[1] = y;
    C->raster_pos[2] = (float)(C->dr[0] + (z < 0 ? 0 : z > 1 ? 1 : z) * (C->dr[1] - C->dr[0]));
    C->raster_pos[3] = 0;
    C->raster_valid = 1;
    memcpy(C->raster_col, C->cur_col, 16);
    memcpy(C->raster_tc, C->cur_tc[0], 16);
}
#define WP(x, y, z) do { GLC(); window_pos(C, (float)(x), (float)(y), (float)(z)); } while (0)
void glWindowPos2f(GLfloat x, GLfloat y) { WP(x, y, 0); }
void glWindowPos2d(GLdouble x, GLdouble y) { WP(x, y, 0); }
void glWindowPos2i(GLint x, GLint y) { WP(x, y, 0); }
void glWindowPos2s(GLshort x, GLshort y) { WP(x, y, 0); }
void glWindowPos3f(GLfloat x, GLfloat y, GLfloat z) { WP(x, y, z); }
void glWindowPos3d(GLdouble x, GLdouble y, GLdouble z) { WP(x, y, z); }
void glWindowPos3i(GLint x, GLint y, GLint z) { WP(x, y, z); }
void glWindowPos3s(GLshort x, GLshort y, GLshort z) { WP(x, y, z); }
void glWindowPos2fv(const GLfloat *v) { WP(v[0], v[1], 0); }
void glWindowPos2dv(const GLdouble *v) { WP(v[0], v[1], 0); }
void glWindowPos2iv(const GLint *v) { WP(v[0], v[1], 0); }
void glWindowPos2sv(const GLshort *v) { WP(v[0], v[1], 0); }
void glWindowPos3fv(const GLfloat *v) { WP(v[0], v[1], v[2]); }
void glWindowPos3dv(const GLdouble *v) { WP(v[0], v[1], v[2]); }
void glWindowPos3iv(const GLint *v) { WP(v[0], v[1], v[2]); }
void glWindowPos3sv(const GLshort *v) { WP(v[0], v[1], v[2]); }
void glWindowPos2fARB(GLfloat x, GLfloat y) { WP(x, y, 0); }
void glWindowPos2dARB(GLdouble x, GLdouble y) { WP(x, y, 0); }
void glWindowPos2iARB(GLint x, GLint y) { WP(x, y, 0); }
void glWindowPos2sARB(GLshort x, GLshort y) { WP(x, y, 0); }
void glWindowPos3fARB(GLfloat x, GLfloat y, GLfloat z) { WP(x, y, z); }
void glWindowPos3dARB(GLdouble x, GLdouble y, GLdouble z) { WP(x, y, z); }
void glWindowPos3iARB(GLint x, GLint y, GLint z) { WP(x, y, z); }
void glWindowPos3sARB(GLshort x, GLshort y, GLshort z) { WP(x, y, z); }
void glWindowPos2fvARB(const GLfloat *v) { WP(v[0], v[1], 0); }
void glWindowPos2dvARB(const GLdouble *v) { WP(v[0], v[1], 0); }
void glWindowPos2ivARB(const GLint *v) { WP(v[0], v[1], 0); }
void glWindowPos2svARB(const GLshort *v) { WP(v[0], v[1], 0); }
void glWindowPos3fvARB(const GLfloat *v) { WP(v[0], v[1], v[2]); }
void glWindowPos3dvARB(const GLdouble *v) { WP(v[0], v[1], v[2]); }
void glWindowPos3ivARB(const GLint *v) { WP(v[0], v[1], v[2]); }
void glWindowPos3svARB(const GLshort *v) { WP(v[0], v[1], v[2]); }
void glPixelZoom(GLfloat x, GLfloat y) { GLC(); REC(DL_ZOOM, x, y); C->pzoom[0] = x; C->pzoom[1] = y; }
void glPixelTransferf(GLenum p, GLfloat v) {
    GLC();
    switch (p) {
    case GL_RED_SCALE: C->pmap_scale[0] = v; return;
    case GL_GREEN_SCALE: C->pmap_scale[1] = v; return;
    case GL_BLUE_SCALE: C->pmap_scale[2] = v; return;
    case GL_ALPHA_SCALE: C->pmap_scale[3] = v; return;
    case GL_RED_BIAS: C->pmap_bias[0] = v; return;
    case GL_GREEN_BIAS: C->pmap_bias[1] = v; return;
    case GL_BLUE_BIAS: C->pmap_bias[2] = v; return;
    case GL_ALPHA_BIAS: C->pmap_bias[3] = v; return;
    case GL_MAP_COLOR: case GL_MAP_STENCIL: case GL_INDEX_SHIFT: case GL_INDEX_OFFSET: case GL_DEPTH_SCALE: case GL_DEPTH_BIAS: return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glPixelTransferi(GLenum p, GLint v) { glPixelTransferf(p, (float)v); }
static float g_pmap[10][256];
static int g_pmap_n[10];
void glPixelMapfv(GLenum map, GLsizei n, const GLfloat *v) { int i = (int)(map - GL_PIXEL_MAP_I_TO_I); if (i < 0 || i >= 10 || n < 0 || n > 256) return; memcpy(g_pmap[i], v, (size_t)n * 4); g_pmap_n[i] = n; }
void glPixelMapuiv(GLenum map, GLsizei n, const GLuint *v) { float f[256]; for (int i = 0; i < n && i < 256; i++) f[i] = (float)(v[i] / 4294967295.0); glPixelMapfv(map, n, f); }
void glPixelMapusv(GLenum map, GLsizei n, const GLushort *v) { float f[256]; for (int i = 0; i < n && i < 256; i++) f[i] = v[i] / 65535.0f; glPixelMapfv(map, n, f); }
void glGetPixelMapfv(GLenum map, GLfloat *v) { int i = (int)(map - GL_PIXEL_MAP_I_TO_I); if (i >= 0 && i < 10) memcpy(v, g_pmap[i], (size_t)g_pmap_n[i] * 4); }
void glGetPixelMapuiv(GLenum map, GLuint *v) { int i = (int)(map - GL_PIXEL_MAP_I_TO_I); if (i >= 0 && i < 10) for (int k = 0; k < g_pmap_n[i]; k++) v[k] = (GLuint)(g_pmap[i][k] * 4294967295.0); }
void glGetPixelMapusv(GLenum map, GLushort *v) { int i = (int)(map - GL_PIXEL_MAP_I_TO_I); if (i >= 0 && i < 10) for (int k = 0; k < g_pmap_n[i]; k++) v[k] = (GLushort)(g_pmap[i][k] * 65535); }
static void frag_px(gl_ctx_t *C, gl_target_t *T, int x, int y, const float *col, float z) {
    gl_frag_t f;
    memset(&f, 0, sizeof f);
    f.coord[0] = x + 0.5f; f.coord[1] = y + 0.5f; f.coord[2] = z; f.coord[3] = 1;
    for (int i = 0; i < GLX_DRAW; i++) memcpy(f.color[i], col, 16);
    gl_pixel_write(C, T, x, y, &f);
}
void glDrawPixels(GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *px) {
    GLC();
    if (w < 0 || h < 0) GLERR(GL_INVALID_VALUE, );
    if (C->dl_cur && !C->dl_exec) {
        /* a lista guarda a imagem já convertida em RGBA float */
        float *img = malloc((size_t)w * h * 16 + 1);
        if (!img) GLERR(GL_OUT_OF_MEMORY, );
        gl_unpack_pixels(C, w, h, 1, f, ty, px, 125, (u8 *)img, f == GL_ALPHA ? GL_ALPHA : GL_RGBA);
        u32 a[2] = {(u32)w, (u32)h};
        int r = dl_blob(C, DL_DRAWPIX, a, 8, img, (i64)w * h * 16);
        free(img);
        if (r) return;
    }
    if (!C->raster_valid || !w || !h) return;
    gl_target_t T;
    if (!gl_resolve_target(C, 1, &T)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    const u8 *src = px;
    if (C->b_unpack) { if (!C->b_unpack->data) GLERR(GL_INVALID_OPERATION, ); src = C->b_unpack->data + (uintptr_t)px; }
    int depth = f == GL_DEPTH_COMPONENT, stencil = f == GL_STENCIL_INDEX;
    int dfmt = depth ? 252 : stencil ? 253 : 125;
    float *img = malloc((size_t)w * h * 16 + 1);
    if (!img) GLERR(GL_OUT_OF_MEMORY, );
    gl_unpack_pixels(C, w, h, 1, f, ty, src, dfmt, (u8 *)img, depth ? GL_DEPTH_COMPONENT : stencil ? GL_STENCIL_INDEX : GL_RGBA);
    float zx = C->pzoom[0], zy = C->pzoom[1];
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            float col[4];
            float z = C->raster_pos[2];
            if (depth) { z = ((float *)img)[r * w + c]; memcpy(col, C->raster_col, 16); }
            else if (stencil) memcpy(col, C->raster_col, 16);
            else for (int k = 0; k < 4; k++) col[k] = img[(r * w + c) * 4 + k] * C->pmap_scale[k] + C->pmap_bias[k];
            int xa = (int)floor(C->raster_pos[0] + c * zx), xb = (int)floor(C->raster_pos[0] + (c + 1) * zx);
            int ya = (int)floor(C->raster_pos[1] + r * zy), yb = (int)floor(C->raster_pos[1] + (r + 1) * zy);
            if (xa > xb) { int t = xa; xa = xb; xb = t; }
            if (ya > yb) { int t = ya; ya = yb; yb = t; }
            for (int y = ya; y < yb; y++)
                for (int x = xa; x < xb; x++) {
                    if (stencil) {
                        if (x < 0 || y < 0 || x >= T.w || y >= T.h || !T.stencil) continue;
                        u8 *q = mx_texel(T.stencil, x, y, 0, 0, 0);
                        u8 *s = T.stencil->fmt == 260 ? q + 4 : q;
                        u8 v = ((u8 *)img)[r * w + c];
                        *s = (u8)((*s & ~C->swmask[0]) | (v & C->swmask[0]));
                        continue;
                    }
                    frag_px(C, &T, x, y, col, z);
                }
        }
    free(img);
}
void glBitmap(GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym, const GLubyte *bm) {
    GLC();
    if (w < 0 || h < 0) GLERR(GL_INVALID_VALUE, );
    if (C->dl_cur && !C->dl_exec) {
        i64 rs = ((w + 7) / 8 + C->up_align - 1) / C->up_align * C->up_align;
        float a[6] = {(float)w, (float)h, xo, yo, xm, ym};
        if (dl_blob(C, DL_BITMAP, a, sizeof a, bm, rs * h)) return;
    }
    if (!C->raster_valid) return;
    gl_target_t T;
    if (w && h && bm && gl_resolve_target(C, 1, &T)) {
        const u8 *src = bm;
        if (C->b_unpack) src = C->b_unpack->data ? C->b_unpack->data + (uintptr_t)bm : 0;
        i64 rs = ((w + 7) / 8 + C->up_align - 1) / C->up_align * C->up_align;
        int bx = (int)floor(C->raster_pos[0] - xo), by = (int)floor(C->raster_pos[1] - yo);
        if (src)
            for (int r = 0; r < h; r++)
                for (int c = 0; c < w; c++) {
                    int bit = C->up_lsb ? (src[r * rs + (c >> 3)] >> (c & 7)) & 1 : (src[r * rs + (c >> 3)] >> (7 - (c & 7))) & 1;
                    if (bit) frag_px(C, &T, bx + c, by + r, C->raster_col, C->raster_pos[2]);
                }
    }
    C->raster_pos[0] += xm;
    C->raster_pos[1] += ym;
}
int gl_read_source(gl_ctx_t *C, msl_tex_t **src);
void glCopyPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum type) {
    GLC();
    if (w < 0 || h < 0) GLERR(GL_INVALID_VALUE, );
    if (!C->raster_valid) return;
    gl_target_t S, D;
    if (!gl_resolve_target(C, 0, &S) || !gl_resolve_target(C, 1, &D)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    msl_tex_t *src = type == GL_COLOR ? S.color[0] : type == GL_DEPTH ? S.depth : S.stencil;
    if (!src) GLERR(GL_INVALID_OPERATION, );
    float *tmp = malloc((size_t)w * h * 16 + 1);
    if (!tmp) GLERR(GL_OUT_OF_MEMORY, );
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            double v[4] = {0, 0, 0, 1};
            if (x + c >= 0 && y + r >= 0 && x + c < src->w && y + r < src->h) {
                if (type == GL_STENCIL) { u8 *q = mx_texel(src, x + c, y + r, 0, 0, 0); v[0] = src->fmt == 260 ? q[4] : q[0]; }
                else msl_tex_load(src, x + c, y + r, 0, 0, 0, v);
            }
            for (int k = 0; k < 4; k++) tmp[(r * w + c) * 4 + k] = (float)v[k];
        }
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            int dx = (int)floor(C->raster_pos[0] + c * C->pzoom[0]), dy = (int)floor(C->raster_pos[1] + r * C->pzoom[1]);
            float *v = &tmp[(r * w + c) * 4];
            if (type == GL_COLOR) frag_px(C, &D, dx, dy, v, C->raster_pos[2]);
            else if (type == GL_DEPTH) frag_px(C, &D, dx, dy, C->raster_col, v[0]);
            else if (D.stencil && dx >= 0 && dy >= 0 && dx < D.w && dy < D.h) { u8 *q = mx_texel(D.stencil, dx, dy, 0, 0, 0); u8 *s = D.stencil->fmt == 260 ? q + 4 : q; *s = (u8)((*s & ~C->swmask[0]) | ((u8)v[0] & C->swmask[0])); }
        }
    free(tmp);
}

/* ================================================================ seleção e feedback */
void glSelectBuffer(GLsizei n, GLuint *buf) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); if (C->render_mode == GL_SELECT) GLERR(GL_INVALID_OPERATION, ); C->sel_buf = buf; C->sel_size = n; }
void glFeedbackBuffer(GLsizei n, GLenum type, GLfloat *buf) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); if (C->render_mode == GL_FEEDBACK) GLERR(GL_INVALID_OPERATION, ); C->fb_buf = buf; C->fb_size = n; C->fb_type = type; }
static void sel_flush(gl_ctx_t *C) {
    if (!C->sel_hit) return;
    int need = 3 + C->name_top;
    if (C->sel_n + need > C->sel_size) { C->sel_overflow = 1; C->sel_hit = 0; return; }
    u32 *b = C->sel_buf + C->sel_n;
    b[0] = (u32)C->name_top;
    b[1] = (u32)(mx_cl01(C->sel_min) * 4294967295.0);
    b[2] = (u32)(mx_cl01(C->sel_max) * 4294967295.0);
    for (int i = 0; i < C->name_top; i++) b[3 + i] = C->name_stack[i];
    C->sel_n += need;
    C->sel_hits++;
    C->sel_hit = 0;
}
GLint glRenderMode(GLenum mode) {
    GLC(0);
    if (mode != GL_RENDER && mode != GL_SELECT && mode != GL_FEEDBACK) GLERR(GL_INVALID_ENUM, 0);
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, 0);
    GLint r = 0;
    if (C->render_mode == GL_SELECT) { sel_flush(C); r = C->sel_overflow ? -1 : C->sel_hits; }
    else if (C->render_mode == GL_FEEDBACK) r = C->fb_n > C->fb_size ? -1 : C->fb_n;
    if (mode == GL_SELECT && !C->sel_buf) GLERR(GL_INVALID_OPERATION, 0);
    if (mode == GL_FEEDBACK && !C->fb_buf) GLERR(GL_INVALID_OPERATION, 0);
    C->render_mode = mode;
    C->sel_n = C->sel_hits = C->sel_overflow = 0;
    C->sel_hit = 0;
    C->name_top = 0;
    C->fb_n = 0;
    return r;
}
void glInitNames(void) { GLC(); if (C->render_mode == GL_SELECT) sel_flush(C); C->name_top = 0; }
void glLoadName(GLuint n) { GLC(); RECU(DL_LOADNAME, n); if (C->render_mode != GL_SELECT) return; if (!C->name_top) GLERR(GL_INVALID_OPERATION, ); sel_flush(C); C->name_stack[C->name_top - 1] = n; }
void glPushName(GLuint n) { GLC(); RECU(DL_PUSHNAME, n); if (C->render_mode != GL_SELECT) return; if (C->name_top >= 64) GLERR(GL_STACK_OVERFLOW, ); sel_flush(C); C->name_stack[C->name_top++] = n; }
void glPopName(void) { GLC(); RECU(DL_POPNAME, 0); if (C->render_mode != GL_SELECT) return; if (!C->name_top) GLERR(GL_STACK_UNDERFLOW, ); sel_flush(C); C->name_top--; }
void glPassThrough(GLfloat t) { GLC(); if (C->render_mode == GL_FEEDBACK) { if (C->fb_buf && C->fb_n + 2 <= C->fb_size) { C->fb_buf[C->fb_n] = GL_PASS_THROUGH_TOKEN; C->fb_buf[C->fb_n + 1] = t; } C->fb_n += 2; } }

/* ================================================================ glPushAttrib / glPopAttrib */
typedef struct { GLbitfield mask; gl_ctx_t snap; } attr_snap_t;
void glPushAttrib(GLbitfield mask) {
    GLC();
    RECU(DL_PUSHA, mask);
    if (C->attr_top >= 16) GLERR(GL_STACK_OVERFLOW, );
    attr_snap_t *s = malloc(sizeof *s);
    if (!s) GLERR(GL_OUT_OF_MEMORY, );
    s->mask = mask;
    memcpy(&s->snap, C, sizeof *C);
    C->attr_stack[C->attr_top++] = s;
}
#define RESTORE(f) memcpy(&C->f, &o->f, sizeof C->f)
void glPopAttrib(void) {
    GLC();
    RECU(DL_POPA, 0);
    if (!C->attr_top) GLERR(GL_STACK_UNDERFLOW, );
    attr_snap_t *s = C->attr_stack[--C->attr_top];
    gl_ctx_t *o = &s->snap;
    GLbitfield m = s->mask;
    if (m & GL_CURRENT_BIT) { RESTORE(cur_col); RESTORE(cur_col2); RESTORE(cur_nrm); RESTORE(cur_tc); RESTORE(cur_fog); RESTORE(raster_pos); RESTORE(raster_valid); RESTORE(raster_col); RESTORE(raster_tc); RESTORE(cur_edge); }
    if (m & GL_ENABLE_BIT) { RESTORE(en); RESTORE(blend_on); for (int i = 0; i < GLX_LIGHTS; i++) C->light[i].on = o->light[i].on; }
    if (m & GL_COLOR_BUFFER_BIT) {
        RESTORE(clear_c); RESTORE(bsrc_rgb); RESTORE(bdst_rgb); RESTORE(bsrc_a); RESTORE(bdst_a); RESTORE(beq_rgb); RESTORE(beq_a); RESTORE(bcolor);
        RESTORE(cmask); RESTORE(logic_op); RESTORE(alpha_func); RESTORE(alpha_ref); RESTORE(blend_on); RESTORE(def_draw_buf);
        int b;
        for (GLenum cap = GL_BLEND; cap; cap = 0) if (gl_cap_index(cap, &b)) { (void)b; }
    }
    if (m & GL_DEPTH_BUFFER_BIT) { RESTORE(dfunc); RESTORE(dmask); RESTORE(clear_d); }
    if (m & GL_STENCIL_BUFFER_BIT) { RESTORE(sfunc); RESTORE(sfail); RESTORE(szfail); RESTORE(szpass); RESTORE(sref); RESTORE(svmask); RESTORE(swmask); RESTORE(clear_s); }
    if (m & GL_VIEWPORT_BIT) { RESTORE(vp); RESTORE(dr); }
    if (m & GL_SCISSOR_BIT) RESTORE(sc);
    if (m & GL_POLYGON_BIT) { RESTORE(cull_mode); RESTORE(front_face); RESTORE(pmode); RESTORE(poff_factor); RESTORE(poff_units); }
    if (m & GL_LINE_BIT) { RESTORE(line_w); RESTORE(ls_factor); RESTORE(ls_pattern); }
    if (m & GL_POLYGON_STIPPLE_BIT) RESTORE(pstipple);
    if (m & GL_POINT_BIT) { RESTORE(point_s); RESTORE(point_min); RESTORE(point_max); RESTORE(point_fade); RESTORE(point_att); RESTORE(point_origin); }
    if (m & GL_LIGHTING_BIT) { RESTORE(light); RESTORE(lm_amb); RESTORE(lm_local); RESTORE(lm_two); RESTORE(lm_sep_spec); RESTORE(mat); RESTORE(cm_face); RESTORE(cm_mode); RESTORE(shade); }
    if (m & GL_FOG_BIT) { RESTORE(fog_mode); RESTORE(fog_src); RESTORE(fog_d); RESTORE(fog_s); RESTORE(fog_e); RESTORE(fog_c); RESTORE(fog_index); }
    if (m & GL_TEXTURE_BIT) { RESTORE(te); RESTORE(unit); RESTORE(tex); }
    if (m & GL_TRANSFORM_BIT) { RESTORE(mmode); RESTORE(clip_plane); }
    if (m & GL_HINT_BIT) RESTORE(hint);
    if (m & GL_PIXEL_MODE_BIT) { RESTORE(pzoom); RESTORE(pmap_scale); RESTORE(pmap_bias); RESTORE(def_read_buf); }
    if (m & GL_LIST_BIT) RESTORE(list_base);
    if (m & GL_MULTISAMPLE_BIT) { RESTORE(samp_cov); RESTORE(samp_inv); }
    /* os bits de enable que pertencem a cada grupo também voltam */
    if (m & (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_POLYGON_BIT | GL_LIGHTING_BIT | GL_FOG_BIT | GL_TEXTURE_BIT | GL_SCISSOR_BIT | GL_TRANSFORM_BIT | GL_LINE_BIT | GL_POINT_BIT | GL_MULTISAMPLE_BIT | GL_POLYGON_STIPPLE_BIT))
        RESTORE(en);
    free(s);
}
void glPushClientAttrib(GLbitfield mask) {
    GLC();
    if (C->cattr_top >= 16) GLERR(GL_STACK_OVERFLOW, );
    struct { gl_ctx_t c; gl_vao_t v; } *s = malloc(sizeof *s);
    if (!s) GLERR(GL_OUT_OF_MEMORY, );
    memcpy(&s->c, C, sizeof *C);
    memcpy(&s->v, C->vao, sizeof *C->vao);
    s->c.attr_mask[0] = mask;
    C->cattr_stack[C->cattr_top++] = s;
}
void glPopClientAttrib(void) {
    GLC();
    if (!C->cattr_top) GLERR(GL_STACK_UNDERFLOW, );
    struct { gl_ctx_t c; gl_vao_t v; } *snap = C->cattr_stack[--C->cattr_top];
    gl_ctx_t *o = &snap->c;
    gl_vao_t *ov = &snap->v;
    GLbitfield m = o->attr_mask[0];
    if (m & GL_CLIENT_PIXEL_STORE_BIT) {
        RESTORE(pk_align); RESTORE(pk_row); RESTORE(pk_skrow); RESTORE(pk_skpix); RESTORE(pk_imgh); RESTORE(pk_skimg); RESTORE(pk_swap); RESTORE(pk_lsb);
        RESTORE(up_align); RESTORE(up_row); RESTORE(up_skrow); RESTORE(up_skpix); RESTORE(up_imgh); RESTORE(up_skimg); RESTORE(up_swap); RESTORE(up_lsb);
    }
    if (m & GL_CLIENT_VERTEX_ARRAY_BIT) {
        /* só os campos de estado: as referências de buffer ficam como estão para não desbalancear a contagem */
        gl_attr_t *dst[] = {&C->vao->ca_vertex, &C->vao->ca_normal, &C->vao->ca_color, &C->vao->ca_color2, &C->vao->ca_fog, &C->vao->ca_index, &C->vao->ca_edge};
        const gl_attr_t *src[] = {&ov->ca_vertex, &ov->ca_normal, &ov->ca_color, &ov->ca_color2, &ov->ca_fog, &ov->ca_index, &ov->ca_edge};
        for (int i = 0; i < 7; i++) { gl_buf_t *keep = dst[i]->buf; *dst[i] = *src[i]; dst[i]->buf = keep; }
        for (int u = 0; u < 8; u++) { gl_buf_t *keep = C->vao->ca_tc[u].buf; C->vao->ca_tc[u] = ov->ca_tc[u]; C->vao->ca_tc[u].buf = keep; }
        RESTORE(client_unit);
        C->b_array = o->b_array;
    }
    free(snap);
}

/* ================================================================ execução das listas */
static void dl_exec(gl_ctx_t *C, gl_dlist_t *L, int depth) {
    if (depth > 64) return;
    int saved = C->dl_exec;
    C->dl_exec = 1;
    for (gl_dlcmd_t *c = L->head; c; c = c->next) {
        float *f = c->a.f;
        u32 *u = c->a.u;
        switch (c->op) {
        case DL_ENABLE: glEnable(u[0]); break;
        case DL_DISABLE: glDisable(u[0]); break;
        case DL_VIEWPORT: glViewport(c->a.i[0], c->a.i[1], c->a.i[2], c->a.i[3]); break;
        case DL_BINDTEX: glBindTexture(u[0], u[1]); break;
        case DL_CLEAR: glClear(u[0]); break;
        case DL_BEGIN: glBegin(u[0]); break;
        case DL_END: glEnd(); break;
        case DL_VERTEX: vtx4(C, f[0], f[1], f[2], f[3]); break;
        case DL_COLOR: color4(C, f[0], f[1], f[2], f[3]); break;
        case DL_NORMAL: normal3(C, f[0], f[1], f[2]); break;
        case DL_TEXCOORD: texc(C, (int)f[0], f[1], f[2], f[3], f[4]); break;
        case DL_COLOR2: color2(C, f[0], f[1], f[2]); break;
        case DL_FOG: fogc(C, f[0]); break;
        case DL_EDGE: C->cur_edge = u[0] != 0; break;
        case DL_MMODE: glMatrixMode(u[0]); break;
        case DL_LOADM: loadm(C, f); break;
        case DL_MULTM: multm(C, f); break;
        case DL_PUSHM: glPushMatrix(); break;
        case DL_POPM: glPopMatrix(); break;
        case DL_IDENT: glLoadIdentity(); break;
        case DL_MATERIAL: material(C, (GLenum)f[0], (GLenum)f[1], f + 2); break;
        case DL_LIGHT: lightv(C, (GLenum)f[0], (GLenum)f[1], f + 2); break;
        case DL_LMODEL: lmodel(C, (GLenum)f[0], f + 1); break;
        case DL_SHADE: glShadeModel(u[0]); break;
        case DL_ALPHAF: glAlphaFunc((GLenum)f[0], f[1]); break;
        case DL_COLORMAT: glColorMaterial(u[0], u[1]); break;
        case DL_TEXENV: texenv(C, (GLenum)f[0], (GLenum)f[1], f + 2); break;
        case DL_FOGP: fogv(C, (GLenum)f[0], f + 1); break;
        case DL_TEXGEN: { double d[4] = {f[2], f[3], f[4], f[5]}; texgen(C, (GLenum)f[0], (GLenum)f[1], d); break; }
        case DL_CLIPP: { double d[4] = {f[1], f[2], f[3], f[4]}; glClipPlane((GLenum)f[0], d); break; }
        case DL_CALL: { gl_dlist_t *S = gl_names_get(&C->sh->lists, u[0]); if (S) dl_exec(C, S, depth + 1); break; }
        case DL_RASTER: raster_pos(C, f[0], f[1], f[2], f[3]); break;
        case DL_BITMAP: glBitmap((GLsizei)f[0], (GLsizei)f[1], f[2], f[3], f[4], f[5], c->blob); break;
        case DL_DRAWPIX: {
            /* imagem já em RGBA float, sem o estado de unpack atual */
            int sa = C->up_align, sr = C->up_row, ssr = C->up_skrow, ssp = C->up_skpix;
            gl_buf_t *pb = C->b_unpack;
            C->up_align = 4; C->up_row = 0; C->up_skrow = 0; C->up_skpix = 0; C->b_unpack = 0;
            glDrawPixels((GLsizei)u[0], (GLsizei)u[1], GL_RGBA, GL_FLOAT, c->blob);
            C->up_align = sa; C->up_row = sr; C->up_skrow = ssr; C->up_skpix = ssp; C->b_unpack = pb;
            break;
        }
        case DL_LSTIPPLE: glLineStipple((GLint)u[0], (GLushort)u[1]); break;
        case DL_PSTIPPLE: if (c->blob) memcpy(C->pstipple, c->blob, sizeof C->pstipple); break;
        case DL_PUSHA: glPushAttrib(u[0]); break;
        case DL_POPA: glPopAttrib(); break;
        case DL_LOADNAME: glLoadName(u[0]); break;
        case DL_PUSHNAME: glPushName(u[0]); break;
        case DL_POPNAME: glPopName(); break;
        case DL_LISTBASE: glListBase(u[0]); break;
        case DL_ZOOM: glPixelZoom(f[0], f[1]); break;
        }
    }
    C->dl_exec = saved;
}

/* ================================================================ atributos empacotados do compat (GL 3.3) */
static void unp(GLenum t, GLuint v, int n, int norm, float *o) {
    for (int k = 0; k < 4; k++) {
        int bits = k == 3 ? 2 : 10;
        u32 raw = (v >> (k * 10)) & ((1u << bits) - 1);
        float f;
        if (t == GL_INT_2_10_10_10_REV) { i32 s = (i32)(raw << (32 - bits)) >> (32 - bits); f = norm ? (float)s / (float)((1 << (bits - 1)) - 1) : (float)s; if (norm && f < -1) f = -1; }
        else f = norm ? (float)raw / (float)((1u << bits) - 1) : (float)raw;
        o[k] = k < n ? f : (k == 3 ? 1 : 0);
    }
}
#define PK(name, n, norm, call) \
    void gl##name##ui(GLenum t, GLuint v) { float o[4]; unp(t, v, n, norm, o); call; } \
    void gl##name##uiv(GLenum t, const GLuint *v) { gl##name##ui(t, v[0]); }
PK(VertexP2, 2, 0, glVertex2f(o[0], o[1]))
PK(VertexP3, 3, 0, glVertex3f(o[0], o[1], o[2]))
PK(VertexP4, 4, 0, glVertex4f(o[0], o[1], o[2], o[3]))
PK(NormalP3, 3, 1, glNormal3f(o[0], o[1], o[2]))
PK(ColorP3, 3, 1, glColor3f(o[0], o[1], o[2]))
PK(ColorP4, 4, 1, glColor4f(o[0], o[1], o[2], o[3]))
PK(SecondaryColorP3, 3, 1, glSecondaryColor3f(o[0], o[1], o[2]))
PK(TexCoordP1, 1, 0, glTexCoord1f(o[0]))
PK(TexCoordP2, 2, 0, glTexCoord2f(o[0], o[1]))
PK(TexCoordP3, 3, 0, glTexCoord3f(o[0], o[1], o[2]))
PK(TexCoordP4, 4, 0, glTexCoord4f(o[0], o[1], o[2], o[3]))
void glMultiTexCoordP1ui(GLenum u, GLenum t, GLuint v) { float o[4]; unp(t, v, 1, 0, o); glMultiTexCoord1f(u, o[0]); }
void glMultiTexCoordP2ui(GLenum u, GLenum t, GLuint v) { float o[4]; unp(t, v, 2, 0, o); glMultiTexCoord2f(u, o[0], o[1]); }
void glMultiTexCoordP3ui(GLenum u, GLenum t, GLuint v) { float o[4]; unp(t, v, 3, 0, o); glMultiTexCoord3f(u, o[0], o[1], o[2]); }
void glMultiTexCoordP4ui(GLenum u, GLenum t, GLuint v) { float o[4]; unp(t, v, 4, 0, o); glMultiTexCoord4f(u, o[0], o[1], o[2], o[3]); }
void glMultiTexCoordP1uiv(GLenum u, GLenum t, const GLuint *v) { glMultiTexCoordP1ui(u, t, v[0]); }
void glMultiTexCoordP2uiv(GLenum u, GLenum t, const GLuint *v) { glMultiTexCoordP2ui(u, t, v[0]); }
void glMultiTexCoordP3uiv(GLenum u, GLenum t, const GLuint *v) { glMultiTexCoordP3ui(u, t, v[0]); }
void glMultiTexCoordP4uiv(GLenum u, GLenum t, const GLuint *v) { glMultiTexCoordP4ui(u, t, v[0]); }

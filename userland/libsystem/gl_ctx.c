/*
 * mOS libSystem — OpenGL por software: contexto, grupo compartilhado, nomes, erros, estado fixo
 * (enable/disable, viewport, blend, depth, stencil, máscaras...), glGet* e glGetString.
 * O renderizador se apresenta como "mOS Software Renderer": 2.1 compat ou 4.1 core, como o
 * renderizador genérico do macOS.
 */
#include "gl_int.h"
#include "gl_proto.h"

int pthread_key_create(unsigned long *k, void (*dtor)(void *));
void *pthread_getspecific(unsigned long k);
int pthread_setspecific(unsigned long k, const void *v);

/* ================================================================ nomes */
static int names_grow(gl_names_t *t, u32 need) {
    if (need < t->cap) return 1;
    u32 nc = t->cap ? t->cap * 2 : 64;
    while (nc <= need) nc *= 2;
    void **v = realloc(t->v, nc * sizeof *v);
    u8 *u = realloc(t->used, nc);
    if (!v || !u) { if (v) t->v = v; if (u) t->used = u; return 0; }
    memset(v + t->cap, 0, (nc - t->cap) * sizeof *v);
    memset(u + t->cap, 0, nc - t->cap);
    t->v = v;
    t->used = u;
    t->cap = nc;
    return 1;
}
u32 gl_names_gen(gl_names_t *t) {
    for (u32 i = 1; i < t->cap; i++) if (!t->used[i]) { t->used[i] = 1; if (i >= t->n) t->n = i + 1; return i; }
    u32 i = t->cap ? t->cap : 1;
    if (!names_grow(t, i)) return 0;
    t->used[i] = 1;
    t->n = i + 1;
    return i;
}
void gl_names_reserve(gl_names_t *t, u32 name) {
    if (!name || !names_grow(t, name)) return;
    t->used[name] = 1;
    if (name >= t->n) t->n = name + 1;
}
void *gl_names_get(gl_names_t *t, u32 name) { return name && name < t->cap ? t->v[name] : 0; }
void gl_names_set(gl_names_t *t, u32 name, void *obj) { if (name && names_grow(t, name)) { t->v[name] = obj; t->used[name] = 1; if (name >= t->n) t->n = name + 1; } }
int gl_names_is(gl_names_t *t, u32 name) { return name && name < t->cap && t->used[name]; }
void gl_names_del(gl_names_t *t, u32 name) { if (name && name < t->cap) { t->v[name] = 0; t->used[name] = 0; } }

/* ================================================================ contexto atual */
static unsigned long g_key;
static int g_key_ok;
static void key_init(void) {
    if (__atomic_load_n(&g_key_ok, __ATOMIC_ACQUIRE)) return;
    unsigned long k = 0;
    if (pthread_key_create(&k, 0) == 0) { g_key = k; __atomic_store_n(&g_key_ok, 1, __ATOMIC_RELEASE); }
}
gl_ctx_t *gl_cur(void) { key_init(); return g_key_ok ? pthread_getspecific(g_key) : 0; }
void gl_ctx_make_current(gl_ctx_t *C) { key_init(); if (g_key_ok) pthread_setspecific(g_key, C); }
void gl_err(gl_ctx_t *C, GLenum e) { if (C && !C->err) C->err = e; }
GLenum glGetError(void) { GLC(GL_INVALID_OPERATION); GLenum e = C->err; C->err = GL_NO_ERROR; return e; }

/* ================================================================ capacidades */
static const GLenum caps[] = {
    GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_DITHER, GL_POLYGON_OFFSET_FILL, GL_POLYGON_OFFSET_LINE, GL_POLYGON_OFFSET_POINT,
    GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_ALPHA_TO_ONE, GL_SAMPLE_COVERAGE, GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_COLOR_LOGIC_OP,
    GL_MULTISAMPLE, GL_LINE_SMOOTH, GL_POLYGON_SMOOTH, GL_PROGRAM_POINT_SIZE, GL_PRIMITIVE_RESTART, GL_DEPTH_CLAMP,
    GL_TEXTURE_CUBE_MAP_SEAMLESS, GL_FRAMEBUFFER_SRGB, GL_RASTERIZER_DISCARD, GL_SAMPLE_SHADING, GL_SAMPLE_MASK,
    GL_CLIP_DISTANCE0, GL_CLIP_DISTANCE1, GL_CLIP_DISTANCE2, GL_CLIP_DISTANCE3, GL_CLIP_DISTANCE4, GL_CLIP_DISTANCE5,
    GL_CLIP_DISTANCE6, GL_CLIP_DISTANCE7,
    /* compat */
    GL_ALPHA_TEST, GL_LIGHTING, GL_LIGHT0, GL_LIGHT1, GL_LIGHT2, GL_LIGHT3, GL_LIGHT4, GL_LIGHT5, GL_LIGHT6, GL_LIGHT7,
    GL_COLOR_MATERIAL, GL_NORMALIZE, GL_RESCALE_NORMAL, GL_FOG, GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D,
    GL_TEXTURE_CUBE_MAP, GL_TEXTURE_RECTANGLE, GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T, GL_TEXTURE_GEN_R, GL_TEXTURE_GEN_Q,
    GL_POINT_SMOOTH, GL_POINT_SPRITE, GL_LINE_STIPPLE, GL_POLYGON_STIPPLE, GL_COLOR_SUM, GL_VERTEX_PROGRAM_TWO_SIDE,
    GL_INDEX_LOGIC_OP, GL_AUTO_NORMAL, GL_MAP1_VERTEX_3, GL_MAP1_VERTEX_4, GL_MAP2_VERTEX_3, GL_MAP2_VERTEX_4,
    GL_MAP1_COLOR_4, GL_MAP2_COLOR_4, GL_MAP1_NORMAL, GL_MAP2_NORMAL, GL_MAP1_TEXTURE_COORD_1, GL_MAP1_TEXTURE_COORD_2,
    GL_MAP1_TEXTURE_COORD_3, GL_MAP1_TEXTURE_COORD_4, GL_MAP2_TEXTURE_COORD_1, GL_MAP2_TEXTURE_COORD_2,
    GL_MAP2_TEXTURE_COORD_3, GL_MAP2_TEXTURE_COORD_4, GL_MAP1_INDEX, GL_MAP2_INDEX, GL_VERTEX_PROGRAM_POINT_SIZE,
    GL_CLIP_PLANE0, GL_CLIP_PLANE1, GL_CLIP_PLANE2, GL_CLIP_PLANE3, GL_CLIP_PLANE4, GL_CLIP_PLANE5,
    GL_STENCIL_TEST_TWO_SIDE_EXT, GL_DEBUG_OUTPUT, GL_DEBUG_OUTPUT_SYNCHRONOUS,
    0};
/* capacidades por unidade de textura (compat): bit = base + unidade */
static int tex_cap(GLenum cap) {
    switch (cap) {
    case GL_TEXTURE_1D: return 0; case GL_TEXTURE_2D: return 1; case GL_TEXTURE_3D: return 2; case GL_TEXTURE_CUBE_MAP: return 3;
    case GL_TEXTURE_RECTANGLE: return 4; case GL_TEXTURE_GEN_S: return 5; case GL_TEXTURE_GEN_T: return 6;
    case GL_TEXTURE_GEN_R: return 7; case GL_TEXTURE_GEN_Q: return 8;
    }
    return -1;
}
int gl_cap_index(GLenum cap, int *idx) {
    for (int i = 0; caps[i]; i++) if (caps[i] == cap) { *idx = i; return 1; }
    return 0;
}
/* bits 0..127: caps globais; 128 + 9 * unidade + tex_cap: por unidade (8 unidades de fixed function) */
static int cap_bit(gl_ctx_t *C, GLenum cap) {
    int t = tex_cap(cap);
    if (t >= 0) return 128 + 9 * (C->unit < 8 ? C->unit : 7) + t;
    int i;
    return gl_cap_index(cap, &i) ? i : -1;
}
static void set_bit(gl_ctx_t *C, int b, int on) { if (b < 0 || b >= 256) return; if (on) C->en[b >> 6] |= 1ull << (b & 63); else C->en[b >> 6] &= ~(1ull << (b & 63)); }
static int get_bit(gl_ctx_t *C, int b) { return b >= 0 && b < 256 && (C->en[b >> 6] >> (b & 63)) & 1; }
int gl_is_on(gl_ctx_t *C, GLenum cap) { return get_bit(C, cap_bit(C, cap)); }
int gl_unit_on(gl_ctx_t *C, int unit, GLenum cap) { int t = tex_cap(cap); return t >= 0 && unit < 8 && get_bit(C, 128 + 9 * unit + t); }

static void cap_set(GLenum cap, int on) {
    GLC();
    if (C->dl_cur && gl_dl_record(C, on ? 1 : 2, &cap, sizeof cap)) return;
    if (cap >= GL_LIGHT0 && cap <= GL_LIGHT7) C->light[cap - GL_LIGHT0].on = on;
    int b = cap_bit(C, cap);
    if (b < 0) GLERR(GL_INVALID_ENUM, );
    if (C->profile_core && (b >= 32 && b < 128 && cap != GL_DEBUG_OUTPUT && cap != GL_DEBUG_OUTPUT_SYNCHRONOUS)) GLERR(GL_INVALID_ENUM, );
    if (C->profile_core && b >= 128) GLERR(GL_INVALID_ENUM, );
    set_bit(C, b, on);
    if (cap == GL_BLEND) for (int i = 0; i < GLX_DRAW; i++) C->blend_on[i] = on;
}
void glEnable(GLenum cap) { cap_set(cap, 1); }
void glDisable(GLenum cap) { cap_set(cap, 0); }
GLboolean glIsEnabled(GLenum cap) {
    GLC(0);
    int b = cap_bit(C, cap);
    if (b < 0) GLERR(GL_INVALID_ENUM, 0);
    return (GLboolean)get_bit(C, b);
}
void glEnablei(GLenum cap, GLuint i) { GLC(); if (cap != GL_BLEND || i >= GLX_DRAW) GLERR(cap != GL_BLEND ? GL_INVALID_ENUM : GL_INVALID_VALUE, ); C->blend_on[i] = 1; }
void glDisablei(GLenum cap, GLuint i) { GLC(); if (cap != GL_BLEND || i >= GLX_DRAW) GLERR(cap != GL_BLEND ? GL_INVALID_ENUM : GL_INVALID_VALUE, ); C->blend_on[i] = 0; }
GLboolean glIsEnabledi(GLenum cap, GLuint i) { GLC(0); if (cap != GL_BLEND || i >= GLX_DRAW) GLERR(GL_INVALID_ENUM, 0); return (GLboolean)C->blend_on[i]; }
void glEnableIndexedEXT(GLenum cap, GLuint i) { glEnablei(cap, i); }
void glDisableIndexedEXT(GLenum cap, GLuint i) { glDisablei(cap, i); }
GLboolean glIsEnabledIndexedEXT(GLenum cap, GLuint i) { return glIsEnabledi(cap, i); }

/* ================================================================ criação */
static void tex_defaults(gl_ctx_t *C) {
    static const GLenum tg[11] = {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_RECTANGLE,
                                  GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BUFFER, GL_TEXTURE_2D_MULTISAMPLE,
                                  GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_CUBE_MAP_ARRAY};
    for (int i = 0; i < 11; i++) {
        C->tex_default[i] = gl_tex_new(0, tg[i]);
        for (int u = 0; u < GLX_UNITS; u++) C->tex[u][i] = C->tex_default[i];
    }
}
static void fb_alloc(msl_tex_t *t, int fmt, int w, int h) {
    int bpp = mx_bpp(fmt);
    free(t->data);
    memset(t, 0, sizeof *t);
    t->fmt = fmt;
    t->type = MSL_TEX_2D;
    t->w = w > 0 ? w : 1;
    t->h = h > 0 ? h : 1;
    t->d = 1;
    t->levels = 1;
    t->slices = 1;
    t->lbpr[0] = (u64)t->w * bpp;
    t->lslice[0] = t->lbpr[0] * t->h;
    t->data = calloc(1, t->lslice[0]);
}
void gl_fb_resize(gl_ctx_t *C, int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w == C->fb_w && h == C->fb_h && C->fb_back.data) return;
    int first = !C->fb_back.data;
    C->fb_w = w;
    C->fb_h = h;
    fb_alloc(&C->fb_back, 70, w, h);
    if (C->fb_double) fb_alloc(&C->fb_front, 70, w, h);
    if (C->fb_has_depth || C->fb_has_stencil) fb_alloc(&C->fb_depth, 260, w, h);
    if (first) {
        /* viewport e scissor seguem o primeiro tamanho do drawable, como no macOS */
        C->vp[0] = C->vp[1] = 0; C->vp[2] = w; C->vp[3] = h;
        C->sc[0] = C->sc[1] = 0; C->sc[2] = w; C->sc[3] = h;
    }
}
gl_ctx_t *gl_ctx_create(gl_ctx_t *share, int core, int major, int minor, int w, int h, int depth, int stencil, int dbl, int alpha) {
    gl_ctx_t *C = calloc(1, sizeof *C);
    if (!C) return 0;
    C->profile_core = core;
    C->major = major;
    C->minor = minor;
    if (share) { C->sh = share->sh; C->sh->refs++; }
    else { C->sh = calloc(1, sizeof *C->sh); C->sh->refs = 1; }
    C->fb_double = dbl;
    C->fb_has_depth = depth;
    C->fb_has_stencil = stencil;
    C->fb_alpha = alpha;
    gl_fb_resize(C, w, h);
    C->vao = &C->vao0;
    C->xfb = &C->xfb0;
    tex_defaults(C);
    C->def_draw_buf[0] = dbl ? GL_BACK : GL_FRONT;
    C->def_read_buf = dbl ? GL_BACK : GL_FRONT;
    C->dr[0] = 0; C->dr[1] = 1;
    C->clear_d = 1;
    for (int i = 0; i < GLX_DRAW; i++) {
        C->bsrc_rgb[i] = C->bsrc_a[i] = GL_ONE;
        C->bdst_rgb[i] = C->bdst_a[i] = GL_ZERO;
        C->beq_rgb[i] = C->beq_a[i] = GL_FUNC_ADD;
        C->cmask[i][0] = C->cmask[i][1] = C->cmask[i][2] = C->cmask[i][3] = 1;
    }
    C->dfunc = GL_LESS;
    C->dmask = 1;
    for (int f = 0; f < 2; f++) {
        C->sfunc[f] = GL_ALWAYS;
        C->sfail[f] = C->szfail[f] = C->szpass[f] = GL_KEEP;
        C->svmask[f] = C->swmask[f] = 0xffffffffu;
    }
    C->cull_mode = GL_BACK;
    C->front_face = GL_CCW;
    C->pmode[0] = C->pmode[1] = GL_FILL;
    C->line_w = C->point_s = 1;
    C->point_min = 0; C->point_max = 1024; C->point_fade = 1;
    C->point_att[0] = 1;
    C->point_origin = GL_UPPER_LEFT;
    C->logic_op = GL_COPY;
    C->samp_cov = 1;
    C->provoke = GL_LAST_VERTEX_CONVENTION;
    for (int i = 0; i < 8; i++) C->hint[i] = GL_DONT_CARE;
    C->pk_align = C->up_align = 4;
    C->pzoom[0] = C->pzoom[1] = 1;
    for (int i = 0; i < 4; i++) C->pmap_scale[i] = 1;
    set_bit(C, cap_bit(C, GL_DITHER), 1);
    set_bit(C, cap_bit(C, GL_MULTISAMPLE), 1);
    C->patch_vertices = 3;
    C->render_mode = GL_RENDER;
    gl_ffp_init(C);
    return C;
}
static void share_free(gl_ctx_t *C) {
    gl_share_t *s = C->sh;
    if (--s->refs > 0) return;
    for (u32 i = 1; i < s->texs.n; i++) { gl_tex_t *t = s->texs.v ? s->texs.v[i] : 0; if (t) gl_tex_free(t); }
    for (u32 i = 1; i < s->bufs.n; i++) { gl_buf_t *b = s->bufs.v ? s->bufs.v[i] : 0; if (b) { free(b->data); free(b); } }
    for (u32 i = 1; i < s->rbs.n; i++) { gl_rb_t *r = s->rbs.v ? s->rbs.v[i] : 0; if (r) { free(r->data); free(r); } }
    for (u32 i = 1; i < s->samps.n; i++) free(s->samps.v ? s->samps.v[i] : 0);
    free(s->texs.v); free(s->texs.used); free(s->bufs.v); free(s->bufs.used); free(s->rbs.v); free(s->rbs.used);
    free(s->samps.v); free(s->samps.used); free(s->progs.v); free(s->progs.used); free(s->lists.v); free(s->lists.used);
    free(s);
}
void gl_ctx_destroy(gl_ctx_t *C) {
    if (!C) return;
    if (gl_cur() == C) gl_ctx_make_current(0);
    free(C->fb_back.data); free(C->fb_front.data); free(C->fb_depth.data);
    for (int i = 0; i < 11; i++) gl_tex_free(C->tex_default[i]);
    for (u32 i = 1; i < C->vaos.n; i++) free(C->vaos.v ? C->vaos.v[i] : 0);
    for (u32 i = 1; i < C->fbos.n; i++) free(C->fbos.v ? C->fbos.v[i] : 0);
    for (u32 i = 1; i < C->queries.n; i++) free(C->queries.v ? C->queries.v[i] : 0);
    for (u32 i = 1; i < C->xfbs.n; i++) free(C->xfbs.v ? C->xfbs.v[i] : 0);
    free(C->vaos.v); free(C->vaos.used); free(C->fbos.v); free(C->fbos.used); free(C->queries.v); free(C->queries.used);
    free(C->xfbs.v); free(C->xfbs.used);
    free(C->imv);
    free(C->ext_list);
    share_free(C);
    free(C);
}
void gl_ctx_swap(gl_ctx_t *C) {
    if (!C->fb_double) return;
    /* no macOS o conteúdo do back buffer depois da troca é indefinido; copiar mantém o desenho incremental dos apps antigos */
    memcpy(C->fb_front.data, C->fb_back.data, C->fb_back.lslice[0]);
}
void gl_ctx_size(gl_ctx_t *C, int *w, int *h) { *w = C->fb_w; *h = C->fb_h; }
void gl_ctx_read_bgra(gl_ctx_t *C, u8 *dst, u64 dbpr, int w, int h) {
    msl_tex_t *s = C->fb_double ? &C->fb_front : &C->fb_back;
    if (w > s->w) w = s->w;
    if (h > s->h) h = s->h;
    for (int y = 0; y < h; y++) {
        const u8 *sp = s->data + (u64)(s->h - 1 - y) * s->lbpr[0];
        u8 *dp = dst + (u64)y * dbpr;
        for (int x = 0; x < w; x++) {
            dp[x * 4 + 0] = sp[x * 4 + 2];
            dp[x * 4 + 1] = sp[x * 4 + 1];
            dp[x * 4 + 2] = sp[x * 4 + 0];
            dp[x * 4 + 3] = C->fb_alpha ? sp[x * 4 + 3] : 255;
        }
    }
}

/* ================================================================ estado simples */
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    GLC();
    if (w < 0 || h < 0) GLERR(GL_INVALID_VALUE, );
    if (C->dl_cur) { int a[4] = {x, y, w, h}; if (gl_dl_record(C, 3, a, sizeof a)) return; }
    C->vp[0] = x; C->vp[1] = y; C->vp[2] = w > 16384 ? 16384 : w; C->vp[3] = h > 16384 ? 16384 : h;
}
void glDepthRange(GLdouble n, GLdouble f) { GLC(); C->dr[0] = n < 0 ? 0 : n > 1 ? 1 : n; C->dr[1] = f < 0 ? 0 : f > 1 ? 1 : f; }
void glDepthRangef(GLfloat n, GLfloat f) { glDepthRange(n, f); }
void glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v) { if (first == 0 && count > 0 && v) glDepthRange(v[0], v[1]); }
void glDepthRangeIndexed(GLuint i, GLdouble n, GLdouble f) { if (i == 0) glDepthRange(n, f); }
void glViewportIndexedf(GLuint i, GLfloat x, GLfloat y, GLfloat w, GLfloat h) { if (i == 0) glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h); }
void glViewportIndexedfv(GLuint i, const GLfloat *v) { if (v) glViewportIndexedf(i, v[0], v[1], v[2], v[3]); }
void glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v) { for (int i = 0; i < count; i++) glViewportIndexedfv(first + i, v + 4 * i); }
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h) { GLC(); if (w < 0 || h < 0) GLERR(GL_INVALID_VALUE, ); C->sc[0] = x; C->sc[1] = y; C->sc[2] = w; C->sc[3] = h; }
void glScissorIndexed(GLuint i, GLint x, GLint y, GLsizei w, GLsizei h) { if (i == 0) glScissor(x, y, w, h); }
void glScissorIndexedv(GLuint i, const GLint *v) { if (v) glScissorIndexed(i, v[0], v[1], v[2], v[3]); }
void glScissorArrayv(GLuint first, GLsizei count, const GLint *v) { for (int i = 0; i < count; i++) glScissorIndexedv(first + i, v + 4 * i); }
static float c01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { GLC(); C->clear_c[0] = r; C->clear_c[1] = g; C->clear_c[2] = b; C->clear_c[3] = a; }
void glClearDepth(GLdouble d) { GLC(); C->clear_d = c01((float)d); }
void glClearDepthf(GLfloat d) { glClearDepth(d); }
void glClearStencil(GLint s) { GLC(); C->clear_s = s; }
void glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { GLC(); C->clear_accum[0] = r; C->clear_accum[1] = g; C->clear_accum[2] = b; C->clear_accum[3] = a; }
void glClearIndex(GLfloat c) { (void)c; }
static int blend_factor_ok(GLenum f) {
    switch (f) {
    case GL_ZERO: case GL_ONE: case GL_SRC_COLOR: case GL_ONE_MINUS_SRC_COLOR: case GL_DST_COLOR: case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA: case GL_ONE_MINUS_SRC_ALPHA: case GL_DST_ALPHA: case GL_ONE_MINUS_DST_ALPHA: case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR: case GL_CONSTANT_ALPHA: case GL_ONE_MINUS_CONSTANT_ALPHA: case GL_SRC_ALPHA_SATURATE:
    case GL_SRC1_COLOR: case GL_ONE_MINUS_SRC1_COLOR: case GL_SRC1_ALPHA: case GL_ONE_MINUS_SRC1_ALPHA:
        return 1;
    }
    return 0;
}
static int blend_eq_ok(GLenum e) { return e == GL_FUNC_ADD || e == GL_FUNC_SUBTRACT || e == GL_FUNC_REVERSE_SUBTRACT || e == GL_MIN || e == GL_MAX; }
void glBlendFuncSeparatei(GLuint i, GLenum sr, GLenum dr, GLenum sa, GLenum da) {
    GLC();
    if (i >= GLX_DRAW) GLERR(GL_INVALID_VALUE, );
    if (!blend_factor_ok(sr) || !blend_factor_ok(dr) || !blend_factor_ok(sa) || !blend_factor_ok(da)) GLERR(GL_INVALID_ENUM, );
    C->bsrc_rgb[i] = sr; C->bdst_rgb[i] = dr; C->bsrc_a[i] = sa; C->bdst_a[i] = da;
}
void glBlendFuncSeparate(GLenum sr, GLenum dr, GLenum sa, GLenum da) { for (int i = 0; i < GLX_DRAW; i++) glBlendFuncSeparatei(i, sr, dr, sa, da); }
void glBlendFunc(GLenum s, GLenum d) { glBlendFuncSeparate(s, d, s, d); }
void glBlendFunci(GLuint i, GLenum s, GLenum d) { glBlendFuncSeparatei(i, s, d, s, d); }
void glBlendFuncSeparateEXT(GLenum a, GLenum b, GLenum c, GLenum d) { glBlendFuncSeparate(a, b, c, d); }
void glBlendEquationSeparatei(GLuint i, GLenum rgb, GLenum a) {
    GLC();
    if (i >= GLX_DRAW) GLERR(GL_INVALID_VALUE, );
    if (!blend_eq_ok(rgb) || !blend_eq_ok(a)) GLERR(GL_INVALID_ENUM, );
    C->beq_rgb[i] = rgb; C->beq_a[i] = a;
}
void glBlendEquationSeparate(GLenum rgb, GLenum a) { for (int i = 0; i < GLX_DRAW; i++) glBlendEquationSeparatei(i, rgb, a); }
void glBlendEquation(GLenum e) { glBlendEquationSeparate(e, e); }
void glBlendEquationi(GLuint i, GLenum e) { glBlendEquationSeparatei(i, e, e); }
void glBlendEquationSeparateEXT(GLenum a, GLenum b) { glBlendEquationSeparate(a, b); }
void glBlendEquationEXT(GLenum e) { glBlendEquation(e); }
void glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { GLC(); C->bcolor[0] = r; C->bcolor[1] = g; C->bcolor[2] = b; C->bcolor[3] = a; }
void glBlendColorEXT(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { glBlendColor(r, g, b, a); }
void glColorMaski(GLuint i, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    GLC();
    if (i >= GLX_DRAW) GLERR(GL_INVALID_VALUE, );
    C->cmask[i][0] = r != 0; C->cmask[i][1] = g != 0; C->cmask[i][2] = b != 0; C->cmask[i][3] = a != 0;
}
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { for (int i = 0; i < GLX_DRAW; i++) glColorMaski(i, r, g, b, a); }
void glColorMaskIndexedEXT(GLuint i, GLboolean r, GLboolean g, GLboolean b, GLboolean a) { glColorMaski(i, r, g, b, a); }
static int func_ok(GLenum f) { return f >= GL_NEVER && f <= GL_ALWAYS; }
void glDepthFunc(GLenum f) { GLC(); if (!func_ok(f)) GLERR(GL_INVALID_ENUM, ); C->dfunc = f; }
void glDepthMask(GLboolean m) { GLC(); C->dmask = m != 0; }
void glStencilFuncSeparate(GLenum face, GLenum f, GLint ref, GLuint mask) {
    GLC();
    if (!func_ok(f)) GLERR(GL_INVALID_ENUM, );
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < 2; i++)
        if (face == GL_FRONT_AND_BACK || (i == 0 ? face == GL_FRONT : face == GL_BACK)) { C->sfunc[i] = f; C->sref[i] = ref; C->svmask[i] = mask; }
}
void glStencilFunc(GLenum f, GLint ref, GLuint mask) { glStencilFuncSeparate(GL_FRONT_AND_BACK, f, ref, mask); }
static int sop_ok(GLenum o) { return o == GL_KEEP || o == GL_ZERO || o == GL_REPLACE || o == GL_INCR || o == GL_DECR || o == GL_INVERT || o == GL_INCR_WRAP || o == GL_DECR_WRAP; }
void glStencilOpSeparate(GLenum face, GLenum sf, GLenum zf, GLenum zp) {
    GLC();
    if (!sop_ok(sf) || !sop_ok(zf) || !sop_ok(zp)) GLERR(GL_INVALID_ENUM, );
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < 2; i++)
        if (face == GL_FRONT_AND_BACK || (i == 0 ? face == GL_FRONT : face == GL_BACK)) { C->sfail[i] = sf; C->szfail[i] = zf; C->szpass[i] = zp; }
}
void glStencilOp(GLenum sf, GLenum zf, GLenum zp) { glStencilOpSeparate(GL_FRONT_AND_BACK, sf, zf, zp); }
void glStencilMaskSeparate(GLenum face, GLuint m) {
    GLC();
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) GLERR(GL_INVALID_ENUM, );
    if (face != GL_BACK) C->swmask[0] = m;
    if (face != GL_FRONT) C->swmask[1] = m;
}
void glStencilMask(GLuint m) { glStencilMaskSeparate(GL_FRONT_AND_BACK, m); }
void glStencilFuncSeparateATI(GLenum ff, GLenum bf, GLint ref, GLuint mask) { glStencilFuncSeparate(GL_FRONT, ff, ref, mask); glStencilFuncSeparate(GL_BACK, bf, ref, mask); }
void glStencilOpSeparateATI(GLenum face, GLenum a, GLenum b, GLenum c) { glStencilOpSeparate(face, a, b, c); }
static GLenum g_active_face = GL_FRONT;
void glActiveStencilFaceEXT(GLenum face) { g_active_face = face; }
void glCullFace(GLenum m) { GLC(); if (m != GL_FRONT && m != GL_BACK && m != GL_FRONT_AND_BACK) GLERR(GL_INVALID_ENUM, ); C->cull_mode = m; }
void glFrontFace(GLenum m) { GLC(); if (m != GL_CW && m != GL_CCW) GLERR(GL_INVALID_ENUM, ); C->front_face = m; }
void glPolygonMode(GLenum face, GLenum mode) {
    GLC();
    if (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL) GLERR(GL_INVALID_ENUM, );
    if (face == GL_FRONT_AND_BACK) C->pmode[0] = C->pmode[1] = mode;
    else if (!C->profile_core && face == GL_FRONT) C->pmode[0] = mode;
    else if (!C->profile_core && face == GL_BACK) C->pmode[1] = mode;
    else GLERR(GL_INVALID_ENUM, );
}
void glPolygonOffset(GLfloat f, GLfloat u) { GLC(); C->poff_factor = f; C->poff_units = u; C->poff_clamp = 0; }
void glPolygonOffsetClamp(GLfloat f, GLfloat u, GLfloat c) { GLC(); C->poff_factor = f; C->poff_units = u; C->poff_clamp = c; }
void glLineWidth(GLfloat w) { GLC(); if (w <= 0) GLERR(GL_INVALID_VALUE, ); C->line_w = w; }
void glPointSize(GLfloat s) { GLC(); if (s <= 0) GLERR(GL_INVALID_VALUE, ); C->point_s = s; }
void glPointParameterf(GLenum p, GLfloat v) {
    GLC();
    switch (p) {
    case GL_POINT_SIZE_MIN: C->point_min = v; break;
    case GL_POINT_SIZE_MAX: C->point_max = v; break;
    case GL_POINT_FADE_THRESHOLD_SIZE: C->point_fade = v; break;
    case GL_POINT_SPRITE_COORD_ORIGIN: if (v != GL_UPPER_LEFT && v != GL_LOWER_LEFT) GLERR(GL_INVALID_VALUE, ); C->point_origin = (GLenum)v; break;
    default: GLERR(GL_INVALID_ENUM, );
    }
}
void glPointParameterfv(GLenum p, const GLfloat *v) {
    GLC();
    if (!v) return;
    if (p == GL_POINT_DISTANCE_ATTENUATION) { C->point_att[0] = v[0]; C->point_att[1] = v[1]; C->point_att[2] = v[2]; return; }
    glPointParameterf(p, v[0]);
}
void glPointParameteri(GLenum p, GLint v) { glPointParameterf(p, (float)v); }
void glPointParameteriv(GLenum p, const GLint *v) { if (!v) return; if (p == GL_POINT_DISTANCE_ATTENUATION) { float f[3] = {(float)v[0], (float)v[1], (float)v[2]}; glPointParameterfv(p, f); } else glPointParameterf(p, (float)v[0]); }
void glPointParameterfARB(GLenum p, GLfloat v) { glPointParameterf(p, v); }
void glPointParameterfvARB(GLenum p, const GLfloat *v) { glPointParameterfv(p, v); }
void glLogicOp(GLenum op) { GLC(); if (op < GL_CLEAR || op > GL_SET) GLERR(GL_INVALID_ENUM, ); C->logic_op = op; }
void glSampleCoverage(GLfloat v, GLboolean inv) { GLC(); C->samp_cov = c01(v); C->samp_inv = inv != 0; }
void glSampleMaski(GLuint i, GLbitfield m) { (void)i; (void)m; }
void glMinSampleShading(GLfloat v) { GLC(); C->min_sample_shading = c01(v); }
void glProvokingVertex(GLenum m) { GLC(); if (m != GL_FIRST_VERTEX_CONVENTION && m != GL_LAST_VERTEX_CONVENTION) GLERR(GL_INVALID_ENUM, ); C->provoke = m; }
void glProvokingVertexEXT(GLenum m) { glProvokingVertex(m); }
void glPrimitiveRestartIndex(GLuint i) { GLC(); C->restart_idx = i; }
void glPatchParameteri(GLenum p, GLint v) { GLC(); if (p != GL_PATCH_VERTICES) GLERR(GL_INVALID_ENUM, ); if (v <= 0 || v > 32) GLERR(GL_INVALID_VALUE, ); C->patch_vertices = (u32)v; }
void glPatchParameterfv(GLenum p, const GLfloat *v) { (void)p; (void)v; }
void glHint(GLenum t, GLenum m) {
    GLC();
    if (m != GL_FASTEST && m != GL_NICEST && m != GL_DONT_CARE) GLERR(GL_INVALID_ENUM, );
    static const GLenum ts[] = {GL_LINE_SMOOTH_HINT, GL_POLYGON_SMOOTH_HINT, GL_TEXTURE_COMPRESSION_HINT,
                                GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_PERSPECTIVE_CORRECTION_HINT, GL_POINT_SMOOTH_HINT,
                                GL_FOG_HINT, GL_GENERATE_MIPMAP_HINT};
    for (int i = 0; i < 8; i++) if (ts[i] == t) { C->hint[i] = m; return; }
    GLERR(GL_INVALID_ENUM, );
}
static void pstore(GLenum p, int v) {
    GLC();
    int nonneg = 1;
    switch (p) {
    case GL_PACK_ALIGNMENT: case GL_UNPACK_ALIGNMENT:
        if (v != 1 && v != 2 && v != 4 && v != 8) GLERR(GL_INVALID_VALUE, );
        if (p == GL_PACK_ALIGNMENT) C->pk_align = v; else C->up_align = v;
        return;
    case GL_PACK_ROW_LENGTH: C->pk_row = v; break;
    case GL_PACK_SKIP_ROWS: C->pk_skrow = v; break;
    case GL_PACK_SKIP_PIXELS: C->pk_skpix = v; break;
    case GL_PACK_IMAGE_HEIGHT: C->pk_imgh = v; break;
    case GL_PACK_SKIP_IMAGES: C->pk_skimg = v; break;
    case GL_PACK_SWAP_BYTES: C->pk_swap = v != 0; nonneg = 0; break;
    case GL_PACK_LSB_FIRST: C->pk_lsb = v != 0; nonneg = 0; break;
    case GL_UNPACK_ROW_LENGTH: C->up_row = v; break;
    case GL_UNPACK_SKIP_ROWS: C->up_skrow = v; break;
    case GL_UNPACK_SKIP_PIXELS: C->up_skpix = v; break;
    case GL_UNPACK_IMAGE_HEIGHT: C->up_imgh = v; break;
    case GL_UNPACK_SKIP_IMAGES: C->up_skimg = v; break;
    case GL_UNPACK_SWAP_BYTES: C->up_swap = v != 0; nonneg = 0; break;
    case GL_UNPACK_LSB_FIRST: C->up_lsb = v != 0; nonneg = 0; break;
    case GL_UNPACK_CLIENT_STORAGE_APPLE: nonneg = 0; break;   /* o mOS sempre copia: o resultado é o mesmo */
    case GL_UNPACK_ROW_BYTES_APPLE: case GL_PACK_ROW_BYTES_APPLE: break;
    default: GLERR(GL_INVALID_ENUM, );
    }
    if (nonneg && v < 0) GLERR(GL_INVALID_VALUE, );
}
void glPixelStorei(GLenum p, GLint v) { pstore(p, v); }
void glPixelStoref(GLenum p, GLfloat v) { pstore(p, (int)(v + 0.5f)); }
void glFlush(void) { GLC(); }
void glFinish(void) { GLC(); }
void glFlushRenderAPPLE(void) {}
void glFinishRenderAPPLE(void) {}
void glSwapAPPLE(void) {}

/* ================================================================ strings */
static const char *g_ext_compat[] = {
    "GL_ARB_color_buffer_float", "GL_ARB_depth_buffer_float", "GL_ARB_depth_clamp", "GL_ARB_depth_texture",
    "GL_ARB_draw_buffers", "GL_ARB_draw_elements_base_vertex", "GL_ARB_draw_instanced", "GL_ARB_fragment_program_shadow",
    "GL_ARB_fragment_shader", "GL_ARB_framebuffer_object", "GL_ARB_framebuffer_sRGB", "GL_ARB_half_float_pixel",
    "GL_ARB_half_float_vertex", "GL_ARB_instanced_arrays", "GL_ARB_multisample", "GL_ARB_multitexture",
    "GL_ARB_occlusion_query", "GL_ARB_pixel_buffer_object", "GL_ARB_point_parameters", "GL_ARB_point_sprite",
    "GL_ARB_provoking_vertex", "GL_ARB_seamless_cube_map", "GL_ARB_shader_objects", "GL_ARB_shader_texture_lod",
    "GL_ARB_shading_language_100", "GL_ARB_shadow", "GL_ARB_sync", "GL_ARB_texture_border_clamp",
    "GL_ARB_texture_compression", "GL_ARB_texture_compression_rgtc", "GL_ARB_texture_cube_map", "GL_ARB_texture_env_add",
    "GL_ARB_texture_env_combine", "GL_ARB_texture_env_crossbar", "GL_ARB_texture_env_dot3", "GL_ARB_texture_float",
    "GL_ARB_texture_mirrored_repeat", "GL_ARB_texture_non_power_of_two", "GL_ARB_texture_rectangle", "GL_ARB_texture_rg",
    "GL_ARB_transpose_matrix", "GL_ARB_vertex_array_bgra", "GL_ARB_vertex_blend", "GL_ARB_vertex_buffer_object",
    "GL_ARB_vertex_shader", "GL_ARB_window_pos", "GL_EXT_abgr", "GL_EXT_bgra", "GL_EXT_blend_color",
    "GL_EXT_blend_equation_separate", "GL_EXT_blend_func_separate", "GL_EXT_blend_minmax", "GL_EXT_blend_subtract",
    "GL_EXT_clip_volume_hint", "GL_EXT_debug_label", "GL_EXT_debug_marker", "GL_EXT_draw_buffers2", "GL_EXT_draw_range_elements",
    "GL_EXT_fog_coord", "GL_EXT_framebuffer_blit", "GL_EXT_framebuffer_multisample", "GL_EXT_framebuffer_multisample_blit_scaled",
    "GL_EXT_framebuffer_object", "GL_EXT_framebuffer_sRGB", "GL_EXT_geometry_shader4", "GL_EXT_gpu_program_parameters",
    "GL_EXT_gpu_shader4", "GL_EXT_multi_draw_arrays", "GL_EXT_packed_depth_stencil", "GL_EXT_packed_float",
    "GL_EXT_provoking_vertex", "GL_EXT_rescale_normal", "GL_EXT_secondary_color", "GL_EXT_separate_specular_color",
    "GL_EXT_shadow_funcs", "GL_EXT_stencil_two_side", "GL_EXT_stencil_wrap", "GL_EXT_texture_array",
    "GL_EXT_texture_compression_dxt1", "GL_EXT_texture_compression_s3tc", "GL_EXT_texture_env_add",
    "GL_EXT_texture_filter_anisotropic", "GL_EXT_texture_integer", "GL_EXT_texture_lod_bias", "GL_EXT_texture_mirror_clamp",
    "GL_EXT_texture_rectangle", "GL_EXT_texture_shared_exponent", "GL_EXT_texture_sRGB", "GL_EXT_texture_sRGB_decode",
    "GL_EXT_timer_query", "GL_EXT_transform_feedback", "GL_EXT_vertex_array_bgra", "GL_APPLE_aux_depth_stencil",
    "GL_APPLE_client_storage", "GL_APPLE_element_array", "GL_APPLE_fence", "GL_APPLE_float_pixels",
    "GL_APPLE_flush_buffer_range", "GL_APPLE_flush_render", "GL_APPLE_object_purgeable", "GL_APPLE_packed_pixels",
    "GL_APPLE_pixel_buffer", "GL_APPLE_rgb_422", "GL_APPLE_row_bytes", "GL_APPLE_specular_vector",
    "GL_APPLE_texture_range", "GL_APPLE_transform_hint", "GL_APPLE_vertex_array_object", "GL_APPLE_vertex_array_range",
    "GL_APPLE_vertex_point_size", "GL_APPLE_vertex_program_evaluators", "GL_APPLE_ycbcr_422", "GL_ATI_separate_stencil",
    "GL_ATI_texture_env_combine3", "GL_ATI_texture_float", "GL_ATI_texture_mirror_once", "GL_IBM_rasterpos_clip",
    "GL_NV_blend_square", "GL_NV_conditional_render", "GL_NV_depth_clamp", "GL_NV_fog_distance", "GL_NV_light_max_exponent",
    "GL_NV_texgen_reflection", "GL_NV_texture_barrier", "GL_SGIS_generate_mipmap", "GL_SGIS_texture_edge_clamp",
    "GL_SGIS_texture_lod", 0};
static const char *g_ext_core[] = {
    "GL_ARB_blend_func_extended", "GL_ARB_draw_buffers_blend", "GL_ARB_draw_indirect", "GL_ARB_ES2_compatibility",
    "GL_ARB_explicit_attrib_location", "GL_ARB_gpu_shader_fp64", "GL_ARB_gpu_shader5", "GL_ARB_instanced_arrays",
    "GL_ARB_internalformat_query", "GL_ARB_occlusion_query2", "GL_ARB_sample_shading", "GL_ARB_sampler_objects",
    "GL_ARB_separate_shader_objects", "GL_ARB_shader_bit_encoding", "GL_ARB_shader_subroutine", "GL_ARB_shading_language_include",
    "GL_ARB_tessellation_shader", "GL_ARB_texture_buffer_object_rgb32", "GL_ARB_texture_cube_map_array",
    "GL_ARB_texture_gather", "GL_ARB_texture_query_lod", "GL_ARB_texture_rgb10_a2ui", "GL_ARB_texture_storage",
    "GL_ARB_texture_swizzle", "GL_ARB_timer_query", "GL_ARB_transform_feedback2", "GL_ARB_transform_feedback3",
    "GL_ARB_vertex_attrib_64bit", "GL_ARB_vertex_type_2_10_10_10_rev", "GL_ARB_viewport_array", "GL_EXT_debug_label",
    "GL_EXT_debug_marker", "GL_EXT_depth_bounds_test", "GL_EXT_texture_compression_s3tc", "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_mirror_clamp", "GL_EXT_texture_sRGB_decode", "GL_APPLE_client_storage", "GL_APPLE_container_object_shareable",
    "GL_APPLE_flush_render", "GL_APPLE_object_purgeable", "GL_APPLE_rgb_422", "GL_APPLE_row_bytes", "GL_APPLE_texture_range",
    "GL_ATI_texture_mirror_once", "GL_NV_texture_barrier", 0};
static void ext_build(gl_ctx_t *C) {
    if (C->ext_str[0]) return;
    const char **e = C->profile_core ? g_ext_core : g_ext_compat;
    int n = 0, len = 0;
    while (e[n]) n++;
    C->ext_list = calloc(n + 1, sizeof *C->ext_list);
    C->next = n;
    for (int i = 0; i < n; i++) {
        C->ext_list[i] = (char *)e[i];
        int l = (int)strlen(e[i]);
        if (len + l + 2 >= (int)sizeof C->ext_str) break;
        memcpy(C->ext_str + len, e[i], l);
        len += l;
        C->ext_str[len++] = ' ';
    }
    C->ext_str[len] = 0;
}
const GLubyte *glGetString(GLenum name) {
    GLC(0);
    switch (name) {
    case GL_VENDOR: return (const GLubyte *)"mOS";
    case GL_RENDERER: return (const GLubyte *)"mOS Software Renderer";
    case GL_VERSION: return (const GLubyte *)(C->profile_core ? "4.1 mOS-1.0" : "2.1 mOS-1.0");
    case GL_SHADING_LANGUAGE_VERSION: return (const GLubyte *)(C->profile_core ? "4.10" : "1.20");
    case GL_EXTENSIONS:
        if (C->profile_core) GLERR(GL_INVALID_ENUM, 0);   /* core: só glGetStringi */
        ext_build(C);
        return (const GLubyte *)C->ext_str;
    }
    GLERR(GL_INVALID_ENUM, 0);
}
const GLubyte *glGetStringi(GLenum name, GLuint i) {
    GLC(0);
    if (name != GL_EXTENSIONS) GLERR(GL_INVALID_ENUM, 0);
    ext_build(C);
    if ((int)i >= C->next) GLERR(GL_INVALID_VALUE, 0);
    return (const GLubyte *)C->ext_list[i];
}

/* ================================================================ glGet */
enum { T_I = 0, T_F = 1, T_B = 2, T_D = 3 };
typedef struct { int n, kind; double d[16]; i64 i[16]; } gv_t;
static void gi(gv_t *g, int n, ...) {
    va_list ap;
    va_start(ap, n);
    g->n = n;
    g->kind = T_I;
    for (int i = 0; i < n; i++) { g->i[i] = va_arg(ap, i64); g->d[i] = (double)g->i[i]; }
    va_end(ap);
}
static void gfv(gv_t *g, int n, const float *v) { g->n = n; g->kind = T_F; for (int i = 0; i < n; i++) { g->d[i] = v[i]; g->i[i] = (i64)v[i]; } }
static void gdv(gv_t *g, int n, const double *v) { g->n = n; g->kind = T_F; for (int i = 0; i < n; i++) { g->d[i] = v[i]; g->i[i] = (i64)v[i]; } }
static void gb(gv_t *g, int n, const u8 *v) { g->n = n; g->kind = T_B; for (int i = 0; i < n; i++) { g->i[i] = v[i] != 0; g->d[i] = (double)g->i[i]; } }
#define I1(v) gi(g, 1, (i64)(v))
static u32 tex_name(gl_ctx_t *C, int ti) { gl_tex_t *t = C->tex[C->unit][ti]; return t ? t->name : 0; }
static u32 buf_name(gl_buf_t *b) { return b ? b->name : 0; }
/* matrizes (compat) */
static void gmat(gv_t *g, const mstack_t *s) { gfv(g, 16, s->s[s->top].m); }
static int get_any(gl_ctx_t *C, GLenum p, gv_t *g) {
    if (p >= GL_LIGHT0 && p <= GL_LIGHT7) { gb(g, 1, (u8[]){(u8)C->light[p - GL_LIGHT0].on}); return 1; }
    switch (p) {
    case GL_MAJOR_VERSION: I1(C->profile_core ? 4 : 2); return 1;
    case GL_MINOR_VERSION: I1(C->profile_core ? 1 : 1); return 1;
    case GL_CONTEXT_PROFILE_MASK: I1(C->profile_core ? GL_CONTEXT_CORE_PROFILE_BIT : GL_CONTEXT_COMPATIBILITY_PROFILE_BIT); return 1;
    case GL_CONTEXT_FLAGS: I1(C->profile_core ? GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT : 0); return 1;
    case GL_NUM_EXTENSIONS: ext_build(C); I1(C->next); return 1;
    case GL_VIEWPORT: gi(g, 4, (i64)C->vp[0], (i64)C->vp[1], (i64)C->vp[2], (i64)C->vp[3]); return 1;
    case GL_SCISSOR_BOX: gi(g, 4, (i64)C->sc[0], (i64)C->sc[1], (i64)C->sc[2], (i64)C->sc[3]); return 1;
    case GL_DEPTH_RANGE: gdv(g, 2, C->dr); return 1;
    case GL_COLOR_CLEAR_VALUE: gfv(g, 4, C->clear_c); return 1;
    case GL_DEPTH_CLEAR_VALUE: gdv(g, 1, &C->clear_d); return 1;
    case GL_STENCIL_CLEAR_VALUE: I1(C->clear_s); return 1;
    case GL_ACCUM_CLEAR_VALUE: gfv(g, 4, C->clear_accum); return 1;
    case GL_BLEND_SRC_RGB: case GL_BLEND_SRC: I1(C->bsrc_rgb[0]); return 1;
    case GL_BLEND_DST_RGB: case GL_BLEND_DST: I1(C->bdst_rgb[0]); return 1;
    case GL_BLEND_SRC_ALPHA: I1(C->bsrc_a[0]); return 1;
    case GL_BLEND_DST_ALPHA: I1(C->bdst_a[0]); return 1;
    case GL_BLEND_EQUATION_RGB: I1(C->beq_rgb[0]); return 1;
    case GL_BLEND_EQUATION_ALPHA: I1(C->beq_a[0]); return 1;
    case GL_BLEND_COLOR: gfv(g, 4, C->bcolor); return 1;
    case GL_COLOR_WRITEMASK: gb(g, 4, C->cmask[0]); return 1;
    case GL_DEPTH_FUNC: I1(C->dfunc); return 1;
    case GL_DEPTH_WRITEMASK: gb(g, 1, (u8[]){(u8)C->dmask}); return 1;
    case GL_STENCIL_FUNC: I1(C->sfunc[0]); return 1;
    case GL_STENCIL_REF: I1(C->sref[0]); return 1;
    case GL_STENCIL_VALUE_MASK: I1(C->svmask[0]); return 1;
    case GL_STENCIL_WRITEMASK: I1(C->swmask[0]); return 1;
    case GL_STENCIL_FAIL: I1(C->sfail[0]); return 1;
    case GL_STENCIL_PASS_DEPTH_FAIL: I1(C->szfail[0]); return 1;
    case GL_STENCIL_PASS_DEPTH_PASS: I1(C->szpass[0]); return 1;
    case GL_STENCIL_BACK_FUNC: I1(C->sfunc[1]); return 1;
    case GL_STENCIL_BACK_REF: I1(C->sref[1]); return 1;
    case GL_STENCIL_BACK_VALUE_MASK: I1(C->svmask[1]); return 1;
    case GL_STENCIL_BACK_WRITEMASK: I1(C->swmask[1]); return 1;
    case GL_STENCIL_BACK_FAIL: I1(C->sfail[1]); return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL: I1(C->szfail[1]); return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS: I1(C->szpass[1]); return 1;
    case GL_CULL_FACE_MODE: I1(C->cull_mode); return 1;
    case GL_FRONT_FACE: I1(C->front_face); return 1;
    case GL_POLYGON_MODE: gi(g, 2, (i64)C->pmode[0], (i64)C->pmode[1]); return 1;
    case GL_POLYGON_OFFSET_FACTOR: gfv(g, 1, &C->poff_factor); return 1;
    case GL_POLYGON_OFFSET_UNITS: gfv(g, 1, &C->poff_units); return 1;
    case GL_LINE_WIDTH: gfv(g, 1, &C->line_w); return 1;
    case GL_POINT_SIZE: gfv(g, 1, &C->point_s); return 1;
    case GL_POINT_SIZE_MIN: gfv(g, 1, &C->point_min); return 1;
    case GL_POINT_SIZE_MAX: gfv(g, 1, &C->point_max); return 1;
    case GL_POINT_FADE_THRESHOLD_SIZE: gfv(g, 1, &C->point_fade); return 1;
    case GL_POINT_DISTANCE_ATTENUATION: gfv(g, 3, C->point_att); return 1;
    case GL_POINT_SPRITE_COORD_ORIGIN: I1(C->point_origin); return 1;
    case GL_LOGIC_OP_MODE: I1(C->logic_op); return 1;
    case GL_SAMPLE_COVERAGE_VALUE: gfv(g, 1, &C->samp_cov); return 1;
    case GL_SAMPLE_COVERAGE_INVERT: gb(g, 1, (u8[]){(u8)C->samp_inv}); return 1;
    case GL_PROVOKING_VERTEX: I1(C->provoke); return 1;
    case GL_PRIMITIVE_RESTART_INDEX: I1(C->restart_idx); return 1;
    case GL_PATCH_VERTICES: I1(C->patch_vertices); return 1;
    case GL_MIN_SAMPLE_SHADING_VALUE: gfv(g, 1, &C->min_sample_shading); return 1;
    case GL_PACK_ALIGNMENT: I1(C->pk_align); return 1;
    case GL_UNPACK_ALIGNMENT: I1(C->up_align); return 1;
    case GL_PACK_ROW_LENGTH: I1(C->pk_row); return 1;
    case GL_UNPACK_ROW_LENGTH: I1(C->up_row); return 1;
    case GL_PACK_SKIP_ROWS: I1(C->pk_skrow); return 1;
    case GL_UNPACK_SKIP_ROWS: I1(C->up_skrow); return 1;
    case GL_PACK_SKIP_PIXELS: I1(C->pk_skpix); return 1;
    case GL_UNPACK_SKIP_PIXELS: I1(C->up_skpix); return 1;
    case GL_PACK_IMAGE_HEIGHT: I1(C->pk_imgh); return 1;
    case GL_UNPACK_IMAGE_HEIGHT: I1(C->up_imgh); return 1;
    case GL_PACK_SKIP_IMAGES: I1(C->pk_skimg); return 1;
    case GL_UNPACK_SKIP_IMAGES: I1(C->up_skimg); return 1;
    case GL_PACK_SWAP_BYTES: I1(C->pk_swap); return 1;
    case GL_UNPACK_SWAP_BYTES: I1(C->up_swap); return 1;
    case GL_PACK_LSB_FIRST: I1(C->pk_lsb); return 1;
    case GL_UNPACK_LSB_FIRST: I1(C->up_lsb); return 1;
    /* limites */
    case GL_MAX_TEXTURE_SIZE: case GL_MAX_RECTANGLE_TEXTURE_SIZE: case GL_MAX_RENDERBUFFER_SIZE: I1(16384); return 1;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE: I1(16384); return 1;
    case GL_MAX_3D_TEXTURE_SIZE: I1(2048); return 1;
    case GL_MAX_ARRAY_TEXTURE_LAYERS: I1(2048); return 1;
    case GL_MAX_TEXTURE_BUFFER_SIZE: I1(134217728); return 1;
    case GL_MAX_VIEWPORT_DIMS: gi(g, 2, (i64)16384, (i64)16384); return 1;
    case GL_MAX_VIEWPORTS: I1(16); return 1;
    case GL_VIEWPORT_SUBPIXEL_BITS: I1(0); return 1;
    case GL_VIEWPORT_BOUNDS_RANGE: gi(g, 2, (i64)-32768, (i64)32767); return 1;
    case GL_MAX_TEXTURE_IMAGE_UNITS: case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS: I1(16); return 1;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: I1(GLX_UNITS); return 1;
    case GL_MAX_TEXTURE_UNITS: I1(8); return 1;
    case GL_MAX_TEXTURE_COORDS: I1(8); return 1;
    case GL_MAX_VERTEX_ATTRIBS: I1(GLX_ATTR); return 1;
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS: case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS: case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS: I1(4096); return 1;
    case GL_MAX_VERTEX_UNIFORM_VECTORS: case GL_MAX_FRAGMENT_UNIFORM_VECTORS: I1(1024); return 1;
    case GL_MAX_VARYING_COMPONENTS: case GL_MAX_VERTEX_OUTPUT_COMPONENTS: case GL_MAX_FRAGMENT_INPUT_COMPONENTS: I1(64); return 1;
    case GL_MAX_VARYING_VECTORS: I1(16); return 1;
    case GL_MAX_GEOMETRY_OUTPUT_VERTICES: I1(256); return 1;
    case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS: I1(1024); return 1;
    case GL_MAX_GEOMETRY_INPUT_COMPONENTS: case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS: I1(128); return 1;
    case GL_MAX_DRAW_BUFFERS: case GL_MAX_COLOR_ATTACHMENTS: I1(GLX_DRAW); return 1;
    case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS: I1(1); return 1;
    case GL_MAX_SAMPLES: case GL_MAX_COLOR_TEXTURE_SAMPLES: case GL_MAX_DEPTH_TEXTURE_SAMPLES: case GL_MAX_INTEGER_SAMPLES: I1(8); return 1;
    case GL_MAX_SAMPLE_MASK_WORDS: I1(1); return 1;
    case GL_SAMPLES: I1(0); return 1;
    case GL_SAMPLE_BUFFERS: I1(0); return 1;
    case GL_MAX_ELEMENTS_VERTICES: case GL_MAX_ELEMENTS_INDICES: I1(1048575); return 1;
    case GL_MAX_TEXTURE_LOD_BIAS: { float f = 16; gfv(g, 1, &f); return 1; }
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: { float f = 16; gfv(g, 1, &f); return 1; }
    case GL_MAX_CLIP_DISTANCES: I1(GLX_CLIP); return 1;
    case GL_MAX_UNIFORM_BUFFER_BINDINGS: I1(GLX_UBO); return 1;
    case GL_MAX_UNIFORM_BLOCK_SIZE: I1(65536); return 1;
    case GL_MAX_VERTEX_UNIFORM_BLOCKS: case GL_MAX_FRAGMENT_UNIFORM_BLOCKS: case GL_MAX_GEOMETRY_UNIFORM_BLOCKS: I1(12); return 1;
    case GL_MAX_COMBINED_UNIFORM_BLOCKS: I1(GLX_UBO); return 1;
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: I1(16); return 1;
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS: case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: I1(4096 + 12 * 16384); return 1;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS: case GL_MAX_TRANSFORM_FEEDBACK_BUFFERS: I1(GLX_XFB); return 1;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS: I1(4); return 1;
    case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: I1(64); return 1;
    case GL_MAX_PROGRAM_TEXEL_OFFSET: I1(7); return 1;
    case GL_MIN_PROGRAM_TEXEL_OFFSET: I1(-8); return 1;
    case GL_MAX_SERVER_WAIT_TIMEOUT: I1(0); return 1;
    case GL_MAX_LIGHTS: I1(GLX_LIGHTS); return 1;
    case GL_MAX_MODELVIEW_STACK_DEPTH: I1(32); return 1;
    case GL_MAX_PROJECTION_STACK_DEPTH: case GL_MAX_TEXTURE_STACK_DEPTH: case GL_MAX_COLOR_MATRIX_STACK_DEPTH: I1(4); return 1;
    case GL_MAX_ATTRIB_STACK_DEPTH: case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH: I1(16); return 1;
    case GL_MAX_LIST_NESTING: I1(64); return 1;
    case GL_MAX_NAME_STACK_DEPTH: I1(64); return 1;
    case GL_MAX_EVAL_ORDER: I1(8); return 1;
    case GL_MAX_PIXEL_MAP_TABLE: I1(256); return 1;
    case GL_MAX_TESS_GEN_LEVEL: I1(64); return 1;
    case GL_MAX_PATCH_VERTICES: I1(32); return 1;
    case GL_SUBPIXEL_BITS: I1(8); return 1;
    case GL_ALIASED_LINE_WIDTH_RANGE: case GL_SMOOTH_LINE_WIDTH_RANGE: { float f[2] = {1, 64}; gfv(g, 2, f); return 1; }
    case GL_ALIASED_POINT_SIZE_RANGE: case GL_SMOOTH_POINT_SIZE_RANGE: { float f[2] = {1, 1024}; gfv(g, 2, f); return 1; }
    case GL_SMOOTH_LINE_WIDTH_GRANULARITY: case GL_POINT_SIZE_GRANULARITY: { float f = 0.125f; gfv(g, 1, &f); return 1; }
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS: I1(10); return 1;
    case GL_COMPRESSED_TEXTURE_FORMATS:
        gi(g, 10, (i64)GL_COMPRESSED_RGB_S3TC_DXT1_EXT, (i64)GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, (i64)GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
           (i64)GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, (i64)GL_COMPRESSED_RED_RGTC1, (i64)GL_COMPRESSED_SIGNED_RED_RGTC1,
           (i64)GL_COMPRESSED_RG_RGTC2, (i64)GL_COMPRESSED_SIGNED_RG_RGTC2, (i64)GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
           (i64)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
        return 1;
    case GL_NUM_SHADER_BINARY_FORMATS: case GL_NUM_PROGRAM_BINARY_FORMATS: I1(0); return 1;
    case GL_SHADER_COMPILER: gb(g, 1, (u8[]){1}); return 1;
    case GL_DOUBLEBUFFER: gb(g, 1, (u8[]){(u8)C->fb_double}); return 1;
    case GL_STEREO: gb(g, 1, (u8[]){0}); return 1;
    case GL_RED_BITS: case GL_GREEN_BITS: case GL_BLUE_BITS: I1(8); return 1;
    case GL_ALPHA_BITS: I1(C->fb_alpha ? 8 : 0); return 1;
    case GL_DEPTH_BITS: I1(C->fb_has_depth ? 24 : 0); return 1;
    case GL_STENCIL_BITS: I1(C->fb_has_stencil ? 8 : 0); return 1;
    case GL_ACCUM_RED_BITS: case GL_ACCUM_GREEN_BITS: case GL_ACCUM_BLUE_BITS: case GL_ACCUM_ALPHA_BITS: case GL_INDEX_BITS: I1(0); return 1;
    case GL_AUX_BUFFERS: I1(0); return 1;
    case GL_RGBA_MODE: gb(g, 1, (u8[]){1}); return 1;
    case GL_INDEX_MODE: gb(g, 1, (u8[]){0}); return 1;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT: I1(GL_BGRA); return 1;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE: I1(GL_UNSIGNED_INT_8_8_8_8_REV); return 1;
    /* bindings */
    case GL_ACTIVE_TEXTURE: I1(GL_TEXTURE0 + C->unit); return 1;
    case GL_CLIENT_ACTIVE_TEXTURE: I1(GL_TEXTURE0 + C->client_unit); return 1;
    case GL_TEXTURE_BINDING_1D: I1(tex_name(C, 0)); return 1;
    case GL_TEXTURE_BINDING_2D: I1(tex_name(C, 1)); return 1;
    case GL_TEXTURE_BINDING_3D: I1(tex_name(C, 2)); return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP: I1(tex_name(C, 3)); return 1;
    case GL_TEXTURE_BINDING_RECTANGLE: I1(tex_name(C, 4)); return 1;
    case GL_TEXTURE_BINDING_1D_ARRAY: I1(tex_name(C, 5)); return 1;
    case GL_TEXTURE_BINDING_2D_ARRAY: I1(tex_name(C, 6)); return 1;
    case GL_TEXTURE_BINDING_BUFFER: I1(tex_name(C, 7)); return 1;
    case GL_TEXTURE_BINDING_2D_MULTISAMPLE: I1(tex_name(C, 8)); return 1;
    case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY: I1(tex_name(C, 9)); return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY: I1(tex_name(C, 10)); return 1;
    case GL_SAMPLER_BINDING: I1(C->samp[C->unit] ? C->samp[C->unit]->name : 0); return 1;
    case GL_ARRAY_BUFFER_BINDING: I1(buf_name(C->b_array)); return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: I1(buf_name(C->vao->ebo)); return 1;
    case GL_COPY_READ_BUFFER: I1(buf_name(C->b_copy_r)); return 1;
    case GL_COPY_WRITE_BUFFER: I1(buf_name(C->b_copy_w)); return 1;
    case GL_PIXEL_PACK_BUFFER_BINDING: I1(buf_name(C->b_pack)); return 1;
    case GL_PIXEL_UNPACK_BUFFER_BINDING: I1(buf_name(C->b_unpack)); return 1;
    case GL_UNIFORM_BUFFER_BINDING: I1(buf_name(C->b_ubo)); return 1;
    case GL_TEXTURE_BUFFER: I1(buf_name(C->b_tbo)); return 1;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING: I1(buf_name(C->b_xfb)); return 1;
    case GL_DRAW_INDIRECT_BUFFER_BINDING: I1(buf_name(C->b_indirect)); return 1;
    case GL_VERTEX_ARRAY_BINDING: I1(C->vao->name); return 1;
    case GL_DRAW_FRAMEBUFFER_BINDING: I1(C->draw_fbo ? C->draw_fbo->name : 0); return 1;
    case GL_READ_FRAMEBUFFER_BINDING: I1(C->read_fbo ? C->read_fbo->name : 0); return 1;
    case GL_RENDERBUFFER_BINDING: I1(C->rb ? C->rb->name : 0); return 1;
    case GL_CURRENT_PROGRAM: I1(C->prog ? C->prog->name : 0); return 1;
    case GL_TRANSFORM_FEEDBACK_BINDING: I1(C->xfb->name); return 1;
    case GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED: gb(g, 1, (u8[]){(u8)C->xfb->paused}); return 1;
    case GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE: gb(g, 1, (u8[]){(u8)C->xfb->active}); return 1;
    case GL_DRAW_BUFFER: case GL_DRAW_BUFFER0: I1(C->draw_fbo ? C->draw_fbo->draw_buf[0] : C->def_draw_buf[0]); return 1;
    case GL_READ_BUFFER: I1(C->read_fbo ? C->read_fbo->read_buf : C->def_read_buf); return 1;
    case GL_TIMESTAMP: I1(0); return 1;
    /* fixed function */
    case GL_MATRIX_MODE: I1(C->mmode); return 1;
    case GL_MODELVIEW_MATRIX: gmat(g, &C->mv); return 1;
    case GL_PROJECTION_MATRIX: gmat(g, &C->proj); return 1;
    case GL_TEXTURE_MATRIX: gmat(g, &C->texm[C->unit < 8 ? C->unit : 7]); return 1;
    case GL_COLOR_MATRIX: gmat(g, &C->colm); return 1;
    case GL_TRANSPOSE_MODELVIEW_MATRIX: case GL_TRANSPOSE_PROJECTION_MATRIX: case GL_TRANSPOSE_TEXTURE_MATRIX: case GL_TRANSPOSE_COLOR_MATRIX: {
        const mstack_t *s = p == GL_TRANSPOSE_MODELVIEW_MATRIX ? &C->mv : p == GL_TRANSPOSE_PROJECTION_MATRIX ? &C->proj :
                            p == GL_TRANSPOSE_TEXTURE_MATRIX ? &C->texm[C->unit < 8 ? C->unit : 7] : &C->colm;
        float t[16];
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) t[i * 4 + j] = s->s[s->top].m[j * 4 + i];
        gfv(g, 16, t);
        return 1;
    }
    case GL_MODELVIEW_STACK_DEPTH: I1(C->mv.top + 1); return 1;
    case GL_PROJECTION_STACK_DEPTH: I1(C->proj.top + 1); return 1;
    case GL_TEXTURE_STACK_DEPTH: I1(C->texm[C->unit < 8 ? C->unit : 7].top + 1); return 1;
    case GL_COLOR_MATRIX_STACK_DEPTH: I1(C->colm.top + 1); return 1;
    case GL_ATTRIB_STACK_DEPTH: I1(C->attr_top); return 1;
    case GL_CLIENT_ATTRIB_STACK_DEPTH: I1(C->cattr_top); return 1;
    case GL_CURRENT_COLOR: gfv(g, 4, C->cur_col); return 1;
    case GL_CURRENT_SECONDARY_COLOR: gfv(g, 4, C->cur_col2); return 1;
    case GL_CURRENT_NORMAL: gfv(g, 3, C->cur_nrm); return 1;
    case GL_CURRENT_TEXTURE_COORDS: gfv(g, 4, C->cur_tc[C->unit < 8 ? C->unit : 7]); return 1;
    case GL_CURRENT_FOG_COORD: gfv(g, 1, &C->cur_fog); return 1;
    case GL_CURRENT_RASTER_POSITION: gfv(g, 4, C->raster_pos); return 1;
    case GL_CURRENT_RASTER_POSITION_VALID: gb(g, 1, (u8[]){(u8)C->raster_valid}); return 1;
    case GL_CURRENT_RASTER_COLOR: gfv(g, 4, C->raster_col); return 1;
    case GL_CURRENT_RASTER_TEXTURE_COORDS: gfv(g, 4, C->raster_tc); return 1;
    case GL_EDGE_FLAG: gb(g, 1, (u8[]){(u8)C->cur_edge}); return 1;
    case GL_SHADE_MODEL: I1(C->shade); return 1;
    case GL_ALPHA_TEST_FUNC: I1(C->alpha_func); return 1;
    case GL_ALPHA_TEST_REF: gfv(g, 1, &C->alpha_ref); return 1;
    case GL_LIGHT_MODEL_AMBIENT: gfv(g, 4, C->lm_amb); return 1;
    case GL_LIGHT_MODEL_LOCAL_VIEWER: gb(g, 1, (u8[]){(u8)C->lm_local}); return 1;
    case GL_LIGHT_MODEL_TWO_SIDE: gb(g, 1, (u8[]){(u8)C->lm_two}); return 1;
    case GL_LIGHT_MODEL_COLOR_CONTROL: I1(C->lm_sep_spec ? GL_SEPARATE_SPECULAR_COLOR : GL_SINGLE_COLOR); return 1;
    case GL_COLOR_MATERIAL_FACE: I1(C->cm_face); return 1;
    case GL_COLOR_MATERIAL_PARAMETER: I1(C->cm_mode); return 1;
    case GL_FOG_MODE: I1(C->fog_mode); return 1;
    case GL_FOG_DENSITY: gfv(g, 1, &C->fog_d); return 1;
    case GL_FOG_START: gfv(g, 1, &C->fog_s); return 1;
    case GL_FOG_END: gfv(g, 1, &C->fog_e); return 1;
    case GL_FOG_COLOR: gfv(g, 4, C->fog_c); return 1;
    case GL_FOG_INDEX: gfv(g, 1, &C->fog_index); return 1;
    case GL_FOG_COORD_SRC: I1(C->fog_src); return 1;
    case GL_RENDER_MODE: I1(C->render_mode); return 1;
    case GL_LIST_BASE: I1(C->list_base); return 1;
    case GL_LIST_INDEX: I1(C->dl_cur ? C->dl_cur->name : 0); return 1;
    case GL_LIST_MODE: I1(C->dl_cur ? C->dl_mode : 0); return 1;
    case GL_NAME_STACK_DEPTH: I1(C->name_top); return 1;
    case GL_ZOOM_X: gfv(g, 1, &C->pzoom[0]); return 1;
    case GL_ZOOM_Y: gfv(g, 1, &C->pzoom[1]); return 1;
    case GL_RED_SCALE: gfv(g, 1, &C->pmap_scale[0]); return 1;
    case GL_GREEN_SCALE: gfv(g, 1, &C->pmap_scale[1]); return 1;
    case GL_BLUE_SCALE: gfv(g, 1, &C->pmap_scale[2]); return 1;
    case GL_ALPHA_SCALE: gfv(g, 1, &C->pmap_scale[3]); return 1;
    case GL_RED_BIAS: gfv(g, 1, &C->pmap_bias[0]); return 1;
    case GL_GREEN_BIAS: gfv(g, 1, &C->pmap_bias[1]); return 1;
    case GL_BLUE_BIAS: gfv(g, 1, &C->pmap_bias[2]); return 1;
    case GL_ALPHA_BIAS: gfv(g, 1, &C->pmap_bias[3]); return 1;
    case GL_PERSPECTIVE_CORRECTION_HINT: I1(C->hint[4]); return 1;
    case GL_LINE_SMOOTH_HINT: I1(C->hint[0]); return 1;
    case GL_POLYGON_SMOOTH_HINT: I1(C->hint[1]); return 1;
    case GL_TEXTURE_COMPRESSION_HINT: I1(C->hint[2]); return 1;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT: I1(C->hint[3]); return 1;
    case GL_POINT_SMOOTH_HINT: I1(C->hint[5]); return 1;
    case GL_FOG_HINT: I1(C->hint[6]); return 1;
    case GL_GENERATE_MIPMAP_HINT: I1(C->hint[7]); return 1;
    case GL_VERTEX_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_vertex.enabled}); return 1;
    case GL_NORMAL_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_normal.enabled}); return 1;
    case GL_COLOR_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_color.enabled}); return 1;
    case GL_SECONDARY_COLOR_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_color2.enabled}); return 1;
    case GL_FOG_COORD_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_fog.enabled}); return 1;
    case GL_TEXTURE_COORD_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_tc[C->client_unit].enabled}); return 1;
    case GL_INDEX_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_index.enabled}); return 1;
    case GL_EDGE_FLAG_ARRAY: gb(g, 1, (u8[]){(u8)C->vao->ca_edge.enabled}); return 1;
    case GL_VERTEX_ARRAY_SIZE: I1(C->vao->ca_vertex.size); return 1;
    case GL_VERTEX_ARRAY_TYPE: I1(C->vao->ca_vertex.type); return 1;
    case GL_VERTEX_ARRAY_STRIDE: I1(C->vao->ca_vertex.stride); return 1;
    case GL_COLOR_ARRAY_SIZE: I1(C->vao->ca_color.size); return 1;
    case GL_COLOR_ARRAY_TYPE: I1(C->vao->ca_color.type); return 1;
    case GL_COLOR_ARRAY_STRIDE: I1(C->vao->ca_color.stride); return 1;
    case GL_NORMAL_ARRAY_TYPE: I1(C->vao->ca_normal.type); return 1;
    case GL_NORMAL_ARRAY_STRIDE: I1(C->vao->ca_normal.stride); return 1;
    case GL_TEXTURE_COORD_ARRAY_SIZE: I1(C->vao->ca_tc[C->client_unit].size); return 1;
    case GL_TEXTURE_COORD_ARRAY_TYPE: I1(C->vao->ca_tc[C->client_unit].type); return 1;
    case GL_TEXTURE_COORD_ARRAY_STRIDE: I1(C->vao->ca_tc[C->client_unit].stride); return 1;
    case GL_VERTEX_ARRAY_BUFFER_BINDING: I1(buf_name(C->vao->ca_vertex.buf)); return 1;
    case GL_COLOR_ARRAY_BUFFER_BINDING: I1(buf_name(C->vao->ca_color.buf)); return 1;
    case GL_NORMAL_ARRAY_BUFFER_BINDING: I1(buf_name(C->vao->ca_normal.buf)); return 1;
    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: I1(buf_name(C->vao->ca_tc[C->client_unit].buf)); return 1;
    case GL_BLEND: gb(g, 1, (u8[]){(u8)C->blend_on[0]}); return 1;
    }
    int b = cap_bit(C, p);
    if (b >= 0) { gb(g, 1, (u8[]){(u8)get_bit(C, b)}); return 1; }
    return 0;
}
static int get_idx(gl_ctx_t *C, GLenum p, GLuint i, gv_t *g) {
    if (p == GL_BLEND && i < GLX_DRAW) { gb(g, 1, (u8[]){(u8)C->blend_on[i]}); return 1; }
    if (p == GL_COLOR_WRITEMASK && i < GLX_DRAW) { gb(g, 4, C->cmask[i]); return 1; }
    if (p == GL_BLEND_SRC_RGB && i < GLX_DRAW) { I1(C->bsrc_rgb[i]); return 1; }
    if (p == GL_BLEND_DST_RGB && i < GLX_DRAW) { I1(C->bdst_rgb[i]); return 1; }
    if (p == GL_BLEND_SRC_ALPHA && i < GLX_DRAW) { I1(C->bsrc_a[i]); return 1; }
    if (p == GL_BLEND_DST_ALPHA && i < GLX_DRAW) { I1(C->bdst_a[i]); return 1; }
    if (p == GL_BLEND_EQUATION_RGB && i < GLX_DRAW) { I1(C->beq_rgb[i]); return 1; }
    if (p == GL_BLEND_EQUATION_ALPHA && i < GLX_DRAW) { I1(C->beq_a[i]); return 1; }
    if (p == GL_UNIFORM_BUFFER_BINDING && i < GLX_UBO) { I1(buf_name(C->ubo[i])); return 1; }
    if (p == GL_UNIFORM_BUFFER_START && i < GLX_UBO) { I1(C->ubo_off[i]); return 1; }
    if (p == GL_UNIFORM_BUFFER_SIZE && i < GLX_UBO) { I1(C->ubo_size[i]); return 1; }
    if (p == GL_TRANSFORM_FEEDBACK_BUFFER_BINDING && i < GLX_XFB) { I1(buf_name(C->xfb->buf[i])); return 1; }
    if (p == GL_TRANSFORM_FEEDBACK_BUFFER_START && i < GLX_XFB) { I1(C->xfb->off[i]); return 1; }
    if (p == GL_TRANSFORM_FEEDBACK_BUFFER_SIZE && i < GLX_XFB) { I1(C->xfb->size[i]); return 1; }
    if ((p == GL_VIEWPORT || p == GL_SCISSOR_BOX || p == GL_DEPTH_RANGE) && i < 16) return get_any(C, p, g);
    if (p == GL_SAMPLE_MASK_VALUE && i == 0) { I1(0xffffffffu); return 1; }
    return 0;
}
void glGetIntegerv(GLenum p, GLint *out) {
    GLC();
    gv_t g;
    if (!out) return;
    if (!get_any(C, p, &g)) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < g.n; i++) {
        if (g.kind == T_F && (p == GL_COLOR_CLEAR_VALUE || p == GL_BLEND_COLOR || p == GL_CURRENT_COLOR || p == GL_DEPTH_RANGE ||
                              p == GL_DEPTH_CLEAR_VALUE || p == GL_FOG_COLOR || p == GL_LIGHT_MODEL_AMBIENT || p == GL_ACCUM_CLEAR_VALUE)) {
            /* cores e profundidade viram inteiros mapeados em [-2^31, 2^31-1] */
            double v = g.d[i] < -1 ? -1 : g.d[i] > 1 ? 1 : g.d[i];
            out[i] = (GLint)(v >= 0 ? v * 2147483647.0 : v * 2147483648.0);
        } else out[i] = g.kind == T_F ? (GLint)(g.d[i] < 0 ? g.d[i] - 0.5 : g.d[i] + 0.5) : (GLint)g.i[i];
    }
}
void glGetInteger64v(GLenum p, GLint64 *out) {
    GLC();
    gv_t g;
    if (!out) return;
    if (p == GL_MAX_SERVER_WAIT_TIMEOUT) { out[0] = 0; return; }
    if (!get_any(C, p, &g)) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < g.n; i++) out[i] = g.kind == T_F ? (GLint64)(g.d[i] + 0.5) : (GLint64)g.i[i];
}
void glGetFloatv(GLenum p, GLfloat *out) {
    GLC();
    gv_t g;
    if (!out) return;
    if (!get_any(C, p, &g)) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < g.n; i++) out[i] = (float)g.d[i];
}
void glGetDoublev(GLenum p, GLdouble *out) {
    GLC();
    gv_t g;
    if (!out) return;
    if (!get_any(C, p, &g)) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < g.n; i++) out[i] = g.d[i];
}
void glGetBooleanv(GLenum p, GLboolean *out) {
    GLC();
    gv_t g;
    if (!out) return;
    if (!get_any(C, p, &g)) GLERR(GL_INVALID_ENUM, );
    for (int i = 0; i < g.n; i++) out[i] = g.kind == T_F ? g.d[i] != 0 : g.i[i] != 0;
}
void glGetIntegeri_v(GLenum p, GLuint i, GLint *out) { GLC(); gv_t g; if (!out) return; if (!get_idx(C, p, i, &g)) GLERR(GL_INVALID_ENUM, ); for (int k = 0; k < g.n; k++) out[k] = (GLint)g.i[k]; }
void glGetInteger64i_v(GLenum p, GLuint i, GLint64 *out) { GLC(); gv_t g; if (!out) return; if (!get_idx(C, p, i, &g)) GLERR(GL_INVALID_ENUM, ); for (int k = 0; k < g.n; k++) out[k] = g.i[k]; }
void glGetBooleani_v(GLenum p, GLuint i, GLboolean *out) { GLC(); gv_t g; if (!out) return; if (!get_idx(C, p, i, &g)) GLERR(GL_INVALID_ENUM, ); for (int k = 0; k < g.n; k++) out[k] = g.i[k] != 0; }
void glGetFloati_v(GLenum p, GLuint i, GLfloat *out) { GLC(); gv_t g; if (!out) return; if (!get_idx(C, p, i, &g)) GLERR(GL_INVALID_ENUM, ); for (int k = 0; k < g.n; k++) out[k] = (float)g.d[k]; }
void glGetDoublei_v(GLenum p, GLuint i, GLdouble *out) { GLC(); gv_t g; if (!out) return; if (!get_idx(C, p, i, &g)) GLERR(GL_INVALID_ENUM, ); for (int k = 0; k < g.n; k++) out[k] = g.d[k]; }
void glGetIntegerIndexedvEXT(GLenum p, GLuint i, GLint *out) { glGetIntegeri_v(p, i, out); }
void glGetBooleanIndexedvEXT(GLenum p, GLuint i, GLboolean *out) { glGetBooleani_v(p, i, out); }
void glGetPointerv(GLenum p, void **out) {
    GLC();
    if (!out) return;
    switch (p) {
    case GL_VERTEX_ARRAY_POINTER: *out = (void *)C->vao->ca_vertex.ptr; return;
    case GL_NORMAL_ARRAY_POINTER: *out = (void *)C->vao->ca_normal.ptr; return;
    case GL_COLOR_ARRAY_POINTER: *out = (void *)C->vao->ca_color.ptr; return;
    case GL_SECONDARY_COLOR_ARRAY_POINTER: *out = (void *)C->vao->ca_color2.ptr; return;
    case GL_FOG_COORD_ARRAY_POINTER: *out = (void *)C->vao->ca_fog.ptr; return;
    case GL_TEXTURE_COORD_ARRAY_POINTER: *out = (void *)C->vao->ca_tc[C->client_unit].ptr; return;
    case GL_INDEX_ARRAY_POINTER: *out = (void *)C->vao->ca_index.ptr; return;
    case GL_EDGE_FLAG_ARRAY_POINTER: *out = (void *)C->vao->ca_edge.ptr; return;
    case GL_SELECTION_BUFFER_POINTER: *out = C->sel_buf; return;
    case GL_FEEDBACK_BUFFER_POINTER: *out = C->fb_buf; return;
    }
    GLERR(GL_INVALID_ENUM, );
}

/* ================================================================ alvos */
int gl_tgt_index(GLenum t) {
    switch (t) {
    case GL_TEXTURE_1D: case GL_PROXY_TEXTURE_1D: return 0;
    case GL_TEXTURE_2D: case GL_PROXY_TEXTURE_2D: return 1;
    case GL_TEXTURE_3D: case GL_PROXY_TEXTURE_3D: return 2;
    case GL_TEXTURE_CUBE_MAP: case GL_PROXY_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X: case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return 3;
    case GL_TEXTURE_RECTANGLE: case GL_PROXY_TEXTURE_RECTANGLE: return 4;
    case GL_TEXTURE_1D_ARRAY: case GL_PROXY_TEXTURE_1D_ARRAY: return 5;
    case GL_TEXTURE_2D_ARRAY: case GL_PROXY_TEXTURE_2D_ARRAY: return 6;
    case GL_TEXTURE_BUFFER: return 7;
    case GL_TEXTURE_2D_MULTISAMPLE: case GL_PROXY_TEXTURE_2D_MULTISAMPLE: return 8;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY: return 9;
    case GL_TEXTURE_CUBE_MAP_ARRAY: case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY: return 10;
    }
    return -1;
}
gl_tex_t *gl_tex_bound(gl_ctx_t *C, GLenum target) { int i = gl_tgt_index(target); return i < 0 ? 0 : C->tex[C->unit][i]; }
gl_buf_t **gl_buf_slot(gl_ctx_t *C, GLenum t) {
    switch (t) {
    case GL_ARRAY_BUFFER: return &C->b_array;
    case GL_ELEMENT_ARRAY_BUFFER: return &C->vao->ebo;
    case GL_COPY_READ_BUFFER: return &C->b_copy_r;
    case GL_COPY_WRITE_BUFFER: return &C->b_copy_w;
    case GL_PIXEL_PACK_BUFFER: return &C->b_pack;
    case GL_PIXEL_UNPACK_BUFFER: return &C->b_unpack;
    case GL_UNIFORM_BUFFER: return &C->b_ubo;
    case GL_TEXTURE_BUFFER: return &C->b_tbo;
    case GL_TRANSFORM_FEEDBACK_BUFFER: return &C->b_xfb;
    case GL_DRAW_INDIRECT_BUFFER: return &C->b_indirect;
    }
    return 0;
}

/* ================================================================ depuração (KHR_debug / EXT_debug_*) */
void glDebugMessageCallback(GLDEBUGPROC cb, const void *user) { (void)cb; (void)user; }
void glDebugMessageControl(GLenum s, GLenum t, GLenum sev, GLsizei n, const GLuint *ids, GLboolean en) { (void)s; (void)t; (void)sev; (void)n; (void)ids; (void)en; }
void glDebugMessageInsert(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei n, const GLchar *m) { (void)s; (void)t; (void)id; (void)sev; (void)n; (void)m; }
GLuint glGetDebugMessageLog(GLuint count, GLsizei bs, GLenum *s, GLenum *t, GLuint *ids, GLenum *sev, GLsizei *len, GLchar *log) {
    (void)count; (void)bs; (void)s; (void)t; (void)ids; (void)sev; (void)len; (void)log;
    return 0;   /* nenhuma mensagem é gerada: os erros ficam em glGetError */
}
void glPushGroupMarkerEXT(GLsizei n, const GLchar *m) { (void)n; (void)m; }
void glPopGroupMarkerEXT(void) {}
void glInsertEventMarkerEXT(GLsizei n, const GLchar *m) { (void)n; (void)m; }
void glLabelObjectEXT(GLenum t, GLuint o, GLsizei n, const GLchar *l) { (void)t; (void)o; (void)n; (void)l; }
void glGetObjectLabelEXT(GLenum t, GLuint o, GLsizei bs, GLsizei *len, GLchar *l) { (void)t; (void)o; if (len) *len = 0; if (l && bs > 0) l[0] = 0; }

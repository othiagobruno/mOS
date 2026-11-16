/* mOS libSystem — OpenGL por software: estruturas internas compartilhadas por gl_ctx.c, gl_obj.c,
 * gl_raster.c, gl_ffp.c e gl_glsl.c. Texturas usam o msl_tex_t e a amostragem do Metal (mtltex.c);
 * shaders GLSL rodam no interpretador de MSL (mtlexec.c). */
#pragma once
#include "mtltex.h"
#include "libc.h"
#include "gl_enum.h"

double sqrt(double), floor(double), ceil(double), pow(double, double), fabs(double), sin(double), cos(double),
    tan(double), exp(double), log(double), ldexp(double, int);
u16 msl_f2h(float f);
float msl_h2f(u16 h);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef double GLdouble;
typedef long GLintptr;
typedef long GLsizeiptr;

#define GLX_UNITS 32          /* unidades de textura combinadas */
#define GLX_ATTR 16           /* atributos de vértice */
#define GLX_VAR 32            /* vec4 de varyings (o GLSL usa até 16; o fixed function usa os 32) */
#define GLX_DRAW 8            /* draw buffers / color attachments */
#define GLX_LIGHTS 8
#define GLX_CLIP 8
#define GLX_UBO 36
#define GLX_XFB 4

/* ---- nomes (tabela por tipo de objeto) */
typedef struct { void **v; u32 n, cap; u8 *used; char **lab; } gl_names_t;   /* lab: rótulos de depuração */
u32 gl_names_gen(gl_names_t *t);
void gl_names_reserve(gl_names_t *t, u32 name);
void *gl_names_get(gl_names_t *t, u32 name);
void gl_names_set(gl_names_t *t, u32 name, void *obj);
int gl_names_is(gl_names_t *t, u32 name);
void gl_names_del(gl_names_t *t, u32 name);

/* ---- buffer */
typedef struct gl_buf {
    u32 name;
    u8 *data;
    i64 size;
    GLenum usage;
    int mapped;
    GLbitfield map_access;
    i64 map_off, map_len;
    int refs;
} gl_buf_t;

/* ---- textura: cada nível guardado à parte; gl_tex_complete monta o msl_tex_t que o amostrador lê */
typedef struct {
    int w, h, d;               /* d = camadas para arrays, 6 faces para cube (layout por face) */
    GLenum ifmt;
    int fmt;                   /* formato Metal (mtltex.c) */
    u8 *data;                  /* w*h*d*bpp */
    u8 *cdata;                 /* imagem comprimida original (glGetCompressedTexImage) */
    i64 csize;
    int ns;                    /* amostras por pixel (texturas multisample; 0 = uma); data tem w*h*d*ns texels */
} gl_level_t;
typedef struct gl_tex {
    u32 name;
    GLenum target;
    gl_level_t lv[16];
    int immutable, imm_levels;
    int fixed_loc;             /* GL_TEXTURE_FIXED_SAMPLE_LOCATIONS */
    /* parâmetros de amostragem */
    GLenum min_f, mag_f, wrap[3], cmp_mode, cmp_func;
    float min_lod, max_lod, lod_bias, border[4], aniso;
    int base_level, max_level;
    GLenum swz[4];
    GLenum depth_mode;         /* GL_DEPTH_TEXTURE_MODE (compat) */
    int gen_mipmap;            /* GL_GENERATE_MIPMAP (compat) */
    /* montado */
    msl_tex_t mt;
    u8 *packed;
    int dirty, complete;
    gl_buf_t *tbo;             /* GL_TEXTURE_BUFFER */
    GLenum tbo_fmt;
    i64 tbo_off, tbo_size;
    void *iosurf;              /* CGLTexImageIOSurface2D */
    int refs;
} gl_tex_t;

typedef struct gl_samp {
    u32 name;
    GLenum min_f, mag_f, wrap[3], cmp_mode, cmp_func;
    float min_lod, max_lod, lod_bias, border[4], aniso;
} gl_samp_t;

/* ---- renderbuffer */
typedef struct gl_rb {
    u32 name;
    GLenum ifmt;
    int fmt, w, h, samples;
    msl_tex_t mt;
    u8 *data;
    int refs;
} gl_rb_t;

/* ---- framebuffer */
typedef struct {
    GLenum type;               /* 0, GL_TEXTURE, GL_RENDERBUFFER */
    u32 name;
    int level, layer;          /* layer: camada do array, face do cube (0..5) ou fatia do 3D; -1 = em camadas */
} gl_att_t;
typedef struct gl_fbo {
    u32 name;
    gl_att_t color[GLX_DRAW], depth, stencil;
    GLenum draw_buf[GLX_DRAW];
    GLenum read_buf;
    int def_w, def_h;          /* GL_FRAMEBUFFER_DEFAULT_* */
} gl_fbo_t;

/* superfície onde se desenha (resolvida de um FBO ou do framebuffer padrão) */
typedef struct {
    msl_tex_t *color[GLX_DRAW];
    int clayer[GLX_DRAW], clevel[GLX_DRAW];
    msl_tex_t *depth, *stencil;
    int dlayer, dlevel, slayer, slevel;
    int w, h, ncolor;
    int srgb[GLX_DRAW], isint[GLX_DRAW];
    int ybottom;               /* 1: a linha 0 do msl_tex_t é a de baixo (framebuffer padrão e texturas GL) */
    int samples;               /* amostras por pixel de todos os anexos (1 = sem multiamostra) */
    /* FBO em camadas (glFramebufferTexture de array, 3D ou cube): camada 0 por padrão, gl_Layer escolhe outra */
    int layered, nlayers;
    msl_tex_t *lt[GLX_DRAW + 2];   /* anexos em camadas (cor..., profundidade, estêncil) */
    u8 *lbase[GLX_DRAW + 2];
    u64 lstride[GLX_DRAW + 2];
} gl_target_t;
void gl_target_layer(gl_target_t *T, int layer);   /* aponta os anexos em camadas para a camada */

/* ---- VAO */
typedef struct {
    int enabled;
    GLint size;                /* 1..4 ou GL_BGRA */
    GLenum type;
    int normalized, integer, isdouble;
    GLsizei stride;
    gl_buf_t *buf;
    i64 off;                   /* deslocamento no buffer ou ponteiro de cliente (buf 0) */
    const void *ptr;
    u32 divisor;
} gl_attr_t;
typedef struct gl_vao {
    u32 name;
    gl_attr_t a[GLX_ATTR];
    gl_buf_t *ebo;
    /* arrays de cliente do compat: também fazem parte do VAO (APPLE_vertex_array_object) */
    gl_attr_t ca_vertex, ca_normal, ca_color, ca_color2, ca_fog, ca_index, ca_edge, ca_tc[8];
} gl_vao_t;

/* ---- shaders e programas (gl_glsl.c); shaders e programas dividem o espaço de nomes */
typedef struct msl_lib msl_lib_t;
typedef struct msl_fn msl_fn_t;
typedef struct gl_shader {
    int kind;                  /* 1 */
    u32 name;
    GLenum type;
    char *src;
    char *log;
    int compiled, deleted, refs;
} gl_shader_t;
typedef struct gl_prog {
    int kind;                  /* 2 */
    u32 name;
    gl_shader_t *sh[8];
    int nsh;
    int linked, validated, deleted, refs, separable;
    char *log;
    int nxfb;                  /* varyings de transform feedback do último link */
    /* pedidos antes do link */
    char *abind_name[64]; int abind_loc[64]; int nabind;
    char *fbind_name[16]; int fbind_loc[16], fbind_idx[16]; int nfbind;
    char **xfb_names; int nxfb_names; GLenum xfb_mode;
    void *priv;                /* estado do último link (gl_glsl.c) */
    void *priv_shaders;        /* programas internos: os shaders são do próprio programa */
} gl_prog_t;

typedef struct gl_query { u32 name; GLenum target; u64 result; int active, avail; } gl_query_t;
typedef struct gl_sync { u32 magic; int signaled; } gl_sync_t;
typedef struct gl_xfb { u32 name; gl_buf_t *buf[GLX_XFB]; i64 off[GLX_XFB], size[GLX_XFB]; int active, paused; GLenum prim; u64 written; } gl_xfb_t;

/* ---- listas de exibição (compat): comandos gravados */
typedef struct gl_dlcmd gl_dlcmd_t;
typedef struct gl_dlist { u32 name; gl_dlcmd_t *head, *tail; } gl_dlist_t;

/* ---- grupo compartilhado */
typedef struct gl_share {
    int refs;
    gl_names_t bufs, texs, rbs, progs, samps, lists;   /* shaders e programas dividem os nomes */
} gl_share_t;

/* ---- fixed function */
typedef struct { float m[16]; } mat4_t;
typedef struct { mat4_t s[32]; int top, max; } mstack_t;
typedef struct {
    float amb[4], dif[4], spec[4], pos[4], spot_dir[3], spot_exp, spot_cut, att[3];
    int on;
} gl_light_t;
typedef struct { float amb[4], dif[4], spec[4], emi[4], shin; int ci[3]; } gl_mat_t;
typedef struct {
    GLenum mode;               /* GL_MODULATE ... GL_COMBINE */
    float color[4];
    GLenum comb_rgb, comb_a, src[2][3], op[2][3];
    float scale_rgb, scale_a;
    float lod_bias;
    int gen_on[4];             /* S T R Q */
    GLenum gen_mode[4];
    float gen_obj[4][4], gen_eye[4][4];
    int coord_replace;
} gl_texenv_t;
/* atributo imediato (glBegin): posição + correntes */
typedef struct {
    float pos[4], col[4], col2[4], nrm[3], tc[8][4], fog, attr[GLX_ATTR][4];
    int edge;
} gl_imv_t;

/* ---- vértice processado e fragmento */
typedef struct {
    float clip[4];             /* posição no espaço de recorte */
    float win[4];              /* depois da divisão e da viewport: x, y, z, 1/w */
    float v[GLX_VAR][4];
    float psize;
    float cd[GLX_CLIP];     /* gl_ClipDistance / planos do usuário */
    int edge;
    float xfb[64];             /* saídas capturadas pelo transform feedback, na ordem do programa */
    int nxfb;
    int layer;                 /* gl_Layer do GS: camada do FBO em camadas */
} gl_vtx_t;
typedef struct {
    float coord[4];            /* gl_FragCoord */
    int front;
    float pcoord[2];
    float v[GLX_VAR][4];
    float color[GLX_DRAW][4];
    i32 icolor[GLX_DRAW][4];
    float depth;
    int discard;
    u32 prim_id;
    float dx[GLX_VAR][4], dy[GLX_VAR][4];   /* derivadas em x e y dos slots em gl_stage_t.dmask */
    int sample_id;             /* gl_SampleID (sombreamento por amostra; 0 fora dele) */
    float sample_pos[2];       /* gl_SamplePosition */
    u32 mask_in;               /* gl_SampleMaskIn: amostras cobertas */
    u32 mask_out;              /* gl_SampleMask escrito pelo shader (S->writes_mask) */
} gl_frag_t;

struct gl_ctx;
typedef struct {
    int nvar;                  /* slots de varying usados */
    u8 interp[GLX_VAR];     /* MSL_INTERP_* por slot */
    int nclip;
    void (*vs)(struct gl_ctx *C, u32 vid, u32 iid, gl_vtx_t *out);
    void (*fs)(struct gl_ctx *C, gl_frag_t *f);
    int writes_depth, discards;
    int two_side;              /* fixed function: v[2..3] são as cores do verso */
    int point_sprite;
    u32 dmask;                 /* slots com derivadas calculadas (lod das texturas do fixed function) */
    int alpha_test;            /* compat: o teste de alfa vale (fixed function e GLSL no perfil compat) */
    int nxfb;                  /* floats por vértice no transform feedback */
    void *self;
    void (*gs)(struct gl_ctx *C, gl_vtx_t **in, int n, u32 prim);   /* geometry shader (0 = não há) */
    GLenum gs_in;              /* primitivo de entrada do GS */
    GLenum gs_out;             /* primitivo de saída do GS (GL_POINTS, GL_LINE_STRIP, GL_TRIANGLE_STRIP) */
    int per_sample;            /* o fragment shader lê gl_SampleID/gl_SamplePosition: roda uma vez por amostra */
    int writes_mask;           /* o fragment shader escreve gl_SampleMask */
    u32 centroid;              /* slots com interpolação centroid */
    void (*tess)(struct gl_ctx *C, gl_vtx_t **in, int n, u32 prim);   /* tesselação (0 = não há): um patch */
    GLenum tess_out;           /* primitivo que a tesselação gera: GL_TRIANGLES, GL_LINES ou GL_POINTS */
} gl_stage_t;

/* ---- contexto */
typedef struct gl_ctx {
    int profile_core;          /* 1 = 3.2+ core (CGL kCGLOGLPVersion_3_2_Core / 4_1) */
    int major, minor;
    GLenum err;
    gl_share_t *sh;
    /* framebuffer padrão (back e front; linha 0 embaixo) */
    msl_tex_t fb_back, fb_front, fb_depth;
    int fb_w, fb_h, fb_double, fb_has_depth, fb_has_stencil, fb_alpha;
    int fb_samples;            /* framebuffer padrão multisample (kCGLPFASamples); fb_res recebe o resolve */
    msl_tex_t fb_res;
    gl_names_t vaos, fbos, queries, xfbs;
    gl_vao_t vao0, *vao;
    gl_fbo_t *draw_fbo, *read_fbo;
    GLenum def_draw_buf[GLX_DRAW], def_read_buf;
    gl_buf_t *b_array, *b_copy_r, *b_copy_w, *b_pack, *b_unpack, *b_ubo, *b_tbo, *b_xfb, *b_indirect;
    gl_buf_t *ubo[GLX_UBO]; i64 ubo_off[GLX_UBO], ubo_size[GLX_UBO];
    int unit;                  /* glActiveTexture */
    gl_tex_t *tex[GLX_UNITS][11];     /* por alvo (gl_tgt_index) */
    gl_tex_t *tex_default[11];
    gl_samp_t *samp[GLX_UNITS];
    gl_rb_t *rb;
    gl_prog_t *prog;
    gl_prog_t *arb_prog;       /* ARB_vertex/fragment_program do draw atual (gl_arbprog.c) */
    void *arb_state;
    gl_xfb_t xfb0, *xfb;
    gl_query_t *q_active[8];
    /* estado */
    u64 en[4];                 /* GL_BLEND etc.: bit por gl_cap_index */
    int vp[4]; double dr[2];
    int sc[4];
    float clear_c[4]; double clear_d; int clear_s;
    float clear_accum[4];
    GLenum bsrc_rgb[GLX_DRAW], bdst_rgb[GLX_DRAW], bsrc_a[GLX_DRAW], bdst_a[GLX_DRAW], beq_rgb[GLX_DRAW], beq_a[GLX_DRAW];
    int blend_on[GLX_DRAW];
    float bcolor[4];
    u8 cmask[GLX_DRAW][4];
    GLenum dfunc; int dmask;
    GLenum sfunc[2], sfail[2], szfail[2], szpass[2]; int sref[2]; u32 svmask[2], swmask[2];
    GLenum cull_mode, front_face, pmode[2];
    float poff_factor, poff_units, poff_clamp;
    struct gl_dbg *dbg;           /* KHR_debug: log, callback, grupos (gl_debug.c) */
    float line_w, point_s, point_min, point_max, point_fade, point_att[3];
    int ls_factor; u16 ls_pattern; u32 pstipple[32]; /* pontilhado: linha (fator, padrão) e polígono (bit x da linha y) */
    GLenum point_origin;
    GLenum logic_op;
    float samp_cov; int samp_inv;
    u32 samp_mask;             /* glSampleMaski(0, m) */
    GLenum provoke;
    u32 restart_idx;
    GLenum hint[8];
    int pk_align, pk_row, pk_skrow, pk_skpix, pk_imgh, pk_skimg, pk_swap, pk_lsb;
    int up_align, up_row, up_skrow, up_skpix, up_imgh, up_skimg, up_swap, up_lsb;
    float min_sample_shading;
    u32 patch_vertices;
    float patch_outer[4], patch_inner[2];   /* GL_PATCH_DEFAULT_OUTER/INNER_LEVEL (sem tessellation control shader) */
    /* fixed function */
    GLenum mmode;
    mstack_t mv, proj, texm[8], colm;
    gl_light_t light[GLX_LIGHTS];
    float lm_amb[4]; int lm_local, lm_two, lm_sep_spec;
    gl_mat_t mat[2];
    GLenum cm_face, cm_mode;
    GLenum shade;
    GLenum alpha_func; float alpha_ref;
    GLenum fog_mode, fog_src; float fog_d, fog_s, fog_e, fog_c[4], fog_index;
    gl_texenv_t te[8];
    double clip_plane[GLX_CLIP][4];  /* em coordenadas do olho */
    float cur_col[4], cur_col2[4], cur_nrm[3], cur_tc[8][4], cur_fog, cur_attr[GLX_ATTR][4];
    i32 cur_attri[GLX_ATTR][4];
    int cur_edge;
    float raster_pos[4]; int raster_valid; float raster_col[4], raster_tc[4];
    float pzoom[2];
    float pmap_scale[4], pmap_bias[4];
    /* glBegin/glEnd */
    int in_begin; GLenum begin_mode;
    gl_imv_t *imv; u32 nimv, capimv;
    /* arrays de cliente (compat) */
    int client_unit;
    /* listas de exibição */
    gl_dlist_t *dl_cur; GLenum dl_mode; u32 list_base;
    int dl_exec;
    /* atributos empilhados (glPushAttrib) */
    void *attr_stack[16]; GLbitfield attr_mask[16]; int attr_top;
    void *cattr_stack[16]; int cattr_top;
    /* seleção/feedback (compat) */
    GLenum render_mode;
    u32 *sel_buf; int sel_size, sel_n, sel_hits, sel_overflow; u32 name_stack[64]; int name_top; float sel_min, sel_max; int sel_hit;
    float *fb_buf; int fb_size, fb_n; GLenum fb_type;
    /* estágio montado para o draw */
    gl_stage_t st;
    gl_target_t tgt;
    u32 prim_counter;
    u64 samples_passed, prims_generated, prims_written;
    void *ffp_state;           /* gl_ffp.c: matrizes derivadas e texturas do draw */
    void *glsl_state;          /* gl_glsl.c */
    void *eval_state;          /* gl_eval.c: avaliadores */
    float *accum; int accum_w, accum_h;   /* buffer de acumulação (RGBA float) */
    /* dono (CGL) */
    void *cgl;
    char ext_str[2048];
    char **ext_list; int next;
} gl_ctx_t;

/* ---- utilidades */
gl_ctx_t *gl_cur(void);
void gl_err(gl_ctx_t *C, GLenum e);
/* depuração (gl_debug.c) */
struct gl_dbg;
void gl_debug_error(gl_ctx_t *C, GLenum e);
void gl_debug_free(gl_ctx_t *C);
int gl_debug_get(gl_ctx_t *C, GLenum p, long long *v);
int gl_debug_getp(gl_ctx_t *C, GLenum p, void **out);
#define GLC(ret) gl_ctx_t *C = gl_cur(); if (!C) return ret
#define GLERR(e, ret) do { gl_err(C, (e)); return ret; } while (0)

int gl_cap_index(GLenum cap, int *idx);
int gl_is_on(gl_ctx_t *C, GLenum cap);
int gl_tgt_index(GLenum target);
gl_tex_t *gl_tex_bound(gl_ctx_t *C, GLenum target);
gl_buf_t **gl_buf_slot(gl_ctx_t *C, GLenum target);

/* texturas e pixels (gl_obj.c) */
int gl_ifmt_to_mtl(GLenum ifmt, int *ok);
GLenum gl_ifmt_base(GLenum ifmt);
int gl_tex_complete(gl_ctx_t *C, gl_tex_t *t);
void gl_tex_sampler(gl_ctx_t *C, gl_tex_t *t, gl_samp_t *s, msl_samp_t *out);
void gl_tex_free(gl_tex_t *t);
gl_tex_t *gl_tex_new(u32 name, GLenum target);
void gl_unpack_pixels(gl_ctx_t *C, int w, int h, int d, GLenum format, GLenum type, const void *src, int dfmt, u8 *dst, int pack_state);
void gl_pack_pixels(gl_ctx_t *C, const msl_tex_t *t, int lvl, int layer, int x, int y, int w, int h, GLenum format, GLenum type, void *dst, int ybottom);
int gl_resolve_target(gl_ctx_t *C, int draw, gl_target_t *T);
void gl_fb_resize(gl_ctx_t *C, int w, int h);
void gl_ctx_set_samples(gl_ctx_t *C, int n);   /* CGL: framebuffer padrão com n amostras */
const msl_tex_t *gl_fb_resolved(gl_ctx_t *C, const msl_tex_t *src);   /* resolve do framebuffer padrão multisample */
const u8 *gl_buf_ptr(gl_ctx_t *C, gl_buf_t *b, const void *ptr_or_off, i64 need);

/* rasterizador (gl_raster.c) */
void gl_draw(gl_ctx_t *C, GLenum mode, i64 first, i64 count, GLenum itype, const void *indices, i64 inst, i64 binst, i64 bvert);
void gl_clear_target(gl_ctx_t *C, GLbitfield mask);
void gl_pixel_write(gl_ctx_t *C, gl_target_t *T, int x, int y, gl_frag_t *f);
int gl_stage_setup(gl_ctx_t *C);

/* fixed function (gl_ffp.c) */
void gl_ffp_init(gl_ctx_t *C);
void gl_vao_defaults(gl_vao_t *v);
void gl_ffp_stage(gl_ctx_t *C);
void gl_mat_mul(float *r, const float *a, const float *b);
void gl_mat_vec(float *r, const float *m, const float *v);
int gl_mat_inv(float *r, const float *m);
mstack_t *gl_cur_stack(gl_ctx_t *C);
int gl_dl_record(gl_ctx_t *C, int op, const void *args, int nbytes);

/* GLSL (gl_glsl.c) */
void gl_glsl_stage(gl_ctx_t *C);
void gl_prog_unref(gl_ctx_t *C, gl_prog_t *p);
void gl_shader_unref(gl_ctx_t *C, gl_shader_t *s);

/* contexto (gl_ctx.c), chamado pela CGL */
gl_ctx_t *gl_ctx_create(gl_ctx_t *share, int core, int major, int minor, int w, int h, int depth, int stencil, int dbl, int alpha);
void gl_ctx_destroy(gl_ctx_t *C);
void gl_ctx_make_current(gl_ctx_t *C);
void gl_ctx_swap(gl_ctx_t *C);
/* copia o buffer da frente (ou o de trás se single buffer) para BGRA8 com a linha 0 em cima */
void gl_ctx_read_bgra(gl_ctx_t *C, u8 *dst, u64 dbpr, int w, int h);

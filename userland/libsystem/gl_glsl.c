/*
 * mOS libSystem — OpenGL por software: shaders e programas GLSL.
 *   compilação: GLSL -> pré-processador (msl_preprocess) -> tradução (gl_glsl_tr.c) -> prelúdio
 *   (gl_glsl_pre.c) + texto -> compilador de MSL no modo GLSL (msl_compile2). O main vira função do
 *   interpretador; uniforms, in/out e gl_* são globais no gmem da biblioteca.
 *   link: atributos (layout, glBindAttribLocation ou automático), varyings casados por nome (blocos pelo
 *   nome do bloco) em slots de vec4, saídas do fragmento (layout, glBindFragDataLocation), uniforms
 *   achatados em locations (structs, arrays, samplers), blocos de uniform com layout std140, transform
 *   feedback, e as embutidas do perfil de compatibilidade (matrizes, luzes, material, fog...).
 *   execução: por vértice copia os atributos para o gmem, roda o main e lê gl_Position e as saídas; por
 *   fragmento escreve os varyings interpolados, roda e lê gl_FragColor/gl_FragData/saídas e gl_FragDepth.
 * Também: ARB_shader_objects, programas separáveis e pipelines (ARB_separate_shader_objects).
 */
#include "gl_glsl.h"
#include "gl_proto.h"

int snprintf(char *b, size_t n, const char *fmt, ...);
char *strrchr(const char *s, int c);
int gl_attr_fetch(gl_ctx_t *C, const gl_attr_t *a, u32 e, float out[4], i32 iout[4]);
void gl_ffp_fetch(gl_ctx_t *C, u32 e, int what, float out[4]);
void gl_imm_attr(gl_ctx_t *C, u32 e, int a, float out[4], i32 iout[4]);
int gl_ffp_imm(void);
msl_tex_t *gl_tex_mt(gl_ctx_t *C, gl_tex_t *t);
int gl_unit_on(gl_ctx_t *C, int unit, GLenum cap);

/* ================================================================ estado de um programa ligado */
typedef struct { const char *name; int off, size; ty_t *t; } gv_t;   /* global de um estágio */
typedef struct { char *name; int loc, n, off; ty_t *t; int base; GLenum gltype; int builtin; } pa_t;
#define NST 5                  /* estágios: 0 vértice, 1 fragmento, 2 geometria, 3 controle e 4 avaliação de tesselação */
typedef struct { char *name; int off[2], size, slot, nslots, interp, aux; } pv_t;
typedef struct { char *name; int loc, index, off, n; ty_t *t; int isint; } po_t;
typedef struct {
    char *name;
    GLenum gltype;
    ty_t *t;                   /* tipo do elemento */
    int arr, loc;
    int off[NST], stride;      /* deslocamento no gmem por estágio (-1 = ausente) */
    int sampler, target;       /* sampler: índice do alvo (gl_tgt_index) */
    int *units;
    int block, boff, bastride, bmstride;
    gl_buf_t *bbuf;            /* EXT_bindable_uniform */
} pu_t;
typedef struct { char *name; char *inst; ty_t *t; int off[NST], size140, binding, arr, rowmajor; int *mem; int nmem; } pb_t;
typedef struct { int stage, off, comps, skip; } px_t;
typedef struct { int src, dst, esize, arr; } tcopy_t;   /* saída do controle -> entrada da avaliação (arr: por vértice) */
typedef struct {
    msl_lib_t *lib[NST];
    msl_fn_t *main[NST];
    gl_shinfo_t info[NST];
    u8 *init[NST];
    gv_t *plain[NST]; int nplain[NST];
    int compat;
    /* embutidas */
    int b_pos, b_psize, b_clip, b_vid, b_iid, b_clipvert;
    int b_coord, b_front, b_pcoord, b_depth, b_pid, b_fcolor, b_fdata, b_sid, b_spos, b_smaskin, b_smask;
    int w_psize, w_clip, w_clipvert, w_depth, w_fcolor, w_fdata, discards, w_smask, per_sample;
    int c_vertex, c_normal, c_color, c_scolor, c_fog, c_mtc[8];
    int c_out[13], c_in[13];   /* slots do compat: deslocamentos no VS (saída) e no FS (entrada) */
    pa_t attr[GLX_ATTR * 2]; int nattr;
    pv_t var[GLX_VAR]; int nvar, nslots, slot0;
    po_t out[GLX_DRAW * 2]; int nout;
    pu_t *uni; int nuni, capuni, nloc;
    pb_t blk[GLX_UBO]; int nblk;
    px_t xfb[64]; int nxfb, xfb_inter, xfb_comps[64], nxfb_groups;
    msl_samp_t *sstore; int nsstore;
    int needs_compat_uniforms;
    char *src_user[NST];       /* texto traduzido (sem prelúdio), para achar o que o shader usa */
    /* geometry shader */
    pv_t gvar[GLX_VAR]; int ngvar;          /* vértice -> geometria: off[0] no VS, off[1] = base do array no GS */
    int gs_n, gs_cut, gs_pos, gs_psize, gs_pidout, gs_in_arr, gs_pidin, gs_invid, gs_max, gs_inv;
    GLenum gs_inp, gs_outp;
    /* tesselação */
    int ost;                   /* último estágio de vértice: 0 (VS) ou 4 (avaliação de tesselação) */
    int tess, tcs_n, tcw, tpoint;
    GLenum tmode, tspacing;
    int v_pos, v_psize, v_clip, vw_psize, vw_clip;   /* saídas do VS quando ele alimenta a tesselação */
    pv_t pvar[GLX_VAR]; int npvar;          /* VS -> controle (ou avaliação): off[0] no VS, off[1] = base do array de 32 */
    tcopy_t tcp[64]; int ntcp;             /* controle -> avaliação */
    int c_in_arr, c_out_arr, c_pvi, c_pid, c_inv, c_outer, c_inner;
    int e_in_arr, e_pvi, e_pid, e_coord, e_outer, e_inner;
} pg_t;

/* ================================================================ nomes compartilhados */
static void *obj_get(gl_ctx_t *C, GLuint n) { return gl_names_get(&C->sh->progs, n); }
static gl_shader_t *sh_get(gl_ctx_t *C, GLuint n) { int *k = obj_get(C, n); return k && *k == 1 ? (gl_shader_t *)k : 0; }
static gl_prog_t *pr_get(gl_ctx_t *C, GLuint n) { int *k = obj_get(C, n); return k && *k == 2 ? (gl_prog_t *)k : 0; }
static char *dupstr(const char *s) { return s ? strdup(s) : 0; }
static void set_log(char **dst, const char *s) { free(*dst); *dst = dupstr(s ? s : ""); }

/* ================================================================ compilação */
static int glsl_version(const char *s, int *compat) {
    int v = 110;
    *compat = 1;
    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0] == '*' && p[1] == '/')) p++; if (*p) p += 2; continue; }
        if (*p == '#') {
            const char *q = p + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (!strncmp(q, "version", 7)) {
                q += 7;
                v = (int)strtol(q, (char **)&q, 10);
                while (*q == ' ' || *q == '\t') q++;
                *compat = v < 140 || !strncmp(q, "compatibility", 13);
                if (!strncmp(q, "es", 2)) *compat = 0;
            }
        }
        break;
    }
    return v;
}
/* "#version"/"#extension" viram linha vazia (o pré-processador do MSL não conhece) */
static char *strip_directives(const char *s) {
    char *o = strdup(s);
    for (char *p = o; *p;) {
        char *bol = p;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#') {
            char *q = p + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (!strncmp(q, "version", 7) || !strncmp(q, "extension", 9) || !strncmp(q, "line", 4) || !strncmp(q, "pragma", 6))
                for (char *r = bol; *r && *r != '\n'; r++) *r = ' ';
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    return o;
}
static int count_lines(const char *s, u64 n) { int l = 0; for (u64 i = 0; i < n; i++) if (s[i] == '\n') l++; return l; }
static const char *DEFS_COMPAT =
    "GL_ARB_texture_rectangle=1\nGL_ARB_shader_texture_lod=1\nGL_EXT_gpu_shader4=1\nGL_EXT_texture_array=1\n"
    "GL_ARB_draw_buffers=1\nGL_ARB_explicit_attrib_location=1\nGL_EXT_frag_depth=1\nGL_compatibility_profile=1\n";
static const char *DEFS_CORE =
    "GL_ARB_texture_rectangle=1\nGL_ARB_explicit_attrib_location=1\nGL_ARB_separate_shader_objects=1\nGL_ARB_gpu_shader5=1\n"
    "GL_ARB_texture_gather=1\nGL_ARB_shader_bit_encoding=1\nGL_ARB_sample_shading=1\nGL_ARB_texture_query_lod=1\n"
    "GL_ARB_gpu_shader_fp64=1\nGL_ARB_texture_cube_map_array=1\nGL_core_profile=1\n";
/* compila uma ou mais fontes de um estágio; devolve a biblioteca ou 0 com o log */
static msl_lib_t *compile_stage(gl_ctx_t *C, int stage, const char *src, gl_shinfo_t *I, char **user, char **log) {
    (void)C;
    int compat;
    int ver = glsl_version(src, &compat);
    memset(I, 0, sizeof *I);
    I->stage = stage;
    I->version = ver;
    I->compat = compat;
    char *clean = strip_directives(src);
    char defs[1024];
    snprintf(defs, sizeof defs, "__VERSION__=%d\nGL_FRAGMENT_PRECISION_HIGH=1\n%s", ver, compat ? DEFS_COMPAT : DEFS_CORE);
    char err[512];
    char *pp = msl_preprocess(clean, strlen(clean), defs, err, sizeof err);
    free(clean);
    if (!pp) { char m[600]; snprintf(m, sizeof m, "ERROR: 0:%s\n", err); set_log(log, m); return 0; }
    char *tr = gl_glsl_translate(pp, I);
    free(pp);
    if (!tr) { set_log(log, "ERROR: sem memória\n"); return 0; }
    if (stage == 2) {
        /* geometry shader: EmitVertex copia as saídas para arrays de max_vertices posições */
        if (!I->gs_in_set || !I->gs_out_set) { set_log(log, "ERROR: 0:0: o geometry shader precisa de layout(...) in e layout(..., max_vertices = N) out\n"); free(tr); return 0; }
        if (I->gs_max <= 0 || I->gs_max > 1024) { set_log(log, "ERROR: 0:0: max_vertices fora de 1..1024\n"); free(tr); return 0; }
        u64 cap = 4096 + (u64)I->ndecl * 512, len = 0;
        char *ep = malloc(cap);
        if (!ep) { free(tr); return 0; }
        int M = I->gs_max;
#define EP(...) do { int _n = snprintf(ep + len, cap - len, __VA_ARGS__); if (_n > 0) len += (u64)_n < cap - len ? (u64)_n : cap - len - 1; } while (0)
        EP("\nint __gs_n; int __gs_cut[%d]; float4 __gs_pos[%d]; float __gs_psize[%d]; int __gs_pid[%d];\n", M + 1, M, M, M);
        for (int i = 0; i < I->ndecl; i++) {
            gl_decl_t *d = &I->decl[i];
            if (d->storage != GS_OUT || !d->type) continue;
            EP("%s __gs_%s[%d]%s;\n", d->type, d->name, M, d->dims ? d->dims : "");
        }
        EP("void EmitVertex() { if (__gs_n >= %d) return; __gs_pos[__gs_n] = gl_Position; __gs_psize[__gs_n] = gl_PointSize; __gs_pid[__gs_n] = gl_PrimitiveID;", M);
        for (int i = 0; i < I->ndecl; i++) {
            gl_decl_t *d = &I->decl[i];
            if (d->storage != GS_OUT || !d->type) continue;
            if (d->dims) { int n = (int)strtol(d->dims + 1, 0, 10); EP(" for (int __i = 0; __i < %d; __i++) __gs_%s[__gs_n][__i] = %s[__i];", n > 0 ? n : 1, d->name, d->name); }
            else EP(" __gs_%s[__gs_n] = %s;", d->name, d->name);
        }
        EP(" __gs_n++; }\nvoid EndPrimitive() { __gs_cut[__gs_n] = 1; }\n");
#undef EP
        char *nt = malloc(strlen(tr) + len + 1);
        if (!nt) { free(ep); free(tr); return 0; }
        memcpy(nt, tr, strlen(tr));
        memcpy(nt + strlen(tr), ep, len + 1);
        free(ep);
        free(tr);
        tr = nt;
    }
    u64 plen;
    const char *pre = gl_glsl_prelude(stage, compat, &plen);
    u64 tlen = strlen(tr);
    char *full = malloc(plen + tlen + 2);
    if (!full) { free(tr); set_log(log, "ERROR: sem memória\n"); return 0; }
    memcpy(full, pre, plen);
    full[plen] = '\n';
    memcpy(full + plen + 1, tr, tlen + 1);
    I->prelude_lines = count_lines(pre, plen) + 1;
    msl_lib_t *L = msl_compile2(full, plen + 1 + tlen, 0, err, sizeof err, MSL_FLAG_GLSL);
    free(full);
    if (!L) {
        /* a mensagem do compilador traz a linha do texto inteiro: desconta o prelúdio */
        char m[700];
        int line = 0;
        const char *p = strstr(err, "linha ");
        if (p) line = (int)strtol(p + 6, 0, 10);
        if (line > I->prelude_lines) snprintf(m, sizeof m, "ERROR: 0:%d: %s\n", line - I->prelude_lines, err);
        else snprintf(m, sizeof m, "ERROR: 0:0: %s\n", err);
        set_log(log, m);
        free(tr);
        return 0;
    }
    if (!msl_find(L, "main")) { set_log(log, "ERROR: 0:0: 'main' não foi definido\n"); msl_lib_free(L); free(tr); return 0; }
    *user = tr;
    set_log(log, "");
    return L;
}

/* ================================================================ shaders (API) */
GLuint glCreateShader(GLenum type) {
    GLC(0);
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER && type != GL_GEOMETRY_SHADER && type != GL_TESS_CONTROL_SHADER && type != GL_TESS_EVALUATION_SHADER) GLERR(GL_INVALID_ENUM, 0);
    gl_shader_t *s = calloc(1, sizeof *s);
    if (!s) GLERR(GL_OUT_OF_MEMORY, 0);
    s->kind = 1;
    s->type = type;
    s->refs = 1;
    s->name = gl_names_gen(&C->sh->progs);
    gl_names_set(&C->sh->progs, s->name, s);
    return s->name;
}
void gl_shader_unref(gl_ctx_t *C, gl_shader_t *s) {
    if (!s || --s->refs > 0) return;
    if (s->name && obj_get(C, s->name) == s) gl_names_del(&C->sh->progs, s->name);
    free(s->src);
    free(s->log);
    free(s);
}
void glDeleteShader(GLuint n) {
    GLC();
    if (!n) return;
    gl_shader_t *s = sh_get(C, n);
    if (!s) GLERR(obj_get(C, n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    if (s->deleted) return;
    s->deleted = 1;
    gl_shader_unref(C, s);
}
GLboolean glIsShader(GLuint n) { GLC(0); return sh_get(C, n) != 0; }
void glShaderSource(GLuint n, GLsizei count, const GLchar *const *str, const GLint *len) {
    GLC();
    gl_shader_t *s = sh_get(C, n);
    if (!s) GLERR(obj_get(C, n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    if (count < 0) GLERR(GL_INVALID_VALUE, );
    u64 total = 1;
    for (int i = 0; i < count; i++) total += (u64)(len && len[i] >= 0 ? len[i] : (str[i] ? (int)strlen(str[i]) : 0));
    char *b = malloc(total);
    if (!b) GLERR(GL_OUT_OF_MEMORY, );
    u64 o = 0;
    for (int i = 0; i < count; i++) {
        u64 l = (u64)(len && len[i] >= 0 ? len[i] : (str[i] ? (int)strlen(str[i]) : 0));
        if (str[i]) memcpy(b + o, str[i], l);
        o += l;
    }
    b[o] = 0;
    free(s->src);
    s->src = b;
}
void glCompileShader(GLuint n) {
    GLC();
    gl_shader_t *s = sh_get(C, n);
    if (!s) GLERR(obj_get(C, n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    s->compiled = 0;
    gl_shinfo_t I;
    char *user = 0;
    int st = s->type == GL_FRAGMENT_SHADER ? 1 : s->type == GL_GEOMETRY_SHADER ? 2 : s->type == GL_TESS_CONTROL_SHADER ? 3 : s->type == GL_TESS_EVALUATION_SHADER ? 4 : 0;
    msl_lib_t *L = compile_stage(C, st, s->src ? s->src : "", &I, &user, &s->log);
    if (L) { s->compiled = 1; msl_lib_free(L); }
    for (int i = 0; i < I.ndecl; i++) { free(I.decl[i].name); free(I.decl[i].block); free(I.decl[i].type); free(I.decl[i].dims); }
    free(I.decl);
    free(user);
}
void glGetShaderiv(GLuint n, GLenum p, GLint *v) {
    GLC();
    gl_shader_t *s = sh_get(C, n);
    if (!s) GLERR(obj_get(C, n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    switch (p) {
    case GL_SHADER_TYPE: *v = (GLint)s->type; return;
    case GL_DELETE_STATUS: *v = s->deleted; return;
    case GL_COMPILE_STATUS: *v = s->compiled; return;
    case GL_INFO_LOG_LENGTH: *v = s->log && s->log[0] ? (GLint)strlen(s->log) + 1 : 0; return;
    case GL_SHADER_SOURCE_LENGTH: *v = s->src ? (GLint)strlen(s->src) + 1 : 0; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
static void copy_str(const char *s, GLsizei bs, GLsizei *len, GLchar *out) {
    if (!s) s = "";
    GLsizei n = (GLsizei)strlen(s);
    if (bs <= 0) { if (len) *len = 0; return; }
    if (n > bs - 1) n = bs - 1;
    if (out) { memcpy(out, s, (size_t)n); out[n] = 0; }
    if (len) *len = n;
}
void glGetShaderInfoLog(GLuint n, GLsizei bs, GLsizei *len, GLchar *log) { GLC(); gl_shader_t *s = sh_get(C, n); if (!s) GLERR(GL_INVALID_VALUE, ); copy_str(s->log, bs, len, log); }
void glGetShaderSource(GLuint n, GLsizei bs, GLsizei *len, GLchar *src) { GLC(); gl_shader_t *s = sh_get(C, n); if (!s) GLERR(GL_INVALID_VALUE, ); copy_str(s->src, bs, len, src); }
void glGetShaderPrecisionFormat(GLenum st, GLenum pt, GLint *range, GLint *prec) {
    (void)st;
    int fl = pt == GL_LOW_FLOAT || pt == GL_MEDIUM_FLOAT || pt == GL_HIGH_FLOAT;
    if (range) { range[0] = fl ? 127 : 31; range[1] = fl ? 127 : 30; }
    if (prec) *prec = fl ? 23 : 0;
}
void glShaderBinary(GLsizei n, const GLuint *sh, GLenum fmt, const void *bin, GLsizei len) { (void)n; (void)sh; (void)fmt; (void)bin; (void)len; GLC(); GLERR(GL_INVALID_ENUM, ); }

/* ================================================================ programas (API) */
GLuint glCreateProgram(void) {
    GLC(0);
    gl_prog_t *p = calloc(1, sizeof *p);
    if (!p) GLERR(GL_OUT_OF_MEMORY, 0);
    p->kind = 2;
    p->refs = 1;
    p->xfb_mode = GL_INTERLEAVED_ATTRIBS;
    p->name = gl_names_gen(&C->sh->progs);
    gl_names_set(&C->sh->progs, p->name, p);
    return p->name;
}
static void pg_free(pg_t *g) {
    if (!g) return;
    for (int s = 0; s < NST; s++) {
        msl_lib_free(g->lib[s]);
        free(g->init[s]);
        free(g->plain[s]);
        free(g->src_user[s]);
        for (int i = 0; i < g->info[s].ndecl; i++) { free(g->info[s].decl[i].name); free(g->info[s].decl[i].block); free(g->info[s].decl[i].type); free(g->info[s].decl[i].dims); }
        free(g->info[s].decl);
    }
    for (int i = 0; i < g->nattr; i++) free(g->attr[i].name);
    for (int i = 0; i < g->nvar; i++) free(g->var[i].name);
    for (int i = 0; i < g->nout; i++) free(g->out[i].name);
    for (int i = 0; i < g->nuni; i++) { free(g->uni[i].name); free(g->uni[i].units); }
    free(g->uni);
    for (int i = 0; i < g->nblk; i++) { free(g->blk[i].name); free(g->blk[i].inst); free(g->blk[i].mem); }
    free(g->sstore);
    for (int i = 0; i < g->ngvar; i++) free(g->gvar[i].name);
    for (int i = 0; i < g->npvar; i++) free(g->pvar[i].name);
    free(g);
}
void gl_prog_unref(gl_ctx_t *C, gl_prog_t *p) {
    if (!p || --p->refs > 0) return;
    for (int i = 0; i < p->nsh; i++) gl_shader_unref(C, p->sh[i]);
    if (p->name && obj_get(C, p->name) == p) gl_names_del(&C->sh->progs, p->name);
    pg_free(p->priv);
    for (int i = 0; i < p->nabind; i++) free(p->abind_name[i]);
    for (int i = 0; i < p->nfbind; i++) free(p->fbind_name[i]);
    for (int i = 0; i < p->nxfb_names; i++) free(p->xfb_names[i]);
    free(p->xfb_names);
    free(p->log);
    free(p);
}
void glDeleteProgram(GLuint n) {
    GLC();
    if (!n) return;
    gl_prog_t *p = pr_get(C, n);
    if (!p) GLERR(obj_get(C, n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    if (p->deleted) return;
    p->deleted = 1;
    gl_prog_unref(C, p);
}
GLboolean glIsProgram(GLuint n) { GLC(0); return pr_get(C, n) != 0; }
void glAttachShader(GLuint pn, GLuint sn) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    gl_shader_t *s = sh_get(C, sn);
    if (!p || !s) GLERR(!obj_get(C, pn) || !obj_get(C, sn) ? GL_INVALID_VALUE : GL_INVALID_OPERATION, );
    for (int i = 0; i < p->nsh; i++) if (p->sh[i] == s) GLERR(GL_INVALID_OPERATION, );
    if (p->nsh >= 8) GLERR(GL_INVALID_OPERATION, );
    p->sh[p->nsh++] = s;
    s->refs++;
}
void glDetachShader(GLuint pn, GLuint sn) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    gl_shader_t *s = sh_get(C, sn);
    if (!p || !s) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < p->nsh; i++)
        if (p->sh[i] == s) {
            for (int j = i; j + 1 < p->nsh; j++) p->sh[j] = p->sh[j + 1];
            p->nsh--;
            gl_shader_unref(C, s);
            return;
        }
    GLERR(GL_INVALID_OPERATION, );
}
void glGetAttachedShaders(GLuint pn, GLsizei max, GLsizei *count, GLuint *out) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    int n = 0;
    for (int i = 0; i < p->nsh && n < max; i++) out[n++] = p->sh[i]->name;
    if (count) *count = n;
}
void glBindAttribLocation(GLuint pn, GLuint idx, const GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    if (idx >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    if (!strncmp(name, "gl_", 3)) GLERR(GL_INVALID_OPERATION, );
    for (int i = 0; i < p->nabind; i++) if (!strcmp(p->abind_name[i], name)) { p->abind_loc[i] = (int)idx; return; }
    if (p->nabind < 64) { p->abind_name[p->nabind] = strdup(name); p->abind_loc[p->nabind++] = (int)idx; }
}
void glBindFragDataLocationIndexed(GLuint pn, GLuint color, GLuint index, const GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    if (color >= GLX_DRAW || index > 1) GLERR(GL_INVALID_VALUE, );
    if (!strncmp(name, "gl_", 3)) GLERR(GL_INVALID_OPERATION, );
    for (int i = 0; i < p->nfbind; i++) if (!strcmp(p->fbind_name[i], name)) { p->fbind_loc[i] = (int)color; p->fbind_idx[i] = (int)index; return; }
    if (p->nfbind < 16) { p->fbind_name[p->nfbind] = strdup(name); p->fbind_loc[p->nfbind] = (int)color; p->fbind_idx[p->nfbind++] = (int)index; }
}
void glBindFragDataLocation(GLuint pn, GLuint color, const GLchar *name) { glBindFragDataLocationIndexed(pn, color, 0, name); }
void glBindFragDataLocationEXT(GLuint pn, GLuint color, const GLchar *name) { glBindFragDataLocation(pn, color, name); }
void glTransformFeedbackVaryings(GLuint pn, GLsizei n, const GLchar *const *names, GLenum mode) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    if (mode != GL_INTERLEAVED_ATTRIBS && mode != GL_SEPARATE_ATTRIBS) GLERR(GL_INVALID_ENUM, );
    if (mode == GL_SEPARATE_ATTRIBS && n > GLX_XFB) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < p->nxfb_names; i++) free(p->xfb_names[i]);
    free(p->xfb_names);
    p->xfb_names = calloc((size_t)(n > 0 ? n : 1), sizeof *p->xfb_names);
    for (int i = 0; i < n; i++) p->xfb_names[i] = strdup(names[i]);
    p->nxfb_names = n;
    p->xfb_mode = mode;
}
void glTransformFeedbackVaryingsEXT(GLuint pn, GLsizei n, const GLchar *const *names, GLenum mode) { glTransformFeedbackVaryings(pn, n, names, mode); }

/* ================================================================ link */
static gv_t find_glob(msl_lib_t *L, const char *name) {
    gv_t g = {name, -1, 0, 0};
    if (!L) return g;
    for (int i = L->nglob - 1; i >= 0; i--) {
        var_t *v = L->globs[i];
        if (v->where == 1 && !strcmp(v->name, name)) { g.off = v->off; g.size = v->t->size; g.t = v->t; return g; }
    }
    return g;
}
static int g_off(msl_lib_t *L, const char *name) { return find_glob(L, name).off; }
static int uses(const char *src, const char *name) {
    if (!src) return 0;
    int n = (int)strlen(name);
    for (const char *p = strstr(src, name); p; p = strstr(p + 1, name)) {
        int l = p == src || !((p[-1] >= 'a' && p[-1] <= 'z') || (p[-1] >= 'A' && p[-1] <= 'Z') || (p[-1] >= '0' && p[-1] <= '9') || p[-1] == '_');
        char c = p[n];
        int r = !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
        if (l && r) return 1;
    }
    return 0;
}
static int sampler_target(const char *sname) {
    const char *s = sname;
    if (*s == 'i' || *s == 'u') s++;
    if (strncmp(s, "sampler", 7)) return -1;
    s += 7;
    static const struct { const char *n; int t; } T[] = {
        {"1DArrayShadow", 5}, {"2DArrayShadow", 6}, {"CubeArrayShadow", 10}, {"2DRectShadow", 4}, {"1DShadow", 0},
        {"2DShadow", 1}, {"CubeShadow", 3}, {"2DMSArray", 9}, {"2DMS", 8}, {"1DArray", 5}, {"2DArray", 6},
        {"CubeArray", 10}, {"2DRect", 4}, {"Buffer", 7}, {"Cube", 3}, {"1D", 0}, {"2D", 1}, {"3D", 2}, {0, 0}};
    for (int i = 0; T[i].n; i++) if (!strcmp(s, T[i].n)) return T[i].t;
    return -1;
}
static GLenum sampler_gltype(const char *sname) {
    static const struct { const char *n; GLenum t; } T[] = {
        {"sampler1D", GL_SAMPLER_1D}, {"sampler2D", GL_SAMPLER_2D}, {"sampler3D", GL_SAMPLER_3D}, {"samplerCube", GL_SAMPLER_CUBE},
        {"sampler2DRect", GL_SAMPLER_2D_RECT}, {"sampler1DArray", GL_SAMPLER_1D_ARRAY}, {"sampler2DArray", GL_SAMPLER_2D_ARRAY},
        {"samplerCubeArray", GL_SAMPLER_CUBE_MAP_ARRAY}, {"samplerBuffer", GL_SAMPLER_BUFFER}, {"sampler2DMS", GL_SAMPLER_2D_MULTISAMPLE},
        {"sampler2DMSArray", GL_SAMPLER_2D_MULTISAMPLE_ARRAY}, {"sampler1DShadow", GL_SAMPLER_1D_SHADOW},
        {"sampler2DShadow", GL_SAMPLER_2D_SHADOW}, {"samplerCubeShadow", GL_SAMPLER_CUBE_SHADOW},
        {"sampler2DRectShadow", GL_SAMPLER_2D_RECT_SHADOW}, {"sampler1DArrayShadow", GL_SAMPLER_1D_ARRAY_SHADOW},
        {"sampler2DArrayShadow", GL_SAMPLER_2D_ARRAY_SHADOW}, {"samplerCubeArrayShadow", GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW},
        {"isampler1D", GL_INT_SAMPLER_1D}, {"isampler2D", GL_INT_SAMPLER_2D}, {"isampler3D", GL_INT_SAMPLER_3D},
        {"isamplerCube", GL_INT_SAMPLER_CUBE}, {"isampler2DRect", GL_INT_SAMPLER_2D_RECT}, {"isampler1DArray", GL_INT_SAMPLER_1D_ARRAY},
        {"isampler2DArray", GL_INT_SAMPLER_2D_ARRAY}, {"isamplerBuffer", GL_INT_SAMPLER_BUFFER}, {"isampler2DMS", GL_INT_SAMPLER_2D_MULTISAMPLE},
        {"isamplerCubeArray", GL_INT_SAMPLER_CUBE_MAP_ARRAY}, {"isampler2DMSArray", GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY},
        {"usampler1D", GL_UNSIGNED_INT_SAMPLER_1D}, {"usampler2D", GL_UNSIGNED_INT_SAMPLER_2D}, {"usampler3D", GL_UNSIGNED_INT_SAMPLER_3D},
        {"usamplerCube", GL_UNSIGNED_INT_SAMPLER_CUBE}, {"usampler2DRect", GL_UNSIGNED_INT_SAMPLER_2D_RECT},
        {"usampler1DArray", GL_UNSIGNED_INT_SAMPLER_1D_ARRAY}, {"usampler2DArray", GL_UNSIGNED_INT_SAMPLER_2D_ARRAY},
        {"usamplerBuffer", GL_UNSIGNED_INT_SAMPLER_BUFFER}, {"usampler2DMS", GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE},
        {"usamplerCubeArray", GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY}, {"usampler2DMSArray", GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY},
        {0, 0}};
    for (int i = 0; T[i].n; i++) if (!strcmp(sname, T[i].n)) return T[i].t;
    return 0;
}
static GLenum num_gltype(ty_t *t) {
    int k = ty_scal(t);
    if (t->k == K_MAT) {
        int c = t->n, r = t->e->n;
        static const GLenum M[3][3] = {{GL_FLOAT_MAT2, GL_FLOAT_MAT2x3, GL_FLOAT_MAT2x4}, {GL_FLOAT_MAT3x2, GL_FLOAT_MAT3, GL_FLOAT_MAT3x4}, {GL_FLOAT_MAT4x2, GL_FLOAT_MAT4x3, GL_FLOAT_MAT4}};
        return M[c - 2][r - 2];
    }
    int n = t->k == K_VEC ? t->n : 1;
    static const GLenum F[4] = {GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT_VEC4}, I[4] = {GL_INT, GL_INT_VEC2, GL_INT_VEC3, GL_INT_VEC4},
                        U[4] = {GL_UNSIGNED_INT, GL_UNSIGNED_INT_VEC2, GL_UNSIGNED_INT_VEC3, GL_UNSIGNED_INT_VEC4}, B[4] = {GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4};
    if (k == K_BOOL) return B[n - 1];
    if (k == K_UINT || k == K_UCHAR || k == K_USHORT || k == K_ULONG) return U[n - 1];
    if (IS_INTK(k)) return I[n - 1];
    return F[n - 1];
}
static int ty_locs(ty_t *t) { return t->k == K_MAT ? t->n : 1; }
/* ---- std140 */
static int s140_align(ty_t *t);
static int s140_size(ty_t *t);
static int rup(int v, int a) { return (v + a - 1) / a * a; }
static int s140_align(ty_t *t) {
    switch (t->k) {
    case K_VEC: return t->n == 2 ? 8 : 16;
    case K_MAT: return 16;
    case K_ARRAY: return rup(s140_align(t->e), 16);
    case K_STRUCT: { int a = 4; for (int i = 0; i < t->nf; i++) { int b = s140_align(t->f[i].t); if (b > a) a = b; } return rup(a, 16); }
    }
    return 4;
}
static int s140_stride(ty_t *e) { return rup(s140_size(e), rup(s140_align(e), 16)); }
static int s140_size(ty_t *t) {
    switch (t->k) {
    case K_VEC: return 4 * t->n;
    case K_MAT: return 16 * t->n;
    case K_ARRAY: return s140_stride(t->e) * t->n;
    case K_STRUCT: {
        int off = 0;
        for (int i = 0; i < t->nf; i++) { off = rup(off, s140_align(t->f[i].t)); off += s140_size(t->f[i].t); }
        return rup(off, s140_align(t));
    }
    }
    return 4;
}
static void copy140(ty_t *t, const u8 *src, i64 avail, u8 *dst, int rowmajor) {
    if (avail <= 0) return;
    switch (t->k) {
    case K_STRUCT: {
        int off = 0;
        for (int i = 0; i < t->nf; i++) {
            off = rup(off, s140_align(t->f[i].t));
            copy140(t->f[i].t, src + off, avail - off, dst + t->f[i].off, rowmajor);
            off += s140_size(t->f[i].t);
        }
        return;
    }
    case K_ARRAY: {
        int st = s140_stride(t->e);
        for (int i = 0; i < t->n; i++) copy140(t->e, src + (i64)i * st, avail - (i64)i * st, dst + (i64)i * t->e->size, rowmajor);
        return;
    }
    case K_MAT: {
        int cols = t->n, rows = t->e->n, cs = t->e->size;
        for (int c = 0; c < cols; c++)
            for (int r = 0; r < rows; r++) {
                i64 so = rowmajor ? (i64)r * 16 + c * 4 : (i64)c * 16 + r * 4;
                if (so + 4 > avail) continue;
                memcpy(dst + (i64)c * cs + r * 4, src + so, 4);
            }
        return;
    }
    }
    int n = t->k == K_VEC ? t->n : 1, k = ty_scal(t), es = scal_size(k);
    for (int i = 0; i < n; i++) {
        if (i * 4 + 4 > avail) return;
        if (k == K_BOOL) dst[i * es] = src[i * 4] || src[i * 4 + 1] || src[i * 4 + 2] || src[i * 4 + 3];
        else memcpy(dst + i * es, src + i * 4, 4);
    }
}
/* ---- uniforms: achata structs e arrays em folhas com nome completo */
static pu_t *uni_find(pg_t *g, const char *name) { for (int i = 0; i < g->nuni; i++) if (!strcmp(g->uni[i].name, name)) return &g->uni[i]; return 0; }
static void uni_leaf(pg_t *g, int stage, const char *name, ty_t *t, int off, int arr, int stride, int block, int boff, int bastride) {
    pu_t *u = uni_find(g, name);
    if (!u) {
        if (g->nuni == g->capuni) { g->capuni = g->capuni ? g->capuni * 2 : 32; g->uni = realloc(g->uni, (size_t)g->capuni * sizeof *g->uni); }
        u = &g->uni[g->nuni++];
        memset(u, 0, sizeof *u);
        u->name = strdup(name);
        u->t = t;
        u->arr = arr;
        u->stride = stride;
        for (int st = 0; st < NST; st++) u->off[st] = -1;
        u->loc = -1;
        u->block = block;
        u->boff = boff;
        u->bastride = bastride;
        u->bmstride = t->k == K_MAT ? 16 : 0;
        int si = t->k == K_STRUCT && t->name ? sampler_target(t->name) : -1;
        if (si >= 0) { u->sampler = 1; u->target = si; u->gltype = sampler_gltype(t->name); u->units = calloc((size_t)arr, sizeof(int)); }
        else u->gltype = num_gltype(t);
    }
    if (stage >= 0) u->off[stage] = off;
}
static void uni_flatten(pg_t *g, int stage, const char *name, ty_t *t, int off, int block, int boff) {
    if (t->k == K_ARRAY) {
        ty_t *e = t->e;
        int leafy = e->k != K_STRUCT && e->k != K_ARRAY;
        if (e->k == K_STRUCT && e->name && sampler_target(e->name) >= 0) leafy = 1;
        if (leafy) {
            char nm[256];
            snprintf(nm, sizeof nm, "%s[0]", name);
            uni_leaf(g, stage, nm, e, off, t->n, e->size, block, boff, block >= 0 ? s140_stride(e) : 0);
            return;
        }
        for (int i = 0; i < t->n; i++) {
            char nm[256];
            snprintf(nm, sizeof nm, "%s[%d]", name, i);
            uni_flatten(g, stage, nm, e, off + i * e->size, block, boff + (block >= 0 ? i * s140_stride(e) : 0));
        }
        return;
    }
    if (t->k == K_STRUCT && !(t->name && sampler_target(t->name) >= 0)) {
        int o140 = 0;
        for (int i = 0; i < t->nf; i++) {
            char nm[256];
            snprintf(nm, sizeof nm, "%s.%s", name, t->f[i].name);
            o140 = rup(o140, s140_align(t->f[i].t));
            uni_flatten(g, stage, nm, t->f[i].t, off + t->f[i].off, block, boff + o140);
            o140 += s140_size(t->f[i].t);
        }
        return;
    }
    uni_leaf(g, stage, name, t, off, 1, t->size, block, boff, 0);
}
static void plog(gl_prog_t *p, const char *fmt, const char *a) {
    char m[512];
    snprintf(m, sizeof m, fmt, a ? a : "");
    u64 old = p->log ? strlen(p->log) : 0;
    char *n = realloc(p->log, old + strlen(m) + 2);
    if (!n) return;
    memcpy(n + old, m, strlen(m) + 1);
    p->log = n;
}
static gl_decl_t *decl_find(gl_shinfo_t *I, int storage, const char *name, int byblock) {
    for (int i = 0; i < I->ndecl; i++) {
        gl_decl_t *d = &I->decl[i];
        if (d->storage != storage) continue;
        if (byblock ? (d->block && !strcmp(d->block, name)) : (!d->block && !strcmp(d->name, name))) return d;
    }
    return 0;
}
static const char *COMPAT_VS_OUT[13] = {"gl_FrontColor", "gl_FrontSecondaryColor", "gl_BackColor", "gl_BackSecondaryColor",
                                        "gl_TexCoord", 0, 0, 0, 0, 0, 0, 0, "gl_FogFragCoord"};
static int link_prog(gl_ctx_t *C, gl_prog_t *p) {
    (void)C;
    pg_t *g = calloc(1, sizeof *g);
    if (!g) return 0;
    /* fontes por estágio (vários shaders do mesmo estágio são concatenados; só um #version vale) */
    char *src[NST] = {0};
    int has[NST] = {0};
    char *log = 0;
#define LFAIL(...) do { plog(p, __VA_ARGS__); free(log); for (int k_ = 0; k_ < NST; k_++) free(src[k_]); pg_free(g); return 0; } while (0)
    for (int i = 0; i < p->nsh; i++) {
        gl_shader_t *s = p->sh[i];
        int st = s->type == GL_FRAGMENT_SHADER ? 1 : s->type == GL_VERTEX_SHADER ? 0 : s->type == GL_GEOMETRY_SHADER ? 2 :
                 s->type == GL_TESS_CONTROL_SHADER ? 3 : 4;
        if (!s->compiled) LFAIL("ERROR: shader anexado não compilou\n", 0);
        u64 a = src[st] ? strlen(src[st]) : 0, b = s->src ? strlen(s->src) : 0;
        char *n = realloc(src[st], a + b + 2);
        if (!n) LFAIL("ERROR: sem memória\n", 0);
        memcpy(n + a, s->src ? s->src : "", b);
        n[a + b] = '\n';
        n[a + b + 1] = 0;
        if (a) {
            /* o segundo #version vira linha vazia */
            for (char *q = strstr(n + a, "#version"); q; q = strstr(q + 1, "#version")) for (char *r = q; *r && *r != '\n'; r++) *r = ' ';
        }
        src[st] = n;
        has[st] = 1;
    }
    if (!has[0] && !has[1]) LFAIL("ERROR: nenhum shader anexado\n", 0);
    if (has[3] && !has[4]) LFAIL("ERROR: tessellation control shader sem tessellation evaluation shader\n", 0);
    if ((has[3] || has[4]) && !has[0]) LFAIL("ERROR: a tesselação precisa de um vertex shader\n", 0);
    /* sem vertex shader, o fixed function faz o vértice: o mOS exige os dois estágios no GLSL */
    if (!has[0]) src[0] = strdup("#version 120\nvoid main() { gl_Position = ftransform(); gl_FrontColor = gl_Color; gl_FrontSecondaryColor = gl_SecondaryColor;"
                                 " for (int i = 0; i < 8; i++) gl_TexCoord[i] = gl_TextureMatrix[i] * (i == 0 ? gl_MultiTexCoord0 : i == 1 ? gl_MultiTexCoord1 :"
                                 " i == 2 ? gl_MultiTexCoord2 : i == 3 ? gl_MultiTexCoord3 : i == 4 ? gl_MultiTexCoord4 : i == 5 ? gl_MultiTexCoord5 :"
                                 " i == 6 ? gl_MultiTexCoord6 : gl_MultiTexCoord7); gl_FogFragCoord = abs((gl_ModelViewMatrix * gl_Vertex).z); }\n");
    if (!has[1]) src[1] = strdup("#version 120\nvoid main() { gl_FragColor = gl_Color; }\n");
    has[0] = has[1] = 1;
    for (int st = 0; st < NST; st++) {
        if (!has[st]) continue;
        char *user = 0;
        g->lib[st] = compile_stage(C, st, src[st], &g->info[st], &user, &log);
        if (!g->lib[st]) LFAIL("%s", log);
        g->src_user[st] = user;
        g->main[st] = msl_find(g->lib[st], "main");
        g->init[st] = malloc((size_t)(g->lib[st]->gsize ? g->lib[st]->gsize : 1));
        if (g->init[st]) memcpy(g->init[st], g->lib[st]->gmem, (size_t)g->lib[st]->gsize);
    }
    /* último estágio que produz vértices antes do GS/raster: a avaliação de tesselação ou o vertex shader */
    int ost = has[4] ? 4 : 0;
    g->ost = ost;
    if (has[4]) {
        gl_shinfo_t *CI = has[3] ? &g->info[3] : 0, *EI = &g->info[4];
        if (!EI->tes_set) LFAIL("ERROR: o tessellation evaluation shader precisa de layout(triangles|quads|isolines) in\n", 0);
        if (CI && (CI->tcs_vertices <= 0 || CI->tcs_vertices > 32)) LFAIL("ERROR: o tessellation control shader precisa de layout(vertices = N) out com N em 1..32\n", 0);
        g->tess = 1;
        g->tcs_n = CI ? CI->tcs_vertices : 0;
        g->tmode = EI->tes_mode;
        g->tspacing = EI->tes_spacing ? EI->tes_spacing : GL_EQUAL;
        g->tcw = EI->tes_cw;
        g->tpoint = EI->tes_point;
        msl_lib_t *VLs = g->lib[0];
        g->v_pos = g_off(VLs, "gl_Position"); g->v_psize = g_off(VLs, "gl_PointSize"); g->v_clip = g_off(VLs, "gl_ClipDistance");
        int cst = has[3] ? 3 : 4;   /* quem lê as saídas do VS: o controle (ou a avaliação, sem controle) */
        msl_lib_t *CL = g->lib[cst];
        /* vértice -> primeiro estágio da tesselação: cada entrada por vértice é um array de 32 com o tipo da saída do VS */
        int pslot = 0;
        for (int i = 0; i < g->info[cst].ndecl; i++) {
            gl_decl_t *d = &g->info[cst].decl[i];
            if (d->storage != GS_IN || d->patch) continue;
            gl_decl_t *vd = d->block ? decl_find(&g->info[0], GS_OUT, d->block, 1) : decl_find(&g->info[0], GS_OUT, d->name, 0);
            if (!vd) LFAIL("ERROR: a tesselação lê '%s', que o vertex shader não escreve\n", d->block ? d->block : d->name);
            gv_t vv = find_glob(g->lib[0], vd->name), cg = find_glob(CL, d->name);
            if (vv.off < 0 || cg.off < 0) continue;
            if (cg.t->k != K_ARRAY || cg.t->e->size != vv.size) LFAIL("ERROR: tipos diferentes na entrada '%s' da tesselação\n", d->name);
            int ns = (vv.size + 15) / 16;
            if (pslot + ns > GLX_VAR) LFAIL("ERROR: varyings demais\n", 0);
            pv_t *pv = &g->pvar[g->npvar++];
            pv->name = strdup(d->block ? d->block : d->name);
            pv->off[0] = vv.off;
            pv->off[1] = cg.off;
            pv->size = vv.size;
            pv->slot = pslot;
            pv->nslots = ns;
            pslot += ns;
        }
        g->c_in_arr = g_off(CL, "gl_in");
        if (has[3]) {
            msl_lib_t *TL = g->lib[3];
            g->c_out_arr = g_off(TL, "gl_out"); g->c_pvi = g_off(TL, "gl_PatchVerticesIn"); g->c_pid = g_off(TL, "gl_PrimitiveID");
            g->c_inv = g_off(TL, "gl_InvocationID"); g->c_outer = g_off(TL, "gl_TessLevelOuter"); g->c_inner = g_off(TL, "gl_TessLevelInner");
            g->e_in_arr = g_off(g->lib[4], "gl_in");
            /* controle -> avaliação: saídas por vértice (arrays) e por patch, pelo nome */
            for (int i = 0; i < EI->ndecl; i++) {
                gl_decl_t *d = &EI->decl[i];
                if (d->storage != GS_IN) continue;
                gl_decl_t *cd = d->block ? decl_find(CI, GS_OUT, d->block, 1) : decl_find(CI, GS_OUT, d->name, 0);
                if (!cd) LFAIL("ERROR: o tessellation evaluation shader lê '%s', que o control shader não escreve\n", d->block ? d->block : d->name);
                if (cd->patch != d->patch) LFAIL("ERROR: '%s' é patch num estágio e por vértice no outro\n", d->name);
                gv_t cv = find_glob(TL, cd->name), ev = find_glob(g->lib[4], d->name);
                if (cv.off < 0 || ev.off < 0) continue;
                if (g->ntcp >= 64) LFAIL("ERROR: saídas de tesselação demais\n", 0);
                tcopy_t *tc = &g->tcp[g->ntcp++];
                tc->src = cv.off;
                tc->dst = ev.off;
                if (d->patch) {
                    if (cv.size != ev.size) LFAIL("ERROR: tipos diferentes no patch '%s'\n", d->name);
                    tc->esize = cv.size;
                    tc->arr = 0;
                } else {
                    if (cv.t->k != K_ARRAY || ev.t->k != K_ARRAY || cv.t->e->size != ev.t->e->size) LFAIL("ERROR: tipos diferentes na saída '%s' do control shader\n", d->name);
                    tc->esize = cv.t->e->size;
                    tc->arr = 1;
                }
            }
        }
        msl_lib_t *EL = g->lib[4];
        g->e_pvi = g_off(EL, "gl_PatchVerticesIn"); g->e_pid = g_off(EL, "gl_PrimitiveID"); g->e_coord = g_off(EL, "gl_TessCoord");
        g->e_outer = g_off(EL, "gl_TessLevelOuter"); g->e_inner = g_off(EL, "gl_TessLevelInner");
        if (!has[3]) g->e_in_arr = g->c_in_arr;
    }
    if (has[2]) {
        g->gs_n = g_off(g->lib[2], "__gs_n"); g->gs_cut = g_off(g->lib[2], "__gs_cut"); g->gs_pos = g_off(g->lib[2], "__gs_pos");
        g->gs_psize = g_off(g->lib[2], "__gs_psize"); g->gs_pidout = g_off(g->lib[2], "__gs_pid"); g->gs_in_arr = g_off(g->lib[2], "gl_in");
        g->gs_pidin = g_off(g->lib[2], "gl_PrimitiveIDIn"); g->gs_invid = g_off(g->lib[2], "gl_InvocationID");
        g->gs_max = g->info[2].gs_max;
        g->gs_inv = g->info[2].gs_inv > 0 ? g->info[2].gs_inv : 1;
        g->gs_inp = g->info[2].gs_in;
        g->gs_outp = g->info[2].gs_out;
        /* vértice (ou avaliação de tesselação) -> geometria: cada entrada do GS é um array de 6 com o tipo da saída */
        int gslot = 0;
        for (int i = 0; i < g->info[2].ndecl; i++) {
            gl_decl_t *d = &g->info[2].decl[i];
            if (d->storage != GS_IN) continue;
            gl_decl_t *vd = d->block ? decl_find(&g->info[ost], GS_OUT, d->block, 1) : decl_find(&g->info[ost], GS_OUT, d->name, 0);
            if (!vd) LFAIL("ERROR: o geometry shader lê '%s', que o estágio anterior não escreve\n", d->block ? d->block : d->name);
            gv_t vv = find_glob(g->lib[ost], vd->name), gg = find_glob(g->lib[2], d->name);
            if (vv.off < 0 || gg.off < 0) continue;
            if (gg.t->k != K_ARRAY || gg.t->e->size != vv.size) LFAIL("ERROR: tipos diferentes na entrada '%s' do geometry shader\n", d->name);
            int ns = (vv.size + 15) / 16;
            if (gslot + ns > GLX_VAR) LFAIL("ERROR: varyings demais\n", 0);
            pv_t *pv = &g->gvar[g->ngvar++];
            pv->name = strdup(d->block ? d->block : d->name);
            pv->off[0] = vv.off;
            pv->off[1] = gg.off;
            pv->size = vv.size;
            pv->slot = gslot;
            pv->nslots = ns;
            gslot += ns;
        }
        if (g->tess) {
            /* o primitivo de entrada do GS tem de ser o que a tesselação gera */
            GLenum want = g->tpoint ? GL_POINTS : g->tmode == GL_ISOLINES ? GL_LINES : GL_TRIANGLES;
            if (g->gs_inp != want) LFAIL("ERROR: o geometry shader não recebe o primitivo que a tesselação gera\n", 0);
        }
    }
#undef LFAIL
    free(log);
    for (int k = 0; k < NST; k++) free(src[k]);
    g->compat = g->info[0].compat || g->info[1].compat;
    msl_lib_t *VL = g->lib[0], *FL = g->lib[1], *OL = g->lib[g->ost];
    /* globais comuns que voltam ao valor inicial */
    for (int st = 0; st < NST; st++) {
        if (!g->lib[st]) continue;
        gl_shinfo_t *I = &g->info[st];
        g->plain[st] = calloc((size_t)(I->ndecl + 1), sizeof(gv_t));
        for (int i = 0; i < I->ndecl; i++)
            if (I->decl[i].storage == GS_PLAIN) { gv_t v = find_glob(g->lib[st], I->decl[i].name); if (v.off >= 0) g->plain[st][g->nplain[st]++] = v; }
    }
    /* embutidas */
    /* posição, tamanho de ponto e recorte vêm do último estágio de vértice (VS ou avaliação de tesselação) */
    g->b_pos = g_off(OL, "gl_Position"); g->b_psize = g_off(OL, "gl_PointSize"); g->b_clip = g_off(OL, "gl_ClipDistance");
    g->b_vid = g_off(VL, "gl_VertexID"); g->b_iid = g_off(VL, "gl_InstanceID"); g->b_clipvert = g_off(OL, "gl_ClipVertex");
    g->b_coord = g_off(FL, "gl_FragCoord"); g->b_front = g_off(FL, "gl_FrontFacing"); g->b_pcoord = g_off(FL, "gl_PointCoord");
    g->b_depth = g_off(FL, "gl_FragDepth"); g->b_pid = g_off(FL, "gl_PrimitiveID"); g->b_fcolor = g_off(FL, "gl_FragColor");
    g->b_fdata = g_off(FL, "gl_FragData");
    g->b_sid = g_off(FL, "gl_SampleID"); g->b_spos = g_off(FL, "gl_SamplePosition");
    g->b_smaskin = g_off(FL, "gl_SampleMaskIn"); g->b_smask = g_off(FL, "gl_SampleMask");
    g->w_smask = uses(g->src_user[1], "gl_SampleMask");
    g->per_sample = uses(g->src_user[1], "gl_SampleID") || uses(g->src_user[1], "gl_SamplePosition");
    g->w_psize = uses(g->src_user[g->ost], "gl_PointSize");
    g->w_clip = uses(g->src_user[g->ost], "gl_ClipDistance");
    g->w_clipvert = uses(g->src_user[g->ost], "gl_ClipVertex");
    if (g->tess) {
        g->vw_psize = uses(g->src_user[0], "gl_PointSize");
        g->vw_clip = uses(g->src_user[0], "gl_ClipDistance");
    }
    g->w_depth = uses(g->src_user[1], "gl_FragDepth");
    g->w_fcolor = uses(g->src_user[1], "gl_FragColor");
    g->w_fdata = uses(g->src_user[1], "gl_FragData");
    g->discards = uses(g->src_user[1], "discard_fragment");
    static const char *cattr[6] = {"gl_Vertex", "gl_Normal", "gl_Color", "gl_SecondaryColor", "gl_FogCoord", 0};
    int *cdst[5] = {&g->c_vertex, &g->c_normal, &g->c_color, &g->c_scolor, &g->c_fog};
    for (int i = 0; i < 5; i++) *cdst[i] = g->compat && uses(g->src_user[0], cattr[i]) ? g_off(VL, cattr[i]) : -1;
    for (int i = 0; i < 8; i++) { char nm[32]; snprintf(nm, sizeof nm, "gl_MultiTexCoord%d", i); g->c_mtc[i] = g->compat && uses(g->src_user[0], nm) ? g_off(VL, nm) : -1; }
    if (uses(g->src_user[0], "ftransform")) g->c_vertex = g_off(VL, "gl_Vertex");
    /* varyings do compat em slots fixos */
    for (int i = 0; i < 13; i++) g->c_out[i] = g->c_in[i] = -1;
    if (g->compat) {
        for (int i = 0; i < 4; i++) if (uses(g->src_user[0], COMPAT_VS_OUT[i])) g->c_out[i] = g_off(VL, COMPAT_VS_OUT[i]);
        int tc = uses(g->src_user[0], "gl_TexCoord") ? g_off(VL, "gl_TexCoord") : -1;
        for (int i = 0; i < 8; i++) g->c_out[4 + i] = tc >= 0 ? tc + 16 * i : -1;
        if (uses(g->src_user[0], "gl_FogFragCoord")) g->c_out[12] = g_off(VL, "gl_FogFragCoord");
        if (uses(g->src_user[1], "gl_Color")) g->c_in[0] = g_off(FL, "gl_Color");
        if (uses(g->src_user[1], "gl_SecondaryColor")) g->c_in[1] = g_off(FL, "gl_SecondaryColor");
        int ftc = uses(g->src_user[1], "gl_TexCoord") ? g_off(FL, "gl_TexCoord") : -1;
        for (int i = 0; i < 8; i++) g->c_in[4 + i] = ftc >= 0 ? ftc + 16 * i : -1;
        if (uses(g->src_user[1], "gl_FogFragCoord")) g->c_in[12] = g_off(FL, "gl_FogFragCoord");
        g->slot0 = 13;
    }
    if (g->lib[2] || g->tess) for (int i = 0; i < 13; i++) g->c_out[i] = g->c_in[i] = -1;   /* com GS ou tesselação os slots são todos do usuário */
    /* atributos */
    u32 used = 0;
    gl_shinfo_t *VI = &g->info[0], *FI = &g->info[1];
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < VI->ndecl; i++) {
            gl_decl_t *d = &VI->decl[i];
            if (d->storage != GS_IN || d->block) continue;
            gv_t v = find_glob(VL, d->name);
            if (v.off < 0) continue;
            ty_t *t = v.t;
            int arr = 1;
            if (t->k == K_ARRAY) { arr = t->n; t = t->e; }
            int n = ty_locs(t) * arr;
            int loc = d->loc;
            for (int k = 0; k < p->nabind; k++) if (!strcmp(p->abind_name[k], d->name)) loc = p->abind_loc[k];
            if ((loc >= 0) != (pass == 0)) continue;
            if (loc < 0) {
                int start = g->compat && g->c_vertex >= 0 ? 1 : 0;
                for (loc = start; loc + n <= GLX_ATTR; loc++) { u32 m = ((1u << n) - 1) << loc; if (!(used & m)) break; }
                if (loc + n > GLX_ATTR) { plog(p, "ERROR: atributos demais (%s)\n", d->name); pg_free(g); return 0; }
            }
            if (loc + n > GLX_ATTR) { plog(p, "ERROR: location de atributo inválida (%s)\n", d->name); pg_free(g); return 0; }
            used |= ((1u << n) - 1) << loc;
            pa_t *a = &g->attr[g->nattr++];
            a->name = strdup(d->name);
            a->loc = loc;
            a->n = n;
            a->off = v.off;
            a->t = t;
            a->base = ty_scal(t);
            a->gltype = num_gltype(t);
            if (arr > 1) a->gltype = num_gltype(t);
        }
    /* varyings */
    int slot = g->lib[2] || g->tess ? 0 : g->slot0;
    gl_shinfo_t *PI = g->lib[2] ? &g->info[2] : &g->info[g->ost];   /* estágio anterior ao fragmento */
    for (int i = 0; i < FI->ndecl; i++) {
        gl_decl_t *d = &FI->decl[i];
        if (d->storage != GS_IN) continue;
        gl_decl_t *vd = d->block ? decl_find(PI, GS_OUT, d->block, 1) : decl_find(PI, GS_OUT, d->name, 0);
        if (!vd) { plog(p, "ERROR: o fragment shader lê '%s', que o estágio anterior não escreve\n", d->block ? d->block : d->name); pg_free(g); return 0; }
        gv_t vv, fv = find_glob(FL, d->name);
        if (g->lib[2]) {
            /* do array de saída do GS: o elemento é do tipo da saída */
            char bn[160];
            snprintf(bn, sizeof bn, "__gs_%s", vd->name);
            vv = find_glob(g->lib[2], bn);
            if (vv.off >= 0 && vv.t->k == K_ARRAY) vv.size = vv.t->e->size;
        } else vv = find_glob(OL, vd->name);
        if (vv.off < 0 || fv.off < 0) continue;
        if (vv.size != fv.size) { plog(p, "ERROR: tipos diferentes no varying '%s'\n", d->name); pg_free(g); return 0; }
        int ns = (fv.size + 15) / 16;
        if (slot + ns > GLX_VAR) { plog(p, "ERROR: varyings demais\n", 0); pg_free(g); return 0; }
        pv_t *pv = &g->var[g->nvar++];
        pv->name = strdup(d->block ? d->block : d->name);
        pv->off[0] = vv.off;
        pv->off[1] = fv.off;
        pv->size = fv.size;
        pv->slot = slot;
        pv->nslots = ns;
        int k = fv.t->k == K_ARRAY ? ty_scal(fv.t->e->k == K_ARRAY ? fv.t->e->e : fv.t->e) : fv.t->k == K_STRUCT ? K_FLOAT : ty_scal(fv.t);
        pv->interp = IS_INTK(k) ? MSL_INTERP_FLAT : d->interp != MSL_INTERP_PERSP ? d->interp : vd->interp;
        pv->aux = d->aux ? d->aux : vd->aux;
        if (pv->aux == 2) g->per_sample = 1;   /* entrada 'sample': sombreamento por amostra */
        slot += ns;
    }
    g->nslots = slot;
    /* saídas do fragmento */
    int next = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < FI->ndecl; i++) {
            gl_decl_t *d = &FI->decl[i];
            if (d->storage != GS_OUT || d->block) continue;
            gv_t v = find_glob(FL, d->name);
            if (v.off < 0) continue;
            int loc = d->loc, idx = d->index < 0 ? 0 : d->index;
            for (int k = 0; k < p->nfbind; k++) if (!strcmp(p->fbind_name[k], d->name)) { loc = p->fbind_loc[k]; idx = p->fbind_idx[k]; }
            if ((loc >= 0) != (pass == 0)) continue;
            ty_t *t = v.t;
            int n = 1;
            if (t->k == K_ARRAY) { n = t->n; t = t->e; }
            if (loc < 0) { loc = next; }
            if (loc + n > GLX_DRAW) { plog(p, "ERROR: saída de fragmento fora dos draw buffers (%s)\n", d->name); pg_free(g); return 0; }
            if (loc + n > next) next = loc + n;
            po_t *o = &g->out[g->nout++];
            o->name = strdup(d->name);
            o->loc = loc;
            o->index = idx;
            o->off = v.off;
            o->n = n;
            o->t = t;
            o->isint = IS_INTK(ty_scal(t)) && ty_scal(t) != K_BOOL;
        }
    /* uniforms e blocos (de todos os estágios) */
    for (int st = 0; st < NST; st++) {
        if (!g->lib[st]) continue;
        gl_shinfo_t *I = &g->info[st];
        for (int i = 0; i < I->ndecl; i++) {
            gl_decl_t *d = &I->decl[i];
            if (d->storage != GS_UNIFORM) continue;
            gv_t v = find_glob(g->lib[st], d->name);
            if (v.off < 0) continue;
            if (d->block) {
                int bi = -1;
                for (int k = 0; k < g->nblk; k++) if (!strcmp(g->blk[k].name, d->block)) bi = k;
                if (bi < 0) {
                    if (g->nblk >= GLX_UBO) { plog(p, "ERROR: blocos de uniform demais\n", 0); pg_free(g); return 0; }
                    bi = g->nblk++;
                    pb_t *b = &g->blk[bi];
                    memset(b, 0, sizeof *b);
                    b->name = strdup(d->block);
                    b->inst = strdup(d->name);
                    b->t = v.t->k == K_ARRAY ? v.t->e : v.t;
                    for (int st = 0; st < NST; st++) b->off[st] = -1;
                    b->size140 = s140_size(b->t);
                    b->rowmajor = d->rowmajor;
                    b->arr = d->arr;
                    b->binding = 0;
                    /* membros como uniforms do bloco: "Bloco.membro" com instância, "membro" sem */
                    int first = g->nuni;
                    int o140 = 0;
                    ty_t *bt = b->t;
                    for (int f = 0; f < bt->nf; f++) {
                        char nm[256];
                        if (d->has_inst) snprintf(nm, sizeof nm, "%s.%s", d->block, bt->f[f].name);
                        else snprintf(nm, sizeof nm, "%s", bt->f[f].name);
                        o140 = rup(o140, s140_align(bt->f[f].t));
                        uni_flatten(g, -1, nm, bt->f[f].t, 0, bi, o140);
                        o140 += s140_size(bt->f[f].t);
                    }
                    b->nmem = g->nuni - first;
                    b->mem = calloc((size_t)(b->nmem + 1), sizeof(int));
                    for (int k = 0; k < b->nmem; k++) b->mem[k] = first + k;
                }
                g->blk[bi].off[st] = v.off;
                continue;
            }
            uni_flatten(g, st, d->name, v.t, v.off, -1, 0);
        }
    }
    /* locations: uniforms fora de bloco, na ordem */
    int loc = 0;
    for (int i = 0; i < g->nuni; i++) {
        pu_t *u = &g->uni[i];
        if (u->block >= 0) continue;
        u->loc = loc;
        loc += u->arr;
    }
    g->nloc = loc;
    int ns = 0;
    for (int i = 0; i < g->nuni; i++) if (g->uni[i].sampler) ns += g->uni[i].arr;
    g->sstore = calloc((size_t)(ns ? ns : 1), sizeof(msl_samp_t));
    g->nsstore = ns;
    g->needs_compat_uniforms = g->compat;
    /* transform feedback */
    g->xfb_inter = p->xfb_mode == GL_INTERLEAVED_ATTRIBS;
    for (int i = 0; i < p->nxfb_names && g->nxfb < 64; i++) {
        const char *nm = p->xfb_names[i];
        px_t *x = &g->xfb[g->nxfb];
        memset(x, 0, sizeof *x);
        if (!strncmp(nm, "gl_SkipComponents", 17)) { x->skip = 1; x->comps = (int)strtol(nm + 17, 0, 10); g->xfb_comps[g->nxfb++] = x->comps; continue; }
        if (!strcmp(nm, "gl_NextBuffer")) continue;
        char base[128];
        int el = -1;
        snprintf(base, sizeof base, "%s", nm);
        char *br = strchr(base, '[');
        if (br) { el = (int)strtol(br + 1, 0, 10); *br = 0; }
        gv_t v = find_glob(OL, base);
        if (v.off < 0) { plog(p, "ERROR: varying de transform feedback desconhecido: %s\n", nm); pg_free(g); return 0; }
        ty_t *t = v.t;
        int off = v.off;
        if (el >= 0 && t->k == K_ARRAY) { off += el * t->e->size; t = t->e; }
        int comps = t->k == K_ARRAY ? t->n * ty_ncomp(t->e) : ty_ncomp(t);
        x->off = off;
        x->comps = comps;
        g->xfb_comps[g->nxfb++] = comps;
    }
    p->nxfb = g->nxfb;
    pg_free(p->priv);
    p->priv = g;
    return 1;
}
void glLinkProgram(GLuint pn) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(obj_get(C, pn) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    if (C->xfb->active && C->prog == p) GLERR(GL_INVALID_OPERATION, );
    free(p->log);
    p->log = strdup("");
    p->linked = link_prog(C, p);
}
void glValidateProgram(GLuint pn) { GLC(); gl_prog_t *p = pr_get(C, pn); if (!p) GLERR(GL_INVALID_VALUE, ); p->validated = p->linked; }
void glUseProgram(GLuint pn) {
    GLC();
    if (C->xfb->active && !C->xfb->paused) GLERR(GL_INVALID_OPERATION, );
    gl_prog_t *p = pn ? pr_get(C, pn) : 0;
    if (pn && !p) GLERR(obj_get(C, pn) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    if (p && !p->linked) GLERR(GL_INVALID_OPERATION, );
    if (p) p->refs++;
    if (C->prog) gl_prog_unref(C, C->prog);
    C->prog = p;
}
void glGetProgramiv(GLuint pn, GLenum q, GLint *v) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(obj_get(C, pn) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    int mx = 0;
    switch (q) {
    case GL_DELETE_STATUS: *v = p->deleted; return;
    case GL_LINK_STATUS: *v = p->linked; return;
    case GL_VALIDATE_STATUS: *v = p->validated; return;
    case GL_INFO_LOG_LENGTH: *v = p->log && p->log[0] ? (GLint)strlen(p->log) + 1 : 0; return;
    case GL_ATTACHED_SHADERS: *v = p->nsh; return;
    case GL_ACTIVE_ATTRIBUTES: *v = g && p->linked ? g->nattr : 0; return;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: if (g) for (int i = 0; i < g->nattr; i++) if ((int)strlen(g->attr[i].name) + 1 > mx) mx = (int)strlen(g->attr[i].name) + 1; *v = mx; return;
    case GL_ACTIVE_UNIFORMS: *v = g && p->linked ? g->nuni : 0; return;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH: if (g) for (int i = 0; i < g->nuni; i++) if ((int)strlen(g->uni[i].name) + 1 > mx) mx = (int)strlen(g->uni[i].name) + 1; *v = mx; return;
    case GL_ACTIVE_UNIFORM_BLOCKS: *v = g && p->linked ? g->nblk : 0; return;
    case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH: if (g) for (int i = 0; i < g->nblk; i++) if ((int)strlen(g->blk[i].name) + 1 > mx) mx = (int)strlen(g->blk[i].name) + 1; *v = mx; return;
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE: *v = (GLint)p->xfb_mode; return;
    case GL_TRANSFORM_FEEDBACK_VARYINGS: *v = p->nxfb_names; return;
    case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH: for (int i = 0; i < p->nxfb_names; i++) if ((int)strlen(p->xfb_names[i]) + 1 > mx) mx = (int)strlen(p->xfb_names[i]) + 1; *v = mx; return;
    case GL_PROGRAM_SEPARABLE: *v = p->separable; return;
    case GL_PROGRAM_BINARY_LENGTH: *v = 0; return;
    case GL_PROGRAM_BINARY_RETRIEVABLE_HINT: *v = 0; return;
    case GL_GEOMETRY_VERTICES_OUT: if (!g || !g->lib[2]) GLERR(GL_INVALID_OPERATION, ); *v = g->gs_max; return;
    case GL_GEOMETRY_INPUT_TYPE: if (!g || !g->lib[2]) GLERR(GL_INVALID_OPERATION, ); *v = (GLint)g->gs_inp; return;
    case GL_GEOMETRY_OUTPUT_TYPE: if (!g || !g->lib[2]) GLERR(GL_INVALID_OPERATION, ); *v = (GLint)g->gs_outp; return;
    case GL_GEOMETRY_SHADER_INVOCATIONS: if (!g || !g->lib[2]) GLERR(GL_INVALID_OPERATION, ); *v = g->gs_inv; return;
    case GL_TESS_CONTROL_OUTPUT_VERTICES: if (!g || !g->lib[3]) GLERR(GL_INVALID_OPERATION, ); *v = g->tcs_n; return;
    case GL_TESS_GEN_MODE: if (!g || !g->tess) GLERR(GL_INVALID_OPERATION, ); *v = (GLint)g->tmode; return;
    case GL_TESS_GEN_SPACING: if (!g || !g->tess) GLERR(GL_INVALID_OPERATION, ); *v = (GLint)g->tspacing; return;
    case GL_TESS_GEN_VERTEX_ORDER: if (!g || !g->tess) GLERR(GL_INVALID_OPERATION, ); *v = g->tcw ? GL_CW : GL_CCW; return;
    case GL_TESS_GEN_POINT_MODE: if (!g || !g->tess) GLERR(GL_INVALID_OPERATION, ); *v = g->tpoint; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glGetProgramInfoLog(GLuint pn, GLsizei bs, GLsizei *len, GLchar *log) { GLC(); gl_prog_t *p = pr_get(C, pn); if (!p) GLERR(GL_INVALID_VALUE, ); copy_str(p->log, bs, len, log); }
void glProgramParameteri(GLuint pn, GLenum q, GLint v) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    if (q == GL_PROGRAM_SEPARABLE) p->separable = v != 0;
    else if (q != GL_PROGRAM_BINARY_RETRIEVABLE_HINT) GLERR(GL_INVALID_ENUM, );
}
void glGetProgramBinary(GLuint pn, GLsizei bs, GLsizei *len, GLenum *fmt, void *bin) { (void)pn; (void)bs; (void)fmt; (void)bin; if (len) *len = 0; GLC(); GLERR(GL_INVALID_OPERATION, ); }
void glProgramBinary(GLuint pn, GLenum fmt, const void *bin, GLsizei len) {
    (void)fmt; (void)bin; (void)len;
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    p->linked = 0;   /* nenhum formato binário: o app recompila do fonte */
    GLERR(GL_INVALID_ENUM, );
}

/* ---- atributos e saídas */
GLint glGetAttribLocation(GLuint pn, const GLchar *name) {
    GLC(-1);
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, -1);
    if (!p->linked) GLERR(GL_INVALID_OPERATION, -1);
    pg_t *g = p->priv;
    for (int i = 0; i < g->nattr; i++) if (!strcmp(g->attr[i].name, name)) return g->attr[i].loc;
    return -1;
}
void glGetActiveAttrib(GLuint pn, GLuint idx, GLsizei bs, GLsizei *len, GLint *size, GLenum *type, GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    if (!g || (int)idx >= g->nattr) GLERR(GL_INVALID_VALUE, );
    pa_t *a = &g->attr[idx];
    copy_str(a->name, bs, len, name);
    if (size) *size = a->n / ty_locs(a->t);
    if (type) *type = a->gltype;
}
GLint glGetFragDataLocation(GLuint pn, const GLchar *name) {
    GLC(-1);
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, -1);
    if (!p->linked) GLERR(GL_INVALID_OPERATION, -1);
    pg_t *g = p->priv;
    for (int i = 0; i < g->nout; i++) if (!strcmp(g->out[i].name, name)) return g->out[i].loc;
    return -1;
}
GLint glGetFragDataIndex(GLuint pn, const GLchar *name) {
    GLC(-1);
    gl_prog_t *p = pr_get(C, pn);
    if (!p || !p->linked) GLERR(GL_INVALID_OPERATION, -1);
    pg_t *g = p->priv;
    for (int i = 0; i < g->nout; i++) if (!strcmp(g->out[i].name, name)) return g->out[i].index;
    return -1;
}
GLint glGetFragDataLocationEXT(GLuint pn, const GLchar *name) { return glGetFragDataLocation(pn, name); }
void glGetTransformFeedbackVarying(GLuint pn, GLuint idx, GLsizei bs, GLsizei *len, GLsizei *size, GLenum *type, GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    if ((int)idx >= p->nxfb_names) GLERR(GL_INVALID_VALUE, );
    copy_str(p->xfb_names[idx], bs, len, name);
    pg_t *g = p->priv;
    if (size) *size = 1;
    if (type) {
        *type = GL_FLOAT;
        if (g) {
            gv_t v = find_glob(g->lib[g->ost], p->xfb_names[idx]);
            if (v.t) { ty_t *t = v.t->k == K_ARRAY ? v.t->e : v.t; *type = num_gltype(t); if (size && v.t->k == K_ARRAY) *size = v.t->n; }
        }
    }
}
void glGetTransformFeedbackVaryingEXT(GLuint pn, GLuint idx, GLsizei bs, GLsizei *len, GLsizei *size, GLenum *type, GLchar *name) { glGetTransformFeedbackVarying(pn, idx, bs, len, size, type, name); }
int gl_prog_xfb_layout(gl_ctx_t *C, int *interleaved, int *n, int *comps) {
    if (!C->prog || !C->prog->priv) return 0;
    pg_t *g = C->prog->priv;
    *interleaved = g->xfb_inter;
    *n = g->nxfb;
    for (int i = 0; i < g->nxfb; i++) comps[i] = g->xfb_comps[i];
    return g->nxfb > 0;
}

/* ================================================================ uniforms */
static pu_t *uni_at(pg_t *g, int loc, int *el) {
    for (int i = 0; i < g->nuni; i++) {
        pu_t *u = &g->uni[i];
        if (u->block < 0 && loc >= u->loc && loc < u->loc + u->arr) { *el = loc - u->loc; return u; }
    }
    return 0;
}
GLint glGetUniformLocation(GLuint pn, const GLchar *name) {
    GLC(-1);
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(obj_get(C, pn) ? GL_INVALID_OPERATION : GL_INVALID_VALUE, -1);
    if (!p->linked) GLERR(GL_INVALID_OPERATION, -1);
    pg_t *g = p->priv;
    /* "x", "x[0]" e "x[i]" */
    char base[256];
    snprintf(base, sizeof base, "%s", name);
    int el = 0;
    char *br = strrchr(base, '[');
    if (br && br[strlen(br) - 1] == ']') { el = (int)strtol(br + 1, 0, 10); }
    for (int i = 0; i < g->nuni; i++) {
        pu_t *u = &g->uni[i];
        if (u->block >= 0) continue;
        if (!strcmp(u->name, name)) return u->loc;
        u64 ln = strlen(u->name);
        if (u->arr >= 1 && ln > 3 && !strcmp(u->name + ln - 3, "[0]")) {
            if (strlen(name) == ln - 3 && !strncmp(u->name, name, ln - 3)) return u->loc;
            if (br && (u64)(br - base) == ln - 3 && !strncmp(u->name, base, ln - 3) && el >= 0 && el < u->arr) return u->loc + el;
        }
    }
    return -1;
}
static int base_kind(pu_t *u) { if (u->sampler) return 'i'; int k = ty_scal(u->t); return k == K_BOOL ? 'b' : (k == K_UINT || k == K_UCHAR || k == K_USHORT || k == K_ULONG) ? 'u' : IS_INTK(k) ? 'i' : 'f'; }
/* escreve count elementos a partir de el; v são números do tipo kind (f, i, u, d) */
static void uni_write(gl_ctx_t *C, gl_prog_t *p, int loc, int count, int kind, int comps, int cols, int transpose, const void *data) {
    if (!p || !p->linked) { gl_err(C, GL_INVALID_OPERATION); return; }
    if (loc == -1) return;
    pg_t *g = p->priv;
    int el;
    pu_t *u = uni_at(g, loc, &el);
    if (!u || count < 0) { gl_err(C, count < 0 ? GL_INVALID_VALUE : GL_INVALID_OPERATION); return; }
    ty_t *t = u->t;
    int ucols = t->k == K_MAT ? t->n : 1, urows = t->k == K_MAT ? t->e->n : t->k == K_VEC ? t->n : 1;
    if (u->sampler) urows = 1;
    if (cols != ucols || comps != urows) { gl_err(C, GL_INVALID_OPERATION); return; }
    int bk = base_kind(u);
    if (cols > 1 && kind != 'f' && kind != 'd') { gl_err(C, GL_INVALID_OPERATION); return; }
    if (bk == 'f' && kind != 'f' && kind != 'd') { gl_err(C, GL_INVALID_OPERATION); return; }
    if ((bk == 'i' || bk == 'u') && kind != bk && !(u->sampler && kind == 'i')) { gl_err(C, GL_INVALID_OPERATION); return; }
    if (count > 1 && u->arr == 1) { gl_err(C, GL_INVALID_OPERATION); return; }
    if (el + count > u->arr) count = u->arr - el;
    int per = cols * comps;
    for (int n = 0; n < count; n++) {
        double dv[16];
        i64 iv[16];
        for (int k = 0; k < per; k++) {
            int src = k;
            if (transpose && cols > 1) { int c = k / comps, r = k % comps; src = r * cols + c; }
            switch (kind) {
            case 'f': dv[k] = ((const float *)data)[n * per + src]; iv[k] = (i64)dv[k]; break;
            case 'd': dv[k] = ((const double *)data)[n * per + src]; iv[k] = (i64)dv[k]; break;
            case 'i': iv[k] = ((const i32 *)data)[n * per + src]; dv[k] = (double)iv[k]; break;
            default: iv[k] = ((const u32 *)data)[n * per + src]; dv[k] = (double)iv[k]; break;
            }
        }
        if (u->sampler) {
            if (iv[0] < 0 || iv[0] >= GLX_UNITS) { gl_err(C, GL_INVALID_VALUE); return; }
            u->units[el + n] = (int)iv[0];
            continue;
        }
        for (int st = 0; st < NST; st++) {
            if (u->off[st] < 0) continue;
            u8 *dst = g->lib[st]->gmem + u->off[st] + (i64)(el + n) * u->stride;
            int sk = ty_scal(t), es = scal_size(sk), cs = t->k == K_MAT ? t->e->size : 0;
            for (int k = 0; k < per; k++) {
                u8 *q = dst + (cols > 1 ? (i64)(k / comps) * cs + (k % comps) * es : (i64)k * es);
                switch (sk) {
                case K_FLOAT: { float f = (float)dv[k]; memcpy(q, &f, 4); break; }
                case K_HALF: { u16 h = msl_f2h((float)dv[k]); memcpy(q, &h, 2); break; }
                case K_BOOL: *q = kind == 'f' || kind == 'd' ? dv[k] != 0 : iv[k] != 0; break;
                case K_INT: case K_UINT: { i32 x = (i32)iv[k]; memcpy(q, &x, 4); break; }
                default: memcpy(q, &iv[k], (size_t)es);
                }
            }
        }
    }
}
static gl_prog_t *cur_prog(gl_ctx_t *C);
#define UF(N, T, K, CC) \
    void glUniform##N(GLint l, T x) { GLC(); T v[1] = {x}; uni_write(C, cur_prog(C), l, 1, K, 1, 1, 0, v); }
void glUniform1f(GLint l, GLfloat x) { GLC(); float v[1] = {x}; uni_write(C, cur_prog(C), l, 1, 'f', 1, 1, 0, v); }
void glUniform2f(GLint l, GLfloat x, GLfloat y) { GLC(); float v[2] = {x, y}; uni_write(C, cur_prog(C), l, 1, 'f', 2, 1, 0, v); }
void glUniform3f(GLint l, GLfloat x, GLfloat y, GLfloat z) { GLC(); float v[3] = {x, y, z}; uni_write(C, cur_prog(C), l, 1, 'f', 3, 1, 0, v); }
void glUniform4f(GLint l, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { GLC(); float v[4] = {x, y, z, w}; uni_write(C, cur_prog(C), l, 1, 'f', 4, 1, 0, v); }
void glUniform1i(GLint l, GLint x) { GLC(); i32 v[1] = {x}; uni_write(C, cur_prog(C), l, 1, 'i', 1, 1, 0, v); }
void glUniform2i(GLint l, GLint x, GLint y) { GLC(); i32 v[2] = {x, y}; uni_write(C, cur_prog(C), l, 1, 'i', 2, 1, 0, v); }
void glUniform3i(GLint l, GLint x, GLint y, GLint z) { GLC(); i32 v[3] = {x, y, z}; uni_write(C, cur_prog(C), l, 1, 'i', 3, 1, 0, v); }
void glUniform4i(GLint l, GLint x, GLint y, GLint z, GLint w) { GLC(); i32 v[4] = {x, y, z, w}; uni_write(C, cur_prog(C), l, 1, 'i', 4, 1, 0, v); }
void glUniform1ui(GLint l, GLuint x) { GLC(); u32 v[1] = {x}; uni_write(C, cur_prog(C), l, 1, 'u', 1, 1, 0, v); }
void glUniform2ui(GLint l, GLuint x, GLuint y) { GLC(); u32 v[2] = {x, y}; uni_write(C, cur_prog(C), l, 1, 'u', 2, 1, 0, v); }
void glUniform3ui(GLint l, GLuint x, GLuint y, GLuint z) { GLC(); u32 v[3] = {x, y, z}; uni_write(C, cur_prog(C), l, 1, 'u', 3, 1, 0, v); }
void glUniform4ui(GLint l, GLuint x, GLuint y, GLuint z, GLuint w) { GLC(); u32 v[4] = {x, y, z, w}; uni_write(C, cur_prog(C), l, 1, 'u', 4, 1, 0, v); }
void glUniform1d(GLint l, GLdouble x) { GLC(); double v[1] = {x}; uni_write(C, cur_prog(C), l, 1, 'd', 1, 1, 0, v); }
void glUniform2d(GLint l, GLdouble x, GLdouble y) { GLC(); double v[2] = {x, y}; uni_write(C, cur_prog(C), l, 1, 'd', 2, 1, 0, v); }
void glUniform3d(GLint l, GLdouble x, GLdouble y, GLdouble z) { GLC(); double v[3] = {x, y, z}; uni_write(C, cur_prog(C), l, 1, 'd', 3, 1, 0, v); }
void glUniform4d(GLint l, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { GLC(); double v[4] = {x, y, z, w}; uni_write(C, cur_prog(C), l, 1, 'd', 4, 1, 0, v); }
#define UV(N, T, K, CC) void glUniform##N(GLint l, GLsizei c, const T *v) { GLC(); uni_write(C, cur_prog(C), l, c, K, CC, 1, 0, v); }
UV(1fv, GLfloat, 'f', 1) UV(2fv, GLfloat, 'f', 2) UV(3fv, GLfloat, 'f', 3) UV(4fv, GLfloat, 'f', 4)
UV(1iv, GLint, 'i', 1) UV(2iv, GLint, 'i', 2) UV(3iv, GLint, 'i', 3) UV(4iv, GLint, 'i', 4)
UV(1uiv, GLuint, 'u', 1) UV(2uiv, GLuint, 'u', 2) UV(3uiv, GLuint, 'u', 3) UV(4uiv, GLuint, 'u', 4)
UV(1dv, GLdouble, 'd', 1) UV(2dv, GLdouble, 'd', 2) UV(3dv, GLdouble, 'd', 3) UV(4dv, GLdouble, 'd', 4)
#define UM(N, T, K, COLS, ROWS) void glUniformMatrix##N(GLint l, GLsizei c, GLboolean tr, const T *v) { GLC(); uni_write(C, cur_prog(C), l, c, K, ROWS, COLS, tr, v); }
UM(2fv, GLfloat, 'f', 2, 2) UM(3fv, GLfloat, 'f', 3, 3) UM(4fv, GLfloat, 'f', 4, 4) UM(2x3fv, GLfloat, 'f', 2, 3) UM(3x2fv, GLfloat, 'f', 3, 2)
UM(2x4fv, GLfloat, 'f', 2, 4) UM(4x2fv, GLfloat, 'f', 4, 2) UM(3x4fv, GLfloat, 'f', 3, 4) UM(4x3fv, GLfloat, 'f', 4, 3)
UM(2dv, GLdouble, 'd', 2, 2) UM(3dv, GLdouble, 'd', 3, 3) UM(4dv, GLdouble, 'd', 4, 4) UM(2x3dv, GLdouble, 'd', 2, 3) UM(3x2dv, GLdouble, 'd', 3, 2)
UM(2x4dv, GLdouble, 'd', 2, 4) UM(4x2dv, GLdouble, 'd', 4, 2) UM(3x4dv, GLdouble, 'd', 3, 4) UM(4x3dv, GLdouble, 'd', 4, 3)
/* glProgramUniform* (ARB_separate_shader_objects) */
#define PF(N, T, K, CC) void glProgramUniform##N(GLuint pn, GLint l, GLsizei c, const T *v) { GLC(); gl_prog_t *p = pr_get(C, pn); if (!p) GLERR(GL_INVALID_VALUE, ); uni_write(C, p, l, c, K, CC, 1, 0, v); }
PF(1fv, GLfloat, 'f', 1) PF(2fv, GLfloat, 'f', 2) PF(3fv, GLfloat, 'f', 3) PF(4fv, GLfloat, 'f', 4)
PF(1iv, GLint, 'i', 1) PF(2iv, GLint, 'i', 2) PF(3iv, GLint, 'i', 3) PF(4iv, GLint, 'i', 4)
PF(1uiv, GLuint, 'u', 1) PF(2uiv, GLuint, 'u', 2) PF(3uiv, GLuint, 'u', 3) PF(4uiv, GLuint, 'u', 4)
PF(1dv, GLdouble, 'd', 1) PF(2dv, GLdouble, 'd', 2) PF(3dv, GLdouble, 'd', 3) PF(4dv, GLdouble, 'd', 4)
#define PS(N, T, K, ...) void glProgramUniform##N(GLuint pn, GLint l, __VA_ARGS__)
#define PSB(K, T, ...) { GLC(); gl_prog_t *p = pr_get(C, pn); if (!p) GLERR(GL_INVALID_VALUE, ); T v[] = {__VA_ARGS__}; uni_write(C, p, l, 1, K, (int)(sizeof v / sizeof *v), 1, 0, v); }
PS(1f, GLfloat, 'f', GLfloat x) PSB('f', float, x)
PS(2f, GLfloat, 'f', GLfloat x, GLfloat y) PSB('f', float, x, y)
PS(3f, GLfloat, 'f', GLfloat x, GLfloat y, GLfloat z) PSB('f', float, x, y, z)
PS(4f, GLfloat, 'f', GLfloat x, GLfloat y, GLfloat z, GLfloat w) PSB('f', float, x, y, z, w)
PS(1i, GLint, 'i', GLint x) PSB('i', i32, x)
PS(2i, GLint, 'i', GLint x, GLint y) PSB('i', i32, x, y)
PS(3i, GLint, 'i', GLint x, GLint y, GLint z) PSB('i', i32, x, y, z)
PS(4i, GLint, 'i', GLint x, GLint y, GLint z, GLint w) PSB('i', i32, x, y, z, w)
PS(1ui, GLuint, 'u', GLuint x) PSB('u', u32, x)
PS(2ui, GLuint, 'u', GLuint x, GLuint y) PSB('u', u32, x, y)
PS(3ui, GLuint, 'u', GLuint x, GLuint y, GLuint z) PSB('u', u32, x, y, z)
PS(4ui, GLuint, 'u', GLuint x, GLuint y, GLuint z, GLuint w) PSB('u', u32, x, y, z, w)
PS(1d, GLdouble, 'd', GLdouble x) PSB('d', double, x)
PS(2d, GLdouble, 'd', GLdouble x, GLdouble y) PSB('d', double, x, y)
PS(3d, GLdouble, 'd', GLdouble x, GLdouble y, GLdouble z) PSB('d', double, x, y, z)
PS(4d, GLdouble, 'd', GLdouble x, GLdouble y, GLdouble z, GLdouble w) PSB('d', double, x, y, z, w)
#define PM(N, T, K, COLS, ROWS) void glProgramUniformMatrix##N(GLuint pn, GLint l, GLsizei c, GLboolean tr, const T *v) { GLC(); gl_prog_t *p = pr_get(C, pn); if (!p) GLERR(GL_INVALID_VALUE, ); uni_write(C, p, l, c, K, ROWS, COLS, tr, v); }
PM(2fv, GLfloat, 'f', 2, 2) PM(3fv, GLfloat, 'f', 3, 3) PM(4fv, GLfloat, 'f', 4, 4) PM(2x3fv, GLfloat, 'f', 2, 3) PM(3x2fv, GLfloat, 'f', 3, 2)
PM(2x4fv, GLfloat, 'f', 2, 4) PM(4x2fv, GLfloat, 'f', 4, 2) PM(3x4fv, GLfloat, 'f', 3, 4) PM(4x3fv, GLfloat, 'f', 4, 3)
PM(2dv, GLdouble, 'd', 2, 2) PM(3dv, GLdouble, 'd', 3, 3) PM(4dv, GLdouble, 'd', 4, 4) PM(2x3dv, GLdouble, 'd', 2, 3) PM(3x2dv, GLdouble, 'd', 3, 2)
PM(2x4dv, GLdouble, 'd', 2, 4) PM(4x2dv, GLdouble, 'd', 4, 2) PM(3x4dv, GLdouble, 'd', 3, 4) PM(4x3dv, GLdouble, 'd', 4, 3)
/* leitura */
static int uni_read(gl_ctx_t *C, GLuint pn, GLint loc, double *dv, i64 *iv, GLsizei max) {
    gl_prog_t *p = pr_get(C, pn);
    if (!p) { gl_err(C, GL_INVALID_VALUE); return 0; }
    if (!p->linked) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    pg_t *g = p->priv;
    int el;
    pu_t *u = uni_at(g, loc, &el);
    if (!u) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    if (u->sampler) { dv[0] = iv[0] = u->units[el]; return 1; }
    int st = 0;
    while (st < NST - 1 && u->off[st] < 0) st++;
    ty_t *t = u->t;
    int cols = t->k == K_MAT ? t->n : 1, rows = t->k == K_MAT ? t->e->n : t->k == K_VEC ? t->n : 1;
    int sk = ty_scal(t), es = scal_size(sk), cs = t->k == K_MAT ? t->e->size : 0;
    u8 *src = g->lib[st]->gmem + u->off[st] + (i64)el * u->stride;
    int n = 0;
    for (int c = 0; c < cols; c++)
        for (int r = 0; r < rows && n < max; r++, n++) {
            u8 *q = src + (cols > 1 ? (i64)c * cs + r * es : (i64)r * es);
            switch (sk) {
            case K_FLOAT: { float f; memcpy(&f, q, 4); dv[n] = f; iv[n] = (i64)f; break; }
            case K_BOOL: dv[n] = iv[n] = *q != 0; break;
            case K_UINT: { u32 x; memcpy(&x, q, 4); dv[n] = x; iv[n] = x; break; }
            default: { i32 x; memcpy(&x, q, 4); dv[n] = x; iv[n] = x; break; }
            }
        }
    return n;
}
void glGetUniformfv(GLuint p, GLint l, GLfloat *v) { GLC(); double d[16]; i64 i[16]; int n = uni_read(C, p, l, d, i, 16); for (int k = 0; k < n; k++) v[k] = (float)d[k]; }
void glGetUniformiv(GLuint p, GLint l, GLint *v) { GLC(); double d[16]; i64 i[16]; int n = uni_read(C, p, l, d, i, 16); for (int k = 0; k < n; k++) v[k] = (GLint)i[k]; }
void glGetUniformuiv(GLuint p, GLint l, GLuint *v) { GLC(); double d[16]; i64 i[16]; int n = uni_read(C, p, l, d, i, 16); for (int k = 0; k < n; k++) v[k] = (GLuint)i[k]; }
void glGetUniformdv(GLuint p, GLint l, GLdouble *v) { GLC(); double d[16]; i64 i[16]; int n = uni_read(C, p, l, d, i, 16); for (int k = 0; k < n; k++) v[k] = d[k]; }
void glGetActiveUniform(GLuint pn, GLuint idx, GLsizei bs, GLsizei *len, GLint *size, GLenum *type, GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    if (!g || (int)idx >= g->nuni) GLERR(GL_INVALID_VALUE, );
    pu_t *u = &g->uni[idx];
    copy_str(u->name, bs, len, name);
    if (size) *size = u->arr;
    if (type) *type = u->gltype;
}
void glGetActiveUniformName(GLuint pn, GLuint idx, GLsizei bs, GLsizei *len, GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    if (!g || (int)idx >= g->nuni) GLERR(GL_INVALID_VALUE, );
    copy_str(g->uni[idx].name, bs, len, name);
}
void glGetUniformIndices(GLuint pn, GLsizei n, const GLchar *const *names, GLuint *idx) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    for (int i = 0; i < n; i++) {
        idx[i] = GL_INVALID_INDEX;
        if (!g) continue;
        for (int k = 0; k < g->nuni; k++) {
            u64 ln = strlen(g->uni[k].name);
            if (!strcmp(g->uni[k].name, names[i]) || (ln > 3 && !strcmp(g->uni[k].name + ln - 3, "[0]") && strlen(names[i]) == ln - 3 && !strncmp(g->uni[k].name, names[i], ln - 3))) { idx[i] = (GLuint)k; break; }
        }
    }
}
void glGetActiveUniformsiv(GLuint pn, GLsizei n, const GLuint *idx, GLenum q, GLint *v) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    for (int i = 0; i < n; i++) {
        if (!g || (int)idx[i] >= g->nuni) GLERR(GL_INVALID_VALUE, );
        pu_t *u = &g->uni[idx[i]];
        switch (q) {
        case GL_UNIFORM_TYPE: v[i] = (GLint)u->gltype; break;
        case GL_UNIFORM_SIZE: v[i] = u->arr; break;
        case GL_UNIFORM_NAME_LENGTH: v[i] = (GLint)strlen(u->name) + 1; break;
        case GL_UNIFORM_BLOCK_INDEX: v[i] = u->block; break;
        case GL_UNIFORM_OFFSET: v[i] = u->block >= 0 ? u->boff : -1; break;
        case GL_UNIFORM_ARRAY_STRIDE: v[i] = u->block >= 0 && u->arr > 1 ? u->bastride : u->block >= 0 ? 0 : -1; break;
        case GL_UNIFORM_MATRIX_STRIDE: v[i] = u->block >= 0 && u->t->k == K_MAT ? 16 : u->block >= 0 ? 0 : -1; break;
        case GL_UNIFORM_IS_ROW_MAJOR: v[i] = u->block >= 0 && u->t->k == K_MAT ? g->blk[u->block].rowmajor : 0; break;
        default: GLERR(GL_INVALID_ENUM, );
        }
    }
}
GLuint glGetUniformBlockIndex(GLuint pn, const GLchar *name) {
    GLC(GL_INVALID_INDEX);
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, GL_INVALID_INDEX);
    pg_t *g = p->priv;
    if (g) for (int i = 0; i < g->nblk; i++) if (!strcmp(g->blk[i].name, name)) return (GLuint)i;
    return GL_INVALID_INDEX;
}
void glGetActiveUniformBlockName(GLuint pn, GLuint b, GLsizei bs, GLsizei *len, GLchar *name) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    if (!g || (int)b >= g->nblk) GLERR(GL_INVALID_VALUE, );
    copy_str(g->blk[b].name, bs, len, name);
}
void glGetActiveUniformBlockiv(GLuint pn, GLuint b, GLenum q, GLint *v) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    if (!g || (int)b >= g->nblk) GLERR(GL_INVALID_VALUE, );
    pb_t *B = &g->blk[b];
    switch (q) {
    case GL_UNIFORM_BLOCK_BINDING: *v = B->binding; return;
    case GL_UNIFORM_BLOCK_DATA_SIZE: *v = B->size140; return;
    case GL_UNIFORM_BLOCK_NAME_LENGTH: *v = (GLint)strlen(B->name) + 1; return;
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS: *v = B->nmem; return;
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: for (int i = 0; i < B->nmem; i++) v[i] = B->mem[i]; return;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: *v = B->off[0] >= 0; return;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: *v = B->off[1] >= 0; return;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER: *v = B->off[2] >= 0; return;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER: *v = B->off[3] >= 0; return;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER: *v = B->off[4] >= 0; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glUniformBlockBinding(GLuint pn, GLuint b, GLuint binding) {
    GLC();
    gl_prog_t *p = pr_get(C, pn);
    if (!p) GLERR(GL_INVALID_VALUE, );
    pg_t *g = p->priv;
    if (!g || (int)b >= g->nblk || binding >= GLX_UBO) GLERR(GL_INVALID_VALUE, );
    g->blk[b].binding = (int)binding;
}
/* subrotinas (ARB_shader_subroutine): o compilador não aceita a palavra subroutine, então não há nenhuma */
GLint glGetSubroutineUniformLocation(GLuint p, GLenum t, const GLchar *n) { (void)p; (void)t; (void)n; return -1; }
GLuint glGetSubroutineIndex(GLuint p, GLenum t, const GLchar *n) { (void)p; (void)t; (void)n; return GL_INVALID_INDEX; }
void glGetActiveSubroutineUniformiv(GLuint p, GLenum t, GLuint i, GLenum q, GLint *v) { (void)p; (void)t; (void)i; (void)q; (void)v; GLC(); GLERR(GL_INVALID_VALUE, ); }
void glGetActiveSubroutineUniformName(GLuint p, GLenum t, GLuint i, GLsizei bs, GLsizei *l, GLchar *n) { (void)p; (void)t; (void)i; (void)bs; (void)l; (void)n; GLC(); GLERR(GL_INVALID_VALUE, ); }
void glGetActiveSubroutineName(GLuint p, GLenum t, GLuint i, GLsizei bs, GLsizei *l, GLchar *n) { (void)p; (void)t; (void)i; (void)bs; (void)l; (void)n; GLC(); GLERR(GL_INVALID_VALUE, ); }
void glUniformSubroutinesuiv(GLenum t, GLsizei c, const GLuint *i) { (void)t; (void)i; GLC(); if (c) GLERR(GL_INVALID_VALUE, ); }
void glGetUniformSubroutineuiv(GLenum t, GLint l, GLuint *v) { (void)t; (void)l; (void)v; GLC(); GLERR(GL_INVALID_VALUE, ); }
void glGetProgramStageiv(GLuint p, GLenum t, GLenum q, GLint *v) { (void)p; (void)t; (void)q; if (v) *v = 0; }

/* ================================================================ pipelines (ARB_separate_shader_objects) */
typedef struct { u32 name; gl_prog_t *vs, *fs, *active; gl_prog_t combo; int dirty; char *log; } ppl_t;
static gl_names_t g_ppl;
gl_names_t *gl_ppl_names(void) { return &g_ppl; }
static ppl_t *g_ppl_bound;
void glGenProgramPipelines(GLsizei n, GLuint *out) { for (int i = 0; i < n; i++) { u32 k = gl_names_gen(&g_ppl); ppl_t *p = calloc(1, sizeof *p); p->name = k; gl_names_set(&g_ppl, k, p); out[i] = k; } }
void glDeleteProgramPipelines(GLsizei n, const GLuint *ids) {
    for (int i = 0; i < n; i++) {
        ppl_t *p = gl_names_get(&g_ppl, ids[i]);
        if (p == g_ppl_bound) g_ppl_bound = 0;
        gl_names_del(&g_ppl, ids[i]);
        if (p) { pg_free(p->combo.priv); free(p->log); free(p); }
    }
}
GLboolean glIsProgramPipeline(GLuint id) { return gl_names_get(&g_ppl, id) != 0; }
void glBindProgramPipeline(GLuint id) { g_ppl_bound = id ? gl_names_get(&g_ppl, id) : 0; }
void glUseProgramStages(GLuint id, GLbitfield stages, GLuint prog) {
    GLC();
    ppl_t *P = gl_names_get(&g_ppl, id);
    if (!P) GLERR(GL_INVALID_OPERATION, );
    gl_prog_t *p = prog ? pr_get(C, prog) : 0;
    if (prog && (!p || !p->linked || !p->separable)) GLERR(GL_INVALID_OPERATION, );
    if (stages & GL_VERTEX_SHADER_BIT) P->vs = p;
    if (stages & GL_FRAGMENT_SHADER_BIT) P->fs = p;
    P->dirty = 1;
}
void glActiveShaderProgram(GLuint id, GLuint prog) { GLC(); ppl_t *P = gl_names_get(&g_ppl, id); if (!P) GLERR(GL_INVALID_OPERATION, ); P->active = prog ? pr_get(C, prog) : 0; }
GLuint glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strs) {
    GLuint s = glCreateShader(type);
    if (!s) return 0;
    glShaderSource(s, count, strs, 0);
    glCompileShader(s);
    GLuint p = glCreateProgram();
    glProgramParameteri(p, GL_PROGRAM_SEPARABLE, 1);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (ok) { glAttachShader(p, s); glLinkProgram(p); glDetachShader(p, s); }
    else { GLC(p); gl_prog_t *pr = pr_get(C, p); gl_shader_t *sh = sh_get(C, s); if (pr && sh) set_log(&pr->log, sh->log); }
    glDeleteShader(s);
    return p;
}
void glGetProgramPipelineiv(GLuint id, GLenum q, GLint *v) {
    ppl_t *P = gl_names_get(&g_ppl, id);
    if (!P) return;
    switch (q) {
    case GL_ACTIVE_PROGRAM: *v = P->active ? (GLint)P->active->name : 0; return;
    case GL_VERTEX_SHADER: *v = P->vs ? (GLint)P->vs->name : 0; return;
    case GL_FRAGMENT_SHADER: *v = P->fs ? (GLint)P->fs->name : 0; return;
    case GL_GEOMETRY_SHADER: case GL_TESS_CONTROL_SHADER: case GL_TESS_EVALUATION_SHADER: *v = 0; return;
    case GL_VALIDATE_STATUS: *v = P->vs != 0; return;
    case GL_INFO_LOG_LENGTH: *v = P->log ? (GLint)strlen(P->log) + 1 : 0; return;
    }
}
void glValidateProgramPipeline(GLuint id) { (void)id; }
void glGetProgramPipelineInfoLog(GLuint id, GLsizei bs, GLsizei *len, GLchar *log) { ppl_t *P = gl_names_get(&g_ppl, id); copy_str(P ? P->log : "", bs, len, log); }
/* o pipeline junta o vértice de um programa e o fragmento de outro: liga de novo com as duas fontes */
static gl_prog_t *pipeline_prog(gl_ctx_t *C) {
    ppl_t *P = g_ppl_bound;
    if (!P || !P->vs) return 0;
    if (!P->dirty && P->combo.linked) return &P->combo;
    gl_prog_t *c = &P->combo;
    pg_free(c->priv);
    c->priv = 0;
    memset(c->sh, 0, sizeof c->sh);
    c->nsh = 0;
    c->kind = 2;
    gl_prog_t *srcs[2] = {P->vs, P->fs};
    for (int k = 0; k < 2; k++) {
        gl_prog_t *p = srcs[k];
        if (!p) continue;
        for (int i = 0; i < p->nsh && c->nsh < 8; i++) {
            int want = k == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
            if (p->sh[i]->type == (GLenum)want) c->sh[c->nsh++] = p->sh[i];
        }
    }
    c->xfb_mode = P->vs->xfb_mode;
    c->xfb_names = P->vs->xfb_names;
    c->nxfb_names = P->vs->nxfb_names;
    free(c->log);
    c->log = strdup("");
    c->linked = link_prog(C, c);
    free(P->log);
    P->log = strdup(c->log ? c->log : "");
    c->xfb_names = 0;
    c->nxfb_names = 0;
    P->dirty = 0;
    return c->linked ? c : 0;
}
static gl_prog_t *cur_prog(gl_ctx_t *C) {
    if (C->prog) return C->prog;
    if (g_ppl_bound && g_ppl_bound->active) return g_ppl_bound->active;
    return 0;
}

/* ---- ARB_shader_objects (GLhandleARB é ponteiro no macOS: o valor é o nome) */
#define H(x) ((GLhandleARB)(uintptr_t)(x))
#define N(x) ((GLuint)(uintptr_t)(x))
GLhandleARB glCreateShaderObjectARB(GLenum t) { return H(glCreateShader(t)); }
GLhandleARB glCreateProgramObjectARB(void) { return H(glCreateProgram()); }
void glShaderSourceARB(GLhandleARB s, GLsizei c, const GLcharARB **str, const GLint *len) { glShaderSource(N(s), c, (const GLchar *const *)str, len); }
void glCompileShaderARB(GLhandleARB s) { glCompileShader(N(s)); }
void glAttachObjectARB(GLhandleARB p, GLhandleARB s) { glAttachShader(N(p), N(s)); }
void glDetachObjectARB(GLhandleARB p, GLhandleARB s) { glDetachShader(N(p), N(s)); }
void glLinkProgramARB(GLhandleARB p) { glLinkProgram(N(p)); }
void glUseProgramObjectARB(GLhandleARB p) { glUseProgram(N(p)); }
void glValidateProgramARB(GLhandleARB p) { glValidateProgram(N(p)); }
void glDeleteObjectARB(GLhandleARB o) { GLC(); int *k = obj_get(C, N(o)); if (!k) return; if (*k == 1) glDeleteShader(N(o)); else glDeleteProgram(N(o)); }
GLhandleARB glGetHandleARB(GLenum p) { GLC(0); return p == GL_PROGRAM_OBJECT_ARB && C->prog ? H(C->prog->name) : 0; }
void glGetObjectParameterivARB(GLhandleARB o, GLenum p, GLint *v) {
    GLC();
    int *k = obj_get(C, N(o));
    if (!k) GLERR(GL_INVALID_VALUE, );
    if (p == GL_OBJECT_TYPE_ARB) { *v = *k == 1 ? GL_SHADER_OBJECT_ARB : GL_PROGRAM_OBJECT_ARB; return; }
    if (p == GL_OBJECT_SUBTYPE_ARB && *k == 1) { *v = (GLint)((gl_shader_t *)k)->type; return; }
    if (*k == 1) glGetShaderiv(N(o), p, v); else glGetProgramiv(N(o), p, v);
}
void glGetObjectParameterfvARB(GLhandleARB o, GLenum p, GLfloat *v) { GLint i = 0; glGetObjectParameterivARB(o, p, &i); *v = (float)i; }
void glGetInfoLogARB(GLhandleARB o, GLsizei bs, GLsizei *len, GLcharARB *log) { GLC(); int *k = obj_get(C, N(o)); if (!k) GLERR(GL_INVALID_VALUE, ); if (*k == 1) glGetShaderInfoLog(N(o), bs, len, log); else glGetProgramInfoLog(N(o), bs, len, log); }
void glGetAttachedObjectsARB(GLhandleARB p, GLsizei max, GLsizei *count, GLhandleARB *out) {
    GLuint tmp[8];
    GLsizei n = 0;
    glGetAttachedShaders(N(p), max < 8 ? max : 8, &n, tmp);
    for (int i = 0; i < n; i++) out[i] = H(tmp[i]);
    if (count) *count = n;
}
GLint glGetUniformLocationARB(GLhandleARB p, const GLcharARB *n) { return glGetUniformLocation(N(p), n); }
void glGetActiveUniformARB(GLhandleARB p, GLuint i, GLsizei bs, GLsizei *l, GLint *s, GLenum *t, GLcharARB *n) { glGetActiveUniform(N(p), i, bs, l, s, t, n); }
void glGetUniformfvARB(GLhandleARB p, GLint l, GLfloat *v) { glGetUniformfv(N(p), l, v); }
void glGetUniformivARB(GLhandleARB p, GLint l, GLint *v) { glGetUniformiv(N(p), l, v); }
void glGetShaderSourceARB(GLhandleARB s, GLsizei bs, GLsizei *l, GLcharARB *src) { glGetShaderSource(N(s), bs, l, src); }
void glBindAttribLocationARB(GLhandleARB p, GLuint i, const GLcharARB *n) { glBindAttribLocation(N(p), i, n); }
void glGetActiveAttribARB(GLhandleARB p, GLuint i, GLsizei bs, GLsizei *l, GLint *s, GLenum *t, GLcharARB *n) { glGetActiveAttrib(N(p), i, bs, l, s, t, n); }
GLint glGetAttribLocationARB(GLhandleARB p, const GLcharARB *n) { return glGetAttribLocation(N(p), n); }
void glUniform1fARB(GLint l, GLfloat x) { glUniform1f(l, x); }
void glUniform2fARB(GLint l, GLfloat x, GLfloat y) { glUniform2f(l, x, y); }
void glUniform3fARB(GLint l, GLfloat x, GLfloat y, GLfloat z) { glUniform3f(l, x, y, z); }
void glUniform4fARB(GLint l, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { glUniform4f(l, x, y, z, w); }
void glUniform1iARB(GLint l, GLint x) { glUniform1i(l, x); }
void glUniform2iARB(GLint l, GLint x, GLint y) { glUniform2i(l, x, y); }
void glUniform3iARB(GLint l, GLint x, GLint y, GLint z) { glUniform3i(l, x, y, z); }
void glUniform4iARB(GLint l, GLint x, GLint y, GLint z, GLint w) { glUniform4i(l, x, y, z, w); }
void glUniform1fvARB(GLint l, GLsizei c, const GLfloat *v) { glUniform1fv(l, c, v); }
void glUniform2fvARB(GLint l, GLsizei c, const GLfloat *v) { glUniform2fv(l, c, v); }
void glUniform3fvARB(GLint l, GLsizei c, const GLfloat *v) { glUniform3fv(l, c, v); }
void glUniform4fvARB(GLint l, GLsizei c, const GLfloat *v) { glUniform4fv(l, c, v); }
void glUniform1ivARB(GLint l, GLsizei c, const GLint *v) { glUniform1iv(l, c, v); }
void glUniform2ivARB(GLint l, GLsizei c, const GLint *v) { glUniform2iv(l, c, v); }
void glUniform3ivARB(GLint l, GLsizei c, const GLint *v) { glUniform3iv(l, c, v); }
void glUniform4ivARB(GLint l, GLsizei c, const GLint *v) { glUniform4iv(l, c, v); }
void glUniformMatrix2fvARB(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { glUniformMatrix2fv(l, c, t, v); }
void glUniformMatrix3fvARB(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { glUniformMatrix3fv(l, c, t, v); }
void glUniformMatrix4fvARB(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { glUniformMatrix4fv(l, c, t, v); }
void glUniform1uiEXT(GLint l, GLuint x) { glUniform1ui(l, x); }
void glUniform2uiEXT(GLint l, GLuint x, GLuint y) { glUniform2ui(l, x, y); }
void glUniform3uiEXT(GLint l, GLuint x, GLuint y, GLuint z) { glUniform3ui(l, x, y, z); }
void glUniform4uiEXT(GLint l, GLuint x, GLuint y, GLuint z, GLuint w) { glUniform4ui(l, x, y, z, w); }
void glUniform1uivEXT(GLint l, GLsizei c, const GLuint *v) { glUniform1uiv(l, c, v); }
void glUniform2uivEXT(GLint l, GLsizei c, const GLuint *v) { glUniform2uiv(l, c, v); }
void glUniform3uivEXT(GLint l, GLsizei c, const GLuint *v) { glUniform3uiv(l, c, v); }
void glUniform4uivEXT(GLint l, GLsizei c, const GLuint *v) { glUniform4uiv(l, c, v); }
void glGetUniformuivEXT(GLuint p, GLint l, GLuint *v) { glGetUniformuiv(p, l, v); }

/* ================================================================ execução */
static void put_vec(u8 *dst, ty_t *t, const float *f, const i32 *iv) {
    int n = t->k == K_VEC ? t->n : 1, k = ty_scal(t), es = scal_size(k);
    for (int i = 0; i < n; i++) {
        u8 *q = dst + i * es;
        switch (k) {
        case K_FLOAT: memcpy(q, &f[i], 4); break;
        case K_HALF: { u16 h = msl_f2h(f[i]); memcpy(q, &h, 2); break; }
        case K_BOOL: *q = f[i] != 0; break;
        default: memcpy(q, &iv[i], (size_t)(es < 4 ? es : 4));
        }
    }
}
static void put_f4(msl_lib_t *L, int off, const float *v, int n) { if (off >= 0) memcpy(L->gmem + off, v, (size_t)n * 4); }
typedef struct { gl_prog_t *p; pg_t *g; } run_t;
static void vtx_out(gl_ctx_t *C, pg_t *g, msl_lib_t *L, gl_vtx_t *o);
static run_t g_run;   /* programa do draw atual (o raster é síncrono) */
static void glsl_vs(gl_ctx_t *C, u32 e, u32 iid, gl_vtx_t *o) {
    pg_t *g = g_run.g;
    msl_lib_t *L = g->lib[0];
    for (int i = 0; i < g->nplain[0]; i++) memcpy(L->gmem + g->plain[0][i].off, g->init[0] + g->plain[0][i].off, (size_t)g->plain[0][i].size);
    int imm = gl_ffp_imm();
    for (int i = 0; i < g->nattr; i++) {
        pa_t *a = &g->attr[i];
        int cols = ty_locs(a->t), cs = a->t->k == K_MAT ? a->t->e->size : 0;
        ty_t *ct = a->t->k == K_MAT ? a->t->e : a->t;
        for (int k = 0; k < a->n; k++) {
            int loc = a->loc + k;
            float f[4];
            i32 iv[4];
            gl_attr_t *at = &C->vao->a[loc];
            if (imm) gl_imm_attr(C, e, loc, f, iv);
            else if (at->enabled) {
                u32 el = at->divisor ? iid / at->divisor : e;
                if (!gl_attr_fetch(C, at, el, f, iv)) { f[0] = f[1] = f[2] = 0; f[3] = 1; iv[0] = iv[1] = iv[2] = 0; iv[3] = 1; }
            } else { memcpy(f, C->cur_attr[loc], 16); memcpy(iv, C->cur_attri[loc], 16); }
            int arr_el = k / cols, col = k % cols;
            put_vec(L->gmem + a->off + (i64)arr_el * a->t->size + (i64)col * cs, ct, f, iv);
        }
    }
    if (g->c_vertex >= 0) { float v[4]; gl_ffp_fetch(C, e, 0, v); put_f4(L, g->c_vertex, v, 4); }
    if (g->c_normal >= 0) { float v[4]; gl_ffp_fetch(C, e, 1, v); put_f4(L, g->c_normal, v, 3); }
    if (g->c_color >= 0) { float v[4]; gl_ffp_fetch(C, e, 2, v); put_f4(L, g->c_color, v, 4); }
    if (g->c_scolor >= 0) { float v[4]; gl_ffp_fetch(C, e, 3, v); put_f4(L, g->c_scolor, v, 4); }
    if (g->c_fog >= 0) { float v[4]; gl_ffp_fetch(C, e, 4, v); put_f4(L, g->c_fog, v, 1); }
    for (int u = 0; u < 8; u++) if (g->c_mtc[u] >= 0) { float v[4]; gl_ffp_fetch(C, e, 5 + u, v); put_f4(L, g->c_mtc[u], v, 4); }
    if (g->b_vid >= 0) { i32 v = (i32)e; memcpy(L->gmem + g->b_vid, &v, 4); }
    if (g->b_iid >= 0) { i32 v = (i32)iid; memcpy(L->gmem + g->b_iid, &v, 4); }
    msl_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.vid = e;
    ctx.iid = iid;
    u8 dummy[64];
    msl_run(g->main[0], &ctx, dummy);
    if (g->tess) {
        /* o VS alimenta a tesselação: posição, tamanho de ponto e recorte crus, e as saídas nos slots do patch */
        memcpy(o->clip, L->gmem + g->v_pos, 16);
        o->psize = C->point_s;
        if (g->vw_psize && g->v_psize >= 0) memcpy(&o->psize, L->gmem + g->v_psize, 4);
        for (int i = 0; i < GLX_CLIP; i++) o->cd[i] = 1;
        if (g->vw_clip && g->v_clip >= 0) memcpy(o->cd, L->gmem + g->v_clip, 4 * GLX_CLIP);
        for (int i = 0; i < g->npvar; i++) { pv_t *v = &g->pvar[i]; memcpy(o->v[v->slot], L->gmem + v->off[0], (size_t)v->size); }
        o->nxfb = 0;
        return;
    }
    vtx_out(C, g, L, o);
}
/* saídas do último estágio de vértice (VS ou avaliação de tesselação) para o gl_vtx_t do raster/GS */
static void vtx_out(gl_ctx_t *C, pg_t *g, msl_lib_t *L, gl_vtx_t *o) {
    memcpy(o->clip, L->gmem + g->b_pos, 16);
    o->psize = C->point_s;
    if (g->w_psize && (gl_is_on(C, GL_PROGRAM_POINT_SIZE) || gl_is_on(C, GL_VERTEX_PROGRAM_POINT_SIZE))) memcpy(&o->psize, L->gmem + g->b_psize, 4);
    for (int i = 0; i < GLX_CLIP; i++) o->cd[i] = 1;
    if (g->w_clip) memcpy(o->cd, L->gmem + g->b_clip, 4 * GLX_CLIP);
    else if (g->w_clipvert) {
        float cv[4];
        memcpy(cv, L->gmem + g->b_clipvert, 16);
        for (int i = 0; i < GLX_CLIP; i++) o->cd[i] = (float)(C->clip_plane[i][0] * cv[0] + C->clip_plane[i][1] * cv[1] + C->clip_plane[i][2] * cv[2] + C->clip_plane[i][3] * cv[3]);
    }
    for (int i = 0; i < 13; i++) if (g->c_out[i] >= 0) memcpy(o->v[i], L->gmem + g->c_out[i], i == 12 ? 4 : 16);
    if (g->lib[2]) for (int i = 0; i < g->ngvar; i++) { pv_t *v = &g->gvar[i]; memcpy(o->v[v->slot], L->gmem + v->off[0], (size_t)v->size); }
    else for (int i = 0; i < g->nvar; i++) {
        pv_t *v = &g->var[i];
        memcpy(o->v[v->slot], L->gmem + v->off[0], (size_t)v->size);
    }
    if (g->nxfb && C->xfb->active && !C->xfb->paused) {
        int n = 0;
        for (int i = 0; i < g->nxfb && n < 64; i++) {
            px_t *x = &g->xfb[i];
            for (int k = 0; k < x->comps && n < 64; k++, n++) {
                if (x->skip) { o->xfb[n] = 0; continue; }
                /* vec3 ocupa 16 bytes no gmem; os componentes saem em sequência */
                memcpy(&o->xfb[n], L->gmem + x->off + 4 * k, 4);
            }
        }
        o->nxfb = n;
    }
}
/* entradas do fragmento: o próprio pixel (nb 0) ou o vizinho x+1 (1) / y+1 (2) pelas derivadas do raster */
static void fs_load(gl_ctx_t *C, pg_t *g, gl_frag_t *f, int nb) {
    msl_lib_t *L = g->lib[1];
    for (int i = 0; i < g->nplain[1]; i++) memcpy(L->gmem + g->plain[1][i].off, g->init[1] + g->plain[1][i].off, (size_t)g->plain[1][i].size);
    float tmp[GLX_VAR][4];
    const float (*src)[4] = (const float (*)[4])f->v;
    if (nb) {
        const float (*d)[4] = nb == 1 ? (const float (*)[4])f->dx : (const float (*)[4])f->dy;
        for (int s = 0; s < g->nslots && s < GLX_VAR; s++)
            for (int k = 0; k < 4; k++) tmp[s][k] = C->st.interp[s] == MSL_INTERP_FLAT ? f->v[s][k] : f->v[s][k] + d[s][k];
        src = (const float (*)[4])tmp;
    }
    for (int i = 0; i < g->nvar; i++) { pv_t *v = &g->var[i]; memcpy(L->gmem + v->off[1], src[v->slot], (size_t)v->size); }
    for (int i = 0; i < 13; i++) if (g->c_in[i] >= 0) memcpy(L->gmem + g->c_in[i], src[i], i == 12 ? 4 : 16);
    if (g->b_coord >= 0) {
        float c[4] = {f->coord[0] + (nb == 1), f->coord[1] + (nb == 2), f->coord[2], f->coord[3]};
        if (g->info[1].origin_ul) c[1] = (float)C->tgt.h - c[1];
        if (g->info[1].pixel_int) { c[0] -= 0.5f; c[1] -= 0.5f; }
        memcpy(L->gmem + g->b_coord, c, 16);
    }
    if (g->b_front >= 0) L->gmem[g->b_front] = (u8)f->front;
    if (g->b_pcoord >= 0) memcpy(L->gmem + g->b_pcoord, f->pcoord, 8);
    if (g->b_pid >= 0) { i32 v = (i32)f->prim_id; memcpy(L->gmem + g->b_pid, &v, 4); }
    if (g->b_depth >= 0) memcpy(L->gmem + g->b_depth, &f->coord[2], 4);
    if (g->b_sid >= 0) { i32 v = f->sample_id; memcpy(L->gmem + g->b_sid, &v, 4); }
    if (g->b_spos >= 0) memcpy(L->gmem + g->b_spos, f->sample_pos, 8);
    if (g->b_smaskin >= 0) { i32 v = (i32)f->mask_in; memcpy(L->gmem + g->b_smaskin, &v, 4); }
    if (g->b_smask >= 0) { i32 v = -1; memcpy(L->gmem + g->b_smask, &v, 4); }
}
static int g_need_deriv;
static void glsl_fs(gl_ctx_t *C, gl_frag_t *f) {
    pg_t *g = g_run.g;
    msl_lib_t *L = g->lib[1];
    msl_ctx_t ctx;
    u8 dummy[64];
    msl_deriv_t D;
    if (g_need_deriv) {
        /* vizinhos primeiro, gravando os argumentos de derivada e as coordenadas de textura */
        D.mode = 0;
        D.n[0] = D.n[1] = 0;
        for (int nb = 1; nb <= 2; nb++) {
            fs_load(C, g, f, nb);
            memset(&ctx, 0, sizeof ctx);
            D.mode = nb;
            D.k = 0;
            ctx.deriv = &D;
            msl_run(g->main[1], &ctx, dummy);
        }
        D.mode = 3;
        D.k = 0;
    }
    fs_load(C, g, f, 0);
    memset(&ctx, 0, sizeof ctx);
    memcpy(ctx.fragpos, f->coord, 16);
    ctx.front_facing = f->front;
    if (g_need_deriv) ctx.deriv = &D;
    msl_run(g->main[1], &ctx, dummy);
    if (ctx.discarded) { f->discard = 1; return; }
    if (g->w_fcolor) { float c[4]; memcpy(c, L->gmem + g->b_fcolor, 16); for (int i = 0; i < GLX_DRAW; i++) memcpy(f->color[i], c, 16); }
    if (g->w_fdata) for (int i = 0; i < GLX_DRAW; i++) memcpy(f->color[i], L->gmem + g->b_fdata + 16 * i, 16);
    for (int i = 0; i < g->nout; i++) {
        po_t *o = &g->out[i];
        for (int k = 0; k < o->n; k++) {
            int slot = o->index == 1 ? 1 : o->loc + k;
            if (slot >= GLX_DRAW) continue;
            u8 *src = L->gmem + o->off + (i64)k * o->t->size;
            int n = o->t->k == K_VEC ? o->t->n : 1;
            int sk = ty_scal(o->t);
            float c[4] = {0, 0, 0, 1};
            i32 ic[4] = {0, 0, 0, 1};
            for (int j = 0; j < n; j++) {
                if (sk == K_FLOAT) memcpy(&c[j], src + 4 * j, 4);
                else if (sk == K_HALF) { u16 h; memcpy(&h, src + 2 * j, 2); c[j] = msl_h2f(h); }
                else { memcpy(&ic[j], src + 4 * j, 4); c[j] = (float)ic[j]; }
            }
            memcpy(f->color[slot], c, 16);
            memcpy(f->icolor[slot], ic, 16);
        }
    }
    if (g->w_depth) memcpy(&f->depth, L->gmem + g->b_depth, 4);
    if (g->w_smask && g->b_smask >= 0) memcpy(&f->mask_out, L->gmem + g->b_smask, 4);
}
/* ---- tesselação: controle (por patch, as invocações juntas), gerador de primitivos e avaliação (por coordenada) */
typedef struct { float u, v, w; } tess_pt_t;
int gl_tess_gen(GLenum mode, GLenum sp, int cw, int point, const float outer[4], const float inner[2],
                tess_pt_t **pts, int *npts, u32 **ix, int *nix, int *pv);
void gl_raster_tess_emit(gl_ctx_t *C, int pv, gl_vtx_t *v, const u32 *ix, int nix);
/* gl_in[i] (gl_PerVertexIn: gl_Position, gl_PointSize, gl_ClipDistance) a partir de um vértice */
static void pervtx_put(msl_lib_t *L, int i, const gl_vtx_t *v) {
    gv_t gi = find_glob(L, "gl_in");
    if (gi.off < 0 || gi.t->k != K_ARRAY || gi.t->e->k != K_STRUCT) return;
    ty_t *st = gi.t->e;
    u8 *b = L->gmem + gi.off + (i64)i * st->size;
    for (int f = 0; f < st->nf; f++) {
        const char *n = st->f[f].name;
        if (!strcmp(n, "gl_Position")) memcpy(b + st->f[f].off, v->clip, 16);
        else if (!strcmp(n, "gl_PointSize")) memcpy(b + st->f[f].off, &v->psize, 4);
        else if (!strcmp(n, "gl_ClipDistance")) memcpy(b + st->f[f].off, v->cd, 4 * GLX_CLIP);
    }
}
static void reset_plain(pg_t *g, int st) {
    msl_lib_t *L = g->lib[st];
    for (int i = 0; i < g->nplain[st]; i++) memcpy(L->gmem + g->plain[st][i].off, g->init[st] + g->plain[st][i].off, (size_t)g->plain[st][i].size);
}
static void glsl_tess(gl_ctx_t *C, gl_vtx_t **in, int n, u32 prim) {
    pg_t *g = g_run.g;
    msl_lib_t *EL = g->lib[4], *TL = g->lib[3];
    float outer[4], inner[2];
    int N;
    /* entradas do primeiro estágio: gl_in[] e as saídas do VS nos arrays de 32 */
    msl_lib_t *CL = TL ? TL : EL;
    for (int i = 0; i < n && i < 32; i++) {
        pervtx_put(CL, i, in[i]);
        for (int k = 0; k < g->npvar; k++) { pv_t *v = &g->pvar[k]; memcpy(CL->gmem + v->off[1] + (i64)i * v->size, in[i]->v[v->slot], (size_t)v->size); }
    }
    if (TL) {
        reset_plain(g, 3);
        i32 pvi = n, pid = (i32)prim;
        if (g->c_pvi >= 0) memcpy(TL->gmem + g->c_pvi, &pvi, 4);
        if (g->c_pid >= 0) memcpy(TL->gmem + g->c_pid, &pid, 4);
        /* os níveis começam indefinidos; zero descarta o patch se o shader não escrever */
        if (g->c_outer >= 0) memset(TL->gmem + g->c_outer, 0, 16);
        if (g->c_inner >= 0) memset(TL->gmem + g->c_inner, 0, 8);
        msl_ctx_t ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.tid_out = g->c_inv >= 0 ? (u32 *)(TL->gmem + g->c_inv) : 0;
        ctx.primitive_id = prim;
        u32 tg[3] = {0, 0, 0}, tp[3] = {(u32)g->tcs_n, 1, 1};
        msl_run_group(g->main[3], &ctx, tg, tp, tp, 0);
        N = g->tcs_n;
        if (g->c_outer >= 0) memcpy(outer, TL->gmem + g->c_outer, 16); else for (int i = 0; i < 4; i++) outer[i] = 0;
        if (g->c_inner >= 0) memcpy(inner, TL->gmem + g->c_inner, 8); else inner[0] = inner[1] = 0;
        /* controle -> avaliação */
        for (int k = 0; k < g->ntcp; k++) {
            tcopy_t *t = &g->tcp[k];
            if (t->arr) memcpy(EL->gmem + t->dst, TL->gmem + t->src, (size_t)t->esize * (size_t)N);
            else memcpy(EL->gmem + t->dst, TL->gmem + t->src, (size_t)t->esize);
        }
        gv_t go = find_glob(TL, "gl_out");
        if (go.off >= 0 && g->e_in_arr >= 0 && go.t->k == K_ARRAY) memcpy(EL->gmem + g->e_in_arr, TL->gmem + go.off, (size_t)go.t->e->size * (size_t)N);
    } else {
        N = n;
        memcpy(outer, C->patch_outer, 16);
        memcpy(inner, C->patch_inner, 8);
    }
    i32 pvi = N, pid = (i32)prim;
    if (g->e_pvi >= 0) memcpy(EL->gmem + g->e_pvi, &pvi, 4);
    if (g->e_pid >= 0) memcpy(EL->gmem + g->e_pid, &pid, 4);
    if (g->e_outer >= 0) memcpy(EL->gmem + g->e_outer, outer, 16);
    if (g->e_inner >= 0) memcpy(EL->gmem + g->e_inner, inner, 8);
    tess_pt_t *pts = 0;
    u32 *ix = 0;
    int np = 0, ni = 0, pv = 0;
    if (!gl_tess_gen(g->tmode, g->tspacing, g->tcw, g->tpoint, outer, inner, &pts, &np, &ix, &ni, &pv)) return;
    gl_vtx_t *out = np ? malloc(sizeof *out * (size_t)np) : 0;
    if (np && !out) { free(pts); free(ix); return; }
    for (int k = 0; k < np; k++) {
        reset_plain(g, 4);
        float tc[3] = {pts[k].u, pts[k].v, pts[k].w};
        if (g->e_coord >= 0) memcpy(EL->gmem + g->e_coord, tc, 12);
        msl_ctx_t ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.primitive_id = prim;
        u8 dummy[64];
        msl_run(g->main[4], &ctx, dummy);
        out[k].edge = 1;
        out[k].nxfb = 0;
        vtx_out(C, g, EL, &out[k]);
    }
    gl_raster_tess_emit(C, pv, out, ix, ni);
    free(out);
    free(pts);
    free(ix);
}
/* geometry shader: roda sobre um primitivo de entrada e manda as saídas ao raster */
void gl_raster_gs_emit(gl_ctx_t *C, GLenum outprim, gl_vtx_t *v, const u8 *cut, int n);
static void glsl_gs(gl_ctx_t *C, gl_vtx_t **in, int n, u32 prim) {
    pg_t *g = g_run.g;
    msl_lib_t *L = g->lib[2];
    for (int i = 0; i < g->nplain[2]; i++) memcpy(L->gmem + g->plain[2][i].off, g->init[2] + g->plain[2][i].off, (size_t)g->plain[2][i].size);
    gv_t gin = find_glob(L, "gl_in");
    int es = gin.t && gin.t->k == K_ARRAY ? gin.t->e->size : 0;
    ty_t *pvt = gin.t && gin.t->k == K_ARRAY ? gin.t->e : 0;
    for (int k = 0; k < n && k < 6; k++) {
        if (pvt) {
            u8 *b = L->gmem + gin.off + (i64)k * es;
            for (int f = 0; f < pvt->nf; f++) {
                if (!strcmp(pvt->f[f].name, "gl_Position")) memcpy(b + pvt->f[f].off, in[k]->clip, 16);
                else if (!strcmp(pvt->f[f].name, "gl_PointSize")) memcpy(b + pvt->f[f].off, &in[k]->psize, 4);
                else if (!strcmp(pvt->f[f].name, "gl_ClipDistance")) memcpy(b + pvt->f[f].off, in[k]->cd, 4 * GLX_CLIP);
            }
        }
        for (int i = 0; i < g->ngvar; i++) { pv_t *v = &g->gvar[i]; memcpy(L->gmem + v->off[1] + (i64)k * v->size, in[k]->v[v->slot], (size_t)v->size); }
    }
    if (g->gs_pidin >= 0) { i32 v = (i32)prim; memcpy(L->gmem + g->gs_pidin, &v, 4); }
    int M = g->gs_max;
    gl_vtx_t *out = malloc(sizeof(gl_vtx_t) * (size_t)(M > 0 ? M : 1));
    u8 *cut = malloc((size_t)M + 1);
    if (!out || !cut) { free(out); free(cut); return; }
    for (int inv = 0; inv < g->gs_inv; inv++) {
        if (g->gs_invid >= 0) { i32 v = inv; memcpy(L->gmem + g->gs_invid, &v, 4); }
        i32 zero = 0;
        if (g->gs_n >= 0) memcpy(L->gmem + g->gs_n, &zero, 4);
        if (g->gs_cut >= 0) memset(L->gmem + g->gs_cut, 0, (size_t)(M + 1) * 4);
        msl_ctx_t ctx;
        memset(&ctx, 0, sizeof ctx);
        u8 dummy[64];
        msl_run(g->main[2], &ctx, dummy);
        i32 cnt = 0;
        memcpy(&cnt, L->gmem + g->gs_n, 4);
        if (cnt > M) cnt = M;
        for (int v = 0; v < cnt; v++) {
            gl_vtx_t *o = &out[v];
            memset(o, 0, sizeof *o);
            memcpy(o->clip, L->gmem + g->gs_pos + 16 * v, 16);
            memcpy(&o->psize, L->gmem + g->gs_psize + 4 * v, 4);
            if (!gl_is_on(C, GL_PROGRAM_POINT_SIZE)) o->psize = C->point_s;
            for (int c = 0; c < GLX_CLIP; c++) o->cd[c] = 1;
            o->edge = 1;
            for (int i = 0; i < g->nvar; i++) { pv_t *pv = &g->var[i]; memcpy(o->v[pv->slot], L->gmem + pv->off[0] + (i64)v * pv->size, (size_t)pv->size); }
            i32 c = 0;
            memcpy(&c, L->gmem + g->gs_cut + 4 * v, 4);
            cut[v] = (u8)(c != 0);
        }
        i32 c = 0;
        memcpy(&c, L->gmem + g->gs_cut + 4 * cnt, 4);
        cut[cnt] = (u8)(c != 0);
        gl_raster_gs_emit(C, g->gs_outp, out, cut, cnt);
    }
    free(out);
    free(cut);
}
/* embutidas uniformes do compat */
static void set_glob(pg_t *g, const char *name, int idx, const float *v, int n) {
    for (int st = 0; st < NST; st++) {
        if (!g->lib[st]) continue;
        gv_t x = find_glob(g->lib[st], name);
        if (x.off < 0) continue;
        int es = x.t->k == K_ARRAY ? x.t->e->size : x.t->size;
        u8 *d = g->lib[st]->gmem + x.off + (i64)idx * es;
        ty_t *t = x.t->k == K_ARRAY ? x.t->e : x.t;
        if (t->k == K_MAT) {
            int cols = t->n, rows = t->e->n;
            for (int c = 0; c < cols; c++) for (int r = 0; r < rows; r++) memcpy(d + c * t->e->size + r * 4, &v[c * 4 + r], 4);
        } else memcpy(d, v, (size_t)n * 4);
    }
}
static void set_field(pg_t *g, const char *name, int idx, const char *field, const float *v, int n) {
    for (int st = 0; st < NST; st++) {
        if (!g->lib[st]) continue;
        gv_t x = find_glob(g->lib[st], name);
        if (x.off < 0) continue;
        ty_t *t = x.t->k == K_ARRAY ? x.t->e : x.t;
        if (t->k != K_STRUCT) continue;
        for (int i = 0; i < t->nf; i++)
            if (!strcmp(t->f[i].name, field)) memcpy(g->lib[st]->gmem + x.off + (i64)idx * t->size + t->f[i].off, v, (size_t)n * 4);
    }
}
static void transp(float *o, const float *m) { for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) o[i * 4 + j] = m[j * 4 + i]; }
static void compat_uniforms(gl_ctx_t *C, pg_t *g) {
    const char *u = g->src_user[0], *fu = g->src_user[1], *gu = g->src_user[2], *cu = g->src_user[3], *eu = g->src_user[4];
#define USED(n) (uses(u, n) || uses(fu, n) || uses(gu, n) || uses(cu, n) || uses(eu, n) || (uses(u, "ftransform") && !strcmp(n, "gl_ModelViewProjectionMatrix")))
    float dr[3] = {(float)C->dr[0], (float)C->dr[1], (float)(C->dr[1] - C->dr[0])};
    if (USED("gl_DepthRange")) { set_field(g, "gl_DepthRange", 0, "near", &dr[0], 1); set_field(g, "gl_DepthRange", 0, "far", &dr[1], 1); set_field(g, "gl_DepthRange", 0, "diff", &dr[2], 1); }
    if (!g->compat) return;
    const float *mv = C->mv.s[C->mv.top].m, *pj = C->proj.s[C->proj.top].m;
    float mvp[16], inv[16], t[16];
    gl_mat_mul(mvp, pj, mv);
    struct { const char *n; const float *m; } M[] = {{"gl_ModelViewMatrix", mv}, {"gl_ProjectionMatrix", pj}, {"gl_ModelViewProjectionMatrix", mvp}};
    for (int i = 0; i < 3; i++) {
        char nm[80];
        if (USED(M[i].n)) set_glob(g, M[i].n, 0, M[i].m, 16);
        gl_mat_inv(inv, M[i].m);
        snprintf(nm, sizeof nm, "%sInverse", M[i].n); if (USED(nm)) set_glob(g, nm, 0, inv, 16);
        transp(t, M[i].m);
        snprintf(nm, sizeof nm, "%sTranspose", M[i].n); if (USED(nm)) set_glob(g, nm, 0, t, 16);
        transp(t, inv);
        snprintf(nm, sizeof nm, "%sInverseTranspose", M[i].n); if (USED(nm)) set_glob(g, nm, 0, t, 16);
    }
    if (USED("gl_TextureMatrix") || USED("gl_TextureMatrixInverse") || USED("gl_TextureMatrixTranspose") || USED("gl_TextureMatrixInverseTranspose"))
        for (int k = 0; k < 8; k++) {
            const float *tm = C->texm[k].s[C->texm[k].top].m;
            set_glob(g, "gl_TextureMatrix", k, tm, 16);
            gl_mat_inv(inv, tm);
            set_glob(g, "gl_TextureMatrixInverse", k, inv, 16);
            transp(t, tm);
            set_glob(g, "gl_TextureMatrixTranspose", k, t, 16);
            transp(t, inv);
            set_glob(g, "gl_TextureMatrixInverseTranspose", k, t, 16);
        }
    if (USED("gl_NormalMatrix")) {
        gl_mat_inv(inv, mv);
        float nm[16] = {0};
        for (int c = 0; c < 3; c++) for (int r = 0; r < 3; r++) nm[c * 4 + r] = inv[r * 4 + c];
        set_glob(g, "gl_NormalMatrix", 0, nm, 9);
    }
    if (USED("gl_LightSource") || USED("gl_FrontLightProduct") || USED("gl_BackLightProduct"))
        for (int i = 0; i < GLX_LIGHTS; i++) {
            gl_light_t *L = &C->light[i];
            set_field(g, "gl_LightSource", i, "ambient", L->amb, 4);
            set_field(g, "gl_LightSource", i, "diffuse", L->dif, 4);
            set_field(g, "gl_LightSource", i, "specular", L->spec, 4);
            set_field(g, "gl_LightSource", i, "position", L->pos, 4);
            float h[4] = {L->pos[0], L->pos[1], L->pos[2] + 1, 0};
            if (L->pos[3] == 0) { double l = sqrt((double)h[0] * h[0] + (double)h[1] * h[1] + (double)h[2] * h[2]); if (l > 0) for (int k = 0; k < 3; k++) h[k] = (float)(h[k] / l); }
            set_field(g, "gl_LightSource", i, "halfVector", h, 4);
            set_field(g, "gl_LightSource", i, "spotDirection", L->spot_dir, 3);
            set_field(g, "gl_LightSource", i, "spotExponent", &L->spot_exp, 1);
            set_field(g, "gl_LightSource", i, "spotCutoff", &L->spot_cut, 1);
            float cc = (float)cos(L->spot_cut * 3.14159265358979323846 / 180);
            set_field(g, "gl_LightSource", i, "spotCosCutoff", &cc, 1);
            set_field(g, "gl_LightSource", i, "constantAttenuation", &L->att[0], 1);
            set_field(g, "gl_LightSource", i, "linearAttenuation", &L->att[1], 1);
            set_field(g, "gl_LightSource", i, "quadraticAttenuation", &L->att[2], 1);
            for (int f = 0; f < 2; f++) {
                gl_mat_t *m = &C->mat[f];
                float a[4], d[4], s[4];
                for (int k = 0; k < 4; k++) { a[k] = m->amb[k] * L->amb[k]; d[k] = m->dif[k] * L->dif[k]; s[k] = m->spec[k] * L->spec[k]; }
                const char *pn = f ? "gl_BackLightProduct" : "gl_FrontLightProduct";
                set_field(g, pn, i, "ambient", a, 4);
                set_field(g, pn, i, "diffuse", d, 4);
                set_field(g, pn, i, "specular", s, 4);
            }
        }
    for (int f = 0; f < 2; f++) {
        gl_mat_t *m = &C->mat[f];
        const char *mn = f ? "gl_BackMaterial" : "gl_FrontMaterial";
        if (USED(mn)) {
            set_field(g, mn, 0, "emission", m->emi, 4);
            set_field(g, mn, 0, "ambient", m->amb, 4);
            set_field(g, mn, 0, "diffuse", m->dif, 4);
            set_field(g, mn, 0, "specular", m->spec, 4);
            set_field(g, mn, 0, "shininess", &m->shin, 1);
        }
        const char *pn = f ? "gl_BackLightModelProduct" : "gl_FrontLightModelProduct";
        if (USED(pn)) { float sc[4]; for (int k = 0; k < 4; k++) sc[k] = m->emi[k] + m->amb[k] * C->lm_amb[k]; sc[3] = m->dif[3]; set_field(g, pn, 0, "sceneColor", sc, 4); }
    }
    if (USED("gl_LightModel")) set_field(g, "gl_LightModel", 0, "ambient", C->lm_amb, 4);
    if (USED("gl_Fog")) {
        set_field(g, "gl_Fog", 0, "color", C->fog_c, 4);
        set_field(g, "gl_Fog", 0, "density", &C->fog_d, 1);
        set_field(g, "gl_Fog", 0, "start", &C->fog_s, 1);
        set_field(g, "gl_Fog", 0, "end", &C->fog_e, 1);
        float sc = C->fog_e != C->fog_s ? 1.0f / (C->fog_e - C->fog_s) : 0;
        set_field(g, "gl_Fog", 0, "scale", &sc, 1);
    }
    if (USED("gl_Point")) {
        set_field(g, "gl_Point", 0, "size", &C->point_s, 1);
        set_field(g, "gl_Point", 0, "sizeMin", &C->point_min, 1);
        set_field(g, "gl_Point", 0, "sizeMax", &C->point_max, 1);
        set_field(g, "gl_Point", 0, "fadeThresholdSize", &C->point_fade, 1);
        set_field(g, "gl_Point", 0, "distanceConstantAttenuation", &C->point_att[0], 1);
        set_field(g, "gl_Point", 0, "distanceLinearAttenuation", &C->point_att[1], 1);
        set_field(g, "gl_Point", 0, "distanceQuadraticAttenuation", &C->point_att[2], 1);
    }
    if (USED("gl_ClipPlane")) for (int i = 0; i < GLX_CLIP; i++) { float pl[4] = {(float)C->clip_plane[i][0], (float)C->clip_plane[i][1], (float)C->clip_plane[i][2], (float)C->clip_plane[i][3]}; set_glob(g, "gl_ClipPlane", i, pl, 4); }
    if (USED("gl_TextureEnvColor")) for (int i = 0; i < 8; i++) set_glob(g, "gl_TextureEnvColor", i, C->te[i].color, 4);
    static const char *ep[4] = {"gl_EyePlaneS", "gl_EyePlaneT", "gl_EyePlaneR", "gl_EyePlaneQ"}, *op[4] = {"gl_ObjectPlaneS", "gl_ObjectPlaneT", "gl_ObjectPlaneR", "gl_ObjectPlaneQ"};
    for (int c = 0; c < 4; c++) {
        if (USED(ep[c])) for (int i = 0; i < 8; i++) set_glob(g, ep[c], i, C->te[i].gen_eye[c], 4);
        if (USED(op[c])) for (int i = 0; i < 8; i++) set_glob(g, op[c], i, C->te[i].gen_obj[c], 4);
    }
    if (USED("gl_NormalScale")) { gl_mat_inv(inv, mv); double s = sqrt((double)inv[8] * inv[8] + (double)inv[9] * inv[9] + (double)inv[10] * inv[10]); float f = (float)(s ? 1 / s : 1); set_glob(g, "gl_NormalScale", 0, &f, 1); }
#undef USED
}
void gl_glsl_stage(gl_ctx_t *C) {
    gl_stage_t *S = &C->st;
    memset(S, 0, sizeof *S);
    gl_prog_t *p = C->prog;
    if (!p) p = C->arb_prog;
    if (!p) p = pipeline_prog(C);
    if (!p || !p->priv) return;
    pg_t *g = p->priv;
    g_run.p = p;
    g_run.g = g;
    /* samplers: textura e estado da unidade de cada elemento */
    int si = 0;
    for (int i = 0; i < g->nuni; i++) {
        pu_t *u = &g->uni[i];
        if (!u->sampler) continue;
        for (int k = 0; k < u->arr; k++, si++) {
            int unit = u->units[k];
            gl_tex_t *t = C->tex[unit][u->target];
            msl_tex_t *mt = gl_tex_mt(C, t);
            msl_samp_t *ss = &g->sstore[si];
            gl_tex_sampler(C, t, C->samp[unit], ss);
            void *ptrs[2] = {mt, ss};
            for (int st = 0; st < NST; st++) if (u->off[st] >= 0) memcpy(g->lib[st]->gmem + u->off[st] + (i64)k * u->stride, ptrs, 16);
        }
    }
    /* blocos de uniform a partir dos buffers ligados */
    for (int i = 0; i < g->nblk; i++) {
        pb_t *B = &g->blk[i];
        gl_buf_t *b = C->ubo[B->binding];
        if (!b || !b->data) continue;
        i64 off = C->ubo_off[B->binding], avail = b->size - off;
        if (C->ubo_size[B->binding] && C->ubo_size[B->binding] < avail) avail = C->ubo_size[B->binding];
        for (int st = 0; st < NST; st++) if (B->off[st] >= 0) copy140(B->t, b->data + off, avail, g->lib[st]->gmem + B->off[st], B->rowmajor);
    }
    /* EXT_bindable_uniform: o buffer já está no layout do gmem (glGetUniformOffsetEXT) */
    for (int i = 0; i < g->nuni; i++) {
        pu_t *u = &g->uni[i];
        if (!u->bbuf || !u->bbuf->data) continue;
        i64 n = (i64)u->stride * u->arr;
        if (n > u->bbuf->size) n = u->bbuf->size;
        for (int st = 0; st < NST; st++) if (u->off[st] >= 0) memcpy(g->lib[st]->gmem + u->off[st], u->bbuf->data, (size_t)n);
    }
    compat_uniforms(C, g);
    S->nvar = g->nslots;
    for (int i = 0; i < GLX_VAR; i++) S->interp[i] = MSL_INTERP_PERSP;
    if (g->compat && C->shade == GL_FLAT) for (int i = 0; i < 4; i++) S->interp[i] = MSL_INTERP_FLAT;
    for (int i = 0; i < g->nvar; i++) for (int k = 0; k < g->var[i].nslots; k++) S->interp[g->var[i].slot + k] = (u8)g->var[i].interp;
    S->centroid = 0;
    for (int i = 0; i < g->nvar; i++) if (g->var[i].aux == 1) for (int k = 0; k < g->var[i].nslots; k++) S->centroid |= 1u << (g->var[i].slot + k);
    S->per_sample = g->per_sample;
    S->writes_mask = g->w_smask;
    /* derivadas: o shader pede dFdx/dFdy/fwidth ou amostra textura com mipmap / filtros min e mag diferentes */
    g_need_deriv = uses(g->src_user[1], "dFdx") || uses(g->src_user[1], "dFdy") || uses(g->src_user[1], "fwidth") ||
                   uses(g->src_user[1], "dFdxFine") || uses(g->src_user[1], "dFdyFine") || uses(g->src_user[1], "dFdxCoarse") ||
                   uses(g->src_user[1], "dFdyCoarse") || uses(g->src_user[1], "fwidthFine") || uses(g->src_user[1], "fwidthCoarse");
    for (int i = 0, k = 0; i < g->nuni && !g_need_deriv; i++) {
        pu_t *u = &g->uni[i];
        if (!u->sampler) continue;
        for (int e = 0; e < u->arr; e++, k++) {
            msl_samp_t *ss = &g->sstore[k];
            msl_tex_t *mt = gl_tex_mt(C, C->tex[u->units[e]][u->target]);
            if (u->off[1] >= 0 && mt->data && ((ss->mipf && mt->levels > 1) || ss->minf != ss->magf)) g_need_deriv = 1;
        }
    }
    if (g_need_deriv) S->dmask = g->nslots >= 32 ? 0xffffffffu : (1u << g->nslots) - 1;
    S->nclip = GLX_CLIP;
    if (g->lib[2]) { S->gs = glsl_gs; S->gs_in = g->gs_inp; }
    if (g->tess) { S->tess = glsl_tess; S->tess_out = g->tpoint ? GL_POINTS : g->tmode == GL_ISOLINES ? GL_LINES : GL_TRIANGLES; }
    S->vs = glsl_vs;
    S->fs = glsl_fs;
    S->writes_depth = g->w_depth;
    S->discards = g->discards;
    S->alpha_test = g->compat;
    S->two_side = g->compat && gl_is_on(C, GL_VERTEX_PROGRAM_TWO_SIDE);
    S->nxfb = 0;
}

/* ================================================================ programas internos (ARB_vertex/fragment_program) */
/* liga um programa a partir de duas fontes GLSL sem registrar nomes no contexto; abind: "nome=loc" por linha */
gl_prog_t *gl_glsl_build(gl_ctx_t *C, const char *vs, const char *fs, const char *const *anames, const int *alocs, int na, char **log) {
    gl_prog_t *p = calloc(1, sizeof *p);
    gl_shader_t *sh = calloc(2, sizeof *sh);
    if (!p || !sh) { free(p); free(sh); return 0; }
    p->kind = 2;
    p->refs = 1;
    p->xfb_mode = GL_INTERLEAVED_ATTRIBS;
    const char *src[2] = {vs, fs};
    for (int i = 0; i < 2; i++) {
        if (!src[i]) continue;
        sh[i].kind = 1;
        sh[i].type = i ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER;
        sh[i].src = strdup(src[i]);
        sh[i].compiled = 1;
        sh[i].refs = 2;   /* o programa não libera: a memória é deste bloco */
        p->sh[p->nsh++] = &sh[i];
    }
    for (int i = 0; i < na && i < 64; i++) { p->abind_name[i] = strdup(anames[i]); p->abind_loc[i] = alocs[i]; }
    p->nabind = na < 64 ? na : 64;
    p->log = strdup("");
    p->linked = link_prog(C, p);
    if (log) *log = strdup(p->log ? p->log : "");
    p->priv_shaders = sh;
    return p;
}
void gl_glsl_free(gl_prog_t *p) {
    if (!p) return;
    gl_shader_t *sh = p->priv_shaders;
    if (sh) { free(sh[0].src); free(sh[1].src); free(sh); }
    pg_free(p->priv);
    for (int i = 0; i < p->nabind; i++) free(p->abind_name[i]);
    free(p->log);
    free(p);
}
/* escreve uma global (uniform interno) nos dois estágios */
void gl_glsl_set(gl_prog_t *p, const char *name, int idx, const float *v, int n) { if (p && p->priv) set_glob(p->priv, name, idx, v, n); }
/* unidade de um sampler interno */
void gl_glsl_sampler_unit(gl_prog_t *p, const char *name, int unit) {
    pg_t *g = p ? p->priv : 0;
    if (!g) return;
    pu_t *u = uni_find(g, name);
    if (u && u->sampler) u->units[0] = unit;
}
void gl_glsl_set_stage(gl_prog_t *p, int stage, const char *name, const float *v, int n) {
    pg_t *g = p ? p->priv : 0;
    if (!g || stage < 0 || stage > 1) return;
    gv_t x = find_glob(g->lib[stage], name);
    if (x.off >= 0 && n * 4 <= x.size) memcpy(g->lib[stage]->gmem + x.off, v, (size_t)n * 4);
}

/* ================================================================ EXT_bindable_uniform */
static pu_t *bind_uni(gl_ctx_t *C, GLuint pn, GLint loc, gl_prog_t **pp) {
    gl_prog_t *p = pr_get(C, pn);
    if (!p) { gl_err(C, GL_INVALID_VALUE); return 0; }
    if (!p->linked) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    int el;
    pu_t *u = uni_at(p->priv, loc, &el);
    if (!u || u->sampler) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    if (pp) *pp = p;
    return u;
}
void glUniformBufferEXT(GLuint pn, GLint loc, GLuint buf) {
    GLC();
    pu_t *u = bind_uni(C, pn, loc, 0);
    if (!u) return;
    gl_buf_t *b = buf ? gl_names_get(&C->sh->bufs, buf) : 0;
    if (buf && !b) GLERR(GL_INVALID_OPERATION, );
    u->bbuf = b;
}
GLint glGetUniformBufferSizeEXT(GLuint pn, GLint loc) { GLC(0); pu_t *u = bind_uni(C, pn, loc, 0); return u ? u->stride * u->arr : 0; }
GLintptr glGetUniformOffsetEXT(GLuint pn, GLint loc) {
    GLC(-1);
    gl_prog_t *p;
    pu_t *u = bind_uni(C, pn, loc, &p);
    if (!u) return -1;
    return (GLintptr)(loc - u->loc) * u->stride;
}

/* mOS libSystem — compilador/interpretador de Metal Shading Language (subconjunto), usado pelo Metal
 * por software (metal.c). Interface entre o compilador (mtlshader.c) e o runtime. */
#pragma once
#include "sys.h"

/* estágios */
enum { MSL_HELPER = 0, MSL_VERTEX = 1, MSL_FRAGMENT = 2, MSL_KERNEL = 3 };

/* tipos escalares expostos ao runtime (campos de stage_in / saída) */
enum { MSL_BOOL = 1, MSL_CHAR, MSL_UCHAR, MSL_SHORT, MSL_USHORT, MSL_INT, MSL_UINT, MSL_LONG, MSL_ULONG, MSL_HALF, MSL_FLOAT };

/* atributos [[...]] */
enum {
    A_NONE = 0, A_POSITION, A_ATTRIBUTE, A_USER, A_COLOR, A_BUFFER, A_TEXTURE, A_SAMPLER, A_STAGE_IN,
    A_VID, A_IID, A_BASE_VERTEX, A_BASE_INSTANCE, A_TPIG, A_TPITG, A_TGPIG, A_TPTG, A_TPG, A_TGPG, A_TIITG,
    A_POINT_SIZE, A_DEPTH, A_FRONT_FACING, A_POINT_COORD, A_SAMPLE_ID, A_THREADGROUP, A_CLIP_DISTANCE,
    A_RT_INDEX, A_SIMD_LANE, A_SIMD_GROUP, A_SIMD_SIZE, A_SAMPLE_MASK, A_PRIMITIVE_ID, A_VIEWPORT_INDEX,
};
enum { MSL_INTERP_PERSP = 0, MSL_INTERP_FLAT = 1, MSL_INTERP_NOPERSP = 2 };

typedef struct {
    const char *name;          /* nome do membro */
    const char *user;          /* [[user(x)]] ou 0 */
    int off, size;             /* posição e tamanho no blob */
    int scal, n;               /* tipo escalar e número de componentes (vetor; 1 = escalar) */
    int attr, idx;             /* atributo e índice ([[attribute(2)]], [[color(1)]]) */
    int interp;
} msl_field_t;

/* textura e sampler como o interpretador as vê (preenchidos pelo metal.c) */
enum { MSL_TEX_1D = 0, MSL_TEX_1D_ARRAY, MSL_TEX_2D, MSL_TEX_2D_ARRAY, MSL_TEX_2D_MS, MSL_TEX_CUBE, MSL_TEX_CUBE_ARRAY,
       MSL_TEX_3D, MSL_TEX_2D_MS_ARRAY, MSL_TEX_BUFFER };
typedef struct msl_tex {
    u8 *data;
    int fmt, type;
    int w, h, d, levels, slices;
    u64 loff[16];              /* deslocamento de cada nível */
    u64 lbpr[16];              /* bytes por linha de cada nível */
    u64 lslice[16];            /* bytes por fatia (slice) de cada nível */
} msl_tex_t;
typedef struct msl_samp {
    int minf, magf, mipf;      /* 0 nearest, 1 linear; mip: 0 sem, 1 nearest, 2 linear */
    int addr[3];               /* MTLSamplerAddressMode */
    int normalized, compare, border;
    float lodmin, lodmax;
} msl_samp_t;

/* implementados no metal.c */
void msl_tex_load(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, double out[4]);
void msl_tex_store(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, const double in[4]);
int msl_fmt_isint(int fmt);
void msl_tex_sample(const msl_tex_t *t, const msl_samp_t *s, const double *coord, int ncoord, int slice, double lod, double out[4]);

/* contexto de uma execução */
typedef struct msl_ctx {
    void *buf[31];             /* [[buffer(n)]] já com o deslocamento */
    msl_tex_t *tex[128];       /* [[texture(n)]] */
    msl_samp_t *samp[16];      /* [[sampler(n)]] */
    u8 *tgbuf[31];             /* [[threadgroup(n)]] */
    u8 *tgstatic;              /* threadgroup declarados dentro do kernel */
    void *stage_in;
    u32 vid, iid, base_vertex, base_instance;
    float fragpos[4];
    int front_facing;
    u32 sample_id, primitive_id;
    float point_coord[2];
    u32 tpig[3], tpitg[3], tgpig[3], tptg[3], tpg[3], tgpg[3], tiitg;
    int discarded;
} msl_ctx_t;

typedef struct msl_lib msl_lib_t;
typedef struct msl_fn msl_fn_t;

msl_lib_t *msl_compile(const char *src, u64 n, const char *defines, char *err, int errn);
void msl_lib_free(msl_lib_t *l);
int msl_fn_count(msl_lib_t *l);
msl_fn_t *msl_fn_at(msl_lib_t *l, int i);
msl_fn_t *msl_find(msl_lib_t *l, const char *name);
const char *msl_fn_name(msl_fn_t *f);
int msl_fn_stage(msl_fn_t *f);
/* layout do valor devolvido (vértice/fragmento) e do [[stage_in]] */
int msl_fn_out(msl_fn_t *f, const msl_field_t **fields, int *size);
int msl_fn_stagein(msl_fn_t *f, const msl_field_t **fields, int *size);
u32 msl_fn_tgstatic(msl_fn_t *f);          /* memória threadgroup estática do kernel */
int msl_fn_barrier(msl_fn_t *f);            /* usa threadgroup_barrier */
int msl_fn_discards(msl_fn_t *f);           /* pode chamar discard_fragment */
/* SPIR-V da função, se a tradução coube. words aponta para o módulo; nwords é a contagem.
 * Devolve 1 quando há módulo. A nota explica o que ficou de fora (pode ser vazia). */
int msl_fn_spirv(msl_fn_t *f, const u32 **words, u32 *nwords);
const char *msl_fn_spirv_note(msl_fn_t *f);
extern char *(*msl_include_hook)(const char *name, u64 *len);
/* roda a função de entrada; out recebe o valor devolvido (tamanho de msl_fn_out) */
void msl_run(msl_fn_t *f, msl_ctx_t *c, void *out);
/* despacha um threadgroup inteiro (com barreiras, cada thread numa fibra) */
void msl_run_group(msl_fn_t *f, msl_ctx_t *tmpl, const u32 tg[3], const u32 tptg[3], const u32 grid[3], int exact);

u16 msl_f2h(float f);
float msl_h2f(u16 h);

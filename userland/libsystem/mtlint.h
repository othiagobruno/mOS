/* mOS libSystem — estruturas internas do compilador de MSL (mtlshader.c) e do interpretador (mtlexec.c) */
#pragma once
#include "mtlshader.h"
#include "libc.h"

int snprintf(char *b, size_t n, const char *fmt, ...);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
unsigned long long strtoull(const char *s, char **e, int b);
char *strstr(const char *h, const char *n);

/* tipos: K_BOOL..K_FLOAT têm os mesmos valores de MSL_BOOL..MSL_FLOAT */
enum { K_VOID = 0, K_BOOL, K_CHAR, K_UCHAR, K_SHORT, K_USHORT, K_INT, K_UINT, K_LONG, K_ULONG, K_HALF, K_FLOAT,
       K_VEC, K_MAT, K_STRUCT, K_ARRAY, K_PTR, K_TEX, K_SAMP, K_ATOMIC };
enum { SP_THREAD = 0, SP_DEVICE, SP_CONSTANT, SP_THREADGROUP };

typedef struct ty ty_t;
typedef struct {
    const char *name;
    ty_t *t;
    int off, attr, idx, interp;
    const char *user;
} fld_t;
struct ty {
    int k, size, align;
    ty_t *e;                  /* vec: escalar; mat: vetor-coluna; array/ptr/atomic: elemento; tex: escalar */
    int n;                    /* vec: componentes; mat: colunas; array: elementos */
    int packed, space, tex, depth;
    const char *name;
    fld_t *f;
    int nf;
    ty_t *next;
};

#define IS_SCALAR(t) ((t)->k >= K_BOOL && (t)->k <= K_FLOAT)
#define IS_NUM(t) (IS_SCALAR(t) || (t)->k == K_VEC || (t)->k == K_MAT)
#define IS_INTK(k) ((k) >= K_BOOL && (k) <= K_ULONG)
#define IS_UNSK(k) ((k) == K_UCHAR || (k) == K_USHORT || (k) == K_UINT || (k) == K_ULONG || (k) == K_BOOL)
static inline int ty_scal(const ty_t *t) { return t->k == K_VEC ? t->e->k : t->k == K_MAT ? t->e->e->k : t->k == K_ATOMIC ? t->e->k : t->k; }
static inline int ty_ncomp(const ty_t *t) { return t->k == K_VEC ? t->n : t->k == K_MAT ? t->n * t->e->n : 1; }
static inline int scal_size(int k) { return k <= K_UCHAR ? 1 : k <= K_USHORT || k == K_HALF ? 2 : k == K_LONG || k == K_ULONG ? 8 : 4; }

/* nós da árvore */
enum {
    E_LIT = 1, E_VAR, E_MEMBER, E_SWZ, E_INDEX, E_CALL, E_BI, E_CTOR, E_CAST, E_BITCAST, E_UN, E_BIN, E_LAND, E_LOR,
    E_COND, E_ASSIGN, E_PREINC, E_PREDEC, E_POSTINC, E_POSTDEC, E_ADDR, E_DEREF, E_INIT, E_TEX, E_COMMA,
    S_EXPR = 40, S_DECL, S_BLOCK, S_IF, S_FOR, S_WHILE, S_DO, S_RET, S_BREAK, S_CONT, S_SWITCH, S_NOP,
};
typedef struct var {
    const char *name;
    ty_t *t;
    int off, ref, where;      /* where: 0 quadro, 1 global, 2 threadgroup estático */
    int attr, idx;
    const char *user;
} var_t;
typedef struct node node_t;
struct node {
    int op, sub, line;
    ty_t *t;
    node_t *a, *b, *c, *d;
    node_t **v;
    int nv;
    double fv;
    i64 iv;
    var_t *var;
    int off;
    u8 swz[4];
    int nswz;
    struct msl_fn *fn;
    int bi;
    i64 *cases;               /* switch: valores (índice em v); default = -1 em sub */
};

/* opções das chamadas de textura */
enum { TO_NONE = 0, TO_LEVEL, TO_BIAS, TO_GRAD, TO_MINLOD };

struct msl_fn {
    const char *name;
    int stage;
    ty_t *ret;
    var_t **params;
    int np;
    node_t *body;
    int frame;                /* tamanho do quadro (variáveis locais) */
    int retoff;
    int barrier, discard;
    u32 tgstatic;
    msl_field_t *out, *in;
    int nout, outsize, nin, insize;
    struct msl_lib *lib;
    int defined;
    u32 *spirv;
    u32 spirv_n;
    char spirv_note[80];
};

typedef struct arena_blk { struct arena_blk *next; u64 used, cap; u8 data[]; } arena_blk_t;
struct msl_lib {
    msl_fn_t **fns;
    int nfn, capfn;
    u8 *gmem;
    int gsize, gcap;
    arena_blk_t *arena;
};
void *msl_alloc(msl_lib_t *l, u64 n);

/* funções embutidas */
enum {
    B_NONE = 0,
    /* genéricas, componente a componente (resultado do tipo promovido) */
    B_SIN, B_COS, B_TAN, B_ASIN, B_ACOS, B_ATAN, B_SINH, B_COSH, B_TANH, B_ASINH, B_ACOSH, B_ATANH,
    B_EXP, B_EXP2, B_EXP10, B_LOG, B_LOG2, B_LOG10, B_SQRT, B_RSQRT, B_FLOOR, B_CEIL, B_ROUND, B_RINT, B_TRUNC,
    B_FRACT, B_SATURATE, B_SIGN, B_ABS, B_SINPI, B_COSPI, B_TANPI,
    B_POW, B_ATAN2, B_FMOD, B_MIN, B_MAX, B_STEP, B_COPYSIGN, B_FDIM,
    B_CLAMP, B_MIX, B_SMOOTHSTEP, B_FMA, B_SELECT,
    B_LAST_GEN,
    /* resultado próprio */
    B_DOT, B_LENGTH, B_LENGTH2, B_DISTANCE, B_DISTANCE2, B_NORMALIZE, B_CROSS, B_REFLECT, B_REFRACT, B_FACEFORWARD,
    B_ANY, B_ALL, B_ISNAN, B_ISINF, B_ISFINITE, B_SIGNBIT, B_TRANSPOSE, B_DETERMINANT,
    B_POPCOUNT, B_CLZ, B_CTZ, B_REVERSE_BITS, B_MULHI, B_ROTATE, B_ABSDIFF, B_HADD, B_EXTRACT_BITS, B_INSERT_BITS,
    B_DFDX, B_DFDY, B_FWIDTH,
    B_DISCARD, B_BARRIER, B_SIMD_BARRIER,
    B_ATOMIC_LOAD, B_ATOMIC_STORE, B_ATOMIC_XCHG, B_ATOMIC_CAS, B_ATOMIC_ADD, B_ATOMIC_SUB, B_ATOMIC_MIN,
    B_ATOMIC_MAX, B_ATOMIC_AND, B_ATOMIC_OR, B_ATOMIC_XOR,
    B_PACK_UNORM4X8, B_PACK_SNORM4X8, B_PACK_UNORM2X16, B_PACK_SNORM2X16, B_PACK_HALF2X16,
    B_UNPACK_UNORM4X8, B_UNPACK_SNORM4X8, B_UNPACK_UNORM2X16, B_UNPACK_SNORM2X16, B_UNPACK_HALF2X16,
    B_LEVEL, B_BIAS, B_GRAD, B_MINLOD,
    B_SIMD_IDENT,             /* simd_broadcast_first etc. com uma thread só por simdgroup */
    B_SIMD_SUM,
};
/* métodos de textura */
enum { T_SAMPLE = 1, T_READ, T_WRITE, T_GET_W, T_GET_H, T_GET_D, T_GET_ARR, T_GET_MIPS, T_SAMPLE_CMP, T_GATHER, T_FENCE, T_GET_SAMPLES };

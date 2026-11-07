/* mOS libSystem — estruturas internas do Metal por software (metal.c: objetos e API; metalexec.c:
 * formatos de pixel, rasterizador, compute e blit) */
#pragma once
#include "foundation.h"
#include "libc.h"
#include "mtlshader.h"

typedef struct { u64 x, y, z; } mtl_origin_t;
typedef struct { u64 w, h, d; } mtl_size_t;
typedef struct { mtl_origin_t o; mtl_size_t s; } mtl_region_t;
typedef struct { double r, g, b, a; } mtl_cc_t;
typedef struct { double x, y, w, h, zn, zf; } mtl_vp_t;
typedef struct { u64 x, y, w, h; } mtl_sc_t;

/* cabeçalho comum dos objetos (isa, label, device) */
#define MHDR cx_class_t *isa; id label; id dev
typedef struct { MHDR; } mobj_t;

typedef struct mbuf {
    MHDR;
    u8 *p;
    u64 len, opts;
    int own;
    void *dealloc_blk;
    id heap;
} mbuf_t;

typedef struct mtex {
    MHDR;
    msl_tex_t t;              /* o que o interpretador e o rasterizador enxergam */
    int fmt, type, usage, storage, samples, levels, arrlen;
    u64 w, h, d, opts;
    u8 *alloc;                /* memória própria (0 = de um IOSurface, buffer ou textura-mãe) */
    id surf;
    int plane;
    id buffer;
    u64 boff, bbpr;
    id parent;
    int fbonly;
    u32 swizzle;
    id heap;
} mtex_t;

typedef struct msamp { MHDR; msl_samp_t s; } msamp_t;
typedef struct { int cmp, fail, dfail, pass; u32 rmask, wmask; } stencil_t;
typedef struct mdss { MHDR; int dcmp, dwrite, stencil_on; stencil_t front, back; } mdss_t;
typedef struct mlib { MHDR; msl_lib_t *L; id names; } mlib_t;
typedef struct mfunc { MHDR; id lib; msl_fn_t *fn; id name; } mfunc_t;

typedef struct { int fmt, blend, srgb, drgb, rgbop, sa, da, aop; u32 wmask; } cattach_t;
typedef struct { int fmt, buf; u64 off; } vattr_t;
typedef struct { u64 stride, rate; int step; } vlayout_t;
typedef struct mrpipe {
    MHDR;
    id vfunc, ffunc;
    msl_fn_t *vfn, *ffn;
    cattach_t ca[8];
    int dfmt, sfmt, raster, samples, a2c, has_vd;
    vattr_t attr[31];
    vlayout_t lay[31];
    /* ligações calculadas na criação */
    const msl_field_t *vout, *fin, *fout, *vin;
    int nvout, nfin, nfout, nvin, vout_size, fin_size, fout_size, vin_size;
    int pos_field, psize_field;
    int fmap[64];             /* fragmento stage_in[i] <- vout[fmap[i]] (-1 nada, -2 posição) */
    int finterp[64];
    int fdepth;               /* índice do campo [[depth]] em fout (-1) */
    int fcolor[8];            /* índice do campo [[color(i)]] em fout (-1) */
    int discards;
} mrpipe_t;
typedef struct mcpipe { MHDR; id func; msl_fn_t *fn; u32 maxthreads; } mcpipe_t;

/* ---- gravação de comandos */
typedef struct { id obj; u64 off; void *bytes; } bind_t;
typedef struct rstate {
    mrpipe_t *pipe;
    bind_t vb[31], fb[31];
    mtex_t *vt[32], *ft[32];
    msamp_t *vs[16], *fs[16];
    mtl_vp_t vp;
    mtl_sc_t sc;
    int vp_set, sc_set, cull, wind, fill, dclip;
    float dbias, dslope, dclamp;
    mdss_t *dss;
    u32 sref_f, sref_b;
    float bc[4];
} rstate_t;
typedef struct cstate {
    mcpipe_t *pipe;
    bind_t b[31];
    mtex_t *t[32];
    msamp_t *s[16];
    u64 tglen[31];
} cstate_t;
typedef struct {
    mtex_t *tex, *resolve;
    int level, slice, plane, load, store, rlevel, rslice, rplane;
    mtl_cc_t cc;
    double cd;
    u32 cs;
} attach_t;
typedef struct { attach_t c[8], d, s; u64 rtw, rth; } rpass_t;

enum { C_RBEGIN = 1, C_DRAW, C_REND, C_DISPATCH, C_COPY_BB, C_COPY_BT, C_COPY_TB, C_COPY_TT, C_FILL, C_MIPS, C_SIGNAL };
typedef struct cmd {
    int k;
    union {
        rpass_t *rp;
        struct {
            rstate_t *st;
            int prim, indexed, itype;
            u64 vstart, vcount, icount, binst, ioff, icnt, indoff;
            i64 bvert;
            mbuf_t *ib, *indirect;
        } d;
        struct { cstate_t *st; u32 g[3], t[3], grid[3]; int exact; mbuf_t *indirect; u64 indoff; } c;
        struct { mbuf_t *sb, *db; u64 so, dof, n; } bb;
        struct { mbuf_t *b; u64 off, bpr, bpi, slice, level; mtl_size_t sz; mtex_t *t; mtl_origin_t org; } bt;
        struct { mtex_t *s, *d; u64 ss, sl, ds, dl, nslices, nlevels; mtl_origin_t so, dorg; mtl_size_t sz; int whole; } tt;
        struct { mbuf_t *b; u64 off, n; u8 v; } fill;
        struct { mtex_t *t; } mips;
        struct { id ev; u64 v; } sig;
    };
} cmd_t;

/* formatos de pixel */
int mx_bpp(int fmt);
int mx_isdepth(int fmt);
int mx_hasstencil(int fmt);
void mx_tex_layout(mtex_t *t, u64 bpr0);
u64 mx_tex_bytes(mtex_t *t);
u8 *mx_texel(const msl_tex_t *t, int x, int y, int z, int slice, int lvl);
void mx_to_bgra8(mtex_t *src, u8 *dst, u64 dbpr);
void mx_exec_cmd(cmd_t *c);
void mx_signal_event(id ev, u64 v);

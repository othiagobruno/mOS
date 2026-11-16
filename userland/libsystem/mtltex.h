/* mOS libSystem — formatos de pixel e amostragem de textura, compartilhados pelo Metal por software
 * (metalexec.c) e pelo OpenGL por software (gl*.c). Não depende do runtime ObjC. */
#pragma once
#include "mtlshader.h"

enum { PK_NONE = 0, PK_UN8, PK_SN8, PK_U8, PK_S8, PK_UN16, PK_SN16, PK_U16, PK_S16, PK_HALF, PK_U32, PK_S32, PK_F32,
       PK_RGB10A2, PK_RGB10A2U, PK_RG11B10F, PK_RGB9E5, PK_B5G6R5, PK_A1BGR5, PK_ABGR4, PK_BGR5A1,
       PK_D16, PK_D32F, PK_ST8, PK_D32FS8, PK_D24S8, PK_X32S8, PK_X24S8, PK_BGRA10XR, PK_BGR10XR };
enum { PF_BGRA = 1, PF_SRGB = 2, PF_ALPHA = 4 };
typedef struct { u16 fmt; u8 bpp, nc, kind, fl; } pf_t;
const pf_t *mx_pf(int fmt);
void mx_decode(const pf_t *p, const u8 *q, double o[4]);
void mx_encode(const pf_t *p, u8 *q, const double in[4]);
void mx_srgb_init(void);
int mx_bpp(int fmt);
int mx_isdepth(int fmt);
int mx_hasstencil(int fmt);
u8 *mx_texel(const msl_tex_t *t, int x, int y, int z, int slice, int lvl);
static inline double mx_cl01(double v) { return v != v ? 0 : v < 0 ? 0 : v > 1 ? 1 : v; }
static inline double mx_clm11(double v) { return v != v ? 0 : v < -1 ? -1 : v > 1 ? 1 : v; }

/* mOS ColorSync — motor de cor (colorsync.c). Sem CoreFoundation: o teste de host (tools/colorsync_host.c)
 * liga só isto. PCS = XYZ relativo a D50, como no ICC. */
#ifndef MOS_COLORSYNC_H
#define MOS_COLORSYNC_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int type;            /* 0 identidade, 1 gama, 2 tabela, 3 paramétrica (ICC tipos 0..4), 4 PQ, 5 HLG */
    double g;            /* gama */
    int n;
    float *tab;          /* tabela 0..1 com n entradas */
    int ptype;
    double p[7];         /* g, a, b, c, d, e, f */
} cs_curve;

typedef struct {
    int kind;            /* 1 mft1/mft2 (lut8/lut16), 2 mAB/mBA */
    int in, out, grid[16];
    cs_curve incv[16], outcv[16], mcv[3];
    float *clut;         /* grid^in × out, 0..1 */
    double mat[12];      /* mAB: 3×3 + offset; lut16: 3×3 só com entrada XYZ */
    int has_mat, has_m;
} cs_lut;

typedef struct cs_prof {
    uint32_t space;      /* assinatura ICC do espaço do dispositivo: 'RGB ', 'GRAY', 'CMYK', 'Lab ', 'XYZ ' */
    uint32_t pcs;        /* 'XYZ ' ou 'Lab ' */
    uint32_t cls;        /* 'mntr', 'scnr', 'prtr', 'spac', 'abst', 'link' */
    int nchan;
    int has_mtx;
    double mtx[9];       /* dispositivo linear → XYZ (colunas r, g, b) */
    cs_curve trc[3];     /* RGB; GRAY usa trc[0] */
    double wp[3];        /* ponto branco do meio (wtpt), D50 nos v4 */
    cs_lut a2b, b2a;
    int has_a2b, has_b2a;
    int naive_cmyk;      /* CMYK sem LUT: (1−C)(1−K) em sRGB */
    int lab;             /* espaço Lab (CGColorSpaceCreateLab): wp é o branco de referência */
    double range[4];     /* Lab: a e b mínimo/máximo */
    char desc[128];
    uint32_t version;
} cs_prof;

/* 0 = ok. intent: 0 perceptual (A2B0), 1 colorimétrico relativo (A2B1), 2 saturação (A2B2), 3 absoluto
 * (A2B1 + branco do meio); sem a tabela da intenção, usa a A2B0 */
int cs_parse(const uint8_t *d, size_t n, cs_prof *out);
int cs_parse_intent(const uint8_t *d, size_t n, int intent, cs_prof *out);
void cs_free(cs_prof *p);
/* perfis embutidos pelo nome do CGColorSpace/ColorSync ("kCGColorSpaceSRGB", "sRGB"...): 0 = ok */
int cs_builtin(const char *name, cs_prof *out);
/* calibrados (CGColorSpaceCreateCalibratedRGB/Gray) e Lab */
void cs_calibrated_rgb(cs_prof *p, const double *white, const double *gamma, const double *matrix);
void cs_calibrated_gray(cs_prof *p, const double *white, double gamma);
void cs_lab(cs_prof *p, const double *white, const double *range);
/* ICC v2 de um perfil matriz/TRC ou cinza (o que cs_builtin/cs_calibrated_* montam); malloc em *out */
int cs_write_icc(const cs_prof *p, uint8_t **out, size_t *n);

/* dispositivo (0..1 por canal; Lab em L 0..100, a/b crus) ↔ XYZ D50 */
void cs_to_xyz(const cs_prof *p, const double *in, double *xyz);
void cs_from_xyz(const cs_prof *p, const double *xyz, double *out);
/* atalho: dispositivo → sRGB 0..1 (com recorte) */
void cs_to_srgb(const cs_prof *p, const double *in, double *rgb);
/* 1 se o perfil é (praticamente) o sRGB: nada a converter */
int cs_is_srgb(const cs_prof *p);

double cs_curve_eval(const cs_curve *c, double x);
double cs_curve_inv(const cs_curve *c, double y);
void cs_md5(const uint8_t *d, size_t n, uint8_t out[16]);
/* ID do perfil (ICC 4.x): MD5 com flags, intenção e o próprio ID zerados */
void cs_profile_id(const uint8_t *d, size_t n, uint8_t out[16]);

#endif

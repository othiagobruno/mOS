/*
 * H.264 auxiliar do libSystem: largura e altura de um SPS (CMVideoFormatDescriptionCreateFromH264ParameterSets) e a cópia
 * NV12 -> I420 (VTCompressionSession). Codificar e decodificar: OpenH264 corrigido (host-audio/h264_glue.cpp,
 * h264enc_glue.cpp). Código original do mOS (MIT).
 */
#include "h264sw.h"
#include <stdlib.h>
#include <string.h>

typedef struct { const uint8_t *b; int nbytes, bit, err; } br;
static int br_get(br *r, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        int bi = r->bit++;
        if ((bi >> 3) >= r->nbytes) { r->err = 1; return 0; }
        v = (v << 1) | ((r->b[bi >> 3] >> (7 - (bi & 7))) & 1);
    }
    return v;
}
static unsigned br_ue(br *r) {
    int z = 0;
    while (z < 31 && !br_get(r, 1)) z++;
    if (r->err || z >= 31) { r->err = 1; return 0; }
    unsigned x = 1;
    for (int i = 0; i < z; i++) x = (x << 1) | (unsigned)br_get(r, 1);
    return x - 1;
}
static int br_se(br *r) { unsigned k = br_ue(r); return k & 1 ? (int)((k + 1) / 2) : -(int)(k / 2); }
static uint8_t *rbsp_unescape(const uint8_t *nal, int n, int *out_n) {
    uint8_t *o = malloc((size_t)n + 1);
    if (!o) return 0;
    int w = 0, z = 0;
    for (int i = 0; i < n; i++) {
        if (z >= 2 && nal[i] == 3) { z = 0; continue; }
        o[w++] = nal[i];
        z = nal[i] == 0 ? z + 1 : 0;
    }
    *out_n = w;
    return o;
}
static void skip_scaling_list(br *r, int size) {
    int last = 8, next = 8;
    for (int j = 0; j < size; j++) {
        if (next) next = (last + br_se(r) + 256) % 256;
        last = next ? next : last;
    }
}

/* seq_parameter_set_rbsp (H.264 7.3.2.1.1) até o recorte; nal com o cabeçalho de 1 byte, ainda com emulação */
int h264_sps_wh(const uint8_t *nal, int n, int *w, int *h) {
    int rn = 0;
    uint8_t *rb = rbsp_unescape(nal, n, &rn);
    if (!rb || rn < 4 || (rb[0] & 31) != 7) { free(rb); return -1; }
    br r = {rb + 1, rn - 1, 0, 0};
    int profile = br_get(&r, 8);
    br_get(&r, 8); br_get(&r, 8);   /* restrições, nível */
    br_ue(&r);                       /* seq_parameter_set_id */
    unsigned chroma = 1;
    if (profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 || profile == 83 || profile == 86 ||
        profile == 118 || profile == 128 || profile == 138 || profile == 139 || profile == 134 || profile == 135) {
        chroma = br_ue(&r);
        if (chroma == 3) br_get(&r, 1);   /* separate_colour_plane_flag */
        br_ue(&r); br_ue(&r);             /* bit_depth_luma/chroma_minus8 */
        br_get(&r, 1);                    /* qpprime_y_zero_transform_bypass_flag */
        if (br_get(&r, 1))                /* seq_scaling_matrix_present_flag */
            for (int i = 0; i < (chroma != 3 ? 8 : 12); i++)
                if (br_get(&r, 1)) skip_scaling_list(&r, i < 6 ? 16 : 64);
    }
    br_ue(&r);                            /* log2_max_frame_num_minus4 */
    unsigned poc = br_ue(&r);
    if (poc == 0) br_ue(&r);
    else if (poc == 1) {
        br_get(&r, 1); br_se(&r); br_se(&r);
        unsigned cyc = br_ue(&r);
        if (cyc > 255) { free(rb); return -1; }
        for (unsigned i = 0; i < cyc; i++) br_se(&r);
    }
    br_ue(&r);                            /* max_num_ref_frames */
    br_get(&r, 1);                        /* gaps_in_frame_num_value_allowed_flag */
    unsigned mbw = br_ue(&r) + 1, map_h = br_ue(&r) + 1;
    int frame_mbs_only = br_get(&r, 1);
    if (!frame_mbs_only) br_get(&r, 1);   /* mb_adaptive_frame_field_flag */
    br_get(&r, 1);                        /* direct_8x8_inference_flag */
    unsigned cl = 0, cr = 0, ct = 0, cb = 0;
    if (br_get(&r, 1)) { cl = br_ue(&r); cr = br_ue(&r); ct = br_ue(&r); cb = br_ue(&r); }
    free(rb);
    if (r.err) return -1;
    /* unidades de recorte (tabela 6-1): 4:2:0 2x2, 4:2:2 2x1, 4:4:4 e monocromático 1x1; campos dobram o vertical */
    unsigned cux = chroma == 1 || chroma == 2 ? 2 : 1, cuy = (chroma == 1 ? 2 : 1) * (2 - (unsigned)frame_mbs_only);
    long fw = (long)mbw * 16 - (long)(cl + cr) * cux, fh = (long)(2 - frame_mbs_only) * map_h * 16 - (long)(ct + cb) * cuy;
    if (fw <= 0 || fh <= 0) return -1;
    *w = (int)fw;
    *h = (int)fh;
    return 0;
}

void h264_nv12_to_i420(const uint8_t *yp, int sy, const uint8_t *uv, int suv,
                       int w, int h, uint8_t *y, uint8_t *u, uint8_t *v, int dy, int du, int dv) {
    for (int row = 0; row < h; row++) memcpy(y + row * dy, yp + row * sy, (size_t)w);
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        u[row * du + col] = uv[row * suv + col * 2];
        v[row * dv + col] = uv[row * suv + col * 2 + 1];
    }
}

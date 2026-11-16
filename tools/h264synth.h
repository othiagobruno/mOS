/* H.264 sintético para testes de host (sem codificador e sem ffmpeg gerando vídeo): SPS/PPS Baseline, IDR com todos os
 * macroblocos I_PCM (a imagem decodificada é exatamente a de entrada) e P com todos os macroblocos em skip (repete a
 * referência). Saída em AVCC (tamanho de 4 bytes + NAL com prevenção de emulação). Usado por tools/avwcore_test.c e
 * tools/avwriter_host_test.c. Código original do mOS (MIT). */
#pragma once
#include <stdint.h>
#include <string.h>

typedef struct { uint8_t b[1 << 18]; int bit; } hs_bw;
static void hs_put(hs_bw *w, uint32_t v, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (v >> i & 1) w->b[w->bit >> 3] |= (uint8_t)(0x80 >> (w->bit & 7));
        w->bit++;
    }
}
static void hs_ue(hs_bw *w, uint32_t v) { uint32_t x = v + 1; int len = 0; while (x >> len) len++; hs_put(w, 0, len - 1); hs_put(w, x, len); }
static void hs_se(hs_bw *w, int v) { hs_ue(w, v > 0 ? (uint32_t)(2 * v - 1) : (uint32_t)(-2 * v)); }
static void hs_trailing(hs_bw *w) { hs_put(w, 1, 1); while (w->bit & 7) hs_put(w, 0, 1); }
static void hs_align0(hs_bw *w) { while (w->bit & 7) hs_put(w, 0, 1); }
/* NAL com prevenção de emulação e prefixo de tamanho de 4 bytes */
static uint32_t hs_nal(uint8_t *out, int hdr, const hs_bw *w) {
    uint32_t n = 4;
    out[n++] = (uint8_t)hdr;
    int z = 0, len = w->bit >> 3;
    for (int i = 0; i < len; i++) {
        if (z >= 2 && w->b[i] <= 3) { out[n++] = 3; z = 0; }
        out[n++] = w->b[i];
        z = w->b[i] == 0 ? z + 1 : 0;
    }
    uint32_t L = n - 4;
    out[0] = (uint8_t)(L >> 24); out[1] = (uint8_t)(L >> 16); out[2] = (uint8_t)(L >> 8); out[3] = (uint8_t)L;
    return n;
}
/* SPS e PPS sem o prefixo de tamanho (com o cabeçalho de 1 byte) */
static void hs_params(int w, int h, uint8_t *sps, uint32_t *sps_n, uint8_t *pps, uint32_t *pps_n) {
    static hs_bw b;
    static uint8_t t[128];
    memset(&b, 0, sizeof b);
    hs_put(&b, 66, 8); hs_put(&b, 0xc0, 8); hs_put(&b, 30, 8);   /* Baseline, constraint_set0/1, nível 3.0 */
    hs_ue(&b, 0); hs_ue(&b, 0); hs_ue(&b, 2); hs_ue(&b, 1); hs_put(&b, 0, 1);   /* id, log2_max_frame_num-4, poc tipo 2, 1 ref */
    hs_ue(&b, (uint32_t)(w / 16 - 1)); hs_ue(&b, (uint32_t)(h / 16 - 1)); hs_put(&b, 1, 1); hs_put(&b, 1, 1); hs_put(&b, 0, 1); hs_put(&b, 0, 1);
    hs_trailing(&b);
    *sps_n = hs_nal(t, 0x67, &b) - 4; memcpy(sps, t + 4, *sps_n);
    memset(&b, 0, sizeof b);
    hs_ue(&b, 0); hs_ue(&b, 0); hs_put(&b, 0, 1); hs_put(&b, 0, 1); hs_ue(&b, 0); hs_ue(&b, 0); hs_ue(&b, 0); hs_put(&b, 0, 1); hs_put(&b, 0, 2);
    hs_se(&b, 0); hs_se(&b, 0); hs_se(&b, 0); hs_put(&b, 1, 1); hs_put(&b, 0, 1); hs_put(&b, 0, 1);
    hs_trailing(&b);
    *pps_n = hs_nal(t, 0x68, &b) - 4; memcpy(pps, t + 4, *pps_n);
}
static uint32_t hs_avcc(uint8_t *o, const uint8_t *sps, uint32_t sps_n, const uint8_t *pps, uint32_t pps_n) {
    uint32_t n = 0;
    o[n++] = 1; o[n++] = sps[1]; o[n++] = sps[2]; o[n++] = sps[3]; o[n++] = 0xff; o[n++] = 0xe1;
    o[n++] = (uint8_t)(sps_n >> 8); o[n++] = (uint8_t)sps_n; memcpy(o + n, sps, sps_n); n += sps_n;
    o[n++] = 1; o[n++] = (uint8_t)(pps_n >> 8); o[n++] = (uint8_t)pps_n; memcpy(o + n, pps, pps_n); n += pps_n;
    return n;
}
/* IDR: planos I420 contíguos (y w*h, u e v w/2*h/2) */
static uint32_t hs_idr(uint8_t *out, int w, int h, const uint8_t *y, const uint8_t *u, const uint8_t *v, int idr_id) {
    static hs_bw b;
    memset(&b, 0, sizeof b);
    hs_ue(&b, 0); hs_ue(&b, 7); hs_ue(&b, 0); hs_put(&b, 0, 4); hs_ue(&b, (uint32_t)idr_id);
    hs_put(&b, 0, 1); hs_put(&b, 0, 1); hs_se(&b, 0); hs_ue(&b, 1);   /* dec_ref_pic_marking, qp, sem deblocking */
    int mbw = w / 16, nmb = mbw * (h / 16);
    for (int mb = 0; mb < nmb; mb++) {
        int mx = mb % mbw * 16, my = mb / mbw * 16;
        hs_ue(&b, 25); hs_align0(&b);
        for (int r = 0; r < 16; r++) for (int c = 0; c < 16; c++) hs_put(&b, y[(my + r) * w + mx + c], 8);
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) hs_put(&b, u[(my / 2 + r) * (w / 2) + mx / 2 + c], 8);
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) hs_put(&b, v[(my / 2 + r) * (w / 2) + mx / 2 + c], 8);
    }
    hs_trailing(&b);
    return hs_nal(out, 0x65, &b);
}
static uint32_t hs_pskip(uint8_t *out, int w, int h, int frame_num) {
    static hs_bw b;
    memset(&b, 0, sizeof b);
    hs_ue(&b, 0); hs_ue(&b, 5); hs_ue(&b, 0); hs_put(&b, (uint32_t)frame_num & 15, 4);
    hs_put(&b, 0, 1); hs_put(&b, 0, 1); hs_put(&b, 0, 1); hs_se(&b, 0); hs_ue(&b, 1);
    hs_ue(&b, (uint32_t)((w / 16) * (h / 16)));
    hs_trailing(&b);
    return hs_nal(out, 0x41, &b);
}

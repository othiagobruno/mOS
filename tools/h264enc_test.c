/* Teste de host do codificador H.264 (casca cx_h264enc_*, OpenH264 2.6.0). Rodado por tools/h264_test.sh.
 * uso: h264enc_test fonte.yuv w h quadros fps perfil(66|77|100) bitrate(0 = qp 24) keyint forçar_idr_no_quadro saida.264
 * Saída: "h264enc ok <bytes> <kbps> idr:<lista>" — o resto (perfil, PSNR) o script confere com ffprobe/ffmpeg. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "h264_glue.h"
static FILE *out; static long bytes; static int nals_sps, nals_pps;
static void on_nal(void *ctx, const uint8_t *n, int len, int type) {
    (void)ctx;
    static const uint8_t sc[4] = {0, 0, 0, 1};
    fwrite(sc, 1, 4, out); fwrite(n, 1, len, out); bytes += len + 4;
    if (type == 7) nals_sps++;
    if (type == 8) nals_pps++;
}
int main(int argc, char **argv) {
    if (argc < 11) return 2;
    int w = atoi(argv[2]), h = atoi(argv[3]), nf = atoi(argv[4]), force = atoi(argv[9]);
    cx_h264enc_cfg c = {w, h, atof(argv[5]), atoi(argv[7]), 24, atoi(argv[8]), atoi(argv[6]), 0, 0};
    cx_h264enc *e = cx_h264enc_open(&c);
    if (!e) { puts("h264enc FALHA abrir"); return 1; }
    FILE *in = fopen(argv[1], "rb");
    out = fopen(argv[10], "wb");
    long fs = (long)w * h * 3 / 2;
    unsigned char *b = malloc(fs);
    char idrs[512] = "";
    for (int i = 0; i < nf && fread(b, 1, fs, in) == (size_t)fs; i++) {
        int idr = 0;
        if (cx_h264enc_encode(e, b, b + w * h, b + w * h * 5 / 4, w, w / 2, (uint64_t)(i * 1000 / c.fps), i == force, on_nal, 0, &idr)) { printf("h264enc FALHA quadro %d\n", i); return 1; }
        if (idr && strlen(idrs) < 480) { char t[12]; snprintf(t, sizeof t, "%s%d", idrs[0] ? "," : "", i); strcat(idrs, t); }
    }
    cx_h264enc_close(e);
    fclose(out);
    if (!nals_sps || !nals_pps) { puts("h264enc FALHA sem SPS/PPS"); return 1; }
    printf("h264enc ok %ld %.0f idr:%s\n", bytes, bytes * 8.0 / (nf / c.fps) / 1000, idrs);
    return 0;
}

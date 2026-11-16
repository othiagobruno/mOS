/* Teste de host da casca cx_h264_* (userland/host-audio/h264_glue.cpp), rodado por tools/h264_test.sh.
 * Parte o Annex B em unidades de acesso (como o VideoToolbox recebe, uma amostra por chamada) e confere:
 *  - ordem de exibição: o I420 inteiro igual à referência do ffmpeg e os carimbos formando uma permutação de 0..N-1;
 *  - ordem de decodificação: cada quadro sai na própria chamada, com o carimbo dela (0, 1, 2, ...);
 *  - depois do cx_h264_flush, o mesmo fluxo de novo dá o mesmo resultado (o decodificador continua utilizável).
 * uso: h264_glue_test fluxo.264 referencia.yuv [faixa,matriz,primárias,transferência]  → "glue ok ..." */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "h264_glue.h"
typedef struct { unsigned char *buf; long len, cap; int n; uint64_t tags[4096]; long cur_call; int bad_order; cx_h264_color color; } sink_t;
static void emit(void *ctx, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, int w, int h, uint64_t tag, const cx_h264_color *col) {
    sink_t *s = ctx;
    s->color = *col;
    long need = s->len + (long)w * h * 3 / 2;
    if (need > s->cap) { s->cap = need * 2; s->buf = realloc(s->buf, s->cap); }
    for (int r = 0; r < h; r++) { memcpy(s->buf + s->len, y + (long)r * sy, w); s->len += w; }
    for (int r = 0; r < h / 2; r++) { memcpy(s->buf + s->len, u + (long)r * suv, w / 2); s->len += w / 2; }
    for (int r = 0; r < h / 2; r++) { memcpy(s->buf + s->len, v + (long)r * suv, w / 2); s->len += w / 2; }
    if (s->n < 4096) s->tags[s->n] = tag;
    if (s->cur_call >= 0 && (long)tag != s->cur_call) s->bad_order = 1;
    s->n++;
}
static long next_nal(const unsigned char *b, long n, long at) {
    for (long e = at + 3; e + 3 < n; e++)
        if (b[e] == 0 && b[e + 1] == 0 && (b[e + 2] == 1 || (b[e + 2] == 0 && b[e + 3] == 1))) return e;
    return n;
}
static int hdr(const unsigned char *b, long n, long at) {   /* deslocamento do cabeçalho da NAL depois do start code */
    long i = at;
    while (i < n && b[i] == 0) i++;
    return (int)(i + 1 - at);
}
/* fronteiras das unidades de acesso: AUD/SPS/PPS/SEI ou fatia com first_mb_in_slice 0 depois de uma fatia */
static int split(const unsigned char *b, long n, long *starts) {
    int na = 0, had_vcl = 0;
    for (long at = 0; at < n; at = next_nal(b, n, at)) {
        int h = hdr(b, n, at);
        if (at + h + 1 >= n) break;
        int t = b[at + h] & 31;   /* b[at + h] é o cabeçalho da NAL; o byte seguinte começa a fatia */
        int vcl = t == 1 || t == 5;
        int first = vcl && (b[at + h + 1] & 0x80);
        if ((had_vcl && (t == 9 || t == 7 || t == 8 || t == 6 || first)) || na == 0) { starts[na++] = at; had_vcl = 0; }
        if (vcl) had_vcl = 1;
    }
    starts[na] = n;
    return na;
}
static int run(const unsigned char *b, long n, long *st, int na, int order, sink_t *s, int twice) {
    cx_h264 *d = cx_h264_open(order);
    if (!d) return -1;
    for (int pass = 0; pass < (twice ? 2 : 1); pass++) {
        for (int i = 0; i < na; i++) {
            s->cur_call = order ? i + (long)pass * na : -1;
            if (cx_h264_decode(d, b + st[i], st[i + 1] - st[i], (uint64_t)(i + (long)pass * na), emit, s)) { cx_h264_close(d); return -2; }
        }
        s->cur_call = -1;
        cx_h264_flush(d, emit, s);
    }
    cx_h264_close(d);
    return 0;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
    f = fopen(argv[2], "rb");
    fseek(f, 0, SEEK_END); long rn = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *ref = malloc(rn); if (fread(ref, 1, rn, f) != (size_t)rn) return 2; fclose(f);
    static long st[8192];
    int na = split(b, n, st);
    sink_t disp = {0}, dec = {0}, rep = {0}, rep2 = {0};
    if (run(b, n, st, na, 0, &disp, 0) || run(b, n, st, na, 1, &dec, 0)) { puts("glue FALHA decodificação"); return 1; }
    int ok = 1;
    if (disp.len != rn || memcmp(disp.buf, ref, rn)) { printf("glue FALHA exibição: %ld bytes, referência %ld\n", disp.len, rn); ok = 0; }
    char seen[4096] = {0};
    for (int i = 0; i < disp.n && i < 4096; i++) if (disp.tags[i] < 4096) seen[disp.tags[i]]++;
    for (int i = 0; i < na; i++) if (seen[i] != 1) { printf("glue FALHA carimbo %d saiu %d vezes na exibição\n", i, seen[i]); ok = 0; break; }
    if (dec.n != na || dec.bad_order) { printf("glue FALHA ordem de decodificação: %d quadros para %d amostras, fora de ordem %d\n", dec.n, na, dec.bad_order); ok = 0; }
    /* o mesmo fluxo duas vezes no mesmo decodificador, com flush no meio */
    if (run(b, n, st, na, 0, &rep, 1) || rep.len != 2 * rn || memcmp(rep.buf, ref, rn) || memcmp(rep.buf + rn, ref, rn)) {
        printf("glue FALHA repetição em exibição: %ld bytes\n", rep.len); ok = 0;
    }
    if (run(b, n, st, na, 1, &rep2, 1) || rep2.n != 2 * na || rep2.bad_order) { printf("glue FALHA repetição em decodificação: %d quadros\n", rep2.n); ok = 0; }
    if (argc > 3) {   /* cor esperada: faixa,matriz,primárias,transferência */
        int e[4];
        if (sscanf(argv[3], "%d,%d,%d,%d", &e[0], &e[1], &e[2], &e[3]) == 4 &&
            (disp.color.full_range != e[0] || disp.color.matrix != e[1] || disp.color.primaries != e[2] || disp.color.transfer != e[3])) {
            printf("glue FALHA cor %d,%d,%d,%d (esperado %s)\n", disp.color.full_range, disp.color.matrix, disp.color.primaries, disp.color.transfer, argv[3]);
            ok = 0;
        }
    }
    if (ok) printf("glue ok %d amostras: exibição bit-exata, carimbos completos, ordem de decodificação 1 quadro por chamada, flush e repetição\n", na);
    return !ok;
}

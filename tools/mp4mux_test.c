/* Teste de host do mp4mux.c: copia as faixas de um MP4 (lido pelo mp4demux.c) para um arquivo novo e confere que o
 * demuxer lê de volta as mesmas amostras (tamanho, bytes, pts, dts, quadro-chave). O script compara a decodificação do
 * ffmpeg (framemd5) dos dois arquivos. uso: mp4mux_test entrada saida mp4|mov|m4a faststart(0|1)  → "mp4mux ok ..." */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp4demux.h"
#include "mp4mux.h"
static unsigned char *slurp(const char *p, long *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != (size_t)*n) return 0; fclose(f); return b; }
int main(int argc, char **argv) {
    long n; unsigned char *b = slurp(argv[1], &n);
    mp4_file m;
    if (!b || mp4_open_mem(&m, b, n)) { puts("mp4mux FALHA entrada"); return 1; }
    int kind = !strcmp(argv[3], "mov") ? MP4W_MOV : !strcmp(argv[3], "m4a") ? MP4W_M4A : MP4W_MP4;
    mp4w_t *w = mp4w_open(argv[2], kind, atoi(argv[4]));
    if (!w) { puts("mp4mux FALHA abrir"); return 1; }
    int map[8], nt = 0;
    for (uint32_t i = 0; i < m.ntracks && nt < 8; i++) {
        mp4_track *t = &m.t[i];
        int r = -1;
        if (t->codec == MP4_FOURCC('a', 'v', 'c', '1')) r = mp4w_add_h264(w, (int)t->width, (int)t->height, t->cfg, t->cfg_len, t->timescale);
        else if (t->codec == MP4_FOURCC('m', 'p', '4', 'a') && t->oti == 0x40)
            r = mp4w_add_aac(w, (int)t->sample_rate, (int)t->channels, t->cfg, t->cfg_len, 128000, t->edit_media_time > 0 ? (uint32_t)t->edit_media_time : 0);
        else if (t->codec == MP4_FOURCC('O', 'p', 'u', 's')) r = mp4w_add_opus(w, (int)t->channels, t->cfg_len >= 4 ? (uint32_t)(t->cfg[2] << 8 | t->cfg[3]) : 312, 48000);
        if (r < 0) { printf("mp4mux FALHA faixa %u (%.4s)\n", i, (char *)&t->codec); return 1; }
        map[nt++] = (int)i;
    }
    /* intercala por dts, como um gravador */
    uint32_t idx[8] = {0};
    unsigned char *s = malloc(1 << 22);
    long written = 0;
    for (;;) {
        int best = -1; double bt = 1e300;
        for (int k = 0; k < nt; k++) { mp4_track *t = &m.t[map[k]]; if (idx[k] < t->nsamples) { double d = (double)t->s[idx[k]].dts / t->timescale; if (d < bt) { bt = d; best = k; } } }
        if (best < 0) break;
        mp4_track *t = &m.t[map[best]];
        mp4_sample *x = &t->s[idx[best]];
        mp4_read_sample(&m, t, idx[best], s);
        if (mp4w_write(w, best, s, x->size, x->dts + x->cto, x->dts, x->dur, x->sync)) { puts("mp4mux FALHA write"); return 1; }
        idx[best]++; written++;
    }
    if (mp4w_close(w)) { puts("mp4mux FALHA close"); return 1; }
    long n2; unsigned char *b2 = slurp(argv[2], &n2);
    mp4_file m2;
    if (!b2 || mp4_open_mem(&m2, b2, n2) || m2.ntracks != (uint32_t)nt) { puts("mp4mux FALHA reler"); return 1; }
    for (int k = 0; k < nt; k++) {
        mp4_track *a = &m.t[map[k]], *c = &m2.t[k];
        if (a->nsamples != c->nsamples || a->codec != c->codec || a->cfg_len != c->cfg_len) { printf("mp4mux FALHA faixa %d difere\n", k); return 1; }
        unsigned char *s2 = malloc(1 << 22);
        for (uint32_t i = 0; i < a->nsamples; i++) {
            mp4_read_sample(&m, a, i, s); mp4_read_sample(&m2, c, i, s2);
            if (a->s[i].size != c->s[i].size || memcmp(s, s2, a->s[i].size) || a->s[i].cto != c->s[i].cto || a->s[i].dur != c->s[i].dur ||
                (a->codec == MP4_FOURCC('a', 'v', 'c', '1') && a->s[i].sync != c->s[i].sync)) { printf("mp4mux FALHA amostra %u da faixa %d\n", i, k); return 1; }
        }
        if (a->edit_media_time != c->edit_media_time && !(a->edit_media_time <= 0 && c->edit_media_time <= 0)) { printf("mp4mux FALHA edit list %lld vs %lld\n", (long long)a->edit_media_time, (long long)c->edit_media_time); return 1; }
    }
    printf("mp4mux ok %s%s: %d faixas, %ld amostras idênticas (bytes, pts, dts, chave, edit list), %ld bytes\n", argv[3], atoi(argv[4]) ? " faststart" : "", nt, written, n2);
    return 0;
}

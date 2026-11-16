/* cx_pdec (decodificação por pacote, usada pelo AudioConverter) contra o cx_audiodec (arquivo inteiro, validado contra
 * o ffmpeg em tools/midia_audio_test.sh). Mesmo decodificador: depois do priming da edit list, as amostras têm de ser
 * iguais. uso: midia_pdec_test arquivo.mp4  → "pdec ok ..." */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp4demux.h"
typedef struct { int kind, status; const char *limit; int rate, channels, frames; short *pcm; } cx_audiodec_out;
int cx_audiodec(const void *b, unsigned long long n, cx_audiodec_out *o);
typedef struct cx_pdec cx_pdec;
cx_pdec *cx_pdec_open(uint32_t fmt, const uint8_t *cookie, int cookie_n, double rate, int ch);
int cx_pdec_decode(cx_pdec *d, const uint8_t *pkt, int n, int16_t *pcm, int max_frames, int *ch, int *rate);
void cx_pdec_close(cx_pdec *d);
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
    cx_audiodec_out ref; if (!cx_audiodec(b, n, &ref) || ref.status) { puts("FALHA ref"); return 1; }
    mp4_file m; if (mp4_open_mem(&m, b, n)) { puts("FALHA mp4"); return 1; }
    mp4_track *t = &m.t[mp4_find_track(&m, MP4_FOURCC('s', 'o', 'u', 'n'))];
    uint32_t fmt = t->codec == MP4_FOURCC('m', 'p', '4', 'a') ? (t->oti == 0x6B || t->oti == 0x69 ? MP4_FOURCC('.', 'm', 'p', '3') : MP4_FOURCC('a', 'a', 'c', ' '))
                 : t->codec == MP4_FOURCC('O', 'p', 'u', 's') ? MP4_FOURCC('o', 'p', 'u', 's') : t->codec;
    const uint8_t *ck = fmt == MP4_FOURCC('o', 'p', 'u', 's') ? 0 : t->cfg;
    int ckn = ck ? (int)t->cfg_len : 0;
    cx_pdec *d = cx_pdec_open(fmt, ck, ckn, t->sample_rate, (int)t->channels);
    if (!d) { puts("FALHA open"); return 1; }
    size_t cap = 1 << 20, len = 0; int16_t *pcm = malloc(cap * 2 * 8); int ch = 0, rate = 0;
    uint8_t *pk = malloc(1 << 20);
    for (uint32_t i = 0; i < t->nsamples; i++) {
        mp4_read_sample(&m, t, i, pk);
        int c = 0, r = 0, got = cx_pdec_decode(d, pk, (int)t->s[i].size, pcm + len * (ch ? ch : 8), 8192, &c, &r);
        if (got > 0) { if (!ch) ch = c, rate = r; len += (size_t)got; }
    }
    cx_pdec_close(d);
    long skip = t->edit_media_time > 0 ? (long)(t->edit_media_time * (double)rate / t->timescale + 0.5) : 0;
    if (fmt == MP4_FOURCC('o', 'p', 'u', 's') && t->edit_media_time < 0) skip = 312;
    int diff = 0; long m2 = ref.frames;
    if (ch != ref.channels || rate != ref.rate) { printf("FALHA formato %d/%d vs %d/%d\n", ch, rate, ref.channels, ref.rate); return 1; }
    if ((long)len - skip < m2) { printf("FALHA curto %ld vs %ld\n", (long)len - skip, m2); return 1; }
    for (long i = 0; i < m2 * ch; i++) if (pcm[skip * ch + i] != ref.pcm[i]) diff++;
    printf("%s %d Hz %d ch %ld quadros, %d amostras diferentes\n", diff ? "FALHA" : "pdec ok", rate, ch, m2, diff);
    return diff != 0;
}

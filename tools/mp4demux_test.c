/* Teste de host do demuxer ISO BMFF. Imprime cada amostra como o ffprobe -show_packets:
 *   faixa,pos,tamanho,pts(ticks da faixa),K|_
 * tools/mp4demux_test.sh gera arquivos com o ffmpeg e compara as duas listas.
 * gcc -O1 -Iuserland/libsystem tools/mp4demux_test.c userland/libsystem/mp4demux.c -o /tmp/mp4demux_test */
#include <stdio.h>
#include <stdlib.h>
#include "mp4demux.h"

static int file_read(void *ctx, uint64_t off, void *dst, uint32_t n) {
    FILE *f = ctx;
    if (fseeko(f, (off_t)off, SEEK_SET)) return -1;
    return fread(dst, 1, n, f) == n ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) return 2;
    fseeko(fp, 0, SEEK_END);
    uint64_t size = (uint64_t)ftello(fp);
    mp4_file f;
    int rc = mp4_open(&f, file_read, fp, size);
    if (rc) { fprintf(stderr, "mp4_open %d %s\n", rc, f.err ? f.err : ""); return 1; }
    for (uint32_t i = 0; i < f.ntracks; i++) {
        mp4_track *t = &f.t[i];
        fprintf(stderr, "faixa %u %.4s %.4s ts=%u n=%u %ux%u ch=%u rate=%u cfg=%.4s/%u oti=%02x dur=%.3f\n", i,
                (char *)&(uint32_t){__builtin_bswap32(t->handler)}, (char *)&(uint32_t){__builtin_bswap32(t->codec)},
                t->timescale, t->nsamples, t->width, t->height, t->channels, t->sample_rate,
                (char *)&(uint32_t){__builtin_bswap32(t->cfg_type)}, t->cfg_len, t->oti, mp4_track_seconds(t));
        int64_t mt = t->edit_media_time > 0 ? t->edit_media_time : 0;
        for (uint32_t k = 0; k < t->nsamples; k++) {
            mp4_sample *s = &t->s[k];
            printf("%u,%llu,%u,%lld,%c\n", i, (unsigned long long)s->off, s->size,
                   (long long)(s->dts + s->cto - mt), s->sync ? 'K' : '_');
        }
        /* conferência de leitura: a última amostra inteira */
        if (t->nsamples) {
            void *b = malloc(t->s[t->nsamples - 1].size + 1);
            if (mp4_read_sample(&f, t, t->nsamples - 1, b)) { fprintf(stderr, "leitura falhou\n"); return 1; }
            free(b);
            uint32_t key;
            uint32_t at = mp4_sample_at(t, mp4_track_seconds(t) / 2, &key);
            if (key > at || !t->s[key].sync) { fprintf(stderr, "busca errada\n"); return 1; }
        }
    }
    mp4_close(&f);
    fclose(fp);
    return 0;
}

/* Teste de host de userland/host-audio/vorbis_glue.c contra o ffmpeg.
 * uso: vorbis_test arquivo.ogg|arquivo.mka referencia_s16le  → "vorbis ok ..." (correlação e SNR por amostra) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vorbis_glue.h"
#include "mkvdemux.h"
static unsigned char *slurp(const char *p, long *n) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != (size_t)*n) exit(2); fclose(f); return b; }
int main(int c, char **v) {
    (void)c;
    long n, rn; unsigned char *b = slurp(v[1], &n), *ref = slurp(v[2], &rn);
    short *pcm = 0; int ch = 0, rate = 0, frames = 0, rc;
    if (b[0] == 'O') rc = cx_vorbis_decode_ogg(b, (uint64_t)n, &pcm, &ch, &rate, &frames);
    else {
        mkv_file f;
        if (mkv_open_mem(&f, b, (uint64_t)n)) { puts("vorbis FALHA mkv"); return 1; }
        int t = mkv_find_track(&f, 2);
        const unsigned char **pk = malloc(f.np * sizeof *pk); uint32_t *ln = malloc(f.np * 4), k = 0;
        for (uint32_t i = 0; i < f.np; i++) if ((int)f.p[i].track == t) { pk[k] = b + f.p[i].off; ln[k++] = f.p[i].size; }
        rc = cx_vorbis_decode_packets(f.t[t].priv, f.t[t].priv_n, pk, ln, k, &pcm, &ch, &rate, &frames);
        if (!rc && f.duration_ns > 0 && frames > (int)(f.duration_ns / 1e9 * rate + 0.5)) frames = (int)(f.duration_ns / 1e9 * rate + 0.5);   /* como o audiodec.c */
    }
    if (rc) { puts("vorbis FALHA decodificar"); return 1; }
    long rf = rn / 2 / ch, m = frames < rf ? frames : rf;
    const short *r = (const short *)ref;
    double se = 0, ne = 0, dot = 0, ea = 0, eb = 0;
    for (long i = 0; i < m * ch; i++) { double a = r[i], x = pcm[i]; se += a * a; ne += (a - x) * (a - x); dot += a * x; ea += a * a; eb += x * x; }
    double snr = 10 * log10(se / (ne + 1e-9)), corr = dot / sqrt(ea * eb + 1e-9);
    int ok = corr > 0.9999 && snr > 50 && (b[0] == 'O' ? frames == rf : labs(frames - rf) <= 1024);   /* Ogg: corte exato pelo granulepos; Matroska: Duration em ms e o ffmpeg nem sempre corta */
    printf("%s %d Hz %d ch %d quadros (ffmpeg %ld), correlação %.6f, SNR %.1f dB\n", ok ? "vorbis ok" : "vorbis FALHA", rate, ch, frames, rf, corr, snr);
    return !ok;
}

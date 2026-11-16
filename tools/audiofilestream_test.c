/* Teste de host do AudioFileStream (userland/libsystem/audiofilestream.c). O arquivo chega em pedaços de tamanho
 * aleatório; os pacotes entregues são concatenados num arquivo e comparados com o "ffmpeg -c copy -f data".
 * uso: audiofilestream_test arquivo saída.bin semente  → imprime formato, pacotes e propriedades */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int OSStatus;
typedef struct { double r; unsigned id, fl, bpp, fpp, bpf, ch, bits, res; } asbd_t;
typedef struct { long long off; unsigned vf, sz; } aspd_t;
OSStatus AudioFileStreamOpen(void *, void (*)(void *, void *, unsigned, unsigned *), void (*)(void *, unsigned, unsigned, const void *, aspd_t *), unsigned, void **);
OSStatus AudioFileStreamParseBytes(void *, unsigned, const void *, unsigned);
OSStatus AudioFileStreamGetProperty(void *, unsigned, unsigned *, void *);
OSStatus AudioFileStreamClose(void *);
static FILE *out; static long packets, bytes; static int ready, props;
static void *g_stream;
static void lis(void *c, void *s, unsigned id, unsigned *fl) { (void)c; (void)fl; g_stream = s; props++; if (id == 0x72656479) ready = 1; }
static void pk(void *c, unsigned nb, unsigned np, const void *d, aspd_t *pd) {
    (void)c;
    if (!pd) { fwrite(d, 1, nb, out); bytes += nb; packets += np; return; }
    for (unsigned i = 0; i < np; i++) { fwrite((const char *)d + pd[i].off, 1, pd[i].sz, out); bytes += pd[i].sz; }
    packets += np;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
    out = fopen(argv[2], "wb"); srand(atoi(argv[3]));
    void *s; if (AudioFileStreamOpen(0, lis, pk, 0, &s)) return 1;
    long at = 0;
    while (at < n) {
        long k = 1 + rand() % 3000; if (k > n - at) k = n - at;
        OSStatus r = AudioFileStreamParseBytes(s, (unsigned)k, b + at, 0);
        if (r) { char c4[5] = {(char)(r >> 24), (char)(r >> 16), (char)(r >> 8), (char)r, 0}; printf("erro %s\n", c4); return 3; }
        at += k;
    }
    asbd_t fmt; unsigned sz = sizeof fmt;
    AudioFileStreamGetProperty(s, 0x64666d74, &sz, &fmt);
    char id[5] = {(char)(fmt.id >> 24), (char)(fmt.id >> 16), (char)(fmt.id >> 8), (char)fmt.id, 0};
    unsigned char ck[128]; unsigned cs = sizeof ck;
    int hc = !AudioFileStreamGetProperty(s, 0x6d676963, &cs, ck);
    printf("pronto=%d fmt=%s %.0f Hz %u ch fpp=%u pacotes=%ld bytes=%ld cookie=%u\n", ready, id, fmt.r, fmt.ch, fmt.fpp, packets, bytes, hc ? cs : 0);
    AudioFileStreamClose(s); fclose(out);
    return 0;
}

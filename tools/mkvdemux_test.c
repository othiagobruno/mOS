/* Teste de host de userland/libsystem/mkvdemux.c. Com um arquivo: imprime "faixa pts_ms tamanho chave" por pacote (o script
 * compara com o ffprobe). Com "--lacing": monta um Matroska com blocos em lacing Xiph, EBML e fixo e confere os quadros.
 * Saída do modo lacing: "mkvdemux lacing ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mkvdemux.h"
static unsigned char buf[4096]; static int n;
static void b1(int v) { buf[n++] = (unsigned char)v; }
static int open_el(unsigned id, int idlen) { for (int i = idlen - 1; i >= 0; i--) b1((id >> (8 * i)) & 0xFF); b1(0x10); b1(0); b1(0); b1(0); return n; } /* tamanho de 4 bytes */
static void close_el(int at) { int s = n - at; buf[at - 4] = 0x10; buf[at - 3] = (unsigned char)(s >> 16); buf[at - 2] = (unsigned char)(s >> 8); buf[at - 1] = (unsigned char)s; }
static void uint_el(unsigned id, int idlen, unsigned v) { for (int i = idlen - 1; i >= 0; i--) b1((id >> (8 * i)) & 0xFF); b1(0x81); b1((int)v); }
static int lacing_test(void) {
    n = 0;
    int e = open_el(0x1A45DFA3, 4); b1(0x42); b1(0x82); b1(0x88); memcpy(buf + n, "matroska", 8); n += 8; close_el(e);
    int seg = open_el(0x18538067, 4);
    int tr = open_el(0x1654AE6B, 4); int te = open_el(0xAE, 1); uint_el(0xD7, 1, 1); uint_el(0x83, 1, 2);
    b1(0x86); b1(0x88); memcpy(buf + n, "A_VORBIS", 8); n += 8; close_el(te); close_el(tr);
    int cl = open_el(0x1F43B675, 4); uint_el(0xE7, 1, 100);
    /* Xiph: 3 quadros de 300, 5 e 10 bytes */
    int sb = open_el(0xA3, 1); b1(0x81); b1(0); b1(5); b1(0x80 | 2); b1(2); b1(255); b1(45); b1(5);
    for (int i = 0; i < 315; i++) b1(i < 300 ? 'A' : i < 305 ? 'B' : 'C');
    close_el(sb);
    /* EBML: 3 quadros de 20, 18 e 25 bytes (diferença -2 em vint de 1 byte: 0x80 | (63 - 2)) */
    sb = open_el(0xA3, 1); b1(0x81); b1(0); b1(10); b1(0x80 | 6); b1(2); b1(0x80 | 20); b1(0x80 | 61);
    for (int i = 0; i < 63; i++) b1(i < 20 ? 'D' : i < 38 ? 'E' : 'F');
    close_el(sb);
    /* fixo: 4 quadros de 7 */
    sb = open_el(0xA3, 1); b1(0x81); b1(0); b1(20); b1(0x80 | 4); b1(3); for (int i = 0; i < 28; i++) b1('G' + i / 7); close_el(sb);
    close_el(cl); close_el(seg);
    mkv_file f;
    if (mkv_open_mem(&f, buf, (uint64_t)n)) { printf("mkvdemux FALHA lacing: %s\n", f.err); return 1; }
    unsigned exp_size[] = {300, 5, 10, 20, 18, 25, 7, 7, 7, 7};
    char exp_c[] = "ABCDEFGHIJ";
    int expt[] = {105, 105, 105, 110, 110, 110, 120, 120, 120, 120};
    if (f.np != 10) { printf("mkvdemux FALHA lacing: %u pacotes\n", f.np); return 1; }
    for (int i = 0; i < 10; i++)
        if (f.p[i].size != exp_size[i] || buf[f.p[i].off] != (unsigned char)exp_c[i] || f.p[i].pts_ns != expt[i] * 1000000LL || f.p[i].key != (i == 0 || i == 3 || i == 6)) {
            printf("mkvdemux FALHA lacing quadro %d: %u bytes '%c' %lld\n", i, f.p[i].size, buf[f.p[i].off], (long long)f.p[i].pts_ns); return 1;
        }
    puts("mkvdemux lacing ok");
    return 0;
}
int main(int argc, char **argv) {
    if (!strcmp(argv[1], "--lacing")) return lacing_test();
    FILE *fp = fopen(argv[1], "rb"); fseek(fp, 0, SEEK_END); long len = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *b = malloc(len); if (fread(b, 1, len, fp) != (size_t)len) return 2; fclose(fp);
    mkv_file f;
    if (mkv_open_mem(&f, b, (uint64_t)len)) { printf("erro %s\n", f.err); return 1; }
    for (uint32_t i = 0; i < f.np; i++) {   /* apresentação = bloco - CodecDelay arredondado para a unidade do arquivo (como o ffprobe) */
        long long ts = (long long)f.timescale, d = ((long long)f.t[f.p[i].track].codec_delay_ns + ts / 2) / ts;   /* atraso em unidades do arquivo */
        long long ms = (f.p[i].pts_ns / ts - d) * ts / 1000000;
        printf("%u %lld %u %d\n", f.p[i].track, ms, f.p[i].size, f.p[i].key);
    }
    return 0;
}

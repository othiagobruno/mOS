/* Teste de host de userland/libsystem/bzip2_enc.c: ida e volta contra o libbz2 do host e o leitor do mOS (bzip2.c). */
#include <bzlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mos_BZ2_bzBuffToBuffCompress(char *dst, unsigned *dl, char *src, unsigned sl, int b, int v, int w);
int mos_BZ2_bzBuffToBuffDecompress(char *dst, unsigned *dl, char *src, unsigned sl, int s, int v);
int mos_BZ2_bzCompressInit(bz_stream *s, int b, int v, int w);
int mos_BZ2_bzCompress(bz_stream *s, int a);
int mos_BZ2_bzCompressEnd(bz_stream *s);
void *mos_BZ2_bzWriteOpen(int *e, FILE *f, int b, int v, int w);
void mos_BZ2_bzWrite(int *e, void *b, void *buf, int n);
void mos_BZ2_bzWriteClose64(int *e, void *b, int ab, unsigned *il, unsigned *ih, unsigned *ol, unsigned *oh);

static int ok, fails;
static void check(int c, const char *what) { if (c) ok++; else { fails++; printf("FALHA %s\n", what); } }

static void roundtrip(const char *name, const unsigned char *d, unsigned n, int level) {
    unsigned cap = n + n / 100 + 600, cl = cap;
    char *c = malloc(cap);
    int r = mos_BZ2_bzBuffToBuffCompress(c, &cl, (char *)d, n, level, 0, 0);
    char what[128];
    snprintf(what, sizeof what, "%s: comprime (r=%d)", name, r);
    check(r == BZ_OK, what);
    if (r != BZ_OK) { free(c); return; }
    char *o = malloc(n + 1);
    unsigned ol = n + 1;
    r = BZ2_bzBuffToBuffDecompress(o, &ol, c, cl, 0, 0);
    snprintf(what, sizeof what, "%s: libbz2 do host lê (r=%d, %u de %u)", name, r, ol, n);
    check(r == BZ_OK && ol == n && !memcmp(o, d, n), what);
    ol = n + 1;
    r = mos_BZ2_bzBuffToBuffDecompress(o, &ol, c, cl, 0, 0);
    snprintf(what, sizeof what, "%s: bzip2.c lê (r=%d)", name, r);
    check(r == BZ_OK && ol == n && !memcmp(o, d, n), what);
    printf("  %-22s %8u -> %8u bytes\n", name, n, cl);
    free(o); free(c);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    unsigned n = 1500000;
    unsigned char *d = malloc(n);
    roundtrip("vazio", (const unsigned char *)"", 0, 9);
    roundtrip("um byte", (const unsigned char *)"x", 1, 9);
    const char *txt = "O mOS comprime bzip2 agora: BWT, MTF, RUNA/RUNB e Huffman. ";
    for (unsigned i = 0; i < n; i++) d[i] = (unsigned char)txt[i % strlen(txt)];
    roundtrip("texto periódico 1,5MB", d, n, 9);
    memset(d, 'a', n);
    roundtrip("só 'a' (corridas)", d, n, 9);
    unsigned s = 12345;
    for (unsigned i = 0; i < n; i++) { s = s * 1103515245 + 12345; d[i] = (unsigned char)(s >> 16); }
    roundtrip("aleatório nível 1", d, 300000, 1);
    roundtrip("aleatório nível 9", d, n, 9);
    for (unsigned i = 0; i < 256; i++) d[i] = (unsigned char)i;
    roundtrip("256 bytes distintos", d, 256, 9);
    for (unsigned i = 0; i < n; i++) d[i] = (unsigned char)((i / 7) % 3 == 0 ? 'z' : "abcab"[i % 5]);
    roundtrip("misto nível 5", d, n, 5);

    /* API de fluxo em pedaços pequenos, com um BZ_FLUSH no meio */
    {
        bz_stream st; memset(&st, 0, sizeof st);
        check(mos_BZ2_bzCompressInit(&st, 9, 0, 0) == BZ_OK, "fluxo: init");
        unsigned cap = n * 2, total = 0;
        char *out = malloc(cap);
        unsigned off = 0, chunk = 7777;
        int flushed = 0;
        while (off < n) {
            st.next_in = (char *)d + off; st.avail_in = off + chunk > n ? n - off : chunk; off += st.avail_in;
            int act = (!flushed && off > n / 2) ? BZ_FLUSH : BZ_RUN;
            int r;
            do {
                st.next_out = out + total; st.avail_out = 1000;
                r = mos_BZ2_bzCompress(&st, act);
                total += 1000 - st.avail_out;
            } while (act == BZ_FLUSH && r == BZ_FLUSH_OK);
            if (act == BZ_FLUSH) flushed = 1;
        }
        int r;
        do { st.next_out = out + total; st.avail_out = 1000; r = mos_BZ2_bzCompress(&st, BZ_FINISH); total += 1000 - st.avail_out; } while (r == BZ_FINISH_OK);
        check(r == BZ_STREAM_END, "fluxo: FINISH chega ao STREAM_END");
        check(st.total_in_lo32 == n && st.total_out_lo32 == total, "fluxo: contadores");
        mos_BZ2_bzCompressEnd(&st);
        char *o = malloc(n); unsigned ol = n;
        r = BZ2_bzBuffToBuffDecompress(o, &ol, out, total, 0, 0);
        check(r == BZ_OK && ol == n && !memcmp(o, d, n), "fluxo: host lê (dois blocos pelo FLUSH)");
        free(o); free(out);
    }
    /* BZFILE de escrita -> bzip2 -t / -dc do host */
    {
        char path[512]; snprintf(path, sizeof path, "%s/saida.bz2", dir);
        FILE *f = fopen(path, "wb");
        int e = 0;
        void *b = mos_BZ2_bzWriteOpen(&e, f, 9, 0, 0);
        check(b && e == BZ_OK, "arquivo: abre");
        for (unsigned i = 0; i < 10; i++) mos_BZ2_bzWrite(&e, b, (char *)txt, (int)strlen(txt));
        unsigned il, ih, ol, oh;
        mos_BZ2_bzWriteClose64(&e, b, 0, &il, &ih, &ol, &oh);
        fclose(f);
        check(e == BZ_OK && il == 10 * strlen(txt) && ol > 0, "arquivo: fecha com os contadores");
        char cmd[700]; snprintf(cmd, sizeof cmd, "bzip2 -t '%s' && bzip2 -dc '%s' | head -c 60 | grep -q 'O mOS comprime'", path, path);
        check(system(cmd) == 0, "arquivo: bzip2 do host testa e lê");
    }
    printf("bzip2_enc: %d/%d ok\n", ok, ok + fails);
    return fails != 0;
}

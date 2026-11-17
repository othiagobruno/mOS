#define _GNU_SOURCE
/*
 * mOS — teste de host do funopen de userland/libsystem/stdio.c (prefixo mos_, link com --gc-sections).
 * Exercita escrita e leitura por callbacks de cookie (fwrite/fread/fgetc/fseek/fclose).
 * Rodar: tools/funopen_hosttest.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int *__error(void) { static int e; return &e; }   /* cx_errno do mOS = *__error() */

/* --- API do mOS --- */
typedef void MFILE;
MFILE *mos_funopen(const void *, int (*)(void *, char *, int), int (*)(void *, const char *, int),
                   long long (*)(void *, long long, int), int (*)(void *));
size_t mos_fwrite(const void *, size_t, size_t, MFILE *);
size_t mos_fread(void *, size_t, size_t, MFILE *);
int mos_fgetc(MFILE *);
int mos_fseeko(MFILE *, long long, int);
int mos_fflush(MFILE *);
int mos_fclose(MFILE *);

/* --- cookie: buffer de memória que cresce --- */
typedef struct { char *buf; size_t len, cap, pos; int closed; } mem_t;
static int mem_write(void *c, const char *b, int n) {
    mem_t *m = c;
    if (m->pos + (size_t)n > m->cap) { m->cap = (m->pos + n) * 2 + 16; m->buf = realloc(m->buf, m->cap); }
    memcpy(m->buf + m->pos, b, n);
    m->pos += n; if (m->pos > m->len) m->len = m->pos;
    return n;
}
static int mem_read(void *c, char *b, int n) {
    mem_t *m = c;
    size_t avail = m->len - m->pos; if ((size_t)n > avail) n = (int)avail;
    memcpy(b, m->buf + m->pos, n); m->pos += n;
    return n;
}
static long long mem_seek(void *c, long long off, int whence) {
    mem_t *m = c;
    long long base = whence == 1 ? (long long)m->pos : whence == 2 ? (long long)m->len : 0;
    long long np = base + off; if (np < 0) return -1;
    m->pos = (size_t)np; return np;
}
static int mem_close(void *c) { ((mem_t *)c)->closed = 1; return 0; }

static int fails = 0, total = 0;
#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

int main(void) {
    /* ---- escrita por funopen ---- */
    {
        mem_t m = {0};
        MFILE *f = mos_funopen(&m, 0, mem_write, 0, mem_close);
        CK(f != 0, "funopen escrita");
        const char *s = "hello world";
        CK(mos_fwrite(s, 1, strlen(s), f) == strlen(s), "fwrite curto");
        /* bloco grande (> 4096) para exercitar o flush direto */
        char big[9000]; for (int i = 0; i < 9000; i++) big[i] = 'A' + (i % 26);
        CK(mos_fwrite(big, 1, sizeof big, f) == sizeof big, "fwrite grande");
        CK(mos_fclose(f) == 0, "fclose escrita");
        CK(m.closed == 1, "closefn chamado");
        CK(m.len == strlen(s) + sizeof big, "tamanho total");
        CK(memcmp(m.buf, s, strlen(s)) == 0 && memcmp(m.buf + strlen(s), big, sizeof big) == 0, "conteúdo gravado");
        free(m.buf);
    }
    /* ---- leitura por funopen + fseek ---- */
    {
        mem_t m = {0};
        m.buf = strdup("ABCDEFGHIJ"); m.len = 10; m.cap = 11; m.pos = 0;
        MFILE *f = mos_funopen(&m, mem_read, 0, mem_seek, mem_close);
        CK(f != 0, "funopen leitura");
        char out[6] = {0};
        CK(mos_fread(out, 1, 5, f) == 5 && memcmp(out, "ABCDE", 5) == 0, "fread 5");
        CK(mos_fgetc(f) == 'F', "fgetc F");
        CK(mos_fseeko(f, 0, 0) == 0, "fseek início");
        char out2[4] = {0};
        CK(mos_fread(out2, 1, 3, f) == 3 && memcmp(out2, "ABC", 3) == 0, "fread após seek");
        CK(mos_fseeko(f, -2, 2) == 0, "fseek fim-2");
        char out3[3] = {0};
        CK(mos_fread(out3, 1, 2, f) == 2 && memcmp(out3, "IJ", 2) == 0, "fread no fim");
        CK(mos_fclose(f) == 0 && m.closed == 1, "fclose leitura + closefn");
        free(m.buf);
    }

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}

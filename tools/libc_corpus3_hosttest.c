#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_corpus3.c (prefixo mos_).
 * fputws/fwprintf validados por um fputs de captura (conversão larga -> UTF-8); readpassphrase lendo de um pipe.
 * (swprintf/vswprintf ficam em swiftsupport.c/vscode_libc.c; não são testados aqui.)
 * Rodar: tools/libc_corpus3_hosttest.sh   (a entrada vem por pipe no .sh)
 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* open() do mOS: forçamos falha em /dev/tty para o readpassphrase cair na entrada padrão (fd 0, o pipe) */
int open(const char *path, int flags, ...) { (void)path; (void)flags; return -1; }

/* fputs de captura: o fwprintf/fputws do mOS escrevem por aqui */
static char cap[512]; static size_t caplen;
int fputs(const char *s, FILE *f) { (void)f; size_t n = strlen(s); memcpy(cap + caplen, s, n); caplen += n; cap[caplen] = 0; return 0; }

int mos_fputws(const wchar_t *, void *);
int mos_fwprintf(void *, const wchar_t *, ...);
char *mos_readpassphrase(const char *, char *, size_t, int);

static int fails = 0, total = 0;
#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

int main(void) {
    void *fake = (void *)1;   /* FILE opaco; o fputs de captura ignora */
    /* ---- fputws ---- */
    caplen = 0; cap[0] = 0;
    CK(mos_fputws(L"hello world", fake) == 1, "fputws retorno");
    CK(strcmp(cap, "hello world") == 0, "fputws conteúdo");
    /* ---- fwprintf ---- */
    caplen = 0; cap[0] = 0;
    int r = mos_fwprintf(fake, L"x=%d s=%ls hex=%x", 7, L"ab", 255);
    CK(strcmp(cap, "x=7 s=ab hex=ff") == 0, "fwprintf formata");
    CK(r == (int)strlen("x=7 s=ab hex=ff"), "fwprintf retorno = nº de chars");
    caplen = 0; cap[0] = 0;
    mos_fwprintf(fake, L"%5.2f|%c", 3.14159, 'Z');
    CK(strcmp(cap, " 3.14|Z") == 0, "fwprintf float/char");

    /* ---- readpassphrase: lê do pipe (fd 0), sem eco ---- */
    { char buf[64]; char *p = mos_readpassphrase("Senha: ", buf, sizeof buf, 0);
      CK(p == buf && strcmp(buf, "s3cr3t") == 0, "readpassphrase lê a linha sem o \\n"); }

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}

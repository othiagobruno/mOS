#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_corpus3.c (prefixo mos_).
 * fputws/fwprintf validados por um fputs de captura (conversão larga -> UTF-8); readpassphrase lendo de um pipe.
 * swprintf/vswprintf comparados ao swprintf do host.
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
int mos_swprintf(wchar_t *, size_t, const wchar_t *, ...);

static int fails = 0, total = 0;
#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

int main(void) {
    /* ---- swprintf vs host ---- */
    { wchar_t a[64], b[64]; int ra = mos_swprintf(a, 64, L"int=%d str=%s hex=%x", 42, "oi", 255);
      int rb = swprintf(b, 64, L"int=%d str=%s hex=%x", 42, "oi", 255);
      CK(ra == rb && wcscmp(a, b) == 0, "swprintf %d/%s/%x"); }
    { wchar_t a[64], b[64]; int ra = mos_swprintf(a, 64, L"%5.2f|%c|%ld", 3.14159, 'Z', 1000000L);
      int rb = swprintf(b, 64, L"%5.2f|%c|%ld", 3.14159, 'Z', 1000000L);
      CK(ra == rb && wcscmp(a, b) == 0, "swprintf %f/%c/%ld"); }
    { wchar_t a[32], b[32]; int ra = mos_swprintf(a, 32, L"wide=%ls", L"abc");
      int rb = swprintf(b, 32, L"wide=%ls", L"abc");
      CK(ra == rb && wcscmp(a, b) == 0, "swprintf %ls"); }
    { wchar_t a[4]; int ra = mos_swprintf(a, 4, L"abcdef");
      CK(ra < 0, "swprintf estouro -> negativo"); }
    { wchar_t a[16]; int ra = mos_swprintf(a, 16, L"%d-%d-%d", 1, 22, 333);
      CK(ra == 8 && wcscmp(a, L"1-22-333") == 0, "swprintf exato"); }
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

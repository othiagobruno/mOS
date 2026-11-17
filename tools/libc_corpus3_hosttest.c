#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_corpus3.c (prefixo mos_).
 * swprintf/vswprintf comparados ao host; readpassphrase lendo de um pipe (open de /dev/tty forçado a falhar).
 * Rodar: tools/libc_corpus3_hosttest.sh   (a entrada vem por pipe no .sh)
 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>

/* open() do mOS: forçamos falha em /dev/tty para o readpassphrase cair na entrada padrão (fd 0, o pipe) */
int open(const char *path, int flags, ...) { (void)path; (void)flags; return -1; }

int mos_swprintf(wchar_t *, size_t, const wchar_t *, ...);
char *mos_readpassphrase(const char *, char *, size_t, int);

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
    /* estouro: n pequeno deve devolver < 0 */
    { wchar_t a[4]; int ra = mos_swprintf(a, 4, L"abcdef");
      CK(ra < 0, "swprintf estouro -> negativo"); }
    { wchar_t a[16]; int ra = mos_swprintf(a, 16, L"%d-%d-%d", 1, 22, 333);
      CK(ra == 8 && wcscmp(a, L"1-22-333") == 0, "swprintf exato"); }

    /* ---- readpassphrase: lê do pipe (fd 0), sem eco ---- */
    { char buf[64]; char *r = mos_readpassphrase("Senha: ", buf, sizeof buf, 0);
      CK(r == buf && strcmp(buf, "s3cr3t") == 0, "readpassphrase lê a linha sem o \\n"); }

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}

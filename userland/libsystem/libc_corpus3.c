/*
 * mOS libSystem — mais símbolos do corpus (docs/lacunas/leonardo.md):
 *   - getpass / readpassphrase (leitura de senha sem eco) — basictex, comfy, keka, proxyman
 *   - wide stdio: fputws, swprintf/vswprintf, fwprintf/vfwprintf, wprintf/vwprintf
 *     — adobe-acrobat-reader, audacity, kicad, espanso (reusa o vsnprintf narrow + conversão UTF-8)
 * Entra no libSystem.mos pelo glob de tools/build.sh. Teste: tools/libc_corpus3_hosttest.sh
 */
#include "sys.h"
#include <stdarg.h>

typedef int wchar_t_;
typedef void FILE;

/* do resto do libSystem (no host, da libc do host) */
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int fputs(const char *s, FILE *f);
int wctomb(char *s, wchar_t_ wc);
size_t mbstowcs(wchar_t_ *dst, const char *src, size_t len);
size_t wcslen(const wchar_t_ *s);
void *malloc(size_t n);
void free(void *p);
extern FILE *__stdoutp;

/* ---------------------------------------------------------------- readpassphrase / getpass */
int open(const char *path, int flags, ...);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int close(int fd);
size_t strlen(const char *s);
typedef unsigned long tcflag_t_;
typedef struct { tcflag_t_ iflag, oflag, cflag, lflag; unsigned char cc[20]; tcflag_t_ ispeed, ospeed; } cx_termios;
int tcgetattr(int fd, cx_termios *t);
int tcsetattr(int fd, int act, const cx_termios *t);

#define RPP_ECHO_ON    0x01
#define RPP_REQUIRE_TTY 0x02
#define RPP_FORCELOWER 0x04
#define RPP_FORCEUPPER 0x08
#define RPP_SEVENBIT   0x10
#define TC_ECHO        0x8
#define TCSAFLUSH      2

char *readpassphrase(const char *prompt, char *buf, size_t bufsiz, int flags) {
    if (!buf || bufsiz == 0) return 0;
    int close_fd = 1;
    int fd = open("/dev/tty", 0x0002 /* O_RDWR */);
    if (fd < 0) {
        if (flags & RPP_REQUIRE_TTY) return 0;
        fd = 0; close_fd = 0;                       /* sem tty: cai para a entrada padrão */
    }
    if (prompt) write(fd, prompt, strlen(prompt));
    cx_termios old, raw; int restored = 0;
    if (!(flags & RPP_ECHO_ON) && tcgetattr(fd, &old) == 0) {
        raw = old; raw.lflag &= ~(tcflag_t_)TC_ECHO;
        tcsetattr(fd, TCSAFLUSH, &raw); restored = 1;
    }
    size_t i = 0; char c;
    while (i < bufsiz - 1) {
        long r = read(fd, &c, 1);
        if (r <= 0) break;
        if (c == '\n' || c == '\r') break;
        if (flags & RPP_SEVENBIT) c &= 0x7f;
        if ((flags & RPP_FORCELOWER) && c >= 'A' && c <= 'Z') c += 32;
        if ((flags & RPP_FORCEUPPER) && c >= 'a' && c <= 'z') c -= 32;
        buf[i++] = c;
    }
    buf[i] = 0;
    if (restored) { tcsetattr(fd, TCSAFLUSH, &old); write(fd, "\n", 1); }
    if (close_fd) close(fd);
    return buf;
}
char *getpass(const char *prompt) {
    static char b[128];
    return readpassphrase(prompt, b, sizeof b, 0);   /* RPP_ECHO_OFF = 0 */
}

/* ---------------------------------------------------------------- wide stdio (reusa vsnprintf narrow) */
/* a string de formato larga vira UTF-8 narrow (os especificadores %d/%s/%ls/%f são iguais no printf narrow),
 * formata com vsnprintf e, para swprintf, converte o resultado UTF-8 de volta para largo. */
static char *wfmt_to_narrow(const wchar_t_ *wfmt) {
    size_t wl = wcslen(wfmt);
    char *nf = malloc(wl * 4 + 1);
    if (!nf) return 0;
    size_t j = 0;
    for (size_t i = 0; i < wl; i++) {
        char tmp[8]; int k = wctomb(tmp, wfmt[i]);
        if (k <= 0) { nf[j++] = (char)wfmt[i]; }
        else for (int t = 0; t < k; t++) nf[j++] = tmp[t];
    }
    nf[j] = 0;
    return nf;
}

/* swprintf/vswprintf moram aqui. O beffd626 tirou estas achando que swiftsupport.c e vscode_libc.c tinham as suas, e
 * o 547c8faf tirou aquelas no mesmo dia achando que estas ficavam: sobrou nenhuma (88 apps do corpus importam). */
int vswprintf(wchar_t_ *ws, size_t n, const wchar_t_ *fmt, va_list ap) {
    if (!ws || n == 0) return -1;
    char *nf = wfmt_to_narrow(fmt);
    if (!nf) return -1;
    va_list c; va_copy(c, ap);
    int len = vsnprintf(0, 0, nf, c);
    va_end(c);
    if (len < 0) { free(nf); ws[0] = 0; return -1; }
    char *nb = malloc((size_t)len + 1);
    if (!nb) { free(nf); ws[0] = 0; return -1; }
    vsnprintf(nb, (size_t)len + 1, nf, ap);
    free(nf);
    size_t w = mbstowcs(ws, nb, n);
    free(nb);
    if (w == (size_t)-1) { ws[0] = 0; return -1; }
    if (w >= n) { ws[n - 1] = 0; return -1; }   /* não coube (contrato do swprintf) */
    return (int)w;
}
int swprintf(wchar_t_ *ws, size_t n, const wchar_t_ *fmt, ...) {
    va_list ap; va_start(ap, fmt); int r = vswprintf(ws, n, fmt, ap); va_end(ap); return r;
}
int vfwprintf(FILE *f, const wchar_t_ *fmt, va_list ap) {
    char *nf = wfmt_to_narrow(fmt);
    if (!nf) return -1;
    va_list c; va_copy(c, ap);
    int len = vsnprintf(0, 0, nf, c);
    va_end(c);
    if (len < 0) { free(nf); return -1; }
    char *nb = malloc((size_t)len + 1);
    if (!nb) { free(nf); return -1; }
    vsnprintf(nb, (size_t)len + 1, nf, ap);
    free(nf);
    int r = fputs(nb, f);
    free(nb);
    return r < 0 ? -1 : len;
}
int fwprintf(FILE *f, const wchar_t_ *fmt, ...) {
    va_list ap; va_start(ap, fmt); int r = vfwprintf(f, fmt, ap); va_end(ap); return r;
}
int vwprintf(const wchar_t_ *fmt, va_list ap) { return vfwprintf(__stdoutp, fmt, ap); }
int wprintf(const wchar_t_ *fmt, ...) {
    va_list ap; va_start(ap, fmt); int r = vfwprintf(__stdoutp, fmt, ap); va_end(ap); return r;
}
/* fputws: converte a string larga em UTF-8 e escreve com fputs */
int fputws(const wchar_t_ *ws, FILE *f) {
    size_t wl = wcslen(ws);
    char *nb = malloc(wl * 4 + 1);
    if (!nb) return -1;
    size_t j = 0;
    for (size_t i = 0; i < wl; i++) {
        char tmp[8]; int k = wctomb(tmp, ws[i]);
        if (k <= 0) nb[j++] = (char)ws[i];
        else for (int t = 0; t < k; t++) nb[j++] = tmp[t];
    }
    nb[j] = 0;
    int r = fputs(nb, f);
    free(nb);
    return r < 0 ? -1 : 1;
}

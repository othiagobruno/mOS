/* mOS libSystem - libiconv minima: UTF-8, ASCII e ISO-8859-1 (substitui /usr/lib/libiconv.2.dylib) */
#include "sys.h"
#include "libc.h"

typedef struct { int from, to; } cx_iconv_t;   /* 0 = UTF-8, 1 = Latin-1, 2 = ASCII */

static int enc(const char *n) {
    if (!strncasecmp(n, "UTF-8", 5) || !strncasecmp(n, "UTF8", 4)) return 0;
    if (!strncasecmp(n, "ISO-8859-1", 10) || !strncasecmp(n, "LATIN1", 6) || !strncasecmp(n, "ISO8859-1", 9)) return 1;
    if (!strncasecmp(n, "ASCII", 5) || !strncasecmp(n, "US-ASCII", 8) || !strcmp(n, "C") || !strcmp(n, "")) return 2;
    return -1;
}

void *iconv_open(const char *to, const char *from) {
    int t = enc(to), f = enc(from);
    if (t < 0 || f < 0) { cx_errno = 22; return (void *)-1; }
    cx_iconv_t *c = malloc(sizeof *c);
    if (!c) return (void *)-1;
    c->from = f; c->to = t;
    return c;
}
int iconv_close(void *c) { free(c); return 0; }
void *libiconv_open(const char *to, const char *from) { return iconv_open(to, from); }
int libiconv_close(void *c) { return iconv_close(c); }

size_t iconv(void *cd, char **in, size_t *inl, char **out, size_t *outl) {
    cx_iconv_t *c = cd;
    if (!in || !*in) return 0;
    size_t n = 0;
    while (*inl) {
        unsigned cp;
        unsigned char *s = (unsigned char *)*in;
        size_t used = 1;
        if (c->from == 0) {
            if (s[0] < 0x80) cp = s[0];
            else if ((s[0] & 0xE0) == 0xC0 && *inl >= 2) { cp = (s[0] & 0x1F) << 6 | (s[1] & 0x3F); used = 2; }
            else if ((s[0] & 0xF0) == 0xE0 && *inl >= 3) { cp = (s[0] & 0x0F) << 12 | (s[1] & 0x3F) << 6 | (s[2] & 0x3F); used = 3; }
            else if ((s[0] & 0xF8) == 0xF0 && *inl >= 4) { cp = (s[0] & 0x07) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 | (s[3] & 0x3F); used = 4; }
            else { cx_errno = 92; return (size_t)-1; }   /* EILSEQ */
        } else cp = s[0];
        unsigned char tmp[4];
        size_t w;
        if (c->to == 0) {
            if (cp < 0x80) { tmp[0] = (unsigned char)cp; w = 1; }
            else if (cp < 0x800) { tmp[0] = 0xC0 | cp >> 6; tmp[1] = 0x80 | (cp & 0x3F); w = 2; }
            else if (cp < 0x10000) { tmp[0] = 0xE0 | cp >> 12; tmp[1] = 0x80 | ((cp >> 6) & 0x3F); tmp[2] = 0x80 | (cp & 0x3F); w = 3; }
            else { tmp[0] = 0xF0 | cp >> 18; tmp[1] = 0x80 | ((cp >> 12) & 0x3F); tmp[2] = 0x80 | ((cp >> 6) & 0x3F); tmp[3] = 0x80 | (cp & 0x3F); w = 4; }
        } else {
            unsigned lim = c->to == 1 ? 0x100 : 0x80;
            if (cp >= lim) { cx_errno = 92; return (size_t)-1; }
            tmp[0] = (unsigned char)cp; w = 1;
        }
        if (*outl < w) { cx_errno = 7; return (size_t)-1; }   /* E2BIG */
        memcpy(*out, tmp, w);
        *out += w; *outl -= w; *in += used; *inl -= used;
        n++;
    }
    (void)n;
    return 0;
}
size_t libiconv(void *cd, char **in, size_t *inl, char **out, size_t *outl) { return iconv(cd, in, inl, out, outl); }

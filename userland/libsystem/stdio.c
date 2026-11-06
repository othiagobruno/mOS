/*
 * mOS libSystem — stdio: FILE (com o prefixo de layout do macOS, para os macros
 * getc_unlocked/putc_unlocked), printf completo, scanf básico e arquivos via syscalls.
 */
#include "sys.h"
#include "libc.h"

typedef struct { unsigned char *_base; int _size; } sbuf_t;

typedef struct cx_file {
    /* prefixo idêntico ao struct __sFILE do macOS */
    unsigned char *_p;
    int _r, _w;
    short _flags, _file;
    sbuf_t _bf;
    int _lbfsize;
    /* nosso estado */
    int fd;
    int readable, writable, append;
    int eof, err;
    int unget;              /* -1 = nenhum */
    int bufmode;            /* 0 sem buffer, 1 por linha, 2 total */
    u8 *wbuf; size_t wlen, wcap;
    u8 *rbuf; size_t rpos, rlen;
    int is_static;
    struct cx_file *next;
} FILE;

static FILE f_stdin, f_stdout, f_stderr;
FILE *__stdinp = &f_stdin, *__stdoutp = &f_stdout, *__stderrp = &f_stderr;
static FILE *open_files;

static void file_init(FILE *f, int fd, int r, int w, int bufmode) {
    memset(f, 0, sizeof *f);
    f->fd = fd;
    f->_file = (short)fd;
    f->readable = r;
    f->writable = w;
    f->bufmode = bufmode;
    f->unget = -1;
}

void cx_stdio_init(void) {
    file_init(&f_stdin, 0, 1, 0, 2);
    file_init(&f_stdout, 1, 0, 1, 1);
    file_init(&f_stderr, 2, 0, 1, 0);
    f_stdin.is_static = f_stdout.is_static = f_stderr.is_static = 1;
}

static int raw_write(int fd, const void *b, size_t n) {
    const u8 *p = b;
    while (n) {
        ssize_t r = write(fd, p, n);
        if (r <= 0) return -1;
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

int fflush(FILE *f) {
    if (!f) { cx_stdio_flush_all(); return 0; }
    if (f->wlen) {
        int r = raw_write(f->fd, f->wbuf, f->wlen);
        f->wlen = 0;
        if (r < 0) { f->err = 1; return -1; }
    }
    return 0;
}

void cx_stdio_flush_all(void) {
    fflush(&f_stdout);
    fflush(&f_stderr);
    for (FILE *f = open_files; f; f = f->next) fflush(f);
}

static int put_bytes(FILE *f, const void *data, size_t n) {
    if (!f->writable) { f->err = 1; cx_errno = CX_EBADF; return -1; }
    if (f->bufmode == 0) return raw_write(f->fd, data, n) < 0 ? (f->err = 1, -1) : 0;
    if (f->wlen + n > f->wcap) {
        if (fflush(f) < 0) return -1;
        if (n >= 4096) return raw_write(f->fd, data, n) < 0 ? (f->err = 1, -1) : 0;
        if (!f->wbuf) { f->wbuf = malloc(4096); f->wcap = f->wbuf ? 4096 : 0; if (!f->wbuf) return raw_write(f->fd, data, n); }
    }
    memcpy(f->wbuf + f->wlen, data, n);
    f->wlen += n;
    if (f->bufmode == 1 && memchr(data, '\n', n)) return fflush(f);
    return 0;
}

static int fill(FILE *f) {
    if (!f->readable) { f->err = 1; return -1; }
    if (f == &f_stdin) fflush(&f_stdout);
    if (!f->rbuf) { f->rbuf = malloc(4096); if (!f->rbuf) return -1; }
    ssize_t r = read(f->fd, f->rbuf, 4096);
    if (r < 0) { f->err = 1; return -1; }
    if (r == 0) { f->eof = 1; return -1; }
    f->rpos = 0;
    f->rlen = (size_t)r;
    return 0;
}

int fgetc(FILE *f) {
    if (f->unget >= 0) { int c = f->unget; f->unget = -1; return c; }
    if (f->rpos >= f->rlen && fill(f) < 0) return -1;
    return f->rbuf[f->rpos++];
}
int getc(FILE *f) { return fgetc(f); }
int getc_unlocked(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(__stdinp); }
int getchar_unlocked(void) { return fgetc(__stdinp); }
int ungetc(int c, FILE *f) { if (c < 0) return -1; f->unget = (u8)c; f->eof = 0; return c; }

/* usados pelos macros do <stdio.h> do macOS (__sgetc/__sputc) */
int __srget(FILE *f) { f->_r = 0; return fgetc(f); }
int __swbuf(int c, FILE *f) { f->_w = 0; u8 b = (u8)c; return put_bytes(f, &b, 1) < 0 ? -1 : (u8)c; }

int fputc(int c, FILE *f) { u8 b = (u8)c; return put_bytes(f, &b, 1) < 0 ? -1 : (u8)c; }
int putc(int c, FILE *f) { return fputc(c, f); }
int putc_unlocked(int c, FILE *f) { return fputc(c, f); }
int putchar(int c) { return fputc(c, __stdoutp); }
int putchar_unlocked(int c) { return fputc(c, __stdoutp); }
int fputs(const char *s, FILE *f) { return put_bytes(f, s, strlen(s)) < 0 ? -1 : 0; }
int puts(const char *s) { if (fputs(s, __stdoutp) < 0) return -1; return fputc('\n', __stdoutp) < 0 ? -1 : 0; }

size_t fwrite(const void *p, size_t sz, size_t n, FILE *f) {
    if (!sz || !n) return 0;
    return put_bytes(f, p, sz * n) < 0 ? 0 : n;
}

size_t fread(void *p, size_t sz, size_t n, FILE *f) {
    size_t want = sz * n, got = 0;
    u8 *o = p;
    if (!want) return 0;
    if (f->unget >= 0) { o[got++] = (u8)f->unget; f->unget = -1; }
    while (got < want) {
        if (f->rpos >= f->rlen && fill(f) < 0) break;
        size_t k = f->rlen - f->rpos;
        if (k > want - got) k = want - got;
        memcpy(o + got, f->rbuf + f->rpos, k);
        f->rpos += k;
        got += k;
    }
    return got / sz;
}

char *fgets(char *s, int n, FILE *f) {
    int i = 0;
    if (n <= 0) return 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c < 0) break;
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    if (!i) return 0;
    s[i] = 0;
    return s;
}

ssize_t getdelim(char **line, size_t *cap, int delim, FILE *f) {
    size_t n = 0;
    if (!*line || !*cap) { *cap = 128; *line = malloc(*cap); if (!*line) return -1; }
    for (;;) {
        int c = fgetc(f);
        if (c < 0) break;
        if (n + 2 > *cap) { char *nl = realloc(*line, *cap * 2); if (!nl) return -1; *line = nl; *cap *= 2; }
        (*line)[n++] = (char)c;
        if (c == delim) break;
    }
    if (!n) return -1;
    (*line)[n] = 0;
    return (ssize_t)n;
}
ssize_t getline(char **line, size_t *cap, FILE *f) { return getdelim(line, cap, '\n', f); }

int feof(FILE *f) { return f->eof; }
int ferror(FILE *f) { return f->err; }
void clearerr(FILE *f) { f->eof = f->err = 0; }
int fileno(FILE *f) { return f->fd; }
int setvbuf(FILE *f, char *buf, int mode, size_t size) { (void)buf; (void)size; fflush(f); f->bufmode = mode == 2 ? 0 : mode == 1 ? 1 : 2; return 0; }
void setbuf(FILE *f, char *buf) { setvbuf(f, buf, buf ? 0 : 2, 0); }
void setlinebuf(FILE *f) { setvbuf(f, 0, 1, 0); }
void flockfile(FILE *f) { (void)f; }
void funlockfile(FILE *f) { (void)f; }

FILE *fdopen(int fd, const char *mode) {
    FILE *f = malloc(sizeof *f);
    if (!f) return 0;
    int plus = strchr(mode, '+') != 0;
    file_init(f, fd, mode[0] == 'r' || plus, mode[0] != 'r' || plus, 2);
    f->append = mode[0] == 'a';
    f->next = open_files;
    open_files = f;
    return f;
}

FILE *fopen(const char *path, const char *mode) {
    int plus = strchr(mode, '+') != 0, flags;
    if (mode[0] == 'r') flags = plus ? MAC_O_RDWR : 0;
    else if (mode[0] == 'w') flags = (plus ? MAC_O_RDWR : MAC_O_WRONLY) | MAC_O_CREAT | MAC_O_TRUNC;
    else if (mode[0] == 'a') flags = (plus ? MAC_O_RDWR : MAC_O_WRONLY) | MAC_O_CREAT | MAC_O_APPEND;
    else { cx_errno = CX_EINVAL; return 0; }
    if (strchr(mode, 'x')) flags |= MAC_O_EXCL;
    int fd = open(path, flags, 0644);
    if (fd < 0) return 0;
    FILE *f = fdopen(fd, mode);
    if (!f) close(fd);
    return f;
}

int fclose(FILE *f) {
    if (!f) return -1;
    int r = fflush(f);
    if (f->is_static) return r;
    close(f->fd);
    for (FILE **pp = &open_files; *pp; pp = &(*pp)->next)
        if (*pp == f) { *pp = f->next; break; }
    free(f->wbuf);
    free(f->rbuf);
    free(f);
    return r;
}

FILE *freopen(const char *path, const char *mode, FILE *f) {
    if (f) fclose(f);
    return path ? fopen(path, mode) : 0;
}

int fseeko(FILE *f, off_t off, int wh) {
    fflush(f);
    if (wh == 1) off -= (off_t)(f->rlen - f->rpos) + (f->unget >= 0);
    f->rpos = f->rlen = 0;
    f->unget = -1;
    if (lseek(f->fd, off, wh) < 0) return -1;
    f->eof = 0;
    return 0;
}
int fseek(FILE *f, long off, int wh) { return fseeko(f, off, wh); }
off_t ftello(FILE *f) {
    off_t p = lseek(f->fd, 0, 1);
    if (p < 0) return -1;
    return p - (off_t)(f->rlen - f->rpos) - (f->unget >= 0) + (off_t)f->wlen;
}
long ftell(FILE *f) { return ftello(f); }
void rewind(FILE *f) { fseek(f, 0, 0); clearerr(f); }
int fgetpos(FILE *f, long *pos) { *pos = ftell(f); return *pos < 0 ? -1 : 0; }
int fsetpos(FILE *f, const long *pos) { return fseek(f, *pos, 0); }

void perror(const char *s) {
    if (s && *s) cx_fdprintf(2, "%s: %s\n", s, strerror(cx_errno));
    else cx_fdprintf(2, "%s\n", strerror(cx_errno));
}

/* ================================================================ formatação */
typedef struct {
    void (*put)(void *ctx, const char *s, size_t n);
    void *ctx;
    size_t total;
} sink_t;

static void emit(sink_t *k, const char *s, size_t n) { if (n) { k->put(k->ctx, s, n); k->total += n; } }
static void pad(sink_t *k, char c, int n) { char b[32]; memset(b, c, sizeof b); while (n > 0) { int m = n > 32 ? 32 : n; emit(k, b, (size_t)m); n -= m; } }

/* dígitos decimais de x >= 0 com prec casas (prec <= 40); retorna string em buf */
static int fmt_fixed(double x, int prec, char *buf, int cap) {
    int n = 0;
    if (x < 9.0e18 && prec <= 17) {
        double scale = 1;
        for (int i = 0; i < prec; i++) scale *= 10;
        double y = x * scale;
        if (y < 9.0e18) {
            u64 v = (u64)(y + 0.5);
            u64 ip = (u64)(v / (u64)scale), fp = v % (u64)scale;
            char t[24];
            int k = 0;
            do { t[k++] = (char)('0' + ip % 10); ip /= 10; } while (ip);
            while (k && n < cap - 1) buf[n++] = t[--k];
            if (prec) {
                buf[n++] = '.';
                char fd[24];
                for (int i = prec - 1; i >= 0; i--) { fd[i] = (char)('0' + fp % 10); fp /= 10; }
                for (int i = 0; i < prec && n < cap - 1; i++) buf[n++] = fd[i];
            }
            buf[n] = 0;
            return n;
        }
    }
    /* números grandes: dígitos da parte inteira por divisão sucessiva */
    double ip = __builtin_floor(x), fp = x - ip;
    char t[400];
    int k = 0;
    do {
        double q = __builtin_floor(ip / 10);
        int d = (int)(ip - q * 10);
        t[k++] = (char)('0' + (d < 0 ? 0 : d > 9 ? 9 : d));
        ip = q;
    } while (ip >= 1 && k < 330);
    while (k && n < cap - 1) buf[n++] = t[--k];
    if (prec && n < cap - 2) {
        buf[n++] = '.';
        for (int i = 0; i < prec && n < cap - 1; i++) { fp *= 10; int d = (int)fp; buf[n++] = (char)('0' + d); fp -= d; }
    }
    buf[n] = 0;
    return n;
}

static int exp10_of(double x) {
    if (x == 0) return 0;
    int e = 0;
    while (x >= 10) { x /= 10; e++; }
    while (x < 1) { x *= 10; e--; }
    return e;
}

static int fmt_exp(double x, int prec, char *buf, int cap, int upper) {
    int e = exp10_of(x);
    double m = x;
    if (e > 0) for (int i = 0; i < e; i++) m /= 10;
    else for (int i = 0; i < -e; i++) m *= 10;
    char tmp[64];
    fmt_fixed(m, prec, tmp, sizeof tmp);
    if (tmp[0] == '1' && tmp[1] == '0') { /* arredondou para 10.000 */
        e++;
        m /= 10;
        fmt_fixed(m, prec, tmp, sizeof tmp);
    }
    return cx_snprintf(buf, (size_t)cap, "%s%c%c%02d", tmp, upper ? 'E' : 'e', e < 0 ? '-' : '+', e < 0 ? -e : e);
}

static void strip_zeros(char *s) {
    char *dot = strchr(s, '.');
    if (!dot) return;
    char *e = s + strlen(s);
    char *ex = 0;
    for (char *p = dot; *p; p++) if (*p == 'e' || *p == 'E') { ex = p; break; }
    char *end = ex ? ex : e;
    char *q = end;
    while (q > dot + 1 && q[-1] == '0') q--;
    if (q == dot + 1) q = dot;
    memmove(q, end, strlen(end) + 1);
}

static void format_core(sink_t *k, const char *f, va_list ap) {
    for (; *f; f++) {
        if (*f != '%') {
            const char *s = f;
            while (f[1] && f[1] != '%') f++;
            emit(k, s, (size_t)(f - s + 1));
            continue;
        }
        f++;
        int left = 0, plus = 0, space = 0, alt = 0, zero = 0, width = -1, prec = -1, lng = 0;
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '+') plus = 1;
            else if (*f == ' ') space = 1;
            else if (*f == '#') alt = 1;
            else if (*f == '0') zero = 1;
            else if (*f == '\'') {}
            else break;
        }
        if (*f == '*') { width = va_arg(ap, int); if (width < 0) { left = 1; width = -width; } f++; }
        else { width = 0; while (isdigit((u8)*f)) width = width * 10 + (*f++ - '0'); }
        if (*f == '.') {
            f++;
            if (*f == '*') { prec = va_arg(ap, int); f++; }
            else { prec = 0; while (isdigit((u8)*f)) prec = prec * 10 + (*f++ - '0'); }
        }
        for (;; f++) {
            if (*f == 'h') lng--;
            else if (*f == 'l') lng++;
            else if (*f == 'q' || *f == 'j' || *f == 'L') lng = 2;
            else if (*f == 'z' || *f == 't') lng = 1;
            else break;
        }
        char c = *f;
        char buf[512];
        const char *s = buf;
        int len = 0;
        char sign = 0;
        const char *prefix = "";
        switch (c) {
        case 'd': case 'i': {
            i64 v = lng >= 1 ? va_arg(ap, i64) : va_arg(ap, int);
            if (lng == -1) v = (short)v;
            if (lng <= -2) v = (signed char)v;
            u64 u = v < 0 ? (u64)(-(v + 1)) + 1 : (u64)v;
            if (v < 0) sign = '-'; else if (plus) sign = '+'; else if (space) sign = ' ';
            char t[24];
            int n = 0;
            do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u);
            if (prec == 0 && v == 0) n = 0;
            while (n < prec) t[n++] = '0';
            for (int i = 0; i < n; i++) buf[i] = t[n - 1 - i];
            len = n;
            if (prec >= 0) zero = 0;
            break;
        }
        case 'u': case 'x': case 'X': case 'o': case 'p': case 'b': {
            u64 u;
            if (c == 'p') { u = (u64)va_arg(ap, void *); alt = 1; c = 'x'; prefix = "0x"; }
            else {
                u = lng >= 1 ? va_arg(ap, u64) : va_arg(ap, unsigned);
                if (lng == -1) u = (unsigned short)u;
                if (lng <= -2) u = (unsigned char)u;
            }
            unsigned base = c == 'o' ? 8 : c == 'u' ? 10 : c == 'b' ? 2 : 16;
            const char *dig = c == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char t[70];
            int n = 0;
            u64 orig = u;
            do { t[n++] = dig[u % base]; u /= base; } while (u);
            if (prec == 0 && orig == 0) n = 0;
            while (n < prec) t[n++] = '0';
            if (alt && c == 'o' && (n == 0 || t[n - 1] != '0')) t[n++] = '0';
            for (int i = 0; i < n; i++) buf[i] = t[n - 1 - i];
            len = n;
            if (alt && orig && base == 16 && !*prefix) prefix = c == 'X' ? "0X" : "0x";
            if (prec >= 0) zero = 0;
            break;
        }
        case 'c':
            buf[0] = (char)va_arg(ap, int);
            len = 1;
            break;
        case 'C':
            buf[0] = (char)va_arg(ap, int);
            len = 1;
            break;
        case 's': case 'S':
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            len = (int)(prec >= 0 ? strnlen(s, (size_t)prec) : strlen(s));
            zero = 0;
            break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            double v = va_arg(ap, double);
            if (prec < 0) prec = 6;
            if (prec > 40) prec = 40;
            u64 bits;
            memcpy(&bits, &v, 8);
            if (bits >> 63) { sign = '-'; v = -v; } else if (plus) sign = '+'; else if (space) sign = ' ';
            int upper = c == 'F' || c == 'E' || c == 'G' || c == 'A';
            if (v != v) { strcpy(buf, upper ? "NAN" : "nan"); len = 3; zero = 0; break; }
            if (v == __builtin_inf()) { strcpy(buf, upper ? "INF" : "inf"); len = 3; zero = 0; break; }
            if (c == 'f' || c == 'F') len = fmt_fixed(v, prec, buf, sizeof buf);
            else if (c == 'e' || c == 'E') len = fmt_exp(v, prec, buf, sizeof buf, upper);
            else if (c == 'a' || c == 'A') {
                /* hexadecimal: 0x1.xxxxp+e */
                int e2 = 0;
                u64 man = bits & ((1ull << 52) - 1);
                int ex = (int)((bits >> 52) & 0x7FF);
                if (v == 0) len = cx_snprintf(buf, sizeof buf, "%s0p+0", upper ? "0X" : "0x");
                else {
                    e2 = ex - 1023;
                    char h[16];
                    int hn = 0;
                    for (int i = 12; i >= 0 && man; i--) { h[hn++] = "0123456789abcdef"[(man >> (i * 4)) & 15]; man &= (1ull << (i * 4)) - 1; }
                    h[hn] = 0;
                    len = cx_snprintf(buf, sizeof buf, "0x1%s%sp%c%d", hn ? "." : "", h, e2 < 0 ? '-' : '+', e2 < 0 ? -e2 : e2);
                }
            } else {
                int P = prec ? prec : 1;
                int X = v == 0 ? 0 : exp10_of(v);
                /* o arredondamento pode subir o expoente (9.9999 -> 10.0) */
                char t[64];
                fmt_exp(v, P - 1, t, sizeof t, 0);
                char *ep = strchr(t, 'e');
                if (ep) X = (int)strtol(ep + 1, 0, 10);
                if (P > X && X >= -4) len = fmt_fixed(v, P - 1 - X, buf, sizeof buf);
                else len = fmt_exp(v, P - 1, buf, sizeof buf, upper);
                if (!alt) { strip_zeros(buf); len = (int)strlen(buf); }
            }
            break;
        }
        case '@': {                                  /* objeto Objective-C: [obj description] */
            extern const char *cx_describe(void *o) __attribute__((weak));
            void *o = va_arg(ap, void *);
            s = cx_describe ? cx_describe(o) : "(objeto)";
            len = (int)(prec >= 0 ? strnlen(s, (size_t)prec) : strlen(s));
            zero = 0;
            break;
        }
        case 'n': {
            int *p = va_arg(ap, int *);
            if (p) *p = (int)k->total;
            continue;
        }
        case '%':
            buf[0] = '%';
            len = 1;
            break;
        default:
            buf[0] = '%';
            buf[1] = c;
            len = c ? 2 : 1;
            if (!c) f--;
            break;
        }
        int extra = (sign ? 1 : 0) + (int)strlen(prefix);
        int padn = width > len + extra ? width - len - extra : 0;
        if (!left && !zero) pad(k, ' ', padn);
        if (sign) emit(k, &sign, 1);
        emit(k, prefix, strlen(prefix));
        if (!left && zero) pad(k, '0', padn);
        emit(k, s, (size_t)len);
        if (left) pad(k, ' ', padn);
    }
}

/* destinos */
static void put_file(void *ctx, const char *s, size_t n) { put_bytes((FILE *)ctx, s, n); }
typedef struct { char *buf; size_t cap, len; } strbuf_t;
static void put_str(void *ctx, const char *s, size_t n) {
    strbuf_t *b = ctx;
    for (size_t i = 0; i < n; i++) { if (b->len + 1 < b->cap) b->buf[b->len] = s[i]; b->len++; }
}
static void put_dyn(void *ctx, const char *s, size_t n) {
    strbuf_t *b = ctx;
    if (!b->buf) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = (b->len + n + 1) * 2;
        char *nb = realloc(b->buf, nc);
        if (!nb) { free(b->buf); b->buf = 0; return; }
        b->buf = nb;
        b->cap = nc;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
}
typedef struct { int fd; char buf[256]; size_t n; } fdbuf_t;
static void put_fd(void *ctx, const char *s, size_t n) {
    fdbuf_t *b = ctx;
    for (size_t i = 0; i < n; i++) {
        if (b->n == sizeof b->buf) { write(b->fd, b->buf, b->n); b->n = 0; }
        b->buf[b->n++] = s[i];
    }
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
    sink_t k = {put_file, f, 0};
    format_core(&k, fmt, ap);
    return f->err ? -1 : (int)k.total;
}
int vprintf(const char *fmt, va_list ap) { return vfprintf(__stdoutp, fmt, ap); }
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    strbuf_t b = {buf, n, 0};
    sink_t k = {put_str, &b, 0};
    format_core(&k, fmt, ap);
    if (n) buf[b.len < n ? b.len : n - 1] = 0;
    return (int)k.total;
}
int vsprintf(char *buf, const char *fmt, va_list ap) { return vsnprintf(buf, (size_t)-1 >> 1, fmt, ap); }
int vasprintf(char **out, const char *fmt, va_list ap) {
    strbuf_t b = {malloc(64), 64, 0};
    sink_t k = {put_dyn, &b, 0};
    format_core(&k, fmt, ap);
    if (!b.buf) { *out = 0; return -1; }
    b.buf[b.len] = 0;
    *out = b.buf;
    return (int)b.len;
}
int vdprintf(int fd, const char *fmt, va_list ap) {
    fdbuf_t b = {fd, {0}, 0};
    sink_t k = {put_fd, &b, 0};
    format_core(&k, fmt, ap);
    if (b.n) write(fd, b.buf, b.n);
    return (int)k.total;
}

int printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vprintf(fmt, ap); va_end(ap); return r; }
int fprintf(FILE *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int sprintf(char *b, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsprintf(b, fmt, ap); va_end(ap); return r; }
int snprintf(char *b, size_t n, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(b, n, fmt, ap); va_end(ap); return r; }
int asprintf(char **o, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vasprintf(o, fmt, ap); va_end(ap); return r; }
int dprintf(int fd, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vdprintf(fd, fmt, ap); va_end(ap); return r; }

/* variantes "_chk" do _FORTIFY_SOURCE */
static void ovf(void) { cx_puts_fd(2, "detected buffer overflow\n"); abort(); }
int __vsnprintf_chk(char *b, size_t n, int flag, size_t sl, const char *fmt, va_list ap) { (void)flag; if (n > sl) ovf(); return vsnprintf(b, n, fmt, ap); }
int __vsprintf_chk(char *b, int flag, size_t sl, const char *fmt, va_list ap) {
    (void)flag;
    va_list c;
    va_copy(c, ap);
    int need = vsnprintf(0, 0, fmt, c);
    va_end(c);
    if ((size_t)need >= sl) ovf();
    return vsnprintf(b, sl, fmt, ap);
}
int __snprintf_chk(char *b, size_t n, int flag, size_t sl, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = __vsnprintf_chk(b, n, flag, sl, fmt, ap); va_end(ap); return r; }
int __sprintf_chk(char *b, int flag, size_t sl, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = __vsprintf_chk(b, flag, sl, fmt, ap); va_end(ap); return r; }

int cx_snprintf(char *b, size_t n, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(b, n, fmt, ap); va_end(ap); return r; }
int cx_fdprintf(int fd, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vdprintf(fd, fmt, ap); va_end(ap); return r; }
void cx_puts_fd(int fd, const char *s) { write(fd, s, strlen(s)); }

/* err.h */
extern const char *__progname;
static void vwarn_impl(int with_errno, const char *fmt, va_list ap) {
    int e = cx_errno;
    fflush(__stdoutp);
    fprintf(__stderrp, "%s: ", __progname);
    if (fmt) vfprintf(__stderrp, fmt, ap);
    if (with_errno) fprintf(__stderrp, "%s%s", fmt ? ": " : "", strerror(e));
    fputc('\n', __stderrp);
}
void vwarn(const char *fmt, va_list ap) { vwarn_impl(1, fmt, ap); }
void vwarnx(const char *fmt, va_list ap) { vwarn_impl(0, fmt, ap); }
void warn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarn_impl(1, fmt, ap); va_end(ap); }
void warnx(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarn_impl(0, fmt, ap); va_end(ap); }
__attribute__((noreturn)) void verr(int ev, const char *fmt, va_list ap) { vwarn_impl(1, fmt, ap); exit(ev); }
__attribute__((noreturn)) void verrx(int ev, const char *fmt, va_list ap) { vwarn_impl(0, fmt, ap); exit(ev); }
__attribute__((noreturn)) void err(int ev, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarn_impl(1, fmt, ap); va_end(ap); exit(ev); }
__attribute__((noreturn)) void errx(int ev, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarn_impl(0, fmt, ap); va_end(ap); exit(ev); }

/* ================================================================ scanf (subconjunto) */
typedef struct { const char *s; FILE *f; int peeked; } scan_src_t;
static int sc_get(scan_src_t *r) {
    if (r->s) return *r->s ? (u8)*r->s++ : -1;
    return fgetc(r->f);
}
static void sc_unget(scan_src_t *r, int c) {
    if (c < 0) return;
    if (r->s) r->s--; else ungetc(c, r->f);
}

static int vscan(scan_src_t *r, const char *f, va_list ap) {
    int assigned = 0, c;
    for (; *f; f++) {
        if (isspace((u8)*f)) {
            while ((c = sc_get(r)) >= 0 && isspace(c)) {}
            sc_unget(r, c);
            continue;
        }
        if (*f != '%' || f[1] == '%') {
            if (*f == '%') f++;
            c = sc_get(r);
            if (c != (u8)*f) { sc_unget(r, c); return assigned ? assigned : (c < 0 ? -1 : 0); }
            continue;
        }
        f++;
        int skip = 0, width = 0, lng = 0;
        if (*f == '*') { skip = 1; f++; }
        while (isdigit((u8)*f)) width = width * 10 + (*f++ - '0');
        for (;; f++) {
            if (*f == 'l') lng++;
            else if (*f == 'h') lng--;
            else if (*f == 'z' || *f == 'j' || *f == 'q' || *f == 'L') lng = 2;
            else break;
        }
        char conv = *f;
        if (!width) width = 4096;
        if (conv != 'c' && conv != '[') {
            while ((c = sc_get(r)) >= 0 && isspace(c)) {}
            sc_unget(r, c);
        }
        char tok[512];
        int n = 0;
        if (conv == 'c') {
            if (width == 4096) width = 1;
            char *dst = skip ? 0 : va_arg(ap, char *);
            for (int i = 0; i < width; i++) { c = sc_get(r); if (c < 0) return assigned ? assigned : -1; if (dst) dst[i] = (char)c; }
            if (!skip) assigned++;
            continue;
        }
        if (conv == 's' || conv == '[') {
            int neg = 0;
            u8 set[256];
            memset(set, 0, sizeof set);
            if (conv == '[') {
                f++;
                if (*f == '^') { neg = 1; f++; }
                if (*f == ']') { set[']'] = 1; f++; }
                for (; *f && *f != ']'; f++) {
                    if (f[1] == '-' && f[2] && f[2] != ']') { for (int k = (u8)f[0]; k <= (u8)f[2]; k++) set[k] = 1; f += 2; }
                    else set[(u8)*f] = 1;
                }
            }
            char *dst = skip ? 0 : va_arg(ap, char *);
            while (n < width && (c = sc_get(r)) >= 0) {
                int ok = conv == 's' ? !isspace(c) : (neg ? !set[c] : set[c]);
                if (!ok) { sc_unget(r, c); break; }
                if (dst) dst[n] = (char)c;
                n++;
            }
            if (!n) return assigned ? assigned : (c < 0 ? -1 : 0);
            if (dst) { dst[n] = 0; assigned++; }
            continue;
        }
        int isfloat = conv == 'f' || conv == 'e' || conv == 'g' || conv == 'E' || conv == 'G' || conv == 'a';
        while (n < width && n < (int)sizeof tok - 1 && (c = sc_get(r)) >= 0) {
            int ok = isdigit(c) || ((c == '-' || c == '+') && (n == 0 || (isfloat && (tok[n - 1] | 32) == 'e')));
            if (conv == 'x' || conv == 'X' || conv == 'i' || conv == 'p') ok = ok || isxdigit(c) || ((c | 32) == 'x' && n > 0 && n <= 2);
            if (isfloat) ok = ok || c == '.' || (c | 32) == 'e';
            if (!ok) { sc_unget(r, c); break; }
            tok[n++] = (char)c;
        }
        tok[n] = 0;
        if (!n) return assigned ? assigned : (c < 0 ? -1 : 0);
        if (skip) continue;
        if (isfloat) {
            double v = strtod(tok, 0);
            if (lng >= 1) *va_arg(ap, double *) = v; else *va_arg(ap, float *) = (float)v;
        } else {
            int base = conv == 'x' || conv == 'X' || conv == 'p' ? 16 : conv == 'o' ? 8 : conv == 'i' ? 0 : 10;
            long long v = (conv == 'u' || base != 10) ? (long long)strtoul(tok, 0, base) : strtol(tok, 0, base);
            if (conv == 'p') *va_arg(ap, void **) = (void *)v;
            else if (lng >= 1) *va_arg(ap, long long *) = v;
            else if (lng == -1) *va_arg(ap, short *) = (short)v;
            else if (lng <= -2) *va_arg(ap, char *) = (char)v;
            else *va_arg(ap, int *) = (int)v;
        }
        assigned++;
    }
    return assigned;
}

int vsscanf(const char *s, const char *fmt, va_list ap) { scan_src_t r = {s, 0, 0}; return vscan(&r, fmt, ap); }
int vfscanf(FILE *f, const char *fmt, va_list ap) { scan_src_t r = {0, f, 0}; return vscan(&r, fmt, ap); }
int vscanf(const char *fmt, va_list ap) { return vfscanf(__stdinp, fmt, ap); }
int sscanf(const char *s, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsscanf(s, fmt, ap); va_end(ap); return r; }
int fscanf(FILE *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfscanf(f, fmt, ap); va_end(ap); return r; }
int scanf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfscanf(__stdinp, fmt, ap); va_end(ap); return r; }

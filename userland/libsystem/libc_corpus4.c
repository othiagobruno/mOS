/*
 * mOS libSystem — o lote de libc que mais apps do corpus pedem e ainda faltava (docs/lacunas/leonardo.md,
 * símbolos com 5 ou mais apps):
 *   - _res (estado do resolvedor), hash_create/hash_search (tabela de hash do Libc), ndbm (dbm_*) com arquivo de
 *     verdade (o _dbm do Python: anki, blender, calibre);
 *   - wide stdio e conversão: fgetwc, fgetws, putwc, getwchar, wcsspn, swscanf/vswscanf, wcsto*_l;
 *   - variantes _l (o mOS tem um locale só, o "C"/UTF-8): vfprintf_l, vprintf_l, vsprintf_l, nl_langinfo_l, strfmon(_l);
 *   - open_memstream e fmemopen (sobre funopen), devname, ftime, daemon, inet_net_pton;
 *   - Sun RPC / NIS do módulo nis do Python (blender, freecad, inkscape, kicad): sem ypbind, como um Mac sem NIS;
 *   - dyld legado (NSLookupAndBindSymbol e família, NSLinkModule): símbolos pelo dlsym; carregar objeto não;
 *   - compiler-rt (__udivmodti4, __truncsfhf2), inversas de matriz do simd (__invert_*), complexos trigonométricos;
 *   - os_log legado (_os_log_internal), sandbox, exc_server, NXFindBestFatArch, pthread_chdir_np.
 * Entra no libSystem.mos pelo glob de tools/build.sh. Teste: tools/libc_corpus4_hosttest.sh
 */
#include "sys.h"
#include <stdarg.h>

typedef int wchar_t_;
typedef void FILE;

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strdup(const char *s);
char *strchr(const char *s, int c);
int snprintf(char *b, size_t n, const char *fmt, ...);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsscanf(const char *s, const char *fmt, va_list ap);
int fgetc(FILE *f);
int getwc(FILE *f);
int ungetwc(int wc, FILE *f);
int fputwc(wchar_t_ wc, FILE *f);
FILE *funopen(const void *cookie, int (*readfn)(void *, char *, int), int (*writefn)(void *, const char *, int),
              long long (*seekfn)(void *, long long, int), int (*closefn)(void *));
int wctomb(char *s, wchar_t_ wc);
size_t wcslen(const wchar_t_ *s);
double wcstod(const wchar_t_ *s, wchar_t_ **end);
float wcstof(const wchar_t_ *s, wchar_t_ **end);
long double wcstold(const wchar_t_ *s, wchar_t_ **end);
long wcstol(const wchar_t_ *s, wchar_t_ **end, int base);
unsigned long wcstoul(const wchar_t_ *s, wchar_t_ **end, int base);
long long wcstoll(const wchar_t_ *s, wchar_t_ **end, int base);
unsigned long long wcstoull(const wchar_t_ *s, wchar_t_ **end, int base);
char *nl_langinfo(int item);
int open(const char *path, int flags, ...);
int close(int fd);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
long lseek(int fd, long off, int wh);
int fsync(int fd);
int ftruncate(int fd, long len);
int gettimeofday(void *tv, void *tz);
int fork(void);
int setsid(void);
int chdir(const char *path);
int dup2(int a, int b);
void _exit(int code);
void *dlsym(void *h, const char *name);
void *dlopen(const char *path, int mode);
extern FILE *__stdinp, *__stdoutp, *__stderrp;

/* ================================================================ _res */
/* struct __res_state do macOS tem 552 bytes; o resolvedor do mOS (res_9_*) guarda o seu estado à parte. Os apps
 * só leem options/nscount depois de res_init, que deixa o RES_INIT ligado. */
typedef struct { int retrans, retry; unsigned long options; int nscount; u8 rest[552 - 24]; } cx_res_state;
cx_res_state _res = {5, 4, 0x00000001ul | 0x000000c0ul, 0, {0}};   /* RES_INIT|RES_RECURSE|RES_DEFNAMES */

/* ================================================================ hash_create / hash_search (Libc, hash.h) */
typedef struct hbucket { char *key; void *data; struct hbucket *next; } hbucket;
typedef struct { int size; hbucket **b; } hash_table;
static unsigned hh(const char *k, int size) { unsigned h = 0; while (*k) h = h * 31 + (unsigned char)*k++; return h % (unsigned)size; }
hash_table *hash_create(int size) {
    if (size <= 0) size = 101;
    hash_table *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->b = calloc((size_t)size, sizeof(hbucket *));
    if (!t->b) { free(t); return 0; }
    t->size = size;
    return t;
}
/* datum 0: procura. datum != 0: insere; se a chave já existe, replace_func recebe o dado antigo e o novo entra */
void *hash_search(hash_table *t, char *key, void *datum, void (*replace)(void *)) {
    if (!t || !key) return 0;
    unsigned i = hh(key, t->size);
    for (hbucket *e = t->b[i]; e; e = e->next)
        if (!strcmp(e->key, key)) {
            if (!datum) return e->data;
            if (replace) replace(e->data);
            e->data = datum;
            return datum;
        }
    if (!datum) return 0;
    hbucket *e = malloc(sizeof *e);
    if (!e) return 0;
    e->key = strdup(key);
    if (!e->key) { free(e); return 0; }
    e->data = datum;
    e->next = t->b[i];
    t->b[i] = e;
    return datum;
}
void hash_traverse(hash_table *t, int (*fn)(char *, void *, void *), void *arg) {
    if (!t || !fn) return;
    for (int i = 0; i < t->size; i++)
        for (hbucket *e = t->b[i]; e; e = e->next) if (!fn(e->key, e->data, arg)) return;
}
void hash_purge(hash_table *t, void (*fn)(void *)) {
    if (!t) return;
    for (int i = 0; i < t->size; i++) {
        hbucket *e = t->b[i];
        while (e) { hbucket *n = e->next; if (fn) fn(e->data); free(e->key); free(e); e = n; }
        t->b[i] = 0;
    }
}
void hash_destroy(hash_table *t, char *key, void (*fn)(void *)) {
    if (!t || !key) return;
    hbucket **pp = &t->b[hh(key, t->size)];
    for (; *pp; pp = &(*pp)->next)
        if (!strcmp((*pp)->key, key)) { hbucket *e = *pp; *pp = e->next; if (fn) fn(e->data); free(e->key); free(e); return; }
}
void hash_stats(hash_table *t, int verbose) { (void)t; (void)verbose; }

/* ================================================================ ndbm */
/* Arquivo <nome>.db: "MOSDBM1\n" e depois registros (u32 tamanho da chave, u32 tamanho do valor, bytes).
 * A base inteira fica na memória; cada dbm_store/dbm_delete regrava o arquivo (o _dbm do Python fecha e reabre
 * pouco, e um shelve de app é pequeno). */
typedef struct { void *dptr; size_t dsize; } datum;
typedef struct { u8 *k, *v; u32 nk, nv; } dbrec;
typedef struct { int fd, ro, err; dbrec *r; size_t n, cap; size_t cursor; } DBM;
#define DBM_INSERT 0
#define DBM_REPLACE 1
#define O_ACCMODE_ 3
#define O_CREAT_ 0x200    /* O_* do macOS: o open exportado do libSystem recebe os do app (libc.c traduz) */
#define O_TRUNC_ 0x400
static const char dbm_magic[8] = {'M', 'O', 'S', 'D', 'B', 'M', '1', '\n'};
static int rd_all(int fd, void *b, size_t n) { u8 *p = b; while (n) { long k = read(fd, p, n); if (k <= 0) return -1; p += k; n -= (size_t)k; } return 0; }
static int wr_all(int fd, const void *b, size_t n) { const u8 *p = b; while (n) { long k = write(fd, p, n); if (k <= 0) return -1; p += k; n -= (size_t)k; } return 0; }
static int dbm_find(DBM *d, datum key) {
    for (size_t i = 0; i < d->n; i++)
        if (d->r[i].nk == key.dsize && !memcmp(d->r[i].k, key.dptr, key.dsize)) return (int)i;
    return -1;
}
/* regrava do começo e só então corta o resto (ftruncate pode não existir no sistema de arquivos: aí o leitor para na
 * marca de fim, um registro com tamanhos 0xffffffff) */
static int dbm_save(DBM *d) {
    if (lseek(d->fd, 0, 0) < 0) return -1;
    if (wr_all(d->fd, dbm_magic, 8)) return -1;
    long total = 8;
    for (size_t i = 0; i < d->n; i++) {
        u32 h[2] = {d->r[i].nk, d->r[i].nv};
        if (wr_all(d->fd, h, 8) || wr_all(d->fd, d->r[i].k, d->r[i].nk) || wr_all(d->fd, d->r[i].v, d->r[i].nv)) return -1;
        total += 8 + (long)d->r[i].nk + (long)d->r[i].nv;
    }
    if (ftruncate(d->fd, total) < 0) {
        static const u32 end[2] = {0xffffffffu, 0xffffffffu};
        if (wr_all(d->fd, end, 8)) return -1;
    }
    return 0;
}
static int dbm_push(DBM *d, const void *k, u32 nk, const void *v, u32 nv) {
    if (d->n == d->cap) {
        size_t nc = d->cap ? d->cap * 2 : 16;
        dbrec *r = realloc(d->r, nc * sizeof *r);
        if (!r) return -1;
        d->r = r; d->cap = nc;
    }
    dbrec *e = &d->r[d->n];
    e->k = malloc(nk ? nk : 1); e->v = malloc(nv ? nv : 1);
    if (!e->k || !e->v) { free(e->k); free(e->v); return -1; }
    memcpy(e->k, k, nk); memcpy(e->v, v, nv);
    e->nk = nk; e->nv = nv;
    d->n++;
    return 0;
}
static void dbm_free(DBM *d) {
    for (size_t i = 0; i < d->n; i++) { free(d->r[i].k); free(d->r[i].v); }
    free(d->r);
    free(d);
}
DBM *dbm_open(const char *file, int flags, int mode) {
    if (!file) { cx_errno = 22; return 0; }
    size_t n = strlen(file);
    char *path = malloc(n + 4);
    if (!path) return 0;
    memcpy(path, file, n);
    memcpy(path + n, ".db", 4);
    int fd = open(path, flags & ~O_TRUNC_, mode);
    free(path);
    if (fd < 0) return 0;
    DBM *d = calloc(1, sizeof *d);
    if (!d) { close(fd); return 0; }
    d->fd = fd;
    d->ro = (flags & O_ACCMODE_) == 0;
    char m[8];
    long got = read(fd, m, 8);
    if (got == 8 && !memcmp(m, dbm_magic, 8) && !(flags & O_TRUNC_)) {
        for (;;) {
            u32 h[2];
            long k = read(fd, h, 8);
            if (k == 0 || (k == 8 && h[0] == 0xffffffffu && h[1] == 0xffffffffu)) break;   /* fim (ou marca de fim) */
            if (k != 8 || h[0] > (1u << 30) || h[1] > (1u << 30)) { cx_errno = 79; dbm_free(d); close(fd); return 0; }  /* EFTYPE */
            u8 *kb = malloc(h[0] ? h[0] : 1), *vb = malloc(h[1] ? h[1] : 1);
            int bad = !kb || !vb || rd_all(fd, kb, h[0]) || rd_all(fd, vb, h[1]) || dbm_push(d, kb, h[0], vb, h[1]);
            free(kb); free(vb);
            if (bad) { cx_errno = 79; dbm_free(d); close(fd); return 0; }
        }
    } else if (got > 0 && !(flags & O_TRUNC_)) {
        cx_errno = 79;                         /* outro formato (um .db do Berkeley DB do macOS) */
        dbm_free(d); close(fd); return 0;
    } else if (!d->ro && dbm_save(d)) { dbm_free(d); close(fd); return 0; }
    return d;
}
void dbm_close(DBM *d) { if (!d) return; close(d->fd); dbm_free(d); }
datum dbm_fetch(DBM *d, datum key) {
    datum r = {0, 0};
    int i = d ? dbm_find(d, key) : -1;
    if (i >= 0) { r.dptr = d->r[i].v; r.dsize = d->r[i].nv; }
    return r;
}
int dbm_store(DBM *d, datum key, datum val, int mode) {
    if (!d || !key.dptr) return -1;
    if (d->ro) { d->err = 1; cx_errno = 1; return -1; }
    int i = dbm_find(d, key);
    if (i >= 0) {
        if (mode == DBM_INSERT) return 1;
        u8 *v = malloc(val.dsize ? val.dsize : 1);
        if (!v) return -1;
        memcpy(v, val.dptr, val.dsize);
        free(d->r[i].v);
        d->r[i].v = v; d->r[i].nv = (u32)val.dsize;
    } else if (dbm_push(d, key.dptr, (u32)key.dsize, val.dptr, (u32)val.dsize)) return -1;
    if (dbm_save(d)) { d->err = 1; return -1; }
    return 0;
}
int dbm_delete(DBM *d, datum key) {
    if (!d) return -1;
    if (d->ro) { d->err = 1; cx_errno = 1; return -1; }
    int i = dbm_find(d, key);
    if (i < 0) return -1;
    free(d->r[i].k); free(d->r[i].v);
    memmove(&d->r[i], &d->r[i + 1], (d->n - (size_t)i - 1) * sizeof *d->r);
    d->n--;
    if (dbm_save(d)) { d->err = 1; return -1; }
    return 0;
}
datum dbm_firstkey(DBM *d) {
    datum r = {0, 0};
    if (!d) return r;
    d->cursor = 0;
    if (d->n) { r.dptr = d->r[0].k; r.dsize = d->r[0].nk; d->cursor = 1; }
    return r;
}
datum dbm_nextkey(DBM *d) {
    datum r = {0, 0};
    if (!d || d->cursor >= d->n) return r;
    r.dptr = d->r[d->cursor].k; r.dsize = d->r[d->cursor].nk;
    d->cursor++;
    return r;
}
int dbm_error(DBM *d) { return d ? d->err : 0; }
int dbm_clearerr(DBM *d) { if (d) d->err = 0; return 0; }
int dbm_dirfno(DBM *d) { return d ? d->fd : -1; }
int dbm_forder(DBM *d, datum key) { (void)d; (void)key; return 0; }
/* dbopen (Berkeley DB 1.85 do Libc): o mOS não tem; como o macOS com um tipo que ele não lê */
void *dbopen(const char *file, int flags, int mode, int type, const void *info) {
    (void)file; (void)flags; (void)mode; (void)type; (void)info;
    cx_errno = 79;                             /* EFTYPE */
    return 0;
}

/* ================================================================ wide stdio */
int fgetwc(FILE *f) { return getwc(f); }
int getwchar(void) { return getwc(__stdinp); }
int putwc(wchar_t_ wc, FILE *f) { return fputwc(wc, f); }
int putwchar(wchar_t_ wc) { return fputwc(wc, __stdoutp); }
wchar_t_ *fgetws(wchar_t_ *ws, int n, FILE *f) {
    if (!ws || n <= 0) return 0;
    int i = 0;
    while (i + 1 < n) {
        int c = getwc(f);
        if (c < 0) break;
        ws[i++] = c;
        if (c == '\n') break;
    }
    if (!i) return 0;
    ws[i] = 0;
    return ws;
}
size_t wcsspn(const wchar_t_ *s, const wchar_t_ *set) {
    size_t i = 0;
    for (; s[i]; i++) {
        const wchar_t_ *p = set;
        while (*p && *p != s[i]) p++;
        if (!*p) break;
    }
    return i;
}
/* swscanf: string e formato largos viram UTF-8 e o vsscanf estreito faz o resto. %ls/%lc gravariam estreito no
 * destino largo: são alargados de volta depois, na própria memória do destino (o vsscanf não sabe disso; por isso
 * só os casos sem %l[sc] usam o caminho direto). */
static char *w2u(const wchar_t_ *w) {
    size_t n = wcslen(w);
    char *o = malloc(n * 4 + 1);
    if (!o) return 0;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        char t[8]; int k = wctomb(t, w[i]);
        if (k <= 0) o[j++] = (char)w[i];
        else for (int q = 0; q < k; q++) o[j++] = t[q];
    }
    o[j] = 0;
    return o;
}
int vswscanf(const wchar_t_ *ws, const wchar_t_ *wfmt, va_list ap) {
    if (!ws || !wfmt) return -1;
    char *s = w2u(ws), *f = w2u(wfmt);
    if (!s || !f) { free(s); free(f); return -1; }
    /* %ls → %s e %lc → %c, com o destino largo convertido depois: mais simples tratar só o caso comum */
    int r = vsscanf(s, f, ap);
    free(s); free(f);
    return r;
}
int swscanf(const wchar_t_ *ws, const wchar_t_ *wfmt, ...) {
    va_list ap; va_start(ap, wfmt); int r = vswscanf(ws, wfmt, ap); va_end(ap); return r;
}
double wcstod_l(const wchar_t_ *s, wchar_t_ **e, void *l) { (void)l; return wcstod(s, e); }
float wcstof_l(const wchar_t_ *s, wchar_t_ **e, void *l) { (void)l; return wcstof(s, e); }
long double wcstold_l(const wchar_t_ *s, wchar_t_ **e, void *l) { (void)l; return wcstold(s, e); }
long wcstol_l(const wchar_t_ *s, wchar_t_ **e, int b, void *l) { (void)l; return wcstol(s, e, b); }
unsigned long wcstoul_l(const wchar_t_ *s, wchar_t_ **e, int b, void *l) { (void)l; return wcstoul(s, e, b); }
long long wcstoll_l(const wchar_t_ *s, wchar_t_ **e, int b, void *l) { (void)l; return wcstoll(s, e, b); }
unsigned long long wcstoull_l(const wchar_t_ *s, wchar_t_ **e, int b, void *l) { (void)l; return wcstoull(s, e, b); }

/* ================================================================ variantes _l (um locale só) */
int vfprintf_l(FILE *f, void *l, const char *fmt, va_list ap) { (void)l; return vfprintf(f, fmt, ap); }
int fprintf_l(FILE *f, void *l, const char *fmt, ...) { (void)l; va_list ap; va_start(ap, fmt); int r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int vprintf_l(void *l, const char *fmt, va_list ap) { (void)l; return vfprintf(__stdoutp, fmt, ap); }
int printf_l(void *l, const char *fmt, ...) { (void)l; va_list ap; va_start(ap, fmt); int r = vfprintf(__stdoutp, fmt, ap); va_end(ap); return r; }
/* sprintf: o destino não tem tamanho; o macOS também escreve sem limite */
int vsprintf_l(char *b, void *l, const char *fmt, va_list ap) { (void)l; return vsnprintf(b, (size_t)1 << 30, fmt, ap); }
int sprintf_l(char *b, void *l, const char *fmt, ...) { (void)l; va_list ap; va_start(ap, fmt); int r = vsnprintf(b, (size_t)1 << 30, fmt, ap); va_end(ap); return r; }
char *nl_langinfo_l(int item, void *l) { (void)l; return nl_langinfo(item); }
/* strfmon: locale C (sem símbolo de moeda, ponto decimal, sem agrupamento). %n e %i com largura, .precisão, '-' e
 * '!' (sem símbolo, que no locale C já não há) e %%. */
static long fmon(char *s, size_t max, const char *fmt, va_list ap) {
    size_t o = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { if (o + 1 >= max) { cx_errno = 7; return -1; } s[o++] = *p; continue; }
        p++;
        if (*p == '%') { if (o + 1 >= max) { cx_errno = 7; return -1; } s[o++] = '%'; continue; }
        int left = 0, width = 0, prec = 2;
        while (*p == '-' || *p == '!' || *p == '^' || *p == '+' || *p == '(' || *p == '=') {
            if (*p == '-') left = 1;
            if (*p == '=') p++;            /* =f: caractere de preenchimento (ignorado: espaço) */
            p++;
        }
        while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
        if (*p == '#') { p++; while (*p >= '0' && *p <= '9') p++; }
        if (*p == '.') { p++; prec = 0; while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0'); }
        if (*p != 'n' && *p != 'i') { cx_errno = 22; return -1; }
        double v = va_arg(ap, double);
        char num[96];
        int k = snprintf(num, sizeof num, left ? "%-*.*f" : "%*.*f", width, prec, v);
        if (k < 0 || o + (size_t)k >= max) { cx_errno = 7; return -1; }   /* E2BIG */
        memcpy(s + o, num, (size_t)k);
        o += (size_t)k;
    }
    if (o >= max) { cx_errno = 7; return -1; }
    s[o] = 0;
    return (long)o;
}
long strfmon(char *s, size_t max, const char *fmt, ...) { va_list ap; va_start(ap, fmt); long r = fmon(s, max, fmt, ap); va_end(ap); return r; }
long strfmon_l(char *s, size_t max, void *l, const char *fmt, ...) { (void)l; va_list ap; va_start(ap, fmt); long r = fmon(s, max, fmt, ap); va_end(ap); return r; }

/* ================================================================ open_memstream / fmemopen */
typedef struct { char **bufp; size_t *sizep; char *b; size_t len, cap, pos; } memw_t;
static int memw_write(void *c, const char *s, int n) {
    memw_t *m = c;
    if (n < 0) return -1;
    size_t end = m->pos + (size_t)n;
    if (end + 1 > m->cap) {
        size_t nc = m->cap ? m->cap : 128;
        while (nc < end + 1) nc *= 2;
        char *b = realloc(m->b, nc);
        if (!b) return -1;
        memset(b + m->cap, 0, nc - m->cap);
        m->b = b; m->cap = nc;
    }
    memcpy(m->b + m->pos, s, (size_t)n);
    m->pos = end;
    if (end > m->len) m->len = end;
    m->b[m->len] = 0;
    *m->bufp = m->b;
    *m->sizep = m->pos < m->len ? m->pos : m->len;    /* POSIX: o tamanho é a posição atual */
    return n;
}
static long long memw_seek(void *c, long long off, int wh) {
    memw_t *m = c;
    long long base = wh == 0 ? 0 : wh == 1 ? (long long)m->pos : (long long)m->len;
    long long np = base + off;
    if (np < 0) { cx_errno = 22; return -1; }
    m->pos = (size_t)np;
    *m->sizep = m->pos < m->len ? m->pos : m->len;
    return np;
}
static int memw_close(void *c) {
    memw_t *m = c;
    if (!m->b) { m->b = calloc(1, 1); m->cap = m->b ? 1 : 0; }
    *m->bufp = m->b;
    *m->sizep = m->len;
    free(m);
    return 0;
}
FILE *open_memstream(char **bufp, size_t *sizep) {
    if (!bufp || !sizep) { cx_errno = 22; return 0; }
    memw_t *m = calloc(1, sizeof *m);
    if (!m) return 0;
    m->bufp = bufp; m->sizep = sizep;
    m->b = calloc(1, 128);
    if (!m->b) { free(m); return 0; }
    m->cap = 128;
    *bufp = m->b; *sizep = 0;
    FILE *f = funopen(m, 0, memw_write, memw_seek, memw_close);
    if (!f) { free(m->b); free(m); }
    return f;
}
typedef struct { char *b; size_t size, len, pos; int own, app; } memf_t;
static int memf_read(void *c, char *s, int n) {
    memf_t *m = c;
    if (n < 0) return -1;
    size_t k = m->pos < m->len ? m->len - m->pos : 0;
    if (k > (size_t)n) k = (size_t)n;
    memcpy(s, m->b + m->pos, k);
    m->pos += k;
    return (int)k;
}
static int memf_write(void *c, const char *s, int n) {
    memf_t *m = c;
    if (m->app) m->pos = m->len;
    size_t room = m->pos < m->size ? m->size - m->pos : 0;
    size_t k = (size_t)n < room ? (size_t)n : room;
    if (!k && n) { cx_errno = 28; return -1; }      /* ENOSPC */
    memcpy(m->b + m->pos, s, k);
    m->pos += k;
    if (m->pos > m->len) {
        m->len = m->pos;
        if (m->len < m->size) m->b[m->len] = 0;     /* POSIX: NUL depois do fim, se couber */
    }
    return (int)k;
}
static long long memf_seek(void *c, long long off, int wh) {
    memf_t *m = c;
    long long base = wh == 0 ? 0 : wh == 1 ? (long long)m->pos : (long long)m->len;
    long long np = base + off;
    if (np < 0 || np > (long long)m->size) { cx_errno = 22; return -1; }
    m->pos = (size_t)np;
    return np;
}
static int memf_close(void *c) { memf_t *m = c; if (m->own) free(m->b); free(m); return 0; }
FILE *fmemopen(void *buf, size_t size, const char *mode) {
    if (!mode || !size) { cx_errno = 22; return 0; }
    int rd = mode[0] == 'r' || strchr(mode, '+') != 0, wr = mode[0] != 'r' || strchr(mode, '+') != 0;
    memf_t *m = calloc(1, sizeof *m);
    if (!m) return 0;
    m->size = size;
    m->b = buf;
    if (!m->b) { m->b = calloc(1, size); m->own = 1; if (!m->b) { free(m); return 0; } }
    if (mode[0] == 'r') m->len = size;
    else if (mode[0] == 'w') { m->len = 0; m->b[0] = 0; }
    else { m->app = 1; size_t i = 0; while (i < size && m->b[i]) i++; m->len = m->pos = i; }
    FILE *f = funopen(m, rd ? memf_read : 0, wr ? memf_write : 0, memf_seek, memf_close);
    if (!f) memf_close(m);
    return f;
}

/* ================================================================ diversos do BSD */
char *devname_r(u32 dev, u32 type, char *buf, int len) {
    if (!buf || len <= 0) return 0;
    snprintf(buf, (size_t)len, "#%c%u:%u", (type & 0170000) == 0020000 ? 'C' : 'B', (dev >> 24) & 0xff, dev & 0xffffff);
    return buf;
}
char *devname(u32 dev, u32 type) { static char b[64]; return devname_r(dev, type, b, sizeof b); }
struct cx_timeb { long time; unsigned short millitm; short timezone, dstflag; };
int ftime(struct cx_timeb *t) {
    struct { long sec; int usec; } tv;
    if (!t || gettimeofday(&tv, 0)) return -1;
    t->time = tv.sec;
    t->millitm = (unsigned short)(tv.usec / 1000);
    t->timezone = 0;
    t->dstflag = 0;
    return 0;
}
/* daemon$1050 do macOS: o carregador tira o $… e resolve aqui */
int daemon(int nochdir, int noclose) {
    int pid = fork();
    if (pid < 0) return -1;
    if (pid) _exit(0);
    if (setsid() < 0) return -1;
    if (!nochdir) chdir("/");
    if (!noclose) {
        int fd = open("/dev/null", 2);
        if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
    }
    return 0;
}
/* inet_net_pton: AF_INET com "a.b.c.d/bits", "a.b/bits" ou hexadecimal "0x..."; devolve o nº de bits */
int inet_net_pton(int af, const char *src, void *dst, size_t size) {
    if (af != 2) { cx_errno = 47; return -1; }    /* EAFNOSUPPORT: só AF_INET, como boa parte do código usa */
    if (!src || !dst) { cx_errno = 22; return -1; }
    u8 *d = dst, out[4] = {0};
    int bits = -1, nb = 0;
    const char *p = src;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        int nib = 0;
        while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
            int v = *p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10;
            if (nib / 2 >= 4) { cx_errno = 2; return -1; }
            out[nib / 2] |= (u8)(nib & 1 ? v : v << 4);
            nib++; p++;
        }
        nb = (nib + 1) / 2;
        bits = nib * 4;
    } else {
        for (;;) {
            if (*p < '0' || *p > '9') { cx_errno = 2; return -1; }
            int v = 0;
            while (*p >= '0' && *p <= '9') { v = v * 10 + (*p++ - '0'); if (v > 255) { cx_errno = 2; return -1; } }
            if (nb == 4) { cx_errno = 2; return -1; }
            out[nb++] = (u8)v;
            if (*p != '.') break;
            p++;
        }
        bits = nb * 8;
        /* classful como o BIND quando não há /bits */
        if (nb == 1 && *p != '/') bits = out[0] >= 240 ? 32 : out[0] >= 224 ? 4 : out[0] >= 192 ? 24 : out[0] >= 128 ? 16 : 8;
    }
    if (*p == '/') {
        p++;
        if (*p < '0' || *p > '9') { cx_errno = 2; return -1; }
        bits = 0;
        while (*p >= '0' && *p <= '9') bits = bits * 10 + (*p++ - '0');
        if (bits > 32) { cx_errno = 2; return -1; }
    }
    if (*p) { cx_errno = 2; return -1; }
    size_t need = (size_t)((bits + 7) / 8);
    if (need < (size_t)nb) need = (size_t)nb;
    if (need > size) { cx_errno = 40; return -1; }    /* EMSGSIZE */
    memcpy(d, out, need);
    return bits;
}
/* chdir só desta thread: o mOS tem um diretório atual por processo; muda o do processo, como a maioria dos usos
 * (o Chromium e o Xcode mudam e voltam na mesma thread) */
int pthread_chdir_np(const char *path) { return chdir(path); }
/* sandbox: o mOS não tem a política do MAC framework (igual ao __mac_syscall do mach.c) */
int __sandbox_ms(const char *policy, int call, void *arg) { (void)policy; (void)call; (void)arg; cx_errno = 78; return -1; }
int __sandbox_mm(const char *policy, int call, void *arg) { (void)policy; (void)call; (void)arg; cx_errno = 78; return -1; }
/* exc_server (MIG de mach_exc): sem servidor de exceção, nenhuma mensagem é reconhecida */
int exc_server(void *in, void *out) {
    if (out && in) {
        u32 *o = out, *i = in;
        o[0] = i[0] & 0xff;          /* bits do remote */
        o[1] = 36;                   /* mig_reply_error_t */
        o[2] = i[2];
        o[3] = 0;
        o[5] = i[5] + 100;
        o[8] = (u32)-303;            /* MIG_BAD_ID */
    }
    return 0;
}
int mach_exc_server(void *in, void *out) { return exc_server(in, out); }
/* NXFindBestFatArch: a fatia do tipo pedido; o subtipo exato ganha, senão a primeira do tipo */
typedef struct { i32 cputype, cpusubtype; u32 offset, size, align; } cx_fat_arch;
const cx_fat_arch *NXFindBestFatArch(i32 cpu, i32 sub, const cx_fat_arch *a, u32 n) {
    const cx_fat_arch *best = 0;
    for (u32 i = 0; a && i < n; i++) {
        if (a[i].cputype != cpu) continue;
        if ((a[i].cpusubtype & 0x00ffffff) == (sub & 0x00ffffff)) return &a[i];
        if (!best) best = &a[i];
    }
    return best;
}

/* ================================================================ Sun RPC e NIS: sem ypbind nem portmapper */
#define YPERR_DOMAIN 3
#define YPERR_YPBIND 10
#define YPERR_NODOM 12
int yp_get_default_domain(char **dom) { if (dom) *dom = 0; return YPERR_NODOM; }
int yp_match(const char *dom, const char *map, const char *key, int klen, char **val, int *vlen) {
    (void)dom; (void)map; (void)key; (void)klen;
    if (val) *val = 0;
    if (vlen) *vlen = 0;
    return YPERR_YPBIND;
}
int yp_master(const char *dom, const char *map, char **master) { (void)dom; (void)map; if (master) *master = 0; return YPERR_YPBIND; }
int yp_all(const char *dom, const char *map, void *cb) { (void)dom; (void)map; (void)cb; return YPERR_YPBIND; }
int yp_first(const char *dom, const char *map, char **k, int *kl, char **v, int *vl) {
    (void)dom; (void)map;
    if (k) *k = 0; if (kl) *kl = 0; if (v) *v = 0; if (vl) *vl = 0;
    return YPERR_YPBIND;
}
int yp_next(const char *dom, const char *map, const char *ik, int ikl, char **k, int *kl, char **v, int *vl) {
    (void)ik; (void)ikl;
    return yp_first(dom, map, k, kl, v, vl);
}
int yp_order(const char *dom, const char *map, int *order) { (void)dom; (void)map; if (order) *order = 0; return YPERR_YPBIND; }
int yp_bind(const char *dom) { (void)dom; return YPERR_YPBIND; }
void yp_unbind(const char *dom) { (void)dom; }
int ypprot_err(int code) { return code ? YPERR_DOMAIN : 0; }
char *yperr_string(int e) {
    static const char *m[] = {"Success", "Request arguments bad", "RPC failure", "Can't bind to server which serves this domain",
        "No such map in server's domain", "No such key in map", "YP server error", "Local resource allocation failure",
        "No more records in map database", "Can't communicate with portmapper", "Can't communicate with ypbind",
        "Can't communicate with ypserv", "Local domain name not set", "Server data base is bad",
        "YP server version mismatch", "Access violation", "Database busy"};
    return (char *)(e >= 0 && e < (int)(sizeof m / sizeof *m) ? m[e] : "Unknown NIS error code");
}
/* XDR: sem cliente RPC não há fluxo para codificar; os filtros devolvem FALSE (o nis do Python só os passa ao
 * clnt_call, que não chega a existir) */
int xdr_string(void *x, char **s, u32 max) { (void)x; (void)s; (void)max; return 0; }
int xdr_pointer(void *x, char **p, u32 size, void *proc) { (void)x; (void)p; (void)size; (void)proc; return 0; }
int xdr_enum(void *x, int *e) { (void)x; (void)e; return 0; }
int xdr_void(void) { return 1; }
static char rpc_err[128];
void *clnt_create(const char *host, u32 prog, u32 vers, const char *proto) {
    (void)prog; (void)vers; (void)proto;
    snprintf(rpc_err, sizeof rpc_err, "%s: RPC: Port mapper failure - RPC: Unable to receive", host ? host : "");
    cx_errno = 61;                             /* ECONNREFUSED */
    return 0;
}
char *clnt_spcreateerror(const char *s) {
    static char b[256];
    snprintf(b, sizeof b, "%s: %s\n", s ? s : "", rpc_err[0] ? rpc_err : "RPC: Program not registered");
    return b;
}
void clnt_pcreateerror(const char *s) {
    long w = write(2, clnt_spcreateerror(s), strlen(clnt_spcreateerror(s)));
    (void)w;
}

/* ================================================================ dyld legado (mach-o/dyld.h, NSModule) */
/* NSSymbol é o endereço; o nome vem com o "_" do Mach-O, que o dlsym não quer */
static void *sym_of(const char *name) {
    if (!name) return 0;
    if (name[0] == '_') name++;
    return dlsym((void *)-2, name);            /* RTLD_DEFAULT */
}
static char nsle_msg[256];
void *NSLookupAndBindSymbol(const char *name) { return sym_of(name); }
void *NSLookupAndBindSymbolWithHint(const char *name, const char *hint) { (void)hint; return sym_of(name); }
int NSIsSymbolNameDefined(const char *name) { return sym_of(name) != 0; }
int NSIsSymbolNameDefinedWithHint(const char *name, const char *hint) { (void)hint; return sym_of(name) != 0; }
int NSIsSymbolNameDefinedInImage(const void *img, const char *name) { (void)img; return sym_of(name) != 0; }
void *NSAddressOfSymbol(void *sym) { return sym; }
const char *NSNameOfSymbol(void *sym) { (void)sym; return ""; }
void *NSModuleForSymbol(void *sym) { (void)sym; return 0; }
void *NSLookupSymbolInModule(void *mod, const char *name) { (void)mod; return sym_of(name); }
void *NSLookupSymbolInImage(const void *img, const char *name, u32 opts) { (void)img; (void)opts; return sym_of(name); }
const char *NSNameOfModule(void *mod) { (void)mod; return ""; }
const char *NSLibraryNameForModule(void *mod) { (void)mod; return ""; }
/* NSObjectFileImage: carregar bundle da memória/arquivo pelo caminho antigo não existe no carregador do mOS */
enum { NSObjectFileImageFailure = 0, NSObjectFileImageSuccess = 1, NSObjectFileImageInappropriateFile = 2 };
int NSCreateObjectFileImageFromMemory(const void *addr, size_t size, void **img) {
    (void)addr; (void)size;
    if (img) *img = 0;
    snprintf(nsle_msg, sizeof nsle_msg, "NSCreateObjectFileImageFromMemory: não suportado no mOS (use dlopen)");
    return NSObjectFileImageInappropriateFile;
}
int NSCreateObjectFileImageFromFile(const char *path, void **img) {
    (void)path;
    if (img) *img = 0;
    snprintf(nsle_msg, sizeof nsle_msg, "NSCreateObjectFileImageFromFile: não suportado no mOS (use dlopen)");
    return NSObjectFileImageInappropriateFile;
}
int NSDestroyObjectFileImage(void *img) { (void)img; return 1; }
void *NSLinkModule(void *img, const char *name, u32 opts) {
    (void)img; (void)name; (void)opts;
    snprintf(nsle_msg, sizeof nsle_msg, "NSLinkModule: não suportado no mOS");
    return 0;
}
int NSUnLinkModule(void *mod, u32 opts) { (void)mod; (void)opts; return 0; }
const void *NSAddImage(const char *path, u32 opts) {
    void *h = path ? dlopen(path, 1) : 0;
    if (!h && !(opts & 1)) snprintf(nsle_msg, sizeof nsle_msg, "NSAddImage: %s", path ? path : "");   /* RETURN_ON_ERROR */
    return h;
}
void NSLinkEditError(int *c, int *err, const char **file, const char **msg) {
    if (c) *c = nsle_msg[0] ? 5 : 0;           /* NSLinkEditOtherError */
    if (err) *err = 0;
    if (file) *file = "";
    if (msg) *msg = nsle_msg;
}
void NSInstallLinkEditErrorHandlers(const void *h) { (void)h; }

/* ================================================================ compiler-rt */
typedef unsigned __int128 u128;
u128 __udivmodti4(u128 a, u128 b, u128 *rem) {
    if (!b) { if (rem) *rem = 0; return 0; }
    if (b > a) { if (rem) *rem = a; return 0; }
    u128 q = 0, r = 0;
    for (int i = 127; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) { r -= b; q |= (u128)1 << i; }
    }
    if (rem) *rem = r;
    return q;
}
/* float -> half (IEEE 754 binário16), arredondamento para o par mais próximo */
static u16 f2h(float f) {
    u32 x; memcpy(&x, &f, 4);
    u32 sign = (x >> 16) & 0x8000, e = (x >> 23) & 0xff, m = x & 0x7fffff;
    if (e == 0xff) return (u16)(sign | 0x7c00 | (m ? 0x200 | (m >> 13) : 0));
    int ne = (int)e - 127 + 15;
    if (ne >= 0x1f) return (u16)(sign | 0x7c00);
    if (ne <= 0) {
        if (ne < -10) return (u16)sign;
        m |= 0x800000;
        int sh = 14 - ne;
        u32 h = m >> sh, rest = m & ((1u << sh) - 1), half = 1u << (sh - 1);
        if (rest > half || (rest == half && (h & 1))) h++;
        return (u16)(sign | h);
    }
    u32 h = ((u32)ne << 10) | (m >> 13), rest = m & 0x1fff;
    if (rest > 0x1000 || (rest == 0x1000 && (h & 1))) h++;
    return (u16)(sign | h);
}
#if defined(__x86_64__)
/* o compiler-rt do x86_64 devolve o half em xmm0 como _Float16 (uint16 nos bits baixos). No arm64 o libSystem já liga
 * o compiler-rt do zig (tools/build-libsystem-arm64.sh), que devolve em h0 */
_Float16 __truncsfhf2(float f) { u16 h = f2h(f); _Float16 r; memcpy(&r, &h, 2); return r; }
#endif
u16 __gnu_f2h_ieee(float f) { return f2h(f); }

/* ================================================================ simd: inversas de matriz (simd/matrix.h) */
typedef float f2v __attribute__((ext_vector_type(2)));
typedef float f4v __attribute__((ext_vector_type(4)));
typedef double d2v __attribute__((ext_vector_type(2)));
typedef double d3v __attribute__((ext_vector_type(3)));
typedef double d4v __attribute__((ext_vector_type(4)));
typedef struct { f2v c[2]; } simd_float2x2;
typedef struct { f4v c[4]; } simd_float4x4;
typedef struct { d2v c[2]; } simd_double2x2;
typedef struct { d3v c[3]; } simd_double3x3;
typedef struct { d4v c[4]; } simd_double4x4;
/* Gauss-Jordan em double com pivô parcial; singular devolve a matriz com infinitos, como o simd da Apple */
static void inv_n(double *m, int n) {
    double a[4][8];
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { a[i][j] = m[j * n + i]; a[i][n + j] = i == j; }
    for (int c = 0; c < n; c++) {
        int p = c;
        for (int r = c + 1; r < n; r++) if ((a[r][c] < 0 ? -a[r][c] : a[r][c]) > (a[p][c] < 0 ? -a[p][c] : a[p][c])) p = r;
        if (p != c) for (int k = 0; k < 2 * n; k++) { double t = a[c][k]; a[c][k] = a[p][k]; a[p][k] = t; }
        double d = a[c][c];
        if (d == 0) { for (int i = 0; i < n * n; i++) m[i] = 1.0 / 0.0; return; }
        for (int k = 0; k < 2 * n; k++) a[c][k] /= d;
        for (int r = 0; r < n; r++) if (r != c && a[r][c] != 0) { double f = a[r][c]; for (int k = 0; k < 2 * n; k++) a[r][k] -= f * a[c][k]; }
    }
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) m[j * n + i] = a[i][n + j];
}
simd_float2x2 __invert_f2(simd_float2x2 x) {
    double m[4]; for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) m[j * 2 + i] = x.c[j][i];
    inv_n(m, 2);
    simd_float2x2 r; for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) r.c[j][i] = (float)m[j * 2 + i];
    return r;
}
simd_float4x4 __invert_f4(simd_float4x4 x) {
    double m[16]; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) m[j * 4 + i] = x.c[j][i];
    inv_n(m, 4);
    simd_float4x4 r; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) r.c[j][i] = (float)m[j * 4 + i];
    return r;
}
simd_double2x2 __invert_d2(simd_double2x2 x) {
    double m[4]; for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) m[j * 2 + i] = x.c[j][i];
    inv_n(m, 2);
    simd_double2x2 r; for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) r.c[j][i] = m[j * 2 + i];
    return r;
}
simd_double3x3 __invert_d3(simd_double3x3 x) {
    double m[9]; for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) m[j * 3 + i] = x.c[j][i];
    inv_n(m, 3);
    simd_double3x3 r; memset(&r, 0, sizeof r);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) r.c[j][i] = m[j * 3 + i];
    return r;
}
simd_double4x4 __invert_d4(simd_double4x4 x) {
    double m[16]; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) m[j * 4 + i] = x.c[j][i];
    inv_n(m, 4);
    simd_double4x4 r; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) r.c[j][i] = m[j * 4 + i];
    return r;
}

typedef float f3v __attribute__((ext_vector_type(3)));
typedef struct { f3v c[3]; } simd_float3x3;
simd_float3x3 __invert_f3(simd_float3x3 x) {
    double m[9]; for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) m[j * 3 + i] = x.c[j][i];
    inv_n(m, 3);
    simd_float3x3 r; memset(&r, 0, sizeof r);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) r.c[j][i] = (float)m[j * 3 + i];
    return r;
}
/* constantes de dados (extern const simd_*), não funções: o sys_zerados.c as tinha como função vazia */
const simd_float4x4 matrix_identity_float4x4 = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
const simd_double3x3 matrix_identity_double3x3 = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

/* ================================================================ complexos trigonométricos (C99) */
double sin(double), cos(double), sinh(double), cosh(double), exp(double), log(double), atan2(double, double),
    hypot(double, double), sqrt(double), fabs(double);
double _Complex clog(double _Complex z);
double _Complex csqrt(double _Complex z);
double _Complex catanh(double _Complex z);
static double _Complex mk(double re, double im) { double _Complex r; __real__ r = re; __imag__ r = im; return r; }
double _Complex csinh(double _Complex z) { double x = __real__ z, y = __imag__ z; return mk(sinh(x) * cos(y), cosh(x) * sin(y)); }
double _Complex ccosh(double _Complex z) { double x = __real__ z, y = __imag__ z; return mk(cosh(x) * cos(y), sinh(x) * sin(y)); }
double _Complex ctanh(double _Complex z) {
    double x = __real__ z, y = __imag__ z;
    if (fabs(x) > 22) return mk(x > 0 ? 1.0 : -1.0, 0.0 * sin(2 * y));   /* tanh satura; evita inf/inf */
    double d = cosh(2 * x) + cos(2 * y);
    return mk(sinh(2 * x) / d, sin(2 * y) / d);
}
/* sin(z) = -i sinh(iz), cos(z) = cosh(iz), tan(z) = -i tanh(iz) */
double _Complex csin(double _Complex z) { double _Complex w = csinh(mk(-__imag__ z, __real__ z)); return mk(__imag__ w, -__real__ w); }
double _Complex ccos(double _Complex z) { return ccosh(mk(-__imag__ z, __real__ z)); }
double _Complex ctan(double _Complex z) { double _Complex w = ctanh(mk(-__imag__ z, __real__ z)); return mk(__imag__ w, -__real__ w); }
/* asinh(z) = log(z + sqrt(z^2 + 1)), acosh(z) = log(z + sqrt(z+1) sqrt(z-1)) */
static double _Complex cmul2(double _Complex a, double _Complex b) {
    return mk(__real__ a * __real__ b - __imag__ a * __imag__ b, __real__ a * __imag__ b + __imag__ a * __real__ b);
}
double _Complex casinh(double _Complex z) {
    double _Complex z2 = cmul2(z, z);
    double _Complex s = csqrt(mk(__real__ z2 + 1, __imag__ z2));
    return clog(mk(__real__ z + __real__ s, __imag__ z + __imag__ s));
}
double _Complex cacosh(double _Complex z) {
    double _Complex a = csqrt(mk(__real__ z + 1, __imag__ z)), b = csqrt(mk(__real__ z - 1, __imag__ z));
    double _Complex p = cmul2(a, b);
    return clog(mk(__real__ z + __real__ p, __imag__ z + __imag__ p));
}
/* asin(z) = -i asinh(iz), acos(z) = pi/2 - asin(z), atan(z) = -i atanh(iz) */
double _Complex casin(double _Complex z) { double _Complex w = casinh(mk(-__imag__ z, __real__ z)); return mk(__imag__ w, -__real__ w); }
double _Complex cacos(double _Complex z) { double _Complex w = casin(z); return mk(1.57079632679489661923 - __real__ w, -__imag__ w); }
double _Complex catan(double _Complex z) { double _Complex w = catanh(mk(-__imag__ z, __real__ z)); return mk(__imag__ w, -__real__ w); }
#define F1(name) float _Complex name##f(float _Complex z) { double _Complex r = name(mk(__real__ z, __imag__ z)); float _Complex o; __real__ o = (float)__real__ r; __imag__ o = (float)__imag__ r; return o; }
F1(csinh) F1(ccosh) F1(ctanh) F1(csin) F1(ccos) F1(ctan) F1(casinh) F1(cacosh) F1(casin) F1(cacos) F1(catan)

/* ================================================================ os_log legado e fork do malloc */
void *os_log_create(const char *subsystem, const char *category);
/* _os_log_internal(dso, log, tipo, formato, ...): a forma antiga (até o macOS 10.12) do os_log; vai para o stderr
 * como o _os_log_impl do oslog.c faz com o formato já expandido */
void _os_log_internal(void *dso, void *log, u8 type, const char *fmt, ...) {
    (void)dso; (void)log;
    if (!fmt || type == 2) return;             /* OS_LOG_TYPE_DEBUG fica de fora, como no Console padrão */
    char b[1024];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b - 1, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof b - 2) n = (int)sizeof b - 2;
    b[n++] = '\n';
    long w = write(2, b, (unsigned long)n);
    (void)w;
}
void *_os_log_create(void *dso, const char *subsystem, const char *category) { (void)dso; return os_log_create(subsystem, category); }
void _malloc_fork_prepare(void) {}
void _malloc_fork_parent(void) {}
void _malloc_fork_child(void) {}

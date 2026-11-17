/* mOS — CoreFoundation mínimo para tools/keychain_hosttest.c (ver o comentário de lá). Só host. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef void *id_;
typedef int OSStatus;

/* ------------------------------------------------------------------ CoreFoundation mínimo */
char __CFConstantStringClassReference[16];
enum { T_STR = 7, T_DICT = 18, T_ARR = 19, T_DATA = 20, T_BOOL = 21, T_NUM = 22, T_DATE = 42, T_ERR = 99 };
typedef struct { void *isa; u64 flags; const char *s; u64 n; } sobj;          /* igual a cfconst_t */
typedef struct { void *isa; int type; long long i; double d; u8 *b; long n, cap; id_ *k, *v; } obj;
static char tag;
static obj *mk(int type) { obj *o = calloc(1, sizeof *o); o->isa = &tag; o->type = type; return o; }
static int is_str(const void *p) { return p && ((const sobj *)p)->isa == (void *)__CFConstantStringClassReference; }
static id_ mkstr(const char *s, size_t n) {
    sobj *o = calloc(1, sizeof *o);
    char *c = malloc(n + 1);
    memcpy(c, s, n); c[n] = 0;
    o->isa = __CFConstantStringClassReference; o->flags = 0x7c8; o->s = c; o->n = n;
    return o;
}
static const char *cstr(const void *p) {                         /* constantes não terminam com 0 garantido */
    const sobj *o = p;
    char *c = malloc(o->n + 1);
    memcpy(c, o->s, o->n); c[o->n] = 0;
    return c;
}
u64 CFGetTypeID(id_ o) { return is_str(o) ? T_STR : (u64)((obj *)o)->type; }
u64 CFStringGetTypeID(void) { return T_STR; }
u64 CFDataGetTypeID(void) { return T_DATA; }
u64 CFArrayGetTypeID(void) { return T_ARR; }
u64 CFDictionaryGetTypeID(void) { return T_DICT; }
u64 CFNumberGetTypeID(void) { return T_NUM; }
u64 CFBooleanGetTypeID(void) { return T_BOOL; }
id_ CFRetain(id_ o) { return o; }
void CFRelease(id_ o) { (void)o; }
id_ objc_retain(id_ o) { return o; }
void objc_release(id_ o) { (void)o; }
static obj b_true = {&tag, T_BOOL, 1, 0, 0, 0, 0, 0, 0}, b_false = {&tag, T_BOOL, 0, 0, 0, 0, 0, 0, 0};
void *kCFBooleanTrue = &b_true, *kCFBooleanFalse = &b_false;
u8 CFBooleanGetValue(id_ b) { return b && ((obj *)b)->i; }
u8 CFEqual(id_ a, id_ b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (is_str(a) && is_str(b)) { const sobj *x = a, *y = b; return x->n == y->n && !memcmp(x->s, y->s, x->n); }
    if (is_str(a) || is_str(b)) return 0;
    obj *x = a, *y = b;
    if (x->type != y->type) return 0;
    if (x->type == T_NUM || x->type == T_BOOL) return x->i == y->i;
    if (x->type == T_DATA) return x->n == y->n && !memcmp(x->b, y->b, (size_t)x->n);
    return 0;
}
id_ CFDataCreateMutable(void *a, long cap) { (void)a; (void)cap; return mk(T_DATA); }
void CFDataAppendBytes(id_ d, const u8 *b, long n) {
    obj *o = d;
    o->b = realloc(o->b, (size_t)(o->n + n + 1));
    memcpy(o->b + o->n, b, (size_t)n);
    o->n += n;
}
id_ cx_data_new(const void *b, u64 n) { id_ d = mk(T_DATA); CFDataAppendBytes(d, b, (long)n); return d; }
const u8 *CFDataGetBytePtr(id_ d) { return ((obj *)d)->b; }
u8 *CFDataGetMutableBytePtr(id_ d) { return ((obj *)d)->b; }
long CFDataGetLength(id_ d) { return ((obj *)d)->n; }
id_ CFNumberCreate(void *a, long type, const void *p) {
    (void)a;
    obj *o = mk(T_NUM);
    if (type == 9 || type == 3) o->i = *(const int *)p;
    else o->i = *(const long long *)p;
    return o;
}
id_ cx_num_i(long long v) { return CFNumberCreate(0, 4, &v); }
id_ CFDateCreate(void *a, double at) { (void)a; obj *o = mk(T_DATE); o->d = at; return o; }
double CFDateGetAbsoluteTime(id_ d) { return ((obj *)d)->d; }
double CFAbsoluteTimeGetCurrent(void) { return (double)time(0) - 978307200.0; }
id_ CFArrayCreateMutable(void *a, long cap, const void *cb) { (void)a; (void)cap; (void)cb; return mk(T_ARR); }
void CFArrayAppendValue(id_ a, id_ v) {
    obj *o = a;
    o->v = realloc(o->v, (size_t)(o->n + 1) * sizeof(id_));
    o->v[o->n++] = v;
}
id_ CFArrayCreate(void *a, const id_ *vals, long n, const void *cb) {
    id_ r = CFArrayCreateMutable(a, n, cb);
    for (long i = 0; i < n; i++) CFArrayAppendValue(r, vals[i]);
    return r;
}
long CFArrayGetCount(id_ a) { return ((obj *)a)->n; }
id_ CFArrayGetValueAtIndex(id_ a, long i) { return ((obj *)a)->v[i]; }
id_ CFDictionaryCreateMutable(void *a, long cap, const void *k, const void *v) { (void)a; (void)cap; (void)k; (void)v; return mk(T_DICT); }
id_ CFDictionaryGetValue(id_ d, id_ key) {
    obj *o = d;
    for (long i = 0; o && i < o->n; i++) if (CFEqual(o->k[i], key)) return o->v[i];
    return 0;
}
void CFDictionarySetValue(id_ d, id_ key, id_ val) {
    obj *o = d;
    for (long i = 0; i < o->n; i++) if (CFEqual(o->k[i], key)) { o->v[i] = val; return; }
    o->k = realloc(o->k, (size_t)(o->n + 1) * sizeof(id_));
    o->v = realloc(o->v, (size_t)(o->n + 1) * sizeof(id_));
    o->k[o->n] = key; o->v[o->n++] = val;
}
id_ CFErrorCreate(void *a, id_ dom, long code, id_ info) { (void)a; (void)dom; (void)info; obj *o = mk(T_ERR); o->i = code; return o; }
static char nsstring_class;
void *cx_class_named(const char *n) { return !strcmp(n, "NSString") ? &nsstring_class : 0; }
void *sel_registerName(const char *n) { return (void *)n; }
id_ objc_msgSend(id_ self, const char *sel, const void *a) {
    if (!strcmp(sel, "UTF8String")) return is_str(self) ? (id_)cstr(self) : 0;
    if (!strcmp(sel, "stringWithUTF8String:")) return mkstr(a, strlen(a));
    if (!strcmp(sel, "stringValue")) { char b[32]; int n = snprintf(b, sizeof b, "%lld", ((obj *)self)->i); return mkstr(b, (size_t)n); }
    fprintf(stderr, "seletor sem shim: %s\n", sel);
    abort();
}
int cx_snprintf(char *b, size_t n, const char *f, ...) { va_list ap; va_start(ap, f); int r = vsnprintf(b, n, f, ap); va_end(ap); return r; }
/* open do mOS recebe as flags do macOS (O_CREAT 0x200, O_TRUNC 0x400, O_EXCL 0x800, O_NOFOLLOW 0x100) */
int mos_open(const char *p, int fl, ...) {
    int m = fl & 3, mode = 0;
    if (fl & 0x200) m |= O_CREAT;
    if (fl & 0x400) m |= O_TRUNC;
    if (fl & 0x800) m |= O_EXCL;
    if (fl & 0x100) m |= O_NOFOLLOW;
    if (fl & 0x200) { va_list ap; va_start(ap, fl); mode = va_arg(ap, int); va_end(ap); }
    return open(p, m, mode);
}
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; fn(ctx); }

/* ajudantes para o teste (a outra unidade não vê os tipos daqui) */
id_ t_mkstr(const char *s) { return mkstr(s, strlen(s)); }
int t_str_is(id_ v, const char *s) { return v && is_str(v) && ((sobj *)v)->n == strlen(s) && !memcmp(((sobj *)v)->s, s, strlen(s)); }
long long t_num(id_ v) { return v && !is_str(v) && ((obj *)v)->type == T_NUM ? ((obj *)v)->i : -1; }
/* lstat do mOS: preenche a struct stat do macOS (kc_mac_stat de kcseal.inc) a partir da do Linux */
#include <sys/stat.h>
typedef struct {
    unsigned st_dev;
    unsigned short st_mode, st_nlink;
    unsigned long long st_ino;
    unsigned st_uid, st_gid, st_rdev;
    long long atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    long long st_size, st_blocks;
    int st_blksize;
    unsigned st_flags, st_gen;
    int lspare;
    long long qspare[2];
} mac_stat;
int mos_lstat(const char *p, void *out) {
    struct stat s;
    if (lstat(p, &s) != 0) return -1;
    mac_stat *m = out;
    memset(m, 0, sizeof *m);
    m->st_dev = (unsigned)s.st_dev; m->st_mode = (unsigned short)s.st_mode; m->st_nlink = (unsigned short)s.st_nlink;
    m->st_ino = s.st_ino; m->st_uid = s.st_uid; m->st_gid = s.st_gid; m->st_size = s.st_size;
    return 0;
}
void *dispatch_get_global_queue(long p, unsigned long f) { (void)p; (void)f; return 0; }
void CFArrayRemoveValueAtIndex(id_ a, long i) {
    obj *o = a;
    for (long k = i; k + 1 < o->n; k++) o->v[k] = o->v[k + 1];
    o->n--;
}

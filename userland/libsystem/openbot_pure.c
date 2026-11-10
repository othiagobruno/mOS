/* Funções pequenas que o OpenBot 0.66.0 importa e o libSystem ainda não exportava.
 * O passeio de nftw usa opendir/readdir/stat deste libSystem. */
#ifdef GAP_HOST
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#define size_t unsigned long
#else
#include "sys.h"
#include "libc.h"
#include <stdarg.h>
long long llround(double x);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
void *opendir(const char *p);
struct gap_dirent { unsigned long ino, off; unsigned short reclen, namlen; unsigned char type; char name[1024]; };
struct gap_dirent *readdir(void *d);
int closedir(void *d);
#endif

#ifndef GAP_HOST
void dispatch_sync(void *q, void *block);
void dispatch_async_and_wait(void *q, void *block) { dispatch_sync(q, block); }
#endif

int pthread_condattr_destroy(void *a) {
    if (a) memset(a, 0, 16);
    return 0;
}
int pthread_condattr_setpshared(void *a, int pshared) {
    if (!a) return 22;
    ((int *)a)[1] = pshared;
    return 0;
}
int pthread_mutexattr_setpshared(void *a, int pshared) {
    if (!a) return 22;
    ((int *)a)[0] = pshared;
    return 0;
}

long long llroundl(long double x) { return llround((double)x); }

int vsnprintf_l(char *b, size_t n, void *loc, const char *fmt, va_list ap) {
    (void)loc;
    return vsnprintf(b, n, fmt, ap);
}

typedef struct { double r, i; } gap_c64;
typedef struct { float r, i; } gap_c32;
gap_c64 __muldc3(double a, double b, double c, double d) {
    gap_c64 z;
    z.r = a * c - b * d;
    z.i = a * d + b * c;
    return z;
}
gap_c32 __mulsc3(float a, float b, float c, float d) {
    gap_c32 z;
    z.r = a * c - b * d;
    z.i = a * d + b * c;
    return z;
}

#define GAP_FTW_F 0
#define GAP_FTW_D 1
#define GAP_FTW_DNR 2
#define GAP_FTW_NS 4
struct gap_ftw { int base; int level; };

#ifndef GAP_HOST
typedef struct {
    unsigned st_dev; unsigned short st_mode, st_nlink; unsigned long long st_ino;
    unsigned st_uid, st_gid, st_rdev;
    long long pad[32];
} gap_stat_t;
int stat(const char *path, gap_stat_t *m);
int snprintf(char *b, size_t n, const char *fmt, ...);
static int gap_walk(const char *path, int (*fn)(const char *, void *, int, struct gap_ftw *), int depth, int level) {
    gap_stat_t st;
    struct gap_ftw ftw;
    void *d;
    struct gap_dirent *e;
    int rc;
    memset(&st, 0, sizeof st);
    ftw.base = 0;
    ftw.level = level;
    if (stat(path, &st) != 0) return fn(path, &st, GAP_FTW_NS, &ftw);
    d = opendir(path);
    if (!d) return fn(path, &st, GAP_FTW_F, &ftw);
    rc = fn(path, &st, GAP_FTW_D, &ftw);
    if (rc != 0) { closedir(d); return rc; }
    if (depth == 0) { closedir(d); return 0; }
    while ((e = readdir(d))) {
        char child[1024];
        if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) continue;
        if (snprintf(child, sizeof child, "%s/%s", path, e->name) >= (int)sizeof child) continue;
        if (e->type == 4) rc = gap_walk(child, fn, depth - 1, level + 1);
        else {
            gap_stat_t cst;
            memset(&cst, 0, sizeof cst);
            stat(child, &cst);
            ftw.level = level + 1;
            rc = fn(child, &cst, GAP_FTW_F, &ftw);
        }
        if (rc != 0) { closedir(d); return rc; }
    }
    closedir(d);
    return 0;
}
int nftw(const char *path, int (*fn)(const char *, void *, int, struct gap_ftw *), int nopen, int flags) {
    (void)nopen;
    (void)flags;
    if (!path || !fn) return -1;
    return gap_walk(path, fn, 16, 0);
}
#endif

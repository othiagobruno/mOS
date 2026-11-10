/* VFS mínimo do SQLite. O formato do arquivo é o do sqlite3 comum. */
#ifdef CD_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
typedef FILE mos_stdio;
#else
typedef void mos_stdio;
extern void *malloc(size_t);
extern void *realloc(void *, size_t);
extern void free(void *);
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern size_t strlen(const char *);
extern char *strcpy(char *, const char *);
extern char *strcat(char *, const char *);
extern void *fopen(const char *, const char *);
extern int fclose(void *);
extern size_t fread(void *, size_t, size_t, void *);
extern size_t fwrite(const void *, size_t, size_t, void *);
extern int fseeko(void *, long, int);
extern long ftello(void *);
extern int fflush(void *);
extern int fileno(void *);
extern int ftruncate(int, long);
extern int unlink(const char *);
extern int access(const char *, int);
extern char *getcwd(char *, size_t);
extern long time(long *);
#endif

typedef struct mos_file {
    sqlite3_file base;
    mos_stdio *f;
    unsigned char *mem;
    sqlite3_int64 mem_n, mem_cap;
    int del;
    char *path;
} mos_file;

static int mos_close(sqlite3_file *pf) {
    mos_file *f = (mos_file *)pf;
    if (f->f) fclose(f->f);
    if (f->del && f->path) unlink(f->path);
    free(f->mem);
    free(f->path);
    f->f = 0;
    f->mem = 0;
    f->path = 0;
    return SQLITE_OK;
}
static int mos_read(sqlite3_file *pf, void *buf, int n, sqlite3_int64 off) {
    mos_file *f = (mos_file *)pf;
    sqlite3_int64 have;
    int got;
    if (n < 0 || off < 0) return SQLITE_IOERR_READ;
    memset(buf, 0, (size_t)n);
    if (!f->f) {
        if (off >= f->mem_n) return n ? SQLITE_IOERR_SHORT_READ : SQLITE_OK;
        have = f->mem_n - off;
        got = have < n ? (int)have : n;
        memcpy(buf, f->mem + off, (size_t)got);
        return got < n ? SQLITE_IOERR_SHORT_READ : SQLITE_OK;
    }
    if (fseeko(f->f, (long)off, 0)) return SQLITE_IOERR_READ;
    got = (int)fread(buf, 1, (size_t)n, f->f);
    return got < n ? SQLITE_IOERR_SHORT_READ : SQLITE_OK;
}
static int mos_grow(mos_file *f, sqlite3_int64 need) {
    unsigned char *g;
    sqlite3_int64 cap;
    if (need <= f->mem_cap) return SQLITE_OK;
    cap = f->mem_cap ? f->mem_cap : 4096;
    while (cap < need) cap *= 2;
    g = (unsigned char *)realloc(f->mem, (size_t)cap);
    if (!g) return SQLITE_IOERR_NOMEM;
    if (cap > f->mem_cap) memset(g + f->mem_cap, 0, (size_t)(cap - f->mem_cap));
    f->mem = g;
    f->mem_cap = cap;
    return SQLITE_OK;
}
static int mos_write(sqlite3_file *pf, const void *buf, int n, sqlite3_int64 off) {
    mos_file *f = (mos_file *)pf;
    if (n < 0 || off < 0) return SQLITE_IOERR_WRITE;
    if (!f->f) {
        if (mos_grow(f, off + n)) return SQLITE_IOERR_NOMEM;
        if (off > f->mem_n) memset(f->mem + f->mem_n, 0, (size_t)(off - f->mem_n));
        memcpy(f->mem + off, buf, (size_t)n);
        if (off + n > f->mem_n) f->mem_n = off + n;
        return SQLITE_OK;
    }
    if (fseeko(f->f, (long)off, 0)) return SQLITE_IOERR_WRITE;
    if ((int)fwrite(buf, 1, (size_t)n, f->f) != n) return SQLITE_IOERR_WRITE;
    return SQLITE_OK;
}
static int mos_trunc(sqlite3_file *pf, sqlite3_int64 sz) {
    mos_file *f = (mos_file *)pf;
    if (!f->f) {
        if (sz < 0) return SQLITE_IOERR_TRUNCATE;
        if (sz > f->mem_n) {
            if (mos_grow(f, sz)) return SQLITE_IOERR_NOMEM;
            memset(f->mem + f->mem_n, 0, (size_t)(sz - f->mem_n));
        }
        f->mem_n = sz;
        return SQLITE_OK;
    }
    if (fflush(f->f)) return SQLITE_IOERR_TRUNCATE;
    if (ftruncate(fileno(f->f), (long)sz)) return SQLITE_IOERR_TRUNCATE;
    return SQLITE_OK;
}
static int mos_sync(sqlite3_file *pf, int flags) {
    mos_file *f = (mos_file *)pf;
    (void)flags;
    if (f->f && fflush(f->f)) return SQLITE_IOERR_FSYNC;
    return SQLITE_OK;
}
static int mos_size(sqlite3_file *pf, sqlite3_int64 *out) {
    mos_file *f = (mos_file *)pf;
    long n;
    if (!f->f) { *out = f->mem_n; return SQLITE_OK; }
    if (fseeko(f->f, 0, 2)) return SQLITE_IOERR_FSTAT;
    n = ftello(f->f);
    if (n < 0) return SQLITE_IOERR_FSTAT;
    *out = n;
    return SQLITE_OK;
}
static int mos_lock(sqlite3_file *pf, int lv) { (void)pf; (void)lv; return SQLITE_OK; }
static int mos_unlock(sqlite3_file *pf, int lv) { (void)pf; (void)lv; return SQLITE_OK; }
static int mos_reserved(sqlite3_file *pf, int *out) { (void)pf; *out = 0; return SQLITE_OK; }
static int mos_cntl(sqlite3_file *pf, int op, void *arg) { (void)pf; (void)op; (void)arg; return SQLITE_NOTFOUND; }
static int mos_sector(sqlite3_file *pf) { (void)pf; return 512; }
static int mos_dev(sqlite3_file *pf) { (void)pf; return 0; }

static const sqlite3_io_methods mos_methods = {
    1, mos_close, mos_read, mos_write, mos_trunc, mos_sync, mos_size,
    mos_lock, mos_unlock, mos_reserved, mos_cntl, mos_sector, mos_dev
};

static int mos_open(sqlite3_vfs *vfs, const char *name, sqlite3_file *pf, int flags, int *out) {
    mos_file *f = (mos_file *)pf;
    (void)vfs;
    memset(f, 0, sizeof *f);
    f->base.pMethods = &mos_methods;
    if (out) *out = flags;
    f->del = (flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
    if (!name || !name[0] || (flags & SQLITE_OPEN_MEMORY)) return SQLITE_OK;
    f->path = (char *)malloc(strlen(name) + 1);
    if (!f->path) return SQLITE_NOMEM;
    strcpy(f->path, name);
    f->f = fopen(name, "r+b");
    if (!f->f && (flags & SQLITE_OPEN_CREATE)) f->f = fopen(name, "w+b");
    if (!f->f) { free(f->path); f->path = 0; return SQLITE_CANTOPEN; }
    return SQLITE_OK;
}
static int mos_delete(sqlite3_vfs *vfs, const char *name, int sync) {
    (void)vfs; (void)sync;
    if (name) unlink(name);
    return SQLITE_OK;
}
static int mos_access(sqlite3_vfs *vfs, const char *name, int flags, int *out) {
    (void)vfs; (void)flags;
    *out = (name && access(name, 0) == 0) ? 1 : 0;
    return SQLITE_OK;
}
static int mos_fullpath(sqlite3_vfs *vfs, const char *name, int nout, char *out) {
    (void)vfs;
    if (!name || nout <= 0) return SQLITE_CANTOPEN;
    if (name[0] == '/') {
        if ((int)strlen(name) >= nout) return SQLITE_CANTOPEN;
        strcpy(out, name);
        return SQLITE_OK;
    }
    if (!getcwd(out, (size_t)nout)) return SQLITE_CANTOPEN;
    if ((int)strlen(out) + (int)strlen(name) + 2 > nout) return SQLITE_CANTOPEN;
    strcat(out, "/");
    strcat(out, name);
    return SQLITE_OK;
}
static int mos_rand(sqlite3_vfs *vfs, int n, char *out) {
    mos_stdio *u;
    int i;
    (void)vfs;
    u = fopen("/dev/urandom", "rb");
    if (u) {
        i = (int)fread(out, 1, (size_t)n, u);
        fclose(u);
        if (i == n) return n;
    }
    for (i = 0; i < n; i++) out[i] = (char)(time(0) + i * 17);
    return n;
}
static int mos_sleep(sqlite3_vfs *vfs, int us) { (void)vfs; (void)us; return 0; }
static int mos_time(sqlite3_vfs *vfs, double *now) {
    (void)vfs;
    *now = (double)time(0) / 86400.0 + 2440587.5;
    return SQLITE_OK;
}
static int mos_err(sqlite3_vfs *vfs, int n, char *buf) { (void)vfs; (void)n; (void)buf; return 0; }

static sqlite3_vfs mos_vfs = {
    1, sizeof(mos_file), 512, 0, "mos", 0,
    mos_open, mos_delete, mos_access, mos_fullpath,
    0, 0, 0, 0,
    mos_rand, mos_sleep, mos_time, mos_err
};

int sqlite3_os_init(void) { return sqlite3_vfs_register(&mos_vfs, 1); }
int sqlite3_os_end(void) { return SQLITE_OK; }

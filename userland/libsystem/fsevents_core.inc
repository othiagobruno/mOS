/* Núcleo do FSEventStream. Incluído por fsevents.c (mOS) e por tools/fseventstest.c (FSE_HOST).
 * A Apple entrega pelo kernel; aqui um fio varre os caminhos com lstat/readdir e chama o
 * callback com o mesmo formato (char ** ou, no mOS com UseCFTypes, CFArray de CFString).
 * STUB rotulado no corpo: IgnoreSelf, MarkSelf, UseExtendedData, histórico em disco, UUID de volume.
 */
#if defined(FSE_HOST)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
#else
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void qsort(void *b, size_t n, size_t s, int (*cmp)(const void *, const void *));
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *getcwd(char *b, size_t n);
int usleep(unsigned us);
int lstat(const char *path, void *st);
void *opendir(const char *path);
void *readdir(void *d);
int closedir(void *d);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_join(unsigned long t, void **ret);
#endif

#define FSE_MAGIC 0x46534576u
#define FSE_PATH 512
#define FSE_MAX_ROOTS 32
#define FSE_MAX_EXCL 64
#define FSE_MAX_ENTS 16384
#define FSE_MAX_STREAMS 64
#define FSE_MAX_FAIL 16
#define FSE_DEV 0x1000002 /* st_dev único de to_mac_stat em libc.c */

#define kFSEventStreamEventIdSinceNow (~(u64)0)
#define kFSEventStreamCreateFlagUseCFTypes 0x00000001u
#define kFSEventStreamCreateFlagNoDefer 0x00000002u
#define kFSEventStreamCreateFlagWatchRoot 0x00000004u
#define kFSEventStreamCreateFlagIgnoreSelf 0x00000008u
#define kFSEventStreamCreateFlagFileEvents 0x00000010u
#define kFSEventStreamCreateFlagMarkSelf 0x00000020u
#define kFSEventStreamCreateFlagUseExtendedData 0x00000040u
#define kFSEventStreamEventFlagMustScanSubDirs 0x00000001u
#define kFSEventStreamEventFlagUserDropped 0x00000002u
#define kFSEventStreamEventFlagKernelDropped 0x00000004u
#define kFSEventStreamEventFlagHistoryDone 0x00000010u
#define kFSEventStreamEventFlagRootChanged 0x00000020u
#define kFSEventStreamEventFlagItemCreated 0x00000100u
#define kFSEventStreamEventFlagItemRemoved 0x00000200u
#define kFSEventStreamEventFlagItemInodeMetaMod 0x00000400u
#define kFSEventStreamEventFlagItemRenamed 0x00000800u
#define kFSEventStreamEventFlagItemModified 0x00001000u
#define kFSEventStreamEventFlagItemIsFile 0x00010000u
#define kFSEventStreamEventFlagItemIsDir 0x00020000u
#define kFSEventStreamEventFlagItemIsSymlink 0x00040000u
#define FSE_TYPE_MASK (kFSEventStreamEventFlagItemIsFile | kFSEventStreamEventFlagItemIsDir | kFSEventStreamEventFlagItemIsSymlink)
#define FSE_KEEP_MASK (kFSEventStreamEventFlagRootChanged | kFSEventStreamEventFlagHistoryDone | kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagKernelDropped)

#define FSE_FILE 1
#define FSE_DIR 2
#define FSE_LNK 3

typedef struct fse_st {
    u8 kind;
    u64 ino, dev;
    i64 mtime_s, mtime_ns, size;
    u32 mode;
} fse_st;

typedef void (*fse_cb_fn)(void *stream, void *info, size_t numEvents, void *eventPaths, const u32 *eventFlags, const u64 *eventIds);

static int fse_plat_lstat(const char *path, fse_st *out);
static int fse_plat_listdir(const char *path, int (*fn)(const char *name, void *ctx), void *ctx);
static void fse_plat_sleep_ms(unsigned ms);
static u64 fse_plat_now_ms(void);
static unsigned long long fse_plat_tid(void);
static int fse_plat_start_thread(void *(*fn)(void *));
static void fse_plat_join_thread(void);
static int fse_plat_getcwd(char *buf, unsigned cap);

#if defined(FSE_HOST)
static pthread_t g_fse_pth;
static int g_fse_pth_ok;
static int fse_plat_start_thread(void *(*fn)(void *)) {
    if (pthread_create(&g_fse_pth, 0, fn, 0) != 0) return -1;
    g_fse_pth_ok = 1;
    return 0;
}
static void fse_plat_join_thread(void) {
    if (!g_fse_pth_ok) return;
    g_fse_pth_ok = 0;
    pthread_join(g_fse_pth, 0);
}
static int fse_plat_lstat(const char *path, fse_st *out) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    memset(out, 0, sizeof *out);
    if (S_ISLNK(st.st_mode)) out->kind = FSE_LNK;
    else if (S_ISDIR(st.st_mode)) out->kind = FSE_DIR;
    else out->kind = FSE_FILE;
    out->ino = (u64)st.st_ino;
    out->dev = (u64)st.st_dev;
    out->mtime_s = (i64)st.st_mtim.tv_sec;
    out->mtime_ns = (i64)st.st_mtim.tv_nsec;
    out->size = (i64)st.st_size;
    out->mode = (u32)(st.st_mode & 07777);
    return 0;
}
static int fse_plat_listdir(const char *path, int (*fn)(const char *name, void *ctx), void *ctx) {
    DIR *d = opendir(path);
    struct dirent *e;
    if (!d) return -1;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' && (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0))) continue;
        if (fn(e->d_name, ctx)) break;
    }
    closedir(d);
    return 0;
}
static u64 fse_plat_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000ull + (u64)ts.tv_nsec / 1000000ull;
}
static void fse_plat_sleep_ms(unsigned ms) {
    struct timespec ts;
    if (!ms) return;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}
static unsigned long long fse_plat_tid(void) { return (unsigned long long)pthread_self(); }
#else
static unsigned long g_fse_pth;
static int g_fse_pth_ok;
static int fse_plat_start_thread(void *(*fn)(void *)) {
    if (pthread_create(&g_fse_pth, 0, fn, 0) != 0) return -1;
    g_fse_pth_ok = 1;
    return 0;
}
static void fse_plat_join_thread(void) {
    if (!g_fse_pth_ok) return;
    g_fse_pth_ok = 0;
    pthread_join(g_fse_pth, 0);
}
/* stat do macOS, o mesmo layout de mac_stat_t em libc.c (preenchido por lstat, não pelo kernel). */
typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} fse_mac_stat;
typedef struct {
    u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[1024];
} fse_mac_dirent;
static int fse_plat_lstat(const char *path, fse_st *out) {
    fse_mac_stat st;
    unsigned mode;
    if (lstat(path, &st) != 0) return -1;
    memset(out, 0, sizeof *out);
    mode = st.st_mode & 0170000u;
    if (mode == 0120000u) out->kind = FSE_LNK;
    else if (mode == 0040000u) out->kind = FSE_DIR;
    else out->kind = FSE_FILE;
    out->ino = st.st_ino;
    out->dev = st.st_dev;
    out->mtime_s = st.mtime;
    out->mtime_ns = st.mtime_ns; /* o to_mac_stat do libc deixa 0: o kernel só tem o segundo */
    out->size = st.st_size;
    out->mode = st.st_mode & 07777u;
    return 0;
}
static int fse_plat_listdir(const char *path, int (*fn)(const char *name, void *ctx), void *ctx) {
    void *d = opendir(path);
    fse_mac_dirent *e;
    if (!d) return -1;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' && (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0))) continue;
        if (fn(e->d_name, ctx)) break;
    }
    closedir(d);
    return 0;
}
static u64 fse_plat_now_ms(void) { return (u64)SYS0(SYS_TIME); }
static unsigned long long fse_plat_tid(void) { return (unsigned long long)SYS0(SYS_GETTID); }
static void fse_plat_sleep_ms(unsigned ms) { if (ms) usleep(ms * 1000u); }
#endif

static int fse_plat_getcwd(char *buf, unsigned cap) { return getcwd(buf, cap) != 0; }

static int fse_copy_path(char *dst, const char *src) {
    size_t n = strlen(src);
    if (n >= FSE_PATH) return -1;
    memcpy(dst, src, n + 1);
    return 0;
}
static void fse_norm(char *p) {
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') p[--n] = 0;
}
static void fse_store_path(char *dst, const char *src) {
    char tmp[FSE_PATH];
    if (!src || !src[0]) { dst[0] = 0; return; }
    if (src[0] == '/') {
        if (fse_copy_path(tmp, src) != 0) { dst[0] = 0; return; }
    } else {
        char cwd[FSE_PATH];
        size_t a, b;
        if (!fse_plat_getcwd(cwd, FSE_PATH)) { dst[0] = 0; return; }
        a = strlen(cwd);
        b = strlen(src);
        if (a + 1 + b >= FSE_PATH) { dst[0] = 0; return; }
        memcpy(tmp, cwd, a);
        tmp[a] = '/';
        memcpy(tmp + a + 1, src, b + 1);
    }
    fse_norm(tmp);
    memcpy(dst, tmp, strlen(tmp) + 1);
}

#if defined(FSE_HOST)
static int fse_collect_paths(void *paths, char out[][FSE_PATH], int max) {
    const char **p = paths;
    int i = 0, w = 0;
    if (!p) return 0;
    for (; p[i] && w < max; i++) {
        fse_store_path(out[w], p[i]);
        if (out[w][0]) w++;
    }
    return w;
}
static void *fse_wrap_paths(int use_cf, char **paths, int n, void **cookie) {
    (void)use_cf; (void)n;
    *cookie = 0;
    return paths;
}
static void fse_unwrap_paths(void *cookie) { (void)cookie; }
#else
typedef void *fse_id;
long CFArrayGetCount(fse_id a);
fse_id CFArrayGetValueAtIndex(fse_id a, long i);
fse_id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(fse_id a, fse_id v);
fse_id CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
void CFRelease(fse_id o);
const char *cx_cstr(fse_id o);
static int fse_collect_paths(void *paths, char out[][FSE_PATH], int max) {
    long n, i;
    int w = 0;
    if (!paths) return 0;
    n = CFArrayGetCount((fse_id)paths);
    for (i = 0; i < n && w < max; i++) {
        const char *s = cx_cstr(CFArrayGetValueAtIndex((fse_id)paths, i));
        if (!s || !s[0] || !strcmp(s, "(null)") || !strcmp(s, "?")) continue;
        fse_store_path(out[w], s);
        if (out[w][0]) w++;
    }
    return w;
}
static void *fse_wrap_paths(int use_cf, char **paths, int n, void **cookie) {
    fse_id arr;
    int i;
    *cookie = 0;
    if (!use_cf) return paths;
    arr = CFArrayCreateMutable(0, 0, 0);
    if (!arr) return 0;
    for (i = 0; i < n; i++) {
        fse_id str = CFStringCreateWithCString(0, paths[i], 0x08000100u);
        if (!str) continue;
        CFArrayAppendValue(arr, str);
        CFRelease(str);
    }
    *cookie = arr;
    return arr;
}
static void fse_unwrap_paths(void *cookie) { if (cookie) CFRelease((fse_id)cookie); }
#endif

typedef struct fse_ent {
    u64 ino, dev;
    i64 mtime_s, mtime_ns, size;
    u32 mode;
    u8 kind;
    char path[FSE_PATH];
} fse_ent;
typedef struct fse_set { fse_ent *e; int n, cap; } fse_set;
typedef struct fse_ev {
    u64 id, ino, dev;
    u32 flags;
    u8 kind;
    char path[FSE_PATH];
} fse_ev;
typedef struct fse_cfg {
    char roots[FSE_MAX_ROOTS][FSE_PATH];
    char excl[FSE_MAX_EXCL][FSE_PATH];
    int nroots, nexcl;
    int file_events, watch_root, need_history, primed, use_cf;
    unsigned latency_ms;
} fse_cfg;
typedef struct fse_stream {
    u32 magic;
    int refs;
    int started, primed, invalid, zombie, in_scan;
    int nodefer, watch_root, file_events, use_cf, need_history;
    int force_flush, dropped;
    unsigned latency_ms;
    unsigned long long scan_tid;
    u64 last_ms, latest;
    fse_cb_fn cb;
    void *info;
    void (*ctx_release)(const void *);
    void *rl, *mode, *queue; /* gravados; a entrega não espera o run loop (ver docs/fsevents.md) */
    int nroots, nexcl;
    char roots[FSE_MAX_ROOTS][FSE_PATH];
    char excl[FSE_MAX_EXCL][FSE_PATH];
    fse_set base;
    /* STUB: IgnoreSelf e MarkSelf ficam aqui e não filtram. O vnode não tem pid de quem escreveu. */
    int ignore_self, mark_self;
    volatile int lock;
} fse_stream;

static fse_stream *g_live[FSE_MAX_STREAMS];
static int g_nlive;
static volatile int g_live_lock;
static volatile int g_thr;
static volatile int g_poll_stop;
static u64 g_eid;

static void fse_slock(volatile int *l) {
    while (__sync_lock_test_and_set(l, 1)) { while (*l) {} }
}
static void fse_sunlock(volatile int *l) { __sync_lock_release(l); }
static void fse_live_lock(void) { fse_slock(&g_live_lock); }
static void fse_live_unlock(void) { fse_sunlock(&g_live_lock); }
static u64 fse_next_id(void) { return __sync_add_and_fetch(&g_eid, 1); }

static fse_stream *fse_of(void *p) {
    fse_stream *s = p;
    if (!s || s->magic != FSE_MAGIC) return 0;
    return s;
}
static void fse_free_set(fse_set *t) { if (!t) return; free(t->e); t->e = 0; t->n = t->cap = 0; }
static void fse_free(fse_stream *s) {
    void (*rel)(const void *);
    void *info;
    if (!s) return;
    rel = s->ctx_release;
    info = s->info;
    s->magic = 0;
    s->ctx_release = 0;
    fse_free_set(&s->base);
    free(s);
    if (rel) rel(info);
}
static void fse_unref(fse_stream *s) {
    int die;
    fse_slock(&s->lock);
    die = --s->refs == 0;
    fse_sunlock(&s->lock);
    if (die) fse_free(s);
}
static int fse_live_add(fse_stream *s) {
    int ok = 0;
    fse_live_lock();
    if (g_nlive < FSE_MAX_STREAMS) { g_live[g_nlive++] = s; ok = 1; }
    fse_live_unlock();
    return ok;
}
static void fse_live_del(fse_stream *s) {
    int i;
    fse_live_lock();
    for (i = 0; i < g_nlive; i++) if (g_live[i] == s) {
        g_live[i] = g_live[--g_nlive];
        break;
    }
    fse_live_unlock();
}

static int fse_is_root(const fse_cfg *c, const char *path) {
    int i;
    for (i = 0; i < c->nroots; i++) if (!strcmp(c->roots[i], path)) return 1;
    return 0;
}
static int fse_under(const char *root, const char *path) {
    size_t n;
    if (!root || !root[0] || !path) return 0;
    if (root[0] == '/' && root[1] == 0) return path[0] == '/';
    n = strlen(root);
    if (!strncmp(path, root, n) && (path[n] == 0 || path[n] == '/')) return 1;
    return 0;
}
static int fse_excluded(const fse_cfg *c, const char *path) {
    int i;
    for (i = 0; i < c->nexcl; i++) if (fse_under(c->excl[i], path)) return 1;
    return 0;
}
static u32 fse_kind_flags(u8 kind) {
    if (kind == FSE_DIR) return kFSEventStreamEventFlagItemIsDir;
    if (kind == FSE_LNK) return kFSEventStreamEventFlagItemIsSymlink;
    return kFSEventStreamEventFlagItemIsFile;
}
static int fse_due(const fse_stream *s) {
    u64 now;
    if (!s->primed || s->latency_ms == 0 || s->nodefer) return 1;
    now = fse_plat_now_ms();
    return now - s->last_ms >= s->latency_ms;
}

static int fse_set_push(fse_set *set, const char *path, const fse_st *st) {
    fse_ent *e;
    int nc;
    if (set->n >= FSE_MAX_ENTS) return -1;
    if (set->n == set->cap) {
        nc = set->cap ? set->cap * 2 : 32;
        if (nc > FSE_MAX_ENTS) nc = FSE_MAX_ENTS;
        if (nc <= set->n) return -1;
        e = realloc(set->e, (size_t)nc * sizeof *e);
        if (!e) return -1;
        set->e = e;
        set->cap = nc;
    }
    e = &set->e[set->n];
    memset(e, 0, sizeof *e);
    if (fse_copy_path(e->path, path) != 0) return -1;
    e->ino = st->ino;
    e->dev = st->dev;
    e->mtime_s = st->mtime_s;
    e->mtime_ns = st->mtime_ns;
    e->size = st->size;
    e->mode = st->mode;
    e->kind = st->kind;
    set->n++;
    return 0;
}

typedef struct fse_walk {
    fse_set *set;
    const fse_cfg *cfg;
    char fails[FSE_MAX_FAIL][FSE_PATH];
    int nfails, hard, depth;
    char dir[FSE_PATH];
} fse_walk;

static int fse_join_ok(char *dst, const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    int need = !(a > 0 && dir[a - 1] == '/');
    if (a + (size_t)need + b >= FSE_PATH) return -1;
    memcpy(dst, dir, a);
    if (need) dst[a++] = '/';
    memcpy(dst + a, name, b + 1);
    return 0;
}
static int fse_add_tree(fse_walk *w, const char *path, int depth);
static int fse_on_name(const char *name, void *ctx) {
    fse_walk *w = ctx;
    char child[FSE_PATH];
    if (w->hard) return 1;
    if (!name || !name[0]) return 0;
    if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) return 0;
    if (fse_join_ok(child, w->dir, name) != 0) { w->hard = 1; return 1; }
    return fse_add_tree(w, child, w->depth + 1) < 0 ? 1 : 0;
}

static int fse_add_tree(fse_walk *w, const char *path, int depth) {
    fse_st st;
    if (w->hard) return -1;
    if (fse_excluded(w->cfg, path)) return 0;
    if (strlen(path) >= FSE_PATH) { w->hard = 1; return -1; }
    if (fse_plat_lstat(path, &st) != 0) return 0;
    if (fse_set_push(w->set, path, &st) != 0) { w->hard = 1; return -1; }
    if (st.kind != FSE_DIR) return 0;
    if (depth >= 32) {
        if (w->nfails < FSE_MAX_FAIL) fse_copy_path(w->fails[w->nfails++], path);
        return 0;
    }
    {
        char saved[FSE_PATH];
        int saved_depth = w->depth;
        memcpy(saved, w->dir, FSE_PATH);
        if (fse_copy_path(w->dir, path) != 0) { w->hard = 1; return -1; }
        w->depth = depth;
        if (fse_plat_listdir(path, fse_on_name, w) != 0) {
            if (w->nfails < FSE_MAX_FAIL) fse_copy_path(w->fails[w->nfails++], path);
            else w->hard = 1;
        }
        memcpy(w->dir, saved, FSE_PATH);
        w->depth = saved_depth;
    }
    return w->hard ? -1 : 0;
}

static int fse_ent_cmp(const void *a, const void *b) {
    return strcmp(((const fse_ent *)a)->path, ((const fse_ent *)b)->path);
}
static void fse_sort_uniq(fse_set *t) {
    int i, w;
    if (t->n <= 0) return;
    qsort(t->e, (size_t)t->n, sizeof t->e[0], fse_ent_cmp);
    w = 1;
    for (i = 1; i < t->n; i++) {
        if (!strcmp(t->e[i].path, t->e[w - 1].path)) continue;
        if (w != i) t->e[w] = t->e[i];
        w++;
    }
    t->n = w;
}

static int fse_ev_add(fse_ev **ev, int *n, int *cap, const char *path, u32 flags, u8 kind, u64 ino, u64 dev) {
    fse_ev *e;
    int nc;
    if (*n >= 8192) return -1;
    if (*n == *cap) {
        nc = *cap ? *cap * 2 : 16;
        if (nc > 8192) nc = 8192;
        if (nc <= *n) return -1;
        e = realloc(*ev, (size_t)nc * sizeof *e);
        if (!e) return -1;
        *ev = e;
        *cap = nc;
    }
    e = &(*ev)[*n];
    memset(e, 0, sizeof *e);
    if (fse_copy_path(e->path, path) != 0) return -1;
    e->flags = flags;
    e->kind = kind;
    e->ino = ino;
    e->dev = dev;
    e->id = fse_next_id();
    (*n)++;
    return 0;
}
static int fse_under_fail(const fse_walk *w, const char *path) {
    int i;
    for (i = 0; i < w->nfails; i++) if (fse_under(w->fails[i], path) && strcmp(w->fails[i], path) != 0) return 1;
    return 0;
}
static void fse_pair_renames(fse_ev *ev, int n) {
    int i, j;
    for (i = 0; i < n; i++) {
        if (!(ev[i].flags & kFSEventStreamEventFlagItemRemoved) || !ev[i].ino) continue;
        for (j = 0; j < n; j++) {
            if (!(ev[j].flags & kFSEventStreamEventFlagItemCreated)) continue;
            if (ev[i].ino != ev[j].ino || ev[i].dev != ev[j].dev || ev[i].kind != ev[j].kind) continue;
            ev[i].flags = (ev[i].flags & (FSE_TYPE_MASK | FSE_KEEP_MASK)) | kFSEventStreamEventFlagItemRenamed;
            ev[j].flags = (ev[j].flags & (FSE_TYPE_MASK | FSE_KEEP_MASK)) | kFSEventStreamEventFlagItemRenamed;
            break;
        }
    }
}
static void fse_parent(const char *path, char *out) {
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] != '/') n--;
    if (n > 1 && path[n - 1] == '/') n--;
    if (n >= FSE_PATH) n = FSE_PATH - 1;
    if (n == 0) { out[0] = '/'; out[1] = 0; return; }
    memcpy(out, path, n);
    out[n] = 0;
}
static void fse_legacy(fse_ev **ev, int *nev, const fse_cfg *cfg) {
    fse_ev *src = *ev, *dst = 0;
    int n = *nev, nd = 0, cap = 0, i, k;
    for (i = 0; i < n; i++) {
        char path[FSE_PATH];
        u32 keep = src[i].flags & FSE_KEEP_MASK;
        u32 f;
        u8 kind;
        int as_file = 0;
        path[0] = 0;
        if (fse_is_root(cfg, src[i].path) && src[i].kind == FSE_FILE) {
            if (fse_copy_path(path, src[i].path) != 0) continue;
            as_file = 1;
            kind = FSE_FILE;
        } else if (src[i].kind == FSE_DIR) {
            if (fse_copy_path(path, src[i].path) != 0) continue;
            kind = FSE_DIR;
        } else {
            fse_parent(src[i].path, path);
            if (!cfg->nroots || !fse_under(cfg->roots[0], path)) {
                int r, best = 0;
                size_t bl = 0;
                for (r = 0; r < cfg->nroots; r++) {
                    size_t rn = strlen(cfg->roots[r]);
                    if (fse_under(cfg->roots[r], src[i].path) && rn >= bl) { best = r; bl = rn; }
                }
                if (cfg->nroots) fse_copy_path(path, cfg->roots[best]);
            }
            kind = FSE_DIR;
        }
        f = kFSEventStreamEventFlagMustScanSubDirs | keep | (as_file ? fse_kind_flags(kind) : kFSEventStreamEventFlagItemIsDir);
        for (k = 0; k < nd; k++) if (!strcmp(dst[k].path, path)) { dst[k].flags |= f; break; }
        if (k == nd) fse_ev_add(&dst, &nd, &cap, path, f, kind, 0, 0);
    }
    free(src);
    *ev = dst;
    *nev = nd;
}

static void fse_diff(const fse_set *old, const fse_set *neu, const fse_cfg *cfg, const fse_walk *w, fse_ev **ev, int *nev) {
    int i = 0, j = 0, cap = 0, failed = 0;
    *ev = 0;
    *nev = 0;
    while (i < old->n || j < neu->n) {
        int c;
        if (i >= old->n) c = 1;
        else if (j >= neu->n) c = -1;
        else c = strcmp(old->e[i].path, neu->e[j].path);
        if (c == 0) {
            const fse_ent *a = &old->e[i], *b = &neu->e[j];
            int meta = a->mode != b->mode || a->kind != b->kind;
            int data = a->size != b->size || a->mtime_s != b->mtime_s || a->mtime_ns != b->mtime_ns || a->ino != b->ino;
            if (meta || data) {
                u32 f = fse_kind_flags(b->kind);
                if (a->kind != b->kind) f |= kFSEventStreamEventFlagItemCreated;
                else {
                    if (a->size != b->size || a->mtime_s != b->mtime_s || a->mtime_ns != b->mtime_ns)
                        f |= kFSEventStreamEventFlagItemModified;
                    if (a->mode != b->mode || a->ino != b->ino) f |= kFSEventStreamEventFlagItemInodeMetaMod;
                }
                if (cfg->watch_root && fse_is_root(cfg, b->path) && a->ino != b->ino)
                    f |= kFSEventStreamEventFlagRootChanged;
                if (fse_ev_add(ev, nev, &cap, b->path, f, b->kind, b->ino, b->dev) != 0) failed = 1;
            }
            i++;
            j++;
        } else if (c < 0) {
            if (!fse_under_fail(w, old->e[i].path)) {
                u32 f = kFSEventStreamEventFlagItemRemoved | fse_kind_flags(old->e[i].kind);
                if (cfg->watch_root && fse_is_root(cfg, old->e[i].path)) f |= kFSEventStreamEventFlagRootChanged;
                if (fse_ev_add(ev, nev, &cap, old->e[i].path, f, old->e[i].kind, old->e[i].ino, old->e[i].dev) != 0)
                    failed = 1;
            }
            i++;
        } else {
            u32 f = kFSEventStreamEventFlagItemCreated | fse_kind_flags(neu->e[j].kind);
            if (fse_ev_add(ev, nev, &cap, neu->e[j].path, f, neu->e[j].kind, neu->e[j].ino, neu->e[j].dev) != 0)
                failed = 1;
            j++;
        }
    }
    if (*ev && *nev) fse_pair_renames(*ev, *nev);
    if (!cfg->file_events && *nev) fse_legacy(ev, nev, cfg);
    if (failed || w->hard) {
        const char *p = cfg->nroots ? cfg->roots[0] : "/";
        fse_ev_add(ev, nev, &cap, p, kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagItemIsDir, FSE_DIR, 0, 0);
    } else if (w->nfails) {
        for (i = 0; i < w->nfails; i++)
            fse_ev_add(ev, nev, &cap, w->fails[i], kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagItemIsDir, FSE_DIR, 0, 0);
    }
}

static int fse_history(const fse_cfg *cfg, const fse_set *neu, fse_ev **ev, int *nev) {
    int i, cap = 0;
    *ev = 0;
    *nev = 0;
    for (i = 0; i < cfg->nroots; i++) {
        u8 kind = FSE_DIR;
        int k;
        for (k = 0; k < neu->n; k++) if (!strcmp(neu->e[k].path, cfg->roots[i])) { kind = neu->e[k].kind; break; }
        if (fse_ev_add(ev, nev, &cap, cfg->roots[i],
                       kFSEventStreamEventFlagHistoryDone | kFSEventStreamEventFlagMustScanSubDirs | fse_kind_flags(kind),
                       kind, 0, 0) != 0)
            return -1;
    }
    return 0;
}

static void fse_call(fse_stream *s, fse_ev *ev, int n) {
    char **paths;
    u32 *flags;
    u64 *ids;
    void *cookie = 0, *packed;
    int i;
    if (!s->cb || n <= 0) return;
    paths = malloc((size_t)n * sizeof *paths);
    flags = malloc((size_t)n * sizeof *flags);
    ids = malloc((size_t)n * sizeof *ids);
    if (!paths || !flags || !ids) { free(paths); free(flags); free(ids); return; }
    for (i = 0; i < n; i++) { paths[i] = ev[i].path; flags[i] = ev[i].flags; ids[i] = ev[i].id; }
    packed = fse_wrap_paths(s->use_cf, paths, n, &cookie);
    if (packed) s->cb(s, s->info, (size_t)n, packed, flags, ids);
    fse_unwrap_paths(cookie);
    free(paths);
    free(flags);
    free(ids);
}

static int fse_begin(fse_stream *s, int force, fse_cfg *cfg) {
    unsigned long long me = fse_plat_tid();
    for (;;) {
        fse_slock(&s->lock);
        if (s->zombie || !s->started) { fse_sunlock(&s->lock); return 0; }
        if (s->in_scan) {
            int mine = s->scan_tid == me;
            fse_sunlock(&s->lock);
            if (mine) return 0;
            fse_plat_sleep_ms(1);
            continue;
        }
        if (s->primed && !force && !s->force_flush && !fse_due(s)) { fse_sunlock(&s->lock); return 0; }
        s->in_scan = 1;
        s->scan_tid = me;
        s->refs++;
        s->force_flush = 0;
        cfg->nroots = s->nroots;
        cfg->nexcl = s->nexcl;
        cfg->file_events = s->file_events;
        cfg->watch_root = s->watch_root;
        cfg->need_history = s->need_history;
        cfg->primed = s->primed;
        cfg->use_cf = s->use_cf;
        cfg->latency_ms = s->latency_ms;
        if (s->nroots) memcpy(cfg->roots, s->roots, (size_t)s->nroots * FSE_PATH);
        if (s->nexcl) memcpy(cfg->excl, s->excl, (size_t)s->nexcl * FSE_PATH);
        fse_sunlock(&s->lock);
        return 1;
    }
}

static void fse_scan(fse_stream *s, int force) {
    fse_cfg *cfg;
    fse_walk walk;
    fse_set neu;
    fse_ev *ev = 0;
    int nev = 0, deliver = 0, i;
    cfg = calloc(1, sizeof *cfg);
    if (!cfg) return;
    if (!fse_begin(s, force, cfg)) { free(cfg); return; }
    memset(&neu, 0, sizeof neu);
    memset(&walk, 0, sizeof walk);
    walk.set = &neu;
    walk.cfg = cfg;
    for (i = 0; i < cfg->nroots && !walk.hard; i++) fse_add_tree(&walk, cfg->roots[i], 0);
    fse_sort_uniq(&neu);
    if (walk.hard) {
        int cap = 0;
        const char *p = cfg->nroots ? cfg->roots[0] : "/";
        fse_ev_add(&ev, &nev, &cap, p, kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagItemIsDir, FSE_DIR, 0, 0);
        fse_free_set(&neu);
        fse_slock(&s->lock);
        /* baseline fica a última completa: a próxima varredura que couber é o prime, ou o diff contra ela */
        s->last_ms = fse_plat_now_ms();
        if (nev) s->latest = ev[nev - 1].id;
        deliver = !s->zombie && s->started && nev && s->cb && !s->dropped;
        s->dropped = 1;
        s->in_scan = 0;
        fse_sunlock(&s->lock);
    } else if (!cfg->primed) {
        if (cfg->need_history) fse_history(cfg, &neu, &ev, &nev);
        fse_slock(&s->lock);
        fse_free_set(&s->base);
        s->base = neu;
        neu.e = 0;
        s->primed = 1;
        s->need_history = 0;
        s->dropped = 0;
        s->last_ms = fse_plat_now_ms();
        if (nev) s->latest = ev[nev - 1].id;
        deliver = !s->zombie && s->started && nev && s->cb;
        s->in_scan = 0;
        fse_sunlock(&s->lock);
    } else {
        int stopped;
        fse_diff(&s->base, &neu, cfg, &walk, &ev, &nev);
        fse_slock(&s->lock);
        stopped = !s->started || s->zombie;
        if (!stopped) {
            fse_free_set(&s->base);
            s->base = neu;
            neu.e = 0;
            s->dropped = 0;
            s->last_ms = fse_plat_now_ms();
            if (nev) s->latest = ev[nev - 1].id;
        } else fse_free_set(&neu);
        deliver = !stopped && nev && s->cb;
        s->in_scan = 0;
        fse_sunlock(&s->lock);
        if (!deliver) { free(ev); ev = 0; nev = 0; }
    }
    if (deliver) fse_call(s, ev, nev);
    free(ev);
    fse_free_set(&neu);
    free(cfg);
    fse_unref(s);
}

static unsigned fse_poll_wait(void) {
    unsigned wait = 300;
    int any = 0, i;
    fse_live_lock();
    for (i = 0; i < g_nlive; i++) {
        fse_stream *s = g_live[i];
        unsigned w;
        fse_slock(&s->lock);
        if (s->started && !s->zombie) {
            any = 1;
            if (s->nodefer || s->force_flush) w = 15; /* NoDefer: a Apple não espera a latência */
            else w = s->latency_ms ? s->latency_ms : 20;
            if (w < 15) w = 15;
            if (w < wait) wait = w;
        }
        fse_sunlock(&s->lock);
    }
    fse_live_unlock();
    return any ? wait : 300;
}
static int fse_want_flush(void);
static void *fse_poll(void *arg) {
    (void)arg;
    while (!g_poll_stop) {
        fse_stream *local[FSE_MAX_STREAMS];
        int nloc = 0, i;
        unsigned left;
        fse_live_lock();
        for (i = 0; i < g_nlive; i++) {
            fse_stream *s = g_live[i];
            fse_slock(&s->lock);
            if (!s->zombie && s->started) { s->refs++; local[nloc++] = s; }
            fse_sunlock(&s->lock);
        }
        fse_live_unlock();
        for (i = 0; i < nloc; i++) fse_scan(local[i], 0);
        for (i = 0; i < nloc; i++) fse_unref(local[i]);
        left = fse_poll_wait();
        while (left && !g_poll_stop) {
            unsigned sl = left > 20 ? 20 : left;
            if (fse_want_flush()) break;
            fse_plat_sleep_ms(sl);
            left -= sl;
        }
    }
    return 0;
}
static int fse_want_flush(void) {
    int i, yes = 0;
    fse_live_lock();
    for (i = 0; i < g_nlive; i++) {
        fse_slock(&g_live[i]->lock);
        if (g_live[i]->force_flush && g_live[i]->started && !g_live[i]->zombie) yes = 1;
        fse_sunlock(&g_live[i]->lock);
        if (yes) break;
    }
    fse_live_unlock();
    return yes;
}
static void fse_ensure_thread(void) {
    if (__sync_bool_compare_and_swap(&g_thr, 0, 1)) {
        if (fse_plat_start_thread(fse_poll) != 0) g_thr = 0;
    }
}

typedef struct {
    long version;
    void *info;
    const void *(*retain)(const void *);
    void (*release)(const void *);
    const void *(*copyDescription)(const void *);
} fse_ctx;

void *FSEventStreamCreate(void *alloc, void *cb, void *ctx, void *paths, u64 since, double latency, u32 flags) {
    fse_stream *s;
    fse_ctx *c = ctx;
    double ms;
    (void)alloc;
    s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->magic = FSE_MAGIC;
    s->refs = 1;
    s->cb = (fse_cb_fn)cb;
    s->need_history = since != kFSEventStreamEventIdSinceNow;
    s->use_cf = (flags & kFSEventStreamCreateFlagUseCFTypes) != 0;
    s->nodefer = (flags & kFSEventStreamCreateFlagNoDefer) != 0;
    s->watch_root = (flags & kFSEventStreamCreateFlagWatchRoot) != 0;
    s->file_events = (flags & kFSEventStreamCreateFlagFileEvents) != 0;
    s->ignore_self = (flags & kFSEventStreamCreateFlagIgnoreSelf) != 0; /* STUB: sem efeito */
    s->mark_self = (flags & kFSEventStreamCreateFlagMarkSelf) != 0;     /* STUB: sem efeito */
    (void)(flags & kFSEventStreamCreateFlagUseExtendedData);            /* STUB: sem dicionário extra */
    ms = latency * 1000.0;
    if (ms < 0) ms = 0;
    if (ms > 600000.0) ms = 600000.0;
    s->latency_ms = (unsigned)ms;
    s->nroots = fse_collect_paths(paths, s->roots, FSE_MAX_ROOTS);
    if (c) {
        s->info = c->info;
        if (c->retain) s->info = (void *)c->retain(c->info);
        s->ctx_release = c->release;
    }
    if (!fse_live_add(s)) { fse_free(s); return 0; }
    return s;
}
unsigned char FSEventStreamStart(void *p) {
    fse_stream *s = fse_of(p);
    if (!s) return 0;
    fse_slock(&s->lock);
    if (s->invalid || s->zombie || s->started) { fse_sunlock(&s->lock); return 0; }
    s->started = 1;
    fse_sunlock(&s->lock);
    fse_ensure_thread();
    fse_scan(s, 1);
    return 1;
}
void FSEventStreamStop(void *p) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    fse_slock(&s->lock);
    s->started = 0;
    fse_sunlock(&s->lock);
}
void FSEventStreamInvalidate(void *p) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    fse_slock(&s->lock);
    s->invalid = 1;
    s->started = 0;
    fse_sunlock(&s->lock);
}
void FSEventStreamRelease(void *p) {
    fse_stream *s = fse_of(p);
    int die;
    if (!s) return;
    fse_live_del(s);
    fse_slock(&s->lock);
    s->zombie = 1;
    s->started = 0;
    die = --s->refs == 0;
    fse_sunlock(&s->lock);
    if (die) fse_free(s);
}
void FSEventStreamScheduleWithRunLoop(void *p, void *rl, void *mode) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    fse_slock(&s->lock);
    s->rl = rl;
    s->mode = mode;
    fse_sunlock(&s->lock);
}
void FSEventStreamUnscheduleFromRunLoop(void *p, void *rl, void *mode) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    (void)rl; (void)mode;
    fse_slock(&s->lock);
    s->rl = 0;
    s->mode = 0;
    fse_sunlock(&s->lock);
}
void FSEventStreamSetDispatchQueue(void *p, void *q) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    fse_slock(&s->lock);
    s->queue = q;
    fse_sunlock(&s->lock);
}
void FSEventStreamFlushSync(void *p) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    fse_scan(s, 1);
}
void FSEventStreamFlushAsync(void *p) {
    fse_stream *s = fse_of(p);
    if (!s) return;
    fse_slock(&s->lock);
    s->force_flush = 1;
    fse_sunlock(&s->lock);
}
int FSEventStreamGetDeviceBeingWatched(void *p) { return fse_of(p) ? FSE_DEV : -1; }
u64 FSEventStreamGetLatestEventId(void *p) {
    fse_stream *s = fse_of(p);
    u64 id = 0;
    if (!s) return 0;
    fse_slock(&s->lock);
    id = s->latest;
    fse_sunlock(&s->lock);
    return id;
}
u64 FSEventsGetCurrentEventId(void) { return g_eid; }
unsigned char FSEventsPurgeEventsForDeviceUpToEventId(int dev, u64 id) {
    (void)dev; (void)id;
    return 1; /* STUB: não há journal em disco para apagar */
}
unsigned char FSEventStreamSetExclusionPaths(void *p, void *paths) {
    fse_stream *s = fse_of(p);
    char (*tmp)[FSE_PATH];
    int n, i, w;
    if (!s) return 0;
    tmp = malloc(FSE_MAX_EXCL * FSE_PATH);
    if (!tmp) return 0;
    n = fse_collect_paths(paths, tmp, FSE_MAX_EXCL);
    for (;;) {
        fse_slock(&s->lock);
        if (!s->in_scan) break;
        fse_sunlock(&s->lock);
        fse_plat_sleep_ms(1);
    }
    s->nexcl = n;
    if (n) memcpy(s->excl, tmp, (size_t)n * FSE_PATH);
    if (s->base.e) {
        w = 0;
        for (i = 0; i < s->base.n; i++) {
            int ex = 0, k;
            for (k = 0; k < n; k++) if (fse_under(s->excl[k], s->base.e[i].path)) { ex = 1; break; }
            if (ex) continue;
            if (w != i) s->base.e[w] = s->base.e[i];
            w++;
        }
        s->base.n = w;
    }
    fse_sunlock(&s->lock);
    free(tmp);
    return 1;
}

void cx_fse_shutdown(void) {
    g_poll_stop = 1;
    if (g_thr) fse_plat_join_thread();
    g_thr = 0;
    g_poll_stop = 0;
}

#ifndef FSE_HOST
static int fse_roots_copy(void *p, char out[][FSE_PATH], int max) {
    fse_stream *s = fse_of(p);
    int n;
    if (!s || max <= 0) return 0;
    fse_slock(&s->lock);
    n = s->nroots;
    if (n > max) n = max;
    if (n) memcpy(out, s->roots, (size_t)n * FSE_PATH);
    fse_sunlock(&s->lock);
    return n;
}
void *FSEventStreamCopyPathsBeingWatched(void *p) {
    char (*tmp)[FSE_PATH];
    fse_id arr;
    int n, i;
    tmp = malloc(FSE_MAX_ROOTS * FSE_PATH);
    if (!tmp) return 0;
    n = fse_roots_copy(p, tmp, FSE_MAX_ROOTS);
    arr = CFArrayCreateMutable(0, 0, 0);
    for (i = 0; i < n && arr; i++) {
        fse_id str = CFStringCreateWithCString(0, tmp[i], 0x08000100u);
        if (!str) continue;
        CFArrayAppendValue(arr, str);
        CFRelease(str);
    }
    free(tmp);
    return arr;
}
void *FSEventStreamCopyDescription(void *p) {
    (void)p;
    return CFStringCreateWithCString(0, "FSEventStream (mOS: varredura de stat/readdir)", 0x08000100u);
}
#endif

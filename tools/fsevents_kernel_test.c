/*
 * mOS — teste de integração do FSEvents no modo journal: o fsevents_core.inc do mOS (o ramo que roda no sistema)
 * sobre o VFS e o journal reais do kernel (kernel/lib/vfs.c, vfsmeta.c, vfsperm.c, vfsevents.c), compilados à parte.
 *   gcc -O1 -g -fsanitize=address,undefined -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools -c \
 *       kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/proc/cred.c
 *   gcc -O1 -g -fsanitize=address,undefined -Ikernel/include -Iuserland/libsystem -c tools/fsevents_kernel_test.c
 *   gcc -fsanitize=address,undefined *.o -o fsek -pthread && ./fsek          (tools/fsevents_kernel_test.sh)
 */
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int32_t i32; typedef int64_t i64;
#include "syscall.h"
int printf(const char *f, ...);

/* lado "kernel" (outras unidades) */
typedef struct vnode vnode_t;
vnode_t *vfs_write_file(const char *path, const void *data, size_t n);
vnode_t *vfs_mkdir(const char *path);
vnode_t *vfs_lookup(const char *path);
int vfs_delete(vnode_t *n);
int vfs_move(vnode_t *n, vnode_t *np, const char *newname);
int vfs_append(vnode_t *f, const void *data, size_t n);
int vfs_xattr_set(vnode_t *n, const char *name, const void *val, u64 len, u64 pos, int opts);
int vfs_ev_read(u64 since, void *out, int max, u64 *latest);
void vfs_init(void);
static u64 fake_ms = 1000;
static int writer_pid = 50;
u64 timer_ticks(void) { return fake_ms; }
int vfs_ev_pid(void) { return writer_pid; }

/* lado "libSystem": o que o fsevents_core.inc do mOS chama */
static long fake_sys(long n, long op, long arg) {
    mos_fsmeta_t *rq = (mos_fsmeta_t *)arg;
    if (n == SYS_TIME) return (long)fake_ms;
    if (n == SYS_GETTID) return 1;
    if (n != SYS_FSMETA) return -22;
    if (op == MOS_FSM_EVENTID) { u64 last = 0; vfs_ev_read(~0ull, 0, 0, &last); return (long)last; }
    if (op == MOS_FSM_EVENTS) return vfs_ev_read(rq->pos, (void *)rq->buf, (int)(rq->size / sizeof(mos_fsevent_t)), 0);
    return -22;
}
#define SYS0(n) fake_sys(n, 0, 0)
#define SYS2(n, a, b) fake_sys(n, (long)(a), (long)(b))
int getpid(void) { return 77; }
/* sem o fio de varredura: o teste entrega só por FlushSync/fse_scan, sem corrida */
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg) { (void)t; (void)attr; (void)fn; (void)arg; return 11; }
int pthread_join(unsigned long t, void **ret) { (void)t; (void)ret; return 0; }
int usleep(unsigned us) { (void)us; return 0; }
char *getcwd(char *b, size_t n) { if (n > 1) { b[0] = '/'; b[1] = 0; } return b; }
int lstat(const char *p, void *st) { (void)p; (void)st; return -1; }
void *opendir(const char *p) { (void)p; return 0; }
void *readdir(void *d) { (void)d; return 0; }
int closedir(void *d) { (void)d; return 0; }
/* CFArray de mentira: {n, char *v[]} e CFString = char* */
typedef struct { long n; const char *v[16]; } fakearr;
static fakearr arrs[64]; static int narr;
long CFArrayGetCount(void *a) { return ((fakearr *)a)->n; }
void *CFArrayGetValueAtIndex(void *a, long i) { return (void *)((fakearr *)a)->v[i]; }
void *CFArrayCreateMutable(void *al, long cap, const void *cb) { (void)al; (void)cap; (void)cb; arrs[narr].n = 0; return &arrs[narr++ % 64]; }
void CFArrayAppendValue(void *a, void *v) { fakearr *f = a; if (f->n < 16) f->v[f->n++] = v; }
void *CFStringCreateWithCString(void *al, const char *s, u32 enc) { (void)al; (void)enc; return (void *)s; }
void CFRelease(void *o) { (void)o; }
const char *cx_cstr(void *o) { return o; }
void *CFDictionaryCreateMutable(void *a, long c, const void *k, const void *v) { (void)a; (void)c; (void)k; (void)v; return 0; }
void CFDictionarySetValue(void *d, const void *k, const void *v) { (void)d; (void)k; (void)v; }
void *CFNumberCreate(void *a, long t, const void *p) { (void)a; (void)t; (void)p; return 0; }
const char kCFTypeDictionaryKeyCallBacks = 0, kCFTypeDictionaryValueCallBacks = 0;
const void *kFSEventStreamEventExtendedDataPathKey, *kFSEventStreamEventExtendedFileIDKey;

#include "fsevents_core.inc"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); for (int q_ = 0; q_ < ngot; q_++) printf("   [%d] %s %x id %llu\n", q_, got[q_].path, got[q_].flags, (unsigned long long)got[q_].id); } } while (0)
static struct { char path[512]; u32 flags; u64 id; } got[256];
static int ngot;
static void cb(void *st, void *info, size_t n, void *paths, const u32 *flags, const u64 *ids) {
    (void)st; (void)info;
    char **p = paths;
    for (size_t i = 0; i < n && ngot < 256; i++) {
        size_t k = 0; while (p[i][k] && k < 511) { got[ngot].path[k] = p[i][k]; k++; } got[ngot].path[k] = 0;
        got[ngot].flags = flags[i]; got[ngot].id = ids[i]; ngot++;
    }
}

static int streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int find(const char *path, u32 must) {
    for (int i = 0; i < ngot; i++) if (streq(got[i].path, path) && (got[i].flags & must) == must) return i;
    return -1;
}
static fakearr *paths1(const char *a) { fakearr *f = &arrs[narr++ % 64]; f->n = 1; f->v[0] = a; return f; }

int main(void) {
    vfs_init();
    vfs_mkdir("/Users/Visitante/w");
    /* ---- FileEvents */
    void *s = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), kFSEventStreamEventIdSinceNow, 0.0,
                                  kFSEventStreamCreateFlagFileEvents);
    CHECK(FSEventStreamStart(s));
    CHECK(ngot == 0);
    vnode_t *a = vfs_write_file("/Users/Visitante/w/a.txt", "1", 1);
    vfs_write_file("/Users/Visitante/fora.txt", "x", 1);        /* fora da raiz: não chega */
    fake_ms += 5;
    FSEventStreamFlushSync(s);
    CHECK(ngot == 1 && find("/Users/Visitante/w/a.txt", kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemIsFile) == 0);
    u64 id1 = ngot ? got[0].id : 0;
    CHECK(FSEventStreamGetLatestEventId(s) == id1 && FSEventsGetCurrentEventId() >= id1);
    ngot = 0;
    vfs_move(a, vfs_lookup("/Users/Visitante/w"), "b.txt");
    vfs_xattr_set(a, "com.apple.quarantine", "0081;1;x;", 9, 0, 0);
    vfs_mkdir("/Users/Visitante/w/sub");
    FSEventStreamFlushSync(s);
    CHECK(find("/Users/Visitante/w/a.txt", kFSEventStreamEventFlagItemRenamed) >= 0);
    CHECK(find("/Users/Visitante/w/b.txt", kFSEventStreamEventFlagItemRenamed) >= 0);
    CHECK(find("/Users/Visitante/w/b.txt", FSEV_XATTR) >= 0);
    CHECK(find("/Users/Visitante/w/sub", kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemIsDir) >= 0);
    for (int i = 1; i < ngot; i++) CHECK(got[i].id > got[i - 1].id);
    ngot = 0;
    /* ---- exclusão */
    fakearr *ex = paths1("/Users/Visitante/w/sub");
    CHECK(FSEventStreamSetExclusionPaths(s, ex));
    vfs_write_file("/Users/Visitante/w/sub/x", "", 0);
    vfs_write_file("/Users/Visitante/w/y", "", 0);
    FSEventStreamFlushSync(s);
    CHECK(ngot == 1 && find("/Users/Visitante/w/y", kFSEventStreamEventFlagItemCreated) == 0);
    ngot = 0;
    FSEventStreamStop(s); FSEventStreamInvalidate(s); FSEventStreamRelease(s);

    /* ---- IgnoreSelf e MarkSelf (pid 77 = este processo) */
    void *s2 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), kFSEventStreamEventIdSinceNow, 0.0,
                                   kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagIgnoreSelf);
    void *s3 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), kFSEventStreamEventIdSinceNow, 0.0,
                                   kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagMarkSelf);
    FSEventStreamStart(s2); FSEventStreamStart(s3);
    writer_pid = 77;
    vfs_write_file("/Users/Visitante/w/meu", "", 0);
    writer_pid = 50;
    vfs_write_file("/Users/Visitante/w/alheio", "", 0);
    FSEventStreamFlushSync(s2);
    CHECK(ngot == 1 && find("/Users/Visitante/w/alheio", 0) == 0);
    ngot = 0;
    FSEventStreamFlushSync(s3);
    CHECK(ngot == 2 && (got[find("/Users/Visitante/w/meu", 0)].flags & FSEV_OWN_EVENT));
    CHECK(find("/Users/Visitante/w/alheio", 0) >= 0 && !(got[find("/Users/Visitante/w/alheio", 0)].flags & FSEV_OWN_EVENT));
    ngot = 0;
    FSEventStreamRelease(s2); FSEventStreamRelease(s3);

    /* ---- histórico desde um id: repete e termina com HistoryDone */
    u64 cur = FSEventsGetCurrentEventId();
    vfs_write_file("/Users/Visitante/w/h1", "", 0);
    vfs_write_file("/Users/Visitante/w/h2", "", 0);
    void *s4 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), cur, 0.0, kFSEventStreamCreateFlagFileEvents);
    FSEventStreamStart(s4);
    CHECK(ngot == 3 && find("/Users/Visitante/w/h1", 0) == 0 && find("/Users/Visitante/w/h2", 0) == 1);
    CHECK(ngot == 3 && (got[2].flags & kFSEventStreamEventFlagHistoryDone));
    ngot = 0;
    FSEventStreamRelease(s4);
    /* id muito antigo (antes do boot/anel): KernelDropped + MustScanSubDirs, depois HistoryDone */
    void *s5 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), 0, 0.0, kFSEventStreamCreateFlagFileEvents);
    FSEventStreamStart(s5);
    CHECK(ngot >= 2 && (got[ngot - 1].flags & kFSEventStreamEventFlagHistoryDone));
    ngot = 0;
    FSEventStreamRelease(s5);

    /* ---- modo pasta (sem FileEvents): o caminho é a pasta, com MustScanSubDirs */
    void *s6 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), kFSEventStreamEventIdSinceNow, 0.0, 0);
    FSEventStreamStart(s6);
    vfs_write_file("/Users/Visitante/w/sub/z1", "", 0);
    vfs_write_file("/Users/Visitante/w/sub/z2", "", 0);
    FSEventStreamFlushSync(s6);
    CHECK(ngot == 1 && find("/Users/Visitante/w/sub", kFSEventStreamEventFlagMustScanSubDirs) == 0);
    ngot = 0;
    FSEventStreamRelease(s6);

    /* ---- latência: nada antes do prazo, tudo junto depois */
    void *s7 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/w"), kFSEventStreamEventIdSinceNow, 1.0, kFSEventStreamCreateFlagFileEvents);
    FSEventStreamStart(s7);
    vfs_write_file("/Users/Visitante/w/l1", "", 0);
    fse_scan(fse_of(s7), 0);
    CHECK(ngot == 0);
    fake_ms += 1500;
    fse_scan(fse_of(s7), 0);
    CHECK(ngot == 1);
    ngot = 0;
    FSEventStreamRelease(s7);

    /* ---- WatchRoot: a raiz some */
    vfs_mkdir("/Users/Visitante/raiz");
    void *s8 = FSEventStreamCreate(0, cb, 0, paths1("/Users/Visitante/raiz"), kFSEventStreamEventIdSinceNow, 0.0,
                                   kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagWatchRoot);
    FSEventStreamStart(s8);
    vfs_delete(vfs_lookup("/Users/Visitante/raiz"));
    FSEventStreamFlushSync(s8);
    CHECK(find("/Users/Visitante/raiz", kFSEventStreamEventFlagRootChanged) >= 0);
    ngot = 0;
    FSEventStreamRelease(s8);
    cx_fse_shutdown();
    printf("fsevents (journal): %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

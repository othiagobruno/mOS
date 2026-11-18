/* Teste no host de resmgr.c, component.c e mpservices.c (rodar por tools/carbon-legacy-hosttest.py).
 * argv[1] = arquivo de recursos gerado pelo Python (vai para o "resource fork" de argv[2]); argv[3] = saída da gravação. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef char **Handle;
typedef struct { uint8_t hidden[80]; } FSRef;
typedef void *id;

/* ---- Memory Manager mínimo: bloco = [tamanho][dados], Handle aponta para o ponteiro mestre */
Handle NewHandle(long n) { Handle h = malloc(sizeof(char *)); long *b = malloc(sizeof(long) + (n ? n : 1)); b[0] = n; *h = (char *)(b + 1); return h; }
long GetHandleSize(Handle h) { return h ? ((long *)*h)[-1] : 0; }
void SetHandleSize(Handle h, long n) { long *b = realloc((long *)*h - 1, sizeof(long) + (n ? n : 1)); b[0] = n; *h = (char *)(b + 1); }
void DisposeHandle(Handle h) { if (h) { free((long *)*h - 1); free(h); } }
/* ---- FSRef = caminho; xattr do resource fork = arquivo argv[2].rsrc */
int32_t FSRefMakePath(const FSRef *r, uint8_t *p, uint32_t max) { snprintf((char *)p, max, "%s", (const char *)r->hidden); return 0; }
int16_t FSCreateFileUnicode(const FSRef *a, unsigned long b, const uint16_t *c, uint32_t d, const void *e, FSRef *f, void *g) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; return -50; }
static char rsrc_path[1100];
long getxattr(const char *p, const char *n, void *v, size_t s, uint32_t pos, int o) {
    (void)n; (void)pos; (void)o;
    snprintf(rsrc_path, sizeof rsrc_path, "%s.rsrc", p);
    FILE *f = fopen(rsrc_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (v) len = (long)fread(v, 1, s, f);
    fclose(f);
    return len;
}
int setxattr(const char *p, const char *n, const void *v, size_t s, uint32_t pos, int o) {
    (void)n; (void)pos; (void)o;
    snprintf(rsrc_path, sizeof rsrc_path, "%s.rsrc", p);
    FILE *f = fopen(rsrc_path, "wb");
    if (!f) return -1;
    fwrite(v, 1, s, f);
    fclose(f);
    return 0;
}
/* ---- pthreads: o que o Linux não tem */
int pthread_cond_timedwait_relative_np(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *rel) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += rel->tv_sec; ts.tv_nsec += rel->tv_nsec;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    return pthread_cond_timedwait(c, m, &ts);
}
/* ---- AudioComponent falso: duas unidades */
typedef struct { uint32_t t, s, m, f, k; } cdesc_t;
static cdesc_t g_ac[2] = {{0x61756f75, 0x64656620, 0x6170706c, 0, 0}, {0x61756d78, 0x6d636d78, 0x6170706c, 0, 0}};
static int g_inst_dead;
void *AudioComponentFindNext(void *after, const cdesc_t *d) {
    int start = after == &g_ac[0] ? 1 : after == &g_ac[1] ? 2 : 0;
    for (int i = start; i < 2; i++) if (!d || !d->t || d->t == g_ac[i].t) return &g_ac[i];
    return 0;
}
int AudioComponentGetDescription(void *c, cdesc_t *o) { *o = *(cdesc_t *)c; return 0; }
int AudioComponentCopyName(void *c, id *o) { *o = c == &g_ac[0] ? "Apple: DefaultOutputUnit" : "Apple: AUMultiChannelMixer"; return 0; }
int AudioComponentGetVersion(void *c, uint32_t *v) { (void)c; *v = 0x10000; return 0; }
static char g_auinst[2];
int AudioComponentInstanceNew(void *c, void **o) { *o = &g_auinst[c == &g_ac[1]]; return 0; }
int AudioComponentInstanceDispose(void *u) { (void)u; g_inst_dead++; return 0; }
void *AudioComponentInstanceGetComponent(void *u) { return u == &g_auinst[0] ? &g_ac[0] : u == &g_auinst[1] ? &g_ac[1] : 0; }
const char *cx_cstr(id s) { return s; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }

/* ---- APIs testadas */
short FSOpenResFile(const FSRef *, signed char);
short FSOpenResourceFile(const FSRef *, unsigned long, const uint16_t *, signed char, short *);
void CloseResFile(short);
short CurResFile(void);
void UseResFile(short);
short ResError(void);
Handle GetResource(uint32_t, short);
Handle Get1Resource(uint32_t, short);
Handle Get1NamedResource(uint32_t, const uint8_t *);
short Count1Resources(uint32_t);
short CountResources(uint32_t);
short Count1Types(void);
void Get1IndType(uint32_t *, short);
Handle Get1IndResource(uint32_t, short);
void GetResInfo(Handle, short *, uint32_t *, uint8_t *);
short GetResAttrs(Handle);
long GetResourceSizeOnDisk(Handle);
void ReleaseResource(Handle);
void AddResource(Handle, uint32_t, short, const uint8_t *);
void ChangedResource(Handle);
void RemoveResource(Handle);
void UpdateResFile(short);
short DetachResourceFile(short);
short InsertResourceFile(short, short);
void SetResourceSize(Handle, long);
short Unique1ID(uint32_t);
short HomeResFile(Handle);
void *FindNextComponent(void *, const cdesc_t *);
long CountComponents(const cdesc_t *);
short GetComponentInfo(void *, cdesc_t *, Handle, Handle, Handle);
typedef struct { uint8_t flags, paramSize; int16_t what; uint32_t pad; long params[]; } cparams_t;
void *RegisterComponent(const cdesc_t *, long (*)(cparams_t *, Handle), short, Handle, Handle, Handle);
short UnregisterComponent(void *);
short OpenAComponent(void *, void **);
void *OpenDefaultComponent(uint32_t, uint32_t);
short CloseComponent(void *);
long CallComponentDispatch(cparams_t *);
long GetComponentVersion(void *);
long ComponentFunctionImplemented(void *, short);
void SetComponentInstanceStorage(void *, Handle);
Handle GetComponentInstanceStorage(void *);
int MPCreateQueue(void **);
int MPDeleteQueue(void *);
int MPNotifyQueue(void *, void *, void *, void *);
int MPWaitOnQueue(void *, void **, void **, void **, int32_t);
int MPCreateSemaphore(uint32_t, uint32_t, void **);
int MPSignalSemaphore(void *);
int MPWaitOnSemaphore(void *, int32_t);
int MPDeleteSemaphore(void *);
int MPCreateTask(int (*)(void *), void *, size_t, void *, void *, void *, uint32_t, void **);
int MPTerminateTask(void *, int);
int MPSetTaskWeight(void *, uint32_t);
void MPExit(int);
void MPYield(void);
int MPCreateCriticalRegion(void **);
int MPEnterCriticalRegion(void *, int32_t);
int MPExitCriticalRegion(void *);
void *MPCurrentTaskID(void);

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU linha %d: %s\n", __LINE__, #c); fails++; } } while (0)
#define FCC(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

/* componente do app: conta as chamadas */
static int g_open, g_close, g_reg;
static long comp_entry(cparams_t *p, Handle storage) {
    void *self = (void *)p->params[p->paramSize / sizeof(long)];
    switch (p->what) {
    case -1: g_open++; SetComponentInstanceStorage(self, (Handle)0x1234); return 0;
    case -2: g_close++; return 0;
    case -3: return p->params[0] == 7 || p->params[0] == -4;
    case -4: return 0x00020003;
    case -5: g_reg++; return 0;
    case 7: return (long)storage + p->params[0] + p->params[1];                 /* soma dos dois parâmetros + storage */
    }
    return (long)(int)0x80008002;
}
static int task_fn(void *p) { int *v = p; for (int i = 0; i < 1000; i++) __atomic_add_fetch(v, 1, __ATOMIC_SEQ_CST); return 42; }
static void *g_sem;
static int waiter_fn(void *p) { (void)p; return MPWaitOnSemaphore(g_sem, 0x7FFFFFFF); }
static int exit_fn(void *p) { (void)p; MPExit(-7); return 0; }
static void *g_crit;
static int g_shared;
static int crit_fn(void *p) {
    (void)p;
    for (int i = 0; i < 2000; i++) {
        MPEnterCriticalRegion(g_crit, 0x7FFFFFFF);
        MPEnterCriticalRegion(g_crit, 0x7FFFFFFF);                               /* reentrante */
        int v = g_shared; MPYield(); g_shared = v + 1;
        MPExitCriticalRegion(g_crit);
        MPExitCriticalRegion(g_crit);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    /* põe o arquivo do Python no "resource fork" de argv[2] */
    FILE *in = fopen(argv[1], "rb"), *out;
    static uint8_t buf[1 << 16];
    size_t n = fread(buf, 1, sizeof buf, in);
    fclose(in);
    out = fopen(argv[2], "wb"); fclose(out);
    setxattr(argv[2], "", buf, n, 0, 0);
    FSRef ref;
    memset(&ref, 0, sizeof ref);
    snprintf((char *)ref.hidden, sizeof ref.hidden, "%s", argv[2]);
    short rf = FSOpenResFile(&ref, 3);
    CHECK(rf > 0 && ResError() == 0 && CurResFile() == rf);
    CHECK(FSOpenResFile(&ref, 1) == rf);                                         /* o mesmo arquivo: mesmo refnum */
    CHECK(Count1Types() == 2 && Count1Resources(FCC('S', 'T', 'R', ' ')) == 2 && Count1Resources(FCC('i', 'c', 'n', 's')) == 1);
    uint32_t t = 0;
    Get1IndType(&t, 2);
    CHECK(t == FCC('i', 'c', 'n', 's'));
    Get1IndType(&t, 3);
    CHECK(ResError() == -192);
    Handle h = Get1Resource(FCC('S', 'T', 'R', ' '), 128);
    CHECK(h && GetHandleSize(h) == 6 && !memcmp(*h, "\005Hello", 6));
    CHECK(Get1Resource(FCC('S', 'T', 'R', ' '), 128) == h);                      /* o mesmo handle */
    short rid; uint32_t rt; uint8_t nm[256];
    GetResInfo(h, &rid, &rt, nm);
    CHECK(rid == 128 && rt == FCC('S', 'T', 'R', ' ') && nm[0] == 5 && !memcmp(nm + 1, "hello", 5));
    CHECK(GetResAttrs(h) == 0x20 && GetResourceSizeOnDisk(h) == 6 && HomeResFile(h) == rf);
    Handle h2 = Get1NamedResource(FCC('S', 'T', 'R', ' '), (const uint8_t *)"\003two");
    CHECK(h2 && GetHandleSize(h2) == 4 && !memcmp(*h2, "\003abc", 4));
    Handle ic = Get1IndResource(FCC('i', 'c', 'n', 's'), 1);
    CHECK(ic && GetHandleSize(ic) == 300 && (uint8_t)(*ic)[299] == (299 & 0xff));
    CHECK(!Get1Resource(FCC('S', 'T', 'R', ' '), 999) && ResError() == -192);
    /* escreve: muda um, apaga outro, acrescenta um */
    memcpy(*h, "\005HELLO", 6);
    ChangedResource(h);
    CHECK(ResError() == 0);
    RemoveResource(h2);
    DisposeHandle(h2);
    Handle nh = NewHandle(3);
    memcpy(*nh, "xyz", 3);
    short uid = Unique1ID(FCC('T', 'E', 'X', 'T'));
    AddResource(nh, FCC('T', 'E', 'X', 'T'), uid, (const uint8_t *)"\004novo");
    CHECK(ResError() == 0 && uid >= 128);
    UpdateResFile(rf);
    CHECK(ResError() == 0);
    ReleaseResource(h);
    CHECK(ResError() == 0);
    Handle again = Get1Resource(FCC('S', 'T', 'R', ' '), 128);
    CHECK(again && !memcmp(*again, "\005HELLO", 6));
    CloseResFile(rf);
    CHECK(ResError() == 0);
    UseResFile(rf);
    CHECK(ResError() == -193);
    /* reabre o que foi gravado e copia para argv[3] (o Python confere) */
    rf = FSOpenResFile(&ref, 1);
    CHECK(rf > 0 && Count1Types() == 3 && Count1Resources(FCC('S', 'T', 'R', ' ')) == 1);
    Handle tx = Get1Resource(FCC('T', 'E', 'X', 'T'), uid);
    CHECK(tx && GetHandleSize(tx) == 3 && !memcmp(*tx, "xyz", 3));
    CHECK(GetResource(FCC('i', 'c', 'n', 's'), 1) != 0);
    CHECK(DetachResourceFile(rf) == 0 && !GetResource(FCC('i', 'c', 'n', 's'), 1) && ResError() == -192);
    CHECK(InsertResourceFile(rf, 4) == 0 && CurResFile() == rf && GetResource(FCC('i', 'c', 'n', 's'), 1) != 0);
    CHECK(DetachResourceFile(rf) == 0);
    CloseResFile(rf);                                                            /* fecha mesmo destacado */
    CHECK(ResError() == 0 && InsertResourceFile(rf, 4) == -193);
    rf = FSOpenResFile(&ref, 1);
    n = (size_t)getxattr(argv[2], "", buf, sizeof buf, 0, 0);
    out = fopen(argv[3], "wb"); fwrite(buf, 1, n, out); fclose(out);
    /* data fork (.rsrc) e erros */
    short df;
    CHECK(FSOpenResourceFile(&ref, 0, 0, 1, &df) == -199 || FSOpenResourceFile(&ref, 0, 0, 1, &df) == -39);   /* data fork vazio */
    uint16_t bad[] = {'X'};
    CHECK(FSOpenResourceFile(&ref, 1, bad, 1, &df) == -50);

    /* ---- Component Manager */
    cdesc_t any = {0, 0, 0, 0, 0};
    CHECK(CountComponents(&any) == 2);
    cdesc_t d = {FCC('t', 'e', 's', 't'), FCC('s', 'u', 'b', ' '), FCC('m', 'O', 'S', ' '), 0x80000000u | 1, 0};
    Handle name = NewHandle(5);
    memcpy(*name, "\004Nome", 5);
    void *c = RegisterComponent(&d, comp_entry, 0, name, 0, 0);
    CHECK(c && g_reg == 1 && CountComponents(&any) == 3);
    cdesc_t want = {FCC('t', 'e', 's', 't'), 0, 0, 1, 1};
    CHECK(FindNextComponent(0, &want) == c && FindNextComponent(c, &want) == 0);
    want.f = 0;
    CHECK(FindNextComponent(0, &want) == 0);                                     /* flags não batem */
    Handle nh2 = NewHandle(0);
    cdesc_t got;
    CHECK(GetComponentInfo(c, &got, nh2, 0, 0) == 0 && got.t == d.t && GetHandleSize(nh2) == 5 && !memcmp(*nh2, "\004Nome", 5));
    void *ci = 0;
    CHECK(OpenAComponent(c, &ci) == 0 && ci && g_open == 1 && GetComponentInstanceStorage(ci) == (Handle)0x1234);
    long pb[8];
    cparams_t *p = (cparams_t *)pb;
    p->flags = 0; p->paramSize = 2 * sizeof(long); p->what = 7; p->pad = 0;
    p->params[0] = 10; p->params[1] = 20; p->params[2] = (long)ci;
    CHECK(CallComponentDispatch(p) == 0x1234 + 30);
    CHECK(GetComponentVersion(ci) == 0x00020003);
    CHECK(ComponentFunctionImplemented(ci, 7) == 1 && ComponentFunctionImplemented(ci, 9) == 0);
    CHECK(UnregisterComponent(c) == -3001);                                      /* ainda tem instância */
    CHECK(CloseComponent(ci) == 0 && g_close == 1);
    CHECK(UnregisterComponent(c) == 0 && CountComponents(&any) == 2);
    void *au = OpenDefaultComponent(FCC('a', 'u', 'o', 'u'), 0);
    CHECK(au == &g_auinst[0]);
    CHECK(GetComponentVersion(au) == 0x10000);
    CHECK(CloseComponent(au) == 0 && g_inst_dead == 1);
    CHECK(GetComponentInfo(&g_ac[1], &got, nh2, 0, 0) == 0 && got.s == FCC('m', 'c', 'm', 'x') && (*nh2)[0] == 26);
    CHECK(CloseComponent((void *)0x99) == (short)0x8001);

    /* ---- MP */
    void *q, *task;
    CHECK(MPCreateQueue(&q) == 0);
    int counter = 0;
    CHECK(MPCreateTask(task_fn, &counter, 0, q, (void *)1, (void *)2, 0, &task) == 0);
    void *r1, *r2, *r3;
    CHECK(MPWaitOnQueue(q, &r1, &r2, &r3, 2000) == 0 && r1 == (void *)1 && r2 == (void *)2 && (long)r3 == 42 && counter == 1000);
    CHECK(MPWaitOnQueue(q, &r1, &r2, &r3, 0) == -29296);
    CHECK(MPWaitOnQueue(q, &r1, &r2, &r3, -20000) == -29296);                    /* 20 ms */
    CHECK(MPCreateTask(exit_fn, 0, 65536, q, 0, 0, 0, &task) == 0);
    CHECK(MPWaitOnQueue(q, &r1, &r2, &r3, 2000) == 0 && (long)r3 == -7);
    CHECK(MPCreateSemaphore(1, 0, &g_sem) == 0);
    CHECK(MPWaitOnSemaphore(g_sem, 10) == -29296);
    CHECK(MPSignalSemaphore(g_sem) == 0 && MPSignalSemaphore(g_sem) == -29298);  /* passa do máximo */
    CHECK(MPWaitOnSemaphore(g_sem, 0) == 0);
    CHECK(MPCreateTask(waiter_fn, 0, 0, q, 0, 0, 0, &task) == 0);
    usleep(20000);
    CHECK(MPSignalSemaphore(g_sem) == 0);
    CHECK(MPWaitOnQueue(q, &r1, &r2, &r3, 2000) == 0 && (long)r3 == 0);
    CHECK(MPCreateTask(waiter_fn, 0, 0, q, 0, 0, 0, &task) == 0);
    usleep(20000);
    CHECK(MPDeleteSemaphore(g_sem) == 0);
    CHECK(MPWaitOnQueue(q, &r1, &r2, &r3, 2000) == 0 && (long)r3 == -29295);    /* quem esperava recebe "apagado" */
    CHECK(MPWaitOnSemaphore(g_sem, 0) == -29299);
    CHECK(MPCreateCriticalRegion(&g_crit) == 0);
    void *t1, *t2;
    MPCreateTask(crit_fn, 0, 0, q, 0, 0, 0, &t1);
    MPCreateTask(crit_fn, 0, 0, q, 0, 0, 0, &t2);
    CHECK(MPWaitOnQueue(q, 0, 0, 0, 5000) == 0 && MPWaitOnQueue(q, 0, 0, 0, 5000) == 0 && g_shared == 4000);
    CHECK(MPSetTaskWeight(t1, 200) == 0 && MPSetTaskWeight((void *)0x5, 1) == -29299);
    CHECK(MPCurrentTaskID() != 0);
    CHECK(MPDeleteQueue(q) == 0 && MPNotifyQueue(q, 0, 0, 0) == -29299);
    printf(fails ? "carbon: %d falha(s)\n" : "carbon: ok\n", fails);
    return fails != 0;
}

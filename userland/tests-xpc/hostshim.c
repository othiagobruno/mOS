/* Harness de host para o xpctest. Não entra no libSystem.
 * bootstrap e mach_msg vivem num mmap compartilhado: depois do fork os dois
 * processos enxergam as mesmas filas. O runtime ObjC daqui só existe para o
 * xpc.c e o teste ligarem neste Linux. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

typedef struct cx_class cx_class_t;
typedef struct objc_object { cx_class_t *isa; } *id;
typedef const char *SEL;
typedef id (*IMP)(id, SEL, ...);
typedef signed char BOOL;
struct cx_class {
    cx_class_t *isa;
    cx_class_t *super;
    void *cache;
    void *vtable;
    unsigned long long data;
};
typedef struct { long sec; int usec; } mac_tv_t;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
#define YES ((BOOL)1)
#define NO ((BOOL)0)

extern unsigned bootstrap_port;

#define T_RECV 0x1u
#define T_SEND 0x10000u
#define MAX_K 64
#define MAX_NAME 256
#define MAX_SVC 64
#define QN 8
#define MSGMAX 8192

struct slot {
    int used, sender;
    uint32_t size;
    uint8_t data[MSGMAX];
};
struct kport {
    int alive, recv_pid;
    uint32_t recv_name;
    int nmsg;
    struct slot q[QN];
};
struct nm {
    int used, pid, k, once;
    uint32_t name, send_refs;
    int recv;
};
struct svc {
    int used, k;
    char name[96];
};
struct arena {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    uint32_t next;
    struct kport kp[MAX_K];
    struct nm nm[MAX_NAME];
    struct svc svc[MAX_SVC];
};
static struct arena *A;

static void lk(void) { pthread_mutex_lock(&A->mu); }
static void un(void) { pthread_mutex_unlock(&A->mu); }
static struct nm *nm_find(int pid, uint32_t name) {
    for (int i = 0; i < MAX_NAME; i++)
        if (A->nm[i].used && A->nm[i].pid == pid && A->nm[i].name == name) return &A->nm[i];
    return 0;
}
static struct nm *nm_new(int pid, int k, int recv, int send, int once) {
    for (int i = 0; i < MAX_NAME; i++) if (!A->nm[i].used) {
        struct nm *n = &A->nm[i];
        memset(n, 0, sizeof *n);
        n->used = 1;
        n->pid = pid;
        n->k = k;
        n->name = A->next;
        A->next += 4;
        n->recv = recv;
        n->send_refs = send;
        n->once = once;
        if (recv) {
            A->kp[k].recv_pid = pid;
            A->kp[k].recv_name = n->name;
        }
        return n;
    }
    return 0;
}
static int k_new(void) {
    for (int i = 0; i < MAX_K; i++) if (!A->kp[i].alive) {
        memset(&A->kp[i], 0, sizeof A->kp[i]);
        A->kp[i].alive = 1;
        return i;
    }
    return -1;
}
static void nm_drop(struct nm *n) {
    if (!n) return;
    n->used = 0;
}

int mach_port_allocate(unsigned task, int right, unsigned *name) {
    (void)task;
    if (!name || right != 1) return 4;
    lk();
    int k = k_new();
    struct nm *n = k >= 0 ? nm_new(getpid(), k, 1, 0, 0) : 0;
    if (n) *name = n->name;
    un();
    return n ? 0 : 5;
}
int mach_port_insert_right(unsigned task, unsigned name, unsigned poly, int type) {
    (void)task;
    (void)poly;
    lk();
    struct nm *n = nm_find(getpid(), name);
    int rc = 4;
    if (n && n->recv && type == 20) { n->send_refs = 1; rc = 0; }
    un();
    return rc;
}
int mach_port_type(unsigned task, unsigned name, unsigned *ptype) {
    (void)task;
    lk();
    struct nm *n = nm_find(getpid(), name);
    if (n && ptype) {
        *ptype = (n->recv ? T_RECV : 0) | (n->send_refs || n->once ? T_SEND : 0);
    }
    int rc = n ? 0 : 4;
    un();
    return rc;
}
int mach_port_deallocate(unsigned task, unsigned name) {
    (void)task;
    lk();
    struct nm *n = nm_find(getpid(), name);
    if (n) {
        if (n->send_refs) n->send_refs--;
        if (!n->recv && !n->send_refs && !n->once) nm_drop(n);
        else if (!n->send_refs && !n->once && !n->recv) nm_drop(n);
    }
    un();
    return 0;
}
int mach_port_request_notification(unsigned task, unsigned name, int variant, unsigned sync, unsigned notify, int poly, unsigned *prev) {
    (void)task; (void)name; (void)variant; (void)sync; (void)notify; (void)poly;
    if (prev) *prev = 0;
    return 0;
}

static uint32_t rd32(const uint8_t *b, uint32_t o) {
    return (uint32_t)b[o] | (uint32_t)b[o + 1] << 8 | (uint32_t)b[o + 2] << 16 | (uint32_t)b[o + 3] << 24;
}
static void wr32(uint8_t *b, uint32_t o, uint32_t v) {
    b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); b[o + 2] = (uint8_t)(v >> 16); b[o + 3] = (uint8_t)(v >> 24);
}
static uint32_t give_send(int pid, int k) {
    struct nm *n = nm_new(pid, k, 0, 1, 0);
    return n ? n->name : 0;
}
static int do_send(void *msg, unsigned send_size) {
    uint8_t *b = msg;
    if (send_size < 24 || send_size > MSGMAX) return 0x10000002;
    int me = getpid();
    uint32_t dest = rd32(b, 8);
    uint32_t bits = rd32(b, 0);
    struct nm *dn = nm_find(me, dest);
    if (!dn || (!dn->send_refs && !dn->once) || !A->kp[dn->k].alive) return 0x10000003;
    struct kport *kp = &A->kp[dn->k];
    if (!kp->recv_name) return 0x10000003;
    if (kp->nmsg >= QN) return 0x10000004;
    struct slot *s = 0;
    for (int i = 0; i < QN; i++) if (!kp->q[i].used) { s = &kp->q[i]; break; }
    if (!s) return 0x10000004;
    memset(s, 0, sizeof *s);
    s->used = 1;
    s->sender = me;
    s->size = send_size;
    memcpy(s->data, b, send_size);
    int once = dn->once;
    if (once) nm_drop(dn);
    int rpid = kp->recv_pid;
    uint32_t local = rd32(b, 12);
    int ldisp = (bits >> 8) & 0xff;
    uint32_t reply = 0;
    if (local && ldisp == 21) {
        struct nm *ln = nm_find(me, local);
        if (ln && ln->recv) {
            struct nm *so = nm_new(rpid, ln->k, 0, 0, 1);
            if (so) reply = so->name;
        }
    }
    wr32(s->data, 8, reply);
    if (bits & 0x80000000u) {
        uint32_t nd = rd32(s->data, 24);
        for (uint32_t i = 0; i < nd && i < 8; i++) {
            uint32_t off = 28 + i * 12;
            if (off + 12 > send_size) break;
            if (s->data[off + 11] != 0) continue;
            uint32_t pn = rd32(s->data, off);
            int disp = s->data[off + 10];
            struct nm *pnr = nm_find(me, pn);
            uint32_t neu = 0;
            if (pnr && (disp == 19 || disp == 20) && (pnr->send_refs || (disp == 20 && pnr->recv)))
                neu = give_send(rpid, pnr->k);
            wr32(s->data, off, neu);
        }
    }
    kp->nmsg++;
    pthread_cond_broadcast(&A->cv);
    return 0;
}
static int do_recv(void *msg, unsigned rcv_size, unsigned rcv_name, int timed, unsigned timeout) {
    int me = getpid();
    struct nm *n = nm_find(me, rcv_name);
    if (!n || !n->recv) return 0x10000003;
    struct kport *kp = &A->kp[n->k];
    struct timespec ts;
    if (timed) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (long)(timeout % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    }
    while (kp->nmsg == 0) {
        if (!timed) pthread_cond_wait(&A->cv, &A->mu);
        else if (pthread_cond_timedwait(&A->cv, &A->mu, &ts) == ETIMEDOUT) return 0x10000004;
        n = nm_find(me, rcv_name);
        if (!n || !n->recv) return 0x10000003;
        kp = &A->kp[n->k];
    }
    struct slot *s = 0;
    for (int i = 0; i < QN; i++) if (kp->q[i].used) { s = &kp->q[i]; break; }
    if (!s) { kp->nmsg = 0; return 0x10000004; }
    if (s->size > rcv_size) { s->used = 0; kp->nmsg--; return 0x10004004; }
    memcpy(msg, s->data, s->size);
    uint8_t *out = msg;
    uint32_t body = (s->size + 3u) & ~3u;
    if (body + 44 <= rcv_size) wr32(out, body + 40, (uint32_t)s->sender);
    s->used = 0;
    kp->nmsg--;
    return 0;
}
int mach_msg(void *msg, int option, unsigned send_size, unsigned rcv_size, unsigned rcv_name, unsigned timeout, unsigned notify) {
    (void)notify;
    lk();
    int rc = 0;
    if (option & 1) rc = do_send(msg, send_size);
    if (rc == 0 && (option & 2)) rc = do_recv(msg, rcv_size, rcv_name, (option & 0x100) != 0, timeout);
    un();
    return rc;
}

int bootstrap_check_in(unsigned bp, const char *name, unsigned *port) {
    (void)bp;
    if (!name || !port) return 5;
    lk();
    int rc = 5;
    for (int i = 0; i < MAX_SVC; i++) if (A->svc[i].used && !strcmp(A->svc[i].name, name)) { rc = 1102; goto out; }
    int k = k_new();
    struct nm *n = k >= 0 ? nm_new(getpid(), k, 1, 0, 0) : 0;
    if (!n) goto out;
    for (int i = 0; i < MAX_SVC; i++) if (!A->svc[i].used) {
        A->svc[i].used = 1;
        A->svc[i].k = k;
        snprintf(A->svc[i].name, sizeof A->svc[i].name, "%s", name);
        *port = n->name;
        rc = 0;
        break;
    }
out:
    un();
    return rc;
}
int bootstrap_look_up(unsigned bp, const char *name, unsigned *port) {
    (void)bp;
    if (!name || !port) return 5;
    lk();
    int rc = 1102;
    for (int i = 0; i < MAX_SVC; i++) if (A->svc[i].used && !strcmp(A->svc[i].name, name)) {
        if (!A->kp[A->svc[i].k].alive) break;
        uint32_t nmv = give_send(getpid(), A->svc[i].k);
        if (!nmv) { rc = 5; break; }
        *port = nmv;
        rc = 0;
        break;
    }
    un();
    return rc;
}

/* ---- ObjC mínimo e blocos ---- */
struct rw {
    uint32_t magic, meta;
    const char *name;
    uint32_t size;
    struct { SEL sel; IMP imp; } m[48];
    int nm;
};
static cx_class_t *classes[64];
static int nclasses;

static struct rw *RW(cx_class_t *c) { return c ? c->vtable : 0; }
const char *cx_class_name(cx_class_t *c) { struct rw *r = RW(c); return r ? r->name : 0; }
u32 cx_instance_size(cx_class_t *c) { struct rw *r = RW(c); return r ? r->size : 0; }
id objc_getClass(const char *name);
cx_class_t *cx_class_named(const char *name) {
    for (int i = 0; i < nclasses; i++) {
        struct rw *r = RW(classes[i]);
        if (r && !r->meta && r->name && !strcmp(r->name, name)) return classes[i];
    }
    return 0;
}
id objc_getClass(const char *name) { return (id)cx_class_named(name); }
static IMP find_imp(cx_class_t *c, SEL sel) {
    for (; c; c = c->super) {
        struct rw *r = RW(c);
        if (!r) break;
        for (int i = r->nm - 1; i >= 0; i--) if (r->m[i].sel == sel) return r->m[i].imp;
    }
    return 0;
}
IMP class_getMethodImplementation(cx_class_t *c, SEL s) { return find_imp(c, s); }
BOOL cx_responds(cx_class_t *c, SEL s) { return find_imp(c, s) != 0; }
static const char *sels[256];
static int nsels;
SEL sel_registerName(const char *name) {
    if (!name) name = "";
    for (int i = 0; i < nsels; i++) if (!strcmp(sels[i], name)) return (SEL)sels[i];
    if (nsels < 256) sels[nsels++] = strdup(name);
    return (SEL)sels[nsels - 1];
}
const char *sel_getName(SEL s) { return s ? s : ""; }

static cx_class_t *new_cls(cx_class_t *super, const char *name, int meta, uint32_t size) {
    cx_class_t *c = calloc(1, sizeof *c);
    struct rw *r = calloc(1, sizeof *r);
    r->magic = 0x52575843u;
    r->meta = meta;
    r->name = strdup(name);
    r->size = size ? size : 8;
    c->vtable = r;
    c->super = super;
    if (nclasses < 64) classes[nclasses++] = c;
    return c;
}
cx_class_t *objc_allocateClassPair(cx_class_t *super, const char *name, u64 extra) {
    uint32_t sz = (super ? cx_instance_size(super) : 8) + (uint32_t)extra;
    cx_class_t *meta = new_cls(super ? super->isa : 0, name, 1, 8);
    cx_class_t *c = new_cls(super, name, 0, sz);
    c->isa = meta;
    meta->isa = super && super->isa ? super->isa : meta;
    return c;
}
void objc_registerClassPair(cx_class_t *c) { (void)c; }
BOOL class_addIvar(cx_class_t *c, const char *name, u64 size, u8 align, const char *types) {
    (void)name; (void)types;
    struct rw *r = RW(c);
    if (!r) return NO;
    uint32_t a = align < 3 ? 8u : (1u << align);
    r->size = (r->size + a - 1u) & ~(a - 1u);
    r->size += (uint32_t)size;
    return YES;
}
static int add_m(cx_class_t *c, SEL s, IMP imp, int replace) {
    struct rw *r = RW(c);
    if (!r) return 0;
    for (int i = 0; i < r->nm; i++) if (r->m[i].sel == s) {
        if (!replace) return 0;
        r->m[i].imp = imp;
        return 1;
    }
    if (r->nm >= 48) return 0;
    r->m[r->nm].sel = s;
    r->m[r->nm].imp = imp;
    r->nm++;
    return 1;
}
BOOL class_addMethod(cx_class_t *c, SEL s, IMP imp, const char *types) {
    (void)types;
    return add_m(c, s, imp, 0) ? YES : NO;
}
IMP class_replaceMethod(cx_class_t *c, SEL s, IMP imp, const char *types) {
    (void)types;
    IMP old = find_imp(c, s);
    add_m(c, s, imp, 1);
    return old;
}

#define REFS 1024
static struct { void *p; int n; } refs[REFS];
static void ref_set(void *p, int n) {
    for (int i = 0; i < REFS; i++) if (refs[i].p == p) { refs[i].n = n; if (!n) refs[i].p = 0; return; }
    if (!n) return;
    for (int i = 0; i < REFS; i++) if (!refs[i].p) { refs[i].p = p; refs[i].n = n; return; }
}
static int ref_get(void *p, int *n) {
    for (int i = 0; i < REFS; i++) if (refs[i].p == p) { *n = refs[i].n; return 1; }
    return 0;
}
id class_createInstance(cx_class_t *c, u64 extra) {
    uint32_t sz = cx_instance_size(c) + (uint32_t)extra;
    if (sz < sizeof(void *)) sz = sizeof(void *);
    id o = calloc(1, sz);
    if (!o) return 0;
    o->isa = c;
    ref_set(o, 1);
    return o;
}
id cx_alloc_instance(cx_class_t *c) { return class_createInstance(c, 0); }
id objc_retain(id o) {
    int n;
    if (o && ref_get(o, &n)) ref_set(o, n + 1);
    return o;
}
id objc_autorelease(id o) { return o; }
void objc_release(id o) {
    int n;
    if (!o || !ref_get(o, &n)) return;
    if (n > 1) { ref_set(o, n - 1); return; }
    ref_set(o, 0);
    IMP d = o->isa ? find_imp(o->isa, sel_registerName("dealloc")) : 0;
    if (d) ((void (*)(id, SEL))d)(o, sel_registerName("dealloc"));
    else free(o);
}
/* Não é variádico de propósito: no SysV os ponteiros vêm nos mesmos registradores
 * quando o chamador faz cast para um protótipo fixo. */
id objc_msgSend(id self, SEL op, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) {
    if (!self) return 0;
    cx_class_t *cls = self->isa;
    IMP imp = find_imp(cls, op);
    if (!imp && cls && RW(cls) && !RW(cls)->meta) {
        IMP res = find_imp(cls->isa, sel_registerName("resolveInstanceMethod:"));
        if (res) {
            BOOL ok = ((BOOL (*)(id, SEL, SEL))res)((id)cls, sel_registerName("resolveInstanceMethod:"), op);
            if (ok) imp = find_imp(cls, op);
        }
    }
    if (!imp) return 0;
    return ((id (*)(id, SEL, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))imp)(self, op, a, b, c, d, e);
}

static id m_alloc(id cls, SEL s) { (void)s; return cx_alloc_instance((cx_class_t *)cls); }
static id m_init(id s, SEL c) { (void)c; return s; }
static void m_dealloc(id s, SEL c) { (void)c; free(s); }

struct sobj {
    cx_class_t *isa;
    char *s;
    long long n;
    int kind; /* 1 string, 2 number */
};
static cx_class_t *str_cls, *num_cls;
id cx_str(const char *s) {
    struct sobj *o = (struct sobj *)class_createInstance(str_cls, 0);
    if (!o) return 0;
    o->kind = 1;
    o->s = strdup(s ? s : "");
    return (id)o;
}
const char *cx_cstr(id o) {
    struct sobj *s = (struct sobj *)o;
    return s && s->kind == 1 && s->s ? s->s : "";
}
int cx_is_str(id o) {
    struct sobj *s = (struct sobj *)o;
    return s && s->kind == 1;
}
id cx_num_i(i64 v) {
    struct sobj *o = (struct sobj *)class_createInstance(num_cls, 0);
    if (!o) return 0;
    o->kind = 2;
    o->n = v;
    return (id)o;
}
static id m_ll(id s, SEL c) { (void)c; return (id)(uintptr_t)((struct sobj *)s)->n; }
static void m_str_dealloc(id s, SEL c) { (void)c; free(((struct sobj *)s)->s); free(s); }
static id m_utf8(id s, SEL c) { (void)c; return (id)cx_cstr(s); }

int cx_launchd_submit(const char *label, const char *program) { (void)label; (void)program; return -1; }
id cx_plist_file(const char *path) { (void)path; return 0; }

typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } hostblk_t;
typedef struct { unsigned long reserved, size; void (*copy)(void *, const void *); void (*dispose)(const void *); } hostbd_t;
typedef struct { void *isa; void *forwarding; int flags, size; void (*keep)(void *, void *); void (*destroy)(void *); } hostby_t;
cx_class_t _NSConcreteStackBlock;
cx_class_t _NSConcreteMallocBlock;
cx_class_t _NSConcreteGlobalBlock;

void *_Block_copy(const void *arg) {
    hostblk_t *b = (hostblk_t *)arg;
    if (!b) return 0;
    if (b->flags & (1 << 28)) return b;
    if (b->flags & (1 << 24)) {
        int f;
        do { f = b->flags; if ((f & 0xfffe) == 0xfffe) return b; }
        while (!__sync_bool_compare_and_swap(&b->flags, f, f + 2));
        return b;
    }
    hostbd_t *d = b->desc;
    hostblk_t *n = malloc(d->size);
    memcpy(n, b, d->size);
    n->isa = &_NSConcreteMallocBlock;
    n->flags = (n->flags & ~0xffff) | (1 << 24) | 2;
    if (b->flags & (1 << 25)) d->copy(n, b);
    return n;
}
void _Block_release(const void *arg) {
    hostblk_t *b = (hostblk_t *)arg;
    if (!b || (b->flags & (1 << 28)) || !(b->flags & (1 << 24))) return;
    int f;
    do {
        f = b->flags;
        if ((f & 0xfffe) == 0xfffe || (f & 0xfffe) == 0) return;
    } while (!__sync_bool_compare_and_swap(&b->flags, f, f - 2));
    if (((f - 2) & 0xfffe) == 0) {
        if (b->flags & (1 << 25) && b->desc) ((hostbd_t *)b->desc)->dispose(b);
        free(b);
    }
}
void _Block_object_assign(void *dst, const void *obj, int flags) {
    if ((flags & 0x1F) == 8 && !(flags & 0x80)) {
        hostby_t *src = (hostby_t *)obj;
        if (src->forwarding == src) {
            hostby_t *cp = malloc((size_t)src->size);
            memcpy(cp, src, (size_t)src->size);
            cp->forwarding = cp;
            src->forwarding = cp;
            *(void **)dst = cp;
        } else *(void **)dst = src->forwarding;
        return;
    }
    *(const void **)dst = obj;
}
void _Block_object_dispose(const void *obj, int flags) { (void)obj; (void)flags; }

void *dispatch_get_global_queue(long p, unsigned long f) { (void)p; (void)f; return (void *)1; }
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; if (fn) fn(ctx); }
void dispatch_sync(void *q, void (^block)(void)) { (void)q; if (block) block(); }

int gettimeofday(mac_tv_t *tv, void *tz) {
    (void)tz;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (tv) { tv->sec = (long)ts.tv_sec; tv->usec = (int)(ts.tv_nsec / 1000); }
    return 0;
}

static void block_cls(cx_class_t *c, const char *name) {
    struct rw *r = calloc(1, sizeof *r);
    r->name = name;
    r->magic = 0x52575843u;
    c->vtable = r;
    if (nclasses < 64) classes[nclasses++] = c;
}
__attribute__((constructor(101))) static void hostshim_init(void) {
    A = mmap(0, sizeof *A, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (A == MAP_FAILED) { perror("mmap"); _exit(1); }
    memset(A, 0, sizeof *A);
    A->next = 3;
    pthread_mutexattr_t ma;
    pthread_condattr_t ca;
    pthread_mutexattr_init(&ma);
    pthread_condattr_init(&ca);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&A->mu, &ma);
    pthread_cond_init(&A->cv, &ca);
    bootstrap_port = 1;
    cx_class_t *ns = objc_allocateClassPair(0, "NSObject", 0);
    class_addMethod(ns->isa, sel_registerName("alloc"), (IMP)m_alloc, "@@:");
    class_addMethod(ns, sel_registerName("init"), (IMP)m_init, "@@:");
    class_addMethod(ns, sel_registerName("dealloc"), (IMP)m_dealloc, "v@:");
    str_cls = objc_allocateClassPair(ns, "NSString", 64);
    RW(str_cls)->size = sizeof(struct sobj);
    class_addMethod(str_cls, sel_registerName("dealloc"), (IMP)m_str_dealloc, "v@:");
    class_addMethod(str_cls, sel_registerName("UTF8String"), (IMP)m_utf8, "@@:");
    class_addMethod(str_cls->isa, sel_registerName("stringWithUTF8String:"), (IMP)m_utf8, "@@:@");
    num_cls = objc_allocateClassPair(ns, "NSNumber", 64);
    RW(num_cls)->size = sizeof(struct sobj);
    class_addMethod(num_cls, sel_registerName("longLongValue"), (IMP)m_ll, "q@:");
    cx_class_t *nx = objc_allocateClassPair(ns, "NSXPCConnection", 0);
    cx_class_t *nl = objc_allocateClassPair(ns, "NSXPCListener", 0);
    RW(nx)->size = 256;
    RW(nl)->size = 256;
    block_cls(&_NSConcreteStackBlock, "__NSStackBlock__");
    block_cls(&_NSConcreteMallocBlock, "__NSMallocBlock__");
    block_cls(&_NSConcreteGlobalBlock, "__NSGlobalBlock__");
}

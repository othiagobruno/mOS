/*
 * dispatch_block_* em cima da libdispatch deste libSystem.
 * O objeto devolvido é um bloco no heap (ABI de Blocks): o invoke está no
 * lugar que dispatch_async já chama. O estado (cópia, cancelamento, espera,
 * notify) fica num registro compartilhado pelas cópias do bloco.
 * Comportamento alinhado ao libdispatch Apache-2.0: cancelado não executa o
 * bloco interno, mas a primeira invocação completa a espera e os notifies.
 */
#ifdef MOS_HOST_TEST
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <time.h>
typedef unsigned long long u64;
static void mos_yield(void) { sched_yield(); }
static u64 mos_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}
#else
#include "sys.h"
void *malloc(size_t n);
void free(void *p);
static void mos_yield(void) { SYS0(SYS_YIELD); }
static u64 mos_now_ns(void) { return (u64)SYS0(SYS_TIME) * 1000000ull; }
#endif

void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async(void *q, void *block);

#define DB_BARRIER 0x1u
#define DB_DETACHED 0x2u
#define DB_ASSIGN_CURRENT 0x4u
#define DB_NO_QOS 0x8u
#define DB_INHERIT_QOS 0x10u
#define DB_ENFORCE_QOS 0x20u
#define DB_PUBLIC_FLAGS (DB_BARRIER | DB_DETACHED | DB_ASSIGN_CURRENT | DB_NO_QOS | DB_INHERIT_QOS | DB_ENFORCE_QOS)
#define DB_TIME_FOREVER (~0ull)

typedef struct db_note {
    void *q;
    void *block;
    struct db_note *next;
} db_note_t;

typedef struct mos_dbpriv {
    volatile int ref;
    unsigned dbflags;
    int qos;
    int rel;
    void *inner;
    volatile int cancelled;
    volatile int performed;
    volatile int lock;
    db_note_t *notes;
} mos_dbpriv_t;

typedef struct {
    unsigned long long reserved;
    unsigned long long size;
    void (*copy)(void *, const void *);
    void (*dispose)(const void *);
} mos_bdesc_t;

typedef struct mos_db {
    void *isa;
    int flags;
    int reserved;
    void (*invoke)(void *);
    mos_bdesc_t *desc;
    mos_dbpriv_t *priv;
} mos_db_t;

static void db_lock(mos_dbpriv_t *p) { while (__sync_lock_test_and_set(&p->lock, 1)) mos_yield(); }
static void db_unlock(mos_dbpriv_t *p) { __sync_lock_release(&p->lock); }

static void db_dispose(const void *obj) {
    mos_db_t *b = (mos_db_t *)obj;
    mos_dbpriv_t *p = b ? b->priv : 0;
    if (!p) return;
    if (__sync_sub_and_fetch(&p->ref, 1) != 0) return;
    if (p->inner) _Block_release(p->inner);
    db_note_t *n = p->notes;
    while (n) {
        db_note_t *nx = n->next;
        if (n->block) _Block_release(n->block);
        free(n);
        n = nx;
    }
    free(p);
}

static void db_copy(void *dst, const void *src) {
    mos_db_t *d = dst;
    mos_db_t *s = (mos_db_t *)src;
    d->priv = s->priv;
    if (d->priv) __sync_fetch_and_add(&d->priv->ref, 1);
}

static mos_bdesc_t db_desc = {0, sizeof(mos_db_t), db_copy, db_dispose};

static int flags_ok(unsigned flags) { return (flags & ~DB_PUBLIC_FLAGS) == 0; }

static int qos_ok(unsigned qos, int rel) {
    switch (qos) {
    case 0x21u: case 0x19u: case 0x15u: case 0x11u: case 0x09u: case 0u:
        break;
    default:
        return 0;
    }
    return rel <= 0 && rel >= -15;
}

static void run_inner(void *block) {
    typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); } hdr_t;
    hdr_t *h = block;
    if (h && h->invoke) h->invoke(block);
}

static void fire_notes(mos_dbpriv_t *p) {
    db_lock(p);
    db_note_t *n = p->notes;
    p->notes = 0;
    db_unlock(p);
    while (n) {
        db_note_t *nx = n->next;
        if (n->block) {
            dispatch_async(n->q, n->block);
            _Block_release(n->block);
        }
        free(n);
        n = nx;
    }
}

static void db_invoke(void *obj) {
    mos_db_t *b = obj;
    mos_dbpriv_t *p = b ? b->priv : 0;
    if (!p) return;
    if ((__atomic_load_n(&p->cancelled, __ATOMIC_ACQUIRE) & 1) == 0 && p->inner)
        run_inner(p->inner);
    if (__sync_fetch_and_add(&p->performed, 1) == 0) fire_notes(p);
}

static void *make_db(unsigned flags, int qos, int rel, void *block) {
    if (!flags_ok(flags) || !block) return 0;
    if (flags & DB_ENFORCE_QOS) flags &= ~DB_INHERIT_QOS;
    mos_dbpriv_t *p = malloc(sizeof *p);
    mos_db_t *b = malloc(sizeof *b);
    if (!p || !b) {
        free(p);
        free(b);
        return 0;
    }
    p->ref = 1;
    p->dbflags = flags;
    p->qos = qos;
    p->rel = rel;
    p->inner = _Block_copy(block);
    p->cancelled = 0;
    p->performed = 0;
    p->lock = 0;
    p->notes = 0;
    if (!p->inner) {
        free(p);
        free(b);
        return 0;
    }
    b->isa = 0;
    /* NEEDS_FREE | HAS_COPY_DISPOSE | uma referência (o par 2 do runtime de Blocks) */
    b->flags = (1 << 24) | (1 << 25) | 2;
    b->reserved = 0;
    b->invoke = db_invoke;
    b->desc = &db_desc;
    b->priv = p;
    return b;
}

void *dispatch_block_create(unsigned long flags, void *block) {
    return make_db((unsigned)flags, 0, 0, block);
}

void *dispatch_block_create_with_qos_class(unsigned long flags, unsigned qos, int rel, void *block) {
    if (!qos_ok(qos, rel)) return 0;
    if (qos == 0) flags |= DB_NO_QOS;
    return make_db((unsigned)flags, (int)qos, rel, block);
}

void dispatch_block_perform(unsigned long flags, void *block) {
    void *b = dispatch_block_create(flags, block);
    if (!b) return;
    db_invoke(b);
    _Block_release(b);
}

void dispatch_block_cancel(void *block) {
    mos_db_t *b = block;
    if (!b || !b->priv || b->invoke != db_invoke) return;
    __atomic_fetch_or(&b->priv->cancelled, 1, __ATOMIC_RELEASE);
}

long dispatch_block_testcancel(void *block) {
    mos_db_t *b = block;
    if (!b || !b->priv || b->invoke != db_invoke) return 0;
    return (__atomic_load_n(&b->priv->cancelled, __ATOMIC_ACQUIRE) & 1) ? 1 : 0;
}

long dispatch_block_wait(void *block, unsigned long long timeout) {
    mos_db_t *b = block;
    if (!b || !b->priv || b->invoke != db_invoke) return 0;
    if (__atomic_load_n(&b->priv->performed, __ATOMIC_ACQUIRE) > 0) return 0;
    if (timeout == 0) return 1;
    if (timeout == DB_TIME_FOREVER) {
        while (__atomic_load_n(&b->priv->performed, __ATOMIC_ACQUIRE) == 0) mos_yield();
        return 0;
    }
    while (__atomic_load_n(&b->priv->performed, __ATOMIC_ACQUIRE) == 0) {
        if (mos_now_ns() >= timeout) return 1;
        mos_yield();
    }
    return 0;
}

void dispatch_block_notify(void *block, void *queue, void *notification) {
    mos_db_t *b = block;
    if (!b || !b->priv || b->invoke != db_invoke || !notification) return;
    if (__atomic_load_n(&b->priv->performed, __ATOMIC_ACQUIRE) > 0) {
        dispatch_async(queue, notification);
        return;
    }
    db_note_t *n = malloc(sizeof *n);
    if (!n) return;
    n->q = queue;
    n->block = _Block_copy(notification);
    n->next = 0;
    mos_dbpriv_t *p = b->priv;
    db_lock(p);
    if (__atomic_load_n(&p->performed, __ATOMIC_ACQUIRE) > 0) {
        db_unlock(p);
        if (n->block) {
            dispatch_async(queue, n->block);
            _Block_release(n->block);
        }
        free(n);
        return;
    }
    n->next = p->notes;
    p->notes = n;
    db_unlock(p);
}

/* Contagem de referência de objeto Swift.
 * O campo de 64 bits segue o InlineRefCounts do runtime da Apple:
 *   bit 0        pure Swift (a liberação é desta rotina)
 *   bits 1..31   unowned extra (0 = só a referência implícita da alocação)
 *   bit 32       deiniting
 *   bits 33..62  strong extra (0 = uma referência forte)
 *   bit 63       há side table
 * Com o bit 63, os bits 0..61 são o ponteiro da side table (alinhado a 8)
 * e o bit 62 é a marca. Imortal: os 32 bits de baixo todos em 1 (o valor
 * usado pelo runtime é ~0). A side table repete esses bits e guarda o weak,
 * que começa em 1 em nome do unowned. Weak aponta para a side table.
 * Strong em zero chama o destrutor no primeiro palavra do metadata.
 * Unowned extra em zero, já em deinit, libera a memória.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Sem stdatomic.h: o stdint mínimo do libSystem não tem os tipos least/fast.
 * __atomic_* é builtin do clang e do gcc. */
#define rc_load_atomic(p) \
    ({ __typeof__(*(p)) _v; __atomic_load((p), &_v, __ATOMIC_SEQ_CST); _v; })
#define rc_store_atomic(p, v) \
    do { __typeof__(*(p)) _v = (v); __atomic_store((p), &_v, __ATOMIC_SEQ_CST); } while (0)
#define rc_cas_atomic(p, exp, des) \
    ({ __typeof__(*(p)) _d = (des); \
       __atomic_compare_exchange((p), (exp), &_d, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); })
#define rc_add_atomic(p, v) __atomic_fetch_add((p), (v), __ATOMIC_SEQ_CST)
#define rc_sub_atomic(p, v) __atomic_fetch_sub((p), (v), __ATOMIC_SEQ_CST)
#define rc_xchg_atomic(p, v) __atomic_exchange_n((p), (v), __ATOMIC_SEQ_CST)

int posix_memalign(void **out, size_t align, size_t n);

#define RC_PURE       (1ull << 0)
#define RC_UNOWNED_SH 1
#define RC_DEINIT     (1ull << 32)
#define RC_STRONG_SH  33
#define RC_STRONG_MAX ((1ull << 30) - 1ull)
#define RC_MARK       (1ull << 62)
#define RC_SLOW       (1ull << 63)
#define RC_IMMORTAL   0xffffffffull

typedef struct mos_side mos_side_t;
typedef struct mos_ho {
    const void *metadata;
    uint64_t bits;
} mos_ho_t;

struct mos_side {
    uintptr_t object;
    uint64_t bits;
    uint32_t weak;
};

typedef struct {
    mos_side_t *side;
} mos_weak_t;

static uint64_t mos_rc_frees;

uint64_t mos_swift_rc_frees(void) { return rc_load_atomic(&mos_rc_frees); }

void mos_swift_rc_make_immortal(void *object) {
    mos_ho_t *o = object;
    if (!o) return;
    rc_store_atomic(&o->bits, ~0ull);
}

static int rc_immortal(uint64_t b) { return (b & RC_IMMORTAL) == RC_IMMORTAL; }
static int rc_slow(uint64_t b) { return (b & RC_SLOW) != 0 && !rc_immortal(b); }
static uint32_t rc_strong(uint64_t b) { return (uint32_t)((b >> RC_STRONG_SH) & RC_STRONG_MAX); }
static uint32_t rc_unowned(uint64_t b) { return (uint32_t)((b >> RC_UNOWNED_SH) & ((1u << 31) - 1u)); }

static mos_side_t *rc_side(uint64_t b) {
    return (mos_side_t *)(uintptr_t)(b & ~(RC_SLOW | RC_MARK));
}

static uint64_t rc_encode_side(mos_side_t *s) {
    return (uint64_t)(uintptr_t)s | RC_SLOW | RC_MARK;
}

static uint64_t rc_load(uint64_t *p, int atom) {
    if (atom) return rc_load_atomic(p);
    return *(volatile uint64_t *)p;
}

static int rc_cas(uint64_t *p, uint64_t *old, uint64_t neu, int atom) {
    if (!atom) {
        volatile uint64_t *v = (volatile uint64_t *)p;
        if (*v != *old) { *old = *v; return 0; }
        *v = neu;
        return 1;
    }
    return rc_cas_atomic(p, old, neu);
}

static uint64_t *rc_word(mos_ho_t *o, int atom, uint64_t *snap) {
    uint64_t b = rc_load(&o->bits, atom);
    if (rc_slow(b)) {
        mos_side_t *s = rc_side(b);
        if (!s) { *snap = b; return &o->bits; }
        *snap = rc_load(&s->bits, atom);
        return &s->bits;
    }
    *snap = b;
    return &o->bits;
}

static void rc_call_destroy(mos_ho_t *o) {
    const void *md = o->metadata;
    if (!md) return;
    void (*fn)(void *) = *(void (**)(void *))md;
    if (fn) fn(o);
}

static void rc_free_object(mos_ho_t *o, int atom) {
    uint64_t b = rc_load(&o->bits, atom);
    if (rc_slow(b)) {
        mos_side_t *s = rc_side(b);
        if (!s) return;
        uintptr_t prev = rc_xchg_atomic(&s->object, 0);
        if (!prev) return;
        uint32_t w = rc_sub_atomic(&s->weak, 1);
        rc_add_atomic(&mos_rc_frees, 1);
        free(o);
        if (w == 1) free(s);
        return;
    }
    uint64_t old = b;
    if (!rc_cas(&o->bits, &old, RC_SLOW, atom)) return;
    rc_add_atomic(&mos_rc_frees, 1);
    free(o);
}

static void *rc_retain(void *object, uint32_t n, int atom) {
    mos_ho_t *o = object;
    if (!o || n == 0) return object;
    for (;;) {
        uint64_t snap;
        uint64_t *w = rc_word(o, atom, &snap);
        if (rc_immortal(snap)) return object;
        if (snap & RC_DEINIT) return object;
        if ((uint64_t)rc_strong(snap) + n > RC_STRONG_MAX) abort();
        uint64_t neu = snap + ((uint64_t)n << RC_STRONG_SH);
        if (rc_cas(w, &snap, neu, atom)) return object;
    }
}

static void rc_release(void *object, uint32_t n, int atom) {
    mos_ho_t *o = object;
    if (!o || n == 0) return;
    for (;;) {
        uint64_t snap;
        uint64_t *w = rc_word(o, atom, &snap);
        if (rc_immortal(snap)) return;
        if (snap & RC_DEINIT) return;
        uint32_t extra = rc_strong(snap);
        if (extra >= n) {
            if (rc_cas(w, &snap, snap - ((uint64_t)n << RC_STRONG_SH), atom)) return;
            continue;
        }
        if (extra + 1 != n) abort();
        uint64_t neu = (snap - ((uint64_t)extra << RC_STRONG_SH)) | RC_DEINIT;
        if (!rc_cas(w, &snap, neu, atom)) continue;
        rc_call_destroy(o);
        return;
    }
}

static void *rc_try(void *object, int atom) {
    mos_ho_t *o = object;
    if (!o) return 0;
    for (;;) {
        uint64_t snap;
        uint64_t *w = rc_word(o, atom, &snap);
        if (rc_immortal(snap)) return object;
        if (snap & RC_DEINIT) return 0;
        if (rc_strong(snap) == RC_STRONG_MAX) abort();
        if (rc_cas(w, &snap, snap + (1ull << RC_STRONG_SH), atom)) return object;
    }
}

static int rc_is_deinit(void *object, int atom) {
    mos_ho_t *o = object;
    if (!o) return 0;
    uint64_t snap;
    rc_word(o, atom, &snap);
    if (rc_immortal(snap)) return 0;
    return (snap & RC_DEINIT) != 0;
}

static void rc_unowned_retain(void *object, uint32_t n, int atom) {
    mos_ho_t *o = object;
    if (!o || n == 0) return;
    for (;;) {
        uint64_t snap;
        uint64_t *w = rc_word(o, atom, &snap);
        if (rc_immortal(snap)) return;
        uint32_t u = rc_unowned(snap);
        if ((uint64_t)u + n >= (1u << 31)) abort();
        uint64_t neu = (snap & ~((((1ull << 31) - 1ull) << RC_UNOWNED_SH))) |
                       ((uint64_t)(u + n) << RC_UNOWNED_SH);
        if (rc_cas(w, &snap, neu, atom)) return;
    }
}

static void rc_unowned_release(void *object, uint32_t n, int atom) {
    mos_ho_t *o = object;
    if (!o || n == 0) return;
    for (;;) {
        uint64_t snap;
        uint64_t *w = rc_word(o, atom, &snap);
        if (rc_immortal(snap)) return;
        uint32_t u = rc_unowned(snap);
        if (u < n) abort();
        uint32_t left = u - n;
        uint64_t neu = (snap & ~((((1ull << 31) - 1ull) << RC_UNOWNED_SH))) |
                       ((uint64_t)left << RC_UNOWNED_SH);
        if (!rc_cas(w, &snap, neu, atom)) continue;
        if ((snap & RC_DEINIT) && left == 0) rc_free_object(o, atom);
        return;
    }
}

static void rc_dealloc(void *object, int atom) {
    mos_ho_t *o = object;
    if (!o) return;
    uint64_t snap;
    rc_word(o, atom, &snap);
    if (rc_immortal(snap)) return;
    if (rc_unowned(snap) != 0) return;
    rc_free_object(o, atom);
}

static mos_side_t *rc_ensure_side(mos_ho_t *o, int atom) {
    for (;;) {
        uint64_t old = rc_load(&o->bits, atom);
        if (rc_immortal(old)) return 0;
        if (rc_slow(old)) return rc_side(old);
        mos_side_t *s = malloc(sizeof *s);
        if (!s) abort();
        memset(s, 0, sizeof *s);
        rc_store_atomic(&s->object, (uintptr_t)o);
        rc_store_atomic(&s->bits, old);
        rc_store_atomic(&s->weak, 1);
        uint64_t neu = rc_encode_side(s);
        if (rc_cas(&o->bits, &old, neu, atom)) return s;
        free(s);
    }
}

static void rc_weak_inc(mos_side_t *s) { if (s) rc_add_atomic(&s->weak, 1); }

static void rc_weak_dec(mos_side_t *s) {
    if (!s) return;
    uint32_t w = rc_sub_atomic(&s->weak, 1);
    if (w == 1 && rc_load_atomic(&s->object) == 0) free(s);
}

static void rc_weak_init(mos_weak_t *ref, void *value, int atom) {
    if (!ref) return;
    ref->side = 0;
    if (!value) return;
    mos_ho_t *o = value;
    uint64_t b = rc_load(&o->bits, atom);
    if (rc_immortal(b)) {
        ref->side = (mos_side_t *)((uintptr_t)value | 1u);
        return;
    }
    if (b & RC_DEINIT) return;
    mos_side_t *s = rc_ensure_side(o, atom);
    if (!s) return;
    uint64_t now;
    rc_word(o, atom, &now);
    if (now & RC_DEINIT) return;
    rc_weak_inc(s);
    ref->side = s;
}

static void rc_weak_destroy(mos_weak_t *ref) {
    if (!ref || !ref->side) return;
    if ((uintptr_t)ref->side & 1u) { ref->side = 0; return; }
    mos_side_t *s = ref->side;
    ref->side = 0;
    rc_weak_dec(s);
}

static void *rc_weak_load(mos_weak_t *ref, int take, int atom) {
    if (!ref || !ref->side) return 0;
    if ((uintptr_t)ref->side & 1u) {
        void *obj = (void *)((uintptr_t)ref->side & ~(uintptr_t)1);
        if (take) ref->side = 0;
        return obj;
    }
    mos_side_t *s = ref->side;
    void *obj = (void *)rc_load_atomic(&s->object);
    if (!obj) {
        if (take) rc_weak_destroy(ref);
        return 0;
    }
    void *kept = rc_try(obj, atom);
    if (take) rc_weak_destroy(ref);
    return kept;
}

static void *bridge_native(void *object) {
    return (void *)((uintptr_t)object & ~(uintptr_t)7);
}

static size_t rc_align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

void *swift_allocObject(const void *metadata, size_t size, size_t align_mask) {
    size_t align = align_mask + 1;
    if (align < sizeof(void *)) align = sizeof(void *);
    if (size < sizeof(mos_ho_t)) size = sizeof(mos_ho_t);
    size = rc_align_up(size, align);
    void *mem = 0;
    if (posix_memalign(&mem, align, size) != 0) return 0;
    memset(mem, 0, size);
    mos_ho_t *o = mem;
    o->metadata = metadata;
    rc_store_atomic(&o->bits, RC_PURE);
    return o;
}

void swift_deallocObject(void *object, size_t allocated_size, size_t allocated_align_mask) {
    (void)allocated_size;
    (void)allocated_align_mask;
    rc_dealloc(object, 1);
}

void swift_deallocUninitializedObject(void *object, size_t allocated_size, size_t allocated_align_mask) {
    mos_ho_t *o = object;
    if (!o) return;
    for (;;) {
        uint64_t snap;
        uint64_t *w = rc_word(o, 1, &snap);
        if (rc_immortal(snap)) return;
        if (snap & RC_DEINIT) break;
        if (rc_cas(w, &snap, snap | RC_DEINIT, 1)) break;
    }
    (void)allocated_size;
    (void)allocated_align_mask;
    rc_dealloc(object, 1);
}

void *swift_initStackObject(const void *metadata, void *object) {
    mos_ho_t *o = object;
    if (!o) return 0;
    o->metadata = metadata;
    /* Unowned alto, fora do padrão imortal, para o dealloc não devolver a pilha. */
    uint64_t unowned = 0x7ffffffeull;
    rc_store_atomic(&o->bits, RC_PURE | (unowned << RC_UNOWNED_SH));
    return o;
}

void *swift_retain(void *object) { return rc_retain(object, 1, 1); }
void *swift_retain_n(void *object, uint32_t n) { return rc_retain(object, n, 1); }
void swift_release(void *object) { rc_release(object, 1, 1); }
void swift_release_n(void *object, uint32_t n) { rc_release(object, n, 1); }
void *swift_tryRetain(void *object) { return rc_try(object, 1); }
int swift_isDeallocating(void *object) { return rc_is_deinit(object, 1); }

void *swift_unknownObjectRetain(void *object) {
    if (!object || ((uintptr_t)object & 7)) return object;
    return swift_retain(object);
}
void *swift_unknownObjectRetain_n(void *object, uint32_t n) {
    if (!object || ((uintptr_t)object & 7)) return object;
    return swift_retain_n(object, n);
}
void swift_unknownObjectRelease(void *object) {
    if (!object || ((uintptr_t)object & 7)) return;
    swift_release(object);
}
void swift_unknownObjectRelease_n(void *object, uint32_t n) {
    if (!object || ((uintptr_t)object & 7)) return;
    swift_release_n(object, n);
}

void *swift_bridgeObjectRetain(void *object) {
    void *n = bridge_native(object);
    if (n) swift_retain(n);
    return object;
}
void *swift_bridgeObjectRetain_n(void *object, uint32_t n) {
    void *p = bridge_native(object);
    if (p) swift_retain_n(p, n);
    return object;
}
void swift_bridgeObjectRelease(void *object) {
    void *n = bridge_native(object);
    if (n) swift_release(n);
}
void swift_bridgeObjectRelease_n(void *object, uint32_t n) {
    void *p = bridge_native(object);
    if (p) swift_release_n(p, n);
}

void *swift_nonatomic_retain(void *object) { return rc_retain(object, 1, 0); }
void *swift_nonatomic_retain_n(void *object, uint32_t n) { return rc_retain(object, n, 0); }
void swift_nonatomic_release(void *object) { rc_release(object, 1, 0); }
void swift_nonatomic_release_n(void *object, uint32_t n) { rc_release(object, n, 0); }

void *swift_nonatomic_unknownObjectRetain(void *object) {
    if (!object || ((uintptr_t)object & 7)) return object;
    return swift_nonatomic_retain(object);
}
void *swift_nonatomic_unknownObjectRetain_n(void *object, uint32_t n) {
    if (!object || ((uintptr_t)object & 7)) return object;
    return swift_nonatomic_retain_n(object, n);
}
void swift_nonatomic_unknownObjectRelease(void *object) {
    if (!object || ((uintptr_t)object & 7)) return;
    swift_nonatomic_release(object);
}
void swift_nonatomic_unknownObjectRelease_n(void *object, uint32_t n) {
    if (!object || ((uintptr_t)object & 7)) return;
    swift_nonatomic_release_n(object, n);
}

void *swift_nonatomic_bridgeObjectRetain(void *object) {
    void *n = bridge_native(object);
    if (n) swift_nonatomic_retain(n);
    return object;
}
void *swift_nonatomic_bridgeObjectRetain_n(void *object, uint32_t n) {
    void *p = bridge_native(object);
    if (p) swift_nonatomic_retain_n(p, n);
    return object;
}
void swift_nonatomic_bridgeObjectRelease(void *object) {
    void *n = bridge_native(object);
    if (n) swift_nonatomic_release(n);
}
void swift_nonatomic_bridgeObjectRelease_n(void *object, uint32_t n) {
    void *p = bridge_native(object);
    if (p) swift_nonatomic_release_n(p, n);
}

void swift_weakInit(void *ref, void *value) { rc_weak_init(ref, value, 1); }
void swift_weakAssign(void *ref, void *value) {
    mos_weak_t *r = ref;
    if (!r) return;
    mos_weak_t neu;
    rc_weak_init(&neu, value, 1);
    rc_weak_destroy(r);
    r->side = neu.side;
}
void *swift_weakLoadStrong(void *ref) { return rc_weak_load(ref, 0, 1); }
void *swift_weakTakeStrong(void *ref) { return rc_weak_load(ref, 1, 1); }
void swift_weakDestroy(void *ref) { rc_weak_destroy(ref); }
void swift_weakCopyInit(void *dest, void *src) {
    mos_weak_t *d = dest;
    mos_weak_t *s = src;
    if (!d) return;
    d->side = s ? s->side : 0;
    if (d->side && !((uintptr_t)d->side & 1u)) rc_weak_inc(d->side);
}
void swift_weakCopyAssign(void *dest, void *src) {
    mos_weak_t tmp;
    swift_weakCopyInit(&tmp, src);
    rc_weak_destroy(dest);
    ((mos_weak_t *)dest)->side = tmp.side;
}
void swift_weakTakeInit(void *dest, void *src) {
    mos_weak_t *d = dest;
    mos_weak_t *s = src;
    if (!d) return;
    d->side = s ? s->side : 0;
    if (s) s->side = 0;
}
void swift_weakTakeAssign(void *dest, void *src) {
    mos_weak_t tmp;
    swift_weakTakeInit(&tmp, src);
    rc_weak_destroy(dest);
    ((mos_weak_t *)dest)->side = tmp.side;
}

void swift_unownedRetain(void *object) { rc_unowned_retain(object, 1, 1); }
void swift_unownedRelease(void *object) { rc_unowned_release(object, 1, 1); }

void *swift_unownedRetainStrong(void *object) {
    if (rc_is_deinit(object, 1)) abort();
    return swift_retain(object);
}

void swift_unownedCheck(void *object) {
    if (rc_is_deinit(object, 1)) abort();
}

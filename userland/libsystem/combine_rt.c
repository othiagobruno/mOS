/*
 * Combine em C, com o comportamento que o teste de host cobre.
 * Não exporta alias mangled: um símbolo que só faz ret não é Publisher.
 * O app Swift ainda não chama estas funções; a ABI dos binários não está ligada aqui.
 */
#ifdef CX_HOST
#include <stddef.h>
#include <stdlib.h>
#else
#include "sys.h"
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
#endif

#define CX_UNLIMITED (-1L)
#define CX_Q 16

typedef struct cx_sub cx_sub;
typedef struct cx_pub cx_pub;
typedef struct cx_can cx_can;
typedef struct cx_set cx_set;
typedef struct cx_pair cx_pair;

struct cx_pair { long a, b; };

struct cx_sub {
    cx_pub *pub;
    void *ctx;
    void (*got_sub)(void *ctx, cx_sub *s);
    long (*got)(void *ctx, long v);
    void (*done)(void *ctx, int err);
    long demand;
    int dead;
    cx_sub *next;
};

struct cx_can {
    cx_sub *sub;
    int dead;
};

struct cx_set {
    cx_can **items;
    int n, cap;
};

enum {
    K_SUBJ = 1, K_CUR, K_MAP, K_FILT, K_DEDUP, K_DEB, K_RECV, K_CL,
    K_DROP, K_FIRST, K_CMAP, K_MERGE, K_ZIP, K_SCAN, K_THR, K_JUST,
    K_EMPTY, K_FAIL, K_FLAT, K_HAND, K_CATCH, K_RETRY, K_DELAY, K_BUF
};

struct cx_pub {
    int kind;
    cx_pub *a, *b, *inner, *repl;
    cx_sub *subs, *up_sub, *up_sub2, *inner_sub;
    long v, last, acc, drop_left, buf_n, buf_need, retry_left;
    int has, has_last, started, failed, connected, thr_open, err;
    long (*mapfn)(long, void *);
    int (*pred)(long, void *);
    int (*cmap)(long, long *, void *);
    long (*scanfn)(long, long, void *);
    cx_pub *(*flatfn)(long, void *);
    void (*hop)(void (*fn)(void *), void *);
    void (*hv)(long, void *);
    void (*hd)(int, void *);
    void *ctx;
    cx_pair pair;
    long qa[CX_Q], qb[CX_Q], qd[CX_Q];
    int na, nb, nd;
};

static void deliver(cx_pub *p, long v);
static void finish(cx_pub *p, int err);
static void wire(cx_pub *p);
static void recv_run(void *boxv);
static void want_all(void *ctx, cx_sub *s);
static long fwd_inner(void *ctx, long v);
cx_sub *cx_subscribe(cx_pub *p, void (*got_sub)(void *, cx_sub *), void *ctx,
                     long (*got)(void *, long), void (*done)(void *, int));

static cx_pub *novo(int kind) {
    cx_pub *p = (cx_pub *)calloc(1, sizeof *p);
    if (p) p->kind = kind;
    return p;
}

static void pushq(long *q, int *n, long v) {
    if (*n < CX_Q) q[(*n)++] = v;
}

static long popq(long *q, int *n) {
    long v = q[0];
    int i;
    for (i = 1; i < *n; i++) q[i - 1] = q[i];
    (*n)--;
    return v;
}

void cx_request(cx_sub *s, long d) {
    if (!s || s->dead) return;
    if (d < 0) s->demand = CX_UNLIMITED;
    else if (s->demand != CX_UNLIMITED) s->demand += d;
}

static int aceita(cx_sub *s) {
    if (!s || s->dead) return 0;
    if (s->demand == 0) return 0;
    if (s->demand > 0) s->demand--;
    return 1;
}

static void deliver(cx_pub *p, long v) {
    cx_sub *s;
    if (!p) return;
    for (s = p->subs; s; s = s->next) {
        long extra;
        if (!aceita(s) || !s->got) continue;
        extra = s->got(s->ctx, v);
        if (extra < 0) s->demand = CX_UNLIMITED;
        else if (extra > 0 && s->demand != CX_UNLIMITED) s->demand += extra;
    }
}

static void finish(cx_pub *p, int err) {
    cx_sub *s;
    if (!p) return;
    for (s = p->subs; s; s = s->next) {
        if (s->dead) continue;
        s->dead = 1;
        if (s->done) s->done(s->ctx, err);
    }
}

static void on_up(cx_pub *p, long v) {
    long out;
    if (!p) return;
    if (p->hv) p->hv(v, p->ctx);
    switch (p->kind) {
    case K_MAP:
        if (p->mapfn) deliver(p, p->mapfn(v, p->ctx));
        break;
    case K_FILT:
        if (!p->pred || p->pred(v, p->ctx)) deliver(p, v);
        break;
    case K_DEDUP:
        if (p->has_last && p->last == v) break;
        p->has_last = 1;
        p->last = v;
        deliver(p, v);
        break;
    case K_DEB:
        p->v = v;
        p->has = 1;
        break;
    case K_DELAY:
        pushq(p->qd, &p->nd, v);
        break;
    case K_RECV:
        if (!p->hop) deliver(p, v);
        else {
            cx_pub **box = (cx_pub **)malloc(sizeof(cx_pub *) + sizeof(long));
            long *slot;
            if (!box) break;
            box[0] = p;
            slot = (long *)(box + 1);
            *slot = v;
            p->hop(recv_run, box);
        }
        break;
    case K_CL:
        break;
    case K_DROP:
        if (p->drop_left > 0) { p->drop_left--; break; }
        deliver(p, v);
        break;
    case K_FIRST:
        deliver(p, v);
        finish(p, 0);
        if (p->up_sub) p->up_sub->dead = 1;
        break;
    case K_CMAP:
        if (p->cmap && p->cmap(v, &out, p->ctx)) deliver(p, out);
        break;
    case K_MERGE:
        deliver(p, v);
        break;
    case K_SCAN:
        if (!p->started) { p->acc = 0; p->started = 1; }
        if (p->scanfn) p->acc = p->scanfn(p->acc, v, p->ctx);
        deliver(p, p->acc);
        break;
    case K_THR:
        if (!p->thr_open) break;
        p->thr_open = 0;
        deliver(p, v);
        break;
    case K_FLAT: {
        cx_pub *in;
        if (p->inner_sub) p->inner_sub->dead = 1;
        in = p->flatfn ? p->flatfn(v, p->ctx) : 0;
        p->inner = in;
        if (in) p->inner_sub = cx_subscribe(in, want_all, p, fwd_inner, 0);
        break;
    }
    case K_BUF:
        p->buf_n++;
        p->v = v;
        if (p->buf_n >= p->buf_need && p->buf_need > 0) {
            deliver(p, p->v);
            p->buf_n = 0;
        }
        break;
    default:
        deliver(p, v);
        break;
    }
}

/* receive(on:) guarda o valor ao lado do ponteiro do publisher */
static void recv_run(void *boxv) {
    cx_pub **box = (cx_pub **)boxv;
    long *slot = (long *)(box + 1);
    deliver(box[0], *slot);
    free(box);
}

static void on_left(cx_pub *p, long v) {
    if (p->kind == K_CL) {
        p->pair.a = v;
        p->has = 1;
        if (p->has && p->has_last) deliver(p, p->pair.a);
        return;
    }
    if (p->kind == K_ZIP) {
        pushq(p->qa, &p->na, v);
        while (p->na && p->nb) {
            p->pair.a = popq(p->qa, &p->na);
            p->pair.b = popq(p->qb, &p->nb);
            deliver(p, p->pair.a);
        }
        return;
    }
    on_up(p, v);
}

static void on_right(cx_pub *p, long v) {
    if (p->kind == K_CL) {
        p->pair.b = v;
        p->has_last = 1;
        if (p->has && p->has_last) deliver(p, p->pair.a);
        return;
    }
    if (p->kind == K_ZIP) {
        pushq(p->qb, &p->nb, v);
        while (p->na && p->nb) {
            p->pair.a = popq(p->qa, &p->na);
            p->pair.b = popq(p->qb, &p->nb);
            deliver(p, p->pair.a);
        }
        return;
    }
    on_up(p, v);
}

static long fwd_left(void *ctx, long v) { on_left((cx_pub *)ctx, v); return 0; }
static long fwd_right(void *ctx, long v) { on_right((cx_pub *)ctx, v); return 0; }
static long fwd_inner(void *ctx, long v) { deliver((cx_pub *)ctx, v); return 0; }
static void want_all(void *ctx, cx_sub *s) { (void)ctx; cx_request(s, CX_UNLIMITED); }

static void up_done(void *ctx, int err) {
    cx_pub *p = (cx_pub *)ctx;
    if (!p) return;
    if (p->hd) p->hd(err, p->ctx);
    if (err && p->kind == K_RETRY && p->retry_left > 0) {
        p->retry_left--;
        p->connected = 0;
        wire(p);
        return;
    }
    if (err && p->kind == K_CATCH && p->repl) {
        p->kind = K_MERGE;
        p->a = p->repl;
        p->connected = 0;
        wire(p);
        return;
    }
    finish(p, err);
}

cx_sub *cx_subscribe(cx_pub *p, void (*got_sub)(void *, cx_sub *), void *ctx,
                     long (*got)(void *, long), void (*done)(void *, int)) {
    cx_sub *s;
    if (!p) return 0;
    s = (cx_sub *)calloc(1, sizeof *s);
    if (!s) return 0;
    s->pub = p;
    s->ctx = ctx;
    s->got_sub = got_sub;
    s->got = got;
    s->done = done;
    s->next = p->subs;
    p->subs = s;
    if (got_sub) got_sub(ctx, s);
    if (p->kind == K_CUR && p->has) deliver(p, p->v);
    if (p->kind == K_JUST) { deliver(p, p->v); finish(p, 0); return s; }
    if (p->kind == K_EMPTY) { finish(p, 0); return s; }
    if (p->kind == K_FAIL) { finish(p, p->err ? p->err : 1); return s; }
    if (!p->connected && p->a) wire(p);
    return s;
}

static void wire(cx_pub *p) {
    p->connected = 1;
    if (p->kind == K_FLAT) {
        /* liga quando chega valor */
        if (p->a) p->up_sub = cx_subscribe(p->a, want_all, p, fwd_left, up_done);
        return;
    }
    if (p->a) p->up_sub = cx_subscribe(p->a, want_all, p, fwd_left, up_done);
    if (p->b) p->up_sub2 = cx_subscribe(p->b, want_all, p, fwd_right, 0);
}

void cx_send(cx_pub *p, long v) {
    if (!p) return;
    if (p->kind == K_CUR) { p->v = v; p->has = 1; }
    deliver(p, v);
}

void cx_complete(cx_pub *p) { finish(p, 0); }
void cx_fail(cx_pub *p, int err) { finish(p, err ? err : 1); }

cx_pub *cx_passthrough(void) { return novo(K_SUBJ); }

cx_pub *cx_current(long v) {
    cx_pub *p = novo(K_CUR);
    if (!p) return 0;
    p->v = v;
    p->has = 1;
    return p;
}

long cx_value(cx_pub *p) { return p ? p->v : 0; }

void cx_cancel_sub(cx_sub *s) {
    if (!s) return;
    s->dead = 1;
    s->demand = 0;
}

void cx_cancel(cx_can *c) {
    if (!c || c->dead) return;
    c->dead = 1;
    cx_cancel_sub(c->sub);
}

cx_can *cx_cancellable(cx_sub *s) {
    cx_can *c = (cx_can *)calloc(1, sizeof *c);
    if (c) c->sub = s;
    return c;
}

cx_set *cx_set_new(void) { return (cx_set *)calloc(1, sizeof(cx_set)); }

int cx_store(cx_can *c, cx_set *set) {
    cx_can **n;
    if (!c || !set) return 0;
    if (set->n >= set->cap) {
        int cap = set->cap ? set->cap * 2 : 4;
        n = (cx_can **)realloc(set->items, (size_t)cap * sizeof *n);
        if (!n) return 0;
        set->items = n;
        set->cap = cap;
    }
    set->items[set->n++] = c;
    return 1;
}

void cx_set_cancel(cx_set *set) {
    int i;
    if (!set) return;
    for (i = 0; i < set->n; i++) cx_cancel(set->items[i]);
}

static cx_can *liga(cx_pub *p, long (*got)(void *, long), void *ctx) {
    cx_sub *s = cx_subscribe(p, want_all, ctx, got, 0);
    return cx_cancellable(s);
}

typedef struct { void (*fn)(long, void *); void *ctx; } sink_box;

static long sink_got(void *ctx, long v) {
    sink_box *b = (sink_box *)ctx;
    if (b->fn) b->fn(v, b->ctx);
    return 0;
}

cx_can *cx_sink(cx_pub *p, void (*fn)(long, void *), void *ctx) {
    sink_box *b = (sink_box *)calloc(1, sizeof *b);
    if (!b) return 0;
    b->fn = fn;
    b->ctx = ctx;
    return liga(p, sink_got, b);
}

typedef struct { long *slot; } asg_box;

static long asg_got(void *ctx, long v) {
    asg_box *b = (asg_box *)ctx;
    if (b->slot) *b->slot = v;
    return 0;
}

cx_can *cx_assign(cx_pub *p, long *slot) {
    asg_box *b = (asg_box *)calloc(1, sizeof *b);
    if (!b) return 0;
    b->slot = slot;
    return liga(p, asg_got, b);
}

static cx_pub *op1(int kind, cx_pub *up) {
    cx_pub *p = novo(kind);
    if (!p) return 0;
    p->a = up;
    return p;
}

cx_pub *cx_map(cx_pub *up, long (*fn)(long, void *), void *ctx) {
    cx_pub *p = op1(K_MAP, up);
    if (!p) return 0;
    p->mapfn = fn;
    p->ctx = ctx;
    return p;
}

cx_pub *cx_filter(cx_pub *up, int (*pred)(long, void *), void *ctx) {
    cx_pub *p = op1(K_FILT, up);
    if (!p) return 0;
    p->pred = pred;
    p->ctx = ctx;
    return p;
}

cx_pub *cx_remove_duplicates(cx_pub *up) { return op1(K_DEDUP, up); }

cx_pub *cx_debounce(cx_pub *up) { return op1(K_DEB, up); }

void cx_debounce_fire(cx_pub *p) {
    if (!p || p->kind != K_DEB || !p->has) return;
    p->has = 0;
    deliver(p, p->v);
}

cx_pub *cx_receive(cx_pub *up, void (*hop)(void (*)(void *), void *)) {
    cx_pub *p = op1(K_RECV, up);
    if (!p) return 0;
    p->hop = hop;
    return p;
}

cx_pub *cx_combine_latest(cx_pub *a, cx_pub *b) {
    cx_pub *p = op1(K_CL, a);
    if (!p) return 0;
    p->b = b;
    return p;
}

void cx_latest_pair(cx_pub *p, long *a, long *b) {
    if (!p) return;
    if (a) *a = p->pair.a;
    if (b) *b = p->pair.b;
}

cx_pub *cx_drop(cx_pub *up, long n) {
    cx_pub *p = op1(K_DROP, up);
    if (p) p->drop_left = n;
    return p;
}

cx_pub *cx_first(cx_pub *up) { return op1(K_FIRST, up); }

cx_pub *cx_compact_map(cx_pub *up, int (*fn)(long, long *, void *), void *ctx) {
    cx_pub *p = op1(K_CMAP, up);
    if (!p) return 0;
    p->cmap = fn;
    p->ctx = ctx;
    return p;
}

cx_pub *cx_merge(cx_pub *a, cx_pub *b) {
    cx_pub *p = op1(K_MERGE, a);
    if (p) p->b = b;
    return p;
}

cx_pub *cx_zip(cx_pub *a, cx_pub *b) {
    cx_pub *p = op1(K_ZIP, a);
    if (p) p->b = b;
    return p;
}

cx_pub *cx_scan(cx_pub *up, long (*fn)(long, long, void *), void *ctx) {
    cx_pub *p = op1(K_SCAN, up);
    if (!p) return 0;
    p->scanfn = fn;
    p->ctx = ctx;
    return p;
}

cx_pub *cx_throttle(cx_pub *up) {
    cx_pub *p = op1(K_THR, up);
    if (p) p->thr_open = 1;
    return p;
}

void cx_throttle_open(cx_pub *p) { if (p) p->thr_open = 1; }

cx_pub *cx_just(long v) {
    cx_pub *p = novo(K_JUST);
    if (p) p->v = v;
    return p;
}

cx_pub *cx_empty(void) { return novo(K_EMPTY); }

cx_pub *cx_fail_pub(int err) {
    cx_pub *p = novo(K_FAIL);
    if (p) p->err = err;
    return p;
}

cx_pub *cx_flat_map(cx_pub *up, cx_pub *(*fn)(long, void *), void *ctx) {
    cx_pub *p = op1(K_FLAT, up);
    if (!p) return 0;
    p->flatfn = fn;
    p->ctx = ctx;
    return p;
}

cx_pub *cx_handle(cx_pub *up, void (*hv)(long, void *), void (*hd)(int, void *), void *ctx) {
    cx_pub *p = op1(K_MERGE, up);
    if (!p) return 0;
    p->kind = K_HAND;
    p->hv = hv;
    p->hd = hd;
    p->ctx = ctx;
    return p;
}

cx_pub *cx_catch(cx_pub *up, cx_pub *repl) {
    cx_pub *p = op1(K_CATCH, up);
    if (p) p->repl = repl;
    return p;
}

cx_pub *cx_retry(cx_pub *up, long n) {
    cx_pub *p = op1(K_RETRY, up);
    if (p) p->retry_left = n;
    return p;
}

cx_pub *cx_delay(cx_pub *up) { return op1(K_DELAY, up); }

void cx_delay_flush(cx_pub *p) {
    if (!p) return;
    while (p->nd) deliver(p, popq(p->qd, &p->nd));
}

cx_pub *cx_buffer(cx_pub *up, long n) {
    cx_pub *p = op1(K_BUF, up);
    if (p) p->buf_need = n;
    return p;
}


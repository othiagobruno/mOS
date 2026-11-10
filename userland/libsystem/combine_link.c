/*
 * Liga os $s7Combine que o Claude e o ChatGPT importam ao runtime cx_*.
 * Os metadados (Mn, Ma, Mc, Mp, WP, Tn, Tq, Tl) estão em combine_meta.c.
 */
#ifdef CX_HOST
#include <stdlib.h>
#else
#include "sys.h"
void *calloc(size_t n, size_t s);
#endif

typedef struct cx_pub cx_pub;
typedef struct cx_can cx_can;
typedef struct cx_set cx_set;
cx_pub *cx_passthrough(void);
cx_pub *cx_current(long v);
void cx_send(cx_pub *p, long v);
void cx_cancel(cx_can *c);
cx_can *cx_cancellable(void *sub);
int cx_store(cx_can *c, cx_set *set);
cx_can *cx_sink(cx_pub *p, void (*fn)(long, void *), void *ctx);
cx_pub *cx_filter(cx_pub *up, int (*pred)(long, void *), void *ctx);
cx_pub *cx_remove_duplicates(cx_pub *up);
cx_pub *cx_drop(cx_pub *up, long n);
cx_pub *cx_first(cx_pub *up);
cx_pub *cx_compact_map(cx_pub *up, int (*fn)(long, long *, void *), void *ctx);
cx_pub *cx_throttle(cx_pub *up);
cx_pub *cx_receive(cx_pub *up, void (*hop)(void (*)(void *), void *));

#define ASM(mangled) __asm__(mangled)

typedef struct { long v; cx_pub *p; } pubd_t;
typedef struct { cx_can *c; void (*fn)(void); } can_box;

static can_box boxes[32];

static void lembrar(cx_can *c, void (*fn)(void)) {
    int i;
    for (i = 0; i < 32; i++) if (!boxes[i].c) { boxes[i].c = c; boxes[i].fn = fn; return; }
}

void mos_any_cancel(cx_can *c) {
    int i;
    for (i = 0; i < 32; i++) if (boxes[i].c == c) {
        if (boxes[i].fn) boxes[i].fn();
        boxes[i].c = 0;
        break;
    }
    cx_cancel(c);
}

int mos_any_store(cx_can *c, cx_set *set) {
    return cx_store(c, set);
}

cx_can *mos_any_init(void (*fn)(void)) {
    cx_can *c = cx_cancellable(0);
    lembrar(c, fn);
    return c;
}

void mos_pass_send(cx_pub *p, long v) { cx_send(p, v); }

cx_pub *mos_pass_init(void) { return cx_passthrough(); }

pubd_t *mos_pub_init(long v) {
    pubd_t *o = (pubd_t *)calloc(1, sizeof *o);
    if (!o) return 0;
    o->v = v;
    o->p = cx_current(v);
    return o;
}

cx_pub *mos_pub_proj(pubd_t *o) {
    return o ? o->p : 0;
}

void mos_pub_proj_set(pubd_t *o, cx_pub *p) {
    if (o) o->p = p;
}

long mos_pub_get(long *slot) {
    return slot ? *slot : 0;
}

void mos_pub_set(long *slot, long v) {
    if (slot) *slot = v;
}

void mos_pub_mod(long *slot, long v) {
    if (slot) *slot = v;
}

cx_pub *mos_erase(cx_pub *p) { return p; }

cx_pub *mos_cmap(cx_pub *p, int (*fn)(long, long *, void *), void *ctx) {
    return cx_compact_map(p, fn, ctx);
}

cx_pub *mos_first(cx_pub *p) { return cx_first(p); }

cx_pub *mos_filter(cx_pub *p, int (*pred)(long, void *), void *ctx) {
    return cx_filter(p, pred, ctx);
}

cx_pub *mos_receive(cx_pub *p, void (*hop)(void (*)(void *), void *)) {
    return cx_receive(p, hop);
}

cx_pub *mos_throttle(cx_pub *p) {
    return cx_throttle(p);
}

cx_pub *mos_drop(cx_pub *p, long n) { return cx_drop(p, n); }

cx_pub *mos_dedup(cx_pub *p) {
    return cx_remove_duplicates(p);
}

cx_can *mos_sink(cx_pub *p, void (*fn)(long, void *), void *ctx) {
    return cx_sink(p, fn, ctx);
}

cx_pub *mos_owc(cx_pub **slot) {
    if (slot && !*slot) *slot = cx_passthrough();
    return slot ? *slot : 0;
}

#define EXP(mangled, src) void src##_exp(void) ASM(mangled) __attribute__((alias(#src)))
EXP("$s7Combine14AnyCancellableC6cancelyyF", mos_any_cancel);
EXP("$s7Combine14AnyCancellableC5store2inyShyACGz_tF", mos_any_store);
EXP("$s7Combine14AnyCancellableCyACyyccfc", mos_any_init);
EXP("$s7Combine18PassthroughSubjectC4sendyyxF", mos_pass_send);
EXP("$s7Combine18PassthroughSubjectCACyxq_Gycfc", mos_pass_init);
EXP("$s7Combine9PublishedV12initialValueACyxGx_tcfC", mos_pub_init);
EXP("$s7Combine9PublishedV14projectedValueAC9PublisherVyx_Gvg", mos_pub_proj);
EXP("$s7Combine9PublishedV14projectedValueAC9PublisherVyx_Gvs", mos_pub_proj_set);
EXP("$s7Combine9PublishedV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAHyqd__ACyxGGtcRld__CluigZ", mos_pub_get);
EXP("$s7Combine9PublishedV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAHyqd__ACyxGGtcRld__CluisZ", mos_pub_set);
EXP("$s7Combine9PublishedV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAHyqd__ACyxGGtcRld__CluiMZ", mos_pub_mod);
EXP("$s7Combine9PublisherPAAE010eraseToAnyB0AA0eB0Vy6OutputQz7FailureQzGyF", mos_erase);
EXP("$s7Combine9PublisherPAAE10compactMapyAA10PublishersO07CompactD0Vy_xqd__Gqd__Sg6OutputQzclF", mos_cmap);
EXP("$s7Combine9PublisherPAAE5firstAA10PublishersO5FirstVy_xGyF", mos_first);
EXP("$s7Combine9PublisherPAAE6filteryAA10PublishersO6FilterVy_xGSb6OutputQzcF", mos_filter);
EXP("$s7Combine9PublisherPAAE7receive2on7optionsAA10PublishersO9ReceiveOnVy_xqd__Gqd___16SchedulerOptionsQyd__SgtAA0I0Rd__lF", mos_receive);
EXP("$s7Combine9PublisherPAAE8throttle3for9scheduler6latestAA10PublishersO8ThrottleVy_xqd__G17SchedulerTimeType_6StrideQYd___qd__SbtAA0I0Rd__lF", mos_throttle);
EXP("$s7Combine9PublisherPAAE9dropFirstyAA10PublishersO4DropVy_xGSiF", mos_drop);
EXP("$s7Combine9PublisherPAASQ6OutputRpzrlE16removeDuplicatesAA10PublishersO06RemoveE0Vy_xGyF", mos_dedup);
EXP("$s7Combine9PublisherPAAs5NeverO7FailureRtzrlE4sink12receiveValueAA14AnyCancellableCy6OutputQzc_tF", mos_sink);
EXP("$s7Combine16ObservableObjectPA2A0bC9PublisherC0c10WillChangeD0RtzrlE06objecteF0AEvg", mos_owc);

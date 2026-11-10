/* DragGesture e os embrulhos onChanged, onEnded e updating(_:body:).
 * mos_sui_drag é a ponte do AppKit: recebe início, movimento e fim do arrasto,
 * atualiza o DragGesture.Value e chama as closures na ordem do SwiftUI.
 */
#ifdef SWIFTUI_GESTURE_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

typedef struct { double x, y; } sui_point;

/* DragGesture.Value: location, startLocation e translação. */
typedef struct { const sui_desc *isa; double x, y, sx, sy; } sui_dragval;
typedef struct { const sui_desc *isa; double min_dist; int64_t space; sui_dragval value; int active; } sui_drag;
/* Mesmo layout do GestureState de swiftui_observable.c. */
typedef struct { const sui_desc *isa; uint64_t value, initial; } sui_gstate;
/* _ChangedGesture, _EndedGesture e GestureStateGesture: base + closure. */
typedef struct { const sui_desc *isa; void *base; sui_fn fn; sui_gstate *state; } sui_gwrap;

static void *sui_gesture_body(void) { return 0; }

sui_desc $s7SwiftUI11DragGestureVMn = { SUI_K_STRUCT, "DragGesture" };
void *$s7SwiftUI11DragGestureVMa(void) { return &$s7SwiftUI11DragGestureVMn; }
sui_conf $s7SwiftUI11DragGestureVAA0D0AAMc = { (void *)sui_gesture_body, "Gesture" };
static sui_desc sui_dragval_desc = { SUI_K_STRUCT, "DragGesture.Value" };
void *$s7SwiftUI11DragGestureV5ValueVMa(void) { return &sui_dragval_desc; }
sui_conf $s7SwiftUI11DragGestureV5ValueVSQAAMc = { 0, "Equatable" };

sui_desc $s7SwiftUI15_ChangedGestureVMn = { SUI_K_STRUCT, "_ChangedGesture" };
sui_conf $s7SwiftUI15_ChangedGestureVyxGAA0D0AAMc = { (void *)sui_gesture_body, "Gesture" };
sui_desc $s7SwiftUI13_EndedGestureVMn = { SUI_K_STRUCT, "_EndedGesture" };
sui_conf $s7SwiftUI13_EndedGestureVyxGAA0D0AAMc = { (void *)sui_gesture_body, "Gesture" };
sui_desc $s7SwiftUI012GestureStateC0VMn = { SUI_K_STRUCT, "GestureStateGesture" };
sui_conf $s7SwiftUI012GestureStateC0Vyxq_GAA0C0AAMc = { (void *)sui_gesture_body, "Gesture" };

void *$s7SwiftUI11DragGestureV15minimumDistance15coordinateSpaceAC12CoreGraphics7CGFloatV_AA010CoordinateH0OtcfC(double min_dist, int64_t space) {
    sui_drag *d = (sui_drag *)sui_new(&$s7SwiftUI11DragGestureVMn, sizeof *d);
    if (!d) return 0;
    d->min_dist = min_dist < 0 ? 0 : min_dist;
    d->space = space;
    d->value.isa = &sui_dragval_desc;
    return d;
}

sui_point $s7SwiftUI11DragGestureV5ValueV8locationSo7CGPointVvg(void) {
    sui_dragval *v = (sui_dragval *)sui_self();
    sui_point p = { 0, 0 };
    if (v) { p.x = v->x; p.y = v->y; }
    return p;
}

sui_point $s7SwiftUI11DragGestureV5ValueV13startLocationSo7CGPointVvg(void) {
    sui_dragval *v = (sui_dragval *)sui_self();
    sui_point p = { 0, 0 };
    if (v) { p.x = v->sx; p.y = v->sy; }
    return p;
}

static sui_gwrap *sui_gwrap_new(const sui_desc *d, void *base, void *fn, void *ctx) {
    sui_gwrap *w = (sui_gwrap *)sui_new(d, sizeof *w);
    if (!w) return 0;
    w->base = base;
    w->fn.fn = fn;
    w->fn.ctx = ctx;
    return w;
}

void *$s7SwiftUI7GesturePAASQ5ValueRpzrlE9onChangedyAA01_fC0VyxGyAEcF(void *fn, void *ctx) {
    return sui_gwrap_new(&$s7SwiftUI15_ChangedGestureVMn, sui_self(), fn, ctx);
}

void *$s7SwiftUI7GesturePAAE7onEndedyAA01_eC0VyxGy5ValueQzcF(void *fn, void *ctx) {
    return sui_gwrap_new(&$s7SwiftUI13_EndedGestureVMn, sui_self(), fn, ctx);
}

void *$s7SwiftUI7GesturePAAE8updating_4bodyAA0c5StateC0Vyxqd__GAA0cF0Vyqd__G_y5ValueQz_qd__zAA11TransactionVztctlF(sui_gstate *state, void *fn, void *ctx) {
    sui_gwrap *w = sui_gwrap_new(&$s7SwiftUI012GestureStateC0VMn, sui_self(), fn, ctx);
    if (w) w->state = state;
    return w;
}

/* phase: 0 começo, 1 movimento, 2 fim. Devolve 1 quando o gesto está ativo. */
int mos_sui_drag(void *gesture, int phase, double x, double y) {
    void *chain[16];
    int n = 0, i;
    void *g = gesture;
    sui_drag *d = 0;
    uint64_t txn = 0;
    while (g && n < 16) {
        const sui_desc *k = sui_isa(g);
        chain[n++] = g;
        if (k == &$s7SwiftUI11DragGestureVMn) { d = (sui_drag *)g; break; }
        if (k != &$s7SwiftUI15_ChangedGestureVMn && k != &$s7SwiftUI13_EndedGestureVMn &&
            k != &$s7SwiftUI012GestureStateC0VMn) return 0;
        g = ((sui_gwrap *)g)->base;
    }
    if (!d) return 0;
    if (phase == 0) {
        d->value.sx = d->value.x = x;
        d->value.sy = d->value.y = y;
        d->active = d->min_dist <= 0;
    } else {
        double dx = x - d->value.sx, dy = y - d->value.sy;
        d->value.x = x;
        d->value.y = y;
        if (!d->active && dx * dx + dy * dy >= d->min_dist * d->min_dist) d->active = 1;
    }
    if (!d->active) return 0;
    for (i = n - 2; i >= 0; i--) {
        sui_gwrap *w = (sui_gwrap *)chain[i];
        const sui_desc *k = w->isa;
        if (phase < 2 && k == &$s7SwiftUI15_ChangedGestureVMn) sui_call1(w->fn, (uint64_t)(uintptr_t)&d->value);
        else if (phase < 2 && k == &$s7SwiftUI012GestureStateC0VMn && w->state)
            sui_call3(w->fn, (uint64_t)(uintptr_t)&d->value, (uint64_t)(uintptr_t)&w->state->value, (uint64_t)(uintptr_t)&txn);
        else if (phase == 2 && k == &$s7SwiftUI012GestureStateC0VMn && w->state) w->state->value = w->state->initial;
        else if (phase == 2 && k == &$s7SwiftUI13_EndedGestureVMn) sui_call1(w->fn, (uint64_t)(uintptr_t)&d->value);
    }
    if (phase == 2) d->active = 0;
    return 1;
}

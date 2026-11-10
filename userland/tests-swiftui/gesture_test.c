/* DragGesture com onChanged, updating(_:body:) e onEnded entregues por mos_sui_drag.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_GESTURE_HOST -DSWIFTUI_OBS_HOST \
 *   userland/libsystem/swiftui_gesture.c userland/libsystem/swiftui_observable.c \
 *   userland/tests-swiftui/gesture_test.c -o /tmp/sui_gesture
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { double x, y; } pt;
typedef struct { const sui_desc *isa; uint64_t value, initial; } gstate;

void *$s7SwiftUI11DragGestureV15minimumDistance15coordinateSpaceAC12CoreGraphics7CGFloatV_AA010CoordinateH0OtcfC(double d, int64_t space);
void *$s7SwiftUI7GesturePAASQ5ValueRpzrlE9onChangedyAA01_fC0VyxGyAEcF(void *fn, void *ctx);
void *$s7SwiftUI7GesturePAAE7onEndedyAA01_eC0VyxGy5ValueQzcF(void *fn, void *ctx);
void *$s7SwiftUI7GesturePAAE8updating_4bodyAA0c5StateC0Vyxqd__GAA0cF0Vyqd__G_y5ValueQz_qd__zAA11TransactionVztctlF(void *state, void *fn, void *ctx);
pt $s7SwiftUI11DragGestureV5ValueV8locationSo7CGPointVvg(void);
pt $s7SwiftUI11DragGestureV5ValueV13startLocationSo7CGPointVvg(void);
void *$s7SwiftUI12GestureStateV12wrappedValueACyxGx_tcfC(const uint64_t *value, void *meta);
int mos_sui_drag(void *gesture, int phase, double x, double y);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

static int changed, ended, updated;
static double last_x, start_x;
static void on_changed(void *value) {
    pt p;
    arm(value);
    p = $s7SwiftUI11DragGestureV5ValueV8locationSo7CGPointVvg();
    last_x = p.x;
    changed++;
}
static void on_ended(void *value) {
    pt p;
    arm(value);
    p = $s7SwiftUI11DragGestureV5ValueV13startLocationSo7CGPointVvg();
    start_x = p.x;
    ended++;
}
/* updating: (Value, inout State, inout Transaction); grava a coordenada no estado. */
static void on_update(void *value, uint64_t *state, void *txn) {
    pt p;
    (void)txn;
    arm(value);
    p = $s7SwiftUI11DragGestureV5ValueV8locationSo7CGPointVvg();
    *state = (uint64_t)p.x;
    updated++;
}

int main(void) {
    int fails = 0;
    uint64_t zero = 0;
    gstate *gs = $s7SwiftUI12GestureStateV12wrappedValueACyxGx_tcfC(&zero, 0);
    void *drag = $s7SwiftUI11DragGestureV15minimumDistance15coordinateSpaceAC12CoreGraphics7CGFloatV_AA010CoordinateH0OtcfC(10, 0);
    void *g;
    arm(drag);
    g = $s7SwiftUI7GesturePAAE8updating_4bodyAA0c5StateC0Vyxqd__GAA0cF0Vyqd__G_y5ValueQz_qd__zAA11TransactionVztctlF(gs, (void *)on_update, 0);
    arm(g);
    g = $s7SwiftUI7GesturePAASQ5ValueRpzrlE9onChangedyAA01_fC0VyxGyAEcF((void *)on_changed, 0);
    arm(g);
    g = $s7SwiftUI7GesturePAAE7onEndedyAA01_eC0VyxGy5ValueQzcF((void *)on_ended, 0);

    if (mos_sui_drag(g, 0, 5, 5) || mos_sui_drag(g, 1, 8, 5) || changed) { fprintf(stderr, "FAIL distância mínima\n"); fails++; }
    if (!mos_sui_drag(g, 1, 20, 5) || changed != 1 || last_x != 20 || updated != 1 || gs->value != 20) {
        fprintf(stderr, "FAIL onChanged/updating\n"); fails++;
    }
    if (!mos_sui_drag(g, 2, 30, 5) || ended != 1 || start_x != 5 || gs->value != 0 || changed != 1) {
        fprintf(stderr, "FAIL onEnded e reset do GestureState\n"); fails++;
    }
    if (mos_sui_drag(&zero, 0, 0, 0)) { fprintf(stderr, "FAIL objeto que não é gesto\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}

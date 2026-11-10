/* ObservedObject, StateObject, Bindable, FocusState, GestureState, Spring,
 * Animation, withAnimation (resultado indireto em rax), withTransaction e AnyTransition.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_OBS_HOST -DSWIFTUI_STATE_HOST \
 *   userland/libsystem/swiftui_observable.c userland/libsystem/swiftui_state.c \
 *   userland/tests-swiftui/observable_test.c -o /tmp/sui_obs
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { uint64_t w0, w1; uint8_t tag; } stobj;
typedef struct { const void *isa; uint64_t value; } focus;
typedef struct { const sui_desc *isa; uint64_t value, initial; } gstate;
typedef struct { const sui_desc *isa; int kind; double duration, delay; double c[4]; double response, damping, blend; int repeat_forever, autoreverses; } anim;
typedef struct { const sui_desc *isa; void *animation; int disables; } txn;
typedef struct { const sui_desc *isa; int kind, edge; double scale, ax, ay; void *a, *b; } trans;

void *$s7SwiftUI14ObservedObjectV12wrappedValueACyxGx_tcfC(void *obj, void *meta, void *wt);
void *$s7SwiftUI14ObservedObjectV14projectedValueAC7WrapperVyx_Gvg(void);
void *$s7SwiftUI8BindableVAARlzC11Observation10ObservableRzlE12wrappedValueACyxGx_tcfC(void *obj, void *meta, void *wt);
void *$s7SwiftUI11StateObjectV12wrappedValuexvg(void);
void *$s7SwiftUI10FocusStateVACySbGycSbRszrlufC(void);
uint64_t $s7SwiftUI10FocusStateV12wrappedValuexvg(void);
void $s7SwiftUI10FocusStateV12wrappedValuexvs(uint64_t v);
void *$s7SwiftUI10FocusStateV14projectedValueAC7BindingVyx_Gvg(void);
uint64_t $s7SwiftUI7BindingV12wrappedValuexvg(void);
void $s7SwiftUI7BindingV12wrappedValuexvs(uint64_t value);
void *$s7SwiftUI12GestureStateV12wrappedValueACyxGx_tcfC(const uint64_t *value, void *meta);
uint64_t $s7SwiftUI12GestureStateV12wrappedValuexvg(void);
void *$s7SwiftUI6SpringV8response12dampingRatioACSd_SdtcfC(double response, double ratio);
void *$s7SwiftUI6SpringV4mass9stiffness7damping16allowOverDampingACSd_S2dSbtcfC(double m, double k, double c, int over);
double $s7SwiftUI6SpringV8durationSdvg(void);
double $s7SwiftUI6SpringV6bounceSdvg(void);
double $s7SwiftUI6SpringV16settlingDurationSdvg(void);
void *$s7SwiftUI9AnimationV9easeInOut8durationACSd_tFZ(double d);
void *$s7SwiftUI9AnimationV5delayyACSdF(double d);
void *$s7SwiftUI9AnimationV13repeatForever12autoreversesACSb_tF(int a);
void *$s7SwiftUI9AnimationV6spring_13blendDurationAcA6SpringV_SdtFZ(void *s, double blend);
void *$s7SwiftUI9AnimationV7defaultACvgZ(void);
void $s7SwiftUI11TransactionV9animationAA9AnimationVSgvs(void *a);
void $s7SwiftUI11TransactionV18disablesAnimationsSbvs(int on);
void *$s7SwiftUI13AnyTransitionV7opacityACvgZ(void);
void *$s7SwiftUI13AnyTransitionV4move4edgeAcA4EdgeO_tFZ(int edge);
void *$s7SwiftUI13AnyTransitionV8combined4withA2C_tF(void *other);
void *$s7SwiftUI13AnyTransitionV10asymmetric9insertion7removalA2C_ACtFZ(void *a, void *b);
void *$s7SwiftUI17OpacityTransitionVACycfC(void);
void *mos_sui_current_animation(void);
uint64_t mos_sui_animation_runs(void);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

static int thunk_calls;
static char model = 'm';
static void *make_model(void) { thunk_calls++; return &model; }

/* Corpo do withAnimation: anota a animação corrente e grava 99 no resultado indireto (rax). */
static void *seen_anim;
static void *seen_ctx;
void wa_record(void *ctx) { seen_anim = mos_sui_current_animation(); seen_ctx = ctx; }
__asm__(
    ".text\n"
    ".globl wa_body\n"
    "wa_body:\n"
    "    push %rax\n"
    "    mov %r13, %rdi\n"
    "    call wa_record\n"
    "    pop %rax\n"
    "    movq $99, (%rax)\n"
    "    ret\n");
void wa_body(void);

static int completions;
static void on_complete(void) { int *c; __asm__ volatile("mov %%r13, %0" : "=r"(c)); (*c)++; }

static void with_animation(void *a, uint64_t *out, void *ctx) {
    void *fn = (void *)wa_body;
    __asm__ volatile(
        "xor %%r12, %%r12\n\t"
        "call \"$s7SwiftUI13withAnimationyxAA0D0VSg_xyKXEtKlF\"\n\t"
        : "+a"(out), "+D"(a), "+S"(fn), "+d"(ctx)
        : "c"(0)
        : "r8", "r9", "r10", "r11", "r12", "memory", "cc",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
}

static void with_animation_done(void *a, uint64_t *out, void *ctx, int *counter) {
    register void *r8 __asm__("r8") = (void *)on_complete;
    register void *r9 __asm__("r9") = counter;
    void *fn = (void *)wa_body;
    __asm__ volatile(
        "xor %%r12, %%r12\n\t"
        "sub $8, %%rsp\n\t"
        "push $0\n\t"
        "call \"$s7SwiftUI13withAnimation_18completionCriteria_0E0xAA0D0VSg_AA0d10CompletionF0VxyKXEyyctKlF\"\n\t"
        "add $16, %%rsp\n\t"
        : "+a"(out), "+D"(a), "+d"(fn), "+c"(ctx), "+r"(r8), "+r"(r9)
        : "S"(0)
        : "r10", "r11", "r12", "memory", "cc",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
}

static int near(double a, double b) { double d = a - b; return d < 1e-6 && d > -1e-6; }

int main(void) {
    int fails = 0;
    char obj = 'o', ctxmark = 'c';
    stobj so;
    focus *f;
    void *b, *sp, *a, *a2;
    gstate *g;
    uint64_t initial = 3, result = 0;
    txn t = { 0, 0, 0 };
    trans *tr;

    if ($s7SwiftUI14ObservedObjectV12wrappedValueACyxGx_tcfC(&obj, 0, 0) != &obj) { fprintf(stderr, "FAIL ObservedObject\n"); fails++; }
    arm(&obj);
    if ($s7SwiftUI14ObservedObjectV14projectedValueAC7WrapperVyx_Gvg() != &obj) { fprintf(stderr, "FAIL projectedValue\n"); fails++; }
    if ($s7SwiftUI8BindableVAARlzC11Observation10ObservableRzlE12wrappedValueACyxGx_tcfC(&obj, 0, 0) != &obj) { fprintf(stderr, "FAIL Bindable\n"); fails++; }

    so.w0 = (uint64_t)(uintptr_t)make_model; so.w1 = 0; so.tag = 0;
    arm(&so);
    a = $s7SwiftUI11StateObjectV12wrappedValuexvg();
    arm(&so);
    a2 = $s7SwiftUI11StateObjectV12wrappedValuexvg();
    if (a != &model || a2 != &model || thunk_calls != 1 || so.tag != 1) { fprintf(stderr, "FAIL StateObject cria uma vez\n"); fails++; }

    f = $s7SwiftUI10FocusStateVACySbGycSbRszrlufC();
    arm(f);
    $s7SwiftUI10FocusStateV12wrappedValuexvs(1);
    arm(f);
    b = $s7SwiftUI10FocusStateV14projectedValueAC7BindingVyx_Gvg();
    arm(b);
    if ($s7SwiftUI7BindingV12wrappedValuexvg() != 1) { fprintf(stderr, "FAIL FocusState via Binding\n"); fails++; }
    arm(b);
    $s7SwiftUI7BindingV12wrappedValuexvs(0);
    arm(f);
    if ($s7SwiftUI10FocusStateV12wrappedValuexvg() != 0) { fprintf(stderr, "FAIL Binding grava no FocusState\n"); fails++; }

    g = $s7SwiftUI12GestureStateV12wrappedValueACyxGx_tcfC(&initial, 0);
    arm(g);
    if (!g || $s7SwiftUI12GestureStateV12wrappedValuexvg() != 3 || g->initial != 3) { fprintf(stderr, "FAIL GestureState\n"); fails++; }

    sp = $s7SwiftUI6SpringV8response12dampingRatioACSd_SdtcfC(0.5, 0.5);
    arm(sp);
    if (!near($s7SwiftUI6SpringV8durationSdvg(), 0.5)) { fprintf(stderr, "FAIL Spring.duration\n"); fails++; }
    arm(sp);
    if (!near($s7SwiftUI6SpringV6bounceSdvg(), 0.5)) { fprintf(stderr, "FAIL Spring.bounce\n"); fails++; }
    arm(sp);
    /* ln(1000) / (0,5 · 4π) = 1,0995... */
    if (!near($s7SwiftUI6SpringV16settlingDurationSdvg(), 6.907755278982137 / (0.5 * 4 * 3.14159265358979323846))) {
        fprintf(stderr, "FAIL Spring.settlingDuration\n"); fails++;
    }
    sp = $s7SwiftUI6SpringV4mass9stiffness7damping16allowOverDampingACSd_S2dSbtcfC(1, 100, 50, 0);
    arm(sp);
    if (!near($s7SwiftUI6SpringV6bounceSdvg(), 0)) { fprintf(stderr, "FAIL Spring sem overdamping\n"); fails++; }

    a = $s7SwiftUI9AnimationV9easeInOut8durationACSd_tFZ(0.3);
    arm(a);
    a2 = $s7SwiftUI9AnimationV5delayyACSdF(0.1);
    arm(a2);
    a2 = $s7SwiftUI9AnimationV13repeatForever12autoreversesACSb_tF(1);
    if (!a2 || ((anim *)a2)->duration != 0.3 || ((anim *)a2)->delay != 0.1 || !((anim *)a2)->repeat_forever ||
        ((anim *)a)->delay != 0 || ((anim *)a)->c[0] != 0.42) { fprintf(stderr, "FAIL Animation\n"); fails++; }
    sp = $s7SwiftUI6SpringV8response12dampingRatioACSd_SdtcfC(0.4, 0.8);
    a2 = $s7SwiftUI9AnimationV6spring_13blendDurationAcA6SpringV_SdtFZ(sp, 0);
    if (!a2 || !near(((anim *)a2)->response, 0.4) || !near(((anim *)a2)->damping, 0.8)) { fprintf(stderr, "FAIL Animation.spring(Spring)\n"); fails++; }

    with_animation(a, &result, &ctxmark);
    if (result != 99 || seen_anim != a || seen_ctx != &ctxmark || mos_sui_current_animation() != 0 || mos_sui_animation_runs() != 1) {
        fprintf(stderr, "FAIL withAnimation\n"); fails++;
    }
    result = 0;
    with_animation_done($s7SwiftUI9AnimationV7defaultACvgZ(), &result, &ctxmark, &completions);
    if (result != 99 || completions != 1 || seen_anim != $s7SwiftUI9AnimationV7defaultACvgZ() || mos_sui_current_animation() != 0) {
        fprintf(stderr, "FAIL withAnimation(completion:)\n"); fails++;
    }

    arm(&t);
    $s7SwiftUI11TransactionV9animationAA9AnimationVSgvs(a);
    arm(&t);
    $s7SwiftUI11TransactionV18disablesAnimationsSbvs(1);
    if (t.animation != a || !t.disables) { fprintf(stderr, "FAIL Transaction\n"); fails++; }

    tr = $s7SwiftUI13AnyTransitionV4move4edgeAcA4EdgeO_tFZ(2);
    arm(tr);
    a2 = $s7SwiftUI13AnyTransitionV8combined4withA2C_tF($s7SwiftUI13AnyTransitionV7opacityACvgZ());
    if (!tr || tr->edge != 2 || !a2 || ((trans *)a2)->a != tr || ((trans *)a2)->b != $s7SwiftUI13AnyTransitionV7opacityACvgZ()) {
        fprintf(stderr, "FAIL AnyTransition\n"); fails++;
    }
    a2 = $s7SwiftUI13AnyTransitionV10asymmetric9insertion7removalA2C_ACtFZ(tr, 0);
    if (!a2 || ((trans *)a2)->a != tr || $s7SwiftUI17OpacityTransitionVACycfC() != $s7SwiftUI13AnyTransitionV7opacityACvgZ()) {
        fprintf(stderr, "FAIL asymmetric/OpacityTransition\n"); fails++;
    }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}

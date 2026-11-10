/* Wrappers de propriedade (ObservedObject, StateObject, Bindable, FocusState,
 * GestureState) e animação (Animation, Spring, Transaction, AnyTransition,
 * withAnimation e withTransaction). O mOS não interpola: withAnimation grava
 * a animação corrente, roda o corpo na hora e a mudança vale imediatamente.
 */
#ifdef SWIFTUI_OBS_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

/* ---- ObservedObject e Bindable: struct de uma referência só ---- */

sui_desc $s7SwiftUI14ObservedObjectVMn = { SUI_K_OBSERVED, "ObservedObject" };

/* O valor do struct é a própria referência ao objeto. */
void *$s7SwiftUI14ObservedObjectV12wrappedValueACyxGx_tcfC(void *obj, void *meta, void *wt) {
    (void)meta; (void)wt;
    return obj;
}

/* $objeto: o Wrapper também guarda só a referência. */
void *$s7SwiftUI14ObservedObjectV14projectedValueAC7WrapperVyx_Gvg(void) { return sui_self(); }

sui_desc $s7SwiftUI8BindableVMn = { SUI_K_BINDABLE, "Bindable" };
void *$s7SwiftUI8BindableVMa(void) { return &$s7SwiftUI8BindableVMn; }
void *$s7SwiftUI8BindableVAARlzC11Observation10ObservableRzlE12wrappedValueACyxGx_tcfC(void *obj, void *meta, void *wt) {
    (void)meta; (void)wt;
    return obj;
}
void *$s7SwiftUI8BindableV12wrappedValuexvg(void) { return sui_self(); }

/* ---- StateObject: Storage = initially(() -> T) | object(T) ----
 * Layout do enum de dois payloads: w0, w1 e o byte de caso (0 = initially). */
typedef struct { uint64_t w0, w1; uint8_t tag; } sui_stobj;
sui_desc $s7SwiftUI11StateObjectVMn = { SUI_K_STATEOBJ, "StateObject" };

void *$s7SwiftUI11StateObjectV12wrappedValuexvg(void) {
    sui_stobj *s = (sui_stobj *)sui_self();
    sui_fn thunk;
    if (!s) return 0;
    if (s->tag == 1) return (void *)(uintptr_t)s->w0;
    thunk.fn = (void *)(uintptr_t)s->w0;
    thunk.ctx = (void *)(uintptr_t)s->w1;
    s->w0 = sui_call0r(thunk);
    s->w1 = 0;
    s->tag = 1;
    return (void *)(uintptr_t)s->w0;
}

/* ---- FocusState: mesmo { isa, valor } que o projectedValue de swiftui_state.c usa ---- */
typedef struct { const void *isa; uint64_t value; } sui_focus;
sui_desc $s7SwiftUI10FocusStateVMn = { SUI_K_FOCUS, "FocusState" };

void *$s7SwiftUI10FocusStateVACySbGycSbRszrlufC(void) {
    sui_focus *f = (sui_focus *)calloc(1, sizeof *f);
    if (!f) return 0;
    f->isa = &$s7SwiftUI10FocusStateVMn;
    return f;
}

uint64_t $s7SwiftUI10FocusStateV12wrappedValuexvg(void) {
    sui_focus *f = (sui_focus *)sui_self();
    return f ? f->value : 0;
}

void $s7SwiftUI10FocusStateV12wrappedValuexvs(uint64_t v) {
    sui_focus *f = (sui_focus *)sui_self();
    if (!f) return;
    if (!f->isa) f->isa = &$s7SwiftUI10FocusStateVMn;
    f->value = v;
}

/* ---- GestureState: valor corrente e o inicial, para voltar no fim do gesto ---- */
typedef struct { const sui_desc *isa; uint64_t value, initial; } sui_gstate;
sui_desc $s7SwiftUI12GestureStateVMn = { SUI_K_GESTURESTATE, "GestureState" };

void *$s7SwiftUI12GestureStateV12wrappedValueACyxGx_tcfC(const uint64_t *value, void *meta) {
    sui_gstate *g = (sui_gstate *)sui_new(&$s7SwiftUI12GestureStateVMn, sizeof *g);
    (void)meta;
    if (!g) return 0;
    g->value = g->initial = value ? value[0] : 0;
    return g;
}

uint64_t $s7SwiftUI12GestureStateV12wrappedValuexvg(void) {
    sui_gstate *g = (sui_gstate *)sui_self();
    return g ? g->value : 0;
}

void *$s7SwiftUI12GestureStateV14projectedValueACyxGvg(void) { return sui_self(); }

/* ---- Binding: conformidades condicionais ---- */
sui_conf $s7SwiftUI7BindingVyxGSkAASMRzSkRzlMc = { 0, "RandomAccessCollection" };
sui_conf $s7SwiftUI7BindingVyxGs12IdentifiableAAsAERzlMc = { 0, "Identifiable" };
sui_desc $s7SwiftUI11AnyLocationCMn = { SUI_K_STRUCT, "AnyLocation" };

/* ---- Matemática sem libm ---- */

static double sui_sqrt(double x) {
    double r;
    if (x <= 0) return 0;
    __asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x));
    return r;
}

/* ln por redução a [1, 2) e série de atanh. */
static double sui_ln(double x) {
    double y, y2, sum = 0, term;
    int k = 0, i;
    if (x <= 0) return -1e300;
    while (x >= 2) { x *= 0.5; k++; }
    while (x < 1) { x *= 2; k--; }
    y = (x - 1) / (x + 1);
    y2 = y * y;
    term = y;
    for (i = 1; i < 40; i += 2) { sum += term / i; term *= y2; }
    return 2 * sum + k * 0.69314718055994530942;
}

/* ---- Spring ---- */

typedef struct { const sui_desc *isa; double mass, stiffness, damping; } sui_spring;
static sui_desc sui_spring_desc = { SUI_K_SPRING, "Spring" };
void *$s7SwiftUI6SpringVMa(void) { return &sui_spring_desc; }

static sui_spring *sui_spring_new(double mass, double stiffness, double damping) {
    sui_spring *s = (sui_spring *)sui_new(&sui_spring_desc, sizeof *s);
    if (!s) return 0;
    s->mass = mass > 0 ? mass : 1;
    s->stiffness = stiffness > 0 ? stiffness : 1;
    s->damping = damping < 0 ? 0 : damping;
    return s;
}

void *$s7SwiftUI6SpringV4mass9stiffness7damping16allowOverDampingACSd_S2dSbtcfC(double mass, double stiffness, double damping, int over) {
    sui_spring *s = sui_spring_new(mass, stiffness, damping);
    if (s && !(over & 1)) {
        double crit = 2 * sui_sqrt(s->stiffness * s->mass);
        if (s->damping > crit) s->damping = crit;
    }
    return s;
}

/* response = período; dampingRatio = ζ. k = (2π/T)² m, c = 4π ζ m / T. */
void *$s7SwiftUI6SpringV8response12dampingRatioACSd_SdtcfC(double response, double ratio) {
    double pi = 3.14159265358979323846, w;
    if (response <= 0) response = 0.5;
    w = 2 * pi / response;
    return sui_spring_new(1, w * w, 2 * ratio * w);
}

static double sui_spring_ratio(const sui_spring *s) {
    return s->damping / (2 * sui_sqrt(s->stiffness * s->mass));
}

double $s7SwiftUI6SpringV8durationSdvg(void) {
    sui_spring *s = (sui_spring *)sui_self();
    if (!s) return 0;
    return 2 * 3.14159265358979323846 / sui_sqrt(s->stiffness / s->mass);
}

double $s7SwiftUI6SpringV6bounceSdvg(void) {
    sui_spring *s = (sui_spring *)sui_self();
    double z;
    if (!s) return 0;
    z = sui_spring_ratio(s);
    return z <= 1 ? 1 - z : 1 / z - 1;
}

/* Tempo até a amplitude cair abaixo de 0,1%: ln(1000) / (ζ ω0). */
double $s7SwiftUI6SpringV16settlingDurationSdvg(void) {
    sui_spring *s = (sui_spring *)sui_self();
    double w0, z;
    if (!s) return 0;
    w0 = sui_sqrt(s->stiffness / s->mass);
    z = sui_spring_ratio(s);
    if (z <= 0 || w0 <= 0) return 1e9;
    if (z > 1) z = 1 / (z + sui_sqrt(z * z - 1));
    return sui_ln(1000.0) / (z * w0);
}

/* ---- Animation ---- */

enum { SUI_A_DEFAULT, SUI_A_LINEAR, SUI_A_EASEIN, SUI_A_EASEOUT, SUI_A_EASEINOUT, SUI_A_CURVE, SUI_A_SPRING };
typedef struct {
    const sui_desc *isa;
    int kind;
    double duration, delay;
    double c[4];               /* pontos de controle da curva */
    double response, damping, blend;
    int repeat_forever, autoreverses;
} sui_anim;

static sui_desc sui_anim_desc = { SUI_K_ANIMATION, "Animation" };
static sui_anim sui_anim_default = { &sui_anim_desc, SUI_A_DEFAULT, 0.35, 0, { 0.42, 0, 0.58, 1 }, 0, 0, 0, 0, 0 };

static sui_anim *sui_anim_new(int kind, double duration) {
    sui_anim *a = (sui_anim *)sui_new(&sui_anim_desc, sizeof *a);
    if (!a) return 0;
    a->kind = kind;
    a->duration = duration < 0 ? 0 : duration;
    return a;
}

static sui_anim *sui_anim_curve(int kind, double duration, double x1, double y1, double x2, double y2) {
    sui_anim *a = sui_anim_new(kind, duration);
    if (a) { a->c[0] = x1; a->c[1] = y1; a->c[2] = x2; a->c[3] = y2; }
    return a;
}

void *$s7SwiftUI9AnimationV7defaultACvgZ(void) { return &sui_anim_default; }
void *$s7SwiftUI9AnimationV6linear8durationACSd_tFZ(double d) { return sui_anim_curve(SUI_A_LINEAR, d, 0, 0, 1, 1); }
void *$s7SwiftUI9AnimationV6easeIn8durationACSd_tFZ(double d) { return sui_anim_curve(SUI_A_EASEIN, d, 0.42, 0, 1, 1); }
void *$s7SwiftUI9AnimationV7easeOut8durationACSd_tFZ(double d) { return sui_anim_curve(SUI_A_EASEOUT, d, 0, 0, 0.58, 1); }
void *$s7SwiftUI9AnimationV9easeInOut8durationACSd_tFZ(double d) { return sui_anim_curve(SUI_A_EASEINOUT, d, 0.42, 0, 0.58, 1); }
void *$s7SwiftUI9AnimationV11timingCurve____8durationACSd_S4dtFZ(double x1, double y1, double x2, double y2, double d) {
    return sui_anim_curve(SUI_A_CURVE, d, x1, y1, x2, y2);
}

void *$s7SwiftUI9AnimationV6spring8response15dampingFraction13blendDurationACSd_S2dtFZ(double response, double damping, double blend) {
    sui_anim *a = sui_anim_new(SUI_A_SPRING, response);
    if (a) { a->response = response; a->damping = damping; a->blend = blend; }
    return a;
}

void *$s7SwiftUI9AnimationV6spring_13blendDurationAcA6SpringV_SdtFZ(sui_spring *s, double blend) {
    double period = 0.5, ratio = 1;
    sui_anim *a;
    if (s && s->stiffness > 0 && s->mass > 0) {
        period = 2 * 3.14159265358979323846 / sui_sqrt(s->stiffness / s->mass);
        ratio = sui_spring_ratio(s);
    }
    a = sui_anim_new(SUI_A_SPRING, period);
    if (a) { a->response = period; a->damping = ratio; a->blend = blend; }
    return a;
}

/* ω0 = sqrt(k/m), ζ = c / (2 sqrt(k m)). */
void *$s7SwiftUI9AnimationV19interpolatingSpring4mass9stiffness7damping15initialVelocityACSd_S3dtFZ(double mass, double stiffness, double damping, double v0) {
    double w0, period, ratio;
    sui_anim *a;
    (void)v0;
    if (mass <= 0) mass = 1;
    if (stiffness <= 0) stiffness = 1;
    w0 = sui_sqrt(stiffness / mass);
    period = 2 * 3.14159265358979323846 / w0;
    ratio = damping / (2 * sui_sqrt(stiffness * mass));
    a = sui_anim_new(SUI_A_SPRING, period);
    if (a) { a->response = period; a->damping = ratio; }
    return a;
}

static sui_anim *sui_anim_copy(const sui_anim *src) {
    sui_anim *a;
    if (!src) return 0;
    a = (sui_anim *)malloc(sizeof *a);
    if (a) *a = *src;
    return a;
}

void *$s7SwiftUI9AnimationV5delayyACSdF(double d) {
    sui_anim *a = sui_anim_copy((sui_anim *)sui_self());
    if (a) a->delay += d < 0 ? 0 : d;
    return a;
}

void *$s7SwiftUI9AnimationV13repeatForever12autoreversesACSb_tF(int autoreverses) {
    sui_anim *a = sui_anim_copy((sui_anim *)sui_self());
    if (a) { a->repeat_forever = 1; a->autoreverses = autoreverses & 1; }
    return a;
}

/* ---- Transaction ---- */

typedef struct { const sui_desc *isa; void *animation; int disables; } sui_txn;
static sui_desc sui_txn_desc = { SUI_K_TRANSACTION, "Transaction" };
sui_meta $s7SwiftUI11TransactionVN = { &sui_txn_desc };

void $s7SwiftUI11TransactionV9animationAA9AnimationVSgvs(void *anim) {
    sui_txn *t = (sui_txn *)sui_self();
    if (t) t->animation = anim;
}

void $s7SwiftUI11TransactionV18disablesAnimationsSbvs(int on) {
    sui_txn *t = (sui_txn *)sui_self();
    if (t) t->disables = on & 1;
}

/* Animação e transação correntes, lidas por quem quiser saber se a mudança veio animada. */
__attribute__((visibility("hidden"))) void *sui_cur_anim;
__attribute__((visibility("hidden"))) void *sui_cur_txn;
__attribute__((visibility("hidden"))) uint64_t sui_anim_runs;

void *mos_sui_current_animation(void) { return sui_cur_anim; }
uint64_t mos_sui_animation_runs(void) { return sui_anim_runs; }

/* withAnimation(_:_:): rdi = animação, rsi/rdx = corpo, rcx = metadado de Result.
 * O resultado indireto segue em rax e o erro em r12 até o corpo. */
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl \"$s7SwiftUI13withAnimationyxAA0D0VSg_xyKXEtKlF\"\n"
    ".type \"$s7SwiftUI13withAnimationyxAA0D0VSg_xyKXEtKlF\", @function\n"
    "\"$s7SwiftUI13withAnimationyxAA0D0VSg_xyKXEtKlF\":\n"
    "    push %r13\n"
    "    push %rbx\n"
    "    sub $8, %rsp\n"
    "    mov sui_cur_anim(%rip), %rbx\n"
    "    mov %rdi, sui_cur_anim(%rip)\n"
    "    incq sui_anim_runs(%rip)\n"
    "    mov %rdx, %r13\n"
    "    call *%rsi\n"
    "    mov %rbx, sui_cur_anim(%rip)\n"
    "    add $8, %rsp\n"
    "    pop %rbx\n"
    "    pop %r13\n"
    "    ret\n"
    /* withAnimation(_:completionCriteria:_:completion:): corpo em rdx/rcx,
     * completion em r8/r9; a completion roda logo depois, se o corpo não lançou. */
    ".p2align 4\n"
    ".globl \"$s7SwiftUI13withAnimation_18completionCriteria_0E0xAA0D0VSg_AA0d10CompletionF0VxyKXEyyctKlF\"\n"
    ".type \"$s7SwiftUI13withAnimation_18completionCriteria_0E0xAA0D0VSg_AA0d10CompletionF0VxyKXEyyctKlF\", @function\n"
    "\"$s7SwiftUI13withAnimation_18completionCriteria_0E0xAA0D0VSg_AA0d10CompletionF0VxyKXEyyctKlF\":\n"
    "    push %r13\n"
    "    push %rbx\n"
    "    push %r14\n"
    "    push %r15\n"
    "    sub $8, %rsp\n"
    "    mov sui_cur_anim(%rip), %r15\n"
    "    mov %rdi, sui_cur_anim(%rip)\n"
    "    incq sui_anim_runs(%rip)\n"
    "    mov %r8, %rbx\n"
    "    mov %r9, %r14\n"
    "    mov %rcx, %r13\n"
    "    call *%rdx\n"
    "    mov %r15, sui_cur_anim(%rip)\n"
    "    test %r12, %r12\n"
    "    jnz 1f\n"
    "    test %rbx, %rbx\n"
    "    jz 1f\n"
    "    mov %r14, %r13\n"
    "    call *%rbx\n"
    "1:\n"
    "    add $8, %rsp\n"
    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %rbx\n"
    "    pop %r13\n"
    "    ret\n"
    /* withTransaction(_:_:): rdi = transação, rsi/rdx = corpo. */
    ".p2align 4\n"
    ".globl \"$s7SwiftUI15withTransactionyxAA0D0V_xyKXEtKlF\"\n"
    ".type \"$s7SwiftUI15withTransactionyxAA0D0V_xyKXEtKlF\", @function\n"
    "\"$s7SwiftUI15withTransactionyxAA0D0V_xyKXEtKlF\":\n"
    "    push %r13\n"
    "    push %rbx\n"
    "    sub $8, %rsp\n"
    "    mov sui_cur_txn(%rip), %rbx\n"
    "    mov %rdi, sui_cur_txn(%rip)\n"
    "    mov %rdx, %r13\n"
    "    call *%rsi\n"
    "    mov %rbx, sui_cur_txn(%rip)\n"
    "    add $8, %rsp\n"
    "    pop %rbx\n"
    "    pop %r13\n"
    "    ret\n");

/* ---- AnyTransition ---- */

enum { SUI_T_IDENTITY, SUI_T_OPACITY, SUI_T_MOVE, SUI_T_SCALE, SUI_T_ASYM, SUI_T_COMBINED, SUI_T_CUSTOM };
typedef struct { const sui_desc *isa; int kind, edge; double scale, ax, ay; void *a, *b; } sui_trans;
static sui_desc sui_trans_desc = { SUI_K_TRANSITION, "AnyTransition" };
static sui_trans sui_trans_opacity = { &sui_trans_desc, SUI_T_OPACITY, 0, 1, 0.5, 0.5, 0, 0 };

static sui_trans *sui_trans_new(int kind) {
    sui_trans *t = (sui_trans *)sui_new(&sui_trans_desc, sizeof *t);
    if (!t) return 0;
    t->kind = kind;
    t->scale = 1;
    t->ax = t->ay = 0.5;
    return t;
}

void *$s7SwiftUI13AnyTransitionV7opacityACvgZ(void) { return &sui_trans_opacity; }

void *$s7SwiftUI13AnyTransitionV4move4edgeAcA4EdgeO_tFZ(int edge) {
    sui_trans *t = sui_trans_new(SUI_T_MOVE);
    if (t) t->edge = edge & 0xFF;
    return t;
}

void *$s7SwiftUI13AnyTransitionV5scaleAD6anchorAC12CoreGraphics7CGFloatV_AA9UnitPointVtFZ(double scale, double ax, double ay) {
    sui_trans *t = sui_trans_new(SUI_T_SCALE);
    if (t) { t->scale = scale; t->ax = ax; t->ay = ay; }
    return t;
}

void *$s7SwiftUI13AnyTransitionV10asymmetric9insertion7removalA2C_ACtFZ(void *ins, void *rem) {
    sui_trans *t = sui_trans_new(SUI_T_ASYM);
    if (t) { t->a = ins; t->b = rem; }
    return t;
}

void *$s7SwiftUI13AnyTransitionV8combined4withA2C_tF(void *other) {
    void *self = sui_self();
    sui_trans *t = sui_trans_new(SUI_T_COMBINED);
    if (t) { t->a = self; t->b = other; }
    return t;
}

void *$s7SwiftUI13AnyTransitionVyACxcAA0D0RzlufC(void *transition, void *meta, void *wt) {
    sui_trans *t = sui_trans_new(SUI_T_CUSTOM);
    (void)meta; (void)wt;
    if (t) t->a = transition;
    return t;
}

/* OpacityTransition() é o mesmo que AnyTransition.opacity. */
static sui_desc sui_opt_desc = { SUI_K_TRANSITION, "OpacityTransition" };
void *$s7SwiftUI17OpacityTransitionVMa(void) { return &sui_opt_desc; }
void *$s7SwiftUI17OpacityTransitionVACycfC(void) { return &sui_trans_opacity; }
sui_conf $s7SwiftUI17OpacityTransitionVAA0D0AAMc = { 0, "Transition" };

/*
 * Corpo de _objc_msgForward. A entrada é objc_forward.S: os registradores e 256
 * bytes de argumentos de pilha já estão no quadro. Daqui a chamada segue com
 * esses argumentos, por um destes caminhos:
 *   - o IMP, se a classe (ou o forwardingTargetForSelector:) o tem;
 *   - forwardInvocation:, com um NSInvocation que carrega os 3+ argumentos.
 * Sem methodSignature, devolve nil. Não é o doesNotRecognizeSelector: da Apple.
 */
#include "objc.h"
#if !defined(__aarch64__)
#include "ffi_x64.h"
#else
typedef struct {
    u64 x[8], x8, nstack;
    u8 *stack;
    u64 pad;
    u8 v[8][16];
    u64 rx[2];
    u8 rv[4][16];
} cx_a64_frame_t;
void cx_a64_call(void *fn, cx_a64_frame_t *f);
#endif

void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);

typedef struct {
    unsigned long long gpr[9];
    unsigned char pad[8];
    unsigned char fpr[8][16];
    unsigned char stack[256];
    unsigned long long ret_gpr;
    unsigned long long ret_gpr2;
    unsigned char ret_fpr[16];
} cx_fwd_frame_t;

_Static_assert(__builtin_offsetof(cx_fwd_frame_t, fpr) == 80, "fpr do _objc_msgForward");
_Static_assert(__builtin_offsetof(cx_fwd_frame_t, stack) == 208, "pilha do _objc_msgForward");
_Static_assert(__builtin_offsetof(cx_fwd_frame_t, ret_gpr) == 464, "retorno do _objc_msgForward");
_Static_assert(__builtin_offsetof(cx_fwd_frame_t, ret_fpr) == 480, "xmm de retorno");
_Static_assert(sizeof(cx_fwd_frame_t) <= 512, "quadro cabe no stack do asm");

static char type_kind(const char *t) {
    if (!t) return '@';
    while (*t == 'r' || *t == 'n' || *t == 'N' || *t == 'o' || *t == 'O' || *t == 'R' || *t == 'V' || *t == 'A') t++;
    return *t ? *t : '@';
}

static SEL sel(const char *n) { return sel_registerName(n); }

static void invoke_imp(cx_fwd_frame_t *f, IMP imp) {
#if !defined(__aarch64__)
    cx_x64_frame_t fr;
    memset(&fr, 0, sizeof fr);
    memcpy(fr.x, f->gpr, sizeof fr.x);
    memcpy(fr.v, f->fpr, sizeof fr.v);
    fr.stack = f->stack;
    fr.nstack = 256;
    cx_x64_call((void *)imp, &fr);
    f->ret_gpr = fr.rax;
    f->ret_gpr2 = fr.rdx;
    memcpy(f->ret_fpr, fr.rv, 16);
#else
    cx_a64_frame_t fr;
    memset(&fr, 0, sizeof fr);
    memcpy(fr.x, f->gpr, sizeof fr.x);
    fr.x8 = f->gpr[8];
    memcpy(fr.v, f->fpr, sizeof fr.v);
    fr.stack = f->stack;
    fr.nstack = 256;
    cx_a64_call((void *)imp, &fr);
    f->ret_gpr = fr.rx[0];
    f->ret_gpr2 = fr.rx[1];
    memcpy(f->ret_fpr, fr.rv[0], 16);
#endif
}

void cx_objc_forward(cx_fwd_frame_t *f) {
    id cur = (id)f->gpr[0];
    SEL cmd = (SEL)f->gpr[1];
    f->ret_gpr = 0;
    f->ret_gpr2 = 0;
    memset(f->ret_fpr, 0, sizeof f->ret_fpr);
    if (!cur || !cur->isa || !cmd) return;

    SEL fwd = sel("forwardInvocation:");
    SEL sigs = sel("methodSignatureForSelector:");
    SEL tgt = sel("forwardingTargetForSelector:");
    if (cmd == fwd || cmd == sigs || cmd == tgt) return;

    for (int hop = 0; hop < 4; hop++) {
        IMP got = cx_lookup(cur->isa, cmd);
        if (got) { invoke_imp(f, got); return; }
        if (!cx_lookup(cur->isa, tgt)) break;
        id t = ((id (*)(id, SEL, SEL))objc_msgSend)(cur, tgt, cmd);
        if (!t || t == cur || !t->isa) break;
        f->gpr[0] = (unsigned long long)t;
        cur = t;
    }

    if (!cx_lookup(cur->isa, sigs)) return;
    id sig = ((id (*)(id, SEL, SEL))objc_msgSend)(cur, sigs, cmd);
    if (!sig) return;
    id invcls = (id)cx_class_named("NSInvocation");
    if (!invcls) return;
    id inv = ((id (*)(id, SEL, id))objc_msgSend)(invcls, sel("invocationWithMethodSignature:"), sig);
    if (!inv) return;

    unsigned long long nargs = ((unsigned long long (*)(id, SEL))objc_msgSend)(sig, sel("numberOfArguments"));
    if (nargs > 16) nargs = 16;
#if defined(__aarch64__)
    const int ngpr = 8;
#else
    const int ngpr = 6;
#endif
    int gi = 0, fi = 0, sp = 0;
    for (unsigned long long i = 0; i < nargs; i++) {
        const char *ty = ((const char *(*)(id, SEL, unsigned long long))objc_msgSend)(sig, sel("getArgumentTypeAtIndex:"), i);
        char k = type_kind(ty);
        unsigned long long slot = 0;
        int fp = k == 'f' || k == 'd' || k == 'D';
        if (fp) {
            if (fi < 8) memcpy(&slot, f->fpr[fi], k == 'f' ? 4 : 8);
            fi++;
        } else if (gi < ngpr) {
            slot = f->gpr[gi++];
        } else {
            if (sp + 8 <= (int)sizeof f->stack) memcpy(&slot, f->stack + sp, 8);
            sp += 8;
        }
        ((void (*)(id, SEL, void *, unsigned long long))objc_msgSend)(inv, sel("setArgument:atIndex:"), &slot, i);
    }
    ((void (*)(id, SEL, id))objc_msgSend)(cur, fwd, inv);

    unsigned long long rlen = ((unsigned long long (*)(id, SEL))objc_msgSend)(sig, sel("methodReturnLength"));
    if (!rlen) return;
    unsigned char ret[16];
    memset(ret, 0, sizeof ret);
    ((void (*)(id, SEL, void *))objc_msgSend)(inv, sel("getReturnValue:"), ret);
    if (rlen > 16) rlen = 16;
    memcpy(&f->ret_gpr, ret, rlen > 8 ? 8 : (size_t)rlen);
    memcpy(f->ret_fpr, ret, 16);
}

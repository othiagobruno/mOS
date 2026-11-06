/* mOS — motor JavaScript "Brisa": geradores (function*, yield, yield*) e funções async/await.
 *
 * O interpretador percorre a árvore recursivamente, então suspender uma função no meio exige
 * guardar a pilha C dela: cada gerador/chamada async roda numa corrotina com pilha própria
 * (kmalloc) e a troca de contexto é feita em assembly (ABI x64 da Microsoft: rbx, rbp, rdi, rsi,
 * r12-r15 e xmm6-xmm15 são preservados). O coletor varre a cadeia de corrotinas ativas e as pilhas
 * suspensas cujo dono (objeto gerador) está vivo. */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

#define CO_STACK  (192u * 1024)
#define CO_MARGIN (40u * 1024)

enum { CO_FRESH, CO_RUNNING, CO_SUSPENDED, CO_DONE };
enum { IN_NEXT, IN_THROW, IN_RETURN };
enum { OUT_YIELD, OUT_RETURN, OUT_THROW, OUT_AWAIT };

typedef struct jscoro {
    void *sp, *caller_sp;
    u8 *stack;
    uptr caller_base;
    u32 caller_limit;
    struct jscoro *caller;
    int state;
    jsobj *owner;
    jsv fn, thisv;
    jsobj *args;
    int in_mode;
    jsv in_val;
    int out_kind;
    jsv out_val;
    jsv ret_val;             /* valor de gen.return(v) */
    jsictx ctx;
    bool is_async, scanned;
    bool is_asyncgen, awaiting;
    jsobj *queue;            /* gerador async: pedidos [modo, valor, promessa, ...] */
    jsobj *promise;
    struct jscoro *next, *prev;
} jscoro;

static jscoro *coros;
static jsobj *genret_marker;

void js_coro_switch(void **save_sp, void *to_sp);
void js_coro_tramp(void);
void js_coro_entry(jscoro *co);

__asm__(
    ".text\n"
    ".globl js_coro_switch\n"
    "js_coro_switch:\n"
    "    pushq %rbp\n    pushq %rbx\n    pushq %rdi\n    pushq %rsi\n"
    "    pushq %r12\n    pushq %r13\n    pushq %r14\n    pushq %r15\n"
    "    subq $168, %rsp\n"
    "    movdqu %xmm6, 0(%rsp)\n    movdqu %xmm7, 16(%rsp)\n    movdqu %xmm8, 32(%rsp)\n    movdqu %xmm9, 48(%rsp)\n"
    "    movdqu %xmm10, 64(%rsp)\n    movdqu %xmm11, 80(%rsp)\n    movdqu %xmm12, 96(%rsp)\n    movdqu %xmm13, 112(%rsp)\n"
    "    movdqu %xmm14, 128(%rsp)\n    movdqu %xmm15, 144(%rsp)\n"
    "    movq %rsp, (%rcx)\n"
    "    movq %rdx, %rsp\n"
    "    movdqu 0(%rsp), %xmm6\n    movdqu 16(%rsp), %xmm7\n    movdqu 32(%rsp), %xmm8\n    movdqu 48(%rsp), %xmm9\n"
    "    movdqu 64(%rsp), %xmm10\n    movdqu 80(%rsp), %xmm11\n    movdqu 96(%rsp), %xmm12\n    movdqu 112(%rsp), %xmm13\n"
    "    movdqu 128(%rsp), %xmm14\n    movdqu 144(%rsp), %xmm15\n"
    "    addq $168, %rsp\n"
    "    popq %r15\n    popq %r14\n    popq %r13\n    popq %r12\n"
    "    popq %rsi\n    popq %rdi\n    popq %rbx\n    popq %rbp\n"
    "    ret\n"
    ".globl js_coro_tramp\n"
    "js_coro_tramp:\n"
    "    movq %rbx, %rcx\n"
    "    subq $40, %rsp\n"
    "    call js_coro_entry\n"
    "    ud2\n");

static jscoro *coro_new(jsobj *owner, jsv fn, jsv thisv, int argc, jsv *argv) {
    jscoro *co = kcalloc(1, sizeof(jscoro));
    if (!co) panic("JS: memória esgotada (corrotina)");
    co->owner = owner;
    co->fn = fn;
    co->thisv = thisv;
    co->args = js_arr_from(argv, argc);
    co->in_val = co->out_val = co->ret_val = JS_UNDEF;
    co->state = CO_FRESH;
    co->next = coros;
    if (coros) coros->prev = co;
    coros = co;
    owner->u.gen.co = co;
    return co;
}

void js_coro_free(jscoro *co) {
    if (!co) return;
    if (co->prev) co->prev->next = co->next; else coros = co->next;
    if (co->next) co->next->prev = co->prev;
    kfree(co->stack);
    kfree(co);
}

void js_coro_mark_owner(jsobj *o) {
    jscoro *co = o->u.gen.co;
    if (!co) return;
    gc_mark_val(co->fn);
    gc_mark_val(co->thisv);
    gc_mark_ptr(co->args);
    gc_mark_val(co->in_val);
    gc_mark_val(co->out_val);
    gc_mark_val(co->ret_val);
    gc_mark_ptr(co->promise);
    gc_mark_ptr(co->queue);
    gc_mark_ptr(co->ctx.script);
    gc_mark_ptr(co->ctx.plabel);
    gc_mark_ptr(co->ctx.label);
    gc_mark_ptr(co->ctx.srcname);
}

void js_coro_gc_begin(void) {
    for (jscoro *co = coros; co; co = co->next) co->scanned = false;
    gc_mark_ptr(genret_marker);
}

/* corrotinas ativas (quem retomou quem): cada uma deixou sua pilha em [caller_sp, caller_base) */
void js_coro_scan_chain(void (*scan)(uptr lo, uptr hi)) {
    for (jscoro *co = J.cur_coro; co; co = co->caller)
        if (co->caller_sp && co->caller_base) scan((uptr)co->caller_sp, co->caller_base);
}

/* pilhas suspensas cujo dono já foi marcado; devolve true se varreu alguma */
bool js_coro_scan(void (*scan)(uptr lo, uptr hi)) {
    bool any = false;
    for (jscoro *co = coros; co; co = co->next) {
        if (co->state != CO_SUSPENDED || co->scanned || !co->owner->h.mark) continue;
        co->scanned = true;
        scan((uptr)co->sp, (uptr)(co->stack + CO_STACK));
        any = true;
    }
    return any;
}

static void co_resume(jscoro *co, int mode, jsv val) {
    jsobj *volatile keep = co->owner;
    if (!co->stack) {
        co->stack = kmalloc(CO_STACK);
        if (!co->stack) panic("JS: memória esgotada (pilha de corrotina)");
        uptr top = ((uptr)co->stack + CO_STACK) & ~(uptr)15;
        u64 *sp = (u64 *)(top - 264);
        memset(sp, 0, 264);
        sp[216 / 8] = (u64)(uptr)co;                 /* rbx = co */
        sp[232 / 8] = (u64)(uptr)js_coro_tramp;      /* endereço de retorno */
        co->sp = sp;
    }
    jsictx saved;
    js_ictx_save(&saved);
    co->caller = J.cur_coro;
    co->caller_base = J.stack_base;
    co->caller_limit = J.stack_limit;
    co->in_mode = mode;
    co->in_val = val;
    if (co->state == CO_SUSPENDED) js_ictx_restore(&co->ctx);
    J.cur_coro = co;
    J.stack_base = ((uptr)co->stack + CO_STACK) & ~(uptr)15;
    J.stack_limit = CO_STACK - CO_MARGIN;
    co->state = CO_RUNNING;
    js_coro_switch(&co->caller_sp, co->sp);
    J.cur_coro = co->caller;
    J.stack_base = co->caller_base;
    J.stack_limit = co->caller_limit;
    co->caller_sp = NULL;
    co->caller = NULL;
    js_ictx_restore(&saved);
    if (co->state == CO_DONE) { kfree(co->stack); co->stack = NULL; }
    (void)keep;
}

static void co_suspend(jscoro *co, int kind, jsv val) {
    co->out_kind = kind;
    co->out_val = val;
    js_ictx_save(&co->ctx);
    co->state = CO_SUSPENDED;
    js_coro_switch(&co->sp, co->caller_sp);
}

void js_coro_entry(jscoro *co) {
    jsobj *args = co->args;
    jsv r = js_call_body(jv_o(co->fn), co->thisv, (int)args->alen, args->arr, JS_UNDEF);
    if (r == JS_EXC) {
        jsv e = js_catch();
        if (js_is_genreturn(e)) { co->out_kind = OUT_RETURN; co->out_val = co->ret_val; }
        else { co->out_kind = OUT_THROW; co->out_val = e; }
    } else {
        co->out_kind = OUT_RETURN;
        co->out_val = r;
    }
    co->state = CO_DONE;
    js_coro_switch(&co->sp, co->caller_sp);
    for (;;) {}
}

bool js_is_genreturn(jsv e) { return genret_marker && e == jv_obj(genret_marker); }

/* ============================================================ geradores */
jsv js_gen_create(jsobj *f, jsv thisv, int argc, jsv *argv) {
    jsv pr = js_getk(jv_obj(f), A[A_prototype]);
    if (pr == JS_EXC) return JS_EXC;
    jsobj *g = js_newobj_cls(CL_GEN, jv_isobj(pr) ? jv_o(pr) : J.realm->p_gen);
    coro_new(g, jv_obj(f), thisv, argc, argv);
    return jv_obj(g);
}

/* retoma o gerador; *done indica fim; JS_EXC em exceção */
static jsv gen_resume(jsobj *g, int mode, jsv val, bool *done) {
    jscoro *co = g->u.gen.co;
    *done = true;
    if (!co || co->is_async) return js_throw_err(E_TYPE, "Este objeto não é um gerador");
    if (co->state == CO_RUNNING) return js_throw_err(E_TYPE, "O gerador já está em execução");
    if (co->state == CO_FRESH && mode != IN_NEXT) co->state = CO_DONE;
    if (co->state == CO_DONE) {
        if (mode == IN_THROW) return js_throw(val);
        return mode == IN_RETURN ? val : JS_UNDEF;
    }
    if (!js_check_stack()) return JS_EXC;
    co_resume(co, mode, val);
    switch (co->out_kind) {
    case OUT_YIELD: *done = false; return co->out_val;
    case OUT_RETURN: return co->out_val;
    default: return js_throw(co->out_val);
    }
}

int js_gen_step(jsobj *g, jsv *out) {
    bool done;
    jsv v = gen_resume(g, IN_NEXT, JS_UNDEF, &done);
    if (v == JS_EXC) return -1;
    if (done) return 0;
    *out = v;
    return 1;
}

static jsv iter_result(jsv v, bool done) {
    jsobj *r = js_newobj(J.realm->p_object);
    js_defprop(r, A[A_value], v, 0);
    js_defprop(r, A[A_done], jv_bool(done), 0);
    return jv_obj(r);
}

NATIVE(genp_op) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_GEN)) return js_throw_err(E_TYPE, "%s chamado em algo que não é gerador",
                                                     self->u.nat.magic == 0 ? "next" : self->u.nat.magic == 1 ? "throw" : "return");
    bool done;
    jsv v = gen_resume(jv_o(thisv), self->u.nat.magic, ARG(0), &done);
    if (v == JS_EXC) return JS_EXC;
    return iter_result(v, done);
}

jsv js_yield(jsv v) {
    jscoro *co = J.cur_coro;
    if (!co || (co->is_async && !co->is_asyncgen)) return js_throw_err(E_SYNTAX, "'yield' fora de um gerador");
    if (co->is_asyncgen) {
        /* em geradores async, o valor entregue é esperado antes */
        v = js_await(v);
        if (v == JS_EXC) return JS_EXC;
    }
    co_suspend(co, OUT_YIELD, v);
    switch (co->in_mode) {
    case IN_THROW: return js_throw(co->in_val);
    case IN_RETURN: co->ret_val = co->in_val; return js_throw(jv_obj(genret_marker));
    default: return co->in_val;
    }
}

jsv js_yield_star(jsv iterable) {
    jscoro *co = J.cur_coro;
    if (!co || (co->is_async && !co->is_asyncgen)) return js_throw_err(E_SYNTAX, "'yield*' fora de um gerador");
    if (co->is_asyncgen) {
        /* yield* em gerador async: delega a um iterável assíncrono ou síncrono */
        jsv itf = jv_isnullish(iterable) ? JS_UNDEF : js_getk(iterable, A[A_sym_asyncIterator]);
        if (itf == JS_EXC) return JS_EXC;
        bool asy = js_iscallable(itf);
        jsv it = asy ? js_call(itf, iterable, 0, NULL) : js_iter_open(iterable);
        if (it == JS_EXC) return JS_EXC;
        for (;;) {
            jsv x;
            if (asy) {
                jsv r = js_invoke(it, A[A_next], 0, NULL);
                if (r == JS_EXC) return JS_EXC;
                r = js_await(r);
                if (r == JS_EXC) return JS_EXC;
                if (!jv_isobj(r)) return js_throw_err(E_TYPE, "O resultado de next() não é um objeto");
                jsv d = js_getk(r, A[A_done]);
                if (d == JS_EXC) return JS_EXC;
                x = js_getk(r, A[A_value]);
                if (x == JS_EXC) return JS_EXC;
                if (js_truthy(d)) return x;
            } else {
                int s = js_iter_step(it, &x);
                if (s < 0) return JS_EXC;
                if (!s) return JS_UNDEF;
            }
            if (js_yield(x) == JS_EXC) return JS_EXC;
        }
    }
    if (jv_iscls(iterable, CL_GEN)) {
        jsobj *inner = jv_o(iterable);
        int mode = IN_NEXT;
        jsv val = JS_UNDEF;
        for (;;) {
            bool done;
            jsv r = gen_resume(inner, mode, val, &done);
            if (r == JS_EXC) return JS_EXC;
            if (done) {
                if (mode == IN_RETURN) { co->ret_val = r; return js_throw(jv_obj(genret_marker)); }
                return r;
            }
            co_suspend(co, OUT_YIELD, r);
            mode = co->in_mode;
            val = co->in_val;
        }
    }
    jsv it = js_iter_open(iterable);
    if (it == JS_EXC) return JS_EXC;
    for (;;) {
        jsv x;
        int s = js_iter_step(it, &x);
        if (s < 0) return JS_EXC;
        if (!s) return JS_UNDEF;
        co_suspend(co, OUT_YIELD, x);
        if (co->in_mode == IN_THROW) return js_throw(co->in_val);
        if (co->in_mode == IN_RETURN) { co->ret_val = co->in_val; return js_throw(jv_obj(genret_marker)); }
    }
}

/* ============================================================ async / await */
static void async_step(jscoro *co, int mode, jsv val);

NATIVE(async_resume) {
    UNUSED_ARGS;
    jsobj *owner = jv_o(self->u.nat.data);
    jscoro *co = owner->u.gen.co;
    if (co && co->state == CO_SUSPENDED) async_step(co, self->u.nat.magic ? IN_THROW : IN_NEXT, ARG(0));
    return JS_UNDEF;
}

static void async_step(jscoro *co, int mode, jsv val) {
    jsobj *volatile keep = co->owner;
    co_resume(co, mode, val);
    switch (co->out_kind) {
    case OUT_AWAIT: {
        jsobj *p = js_promise_from(co->out_val);
        jsobj *ok = js_newnative(async_resume, "", 1, 0), *bad = js_newnative(async_resume, "", 1, 1);
        ok->u.nat.data = bad->u.nat.data = jv_obj(co->owner);
        js_promise_then(p, jv_obj(ok), jv_obj(bad));
        break;
    }
    case OUT_RETURN: js_promise_resolve(co->promise, co->out_val); break;
    default: js_promise_reject(co->promise, co->out_val); break;
    }
    (void)keep;
}

jsv js_async_call(jsobj *f, jsv thisv, int argc, jsv *argv) {
    if (!js_check_stack()) return JS_EXC;
    jsobj *p = js_promise_new();
    jsobj *owner = js_newobj_cls(CL_GEN, NULL);
    jscoro *co = coro_new(owner, jv_obj(f), thisv, argc, argv);
    co->is_async = true;
    co->promise = p;
    async_step(co, IN_NEXT, JS_UNDEF);
    return jv_obj(p);
}

jsv js_await(jsv v) {
    jscoro *co = J.cur_coro;
    if (!co || !co->is_async) return js_throw_err(E_SYNTAX, "'await' só é válido dentro de funções async");
    co_suspend(co, OUT_AWAIT, v);
    if (co->in_mode == IN_THROW) return js_throw(co->in_val);
    return co->in_val;
}

/* ============================================================ geradores async */
static void ag_settle(jscoro *co, bool reject, jsv val) {
    jsobj *q = co->queue;
    if (!q->alen) return;
    jsobj *p = jv_o(q->arr[2]);
    memmove(q->arr, q->arr + 3, sizeof(jsv) * (q->alen - 3));
    q->alen -= 3;
    if (reject) js_promise_reject(p, val);
    else js_promise_resolve(p, val);
}

static void ag_drive(jscoro *co);

static void ag_handle(jscoro *co) {
    switch (co->out_kind) {
    case OUT_AWAIT: {
        extern jsv ag_resume_fn(jscoro *co, int magic);
        co->awaiting = true;
        jsobj *p = js_promise_from(co->out_val);
        js_promise_then(p, ag_resume_fn(co, 0), ag_resume_fn(co, 1));
        break;
    }
    case OUT_YIELD: ag_settle(co, false, iter_result(co->out_val, false)); break;
    case OUT_RETURN: ag_settle(co, false, iter_result(co->out_val, true)); break;
    default: ag_settle(co, true, co->out_val); break;
    }
}

NATIVE(ag_resume) {
    UNUSED_ARGS;
    jscoro *co = jv_o(self->u.nat.data)->u.gen.co;
    if (!co || !co->awaiting) return JS_UNDEF;
    jsobj *volatile keep = co->owner;
    co->awaiting = false;
    co_resume(co, self->u.nat.magic ? IN_THROW : IN_NEXT, ARG(0));
    ag_handle(co);
    ag_drive(co);
    (void)keep;
    return JS_UNDEF;
}

jsv ag_resume_fn(jscoro *co, int magic) {
    jsobj *f = js_newnative(ag_resume, "", 1, magic);
    f->u.nat.data = jv_obj(co->owner);
    return jv_obj(f);
}

static void ag_drive(jscoro *co) {
    jsobj *volatile keep = co->owner;
    while (co->queue->alen && co->state != CO_RUNNING && !co->awaiting) {
        int mode = (int)jv_n(co->queue->arr[0]);
        jsv val = co->queue->arr[1];
        if (co->state == CO_FRESH && mode != IN_NEXT) co->state = CO_DONE;
        if (co->state == CO_DONE) {
            if (mode == IN_THROW) ag_settle(co, true, val);
            else ag_settle(co, false, iter_result(mode == IN_RETURN ? val : JS_UNDEF, true));
            continue;
        }
        if (!js_check_stack()) { ag_settle(co, true, js_catch()); continue; }
        co_resume(co, mode, val);
        ag_handle(co);
    }
    (void)keep;
}

jsv js_asyncgen_create(jsobj *f, jsv thisv, int argc, jsv *argv) {
    jsv pr = js_getk(jv_obj(f), A[A_prototype]);
    if (pr == JS_EXC) return JS_EXC;
    jsobj *g = js_newobj_cls(CL_GEN, jv_isobj(pr) ? jv_o(pr) : J.realm->p_asyncgen);
    jscoro *co = coro_new(g, jv_obj(f), thisv, argc, argv);
    co->is_async = co->is_asyncgen = true;
    co->queue = js_newarr(0);
    return jv_obj(g);
}

NATIVE(agp_op) {
    UNUSED_ARGS;
    jsobj *p = js_promise_new();
    jscoro *co = jv_iscls(thisv, CL_GEN) ? jv_o(thisv)->u.gen.co : NULL;
    if (!co || !co->is_asyncgen) {
        js_promise_reject(p, jv_obj(js_make_err(E_TYPE, js_strz("Este objeto não é um gerador async"))));
        return jv_obj(p);
    }
    js_arr_push(co->queue, jv_int(self->u.nat.magic));
    js_arr_push(co->queue, ARG(0));
    js_arr_push(co->queue, jv_obj(p));
    ag_drive(co);
    return jv_obj(p);
}

NATIVE(ag_self) { UNUSED_ARGS; return thisv; }

/* ============================================================ montagem */
void js_gen_init(jsrealm *r) {
    if (!genret_marker) genret_marker = js_newobj(NULL);
    jsobj *gp = js_newobj(r->p_iter);
    r->p_gen = gp;
    js_defnative(gp, "next", genp_op, 1, IN_NEXT);
    js_defnative(gp, "throw", genp_op, 1, IN_THROW);
    js_defnative(gp, "return", genp_op, 1, IN_RETURN);
    js_defprop(gp, A[A_sym_toStringTag], jv_str(js_intern("Generator", -1)), PF_NOENUM | PF_RO);
    jsobj *ap = js_newobj(r->p_object);
    r->p_asyncgen = ap;
    js_defnative(ap, "next", agp_op, 1, IN_NEXT);
    js_defnative(ap, "throw", agp_op, 1, IN_THROW);
    js_defnative(ap, "return", agp_op, 1, IN_RETURN);
    js_defprop(ap, A[A_sym_asyncIterator], jv_obj(js_newnative(ag_self, "[Symbol.asyncIterator]", 0, 0)), PF_NOENUM);
    js_defprop(ap, A[A_sym_toStringTag], jv_str(js_intern("AsyncGenerator", -1)), PF_NOENUM | PF_RO);
}

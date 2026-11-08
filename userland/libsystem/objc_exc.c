/*
 * mOS libSystem — exceções Objective-C de verdade (@throw/@try/@catch/@finally), nas duas arquiteturas, como o
 * objc-exception.mm do objc4 da Apple: uma exceção ObjC é uma exceção C++ cujo objeto lançado é {id, typeinfo ObjC}.
 *  - objc_exception_throw: __cxa_allocate_exception + __cxa_throw da libc++abi do disco (/usr/lib/libc++.1.dylib, a
 *    mesma que os apps C++ usam), que desenrola pelo _Unwind_RaiseException do libSystem (libunwind do LLVM).
 *  - __objc_personality_v0 (a personalidade das funções ObjC com @try) é o __gxx_personality_v0 da libc++abi.
 *  - o typeinfo ObjC (OBJC_EHTYPE_$_Classe, OBJC_EHTYPE_id) tem o vtable objc_ehtype_vtable + 2, cujo can_catch casa
 *    pela hierarquia de classes; uma exceção C++ nunca casa com um @catch ObjC e uma ObjC só casa com catch(...) no C++.
 *  - não pega por ninguém: o terminate handler imprime nome e motivo, chama o NSSetUncaughtExceptionHandler e aborta.
 * Sem a libc++ no disco (harness mínimo) volta ao comportamento antigo: imprime e aborta no próprio throw.
 */
#include "objc.h"

void *dlopen(const char *path, int mode);
void *dlsym(void *h, const char *name);
void abort(void) __attribute__((noreturn));
int cx_fdprintf(int fd, const char *fmt, ...);
id objc_retain(id o);
void objc_release(id o);
const char *object_getClassName(id o);
void *NSGetUncaughtExceptionHandler(void);
void __cxa_throw(void *, void *, void (*)(void *));                 /* o do libSystem (libcxx.c): só para comparar */

typedef struct { void **vtable; const char *name; cx_class_t *cls; } objc_typeinfo_t;
typedef struct { id obj; objc_typeinfo_t tinfo; } objc_exception_t;

static struct {
    int state;                                                           /* 0 = não procurou, 1 = ok, -1 = sem libc++abi */
    void *(*allocate)(unsigned long);
    void (*throw_)(void *, void *, void (*)(void *));
    void *(*begin_catch)(void *);
    void (*end_catch)(void);
    void (*rethrow)(void) __attribute__((noreturn));
    int (*personality)(int, int, u64, void *, void *);
    void *(*current_type)(void);
    void *(*current_primary)(void);
    void (*decref)(void *);
    void *(*set_terminate)(void (*)(void));
    void (*terminate)(void) __attribute__((noreturn));
} X;
static void (*old_terminate)(void);

/* vtable do typeinfo ObjC no formato do std::type_info da libc++abi: [-2] deslocamento, [-1] typeinfo do typeinfo (um
 * __class_type_info qualquer: o dynamic_cast que a libc++abi faz sobre o tipo lançado volta nulo em vez de cair),
 * 2 destrutores, noop1/noop2, can_catch, search_above_dst, search_below_dst, has_unambiguous_public_base */
static long exc_false(void) { return 0; }
static void exc_noop(void) {}
static long exc_can_catch(const objc_typeinfo_t *catch_ti, const objc_typeinfo_t *throw_ti, void **adjusted);
void *objc_ehtype_vtable[] = { 0, 0, (void *)exc_noop, (void *)exc_noop, (void *)exc_false, (void *)exc_false,
                               (void *)exc_can_catch, (void *)exc_false, (void *)exc_false, (void *)exc_false };
objc_typeinfo_t OBJC_EHTYPE_id = { objc_ehtype_vtable + 2, "id", 0 };

static long exc_can_catch(const objc_typeinfo_t *catch_ti, const objc_typeinfo_t *throw_ti, void **adjusted) {
    if (!throw_ti || throw_ti->vtable != objc_ehtype_vtable + 2) return 0;   /* exceção C++: @catch ObjC não pega */
    id e = ((objc_exception_t *)*adjusted)->obj;
    if (catch_ti == &OBJC_EHTYPE_id) return 1;
    for (cx_class_t *c = e ? e->isa : 0; c; c = c->super) if (c == catch_ti->cls) return 1;
    return 0;
}

static void exc_terminate(void);
static int cxa_ready(void) {
    if (X.state) return X.state > 0;
    X.state = -1;
    void *h = dlopen("/usr/lib/libc++.1.dylib", 2);
    if (!h) return 0;
#define R(f, n) *(void **)&X.f = dlsym(h, n)
    R(allocate, "__cxa_allocate_exception"); R(throw_, "__cxa_throw"); R(begin_catch, "__cxa_begin_catch");
    R(end_catch, "__cxa_end_catch"); R(rethrow, "__cxa_rethrow"); R(personality, "__gxx_personality_v0");
    R(current_type, "__cxa_current_exception_type"); R(current_primary, "__cxa_current_primary_exception");
    R(decref, "__cxa_decrement_exception_refcount"); R(set_terminate, "_ZSt13set_terminatePFvvE"); R(terminate, "_ZSt9terminatev");
#undef R
    /* o libc++ do disco faltando, o carregador devolve o libSystem: as funções falsas de libcxx.c não servem */
    if (!X.allocate || !X.throw_ || !X.begin_catch || !X.end_catch || !X.personality || (void *)X.throw_ == (void *)__cxa_throw) return 0;
    void *ti = dlsym(h, "_ZTISt9type_info");
    if (ti) objc_ehtype_vtable[1] = ti;
    if (X.set_terminate) old_terminate = (void (*)(void))X.set_terminate(exc_terminate);
    X.state = 1;
    return 1;
}

static void exc_destructor(void *p) { objc_release(((objc_exception_t *)p)->obj); }

static const char *str_of(id o) {
    if (!o) return 0;
    id s = MSG0(id, o, "description");
    return s ? MSG0(const char *, s, "UTF8String") : 0;
}
static void report(id e) {
    if (e && cx_responds(e->isa, sel_registerName("name")) && cx_responds(e->isa, sel_registerName("reason"))) {
        const char *n = str_of(MSG0(id, e, "name")), *r = str_of(MSG0(id, e, "reason"));
        cx_fdprintf(2, "*** Terminating app due to uncaught exception '%s', reason: '%s'\n", n ? n : "?", r ? r : "(nil)");
    } else {
        const char *d = str_of(e);
        cx_fdprintf(2, "*** Terminating app due to uncaught exception of class '%s': %s\n", object_getClassName(e), d ? d : "nil");
    }
}

/* terminate de uma exceção ObjC que ninguém pegou: o tratador do app (NSSetUncaughtExceptionHandler), o aviso e abort */
static void exc_terminate(void) {
    const objc_typeinfo_t *t = X.current_type ? X.current_type() : 0;
    if (t && t->vtable == objc_ehtype_vtable + 2 && X.current_primary) {
        objc_exception_t *x = X.current_primary();
        id e = x ? x->obj : 0;
        void (*h)(id) = (void (*)(id))NSGetUncaughtExceptionHandler();
        if (h) h(e);
        report(e);
        abort();
    }
    if (old_terminate) old_terminate();
    abort();
}

typedef id (*objc_exc_pre_t)(id);
objc_exc_pre_t cx_objc_exception_preprocessor(void);                 /* chrome.c: objc_setExceptionPreprocessor */

__attribute__((noreturn)) void objc_exception_throw(id e) {
    objc_exc_pre_t pre = cx_objc_exception_preprocessor();
    if (pre) e = pre(e);
    if (!cxa_ready()) {                                                  /* sem libc++abi: como antes */
        report(e);
        abort();
    }
    objc_exception_t *x = X.allocate(sizeof *x);
    x->obj = objc_retain(e);
    x->tinfo.vtable = objc_ehtype_vtable + 2;
    x->tinfo.name = object_getClassName(e);
    x->tinfo.cls = e ? e->isa : 0;
    X.throw_(x, &x->tinfo, exc_destructor);
    abort();
}
__attribute__((noreturn)) void objc_exception_rethrow(void) {
    if (cxa_ready() && X.rethrow) X.rethrow();
    abort();
}
void *objc_begin_catch(void *exc) {
    if (!cxa_ready()) return exc;
    objc_exception_t *x = X.begin_catch(exc);
    return x ? x->obj : 0;
}
void objc_end_catch(void) { if (cxa_ready()) X.end_catch(); }
void objc_terminate(void) { if (cxa_ready() && X.terminate) X.terminate(); abort(); }
/* personalidade das funções ObjC com @try/@catch/@finally: a do C++ (o LSDA é o mesmo) */
int __objc_personality_v0(int version, int actions, u64 cls, void *exc, void *ctx) {
    if (!cxa_ready()) return 8;                                          /* _URC_CONTINUE_UNWIND */
    return X.personality(version, actions, cls, exc, ctx);
}
/* objc_exception_get/set_functions e afins (runtime antigo de 32 bits) não existem no ABI 2 */

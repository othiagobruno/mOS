/* mOS libSystem — runtime de Objective-C (ABI 2 da Apple, x86_64): estruturas internas */
#pragma once
#include "sys.h"

typedef struct cx_class cx_class_t;
typedef struct objc_object { cx_class_t *isa; } *id;
typedef const char *SEL;            /* seletores únicos: ponteiros canônicos para o nome */
typedef id (*IMP)(id, SEL, ...);
typedef signed char BOOL;
#define YES ((BOOL)1)
#define NO ((BOOL)0)
#define nil ((id)0)

/* layout binário de uma classe (igual ao do macOS) */
struct cx_class {
    cx_class_t *isa;
    cx_class_t *super;
    void *cache;
    void *vtable;                   /* nosso: ponteiro para cx_rw_t */
    u64 data;                       /* class_ro_t* (classes compiladas) */
};

typedef struct { SEL sel; IMP imp; const char *types; } cx_meth_t;

typedef struct cx_rw {
    u32 magic;
    u32 meta;
    const char *name;
    u32 size;                       /* tamanho das instâncias */
    cx_meth_t *m;
    u32 nm, capm;
    struct { SEL sel; IMP imp; } *cache;
    u32 cache_mask, cache_used;
    cx_class_t *cls;
    struct { const char *name; u32 *offset; const char *type; } *ivars;
    u32 nivars;
    struct { const char *name; const char *attrs; } *props;
    u32 nprops;
    u32 cxx_ctor;                   /* .cxx_construct na cadeia: 0 = ainda não visto, 1 = nenhum, 2 = há (objc.c) */
} cx_rw_t;

#define RW_MAGIC 0x52575843u

/* API interna usada por foundation.c / appkit.c */
SEL sel_registerName(const char *name);
cx_class_t *cx_define_class(cx_class_t *cls, cx_class_t *meta, const char *name, cx_class_t *super, u32 extra_size);
void cx_add_method(cx_class_t *cls, const char *sel, void *imp);
void cx_add_cmethod(cx_class_t *cls, const char *sel, void *imp);
IMP cx_lookup(cx_class_t *cls, SEL sel);
cx_class_t *cx_class_named(const char *name);
const char *cx_class_name(cx_class_t *cls);
u32 cx_instance_size(cx_class_t *cls);
id cx_alloc_instance(cx_class_t *cls);
BOOL cx_responds(cx_class_t *cls, SEL sel);
BOOL cx_is_subclass(cx_class_t *cls, cx_class_t *of);
cx_class_t *object_getClass(id o);
id objc_msgSend(id self, SEL op, ...);

/* atalho para mensagens a partir do C (sem stret) */
#if defined(__aarch64__)
/* arm64: no Darwin PCS os variádicos vão para a pilha e o IMP lê os registradores, então o objc_msgSend é chamado com o
 * protótipo exato, montado dos tipos dos argumentos (até 10) */
#define CX_MSG_N_(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define CX_MSG_N(...) CX_MSG_N_(0, ##__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define CX_MSG_CAT_(a, b) a##b
#define CX_MSG_CAT(a, b) CX_MSG_CAT_(a, b)
#ifdef CX_FLOAT_PROBE
extern struct cx_float_probe_t { int x; } cx_float_probe_error;
#define CX_TY(x) __typeof__(_Generic((x), float: cx_float_probe_error, default: (x)))
#else
#define CX_TY(x) __typeof__(x)
#endif
#define CX_MSG_0(T, o, s) MSG0(T, o, s)
#define CX_MSG_1(T, o, s, a) ((T (*)(id, SEL, CX_TY(a)))objc_msgSend)((id)(o), sel_registerName(s), a)
#define CX_MSG_2(T, o, s, a, b) ((T (*)(id, SEL, CX_TY(a), CX_TY(b)))objc_msgSend)((id)(o), sel_registerName(s), a, b)
#define CX_MSG_3(T, o, s, a, b, c) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c)
#define CX_MSG_4(T, o, s, a, b, c, d) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d)
#define CX_MSG_5(T, o, s, a, b, c, d, e) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d), CX_TY(e)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d, e)
#define CX_MSG_6(T, o, s, a, b, c, d, e, f) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d), CX_TY(e), CX_TY(f)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d, e, f)
#define CX_MSG_7(T, o, s, a, b, c, d, e, f, g) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d), CX_TY(e), CX_TY(f), CX_TY(g)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d, e, f, g)
#define CX_MSG_8(T, o, s, a, b, c, d, e, f, g, h) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d), CX_TY(e), CX_TY(f), CX_TY(g), CX_TY(h)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d, e, f, g, h)
#define CX_MSG_9(T, o, s, a, b, c, d, e, f, g, h, i) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d), CX_TY(e), CX_TY(f), CX_TY(g), CX_TY(h), CX_TY(i)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d, e, f, g, h, i)
#define CX_MSG_10(T, o, s, a, b, c, d, e, f, g, h, i, j) ((T (*)(id, SEL, CX_TY(a), CX_TY(b), CX_TY(c), CX_TY(d), CX_TY(e), CX_TY(f), CX_TY(g), CX_TY(h), CX_TY(i), CX_TY(j)))objc_msgSend)((id)(o), sel_registerName(s), a, b, c, d, e, f, g, h, i, j)
#define MSG(T, obj, sel, ...) CX_MSG_CAT(CX_MSG_, CX_MSG_N(__VA_ARGS__))(T, obj, sel, ##__VA_ARGS__)
#else
#define MSG(T, obj, sel, ...) ((T (*)(id, SEL, ...))objc_msgSend)((id)(obj), sel_registerName(sel), ##__VA_ARGS__)
#endif
#define MSG0(T, obj, sel) ((T (*)(id, SEL))objc_msgSend)((id)(obj), sel_registerName(sel))

/* chamadas no fim da inicialização do runtime */
void cx_foundation_init(void);
void cx_appkit_init(void);
void cx_objc_init_image(u64 sects, u64 n);

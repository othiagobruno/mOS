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
#define MSG(T, obj, sel, ...) ((T (*)(id, SEL, ...))objc_msgSend)((id)(obj), sel_registerName(sel), ##__VA_ARGS__)
#define MSG0(T, obj, sel) ((T (*)(id, SEL))objc_msgSend)((id)(obj), sel_registerName(sel))

/* chamadas no fim da inicialização do runtime */
void cx_foundation_init(void);
void cx_appkit_init(void);
void cx_objc_init_image(u64 sects, u64 n);

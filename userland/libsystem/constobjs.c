/*
 * mOS libSystem — NSConstantArray e NSConstantDictionary: os objetos que o clang dos SDKs recentes emite
 * no __DATA do app para os literais @[...] e @{...} (e que o Electron/Chromium importa). O layout é o
 * do clang, diferente do NSArray/NSDictionary do mOS:
 *     NSConstantArray      { isa; NSUInteger count; const id *objects; }
 *     NSConstantDictionary { isa; NSUInteger options; NSUInteger count; const id *keys; const id *objects; }
 * As duas classes herdam de NSArray/NSDictionary (isKindOfClass: continua certo) e, para cada método
 * de instância da superclasse, recebem um trampolim que troca self por uma cópia real em cache
 * (construída uma vez por objeto constante) e salta para objc_msgSend: vale para qualquer assinatura.
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
int strcmp(const char *a, const char *b);
id objc_retain(id o);

cx_class_t NSConstantArray_c __asm__("OBJC_CLASS_$_NSConstantArray");
cx_class_t NSConstantArray_m __asm__("OBJC_METACLASS_$_NSConstantArray");
cx_class_t NSConstantDictionary_c __asm__("OBJC_CLASS_$_NSConstantDictionary");
cx_class_t NSConstantDictionary_m __asm__("OBJC_METACLASS_$_NSConstantDictionary");

typedef struct { cx_class_t *isa; u64 count; const id *objects; } kca_t;
typedef struct { cx_class_t *isa; u64 options; u64 count; const id *keys; const id *objects; } kcd_t;

/* ---- cache objeto constante -> cópia real (nunca liberada: os constantes vivem o processo todo) */
#define KC_BUCKETS 1024
typedef struct kc_ent { const void *k; id real; struct kc_ent *next; } kc_ent_t;
static kc_ent_t *kc_tab[KC_BUCKETS];
static volatile int kc_lock;
static void lk(void) { while (__atomic_exchange_n(&kc_lock, 1, __ATOMIC_ACQUIRE)) {} }
static void ulk(void) { __atomic_store_n(&kc_lock, 0, __ATOMIC_RELEASE); }

static id build_real(id self) {
    cx_class_t *c = object_getClass(self);
    if (c == &NSConstantArray_c) {
        kca_t *a = (kca_t *)self;
        return MSG(id, (id)cx_class_named("NSArray"), "arrayWithObjects:count:", a->objects, a->count);
    }
    kcd_t *d = (kcd_t *)self;
    return MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObjects:forKeys:count:", d->objects, d->keys, d->count);
}
/* chamada pelo trampolim: devolve o objeto real que responde no lugar do constante */
id cx_const_real(id self) {
    if (!self) return self;
    u64 h = ((u64)(usize)self >> 4) & (KC_BUCKETS - 1);
    lk();
    for (kc_ent_t *e = kc_tab[h]; e; e = e->next) if (e->k == self) { id r = e->real; ulk(); return r; }
    ulk();
    id r = objc_retain(build_real(self));      /* fora da trava: build_real manda mensagens */
    lk();
    for (kc_ent_t *e = kc_tab[h]; e; e = e->next) if (e->k == self) { id o = e->real; ulk(); return o; }
    kc_ent_t *e = malloc(sizeof *e);
    if (e) { e->k = self; e->real = r; e->next = kc_tab[h]; kc_tab[h] = e; }
    ulk();
    return r;
}

/* trampolim: guarda os registradores de argumento (inteiros, xmm0-7 e al dos variádicos), troca rdi
 * pelo objeto real e salta para objc_msgSend com o mesmo seletor (rsi) e argumentos */
void cx_const_fwd(void);
#if defined(__aarch64__)
/* arm64: x0..x8 (x8 = resultado indireto) e q0..q7; os variádicos do Darwin estão na pilha do chamador, intacta */
__asm__(
    ".text\n"
    ".globl cx_const_fwd\n"
    ".p2align 2\n"
    "cx_const_fwd:\n"
    "  stp x29, x30, [sp, #-16]!\n  mov x29, sp\n  sub sp, sp, #208\n"
    "  stp x0, x1, [sp]\n  stp x2, x3, [sp, #16]\n  stp x4, x5, [sp, #32]\n  stp x6, x7, [sp, #48]\n  str x8, [sp, #64]\n"
    "  stp q0, q1, [sp, #80]\n  stp q2, q3, [sp, #112]\n  stp q4, q5, [sp, #144]\n  stp q6, q7, [sp, #176]\n"
    "  bl cx_const_real\n"
    "  mov x9, x0\n"
    "  ldp q0, q1, [sp, #80]\n  ldp q2, q3, [sp, #112]\n  ldp q4, q5, [sp, #144]\n  ldp q6, q7, [sp, #176]\n"
    "  ldp x0, x1, [sp]\n  ldp x2, x3, [sp, #16]\n  ldp x4, x5, [sp, #32]\n  ldp x6, x7, [sp, #48]\n  ldr x8, [sp, #64]\n"
    "  mov x0, x9\n"
    "  mov sp, x29\n  ldp x29, x30, [sp], #16\n"
    "  b objc_msgSend\n");
#else
__asm__(
    ".text\n"
    ".globl cx_const_fwd\n"
    "cx_const_fwd:\n"
    "  push %rax\n push %rdi\n push %rsi\n push %rdx\n push %rcx\n push %r8\n push %r9\n"
    "  sub $0x80, %rsp\n"
    "  movdqu %xmm0, 0x00(%rsp)\n movdqu %xmm1, 0x10(%rsp)\n movdqu %xmm2, 0x20(%rsp)\n movdqu %xmm3, 0x30(%rsp)\n"
    "  movdqu %xmm4, 0x40(%rsp)\n movdqu %xmm5, 0x50(%rsp)\n movdqu %xmm6, 0x60(%rsp)\n movdqu %xmm7, 0x70(%rsp)\n"
    "  call cx_const_real\n"
    "  movdqu 0x00(%rsp), %xmm0\n movdqu 0x10(%rsp), %xmm1\n movdqu 0x20(%rsp), %xmm2\n movdqu 0x30(%rsp), %xmm3\n"
    "  movdqu 0x40(%rsp), %xmm4\n movdqu 0x50(%rsp), %xmm5\n movdqu 0x60(%rsp), %xmm6\n movdqu 0x70(%rsp), %xmm7\n"
    "  add $0x80, %rsp\n"
    "  pop %r9\n pop %r8\n pop %rcx\n pop %rdx\n pop %rsi\n pop %rdi\n"
    "  mov %rax, %rdi\n"
    "  pop %rax\n"
    "  jmp objc_msgSend\n");
#endif

/* métodos que não vão para a cópia: ciclo de vida e identidade do próprio constante */
static int keep_local(const char *s) {
    static const char *const k[] = {"dealloc", "retain", "release", "autorelease", "retainCount", "class", "self",
                                    "isKindOfClass:", "isMemberOfClass:", "respondsToSelector:", "conformsToProtocol:",
                                    "copy", "copyWithZone:", "_tryRetain", "_isDeallocating", "allowsWeakReference",
                                    "retainWeakReference", "zone", 0};
    for (int i = 0; k[i]; i++) if (!strcmp(s, k[i])) return 1;
    return 0;
}
static id k_self(id self, SEL c) { (void)c; return self; }
static id k_copy_zone(id self, SEL c, void *z) { (void)c; (void)z; return self; }
static u64 k_count_a(id self, SEL c) { (void)c; return ((kca_t *)self)->count; }
static id k_at(id self, SEL c, u64 i) { (void)c; kca_t *a = (kca_t *)self; return i < a->count ? a->objects[i] : 0; }
static u64 k_count_d(id self, SEL c) { (void)c; return ((kcd_t *)self)->count; }

static void wire(cx_class_t *k, cx_class_t *super) {
    cx_rw_t *rw = super ? (cx_rw_t *)super->vtable : 0;
    if (!rw || rw->magic != RW_MAGIC) return;
    for (u32 i = 0; i < rw->nm; i++) {
        SEL s = rw->m[i].sel;
        if (!s || keep_local(s)) continue;
        cx_add_method(k, s, (void *)cx_const_fwd);
    }
    cx_add_method(k, "copy", (void *)k_self);
    cx_add_method(k, "copyWithZone:", (void *)k_copy_zone);
}
void cx_const_objs_init(void) {
    cx_class_t *arr = cx_class_named("NSArray"), *dict = cx_class_named("NSDictionary");
    if (arr && !cx_class_named("NSConstantArray")) {
        cx_define_class(&NSConstantArray_c, &NSConstantArray_m, "NSConstantArray", arr, 0);
        wire(&NSConstantArray_c, arr);
        /* os mais usados respondem direto, sem montar a cópia */
        cx_add_method(&NSConstantArray_c, "count", (void *)k_count_a);
        cx_add_method(&NSConstantArray_c, "objectAtIndex:", (void *)k_at);
        cx_add_method(&NSConstantArray_c, "objectAtIndexedSubscript:", (void *)k_at);
    }
    if (dict && !cx_class_named("NSConstantDictionary")) {
        cx_define_class(&NSConstantDictionary_c, &NSConstantDictionary_m, "NSConstantDictionary", dict, 0);
        wire(&NSConstantDictionary_c, dict);
        cx_add_method(&NSConstantDictionary_c, "count", (void *)k_count_d);
    }
}

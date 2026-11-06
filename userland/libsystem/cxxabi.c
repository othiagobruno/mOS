/*
 * mOS libSystem — o mínimo do ABI de C++ (libc++abi / libc++ do macOS) que código C++ compilado com
 * -fno-exceptions -fno-rtti importa: operator new/delete (todas as variantes), estáticos locais
 * (__cxa_guard_*), std::call_once, funções virtuais puras e fmaf. Usado pelo OpenAttributeGraph e pelo
 * OpenRenderBox (dependências do OpenSwiftUI) e por apps C++ do macOS que não trazem a própria libc++.
 * Sem exceções: falta de memória aborta (como -fno-exceptions faria com std::bad_alloc).
 *
 * Os nomes são os símbolos Itanium do x86_64 (sem o "_" extra do Mach-O, que o carregador tira).
 */
#include "foundation.h"
#include "libc.h"
#include "../../kernel/include/syscall.h"

typedef unsigned long size_t_;

static void *checked(void *p) {
    if (!p) { cx_puts_fd(2, "libc++abi: falta de memória em operator new\n"); abort(); }
    return p;
}
static void *aligned_new(size_t_ n, size_t_ al) {
    if (al < 16) return checked(malloc(n ? n : 1));
    /* guarda o ponteiro original logo antes do bloco alinhado */
    unsigned char *raw = checked(malloc(n + al + sizeof(void *)));
    unsigned long p = ((unsigned long)raw + sizeof(void *) + al - 1) & ~(al - 1);
    ((void **)p)[-1] = raw;
    return (void *)p;
}
static void aligned_free(void *p, size_t_ al) {
    if (!p) return;
    if (al < 16) free(p); else free(((void **)p)[-1]);
}

/* operator new / new[] */
void *cxx_new(size_t_ n) __asm__("_Znwm");
void *cxx_new(size_t_ n) { return checked(malloc(n ? n : 1)); }
void *cxx_new_arr(size_t_ n) __asm__("_Znam");
void *cxx_new_arr(size_t_ n) { return checked(malloc(n ? n : 1)); }
void *cxx_new_nt(size_t_ n, const void *nt) __asm__("_ZnwmRKSt9nothrow_t");
void *cxx_new_nt(size_t_ n, const void *nt) { (void)nt; return malloc(n ? n : 1); }
void *cxx_new_arr_nt(size_t_ n, const void *nt) __asm__("_ZnamRKSt9nothrow_t");
void *cxx_new_arr_nt(size_t_ n, const void *nt) { (void)nt; return malloc(n ? n : 1); }
void *cxx_new_al(size_t_ n, size_t_ al) __asm__("_ZnwmSt11align_val_t");
void *cxx_new_al(size_t_ n, size_t_ al) { return aligned_new(n, al); }
void *cxx_new_arr_al(size_t_ n, size_t_ al) __asm__("_ZnamSt11align_val_t");
void *cxx_new_arr_al(size_t_ n, size_t_ al) { return aligned_new(n, al); }

/* operator delete / delete[] (com e sem tamanho, com e sem alinhamento) */
void cxx_del(void *p) __asm__("_ZdlPv");
void cxx_del(void *p) { free(p); }
void cxx_del_arr(void *p) __asm__("_ZdaPv");
void cxx_del_arr(void *p) { free(p); }
void cxx_del_sz(void *p, size_t_ n) __asm__("_ZdlPvm");
void cxx_del_sz(void *p, size_t_ n) { (void)n; free(p); }
void cxx_del_arr_sz(void *p, size_t_ n) __asm__("_ZdaPvm");
void cxx_del_arr_sz(void *p, size_t_ n) { (void)n; free(p); }
void cxx_del_nt(void *p, const void *nt) __asm__("_ZdlPvRKSt9nothrow_t");
void cxx_del_nt(void *p, const void *nt) { (void)nt; free(p); }
void cxx_del_arr_nt(void *p, const void *nt) __asm__("_ZdaPvRKSt9nothrow_t");
void cxx_del_arr_nt(void *p, const void *nt) { (void)nt; free(p); }
void cxx_del_al(void *p, size_t_ al) __asm__("_ZdlPvSt11align_val_t");
void cxx_del_al(void *p, size_t_ al) { aligned_free(p, al); }
void cxx_del_arr_al(void *p, size_t_ al) __asm__("_ZdaPvSt11align_val_t");
void cxx_del_arr_al(void *p, size_t_ al) { aligned_free(p, al); }
void cxx_del_sz_al(void *p, size_t_ n, size_t_ al) __asm__("_ZdlPvmSt11align_val_t");
void cxx_del_sz_al(void *p, size_t_ n, size_t_ al) { (void)n; aligned_free(p, al); }
void cxx_del_arr_sz_al(void *p, size_t_ n, size_t_ al) __asm__("_ZdaPvmSt11align_val_t");
void cxx_del_arr_sz_al(void *p, size_t_ n, size_t_ al) { (void)n; aligned_free(p, al); }

/* estáticos locais: guarda de 64 bits; byte 0 = já inicializado, byte 1 = inicialização em andamento */
int __cxa_guard_acquire(unsigned long *g) {
    volatile unsigned char *b = (volatile unsigned char *)g;
    for (;;) {
        if (__atomic_load_n(&b[0], __ATOMIC_ACQUIRE)) return 0;
        if (!__sync_lock_test_and_set(&b[1], 1)) {
            if (b[0]) { __sync_lock_release(&b[1]); return 0; }
            return 1;                                   /* quem chamou inicializa e depois chama release */
        }
        SYS0(SYS_YIELD);
    }
}
void __cxa_guard_release(unsigned long *g) {
    volatile unsigned char *b = (volatile unsigned char *)g;
    __atomic_store_n(&b[0], 1, __ATOMIC_RELEASE);
    __sync_lock_release(&b[1]);
}
void __cxa_guard_abort(unsigned long *g) { __sync_lock_release(&((volatile unsigned char *)g)[1]); }

/* std::__1::__call_once(volatile unsigned long& flag, void* arg, void (*fn)(void*)):
 * flag 0 = nunca rodou, 1 = rodando, ~0 = pronto (o mesmo protocolo da libc++) */
void cxx_call_once(volatile unsigned long *flag, void *arg, void (*fn)(void *)) __asm__("_ZNSt3__111__call_onceERVmPvPFvS2_E");
void cxx_call_once(volatile unsigned long *flag, void *arg, void (*fn)(void *)) {
    for (;;) {
        unsigned long v = __atomic_load_n(flag, __ATOMIC_ACQUIRE);
        if (v == ~0UL) return;
        if (v == 0 && __sync_bool_compare_and_swap(flag, 0UL, 1UL)) {
            fn(arg);
            __atomic_store_n(flag, ~0UL, __ATOMIC_RELEASE);
            return;
        }
        SYS0(SYS_YIELD);
    }
}

void __cxa_pure_virtual(void) { cx_puts_fd(2, "libc++abi: chamada de função virtual pura\n"); abort(); }
void __cxa_deleted_virtual(void) { cx_puts_fd(2, "libc++abi: chamada de função virtual removida\n"); abort(); }

/* produto de dois float é exato em double; uma única soma arredondada fica dentro do erro do fmaf */
float fmaf(float a, float b, float c) { return (float)((double)a * (double)b + (double)c); }

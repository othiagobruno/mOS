/*
 * mOS — tradutor arm64: escolha do motor e invalidação de traduções.
 * O JIT (frente JIT) entra definindo A64_HAVE_JIT na build e fornecendo jit*.c.
 */
#include "a64.h"

int a64_run(a64_cpu *c, int engine) {
#ifdef A64_HAVE_JIT
    if (engine == A64_ENGINE_JIT) return a64_jit_run(c);
#else
    (void)engine;
#endif
    return a64_interp_run(c, 0);
}

/* o harness não liga o a64rt: sem apelido. O runtime define a versão forte. */
__attribute__((weak)) int a64_jit_alias_span(a64_u64 addr, a64_u64 len, a64_u64 *lo, a64_u64 *hi) {
    (void)addr; (void)len; (void)lo; (void)hi;
    return 0;
}

void a64_invalidate(a64_u64 addr, a64_u64 len) {
#ifdef A64_HAVE_JIT
    a64_jit_invalidate(addr, len);
    /* a outra vista do mesmo físico (vm_remap sem cópia) também perde a tradução.
     * chama o JIT direto: a64_invalidate de novo reentraria neste gancho. */
    a64_u64 alo = 0, ahi = 0;
    if (len && a64_jit_alias_span(addr, len, &alo, &ahi) && ahi > alo)
        a64_jit_invalidate(alo, ahi - alo);
#else
    (void)addr; (void)len;
#endif
}

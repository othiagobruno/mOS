/*
 * Layout do __darwin_mcontext64 como o clang o calcula para aarch64-apple-macos.
 * Não liga com o a64rt (aquele cabeçalho é compilado no hospedeiro x86_64). Os dois lados têm de
 * concordar: neon 528, mcontext 816, fpsr no offset 800, fpcr no 804. O sigreturn do XNU rejeita
 * outro uc_mcsize (UC_FLAVOR_SIZE64 = soma dos sizeof).
 *
 *   clang -target aarch64-apple-macos11 -ffreestanding -c userland/tests-a64rt/mcontext_sdk.c -o /tmp/mcontext_sdk.o
 */
typedef struct { unsigned long long far; unsigned int esr, exception; } exc_t;
typedef struct {
    unsigned long long x[29], fp, lr, sp, pc;
    unsigned int cpsr, pad;
} thread_t;
typedef struct {
    unsigned __int128 v[32];
    unsigned int fpsr, fpcr;
} neon_t;
typedef struct {
    exc_t es;
    thread_t ss;
    neon_t ns;
} mcontext_t;

_Static_assert(sizeof(exc_t) == 16, "exception");
_Static_assert(sizeof(thread_t) == 272, "thread");
_Static_assert(sizeof(neon_t) == 528, "neon: 520 de dados + pad de cauda do __uint128_t");
_Static_assert(sizeof(mcontext_t) == 816, "mcontext");
_Static_assert(__builtin_offsetof(mcontext_t, ns) == 288, "ns");
_Static_assert(__builtin_offsetof(neon_t, fpsr) == 512, "fpsr");
_Static_assert(__builtin_offsetof(mcontext_t, ns.fpsr) == 800, "fpsr no mcontext");
_Static_assert(__builtin_offsetof(mcontext_t, ns.fpcr) == 804, "fpcr no mcontext");

int mcontext_sdk_ok;

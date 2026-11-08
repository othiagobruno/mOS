/* suíte diferencial C: atômicos C11 (8..128 bits, todas as ordens de memória) */
#include "rt.h"
#include <stdatomic.h>

static _Atomic uint8_t a8; static _Atomic uint16_t a16; static _Atomic uint32_t a32; static _Atomic uint64_t a64;
static _Atomic int32_t s32; static _Atomic int64_t s64;
static atomic_flag fl = ATOMIC_FLAG_INIT;
static unsigned __int128 a128 __attribute__((aligned(16)));

#define ROUND(A, T)                                                                                       \
    do {                                                                                                  \
        T v = (T)rnd64(), e, o;                                                                           \
        o = atomic_fetch_add_explicit(&A, v, memory_order_seq_cst); h_u(&h, (uint64_t)o);                 \
        o = atomic_fetch_sub_explicit(&A, (T)(v >> 1), memory_order_acq_rel); h_u(&h, (uint64_t)o);       \
        o = atomic_fetch_and_explicit(&A, (T)~((uint64_t)v << 3), memory_order_release); h_u(&h, (uint64_t)o); \
        o = atomic_fetch_or_explicit(&A, (T)(v >> 5), memory_order_acquire); h_u(&h, (uint64_t)o);        \
        o = atomic_fetch_xor_explicit(&A, v, memory_order_relaxed); h_u(&h, (uint64_t)o);                 \
        o = atomic_exchange(&A, (T)((uint64_t)v * 3)); h_u(&h, (uint64_t)o);                              \
        e = (T)((uint64_t)v * 3); h_u(&h, atomic_compare_exchange_strong(&A, &e, (T)((uint64_t)v + 1))); h_u(&h, (uint64_t)e); \
        e = (T)(v ^ 1); h_u(&h, atomic_compare_exchange_strong(&A, &e, (T)7)); h_u(&h, (uint64_t)e);      \
        T cur = atomic_load(&A);                                                                          \
        while (!atomic_compare_exchange_weak(&A, &cur, (T)((uint64_t)cur * 5 + 1))) {}                    \
        h_u(&h, (uint64_t)atomic_load_explicit(&A, memory_order_acquire));                                \
        atomic_store_explicit(&A, (T)(v >> 2), memory_order_release);                                     \
        h_u(&h, (uint64_t)atomic_load(&A));                                                               \
    } while (0)

static void run(void) {
    put_s("c_atom\n");
    hsh h; h_init(&h);
    for (int i = 0; i < 3000; i++) {
        ROUND(a8, uint8_t); ROUND(a16, uint16_t); ROUND(a32, uint32_t); ROUND(a64, uint64_t);
        ROUND(s32, int32_t); ROUND(s64, int64_t);
        h_u(&h, atomic_flag_test_and_set(&fl)); if (i & 1) atomic_flag_clear(&fl);
        atomic_thread_fence(memory_order_seq_cst);
        unsigned __int128 e = a128, n = ((unsigned __int128)rnd64() << 64) | rnd64();
        h_u(&h, __atomic_compare_exchange_n(&a128, &e, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
        unsigned __int128 l = __atomic_load_n(&a128, __ATOMIC_ACQUIRE); h_u(&h, (uint64_t)l); h_u(&h, (uint64_t)(l >> 64));
        e = n + 1; h_u(&h, __atomic_compare_exchange_n(&a128, &e, 0, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)); h_u(&h, (uint64_t)(e >> 64));
    }
    kv_x("atomicos", h.h);
    kv_u("a64", atomic_load(&a64)); kv_i("s32", atomic_load(&s32));
}

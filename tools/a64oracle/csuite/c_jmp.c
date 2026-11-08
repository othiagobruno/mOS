/* suíte diferencial C: saltos não locais (rt_setjmp/rt_longjmp de rt.h) */
#include "rt.h"

static rt_jmp_buf jb, jb2;
static volatile int depth_reached;
static __attribute__((noinline)) void dive(int n) {
    volatile char pad[64]; pad[0] = (char)n;
    depth_reached = pad[0];
    if (n == 37) rt_longjmp(jb, 1);
    dive(n + 1);
    pad[1] = 0;
}
static __attribute__((noinline)) void inner(int k) { if (k & 1) rt_longjmp(jb2, 1); }
static __attribute__((noinline)) int try_inner(int k) {
    if (rt_setjmp(jb2)) return -k;
    inner(k);
    return k;
}

static void run(void) {
    put_s("c_jmp\n");
    static volatile int count = 0;
    static volatile double d = 1.5;
    if (rt_setjmp(jb) == 0) { count = 10; d = 2.75; dive(0); put_s("nao deveria chegar\n"); }
    else { kv_u("profundidade", (uint64_t)depth_reached); kv_u("count", (uint64_t)count); kv_d("d", d); }
    hsh h; h_init(&h);
    for (int k = 0; k < 1000; k++) h_u(&h, (uint64_t)(int64_t)try_inner(k));
    kv_x("try/inner", h.h);
    static volatile uint64_t acc = 0;
    static volatile int i;
    for (i = 0; i < 200; i++) {
        if (rt_setjmp(jb) == 0) { acc = acc * 7 + (uint64_t)i; if (i % 5 == 0) dive(30); acc += 1; }
        else acc ^= 0xABCDEF;
    }
    kv_x("acc", acc);
}

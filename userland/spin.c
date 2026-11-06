/* mOS — ocupa a CPU sem nunca ceder: "spin <ms>" (0 = para sempre). Testa a preempção. */
#include "mos.h"

void _start(const char *arg) {
    u64 ms = 0;
    for (const char *p = arg; p && *p >= '0' && *p <= '9'; p++) ms = ms * 10 + (u64)(*p - '0');
    u64 t0 = time_ms(), iters = 0;
    volatile u64 x = 0;
    while (!ms || time_ms() - t0 < ms) { x += iters * 3; iters++; }
    print("spin: pid %d girou %u iterações em %u ms\n", (i64)getpid(), iters, ms);
    exit(0);
}

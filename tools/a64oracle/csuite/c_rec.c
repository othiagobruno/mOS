/* suíte diferencial C: recursão, ponteiros de função, tabelas de salto, variádicos, structs */
#include "rt.h"
#include <stdarg.h>

static uint64_t fib(unsigned n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
static uint64_t ack(uint64_t m, uint64_t n) { return m == 0 ? n + 1 : n == 0 ? ack(m - 1, 1) : ack(m - 1, ack(m, n - 1)); }
static uint64_t hanoi(int n, int a, int b, int c, uint64_t *h) {
    if (!n) return 0;
    uint64_t r = hanoi(n - 1, a, c, b, h);
    *h = *h * 31 + (uint64_t)(a * 3 + c);
    return r + 1 + hanoi(n - 1, c, b, a, h);
}
static int queens(int n, int row, uint32_t cols, uint32_t d1, uint32_t d2) {
    if (row == n) return 1;
    int c = 0;
    for (uint32_t avail = ~(cols | d1 | d2) & ((1u << n) - 1); avail; avail &= avail - 1) {
        uint32_t bit = avail & (0u - avail);
        c += queens(n, row + 1, cols | bit, (d1 | bit) << 1, (d2 | bit) >> 1);
    }
    return c;
}
static int is_even(unsigned n);
static int is_odd(unsigned n) { return n == 0 ? 0 : is_even(n - 1); }
static int is_even(unsigned n) { return n == 0 ? 1 : is_odd(n - 1); }
static __attribute__((noinline)) uint64_t deep(unsigned n, uint64_t acc) {
    volatile uint64_t pad[2] = { acc, n };
    if (!n) return acc;
    uint64_t r = deep(n - 1, pad[0] * 3 + pad[1]);
    return r ^ pad[1];                               /* não é cauda: a pilha cresce de verdade */
}

static int64_t op_add(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
static int64_t op_sub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
static int64_t op_mul(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }
static int64_t op_xor(int64_t a, int64_t b) { return a ^ b; }
static int64_t (*const OPSF[])(int64_t, int64_t) = { op_add, op_sub, op_mul, op_xor };

static __attribute__((noinline)) uint64_t sw(unsigned k, uint64_t v) {
    switch (k) {
    case 0: return v + 1; case 1: return v * 3; case 2: return v ^ 0xFF; case 3: return v >> 3; case 4: return v << 2;
    case 5: return ~v; case 6: return v - 7; case 7: return v | 0x100; case 8: return v & 0xF0F0; case 9: return v * v;
    case 10: return v / 3; case 11: return v % 1000; case 12: return v + 0x1234; case 13: return (v << 7) | (v >> 57);
    case 14: return v ^ (v >> 13); case 15: return 42; default: return v;
    }
}

static __attribute__((noinline)) double vsum(int n, ...) {
    va_list ap; va_start(ap, n); double s = 0;
    for (int i = 0; i < n; i++) {
        if (i % 3 == 0) s += va_arg(ap, int);
        else if (i % 3 == 1) s += va_arg(ap, double);
        else s += (double)va_arg(ap, int64_t);
    }
    va_end(ap); return s;
}

typedef struct { double x, y, z, w; } hfa4;
typedef struct { float a, b; } hfa2;
typedef struct { int64_t a, b, c, d, e; } big;
typedef struct { uint8_t a; uint16_t b; uint32_t c; } mix;
static __attribute__((noinline)) hfa4 hfa_add(hfa4 p, hfa4 q) { hfa4 r = { p.x + q.x, p.y * q.y, p.z - q.z, p.w / q.w }; return r; }
static __attribute__((noinline)) hfa2 hfa2_mul(hfa2 p, float k) { hfa2 r = { p.a * k, p.b - k }; return r; }
static __attribute__((noinline)) big big_rot(big b) { big r = { b.e, b.a, b.b, b.c, b.d }; return r; }
static __attribute__((noinline)) mix mix_f(mix m, int s) { mix r = { (uint8_t)(m.a + s), (uint16_t)(m.b * 3), m.c ^ (uint32_t)s }; return r; }
static __attribute__((noinline)) uint64_t many(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g,
                                               uint64_t h, uint64_t i, uint64_t j, double x, double y) {
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i + 10 * j + (uint64_t)(int64_t)(x * 100) + (uint64_t)(int64_t)(y * 1000);
}

static void run(void) {
    put_s("c_rec\n");
    kv_u("fib(27)", fib(27));
    kv_u("ack(2,3)", ack(2, 3)); kv_u("ack(3,6)", ack(3, 6));
    uint64_t hh = 0; kv_u("hanoi(16)", hanoi(16, 0, 1, 2, &hh)); kv_x("hanoi hash", hh);
    for (int n = 4; n <= 10; n++) { put_s("queens "); put_u((uint64_t)n); put_s(" = "); put_u((uint64_t)queens(n, 0, 0, 0, 0)); put_c('\n'); }
    kv_u("even(12345)", (uint64_t)is_even(12345)); kv_x("deep(20000)", deep(20000, 1));
    hsh h; h_init(&h);
    int64_t acc = 1;
    for (int i = 0; i < 10000; i++) { acc = OPSF[rnd64() & 3](acc, (int64_t)(rnd64() >> 40)); h_u(&h, (uint64_t)acc); }
    uint64_t v = 7; for (int i = 0; i < 10000; i++) { v = sw((unsigned)(rnd64() % 17), v); h_u(&h, v); }
    kv_x("ponteiros/switch", h.h);
    kv_d("variadico", vsum(9, 1, 2.5, (int64_t)-3, 4, 0.125, (int64_t)1 << 40, -7, 1e10, (int64_t)5));
    hfa4 a = { 1, 2, 3, 4 }, b = { 0.5, 0.25, -1, 8 }; hfa4 r = hfa_add(a, b);
    kv_d("hfa.x", r.x); kv_d("hfa.y", r.y); kv_d("hfa.z", r.z); kv_d("hfa.w", r.w);
    hfa2 p = { 1.5f, -2.0f }; p = hfa2_mul(p, 3.0f); kv_d("hfa2.a", p.a); kv_d("hfa2.b", p.b);
    big g = { 1, 2, 3, 4, 5 }; for (int i = 0; i < 3; i++) g = big_rot(g);
    kv_i("big", g.a * 10000 + g.b * 1000 + g.c * 100 + g.d * 10 + g.e);
    mix m = { 250, 40000, 0xDEADBEEF }; m = mix_f(m, 9); kv_u("mix.a", m.a); kv_u("mix.b", m.b); kv_x("mix.c", m.c);
    kv_u("many", many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1.25, -0.5));
}

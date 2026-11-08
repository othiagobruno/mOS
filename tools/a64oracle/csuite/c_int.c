/* suíte diferencial C: inteiros de 8 a 128 bits, overflow (builtins), divisão, shifts, bits */
#include "rt.h"

static const uint64_t SPEC[] = {
    0, 1, 2, 3, 7, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000ull,
    0xFFFFFFFFull, 0x100000000ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull,
    0xFFFFFFFFFFFFFFFEull, 0x5555555555555555ull, 0xAAAAAAAAAAAAAAAAull, 0x0123456789ABCDEFull, 12345, 1000000007,
};
#define NS (sizeof SPEC / sizeof SPEC[0])
static uint64_t val(unsigned i) { return i < NS ? SPEC[i] : rnd64() >> (rnd64() & 63); }

#define OPS(T, UT, NAME)                                                                                 \
    static void ops_##NAME(void) {                                                                   \
        hsh h; h_init(&h);                                                                           \
        const int W = (int)(sizeof(T) * 8);                                                          \
        for (unsigned i = 0; i < NS + 120; i++)                                                      \
            for (unsigned j = 0; j < NS + 40; j++) {                                                 \
                volatile T a = (T)val(i), b = (T)val(j + 7);                                         \
                h_u(&h, (uint64_t)(T)(UT)((uint64_t)a + (uint64_t)b)); h_u(&h, (uint64_t)(T)(UT)((uint64_t)a - (uint64_t)b)); h_u(&h, (uint64_t)(T)(UT)((uint64_t)(UT)a * (uint64_t)(UT)b)); \
                h_u(&h, (uint64_t)(T)(a & b)); h_u(&h, (uint64_t)(T)(a | b)); h_u(&h, (uint64_t)(T)(a ^ b)); \
                h_u(&h, (uint64_t)(T)~a); h_u(&h, a < b); h_u(&h, a <= b); h_u(&h, a == b);          \
                T s = (T)((uint64_t)b % (uint64_t)W);                                                \
                h_u(&h, (uint64_t)(T)(UT)((uint64_t)(UT)a << s)); h_u(&h, (uint64_t)(T)(a >> s));                      \
                if (b != 0 && !((T)-1 < 0 && b == (T)-1)) { h_u(&h, (uint64_t)(T)(a / b)); h_u(&h, (uint64_t)(T)(a % b)); } \
                T r;                                                                                 \
                h_u(&h, __builtin_add_overflow(a, b, &r)); h_u(&h, (uint64_t)r);                     \
                h_u(&h, __builtin_sub_overflow(a, b, &r)); h_u(&h, (uint64_t)r);                     \
                h_u(&h, __builtin_mul_overflow(a, b, &r)); h_u(&h, (uint64_t)r);                     \
            }                                                                                        \
        kv_x(#NAME, h.h);                                                                            \
    }
OPS(uint8_t, uint8_t, u8) OPS(int8_t, uint8_t, i8) OPS(uint16_t, uint16_t, u16) OPS(int16_t, uint16_t, i16)
OPS(uint32_t, uint32_t, u32) OPS(int32_t, uint32_t, i32) OPS(uint64_t, uint64_t, u64) OPS(int64_t, uint64_t, i64)

static void ops128(void) {
    hsh h; h_init(&h);
    for (unsigned i = 0; i < NS + 60; i++)
        for (unsigned j = 0; j < NS + 30; j++) {
            volatile unsigned __int128 a = ((unsigned __int128)val(i) << 64) | val(i + 3), b = ((unsigned __int128)val(j) << (j & 64)) | val(j + 5);
            volatile __int128 sa = (__int128)a, sb = (__int128)b;
            unsigned __int128 x;
            x = a + b; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64));
            x = a - b; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64));
            x = a * b; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64));
            unsigned s = (unsigned)(val(j) & 127);
            x = a << s; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64));
            x = a >> s; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64));
            x = (unsigned __int128)(sa >> s); h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64));
            if (b) { x = a / b; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64)); x = a % b; h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64)); }
            if (sb && !(sb == -1)) { x = (unsigned __int128)(sa / sb); h_u(&h, (uint64_t)x); h_u(&h, (uint64_t)(x >> 64)); }
            h_u(&h, sa < sb); h_u(&h, a < b);
        }
    kv_x("u128", h.h);
}

static void bits(void) {
    hsh h; h_init(&h);
    for (unsigned i = 0; i < 4000; i++) {
        uint64_t v = val(i % 60 == 0 ? i / 60 : 1000);
        uint32_t w = (uint32_t)v; uint16_t s = (uint16_t)v;
        h_u(&h, (uint64_t)__builtin_popcountll(v)); h_u(&h, (uint64_t)__builtin_popcount(w));
        if (v) { h_u(&h, (uint64_t)__builtin_clzll(v)); h_u(&h, (uint64_t)__builtin_ctzll(v)); }
        if (w) { h_u(&h, (uint64_t)__builtin_clz(w)); h_u(&h, (uint64_t)__builtin_ctz(w)); }
        h_u(&h, __builtin_bswap64(v)); h_u(&h, __builtin_bswap32(w)); h_u(&h, __builtin_bswap16(s));
        h_u(&h, __builtin_bitreverse64(v)); h_u(&h, __builtin_bitreverse32(w)); h_u(&h, __builtin_bitreverse8((uint8_t)v));
        unsigned r = (unsigned)(v >> 58);
        h_u(&h, (v << r) | (v >> ((64 - r) & 63))); h_u(&h, (uint64_t)((w >> (r & 31)) | (w << ((32 - (r & 31)) & 31))));
        h_u(&h, (uint64_t)(int64_t)(int8_t)v); h_u(&h, (uint64_t)(int64_t)(int16_t)v); h_u(&h, (uint64_t)(int64_t)(int32_t)v);
        h_u(&h, (uint64_t)__builtin_parityll(v)); h_u(&h, (uint64_t)__builtin_clrsbll((long long)v));
        /* campo de bits e extração/inserção */
        unsigned lo = r % 40, len = 1 + (unsigned)(v & 23);
        uint64_t m = (len >= 64) ? ~0ull : ((1ull << len) - 1);
        h_u(&h, (v >> lo) & m); h_u(&h, (v & ~(m << lo)) | ((w & m) << lo));
        int64_t sv = (int64_t)v; h_u(&h, (uint64_t)(sv >> (r & 63)));
        h_u(&h, (uint64_t)((__int128)(int64_t)v * (int64_t)w >> 64)); h_u(&h, (uint64_t)((unsigned __int128)v * w >> 64));
    }
    kv_x("bits", h.h);
}

struct bf { uint32_t a : 3, b : 7, c : 13; int32_t d : 5, e : 4; uint64_t f : 33; };
static void bitfields(void) {
    hsh h; h_init(&h);
    volatile struct bf x;
    for (unsigned i = 0; i < 2000; i++) {
        uint64_t v = rnd64();
        x.a = (uint32_t)v; x.b = (uint32_t)(v >> 3); x.c = (uint32_t)(v >> 10); x.d = (int32_t)(v >> 23); x.e = (int32_t)(v >> 28); x.f = v >> 31;
        h_u(&h, x.a); h_u(&h, x.b); h_u(&h, x.c); h_u(&h, (uint64_t)(int64_t)x.d); h_u(&h, (uint64_t)(int64_t)x.e); h_u(&h, x.f);
        x.c += x.b; x.d -= 3; h_u(&h, x.c); h_u(&h, (uint64_t)(int64_t)x.d);
    }
    kv_x("bitfields", h.h);
}

static void run(void) {
    put_s("c_int\n");
    ops_u8(); ops_i8(); ops_u16(); ops_i16(); ops_u32(); ops_i32(); ops_u64(); ops_i64();
    ops128(); bits(); bitfields();
    volatile int64_t m = -9223372036854775807ll - 1; volatile int64_t d = 7;
    kv_i("min/7", m / d); kv_i("min%7", m % d); kv_i("-7/2", (int64_t)-7 / 2); kv_i("-7%2", (int64_t)-7 % 2);
    volatile uint64_t u = 0xFFFFFFFFFFFFFFFFull; kv_u("umax/3", u / 3); kv_u("umax%10", u % 10);
}

/* frente FP: ponto flutuante escalar ponta a ponta (a64test × oráculo QEMU via tools/a64cmp.ps1).
 * Executa as instruções de FP escalar sobre uma tabela de casos de borda em todos os modos de
 * FPCR relevantes e imprime resultado + FPSR (+ NZCV para comparações) em hex. A verificação
 * exaustiva fica em tools/a64tests/fp_oracle/ (milhões de vetores); este teste garante que o
 * caminho real (decodificação no laço, MSR/MRS FPCR/FPSR, registradores V, flags) funciona. */
#include "a64t.h"

typedef unsigned long long u64;
typedef unsigned int u32;

static void put(const char *s) { a64t_puts(s); }
static void hex(u64 v) { a64t_hex(v); }
static void nl(void) { put("\n"); }

static inline void set_fpcr(u64 v) { __asm__ volatile("msr fpcr, %0" :: "r"(v)); }
static inline u64 get_fpsr(void) { u64 v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return v; }
static inline void clr_fpsr(void) { __asm__ volatile("msr fpsr, xzr"); }
static inline u64 get_nzcv(void) { u64 v; __asm__ volatile("mrs %0, nzcv" : "=r"(v)); return v; }

static const u64 D[] = {
    0x0000000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull,
    0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000001ull, 0xfff4000000000abcull,
    0x0000000000000001ull, 0x800fffffffffffffull, 0x0010000000000000ull, 0x7fefffffffffffffull,
    0x3ff8000000000000ull, 0x4330000000000001ull, 0x43e0000000000000ull, 0xc3e0000000000001ull,
    0x41dfffffffc00000ull, 0x3fe0000000000000ull, 0x400c000000000000ull, 0x3cb0000000000000ull,
};
static const u32 S[] = {
    0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x7f800000u, 0xff800000u, 0x7fc00123u,
    0xffa00001u, 0x00000001u, 0x807fffffu, 0x00800000u, 0x7f7fffffu, 0x3fc00000u, 0x4b000001u,
    0x4f000000u, 0xcf000001u, 0x3f000000u, 0x40600000u, 0x33800000u, 0x34000000u,
};
static const unsigned short H[] = {
    0x0000, 0x8000, 0x3c00, 0xbc00, 0x7c00, 0xfc00, 0x7e01, 0xfd01, 0x0001, 0x83ff, 0x0400, 0x7bff,
    0x3e00, 0x6401, 0x7800, 0xf801, 0x3800, 0x4300, 0x1400, 0x1800,
};
#define NV 20

static const u64 FPCRS[] = {
    0x0000000, 0x0400000, 0x0800000, 0x0c00000,         /* RN RP RM RZ */
    0x1000000, 0x2000000, 0x0080000, 0x4000000,         /* FZ DN FZ16 AHP */
    0x3c80000,                                          /* tudo junto, RZ */
};
#define NF 9

static void line(const char *op, u64 r, u64 fpsr) { put(op); put(" "); hex(r); put(" "); hex(fpsr); nl(); }

#define BIN_D(name)                                                              \
    static void t_##name##_d(void) {                                             \
        for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i++) for (int j = 0; j < NV; j += 3) { \
            u64 r; set_fpcr(FPCRS[f]); clr_fpsr();                               \
            __asm__ volatile("fmov d1, %1\n fmov d2, %2\n " #name " d0, d1, d2\n fmov %0, d0" \
                             : "=r"(r) : "r"(D[i]), "r"(D[j]) : "v0", "v1", "v2"); \
            line(#name ".d", r, get_fpsr());                                     \
        }                                                                        \
    }
#define BIN_S(name)                                                              \
    static void t_##name##_s(void) {                                             \
        for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i++) for (int j = 1; j < NV; j += 3) { \
            u32 r; set_fpcr(FPCRS[f]); clr_fpsr();                               \
            __asm__ volatile("fmov s1, %w1\n fmov s2, %w2\n " #name " s0, s1, s2\n fmov %w0, s0" \
                             : "=r"(r) : "r"(S[i]), "r"(S[j]) : "v0", "v1", "v2"); \
            line(#name ".s", r, get_fpsr());                                     \
        }                                                                        \
    }
#define BIN_H(name)                                                              \
    static void t_##name##_h(void) {                                             \
        for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i++) for (int j = 2; j < NV; j += 3) { \
            u32 r; set_fpcr(FPCRS[f]); clr_fpsr();                               \
            __asm__ volatile("fmov h1, %w1\n fmov h2, %w2\n " #name " h0, h1, h2\n fmov %w0, h0" \
                             : "=r"(r) : "r"((u32)H[i]), "r"((u32)H[j]) : "v0", "v1", "v2"); \
            line(#name ".h", r, get_fpsr());                                     \
        }                                                                        \
    }
#define BIN(name) BIN_D(name) BIN_S(name) BIN_H(name)
BIN(fadd) BIN(fsub) BIN(fmul) BIN(fdiv) BIN(fmax) BIN(fmin) BIN(fmaxnm) BIN(fminnm) BIN(fnmul)

#define UN_D(name)                                                               \
    static void u_##name##_d(void) {                                             \
        for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i++) {              \
            u64 r; set_fpcr(FPCRS[f]); clr_fpsr();                               \
            __asm__ volatile("fmov d1, %1\n " #name " d0, d1\n fmov %0, d0"      \
                             : "=r"(r) : "r"(D[i]) : "v0", "v1");                \
            line(#name ".d", r, get_fpsr());                                     \
        }                                                                        \
    }
#define UN_S(name)                                                               \
    static void u_##name##_s(void) {                                             \
        for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i++) {              \
            u32 r; set_fpcr(FPCRS[f]); clr_fpsr();                               \
            __asm__ volatile("fmov s1, %w1\n " #name " s0, s1\n fmov %w0, s0"    \
                             : "=r"(r) : "r"(S[i]) : "v0", "v1");                \
            line(#name ".s", r, get_fpsr());                                     \
        }                                                                        \
    }
#define UN(name) UN_D(name) UN_S(name)
UN(fsqrt) UN(frintn) UN(frintp) UN(frintm) UN(frintz) UN(frinta) UN(frintx) UN(frinti)
UN(frint32z) UN(frint32x) UN(frint64z) UN(frint64x) UN(fabs) UN(fneg)

static void conversions(void) {
    for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i++) {
        u64 r, x;
        u32 w;
        set_fpcr(FPCRS[f]);
        clr_fpsr(); __asm__ volatile("fmov d1, %1\n fcvt s0, d1\n fmov %w0, s0" : "=r"(w) : "r"(D[i]) : "v0", "v1"); line("fcvt.s.d", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov d1, %1\n fcvt h0, d1\n fmov %w0, h0" : "=r"(w) : "r"(D[i]) : "v0", "v1"); line("fcvt.h.d", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov s1, %w1\n fcvt d0, s1\n fmov %0, d0" : "=r"(r) : "r"(S[i]) : "v0", "v1"); line("fcvt.d.s", r, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov s1, %w1\n fcvt h0, s1\n fmov %w0, h0" : "=r"(w) : "r"(S[i]) : "v0", "v1"); line("fcvt.h.s", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov h1, %w1\n fcvt s0, h1\n fmov %w0, s0" : "=r"(w) : "r"((u32)H[i]) : "v0", "v1"); line("fcvt.s.h", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov h1, %w1\n fcvt d0, h1\n fmov %0, d0" : "=r"(r) : "r"((u32)H[i]) : "v0", "v1"); line("fcvt.d.h", r, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov s1, %w1\n bfcvt h0, s1\n fmov %w0, h0" : "=r"(w) : "r"(S[i]) : "v0", "v1"); line("bfcvt", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov d1, %1\n fcvtzs %0, d1" : "=r"(x) : "r"(D[i]) : "v1"); line("fcvtzs.x.d", x, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov d1, %1\n fcvtau %w0, d1" : "=r"(w) : "r"(D[i]) : "v1"); line("fcvtau.w.d", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov d1, %1\n fcvtns %w0, d1" : "=r"(w) : "r"(D[i]) : "v1"); line("fcvtns.w.d", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov s1, %w1\n fcvtmu %0, s1" : "=r"(x) : "r"(S[i]) : "v1"); line("fcvtmu.x.s", x, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov s1, %w1\n fcvtps %w0, s1" : "=r"(w) : "r"(S[i]) : "v1"); line("fcvtps.w.s", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov h1, %w1\n fcvtzu %w0, h1" : "=r"(w) : "r"((u32)H[i]) : "v1"); line("fcvtzu.w.h", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("fmov d1, %1\n fcvtzs %w0, d1, #7" : "=r"(w) : "r"(D[i]) : "v1"); line("fcvtzs.fix", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("scvtf d0, %1\n fmov %0, d0" : "=r"(r) : "r"(D[i]) : "v0"); line("scvtf.d.x", r, get_fpsr());
        clr_fpsr(); __asm__ volatile("ucvtf s0, %1\n fmov %w0, s0" : "=r"(w) : "r"(D[i]) : "v0"); line("ucvtf.s.x", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("scvtf h0, %w1\n fmov %w0, h0" : "=r"(w) : "r"((u32)D[i]) : "v0"); line("scvtf.h.w", w, get_fpsr());
        clr_fpsr(); __asm__ volatile("ucvtf d0, %1, #40\n fmov %0, d0" : "=r"(r) : "r"(D[i]) : "v0"); line("ucvtf.fix", r, get_fpsr());
        clr_fpsr();
        u64 z;
        __asm__ volatile("fmov d1, %2\n fjcvtzs %w0, d1\n mrs %1, nzcv" : "=r"(w), "=r"(z) : "r"(D[i]) : "v1", "cc");
        line("fjcvtzs", w | (z << 32), get_fpsr());
    }
}

static void compares(void) {
    for (int f = 0; f < NF; f += 4) for (int i = 0; i < NV; i++) for (int j = 0; j < NV; j += 2) {
        u64 n1, n2, n3, n4;
        set_fpcr(FPCRS[f]); clr_fpsr();
        __asm__ volatile("fmov d1, %4\n fmov d2, %5\n fcmp d1, d2\n mrs %0, nzcv\n fcmpe d1, #0.0\n mrs %1, nzcv\n"
                         "msr nzcv, %6\n fccmp d1, d2, #5, ne\n mrs %2, nzcv\n fccmpe d2, d1, #10, ge\n mrs %3, nzcv"
                         : "=&r"(n1), "=&r"(n2), "=&r"(n3), "=&r"(n4) : "r"(D[i]), "r"(D[j]), "r"((u64)(i & 15) << 28) : "v1", "v2", "cc");
        line("fcmp.d", (n1 >> 28) | (n2 >> 24) | (n3 >> 20) | (n4 >> 16), get_fpsr());
        clr_fpsr();
        __asm__ volatile("fmov s1, %w2\n fmov s2, %w3\n fcmpe s1, s2\n mrs %0, nzcv\n fmov h1, %w4\n fmov h2, %w5\n fcmp h1, h2\n mrs %1, nzcv"
                         : "=&r"(n1), "=&r"(n2) : "r"(S[i]), "r"(S[j]), "r"((u32)H[i]), "r"((u32)H[j]) : "v1", "v2", "cc");
        line("fcmp.sh", (n1 >> 28) | (n2 >> 24), get_fpsr());
        u64 r;
        __asm__ volatile("fmov d1, %1\n fmov d2, %2\n msr nzcv, %3\n fcsel d0, d1, d2, gt\n fmov %0, d0"
                         : "=r"(r) : "r"(D[i]), "r"(D[j]), "r"((u64)(j & 15) << 28) : "v0", "v1", "v2", "cc");
        line("fcsel.d", r, 0);
    }
}

static void fused(void) {
    for (int f = 0; f < NF; f++) for (int i = 0; i < NV; i += 2) for (int j = 0; j < NV; j += 3) for (int k = 0; k < NV; k += 4) {
        /* FPSR zerado antes de CADA instrução fundida: com IXC já acumulado o QEMU usa a FPU do
         * host ("hardfloat") e no build Windows a fma() da libm não é fundida (resultado errado do
         * oráculo, ex. FNMADD 1.5, 1.5, -0x1.0000000000001p63 → ...002 em vez de ...001). */
        u64 r;
        set_fpcr(FPCRS[f]);
#define FMA_D(op)                                                                                 \
        clr_fpsr();                                                                               \
        __asm__ volatile("fmov d1, %1\n fmov d2, %2\n fmov d3, %3\n " op " d0, d1, d2, d3\n fmov %0, d0" \
                         : "=r"(r) : "r"(D[i]), "r"(D[j]), "r"(D[k]) : "v0", "v1", "v2", "v3");  \
        line(op ".d", r, get_fpsr());
        FMA_D("fmadd") FMA_D("fmsub") FMA_D("fnmadd") FMA_D("fnmsub")
        clr_fpsr();
        u32 w;
        __asm__ volatile("fmov s1, %w1\n fmov s2, %w2\n fmov s3, %w3\n fnmadd s0, s1, s2, s3\n fmov %w0, s0"
                         : "=r"(w) : "r"(S[i]), "r"(S[j]), "r"(S[k]) : "v0", "v1", "v2", "v3");
        line("fnmadd.s", w, get_fpsr());
        clr_fpsr();
        __asm__ volatile("fmov h1, %w1\n fmov h2, %w2\n fmov h3, %w3\n fmsub h0, h1, h2, h3\n fmov %w0, h0"
                         : "=r"(w) : "r"((u32)H[i]), "r"((u32)H[j]), "r"((u32)H[k]) : "v0", "v1", "v2", "v3");
        line("fmsub.h", w, get_fpsr());
    }
}

static void moves(void) {
    u64 a, b, c2;
    __asm__ volatile("fmov d0, #-1.25\n fmov %0, d0\n fmov s0, #31.0\n fmov %w1, s0\n fmov h0, #0.125\n fmov %w2, h0"
                     : "=r"(a), "=r"(b), "=r"(c2) :: "v0");
    line("fmov.imm", a ^ (b << 1) ^ (c2 << 7), 0);
    u64 lo, hi;
    __asm__ volatile("movi v0.2d, #-1\n fmov v0.d[1], %2\n fmov %0, d0\n fmov %1, v0.d[1]"
                     : "=r"(lo), "=r"(hi) : "r"(0x0123456789abcdefull) : "v0");
    line("fmov.d1", lo, hi);
    __asm__ volatile("movi v0.2d, #-1\n fmov s0, %w2\n fmov %0, v0.d[1]\n fmov %1, d0"
                     : "=r"(hi), "=r"(lo) : "r"(0x40490fdbu) : "v0");
    line("fmov.zera", lo, hi);
}

int main(void) {
    t_fadd_d(); t_fadd_s(); t_fadd_h(); t_fsub_d(); t_fsub_s(); t_fsub_h();
    t_fmul_d(); t_fmul_s(); t_fmul_h(); t_fdiv_d(); t_fdiv_s(); t_fdiv_h();
    t_fmax_d(); t_fmax_s(); t_fmax_h(); t_fmin_d(); t_fmin_s(); t_fmin_h();
    t_fmaxnm_d(); t_fmaxnm_s(); t_fmaxnm_h(); t_fminnm_d(); t_fminnm_s(); t_fminnm_h();
    t_fnmul_d(); t_fnmul_s(); t_fnmul_h();
    u_fsqrt_d(); u_fsqrt_s(); u_frintn_d(); u_frintn_s(); u_frintp_d(); u_frintp_s();
    u_frintm_d(); u_frintm_s(); u_frintz_d(); u_frintz_s(); u_frinta_d(); u_frinta_s();
    u_frintx_d(); u_frintx_s(); u_frinti_d(); u_frinti_s(); u_frint32z_d(); u_frint32z_s();
    u_frint32x_d(); u_frint32x_s(); u_frint64z_d(); u_frint64z_s(); u_frint64x_d(); u_frint64x_s();
    u_fabs_d(); u_fabs_s(); u_fneg_d(); u_fneg_s();
    conversions();
    compares();
    fused();
    moves();
    set_fpcr(0);
    put("fim\n");
    return 0;
}

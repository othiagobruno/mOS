/* frente SIMD: AdvSIMD inteiro contra referência escalar — aritmética, saturação/QC, shifts, TBL,
 * permutações, across lanes, dot product/I8MM, LD1..LD4/ST1..ST4 e LDnR */
#include <arm_neon.h>
#include "a64t.h"

static int g_bad;
static void chk(const char *nome, unsigned long long a, unsigned long long b) {
    if (a != b) { a64t_puts("FALHA "); a64t_puts(nome); a64t_puts(" "); a64t_hex(a); a64t_puts(" != "); a64t_hex(b); a64t_puts("\n"); g_bad++; }
}
static unsigned long long g_s = 0x123456789abcdefull;
static unsigned long long rnd(void) { g_s ^= g_s << 13; g_s ^= g_s >> 7; g_s ^= g_s << 17; return g_s; }

static unsigned fpsr(void) { unsigned long long v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return (unsigned)v; }
static void set_fpsr(unsigned v) { __asm__ volatile("msr fpsr, %0" :: "r"((unsigned long long)v)); }

static volatile signed char A[16], B[16];

static void arith(void) {
    for (int t = 0; t < 50; t++) {
        for (int i = 0; i < 16; i++) { A[i] = (signed char)rnd(); B[i] = (signed char)rnd(); }
        int8x16_t a = vld1q_s8((const int8_t *)A), b = vld1q_s8((const int8_t *)B);
        int8_t r[16]; int16_t w[8];
        vst1q_s8(r, vqaddq_s8(a, b));
        for (int i = 0; i < 16; i++) { int s = A[i] + B[i]; s = s > 127 ? 127 : s < -128 ? -128 : s; chk("sqadd", (unsigned char)r[i], (unsigned char)s); }
        vst1q_s8(r, vabdq_s8(a, b));
        for (int i = 0; i < 16; i++) { int s = A[i] - B[i]; chk("sabd", (unsigned char)r[i], (unsigned char)(s < 0 ? -s : s)); }
        vst1q_s8(r, vrhaddq_s8(a, b));
        for (int i = 0; i < 16; i++) chk("srhadd", (unsigned char)r[i], (unsigned char)((A[i] + B[i] + 1) >> 1));
        vst1q_s16(w, vmull_high_s8(a, b));
        for (int i = 0; i < 8; i++) chk("smull2", (unsigned short)w[i], (unsigned short)(A[8 + i] * B[8 + i]));
        vst1q_s8(r, vrshlq_s8(a, vshrq_n_s8(b, 5)));
        for (int i = 0; i < 16; i++) {
            int sh = B[i] >> 5, v = A[i];
            int e = sh >= 0 ? (signed char)(v << sh) : (v + (1 << (-sh - 1))) >> -sh;
            chk("srshl", (unsigned char)r[i], (unsigned char)e);
        }
        uint8x16_t ua = vreinterpretq_u8_s8(a);
        chk("addv", (unsigned char)vaddvq_s8(a), (unsigned char)({ int s = 0; for (int i = 0; i < 16; i++) s += A[i]; s; }));
        chk("umaxv", vmaxvq_u8(ua), ({ unsigned m = 0; for (int i = 0; i < 16; i++) if ((unsigned char)A[i] > m) m = (unsigned char)A[i]; m; }));
        chk("cnt", vaddvq_u8(vcntq_u8(ua)), ({ unsigned s = 0; for (int i = 0; i < 16; i++) s += __builtin_popcount((unsigned char)A[i]); s; }));
    }
    /* saturação acende QC */
    set_fpsr(0);
    int16x4_t x = vdup_n_s16(-32768);
    int16x4_t y = vqdmulh_s16(x, x);
    chk("sqdmulh sat", (unsigned short)vget_lane_s16(y, 0), 0x7fff);
    chk("QC", (fpsr() >> 27) & 1, 1);
    set_fpsr(0);
    chk("sqrdmlah", (unsigned)vgetq_lane_s32(vqrdmlahq_s32(vdupq_n_s32(1000), vdupq_n_s32(0x40000000), vdupq_n_s32(0x40000000)), 2),
        1000 + 0x20000000);
    chk("QC limpo", (fpsr() >> 27) & 1, 0);
    chk("sqxtn", (unsigned char)vget_lane_s8(vqmovn_s16(vdupq_n_s16(300)), 3), 127);
    chk("QC xtn", (fpsr() >> 27) & 1, 1);
    chk("uqrshrn", vget_lane_u8(vqrshrn_n_u16(vdupq_n_u16(0x1ff), 1), 0), 0xff);
    chk("sri", vget_lane_u8(vsri_n_u8(vdup_n_u8(0xF0), vdup_n_u8(0xAB), 4), 0), 0xFA);
    chk("sli", vget_lane_u8(vsli_n_u8(vdup_n_u8(0x0F), vdup_n_u8(0xAB), 4), 0), 0xBF);
}

static void perm(void) {
    unsigned char a[16], b[16], o[16], idx[16];
    for (int i = 0; i < 16; i++) { a[i] = (unsigned char)i; b[i] = (unsigned char)(0x10 + i); idx[i] = (unsigned char)(rnd() % 40); }
    uint8x16_t va = vld1q_u8(a), vb = vld1q_u8(b);
    vst1q_u8(o, vzip1q_u8(va, vb));
    for (int i = 0; i < 16; i++) chk("zip1", o[i], (i & 1) ? b[i / 2] : a[i / 2]);
    vst1q_u8(o, vuzp2q_u8(va, vb));
    for (int i = 0; i < 16; i++) chk("uzp2", o[i], i < 8 ? a[2 * i + 1] : b[2 * (i - 8) + 1]);
    vst1q_u8(o, vtrn1q_u8(va, vb));
    for (int i = 0; i < 16; i++) chk("trn1", o[i], (i & 1) ? b[i - 1] : a[i]);
    vst1q_u8(o, vextq_u8(va, vb, 5));
    for (int i = 0; i < 16; i++) chk("ext", o[i], i + 5 < 16 ? a[i + 5] : b[i - 11]);
    uint8x16x2_t tab = { { va, vb } };
    vst1q_u8(o, vqtbx2q_u8(vdupq_n_u8(0xEE), tab, vld1q_u8(idx)));
    for (int i = 0; i < 16; i++) chk("tbx2", o[i], idx[i] < 32 ? (idx[i] < 16 ? a[idx[i]] : b[idx[i] - 16]) : 0xEE);
    vst1q_u8(o, vqtbl1q_u8(va, vld1q_u8(idx)));
    for (int i = 0; i < 16; i++) chk("tbl1", o[i], idx[i] < 16 ? a[idx[i]] : 0);
}

static void dots(void) {
    signed char a[16]; unsigned char b[16];
    for (int i = 0; i < 16; i++) { a[i] = (signed char)rnd(); b[i] = (unsigned char)rnd(); }
    int32x4_t acc = vdupq_n_s32(7);
    int r[4];
    vst1q_s32(r, vusdotq_s32(acc, vld1q_u8(b), vld1q_s8(a)));
    for (int e = 0; e < 4; e++) { int s = 7; for (int k = 0; k < 4; k++) s += b[4 * e + k] * a[4 * e + k]; chk("usdot", (unsigned)r[e], (unsigned)s); }
    vst1q_s32(r, vdotq_laneq_s32(acc, vld1q_s8(a), vreinterpretq_s8_u8(vld1q_u8(b)), 3));
    for (int e = 0; e < 4; e++) { int s = 7; for (int k = 0; k < 4; k++) s += a[4 * e + k] * (signed char)b[12 + k]; chk("sdot lane", (unsigned)r[e], (unsigned)s); }
    vst1q_s32(r, vmmlaq_s32(acc, vld1q_s8(a), vreinterpretq_s8_u8(vld1q_u8(b))));
    for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++) { int s = 7; for (int k = 0; k < 8; k++) s += a[8 * x + k] * (signed char)b[8 * y + k]; chk("smmla", (unsigned)r[2 * x + y], (unsigned)s); }
}

static void ldst(void) {
    static unsigned short m[64], o[64];
    for (int i = 0; i < 64; i++) m[i] = (unsigned short)(i * 3 + 1);
    uint16x8x4_t v4 = vld4q_u16(m);
    for (int s = 0; s < 4; s++) for (int e = 0; e < 8; e++) {
        unsigned short x[8]; vst1q_u16(x, v4.val[s]); chk("ld4", x[e], m[4 * e + s]);
    }
    vst4q_u16(o, v4);
    for (int i = 0; i < 32; i++) chk("st4", o[i], m[i]);
    uint16x4x3_t v3 = vld3_u16(m + 1);
    unsigned short y[4]; vst1_u16(y, v3.val[2]);
    for (int e = 0; e < 4; e++) chk("ld3", y[e], m[1 + 3 * e + 2]);
    uint16x8x2_t r2 = vld2q_dup_u16(m + 5);
    unsigned short z[8]; vst1q_u16(z, r2.val[1]);
    for (int e = 0; e < 8; e++) chk("ld2r", z[e], m[6]);
    uint16x8_t l = vld1q_lane_u16(m + 9, vdupq_n_u16(0x5555), 6);
    vst1q_u16(z, l);
    for (int e = 0; e < 8; e++) chk("ld1 lane", z[e], e == 6 ? m[9] : 0x5555);
    uint16x8x3_t q3 = vld1q_u16_x3(m + 2);
    vst1q_u16_x3(o + 1, q3);
    for (int i = 0; i < 24; i++) chk("ld1x3/st1x3", o[1 + i], m[2 + i]);
}

int main(void) {
    arith();
    perm();
    dots();
    ldst();
    a64t_puts(g_bad ? "simd_int: FALHOU\n" : "simd_int: ok\n");
    return g_bad != 0;
}

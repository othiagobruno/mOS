/* frente SIMD: lanes FP — FADD/FMLA/FMAXNMV/conversões contra a FPU escalar, FCADD/FCMLA, FMLAL, BF16 */
#include <arm_neon.h>
#include "a64t.h"

static int g_bad;
static void chk(const char *nome, unsigned long long a, unsigned long long b) {
    if (a != b) { a64t_puts("FALHA "); a64t_puts(nome); a64t_puts(" "); a64t_hex(a); a64t_puts(" != "); a64t_hex(b); a64t_puts("\n"); g_bad++; }
}
static unsigned u32(float f) { union { float f; unsigned u; } x = { f }; return x.u; }
static float f32(unsigned u) { union { unsigned u; float f; } x = { u }; return x.f; }

static float fmadd(float a, float b, float c) { float r; __asm__("fmadd %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c)); return r; }
static volatile float A[4] = { 1.5f, -2.25f, 3.0e30f, 1e-40f }, B[4] = { 0.75f, 4.0f, -1.0e9f, 2.0f };

int main(void) {
    float32x4_t a = vld1q_f32((const float *)A), b = vld1q_f32((const float *)B);
    float r[4];
    vst1q_f32(r, vaddq_f32(a, b));
    for (int i = 0; i < 4; i++) chk("fadd", u32(r[i]), u32(A[i] + B[i]));
    vst1q_f32(r, vfmaq_f32(b, a, a));
    for (int i = 0; i < 4; i++) chk("fmla", u32(r[i]), u32(fmadd(A[i], A[i], B[i])));
    chk("fmaxnmv", u32(vmaxnmvq_f32(a)), u32(3.0e30f));
    chk("faddp", u32(vpadds_f32(vget_low_f32(a))), u32(A[0] + A[1]));
    int32x4_t c = vcvtq_s32_f32(a);
    chk("fcvtzs sat", (unsigned)vgetq_lane_s32(c, 2), 0x7fffffff);
    chk("scvtf fix", u32(vgetq_lane_f32(vcvtq_n_f32_s32(vdupq_n_s32(6), 2), 1)), u32(1.5f));
    /* complexos: (1+2i), (3+4i) */
    float32x4_t z1 = { 1, 2, 3, 4 }, z2 = { 5, 6, 7, 8 };
    vst1q_f32(r, vcaddq_rot90_f32(z1, z2));               /* z1 + i*z2 */
    chk("fcadd90 re", u32(r[0]), u32(1 - 6.0f)); chk("fcadd90 im", u32(r[1]), u32(2 + 5.0f));
    float32x4_t p = vcmlaq_rot90_f32(vcmlaq_f32(vdupq_n_f32(0), z1, z2), z1, z2);   /* z1*z2 */
    vst1q_f32(r, p);
    chk("fcmla re", u32(r[0]), u32(1 * 5 - 2 * 6.0f)); chk("fcmla im", u32(r[1]), u32(1 * 6 + 2 * 5.0f));
    chk("fcmla re2", u32(r[2]), u32(3 * 7 - 4 * 8.0f)); chk("fcmla im2", u32(r[3]), u32(3 * 8 + 4 * 7.0f));
    /* FMLAL: half → single exato */
    float16x8_t h = { 1.5, -2, 0.5, 3, 7, 8, 9, 10 };
    vst1q_f32(r, vfmlalq_low_f16(vdupq_n_f32(1), h, h));
    chk("fmlal", u32(r[1]), u32(1 + 4.0f));
    vst1q_f32(r, vfmlslq_high_f16(vdupq_n_f32(1), h, h));
    chk("fmlsl2", u32(r[0]), u32(1 - 49.0f));
    /* BF16: 1.5*2 + 3*0.5 + 10 = 14.5 (exato); BFMLALB; BFCVTN */
    bfloat16x8_t x = vreinterpretq_bf16_u16((uint16x8_t){ 0x3fc0, 0x4040, 0, 0, 0, 0, 0, 0 });
    bfloat16x8_t y = vreinterpretq_bf16_u16((uint16x8_t){ 0x4000, 0x3f00, 0, 0, 0, 0, 0, 0 });
    chk("bfdot", u32(vgetq_lane_f32(vbfdotq_f32(vdupq_n_f32(10), x, y), 0)), u32(14.5f));
    chk("bfmlalb", u32(vgetq_lane_f32(vbfmlalbq_f32(vdupq_n_f32(1), x, y), 0)), u32(4.0f));
    chk("bfmlalt", u32(vgetq_lane_f32(vbfmlaltq_f32(vdupq_n_f32(1), x, y), 0)), u32(2.5f));
    chk("bfcvtn", vgetq_lane_u16(vreinterpretq_u16_bf16(vcvtq_low_bf16_f32(vdupq_n_f32(f32(0x3f808000)))), 0), 0x3f80);
    /* BFDOT: denormal vira zero e soma exata zero dá +0 */
    bfloat16x8_t dn = vreinterpretq_bf16_u16(vdupq_n_u16(0x0001));
    chk("bfdot ftz", u32(vgetq_lane_f32(vbfdotq_f32(vdupq_n_f32(-0.0f), dn, dn), 0)), 0);
    a64t_puts(g_bad ? "simd_fp: FALHOU\n" : "simd_fp: ok\n");
    return g_bad != 0;
}

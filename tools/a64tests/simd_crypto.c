/* frente SIMD: AES (FIPS-197 C.1/B), SHA-1 e SHA-256 de "abc", PMULL, SHA3 (EOR3/RAX1/XAR/BCAX) */
#include <arm_neon.h>
#include "a64t.h"

static int g_bad;
static void chk(const char *nome, unsigned long long a, unsigned long long b) {
    if (a != b) { a64t_puts("FALHA "); a64t_puts(nome); a64t_puts(" "); a64t_hex(a); a64t_puts(" != "); a64t_hex(b); a64t_puts("\n"); g_bad++; }
}

static unsigned subword(unsigned w) {
    uint8x16_t s = vaeseq_u8(vreinterpretq_u8_u32(vdupq_n_u32(w)), vdupq_n_u8(0));
    return vgetq_lane_u32(vreinterpretq_u32_u8(s), 0);
}

static void aes_test(void) {
    static const unsigned char key[16] = { 0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c };
    static const unsigned char pt[16] = { 0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34 };
    static const unsigned char ct[16] = { 0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32 };
    static const unsigned char rcon[10] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };
    unsigned w[44];
    for (int i = 0; i < 4; i++) w[i] = key[4*i] | key[4*i+1] << 8 | key[4*i+2] << 16 | (unsigned)key[4*i+3] << 24;
    for (int i = 4; i < 44; i++) {
        unsigned t = w[i - 1];
        if (i % 4 == 0) t = subword((t >> 8) | (t << 24)) ^ rcon[i / 4 - 1];
        w[i] = w[i - 4] ^ t;
    }
    uint8x16_t rk[11];
    for (int r = 0; r < 11; r++) rk[r] = vreinterpretq_u8_u32(vld1q_u32(&w[4 * r]));
    uint8x16_t s = vld1q_u8(pt);
    for (int r = 0; r < 9; r++) s = vaesmcq_u8(vaeseq_u8(s, rk[r]));
    s = veorq_u8(vaeseq_u8(s, rk[9]), rk[10]);
    uint8x16_t e = vld1q_u8(ct);
    chk("aes enc lo", vgetq_lane_u64(vreinterpretq_u64_u8(s), 0), vgetq_lane_u64(vreinterpretq_u64_u8(e), 0));
    chk("aes enc hi", vgetq_lane_u64(vreinterpretq_u64_u8(s), 1), vgetq_lane_u64(vreinterpretq_u64_u8(e), 1));
    s = vaesimcq_u8(vaesdq_u8(s, rk[10]));
    for (int r = 9; r > 1; r--) s = vaesimcq_u8(vaesdq_u8(s, vaesimcq_u8(rk[r])));
    s = veorq_u8(vaesdq_u8(s, vaesimcq_u8(rk[1])), rk[0]);
    uint8x16_t p = vld1q_u8(pt);
    chk("aes dec lo", vgetq_lane_u64(vreinterpretq_u64_u8(s), 0), vgetq_lane_u64(vreinterpretq_u64_u8(p), 0));
    chk("aes dec hi", vgetq_lane_u64(vreinterpretq_u64_u8(s), 1), vgetq_lane_u64(vreinterpretq_u64_u8(p), 1));
}

static void pad_abc(unsigned char *b) {   /* bloco único de "abc" */
    for (int i = 0; i < 64; i++) b[i] = 0;
    b[0] = 'a'; b[1] = 'b'; b[2] = 'c'; b[3] = 0x80; b[63] = 24;
}

static const unsigned K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

static void sha256_test(void) {
    static const unsigned H[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    static const unsigned D[8] = { 0xba7816bf,0x8f01cfea,0x414140de,0x5dae2223,0xb00361a3,0x96177a9c,0xb410ff61,0xf20015ad };
    unsigned char b[64];
    pad_abc(b);
    uint32x4_t s0 = vld1q_u32(H), s1 = vld1q_u32(H + 4), m[4];
    for (int i = 0; i < 4; i++) m[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(b + 16 * i)));
    for (int i = 0; i < 16; i++) {
        uint32x4_t t = vaddq_u32(m[i % 4], vld1q_u32(K256 + 4 * i)), t0 = s0;
        s0 = vsha256hq_u32(s0, s1, t);
        s1 = vsha256h2q_u32(s1, t0, t);
        if (i < 12) m[i % 4] = vsha256su1q_u32(vsha256su0q_u32(m[i % 4], m[(i + 1) % 4]), m[(i + 2) % 4], m[(i + 3) % 4]);
    }
    s0 = vaddq_u32(s0, vld1q_u32(H)); s1 = vaddq_u32(s1, vld1q_u32(H + 4));
    unsigned o[8];
    vst1q_u32(o, s0); vst1q_u32(o + 4, s1);
    for (int i = 0; i < 8; i++) chk("sha256", o[i], D[i]);
}

static void sha1_test(void) {
    static const unsigned H[5] = { 0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0 };
    static const unsigned D[5] = { 0xa9993e36,0x4706816a,0xba3e2571,0x7850c26c,0x9cd0d89d };
    static const unsigned K[4] = { 0x5a827999,0x6ed9eba1,0x8f1bbcdc,0xca62c1d6 };
    unsigned char b[64];
    pad_abc(b);
    uint32x4_t W[20], abcd = vld1q_u32(H);
    unsigned e = H[4];
    for (int i = 0; i < 4; i++) W[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(b + 16 * i)));
    for (int g = 4; g < 20; g++) W[g] = vsha1su1q_u32(vsha1su0q_u32(W[g - 4], W[g - 3], W[g - 2]), W[g - 1]);
    for (int g = 0; g < 20; g++) {
        uint32x4_t t = vaddq_u32(W[g], vdupq_n_u32(K[g / 5]));
        unsigned en = vsha1h_u32(vgetq_lane_u32(abcd, 0));
        if (g < 5) abcd = vsha1cq_u32(abcd, e, t);
        else if (g < 10 || g >= 15) abcd = vsha1pq_u32(abcd, e, t);
        else abcd = vsha1mq_u32(abcd, e, t);
        e = en;
    }
    unsigned o[4];
    vst1q_u32(o, vaddq_u32(abcd, vld1q_u32(H)));
    for (int i = 0; i < 4; i++) chk("sha1", o[i], D[i]);
    chk("sha1 e", e + H[4], D[4]);
}

static unsigned long long clmul_lo(unsigned long long a, unsigned long long b, unsigned long long *hi) {
    unsigned long long l = 0, h = 0;
    for (int i = 0; i < 64; i++) if ((b >> i) & 1) { l ^= a << i; if (i) h ^= a >> (64 - i); }
    *hi = h;
    return l;
}

static unsigned long long rol(unsigned long long x, int r) { return (x << r) | (x >> (64 - r)); }

static void misc_test(void) {
    volatile unsigned long long a = 0x9e3779b97f4a7c15ull, b = 0xc2b2ae3d27d4eb4full, c = 0x165667b19e3779f9ull;
    unsigned long long hi, lo = clmul_lo(a, b, &hi);
    poly128_t p = vmull_p64(a, b);
    chk("pmull lo", (unsigned long long)p, lo);
    chk("pmull hi", (unsigned long long)(p >> 64), hi);
    uint64x2_t va = vdupq_n_u64(a), vb = vdupq_n_u64(b), vc = vdupq_n_u64(c);
    chk("eor3", vgetq_lane_u64(veor3q_u64(va, vb, vc), 1), a ^ b ^ c);
    chk("bcax", vgetq_lane_u64(vbcaxq_u64(va, vb, vc), 0), a ^ (b & ~c));
    chk("rax1", vgetq_lane_u64(vrax1q_u64(va, vb), 1), a ^ rol(b, 1));
    chk("xar", vgetq_lane_u64(vxarq_u64(va, vb, 13), 0), rol(a ^ b, 64 - 13));
}

int main(void) {
    aes_test();
    sha256_test();
    sha1_test();
    misc_test();
    a64t_puts(g_bad ? "simd_crypto: FALHOU\n" : "simd_crypto: ok\n");
    return g_bad != 0;
}

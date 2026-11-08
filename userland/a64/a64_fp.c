/*
 * mOS — tradutor arm64: primitivas de ponto flutuante a64_fp_* (frente FP).
 *
 * Escrito do zero a partir do pseudocódigo do ARM ARM (DDI 0487, shared/functions/float:
 * FPUnpackBase, FPRoundBase, FPProcessNaN(s)(3), FPAdd/FPSub/FPMul/FPMulX/FPDiv/FPSqrt/FPMulAdd,
 * FPMax/FPMin/FPMaxNum/FPMinNum, FPCompare*, FPRecipEstimate/FPRSqrtEstimate/FPRecpX,
 * FPRecipStepFused/FPRSqrtStepFused, FPRoundInt/FPRoundIntN, FPConvert/FPConvertNaN/FPConvertBF,
 * FPToFixed/FPToFixedJS/FixedToFP, VFPExpandImm, UnsignedRecipEstimate/UnsignedRSqrtEstimate).
 * Nenhum código de terceiros; nenhuma instrução de FP do host (só inteiros), então o resultado e
 * as flags não dependem do MXCSR nem de diferenças x87/SSE.
 *
 * Representação interna: um valor finito não nulo é sig * 2^exp (sig inteiro). As operações
 * calculam o valor exato (ou exato até um bit "grudento" bem abaixo da posição de arredondamento)
 * e passam por round_base(), que é o FPRoundBase literal: FZ antes do arredondamento,
 * tininess ANTES do arredondamento (como o ARM: biased_exp calculado do valor exato), UFC só se
 * inexato, OFC+IXC no overflow, AHP com IOC, arredondamento para ímpar (FCVTXN).
 * Recursos AFP (FPCR.AH/FIZ/NEP) e RPRES não são anunciados: o caminho "altfp" não existe aqui.
 */
#include "a64.h"

typedef a64_u128 u128;
typedef a64_u64 u64;
typedef a64_u32 u32;

typedef struct fmt { int N, E, F, emin, bias, shift; } fmt;
static const fmt F16 = {16, 5, 10, -14, 15, 0};
static const fmt F32 = {32, 8, 23, -126, 127, 0};
static const fmt F64 = {64, 11, 52, -1022, 1023, 0};
static const fmt FBF = {32, 8, 7, -126, 127, 16};   /* BFloat16 nos 16 bits altos de um "single" */

static const fmt *fmt_of(int es) { return es == 2 ? &F16 : es == 4 ? &F32 : &F64; }

enum { T_ZERO, T_DENORM, T_NORMAL, T_INF, T_QNAN, T_SNAN };
typedef struct fpv { int type, sign, exp; u64 sig, bits; } fpv;   /* valor = (-1)^sign * sig * 2^exp */

#define IOC A64_FPSR_IOC
#define DZC A64_FPSR_DZC
#define OFC A64_FPSR_OFC
#define UFC A64_FPSR_UFC
#define IXC A64_FPSR_IXC
#define IDC A64_FPSR_IDC

static inline int is_nan(const fpv *v) { return v->type >= T_QNAN; }
static inline int rmode_of(u32 fpcr) { return (fpcr >> A64_FPCR_RMODE_SHIFT) & 3; }
static inline int resolve_rm(u32 fpcr, int rm) { return rm == A64_RM_FPCR ? rmode_of(fpcr) : rm; }
/* FZ que vale para operações aritméticas (FPUnpack/FPRound): FZ16 para half, FZ para o resto */
static inline int fz_of(const fmt *f, u32 fpcr) { return f->N == 16 ? !!(fpcr & A64_FPCR_FZ16) : !!(fpcr & A64_FPCR_FZ); }

static inline int msb64(u64 x) { return 63 - __builtin_clzll(x); }
static inline int msb128(u128 x) {
    u64 hi = (u64)(x >> 64);
    return hi ? 64 + msb64(hi) : msb64((u64)x);
}

/* ---------------------------------------------------------------- codificação */

static inline u64 signbit(const fmt *f) { return 1ull << (f->N - 1); }
static inline u64 encode(const fmt *f, int sign, u64 bexp, u64 mant) {
    return ((u64)sign << (f->N - 1)) | (bexp << (f->F + f->shift)) | ((mant & ((1ull << f->F) - 1)) << f->shift);
}
static inline u64 fzero(const fmt *f, int sign) { return (u64)sign << (f->N - 1); }
static inline u64 finf(const fmt *f, int sign) { return encode(f, sign, (1u << f->E) - 1, 0); }
static inline u64 fmaxnormal(const fmt *f, int sign) { return encode(f, sign, (1u << f->E) - 2, (1ull << f->F) - 1); }
static inline u64 fdnan(const fmt *f) { return encode(f, 0, (1u << f->E) - 1, 1ull << (f->F - 1)); }
static inline u64 fconst(const fmt *f, int sign, int unbiased, u64 fracbits_top4) {
    /* valor 2^unbiased * (1 + fracbits_top4/16) — para FPTwo/FPOnePointFive */
    return encode(f, sign, (u64)(unbiased + f->bias), fracbits_top4 << (f->F - 4));
}

/* FPUnpackBase. fz = flush de denormais na entrada; ahp = half alternativo (só FPUnpackCV).
 * IDC só para single/double (FZ16 não sinaliza Input Denormal). */
static void unpack(const fmt *f, u64 bits, int fz, int ahp, fpv *v, u32 *fl) {
    u64 fracmask = (1ull << f->F) - 1;
    u32 emax = (1u << f->E) - 1;
    v->bits = bits;
    v->sign = (int)((bits >> (f->N - 1)) & 1);
    u32 e = (u32)((bits >> f->F) & emax);
    u64 frac = bits & fracmask;
    v->sig = 0; v->exp = 0;
    if (e == 0) {
        if (frac == 0 || fz) {
            v->type = T_ZERO;
            if (frac && f->N != 16) *fl |= IDC;
        } else {
            v->type = T_DENORM; v->sig = frac; v->exp = f->emin - f->F;
        }
    } else if (e == emax && !ahp) {
        if (frac == 0) v->type = T_INF;
        else v->type = (frac >> (f->F - 1)) & 1 ? T_QNAN : T_SNAN;
    } else {
        v->type = T_NORMAL; v->sig = frac | (1ull << f->F); v->exp = (int)e - f->bias - f->F;
    }
}

/* FPRoundBase: valor = (-1)^sign * sig * 2^exp, sig != 0. rm = A64_RM_* já resolvido. */
static u64 round_base(const fmt *f, int sign, u128 sig, int exp, int rm, int fz, int ahp, u32 *fl) {
    int msb = msb128(sig);
    int e = exp + msb;                              /* valor em [2^e, 2^(e+1)) */
    if (fz && e < f->emin) { *fl |= UFC; return fzero(f, sign); }
    int biased = e - f->emin + 1;
    if (biased < 0) biased = 0;
    int lsbexp = biased > 0 ? e - f->F : f->emin - f->F;
    int sh = lsbexp - exp;
    u64 mant;
    int rbit, sticky;
    if (sh <= 0) {
        mant = (u64)(sig << -sh); rbit = sticky = 0;
    } else {
        mant = sh >= 128 ? 0 : (u64)(sig >> sh);
        rbit = sh - 1 >= 128 ? 0 : (int)((sig >> (sh - 1)) & 1);
        sticky = sh - 1 >= 128 ? sig != 0 : (sig & (((u128)1 << (sh - 1)) - 1)) != 0;
    }
    int inexact = rbit | sticky;
    if (biased == 0 && inexact) *fl |= UFC;          /* tininess antes do arredondamento */
    int up, ovf_inf;
    switch (rm) {
    case A64_RM_TIEEVEN: up = rbit && (sticky || (mant & 1)); ovf_inf = 1; break;
    case A64_RM_POSINF:  up = inexact && !sign; ovf_inf = !sign; break;
    case A64_RM_NEGINF:  up = inexact && sign; ovf_inf = sign; break;
    case A64_RM_TIEAWAY: up = rbit; ovf_inf = 1; break;
    default:             up = 0; ovf_inf = 0; break;   /* ZERO, ODD */
    }
    if (up) {
        mant++;
        if (mant == 1ull << f->F) biased = 1;
        if (mant == 1ull << (f->F + 1)) { biased++; mant >>= 1; }
    }
    if (inexact && rm == A64_RM_ODD) mant |= 1;
    u64 r;
    if (!ahp) {
        if (biased >= (1 << f->E) - 1) {
            r = ovf_inf ? finf(f, sign) : fmaxnormal(f, sign);
            *fl |= OFC; inexact = 1;
        } else r = encode(f, sign, (u64)biased, mant);
    } else {
        if (biased >= (1 << f->E)) {
            r = ((u64)sign << 15) | 0x7FFF;
            *fl |= IOC; inexact = 0;
        } else r = encode(f, sign, (u64)biased, mant);
    }
    if (inexact) *fl |= IXC;
    return r;
}

/* FPRound: usa FZ/FZ16 conforme o formato, AHP = 0 */
static inline u64 fpround(const fmt *f, int sign, u128 sig, int exp, u32 fpcr, int rm, u32 *fl) {
    return round_base(f, sign, sig, exp, rm, fz_of(f, fpcr), 0, fl);
}

/* ---------------------------------------------------------------- NaN */

static u64 process_nan(const fmt *f, const fpv *v, u32 fpcr, u32 *fl) {
    u64 r = v->bits;
    if (v->type == T_SNAN) { r |= 1ull << (f->F - 1); *fl |= IOC; }
    if (fpcr & A64_FPCR_DN) r = fdnan(f);
    return r;
}
static int process_nans(const fmt *f, const fpv *a, const fpv *b, u32 fpcr, u32 *fl, u64 *r) {
    if (a->type == T_SNAN) { *r = process_nan(f, a, fpcr, fl); return 1; }
    if (b->type == T_SNAN) { *r = process_nan(f, b, fpcr, fl); return 1; }
    if (a->type == T_QNAN) { *r = process_nan(f, a, fpcr, fl); return 1; }
    if (b->type == T_QNAN) { *r = process_nan(f, b, fpcr, fl); return 1; }
    return 0;
}
static int process_nans3(const fmt *f, const fpv *a, const fpv *b, const fpv *c, u32 fpcr, u32 *fl, u64 *r) {
    if (a->type == T_SNAN) { *r = process_nan(f, a, fpcr, fl); return 1; }
    if (b->type == T_SNAN) { *r = process_nan(f, b, fpcr, fl); return 1; }
    if (c->type == T_SNAN) { *r = process_nan(f, c, fpcr, fl); return 1; }
    if (a->type == T_QNAN) { *r = process_nan(f, a, fpcr, fl); return 1; }
    if (b->type == T_QNAN) { *r = process_nan(f, b, fpcr, fl); return 1; }
    if (c->type == T_QNAN) { *r = process_nan(f, c, fpcr, fl); return 1; }
    return 0;
}

/* ---------------------------------------------------------------- aritmética exata */

static inline u128 shr_jam(u128 m, int d) {
    if (d <= 0) return m;
    if (d >= 128) return m != 0;
    return (m >> d) | ((m & (((u128)1 << d) - 1)) != 0);
}

/* soma exata (até o bit grudento, sempre >= 70 bits abaixo do msb do resultado quando há perda)
 * de dois valores sig*2^exp com sinal. sig pode ter até 126 bits. Devolve sig 0 se a soma é 0. */
static void exact_add(int s1, u128 m1, int e1, int s2, u128 m2, int e2, int *rs, u128 *rm, int *re) {
    if (m1 == 0) { *rs = s2; *rm = m2; *re = e2; return; }
    if (m2 == 0) { *rs = s1; *rm = m1; *re = e1; return; }
    int l1 = 125 - msb128(m1), l2 = 125 - msb128(m2);
    m1 <<= l1; e1 -= l1;
    m2 <<= l2; e2 -= l2;
    if (e1 < e2 || (e1 == e2 && m1 < m2)) {
        int ts = s1; s1 = s2; s2 = ts;
        u128 tm = m1; m1 = m2; m2 = tm;
        int te = e1; e1 = e2; e2 = te;
    }
    m2 = shr_jam(m2, e1 - e2);
    *rs = s1; *re = e1;
    *rm = s1 == s2 ? m1 + m2 : m1 - m2;
}

/* quociente de (hi:lo)/d com hi < d */
static inline u64 udiv128(u64 hi, u64 lo, u64 d, u64 *rem) {
#if defined(__x86_64__)
    u64 q, r;
    __asm__("divq %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    *rem = r;
    return q;
#else
    u128 n = ((u128)hi << 64) | lo;
    *rem = (u64)(n % d);
    return (u64)(n / d);
#endif
}

/* raiz inteira (piso) de n < 2^127; *rem = n - r^2 */
static u64 isqrt128(u128 n, u128 *rem) {
    u128 res = 0, bit = (u128)1 << 126;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
        else res >>= 1;
        bit >>= 2;
    }
    *rem = n;
    return (u64)res;
}

/* -1, 0, 1 para valores não-NaN já desempacotados (denormais com FZ já são zero) */
static int cmp_val(const fmt *f, const fpv *a, const fpv *b) {
    u64 ma = a->type == T_ZERO ? 0 : a->bits & ~signbit(f) & ((signbit(f) << 1) - 1);
    u64 mb = b->type == T_ZERO ? 0 : b->bits & ~signbit(f) & ((signbit(f) << 1) - 1);
    if (ma == 0 && mb == 0) return 0;
    int sa = ma ? a->sign : 0, sb = mb ? b->sign : 0;
    if (sa != sb) return sa ? -1 : 1;
    int r = ma < mb ? -1 : ma > mb ? 1 : 0;
    return sa ? -r : r;
}

/* ---------------------------------------------------------------- operações */

static u64 fp_addsub(a64_cpu *c, u64 a, u64 b, int es, int neg) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr), rm = rmode_of(fpcr);
    fpv x, y;
    u64 r;
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (!process_nans(f, &x, &y, fpcr, &fl, &r)) {
        int s2 = y.sign ^ neg;
        int inf1 = x.type == T_INF, inf2 = y.type == T_INF;
        if (inf1 && inf2 && x.sign != s2) { r = fdnan(f); fl |= IOC; }
        else if ((inf1 && !x.sign) || (inf2 && !s2)) r = finf(f, 0);
        else if ((inf1 && x.sign) || (inf2 && s2)) r = finf(f, 1);
        else if (x.type == T_ZERO && y.type == T_ZERO && x.sign == s2) r = fzero(f, x.sign);
        else {
            int rs, re; u128 m;
            exact_add(x.sign, x.sig, x.exp, s2, y.sig, y.exp, &rs, &m, &re);
            r = m == 0 ? fzero(f, rm == A64_RM_NEGINF) : fpround(f, rs, m, re, fpcr, rm, &fl);
        }
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_add(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_addsub(c, a, b, es, 0); }
a64_u64 a64_fp_sub(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_addsub(c, a, b, es, 1); }

a64_u64 a64_fp_abd(a64_cpu *c, a64_u64 a, a64_u64 b, int es) {
    const fmt *f = fmt_of(es);
    return fp_addsub(c, a, b, es, 1) & ~signbit(f);
}

static u64 fp_mul(a64_cpu *c, u64 a, u64 b, int es, int mulx) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr);
    fpv x, y;
    u64 r;
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (!process_nans(f, &x, &y, fpcr, &fl, &r)) {
        int s = x.sign ^ y.sign;
        int inf1 = x.type == T_INF, inf2 = y.type == T_INF, z1 = x.type == T_ZERO, z2 = y.type == T_ZERO;
        if ((inf1 && z2) || (z1 && inf2)) {
            if (mulx) r = fconst(f, s, 1, 0);
            else { r = fdnan(f); fl |= IOC; }
        } else if (inf1 || inf2) r = finf(f, s);
        else if (z1 || z2) r = fzero(f, s);
        else r = fpround(f, s, (u128)x.sig * y.sig, x.exp + y.exp, fpcr, rmode_of(fpcr), &fl);
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_mul(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_mul(c, a, b, es, 0); }
a64_u64 a64_fp_mulx(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_mul(c, a, b, es, 1); }

a64_u64 a64_fp_div(a64_cpu *c, a64_u64 a, a64_u64 b, int es) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr);
    fpv x, y;
    u64 r;
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (!process_nans(f, &x, &y, fpcr, &fl, &r)) {
        int s = x.sign ^ y.sign;
        int inf1 = x.type == T_INF, inf2 = y.type == T_INF, z1 = x.type == T_ZERO, z2 = y.type == T_ZERO;
        if ((inf1 && inf2) || (z1 && z2)) { r = fdnan(f); fl |= IOC; }
        else if (inf1 || z2) { r = finf(f, s); if (!inf1) fl |= DZC; }
        else if (z1 || inf2) r = fzero(f, s);
        else {
            int l1 = 62 - msb64(x.sig), l2 = 63 - msb64(y.sig);
            u64 n = x.sig << l1, d = y.sig << l2, rem;
            u64 q = udiv128(n, 0, d, &rem);
            q |= rem != 0;
            r = fpround(f, s, q, (x.exp - l1) - 64 - (y.exp - l2), fpcr, rmode_of(fpcr), &fl);
        }
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_sqrt(a64_cpu *c, a64_u64 a, int es) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    u64 r;
    unpack(f, a, fz_of(f, fpcr), 0, &x, &fl);
    if (is_nan(&x)) r = process_nan(f, &x, fpcr, &fl);
    else if (x.type == T_ZERO) r = fzero(f, x.sign);
    else if (x.type == T_INF && !x.sign) r = finf(f, 0);
    else if (x.sign) { r = fdnan(f); fl |= IOC; }
    else {
        int sh = 124 - msb64(x.sig);
        if ((x.exp - sh) & 1) sh++;
        u128 rem;
        u64 root = isqrt128((u128)x.sig << sh, &rem);
        root |= rem != 0;
        r = fpround(f, 0, root, (x.exp - sh) / 2, fpcr, rmode_of(fpcr), &fl);
    }
    c->fpsr |= fl;
    return r;
}

/* núcleo de FPMulAdd/FPRecipStepFused/FPRSqrtStepFused depois do tratamento de casos especiais:
 * addend + p1*p2 (valores finitos já desempacotados), com expadj somado ao expoente final. */
static u64 fused_value(const fmt *f, int sa, u128 ma, int ea, const fpv *x, const fpv *y, int expadj, u32 fpcr, u32 *fl) {
    int rm = rmode_of(fpcr);
    int sp = x->sign ^ y->sign;
    u128 mp = (u128)x->sig * y->sig;
    int rs, re; u128 m;
    exact_add(sa, ma, ea, sp, mp, x->exp + y->exp, &rs, &m, &re);
    if (m == 0) return fzero(f, rm == A64_RM_NEGINF);
    return fpround(f, rs, m, re + expadj, fpcr, rm, fl);
}

a64_u64 a64_fp_muladd(a64_cpu *c, a64_u64 addend, a64_u64 m1, a64_u64 m2, int es) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr);
    fpv A, x, y;
    u64 r;
    unpack(f, addend, fz, 0, &A, &fl);
    unpack(f, m1, fz, 0, &x, &fl);
    unpack(f, m2, fz, 0, &y, &fl);
    int inf1 = x.type == T_INF, inf2 = y.type == T_INF, z1 = x.type == T_ZERO, z2 = y.type == T_ZERO;
    int done = process_nans3(f, &A, &x, &y, fpcr, &fl, &r);
    if (A.type == T_QNAN && ((inf1 && z2) || (z1 && inf2))) { r = fdnan(f); fl |= IOC; }
    if (!done) {
        int infA = A.type == T_INF, zA = A.type == T_ZERO;
        int sP = x.sign ^ y.sign, infP = inf1 || inf2, zP = z1 || z2;
        if ((inf1 && z2) || (z1 && inf2) || (infA && infP && A.sign != sP)) { r = fdnan(f); fl |= IOC; }
        else if ((infA && !A.sign) || (infP && !sP)) r = finf(f, 0);
        else if ((infA && A.sign) || (infP && sP)) r = finf(f, 1);
        else if (zA && zP && A.sign == sP) r = fzero(f, A.sign);
        else r = fused_value(f, A.sign, A.sig, A.exp, &x, &y, 0, fpcr, &fl);
    }
    c->fpsr |= fl;
    return r;
}

/* FPRecipStepFused (rsqrt=0): 2 - a*b ; FPRSqrtStepFused (rsqrt=1): (3 - a*b)/2 */
static u64 fp_step(a64_cpu *c, u64 a, u64 b, int es, int rsqrt) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr);
    fpv x, y;
    u64 r;
    a ^= signbit(f);                                  /* FPNeg(op1) antes de tudo */
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (!process_nans(f, &x, &y, fpcr, &fl, &r)) {
        int inf1 = x.type == T_INF, inf2 = y.type == T_INF, z1 = x.type == T_ZERO, z2 = y.type == T_ZERO;
        if ((inf1 && z2) || (z1 && inf2)) r = rsqrt ? fconst(f, 0, 0, 8) : fconst(f, 0, 1, 0);
        else if (inf1 || inf2) r = finf(f, x.sign ^ y.sign);
        else if (rsqrt) r = fused_value(f, 0, 3, 0, &x, &y, -1, fpcr, &fl);
        else r = fused_value(f, 0, 1, 1, &x, &y, 0, fpcr, &fl);
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_recps(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_step(c, a, b, es, 0); }
a64_u64 a64_fp_rsqrts(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_step(c, a, b, es, 1); }

/* FPMax/FPMin (ismax), com FPMaxNum/FPMinNum (num) na frente */
static u64 fp_maxmin(a64_cpu *c, u64 a, u64 b, int es, int ismax, int num) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr);
    fpv x, y;
    u64 r;
    if (num) {
        unpack(f, a, fz, 0, &x, &fl);
        unpack(f, b, fz, 0, &y, &fl);
        if (x.type == T_QNAN && y.type != T_QNAN) a = finf(f, ismax);
        else if (x.type != T_QNAN && y.type == T_QNAN) b = finf(f, ismax);
    }
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (!process_nans(f, &x, &y, fpcr, &fl, &r)) {
        int k = cmp_val(f, &x, &y);
        const fpv *p = (ismax ? k > 0 : k < 0) ? &x : &y;
        if (p->type == T_INF) r = finf(f, p->sign);
        else if (p->type == T_ZERO) r = fzero(f, ismax ? (x.sign & y.sign) : (x.sign | y.sign));
        else r = p->bits;                              /* FPRound de valor representável: exato */
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_max(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_maxmin(c, a, b, es, 1, 0); }
a64_u64 a64_fp_min(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_maxmin(c, a, b, es, 0, 0); }
a64_u64 a64_fp_maxnum(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_maxmin(c, a, b, es, 1, 1); }
a64_u64 a64_fp_minnum(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_maxmin(c, a, b, es, 0, 1); }

a64_u32 a64_fp_compare(a64_cpu *c, a64_u64 a, a64_u64 b, int es, int signal) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0, r;
    int fz = fz_of(f, fpcr);
    fpv x, y;
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (is_nan(&x) || is_nan(&y)) {
        r = A64_C | A64_V;
        if (x.type == T_SNAN || y.type == T_SNAN || signal) fl |= IOC;
    } else {
        int k = cmp_val(f, &x, &y);
        r = k == 0 ? (A64_Z | A64_C) : k < 0 ? A64_N : A64_C;
    }
    c->fpsr |= fl;
    return r;
}

/* 0 = EQ, 1 = GE, 2 = GT */
static int fp_cmpmask(a64_cpu *c, u64 a, u64 b, int es, int kind) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr), r;
    fpv x, y;
    unpack(f, a, fz, 0, &x, &fl);
    unpack(f, b, fz, 0, &y, &fl);
    if (is_nan(&x) || is_nan(&y)) {
        r = 0;
        if (kind != 0 || x.type == T_SNAN || y.type == T_SNAN) fl |= IOC;
    } else {
        int k = cmp_val(f, &x, &y);
        r = kind == 0 ? k == 0 : kind == 1 ? k >= 0 : k > 0;
    }
    c->fpsr |= fl;
    return r;
}

int a64_fp_cmp_eq(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_cmpmask(c, a, b, es, 0); }
int a64_fp_cmp_ge(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_cmpmask(c, a, b, es, 1); }
int a64_fp_cmp_gt(a64_cpu *c, a64_u64 a, a64_u64 b, int es) { return fp_cmpmask(c, a, b, es, 2); }

/* ---------------------------------------------------------------- estimativas */

static int recip_estimate(int a) {          /* RecipEstimate: 256 <= a < 512 */
    a = a * 2 + 1;
    int b = (1 << 19) / a;
    return (b + 1) / 2;
}
static int rsqrt_estimate(int a) {          /* RecipSqrtEstimate: 128 <= a < 512 */
    if (a < 256) a = a * 2 + 1;
    else { a = (a >> 1) << 1; a = (a + 1) * 2; }
    int b = 512;
    while ((long long)a * (b + 1) * (b + 1) < (1ll << 28)) b++;
    return (b + 1) / 2;
}

/* fração de 52 bits e expoente cru do operando (campos do FPRecipEstimate/FPRSqrtEstimate) */
static void raw_frac52(const fmt *f, u64 bits, u64 *frac, int *exp) {
    *exp = (int)((bits >> f->F) & ((1u << f->E) - 1));
    *frac = (bits & ((1ull << f->F) - 1)) << (52 - f->F);
}

a64_u64 a64_fp_recpe(a64_cpu *c, a64_u64 a, int es) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    int fz = fz_of(f, fpcr), rm = rmode_of(fpcr);
    fpv x;
    u64 r;
    unpack(f, a, fz, 0, &x, &fl);
    int emsb = (x.type == T_NORMAL || x.type == T_DENORM) ? x.exp + msb64(x.sig) : 0;
    int lo = f->N == 16 ? -16 : f->N == 32 ? -128 : -1024;     /* |v| < 2^lo */
    int hi = f->N == 16 ? 14 : f->N == 32 ? 126 : 1022;        /* |v| >= 2^hi */
    if (is_nan(&x)) r = process_nan(f, &x, fpcr, &fl);
    else if (x.type == T_INF) r = fzero(f, x.sign);
    else if (x.type == T_ZERO) { r = finf(f, x.sign); fl |= DZC; }
    else if (emsb < lo) {
        int oti = rm == A64_RM_TIEEVEN ? 1 : rm == A64_RM_POSINF ? !x.sign : rm == A64_RM_NEGINF ? x.sign : 0;
        r = oti ? finf(f, x.sign) : fmaxnormal(f, x.sign);
        fl |= OFC | IXC;
    } else if (fz && emsb >= hi) {
        r = fzero(f, x.sign);
        fl |= UFC;
    } else {
        u64 frac; int exp;
        raw_frac52(f, a, &frac, &exp);
        if (exp == 0) {
            if (!((frac >> 51) & 1)) { exp = -1; frac = (frac << 2) & ((1ull << 52) - 1); }
            else frac = (frac << 1) & ((1ull << 52) - 1);
        }
        int scaled = (int)(256 | (frac >> 44));
        int result_exp = (f->N == 16 ? 29 : f->N == 32 ? 253 : 2045) - exp;
        int est = recip_estimate(scaled);
        frac = (u64)(est & 0xFF) << 44;
        if (result_exp == 0) frac = (1ull << 51) | (frac >> 1);
        else if (result_exp == -1) { frac = (1ull << 50) | (frac >> 2); result_exp = 0; }
        r = ((u64)x.sign << (f->N - 1)) | ((u64)result_exp << f->F) | (frac >> (52 - f->F));
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_rsqrte(a64_cpu *c, a64_u64 a, int es) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    u64 r;
    unpack(f, a, fz_of(f, fpcr), 0, &x, &fl);
    if (is_nan(&x)) r = process_nan(f, &x, fpcr, &fl);
    else if (x.type == T_ZERO) { r = finf(f, x.sign); fl |= DZC; }
    else if (x.sign) { r = fdnan(f); fl |= IOC; }
    else if (x.type == T_INF) r = fzero(f, 0);
    else {
        u64 frac; int exp;
        raw_frac52(f, a, &frac, &exp);
        if (exp == 0) {
            while (!((frac >> 51) & 1)) { frac = (frac << 1) & ((1ull << 52) - 1); exp--; }
            frac = (frac << 1) & ((1ull << 52) - 1);
        }
        int scaled = (exp & 1) == 0 ? (int)(256 | (frac >> 44)) : (int)(128 | (frac >> 45));
        int base = f->N == 16 ? 44 : f->N == 32 ? 380 : 3068;
        int result_exp = (base - exp) / 2;           /* base - exp > 0 sempre */
        int est = rsqrt_estimate(scaled);
        r = ((u64)result_exp << f->F) | ((u64)(est & 0xFF) << (f->F - 8));
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_recpx(a64_cpu *c, a64_u64 a, int es) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    u64 r;
    unpack(f, a, fz_of(f, fpcr), 0, &x, &fl);
    if (is_nan(&x)) r = process_nan(f, &x, fpcr, &fl);
    else {
        u64 emask = (1u << f->E) - 1;
        u64 e = (a >> f->F) & emask;
        r = ((u64)x.sign << (f->N - 1)) | ((e == 0 ? emask - 1 : (~e & emask)) << f->F);
    }
    c->fpsr |= fl;
    return r;
}

a64_u32 a64_recpe_u32(a64_u32 a) {
    if (!(a >> 31)) return 0xFFFFFFFFu;
    return (a64_u32)(recip_estimate((int)(a >> 23)) & 0x1FF) << 23;
}
a64_u32 a64_rsqrte_u32(a64_u32 a) {
    if (!(a >> 30)) return 0xFFFFFFFFu;
    return (a64_u32)(rsqrt_estimate((int)(a >> 23)) & 0x1FF) << 23;
}

/* ---------------------------------------------------------------- inteiros */

/* arredonda |v| = sig * 2^-sh (sh > 0) para inteiro conforme rm e o sinal do valor */
static u64 round_mag(u64 sig, int sh, int sign, int rm, int *inexact) {
    u64 m = sh >= 64 ? 0 : sig >> sh;
    int rbit = sh - 1 >= 64 ? 0 : (int)((sig >> (sh - 1)) & 1);
    int sticky = sh - 1 >= 64 ? sig != 0 : (sig & ((1ull << (sh - 1)) - 1)) != 0;
    int up;
    switch (rm) {
    case A64_RM_TIEEVEN: up = rbit && (sticky || (m & 1)); break;
    case A64_RM_POSINF:  up = (rbit || sticky) && !sign; break;
    case A64_RM_NEGINF:  up = (rbit || sticky) && sign; break;
    case A64_RM_TIEAWAY: up = rbit; break;
    default:             up = 0; break;
    }
    *inexact = rbit | sticky;
    return m + up;
}

/* parte inteira arredondada de um finito não nulo multiplicado por 2^fbits.
 * Devolve 0 e *huge = 1 se a magnitude é >= 2^64. */
static u64 to_int_mag(const fpv *x, int fbits, int rm, int *inexact, int *huge) {
    int t = x->exp + fbits;
    *huge = 0; *inexact = 0;
    if (t >= 0) {
        if (msb64(x->sig) + t >= 64) { *huge = 1; return 0; }
        return x->sig << t;
    }
    return round_mag(x->sig, -t, x->sign, rm, inexact);
}

a64_u64 a64_fp_roundint(a64_cpu *c, a64_u64 a, int es, int rm, int exact) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    u64 r;
    rm = resolve_rm(fpcr, rm);
    unpack(f, a, fz_of(f, fpcr), 0, &x, &fl);
    if (is_nan(&x)) r = process_nan(f, &x, fpcr, &fl);
    else if (x.type == T_INF) r = finf(f, x.sign);
    else if (x.type == T_ZERO) r = fzero(f, x.sign);
    else if (x.exp >= 0) r = a;                          /* já é inteiro */
    else {
        int inexact;
        u64 m = round_mag(x.sig, -x.exp, x.sign, rm, &inexact);
        r = m == 0 ? fzero(f, x.sign) : fpround(f, x.sign, m, 0, fpcr, A64_RM_ZERO, &fl);
        if (inexact && exact) fl |= IXC;
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_roundint_n(a64_cpu *c, a64_u64 a, int es, int rm, int intbits) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    u64 r;
    u64 minint = encode(f, 1, (u64)(f->bias + intbits - 1), 0);   /* -2^(intbits-1) */
    rm = resolve_rm(fpcr, rm);
    unpack(f, a, fz_of(f, fpcr), 0, &x, &fl);
    if (is_nan(&x) || x.type == T_INF) { r = minint; fl |= IOC; }
    else if (x.type == T_ZERO) r = fzero(f, x.sign);
    else {
        int inexact, huge;
        u64 m = to_int_mag(&x, 0, rm, &inexact, &huge);
        u64 lim = 1ull << (intbits - 1);
        if (huge || (x.sign ? m > lim : m > lim - 1)) { r = minint; fl |= IOC; }
        else {
            r = m == 0 ? fzero(f, x.sign) : fpround(f, x.sign, m, 0, fpcr, A64_RM_ZERO, &fl);
            if (inexact) fl |= IXC;
        }
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_fp_to_int(a64_cpu *c, a64_u64 a, int es, int fbits, int ibits, int is_unsigned, int rm) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    u64 r = 0;
    u64 mask = ibits >= 64 ? ~0ull : (1ull << ibits) - 1;
    rm = resolve_rm(fpcr, rm);
    unpack(f, a, fz_of(f, fpcr), 0, &x, &fl);
    if (is_nan(&x)) { fl |= IOC; r = 0; }
    else if (x.type == T_ZERO) r = 0;
    else {
        int inexact = 0, huge = 0, ovf;
        u64 m = 0;
        if (x.type == T_INF) huge = 1;
        else m = to_int_mag(&x, fbits, rm, &inexact, &huge);
        if (is_unsigned) {
            if (x.sign) { ovf = huge || m != 0; r = 0; }
            else { ovf = huge || m > mask; r = ovf ? mask : m; }
        } else {
            u64 lim = 1ull << (ibits - 1);
            if (x.sign) { ovf = huge || m > lim; r = ovf ? lim : (0 - m); }
            else { ovf = huge || m > lim - 1; r = ovf ? lim - 1 : m; }
            if (x.sign && ovf) r = 0 - lim;
        }
        r &= mask;
        if (ovf) fl |= IOC;
        else if (inexact) fl |= IXC;
    }
    c->fpsr |= fl;
    return r;
}

a64_u64 a64_int_to_fp(a64_cpu *c, a64_u64 v, int ibits, int is_unsigned, int fbits, int es, int rm) {
    const fmt *f = fmt_of(es);
    u32 fpcr = c->fpcr, fl = 0;
    u64 r;
    rm = resolve_rm(fpcr, rm);
    if (ibits < 64) {
        v &= (1ull << ibits) - 1;
        if (!is_unsigned && ((v >> (ibits - 1)) & 1)) v |= ~0ull << ibits;
    }
    int sign = !is_unsigned && (a64_i64)v < 0;
    u64 m = sign ? 0 - v : v;
    if (m == 0) r = fzero(f, 0);
    else r = fpround(f, sign, m, -fbits, fpcr, rm, &fl);
    c->fpsr |= fl;
    return r;
}

a64_u32 a64_fp_jcvtzs(a64_cpu *c, a64_u64 d, int *z) {
    u32 fpcr = c->fpcr, fl = 0;
    fpv x;
    int Z = 1;
    u64 low = 0;
    unpack(&F64, d, !!(fpcr & A64_FPCR_FZ), 0, &x, &fl);
    if (is_nan(&x)) { fl |= IOC; Z = 0; }
    else if (x.type == T_INF) { fl |= IOC; Z = 0; low = 0; }
    else if (x.type == T_ZERO) { if (x.sign || (d & ((1ull << 52) - 1))) Z = 0; }
    else {
        int inexact = 0, ovf;
        u64 m;
        if (x.exp >= 0) {
            ovf = msb64(x.sig) + x.exp >= 31 + x.sign ? 1 : 0;
            m = x.exp >= 64 ? 0 : x.sig << x.exp;     /* só os bits baixos importam */
            if (!ovf) ovf = x.sign ? m > (1ull << 31) : m > (1ull << 31) - 1;
        } else {
            m = round_mag(x.sig, -x.exp, x.sign, A64_RM_ZERO, &inexact);
            ovf = x.sign ? m > (1ull << 31) : m > (1ull << 31) - 1;
        }
        low = x.sign ? 0 - m : m;
        if (ovf) { fl |= IOC; Z = 0; }
        else if (inexact) { fl |= IXC; Z = 0; }
    }
    *z = Z;
    c->fpsr |= fl;
    return (a64_u32)low;
}

/* ---------------------------------------------------------------- conversões FP↔FP */

/* FPConvertNaN: preserva sinal e os bits altos do payload (abaixo do bit de quiet) */
static u64 convert_nan(const fmt *from, const fmt *to, u64 op) {
    int sign = (int)((op >> (from->N - 1)) & 1);
    u64 data = (op & ((1ull << (from->F - 1)) - 1)) << (51 - (from->F - 1));   /* 51 bits */
    u64 r = encode(to, sign, (1u << to->E) - 1, 1ull << (to->F - 1));
    return r | ((data >> (51 - (to->F - 1))) << to->shift);
}

a64_u64 a64_fp_convert(a64_cpu *c, a64_u64 a, int from_es, int to_es, int rm) {
    const fmt *ff = fmt_of(from_es), *tf = fmt_of(to_es);
    u32 fpcr = c->fpcr, fl = 0;
    int ahp = !!(fpcr & A64_FPCR_AHP), fz = !!(fpcr & A64_FPCR_FZ);
    int alt_hp = to_es == 2 && ahp;
    fpv x;
    u64 r;
    rm = resolve_rm(fpcr, rm);
    unpack(ff, a, from_es == 2 ? 0 : fz, from_es == 2 && ahp, &x, &fl);
    if (is_nan(&x)) {
        if (alt_hp) r = fzero(tf, x.sign);
        else if (fpcr & A64_FPCR_DN) r = fdnan(tf);
        else r = convert_nan(ff, tf, a);
        if (x.type == T_SNAN || alt_hp) fl |= IOC;
    } else if (x.type == T_INF) {
        if (alt_hp) { r = ((u64)x.sign << 15) | 0x7FFF; fl |= IOC; }
        else r = finf(tf, x.sign);
    } else if (x.type == T_ZERO) r = fzero(tf, x.sign);
    else r = round_base(tf, x.sign, x.sig, x.exp, rm, to_es == 2 ? 0 : fz, alt_hp, &fl);
    c->fpsr |= fl;
    return r;
}

a64_u16 a64_fp_to_bf16(a64_cpu *c, a64_u32 s) {
    u32 fpcr = c->fpcr, fl = 0;
    int fz = !!(fpcr & A64_FPCR_FZ);
    fpv x;
    u64 r;
    unpack(&F32, s, fz, 0, &x, &fl);
    if (is_nan(&x)) {
        r = (fpcr & A64_FPCR_DN) ? fdnan(&F32) : convert_nan(&F32, &F32, s);
        if (x.type == T_SNAN) fl |= IOC;
    } else if (x.type == T_INF) r = finf(&F32, x.sign);
    else if (x.type == T_ZERO) r = fzero(&F32, x.sign);
    else r = round_base(&FBF, x.sign, x.sig, x.exp, rmode_of(fpcr), fz, 0, &fl);
    c->fpsr |= fl;
    return (a64_u16)(r >> 16);
}

/* ---------------------------------------------------------------- constantes */

a64_u64 a64_fp_default_nan(int es) { return fdnan(fmt_of(es)); }

a64_u64 a64_fp_expand_imm8(a64_u8 imm8, int es) {
    const fmt *f = fmt_of(es);
    u64 sign = (imm8 >> 7) & 1, b6 = (imm8 >> 6) & 1;
    u64 exp = ((b6 ^ 1) << (f->E - 1)) | ((b6 ? (1ull << (f->E - 3)) - 1 : 0) << 2) | ((imm8 >> 4) & 3);
    u64 frac = (u64)(imm8 & 15) << (f->F - 4);
    return (sign << (f->N - 1)) | (exp << f->F) | frac;
}

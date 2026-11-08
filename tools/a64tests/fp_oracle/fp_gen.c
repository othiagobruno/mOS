/* mOS — oráculo FP: gerador de vetores (host x86_64).
 *   fp_gen <in.bin> <saida-no-qemu.bin> [vetores-por-modelo] [semente] [filtro]
 * Modelos: todas as instruções de FP escalar do A64 (H/S/D), mais palavras aleatórias do grupo
 * (bit 28 = 1, bit 30 = 0) para cobrir a decodificação/UDF. Operandos com viés para casos de
 * borda: ±0, ±inf, QNaN/SNaN com payload, subnormais, min/max normal, inteiros, empates, limites
 * de conversão, cancelamento em add/FMA. FPCR com todas as combinações de RMode/FZ/DN/FZ16/AHP.
 * Usa as primitivas do próprio tradutor só para *construir* entradas (produtos para FMA). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fpvec.h"
#include "../../../userland/a64/a64.h"

typedef uint64_t u64;
typedef uint32_t u32;

static u64 g_s = 0x9E3779B97F4A7C15ull;
static u64 rnd(void) { g_s ^= g_s >> 12; g_s ^= g_s << 25; g_s ^= g_s >> 27; return g_s * 0x2545F4914F6CDD1Dull; }
static u64 rr(u64 n) { return rnd() % n; }

static a64_cpu g_c;   /* só para montar entradas */

static int F_of(int es) { return es == 2 ? 10 : es == 4 ? 23 : 52; }
static u64 mk(int es, u64 s, u64 e, u64 f) { int F = F_of(es); return (s << (es * 8 - 1)) | (e << F) | (f & ((1ull << F) - 1)); }

static u64 gen_fp(int es) {
    int N = es * 8, F = F_of(es), E = N - 1 - F;
    u64 emax = (1ull << E) - 1, bias = emax >> 1, fm = (1ull << F) - 1;
    u64 s = rnd() & 1, f;
    switch (rr(24)) {
    case 0: return mk(es, s, 0, 0);
    case 1: return mk(es, s, emax, 0);
    case 2: f = (1ull << (F - 1)) | (rr(4) ? rnd() & (fm >> 1) : 0); return mk(es, s, emax, f);
    case 3: f = rnd() & (fm >> 1); if (!f) f = 1 + rr(3); return mk(es, s, emax, f);
    case 4: case 5:
        switch (rr(5)) {
        case 0: f = 1; break;
        case 1: f = fm; break;
        case 2: f = 1ull << rr(F); break;
        case 3: f = (rnd() & fm) >> rr(F); break;
        default: f = rnd() & fm; break;
        }
        if (!f) f = 1;
        return mk(es, s, 0, f);
    case 6: return mk(es, s, rr(2), rr(2) ? rr(8) : fm - rr(8));
    case 7: return mk(es, s, emax - 1 - rr(2), rr(2) ? fm - rr(8) : rr(8));
    case 8: case 9: {
        int k = (int)rr(4001) - 2000;
        return a64_int_to_fp(&g_c, (u64)(int64_t)k, 64, 0, (int)rr(4), es, A64_RM_TIEEVEN);
    }
    case 10: case 11: {
        static const int p[] = {10, 11, 15, 16, 23, 24, 31, 32, 52, 53, 63, 64};
        u64 e = bias + p[rr(12)] - rr(2);
        if (e >= emax) e = emax - 1;
        return mk(es, s, e, rr(2) ? rr(16) : fm - rr(16));
    }
    case 12: case 13: return mk(es, s, bias - 4 + rr(9), rnd() & fm);
    case 14: {                               /* padrão de empate: ...1000 numa posição qualquer */
        int k = 1 + (int)rr(F);
        f = ((rnd() & fm) & ~((1ull << k) - 1)) | (1ull << (k - 1));
        return mk(es, s, rr(emax), f);
    }
    default: {
        u64 e = rr(emax);
        f = rnd() & fm;
        if (rr(3) == 0) f &= ~((1ull << rr(F)) - 1);
        return mk(es, s, e, f);
    }
    }
}

/* finito com expoente sem viés 'ue' (subnormal se abaixo do mínimo do formato) */
static u64 gen_at(int es, int ue) {
    int N = es * 8, F = F_of(es), E = N - 1 - F;
    int bias = (1 << (E - 1)) - 1, emin = 1 - bias;
    u64 fm = (1ull << F) - 1, f = rnd() & fm, s = rnd() & 1;
    switch (rr(4)) {
    case 0: f = rr(2) ? fm - rr(4) : rr(4); break;
    case 1: { int k = 1 + (int)rr(F); f = (f & ~((1ull << k) - 1)) | (1ull << (k - 1)); break; }
    default: break;
    }
    if (ue >= emin) {
        if (ue > bias) ue = bias;
        return mk(es, s, (u64)(ue + bias), f);
    }
    int sh = emin - ue;                       /* subnormal: 2^ue * 1.f */
    if (sh > F) return mk(es, s, 0, 1 + rr(3));
    return mk(es, s, 0, ((f | (1ull << F)) >> sh) ? ((f | (1ull << F)) >> sh) : 1);
}

/* expoentes interessantes como destino de conversões (min/max normal e subnormal de h/s/bf16/d) */
static int interesting_exp(void) {
    static const int p[] = {-14, -15, -16, -24, -25, -23, 15, 16, -126, -127, -128, -149, -150, -148,
                            127, 128, -1022, -1023, -1074, -1075, 1023, 0, 31, 63, 64, 32};
    return p[rr(sizeof p / sizeof p[0])] + (int)rr(3) - 1;
}

/* pares (a, b) cujo produto ou quociente cai perto de min normal, max normal ou no subnormal */
static void gen_boundary_pair(int es, u64 *a, u64 *b) {
    int N = es * 8, F = F_of(es), E = N - 1 - F;
    int bias = (1 << (E - 1)) - 1, emin = 1 - bias;
    int target;
    switch (rr(4)) {
    case 0: target = emin - 1 + (int)rr(3); break;
    case 1: target = emin - 1 - (int)rr(F + 2); break;
    case 2: target = bias - (int)rr(2); break;
    default: target = bias + 1; break;
    }
    int e1 = emin + (int)rr((u64)(2 * bias));
    int e2 = rr(2) ? target - e1 : e1 - target;      /* produto ou quociente */
    if (e2 < emin - F) e2 = emin - F;
    if (e2 > bias) e2 = bias;
    *a = gen_at(es, e1);
    *b = gen_at(es, e2);
}

/* b relacionado com a (cancelamento, empates, expoentes próximos) */
static u64 related(int es, u64 a) {
    int F = F_of(es), N = es * 8;
    u64 sb = 1ull << (N - 1);
    switch (rr(4)) {
    case 0: return a ^ sb;
    case 1: return (a ^ sb) + (rr(2) ? rr(8) : -(u64)rr(8));
    case 2: return a + ((u64)(rr(2 * F) + 1) << F) * (rr(2) ? 1 : (u64)-1);
    default: return (a & ~((1ull << F) - 1)) | (rnd() & ((1ull << F) - 1));
    }
}

static u64 mask_es(int es) { return es == 8 ? ~0ull : (1ull << (es * 8)) - 1; }

static u32 gen_fpcr(void) {
    u32 r = (u32)rr(4) << A64_FPCR_RMODE_SHIFT;
    if (rr(2)) r |= A64_FPCR_FZ;
    if (rr(2)) r |= A64_FPCR_DN;
    if (rr(2)) r |= A64_FPCR_FZ16;
    if (rr(2)) r |= A64_FPCR_AHP;
    return r;
}

static u64 gen_int(void) {
    static const u64 k[] = {0, 1, ~0ull, 0x7FFFFFFFull, 0x80000000ull, 0xFFFFFFFFull, 0x100000000ull,
                            0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFF, 0x10000, 0x8000, 0x7FFF};
    switch (rr(6)) {
    case 0: return k[rr(13)] + (rr(2) ? rr(4) : -(u64)rr(4));
    case 1: return rnd() >> rr(64);
    case 2: return (rnd() >> rr(64)) << rr(64);
    case 3: return (u64)-(int64_t)(rnd() >> rr(64));
    case 4: return (1ull << rr(64)) + (rr(2) ? rr(4) : -(u64)rr(4));
    default: return rnd();
    }
}

/* valores perto de inteiros e de limites de conversão */
static u64 gen_near_int(int es) {
    if (rr(3) == 0) return gen_fp(es);
    u64 v = gen_int();
    int sh = (int)rr(3) ? 0 : (int)rr(70);
    u64 r = a64_int_to_fp(&g_c, v, 64, (int)rr(2), sh, es, (int)rr(4));
    if ((r & ~(1ull << (es * 8 - 1))) == 0) return r;
    return (r + (rr(2) ? rr(4) : -(u64)rr(4))) & mask_es(es);
}

enum { K_2SRC, K_3SRC, K_1SRC, K_CMP, K_CCMP, K_CSEL, K_IMM, K_F2I, K_I2F, K_FMOV, K_FIXF2I, K_FIXI2F, K_RAND, K_PRIM1, K_PRIM2 };

typedef struct tmpl { char name[40]; u32 insn; int kind, es, sf; } tmpl;
static tmpl T[400];
static int NT;

static void add(const char *name, u32 insn, int kind, int es, int sf) {
    tmpl *t = &T[NT++];
    snprintf(t->name, sizeof t->name, "%s", name);
    t->insn = insn; t->kind = kind; t->es = es; t->sf = sf;
}

static u32 ft(int es) { return es == 4 ? 0 : es == 8 ? 1 : 3; }
static const char *esn(int es) { return es == 2 ? "h" : es == 4 ? "s" : "d"; }

static void build(void) {
    static const int ess[3] = {4, 8, 2};
    char nm[64];
    static const char *n2[] = {"fmul", "fdiv", "fadd", "fsub", "fmax", "fmin", "fmaxnm", "fminnm", "fnmul"};
    static const char *n3[] = {"fmadd", "fmsub", "fnmadd", "fnmsub"};
    static const char *nr[] = {"frintn", "frintp", "frintm", "frintz", "frinta", "?", "frintx", "frinti"};
    static const char *ncv[] = {"fcvtn", "fcvtp", "fcvtm", "fcvtz"};
    for (int i = 0; i < 3; i++) {
        int es = ess[i];
        u32 f = ft(es) << 22;
        for (int op = 0; op < 9; op++) { snprintf(nm, 64, "%s.%s", n2[op], esn(es)); add(nm, 0x1E200800 | f | (op << 12) | (2 << 16) | (1 << 5), K_2SRC, es, 0); }
        for (int op = 0; op < 4; op++) { snprintf(nm, 64, "%s.%s", n3[op], esn(es)); add(nm, 0x1F000000 | f | ((op >> 1) << 21) | ((op & 1) << 15) | (2 << 16) | (3 << 10) | (1 << 5), K_3SRC, es, 0); }
        static const char *n1[] = {"fmov", "fabs", "fneg", "fsqrt"};
        for (int op = 0; op < 4; op++) { snprintf(nm, 64, "%s.%s", n1[op], esn(es)); add(nm, 0x1E204000 | f | (op << 15) | (1 << 5), K_1SRC, es, 0); }
        for (int opc = 0; opc < 4; opc++) {
            int tes = opc == 0 ? 4 : opc == 1 ? 8 : opc == 3 ? 2 : 0;
            if (!tes || tes == es) continue;
            snprintf(nm, 64, "fcvt.%s.%s", esn(tes), esn(es));
            add(nm, 0x1E204000 | f | ((4 | opc) << 15) | (1 << 5), K_1SRC, es, 0);
        }
        for (int op = 0; op < 8; op++) { if (op == 5) continue; snprintf(nm, 64, "%s.%s", nr[op], esn(es)); add(nm, 0x1E204000 | f | ((8 + op) << 15) | (1 << 5), K_1SRC, es, 0); }
        if (es != 2) {
            static const char *nn[] = {"frint32z", "frint32x", "frint64z", "frint64x"};
            for (int op = 0; op < 4; op++) { snprintf(nm, 64, "%s.%s", nn[op], esn(es)); add(nm, 0x1E204000 | f | ((16 + op) << 15) | (1 << 5), K_1SRC, es, 0); }
        }
        static const char *nc[] = {"fcmp", "fcmp0", "fcmpe", "fcmpe0"};
        for (int op = 0; op < 4; op++) { snprintf(nm, 64, "%s.%s", nc[op], esn(es)); add(nm, 0x1E202000 | f | (2 << 16) | (1 << 5) | (op << 3), K_CMP, es, 0); }
        snprintf(nm, 64, "fccmp.%s", esn(es)); add(nm, 0x1E200400 | f | (2 << 16) | (1 << 5), K_CCMP, es, 0);
        snprintf(nm, 64, "fccmpe.%s", esn(es)); add(nm, 0x1E200410 | f | (2 << 16) | (1 << 5), K_CCMP, es, 0);
        snprintf(nm, 64, "fcsel.%s", esn(es)); add(nm, 0x1E200C00 | f | (2 << 16) | (1 << 5), K_CSEL, es, 0);
        snprintf(nm, 64, "fmovi.%s", esn(es)); add(nm, 0x1E201000 | f, K_IMM, es, 0);
        for (int sf = 0; sf < 2; sf++) {
            u32 b = (sf << 31) | 0x1E200000 | f | (1 << 5);
            const char *r = sf ? "x" : "w";
            for (int rm = 0; rm < 4; rm++) for (int u = 0; u < 2; u++) {
                snprintf(nm, 64, "%s%s.%s.%s", ncv[rm], u ? "u" : "s", r, esn(es));
                add(nm, b | (rm << 19) | (u << 16), K_F2I, es, sf);
            }
            for (int u = 0; u < 2; u++) { snprintf(nm, 64, "fcvta%s.%s.%s", u ? "u" : "s", r, esn(es)); add(nm, b | ((4 + u) << 16), K_F2I, es, sf); }
            for (int u = 0; u < 2; u++) { snprintf(nm, 64, "%scvtf.%s.%s", u ? "u" : "s", esn(es), r); add(nm, b | ((2 + u) << 16), K_I2F, es, sf); }
            u32 bf = (sf << 31) | 0x1E000000 | f | (1 << 5);
            for (int u = 0; u < 2; u++) { snprintf(nm, 64, "%scvtf.fix.%s.%s", u ? "u" : "s", esn(es), r); add(nm, bf | ((2 + u) << 16), K_FIXI2F, es, sf); }
            for (int u = 0; u < 2; u++) { snprintf(nm, 64, "fcvtz%s.fix.%s.%s", u ? "u" : "s", r, esn(es)); add(nm, bf | (3 << 19) | u << 16, K_FIXF2I, es, sf); }
        }
    }
    /* FMOV gerais */
    add("fmov.w.s", 0x1E260020, K_FMOV, 4, 0);  add("fmov.s.w", 0x1E270020, K_FMOV, 4, 0);
    add("fmov.x.d", 0x9E660020, K_FMOV, 8, 1);  add("fmov.d.x", 0x9E670020, K_FMOV, 8, 1);
    add("fmov.w.h", 0x1EE60020, K_FMOV, 2, 0);  add("fmov.h.w", 0x1EE70020, K_FMOV, 2, 0);
    add("fmov.x.h", 0x9EE60020, K_FMOV, 2, 1);  add("fmov.h.x", 0x9EE70020, K_FMOV, 2, 1);
    add("fmov.x.d1", 0x9EAE0020, K_FMOV, 8, 1); add("fmov.d1.x", 0x9EAF0020, K_FMOV, 8, 1);
    add("fjcvtzs", 0x1E7E0020, K_F2I, 8, 0);
    add("bfcvt", 0x1E634020, K_1SRC, 4, 0);
    add("aleatorio", 0, K_RAND, 4, 0);
    /* primitivas usadas pelo AdvSIMD: formas escalares (s, d, h) */
    {
        static const struct { const char *n; u32 sd, h; int prim, two; } P[] = {
            {"p.fmulx", 0x5E20DC00, 0x5E401C00, P_MULX, 1},   {"p.frecps", 0x5E20FC00, 0x5E403C00, P_RECPS, 1},
            {"p.frsqrts", 0x5EA0FC00, 0x5EC03C00, P_RSQRTS, 1}, {"p.fabd", 0x7EA0D400, 0x7EC01400, P_ABD, 1},
            {"p.fcmeq", 0x5E20E400, 0x5E402400, P_CMEQ, 1},   {"p.fcmge", 0x7E20E400, 0x7E402400, P_CMGE, 1},
            {"p.fcmgt", 0x7EA0E400, 0x7EC02400, P_CMGT, 1},   {"p.facge", 0x7E20EC00, 0x7E402C00, P_ACGE, 1},
            {"p.facgt", 0x7EA0EC00, 0x7EC02C00, P_ACGT, 1},   {"p.frecpe", 0x5EA1D800, 0x5EF9D800, P_RECPE, 0},
            {"p.frsqrte", 0x7EA1D800, 0x7EF9D800, P_RSQRTE, 0}, {"p.frecpx", 0x5EA1F800, 0x5EF9F800, P_RECPX, 0},
            {"p.fcvtzs", 0x5EA1B800, 0x5EF9B800, P_FCVTZS_SD, 0}, {"p.scvtf", 0x5E21D800, 0x5E79D800, P_SCVTF_SD, 0},
        };
        for (unsigned i = 0; i < sizeof P / sizeof P[0]; i++)
            for (int k = 0; k < 3; k++) {
                int es = k == 0 ? 4 : k == 1 ? 8 : 2;
                u32 w = es == 2 ? P[i].h : P[i].sd | (es == 8 ? 1u << 22 : 0);
                w |= (1 << 5) | (P[i].two ? 2 << 16 : 0);
                snprintf(nm, 64, "%s.%s", P[i].n, esn(es));
                int prim = P[i].prim;
                if (es == 2 && prim == P_FCVTZS_SD) prim = P_FCVTZS16;
                if (es == 2 && prim == P_SCVTF_SD) prim = P_SCVTF16;
                add(nm, w, P[i].two ? K_PRIM2 : K_PRIM1, es, prim);
            }
        add("p.fcvtzu.h", 0x7EF9B800 | (1 << 5), K_PRIM1, 2, P_FCVTZU16);
        add("p.fcvtns.h", 0x5E79A800 | (1 << 5), K_PRIM1, 2, P_FCVTNS16);
        add("p.fcvtau.h", 0x7E79C800 | (1 << 5), K_PRIM1, 2, P_FCVTAU16);
        add("p.ucvtf.h", 0x7E79D800 | (1 << 5), K_PRIM1, 2, P_UCVTF16);
        add("p.fcvtxn", 0x7E616800 | (1 << 5), K_PRIM1, 8, P_FCVTXN);
        add("p.urecpe.2s", 0x0EA1C800 | (1 << 5), K_PRIM1, 4, P_URECPE);
        add("p.ursqrte.2s", 0x2EA1C800 | (1 << 5), K_PRIM1, 4, P_URSQRTE);
    }
}

static u64 garbage(void) { return rr(4) ? rnd() : 0; }

/* mascara campos de registrador para 0..3 conforme o grupo (palavras aleatórias) */
static u32 mask_regs(u32 w) {
    u32 rd = 0x1C, rn = 0x380, rm = 0x1C0000, ra = 0x7000;
    if (w & (1u << 24)) return w & ~(rd | rn | rm | ra);
    if (!(w & (1u << 21))) return w & ~(rd | rn);
    switch ((w >> 10) & 3) {
    case 1: return w & ~(rn | rm);
    case 2: case 3: return w & ~(rd | rn | rm);
    }
    if (((w >> 10) & 7) == 4) return w & ~rd;
    if (((w >> 10) & 15) == 8) return w & ~(rn | rm);
    return w & ~(rd | rn);
}

static void gen_vec(const tmpl *t, int ti, fpvec_in *v) {
    memset(v, 0, sizeof *v);
    v->ret = 0xd65f03c0;
    v->tmpl = (u32)ti;
    v->insn = t->insn;
    v->fpcr = rr(8) ? gen_fpcr() : 0;
    v->nzcv = (u32)rr(16) << 28;
    for (int i = 0; i < 4; i++) { v->x[i] = garbage(); v->v[i][0] = garbage(); v->v[i][1] = garbage(); }
    int es = t->es;
    u64 hi = ~mask_es(es) & (rr(2) ? rnd() : 0);
    switch (t->kind) {
    case K_2SRC: case K_CMP: case K_CCMP: case K_CSEL: {
        u64 a = gen_fp(es), b = rr(3) == 0 ? related(es, a) & mask_es(es) : gen_fp(es);
        if (rr(10) == 0) b = a;
        if (t->kind == K_2SRC) switch (rr(6)) {
        case 0: gen_boundary_pair(es, &a, &b); break;
        case 1: {                                  /* soma/diferença perto do mínimo normal */
            int emin = es == 2 ? -14 : es == 4 ? -126 : -1022;
            a = gen_at(es, emin - 1 + (int)rr(3)); b = gen_at(es, emin - (int)rr((u64)F_of(es) + 2));
            break;
        }
        case 2: {                                  /* soma perto do máximo */
            int emax = es == 2 ? 15 : es == 4 ? 127 : 1023;
            a = gen_at(es, emax - (int)rr(2)); b = gen_at(es, emax - (int)rr((u64)F_of(es) + 3));
            break;
        }
        default: break;
        }
        v->v[1][0] = a | hi; v->v[2][0] = b | hi;
        if (t->kind == K_CCMP) v->insn |= ((u32)rr(16) << 12) | (u32)rr(16);
        if (t->kind == K_CSEL) v->insn |= (u32)rr(16) << 12;
        break;
    }
    case K_3SRC: {
        u64 n = gen_fp(es), m = gen_fp(es), a;
        if (rr(4) == 0) gen_boundary_pair(es, &n, &m);
        if (rr(3) == 0) {
            a64_cpu cc; memset(&cc, 0, sizeof cc);
            a = a64_fp_mul(&cc, n, m, es) ^ (1ull << (es * 8 - 1));
            if (rr(2)) a = (a + (rr(2) ? rr(4) : -(u64)rr(4))) & mask_es(es);
        } else if (rr(3) == 0) {
            int emin = es == 2 ? -14 : es == 4 ? -126 : -1022;
            a = gen_at(es, emin - (int)rr((u64)F_of(es) + 2) + 1);
        } else a = rr(3) == 0 ? related(es, n) & mask_es(es) : gen_fp(es);
        v->v[1][0] = n | hi; v->v[2][0] = m | hi; v->v[3][0] = a | hi;
        break;
    }
    case K_1SRC:
        switch (rr(3)) {
        case 0: v->v[1][0] = gen_near_int(es); break;
        case 1: v->v[1][0] = gen_at(es, interesting_exp()); break;
        default: v->v[1][0] = gen_fp(es); break;
        }
        v->v[1][0] |= hi;
        break;
    case K_IMM:
        v->insn |= (u32)rr(256) << 13;
        break;
    case K_F2I: case K_FIXF2I:
        v->v[1][0] = gen_near_int(es) | hi;
        if (t->kind == K_FIXF2I) v->insn |= (u32)(64 - 1 - rr(t->sf ? 64 : 32)) << 10;
        break;
    case K_I2F: case K_FIXI2F:
        v->x[1] = gen_int();
        if (!t->sf && rr(2)) v->x[1] |= rnd() << 32;   /* bits altos devem ser ignorados */
        if (t->kind == K_FIXI2F) v->insn |= (u32)(64 - 1 - rr(t->sf ? 64 : 32)) << 10;
        break;
    case K_FMOV:
        v->x[1] = rnd();
        v->v[1][0] = rnd(); v->v[1][1] = rnd();
        break;
    case K_PRIM1: case K_PRIM2: {
        v->pad[0] = (u32)t->sf; v->pad[1] = (u32)es;
        u64 a, b = gen_fp(es);
        switch (rr(4)) {
        case 0: a = gen_at(es, interesting_exp()); break;
        case 1: a = gen_near_int(es); break;
        default: a = gen_fp(es); break;
        }
        if (t->kind == K_PRIM2) {
            if (rr(3) == 0) b = related(es, a) & mask_es(es);
            else if (rr(4) == 0) gen_boundary_pair(es, &a, &b);
            if (t->sf == P_RECPS || t->sf == P_RSQRTS) {          /* a*b perto de 2 ou 3: cancelamento */
                if (rr(2)) { a64_cpu cc; memset(&cc, 0, sizeof cc);
                    b = a64_fp_recpe(&cc, a, es); if (t->sf == P_RSQRTS) a = a64_fp_mul(&cc, b, a, es); }
            }
        }
        if (t->sf == P_URECPE || t->sf == P_URSQRTE) { a = gen_int(); if (rr(2)) a = (a & ~0xFFFFFFFFull) | ((u64)(u32)gen_int()); }
        if (t->sf == P_UCVTF16 || t->sf == P_SCVTF16) a = gen_int() & 0xFFFF;
        v->v[1][0] = (a & mask_es(es)) | (t->sf == P_URECPE || t->sf == P_URSQRTE ? 0 : hi);
        v->v[2][0] = b | hi;
        break;
    }
    case K_RAND: {
        u32 w = (u32)rnd();
        w = (w & ~(1u << 30)) | (0xFu << 25);        /* bits 28..25 = 1111, bit 30 = 0 */
        w = mask_regs(w);
        v->insn = w;
        for (int i = 1; i < 4; i++) v->v[i][0] = rr(2) ? gen_fp(4) | ((u64)gen_fp(4) << 32) : gen_fp(8);
        v->x[1] = gen_int(); v->x[2] = gen_int();
        break;
    }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: fp_gen in.bin out-qemu.bin [n] [semente] [filtro]\n"); return 2; }
    int n = argc > 3 ? atoi(argv[3]) : 4000;
    if (argc > 4) g_s ^= (strtoull(argv[4], 0, 0) + 1) * 0xD1B54A32D192ED03ull;
    if (!g_s) g_s = 1;
    const char *filt = argc > 5 ? argv[5] : NULL;
    build();
    FILE *fo = fopen(argv[1], "wb");
    if (!fo) { perror(argv[1]); return 1; }
    fpvec_hdr h;
    memset(&h, 0, sizeof h);
    h.magic = FPVEC_MAGIC;
    snprintf(h.outname, sizeof h.outname, "%s", argv[2]);
    fwrite(&h, sizeof h, 1, fo);
    u64 cnt = 0;
    for (int ti = 0; ti < NT; ti++) {
        if (filt && !strstr(T[ti].name, filt)) continue;
        int m = T[ti].kind == K_RAND ? n * 40 : T[ti].kind == K_FMOV || T[ti].kind == K_IMM ? n / 8 + 64 : n;
        for (int i = 0; i < m; i++) {
            fpvec_in v;
            gen_vec(&T[ti], ti, &v);
            fwrite(&v, sizeof v, 1, fo);
            cnt++;
        }
    }
    h.count = cnt;
    fseek(fo, 0, SEEK_SET);
    fwrite(&h, sizeof h, 1, fo);
    fclose(fo);
    /* tabela de nomes para o verificador */
    char nb[600];
    snprintf(nb, sizeof nb, "%s.names", argv[1]);
    FILE *fn = fopen(nb, "w");
    for (int ti = 0; ti < NT; ti++) fprintf(fn, "%s\n", T[ti].name);
    fclose(fn);
    printf("%llu vetores, %d modelos\n", (unsigned long long)cnt, NT);
    return 0;
}

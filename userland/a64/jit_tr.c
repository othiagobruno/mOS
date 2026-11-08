/*
 * mOS — tradutor arm64 (frente JIT): tradução de um bloco A64 → x86_64.
 *
 * Bloco = até JIT_MAX_INSNS instruções, nunca cruza página de 4 KiB, termina em desvio/exceção.
 * Convenção na fronteira dos blocos (entrada, saídas, chamadas ao interpretador): registradores
 * do guest todos em a64_cpu, flags em reserved[14] (formato x86), r15 = a64_cpu*.
 *
 * Dentro do bloco:
 *  - cache de registradores: X0..X30/SP em rbx rbp rsi rdi r8..r14 (LRU, sujos escritos de volta
 *    na saída / antes de helpers). rax rcx rdx são temporários do gerador.
 *  - flags preguiçosas: ficam no EFLAGS do x86 depois da instrução que as calculou, em "modo ADD"
 *    (CF = C do ARM) ou "modo SUB" (CF = !C, porque o C do ARM numa subtração é não-borrow).
 *    Só vão para reserved[14] (lahf/seto) quando algo vai sobrescrever o EFLAGS e elas ainda
 *    servem (análise de vida para trás no bloco; acessos à memória contam como leitura para que
 *    o estado numa falha seja exato).
 *  - cada acesso à memória do guest vira um "ponto de falha" (jfp): offset x86, instrução guest,
 *    onde estão os registradores sujos e as flags. a64_jit_recover usa isso.
 *  - regra do contrato: nenhum registrador de destino é escrito antes do último acesso que pode
 *    falhar; só o ÚLTIMO acesso da instrução pode carregar direto no registrador de destino.
 * Instruções que não são emitidas nativamente chamam o interpretador (jit_h_interp) com o
 * estado sincronizado; casos CONSTRAINED UNPREDICTABLE também vão para o interpretador, para
 * que o JIT tenha exatamente o comportamento dele.
 */
#include "jit.h"
#include "a64_mem.h"

#define BITS(x, hi, lo) (((x) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1))
#define BIT(x, n) (((x) >> (n)) & 1u)
static inline int64_t sext(a64_u64 v, int bits) { return (int64_t)(v << (64 - bits)) >> (64 - bits); }

static const int HREGS[] = { XBX, XBP, XSI, XDI, X8, X9, X10, X11, X12, X13, X14 };
#define NHREGS ((int)(sizeof HREGS / sizeof HREGS[0]))

enum { C_RD = 1, C_WR = 2 };

typedef struct T {
    xb b;
    a64_u8 *start;
    a64_u64 pc0, pc;
    int n, i, ended;
    a64_u32 insn[JIT_MAX_INSNS];
    a64_u8 live[JIT_MAX_INSNS];         /* flags vivas depois da instrução i */
    signed char g2h[32], h2g[16];
    a64_u32 dirty, lock, pend, pnew, tmp;
    a64_u32 age[16], tick;
    int fe, fm;                         /* fe: 0 / 1 ADD / 2 SUB; fm: reserved[14] válido */
    int mxclean;                        /* MXCSR do host = 0x1F80 (RN, mascarado, sem flags) garantido */
    int nfp;
    jfp fp[JIT_MAX_INSNS * 4];
} T;

#define B (&t->b)

/* ---------------------------------------------------------------------------------------------
 * Cache de registradores
 * ------------------------------------------------------------------------------------------- */
static void h_evict(T *t, int h) {
    int g = t->h2g[h];
    if (g >= 0) {
        if (t->dirty & (1u << h)) x_mov_mr(B, 1, XM(RCPU, OFF_X(g)), h);
        t->g2h[g] = -1;
    }
    t->h2g[h] = -1;
    t->dirty &= ~(1u << h);
}
static int h_alloc(T *t) {
    int best = -1;
    a64_u32 ba = ~0u;
    for (int k = 0; k < NHREGS; k++) {
        int h = HREGS[k];
        if (t->lock & (1u << h)) continue;
        if (t->h2g[h] == -1) return h;
        if (t->age[h] < ba) { ba = t->age[h]; best = h; }
    }
    if (best < 0) { t->b.ovf = 1; return XBX; }   /* impossível: poucos travados por instrução */
    h_evict(t, best);
    return best;
}
static int rget(T *t, int g) {          /* lê X[g] (31 = SP) */
    int h = t->g2h[g];
    if (h < 0) {
        h = h_alloc(t);
        x_mov_rm(B, 1, h, XM(RCPU, OFF_X(g)));
        t->g2h[g] = (signed char)h;
        t->h2g[h] = (signed char)g;
    }
    t->lock |= 1u << h;
    t->age[h] = ++t->tick;
    return h;
}
static int rdef(T *t, int g) {          /* vai escrever X[g] (31 = SP) */
    int h = t->g2h[g];
    if (h < 0) {
        h = h_alloc(t);
        t->g2h[g] = (signed char)h;
        t->h2g[h] = (signed char)g;
        t->pnew |= 1u << h;
    }
    t->pend |= 1u << h;
    t->lock |= 1u << h;
    t->age[h] = ++t->tick;
    return h;
}
static int rtmp(T *t) {
    int h = h_alloc(t);
    t->h2g[h] = -2;
    t->tmp |= 1u << h;
    t->lock |= 1u << h;
    return h;
}
static int rx(T *t, int r) {            /* leitura com 31 = XZR */
    if (r == 31) { int h = rtmp(t); x_mov_ri(B, h, 0); return h; }
    return rget(t, r);
}
static int wx(T *t, int r) {            /* escrita com 31 = XZR (descartada) */
    if (r == 31) return rtmp(t);
    return rdef(t, r);
}
static void insn_end(T *t) {
    t->dirty |= t->pend;
    t->pend = t->pnew = 0;
    for (int h = 0; h < 16; h++) if (t->tmp & (1u << h)) t->h2g[h] = -1;
    t->tmp = 0;
    t->lock = 0;
}
static void rc_flush(T *t) {
    for (int k = 0; k < NHREGS; k++) {
        int h = HREGS[k], g = t->h2g[h];
        if (g >= 0 && (t->dirty & (1u << h))) x_mov_mr(B, 1, XM(RCPU, OFF_X(g)), h);
    }
    t->dirty = 0;
}
static void rc_drop(T *t) {
    for (int g = 0; g < 32; g++) t->g2h[g] = -1;
    for (int h = 0; h < 16; h++) t->h2g[h] = -1;
    t->dirty = t->lock = t->pend = t->pnew = t->tmp = 0;
}

/* ---------------------------------------------------------------------------------------------
 * Flags
 * ------------------------------------------------------------------------------------------- */
static void fl_spill(T *t) {            /* EFLAGS → reserved[14] */
    if (t->fe && !t->fm) {
        if (t->fe == 2) { x_cmc(B); t->fe = 1; }
        x_lahf(B);
        x_setcc(B, CC_O, XAX);
        x_mov16_mr(B, XM(RCPU, OFF_FLAGS), XAX);
        t->fm = 1;
    }
}
static void fl_clobber(T *t) {          /* a instrução vai destruir o EFLAGS sem gerar flags guest */
    if (t->fe) {
        if (!t->fm && t->live[t->i]) fl_spill(t);
        t->fe = 0;
    }
}
static void fl_clobber_force(T *t) {    /* idem, mas as flags de antes ainda são necessárias (falha) */
    if (t->fe) { fl_spill(t); t->fe = 0; }
}
static void fl_set(T *t, int mode) { t->fe = mode; t->fm = 0; }
static void fl_need(T *t) {             /* flags guest no EFLAGS (destrói rax) */
    if (!t->fe) {
        x_mov16_rm(B, XAX, XM(RCPU, OFF_FLAGS));
        xe8(B, 0x04); xe8(B, 0x7F);     /* add al, 0x7f → OF = V */
        x_sahf(B);
        t->fe = 1;
    }
}
/* condição ARM → cc do x86 (depois de fl_need). -1 = sempre. Pode trocar o modo com cmc. */
static int fl_cc(T *t, int cond) {
    if (cond >= 14) return -1;
    int cc;
    switch (cond >> 1) {
    case 0: cc = CC_E; break;
    case 1: cc = t->fe == 1 ? CC_B : CC_AE; break;
    case 2: cc = CC_S; break;
    case 3: cc = CC_O; break;
    case 4: if (t->fe == 1) { x_cmc(B); t->fe = 2; } cc = CC_A; break;
    case 5: cc = CC_GE; break;
    default: cc = CC_G; break;
    }
    return (cond & 1) ? cc ^ 1 : cc;
}

/* ---------------------------------------------------------------------------------------------
 * Pontos de falha, saídas
 * ------------------------------------------------------------------------------------------- */
static void fp_mark(T *t) {
    if (t->nfp >= (int)(sizeof t->fp / sizeof t->fp[0])) { t->b.ovf = 1; return; }
    jfp *f = &t->fp[t->nfp++];
    f->off = (a64_u32)(t->b.p - t->start);
    f->idx = (a64_u16)t->i;
    f->fl = (a64_u8)(t->fe | (t->fm ? 4 : 0));
    f->pad = 0;
    for (int h = 0; h < 16; h++) f->map[h] = 0xFF;
    for (int k = 0; k < NHREGS; k++) {
        int h = HREGS[k], g = t->h2g[h];
        if (g >= 0 && (t->dirty & (1u << h)) && !(t->pnew & (1u << h))) f->map[h] = (a64_u8)g;
    }
}

static void end_prep(T *t) { insn_end(t); fl_spill(t); rc_flush(t); }

static void emit_exit(T *t, a64_u64 target) {   /* regs na memória, flags em reserved[14] */
    a64_u8 *jne = NULL;
    if (target <= t->pc0) {
        x_alu_ri(B, 1, A_CMP, XM(RCPU, OFF_STOP), 0);
        jne = x_jcc32(B, CC_NE);
    }
    x_nop(B, (int)((3 - ((uintptr_t)t->b.p & 3)) & 3));
    a64_u8 *site = x_jmp32(B);          /* rel32 = 0 → cai no caminho lento logo abaixo */
    if (!t->b.ovf) x_patch32(jne, t->b.p);
    x_mov_ri(B, XAX, target);
    x_mov_mr(B, 1, XM(RCPU, OFF_PC), XAX);
    x_lea_rip(B, XDX, site);
    x_jmp_to(B, jit_g.g_link);
}

#ifdef _WIN32
#include <stdio.h>
static void fb_log(T *t, a64_u32 w) { if (jit_g.fblog) fprintf(stderr, "[jit] interp %016llx: %08x\n", (unsigned long long)t->pc, w); }
#else
static void fb_log(T *t, a64_u32 w) { (void)t; (void)w; }
#endif
static void emit_fallback(T *t, a64_u32 w) {
    t->mxclean = 0;                     /* código C pode deixar flags no MXCSR */
    insn_end(t);
    fl_spill(t);
    rc_flush(t);
    rc_drop(t);
    x_mov_ri(B, XAX, t->pc);
    x_mov_mr(B, 1, XM(RCPU, OFF_PC), XAX);
#ifdef _WIN32
    x_mov_rr(B, 1, XCX, RCPU);
    x_mov_ri(B, XDX, w);
#else
    x_mov_rr(B, 1, XDI, RCPU);
    x_mov_ri(B, XSI, w);
#endif
    x_call_mrip(B, &jit_g.helpers[JH_INTERP]);
    x_test_rr(B, 0, XAX, XAX);
    x_jcc_to(B, CC_NE, jit_g.g_hexit);
    t->fe = 0;
    t->fm = 1;
}

/* caminho lento: a instrução corrente inteira no interpretador (com o estado sincronizado só
 * neste caminho) e saída do bloco para pc+4. O rastreio de registradores/flags do caminho rápido
 * não muda. Precede o caminho rápido: slow_if(cc) salta por cima dele quando !cc. */
static void emit_slow_interp(T *t) {
    if (t->fe && !t->fm) {
        if (t->fe == 2) x_cmc(B);
        x_lahf(B);
        x_setcc(B, CC_O, XAX);
        x_mov16_mr(B, XM(RCPU, OFF_FLAGS), XAX);
    }
    for (int k = 0; k < NHREGS; k++) {
        int h = HREGS[k], g = t->h2g[h];
        if (g >= 0 && (t->dirty & (1u << h))) x_mov_mr(B, 1, XM(RCPU, OFF_X(g)), h);
    }
    x_mov_ri(B, XAX, t->pc);
    x_mov_mr(B, 1, XM(RCPU, OFF_PC), XAX);
#ifdef _WIN32
    x_mov_rr(B, 1, XCX, RCPU);
    x_mov_ri(B, XDX, t->insn[t->i]);
#else
    x_mov_rr(B, 1, XDI, RCPU);
    x_mov_ri(B, XSI, t->insn[t->i]);
#endif
    x_call_mrip(B, &jit_g.helpers[JH_INTERP]);
    x_test_rr(B, 0, XAX, XAX);
    x_jcc_to(B, CC_NE, jit_g.g_hexit);
    emit_exit(t, t->pc + 4);
}
static void slow_if(T *t, int cc) {
    a64_u8 *j = x_jcc32(B, cc ^ 1);
    emit_slow_interp(t);
    if (!t->b.ovf) x_patch32(j, t->b.p);
}

/* mesmas escolhas da frente INT (interp_int.h): SA0 = 1 (load/store com base SP desalinhado →
 * FAULT antes do acesso) e nAA = 0 (acquire/release que atravessa 16 bytes → FAULT), salvo
 * A64I_ORACLE_COMPAT ou definição explícita */
#ifdef A64I_ORACLE_COMPAT
#define JIT_DEF 0
#else
#define JIT_DEF 1
#endif
#ifdef A64I_SA0
#define JIT_SA0 A64I_SA0
#else
#define JIT_SA0 JIT_DEF
#endif
#ifdef A64I_NAA
#define JIT_NAA A64I_NAA
#else
#define JIT_NAA (!JIT_DEF)
#endif
static void sa0_check(T *t, int rn, int hb) {
    if (!JIT_SA0 || rn != 31) return;
    fl_clobber_force(t);
    x_test_ri(B, 0, XR(hb), 15);
    slow_if(t, CC_NE);
}

/* ---------------------------------------------------------------------------------------------
 * Utilidades de emissão
 * ------------------------------------------------------------------------------------------- */
/* d = a OP b (a, b registradores do host; b pode ser rax) */
static void binop(T *t, int w, int op, int d, int a, int b) {
    int comm = op != A_SUB && op != A_SBB;
    if (d == a) x_alu_rr(B, w, op, d, b);
    else if (d == b) {
        if (comm) x_alu_rr(B, w, op, d, a);
        else { x_mov_rr(B, w, XCX, a); x_alu_rr(B, w, op, XCX, b); x_mov_rr(B, w, d, XCX); }
    } else { x_mov_rr(B, w, d, a); x_alu_rr(B, w, op, d, b); }
}
/* operando com imediato de 64 bits (usa rcx se não couber em imm32 com sinal) */
static void alu_imm(T *t, int w, int op, int d, a64_u64 imm) {
    if (!w || xfits32((int64_t)imm)) x_alu_ri(B, w, op, XR(d), (int32_t)(a64_u32)imm);
    else { x_mov_ri(B, XCX, imm); x_alu_rr(B, w, op, d, XCX); }
}
/* rax = shift(src, type, amount) (LSL LSR ASR ROR); destrói EFLAGS */
static int shifted(T *t, int w, int src, int type, int amt) {
    if (!amt) return src;
    static const int ops[4] = { S_SHL, S_SHR, S_SAR, S_ROR };
    x_mov_rr(B, w, XAX, src);
    x_shift_ri(B, w, ops[type], XAX, amt);
    return XAX;
}

/* DecodeBitMasks (só wmask) */
static int bitmask(int sf, unsigned N, unsigned imms, unsigned immr, a64_u64 *out) {
    unsigned v = (N << 6) | (~imms & 0x3F);
    if (!v) return 0;
    int len = 31 - __builtin_clz(v);
    if (len < 1 || (!sf && N)) return 0;
    unsigned size = 1u << len, lev = size - 1, S = imms & lev, R = immr & lev;
    if (S == lev) return 0;
    a64_u64 e = (S + 1 == 64) ? ~0ull : ((1ull << (S + 1)) - 1);
    if (R) e = ((e >> R) | (e << (size - R))) & (size == 64 ? ~0ull : ((1ull << size) - 1));
    for (unsigned s = size; s < 64; s *= 2) e |= e << s;
    *out = sf ? e : (e & 0xFFFFFFFFull);
    return 1;
}

/* ---------------------------------------------------------------------------------------------
 * Dados com imediato
 * ------------------------------------------------------------------------------------------- */
static int tr_dpimm(T *t, a64_u32 w) {
    int sf = (int)BIT(w, 31), rd = (int)(w & 31), rn = (int)BITS(w, 9, 5);
    switch (BITS(w, 25, 23)) {
    case 0: case 1: {                   /* ADR / ADRP */
        int64_t imm = sext((BITS(w, 23, 5) << 2) | BITS(w, 30, 29), 21);
        a64_u64 v = BIT(w, 31) ? (t->pc & ~0xFFFull) + ((a64_u64)imm << 12) : t->pc + (a64_u64)imm;
        if (rd != 31) x_mov_ri(B, rdef(t, rd), v);
        return 1;
    }
    case 2: {                           /* ADD/SUB (imediato) */
        int S = (int)BIT(w, 29), sub = (int)BIT(w, 30);
        a64_u32 imm = BITS(w, 21, 10) << (BIT(w, 22) ? 12 : 0);
        int hn = rget(t, rn);
        if (!S) {
            int hd = rdef(t, rd);
            if (imm == 0) { if (hd != hn || !sf) x_mov_rr(B, sf, hd, hn); }
            else x_lea(B, sf, hd, XM(hn, sub ? -(int32_t)imm : (int32_t)imm));
            return 1;
        }
        if (rd == 31) {
            if (sub) x_alu_ri(B, sf, A_CMP, XR(hn), (int32_t)imm);
            else { x_mov_rr(B, sf, XAX, hn); x_alu_ri(B, sf, A_ADD, XR(XAX), (int32_t)imm); }
        } else {
            int hd = rdef(t, rd);
            if (hd != hn || !sf) x_mov_rr(B, sf, hd, hn);
            x_alu_ri(B, sf, sub ? A_SUB : A_ADD, XR(hd), (int32_t)imm);
        }
        fl_set(t, sub ? 2 : 1);
        return 1;
    }
    case 4: {                           /* lógicas (imediato) */
        int opc = (int)BITS(w, 30, 29);
        a64_u64 m;
        if (!bitmask(sf, BIT(w, 22), BITS(w, 15, 10), BITS(w, 21, 16), &m)) return 0;
        static const int ops[4] = { A_AND, A_OR, A_XOR, A_AND };
        if (opc == 1 && rn == 31) { x_mov_ri(B, rdef(t, rd), m); return 1; }   /* MOV (bitmask) */
        if (opc != 3) fl_clobber(t);
        int hn = rx(t, rn);
        if (opc == 3 && rd == 31) {     /* TST */
            if (!sf || xfits32((int64_t)m)) x_test_ri(B, sf, XR(hn), (a64_u32)m);
            else { x_mov_ri(B, XCX, m); x_test_rr(B, 1, hn, XCX); }
            fl_set(t, 1);
            return 1;
        }
        int hd = opc == 3 ? wx(t, rd) : rdef(t, rd);
        if (hd != hn || !sf) x_mov_rr(B, sf, hd, hn);
        alu_imm(t, sf, ops[opc], hd, m);
        if (opc == 3) fl_set(t, 1);
        return 1;
    }
    case 5: {                           /* MOVN/MOVZ/MOVK */
        int opc = (int)BITS(w, 30, 29), hw = (int)BITS(w, 22, 21);
        if (opc == 1 || (!sf && hw >= 2)) return 0;
        a64_u64 imm = (a64_u64)BITS(w, 20, 5) << (16 * hw);
        if (opc != 3) {
            a64_u64 v = opc == 0 ? ~imm : imm;
            if (!sf) v &= 0xFFFFFFFFull;
            if (rd != 31) x_mov_ri(B, rdef(t, rd), v);
            return 1;
        }
        if (rd == 31) return 1;
        int h = rget(t, rd);
        rdef(t, rd);
        if (hw == 0) {
            x_mov16_mi(B, XR(h), (a64_u16)imm);
            if (!sf) x_mov_rr(B, 0, h, h);
        } else {
            fl_clobber(t);
            alu_imm(t, sf, A_AND, h, ~(0xFFFFull << (16 * hw)));
            alu_imm(t, sf, A_OR, h, imm);
        }
        return 1;
    }
    case 6: {                           /* SBFM/BFM/UBFM */
        int opc = (int)BITS(w, 30, 29), N = (int)BIT(w, 22);
        int immr = (int)BITS(w, 21, 16), imms = (int)BITS(w, 15, 10), ds = sf ? 64 : 32;
        if (opc == 3 || N != sf || (!sf && (immr >= 32 || imms >= 32))) return 0;
        if (opc != 1 && immr == 0 && (imms == 7 || imms == 15 || (imms == 31 && sf))) {
            int hn = rx(t, rn), hd = wx(t, rd);         /* UXT? e SXT? sem flags */
            if (opc == 2) {
                if (imms == 7) x_movzx8(B, hd, XR(hn));
                else if (imms == 15) x_movzx16(B, hd, XR(hn));
                else x_mov_rr(B, 0, hd, hn);
            } else {
                if (imms == 7) x_movsx8(B, sf, hd, XR(hn));
                else if (imms == 15) x_movsx16(B, sf, hd, XR(hn));
                else x_movsxd(B, hd, XR(hn));
            }
            return 1;
        }
        fl_clobber(t);
        int hn = rx(t, rn);
        int r = opc == 0 ? S_SAR : S_SHR;
        if (opc != 1) {
            x_mov_rr(B, sf, XAX, hn);
            int a, c;
            if (imms >= immr) { int wd = imms - immr + 1; a = ds - (immr + wd); c = ds - wd; }
            else { int wd = imms + 1, lsb = ds - immr; a = ds - wd; c = ds - wd - lsb; }
            if (a) x_shift_ri(B, sf, S_SHL, XAX, a);
            if (c) x_shift_ri(B, sf, r, XAX, c);
            x_mov_rr(B, sf, wx(t, rd), XAX);
            return 1;
        }
        /* BFM: BFXIL / BFI */
        int wd, slsb, pos;
        if (imms >= immr) { wd = imms - immr + 1; slsb = immr; pos = 0; }
        else { wd = imms + 1; slsb = 0; pos = ds - immr; }
        if (rd == 31) return 1;         /* destino XZR: sem efeito */
        int hd = rget(t, rd);
        rdef(t, rd);
        if (wd == ds) { x_mov_rr(B, sf, hd, hn); return 1; }
        x_mov_rr(B, sf, XAX, hn);
        if (ds - (slsb + wd)) x_shift_ri(B, sf, S_SHL, XAX, ds - (slsb + wd));
        x_shift_ri(B, sf, S_SHR, XAX, ds - wd);
        if (pos) x_shift_ri(B, sf, S_SHL, XAX, pos);
        a64_u64 mask = (((wd == 64) ? ~0ull : ((1ull << wd) - 1)) << pos);
        if (!sf) mask &= 0xFFFFFFFFull;
        alu_imm(t, sf, A_AND, hd, ~mask);
        x_alu_rr(B, sf, A_OR, hd, XAX);
        return 1;
    }
    case 7: {                           /* EXTR */
        int rm = (int)BITS(w, 20, 16), imms = (int)BITS(w, 15, 10);
        if (BITS(w, 30, 29) || BIT(w, 21) || (int)BIT(w, 22) != sf || (!sf && imms >= 32)) return 0;
        if (imms == 0) { int hm = rx(t, rm); x_mov_rr(B, sf, wx(t, rd), hm); return 1; }
        fl_clobber(t);
        int hn = rx(t, rn), hm = rx(t, rm);
        x_mov_rr(B, sf, XAX, hm);
        if (rn == rm) x_shift_ri(B, sf, S_ROR, XAX, imms);
        else x_shrd_ri(B, sf, XAX, hn, imms);
        x_mov_rr(B, sf, wx(t, rd), XAX);
        return 1;
    }
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * Desvios e sistema
 * ------------------------------------------------------------------------------------------- */
/* desvio indireto com alvo em rax, no fim do bloco (registradores gravados, flags em memória):
 * busca no cache de saltos da thread copiada em cada ponto (previsão por ponto no x86) */
static void emit_ind(T *t) {
    if (jit_g.noras) { x_jmp_to(B, jit_g.g_ind); return; }
    x_alu_ri(B, 1, A_CMP, XM(RCPU, OFF_STOP), 0);
    a64_u8 *j1 = x_jcc32(B, CC_NE);
    x_mov_rm(B, 1, XCX, XM(RCPU, OFF_JC));
    x_mov_rr(B, 0, XDX, XAX);
    x_shift_ri(B, 0, S_SHR, XDX, 2);
    x_alu_ri(B, 0, A_AND, XR(XDX), (1 << JIT_JC_BITS) - 1);
    x_shift_ri(B, 0, S_SHL, XDX, 4);
    x_alu_rm(B, 1, A_CMP, XAX, XMI(XCX, XDX, 0, 0));
    a64_u8 *j2 = x_jcc32(B, CC_NE);
    x_jmp_m(B, XMI(XCX, XDX, 0, 8));
    if (!t->b.ovf) { x_patch32(j1, t->b.p); x_patch32(j2, t->b.p); }
    x_jmp_to(B, jit_g.g_ind);
}
/* BL/BLR: empilha (pc+4, continuação). Devolve o campo do lea a remendar com o endereço do stub
 * de continuação (uma saída encadeável para pc+4 emitida depois). Destrói rax rcx rdx. */
static a64_u8 *ras_push(T *t) {
    x_mov_rm(B, 1, XCX, XM(RCPU, OFF_RAS));
    x_mov_rm(B, 0, XDX, XM(XCX, 0));
    x_alu_ri(B, 0, A_ADD, XR(XDX), 1);
    x_alu_ri(B, 0, A_AND, XR(XDX), JIT_RAS_N - 1);
    x_mov_mr(B, 0, XM(XCX, 0), XDX);
    x_shift_ri(B, 0, S_SHL, XDX, 4);
    x_mov_ri(B, XAX, t->pc + 4);
    x_mov_mr(B, 1, XMI(XCX, XDX, 0, 8), XAX);
    x_lea_rip(B, XAX, t->b.p);
    a64_u8 *disp = t->b.p - 4;
    x_mov_mr(B, 1, XMI(XCX, XDX, 0, 16), XAX);
    return disp;
}
/* RET: alvo em rax; topo da pilha confere → salta para a continuação; senão busca indireta */
static void emit_ret(T *t) {
    if (jit_g.noras) { emit_ind(t); return; }
    x_mov_rm(B, 1, XCX, XM(RCPU, OFF_RAS));
    x_mov_rm(B, 0, XDX, XM(XCX, 0));
    x_mov_rr(B, 0, X8, XDX);
    x_shift_ri(B, 0, S_SHL, XDX, 4);
    x_alu_rm(B, 1, A_CMP, XAX, XMI(XCX, XDX, 0, 8));
    a64_u8 *j1 = x_jcc32(B, CC_NE);
    x_alu_ri(B, 0, A_SUB, XR(X8), 1);
    x_alu_ri(B, 0, A_AND, XR(X8), JIT_RAS_N - 1);
    x_mov_mr(B, 0, XM(XCX, 0), X8);
    x_alu_ri(B, 1, A_CMP, XM(RCPU, OFF_STOP), 0);
    a64_u8 *j2 = x_jcc32(B, CC_NE);
    x_jmp_m(B, XMI(XCX, XDX, 0, 16));
    if (!t->b.ovf) { x_patch32(j1, t->b.p); x_patch32(j2, t->b.p); }
    emit_ind(t);
}

static void cond_exits(T *t, int cc, a64_u64 taken) {
    a64_u8 *j = x_jcc32(B, cc);
    emit_exit(t, t->pc + 4);
    if (!t->b.ovf) x_patch32(j, t->b.p);
    emit_exit(t, taken);
    t->ended = 1;
}

static int tr_branch(T *t, a64_u32 w) {
    if ((w & 0x7C000000) == 0x14000000) {                       /* B / BL */
        a64_u64 tg = t->pc + (a64_u64)(sext(w & 0x3FFFFFF, 26) << 2);
        if (BIT(w, 31)) x_mov_ri(B, rdef(t, 30), t->pc + 4);
        end_prep(t);
        a64_u8 *cont = BIT(w, 31) ? ras_push(t) : NULL;
        emit_exit(t, tg);
        if (cont) { if (!t->b.ovf) x_patch32(cont, t->b.p); emit_exit(t, t->pc + 4); }
        t->ended = 1;
        return 1;
    }
    if ((w & 0xFF000010) == 0x54000000) {                       /* B.cond */
        a64_u64 tg = t->pc + (a64_u64)(sext(BITS(w, 23, 5), 19) << 2);
        int cond = (int)(w & 15);
        end_prep(t);
        if (cond >= 14) { emit_exit(t, tg); t->ended = 1; return 1; }
        fl_need(t);
        cond_exits(t, fl_cc(t, cond), tg);
        return 1;
    }
    if ((w & 0x7E000000) == 0x34000000) {                       /* CBZ / CBNZ */
        int sf = (int)BIT(w, 31);
        a64_u64 tg = t->pc + (a64_u64)(sext(BITS(w, 23, 5), 19) << 2);
        int h = rx(t, (int)(w & 31));
        end_prep(t);
        x_test_rr(B, sf, h, h);
        cond_exits(t, BIT(w, 24) ? CC_NE : CC_E, tg);
        return 1;
    }
    if ((w & 0x7E000000) == 0x36000000) {                       /* TBZ / TBNZ */
        int bit = (int)((BIT(w, 31) << 5) | BITS(w, 23, 19));
        a64_u64 tg = t->pc + (a64_u64)(sext(BITS(w, 18, 5), 14) << 2);
        int h = rx(t, (int)(w & 31));
        end_prep(t);
        int nz = (int)BIT(w, 24);
        if (bit < 31) { x_test_ri(B, 0, XR(h), 1u << bit); cond_exits(t, nz ? CC_NE : CC_E, tg); }
        else { x_bt_ri(B, 1, h, bit); cond_exits(t, nz ? CC_B : CC_AE, tg); }
        return 1;
    }
    if ((w & 0xFE000000) == 0xD6000000) {                       /* desvio por registrador */
        unsigned opc = BITS(w, 24, 21);
        if (BITS(w, 20, 16) != 31 || BITS(w, 15, 10) != 0 || (w & 31) != 0 || opc > 2) return 0;
        int h = rx(t, (int)BITS(w, 9, 5));
        insn_end(t);
        fl_spill(t);
        x_mov_rr(B, 1, XAX, h);
        /* alvo desalinhado: o interpretador completa o desvio e dá FAULT (PC alignment) */
        x_test_ri(B, 0, XR(XAX), 3);
        t->fe = 0;                      /* flags já em reserved[14] (fl_spill acima) */
        slow_if(t, CC_NE);
        if (opc == 1) x_mov_ri(B, rdef(t, 30), t->pc + 4);
        insn_end(t);
        rc_flush(t);
        if (opc == 2) { emit_ret(t); t->ended = 1; return 1; }
        a64_u8 *cont = NULL;
        if (opc == 1) { x_mov_rr(B, 1, X9, XAX); cont = ras_push(t); x_mov_rr(B, 1, XAX, X9); }
        emit_ind(t);
        if (cont) { if (!t->b.ovf) x_patch32(cont, t->b.p); emit_exit(t, t->pc + 4); }
        t->ended = 1;
        return 1;
    }
    if ((w & 0xFFFFF01F) == 0xD503201F) {                       /* HINT */
        unsigned imm = BITS(w, 11, 5);
        if (imm == 0 || imm == 0x20 || imm == 0x22 || imm == 0x24 || imm == 0x26) return 1;  /* NOP, BTI */
        return 0;
    }
    if ((w & 0xFFFFF0FF) == 0xD503305F) {                       /* CLREX */
        x_mov_mi(B, 0, XM(RCPU, (int32_t)offsetof(a64_cpu, excl_valid)), 0);
        return 1;
    }
    if ((w & 0xFFFFF09F) == 0xD503309F) {                       /* DSB/DMB/ISB */
        unsigned op2 = BITS(w, 7, 5), crm = BITS(w, 11, 8);
        if (op2 == 6) { t->ended = 2; return 1; }               /* ISB: fecha o bloco */
        if ((op2 == 4 || op2 == 5) && (crm & 3)) {
            if ((crm & 3) == 3) x_mfence(B);                    /* só st→ld precisa de barreira no TSO */
            return 1;
        }
        return 0;
    }
    if ((w & 0xFFF00000) == 0xD5300000 || (w & 0xFFF00000) == 0xD5100000) {   /* MRS / MSR (registrador) */
        unsigned sr = BITS(w, 19, 5), rt = w & 31;
        int off;
        if (sr == 0x5A20 || sr == 0x5A21) {                                 /* FPCR / FPSR (32 bits) */
            int fo = sr == 0x5A20 ? (int)offsetof(a64_cpu, fpcr) : (int)offsetof(a64_cpu, fpsr);
            if (BIT(w, 21)) { if (rt != 31) x_mov_rm(B, 0, rdef(t, (int)rt), XM(RCPU, fo)); return 1; }
            fl_clobber(t);
            int h = rx(t, (int)rt);
            x_mov_rr(B, 0, XAX, h);
            x_alu_ri(B, 0, A_AND, XR(XAX), (int32_t)(sr == 0x5A20 ? A64_FPCR_MASK : A64_FPSR_MASK));
            x_mov_mr(B, 0, XM(RCPU, fo), XAX);
            return 1;
        }
        if (sr == 0x5E82) off = (int)offsetof(a64_cpu, tpidr_el0);          /* S3_3_C13_C0_2 */
        else if (sr == 0x5E83) off = (int)offsetof(a64_cpu, tpidrro_el0);   /* S3_3_C13_C0_3 */
        else return 0;
        if (BIT(w, 21)) {               /* MRS */
            if (rt != 31) x_mov_rm(B, 1, rdef(t, (int)rt), XM(RCPU, off));
        } else {
            if (off != (int)offsetof(a64_cpu, tpidr_el0)) return 0;   /* TPIDRRO_EL0 é só leitura em EL0 */
            x_mov_mr(B, 1, XM(RCPU, off), rx(t, (int)rt));
        }
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * Dados com registrador
 * ------------------------------------------------------------------------------------------- */
static int tr_dpreg(T *t, a64_u32 w) {
    int sf = (int)BIT(w, 31), rd = (int)(w & 31), rn = (int)BITS(w, 9, 5), rm = (int)BITS(w, 20, 16);
    unsigned op2 = BITS(w, 24, 21);
    if (!BIT(w, 28)) {
        if (!(op2 & 8)) {               /* lógicas (registrador deslocado) */
            int opc = (int)BITS(w, 30, 29), sh = (int)BITS(w, 23, 22), N = (int)BIT(w, 21), amt = (int)BITS(w, 15, 10);
            if (!sf && amt >= 32) return 0;
            static const int ops[4] = { A_AND, A_OR, A_XOR, A_AND };
            if (opc == 1 && rn == 31 && !amt) {                 /* MOV / MVN */
                int hm = rx(t, rm), hd = wx(t, rd);
                x_mov_rr(B, sf, hd, hm);
                if (N) x_f7(B, sf, 2, XR(hd));
                return 1;
            }
            if (opc != 3) fl_clobber(t);
            int hn = rx(t, rn), hm = rx(t, rm);
            int o = shifted(t, sf, hm, sh, amt);
            if (N) { if (o != XAX) { x_mov_rr(B, sf, XAX, o); o = XAX; } x_f7(B, sf, 2, XR(XAX)); }
            if (opc == 3 && rd == 31) { x_test_rr(B, sf, hn, o); fl_set(t, 1); return 1; }
            int hd = wx(t, rd);
            binop(t, sf, ops[opc], hd, hn, o);
            if (opc == 3) fl_set(t, 1);
            return 1;
        }
        int sub = (int)BIT(w, 30), S = (int)BIT(w, 29);
        int o, hn;
        if (!(op2 & 1)) {               /* ADD/SUB (registrador deslocado) */
            int sh = (int)BITS(w, 23, 22), amt = (int)BITS(w, 15, 10);
            if (sh == 3 || (!sf && amt >= 32)) return 0;
            hn = rx(t, rn);
            int hm = rx(t, rm);
            if (!S && !sub && (amt == 0 || (sh == 0 && amt <= 3))) {
                x_lea(B, sf, wx(t, rd), XMI(hn, hm, amt, 0));
                return 1;
            }
            if (!S) fl_clobber(t);
            o = shifted(t, sf, hm, sh, amt);
        } else {                        /* ADD/SUB (registrador estendido) */
            int opt = (int)BITS(w, 15, 13), amt = (int)BITS(w, 12, 10);
            if (BITS(w, 23, 22) || amt > 4) return 0;
            if (!S) fl_clobber(t);      /* antes de usar rax: salvar as flags destrói rax */
            hn = rget(t, rn);           /* Rn = SP */
            int hm = rx(t, rm);
            switch (opt) {
            case 0: x_movzx8(B, XAX, XR(hm)); break;
            case 1: x_movzx16(B, XAX, XR(hm)); break;
            case 2: x_mov_rr(B, 0, XAX, hm); break;
            case 4: x_movsx8(B, 1, XAX, XR(hm)); break;
            case 5: x_movsx16(B, 1, XAX, XR(hm)); break;
            case 6: x_movsxd(B, XAX, XR(hm)); break;
            default: x_mov_rr(B, 1, XAX, hm); break;
            }
            if (amt == 4) { x_lea(B, 1, XAX, XMI(-1, XAX, 3, 0)); x_lea(B, 1, XAX, XMI(XAX, XAX, 0, 0)); }
            else if (amt) x_lea(B, 1, XAX, XMI(-1, XAX, amt, 0));
            o = XAX;
            if (!S && !sub) { x_lea(B, sf, rdef(t, rd), XMI(hn, XAX, 0, 0)); return 1; }
            if (!S) { binop(t, sf, A_SUB, rdef(t, rd), hn, o); return 1; }   /* Rd = SP */
        }
        if (S && rd == 31) {
            if (sub) x_alu_rr(B, sf, A_CMP, hn, o);
            else if (o == XAX) x_alu_rr(B, sf, A_ADD, XAX, hn);
            else { x_mov_rr(B, sf, XAX, hn); x_alu_rr(B, sf, A_ADD, XAX, o); }
        } else binop(t, sf, sub ? A_SUB : A_ADD, wx(t, rd), hn, o);
        if (S) fl_set(t, sub ? 2 : 1);
        return 1;
    }
    switch (op2) {
    case 0: {                           /* ADC/SBC */
        if (BITS(w, 15, 10)) return 0;
        int sub = (int)BIT(w, 30), S = (int)BIT(w, 29);
        int hn = rx(t, rn), hm = rx(t, rm);
        fl_need(t);
        if (!S && t->live[t->i]) fl_spill(t);
        if (sub && t->fe == 1) { x_cmc(B); t->fe = 2; }
        if (!sub && t->fe == 2) { x_cmc(B); t->fe = 1; }
        binop(t, sf, sub ? A_SBB : A_ADC, wx(t, rd), hn, hm);
        if (S) fl_set(t, sub ? 2 : 1); else t->fe = 0;
        return 1;
    }
    case 2: {                           /* CCMP/CCMN */
        if (!BIT(w, 29) || BIT(w, 10) || BIT(w, 4)) return 0;
        int sub = (int)BIT(w, 30), isimm = (int)BIT(w, 11), cond = (int)BITS(w, 15, 12);
        int hn = rx(t, rn), hm = isimm ? -1 : rx(t, rm);
        fl_need(t);
        int cc = fl_cc(t, cond);
        a64_u8 *jt = NULL, *jd = NULL;
        if (cc >= 0) {
            jt = x_jcc32(B, cc);
            x_mov16_mi(B, XM(RCPU, OFF_FLAGS), jit_nzcv_to_x86((w & 15) << 28));
            jd = x_jmp32(B);
            if (!t->b.ovf) x_patch32(jt, t->b.p);
        }
        if (sub) {
            if (isimm) x_alu_ri(B, sf, A_CMP, XR(hn), (int32_t)rm); else x_alu_rr(B, sf, A_CMP, hn, hm);
            x_cmc(B);
        } else {
            x_mov_rr(B, sf, XAX, hn);
            if (isimm) x_alu_ri(B, sf, A_ADD, XR(XAX), (int32_t)rm); else x_alu_rr(B, sf, A_ADD, XAX, hm);
        }
        x_lahf(B);
        x_setcc(B, CC_O, XAX);
        x_mov16_mr(B, XM(RCPU, OFF_FLAGS), XAX);
        if (jd && !t->b.ovf) x_patch32(jd, t->b.p);
        t->fe = 0;
        t->fm = 1;
        return 1;
    }
    case 4: {                           /* CSEL/CSINC/CSINV/CSNEG */
        int op = (int)BIT(w, 30), o2 = (int)BITS(w, 11, 10);
        if (BIT(w, 29) || o2 > 1) return 0;
        int hn = rx(t, rn), hm = rx(t, rm), hd = wx(t, rd);
        fl_need(t);
        int cc = fl_cc(t, (int)BITS(w, 15, 12));
        if (cc < 0) { x_mov_rr(B, sf, hd, hn); return 1; }
        if (!op && !o2) x_mov_rr(B, sf, XAX, hm);
        else if (!op) x_lea(B, sf, XAX, XM(hm, 1));
        else {
            x_mov_rr(B, sf, XAX, hm);
            x_f7(B, sf, 2, XR(XAX));
            if (o2) x_lea(B, sf, XAX, XM(XAX, 1));
        }
        x_cmov(B, sf, cc, XAX, XR(hn));
        x_mov_rr(B, sf, hd, XAX);
        return 1;
    }
    case 6:
        if (BIT(w, 29)) return 0;
        if (!BIT(w, 30)) {              /* 2 fontes */
            unsigned opc = BITS(w, 15, 10);
            if (opc == 2 || opc == 3) {                         /* UDIV / SDIV */
                fl_clobber(t);
                int hn = rx(t, rn), hm = rx(t, rm), hd = wx(t, rd);
                x_mov_rr(B, sf, XAX, hn);
                x_test_rr(B, sf, hm, hm);
                a64_u8 *jz = x_jcc32(B, CC_E), *jn = NULL, *je1 = NULL;
                if (opc == 3) {
                    x_alu_ri(B, sf, A_CMP, XR(hm), -1);
                    a64_u8 *jd = x_jcc8(B, CC_NE);
                    x_f7(B, sf, 3, XR(XAX));
                    je1 = x_jmp32(B);
                    x_patch8(jd, t->b.p);
                    if (sf) x_cqo(B); else x_cdq(B);
                    x_f7(B, sf, 7, XR(hm));
                } else {
                    x_alu_rr(B, 0, A_XOR, XDX, XDX);
                    x_f7(B, sf, 6, XR(hm));
                }
                jn = x_jmp32(B);
                if (!t->b.ovf) x_patch32(jz, t->b.p);
                x_alu_rr(B, 0, A_XOR, XAX, XAX);
                if (!t->b.ovf) { x_patch32(jn, t->b.p); x_patch32(je1, t->b.p); }
                x_mov_rr(B, sf, hd, XAX);
                return 1;
            }
            if (opc >= 8 && opc <= 11) {                        /* LSLV LSRV ASRV RORV */
                static const int ops[4] = { S_SHL, S_SHR, S_SAR, S_ROR };
                fl_clobber(t);
                int hn = rx(t, rn), hm = rx(t, rm);
                x_mov_rr(B, 0, XCX, hm);
                int hd = wx(t, rd);
                x_mov_rr(B, sf, hd, hn);
                x_shift_cl(B, sf, ops[opc - 8], hd);
                return 1;
            }
            return 0;
        } else {                        /* 1 fonte */
            if (BITS(w, 20, 16)) return 0;
            unsigned opc = BITS(w, 15, 10);
            if (opc == 2 && !sf) opc = 3;                       /* REV de 32 bits */
            else if (opc == 3 && !sf) return 0;
            switch (opc) {
            case 1:                                             /* REV16 */
                fl_clobber(t);
                if (!sf) {
                    int hn = rx(t, rn), hd = wx(t, rd);
                    x_mov_rr(B, 0, hd, hn); x_bswap(B, 0, hd); x_shift_ri(B, 0, S_ROR, hd, 16);
                } else {
                    int hn = rx(t, rn), hd = wx(t, rd);
                    x_mov_rr(B, 1, XAX, hn);
                    x_mov_rr(B, 1, XCX, hn);
                    x_shift_ri(B, 1, S_SHR, XAX, 8);
                    x_mov_ri(B, XDX, 0x00FF00FF00FF00FFull);
                    x_alu_rr(B, 1, A_AND, XAX, XDX);
                    x_alu_rr(B, 1, A_AND, XCX, XDX);
                    x_shift_ri(B, 1, S_SHL, XCX, 8);
                    x_alu_rr(B, 1, A_OR, XAX, XCX);
                    x_mov_rr(B, 1, hd, XAX);
                }
                return 1;
            case 2: {                                           /* REV32 (64 bits) */
                fl_clobber(t);
                int hn = rx(t, rn), hd = wx(t, rd);
                x_mov_rr(B, 1, hd, hn); x_bswap(B, 1, hd); x_shift_ri(B, 1, S_ROR, hd, 32);
                return 1;
            }
            case 3: {                                           /* REV */
                int hn = rx(t, rn), hd = wx(t, rd);
                x_mov_rr(B, sf, hd, hn); x_bswap(B, sf, hd);
                return 1;
            }
            case 4: {                                           /* CLZ */
                fl_clobber(t);
                int hn = rx(t, rn);
                x_lzcnt(B, sf, wx(t, rd), XR(hn));
                return 1;
            }
            case 5: {                                           /* CLS */
                fl_clobber(t);
                int hn = rx(t, rn);
                x_mov_rr(B, sf, XAX, hn);
                x_mov_rr(B, sf, XCX, hn);
                x_shift_ri(B, sf, S_SAR, XCX, 1);
                x_alu_rr(B, sf, A_XOR, XAX, XCX);
                x_lzcnt(B, sf, XAX, XR(XAX));
                x_lea(B, sf, wx(t, rd), XM(XAX, -1));
                return 1;
            }
            }
            return 0;
        }
    default:
        if (!(op2 & 8)) return 0;
        {                               /* 3 fontes */
            unsigned op31 = BITS(w, 23, 21);
            int o0 = (int)BIT(w, 15), ra = (int)BITS(w, 14, 10);
            if (BITS(w, 30, 29) || (!sf && op31)) return 0;
            if (op31 == 0 || op31 == 1 || op31 == 5) {
                fl_clobber(t);
                int hn = rx(t, rn), hm = rx(t, rm), ha = rx(t, ra);
                if (op31 == 0) { x_mov_rr(B, sf, XAX, hn); x_imul_rr(B, sf, XAX, XR(hm)); }
                else if (op31 == 1) { x_movsxd(B, XAX, XR(hn)); x_movsxd(B, XCX, XR(hm)); x_imul_rr(B, 1, XAX, XR(XCX)); }
                else { x_mov_rr(B, 0, XAX, hn); x_mov_rr(B, 0, XCX, hm); x_imul_rr(B, 1, XAX, XR(XCX)); }
                int hd = wx(t, rd);
                if (o0) { x_mov_rr(B, sf, XCX, ha); x_alu_rr(B, sf, A_SUB, XCX, XAX); x_mov_rr(B, sf, hd, XCX); }
                else { x_alu_rr(B, sf, A_ADD, XAX, ha); x_mov_rr(B, sf, hd, XAX); }
                return 1;
            }
            if ((op31 == 2 || op31 == 6) && !o0 && ra == 31) {  /* SMULH / UMULH */
                fl_clobber(t);
                int hn = rx(t, rn), hm = rx(t, rm);
                x_mov_rr(B, 1, XAX, hn);
                x_f7(B, 1, op31 == 2 ? 5 : 4, XR(hm));
                x_mov_rr(B, 1, wx(t, rd), XDX);
                return 1;
            }
            return 0;
        }
    }
}

/* ---------------------------------------------------------------------------------------------
 * Loads / stores
 * ------------------------------------------------------------------------------------------- */
/* acesso de um registrador inteiro: size 0..3, kind 0 store, 1 load zx, 2 load sx64, 3 load sx32 */
static void ld_int(T *t, int hd, xo m, int size, int kind) {
    fp_mark(t);
    if (kind == 1) {
        if (size == 0) x_movzx8(B, hd, m);
        else if (size == 1) x_movzx16(B, hd, m);
        else x_mov_rm(B, size == 3, hd, m);
    } else {
        int w = kind == 2;
        if (size == 0) x_movsx8(B, w, hd, m);
        else if (size == 1) x_movsx16(B, w, hd, m);
        else x_movsxd(B, hd, m);
    }
}
static void st_int(T *t, int rt, xo m, int size) {
    if (rt == 31) {
        fp_mark(t);
        if (size == 0) x_mov8_mi(B, m, 0);
        else if (size == 1) x_mov16_mi(B, m, 0);
        else x_mov_mi(B, size == 3, m, 0);
        return;
    }
    int hs = rget(t, rt);
    fp_mark(t);
    if (size == 0) x_mov8_mr(B, m, hs);
    else if (size == 1) x_mov16_mr(B, m, hs);
    else x_mov_mr(B, size == 3, m, hs);
}
/* SIMD&FP: tamanho em bytes 1,2,4,8,16 ; xmm 'x' como temporário */
static void ld_vec_tmp(T *t, int x, xo m, int bytes) {
    fp_mark(t);
    switch (bytes) {
    case 1: x_movzx8(B, XAX, m); x_movd_ld(B, x, XR(XAX)); break;
    case 2: x_movzx16(B, XAX, m); x_movd_ld(B, x, XR(XAX)); break;
    case 4: x_movd_ld(B, x, m); break;
    case 8: x_movq_ld(B, x, m); break;
    default: x_movdqu_ld(B, x, m); break;
    }
}
static void st_vec(T *t, int vt, xo m, int bytes) {
    xo v = XM(RCPU, OFF_V(vt));
    switch (bytes) {
    case 1: x_movzx8(B, XDX, v); fp_mark(t); x_mov8_mr(B, m, XDX); break;
    case 2: x_movzx16(B, XDX, v); fp_mark(t); x_mov16_mr(B, m, XDX); break;
    case 4: x_mov_rm(B, 0, XDX, v); fp_mark(t); x_mov_mr(B, 0, m, XDX); break;
    case 8: x_mov_rm(B, 1, XDX, v); fp_mark(t); x_mov_mr(B, 1, m, XDX); break;
    default: x_movdqa_ld(B, 0, v); fp_mark(t); x_movdqu_st(B, m, 0); break;
    }
}
static void wb_base(T *t, int rn, int64_t off) {
    int h = rget(t, rn);
    rdef(t, rn);
    x_lea(B, 1, h, XM(h, (int32_t)off));
}

#include "jit_atom.h"
static int s_ldst_multi(T *t, a64_u32 w);
static int tr_ldst(T *t, a64_u32 w) {
    if ((w & 0xBF800000) == 0x0C000000 && !BIT(w, 21)) return s_ldst_multi(t, w);
    if ((w & 0xBFA00000) == 0x0C800000) return s_ldst_multi(t, w);
    if ((w & 0x3B000000) == 0x18000000) {                       /* load literal */
        int opc = (int)BITS(w, 31, 30), V = (int)BIT(w, 26), rt = (int)(w & 31);
        a64_u64 a = t->pc + (a64_u64)(sext(BITS(w, 23, 5), 19) << 2);
        if (!V && opc == 3) return 1;                           /* PRFM (literal) */
        if (V && opc == 3) return 0;
        x_mov_ri(B, XDX, a);
        if (!V) {
            int hd = wx(t, rt);
            ld_int(t, hd, XM(XDX, 0), opc == 1 ? 3 : 2, opc == 2 ? 2 : 1);
        } else {
            ld_vec_tmp(t, 0, XM(XDX, 0), 4 << opc);
            x_movdqa_st(B, XM(RCPU, OFF_V(rt)), 0);
        }
        return 1;
    }
    if ((w & 0x3A000000) == 0x28000000) {                       /* pares */
        int opc = (int)BITS(w, 31, 30), V = (int)BIT(w, 26), idx = (int)BITS(w, 24, 23), L = (int)BIT(w, 22);
        int rt = (int)(w & 31), rn = (int)BITS(w, 9, 5), rt2 = (int)BITS(w, 14, 10);
        int scale, sx = 0;
        if (!V) {
            if (opc == 0) scale = 2;
            else if (opc == 2) scale = 3;
            else if (opc == 1 && L && idx != 0) { scale = 2; sx = 1; }
            else return 0;
        } else {
            if (opc == 3) return 0;
            scale = 2 + opc;
        }
        int wback = idx == 1 || idx == 3;
        if (L && rt == rt2) return 0;                           /* CONSTRAINED UNPREDICTABLE */
        if (!V && wback && rn != 31 && (rt == rn || rt2 == rn)) return 0;
        int64_t off = sext(BITS(w, 21, 15), 7) << scale;
        int sz = 1 << scale;
        int hb = rget(t, rn);
        int32_t d = idx == 1 ? 0 : (int32_t)off;
        xo m1 = XM(hb, d), m2 = XM(hb, d + sz);
        if (!V) {
            /* mesmo padrão de acessos do interpretador (FEAT_LSE2): par de X alinhado a 16 = um
             * acesso de 16 bytes; par de W que não atravessa 16 bytes = um acesso de 8; senão dois */
            sa0_check(t, rn, hb);
            int spal = JIT_SA0 && rn == 31;     /* SP conferido: alinhamento conhecido */
            int single;                         /* 1 = sempre um acesso, 0 = nunca, -1 = em tempo de execução */
            if (scale == 3) single = spal ? ((d & 15) == 0) : -1;
            else single = spal ? ((d & 15) <= 8) : -1;
            int h1 = -1, h2 = -1, s1 = -1, s2 = -1;
            if (L) { h1 = wx(t, rt); h2 = wx(t, rt2); }
            else { s1 = rx(t, rt); s2 = rx(t, rt2); }
            a64_u8 *jsplit = NULL, *jdone = NULL;
            if (single < 0) {
                fl_clobber_force(t);
                if (scale == 3) {
                    if (!(d & 15)) x_test_ri(B, 0, XR(hb), 15);
                    else { x_lea(B, 0, XAX, XM(hb, d)); x_test_ri(B, 0, XR(XAX), 15); }
                    jsplit = x_jcc32(B, CC_NE);
                } else {
                    x_lea(B, 0, XAX, XM(hb, d));
                    x_alu_ri(B, 0, A_AND, XR(XAX), 15);
                    x_alu_ri(B, 0, A_CMP, XR(XAX), 8);
                    jsplit = x_jcc32(B, CC_A);
                }
            }
            if (single != 0) {
                if (scale == 3) {
                    if (L) {
                        fp_mark(t);
                        x_movdqu_ld(B, 0, m1);
                        xop2(B, XF_66 | XF_W, 0x7E, 0, XR(h1));             /* movq h1, xmm0 */
                        xop2(B, XF_66, 0x70, 0, XR(0)); xe8(B, 0xEE);      /* pshufd xmm0, xmm0, 0xEE */
                        xop2(B, XF_66 | XF_W, 0x7E, 0, XR(h2));
                    } else {
                        xop2(B, XF_66 | XF_W, 0x6E, 0, XR(s1));            /* movq xmm0, s1 */
                        xop2(B, XF_66 | XF_W, 0x6E, 1, XR(s2));
                        x_sse66(B, 0x6C, 0, XR(1));                         /* punpcklqdq */
                        fp_mark(t);
                        x_movdqu_st(B, m1, 0);
                    }
                } else {
                    if (single < 0 || 1) fl_clobber_force(t);
                    if (L) {
                        fp_mark(t);
                        x_mov_rm(B, 1, XAX, m1);
                        if (sx) { x_movsxd(B, h1, XR(XAX)); x_shift_ri(B, 1, S_SAR, XAX, 32); x_mov_rr(B, 1, h2, XAX); }
                        else { x_mov_rr(B, 1, h2, XAX); x_shift_ri(B, 1, S_SHR, h2, 32); x_mov_rr(B, 0, h1, XAX); }
                    } else {
                        x_mov_rr(B, 0, XAX, s1);
                        x_mov_rr(B, 1, XCX, s2);
                        x_shift_ri(B, 1, S_SHL, XCX, 32);
                        x_alu_rr(B, 1, A_OR, XAX, XCX);
                        fp_mark(t);
                        x_mov_mr(B, 1, m1, XAX);
                    }
                }
                if (single < 0) jdone = x_jmp32(B);
            }
            if (single <= 0) {
                if (jsplit && !t->b.ovf) x_patch32(jsplit, t->b.p);
                if (L) {
                    if (sx) { fp_mark(t); x_movsxd(B, XAX, m1); fp_mark(t); x_movsxd(B, h2, m2); x_mov_rr(B, 1, h1, XAX); }
                    else { fp_mark(t); x_mov_rm(B, scale == 3, XAX, m1); fp_mark(t); x_mov_rm(B, scale == 3, h2, m2); x_mov_rr(B, scale == 3, h1, XAX); }
                } else {
                    fp_mark(t);
                    if (scale == 3) x_mov_mr(B, 1, m1, s1); else x_mov_mr(B, 0, m1, s1);
                    fp_mark(t);
                    if (scale == 3) x_mov_mr(B, 1, m2, s2); else x_mov_mr(B, 0, m2, s2);
                }
            }
            if (jdone && !t->b.ovf) x_patch32(jdone, t->b.p);
        } else {
            if (L) {
                ld_vec_tmp(t, 0, m1, sz);
                ld_vec_tmp(t, 1, m2, sz);
                x_movdqa_st(B, XM(RCPU, OFF_V(rt)), 0);
                x_movdqa_st(B, XM(RCPU, OFF_V(rt2)), 1);
            } else {
                st_vec(t, rt, m1, sz);
                st_vec(t, rt2, m2, sz);
            }
        }
        if (wback) wb_base(t, rn, off);
        return 1;
    }
    if ((w & 0x3F000000) == 0x08000000) {                       /* exclusivos / ordenados */
        int size = (int)BITS(w, 31, 30), L = (int)BIT(w, 22), rt = (int)(w & 31), rn = (int)BITS(w, 9, 5);
        if (!BIT(w, 23) || BIT(w, 21)) return tr_excl(t, w);
        if (!BIT(w, 23) || BIT(w, 21) || !BIT(w, 15) || BITS(w, 20, 16) != 31 || BITS(w, 14, 10) != 31) return 0;
        int hb = rget(t, rn);
        sa0_check(t, rn, hb);
        if (size && !JIT_NAA && !(JIT_SA0 && rn == 31)) {       /* atravessa 16 bytes: falha de alinhamento */
            fl_clobber_force(t);
            x_mov_rr(B, 0, XAX, hb);
            x_alu_ri(B, 0, A_AND, XR(XAX), 15);
            x_alu_ri(B, 0, A_CMP, XR(XAX), 16 - (1 << size));
            slow_if(t, CC_A);
        }
        if (L) { ld_int(t, wx(t, rt), XM(hb, 0), size, 1); return 1; }     /* LDAR */
        /* STLR: xchg = store + barreira completa (RCsc com LDAR posterior) */
        if (rt == 31) x_mov_ri(B, XAX, 0);
        else x_mov_rr(B, 1, XAX, rget(t, rt));
        fp_mark(t);
        if (size == 0) x_xchg8_mr(B, XM(hb, 0), XAX);
        else if (size == 1) x_xchg16_mr(B, XM(hb, 0), XAX);
        else x_xchg_mr(B, size == 3, XM(hb, 0), XAX);
        return 1;
    }
    if ((w & 0x3A000000) != 0x38000000) return 0;               /* registrador único */
    if (!BIT(w, 24) && BIT(w, 21) && BITS(w, 11, 10) == 0) return tr_lse(t, w);   /* atômicos LSE (A:R no lugar do opc) */
    int size = (int)BITS(w, 31, 30), V = (int)BIT(w, 26), opc = (int)BITS(w, 23, 22);
    int rt = (int)(w & 31), rn = (int)BITS(w, 9, 5);
    int kind, bytes = 0;
    if (!V) {
        if (opc == 0) kind = 0;
        else if (opc == 1) kind = 1;
        else if (opc == 2) kind = size == 3 ? 4 : 2;            /* 4 = PRFM */
        else { if (size >= 2) return 0; kind = 3; }
    } else {
        if (opc & 2) { if (size) return 0; bytes = 16; } else bytes = 1 << size;
        kind = (opc & 1) ? 1 : 0;
    }
    int sc = V ? __builtin_ctz((unsigned)bytes) : size;
    xo m;
    int wback = 0;
    int64_t woff = 0;
    if (BIT(w, 24)) {                                           /* offset imediato sem sinal */
        if (kind == 4) return 1;
        int hb = rget(t, rn);
        if (!V) sa0_check(t, rn, hb);
        m = XM(hb, (int32_t)(BITS(w, 21, 10) << sc));
    } else if (!BIT(w, 21)) {
        int mode = (int)BITS(w, 11, 10);
        if (mode == 2) return 0;                                /* LDTR/STTR: interpretador */
        int64_t imm9 = sext(BITS(w, 20, 12), 9);
        if (kind == 4) { if (mode == 0) return 1; return 0; }   /* PRFUM; pré/pós = não alocado */
        if (mode && !V && rn == rt && rn != 31) return 0;       /* CONSTRAINED UNPREDICTABLE */
        int hb = rget(t, rn);
        if (!V) sa0_check(t, rn, hb);
        m = XM(hb, mode == 1 ? 0 : (int32_t)imm9);
        if (mode) { wback = 1; woff = imm9; }
    } else {
        if (BITS(w, 11, 10) == 0) return tr_lse(t, w);          /* atômicos LSE */
        if (BITS(w, 11, 10) != 2) return 0;                     /* PAC: interpretador */
        int opt = (int)BITS(w, 15, 13), S = (int)BIT(w, 12), rm = (int)BITS(w, 20, 16);
        if (!(opt & 2)) return 0;
        if (kind == 4) return 1;
        int hb = rget(t, rn);
        if (!V) sa0_check(t, rn, hb);    /* antes do índice em rax (o teste pode salvar as flags) */
        int amt = S ? sc : 0;
        int hi;
        if (rm == 31 && (opt & 3) == 3) hi = -1;
        else {
            int hm = rx(t, rm);
            if (opt == 2) { x_mov_rr(B, 0, XAX, hm); hi = XAX; }
            else if (opt == 6) { x_movsxd(B, XAX, XR(hm)); hi = XAX; }
            else hi = hm;
        }
        if (hi >= 0 && amt == 4) {
            if (hi != XAX) { x_mov_rr(B, 1, XAX, hi); hi = XAX; }
            x_lea(B, 1, XAX, XMI(-1, XAX, 3, 0));
            x_lea(B, 1, XAX, XMI(XAX, XAX, 0, 0));
            amt = 0;
        }
        m = hi >= 0 ? XMI(hb, hi, amt, 0) : XM(hb, 0);
    }
    if (!V) {
        if (kind == 0) st_int(t, rt, m, size);
        else if (rt == 31) ld_int(t, XDX, m, size, kind);      /* destino XZR: o acesso ainda acontece */
        else {
            int hd = rdef(t, rt);
            /* se o load escreve direto no registrador e há writeback, o load é o último acesso: ok */
            ld_int(t, hd, m, size, kind);
        }
    } else {
        if (kind == 0) st_vec(t, rt, m, bytes);
        else { ld_vec_tmp(t, 0, m, bytes); x_movdqa_st(B, XM(RCPU, OFF_V(rt)), 0); }
    }
    if (wback) wb_base(t, rn, woff);
    return 1;
}

/* ---------------------------------------------------------------------------------------------
 * Classificação (análise de vida das flags) e fim de bloco
 * ------------------------------------------------------------------------------------------- */
#include "jit_simd.h"

static int classify(a64_u32 w) {
    unsigned op0 = BITS(w, 28, 25);
    if (op0 == 8 || op0 == 9) {
        unsigned op = BITS(w, 25, 23);
        if (op == 2 && BIT(w, 29)) return C_WR;
        if (op == 4 && BITS(w, 30, 29) == 3) return C_WR;
        if (op == 3) return C_RD;
        return 0;
    }
    if (op0 == 0xA || op0 == 0xB) {
        if ((w & 0xFF000010) == 0x54000000) return C_RD;
        if ((w & 0x7C000000) == 0x14000000 || (w & 0x7C000000) == 0x34000000 || (w & 0xFE000000) == 0xD6000000) return 0;
        if ((w & 0xFFFFF01F) == 0xD503201F || (w & 0xFFFFF09F) == 0xD503309F) return C_RD;   /* hints/barreiras */
        return C_RD;
    }
    if (op0 == 5 || op0 == 0xD) {
        unsigned op2 = BITS(w, 24, 21);
        if (!BIT(w, 28)) {
            if (!(op2 & 8)) return BITS(w, 30, 29) == 3 ? C_WR : 0;
            return BIT(w, 29) ? C_WR : 0;
        }
        if (op2 == 0) return BITS(w, 15, 10) ? C_RD : (C_RD | (BIT(w, 29) ? C_WR : 0));
        if (op2 == 2) return C_RD | C_WR;
        if (op2 == 4) return C_RD;
        if (op2 == 6 || (op2 & 8)) return 0;
        return C_RD;
    }
    if ((op0 & 7) == 7) {               /* SIMD/FP nativo: só FCMP/FCCMP/FCSEL tocam NZCV */
        if ((w & 0xFF203C00) == 0x1E202000) return C_WR;          /* FCMP / FCMPE */
        if ((w & 0xFF200C00) == 0x1E200400) return C_RD | C_WR;   /* FCCMP / FCCMPE */
        if ((w & 0xFF200C00) == 0x1E200C00) return C_RD;          /* FCSEL */
        return 0;
    }
    return C_RD;                        /* memória (pode falhar) e o resto (interpretador) */
}

static int ends_block(a64_u32 w) {
    if ((w & 0x7C000000) == 0x14000000) return 1;              /* B/BL */
    if ((w & 0xFF000000) == 0x54000000) return 1;              /* B.cond */
    if ((w & 0x7C000000) == 0x34000000) return 1;              /* CB*, TB* */
    if ((w & 0xFE000000) == 0xD6000000) return 1;              /* BR/BLR/RET/... */
    if ((w & 0xFF000000) == 0xD4000000) return 1;              /* SVC/HVC/SMC/BRK/HLT */
    if ((w & 0xFFFFF0FF) == 0xD50330DF) return 1;              /* ISB */
    if (w == 0 || (w >> 25 & 0xF) < 4) return 1;               /* UDF / não alocado */
    return 0;
}

static int tr_insn(T *t, a64_u32 w) {
    switch ((w >> 25) & 0xF) {
    case 0x8: case 0x9: return tr_dpimm(t, w);
    case 0xA: case 0xB: return tr_branch(t, w);
    case 0x4: case 0x6: case 0xC: case 0xE: return tr_ldst(t, w);
    case 0x5: case 0xD: return tr_dpreg(t, w);
    case 0x7: case 0xF: return tr_simd(t, w);
    default: return 0;                  /* 0..3: UDF/SVE/SME: o interpretador decide */
    }
}

/* emite o bloco em [p, lim). native[] (se não NULL) recebe 1 para cada instrução emitida nativamente */
static void emit_block(T *t, a64_u8 *p, a64_u8 *lim, int selfcheck, a64_u8 *native) {
    a64_u64 pc = t->pc0;
    t->b.p = p; t->b.lim = lim; t->b.ovf = 0;
    t->start = p;
    t->ended = 0;
    t->nfp = 0;
    t->tick = 0;
    rc_drop(t);
    t->fe = 0; t->fm = 1;
    t->mxclean = 0;

    if (selfcheck) {                    /* página volátil: confere o código guest na entrada */
        x_mov_ri(B, XCX, pc);
        a64_u8 *js[JIT_MAX_INSNS];
        int nj = 0;
        for (int i = 0; i < t->n; i += 2) {
            if (i + 1 < t->n) {
                x_mov_ri(B, XAX, (a64_u64)t->insn[i] | (a64_u64)t->insn[i + 1] << 32);
                x_alu_rm(B, 1, A_CMP, XAX, XM(XCX, 4 * i));
            } else x_alu_ri(B, 0, A_CMP, XM(XCX, 4 * i), (int32_t)t->insn[i]);
            js[nj++] = x_jcc32(B, CC_NE);
        }
        a64_u8 *jok = x_jmp32(B);
        for (int k = 0; k < nj && !t->b.ovf; k++) x_patch32(js[k], t->b.p);
        x_mov_ri(B, XAX, pc);
        x_mov_mr(B, 1, XM(RCPU, OFF_PC), XAX);
        x_mov_ri(B, XAX, JX_STALE);
        x_jmp_to(B, jit_g.g_exit);
        if (!t->b.ovf) x_patch32(jok, t->b.p);
    }

    for (t->i = 0; t->i < t->n && !t->ended; t->i++) {
        a64_u32 w = t->insn[t->i];
        t->pc = pc + 4ull * (a64_u64)t->i;
        int ok = jit_g.nonative ? 0 : tr_insn(t, w);
        if (!ok && !native) fb_log(t, w);
        if (native) native[t->i] = (a64_u8)ok;
        if (ok) insn_end(t);
        else emit_fallback(t, w);
        if (t->b.ovf) return;
    }
    if (t->ended != 1) {
        t->pc = pc + 4ull * (a64_u64)t->n;
        end_prep(t);
        emit_exit(t, t->pc);
    }
}

static a64_u8 scratch[1 << 16];

jblk *jit_translate(a64_u64 pc, int selfcheck) {
    static T tt;                        /* trava global tomada pelo chamador */
    T *t = &tt;
    __builtin_memset(t, 0, offsetof(T, fp));
    t->pc0 = pc;
    /* até o fim da página (pc pode estar desalinhado: a palavra que atravessa a página vira um
     * bloco de 1 instrução; sem estouro perto do topo do espaço de endereços) */
    int room = (int)((JIT_PAGE - (pc & (JIT_PAGE - 1))) / 4);
    if (room < 1) room = 1;
    int maxn = (jit_g.maxinsns > 0 && jit_g.maxinsns < JIT_MAX_INSNS) ? jit_g.maxinsns : JIT_MAX_INSNS;
    if (maxn > room) maxn = room;
    for (a64_u64 a = pc; t->n < maxn; a += 4) {
        a64_u32 w = a64_ld32(a);
        t->insn[t->n++] = w;
        if (ends_block(w)) break;
    }
    /* 1ª passada (rascunho, flags sempre vivas): descobre o que é nativo */
    a64_u8 native[JIT_MAX_INSNS];
    for (int i = 0; i < t->n; i++) { t->live[i] = 1; native[i] = 0; }
    emit_block(t, scratch, scratch + sizeof scratch, 0, native);
    /* vida das flags: instrução que vai ao interpretador conta como leitura (estado exato) */
    int L = 1;
    for (int i = t->n - 1; i >= 0; i--) {
        t->live[i] = (a64_u8)L;
        int c = native[i] ? classify(t->insn[i]) : C_RD;
        if (c & C_RD) L = 1; else if (c & C_WR) L = 0;
    }
    a64_u8 *p, *plim;
    jit_code_space(&p, &plim);
    emit_block(t, p, plim, selfcheck, NULL);
    if (t->b.ovf) return NULL;
    jblk *b = jit_data_alloc(sizeof(jblk) + sizeof(jfp) * (a64_u64)t->nfp);
    if (!b) return NULL;
    __builtin_memset(b, 0, sizeof *b);
    b->pc = pc;
    b->end = pc + 4ull * (a64_u64)t->n;
    b->code = t->start;
    b->len = (a64_u32)(t->b.p - t->start);
    b->nfp = (a64_u32)t->nfp;
    b->fp = (jfp *)(b + 1);
    __builtin_memcpy(b->fp, t->fp, sizeof(jfp) * (a64_u64)t->nfp);
    jit_code_commit(t->b.p);
    return b;
}

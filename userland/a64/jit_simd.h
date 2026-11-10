/*
 * mOS — tradutor arm64 (frente JIT): emissão nativa de AdvSIMD/FP que é exata por construção
 * (operações de bits e inteiras, cópias, imediatos, moves entre registradores). Incluído por
 * jit_tr.c (usa o contexto T e os helpers de lá). FP aritmético (escalar e vetorial), comparações
 * e conversões só são nativos no caminho provadamente igual ao ARM (FPCR, flags do x86 e classe do
 * resultado conferidos em tempo de execução); o resto vai ao interpretador (primitivas a64_fp_*).
 * Recursos do host: SSE2 sempre; SSSE3/SSE4.1/SSE4.2 só se o CPUID disser (jit_g.*), senão
 * a instrução vai ao interpretador.
 * Convenção: xmm0..xmm5 são temporários; os registradores V ficam sempre em a64_cpu.
 */

#define VO(r) XM(RCPU, OFF_V(r))
#define VOE(r, off) XM(RCPU, OFF_V(r) + (int32_t)(off))
#define OFF_FPCR ((int32_t)offsetof(a64_cpu, fpcr))
#define OFF_FPSR ((int32_t)offsetof(a64_cpu, fpsr))
static int s_xtn(T *t, int q, int size, int n, int d);
static int s_uzptrn(T *t, int q, int size, int opc, int n, int m, int d);
static int s_fpvec(T *t, a64_u32 w);
static int s_addp(T *t, int q, int size, int n, int m, int d);
static int s_pmull64(T *t, int q, int n, int m, int d);
static int s_qaddsub(T *t, int q, int u, int size, int sub, int n, int m, int d);
static int s_qdmulh_h(T *t, int q, int rnd, int n, int m, int d);
static void s_lanecheck16(T *t, int q);
static void vcvtph2ps_r(T *t, int x, int s);
static void vcvtps2ph_r(T *t, int x, int s);
static int s_qxtn(T *t, int q, int u, int op, int size, int n, int d);
static int s_addhn(T *t, int q, int u, int size, int sub, int n, int m, int d);
static int s_ldst_single(T *t, a64_u32 w);
static void s_round_packed(T *t, int pd, int dst, int src, int mode);
static void s_cvtsi_rax(T *t, int dbl, int uns);
static int s_shlreg(T *t, int q, int u, int size, int n, int m, int d, int scalar);
static int s_pairwise_mm(T *t, int q, int u, int size, int mn, int n, int m, int d);
static int s_rev(T *t, int q, int u, int op, int size, int n, int d);
static int s_cnt(T *t, int q, int n, int d);
static int s_iabs(T *t, int q, int size, int n, int d, int scalar);
static int s_scvtf(T *t, int q, int u, int size, int n, int d, int scalar);
static int s_frint_vec(T *t, int q, int size, int mode, int n, int d);
static int s_fabsneg_vec(T *t, int q, int size, int neg, int n, int d);
static int s_fminmax(T *t, int dbl, int which, int n, int m, int d);
static int s_frint_sc(T *t, int dbl, int opc, int n, int d);
static int s_fcvt_gpr(T *t, int dbl, int sf, int rmode, int opc, int n, int d);
static int s_shift_scalar(T *t, a64_u32 w);
static int s_scalar_simd(T *t, a64_u32 w);

static void s_ld(T *t, int x, int v) { x_movdqu_ld(B, x, VO(v)); }
static void s_st(T *t, int v, int x, int q) {
    if (!q) xop2(B, XF_F3, 0x7E, x, XR(x));        /* movq x, x: zera 127:64 */
    x_movdqu_st(B, VO(v), x);
}
static void s_op(T *t, unsigned op, int x, int y) { xop2(B, XF_66, op, x, XR(y)); }
static void s_op38(T *t, unsigned op, int x, int y) { xop3(B, XF_66, 0x38, op, x, XR(y)); }
static void s_shiftdq(T *t, int x, int left, int bytes) { if (bytes) { xop2(B, XF_66, 0x73, left ? 7 : 3, XR(x)); xe8(B, (a64_u8)bytes); } }
static void s_ones(T *t, int x) { s_op(t, 0x76, x, x); }                 /* pcmpeqd x, x */
static void s_zero(T *t, int x) { s_op(t, 0xEF, x, x); }                 /* pxor x, x */
static void s_const(T *t, int x, a64_u64 pat) {                         /* x = pat:pat */
    x_mov_ri(B, XAX, pat);
    xop2(B, XF_66 | XF_W, 0x6E, x, XR(XAX));        /* movq x, rax */
    s_op(t, 0x6C, x, x);                            /* punpcklqdq */
}
/* escreve um escalar de 'eb' bits (em rax, já zero-estendido) em V[d] zerando o resto */
static void s_wsc_rax(T *t, int d) {
    x_mov_mr(B, 1, VO(d), XAX);
    x_mov_mi(B, 1, VOE(d, 8), 0);
}
static void s_vstore64(T *t, int d, int q) {        /* V[d] = rax : (q ? rax : 0) */
    x_mov_mr(B, 1, VO(d), XAX);
    if (q) x_mov_mr(B, 1, VOE(d, 8), XAX); else x_mov_mi(B, 1, VOE(d, 8), 0);
}
/* elemento de V[v] (índice idx, tamanho 1<<size bytes) em rax, zero-estendido */
static void s_elem(T *t, int v, int idx, int size) {
    xo m = VOE(v, idx << size);
    if (size == 0) x_movzx8(B, XAX, m);
    else if (size == 1) x_movzx16(B, XAX, m);
    else x_mov_rm(B, size == 3, XAX, m);
}
static void s_replicate(T *t, int size) {           /* rax (zero-estendido) → padrão de 64 bits */
    if (size == 0) { x_mov_ri(B, XCX, 0x0101010101010101ull); x_imul_rr(B, 1, XAX, XR(XCX)); }
    else if (size == 1) { x_mov_ri(B, XCX, 0x0001000100010001ull); x_imul_rr(B, 1, XAX, XR(XCX)); }
    else if (size == 2) { x_mov_rr(B, 1, XCX, XAX); x_shift_ri(B, 1, S_SHL, XCX, 32); x_alu_rr(B, 1, A_OR, XAX, XCX); }
}

/* comparação inteira por lanes: x = (x OP y) com OP = gt (signed) / eq; usa SSE4.x para 64 bits */
static int s_cmp(T *t, int eq, int size, int x, int y) {
    if (size == 3) {
        if (eq) { if (!jit_g.sse41) return 0; s_op38(t, 0x29, x, y); }
        else { if (!jit_g.sse42) return 0; s_op38(t, 0x37, x, y); }
        return 1;
    }
    s_op(t, (eq ? 0x74 : 0x64) + (unsigned)size, x, y);
    return 1;
}
static const a64_u64 s_signbits[4] = { 0x8080808080808080ull, 0x8000800080008000ull, 0x8000000080000000ull, 0x8000000000000000ull };

static int s_three_same(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), size = (int)BITS(w, 23, 22), op = (int)BITS(w, 15, 11);
    if (op >= 0x18) return s_fpvec(t, w);           /* FP vetorial */
    int m = (int)BITS(w, 20, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (op == 0x03) {
        s_ld(t, 0, n);
        s_ld(t, 1, m);
        int r = 0;
        switch ((u << 2) | size) {
        case 0: s_op(t, 0xDB, 0, 1); break;
        case 1: s_op(t, 0xDF, 1, 0); r = 1; break;                      /* pandn: ~m & n */
        case 2: s_op(t, 0xEB, 0, 1); break;
        case 3: s_ones(t, 2); s_op(t, 0xEF, 1, 2); s_op(t, 0xEB, 0, 1); break;
        case 4: s_op(t, 0xEF, 0, 1); break;
        case 5: s_ld(t, 2, d); s_op(t, 0xEF, 0, 1); s_op(t, 0xDB, 0, 2); s_op(t, 0xEF, 0, 1); break;   /* BSL */
        case 6: s_ld(t, 2, d); s_op(t, 0xEF, 0, 2); s_op(t, 0xDB, 0, 1); s_op(t, 0xEF, 0, 2); break;   /* BIT */
        default: s_ld(t, 2, d); s_op(t, 0xEF, 0, 2); s_op(t, 0xDF, 1, 0); s_op(t, 0xEF, 1, 2); r = 1; break; /* BIF */
        }
        s_st(t, d, r, q);
        return 1;
    }
    if (size == 3 && !q) return 0;
    switch (op) {
    case 0x10:                                      /* ADD / SUB */
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (size == 3) s_op(t, u ? 0xFB : 0xD4, 0, 1);
        else s_op(t, (u ? 0xF8 : 0xFC) + (unsigned)size, 0, 1);
        s_st(t, d, 0, q);
        return 1;
    case 0x11:                                      /* CMTST / CMEQ */
        if (size == 3 && !jit_g.sse41) return 0;
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (u) s_cmp(t, 1, size, 0, 1);
        else { s_op(t, 0xDB, 0, 1); s_zero(t, 1); s_cmp(t, 1, size, 0, 1); s_ones(t, 1); s_op(t, 0xEF, 0, 1); }
        s_st(t, d, 0, q);
        return 1;
    case 0x06: case 0x07: {                         /* CMGT CMHI / CMGE CMHS */
        if (size == 3 && !jit_g.sse42) return 0;
        int ge = op == 0x07;
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (u) { s_const(t, 2, s_signbits[size]); s_op(t, 0xEF, 0, 2); s_op(t, 0xEF, 1, 2); }
        int r = 0;
        if (!ge) s_cmp(t, 0, size, 0, 1);
        else { s_cmp(t, 0, size, 1, 0); s_ones(t, 2); s_op(t, 0xEF, 1, 2); r = 1; }      /* !(m > n) */
        s_st(t, d, r, q);
        return 1;
    }
    case 0x0C: case 0x0D: {                         /* SMAX UMAX / SMIN UMIN */
        if (size == 3) return 0;
        int mn = op == 0x0D;
        static const unsigned sse2op[3][4] = { { 0, 0xDE, 0, 0xDA }, { 0xEE, 0, 0xEA, 0 }, { 0, 0, 0, 0 } };
        static const unsigned sse4op[3][4] = { { 0x3C, 0, 0x38, 0 }, { 0, 0x3E, 0, 0x3A }, { 0x3D, 0x3F, 0x39, 0x3B } };
        int k = mn * 2 + u;
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (sse2op[size][k]) s_op(t, sse2op[size][k], 0, 1);
        else { if (!jit_g.sse41) return 0; s_op38(t, sse4op[size][k], 0, 1); }
        s_st(t, d, 0, q);
        return 1;
    }
    case 0x16:                                      /* SQDMULH / SQRDMULH */
        return size == 1 ? s_qdmulh_h(t, q, u, n, m, d) : 0;
    case 0x01: case 0x05:                           /* SQADD UQADD / SQSUB UQSUB */
        return s_qaddsub(t, q, u, size, op == 0x05, n, m, d);
    case 0x17:                                      /* ADDP */
        if (u) return 0;
        return s_addp(t, q, size, n, m, d);
    case 0x13:                                      /* MUL */
        if (u || (size != 1 && size != 2) || (size == 2 && !jit_g.sse41)) return 0;
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (size == 1) s_op(t, 0xD5, 0, 1); else s_op38(t, 0x40, 0, 1);
        s_st(t, d, 0, q);
        return 1;
    case 0x08:                                      /* SSHL / USHL */
        return s_shlreg(t, q, u, size, n, m, d, 0);
    case 0x14: case 0x15:                           /* SMAXP UMAXP / SMINP UMINP */
        if (size == 3) return 0;
        return s_pairwise_mm(t, q, u, size, op == 0x15, n, m, d);
    }
    return 0;
}

static int s_misc(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), size = (int)BITS(w, 23, 22), op = (int)BITS(w, 16, 12);
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (op == 0x00 || (op == 0x01 && !u)) return s_rev(t, q, u, op, size, n, d);
    if (op == 0x05 && !u && size == 0) return s_cnt(t, q, n, d);
    if (op == 0x0B && !u) return s_iabs(t, q, size, n, d, 0);
    if (op == 0x1D && !(size & 2)) return s_scvtf(t, q, u, size, n, d, 0);
    {
        int key = (u << 6) | ((size >> 1) << 5) | op;
        if (key == 0x58 || key == 0x59) return 0; /* FRINTA / FRINTX vetoriais → interpretador */
        if (key == 0x18 || key == 0x19 || key == 0x38 || key == 0x39 || key == 0x79)
            return s_frint_vec(t, q, size, key == 0x79 ? -1 : key == 0x18 ? 0 : key == 0x19 ? 1 : key == 0x38 ? 2 : 3, n, d);
        if (key == 0x2F || key == 0x6F) return s_fabsneg_vec(t, q, size, key == 0x6F, n, d);
    }
    if (op == 0x05 && u && size == 0) {             /* NOT */
        s_ld(t, 0, n); s_ones(t, 1); s_op(t, 0xEF, 0, 1); s_st(t, d, 0, q);
        return 1;
    }
    if (size == 3 && !q) return 0;
    switch (op) {
    case 0x08: case 0x09: case 0x0A: {              /* CMGT/CMGE/CMEQ/CMLE/CMLT #0 */
        if (op == 0x0A && u) return 0;
        if (size == 3 && !(op == 0x09 && !u ? jit_g.sse41 : jit_g.sse42)) return 0;
        s_ld(t, 0, n); s_zero(t, 1);
        int r = 0;
        if (op == 0x08 && !u) s_cmp(t, 0, size, 0, 1);                                 /* n > 0 */
        else if (op == 0x08) { s_cmp(t, 0, size, 1, 0); s_ones(t, 2); s_op(t, 0xEF, 1, 2); r = 1; }  /* !(0 > n) */
        else if (!u && op == 0x09) s_cmp(t, 1, size, 0, 1);                            /* n == 0 */
        else if (op == 0x09) { s_cmp(t, 0, size, 0, 1); s_ones(t, 2); s_op(t, 0xEF, 0, 2); } /* !(n > 0) */
        else { s_cmp(t, 0, size, 1, 0); r = 1; }                                        /* 0 > n */
        s_st(t, d, r, q);
        return 1;
    }
    case 0x14:                                      /* SQXTN UQXTN */
        return s_qxtn(t, q, u, 0x14, size, n, d);
    case 0x12:                                      /* XTN / XTN2, SQXTUN */
        if (u) return s_qxtn(t, q, u, 0x12, size, n, d);
        return s_xtn(t, q, size, n, d);
    case 0x0B:                                      /* NEG (ABS vai ao interpretador) */
        if (!u) return 0;
        s_ld(t, 1, n); s_zero(t, 0);
        if (size == 3) s_op(t, 0xFB, 0, 1); else s_op(t, 0xF8 + (unsigned)size, 0, 1);
        s_st(t, d, 0, q);
        return 1;
    }
    return 0;
}

static int s_across(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), size = (int)BITS(w, 23, 22), op = (int)BITS(w, 16, 12);
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (size == 3 || (size == 2 && !q)) return 0;
    if (op == 0x1B && !u && size == 0) {            /* ADDV B */
        s_ld(t, 0, n);
        if (!q) xop2(B, XF_F3, 0x7E, 0, XR(0));
        s_zero(t, 1);
        s_op(t, 0xF6, 0, 1);                        /* psadbw */
        x_movdqa_ld(B, 1, XR(0));
        s_shiftdq(t, 1, 0, 8);
        s_op(t, 0xD4, 0, 1);
        x_movd_st(B, XR(XAX), 0);
        x_movzx8(B, XAX, XR(XAX));
        s_wsc_rax(t, d);
        return 1;
    }
    if (op != 0x0A && op != 0x1A) return 0;         /* MAXV / MINV */
    int mn = op == 0x1A, k = mn * 2 + u;
    static const unsigned sse2op[3][4] = { { 0, 0xDE, 0, 0xDA }, { 0xEE, 0, 0xEA, 0 }, { 0, 0, 0, 0 } };
    static const unsigned sse4op[3][4] = { { 0x3C, 0, 0x38, 0 }, { 0, 0x3E, 0, 0x3A }, { 0x3D, 0x3F, 0x39, 0x3B } };
    if (!sse2op[size][k] && !jit_g.sse41) return 0;
    s_ld(t, 0, n);
    for (int sh = q ? 8 : 4; sh >= (1 << size); sh >>= 1) {
        x_movdqa_ld(B, 1, XR(0));
        s_shiftdq(t, 1, 0, sh);
        if (sse2op[size][k]) s_op(t, sse2op[size][k], 0, 1); else s_op38(t, sse4op[size][k], 0, 1);
    }
    x_movd_st(B, XR(XAX), 0);
    if (size == 0) x_movzx8(B, XAX, XR(XAX));
    else if (size == 1) x_movzx16(B, XAX, XR(XAX));
    s_wsc_rax(t, d);
    return 1;
}

static int s_copy(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), op = (int)BIT(w, 29), imm5 = (int)BITS(w, 20, 16), imm4 = (int)BITS(w, 14, 11);
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if ((imm5 & 0xF) == 0) return 0;
    int size = __builtin_ctz((unsigned)imm5), idx = imm5 >> (size + 1);
    if (op) {                                       /* INS (elemento) */
        if (!q) return 0;
        s_elem(t, n, imm4 >> size, size);
        xo m = VOE(d, idx << size);
        if (size == 0) x_mov8_mr(B, m, XAX); else if (size == 1) x_mov16_mr(B, m, XAX); else x_mov_mr(B, size == 3, m, XAX);
        return 1;
    }
    switch (imm4) {
    case 0: case 1:                                 /* DUP elemento / geral */
        if (size == 3 && !q) return 0;
        fl_clobber(t);
        if (imm4 == 0) s_elem(t, n, idx, size);
        else {
            int h = rx(t, n);
            if (size == 0) x_movzx8(B, XAX, XR(h));
            else if (size == 1) x_movzx16(B, XAX, XR(h));
            else x_mov_rr(B, size == 3, XAX, h);
        }
        s_replicate(t, size);
        s_vstore64(t, d, q);
        return 1;
    case 3: {                                       /* INS (geral) */
        if (!q) return 0;
        xo m = VOE(d, idx << size);
        if (n == 31) {
            if (size == 0) x_mov8_mi(B, m, 0); else if (size == 1) x_mov16_mi(B, m, 0); else x_mov_mi(B, size == 3, m, 0);
            return 1;
        }
        int h = rget(t, n);
        if (size == 0) x_mov8_mr(B, m, h); else if (size == 1) x_mov16_mr(B, m, h); else x_mov_mr(B, size == 3, m, h);
        return 1;
    }
    case 5: {                                       /* SMOV */
        if (size == 3 || (size == 2 && !q)) return 0;
        xo m = VOE(n, idx << size);
        int hd = wx(t, d);
        if (size == 0) x_movsx8(B, q, hd, m);
        else if (size == 1) x_movsx16(B, q, hd, m);
        else x_movsxd(B, hd, m);
        return 1;
    }
    case 7: {                                       /* UMOV */
        if (q ? size != 3 : size == 3) return 0;
        xo m = VOE(n, idx << size);
        int hd = wx(t, d);
        if (size == 0) x_movzx8(B, hd, m);
        else if (size == 1) x_movzx16(B, hd, m);
        else x_mov_rm(B, size == 3, hd, m);
        return 1;
    }
    }
    return 0;
}

static int s_modimm(T *t, a64_u32 w) {             /* MOVI/MVNI/ORR/BIC/FMOV (vetor, imediato) */
    int q = (int)BIT(w, 30), op = (int)BIT(w, 29), cmode = (int)BITS(w, 15, 12), o2 = (int)BIT(w, 11), d = (int)(w & 31);
    a64_u64 imm8 = (BITS(w, 18, 16) << 5) | BITS(w, 9, 5), imm = 0;
    int k = 0;                                      /* 0 MOVI, 1 MVNI, 2 ORR, 3 BIC */
    if (o2) {
        if (cmode != 0xF || op) return 0;
        a64_u64 h = ((imm8 >> 7) << 15) | ((((imm8 >> 6) & 1) ^ 1) << 14) | (((imm8 >> 6) & 1) ? 0x3000 : 0) | ((imm8 & 0x3F) << 6);
        imm = h * 0x0001000100010001ull;
    } else {
        switch (cmode >> 1) {
        case 0: case 1: case 2: case 3:
            imm = (imm8 << (8 * (cmode >> 1))) * 0x0000000100000001ull;
            k = (cmode & 1) ? (op ? 3 : 2) : op;
            break;
        case 4: case 5:
            imm = (imm8 << (8 * ((cmode >> 1) & 1))) * 0x0001000100010001ull;
            k = (cmode & 1) ? (op ? 3 : 2) : op;
            break;
        case 6:
            imm = ((cmode & 1) ? ((imm8 << 16) | 0xFFFF) : ((imm8 << 8) | 0xFF)) * 0x0000000100000001ull;
            k = op;
            break;
        default:
            if (!(cmode & 1)) {
                if (!op) imm = imm8 * 0x0101010101010101ull;
                else for (int b = 0; b < 8; b++) if ((imm8 >> b) & 1) imm |= 0xFFull << (8 * b);
            } else if (!op) {
                a64_u64 s = ((imm8 >> 7) << 31) | ((((imm8 >> 6) & 1) ^ 1) << 30) | (((imm8 >> 6) & 1) ? 0x3E000000ull : 0) | ((imm8 & 0x3F) << 19);
                imm = s * 0x0000000100000001ull;
            } else {
                if (!q) return 0;
                imm = ((imm8 >> 7) << 63) | ((((imm8 >> 6) & 1) ^ 1) << 62) | (((imm8 >> 6) & 1) ? 0x3FC0000000000000ull : 0) | ((imm8 & 0x3F) << 48);
            }
            break;
        }
    }
    if (k == 1) { imm = ~imm; k = 0; }
    if (k == 0) {
        if (!imm) { s_zero(t, 0); s_st(t, d, 0, 1); if (!q) {} return 1; }
        x_mov_ri(B, XAX, imm);
        s_vstore64(t, d, q);
        return 1;
    }
    fl_clobber(t);
    x_mov_ri(B, XAX, k == 2 ? imm : ~imm);
    for (int e = 0; e < 2; e++) {
        if (e && !q) { x_mov_mi(B, 1, VOE(d, 8), 0); break; }
        x_alu_mr(B, 1, k == 2 ? A_OR : A_AND, VOE(d, 8 * e), XAX);
    }
    return 1;
}

static int s_ext(T *t, a64_u32 w) {                /* EXT */
    int q = (int)BIT(w, 30), imm4 = (int)BITS(w, 14, 11), m = (int)BITS(w, 20, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (BITS(w, 23, 22) || (!q && (imm4 & 8))) return 0;
    if (q) {
        s_ld(t, 0, n); s_ld(t, 1, m);
        s_shiftdq(t, 0, 0, imm4);
        if (imm4) { s_shiftdq(t, 1, 1, 16 - imm4); s_op(t, 0xEB, 0, 1); }
    } else {
        x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 1, VO(m));
        s_op(t, 0x6C, 0, 1);                        /* [m_lo : n_lo] */
        s_shiftdq(t, 0, 0, imm4);
    }
    s_st(t, d, 0, q);
    return 1;
}

static int s_perm(T *t, a64_u32 w) {               /* só ZIP1/ZIP2 */
    int q = (int)BIT(w, 30), size = (int)BITS(w, 23, 22), opc = (int)BITS(w, 14, 12);
    int m = (int)BITS(w, 20, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (opc != 3 && opc != 7) return s_uzptrn(t, q, size, opc, n, m, d);
    if (size == 3 && !q) return 0;
    static const unsigned lo[4] = { 0x60, 0x61, 0x62, 0x6C }, hi[4] = { 0x68, 0x69, 0x6A, 0x6D };
    s_ld(t, 0, n); s_ld(t, 1, m);
    if (q) s_op(t, opc == 3 ? lo[size] : hi[size], 0, 1);
    else { s_op(t, lo[size], 0, 1); if (opc == 7) s_shiftdq(t, 0, 0, 8); }
    s_st(t, d, 0, q);
    return 1;
}

/* constante de 128 bits em x a partir de dois u64 (usa xmm5 se x != 5) */
static void s_const2(T *t, int x, a64_u64 lo, a64_u64 hi) {
    x_mov_ri(B, XAX, lo);
    xop2(B, XF_66 | XF_W, 0x6E, x, XR(XAX));
    x_mov_ri(B, XAX, hi);
    xop2(B, XF_66 | XF_W, 0x6E, 5, XR(XAX));
    s_op(t, 0x6C, x, 5);                            /* punpcklqdq x, xmm5 */
}
static void s_pshufb(T *t, int x, int mask) { s_op38(t, 0x00, x, mask); }
/* deslocamento de todas as lanes de x por imediato: kind 0 = lógico à esquerda, 1 = lógico à
 * direita, 2 = aritmético à direita. Bytes emulados com psllw/psrlw + máscara. Usa xmm4. */
static int s_shimm(T *t, int x, int eb, int kind, int sh) {
    static const int ext[3] = { 6, 2, 4 };          /* /6 shl, /2 shr, /4 sar */
    if (!sh) return 1;
    if (eb == 8) {
        if (sh >= 8) { if (kind != 2) { s_zero(t, x); return 1; } sh = 7; }
        xop2(B, XF_66, 0x71, kind == 0 ? 6 : 2, XR(x)); xe8(B, (a64_u8)sh);    /* psllw/psrlw */
        a64_u64 m = kind == 0 ? (0xFFull << sh) & 0xFF : 0xFFull >> sh;
        s_const(t, 4, m * 0x0101010101010101ull);
        s_op(t, 0xDB, x, 4);
        if (kind == 2) {                            /* sinal: (t ^ s) - s, s = 0x80 >> sh */
            s_const(t, 4, (0x80ull >> sh) * 0x0101010101010101ull);
            s_op(t, 0xEF, x, 4);
            s_op(t, 0xF8, x, 4);                    /* psubb */
        }
        return 1;
    }
    /* psraq não existe no SSE: desloca lógico e preenche com o sinal do qword
     * (psrad 31 + pshufd 0xF5 copia o sinal de cada metade de 64 para as duas dwords).
     * sh >= 64 vira 63: o resultado é 0 ou -1, igual ao deslocamento pela largura. */
    if (eb == 64 && kind == 2) {
        if (sh >= 64) sh = 63;
        x_movdqa_ld(B, 4, XR(x));
        xop2(B, XF_66, 0x73, 2, XR(x)); xe8(B, (a64_u8)sh);          /* psrlq */
        xop2(B, XF_66, 0x72, 4, XR(4)); xe8(B, 31);                  /* psrad 31 */
        xop2(B, XF_66, 0x70, 4, XR(4)); xe8(B, 0xF5);                /* sinal do qword nas duas dwords */
        xop2(B, XF_66, 0x73, 6, XR(4)); xe8(B, (a64_u8)(64 - sh));   /* psllq: bits de sinal acima */
        s_op(t, 0xEB, x, 4);
        return 1;
    }
    if (sh >= eb) { if (kind != 2) { s_zero(t, x); return 1; } sh = eb - 1; }
    xop2(B, XF_66, eb == 16 ? 0x71 : eb == 32 ? 0x72 : 0x73, ext[kind], XR(x));
    xe8(B, (a64_u8)sh);
    return 1;
}
/* máscara pshufb que leva a metade baixa de cada lane de eb bits para os 8 bytes de baixo */
static a64_u64 s_narrowmask(int eb) {
    a64_u64 m = 0;
    int hb = eb / 16, k = 0;                        /* bytes por meia lane */
    for (int e = 0; e < 8 / hb; e++)
        for (int b = 0; b < hb; b++) m |= (a64_u64)(e * (eb / 8) + b) << (8 * k++);
    return m;
}
/* ---------------------------------------------------------------------------------------------
 * Saturantes e arredondados. QC (FPSR bit 27) liga quando alguma lane saturou: comparo o
 * resultado saturado com o resultado sem saturação (wrap ou estreitamento truncado).
 * ------------------------------------------------------------------------------------------- */
#define FPSR_QC 0x08000000
/* xmm0 = saturado, xmm1 = sem saturação: QC se alguma lane difere (usa xmm2, rax) */
static void s_qc(T *t) {
    fl_clobber(t);
    x_movdqa_ld(B, 2, XR(0));
    s_op(t, 0x74, 2, 1);                            /* pcmpeqb */
    xop2(B, XF_66, 0xD7, XAX, XR(2));               /* pmovmskb */
    x_alu_ri(B, 0, A_CMP, XR(XAX), 0xFFFF);
    a64_u8 *j = x_jcc8(B, CC_E);
    x_alu_ri(B, 0, A_OR, XM(RCPU, OFF_FPSR), FPSR_QC);
    x_patch8(j, t->b.p);
}
/* SQADD UQADD SQSUB UQSUB em B e H (paddsb/paddusb/psubsb/psubusb, versões w) */
/* x = todos os bits iguais ao bit de sinal de cada lane de 32/64 bits */
static void s_signmask(T *t, int x, int eb) {
    xop2(B, XF_66, 0x72, 4, XR(x)); xe8(B, 31);     /* psrad 31 */
    if (eb == 64) { xop2(B, XF_66, 0x70, x, XR(x)); xe8(B, 0xF5); }   /* pshufd: copia o dword de cima */
}
/* SQADD UQADD SQSUB UQSUB em lanes de 32/64 bits: estouro pelos bits de sinal; QC se algum */
static int s_qaddsub_sd(T *t, int q, int u, int size, int sub, int n, int m, int d) {
    int eb = size == 2 ? 32 : 64;
    if (eb == 64 && !q) return 0;
    if (u && eb == 64 && !jit_g.sse42) return 0;    /* comparação sem sinal de 64 bits */
    unsigned add = eb == 32 ? 0xFE : 0xD4, subo = eb == 32 ? 0xFA : 0xFB;
    if (q) { s_ld(t, 0, n); s_ld(t, 1, m); } else { x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 1, VO(m)); }
    x_movdqa_ld(B, 2, XR(0));                       /* xmm2 = a, xmm1 = b */
    s_op(t, sub ? subo : add, 0, 1);                /* xmm0 = r (com volta) */
    if (u) {                                        /* estouro: r <u a (soma) / a <u b (subtração) */
        s_const(t, 4, eb == 32 ? 0x8000000080000000ull : 0x8000000000000000ull);
        x_movdqa_ld(B, 3, XR(sub ? 1 : 2)); s_op(t, 0xEF, 3, 4);    /* maior: a (soma) / b (subtração) */
        x_movdqa_ld(B, 5, XR(sub ? 2 : 0)); s_op(t, 0xEF, 5, 4);    /* menor: r (soma) / a (subtração) */
        s_cmp(t, 0, size, 3, 5);                    /* xmm3 = estourou */
        x_movdqa_ld(B, 1, XR(0));                   /* xmm1 = r com volta (para o QC) */
        if (sub) { s_op(t, 0xDF, 3, 0); x_movdqa_ld(B, 0, XR(3)); }   /* 0 onde estourou */
        else s_op(t, 0xEB, 0, 3);                   /* todos 1 onde estourou */
    } else {                                        /* com sinal */
        x_movdqa_ld(B, 3, XR(2)); s_op(t, 0xEF, 3, 0);              /* a ^ r */
        x_movdqa_ld(B, 4, XR(sub ? 2 : 1)); s_op(t, 0xEF, 4, sub ? 1 : 0);   /* soma: b ^ r; subtração: a ^ b */
        s_op(t, 0xDB, 3, 4);
        s_signmask(t, 3, eb);                       /* xmm3 = estourou */
        s_signmask(t, 2, eb);                       /* xmm2 = sinal de a */
        s_const(t, 4, eb == 32 ? 0x7FFFFFFF7FFFFFFFull : 0x7FFFFFFFFFFFFFFFull);
        s_op(t, 0xEF, 2, 4);                        /* saturado = sinal(a) ^ MAX */
        x_movdqa_ld(B, 1, XR(0));                   /* xmm1 = r com volta (para o QC) */
        s_op(t, 0xDB, 2, 3);
        s_op(t, 0xDF, 3, 0);                        /* ~estouro & r */
        s_op(t, 0xEB, 3, 2);
        x_movdqa_ld(B, 0, XR(3));
    }
    s_qc(t);
    s_st(t, d, 0, q);
    return 1;
}
/* SQDMULH / SQRDMULH em H: (2ab) >> 16 [arredondado]; só 0x8000 × 0x8000 satura (→ 0x7FFF, QC) */
static int s_qdmulh_h(T *t, int q, int rnd, int n, int m, int d) {
    if (rnd && !jit_g.ssse3) return 0;
    if (q) { s_ld(t, 0, n); s_ld(t, 1, m); } else { x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 1, VO(m)); }
    s_const(t, 4, 0x8000800080008000ull);
    x_movdqa_ld(B, 2, XR(0)); s_op(t, 0x75, 2, 4);  /* a == 0x8000 */
    x_movdqa_ld(B, 3, XR(1)); s_op(t, 0x75, 3, 4);
    s_op(t, 0xDB, 2, 3);                            /* xmm2 = lanes que saturam */
    if (rnd) s_op38(t, 0x0B, 0, 1);                 /* pmulhrsw */
    else {
        x_movdqa_ld(B, 3, XR(0));
        s_op(t, 0xE5, 0, 1);                        /* pmulhw: alto */
        s_op(t, 0xD5, 3, 1);                        /* pmullw: baixo */
        xop2(B, XF_66, 0x71, 6, XR(0)); xe8(B, 1);  /* alto << 1 */
        xop2(B, XF_66, 0x71, 2, XR(3)); xe8(B, 15); /* baixo >> 15 */
        s_op(t, 0xEB, 0, 3);
    }
    s_op(t, 0xEF, 0, 2);                            /* 0x8000 ^ 0xFFFF = 0x7FFF nas que saturam */
    x_movdqa_ld(B, 1, XR(0)); s_op(t, 0xEF, 1, 2);  /* xmm1 difere de xmm0 exatamente nas saturadas */
    s_qc(t);
    s_st(t, d, 0, q);
    return 1;
}

static int s_qaddsub(T *t, int q, int u, int size, int sub, int n, int m, int d) {
    if (size > 1) return s_qaddsub_sd(t, q, u, size, sub, n, m, d);
    static const unsigned sat[2][2][2] = { { { 0xEC, 0xDC }, { 0xE8, 0xD8 } }, { { 0xED, 0xDD }, { 0xE9, 0xD9 } } };
    static const unsigned wrap[2][2] = { { 0xFC, 0xF8 }, { 0xFD, 0xF9 } };
    if (q) { s_ld(t, 0, n); s_ld(t, 3, m); } else { x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 3, VO(m)); }
    x_movdqa_ld(B, 1, XR(0));
    s_op(t, sat[size][sub][u], 0, 3);
    s_op(t, wrap[size][sub], 1, 3);
    s_qc(t);
    s_st(t, d, 0, q);
    return 1;
}
/* deslocamento arredondado à direita de x (lanes de eb): (x >> sh) + ((x >> (sh-1)) & 1).
 * Exato sem alargar (é floor(x/2^sh + 1/2)). kind 1 sem sinal, 2 com sinal. Usa xmm3, xmm4. */
static int s_rshr(T *t, int x, int eb, int kind, int sh) {
    x_movdqa_ld(B, 3, XR(x));
    if (!s_shimm(t, x, eb, kind, sh)) return 0;
    s_shimm(t, 3, eb, 1, sh - 1);
    a64_u64 one = eb == 8 ? 0x0101010101010101ull : eb == 16 ? 0x0001000100010001ull : eb == 32 ? 0x0000000100000001ull : 1;
    s_const(t, 4, one);
    s_op(t, 0xDB, 3, 4);
    s_op(t, eb == 8 ? 0xFC : eb == 16 ? 0xFD : eb == 32 ? 0xFE : 0xD4, x, 3);
    return 1;
}
/* SQXTN UQXTN SQXTUN (e as formas "2") de H→B e S→H */
static int s_qxtn(T *t, int q, int u, int op, int size, int n, int d) {
    if (size > 1 || !jit_g.ssse3) return 0;
    int un = op == 0x12 && u;                       /* SQXTUN: com sinal → sem sinal */
    int uu = op == 0x14 && u;                       /* UQXTN: sem sinal → sem sinal */
    if ((uu || (un && size == 1)) && !jit_g.sse41) return 0;
    s_ld(t, 0, n);
    x_movdqa_ld(B, 1, XR(0));
    if (uu) {                                       /* min(x, máximo) e depois o pack sem sinal */
        s_const(t, 2, size == 0 ? 0x00FF00FF00FF00FFull : 0x0000FFFF0000FFFFull);
        if (size == 0) s_op38(t, 0x3A, 0, 2); else s_op38(t, 0x3B, 0, 2);   /* pminuw / pminud */
    }
    s_zero(t, 2);
    if (size == 0) s_op(t, (un || uu) ? 0x67 : 0x63, 0, 2);   /* packuswb / packsswb */
    else if (un || uu) s_op38(t, 0x2B, 0, 2);                 /* packusdw */
    else s_op(t, 0x6B, 0, 2);                                 /* packssdw */
    /* QC: o resultado (8 bytes baixos de xmm0) re-alargado tem que ser igual ao original (xmm1) */
    static const unsigned lo[2] = { 0x60, 0x61 }, eq[2] = { 0x75, 0x76 };
    x_movq_ld(B, 2, XR(0));
    s_zero(t, 3);
    if (!un && !uu) s_cmp(t, 0, size, 3, 2);           /* SQXTN: estende o sinal */
    s_op(t, lo[size], 2, 3);
    s_op(t, eq[size], 2, 1);                        /* pcmpeqw / pcmpeqd com o original */
    fl_clobber(t);
    xop2(B, XF_66, 0xD7, XAX, XR(2));
    x_alu_ri(B, 0, A_CMP, XR(XAX), 0xFFFF);
    a64_u8 *jq = x_jcc8(B, CC_E);
    x_alu_ri(B, 0, A_OR, XM(RCPU, OFF_FPSR), FPSR_QC);
    x_patch8(jq, t->b.p);
    if (!q) { s_st(t, d, 0, 0); return 1; }
    x_movq_st(B, VOE(d, 8), 0);
    return 1;
}
/* ADDHN SUBHN RADDHN RSUBHN (e as formas "2"): metade de cima de (n ± m [+ arredondamento]) */
static int s_addhn(T *t, int q, int u, int size, int sub, int n, int m, int d) {
    if (size == 3 || !jit_g.ssse3) return 0;
    int wb = 16 << size;
    static const unsigned add[3] = { 0xFD, 0xFE, 0xD4 }, subo[3] = { 0xF9, 0xFA, 0xFB };
    s_ld(t, 0, n); s_ld(t, 1, m);
    s_op(t, sub ? subo[size] : add[size], 0, 1);
    if (u) {                                        /* + 1 << (eb - 1), módulo 2*eb como no ARM */
        a64_u64 r = wb == 16 ? 0x0080008000800080ull : wb == 32 ? 0x0000800000008000ull : 0x0000000080000000ull;
        s_const(t, 2, r);
        s_op(t, add[size], 0, 2);
    }
    s_shimm(t, 0, wb, 1, wb / 2);
    s_const2(t, 1, s_narrowmask(wb), 0x8080808080808080ull);
    s_pshufb(t, 0, 1);
    if (!q) { s_st(t, d, 0, 0); return 1; }
    x_movq_st(B, VOE(d, 8), 0);
    return 1;
}

static int s_shift(T *t, a64_u32 w) {              /* AdvSIMD vetorial, deslocamento por imediato */
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), immh = (int)BITS(w, 22, 19), op = (int)BITS(w, 15, 11);
    int hb = (int)BITS(w, 22, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    int eb = (immh & 8) ? 64 : (immh & 4) ? 32 : (immh & 2) ? 16 : 8;
    int rsh = 2 * eb - hb, lsh = hb - eb;
    if (op == 0x00 || op == 0x02 || (op == 0x0A && !u)) {   /* SSHR USHR SSRA USRA SHL */
        if (eb == 64 && !q) return 0;
        s_ld(t, 0, n);
        if (op == 0x0A) s_shimm(t, 0, eb, 0, lsh);
        else s_shimm(t, 0, eb, u ? 1 : 2, rsh);
        if (op == 0x02) {
            s_ld(t, 1, d);
            s_op(t, eb == 8 ? 0xFC : eb == 16 ? 0xFD : eb == 32 ? 0xFE : 0xD4, 0, 1);
        }
        s_st(t, d, 0, q);
        return 1;
    }
    if (op == 0x04 || op == 0x06) {               /* SRSHR URSHR SRSRA URSRA */
        if (eb == 64 && !q) return 0;
        s_ld(t, 0, n);
        s_rshr(t, 0, eb, u ? 1 : 2, rsh);
        if (op == 0x06) { s_ld(t, 1, d); s_op(t, eb == 8 ? 0xFC : eb == 16 ? 0xFD : eb == 32 ? 0xFE : 0xD4, 0, 1); }
        s_st(t, d, 0, q);
        return 1;
    }
    if (op == 0x11 && !u) {                         /* RSHRN / RSHRN2 */
        if (eb == 64 || !jit_g.ssse3) return 0;
        s_ld(t, 0, n);
        s_rshr(t, 0, 2 * eb, 1, rsh);
        s_const2(t, 1, s_narrowmask(2 * eb), 0x8080808080808080ull);
        s_pshufb(t, 0, 1);
        if (!q) { s_st(t, d, 0, 0); return 1; }
        x_movq_st(B, VOE(d, 8), 0);
        return 1;
    }
    if (op == 0x14) {                               /* SSHLL/USHLL (UXTL/SXTL): alarga a metade Q */
        if (eb == 64) return 0;
        static const unsigned lo[3] = { 0x60, 0x61, 0x62 };
        int sz = eb == 8 ? 0 : eb == 16 ? 1 : 2;
        if (q) x_movq_ld(B, 0, VOE(n, 8)); else x_movq_ld(B, 0, VO(n));
        s_zero(t, 1);
        if (!u) s_cmp(t, 0, sz, 1, 0);              /* xmm1 = (0 > x): extensão de sinal */
        s_op(t, lo[sz], 0, 1);                      /* intercala com zeros / sinal */
        s_shimm(t, 0, 2 * eb, 0, lsh);
        s_st(t, d, 0, 1);
        return 1;
    }
    if (op == 0x10 && !u) {                         /* SHRN / SHRN2 */
        if (eb == 64 || !jit_g.ssse3) return 0;
        s_ld(t, 0, n);
        s_shimm(t, 0, 2 * eb, 1, rsh);
        s_const2(t, 1, s_narrowmask(2 * eb), 0x8080808080808080ull);
        s_pshufb(t, 0, 1);
        if (!q) { s_st(t, d, 0, 0); return 1; }
        x_movq_st(B, VOE(d, 8), 0);                 /* SHRN2: metade de cima, a de baixo fica */
        return 1;
    }
    return 0;
}

/* XTN / XTN2 (2misc op 0x12, U = 0) */
static int s_xtn(T *t, int q, int size, int n, int d) {
    if (size == 3 || !jit_g.ssse3) return 0;
    s_ld(t, 0, n);
    s_const2(t, 1, s_narrowmask(16 << size), 0x8080808080808080ull);
    s_pshufb(t, 0, 1);
    if (!q) { s_st(t, d, 0, 0); return 1; }
    x_movq_st(B, VOE(d, 8), 0);
    return 1;
}

/* TBL/TBX com 1 a 4 registradores de tabela (pshufb): índice >= 16*len → 0 (TBL) / mantém (TBX) */
static int s_tbl(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), m = (int)BITS(w, 20, 16), len = (int)BITS(w, 14, 13) + 1, tbx = (int)BIT(w, 12);
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (BITS(w, 23, 22) || !jit_g.ssse3) return 0;
    s_ld(t, 1, m);                                  /* índices */
    s_zero(t, 0);                                   /* resultado */
    s_const(t, 4, 0x7070707070707070ull);
    for (int k = 0; k < len; k++) {
        x_movdqa_ld(B, 2, XR(1));
        s_op(t, 0xDC, 2, 4);                        /* paddusb: >= 16 → bit 7 (zera no pshufb) */
        s_ld(t, 3, (n + k) & 31);
        s_pshufb(t, 3, 2);
        s_op(t, 0xEB, 0, 3);
        if (k + 1 < len) {                          /* índices -= 16 (os já usados viram >= 0xF0) */
            s_const(t, 5, 0x1010101010101010ull);
            s_op(t, 0xF8, 1, 5);
        }
    }
    if (tbx) {                                      /* fora da faixa: índice original >= 16*len */
        s_ld(t, 1, m);
        s_const(t, 4, (a64_u64)(16 * len - 1) * 0x0101010101010101ull);
        s_op(t, 0xD8, 1, 4);                        /* psubusb: != 0 ⇔ fora */
        s_zero(t, 4);
        s_op(t, 0x74, 1, 4);                        /* 0xFF onde está dentro */
        s_ld(t, 2, d);
        s_op(t, 0xDF, 1, 2);                        /* pandn: ~dentro & d */
        s_op(t, 0xEB, 0, 1);
    }
    s_st(t, d, 0, q);
    return 1;
}

/* UZP1/UZP2/TRN1/TRN2 (ZIP fica em s_perm) */
static int s_uzptrn(T *t, int q, int size, int opc, int n, int m, int d) {
    int eb = 8 << size, by = eb / 8;
    if (size == 3 && !q) return 0;
    if (opc == 1 || opc == 5) {                     /* UZP: elementos pares/ímpares */
        if (!jit_g.ssse3) return 0;
        int odd = opc == 5;
        a64_u64 lo = 0;                             /* pshufb: lanes pares (ou ímpares) → 8 bytes baixos */
        int k = 0;
        for (int e = 0; e < 16 / by && k < 8; e += 2)
            for (int b = 0; b < by; b++, k++) lo |= (a64_u64)((e + odd) * by + b) << (8 * k);
        if (q) {
            s_ld(t, 0, n); s_ld(t, 1, m);
            s_const2(t, 2, lo, 0x8080808080808080ull);
            s_pshufb(t, 0, 2); s_pshufb(t, 1, 2);
            s_op(t, 0x6C, 0, 1);                    /* punpcklqdq: pares de n : pares de m */
        } else {
            x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 1, VO(m));
            s_op(t, 0x6C, 0, 1);                    /* 16 bytes = n_lo : m_lo */
            s_const2(t, 2, lo, 0x8080808080808080ull);
            s_pshufb(t, 0, 2);
        }
        s_st(t, d, 0, q);
        return 1;
    }
    if (opc == 2 || opc == 6) {                     /* TRN1 / TRN2 */
        int two = opc == 6;
        a64_u64 ev = 0;                             /* bytes das lanes pares */
        for (int b = 0; b < 8; b++) if (((b / by) & 1) == 0) ev |= 0xFFull << (8 * b);
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (!two) s_shiftdq(t, 1, 1, by);           /* m[2i] → posição 2i+1 */
        else s_shiftdq(t, 0, 0, by);                /* n[2i+1] → posição 2i */
        if (size == 3) s_const2(t, 2, ~0ull, 0); else s_const(t, 2, ev);
        s_op(t, 0xDB, 0, 2);                        /* n nas lanes pares */
        s_op(t, 0xDF, 2, 1);                        /* m nas ímpares (pandn: ~par & m) */
        s_op(t, 0xEB, 0, 2);
        s_st(t, d, 0, q);
        return 1;
    }
    return 0;
}

/* FMOV (geral) e FMOV (registrador): cópias de bits */
static int s_fmov(T *t, a64_u32 w) {
    if ((w & 0x7F20FC00) == 0x1E200000) {          /* conversão FP ↔ inteiro com opcode 110/111 */
        int sf = (int)BIT(w, 31), type = (int)BITS(w, 23, 22), rmode = (int)BITS(w, 20, 19), opc = (int)BITS(w, 18, 16);
        int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
        if (BIT(w, 29) || (opc != 6 && opc != 7)) return 0;
        int bytes, hi = 0;
        if (type == 0 && !sf && rmode == 0) bytes = 4;
        else if (type == 1 && sf && rmode == 0) bytes = 8;
        else if (type == 2 && sf && rmode == 1) { bytes = 8; hi = 1; }
        else if (type == 3 && rmode == 0) bytes = 2;
        else return 0;
        if (opc == 6) {                             /* FMOV Rd, Vn */
            int hd = wx(t, d);
            xo m = VOE(n, hi ? 8 : 0);
            if (bytes == 2) x_movzx16(B, hd, m); else x_mov_rm(B, bytes == 8, hd, m);
            return 1;
        }
        if (n == 31) x_mov_ri(B, XAX, 0);           /* FMOV Vd, Rn */
        else {
            int hn = rget(t, n);
            if (bytes == 2) x_movzx16(B, XAX, XR(hn)); else x_mov_rr(B, bytes == 8, XAX, hn);
        }
        if (hi) x_mov_mr(B, 1, VOE(d, 8), XAX);
        else s_wsc_rax(t, d);
        return 1;
    }
    if ((w & 0xFF3FFC00) == 0x1E204000) {          /* FMOV (registrador) */
        int type = (int)BITS(w, 23, 22), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
        if (type == 2) return 0;
        if (type == 0) x_mov_rm(B, 0, XAX, VO(n));
        else if (type == 1) x_mov_rm(B, 1, XAX, VO(n));
        else x_movzx16(B, XAX, VO(n));
        s_wsc_rax(t, d);
        return 1;
    }
    return 0;
}

/* LD1/ST1 (múltiplas estruturas, 1 a 4 registradores), com e sem pós-índice */
static int s_ldst_multi(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), L = (int)BIT(w, 22), post = (int)BIT(w, 23), opcode = (int)BITS(w, 15, 12);
    int rm = (int)BITS(w, 20, 16), rn = (int)BITS(w, 9, 5), rt = (int)(w & 31);
    if (!post && rm) return 0;
    int nr, selem = 1, size = (int)BITS(w, 11, 10);
    switch (opcode) {
    case 7: nr = 1; break; case 10: nr = 2; break; case 6: nr = 3; break; case 2: nr = 4; break;
    case 8: nr = 2; selem = 2; break; case 4: nr = 3; selem = 3; break; case 0: nr = 4; selem = 4; break;   /* LD2..LD4 */
    default: return 0;
    }
    if (selem > 1 && size == 3 && !q) return 0;
    int bytes = q ? 16 : 8;
    int hb = rget(t, rn);
    int noted = 0;
    /* o interpretador acessa elemento a elemento: se o intervalo cruza página, o efeito parcial
     * de uma falha seria outro; esse caso vai inteiro ao interpretador */
    fl_clobber_force(t);
    x_mov_rr(B, 0, XAX, hb);
    x_alu_ri(B, 0, A_AND, XR(XAX), 0xFFF);
    x_alu_ri(B, 0, A_CMP, XR(XAX), 0x1000 - nr * bytes);
    slow_if(t, CC_A);
    if (!L) { xmon_pre(t, XM(hb, 0), nr * bytes); hb = rget(t, rn); noted = 1; }
    if (selem > 1) {                                /* estruturas intercaladas: elemento a elemento */
        int eb = 1 << size, ne = bytes >> size;     /* tudo numa página: só o 1º acesso pode falhar */
        for (int e = 0; e < ne; e++)
            for (int s = 0; s < selem; s++) {
                xo mem = XM(hb, (e * selem + s) * eb), v = VOE((rt + s) & 31, e * eb);
                if (L) {
                    if (!e && !s) fp_mark(t);
                    if (size == 0) x_movzx8(B, XDX, mem); else if (size == 1) x_movzx16(B, XDX, mem); else x_mov_rm(B, size == 3, XDX, mem);
                    if (size == 0) x_mov8_mr(B, v, XDX); else if (size == 1) x_mov16_mr(B, v, XDX); else x_mov_mr(B, size == 3, v, XDX);
                } else {
                    if (size == 0) x_movzx8(B, XDX, v); else if (size == 1) x_movzx16(B, XDX, v); else x_mov_rm(B, size == 3, XDX, v);
                    if (!e && !s) fp_mark(t);
                    if (size == 0) x_mov8_mr(B, mem, XDX); else if (size == 1) x_mov16_mr(B, mem, XDX); else x_mov_mr(B, size == 3, mem, XDX);
                }
            }
        if (L && !q) for (int s = 0; s < selem; s++) x_mov_mi(B, 1, VOE((rt + s) & 31, 8), 0);
    } else     if (L) {
        for (int k = 0; k < nr; k++) {
            fp_mark(t);
            if (q) x_movdqu_ld(B, k, XM(hb, k * bytes)); else x_movq_ld(B, k, XM(hb, k * bytes));
        }
        for (int k = 0; k < nr; k++) x_movdqu_st(B, VO((rt + k) & 31), k);
    } else {
        for (int k = 0; k < nr; k++) {
            int v = (rt + k) & 31;
            if (q) { x_movdqu_ld(B, 0, VO(v)); fp_mark(t); x_movdqu_st(B, XM(hb, k * bytes), 0); }
            else { x_mov_rm(B, 1, XDX, VO(v)); fp_mark(t); x_mov_mr(B, 1, XM(hb, k * bytes), XDX); }
        }
    }
    if (post) {
        if (rm == 31) wb_base(t, rn, nr * bytes);
        else {
            int hm = rget(t, rm);
            int h = rget(t, rn);
            rdef(t, rn);
            x_lea(B, 1, h, XMI(h, hm, 0, 0));
        }
    }
    if (noted) xmon_post(t);
    return 1;
}

/* ---------------------------------------------------------------------------------------------
 * FP escalar nativo (S e D), só no caminho provadamente igual ao ARM; o resto pelo caminho lento
 * (a instrução inteira no interpretador, que usa as primitivas a64_fp_* da frente FP):
 *   - FPCR.RMode = RN e FZ = 0 (conferido em tempo de execução);
 *   - nenhuma flag do x86 além de PE (IE, DE = entrada denormal, ZE, OE, UE → lento);
 *   - resultado NaN/infinito ou minúsculo (expoente <= 1 e não zero) → lento (propagação de NaN e
 *     detecção de tininess diferem entre x86 e ARM);
 *   - PE → FPSR.IXC. Operações IEEE corretamente arredondadas: resultado idêntico.
 * MXCSR do host fica em 0x1F80 durante o código gerado (restabelecido depois de código C);
 * PE fica acumulado de propósito (IXC também é acumulado). [rsp] e [rsp+4] são rascunho (frame
 * do trampolim).
 * ------------------------------------------------------------------------------------------- */
#define OFF_FPCR ((int32_t)offsetof(a64_cpu, fpcr))
#define OFF_FPSR ((int32_t)offsetof(a64_cpu, fpsr))

static void mx_clean(T *t) {
    if (t->mxclean) return;
    x_mov_mi(B, 0, XM(XSP, 4), 0x1F80);
    xop2(B, 0, 0xAE, 2, XM(XSP, 4));                /* ldmxcsr [rsp+4] */
    t->mxclean = 1;
}
static void s_wsc_es(T *t, int d, int dbl) {        /* rax (S: zero-estendido) → V[d] zerando o resto */
    if (!dbl) x_mov_rr(B, 0, XAX, XAX);
    s_wsc_rax(t, d);
}

/* FMA3 (VEX.66.0F38 op /r, W = dbl): x = op(x, v, [r15 + disp]) — só base RCPU e disp32 */
static void vex_fma_m(T *t, unsigned op, int dbl, int x, int v, int32_t disp) {
    xe8(B, 0xC4);
    xe8(B, 0xC0 | 0x02);                            /* R̄=1 X̄=1 B̄=0 (r15), mapa 0F38 */
    xe8(B, (dbl ? 0x80 : 0) | ((~v & 0xF) << 3) | 0x01);   /* W vvvv̄ L=0 pp=66 */
    xe8(B, op);
    xe8(B, 0x80 | ((x & 7) << 3) | (RCPU & 7));
    xe32(B, (a64_u32)disp);
}

/* FCMP/FCMPE (S, D): sem NaN o ARM e o x86 não sinalizam nada (FZ = 0); NaN → interpretador.
 * Depois de ucomis: CF = menor, ZF = igual. NZCV: menor 1000, igual 0110, maior 0010.
 * Monta o formato x86 de reserved[14] direto: AH = N<<7 | Z<<6 | 2 | C, AL = V = 0. */
static void fcmp_core(T *t, int ty, int n, int m, int zero) {   /* ty: 0 S, 1 D, 3 H (convertido para S: exato) */
    int dbl = ty == 1;
    if (ty == 3) { x_movzx16(B, XAX, VO(n)); x_movd_ld(B, 0, XR(XAX)); vcvtph2ps_r(t, 0, 0); }
    else if (dbl) x_movq_ld(B, 0, VO(n)); else x_movd_ld(B, 0, VO(n));
    if (zero) s_zero(t, 1);
    else if (ty == 3) { x_movzx16(B, XAX, VO(m)); x_movd_ld(B, 1, XR(XAX)); vcvtph2ps_r(t, 1, 1); }
    else if (dbl) x_movq_ld(B, 1, VO(m)); else x_movd_ld(B, 1, VO(m));
    xop2(B, dbl ? XF_66 : 0, 0x2E, 0, XR(1));      /* ucomisd/ucomiss xmm0, xmm1 */
    slow_if(t, CC_P);
    x_setcc(B, CC_B, XAX);                          /* al = menor */
    x_setcc(B, CC_E, XCX);                          /* cl = igual */
    x_movzx8(B, XAX, XR(XAX));
    x_movzx8(B, XCX, XR(XCX));
    x_mov_rr(B, 0, XDX, XAX);
    x_shift_ri(B, 0, S_SHL, XAX, 7);                /* N = menor */
    x_alu_ri(B, 0, A_XOR, XR(XDX), 1);              /* C = !menor */
    x_alu_rr(B, 0, A_OR, XAX, XDX);
    x_shift_ri(B, 0, S_SHL, XCX, 6);                /* Z = igual */
    x_alu_rr(B, 0, A_OR, XAX, XCX);
    x_alu_ri(B, 0, A_OR, XR(XAX), 2);
    x_shift_ri(B, 0, S_SHL, XAX, 8);
    x_mov16_mr(B, XM(RCPU, OFF_FLAGS), XAX);
}
static int s_fcmp(T *t, a64_u32 w) {
    int type = (int)BITS(w, 23, 22), n = (int)BITS(w, 9, 5), m = (int)BITS(w, 20, 16);
    if (BITS(w, 31, 29) != 0) return 0;
    if ((w & 0xFF200C00) == 0x1E200C00) {          /* FCSEL: seleção de bits, sem exceção */
        if (type == 2) return 0;
        int d = (int)(w & 31);
        fl_need(t);
        int cc = fl_cc(t, (int)BITS(w, 15, 12));
        int sz = type == 1 ? 3 : type == 0 ? 2 : 1;
        xo vn = VO(n), vm = VO(m);
        if (sz == 1) { x_movzx16(B, XCX, vn); x_movzx16(B, XDX, vm); }
        else { x_mov_rm(B, sz == 3, XCX, vn); x_mov_rm(B, sz == 3, XDX, vm); }
        if (cc < 0) x_mov_rr(B, 1, XDX, XCX); else x_cmov(B, 1, cc, XDX, XR(XCX));
        x_mov_rr(B, 1, XAX, XDX);
        s_wsc_rax(t, d);
        return 1;
    }
    if (type == 2 || (type == 3 && !jit_g.f16c)) return 0;   /* half via F16C */
    a64_u32 fzb = type == 3 ? (1u << 19) : (1u << 24);    /* FZ16 / FZ: denormais viram zero → interp */
    if ((w & 0xFF203C00) == 0x1E202000) {          /* FCMP / FCMPE */
        if (BITS(w, 15, 14) || (w & 7)) return 0;
        fl_clobber(t);
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), fzb);  /* FZ (FZ16): denormais viram zero → interp */
        slow_if(t, CC_NE);
        fcmp_core(t, type, n, m, (int)BIT(w, 3));
        t->fe = 0; t->fm = 1;
        return 1;
    }
    if ((w & 0xFF200C00) == 0x1E200400) {          /* FCCMP / FCCMPE */
        int cond = (int)BITS(w, 15, 12);
        fl_need(t);
        int cc = fl_cc(t, cond);
        a64_u8 *jt = NULL, *jd = NULL;
        if (cc >= 0) {
            jt = x_jcc32(B, cc);
            x_mov16_mi(B, XM(RCPU, OFF_FLAGS), jit_nzcv_to_x86((w & 15) << 28));
            jd = x_jmp32(B);
            if (!t->b.ovf) x_patch32(jt, t->b.p);
        }
        /* condição verdadeira: estado do caminho lento = flags de antes, ainda no EFLAGS → salvar */
        x_lahf(B); x_setcc(B, CC_O, XAX);
        if (t->fe == 2) xe8(B, 0x80), xe8(B, 0xF4), xe8(B, 0x01);   /* xor ah, 1: modo SUB → C */
        x_mov16_mr(B, XM(RCPU, OFF_FLAGS), XAX);
        t->fe = 0; t->fm = 1;                       /* o test abaixo destrói o EFLAGS */
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), fzb);
        slow_if(t, CC_NE);
        fcmp_core(t, type, n, m, 0);
        if (jd && !t->b.ovf) x_patch32(jd, t->b.p);
        t->fe = 0; t->fm = 1;
        return 1;
    }
    return 0;
}

/* conversões FP ↔ inteiro (FCVTZS/FCVTZU/SCVTF/UCVTF, registrador geral) e FCVT S ↔ D */
static void ixc_if_pe(T *t) {                       /* stmxcsr já feito em [rsp] */
    x_test8_mi(B, XM(XSP, 0), 0x20);
    a64_u8 *j = x_jcc8(B, CC_E);
    x_alu_ri(B, 0, A_OR, XM(RCPU, OFF_FPSR), 0x10);
    x_patch8(j, t->b.p);
}
static int s_cvt(T *t, a64_u32 w) {
    int type = (int)BITS(w, 23, 22), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (BIT(w, 29) || type == 2) return 0;
    if ((w & 0xFF3E7C00) == 0x1E224000 && (type == 3 || BITS(w, 16, 15) == 3)) {   /* FCVT com half */
        int to = (int)BITS(w, 16, 15);
        if (!jit_g.f16c || to == type || to == 2 || (type == 1 && to == 3)) return 0;   /* D → H: dois arredondamentos */
        fl_clobber(t);
        mx_clean(t);
        if (type == 3) {                            /* H → S/D: exato; NaN, FZ16 e AHP → interp */
            x_test_ri(B, 0, XM(RCPU, OFF_FPCR), (1u << 19) | (1u << 26));
            slow_if(t, CC_NE);
            x_movzx16(B, XAX, VO(n));
            x_mov_rr(B, 0, XCX, XAX);
            x_alu_ri(B, 0, A_AND, XR(XCX), 0x7FFF);
            x_alu_ri(B, 0, A_CMP, XR(XCX), 0x7C00);
            slow_if(t, CC_A);
            x_movd_ld(B, 0, XR(XAX));
            vcvtph2ps_r(t, 0, 0);
            if (to == 1) xop2(B, XF_F3, 0x5A, 0, XR(0));   /* cvtss2sd: exato */
            s_st(t, d, 0, 0);
            if (to == 0) x_mov_mi(B, 0, VOE(d, 4), 0);
            return 1;
        }
        /* S → H: um arredondamento (RN); FZ, FZ16, AHP → interp; só PE; resultado normal e finito */
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 0x01C00000 | (1u << 19) | (1u << 26));
        slow_if(t, CC_NE);
        x_movd_ld(B, 1, VO(n));
        vcvtps2ph_r(t, 0, 1);
        xop2(B, 0, 0xAE, 3, XM(XSP, 0));
        x_test8_mi(B, XM(XSP, 0), 0x1F);
        slow_if(t, CC_NE);
        x_movd_st(B, XR(XAX), 0);
        x_movzx16(B, XAX, XR(XAX));
        x_movd_ld(B, 0, XR(XAX));
        s_lanecheck16(t, 0);
        ixc_if_pe(t);
        x_movd_st(B, XR(XAX), 0);
        s_wsc_rax(t, d);
        return 1;
    }
    if (type > 1) return 0;
    int dbl = type == 1;
    if ((w & 0x7F20FC00) == 0x1E200000) {
        int sf = (int)BIT(w, 31), rmode = (int)BITS(w, 20, 19), opc = (int)BITS(w, 18, 16);
        if (opc <= 1 || ((opc == 4 || opc == 5) && rmode == 0)) /* FCVTN/P/M/Z e FCVTA, com e sem sinal */
            return s_fcvt_gpr(t, dbl, sf, rmode, opc, n, d);
        if (rmode == 0 && (opc == 2 || opc == 3)) { /* SCVTF / UCVTF */
            int u = opc == 3;
            fl_clobber(t);
            int hn = rx(t, n);
            x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 3u << 22);   /* só RN (o resultado nunca é denormal) */
            slow_if(t, CC_NE);
            if (!sf) { if (u) x_mov_rr(B, 0, XAX, hn); else x_movsxd(B, XAX, XR(hn)); }
            else x_mov_rr(B, 1, XAX, hn);
            mx_clean(t);
            s_zero(t, 0);
            s_cvtsi_rax(t, dbl, u && sf);          /* u64 com bit 63: (n>>1)*2, exato acima de 2^63 */
            xop2(B, 0, 0xAE, 3, XM(XSP, 0));
            ixc_if_pe(t);
            x_movdqu_st(B, VO(d), 0);
            return 1;
        }
        return 0;
    }
    if ((w & 0xFF3E7C00) == 0x1E224000) {          /* FCVT (opcode 0001xx): S → D, D → S */
        int to = (int)BITS(w, 16, 15);              /* 0 S, 1 D, 3 H */
        if ((to != 0 && to != 1) || to == type) return 0;
        fl_clobber(t);
        mx_clean(t);
        if (!dbl) {                                 /* S → D: exato fora NaN; FZ muda denormal */
            x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);
            slow_if(t, CC_NE);
            x_mov_rm(B, 0, XAX, VO(n));
            x_mov_rr(B, 0, XCX, XAX);
            x_shift_ri(B, 0, S_SHL, XCX, 1);
            x_alu_ri(B, 0, A_CMP, XR(XCX), (int32_t)0xFF000000);
            slow_if(t, CC_A);                       /* NaN (inf passa: exato) */
            x_movd_ld(B, 0, XR(XAX));
            xop2(B, XF_F3, 0x5A, 0, XR(0));         /* cvtss2sd */
            s_st(t, d, 0, 0);
            return 1;
        }
        /* D → S: arredonda como a aritmética: RN, FZ = 0, só PE, resultado normal e finito */
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 0x01C00000);
        slow_if(t, CC_NE);
        x_movq_ld(B, 0, VO(n));
        xop2(B, XF_F2, 0x5A, 0, XR(0));             /* cvtsd2ss */
        xop2(B, 0, 0xAE, 3, XM(XSP, 0));
        x_test8_mi(B, XM(XSP, 0), 0x1F);
        slow_if(t, CC_NE);
        x_movd_st(B, XR(XAX), 0);
        x_mov_rr(B, 0, XCX, XAX);
        x_shift_ri(B, 0, S_SHL, XCX, 1);
        a64_u8 *jz = x_jcc32(B, CC_E);
        x_shift_ri(B, 0, S_SHR, XCX, 24);
        x_alu_ri(B, 0, A_CMP, XR(XCX), 1);
        a64_u8 *jlo = x_jcc32(B, CC_BE);
        x_alu_ri(B, 0, A_CMP, XR(XCX), 0xFF);
        a64_u8 *jhi = x_jcc32(B, CC_E);
        a64_u8 *jok = x_jmp32(B);
        if (!t->b.ovf) { x_patch32(jlo, t->b.p); x_patch32(jhi, t->b.p); }
        emit_slow_interp(t);
        if (!t->b.ovf) { x_patch32(jz, t->b.p); x_patch32(jok, t->b.p); }
        ixc_if_pe(t);
        s_wsc_es(t, d, 0);
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * FP half (FEAT_FP16): FADD/FSUB/FMUL/FDIV escalar (H) e vetorial (4H, 8H). Converte para float
 * (vcvtph2ps, exato), opera em float e arredonda uma vez para half (vcvtps2ph, RN): como float
 * tem 24 bits >= 2*11+2, o arredondamento duplo é inócuo e o resultado é o corretamente
 * arredondado. IXC = PE da operação ou da conversão. Exige F16C, FPCR RN, FZ = FZ16 = 0, sem
 * flag além de PE e resultado (cada lane) normal e finito; senão interpretador.
 * ------------------------------------------------------------------------------------------- */
static void s_cvtsi_rax(T *t, int dbl, int uns);
static void vcvtph2ps_r(T *t, int x, int s) { xe8(B, 0xC4); xe8(B, 0xE2); xe8(B, 0x79); xe8(B, 0x13); xe8(B, 0xC0 | (x << 3) | s); }
static void vcvtps2ph_r(T *t, int x, int s) { xe8(B, 0xC4); xe8(B, 0xE3); xe8(B, 0x79); xe8(B, 0x1D); xe8(B, 0xC0 | (s << 3) | x); xe8(B, 0x00); }
/* lanes de 16 bits de xmm0: minúsculas (expoente <= 1, não zero) ou expoente máximo → lento */
static void s_lanecheck16(T *t, int q) {
    x_movdqa_ld(B, 1, XR(0));
    xop2(B, XF_66, 0x71, 6, XR(1)); xe8(B, 1);      /* psllw 1: sem sinal */
    s_zero(t, 2);
    x_movdqa_ld(B, 3, XR(1));
    s_op(t, 0x75, 3, 2);                            /* lane zero */
    xop2(B, XF_66, 0x71, 2, XR(1)); xe8(B, 11);     /* psrlw 11: expoente */
    s_const(t, 2, 0x0002000200020002ull);
    s_op(t, 0x65, 2, 1);                            /* 2 > e */
    s_op(t, 0xDF, 3, 2);
    s_const(t, 4, 0x001F001F001F001Full);
    s_op(t, 0x75, 4, 1);
    s_op(t, 0xEB, 3, 4);
    xop2(B, XF_66, 0xD7, XAX, XR(3));
    x_test_ri(B, 0, XR(XAX), q ? 0xFFFF : 0x00FF);
    slow_if(t, CC_NE);
}
/* op: 0x58 add, 0x5C sub, 0x59 mul, 0x5E div; lanes: 1 (escalar), 4 ou 8 */
static int s_f16(T *t, unsigned op, int lanes, int d, int n, int m) {
    if (!jit_g.f16c) return 0;
    fl_clobber(t);
    mx_clean(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 0x01C80000);   /* RMode, FZ, FZ16 */
    slow_if(t, CC_NE);
    for (int half = 0; half < (lanes == 8 ? 2 : 1); half++) {
        int r = half ? 5 : 0;                       /* resultado parcial: xmm0 (baixo) / xmm5 (alto) */
        if (lanes == 1) { x_movzx16(B, XAX, VO(n)); x_movd_ld(B, 1, XR(XAX)); x_movzx16(B, XAX, VO(m)); x_movd_ld(B, 2, XR(XAX)); }
        else { x_movq_ld(B, 1, VOE(n, 8 * half)); x_movq_ld(B, 2, VOE(m, 8 * half)); }
        vcvtph2ps_r(t, 1, 1);
        vcvtph2ps_r(t, 2, 2);
        if (op == 0x51) xop2(B, lanes == 1 ? XF_F3 : 0, 0x51, 1, XR(1));   /* FSQRT: sqrtss/sqrtps */
        else xop2(B, lanes == 1 ? XF_F3 : 0, op, 1, XR(2));  /* escalar: addss... só a lane 0; vetor: addps... */
        vcvtps2ph_r(t, r, 1);
    }
    if (lanes == 8) s_op(t, 0x6C, 0, 5);            /* junta as duas metades */
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));                /* stmxcsr */
    x_test8_mi(B, XM(XSP, 0), 0x1F);
    slow_if(t, CC_NE);
    if (lanes == 1) {                               /* só a lane 0 vale */
        x_movd_st(B, XR(XAX), 0);
        x_movzx16(B, XAX, XR(XAX));
        x_movd_ld(B, 0, XR(XAX));
    }
    s_lanecheck16(t, lanes == 8);
    ixc_if_pe(t);
    if (lanes == 1) { x_movd_st(B, XR(XAX), 0); s_wsc_rax(t, d); }
    else s_st(t, d, 0, lanes == 8);
    return 1;
}

static int s_fp(T *t, a64_u32 w) {
    int type = (int)BITS(w, 23, 22), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (BITS(w, 31, 29) != 0) return 0;             /* M = 0, S = 0 */
    if ((w & 0xFF201FE0) == 0x1E201000) {           /* FMOV (imediato) */
        int es = type == 0 ? 4 : type == 1 ? 8 : type == 3 ? 2 : 0;
        if (!es) return 0;
        x_mov_ri(B, XAX, a64_fp_expand_imm8((a64_u8)BITS(w, 20, 13), es));
        s_wsc_rax(t, d);
        return 1;
    }
    if ((w & 0xFFFFFC00) == 0x1EE1C000) return s_f16(t, 0x51, 1, d, n, n);   /* FSQRT de half */
    if (type == 3 && (w & 0xFF200C00) == 0x1E200800) {    /* FADD/FSUB/FMUL/FDIV de half */
        static const unsigned hop[4] = { 0x59, 0x5E, 0x58, 0x5C };
        unsigned o = BITS(w, 15, 12);
        return o < 4 ? s_f16(t, hop[o], 1, d, n, (int)BITS(w, 20, 16)) : 0;
    }
    if (type > 1) return 0;                         /* half: interpretador */
    int dbl = type == 1;
    int sqrt = 0, neg = 0, fma = 0;
    unsigned op;
    if ((w & 0xFF207C00) == 0x1E204000) {           /* 1 fonte: FABS / FNEG / FSQRT */
        unsigned opc = BITS(w, 20, 15);
        if (opc >= 8 && opc <= 12) return s_frint_sc(t, dbl, (int)opc, n, d);   /* FRINTN/P/M/Z/A */
        if (opc == 14 || opc == 15) return s_frint_sc(t, dbl, (int)opc, n, d);  /* FRINTX / FRINTI */
        if (opc == 1 || opc == 2) {                 /* FABS / FNEG: só o bit de sinal, sem exceção */
            fl_clobber(t);
            x_mov_rm(B, dbl, XAX, VO(n));
            if (dbl) { x_mov_ri(B, XCX, opc == 1 ? 0x7FFFFFFFFFFFFFFFull : 0x8000000000000000ull); x_alu_rr(B, 1, opc == 1 ? A_AND : A_XOR, XAX, XCX); }
            else x_alu_ri(B, 0, opc == 1 ? A_AND : A_XOR, XR(XAX), opc == 1 ? 0x7FFFFFFF : (int32_t)0x80000000);
            s_wsc_es(t, d, dbl);
            return 1;
        }
        if (opc != 3) return 0;
        sqrt = 1; op = 0x51;
    } else if ((w & 0xFF200C00) == 0x1E200800) {    /* 2 fontes */
        switch (BITS(w, 15, 12)) {
        case 0: op = 0x59; break;                   /* FMUL */
        case 1: op = 0x5E; break;                   /* FDIV */
        case 2: op = 0x58; break;                   /* FADD */
        case 3: op = 0x5C; break;                   /* FSUB */
        case 4: case 5: case 6: case 7:             /* FMAX FMIN FMAXNM FMINNM */
            return s_fminmax(t, dbl, (int)BITS(w, 15, 12), n, (int)BITS(w, 20, 16), d);
        case 8: op = 0x59; neg = 1; break;          /* FNMUL = -(n*m) */
        default: return 0;
        }
    } else if ((w & 0xFF000000) == 0x1F000000) {   /* FMADD FMSUB FNMADD FNMSUB (FMA3) */
        if (!jit_g.fma) return 0;
        /* vfmadd231 / vfnmadd231 / vfnmsub231 / vfmsub231: a+nm, a-nm, -a-nm, -a+nm (mesmo valor
         * exato que negar os operandos antes, como o ARM faz) */
        static const unsigned fop[4] = { 0xB9, 0xBD, 0xBF, 0xBB };
        op = fop[(BIT(w, 21) << 1) | BIT(w, 15)];
        fma = 1;
    } else return 0;
    int m = (int)BITS(w, 20, 16);
    int pf = dbl ? XF_F2 : XF_F3;
    fl_clobber(t);
    mx_clean(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 0x01C00000);           /* RMode != RN ou FZ */
    slow_if(t, CC_NE);
    if (fma) {
        int ra = (int)BITS(w, 14, 10);
        if (dbl) { x_movq_ld(B, 0, VO(ra)); x_movq_ld(B, 1, VO(n)); }
        else { x_movd_ld(B, 0, VO(ra)); x_movd_ld(B, 1, VO(n)); }
        vex_fma_m(t, op, dbl, 0, 1, OFF_V(m));
    } else {
        if (dbl) x_movq_ld(B, 0, VO(n)); else x_movd_ld(B, 0, VO(n));
        if (sqrt) xop2(B, pf, op, 0, XR(0));
        else xop2(B, pf, op, 0, VO(m));             /* operando de memória: 32/64 bits baixos */
    }
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));                /* stmxcsr [rsp] */
    x_test8_mi(B, XM(XSP, 0), 0x1F);                /* IE DE ZE OE UE */
    slow_if(t, CC_NE);
    if (dbl) xop2(B, XF_66 | XF_W, 0x7E, 0, XR(XAX)); else x_movd_st(B, XR(XAX), 0);
    /* expoente: 0 (com mantissa) ou 1 → minúsculo; máximo → inf/NaN */
    x_mov_rr(B, dbl, XCX, XAX);
    x_shift_ri(B, dbl, S_SHL, XCX, 1);              /* tira o sinal; zero → rápido */
    a64_u8 *jz = x_jcc32(B, CC_E);
    x_shift_ri(B, dbl, S_SHR, XCX, dbl ? 53 : 24);
    x_alu_ri(B, 0, A_CMP, XR(XCX), 1);
    a64_u8 *jlo = x_jcc32(B, CC_BE);
    x_alu_ri(B, 0, A_CMP, XR(XCX), dbl ? 0x7FF : 0xFF);
    a64_u8 *jhi = x_jcc32(B, CC_E);
    a64_u8 *jok = x_jmp32(B);
    if (!t->b.ovf) { x_patch32(jlo, t->b.p); x_patch32(jhi, t->b.p); }
    emit_slow_interp(t);
    if (!t->b.ovf) { x_patch32(jz, t->b.p); x_patch32(jok, t->b.p); }
    x_test8_mi(B, XM(XSP, 0), 0x20);                /* PE → IXC */
    a64_u8 *jnp = x_jcc8(B, CC_E);
    x_alu_ri(B, 0, A_OR, XM(RCPU, OFF_FPSR), 0x10);
    x_patch8(jnp, t->b.p);
    if (neg) {
        if (dbl) { x_mov_ri(B, XCX, 0x8000000000000000ull); x_alu_rr(B, 1, A_XOR, XAX, XCX); }
        else x_alu_ri(B, 0, A_XOR, XR(XAX), (int32_t)0x80000000);
    }
    s_wsc_es(t, d, dbl);
    return 1;
}

/* FP vetorial (4S, 2S, 2D): FADD FSUB FMUL FDIV FMLA FMLS, mesmo critério do escalar, lane a lane:
 * FPCR = RN e FZ = 0; nenhuma flag do x86 além de PE; toda lane do resultado normal e finita (ou
 * zero). Senão a instrução inteira vai ao interpretador. Usa xmm0..xmm4. */
static void vex_fma_pm(T *t, unsigned op, int dbl, int x, int v, int32_t disp) {   /* empacotado, L = 0 */
    vex_fma_m(t, op, dbl, x, v, disp);
}
/* resultado em xmm0: alguma lane minúscula (expoente <= 1, não zero), infinita ou NaN → lento */
static void s_lanecheck(T *t, int dbl, int q) {
    x_movdqa_ld(B, 1, XR(0));
    if (!dbl) {
        xop2(B, XF_66, 0x72, 6, XR(1)); xe8(B, 1);              /* pslld 1: sem sinal */
        s_zero(t, 2);
        x_movdqa_ld(B, 3, XR(1));
        s_op(t, 0x76, 3, 2);                                    /* xmm3 = lane zero */
        xop2(B, XF_66, 0x72, 2, XR(1)); xe8(B, 24);             /* psrld 24: expoente */
        s_const(t, 2, 0x0000000200000002ull);
        s_op(t, 0x66, 2, 1);                                    /* xmm2 = 2 > e */
        s_op(t, 0xDF, 3, 2);                                    /* xmm3 = ~zero & (e <= 1) */
        s_const(t, 4, 0x000000FF000000FFull);
        s_op(t, 0x76, 4, 1);                                    /* xmm4 = e == 0xFF */
    } else {
        xop2(B, XF_66, 0x73, 6, XR(1)); xe8(B, 1);              /* psllq 1 */
        s_zero(t, 2);
        x_movdqa_ld(B, 3, XR(1));
        s_op(t, 0x76, 3, 2);                                    /* dwords zero */
        xop2(B, XF_66, 0x70, 2, XR(3)); xe8(B, 0xB1);           /* pshufd: troca os dwords do qword */
        s_op(t, 0xDB, 3, 2);                                    /* xmm3 = qword zero */
        xop2(B, XF_66, 0x73, 2, XR(1)); xe8(B, 53);             /* psrlq 53: expoente no dword baixo */
        s_const(t, 2, 0x0000000000000002ull);
        s_op(t, 0x66, 2, 1);                                    /* 2 > e (vale no dword baixo) */
        s_op(t, 0xDF, 3, 2);
        s_const(t, 4, 0x00000000000007FFull);
        s_op(t, 0x76, 4, 1);
    }
    s_op(t, 0xEB, 3, 4);
    xop2(B, XF_66, 0xD7, XAX, XR(3));                           /* pmovmskb eax, xmm3 */
    x_test_ri(B, 0, XR(XAX), dbl ? 0x0F0F : (q ? 0xFFFF : 0x00FF));
    slow_if(t, CC_NE);
}
/* FMA3 empacotado com operandos em registrador: x = op(x, v, r) (todos < 8) */
static void vex_fma_r(T *t, unsigned op, int dbl, int x, int v, int r) {
    xe8(B, 0xC4); xe8(B, 0xE2);
    xe8(B, (dbl ? 0x80 : 0) | ((~v & 0xF) << 3) | 0x01);
    xe8(B, op);
    xe8(B, 0xC0 | (x << 3) | r);
}
/* núcleo do FP vetorial: x86 = opcode (58 add, 5C sub, 59 mul, 5E div; B8/BC fused), lane = -1 →
 * m vetorial, senão lane de m difundida (forma por elemento) */
static int fpv_core(T *t, unsigned x86, int fma, int dbl, int q, int d, int n, int m, int lane) {
    fl_clobber(t);
    mx_clean(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 0x01C00000);
    slow_if(t, CC_NE);
    int xm = fma ? 2 : 1;                           /* onde fica o operando m */
    if (lane < 0) { if (q) s_ld(t, xm, m); else x_movq_ld(B, xm, VO(m)); }
    else if (!dbl) { xop2(B, XF_66, 0x70, xm, VO(m)); xe8(B, (a64_u8)(lane * 0x55)); }   /* pshufd: difunde */
    else { x_movq_ld(B, xm, VOE(m, 8 * lane)); s_op(t, 0x6C, xm, xm); }
    if (fma) {
        if (q) { s_ld(t, 0, d); s_ld(t, 1, n); } else { x_movq_ld(B, 0, VO(d)); x_movq_ld(B, 1, VO(n)); }
        vex_fma_r(t, x86, dbl, 0, 1, 2);
    } else {
        if (q) s_ld(t, 0, n); else x_movq_ld(B, 0, VO(n));
        xop2(B, dbl ? XF_66 : 0, x86, 0, XR(1));
    }
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));                /* stmxcsr */
    x_test8_mi(B, XM(XSP, 0), 0x1F);
    slow_if(t, CC_NE);
    s_lanecheck(t, dbl, q);
    ixc_if_pe(t);
    s_st(t, d, 0, q);
    return 1;
}
static int s_fpvec(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), a = (int)BIT(w, 23), dbl = (int)BIT(w, 22), op = (int)BITS(w, 13, 11);
    int m = (int)BITS(w, 20, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (dbl && !q) return 0;
    if (!u && op == 2) return fpv_core(t, a ? 0x5C : 0x58, 0, dbl, q, d, n, m, -1);       /* FSUB / FADD */
    if (u && !a && op == 3) return fpv_core(t, 0x59, 0, dbl, q, d, n, m, -1);             /* FMUL */
    if (u && !a && op == 7) return q ? fpv_core(t, 0x5E, 0, dbl, q, d, n, m, -1) : 0;     /* FDIV (Q=0: 0/0 nas lanes zeradas) */
    if (!u && op == 1 && jit_g.fma) return fpv_core(t, a ? 0xBC : 0xB8, 1, dbl, q, d, n, m, -1);   /* FMLS / FMLA */
    if (u && !a && op == 2) {                       /* FADDP: haddps/haddpd (SSE3) dão a mesma ordem */
        if (!q) return 0;
        fl_clobber(t);
        mx_clean(t);
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 0x01C00000);
        slow_if(t, CC_NE);
        s_ld(t, 0, n); s_ld(t, 1, m);
        xop2(B, dbl ? XF_66 : XF_F2, 0x7C, 0, XR(1));
        xop2(B, 0, 0xAE, 3, XM(XSP, 0));
        x_test8_mi(B, XM(XSP, 0), 0x1F);
        slow_if(t, CC_NE);
        s_lanecheck(t, dbl, 1);
        ixc_if_pe(t);
        s_st(t, d, 0, 1);
        return 1;
    }
    return 0;
}

/* AdvSIMD por elemento: MUL/MLA/MLS (H, S) e FMUL/FMLA/FMLS (S, D) */
static int s_indexed(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), size = (int)BITS(w, 23, 22), L = (int)BIT(w, 21), M = (int)BIT(w, 20);
    int rm = (int)BITS(w, 19, 16), op = (int)BITS(w, 15, 12), H = (int)BIT(w, 11), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (!u && (op == 9 || op == 1 || op == 5)) {    /* FMUL FMLA FMLS por elemento */
        if (size < 2) return 0;                     /* half: interpretador */
        int dbl = size == 3;
        if (dbl && (L || !q)) return 0;
        int lane = dbl ? H : (H << 1) | L, m = (M << 4) | rm;
        if (op == 9) return fpv_core(t, 0x59, 0, dbl, q, d, n, m, lane);
        if (!jit_g.fma) return 0;
        return fpv_core(t, op == 5 ? 0xBC : 0xB8, 1, dbl, q, d, n, m, lane);
    }
    int key = (u << 4) | op;
    if (key != 0x08 && key != 0x10 && key != 0x14) return 0;   /* MUL MLA MLS */
    if (size != 1 && size != 2) return 0;
    if (size == 2 && !jit_g.sse41) return 0;
    int idx = size == 1 ? (H << 2) | (L << 1) | M : (H << 1) | L, m = size == 1 ? rm : (M << 4) | rm;
    fl_clobber(t);
    if (size == 1) x_movzx16(B, XAX, VOE(m, 2 * idx)); else x_mov_rm(B, 0, XAX, VOE(m, 4 * idx));
    s_replicate(t, size);
    xop2(B, XF_66 | XF_W, 0x6E, 1, XR(XAX));
    s_op(t, 0x6C, 1, 1);                            /* xmm1 = elemento em todas as lanes */
    s_ld(t, 0, n);
    if (size == 1) s_op(t, 0xD5, 0, 1); else s_op38(t, 0x40, 0, 1);   /* pmullw / pmulld */
    if (key != 0x08) {
        s_ld(t, 2, d);
        unsigned add = size == 1 ? 0xFD : 0xFE, sub = size == 1 ? 0xF9 : 0xFA;
        if (key == 0x10) { s_op(t, add, 2, 0); x_movdqa_ld(B, 0, XR(2)); }
        else { s_op(t, sub, 2, 0); x_movdqa_ld(B, 0, XR(2)); }
    }
    s_st(t, d, 0, q);
    return 1;
}

/* 3diff: (S|U)ADDL/ADDW/SUBL/SUBW, (S|U)MULL, (S|U)MLAL, (S|U)MLSL; "2" = metade de cima */
static void s_widen(T *t, int x, int v, int hi, int sz, int sgn) {   /* metade de V[v] alargada em x (usa xmm5) */
    static const unsigned lo[3] = { 0x60, 0x61, 0x62 };
    x_movq_ld(B, x, hi ? VOE(v, 8) : VO(v));
    s_zero(t, 5);
    if (sgn) s_cmp(t, 0, sz, 5, x);                 /* xmm5 = 0 > x */
    s_op(t, lo[sz], x, 5);
}
static int s_3diff(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), size = (int)BITS(w, 23, 22), op = (int)BITS(w, 15, 12);
    int m = (int)BITS(w, 20, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (op == 14 && size == 3 && !u) return s_pmull64(t, q, n, m, d);   /* PMULL/PMULL2 de 64 bits */
    if (size == 3) return 0;
    static const unsigned add[3] = { 0xFD, 0xFE, 0xD4 }, sub[3] = { 0xF9, 0xFA, 0xFB };
    int sgn = !u;
    switch (op) {
    case 0: case 1: case 2: case 3:                 /* ADDL ADDW SUBL SUBW */
        if (op & 1) s_ld(t, 0, n); else s_widen(t, 0, n, q, size, sgn);
        s_widen(t, 1, m, q, size, sgn);
        s_op(t, (op & 2) ? sub[size] : add[size], 0, 1);
        s_st(t, d, 0, 1);
        return 1;
    case 4: case 6:                                 /* ADDHN RADDHN / SUBHN RSUBHN */
        return s_addhn(t, q, u, size, op == 6, n, m, d);
    case 8: case 10: case 12:                       /* MLAL MLSL MULL */
        if (size == 1 && !jit_g.sse41) return 0;
        if (size == 2 && sgn && !jit_g.sse41) return 0;
        s_widen(t, 0, n, q, size, sgn);
        s_widen(t, 1, m, q, size, sgn);
        if (size == 0) s_op(t, 0xD5, 0, 1);         /* pmullw: 8x8 → 16 */
        else if (size == 1) s_op38(t, 0x40, 0, 1);  /* pmulld: 16x16 → 32 */
        else if (sgn) s_op38(t, 0x28, 0, 1);        /* pmuldq: 32x32 → 64 com sinal */
        else s_op(t, 0xF4, 0, 1);                   /* pmuludq */
        if (op != 12) {
            s_ld(t, 2, d);
            s_op(t, op == 8 ? add[size] : sub[size], 2, 0);
            x_movdqa_ld(B, 0, XR(2));
        }
        s_st(t, d, 0, 1);
        return 1;
    }
    return 0;
}

/* ADDP (vetorial) e ADDP escalar (Dd = soma das duas lanes de 64) */
static int s_addp(T *t, int q, int size, int n, int m, int d) {
    if (size == 3 && !q) return 0;
    if (size == 3) {
        s_ld(t, 0, n); s_ld(t, 1, m);
        x_movdqa_ld(B, 2, XR(0));
        s_op(t, 0x6C, 0, 1);                        /* [n0, m0] */
        s_op(t, 0x6D, 2, 1);                        /* [n1, m1] */
        s_op(t, 0xD4, 0, 2);
        s_st(t, d, 0, 1);
        return 1;
    }
    if (!jit_g.ssse3) return 0;
    if (q) { s_ld(t, 0, n); s_ld(t, 1, m); }
    else { x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 1, VO(m)); s_op(t, 0x6C, 0, 1); s_zero(t, 1); }   /* n_lo:m_lo */
    if (size == 1) s_op38(t, 0x01, 0, 1);           /* phaddw */
    else if (size == 2) s_op38(t, 0x02, 0, 1);      /* phaddd */
    else {                                          /* bytes: pares (par + ímpar) & 0xFF, empacotados */
        s_const(t, 4, 0x00FF00FF00FF00FFull);
        x_movdqa_ld(B, 2, XR(0)); x_movdqa_ld(B, 3, XR(1));
        xop2(B, XF_66, 0x71, 2, XR(0)); xe8(B, 8);  /* psrlw 8: ímpares */
        xop2(B, XF_66, 0x71, 2, XR(1)); xe8(B, 8);
        s_op(t, 0xDB, 2, 4); s_op(t, 0xDB, 3, 4);   /* pares */
        s_op(t, 0xFD, 0, 2); s_op(t, 0xFD, 1, 3);   /* somas de 16 bits */
        s_op(t, 0xDB, 0, 4); s_op(t, 0xDB, 1, 4);
        s_op(t, 0x67, 0, 1);                        /* packuswb (valores <= 0xFF) */
    }
    s_st(t, d, 0, q);
    return 1;
}

/* LD1/ST1 de uma lane e LD1R (uma estrutura, selem = 1), com e sem pós-índice: um acesso só */
static int s_ldst_single(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), L = (int)BIT(w, 22), R = (int)BIT(w, 21), post = (int)BIT(w, 23);
    int rm = (int)BITS(w, 20, 16), opcode = (int)BITS(w, 15, 13), S = (int)BIT(w, 12), size = (int)BITS(w, 11, 10);
    int rn = (int)BITS(w, 9, 5), rt = (int)(w & 31);
    if (!post && rm) return 0;
    int selem = (((opcode & 1) << 1) | R) + 1;      /* LD1..LD4 de uma lane / LD1R..LD4R */
    int scale = opcode >> 1, idx = 0, rep = 0;
    switch (scale) {
    case 3: if (!L || S) return 0; scale = size; rep = 1; break;
    case 0: idx = (q << 3) | (S << 2) | size; break;
    case 1: if (size & 1) return 0; idx = (q << 2) | (S << 1) | (size >> 1); break;
    default:
        if (size & 2) return 0;
        if (!(size & 1)) idx = (q << 1) | S;
        else { if (S) return 0; idx = q; scale = 3; }
        break;
    }
    int eb = 1 << scale;
    int hb = rget(t, rn);
    if (selem > 1) {                                /* vários acessos: tudo numa página, senão interp */
        fl_clobber_force(t);
        x_mov_rr(B, 0, XAX, hb);
        x_alu_ri(B, 0, A_AND, XR(XAX), 0xFFF);
        x_alu_ri(B, 0, A_CMP, XR(XAX), 0x1000 - selem * eb);
        slow_if(t, CC_A);
    }
    if (!L) { xmon_pre(t, XM(hb, 0), selem * eb); hb = rget(t, rn); }
    for (int s = 0; s < selem; s++) {
        xo mem = XM(hb, s * eb);
        int v = (rt + s) & 31;
        if (L) {
            if (!s) fp_mark(t);
            if (scale == 0) x_movzx8(B, XDX, mem); else if (scale == 1) x_movzx16(B, XDX, mem); else x_mov_rm(B, scale == 3, XDX, mem);
            if (rep) {
                fl_clobber(t);
                x_mov_rr(B, 1, XAX, XDX);
                s_replicate(t, scale);
                s_vstore64(t, v, q);
            } else {
                xo vv = VOE(v, idx << scale);
                if (scale == 0) x_mov8_mr(B, vv, XDX); else if (scale == 1) x_mov16_mr(B, vv, XDX); else x_mov_mr(B, scale == 3, vv, XDX);
            }
        } else {
            xo vv = VOE(v, idx << scale);
            if (scale == 0) x_movzx8(B, XDX, vv); else if (scale == 1) x_movzx16(B, XDX, vv); else x_mov_rm(B, scale == 3, XDX, vv);
            if (!s) fp_mark(t);
            if (scale == 0) x_mov8_mr(B, mem, XDX); else if (scale == 1) x_mov16_mr(B, mem, XDX); else x_mov_mr(B, scale == 3, mem, XDX);
        }
    }
    if (!L) xmon_post(t);
    if (post) {
        if (rm == 31) wb_base(t, rn, selem * eb);
        else { int hm = rget(t, rm); int h = rget(t, rn); rdef(t, rn); x_lea(B, 1, h, XMI(h, hm, 0, 0)); }
    }
    return 1;
}

/* criptografia: AES (AES-NI), PMULL de 64 bits (PCLMULQDQ), EOR3/BCAX/RAX1/XAR (SSE2).
 * AESE = aesenclast(d ^ n, 0); AESD = aesdeclast(d ^ n, 0); AESIMC = aesimc;
 * AESMC = aesenc(aesdeclast(x, 0), 0): aesdeclast desfaz ShiftRows e SubBytes, aesenc os refaz
 * (comutam: permutação de bytes × mapa byte a byte) e aplica MixColumns; a chave zero não muda nada.
 * SHA1/SHA256/SHA512 ficam no interpretador (SHA-NI divide a rodada de outro jeito). */
/* SHA1H: o dword baixo gira 30; o resto do vetor zera. SHA-NI não faz isso. */
static int s_sha1h(T *t, a64_u32 w) {
    if ((w & 0xFFFFFC00) != 0x5E280800) return 0;
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    fl_clobber(t);
    x_mov_rm(B, 0, XAX, VO(n));
    x_shift_ri(B, 0, S_ROL, XAX, 30);
    s_wsc_rax(t, d);
    return 1;
}
static int s_aes(T *t, a64_u32 w) {
    int op = (int)BITS(w, 16, 12), n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    if (!jit_g.aes || op < 4 || op > 7) return 0;
    if (op == 7) { xop3(B, XF_66, 0x38, 0xDB, 0, VO(n)); s_st(t, d, 0, 1); return 1; }   /* AESIMC */
    s_ld(t, 0, n);
    s_zero(t, 1);
    if (op == 6) { s_op38(t, 0xDF, 0, 1); s_op38(t, 0xDC, 0, 1); }    /* AESMC */
    else {
        s_ld(t, 2, d);
        s_op(t, 0xEF, 0, 2);
        s_op38(t, op == 4 ? 0xDD : 0xDF, 0, 1);    /* aesenclast / aesdeclast */
    }
    s_st(t, d, 0, 1);
    return 1;
}
static int s_pmull64(T *t, int q, int n, int m, int d) {
    if (!jit_g.pclmul) return 0;
    s_ld(t, 0, n);
    xop3(B, XF_66, 0x3A, 0x44, 0, VO(m));
    xe8(B, q ? 0x11 : 0x00);                        /* pclmulqdq: metade de baixo / de cima */
    s_st(t, d, 0, 1);
    return 1;
}
static void s_rol64(T *t, int x, int r) {          /* x = rol(x, r) em cada lane de 64 (usa xmm4) */
    r &= 63;
    if (!r) return;
    x_movdqa_ld(B, 4, XR(x));
    xop2(B, XF_66, 0x73, 6, XR(x)); xe8(B, (a64_u8)r);          /* psllq */
    xop2(B, XF_66, 0x73, 2, XR(4)); xe8(B, (a64_u8)(64 - r));   /* psrlq */
    s_op(t, 0xEB, x, 4);
}
static int s_crypto_c(T *t, a64_u32 w) {           /* op0 = 1100 */
    if (BITS(w, 31, 24) != 0xCE) return 0;
    int m = (int)BITS(w, 20, 16), n = (int)BITS(w, 9, 5), d = (int)(w & 31), ra = (int)BITS(w, 14, 10);
    switch (BITS(w, 23, 21)) {
    case 0: case 1:                                 /* EOR3 / BCAX */
        if (BIT(w, 15)) return 0;
        s_ld(t, 0, n); s_ld(t, 1, m); s_ld(t, 2, ra);
        if (BIT(w, 21)) { s_op(t, 0xDF, 2, 1); s_op(t, 0xEF, 0, 2); }   /* n ^ (m & ~a) */
        else { s_op(t, 0xEF, 0, 1); s_op(t, 0xEF, 0, 2); }
        s_st(t, d, 0, 1);
        return 1;
    case 3:                                         /* RAX1 (os SHA512 vão ao interpretador) */
        if (!BIT(w, 15) || BIT(w, 14) || BITS(w, 13, 12) || BITS(w, 11, 10) != 3) return 0;
        s_ld(t, 1, m);
        s_rol64(t, 1, 1);
        s_ld(t, 0, n);
        s_op(t, 0xEF, 0, 1);
        s_st(t, d, 0, 1);
        return 1;
    case 4:                                         /* XAR: ror(n ^ m, imm6) */
        s_ld(t, 0, n); s_ld(t, 1, m);
        s_op(t, 0xEF, 0, 1);
        s_rol64(t, 0, 64 - (int)BITS(w, 15, 10));
        s_st(t, d, 0, 1);
        return 1;
    }
    return 0;
}

/* MXCSR de volta a 0x1F80. O PE que o x86 acumula não pode vazar para a instrução seguinte
 * quando o ARM não levanta IXC (FRINTN/M/P/Z/A/I, e o +0,5 do FCVTAS). */
static void mx_reset(T *t) {
    x_mov_mi(B, 0, XM(XSP, 4), 0x1F80);
    xop2(B, 0, 0xAE, 2, XM(XSP, 4));
    t->mxclean = 1;
}

/* cvtsi2ss/sd de rax. uns: o bit 63 pode estar ligado; (n>>1)+(n>>1) é o dobro exato,
 * porque acima de 2^63 o ulp é pelo menos 2048 em f64 e muito maior em f32. */
static void s_cvtsi_rax(T *t, int dbl, int uns) {
    int pf = (dbl ? XF_F2 : XF_F3) | XF_W;
    if (!uns) { xop2(B, pf, 0x2A, 0, XR(XAX)); return; }
    x_test_rr(B, 1, XAX, XAX);
    a64_u8 *jp = x_jcc32(B, CC_NS);
    /* bit 0 perdido no >>1: o dobro de n>>1 acerta o valor (ulp >= 2048) mas é inexato */
    x_test_ri(B, 0, XR(XAX), 1);
    x_setcc(B, CC_NE, XCX);
    x_mov8_mr(B, XM(XSP, 0), XCX);
    x_shift_ri(B, 1, S_SHR, XAX, 1);
    xop2(B, pf, 0x2A, 0, XR(XAX));
    xop2(B, dbl ? XF_F2 : XF_F3, 0x58, 0, XR(0));
    x_test8_mi(B, XM(XSP, 0), 0xFF);
    a64_u8 *jz = x_jcc8(B, CC_E);
    xop2(B, 0, 0xAE, 3, XM(XSP, 4));
    x_alu_ri(B, 0, A_OR, XM(XSP, 4), 0x20);
    xop2(B, 0, 0xAE, 2, XM(XSP, 4));
    if (!t->b.ovf) x_patch8(jz, t->b.p);
    a64_u8 *jd = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jp, t->b.p);
    xop2(B, pf, 0x2A, 0, XR(XAX));
    if (!t->b.ovf) x_patch32(jd, t->b.p);
}

/* RC do MXCSR: 0 par, 1 chão, 2 teto, 3 trunca. Alguns hosts ignoram o imediato do
 * round* e usam só o MXCSR; outros honram o imediato e ignoram o MXCSR. Os dois
 * ficam com o mesmo modo, então o resultado coincide. mxclean deixa de valer. */
static void mx_set_rc(T *t, int mode) {
    x_mov_mi(B, 0, XM(XSP, 4), (int32_t)(0x1F80u | ((a64_u32)(mode & 3) << 13)));
    xop2(B, 0, 0xAE, 2, XM(XSP, 4));
    t->mxclean = 0;
}

/* kind: 0 roundss, 1 roundsd, 2 roundps, 3 roundpd. mode: 0 par, 1 chão, 2 teto, 3 trunca. */
static void s_round_packed(T *t, int kind, int dst, int src, int mode) {
    static const unsigned op[4] = { 0x0A, 0x0B, 0x08, 0x09 };
    mx_set_rc(t, mode);
    xop3(B, XF_66, 0x3A, op[kind], dst, XR(src));
    xe8(B, (a64_u8)(4 | (mode & 3)));
}

static void s_round_fpcr(T *t, int kind) {
    x_mov_rm(B, 0, XAX, XM(RCPU, OFF_FPCR));
    x_shift_ri(B, 0, S_SHR, XAX, 22);
    x_alu_ri(B, 0, A_AND, XR(XAX), 3);
    x_alu_ri(B, 0, A_CMP, XR(XAX), 1);
    a64_u8 *jp = x_jcc32(B, CC_E);
    x_alu_ri(B, 0, A_CMP, XR(XAX), 2);
    a64_u8 *jm = x_jcc32(B, CC_E);
    x_alu_ri(B, 0, A_CMP, XR(XAX), 3);
    a64_u8 *jz = x_jcc32(B, CC_E);
    s_round_packed(t, kind, 0, 0, 0);
    a64_u8 *d0 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jp, t->b.p);
    s_round_packed(t, kind, 0, 0, 2);
    a64_u8 *d1 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jm, t->b.p);
    s_round_packed(t, kind, 0, 0, 1);
    a64_u8 *d2 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jz, t->b.p);
    s_round_packed(t, kind, 0, 0, 3);
    if (!t->b.ovf) { x_patch32(d0, t->b.p); x_patch32(d1, t->b.p); x_patch32(d2, t->b.p); }
}

/* lane idx do xmm `src` em rax, zero-estendido. Usa xmm0 se idx != 0. */
static void s_xmm_lane(T *t, int src, int idx, int size) {
    int bytes = idx << size;
    if (bytes) { x_movdqa_ld(B, 0, XR(src)); s_shiftdq(t, 0, 0, bytes); src = 0; }
    if (size == 3) xop2(B, XF_66 | XF_W, 0x7E, src, XR(XAX));
    else if (size == 2) x_movd_st(B, XR(XAX), src);
    else if (size == 1) { x_movd_st(B, XR(XAX), src); x_movzx16(B, XAX, XR(XAX)); }
    else { x_movd_st(B, XR(XAX), src); x_movzx8(B, XAX, XR(XAX)); }
}

/* rax = valor (já estendido), rcx = contagem com sinal de 8 bits. Resultado mascarado em rax. */
static void s_apply_shl(T *t, int eb, int uns) {
    x_test_rr(B, 1, XCX, XCX);
    a64_u8 *jneg = x_jcc32(B, CC_S);
    x_alu_ri(B, 1, A_CMP, XR(XCX), eb);
    a64_u8 *jhi = x_jcc32(B, CC_AE);
    x_shift_cl(B, 1, S_SHL, XAX);
    a64_u8 *j1 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jhi, t->b.p);
    x_alu_rr(B, 1, A_XOR, XAX, XAX);
    a64_u8 *j2 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jneg, t->b.p);
    x_f7(B, 1, 3, XR(XCX));
    x_alu_ri(B, 1, A_CMP, XR(XCX), eb);
    a64_u8 *jge = x_jcc32(B, CC_AE);
    x_shift_cl(B, 1, uns ? S_SHR : S_SAR, XAX);
    a64_u8 *j3 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jge, t->b.p);
    if (uns) x_alu_rr(B, 1, A_XOR, XAX, XAX);
    else x_shift_ri(B, 1, S_SAR, XAX, 63);
    if (!t->b.ovf) { x_patch32(j1, t->b.p); x_patch32(j2, t->b.p); x_patch32(j3, t->b.p); }
    if (eb == 32) x_mov_rr(B, 0, XAX, XAX);
    else if (eb < 32) x_alu_ri(B, 1, A_AND, XR(XAX), eb == 8 ? 0xFF : 0xFFFF);
}

static void s_store_lane(T *t, int d, int size, int idx) {
    int off = idx << size;
    if (size >= 2) x_mov_mr(B, size == 3, VOE(d, off), XAX);
    else if (size == 1) x_mov16_mr(B, VOE(d, off), XAX);
    else x_mov8_mr(B, VOE(d, off), XAX);
}

/* SSHL/USHL. A contagem é o byte baixo da lane, com sinal. À direita, USHL é lógico e SSHL aritmético.
 * Contagem >= largura zera (ou preenche com o sinal, no deslocamento aritmético). */
static int s_shlreg(T *t, int q, int u, int size, int n, int m, int d, int scalar) {
    if (scalar) { if (size != 3) return 0; }
    else if (size == 3 && !q) return 0;
    int eb = 8 << size, ne = scalar ? 1 : (q ? 128 : 64) / eb;
    fl_clobber(t);
    if (scalar) { x_movq_ld(B, 2, VO(n)); x_movq_ld(B, 3, VO(m)); }
    else { s_ld(t, 2, n); s_ld(t, 3, m); }
    for (int e = 0; e < ne; e++) {
        s_xmm_lane(t, 2, e, size);
        if (!u && eb < 64) {
            if (eb == 32) x_movsxd(B, XAX, XR(XAX));
            else if (eb == 16) x_movsx16(B, 1, XAX, XR(XAX));
            else x_movsx8(B, 1, XAX, XR(XAX));
        }
        x_mov_rr(B, 1, XDX, XAX);
        s_xmm_lane(t, 3, e, size);
        x_movsx8(B, 1, XCX, XR(XAX));
        x_mov_rr(B, 1, XAX, XDX);
        s_apply_shl(t, eb, u);
        s_store_lane(t, d, size, e);
    }
    if (scalar || !q) x_mov_mi(B, 1, VOE(d, 8), 0);
    return 1;
}

static void s_pminmax(T *t, int dst, int src, int u, int mn, int size) {
    int sse2 = (size == 1 && !u) || (size == 0 && u);
    unsigned opc = 0;
    if (!u && size == 0) opc = mn ? 0x38 : 0x3C;
    else if (!u && size == 1) opc = mn ? 0xEA : 0xEE;
    else if (!u && size == 2) opc = mn ? 0x39 : 0x3D;
    else if (u && size == 0) opc = mn ? 0xDA : 0xDE;
    else if (u && size == 1) opc = mn ? 0x3A : 0x3E;
    else opc = mn ? 0x3B : 0x3F;
    if (sse2) s_op(t, opc, dst, src); else s_op38(t, opc, dst, src);
}

/* reduz pares adjacentes de xmm x para a metade baixa. Usa xmm1, xmm2 e xmm5. */
static void s_pair_reduce(T *t, int x, int u, int mn, int size) {
    if (size == 2) {
        xop2(B, XF_66, 0x70, 1, XR(x)); xe8(B, 0xB1);
        s_pminmax(t, x, 1, u, mn, size);
        xop2(B, XF_66, 0x70, x, XR(x)); xe8(B, 0x08);
        return;
    }
    x_movdqa_ld(B, 1, XR(x));
    if (size == 0) s_const2(t, 2, 0x0607040502030001ull, 0x0E0F0C0D0A0B0809ull);
    else s_const2(t, 2, 0x0504070601000302ull, 0x0D0C0F0E09080B0Aull);
    s_pshufb(t, 1, 2);
    s_pminmax(t, x, 1, u, mn, size);
    if (size == 0) s_const2(t, 2, 0x0E0C0A0806040200ull, 0x8080808080808080ull);
    else s_const2(t, 2, 0x0D0C090805040100ull, 0x8080808080808080ull);
    s_pshufb(t, x, 2);
}

/* SMAXP/UMAXP/SMINP/UMINP sobre concat(Vm:Vn). Q=0 usa só os 64 bits baixos de cada fonte. */
static int s_pairwise_mm(T *t, int q, int u, int size, int mn, int n, int m, int d) {
    int sse2 = (size == 1 && !u) || (size == 0 && u);
    if (size < 2 && !jit_g.ssse3) return 0;
    if (!sse2 && !jit_g.sse41) return 0;
    if (q) {
        s_ld(t, 0, n);
        s_pair_reduce(t, 0, u, mn, size);
        x_movdqa_ld(B, 4, XR(0));
        s_ld(t, 0, m);
        s_pair_reduce(t, 0, u, mn, size);
        s_op(t, 0x6C, 4, 0);
        s_st(t, d, 4, 1);
    } else {
        x_movq_ld(B, 0, VO(n));
        x_movq_ld(B, 1, VO(m));
        s_op(t, 0x6C, 0, 1);
        s_pair_reduce(t, 0, u, mn, size);
        s_st(t, d, 0, 0);
    }
    return 1;
}

static int s_rev(T *t, int q, int u, int op, int size, int n, int d) {
    if (!jit_g.ssse3) return 0;
    int eb = 8 << size, cb = op == 1 ? 16 : u ? 32 : 64;
    if (size > 2 || eb >= cb) return 0;
    a64_u64 lo, hi;
    if (cb == 16) { lo = 0x0607040502030001ull; hi = 0x0E0F0C0D0A0B0809ull; }
    else if (cb == 32 && eb == 8) { lo = 0x0405060700010203ull; hi = 0x0C0D0E0F08090A0Bull; }
    else if (cb == 32) { lo = 0x0504070601000302ull; hi = 0x0D0C0F0E09080B0Aull; }
    else if (eb == 8) { lo = 0x0001020304050607ull; hi = 0x08090A0B0C0D0E0Full; }
    else if (eb == 16) { lo = 0x0100030205040706ull; hi = 0x09080B0A0D0C0F0Eull; }
    else { lo = 0x0302010007060504ull; hi = 0x0B0A09080F0E0D0Cull; }
    s_ld(t, 0, n);
    s_const2(t, 1, lo, hi);
    s_pshufb(t, 0, 1);
    s_st(t, d, 0, q);
    return 1;
}

static int s_cnt(T *t, int q, int n, int d) {
    if (!jit_g.ssse3) return 0;
    s_ld(t, 0, n);
    s_const(t, 4, 0x0F0F0F0F0F0F0F0Full);
    x_movdqa_ld(B, 1, XR(0));
    s_op(t, 0xDB, 1, 4);
    xop2(B, XF_66, 0x71, 2, XR(0)); xe8(B, 4);
    s_op(t, 0xDB, 0, 4);
    s_const2(t, 2, 0x0302020102010100ull, 0x0403030203020201ull);
    x_movdqa_ld(B, 3, XR(2));
    s_pshufb(t, 2, 1);
    s_pshufb(t, 3, 0);
    s_op(t, 0xFC, 2, 3);
    s_st(t, d, 2, q);
    return 1;
}

/* ABS inteiro com wrap: 0x80 e INT64_MIN permanecem. pabsb/w/d no SSSE3; 64 bits na máscara de sinal. */
static int s_iabs(T *t, int q, int size, int n, int d, int scalar) {
    if (scalar) { if (size != 3) return 0; q = 0; }
    else if (size == 3 && !q) return 0;
    if (size < 3 && !jit_g.ssse3) return 0;
    if (scalar) x_movq_ld(B, 0, VO(n)); else s_ld(t, 0, n);
    if (size < 3) s_op38(t, 0x1C + (unsigned)size, 0, 0);
    else {
        x_movdqa_ld(B, 1, XR(0));
        s_signmask(t, 1, 64);
        s_op(t, 0xEF, 0, 1);
        s_op(t, 0xFB, 0, 1);
    }
    s_st(t, d, 0, q);
    return 1;
}

/* SCVTF/UCVTF de mesma largura (escalar SIMD e vetor). Só RN; u64 com bit 63 pelo dobro de n>>1. */
static int s_scvtf(T *t, int q, int u, int size, int n, int d, int scalar) {
    if (size & 2) return 0;
    int dbl = size & 1;
    if (!scalar && dbl && !q) return 0;
    int ne = scalar ? 1 : (q ? 128 : 64) / (dbl ? 64 : 32);
    fl_clobber(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 3u << 22);
    slow_if(t, CC_NE);
    mx_clean(t);
    for (int e = 0; e < ne; e++) {
        s_elem(t, n, e, dbl ? 3 : 2);
        if (!u && !dbl) x_movsxd(B, XAX, XR(XAX));
        s_zero(t, 0);
        s_cvtsi_rax(t, dbl, u && dbl);
        if (dbl) x_movq_st(B, VOE(d, e * 8), 0);
        else x_movd_st(B, VOE(d, e * 4), 0);
    }
    if (scalar) { if (!dbl) x_mov_mi(B, 0, VOE(d, 4), 0); x_mov_mi(B, 1, VOE(d, 8), 0); }
    else if (!q) x_mov_mi(B, 1, VOE(d, 8), 0);
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));
    ixc_if_pe(t);
    return 1;
}

/* mode: 0 par, 1 chão, 2 teto, 3 trunca, -1 = FPCR. FRINTN/M/P/Z/I não levantam IXC. */
static int s_frint_vec(T *t, int q, int size, int mode, int n, int d) {
    if (!jit_g.sse41) return 0;
    int dbl = size & 1;
    if (dbl && !q) return 0;
    int kind = dbl ? 3 : 2;
    fl_clobber(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);
    slow_if(t, CC_NE);
    mx_clean(t);
    if (q) s_ld(t, 0, n); else x_movq_ld(B, 0, VO(n));
    if (mode < 0) s_round_fpcr(t, kind);
    else s_round_packed(t, kind, 0, 0, mode);
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));
    x_test8_mi(B, XM(XSP, 0), 0x1F);
    slow_if(t, CC_NE);
    s_lanecheck(t, dbl, q);
    s_st(t, d, 0, q);
    mx_reset(t);
    return 1;
}

static int s_fabsneg_vec(T *t, int q, int size, int neg, int n, int d) {
    if ((size & 1) && !q) return 0;
    int dbl = size & 1;
    s_ld(t, 0, n);
    if (neg) { s_const(t, 1, dbl ? 0x8000000000000000ull : 0x8000000080000000ull); s_op(t, 0xEF, 0, 1); }
    else { s_const(t, 1, dbl ? 0x7FFFFFFFFFFFFFFFull : 0x7FFFFFFF7FFFFFFFull); s_op(t, 0xDB, 0, 1); }
    s_st(t, d, 0, q);
    return 1;
}

/* FMAX/FMIN/FMAXNM/FMINNM numéricos. Qualquer NaN, ou FZ, vai ao interpretador.
 * Iguais: MAX faz AND (o +0 vence) e MIN faz OR (o -0 vence). */
static int s_fminmax(T *t, int dbl, int which, int n, int m, int d) {
    int ismin = which & 1;
    fl_clobber(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);
    slow_if(t, CC_NE);
    if (dbl) { x_movq_ld(B, 0, VO(n)); x_movq_ld(B, 1, VO(m)); }
    else { x_movd_ld(B, 0, VO(n)); x_movd_ld(B, 1, VO(m)); }
    xop2(B, dbl ? XF_66 : 0, 0x2E, 0, XR(1));
    slow_if(t, CC_P);
    a64_u8 *jlt = x_jcc32(B, CC_B);
    a64_u8 *jeq = x_jcc32(B, CC_E);
    x_mov_rm(B, dbl, XAX, VO(ismin ? m : n));
    a64_u8 *jd1 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jlt, t->b.p);
    x_mov_rm(B, dbl, XAX, VO(ismin ? n : m));
    a64_u8 *jd2 = x_jmp32(B);
    if (!t->b.ovf) x_patch32(jeq, t->b.p);
    x_mov_rm(B, dbl, XAX, VO(n));
    x_mov_rm(B, dbl, XCX, VO(m));
    x_alu_rr(B, dbl, ismin ? A_OR : A_AND, XAX, XCX);
    if (!t->b.ovf) { x_patch32(jd1, t->b.p); x_patch32(jd2, t->b.p); }
    s_wsc_es(t, d, dbl);
    return 1;
}

/* FRINT escalar. opc 8..11 = N/P/M/Z, 12 = A, 14 = X (IXC), 15 = I (FPCR, sem IXC). */
static int s_frint_sc(T *t, int dbl, int opc, int n, int d) {
    /* FRINTA: somar 0,5 e arredondar erra 2^52+1 / 2^23+1 e o zero com sinal. Interpretador. */
    if (opc == 12) return 0;
    if (!jit_g.sse41) return 0;
    static const int mode[4] = { 0, 2, 1, 3 };      /* N P M Z → par, teto, chão, trunca */
    int kind = dbl ? 1 : 0;
    fl_clobber(t);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);
    slow_if(t, CC_NE);
    mx_clean(t);
    if (dbl) x_movq_ld(B, 0, VO(n)); else x_movd_ld(B, 0, VO(n));
    if (opc == 12) {
        x_mov_ri(B, XAX, dbl ? 0x3fe0000000000000ull : 0x3f000000u);
        if (dbl) xop2(B, XF_66 | XF_W, 0x6E, 3, XR(XAX)); else x_movd_ld(B, 3, XR(XAX));
        if (dbl) xop2(B, XF_66 | XF_W, 0x7E, 0, XR(XAX)); else x_movd_st(B, XR(XAX), 0);
        x_test_rr(B, dbl, XAX, XAX);
        a64_u8 *jneg = x_jcc32(B, CC_S);
        xop2(B, dbl ? XF_F2 : XF_F3, 0x58, 0, XR(3));
        s_round_packed(t, kind, 0, 0, 1);
        a64_u8 *jd = x_jmp32(B);
        if (!t->b.ovf) x_patch32(jneg, t->b.p);
        xop2(B, dbl ? XF_F2 : XF_F3, 0x5C, 0, XR(3));
        s_round_packed(t, kind, 0, 0, 2);
        if (!t->b.ovf) x_patch32(jd, t->b.p);
    } else if (opc == 14 || opc == 15) s_round_fpcr(t, kind);
    else s_round_packed(t, kind, 0, 0, mode[opc - 8]);
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));
    x_test8_mi(B, XM(XSP, 0), 0x1F);
    slow_if(t, CC_NE);
    if (dbl) xop2(B, XF_66 | XF_W, 0x7E, 0, XR(XAX)); else x_movd_st(B, XR(XAX), 0);
    x_mov_rr(B, dbl, XCX, XAX);
    x_shift_ri(B, dbl, S_SHL, XCX, 1);
    a64_u8 *jz = x_jcc32(B, CC_E);
    x_shift_ri(B, dbl, S_SHR, XCX, dbl ? 53 : 24);
    x_alu_ri(B, 0, A_CMP, XR(XCX), 1);
    a64_u8 *jlo = x_jcc32(B, CC_BE);
    x_alu_ri(B, 0, A_CMP, XR(XCX), dbl ? 0x7FF : 0xFF);
    a64_u8 *jhi = x_jcc32(B, CC_E);
    a64_u8 *jok = x_jmp32(B);
    if (!t->b.ovf) { x_patch32(jlo, t->b.p); x_patch32(jhi, t->b.p); }
    emit_slow_interp(t);
    if (!t->b.ovf) { x_patch32(jz, t->b.p); x_patch32(jok, t->b.p); }
    s_wsc_es(t, d, dbl);
    if (opc == 14) ixc_if_pe(t);
    mx_reset(t);
    return 1;
}

/* FCVT* de escalar FP para GPR. rmode 0..3 = N/P/M/Z; opc 4/5 com rmode 0 = FCVTAS/FCVTAU.
 * Fora da faixa, NaN, infinito e FZ vão ao interpretador, que satura e levanta IOC. */
static int s_fcvt_gpr(T *t, int dbl, int sf, int rmode, int opc, int n, int d) {
    int u = opc & 1, away = opc >= 4;
    int bias = dbl ? 1023 : 127;
    int guard = (rmode == 3 && !away) ? 0 : 1;
    int lim = bias + (sf ? 62 - guard : (u ? 31 : 30) - guard);
    int w64 = sf || u;
    fl_clobber(t);
    int hd = wx(t, d);
    x_mov_rm(B, dbl, XAX, VO(n));
    x_mov_rr(B, dbl, XCX, XAX);
    x_shift_ri(B, dbl, S_SHR, XCX, dbl ? 52 : 23);
    if (!u) x_alu_ri(B, 0, A_AND, XR(XCX), dbl ? 0x7FF : 0xFF);
    x_alu_ri(B, 0, A_CMP, XR(XCX), lim);
    slow_if(t, CC_A);
    x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);
    slow_if(t, CC_NE);
    mx_clean(t);
    if (dbl) xop2(B, XF_66 | XF_W, 0x6E, 0, XR(XAX)); else x_movd_ld(B, 0, XR(XAX));
    if (away) {
        x_movdqa_ld(B, 2, XR(0));
        xop2(B, (dbl ? XF_F2 : XF_F3) | XF_W, 0x2C, XDX, XR(0));
        xop2(B, (dbl ? XF_F2 : XF_F3) | XF_W, 0x2A, 1, XR(XDX));
        xop2(B, dbl ? XF_66 : 0, 0x2E, 2, XR(1));
        x_setcc(B, CC_NE, XCX);
        x_mov8_mr(B, XM(XSP, 0), XCX);
        x_movdqa_ld(B, 0, XR(2));
        x_mov_ri(B, XAX, dbl ? 0x3fe0000000000000ull : 0x3f000000u);
        if (dbl) xop2(B, XF_66 | XF_W, 0x6E, 3, XR(XAX)); else x_movd_ld(B, 3, XR(XAX));
        if (dbl) xop2(B, XF_66 | XF_W, 0x7E, 2, XR(XAX)); else x_movd_st(B, XR(XAX), 2);
        x_test_rr(B, dbl, XAX, XAX);
        a64_u8 *jneg = x_jcc32(B, CC_S);
        xop2(B, dbl ? XF_F2 : XF_F3, 0x58, 0, XR(3));
        s_round_packed(t, dbl ? 1 : 0, 0, 0, 1);
        a64_u8 *jd = x_jmp32(B);
        if (!t->b.ovf) x_patch32(jneg, t->b.p);
        xop2(B, dbl ? XF_F2 : XF_F3, 0x5C, 0, XR(3));
        s_round_packed(t, dbl ? 1 : 0, 0, 0, 2);
        if (!t->b.ovf) x_patch32(jd, t->b.p);
        xop2(B, (dbl ? XF_F2 : XF_F3) | (w64 ? XF_W : 0), 0x2C, XDX, XR(0));
        x_mov_rr(B, sf, hd, XDX);
        x_test8_mi(B, XM(XSP, 0), 0xFF);
        a64_u8 *jz = x_jcc8(B, CC_E);
        x_alu_ri(B, 0, A_OR, XM(RCPU, OFF_FPSR), 0x10);
        x_patch8(jz, t->b.p);
        mx_reset(t);
        return 1;
    }
    /* rmode 3 trunca com cvtt (não depende do MXCSR). +inf/−inf passam o RC do MXCSR
     * para o cvt, porque o imediato do round* não é honrado em todo host. */
    if (rmode == 1 || rmode == 2) mx_set_rc(t, rmode == 1 ? 2 : 1);
    xop2(B, (dbl ? XF_F2 : XF_F3) | (w64 ? XF_W : 0), (rmode == 3 ? 0x2C : 0x2D), XDX, XR(0));
    xop2(B, 0, 0xAE, 3, XM(XSP, 0));
    ixc_if_pe(t);
    x_mov_rr(B, sf, hd, XDX);
    if (rmode == 1 || rmode == 2) mx_reset(t);
    return 1;
}

static int s_shift_scalar(T *t, a64_u32 w) {
    int u = (int)BIT(w, 29), immh = (int)BITS(w, 22, 19);
    if (!immh || BIT(w, 23) || !(immh & 8)) return 0;
    int eb = 64;
    int hb = (int)BITS(w, 22, 16), op = (int)BITS(w, 15, 11);
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
    int rsh = 2 * eb - hb, lsh = hb - eb;
    if (op == 0x00 || op == 0x02 || (op == 0x0A && !u)) {
        x_movq_ld(B, 0, VO(n));
        if (op == 0x0A) s_shimm(t, 0, eb, 0, lsh);
        else s_shimm(t, 0, eb, u ? 1 : 2, rsh);
        if (op == 0x02) { s_ld(t, 1, d); s_op(t, 0xD4, 0, 1); }
        s_st(t, d, 0, 0);
        return 1;
    }
    if (op == 0x04 || op == 0x06) {
        x_movq_ld(B, 0, VO(n));
        s_rshr(t, 0, eb, u ? 1 : 2, rsh);
        if (op == 0x06) { s_ld(t, 1, d); s_op(t, 0xD4, 0, 1); }
        s_st(t, d, 0, 0);
        return 1;
    }
    return 0;
}

/* Escalar AdvSIMD (op0 0x5/0x7): só o que tem forma de uma lane de 64, ou SCVTF da mesma largura. */
static int s_scalar_simd(T *t, a64_u32 w) {
    if (BIT(w, 24) && BIT(w, 10) && !BIT(w, 23) && BITS(w, 22, 19) != 0) return s_shift_scalar(t, w);
    if (BIT(w, 21) && BIT(w, 10)) {
        int size = (int)BITS(w, 23, 22), op = (int)BITS(w, 15, 11);
        if (op == 0x08 && size == 3)
            return s_shlreg(t, 0, (int)BIT(w, 29), 3, (int)BITS(w, 9, 5), (int)BITS(w, 20, 16), (int)(w & 31), 1);
        return 0;
    }
    if (BIT(w, 21) && !BIT(w, 10) && BIT(w, 11) && BITS(w, 20, 17) == 0) {
        int size = (int)BITS(w, 23, 22), op = (int)BITS(w, 16, 12);
        int n = (int)BITS(w, 9, 5), d = (int)(w & 31), u = (int)BIT(w, 29);
        if (op == 0x0B && !u) return s_iabs(t, 0, size, n, d, 1);
        if (op == 0x1D && !(size & 2)) return s_scvtf(t, 0, u, size, n, d, 1);
        return 0;
    }
    return 0;
}

/* despacho: op0 = x111 (dados FP/SIMD) */
static int tr_simd(T *t, a64_u32 w) {
    if (((w >> 28) & 1) && !((w >> 30) & 1)) return s_fmov(t, w) || s_fcmp(t, w) || s_cvt(t, w) || s_fp(t, w); /* grupo FP escalar */
    unsigned op0 = w >> 28;
    if ((w & 0xFFFFFC00) == 0x5EF1B800) {           /* ADDP Dd, Vn.2D */
        int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
        fl_clobber(t);
        x_mov_rm(B, 1, XAX, VO(n));
        x_alu_rm(B, 1, A_ADD, XAX, VOE(n, 8));
        s_wsc_rax(t, d);
        return 1;
    }
    if (op0 == 0xC) return s_crypto_c(t, w);
    if ((op0 & 1) && !(op0 & 8)) return (op0 & 4) ? s_scalar_simd(t, w) : 0; /* escalar AdvSIMD */
    if (op0 & 8) return 0;
    if (BIT(w, 24)) {
        if (BIT(w, 10) && !BIT(w, 23) && BITS(w, 22, 19) == 0) return s_modimm(t, w);
        if (BIT(w, 10) && !BIT(w, 23)) return s_shift(t, w);
        if (!BIT(w, 10)) return s_indexed(t, w);
        return 0;
    }
    if (BIT(w, 21)) {
        if (BIT(w, 10)) return s_three_same(t, w);
        if (!BIT(w, 11)) return s_3diff(t, w);
        switch (BITS(w, 20, 17)) {
        case 0x0: return s_misc(t, w);
        case 0x8: return s_across(t, w);
        case 0x4:
            if (!(BITS(w, 31, 29) == 2 && BITS(w, 23, 22) == 0)) return 0;
            if (s_aes(t, w) || s_sha1h(t, w)) return 1;
            return 0;
        default: return 0;
        }
    }
    if (BIT(w, 10)) {
        if (!BIT(w, 15) && BITS(w, 23, 21) == 0) return s_copy(t, w);
        if (BIT(w, 22) && !BIT(w, 21) && !BIT(w, 14)) {   /* FP half vetorial (3same) */
            int q = (int)BIT(w, 30), u = (int)BIT(w, 29), a = (int)BIT(w, 23), op = (int)BITS(w, 13, 11);
            int lanes = q ? 8 : 4, n = (int)BITS(w, 9, 5), m = (int)BITS(w, 20, 16), d = (int)(w & 31);
            if (!u && op == 2) return s_f16(t, a ? 0x5C : 0x58, lanes, d, n, m);
            if (u && !a && op == 3) return s_f16(t, 0x59, lanes, d, n, m);
            if (u && !a && op == 7) return s_f16(t, 0x5E, lanes, d, n, m);
            return 0;
        }
        return 0;
    }
    if (BIT(w, 15)) return 0;
    if (!BIT(w, 29)) return BIT(w, 11) ? s_perm(t, w) : s_tbl(t, w);
    return s_ext(t, w);
}

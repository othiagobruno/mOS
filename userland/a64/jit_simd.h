/*
 * mOS — tradutor arm64 (frente JIT): emissão nativa de AdvSIMD/FP que é exata por construção
 * (operações de bits e inteiras, cópias, imediatos, moves entre registradores). Incluído por
 * jit_tr.c (usa o contexto T e os helpers de lá). FP aritmético NÃO é emitido aqui: vai ao
 * interpretador (primitivas a64_fp_*), porque NaN/FZ/flags do x86 divergem do ARM.
 * Recursos do host: SSE2 sempre; SSSE3/SSE4.1/SSE4.2 só se o CPUID disser (jit_g.*), senão
 * a instrução vai ao interpretador.
 * Convenção: xmm0..xmm5 são temporários; os registradores V ficam sempre em a64_cpu.
 */

#define VO(r) XM(RCPU, OFF_V(r))
#define VOE(r, off) XM(RCPU, OFF_V(r) + (int32_t)(off))

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
    case 0x13:                                      /* MUL */
        if (u || (size != 1 && size != 2) || (size == 2 && !jit_g.sse41)) return 0;
        s_ld(t, 0, n); s_ld(t, 1, m);
        if (size == 1) s_op(t, 0xD5, 0, 1); else s_op38(t, 0x40, 0, 1);
        s_st(t, d, 0, q);
        return 1;
    }
    return 0;
}

static int s_misc(T *t, a64_u32 w) {
    int q = (int)BIT(w, 30), u = (int)BIT(w, 29), size = (int)BITS(w, 23, 22), op = (int)BITS(w, 16, 12);
    int n = (int)BITS(w, 9, 5), d = (int)(w & 31);
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
    if ((opc != 3 && opc != 7) || (size == 3 && !q)) return 0;
    static const unsigned lo[4] = { 0x60, 0x61, 0x62, 0x6C }, hi[4] = { 0x68, 0x69, 0x6A, 0x6D };
    s_ld(t, 0, n); s_ld(t, 1, m);
    if (q) s_op(t, opc == 3 ? lo[size] : hi[size], 0, 1);
    else { s_op(t, lo[size], 0, 1); if (opc == 7) s_shiftdq(t, 0, 0, 8); }
    s_st(t, d, 0, q);
    return 1;
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
    int nr;
    switch (opcode) { case 7: nr = 1; break; case 10: nr = 2; break; case 6: nr = 3; break; case 2: nr = 4; break; default: return 0; }
    int bytes = q ? 16 : 8;
    int hb = rget(t, rn);
    /* o interpretador acessa elemento a elemento: se o intervalo cruza página, o efeito parcial
     * de uma falha seria outro; esse caso vai inteiro ao interpretador */
    fl_clobber_force(t);
    x_mov_rr(B, 0, XAX, hb);
    x_alu_ri(B, 0, A_AND, XR(XAX), 0xFFF);
    x_alu_ri(B, 0, A_CMP, XR(XAX), 0x1000 - nr * bytes);
    slow_if(t, CC_A);
    if (L) {
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
static void fcmp_core(T *t, int dbl, int n, int m, int zero) {
    if (dbl) x_movq_ld(B, 0, VO(n)); else x_movd_ld(B, 0, VO(n));
    if (zero) s_zero(t, 1);
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
    if (type > 1) return 0;                         /* half: interpretador */
    int dbl = type == 1;
    if ((w & 0xFF203C00) == 0x1E202000) {          /* FCMP / FCMPE */
        if (BITS(w, 15, 14) || (w & 7)) return 0;
        fl_clobber(t);
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);   /* FZ: denormais viram zero → interp */
        slow_if(t, CC_NE);
        fcmp_core(t, dbl, n, m, (int)BIT(w, 3));
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
        x_test_ri(B, 0, XM(RCPU, OFF_FPCR), 1u << 24);
        slow_if(t, CC_NE);
        fcmp_core(t, dbl, n, m, 0);
        if (jd && !t->b.ovf) x_patch32(jd, t->b.p);
        t->fe = 0; t->fm = 1;
        return 1;
    }
    return 0;
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
    if (type > 1) return 0;                         /* half: interpretador */
    int dbl = type == 1;
    int sqrt = 0, neg = 0, fma = 0;
    unsigned op;
    if ((w & 0xFF207C00) == 0x1E204000) {           /* 1 fonte: FABS / FNEG / FSQRT */
        unsigned opc = BITS(w, 20, 15);
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
        case 8: op = 0x59; neg = 1; break;          /* FNMUL = -(n*m) */
        default: return 0;                          /* FMAX/FMIN/FMAXNM/FMINNM: interpretador */
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

/* despacho: op0 = x111 (dados FP/SIMD) */
static int tr_simd(T *t, a64_u32 w) {
    if (((w >> 28) & 1) && !((w >> 30) & 1)) return s_fmov(t, w) || s_fcmp(t, w) || s_fp(t, w); /* grupo FP escalar */
    unsigned op0 = w >> 28;
    if ((op0 & 1) || (op0 & 8) || op0 == 0xC) return 0;                        /* escalares / cripto: interpretador */
    if (BIT(w, 24)) {
        if (BIT(w, 10) && !BIT(w, 23) && BITS(w, 22, 19) == 0) return s_modimm(t, w);
        return 0;
    }
    if (BIT(w, 21)) {
        if (BIT(w, 10)) return s_three_same(t, w);
        if (!BIT(w, 11)) return 0;
        switch (BITS(w, 20, 17)) {
        case 0x0: return s_misc(t, w);
        case 0x8: return s_across(t, w);
        default: return 0;
        }
    }
    if (BIT(w, 10)) {
        if (!BIT(w, 15) && BITS(w, 23, 21) == 0) return s_copy(t, w);
        return 0;
    }
    if (BIT(w, 15)) return 0;
    if (!BIT(w, 29)) return BIT(w, 11) ? s_perm(t, w) : 0;
    return s_ext(t, w);
}

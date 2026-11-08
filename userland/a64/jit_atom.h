/*
 * mOS — tradutor arm64 (frente JIT): exclusivos (LDXR/STXR e variantes acquire/release), CAS e
 * atômicos LSE (LDADD/LDCLR/LDEOR/LDSET/SWP, LDAPR) emitidos nativamente. Incluído por jit_tr.c.
 *
 * Mesma semântica do interpretador (interp_int_ls.c):
 *  - monitor = (excl_addr, excl_size, excl_val[0], excl_valid); LDXR lê e arma; STXR faz
 *    lock cmpxchg contra o valor guardado se o monitor bate (endereço e tamanho), status 0/1, e
 *    desarma sempre. ABA aceito (doc).
 *  - acesso que atravessa um bloco de 16 bytes → FAULT de alinhamento (o STXR confere antes do
 *    monitor); SA0 para base SP. Esses casos vão ao interpretador pelo caminho lento.
 *  - instruções com lock no x86 já são barreira completa (acquire/release/RCsc de graça).
 * Pares (LDXP/STXP/CASP) vão ao interpretador.
 */

#define OFF_EXA  ((int32_t)offsetof(a64_cpu, excl_addr))
#define OFF_EXV  ((int32_t)offsetof(a64_cpu, excl_val))
#define OFF_EXS  ((int32_t)offsetof(a64_cpu, excl_size))
#define OFF_EXOK ((int32_t)offsetof(a64_cpu, excl_valid))

/* lock + op de 1/2/4/8 bytes com memória: op8 para byte, op para os outros (0F xx) */
static void x_lock2(T *t, int size, unsigned op8, unsigned op, int reg, xo m) {
    xe8(B, 0xF0);
    if (size == 0) xop2(B, XF_BYTE, op8, reg, m);
    else xop2(B, (size == 1 ? XF_66 : 0) | (size == 3 ? XF_W : 0), op, reg, m);
}
/* rax ← rax zero-estendido do tamanho */
static void zext_rax(T *t, int size) {
    if (size == 0) x_movzx8(B, XAX, XR(XAX));
    else if (size == 1) x_movzx16(B, XAX, XR(XAX));
    else if (size == 2) x_mov_rr(B, 0, XAX, XAX);
}
/* acesso que atravessa 16 bytes → caminho lento (FAULT no interpretador) */
static void cross16_check(T *t, int rn, int hb, int size) {
    if (!size || (JIT_SA0 && rn == 31)) return;
    fl_clobber_force(t);
    x_mov_rr(B, 0, XAX, hb);
    x_alu_ri(B, 0, A_AND, XR(XAX), 15);
    x_alu_ri(B, 0, A_CMP, XR(XAX), 16 - (1 << size));
    slow_if(t, CC_A);
}

static int tr_excl(T *t, a64_u32 w) {           /* grupo 001000 com o2 = 0 (LDXR/STXR) ou CAS */
    int size = (int)BITS(w, 31, 30), o2 = (int)BIT(w, 23), L = (int)BIT(w, 22), o1 = (int)BIT(w, 21);
    int rs = (int)BITS(w, 20, 16), rt2 = (int)BITS(w, 14, 10), rn = (int)BITS(w, 9, 5), rt = (int)(w & 31);
    if (!JIT_DEF) return 0;                     /* configuração de compatibilidade do oráculo */
    if (!o2 && !o1) {
        if (!L && (rs == rt || (rs == rn && rn != 31))) return 0;   /* deixo os casos com sobreposição ao interp */
        int hb = rget(t, rn);
        sa0_check(t, rn, hb);
        cross16_check(t, rn, hb, size);
        if (L) {                                /* LDXR / LDAXR */
            int hd = wx(t, rt);
            fp_mark(t);
            if (size == 0) x_movzx8(B, XDX, XM(hb, 0));
            else if (size == 1) x_movzx16(B, XDX, XM(hb, 0));
            else x_mov_rm(B, size == 3, XDX, XM(hb, 0));
            x_mov_mr(B, 1, XM(RCPU, OFF_EXA), hb);
            x_mov_mi(B, 0, XM(RCPU, OFF_EXS), 1 << size);
            x_mov_mr(B, 1, XM(RCPU, OFF_EXV), XDX);
            x_mov_mi(B, 1, XM(RCPU, OFF_EXV + 8), 0);
            x_mov_mi(B, 0, XM(RCPU, OFF_EXOK), 1);
            x_mov_rr(B, 1, hd, XDX);
            return 1;
        }
        /* STXR / STLXR */
        int hv = rx(t, rt);
        int hst = wx(t, rs);
        fl_clobber_force(t);
        x_alu_ri(B, 0, A_CMP, XM(RCPU, OFF_EXOK), 0);
        a64_u8 *f1 = x_jcc32(B, CC_E);
        x_alu_rm(B, 1, A_CMP, hb, XM(RCPU, OFF_EXA));
        a64_u8 *f2 = x_jcc32(B, CC_NE);
        x_alu_ri(B, 0, A_CMP, XM(RCPU, OFF_EXS), 1 << size);
        a64_u8 *f3 = x_jcc32(B, CC_NE);
        x_mov_rm(B, 1, XAX, XM(RCPU, OFF_EXV));
        fp_mark(t);
        x_lock2(t, size, 0xB0, 0xB1, hv, XM(hb, 0));
        x_setcc(B, CC_NE, XDX);
        x_movzx8(B, XDX, XR(XDX));
        a64_u8 *jd = x_jmp32(B);
        if (!t->b.ovf) { x_patch32(f1, t->b.p); x_patch32(f2, t->b.p); x_patch32(f3, t->b.p); }
        x_mov_ri(B, XDX, 1);
        if (!t->b.ovf) x_patch32(jd, t->b.p);
        x_mov_mi(B, 0, XM(RCPU, OFF_EXOK), 0);
        x_mov_rr(B, 0, hst, XDX);
        return 1;
    }
    if (o2 && o1) {                             /* CAS / CASA / CASL / CASAL */
        if (rt2 != 31) return 0;
        int hb = rget(t, rn);
        int hs = rx(t, rs), hn = rx(t, rt);
        sa0_check(t, rn, hb);
        cross16_check(t, rn, hb, size);
        fl_clobber_force(t);
        int hd = wx(t, rs);
        x_mov_rr(B, 1, XAX, hs);
        zext_rax(t, size);                      /* Rs & máscara (o interp compara e devolve assim) */
        fp_mark(t);
        x_lock2(t, size, 0xB0, 0xB1, hn, XM(hb, 0));
        zext_rax(t, size);
        t->fe = 0;                              /* cmpxchg destrói EFLAGS (salvas acima) */
        x_mov_rr(B, 1, hd, XAX);
        return 1;
    }
    return 0;
}

/* atômicos LSE: grupo de registrador único com bit21 = 1 e bits 11:10 = 00, V = 0 */
static int tr_lse(T *t, a64_u32 w) {
    int size = (int)BITS(w, 31, 30), A = (int)BIT(w, 23), R = (int)BIT(w, 22), rs = (int)BITS(w, 20, 16);
    int o3 = (int)BIT(w, 15), opc = (int)BITS(w, 14, 12), rn = (int)BITS(w, 9, 5), rt = (int)(w & 31);
    if (BIT(w, 26)) return 0;
    if (o3) {
        if (opc == 4) {                         /* LDAPR */
            if (!A || R || rs != 31) return 0;
            int hb = rget(t, rn);
            sa0_check(t, rn, hb);
            if (!JIT_NAA) cross16_check(t, rn, hb, size);
            ld_int(t, rt == 31 ? XDX : rdef(t, rt), XM(hb, 0), size, 1);
            return 1;
        }
        if (opc != 0) return 0;
    }
    int hb = rget(t, rn), hv = rx(t, rs);
    sa0_check(t, rn, hb);
    if (size) {                                 /* cross16 → FAULT (sempre, como no interp) */
        fl_clobber_force(t);
        x_mov_rr(B, 0, XAX, hb);
        x_alu_ri(B, 0, A_AND, XR(XAX), 15);
        x_alu_ri(B, 0, A_CMP, XR(XAX), 16 - (1 << size));
        slow_if(t, CC_A);
    }
    fl_clobber_force(t);
    int hd = wx(t, rt);
    if (o3 || opc == 0) {                       /* SWP: xchg; LDADD: lock xadd */
        x_mov_rr(B, 1, XAX, hv);
        fp_mark(t);
        if (o3) {
            if (size == 0) x_xchg8_mr(B, XM(hb, 0), XAX);
            else if (size == 1) x_xchg16_mr(B, XM(hb, 0), XAX);
            else x_xchg_mr(B, size == 3, XM(hb, 0), XAX);
        } else x_lock2(t, size, 0xC0, 0xC1, XAX, XM(hb, 0));
        zext_rax(t, size);
    } else {                                    /* LDCLR LDEOR LDSET LDSMAX LDSMIN LDUMAX LDUMIN: laço de cmpxchg */
        fp_mark(t);
        if (size == 0) x_movzx8(B, XAX, XM(hb, 0));
        else if (size == 1) x_movzx16(B, XAX, XM(hb, 0));
        else x_mov_rm(B, size == 3, XAX, XM(hb, 0));
        a64_u8 *loop = t->b.p;
        x_mov_rr(B, 1, XCX, hv);
        if (opc >= 4) {                         /* MAX/MIN: compara no tamanho do acesso */
            static const int ccs[4] = { CC_G, CC_L, CC_A, CC_B };   /* SMAX SMIN UMAX UMIN: fica o antigo */
            if (size == 0) xop1(B, XF_BYTE, 0x38, XCX, XR(XAX));
            else xop1(B, (size == 1 ? XF_66 : 0) | (size == 3 ? XF_W : 0), 0x39, XCX, XR(XAX));
            x_cmov(B, 1, ccs[opc - 4], XCX, XR(XAX));
        }
        else if (opc == 1) { x_f7(B, 1, 2, XR(XCX)); x_alu_rr(B, 1, A_AND, XCX, XAX); }
        else x_alu_rr(B, 1, opc == 2 ? A_XOR : A_OR, XCX, XAX);
        fp_mark(t);
        x_lock2(t, size, 0xB0, 0xB1, XCX, XM(hb, 0));
        x_jcc_to(B, CC_NE, loop);
        zext_rax(t, size);
    }
    x_mov_rr(B, 1, hd, XAX);
    return 1;
}

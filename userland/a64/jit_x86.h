/*
 * mOS — tradutor arm64 (frente JIT): codificador x86_64 mínimo usado pelo gerador de código.
 * Só o que o JIT emite. Sem libc. Convenção: operandos "xo" são registrador (reg >= 0) ou
 * memória [base + idx*2^sc + disp] (base/idx = -1 quando ausentes).
 */
#pragma once
#include "a64.h"

enum { XAX, XCX, XDX, XBX, XSP, XBP, XSI, XDI, X8, X9, X10, X11, X12, X13, X14, X15 };
/* condições do x86 (jcc/setcc/cmovcc) */
enum { CC_O, CC_NO, CC_B, CC_AE, CC_E, CC_NE, CC_BE, CC_A, CC_S, CC_NS, CC_P, CC_NP, CC_L, CC_GE, CC_LE, CC_G };
/* grupo 1 (ALU) */
enum { A_ADD, A_OR, A_ADC, A_SBB, A_AND, A_SUB, A_XOR, A_CMP };
/* grupo 2 (deslocamentos) */
enum { S_ROL = 0, S_ROR = 1, S_SHL = 4, S_SHR = 5, S_SAR = 7 };

typedef struct xb { a64_u8 *p, *lim; int ovf; } xb;
typedef struct xo { signed char reg, base, idx, sc; int32_t disp; } xo;

static inline xo XR(int r) { xo o = { (signed char)r, -1, -1, 0, 0 }; return o; }
static inline xo XM(int base, int32_t d) { xo o = { -1, (signed char)base, -1, 0, d }; return o; }
static inline xo XMI(int base, int idx, int sc, int32_t d) { xo o = { -1, (signed char)base, (signed char)idx, (signed char)sc, d }; return o; }

#define XF_W    1   /* REX.W */
#define XF_BYTE 2   /* operando de 8 bits: registradores 4..7 exigem REX (spl/bpl/sil/dil) */
#define XF_66   4
#define XF_F2   8
#define XF_F3   16

static inline void xe8(xb *b, unsigned v) { if (b->p < b->lim) *b->p++ = (a64_u8)v; else b->ovf = 1; }
static inline void xe16(xb *b, unsigned v) { xe8(b, v); xe8(b, v >> 8); }
static inline void xe32(xb *b, a64_u32 v) { xe16(b, v); xe16(b, v >> 16); }
static inline void xe64(xb *b, a64_u64 v) { xe32(b, (a64_u32)v); xe32(b, (a64_u32)(v >> 32)); }
static inline int xfits8(int64_t v) { return v >= -128 && v <= 127; }
static inline int xfits32(int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

static void xmodrm(xb *b, int fl, const a64_u8 *op, int n, int reg, xo rm) {
    if (fl & XF_66) xe8(b, 0x66);
    if (fl & XF_F2) xe8(b, 0xF2);
    if (fl & XF_F3) xe8(b, 0xF3);
    int rex = (fl & XF_W) ? 8 : 0;
    if (reg & 8) rex |= 4;
    if (rm.reg >= 0) { if (rm.reg & 8) rex |= 1; }
    else {
        if (rm.base >= 0 && (rm.base & 8)) rex |= 1;
        if (rm.idx >= 0 && (rm.idx & 8)) rex |= 2;
    }
    int needrex = rex || ((fl & XF_BYTE) && ((reg >= 4 && reg < 8) || (rm.reg >= 4 && rm.reg < 8)));
    if (needrex) xe8(b, 0x40 | rex);
    for (int i = 0; i < n; i++) xe8(b, op[i]);
    reg &= 7;
    if (rm.reg >= 0) { xe8(b, 0xC0 | reg << 3 | (rm.reg & 7)); return; }
    if (rm.base < 0) {
        xe8(b, 0x04 | reg << 3);
        if (rm.idx < 0) xe8(b, 0x25);
        else xe8(b, (rm.sc << 6) | ((rm.idx & 7) << 3) | 5);
        xe32(b, (a64_u32)rm.disp);
        return;
    }
    int bl = rm.base & 7;
    int mod = (rm.disp == 0 && bl != 5) ? 0 : xfits8(rm.disp) ? 1 : 2;
    if (rm.idx >= 0 || bl == 4) {
        int idx = rm.idx >= 0 ? rm.idx : 4;
        xe8(b, (mod << 6) | reg << 3 | 4);
        xe8(b, (rm.sc << 6) | ((idx & 7) << 3) | bl);
    } else xe8(b, (mod << 6) | reg << 3 | bl);
    if (mod == 1) xe8(b, (a64_u8)rm.disp);
    else if (mod == 2) xe32(b, (a64_u32)rm.disp);
}
static inline void xop1(xb *b, int fl, unsigned op, int reg, xo rm) { a64_u8 o = (a64_u8)op; xmodrm(b, fl, &o, 1, reg, rm); }
static inline void xop2(xb *b, int fl, unsigned op, int reg, xo rm) { a64_u8 o[2] = { 0x0F, (a64_u8)op }; xmodrm(b, fl, o, 2, reg, rm); }
static inline void xop3(xb *b, int fl, unsigned op1, unsigned op2, int reg, xo rm) { a64_u8 o[3] = { 0x0F, (a64_u8)op1, (a64_u8)op2 }; xmodrm(b, fl, o, 3, reg, rm); }
#define XW(w) ((w) ? XF_W : 0)

/* ALU: dst(rm) op= src(reg) */
static inline void x_alu_rr(xb *b, int w, int op, int dst, int src) { xop1(b, XW(w), op * 8 + 1, src, XR(dst)); }
static inline void x_alu_rm(xb *b, int w, int op, int dst, xo m) { xop1(b, XW(w), op * 8 + 3, dst, m); }
static inline void x_alu_mr(xb *b, int w, int op, xo m, int src) { xop1(b, XW(w), op * 8 + 1, src, m); }
static inline void x_alu_ri(xb *b, int w, int op, xo dst, int32_t imm) {
    if (xfits8(imm)) { xop1(b, XW(w), 0x83, op, dst); xe8(b, (a64_u8)imm); }
    else { xop1(b, XW(w), 0x81, op, dst); xe32(b, (a64_u32)imm); }
}
static inline void x_mov_rr(xb *b, int w, int dst, int src) { xop1(b, XW(w), 0x89, src, XR(dst)); }
static inline void x_mov_rm(xb *b, int w, int dst, xo m) { xop1(b, XW(w), 0x8B, dst, m); }
static inline void x_mov_mr(xb *b, int w, xo m, int src) { xop1(b, XW(w), 0x89, src, m); }
static inline void x_mov8_mr(xb *b, xo m, int src) { xop1(b, XF_BYTE, 0x88, src, m); }
static inline void x_mov16_mr(xb *b, xo m, int src) { xop1(b, XF_66, 0x89, src, m); }
static inline void x_mov16_rm(xb *b, int dst, xo m) { xop1(b, XF_66, 0x8B, dst, m); }
static inline void x_mov_mi(xb *b, int w, xo m, int32_t imm) { xop1(b, XW(w), 0xC7, 0, m); xe32(b, (a64_u32)imm); }
static inline void x_mov8_mi(xb *b, xo m, a64_u8 imm) { xop1(b, 0, 0xC6, 0, m); xe8(b, imm); }
static inline void x_mov16_mi(xb *b, xo m, a64_u16 imm) { xop1(b, XF_66, 0xC7, 0, m); xe16(b, imm); }
/* mov reg, imm sem tocar nas flags, com a codificação mais curta */
static inline void x_mov_ri(xb *b, int dst, a64_u64 imm) {
    if (imm <= 0xFFFFFFFFull) { if (dst & 8) xe8(b, 0x41); xe8(b, 0xB8 + (dst & 7)); xe32(b, (a64_u32)imm); }
    else if (xfits32((int64_t)imm)) { xop1(b, XF_W, 0xC7, 0, XR(dst)); xe32(b, (a64_u32)imm); }
    else { xe8(b, 0x48 | ((dst & 8) ? 1 : 0)); xe8(b, 0xB8 + (dst & 7)); xe64(b, imm); }
}
static inline void x_movzx8(xb *b, int dst, xo src) { xop2(b, XF_BYTE, 0xB6, dst, src); }
static inline void x_movzx16(xb *b, int dst, xo src) { xop2(b, 0, 0xB7, dst, src); }
static inline void x_movsx8(xb *b, int w, int dst, xo src) { xop2(b, XW(w) | XF_BYTE, 0xBE, dst, src); }
static inline void x_movsx16(xb *b, int w, int dst, xo src) { xop2(b, XW(w), 0xBF, dst, src); }
static inline void x_movsxd(xb *b, int dst, xo src) { xop1(b, XF_W, 0x63, dst, src); }
static inline void x_lea(xb *b, int w, int dst, xo m) { xop1(b, XW(w), 0x8D, dst, m); }
static inline void x_shift_ri(xb *b, int w, int op, int dst, int imm) { xop1(b, XW(w), 0xC1, op, XR(dst)); xe8(b, (a64_u8)imm); }
static inline void x_shift_cl(xb *b, int w, int op, int dst) { xop1(b, XW(w), 0xD3, op, XR(dst)); }
static inline void x_shrd_ri(xb *b, int w, int dst, int src, int imm) { xop2(b, XW(w), 0xAC, src, XR(dst)); xe8(b, (a64_u8)imm); }
static inline void x_imul_rr(xb *b, int w, int dst, xo src) { xop2(b, XW(w), 0xAF, dst, src); }
static inline void x_f7(xb *b, int w, int ext, xo rm) { xop1(b, XW(w), 0xF7, ext, rm); } /* 2 not 3 neg 4 mul 5 imul 6 div 7 idiv */
static inline void x_test_rr(xb *b, int w, int a, int c) { xop1(b, XW(w), 0x85, c, XR(a)); }
static inline void x_test_ri(xb *b, int w, xo r, a64_u32 imm) { xop1(b, XW(w), 0xF7, 0, r); xe32(b, imm); }
static inline void x_test8_mi(xb *b, xo m, a64_u8 imm) { xop1(b, 0, 0xF6, 0, m); xe8(b, imm); }
static inline void x_bt_ri(xb *b, int w, int r, int bit) { xop2(b, XW(w), 0xBA, 4, XR(r)); xe8(b, (a64_u8)bit); }
static inline void x_cmov(xb *b, int w, int cc, int dst, xo src) { xop2(b, XW(w), 0x40 + cc, dst, src); }
static inline void x_setcc(xb *b, int cc, int r) { xop2(b, XF_BYTE, 0x90 + cc, 0, XR(r)); }
static inline void x_bswap(xb *b, int w, int r) { if (w || (r & 8)) xe8(b, 0x40 | (w ? 8 : 0) | ((r & 8) ? 1 : 0)); xe8(b, 0x0F); xe8(b, 0xC8 + (r & 7)); }
static inline void x_lzcnt(xb *b, int w, int dst, xo src) { xop2(b, XW(w) | XF_F3, 0xBD, dst, src); }
static inline void x_tzcnt(xb *b, int w, int dst, xo src) { xop2(b, XW(w) | XF_F3, 0xBC, dst, src); }
static inline void x_xchg_mr(xb *b, int w, xo m, int r) { xop1(b, XW(w), 0x87, r, m); }
static inline void x_xchg8_mr(xb *b, xo m, int r) { xop1(b, XF_BYTE, 0x86, r, m); }
static inline void x_xchg16_mr(xb *b, xo m, int r) { xop1(b, XF_66, 0x87, r, m); }
static inline void x_mfence(xb *b) { xe8(b, 0x0F); xe8(b, 0xAE); xe8(b, 0xF0); }
static inline void x_lahf(xb *b) { xe8(b, 0x9F); }
static inline void x_sahf(xb *b) { xe8(b, 0x9E); }
static inline void x_cmc(xb *b) { xe8(b, 0xF5); }
static inline void x_ret(xb *b) { xe8(b, 0xC3); }
static inline void x_push(xb *b, int r) { if (r & 8) xe8(b, 0x41); xe8(b, 0x50 + (r & 7)); }
static inline void x_pop(xb *b, int r) { if (r & 8) xe8(b, 0x41); xe8(b, 0x58 + (r & 7)); }
static inline void x_jmp_r(xb *b, int r) { xop1(b, 0, 0xFF, 4, XR(r)); }
static inline void x_jmp_m(xb *b, xo m) { xop1(b, 0, 0xFF, 4, m); }
static inline void x_cqo(xb *b) { xe8(b, 0x48); xe8(b, 0x99); }
static inline void x_cdq(xb *b) { xe8(b, 0x99); }
static inline void x_nop(xb *b, int n) {
    while (n >= 3) { xe8(b, 0x0F); xe8(b, 0x1F); xe8(b, 0x00); n -= 3; }  /* nop dword [rax] */
    while (n-- > 0) xe8(b, 0x90);
}
/* saltos com rel32: devolvem o endereço do campo rel32 para remendar */
static inline a64_u8 *x_jcc32(xb *b, int cc) { xe8(b, 0x0F); xe8(b, 0x80 + cc); a64_u8 *r = b->p; xe32(b, 0); return r; }
static inline a64_u8 *x_jmp32(xb *b) { xe8(b, 0xE9); a64_u8 *r = b->p; xe32(b, 0); return r; }
static inline a64_u8 *x_jcc8(xb *b, int cc) { xe8(b, 0x70 + cc); a64_u8 *r = b->p; xe8(b, 0); return r; }
static inline a64_u8 *x_jmp8(xb *b) { xe8(b, 0xEB); a64_u8 *r = b->p; xe8(b, 0); return r; }
static inline void x_patch32(a64_u8 *at, const a64_u8 *target) {
    if (!at) return;
    int32_t rel = (int32_t)(target - (at + 4));
    __builtin_memcpy(at, &rel, 4);
}
static inline void x_patch8(a64_u8 *at, const a64_u8 *target) { if (at) *at = (a64_u8)(int8_t)(target - (at + 1)); }
static inline void x_jmp_to(xb *b, const a64_u8 *t) { a64_u8 *r = x_jmp32(b); if (!b->ovf) x_patch32(r, t); }
static inline void x_jcc_to(xb *b, int cc, const a64_u8 *t) { a64_u8 *r = x_jcc32(b, cc); if (!b->ovf) x_patch32(r, t); }
/* call/jmp/lea relativos ao RIP */
static inline void x_riprel(xb *b, int fl, const a64_u8 *op, int n, int reg, const void *target) {
    if (fl & XF_W || (reg & 8)) xe8(b, 0x40 | ((fl & XF_W) ? 8 : 0) | ((reg & 8) ? 4 : 0));
    for (int i = 0; i < n; i++) xe8(b, op[i]);
    xe8(b, ((reg & 7) << 3) | 5);
    int32_t rel = (int32_t)((const a64_u8 *)target - (b->p + 4));
    xe32(b, (a64_u32)rel);
}
static inline void x_call_mrip(xb *b, const void *slot) { a64_u8 o = 0xFF; x_riprel(b, 0, &o, 1, 2, slot); }
static inline void x_lea_rip(xb *b, int dst, const void *t) { a64_u8 o = 0x8D; x_riprel(b, XF_W, &o, 1, dst, t); }

/* SSE (xmm0..15 no campo reg) */
static inline void x_movdqu_ld(xb *b, int x, xo m) { xop2(b, XF_F3, 0x6F, x, m); }
static inline void x_movdqu_st(xb *b, xo m, int x) { xop2(b, XF_F3, 0x7F, x, m); }
static inline void x_movdqa_ld(xb *b, int x, xo m) { xop2(b, XF_66, 0x6F, x, m); }
static inline void x_movdqa_st(xb *b, xo m, int x) { xop2(b, XF_66, 0x7F, x, m); }
static inline void x_movq_ld(xb *b, int x, xo m) { xop2(b, XF_F3, 0x7E, x, m); }   /* zera 127:64 */
static inline void x_movq_st(xb *b, xo m, int x) { xop2(b, XF_66, 0xD6, x, m); }
static inline void x_movd_ld(xb *b, int x, xo m) { xop2(b, XF_66, 0x6E, x, m); }   /* zera 127:32 */
static inline void x_movd_st(xb *b, xo m, int x) { xop2(b, XF_66, 0x7E, x, m); }
static inline void x_sse66(xb *b, unsigned op, int x, xo m) { xop2(b, XF_66, op, x, m); }  /* pxor=EF pand=DB por=EB ... */

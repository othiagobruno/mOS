/* mOS libSystem — montador da ISA GCN do GFX9: ver amdisa.h. Os campos de cada formato seguem o manual público
 * da ISA Vega; o teste de host (tools/amdisa_host.c) confere palavras de instruções conhecidas. */
#include "amdisa.h"

void amdasm_init(amdasm_t *a, amdisa_u32 *buf, unsigned cap) {
    a->w = buf;
    a->n = 0;
    a->cap = cap;
    a->err = 0;
    a->max_sgpr = a->max_vgpr = 0;
}
void amdasm_word(amdasm_t *a, amdisa_u32 w) {
    if (a->n >= a->cap) { a->err = 1; return; }
    a->w[a->n++] = w;
}
/* registra o uso de registradores (n = quantos a partir de r) */
static void use_s(amdasm_t *a, unsigned r, unsigned n) { if (r < 102 && r + n > a->max_sgpr) a->max_sgpr = r + n; }
static void use_v(amdasm_t *a, unsigned r, unsigned n) { if (r < 256 && r + n > a->max_vgpr) a->max_vgpr = r + n; }
static void use_src(amdasm_t *a, unsigned src) {
    if (src < 102) use_s(a, src, 1);
    else if (src >= 256) use_v(a, src - 256, 1);
}
static void lit(amdasm_t *a, unsigned s0, unsigned s1, amdisa_u32 v) { if (s0 == AMD_LIT || s1 == AMD_LIT) amdasm_word(a, v); }

void amd_sop2(amdasm_t *a, unsigned op, unsigned sdst, unsigned s0, unsigned s1, amdisa_u32 l) {
    use_s(a, sdst, 1); use_src(a, s0); use_src(a, s1);
    amdasm_word(a, 2u << 30 | (op & 0x7F) << 23 | (sdst & 0x7F) << 16 | (s1 & 0xFF) << 8 | (s0 & 0xFF));
    lit(a, s0, s1, l);
}
void amd_sop1(amdasm_t *a, unsigned op, unsigned sdst, unsigned s0, amdisa_u32 l) {
    use_s(a, sdst, 1); use_src(a, s0);
    amdasm_word(a, 0x17Du << 23 | (sdst & 0x7F) << 16 | (op & 0xFF) << 8 | (s0 & 0xFF));
    lit(a, s0, 0, l);
}
void amd_sopk(amdasm_t *a, unsigned op, unsigned sdst, unsigned simm16) {
    use_s(a, sdst, 1);
    amdasm_word(a, 0xBu << 28 | (op & 0x1F) << 23 | (sdst & 0x7F) << 16 | (simm16 & 0xFFFF));
}
void amd_sopc(amdasm_t *a, unsigned op, unsigned s0, unsigned s1, amdisa_u32 l) {
    use_src(a, s0); use_src(a, s1);
    amdasm_word(a, 0x17Eu << 23 | (op & 0x7F) << 16 | (s1 & 0xFF) << 8 | (s0 & 0xFF));
    lit(a, s0, s1, l);
}
void amd_sopp(amdasm_t *a, unsigned op, unsigned simm16) { amdasm_word(a, 0x17Fu << 23 | (op & 0x7F) << 16 | (simm16 & 0xFFFF)); }
void amd_smem(amdasm_t *a, unsigned op, unsigned sdata, unsigned sbase, unsigned offset, int glc) {
    static const unsigned nd[5] = {1, 2, 4, 8, 16};
    use_s(a, sdata, op < 5 ? nd[op] : 1);
    use_s(a, sbase, 2);
    if (sbase & 1) a->err = 1;   /* base de 64 bits em par alinhado */
    amdasm_word(a, 0x30u << 26 | (op & 0xFF) << 18 | 1u << 17 /* IMM */ | (glc ? 1u << 16 : 0) | (sdata & 0x7F) << 6 | ((sbase >> 1) & 0x3F));
    amdasm_word(a, offset & 0x1FFFFF);
}
void amd_vop2(amdasm_t *a, unsigned op, unsigned vdst, unsigned src0, unsigned vsrc1, amdisa_u32 l) {
    use_v(a, vdst, 1); use_src(a, src0); use_v(a, vsrc1, 1);
    amdasm_word(a, (op & 0x3F) << 25 | (vdst & 0xFF) << 17 | (vsrc1 & 0xFF) << 9 | (src0 & 0x1FF));
    lit(a, src0, 0, l);
}
void amd_vop1(amdasm_t *a, unsigned op, unsigned vdst, unsigned src0, amdisa_u32 l) {
    use_v(a, vdst, 1); use_src(a, src0);
    amdasm_word(a, 0x3Fu << 25 | (vdst & 0xFF) << 17 | (op & 0xFF) << 9 | (src0 & 0x1FF));
    lit(a, src0, 0, l);
}
void amd_vopc(amdasm_t *a, unsigned op, unsigned src0, unsigned vsrc1, amdisa_u32 l) {
    use_src(a, src0); use_v(a, vsrc1, 1);
    amdasm_word(a, 0x3Eu << 25 | (op & 0xFF) << 17 | (vsrc1 & 0xFF) << 9 | (src0 & 0x1FF));
    lit(a, src0, 0, l);
}
void amd_vop3(amdasm_t *a, unsigned op, unsigned vdst, unsigned src0, unsigned src1, unsigned src2, unsigned neg, unsigned abs, int clamp) {
    use_v(a, vdst, 1); use_src(a, src0); use_src(a, src1); use_src(a, src2);
    if (src0 == AMD_LIT || src1 == AMD_LIT || src2 == AMD_LIT) a->err = 1;   /* o VOP3 do GFX9 não tem literal */
    amdasm_word(a, 0x34u << 26 | (op & 0x3FF) << 16 | (clamp ? 1u << 15 : 0) | (abs & 7) << 8 | (vdst & 0xFF));
    amdasm_word(a, (neg & 7) << 29 | (src2 & 0x1FF) << 18 | (src1 & 0x1FF) << 9 | (src0 & 0x1FF));
}
void amd_vop3b(amdasm_t *a, unsigned op, unsigned vdst, unsigned sdst, unsigned src0, unsigned src1, unsigned src2) {
    use_v(a, vdst, 1); use_s(a, sdst, 2); use_src(a, src0); use_src(a, src1); use_src(a, src2);
    if (src0 == AMD_LIT || src1 == AMD_LIT || src2 == AMD_LIT) a->err = 1;
    amdasm_word(a, 0x34u << 26 | (op & 0x3FF) << 16 | (sdst & 0x7F) << 8 | (vdst & 0xFF));
    amdasm_word(a, (src2 & 0x1FF) << 18 | (src1 & 0x1FF) << 9 | (src0 & 0x1FF));
}
void amd_ds(amdasm_t *a, unsigned op, unsigned vdst, unsigned addr, unsigned data0, unsigned data1, unsigned offset0, unsigned offset1) {
    use_v(a, vdst, 1); use_v(a, addr, 1); use_v(a, data0, 1); use_v(a, data1, 1);
    amdasm_word(a, 0x36u << 26 | (op & 0xFF) << 17 | (offset1 & 0xFF) << 8 | (offset0 & 0xFF));
    amdasm_word(a, (vdst & 0xFF) << 24 | (data1 & 0xFF) << 16 | (data0 & 0xFF) << 8 | (addr & 0xFF));
}
void amd_global(amdasm_t *a, unsigned op, unsigned vdst, unsigned vaddr, unsigned vdata, unsigned saddr, int offset13, int glc) {
    int loads = op >= 16 && op < 24;
    if (loads) use_v(a, vdst, op == AMD_GLOBAL_LOAD_DWORDX4 ? 4 : op == AMD_GLOBAL_LOAD_DWORDX2 ? 2 : 1);
    else use_v(a, vdata, op == AMD_GLOBAL_STORE_DWORDX4 ? 4 : op == AMD_GLOBAL_STORE_DWORDX2 ? 2 : 1);
    use_v(a, vaddr, saddr == AMD_GLOBAL_OFF ? 2 : 1);
    if (saddr != AMD_GLOBAL_OFF) use_s(a, saddr, 2);
    if (offset13 < -4096 || offset13 > 4095) a->err = 1;
    amdasm_word(a, 0x37u << 26 | (op & 0x7F) << 18 | (glc ? 1u << 16 : 0) | 2u << 14 /* SEG global */ | ((unsigned)offset13 & 0x1FFF));
    amdasm_word(a, (vdst & 0xFF) << 24 | (saddr & 0x7F) << 16 | (vdata & 0xFF) << 8 | (vaddr & 0xFF));
}
unsigned amd_waitcnt(int vmcnt, int expcnt, int lgkmcnt) {
    unsigned vm = vmcnt < 0 ? 63 : (unsigned)vmcnt, ex = expcnt < 0 ? 7 : (unsigned)expcnt, lg = lgkmcnt < 0 ? 15 : (unsigned)lgkmcnt;
    return (vm & 0xF) | (ex & 7) << 4 | (lg & 0xF) << 8 | ((vm >> 4) & 3) << 14;
}
amdisa_u32 amd_rsrc1(unsigned nvgpr, unsigned nsgpr) {
    if (!nvgpr) nvgpr = 1;
    nsgpr += 6;                /* VCC, FLAT_SCRATCH e XNACK_MASK no fim do bloco de SGPRs do GFX9 */
    amdisa_u32 vg = (nvgpr - 1) / 4, sg = (nsgpr - 1) / 8;
    /* FLOAT_MODE 0xC0 (desnormais de f64/f16 preservados), DX10_CLAMP, IEEE_MODE */
    return (vg & 0x3F) | (sg & 0xF) << 6 | 0xC0u << 12 | 1u << 21 | 1u << 23;
}
amdisa_u32 amd_rsrc2(unsigned nuser_sgpr, unsigned tgid, unsigned tid_dims) {
    if (tid_dims < 1) tid_dims = 1;
    if (tid_dims > 3) tid_dims = 3;
    return (nuser_sgpr & 0x1F) << 1 | (tgid & 7) << 7 | (tid_dims - 1) << 11;
}

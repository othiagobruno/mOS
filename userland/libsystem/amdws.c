/* mOS libSystem — driver de usuário da GPU AMD (GFX9): ver amdws.h. Os pacotes seguem o formato PM4 tipo 3 do
 * GFX9 (cabeçalho 0xC0000000 | opcode << 8 | (corpo - 1) << 16) e só usam os opcodes, registradores e endereços que
 * o validador do kernel aceita (registradores de contexto, de shader e UCONFIG de usuário; memória só nos BOs do
 * lote; RELEASE_MEM sem interrupção). Teste de host: tools/amdws_host.c. */
#include "amdws.h"
#include "libc.h"

/* opcodes PM4 */
enum {
    OP_NOP = 0x10, OP_DISPATCH_DIRECT = 0x15, OP_WRITE_DATA = 0x37, OP_WAIT_REG_MEM = 0x3C, OP_RELEASE_MEM = 0x49,
    OP_ACQUIRE_MEM = 0x58, OP_SET_CONTEXT_REG = 0x69, OP_SET_SH_REG = 0x76, OP_SET_UCONFIG_REG = 0x79,
};

/* ---------------------------------------------------------------- dispositivo e memória */
int amdws_open(amdws_t *w) {
    memset(w, 0, sizeof *w);
    mos_gpu_info_t in;
    memset(&in, 0, sizeof in);
    int e = mos_gpu_op(MOS_GPU_INFO, &in, sizeof in);
    if (e) return e;
    if (in.abi < 3 || !(in.caps & MOS_GPU_CAP_AMD)) return -MOS_ENOENT;
    w->family = in.amd_family;
    w->num_cu = in.num_cu;
    w->vram = in.vram_bytes;
    w->gtt = in.gtt_bytes;
    mos_gpu_ctx_t c = {0, MOS_GPU_CTX_AMD, 0};
    e = mos_gpu_op(MOS_GPU_CTX_CREATE, &c, sizeof c);
    if (e) return e;
    w->ctx = c.ctx;
    return 0;
}
void amdws_close(amdws_t *w) {
    if (!w->ctx) return;
    mos_gpu_ctx_t c = {w->ctx, MOS_GPU_CTX_AMD, 0};
    mos_gpu_op(MOS_GPU_CTX_DESTROY, &c, sizeof c);
    w->ctx = 0;
}
int amdws_bo_new(amdws_t *w, amdbo_t *b, amd_u64 size, unsigned flags) {
    memset(b, 0, sizeof *b);
    size = (size + 4095) & ~4095ull;
    if (!size) size = 4096;
    mos_gpu_bo_t bo;
    memset(&bo, 0, sizeof bo);
    bo.flags = flags | MOS_GPU_BO_READ | MOS_GPU_BO_WRITE;
    bo.ctx = w->ctx;
    bo.size = size;
    int e = mos_gpu_op(MOS_GPU_BO_CREATE, &bo, sizeof bo);
    if (e) return e;
    if (!bo.gpu_va) {   /* sem VA o BO não serve para a GPU AMD */
        mos_gpu_bo_t d = {bo.handle, 0, 0, 0, 0, 0};
        mos_gpu_op(MOS_GPU_BO_DESTROY, &d, sizeof d);
        return -MOS_EINVAL;
    }
    mos_gpu_map_t m = {bo.handle, 0, 0, size};
    e = mos_gpu_op(MOS_GPU_BO_MAP, &m, sizeof m);
    if (e) {
        mos_gpu_bo_t d = {bo.handle, 0, 0, 0, 0, 0};
        mos_gpu_op(MOS_GPU_BO_DESTROY, &d, sizeof d);
        return e;
    }
    b->handle = bo.handle;
    b->flags = bo.flags;
    b->va = bo.gpu_va;
    b->size = size;
    b->map = (void *)(unsigned long)m.addr;
    return 0;
}
void amdws_bo_free(amdws_t *w, amdbo_t *b) {
    (void)w;
    if (!b->handle) return;
    if (b->map) { mos_gpu_map_t m = {b->handle, 0, (amd_u64)(unsigned long)b->map, b->size}; mos_gpu_op(MOS_GPU_BO_UNMAP, &m, sizeof m); }
    mos_gpu_bo_t d = {b->handle, 0, 0, 0, 0, 0};
    mos_gpu_op(MOS_GPU_BO_DESTROY, &d, sizeof d);
    memset(b, 0, sizeof *b);
}

/* ---------------------------------------------------------------- fluxo de comandos */
int amdcs_init(amdws_t *w, amdcs_t *cs, int ring, amd_u32 cap_dw) {
    memset(cs, 0, sizeof *cs);
    if (!cap_dw) cap_dw = 16384;
    int e = amdws_bo_new(w, &cs->ib, (amd_u64)cap_dw * 4, MOS_GPU_BO_GPU);
    if (e) return e;
    cs->dw = cs->ib.map;
    cs->cap = (amd_u32)(cs->ib.size / 4);
    cs->ring = ring;
    amdcs_use(cs, &cs->ib);
    return 0;
}
void amdcs_free(amdws_t *w, amdcs_t *cs) { amdws_bo_free(w, &cs->ib); memset(cs, 0, sizeof *cs); }
void amdcs_reset(amdcs_t *cs) {
    cs->n = 0;
    cs->nbo = 0;
    cs->overflow = 0;
    amdcs_use(cs, &cs->ib);
}
void amdcs_use(amdcs_t *cs, const amdbo_t *b) {
    for (unsigned i = 0; i < cs->nbo; i++) if (cs->bo[i] == b->handle) return;
    if (cs->nbo >= AMDCS_MAX_BO) { cs->overflow = 1; return; }
    cs->bo[cs->nbo++] = b->handle;
}
void amdcs_dw(amdcs_t *cs, amd_u32 v) {
    if (cs->n >= cs->cap) { cs->overflow = 1; return; }
    cs->dw[cs->n++] = v;
}
void amdcs_pkt3(amdcs_t *cs, amd_u32 op, amd_u32 body_dw) { amdcs_dw(cs, AMD_PKT3(op, body_dw - 1)); }
static void set_reg(amdcs_t *cs, amd_u32 op, amd_u32 base, amd_u32 reg, const amd_u32 *v, amd_u32 n) {
    if (!n) return;
    amdcs_pkt3(cs, op, n + 1);
    amdcs_dw(cs, (reg - base) / 4);
    for (amd_u32 i = 0; i < n; i++) amdcs_dw(cs, v[i]);
}
void amdcs_set_sh(amdcs_t *cs, amd_u32 reg, const amd_u32 *v, amd_u32 n) { set_reg(cs, OP_SET_SH_REG, 0xB000, reg, v, n); }
void amdcs_set_ctx(amdcs_t *cs, amd_u32 reg, const amd_u32 *v, amd_u32 n) { set_reg(cs, OP_SET_CONTEXT_REG, 0x28000, reg, v, n); }
void amdcs_set_uconfig(amdcs_t *cs, amd_u32 reg, const amd_u32 *v, amd_u32 n) { set_reg(cs, OP_SET_UCONFIG_REG, 0x30000, reg, v, n); }
void amdcs_write_data(amdcs_t *cs, amd_u64 va, const amd_u32 *v, amd_u32 n) {
    if (!n) return;
    amdcs_pkt3(cs, OP_WRITE_DATA, 3 + n);
    amdcs_dw(cs, 5u << 8 | 1u << 20);          /* DST_SEL memória, WR_CONFIRM */
    amdcs_dw(cs, (amd_u32)va);
    amdcs_dw(cs, (amd_u32)(va >> 32));
    for (amd_u32 i = 0; i < n; i++) amdcs_dw(cs, v[i]);
}
void amdcs_acquire_all(amdcs_t *cs) {
    /* CP_COHER_CNTL: TC_WB, TCL1, TC (L2), SH_KCACHE e SH_ICACHE; toda a memória */
    amdcs_pkt3(cs, OP_ACQUIRE_MEM, 6);
    amdcs_dw(cs, 1u << 18 | 1u << 22 | 1u << 23 | 1u << 27 | 1u << 29);
    amdcs_dw(cs, 0xFFFFFFFFu);
    amdcs_dw(cs, 0xFFu);
    amdcs_dw(cs, 0);
    amdcs_dw(cs, 0);
    amdcs_dw(cs, 0x0A);
}
void amdcs_release_mem(amdcs_t *cs, int compute, amd_u64 va, amd_u32 value) {
    /* evento de fim de pipe: CS_DONE (compute) ou BOTTOM_OF_PIPE_TS (gráfico); grava 32 bits, sem interrupção */
    amd_u32 ev = compute ? (0x2Fu | 6u << 8) : (0x28u | 5u << 8);
    amdcs_pkt3(cs, OP_RELEASE_MEM, 7);
    amdcs_dw(cs, ev | 1u << 15 /* TC_WB_ACTION */ | 1u << 17 /* TC_ACTION */);
    amdcs_dw(cs, 1u << 29);                     /* DATA_SEL 32 bits, DST_SEL memória, INT_SEL 0 */
    amdcs_dw(cs, (amd_u32)va & ~3u);
    amdcs_dw(cs, (amd_u32)(va >> 32) & 0xFFFFu);
    amdcs_dw(cs, value);
    amdcs_dw(cs, 0);
    amdcs_dw(cs, 0);
}
void amdcs_wait_mem(amdcs_t *cs, amd_u64 va, amd_u32 ref, amd_u32 mask) {
    amdcs_pkt3(cs, OP_WAIT_REG_MEM, 6);
    amdcs_dw(cs, 3u | 1u << 4);                 /* igual, memória, só esperar */
    amdcs_dw(cs, (amd_u32)va & ~3u);
    amdcs_dw(cs, (amd_u32)(va >> 32) & 0xFFFFu);
    amdcs_dw(cs, ref);
    amdcs_dw(cs, mask);
    amdcs_dw(cs, 0x0A);
}
void amdcs_dispatch(amdcs_t *cs, const amd_dispatch_t *d) {
    amd_u32 start[6] = {0, 0, 0, d->local[0] ? d->local[0] : 1, d->local[1] ? d->local[1] : 1, d->local[2] ? d->local[2] : 1};
    amdcs_set_sh(cs, AMD_COMPUTE_START_X, start, 6);
    amd_u32 pgm[2] = {(amd_u32)(d->code_va >> 8), (amd_u32)(d->code_va >> 40) & 0xFFu};
    amdcs_set_sh(cs, AMD_COMPUTE_PGM_LO, pgm, 2);
    /* RSRC2: USER_SGPR (bits 1..5) e LDS_SIZE (bits 15..23, blocos de 128 dwords) */
    amd_u32 lds = (d->lds_bytes + 511) / 512;
    amd_u32 rs[2] = {d->rsrc1, (d->rsrc2 & ~(0x1Fu << 1) & ~(0x1FFu << 15)) | ((amd_u32)d->nuser & 0x1F) << 1 | (lds & 0x1FF) << 15};
    amdcs_set_sh(cs, AMD_COMPUTE_PGM_RSRC1, rs, 2);
    /* RESOURCE_LIMITS, STATIC_THREAD_MGMT_SE0/SE1 (todas as CUs), TMPRING_SIZE (sem scratch), SE2/SE3 */
    amd_u32 lim[6] = {0, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, 0xFFFFFFFFu};
    amdcs_set_sh(cs, AMD_COMPUTE_RESOURCE_LIMITS, lim, 6);
    if (d->nuser) amdcs_set_sh(cs, AMD_COMPUTE_USER_DATA_0, d->user, (amd_u32)(d->nuser > 16 ? 16 : d->nuser));
    amdcs_pkt3(cs, OP_DISPATCH_DIRECT, 4);
    amdcs_dw(cs, d->groups[0] ? d->groups[0] : 1);
    amdcs_dw(cs, d->groups[1] ? d->groups[1] : 1);
    amdcs_dw(cs, d->groups[2] ? d->groups[2] : 1);
    amdcs_dw(cs, 1u | 1u << 2);                 /* COMPUTE_SHADER_EN, FORCE_START_AT_000 */
}

/* ---------------------------------------------------------------- envio e fence */
int amdws_submit(amdws_t *w, amdcs_t *cs, unsigned fence) {
    if (cs->overflow) return -MOS_ENOMEM;
    if (!cs->n) return -MOS_EINVAL;
    /* GFX9: o CP busca o IB em blocos de 8 dwords; completa com NOP de um dword */
    while (cs->n & 7) amdcs_dw(cs, AMD_PKT3(OP_NOP, 0x3FFF));
    if (cs->overflow) return -MOS_ENOMEM;
    unsigned char buf[sizeof(mos_gpu_amd_batch_t) + sizeof(mos_gpu_amd_ib_t) + 4 * AMDCS_MAX_BO];
    mos_gpu_amd_batch_t h = {MOS_GPU_AMD_BATCH_VER, 1, cs->nbo, 0};
    mos_gpu_amd_ib_t ib = {cs->ib.handle, (unsigned)cs->ring, 0, cs->n};
    memcpy(buf, &h, sizeof h);
    memcpy(buf + sizeof h, &ib, sizeof ib);
    memcpy(buf + sizeof h + sizeof ib, cs->bo, 4u * cs->nbo);
    mos_gpu_submit_t s;
    memset(&s, 0, sizeof s);
    s.ctx = w->ctx;
    s.cmd_bytes = (unsigned)(sizeof h + sizeof ib + 4u * cs->nbo);
    s.fence = fence;
    s.flags = MOS_GPU_SUBMIT_AMD;
    s.cmds = (amd_u64)(unsigned long)buf;
    return mos_gpu_op(MOS_GPU_SUBMIT, &s, sizeof s);
}
int amdws_fence_new(unsigned *fence) {
    mos_gpu_fence_t f = {0, 0, 0, 0};
    int e = mos_gpu_op(MOS_GPU_FENCE_CREATE, &f, sizeof f);
    if (!e) *fence = f.fence;
    return e;
}
int amdws_fence_wait(unsigned fence, unsigned timeout_ms) {
    mos_gpu_fence_t f = {fence, timeout_ms, 0, 0};
    return mos_gpu_op(MOS_GPU_FENCE_WAIT, &f, sizeof f);
}
void amdws_fence_free(unsigned fence) {
    mos_gpu_fence_t f = {fence, 0, 0, 0};
    mos_gpu_op(MOS_GPU_FENCE_DESTROY, &f, sizeof f);
}

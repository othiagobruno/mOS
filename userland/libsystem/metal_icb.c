/* mOS libSystem — MTLIndirectCommandBuffer do Metal por software.
 *
 * O ICB guarda até maxCommandCount comandos gravados pela CPU (indirectRenderCommandAtIndex: /
 * indirectComputeCommandAtIndex:): pipeline, buffers por índice e um desenho ou despacho. executeCommandsInBuffer:
 * repete cada comando do intervalo pela API do próprio encoder (o que inherit* pede vem do estado do encoder), então
 * o resultado é o mesmo de codificar os comandos à mão. O blit encoder copia e limpa intervalos de comandos. */
#include "metalint.h"

typedef struct { u64 x, y, z; } isz_t;
typedef struct {
    int kind;                  /* 0 vazio, 1 draw, 2 drawIndexed, 3 dispatchThreadgroups, 4 dispatchThreads */
    id pipe;
    id buf[3][31];             /* vertex, fragment, kernel */
    u64 off[3][31];
    u32 used[3];               /* bit i = buffer i definido */
    u64 tglen[31];
    u64 prim, start, count, inst, binst, itype, ioff;
    i64 bvert;
    id ib;
    isz_t g, t;
    int barrier;
} icmd_t;
typedef struct {
    MHDR;
    u64 types;
    int inh_pipe, inh_buf;
    u64 n;
    icmd_t *cmd;
    id *views;                 /* MTLIndirectRenderCommand / MTLIndirectComputeCommand de cada índice */
} micb_t;
typedef struct { MHDR; micb_t *icb; u64 i; } micmd_t;

static cx_class_t MICB_c, MICB_m, MIRC_c, MIRC_m, MICC_c, MICC_m;
u64 mtl_dget(id o, const char *name);
id objc_retain(id o);
void objc_release(id o);

static void keep(id *slot, id v) { if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static void cmd_clear(icmd_t *c) {
    keep(&c->pipe, 0);
    keep(&c->ib, 0);
    for (int k = 0; k < 3; k++) for (int i = 0; i < 31; i++) keep(&c->buf[k][i], 0);
    memset(c, 0, sizeof *c);
}
/* ---- o ICB */
static id dev_new_icb(id s, SEL c, id desc, u64 max, u64 opts) {
    (void)c; (void)opts;
    if (!desc || !max || max > (1u << 20)) return 0;
    micb_t *b = (micb_t *)cx_alloc_instance(&MICB_c);
    b->dev = s;
    b->types = mtl_dget(desc, "commandTypes");
    b->inh_pipe = (int)mtl_dget(desc, "inheritPipelineState");
    b->inh_buf = (int)mtl_dget(desc, "inheritBuffers");
    b->n = max;
    b->cmd = calloc(max, sizeof(icmd_t));
    b->views = calloc(max, sizeof(id));
    if (!b->cmd || !b->views) { objc_release((id)b); return 0; }
    return (id)b;
}
static void icb_dealloc(id s, SEL c) {
    micb_t *b = (micb_t *)s;
    for (u64 i = 0; i < b->n; i++) { cmd_clear(&b->cmd[i]); if (b->views[i]) objc_release(b->views[i]); }
    free(b->cmd);
    free(b->views);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static u64 icb_size(id s, SEL c) { (void)c; return ((micb_t *)s)->n; }
static id icb_view(id s, u64 i, cx_class_t *k) {
    micb_t *b = (micb_t *)s;
    if (i >= b->n) return 0;
    if (!b->views[i] || b->views[i]->isa != k) {
        if (b->views[i]) objc_release(b->views[i]);
        micmd_t *v = (micmd_t *)cx_alloc_instance(k);
        v->icb = b;
        v->i = i;
        b->views[i] = (id)v;
    }
    return b->views[i];
}
static id icb_render_at(id s, SEL c, u64 i) { (void)c; return icb_view(s, i, &MIRC_c); }
static id icb_compute_at(id s, SEL c, u64 i) { (void)c; return icb_view(s, i, &MICC_c); }
typedef struct { u64 loc, len; } nsrange_t;
static void icb_reset(id s, SEL c, nsrange_t r) {
    (void)c;
    micb_t *b = (micb_t *)s;
    for (u64 i = r.loc; i < r.loc + r.len && i < b->n; i++) cmd_clear(&b->cmd[i]);
}
static u64 icb_rid(id s, SEL c) { (void)c; return (u64)(uintptr_t)s; }

/* ---- comandos gravados */
static icmd_t *cmd_of(id s) { micmd_t *v = (micmd_t *)s; return &v->icb->cmd[v->i]; }
static void ic_pipe(id s, SEL c, id p) { (void)c; keep(&cmd_of(s)->pipe, p); }
static void ic_buf(id s, int k, id b, u64 off, u64 i) {
    if (i >= 31) return;
    icmd_t *m = cmd_of(s);
    keep(&m->buf[k][i], b);
    m->off[k][i] = off;
    if (b) m->used[k] |= 1u << i; else m->used[k] &= ~(1u << i);
}
static void ic_vbuf(id s, SEL c, id b, u64 off, u64 i) { (void)c; ic_buf(s, 0, b, off, i); }
static void ic_fbuf(id s, SEL c, id b, u64 off, u64 i) { (void)c; ic_buf(s, 1, b, off, i); }
static void ic_kbuf(id s, SEL c, id b, u64 off, u64 i) { (void)c; ic_buf(s, 2, b, off, i); }
static void ic_draw(id s, SEL c, u64 prim, u64 start, u64 count, u64 inst, u64 binst) {
    (void)c;
    icmd_t *m = cmd_of(s);
    m->kind = 1; m->prim = prim; m->start = start; m->count = count; m->inst = inst; m->binst = binst;
}
static void ic_drawidx(id s, SEL c, u64 prim, u64 count, u64 itype, id ib, u64 ioff, u64 inst, i64 bvert, u64 binst) {
    (void)c;
    icmd_t *m = cmd_of(s);
    m->kind = 2; m->prim = prim; m->count = count; m->itype = itype; keep(&m->ib, ib); m->ioff = ioff; m->inst = inst; m->bvert = bvert; m->binst = binst;
}
static void ic_tgmem(id s, SEL c, u64 len, u64 i) { (void)c; if (i < 31) cmd_of(s)->tglen[i] = len; }
static void ic_disp_tg(id s, SEL c, isz_t g, isz_t t) { (void)c; icmd_t *m = cmd_of(s); m->kind = 3; m->g = g; m->t = t; }
static void ic_disp_th(id s, SEL c, isz_t g, isz_t t) { (void)c; icmd_t *m = cmd_of(s); m->kind = 4; m->g = g; m->t = t; }
static void ic_barrier(id s, SEL c) { (void)c; cmd_of(s)->barrier = 1; }
static void ic_clear_barrier(id s, SEL c) { (void)c; cmd_of(s)->barrier = 0; }
static void ic_reset(id s, SEL c) { (void)c; cmd_clear(cmd_of(s)); }
static void ic_noop(id s, SEL c) { (void)s; (void)c; }

/* ---- execução: repete pela API do encoder */
static void run_render(id enc, micb_t *b, u64 i) {
    icmd_t *m = &b->cmd[i];
    if (m->kind != 1 && m->kind != 2) return;
    if (!b->inh_pipe && m->pipe) MSG(void, enc, "setRenderPipelineState:", m->pipe);
    if (!b->inh_buf)
        for (u64 k = 0; k < 31; k++) {
            if (m->used[0] >> k & 1) MSG(void, enc, "setVertexBuffer:offset:atIndex:", m->buf[0][k], m->off[0][k], k);
            if (m->used[1] >> k & 1) MSG(void, enc, "setFragmentBuffer:offset:atIndex:", m->buf[1][k], m->off[1][k], k);
        }
    if (m->kind == 1) MSG(void, enc, "drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:", m->prim, m->start, m->count, m->inst, m->binst);
    else ((void (*)(id, SEL, u64, u64, u64, id, u64, u64, i64, u64))objc_msgSend)(enc, S("drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:"),
                                                                                    m->prim, m->count, m->itype, m->ib, m->ioff, m->inst, m->bvert, m->binst);
}
static void run_compute(id enc, micb_t *b, u64 i) {
    icmd_t *m = &b->cmd[i];
    if (m->kind != 3 && m->kind != 4) return;
    if (m->barrier) MSG(void, enc, "memoryBarrierWithScope:", (u64)1);
    if (!b->inh_pipe && m->pipe) MSG(void, enc, "setComputePipelineState:", m->pipe);
    if (!b->inh_buf) for (u64 k = 0; k < 31; k++) if (m->used[2] >> k & 1) MSG(void, enc, "setBuffer:offset:atIndex:", m->buf[2][k], m->off[2][k], k);
    for (u64 k = 0; k < 31; k++) if (m->tglen[k]) MSG(void, enc, "setThreadgroupMemoryLength:atIndex:", m->tglen[k], k);
    ((void (*)(id, SEL, isz_t, isz_t))objc_msgSend)(enc, S(m->kind == 3 ? "dispatchThreadgroups:threadsPerThreadgroup:" : "dispatchThreads:threadsPerThreadgroup:"), m->g, m->t);
}
static void enc_exec(id enc, id icb, nsrange_t r, int compute) {
    micb_t *b = (micb_t *)icb;
    if (!b || b->isa != &MICB_c) return;
    for (u64 i = r.loc; i < r.loc + r.len && i < b->n; i++) { if (compute) run_compute(enc, b, i); else run_render(enc, b, i); }
}
static void re_exec(id s, SEL c, id icb, nsrange_t r) { (void)c; enc_exec(s, icb, r, 0); }
static void ce_exec(id s, SEL c, id icb, nsrange_t r) { (void)c; enc_exec(s, icb, r, 1); }
/* intervalo num buffer: struct { u32 location; u32 length; } (MTLIndirectCommandBufferExecutionRange) */
static void exec_ind(id s, id icb, id buf, u64 off, int compute) {
    u8 *p = buf ? MSG0(u8 *, buf, "contents") : 0;
    if (!p) return;
    u32 v[2];
    memcpy(v, p + off, 8);
    nsrange_t r = {v[0], v[1]};
    enc_exec(s, icb, r, compute);
}
static void re_exec_ind(id s, SEL c, id icb, id buf, u64 off) { (void)c; exec_ind(s, icb, buf, off, 0); }
static void ce_exec_ind(id s, SEL c, id icb, id buf, u64 off) { (void)c; exec_ind(s, icb, buf, off, 1); }
/* blit: reset e cópia de comandos */
static void be_reset(id s, SEL c, id icb, nsrange_t r) { (void)s; (void)c; if (icb) icb_reset(icb, 0, r); }
static void be_copy(id s, SEL c, id src, nsrange_t r, id dst, u64 di) {
    (void)s; (void)c;
    micb_t *a = (micb_t *)src, *b = (micb_t *)dst;
    if (!a || !b) return;
    for (u64 i = 0; i < r.len && r.loc + i < a->n && di + i < b->n; i++) {
        icmd_t *x = &a->cmd[r.loc + i], *y = &b->cmd[di + i];
        cmd_clear(y);
        *y = *x;
        if (y->pipe) objc_retain(y->pipe);
        if (y->ib) objc_retain(y->ib);
        for (int k = 0; k < 3; k++) for (int j = 0; j < 31; j++) if (y->buf[k][j]) objc_retain(y->buf[k][j]);
    }
}
static BOOL ret_yes(id s, SEL c) { (void)s; (void)c; return YES; }

void mtl_icb_register(cx_class_t *obj, cx_class_t *dev, cx_class_t *renc, cx_class_t *cenc, cx_class_t *benc, cx_class_t *rpipe, cx_class_t *cpipe) {
    cx_define_class(&MICB_c, &MICB_m, "MTLSoftIndirectCommandBuffer", obj, sizeof(micb_t) - sizeof(mobj_t));
    cx_add_method(&MICB_c, "dealloc", (void *)icb_dealloc);
    cx_add_method(&MICB_c, "size", (void *)icb_size);
    cx_add_method(&MICB_c, "indirectRenderCommandAtIndex:", (void *)icb_render_at);
    cx_add_method(&MICB_c, "indirectComputeCommandAtIndex:", (void *)icb_compute_at);
    cx_add_method(&MICB_c, "resetWithRange:", (void *)icb_reset);
    cx_add_method(&MICB_c, "gpuResourceID", (void *)icb_rid);
    cx_define_class(&MIRC_c, &MIRC_m, "MTLSoftIndirectRenderCommand", obj, sizeof(micmd_t) - sizeof(mobj_t));
    cx_add_method(&MIRC_c, "setRenderPipelineState:", (void *)ic_pipe);
    cx_add_method(&MIRC_c, "setVertexBuffer:offset:atIndex:", (void *)ic_vbuf);
    cx_add_method(&MIRC_c, "setFragmentBuffer:offset:atIndex:", (void *)ic_fbuf);
    cx_add_method(&MIRC_c, "drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:", (void *)ic_draw);
    cx_add_method(&MIRC_c, "drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:", (void *)ic_drawidx);
    cx_add_method(&MIRC_c, "reset", (void *)ic_reset);
    cx_add_method(&MIRC_c, "setCullMode:", (void *)ic_noop);
    cx_define_class(&MICC_c, &MICC_m, "MTLSoftIndirectComputeCommand", obj, sizeof(micmd_t) - sizeof(mobj_t));
    cx_add_method(&MICC_c, "setComputePipelineState:", (void *)ic_pipe);
    cx_add_method(&MICC_c, "setKernelBuffer:offset:atIndex:", (void *)ic_kbuf);
    cx_add_method(&MICC_c, "setThreadgroupMemoryLength:atIndex:", (void *)ic_tgmem);
    cx_add_method(&MICC_c, "concurrentDispatchThreadgroups:threadsPerThreadgroup:", (void *)ic_disp_tg);
    cx_add_method(&MICC_c, "concurrentDispatchThreads:threadsPerThreadgroup:", (void *)ic_disp_th);
    cx_add_method(&MICC_c, "setBarrier", (void *)ic_barrier);
    cx_add_method(&MICC_c, "clearBarrier", (void *)ic_clear_barrier);
    cx_add_method(&MICC_c, "reset", (void *)ic_reset);
    cx_add_method(dev, "newIndirectCommandBufferWithDescriptor:maxCommandCount:options:", (void *)dev_new_icb);
    cx_add_method(renc, "executeCommandsInBuffer:withRange:", (void *)re_exec);
    cx_add_method(renc, "executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:", (void *)re_exec_ind);
    cx_add_method(cenc, "executeCommandsInBuffer:withRange:", (void *)ce_exec);
    cx_add_method(cenc, "executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:", (void *)ce_exec_ind);
    cx_add_method(benc, "resetCommandsInBuffer:withRange:", (void *)be_reset);
    cx_add_method(benc, "copyIndirectCommandBuffer:sourceRange:destination:destinationIndex:", (void *)be_copy);
    cx_add_method(rpipe, "supportIndirectCommandBuffers", (void *)ret_yes);
    cx_add_method(cpipe, "supportIndirectCommandBuffers", (void *)ret_yes);
}

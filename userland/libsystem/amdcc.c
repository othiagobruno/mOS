/* mOS libSystem — compilador de kernels de compute do Metal para GCN/GFX9: ver amdcc.h.
 * Cada thread é uma lane da wave; valores ficam em VGPRs (um por componente); controle de fluxo divergente por
 * máscara de EXEC: if salva o EXEC num par de SGPRs, desliga as lanes que não entram e devolve no fim; laços
 * desligam as lanes cuja condição falhou até todas saírem; return junta as lanes num par "mortas" (RET) que todo
 * retorno de EXEC desconta. */
#include "amdcc.h"
#include "mtlint.h"
#include "libc.h"

typedef struct { int v, n, k; } val_t;          /* VGPR inicial, componentes, tipo escalar (K_FLOAT/K_INT/K_UINT/K_BOOL) */
typedef struct { var_t *var; int v, n, k; int sref; } loc_t;   /* sref >= 0: referência constant em SGPRs */
typedef struct {
    amdasm_t A;
    amdisa_u32 buf[16384];
    int vtop, vmax;            /* VGPRs: 0..2 ids da thread */
    int stop;                  /* próximo SGPR livre */
    int bufpair[AMDCC_MAX_BUF + 1];   /* par de SGPRs com o endereço do buffer (-1 = não usado) */
    int s_local, s_grid;       /* 4 SGPRs cada: tamanho do grupo e da grade */
    int s_ret;                 /* par: lanes que já retornaram */
    loc_t loc[256]; int nloc;
    int fail;
    char why[96];
    unsigned used;
} cc_t;

#define FAIL(C, msg) do { if (!(C)->fail) { (C)->fail = 1; snprintf((C)->why, sizeof (C)->why, "%s", msg); } } while (0)
static int kind_of(ty_t *t) {
    if (!t) return K_INT;
    int k = ty_scal(t);
    if (k == K_FLOAT) return K_FLOAT;
    if (k == K_UINT || k == K_USHORT || k == K_UCHAR) return K_UINT;
    if (k == K_BOOL) return K_BOOL;
    return K_INT;
}
static int ncomp(ty_t *t) { return t ? (t->k == K_VEC ? t->n : 1) : 1; }
static int num_ok(cc_t *C, ty_t *t) {
    if (!t) return 0;
    int k = ty_scal(t);
    if (t->k == K_MAT || t->k == K_ARRAY || t->k == K_STRUCT || t->k == K_PTR) { FAIL(C, "tipo não numérico"); return 0; }
    if (k != K_FLOAT && k != K_INT && k != K_UINT && k != K_BOOL) { FAIL(C, "tipo de 8/16/64 bits"); return 0; }
    return 1;
}
static int valloc(cc_t *C, int n) {
    int v = C->vtop;
    C->vtop += n;
    if (C->vtop > C->vmax) C->vmax = C->vtop;
    if (C->vtop > 250) FAIL(C, "VGPRs demais");
    return v;
}
static int salloc(cc_t *C, int n, int align) {
    while (C->stop % align) C->stop++;
    int s = C->stop;
    C->stop += n;
    if (C->stop > 96) FAIL(C, "SGPRs demais");
    return s;
}
/* ---- ramos: s_branch/s_cbranch com destino depois */
static int br(cc_t *C, unsigned op) { amd_sopp(&C->A, op, 0); return (int)C->A.n - 1; }
static void patch(cc_t *C, int at) { int off = (int)C->A.n - (at + 1); C->A.w[at] = (C->A.w[at] & ~0xFFFFu) | ((unsigned)off & 0xFFFF); }
static void br_to(cc_t *C, unsigned op, int target) { int off = target - ((int)C->A.n + 1); amd_sopp(&C->A, op, (unsigned)off & 0xFFFF); }
/* EXEC = saved & ~RET */
static void exec_restore(cc_t *C, int saved) { amd_sop2(&C->A, AMD_S_ANDN2_B64, AMD_EXEC_LO, AMD_S(saved), AMD_S(C->s_ret), 0); }

/* ---- conversões */
static val_t vmov_lit(cc_t *C, amdisa_u32 bits, int k) {
    val_t r = {valloc(C, 1), 1, k};
    amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)r.v, AMD_LIT, bits);
    return r;
}
static val_t conv(cc_t *C, val_t a, int k) {
    if (a.k == k) return a;
    val_t r = {valloc(C, a.n), a.n, k};
    for (int i = 0; i < a.n; i++) {
        unsigned s = AMD_V(a.v + i), d = (unsigned)(r.v + i);
        if (k == K_BOOL) {
            if (a.k == K_FLOAT) amd_vopc(&C->A, 0x4D /* v_cmp_neq_f32 */, AMD_ZERO, (unsigned)(a.v + i), 0);
            else amd_vopc(&C->A, 0xCD /* v_cmp_ne_u32 */, AMD_ZERO, (unsigned)(a.v + i), 0);
            amd_vop3(&C->A, 0x100 /* v_cndmask_b32 */, d, AMD_ZERO, AMD_IC(1), AMD_VCC_LO, 0, 0, 0);
        } else if (k == K_FLOAT) amd_vop1(&C->A, a.k == K_INT ? AMD_V_CVT_F32_I32 : AMD_V_CVT_F32_U32, d, s, 0);
        else if (a.k == K_FLOAT) amd_vop1(&C->A, k == K_INT ? AMD_V_CVT_I32_F32 : AMD_V_CVT_U32_F32, d, s, 0);
        else amd_vop1(&C->A, AMD_V_MOV_B32, d, s, 0);   /* int/uint/bool entre si: mesmos bits */
    }
    return r;
}
static val_t bcast(cc_t *C, val_t a, int n) {
    if (a.n == n || a.n != 1) return a;
    val_t r = {valloc(C, n), n, a.k};
    for (int i = 0; i < n; i++) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r.v + i), AMD_V(a.v), 0);
    return r;
}

static val_t expr(cc_t *C, node_t *n);
static void stmt(cc_t *C, node_t *s);

/* ---- endereço de p[i] (+ deslocamento) num par de VGPRs */
typedef struct { int v; int off; int base; ty_t *t; } addr_t;   /* base: par de SGPRs; v: par de VGPRs com o endereço */
static int lookup_buf(cc_t *C, node_t *p) {
    if (p->op != E_VAR || !p->var || p->var->attr != A_BUFFER) { FAIL(C, "ponteiro que não é [[buffer(n)]]"); return -1; }
    int i = p->var->idx;
    if (i < 0 || i > AMDCC_MAX_BUF || C->bufpair[i] < 0) { FAIL(C, "buffer fora da tabela"); return -1; }
    return C->bufpair[i];
}
static addr_t address(cc_t *C, node_t *n) {
    addr_t a = {-1, 0, -1, n->t};
    if (n->op == E_MEMBER) {
        addr_t b = address(C, n->a);
        b.off += n->off;
        b.t = n->t;
        return b;
    }
    node_t *p, *ix = 0;
    if (n->op == E_INDEX && n->a->t && n->a->t->k == K_PTR) { p = n->a; ix = n->b; }
    else if (n->op == E_DEREF && n->a->t && n->a->t->k == K_PTR) p = n->a;
    else { FAIL(C, "acesso à memória não suportado"); return a; }
    int base = lookup_buf(C, p);
    if (base < 0) return a;
    a.base = base;
    unsigned es = p->t->e && p->t->e->size ? (unsigned)p->t->e->size : 1;
    a.v = valloc(C, 2);
    if (!ix) {
        amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)a.v, AMD_S(base), 0);
        amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)a.v + 1, AMD_S(base + 1), 0);
        return a;
    }
    val_t i = expr(C, ix);
    if (C->fail) return a;
    if (i.k == K_FLOAT) { FAIL(C, "índice float"); return a; }
    unsigned sz;
    if (es <= 64) sz = AMD_IC((int)es);
    else { val_t s = vmov_lit(C, es, K_UINT); sz = AMD_V(s.v); }
    /* endereço = base + i * tamanho (64 bits, com sinal se o índice é int) */
    amd_vop3b(&C->A, i.k == K_INT ? 0x1E9 : 0x1E8, (unsigned)a.v, AMD_VCC_LO, AMD_V(i.v), sz, AMD_S(base));
    return a;
}
static unsigned load_op(int nd) { return nd == 1 ? AMD_GLOBAL_LOAD_DWORD : nd == 2 ? AMD_GLOBAL_LOAD_DWORDX2 : nd == 3 ? 22u : AMD_GLOBAL_LOAD_DWORDX4; }
static unsigned store_op(int nd) { return nd == 1 ? AMD_GLOBAL_STORE_DWORD : nd == 2 ? AMD_GLOBAL_STORE_DWORDX2 : nd == 3 ? 30u : AMD_GLOBAL_STORE_DWORDX4; }
static val_t load(cc_t *C, node_t *n) {
    val_t r = {0, 1, K_INT};
    if (!num_ok(C, n->t)) return r;
    addr_t a = address(C, n);
    if (C->fail) return r;
    if (a.off > 4095) { FAIL(C, "deslocamento grande"); return r; }
    r.n = ncomp(n->t);
    r.k = kind_of(n->t);
    r.v = valloc(C, r.n);
    amd_global(&C->A, load_op(r.n), (unsigned)r.v, (unsigned)a.v, 0, AMD_GLOBAL_OFF, a.off, 0);
    amd_sopp(&C->A, AMD_S_WAITCNT, amd_waitcnt(0, -1, -1));
    return r;
}
static void store(cc_t *C, node_t *l, val_t v) {
    if (!num_ok(C, l->t)) return;
    int k = kind_of(l->t), n = ncomp(l->t);
    v = bcast(C, conv(C, v, k == K_BOOL ? K_UINT : k), n);
    addr_t a = address(C, l);
    if (C->fail) return;
    if (a.off > 4095) { FAIL(C, "deslocamento grande"); return; }
    amd_global(&C->A, store_op(n), 0, (unsigned)a.v, (unsigned)v.v, AMD_GLOBAL_OFF, a.off, 0);
}

/* ---- embutidas de posição */
static val_t thread_pos(cc_t *C, int attr, int n) {
    val_t r = {valloc(C, n), n, K_UINT};
    for (int i = 0; i < n; i++) {
        unsigned d = (unsigned)(r.v + i);
        switch (attr) {
        case A_TPITG: amd_vop1(&C->A, AMD_V_MOV_B32, d, AMD_V(i), 0); break;
        case A_TGPIG: amd_vop1(&C->A, AMD_V_MOV_B32, d, AMD_S(2 + i), 0); break;
        case A_TPTG: amd_vop1(&C->A, AMD_V_MOV_B32, d, AMD_S(C->s_local + i), 0); break;
        case A_TPG: amd_vop1(&C->A, AMD_V_MOV_B32, d, AMD_S(C->s_grid + i), 0); break;
        case A_TPIG: {
            int t = salloc(C, 1, 1);
            amd_sop2(&C->A, AMD_S_MUL_I32, (unsigned)t, AMD_S(2 + i), AMD_S(C->s_local + i), 0);
            amd_vop2(&C->A, AMD_V_ADD_U32, d, AMD_S(t), (unsigned)i, 0);
            break;
        }
        case A_TIITG: {   /* x + y*lx + z*lx*ly */
            if (i) break;
            int t = salloc(C, 2, 1);
            amd_vop1(&C->A, AMD_V_MOV_B32, d, AMD_V(0), 0);
            val_t y = {valloc(C, 1), 1, K_UINT};
            amd_vop3(&C->A, AMD_V3_MUL_LO_U32, (unsigned)y.v, AMD_V(1), AMD_S(C->s_local), AMD_ZERO, 0, 0, 0);
            amd_vop2(&C->A, AMD_V_ADD_U32, d, AMD_V(y.v), d, 0);
            amd_sop2(&C->A, AMD_S_MUL_I32, (unsigned)t, AMD_S(C->s_local), AMD_S(C->s_local + 1), 0);
            amd_vop3(&C->A, AMD_V3_MUL_LO_U32, (unsigned)y.v, AMD_V(2), AMD_S(t), AMD_ZERO, 0, 0, 0);
            amd_vop2(&C->A, AMD_V_ADD_U32, d, AMD_V(y.v), d, 0);
            break;
        }
        default: FAIL(C, "atributo de thread não suportado");
        }
    }
    return r;
}

/* ---- operações */
static unsigned cmp_op(int op, int k) {
    static const unsigned F[6] = {0x41, 0x42, 0x43, 0x44, 0x4D, 0x46}, I[6] = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6}, U[6] = {0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE};
    int i = op == '<' ? 0 : op == 'e' ? 1 : op == 'l' ? 2 : op == '>' ? 3 : op == 'n' ? 4 : 5;
    return k == K_FLOAT ? F[i] : k == K_INT ? I[i] : U[i];
}
/* VCC <- comparação lane a lane de a[i] op b[i] */
static void cmp_vcc(cc_t *C, int op, val_t a, val_t b, int i) {
    amd_vopc(&C->A, cmp_op(op, a.k), AMD_V(a.v + i), (unsigned)(b.v + i), 0);
}
static val_t vcc_bool(cc_t *C) {
    val_t r = {valloc(C, 1), 1, K_BOOL};
    amd_vop3(&C->A, 0x100, (unsigned)r.v, AMD_ZERO, AMD_IC(1), AMD_VCC_LO, 0, 0, 0);
    return r;
}
static val_t binop(cc_t *C, int op, val_t a, val_t b, ty_t *rt) {
    int cmp = op == 'e' || op == 'n' || op == '<' || op == '>' || op == 'l' || op == 'g';
    int k = a.k == K_FLOAT || b.k == K_FLOAT ? K_FLOAT : a.k == K_UINT || b.k == K_UINT ? K_UINT : K_INT;
    if (op == 'L' || op == 'R') k = a.k == K_BOOL ? K_INT : a.k;
    int n = a.n > b.n ? a.n : b.n;
    a = bcast(C, conv(C, a, k), n);
    b = bcast(C, op == 'L' || op == 'R' ? conv(C, b, K_UINT) : conv(C, b, k), n);
    val_t r = {valloc(C, n), n, cmp ? K_BOOL : k};
    for (int i = 0; i < n; i++) {
        unsigned d = (unsigned)(r.v + i), s0 = AMD_V(a.v + i), s1 = (unsigned)(b.v + i);
        if (cmp) { cmp_vcc(C, op, a, b, i); amd_vop3(&C->A, 0x100, d, AMD_ZERO, AMD_IC(1), AMD_VCC_LO, 0, 0, 0); continue; }
        switch (op) {
        case '+': amd_vop2(&C->A, k == K_FLOAT ? AMD_V_ADD_F32 : AMD_V_ADD_U32, d, s0, s1, 0); break;
        case '-': amd_vop2(&C->A, k == K_FLOAT ? AMD_V_SUB_F32 : AMD_V_SUB_U32, d, s0, s1, 0); break;
        case '*':
            if (k == K_FLOAT) amd_vop2(&C->A, AMD_V_MUL_F32, d, s0, s1, 0);
            else amd_vop3(&C->A, AMD_V3_MUL_LO_U32, d, s0, AMD_V(b.v + i), AMD_ZERO, 0, 0, 0);
            break;
        case '/':
            if (k != K_FLOAT) { FAIL(C, "divisão inteira"); return r; }
            amd_vop1(&C->A, AMD_V_RCP_F32, d, AMD_V(b.v + i), 0);
            amd_vop2(&C->A, AMD_V_MUL_F32, d, s0, d, 0);
            break;
        case '&': amd_vop2(&C->A, AMD_V_AND_B32, d, s0, s1, 0); break;
        case '|': amd_vop2(&C->A, AMD_V_OR_B32, d, s0, s1, 0); break;
        case '^': amd_vop2(&C->A, AMD_V_XOR_B32, d, s0, s1, 0); break;
        /* deslocamentos: o primeiro operando do *rev é a quantidade */
        case 'L': amd_vop2(&C->A, AMD_V_LSHLREV_B32, d, AMD_V(b.v + i), (unsigned)(a.v + i), 0); break;
        case 'R': amd_vop2(&C->A, k == K_INT ? 17u /* v_ashrrev_i32 */ : AMD_V_LSHRREV_B32, d, AMD_V(b.v + i), (unsigned)(a.v + i), 0); break;
        default: FAIL(C, "operador não suportado"); return r;
        }
    }
    (void)rt;
    return r;
}
static val_t builtin(cc_t *C, node_t *n) {
    val_t a[3] = {{0}};
    int na = n->nv;
    if (na > 3) { FAIL(C, "embutida com muitos argumentos"); return a[0]; }
    for (int i = 0; i < na; i++) { a[i] = expr(C, n->v[i]); if (C->fail) return a[0]; }
    int k = kind_of(n->t), nc = ncomp(n->t);
    if (n->bi == B_DOT) {
        val_t x = conv(C, a[0], K_FLOAT), y = conv(C, a[1], K_FLOAT);
        val_t r = {valloc(C, 1), 1, K_FLOAT};
        amd_vop2(&C->A, AMD_V_MUL_F32, (unsigned)r.v, AMD_V(x.v), (unsigned)y.v, 0);
        for (int i = 1; i < x.n; i++) amd_vop3(&C->A, AMD_V3_FMA_F32, (unsigned)r.v, AMD_V(x.v + i), AMD_V(y.v + i), AMD_V(r.v), 0, 0, 0);
        return r;
    }
    if (n->bi == B_BARRIER) {
        amd_sopp(&C->A, AMD_S_WAITCNT, amd_waitcnt(0, -1, 0));
        amd_sopp(&C->A, AMD_S_BARRIER, 0);
        return (val_t){0, 0, K_INT};
    }
    if (n->bi >= B_LAST_GEN || !na) { FAIL(C, "embutida não suportada"); return a[0]; }
    for (int i = 0; i < na; i++) a[i] = bcast(C, conv(C, a[i], n->bi == B_SELECT && i == 2 ? K_BOOL : k), nc);
    val_t r = {valloc(C, nc), nc, k};
    for (int c = 0; c < nc; c++) {
        unsigned d = (unsigned)(r.v + c), x = AMD_V(a[0].v + c), y = na > 1 ? (unsigned)(a[1].v + c) : 0, z = na > 2 ? AMD_V(a[2].v + c) : 0;
        int f = k == K_FLOAT;
        switch (n->bi) {
        case B_MIN: amd_vop2(&C->A, f ? AMD_V_MIN_F32 : k == K_INT ? 12u : 14u, d, x, y, 0); break;
        case B_MAX: amd_vop2(&C->A, f ? AMD_V_MAX_F32 : k == K_INT ? 13u : 15u, d, x, y, 0); break;
        case B_CLAMP:
            amd_vop2(&C->A, f ? AMD_V_MAX_F32 : k == K_INT ? 13u : 15u, d, x, y, 0);
            amd_vop2(&C->A, f ? AMD_V_MIN_F32 : k == K_INT ? 12u : 14u, d, AMD_V(d), (unsigned)(a[2].v + c), 0);
            break;
        case B_ABS:
            if (f) amd_vop2(&C->A, AMD_V_AND_B32, d, AMD_LIT, (unsigned)(a[0].v + c), 0x7FFFFFFFu);
            else { amd_vop2(&C->A, AMD_V_SUB_U32, d, AMD_ZERO, (unsigned)(a[0].v + c), 0); amd_vop2(&C->A, 13u, d, x, d, 0); }
            break;
        case B_SQRT: case B_RSQRT: case B_FLOOR: case B_CEIL: case B_TRUNC: case B_FRACT: case B_RINT: case B_EXP2: case B_LOG2: {
            if (!f) { FAIL(C, "embutida float com inteiro"); return r; }
            unsigned op = n->bi == B_SQRT ? 39u : n->bi == B_RSQRT ? 36u : n->bi == B_FLOOR ? 31u : n->bi == B_CEIL ? 29u :
                          n->bi == B_TRUNC ? 28u : n->bi == B_FRACT ? 27u : n->bi == B_RINT ? 30u : n->bi == B_EXP2 ? 32u : 33u;
            amd_vop1(&C->A, op, d, x, 0);
            break;
        }
        case B_FMA: amd_vop3(&C->A, AMD_V3_FMA_F32, d, x, AMD_V(y), z, 0, 0, 0); break;
        case B_MIX:   /* x + (y - x) * t */
            amd_vop2(&C->A, AMD_V_SUB_F32, d, AMD_V(y), (unsigned)(a[0].v + c), 0);
            amd_vop3(&C->A, AMD_V3_FMA_F32, d, AMD_V(d), z, x, 0, 0, 0);
            break;
        case B_SELECT:   /* select(a, b, c): c ? b : a */
            amd_vopc(&C->A, 0xCD, AMD_ZERO, (unsigned)(a[2].v + c), 0);
            amd_vop2(&C->A, AMD_V_CNDMASK_B32, d, x, y, 0);
            break;
        default: FAIL(C, "embutida não suportada"); return r;
        }
    }
    return r;
}
static loc_t *loc_of(cc_t *C, var_t *v) { for (int i = C->nloc - 1; i >= 0; i--) if (C->loc[i].var == v) return &C->loc[i]; return 0; }
static val_t var_val(cc_t *C, node_t *n) {
    var_t *v = n->var;
    if (v->attr == A_TPIG || v->attr == A_TPITG || v->attr == A_TGPIG || v->attr == A_TPTG || v->attr == A_TPG || v->attr == A_TIITG)
        return thread_pos(C, v->attr, ncomp(v->t));
    loc_t *l = loc_of(C, v);
    if (!l) { FAIL(C, "variável não suportada (global, array ou ponteiro)"); return (val_t){0, 1, K_INT}; }
    if (l->sref >= 0) {   /* referência constant: valor uniforme em SGPRs */
        val_t r = {valloc(C, l->n), l->n, l->k};
        for (int i = 0; i < l->n; i++) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r.v + i), AMD_S(l->sref + i), 0);
        return r;
    }
    return (val_t){l->v, l->n, l->k};
}
/* lugar de uma atribuição a variável local: VGPRs e componentes (swizzle) */
static int local_target(cc_t *C, node_t *l, loc_t **out, int *comp, int *nc) {
    if (l->op == E_VAR) {
        loc_t *x = loc_of(C, l->var);
        if (!x || x->sref >= 0) return 0;
        *out = x; *nc = x->n;
        for (int i = 0; i < x->n; i++) comp[i] = i;
        return 1;
    }
    if (l->op == E_SWZ && l->a->op == E_VAR) {
        loc_t *x = loc_of(C, l->a->var);
        if (!x || x->sref >= 0) return 0;
        *out = x; *nc = l->nswz;
        for (int i = 0; i < l->nswz; i++) comp[i] = l->swz[i];
        return 1;
    }
    if (l->op == E_INDEX && l->a->op == E_VAR && l->a->t && l->a->t->k == K_VEC && l->b->op == E_LIT) {
        loc_t *x = loc_of(C, l->a->var);
        if (!x || x->sref >= 0) return 0;
        *out = x; *nc = 1; comp[0] = (int)l->b->iv;
        return 1;
    }
    return 0;
}
static val_t assign(cc_t *C, node_t *n) {
    node_t *l = n->a;
    val_t v = expr(C, n->b);
    if (C->fail) return v;
    if (n->sub != '=') { val_t cur = expr(C, l); if (C->fail) return v; v = binop(C, n->sub, cur, v, l->t); }
    loc_t *x;
    int comp[4], nc;
    if (local_target(C, l, &x, comp, &nc)) {
        v = bcast(C, conv(C, v, x->k), nc);
        for (int i = 0; i < nc; i++) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(x->v + comp[i]), AMD_V(v.v + (v.n > 1 ? i : 0)), 0);
        return v;
    }
    store(C, l, v);
    return v;
}
static val_t expr(cc_t *C, node_t *n) {
    val_t z = {0, 1, K_INT};
    if (C->fail || !n) { FAIL(C, "expressão vazia"); return z; }
    switch (n->op) {
    case E_LIT: {
        if (!num_ok(C, n->t)) return z;
        int k = kind_of(n->t);
        float f = (float)n->fv;
        amdisa_u32 bits;
        if (k == K_FLOAT) memcpy(&bits, &f, 4); else bits = (amdisa_u32)n->iv;
        return vmov_lit(C, bits, k);
    }
    case E_VAR:
        if (n->var->attr == A_BUFFER || n->var->ref) {
            loc_t *l = loc_of(C, n->var);
            if (l) return var_val(C, n);
            FAIL(C, "uso de buffer como valor");
            return z;
        }
        return var_val(C, n);
    case E_INDEX:
        if (n->a->t && n->a->t->k == K_PTR) return load(C, n);
        if (n->a->t && n->a->t->k == K_VEC && n->b->op == E_LIT) {
            val_t a = expr(C, n->a);
            int i = (int)n->b->iv;
            return (val_t){a.v + (i >= 0 && i < a.n ? i : 0), 1, a.k};
        }
        FAIL(C, "índice dinâmico em vetor ou array");
        return z;
    case E_DEREF: return load(C, n);
    case E_MEMBER: return load(C, n);
    case E_SWZ: {
        val_t a = expr(C, n->a);
        if (C->fail) return z;
        val_t r = {valloc(C, n->nswz), n->nswz, a.k};
        for (int i = 0; i < n->nswz; i++) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r.v + i), AMD_V(a.v + n->swz[i]), 0);
        return r;
    }
    case E_CAST: {
        if (!num_ok(C, n->t)) return z;
        val_t a = expr(C, n->a);
        return bcast(C, conv(C, a, kind_of(n->t)), ncomp(n->t));
    }
    case E_CTOR: {
        if (!num_ok(C, n->t)) return z;
        int nc = ncomp(n->t), k = kind_of(n->t), at = 0;
        val_t r = {valloc(C, nc), nc, k};
        for (int i = 0; i < n->nv && at < nc; i++) {
            val_t a = conv(C, expr(C, n->v[i]), k);
            if (C->fail) return z;
            if (n->nv == 1 && a.n == 1) { for (; at < nc; at++) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r.v + at), AMD_V(a.v), 0); break; }
            for (int c = 0; c < a.n && at < nc; c++, at++) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r.v + at), AMD_V(a.v + c), 0);
        }
        return r;
    }
    case E_UN: {
        val_t a = expr(C, n->a);
        if (C->fail) return z;
        if (n->sub == '+') return a;
        if (n->sub == '!') {
            val_t b = conv(C, a, K_BOOL);
            val_t r = {valloc(C, b.n), b.n, K_BOOL};
            for (int i = 0; i < b.n; i++) amd_vop2(&C->A, AMD_V_XOR_B32, (unsigned)(r.v + i), AMD_IC(1), (unsigned)(b.v + i), 0);
            return r;
        }
        val_t r = {valloc(C, a.n), a.n, a.k};
        for (int i = 0; i < a.n; i++) {
            if (n->sub == '~') amd_vop1(&C->A, 43u /* v_not_b32 */, (unsigned)(r.v + i), AMD_V(a.v + i), 0);
            else if (a.k == K_FLOAT) amd_vop2(&C->A, AMD_V_XOR_B32, (unsigned)(r.v + i), AMD_LIT, (unsigned)(a.v + i), 0x80000000u);
            else amd_vop2(&C->A, AMD_V_SUB_U32, (unsigned)(r.v + i), AMD_ZERO, (unsigned)(a.v + i), 0);
        }
        return r;
    }
    case E_BIN: {
        if (!num_ok(C, n->t)) return z;
        val_t a = expr(C, n->a), b = expr(C, n->b);
        if (C->fail) return z;
        return binop(C, n->sub, a, b, n->t);
    }
    case E_LAND: case E_LOR: {
        /* os dois lados são avaliados (sem efeitos colaterais nos operandos que o compilador aceita) */
        val_t a = conv(C, expr(C, n->a), K_BOOL), b = conv(C, expr(C, n->b), K_BOOL);
        if (C->fail) return z;
        val_t r = {valloc(C, 1), 1, K_BOOL};
        amd_vop2(&C->A, n->op == E_LAND ? AMD_V_AND_B32 : AMD_V_OR_B32, (unsigned)r.v, AMD_V(a.v), (unsigned)b.v, 0);
        return r;
    }
    case E_COND: {
        val_t c = conv(C, expr(C, n->a), K_BOOL), x = expr(C, n->b), y = expr(C, n->c);
        if (C->fail) return z;
        int k = kind_of(n->t), nc = ncomp(n->t);
        x = bcast(C, conv(C, x, k), nc);
        y = bcast(C, conv(C, y, k), nc);
        val_t r = {valloc(C, nc), nc, k};
        for (int i = 0; i < nc; i++) {
            amd_vopc(&C->A, 0xCD, AMD_ZERO, (unsigned)(c.v + (c.n > 1 ? i : 0)), 0);
            amd_vop2(&C->A, AMD_V_CNDMASK_B32, (unsigned)(r.v + i), AMD_V(y.v + i), (unsigned)(x.v + i), 0);
        }
        return r;
    }
    case E_ASSIGN: return assign(C, n);
    case E_PREINC: case E_PREDEC: case E_POSTINC: case E_POSTDEC: {
        loc_t *x;
        int comp[4], nc;
        if (!local_target(C, n->a, &x, comp, &nc) || nc != 1) { FAIL(C, "++/-- fora de variável local"); return z; }
        val_t old = {valloc(C, 1), 1, x->k};
        unsigned d = (unsigned)(x->v + comp[0]);
        amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)old.v, AMD_V(d), 0);
        int inc = n->op == E_PREINC || n->op == E_POSTINC;
        /* d = d + 1 ou d = d - 1 (subrev: vsrc1 - src0) */
        if (x->k == K_FLOAT) amd_vop2(&C->A, inc ? AMD_V_ADD_F32 : 3u /* v_subrev_f32 */, d, AMD_F_ONE, d, 0);
        else amd_vop2(&C->A, inc ? AMD_V_ADD_U32 : 54u /* v_subrev_u32 */, d, AMD_IC(1), d, 0);
        return n->op == E_PREINC || n->op == E_PREDEC ? (val_t){(int)d, 1, x->k} : old;
    }
    case E_BI: return builtin(C, n);
    case E_COMMA: expr(C, n->a); return expr(C, n->b);
    }
    FAIL(C, "expressão não suportada");
    return z;
}

/* ---- comandos */
static void cond_exec(cc_t *C, node_t *cond, int saved) {
    /* EXEC &= cond (as lanes cuja condição é falsa saem); saved recebe o EXEC de antes */
    val_t c = conv(C, expr(C, cond), K_BOOL);
    if (C->fail) return;
    amd_vopc(&C->A, 0xCD, AMD_ZERO, (unsigned)c.v, 0);
    if (saved >= 0) amd_sop1(&C->A, AMD_S_AND_SAVEEXEC_B64, (unsigned)saved, AMD_VCC_LO, 0);
    else amd_sop2(&C->A, AMD_S_AND_B64, AMD_EXEC_LO, AMD_EXEC_LO, AMD_VCC_LO, 0);
}
static void stmt(cc_t *C, node_t *s) {
    if (C->fail || !s) return;
    int vmark = C->vtop, smark = C->stop;
    switch (s->op) {
    case S_NOP: break;
    case S_EXPR: expr(C, s->a); C->vtop = vmark; break;
    case S_BLOCK: {
        int nl = C->nloc;
        for (int i = 0; i < s->nv && !C->fail; i++) stmt(C, s->v[i]);
        C->nloc = nl;
        C->vtop = vmark;   /* variáveis do bloco somem no fim dele */
        C->stop = smark;
        return;
    }
    case S_DECL: {
        var_t *v = s->var;
        if (v->ref || !num_ok(C, v->t)) { FAIL(C, "declaração não suportada"); return; }
        if (C->nloc >= 256) { FAIL(C, "variáveis demais"); return; }
        int nc = ncomp(v->t), k = kind_of(v->t);
        val_t init = {0, 0, k};
        if (s->a) { init = expr(C, s->a); if (C->fail) return; init = bcast(C, conv(C, init, k), nc); }
        C->vtop = vmark;
        int r = valloc(C, nc);
        for (int i = 0; i < nc; i++) {
            if (s->a) amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r + i), AMD_V(init.v + i), 0);
            else amd_vop1(&C->A, AMD_V_MOV_B32, (unsigned)(r + i), AMD_ZERO, 0);
        }
        /* init pode ocupar VGPRs acima de r: a cópia lê antes de r ser sobrescrito só se init.v >= r */
        C->loc[C->nloc++] = (loc_t){v, r, nc, k, -1};
        return;
    }
    case S_IF: {
        int saved = salloc(C, 2, 2), cm = salloc(C, 2, 2);
        cond_exec(C, s->a, saved);
        amd_sop1(&C->A, AMD_S_MOV_B64, (unsigned)cm, AMD_EXEC_LO, 0);   /* lanes que entram no então */
        C->vtop = vmark;
        int j = br(C, AMD_S_CBRANCH_EXECZ);
        stmt(C, s->b);
        patch(C, j);
        if (s->c) {
            /* senão: as lanes de antes que não entraram e não retornaram */
            amd_sop2(&C->A, AMD_S_ANDN2_B64, AMD_EXEC_LO, AMD_S(saved), AMD_S(cm), 0);
            amd_sop2(&C->A, AMD_S_ANDN2_B64, AMD_EXEC_LO, AMD_EXEC_LO, AMD_S(C->s_ret), 0);
            int j2 = br(C, AMD_S_CBRANCH_EXECZ);
            stmt(C, s->c);
            patch(C, j2);
        }
        exec_restore(C, saved);
        break;
    }
    case S_FOR: case S_WHILE: {
        node_t *init = s->op == S_FOR ? s->a : 0, *cond = s->op == S_FOR ? s->b : s->a, *step = s->op == S_FOR ? s->c : 0, *body = s->op == S_FOR ? s->d : s->b;
        int nl = C->nloc;
        if (init) stmt(C, init);
        int saved = salloc(C, 2, 2);
        amd_sop1(&C->A, AMD_S_MOV_B64, (unsigned)saved, AMD_EXEC_LO, 0);
        int head = (int)C->A.n;
        int vm2 = C->vtop;
        if (cond) cond_exec(C, cond, -1);
        C->vtop = vm2;
        int j = br(C, AMD_S_CBRANCH_EXECZ);
        stmt(C, body);
        if (step) { expr(C, step); C->vtop = vm2; }
        br_to(C, AMD_S_BRANCH, head);
        patch(C, j);
        exec_restore(C, saved);
        C->nloc = nl;
        break;
    }
    case S_RET:
        if (s->a) { FAIL(C, "kernel devolvendo valor"); return; }
        amd_sop2(&C->A, AMD_S_OR_B64, (unsigned)C->s_ret, AMD_S(C->s_ret), AMD_EXEC_LO, 0);
        amd_sop1(&C->A, AMD_S_MOV_B64, AMD_EXEC_LO, AMD_ZERO, 0);
        break;
    case S_BREAK: case S_CONT: FAIL(C, "break/continue"); return;
    default: FAIL(C, "comando não suportado"); return;
    }
    C->vtop = vmark;
    C->stop = smark;
}

int amdcc_compile(msl_fn_t *f, amdcc_kernel_t *k) {
    memset(k, 0, sizeof *k);
    cc_t *C = calloc(1, sizeof *C);
    if (!C) { snprintf(k->why, sizeof k->why, "sem memória"); return 0; }
    amdasm_init(&C->A, C->buf, 16384);
    C->vtop = C->vmax = 3;
    C->stop = 6;
    for (int i = 0; i <= AMDCC_MAX_BUF; i++) C->bufpair[i] = -1;
    if (!f || f->stage != MSL_KERNEL) FAIL(C, "não é kernel");
    if (f && f->tgstatic) FAIL(C, "memória threadgroup");
    /* prólogo: endereços dos buffers, tamanhos do grupo e da grade, máscara de retorno */
    C->s_local = salloc(C, 4, 4);
    C->s_grid = salloc(C, 4, 4);
    amd_smem(&C->A, AMD_S_LOAD_DWORDX4, (unsigned)C->s_local, 0, AMDCC_ARGS_LOCAL, 0);
    amd_smem(&C->A, AMD_S_LOAD_DWORDX4, (unsigned)C->s_grid, 0, AMDCC_ARGS_GRID, 0);
    for (int i = 0; f && i < f->np && !C->fail; i++) {
        var_t *p = f->params[i];
        if (p->attr == A_BUFFER) {
            if (p->idx < 0 || p->idx > AMDCC_MAX_BUF) { FAIL(C, "índice de buffer"); break; }
            if (C->bufpair[p->idx] < 0) {
                C->bufpair[p->idx] = salloc(C, 2, 2);
                amd_smem(&C->A, AMD_S_LOAD_DWORDX2, (unsigned)C->bufpair[p->idx], 0, 8u * (unsigned)p->idx, 0);
                C->used |= 1u << p->idx;
            }
            if (p->ref || p->t->k != K_PTR) {
                /* constant T &x [[buffer(n)]]: lido uma vez para SGPRs */
                if (!num_ok(C, p->t)) break;
                int nc = ncomp(p->t);
                int s = salloc(C, nc == 3 ? 4 : nc, nc >= 4 ? 4 : nc >= 2 ? 2 : 1);
                amd_sopp(&C->A, AMD_S_WAITCNT, amd_waitcnt(-1, -1, 0));
                amd_smem(&C->A, nc == 1 ? AMD_S_LOAD_DWORD : nc == 2 ? AMD_S_LOAD_DWORDX2 : AMD_S_LOAD_DWORDX4, (unsigned)s, (unsigned)C->bufpair[p->idx], 0, 0);
                C->loc[C->nloc++] = (loc_t){p, -1, nc, kind_of(p->t), s};
            }
        } else if (p->attr != A_TPIG && p->attr != A_TPITG && p->attr != A_TGPIG && p->attr != A_TPTG && p->attr != A_TPG && p->attr != A_TIITG)
            FAIL(C, "parâmetro não suportado (textura, sampler, threadgroup...)");
    }
    C->s_ret = salloc(C, 2, 2);
    amd_sop1(&C->A, AMD_S_MOV_B64, (unsigned)C->s_ret, AMD_ZERO, 0);
    amd_sopp(&C->A, AMD_S_WAITCNT, amd_waitcnt(-1, -1, 0));
    /* grade que não é múltipla do grupo (dispatchThreads): lanes com posição >= threads_per_grid não rodam */
    {
        int t = salloc(C, 1, 1), v = valloc(C, 1);
        for (int d = 0; d < 3; d++) {
            amd_sop2(&C->A, AMD_S_MUL_I32, (unsigned)t, AMD_S(2 + d), AMD_S(C->s_local + d), 0);
            amd_vop2(&C->A, AMD_V_ADD_U32, (unsigned)v, AMD_S(t), (unsigned)d, 0);
            amd_vopc(&C->A, 0xCC /* v_cmp_gt_u32: grid > posição */, AMD_S(C->s_grid + d), (unsigned)v, 0);
            amd_sop2(&C->A, AMD_S_AND_B64, AMD_EXEC_LO, AMD_EXEC_LO, AMD_VCC_LO, 0);
        }
        C->vtop = 3;
    }
    if (f && !C->fail) stmt(C, f->body);
    amd_sopp(&C->A, AMD_S_ENDPGM, 0);
    if (C->A.err) FAIL(C, "código grande demais ou operando inválido");
    if (C->fail) { snprintf(k->why, sizeof k->why, "%s", C->why); free(C); return 0; }
    k->code = malloc(sizeof(amdisa_u32) * C->A.n);
    if (!k->code) { free(C); snprintf(k->why, sizeof k->why, "sem memória"); return 0; }
    memcpy(k->code, C->A.w, sizeof(amdisa_u32) * C->A.n);
    k->ncode = C->A.n;
    unsigned nv = (unsigned)C->vmax > C->A.max_vgpr ? (unsigned)C->vmax : C->A.max_vgpr;
    unsigned ns = (unsigned)C->stop > C->A.max_sgpr ? (unsigned)C->stop : C->A.max_sgpr;
    k->rsrc1 = amd_rsrc1(nv, ns);
    k->rsrc2 = amd_rsrc2(2, 7, 3);
    k->nuser = 2;
    k->used_buf = C->used;
    free(C);
    return 1;
}
void amdcc_free(amdcc_kernel_t *k) { free(k->code); k->code = 0; }

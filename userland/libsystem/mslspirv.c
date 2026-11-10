/* Tradução de um subconjunto de MSL para SPIR-V (Shader, GLCompute).
 *
 * O módulo é montado neste arquivo. O interpretador em mtlexec.c continua
 * executando a mesma árvore na CPU. O que não cabe no subconjunto não gera
 * módulo; msl_fn_spirv_note explica o motivo.
 *
 * Cabe: kernel; ponteiro device para float ou uint com [[buffer(n)]];
 * [[thread_position_in_grid]] como uint ou uint3; literais; aritmética
 * escalar do mesmo tipo; índice; atribuição; if/else; local escalar.
 * O LocalSize do módulo é 1x1x1. O dispatch do interpretador é que usa o
 * tamanho real do grupo: a ABI de GPU pede virgl ou venus, e este SPIR-V
 * ainda não é embrulhado nesse formato.
 */
#include "mtlint.h"

enum {
    OP_MEMORY_MODEL = 14, OP_ENTRY = 15, OP_EXEC_MODE = 16, OP_CAP = 17,
    OP_TYPE_VOID = 19, OP_TYPE_BOOL = 20, OP_TYPE_INT = 21, OP_TYPE_FLOAT = 22,
    OP_TYPE_VECTOR = 23, OP_TYPE_RTARR = 29, OP_TYPE_STRUCT = 30, OP_TYPE_PTR = 32,
    OP_TYPE_FN = 33, OP_CONST = 43, OP_FUNCTION = 54, OP_FN_END = 56,
    OP_VARIABLE = 59, OP_LOAD = 61, OP_STORE = 62, OP_ACCESS = 65,
    OP_DECORATE = 71, OP_MEMBER = 72, OP_EXTRACT = 81, OP_FNEG = 127,
    OP_IADD = 128, OP_FADD = 129, OP_ISUB = 130, OP_FSUB = 131,
    OP_IMUL = 132, OP_FMUL = 133, OP_UDIV = 134, OP_SDIV = 135, OP_FDIV = 136,
    OP_LOGICAL_NOT = 168, OP_IEQUAL = 170, OP_FORD_LT = 184, OP_FORD_GT = 190,
    OP_SEL_MERGE = 247, OP_LABEL = 248, OP_BRANCH = 249, OP_BRANCH_COND = 250,
    OP_RETURN = 253
};

enum { SK_SSBO_F = 1, SK_SSBO_U, SK_LOC_F, SK_LOC_U, SK_GID, SK_GID3 };
enum { VK_F = 1, VK_U, VK_B, VK_U3, VK_PF, VK_PU };

typedef struct { var_t *var; u32 id; int sk; } slot_t;
typedef struct { u32 id; int k; } val_t;

typedef struct {
    u32 *ty, *dec, *fn, *hd;
    u32 tn, tc, dn, dc, fnn, fnc, hn, hc;
    u32 next;
    int bad, ended;
    char note[80];
    u32 ty_void, ty_bool, ty_f, ty_u, ty_u3, ty_fn;
    u32 ptr_f, ptr_u, ptr_fn_f, ptr_fn_u;
    u32 gid, fn_id;
    slot_t sl[64];
    int ns;
} bld_t;

static void fail(bld_t *b, const char *s) {
    if (b->bad) return;
    b->bad = 1;
    snprintf(b->note, sizeof b->note, "%s", s);
}
static void push(u32 **w, u32 *n, u32 *cap, u32 v) {
    if (*n == *cap) {
        u32 c = *cap ? *cap * 2 : 64;
        u32 *p = realloc(*w, (size_t)c * sizeof *p);
        if (!p) return;
        *w = p;
        *cap = c;
    }
    (*w)[(*n)++] = v;
}
static void op(u32 **w, u32 *n, u32 *cap, u32 code, const u32 *a, int na) {
    push(w, n, cap, (u32)((na + 1) << 16) | code);
    for (int i = 0; i < na; i++) push(w, n, cap, a[i]);
}
static u32 fresh(bld_t *b) { return b->next++; }
static void ty(bld_t *b, u32 code, const u32 *a, int na) { op(&b->ty, &b->tn, &b->tc, code, a, na); }
static void fn(bld_t *b, u32 code, const u32 *a, int na) { op(&b->fn, &b->fnn, &b->fnc, code, a, na); }
static void dec1(bld_t *b, u32 target, u32 d) {
    u32 a[2] = {target, d};
    op(&b->dec, &b->dn, &b->dc, OP_DECORATE, a, 2);
}
static void dec2(bld_t *b, u32 target, u32 d, u32 lit) {
    u32 a[3] = {target, d, lit};
    op(&b->dec, &b->dn, &b->dc, OP_DECORATE, a, 3);
}
static void member_off(bld_t *b, u32 st) {
    u32 a[4] = {st, 0, 35, 0};
    op(&b->dec, &b->dn, &b->dc, OP_MEMBER, a, 4);
}

static slot_t *find_var(bld_t *b, var_t *v) {
    for (int i = 0; i < b->ns; i++) if (b->sl[i].var == v) return &b->sl[i];
    return 0;
}
static void add_slot(bld_t *b, var_t *v, u32 id, int sk) {
    if (b->ns >= 64) { fail(b, "parâmetros demais para o SPIR-V"); return; }
    b->sl[b->ns].var = v;
    b->sl[b->ns].id = id;
    b->sl[b->ns].sk = sk;
    b->ns++;
}
static int is_f(const ty_t *t) { return t && t->k == K_FLOAT; }
static int is_u(const ty_t *t) { return t && (t->k == K_UINT || t->k == K_INT); }

static u32 c_u(bld_t *b, u32 v) {
    u32 id = fresh(b);
    u32 a[3] = {b->ty_u, id, v};
    ty(b, OP_CONST, a, 3);
    return id;
}
static u32 c_f(bld_t *b, float f) {
    u32 bits, id = fresh(b);
    memcpy(&bits, &f, 4);
    u32 a[3] = {b->ty_f, id, bits};
    ty(b, OP_CONST, a, 3);
    return id;
}

static val_t ev(bld_t *b, node_t *n, int lval);
static void stmt(bld_t *b, node_t *n);

static val_t binop(bld_t *b, node_t *n) {
    val_t z = {0, 0};
    val_t l = ev(b, n->a, 0), r = ev(b, n->b, 0);
    if (b->bad) return z;
    if (l.k != r.k || (l.k != VK_F && l.k != VK_U)) { fail(b, "aritmética fora de float/uint escalar"); return z; }
    u32 opc = 0;
    int cmp = 0;
    switch (n->sub) {
    case '+': opc = l.k == VK_F ? OP_FADD : OP_IADD; break;
    case '-': opc = l.k == VK_F ? OP_FSUB : OP_ISUB; break;
    case '*': opc = l.k == VK_F ? OP_FMUL : OP_IMUL; break;
    case '/': opc = l.k == VK_F ? OP_FDIV : OP_UDIV; break;
    case '>': opc = OP_FORD_GT; cmp = 1; break;
    case '<': opc = OP_FORD_LT; cmp = 1; break;
    case 'e': opc = OP_IEQUAL; cmp = 1; break;
    default: fail(b, "operador ainda não vira SPIR-V"); return z;
    }
    if (cmp && l.k != VK_F && opc != OP_IEQUAL) { fail(b, "comparação ainda não vira SPIR-V"); return z; }
    val_t o;
    o.k = cmp ? VK_B : l.k;
    o.id = fresh(b);
    u32 t = o.k == VK_B ? b->ty_bool : o.k == VK_F ? b->ty_f : b->ty_u;
    u32 a[4] = {t, o.id, l.id, r.id};
    fn(b, opc, a, 4);
    return o;
}

static val_t ev(bld_t *b, node_t *n, int lval) {
    val_t z = {0, 0};
    if (!n || b->bad) return z;
    if (n->op == E_LIT) {
        val_t v;
        if (is_f(n->t)) { v.id = c_f(b, (float)n->fv); v.k = VK_F; return v; }
        if (is_u(n->t)) { v.id = c_u(b, (u32)n->iv); v.k = VK_U; return v; }
        fail(b, "literal fora de float/uint");
        return z;
    }
    if (n->op == E_VAR) {
        slot_t *s = find_var(b, n->var);
        if (!s) { fail(b, "variável sem lugar no SPIR-V"); return z; }
        if (s->sk == SK_SSBO_F || s->sk == SK_SSBO_U) {
            val_t v;
            v.id = s->id;
            v.k = s->sk == SK_SSBO_F ? VK_PF : VK_PU;
            return v;
        }
        if (s->sk == SK_LOC_F || s->sk == SK_LOC_U) {
            if (lval) {
                val_t v;
                v.id = s->id;
                v.k = s->sk == SK_LOC_F ? VK_PF : VK_PU;
                return v;
            }
            val_t v;
            v.k = s->sk == SK_LOC_F ? VK_F : VK_U;
            v.id = fresh(b);
            u32 a[3] = {v.k == VK_F ? b->ty_f : b->ty_u, v.id, s->id};
            fn(b, OP_LOAD, a, 3);
            return v;
        }
        if (s->sk == SK_GID || s->sk == SK_GID3) {
            val_t v;
            v.k = s->sk == SK_GID3 ? VK_U3 : VK_U;
            v.id = fresh(b);
            u32 a[3] = {v.k == VK_U3 ? b->ty_u3 : b->ty_u, v.id, s->id};
            fn(b, OP_LOAD, a, 3);
            return v;
        }
        fail(b, "variável sem lugar no SPIR-V");
        return z;
    }
    if (n->op == E_INDEX) {
        val_t base = ev(b, n->a, 0);
        val_t ix = ev(b, n->b, 0);
        if (b->bad) return z;
        if (base.k != VK_PF && base.k != VK_PU) { fail(b, "índice só em ponteiro de buffer"); return z; }
        if (ix.k == VK_U3) {
            u32 x = fresh(b);
            u32 a[4] = {b->ty_u, x, ix.id, 0};
            fn(b, OP_EXTRACT, a, 4);
            ix.id = x;
            ix.k = VK_U;
        }
        if (ix.k != VK_U) { fail(b, "índice precisa ser uint"); return z; }
        u32 zero = c_u(b, 0);
        u32 el = fresh(b);
        u32 pty = base.k == VK_PF ? b->ptr_f : b->ptr_u;
        u32 a[5] = {pty, el, base.id, zero, ix.id};
        fn(b, OP_ACCESS, a, 5);
        if (lval) {
            val_t v;
            v.id = el;
            v.k = base.k;
            return v;
        }
        val_t v;
        v.k = base.k == VK_PF ? VK_F : VK_U;
        v.id = fresh(b);
        u32 ld[3] = {v.k == VK_F ? b->ty_f : b->ty_u, v.id, el};
        fn(b, OP_LOAD, ld, 3);
        return v;
    }
    if (n->op == E_BIN) return binop(b, n);
    if (n->op == E_UN && n->sub == '-' && n->a && is_f(n->a->t)) {
        val_t a = ev(b, n->a, 0);
        val_t v;
        v.id = fresh(b);
        v.k = VK_F;
        u32 opv[3] = {b->ty_f, v.id, a.id};
        fn(b, OP_FNEG, opv, 3);
        return v;
    }
    if (n->op == E_UN && n->sub == '!') {
        val_t a = ev(b, n->a, 0);
        if (a.k != VK_B) { fail(b, "'!' fora de bool"); return z; }
        val_t v;
        v.id = fresh(b);
        v.k = VK_B;
        u32 opv[3] = {b->ty_bool, v.id, a.id};
        fn(b, OP_LOGICAL_NOT, opv, 3);
        return v;
    }
    if (n->op == E_ASSIGN && n->sub == '=') {
        val_t dst = ev(b, n->a, 1);
        val_t src = ev(b, n->b, 0);
        if (b->bad) return z;
        int dk = dst.k == VK_PF ? VK_F : dst.k == VK_PU ? VK_U : 0;
        if (!dk || dk != src.k) { fail(b, "atribuição com tipos diferentes"); return z; }
        u32 a[2] = {dst.id, src.id};
        fn(b, OP_STORE, a, 2);
        return src;
    }
    fail(b, "expressão ainda não vira SPIR-V");
    return z;
}

static void stmt(bld_t *b, node_t *n) {
    if (!n || b->bad || b->ended) return;
    if (n->op == S_NOP) return;
    if (n->op == S_BLOCK) {
        for (int i = 0; i < n->nv; i++) stmt(b, n->v[i]);
        return;
    }
    if (n->op == S_EXPR) { ev(b, n->a, 0); return; }
    if (n->op == S_RET) {
        if (n->a) { fail(b, "return com valor ainda não vira SPIR-V"); return; }
        fn(b, OP_RETURN, 0, 0);
        b->ended = 1;
        return;
    }
    if (n->op == S_DECL) {
        if (!n->var || !n->var->t) { fail(b, "declaração sem tipo"); return; }
        int sk, vk;
        u32 pty;
        if (n->var->t->k == K_FLOAT) { sk = SK_LOC_F; vk = VK_F; pty = b->ptr_fn_f; }
        else if (is_u(n->var->t)) { sk = SK_LOC_U; vk = VK_U; pty = b->ptr_fn_u; }
        else { fail(b, "local fora de float/uint"); return; }
        u32 id = fresh(b);
        u32 a[3] = {pty, id, 7};
        fn(b, OP_VARIABLE, a, 3);
        add_slot(b, n->var, id, sk);
        if (n->a) {
            val_t src = ev(b, n->a, 0);
            if (src.k != vk) { fail(b, "inicializador incompatível"); return; }
            u32 st[2] = {id, src.id};
            fn(b, OP_STORE, st, 2);
        }
        return;
    }
    if (n->op == S_IF) {
        val_t c = ev(b, n->a, 0);
        if (c.k != VK_B) { fail(b, "if sem condição bool"); return; }
        u32 th = fresh(b), el = fresh(b), mg = fresh(b);
        u32 sm[2] = {mg, 0};
        fn(b, OP_SEL_MERGE, sm, 2);
        u32 bc[3] = {c.id, th, el};
        fn(b, OP_BRANCH_COND, bc, 3);
        u32 lb[1];
        lb[0] = th;
        fn(b, OP_LABEL, lb, 1);
        stmt(b, n->b);
        if (b->ended) { fail(b, "return dentro de if ainda não vira SPIR-V"); return; }
        lb[0] = mg;
        fn(b, OP_BRANCH, lb, 1);
        lb[0] = el;
        fn(b, OP_LABEL, lb, 1);
        if (n->c) stmt(b, n->c);
        if (b->ended) { fail(b, "return dentro de if ainda não vira SPIR-V"); return; }
        lb[0] = mg;
        fn(b, OP_BRANCH, lb, 1);
        lb[0] = mg;
        fn(b, OP_LABEL, lb, 1);
        return;
    }
    fail(b, "comando ainda não vira SPIR-V");
}

static u32 ptr_ty(bld_t *b, u32 to, u32 sc) {
    u32 id = fresh(b);
    u32 a[3] = {id, sc, to};
    ty(b, OP_TYPE_PTR, a, 3);
    return id;
}
static void ssbo(bld_t *b, int flt, u32 *struct_ty, u32 *var_ty) {
    u32 elem = flt ? b->ty_f : b->ty_u;
    u32 rt = fresh(b);
    u32 rta[2] = {rt, elem};
    ty(b, OP_TYPE_RTARR, rta, 2);
    dec2(b, rt, 6, flt ? 4u : 4u);
    u32 st = fresh(b);
    u32 sta[2] = {st, rt};
    ty(b, OP_TYPE_STRUCT, sta, 2);
    dec1(b, st, 2);
    member_off(b, st);
    *struct_ty = st;
    *var_ty = ptr_ty(b, st, 12);
}

static void build(bld_t *b, msl_fn_t *f) {
    b->next = 1;
    b->ty_void = fresh(b);
    u32 tv[1] = {b->ty_void};
    ty(b, OP_TYPE_VOID, tv, 1);
    b->ty_bool = fresh(b);
    tv[0] = b->ty_bool;
    ty(b, OP_TYPE_BOOL, tv, 1);
    b->ty_f = fresh(b);
    u32 tf[2] = {b->ty_f, 32};
    ty(b, OP_TYPE_FLOAT, tf, 2);
    b->ty_u = fresh(b);
    u32 tu[3] = {b->ty_u, 32, 0};
    ty(b, OP_TYPE_INT, tu, 3);
    b->ty_u3 = fresh(b);
    u32 t3[3] = {b->ty_u3, b->ty_u, 3};
    ty(b, OP_TYPE_VECTOR, t3, 3);
    b->ty_fn = fresh(b);
    u32 tfn[2] = {b->ty_fn, b->ty_void};
    ty(b, OP_TYPE_FN, tfn, 2);
    u32 st_f = 0, ps_f = 0, st_u = 0, ps_u = 0;
    ssbo(b, 1, &st_f, &ps_f);
    ssbo(b, 0, &st_u, &ps_u);
    (void)st_f;
    (void)st_u;
    b->ptr_f = ptr_ty(b, b->ty_f, 12);
    b->ptr_u = ptr_ty(b, b->ty_u, 12);
    b->ptr_fn_f = ptr_ty(b, b->ty_f, 7);
    b->ptr_fn_u = ptr_ty(b, b->ty_u, 7);
    for (int i = 0; i < f->np; i++) {
        var_t *v = f->params[i];
        if (!v || !v->t) { fail(b, "parâmetro sem tipo"); return; }
        if (v->attr == A_TPIG) {
            int vec = v->t->k == K_VEC && v->t->n == 3 && v->t->e->k == K_UINT;
            int scl = v->t->k == K_UINT;
            if (!vec && !scl) { fail(b, "thread_position_in_grid precisa ser uint ou uint3"); return; }
            u32 pt = ptr_ty(b, vec ? b->ty_u3 : b->ty_u, 1);
            b->gid = fresh(b);
            u32 a[3] = {pt, b->gid, 1};
            ty(b, OP_VARIABLE, a, 3);
            dec2(b, b->gid, 11, 28);
            add_slot(b, v, b->gid, vec ? SK_GID3 : SK_GID);
            continue;
        }
        if (v->t->k == K_PTR && v->t->e && (v->t->e->k == K_FLOAT || v->t->e->k == K_UINT)) {
            int flt = v->t->e->k == K_FLOAT;
            u32 var = fresh(b);
            u32 a[3] = {flt ? ps_f : ps_u, var, 12};
            ty(b, OP_VARIABLE, a, 3);
            unsigned bind = v->attr == A_BUFFER ? (unsigned)v->idx : (unsigned)i;
            dec2(b, var, 33, bind);
            dec2(b, var, 34, 0);
            add_slot(b, v, var, flt ? SK_SSBO_F : SK_SSBO_U);
            continue;
        }
        fail(b, "parâmetro fora de buffer float/uint ou thread id");
        return;
    }
    b->fn_id = fresh(b);
    u32 ent = fresh(b);
    u32 fa[4] = {b->ty_void, b->fn_id, 0, b->ty_fn};
    fn(b, OP_FUNCTION, fa, 4);
    u32 lb[1] = {ent};
    fn(b, OP_LABEL, lb, 1);
    stmt(b, f->body);
    if (!b->ended) fn(b, OP_RETURN, 0, 0);
    fn(b, OP_FN_END, 0, 0);

    u32 cap[1] = {1};
    op(&b->hd, &b->hn, &b->hc, OP_CAP, cap, 1);
    u32 mm[2] = {0, 1};
    op(&b->hd, &b->hn, &b->hc, OP_MEMORY_MODEL, mm, 2);
    const char *name = f->name ? f->name : "k";
    int sl = (int)strlen(name) + 1;
    int sw = (sl + 3) / 4;
    u32 words[16];
    memset(words, 0, sizeof words);
    if (sw > 14) { fail(b, "nome de kernel longo demais"); return; }
    memcpy(words, name, (size_t)sl);
    u32 ep[20];
    int ne = 0;
    ep[ne++] = 5;
    ep[ne++] = b->fn_id;
    for (int i = 0; i < sw; i++) ep[ne++] = words[i];
    if (b->gid) ep[ne++] = b->gid;
    op(&b->hd, &b->hn, &b->hc, OP_ENTRY, ep, ne);
    u32 em[5] = {b->fn_id, 17, 1, 1, 1};
    op(&b->hd, &b->hn, &b->hc, OP_EXEC_MODE, em, 5);
}

void msl_spirv_attach(msl_fn_t *f) {
    if (!f || !f->defined) return;
    if (f->stage != MSL_KERNEL) {
        snprintf(f->spirv_note, sizeof f->spirv_note, "só kernel vira SPIR-V nesta revisão");
        return;
    }
    bld_t b;
    memset(&b, 0, sizeof b);
    build(&b, f);
    if (b.bad || !b.hd || !b.ty || !b.fn) {
        if (!b.note[0]) snprintf(b.note, sizeof b.note, "SPIR-V não gerado");
        memcpy(f->spirv_note, b.note, sizeof f->spirv_note);
        free(b.hd);
        free(b.dec);
        free(b.ty);
        free(b.fn);
        return;
    }
    u32 n = 5 + b.hn + b.dn + b.tn + b.fnn;
    u32 *m = malloc((size_t)n * sizeof *m);
    if (!m) {
        snprintf(f->spirv_note, sizeof f->spirv_note, "sem memória para o SPIR-V");
        free(b.hd);
        free(b.dec);
        free(b.ty);
        free(b.fn);
        return;
    }
    m[0] = 0x07230203u;
    m[1] = 0x00010000u;
    m[2] = 0;
    m[3] = b.next;
    m[4] = 0;
    u32 at = 5;
    memcpy(m + at, b.hd, b.hn * sizeof *m);
    at += b.hn;
    if (b.dn) memcpy(m + at, b.dec, b.dn * sizeof *m);
    at += b.dn;
    memcpy(m + at, b.ty, b.tn * sizeof *m);
    at += b.tn;
    memcpy(m + at, b.fn, b.fnn * sizeof *m);
    f->spirv = m;
    f->spirv_n = n;
    snprintf(f->spirv_note, sizeof f->spirv_note, "LocalSize 1x1x1; não é virgl/venus");
    free(b.hd);
    free(b.dec);
    free(b.ty);
    free(b.fn);
}

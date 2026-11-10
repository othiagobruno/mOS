/*
 * Emissão dos nós do interpretador a partir do módulo AIR já decodificado.
 * Incluído por mtlair_lower.inc só no alvo (o airdump de host não gera nós).
 *
 * O que este arquivo faz de verdade: os 17 opcodes, SELECT, casts, GEP em bytes, PHI em cópia
 * paralela e as CALL cujo nome está na tabela (air.* e o llvm.* de matemática). O que não está
 * na tabela, atômico, pack/unpack e chamada indireta fazem a função ser omitida.
 *
 * Limitações conhecidas (não são suporte): dfdx/dfdy/fwidth valem 0, como no interpretador de MSL;
 * air.convert é um cast sem saturação; comparações de ponto flutuante sem ordem (uno/ueq/...) não
 * baixam; double não tem tipo; alloca de tamanho não constante não baixa; um loop que não termina
 * para no teto do interpretador (LOOP_MAX).
 */

ty_t *msl_tscal(int k);
ty_t *msl_tvec(int k, int n, int packed);
typedef struct { node_t **s; int n, cap; int term; } lowbb_t;

typedef struct {
    msl_lib_t *lib;
    air_module_t *m;
    airfn_t *afn;
    msl_fn_t *fn;
    const char *fname;
    ty_t **cache;
    ty_t *bytep;
    var_t **slot;          /* por valor local */
    var_t **amem;          /* por instrução do corpo: objeto do alloca */
    int *iloc;             /* índice local do resultado, ou -1 */
    var_t *vbb, *vstep;
    int frame, failed, depth;
    int *cals, nc, capc;
    char err[160];
    lowbb_t *bb;
    int nbb;
} low_t;

static void low_fail(low_t *L, const char *msg) {
    if (!L->failed) snprintf(L->err, sizeof L->err, "%s: %s", L->fname ? L->fname : "?", msg);
    L->failed = 1;
}

static int align_to(int o, int a) { if (a < 1) a = 1; return (o + a - 1) & ~(a - 1); }

static node_t *Nn(low_t *L, int op, ty_t *t) {
    node_t *n = msl_alloc(L->lib, sizeof *n);
    if (!n) { low_fail(L, "sem memória"); return 0; }
    n->op = op;
    n->t = t;
    return n;
}
static node_t *lit_i(low_t *L, i64 v, int k) {
    node_t *n = Nn(L, E_LIT, msl_tscal(k));
    if (!n) return 0;
    n->iv = v;
    n->fv = (double)v;
    return n;
}
static node_t *lit_f(low_t *L, double v, int k) {
    node_t *n = Nn(L, E_LIT, msl_tscal(k));
    if (!n) return 0;
    n->fv = v;
    n->iv = (i64)v;
    return n;
}
static node_t *varn(low_t *L, var_t *v) {
    if (!v) { low_fail(L, "variável ausente"); return 0; }
    node_t *n = Nn(L, E_VAR, v->t);
    if (n) n->var = v;
    return n;
}
static node_t *bin(low_t *L, int sub, node_t *a, node_t *b, ty_t *t) {
    node_t *n = Nn(L, E_BIN, t);
    if (!n) return 0;
    n->sub = sub;
    n->a = a;
    n->b = b;
    return n;
}
static node_t *castn(low_t *L, node_t *a, ty_t *t) {
    node_t *n = Nn(L, E_CAST, t);
    if (n) n->a = a;
    return n;
}
static node_t *bitc(low_t *L, node_t *a, ty_t *t) {
    node_t *n = Nn(L, E_BITCAST, t);
    if (n) n->a = a;
    return n;
}
static node_t *asn(low_t *L, node_t *lhs, node_t *rhs) {
    node_t *n = Nn(L, E_ASSIGN, lhs ? lhs->t : 0);
    if (!n) return 0;
    n->sub = '=';
    n->a = lhs;
    n->b = rhs;
    return n;
}
static node_t *sexpr(low_t *L, node_t *e) {
    node_t *n = Nn(L, S_EXPR, msl_tscal(K_VOID));
    if (n) n->a = e;
    return n;
}
static node_t *sblock(low_t *L, node_t **s, int n) {
    node_t *b = Nn(L, S_BLOCK, msl_tscal(K_VOID));
    if (!b) return 0;
    b->nv = n;
    if (n > 0) {
        b->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)n);
        if (!b->v) { low_fail(L, "sem memória"); return 0; }
        for (int i = 0; i < n; i++) b->v[i] = s[i];
    }
    return b;
}
static void push_bb(low_t *L, int bb, node_t *s) {
    if (!s || bb < 0 || bb >= L->nbb) return;
    lowbb_t *b = &L->bb[bb];
    if (b->n == b->cap) {
        int c = b->cap ? b->cap * 2 : 8;
        node_t **ns = realloc(b->s, sizeof(node_t *) * (size_t)c);
        if (!ns) { low_fail(L, "sem memória"); return; }
        b->s = ns;
        b->cap = c;
    }
    b->s[b->n++] = s;
}
static var_t *new_var(low_t *L, ty_t *t, int attr, int idx) {
    if (!t) { low_fail(L, "tipo ausente"); return 0; }
    var_t *v = msl_alloc(L->lib, sizeof *v);
    if (!v) { low_fail(L, "sem memória"); return 0; }
    v->t = t;
    v->attr = attr;
    v->idx = idx;
    int al = t->align ? t->align : 1;
    int sz = t->size ? t->size : 1;
    L->frame = align_to(L->frame, al);
    v->off = L->frame;
    L->frame += sz;
    return v;
}
static ty_t *make_ptr(low_t *L, ty_t *e) {
    ty_t *t = msl_alloc(L->lib, sizeof *t);
    if (!t) return 0;
    t->k = K_PTR;
    t->e = e ? e : msl_tscal(K_UCHAR);
    t->size = 8;
    t->align = 8;
    return t;
}
static ty_t *make_arr(low_t *L, ty_t *e, int n) {
    ty_t *t = msl_alloc(L->lib, sizeof *t);
    if (!t || !e) return 0;
    t->k = K_ARRAY;
    t->e = e;
    t->n = n;
    t->size = e->size * n;
    t->align = e->align ? e->align : 1;
    return t;
}
static ty_t *make_tex(low_t *L, int kind) {
    ty_t *t = msl_alloc(L->lib, sizeof *t);
    if (!t) return 0;
    t->k = kind;
    t->e = msl_tscal(K_FLOAT);
    t->size = 8;
    t->align = 8;
    t->tex = MSL_TEX_2D;
    return t;
}

static int ty_bits(air_module_t *m, int ti) {
    if (ti < 0 || ti >= m->ntypes) return 32;
    airty_t *t = &m->types[ti];
    if (t->kind == TYC_INTEGER) return t->bits ? t->bits : 32;
    if (t->kind == TYC_HALF || t->kind == TYC_BFLOAT) return 16;
    if (t->kind == TYC_FLOAT) return 32;
    if (t->kind == TYC_DOUBLE) return 64;
    if (t->kind == TYC_POINTER || t->kind == TYC_OPAQUE_POINTER) return 64;
    if (t->kind == TYC_VECTOR) return ty_bits(m, t->elem);
    return 32;
}
static int scal_of(air_module_t *m, int ti, int *bits) {
    if (ti < 0 || ti >= m->ntypes) return 0;
    airty_t *t = &m->types[ti];
    if (bits) *bits = ty_bits(m, ti);
    switch (t->kind) {
    case TYC_HALF: return K_HALF;
    case TYC_FLOAT: return K_FLOAT;
    case TYC_INTEGER:
        if (t->bits <= 1) return K_BOOL;
        if (t->bits <= 8) return K_CHAR;
        if (t->bits <= 16) return K_SHORT;
        if (t->bits <= 32) return K_INT;
        return K_LONG;
    default: return 0;
    }
}
static ty_t *ty_of(low_t *L, int ti);

static ty_t *ty_struct(low_t *L, int ti) {
    airty_t *at = &L->m->types[ti];
    ty_t *t = msl_alloc(L->lib, sizeof *t);
    if (!t) return 0;
    t->k = K_STRUCT;
    L->cache[ti] = t;
    int n = at->n;
    t->nf = n;
    t->f = n ? msl_alloc(L->lib, sizeof(fld_t) * (u64)n) : 0;
    int off = 0, al = 1;
    for (int i = 0; i < n; i++) {
        ty_t *ft = ty_of(L, at->params ? at->params[i] : -1);
        if (!ft) return 0;
        int fa = at->packed ? 1 : (ft->align ? ft->align : 1);
        if (!at->packed) off = align_to(off, fa);
        if (t->f) {
            t->f[i].t = ft;
            t->f[i].off = off;
            t->f[i].idx = at->params ? at->params[i] : -1;
        }
        off += ft->size;
        if (fa > al) al = fa;
    }
    if (!at->packed) {
        off = align_to(off, al);
        t->align = al;
    } else t->align = 1;
    t->size = off ? off : 1;
    return t;
}
static ty_t *ty_of(low_t *L, int ti) {
    if (ti == -2) return L->bytep;
    if (ti < 0 || ti >= L->m->ntypes) { low_fail(L, "tipo ausente"); return 0; }
    if (L->cache[ti]) return L->cache[ti];
    airty_t *at = &L->m->types[ti];
    ty_t *r = 0;
    int k;
    switch (at->kind) {
    case TYC_VOID: r = msl_tscal(K_VOID); break;
    case TYC_HALF: r = msl_tscal(K_HALF); break;
    case TYC_FLOAT: r = msl_tscal(K_FLOAT); break;
    case TYC_DOUBLE: low_fail(L, "double não é executável"); return 0;
    case TYC_INTEGER:
        k = scal_of(L->m, ti, 0);
        r = msl_tscal(k ? k : K_INT);
        break;
    case TYC_POINTER: case TYC_OPAQUE_POINTER: r = L->bytep; break;
    case TYC_VECTOR: {
        ty_t *e = ty_of(L, at->elem);
        if (!e) return 0;
        int sk = e->k == K_VEC ? e->e->k : e->k;
        r = msl_tvec(sk, at->n, 0);
        break;
    }
    case TYC_ARRAY: {
        ty_t *e = ty_of(L, at->elem);
        if (!e) return 0;
        r = make_arr(L, e, at->n);
        break;
    }
    case TYC_STRUCT_ANON: case TYC_STRUCT_NAMED: return ty_struct(L, ti);
    default: low_fail(L, "tipo LLVM sem tradução"); return 0;
    }
    L->cache[ti] = r;
    return r;
}

static airval_t *val_at(low_t *L, int abs) {
    if (abs < 0) return 0;
    if (abs < L->afn->nmod_at) {
        if (abs < L->m->nmvals) return &L->m->mvals[abs];
        return 0;
    }
    int loc = abs - L->afn->nmod_at;
    if (loc >= 0 && loc < L->afn->nvals) return &L->afn->vals[loc];
    return 0;
}
static int val_ti(low_t *L, int abs) {
    airval_t *v = val_at(L, abs);
    if (!v) return -1;
    if (v->kind == AV_INST && v->aux >= 0 && v->aux < L->m->ninsts) {
        airinst_t *in = &L->m->insts[v->aux];
        if (in->code == 9 || in->code == 28) {
            for (int i = 0; i < L->m->ntypes; i++)
                if (L->m->types[i].kind == TYC_INTEGER && L->m->types[i].bits == 1) return i;
        }
        if (in->ty >= 0) return in->ty;
        if (in->ty == -2 || in->code == 19 || in->code == 43) return -2;
        if ((in->code == 2 || in->code == 5 || in->code == 7 || in->code == 27) && in->narg >= 1) return val_ti(L, in->args[0]);
        if (in->code == 6 && in->narg >= 1) {
            int vt = val_ti(L, in->args[0]);
            if (vt >= 0 && vt < L->m->ntypes && L->m->types[vt].kind == TYC_VECTOR) return L->m->types[vt].elem;
        }
    }
    return v->ty;
}
static int const_i64(low_t *L, int abs, i64 *out) {
    airval_t *v = val_at(L, abs);
    if (!v || v->kind != AV_CONST) return 0;
    if (v->ckind == CK_INT) { *out = v->iv; return 1; }
    if (v->ckind == CK_NULL || v->ckind == CK_UNDEF || v->ckind == CK_POISON) { *out = 0; return 1; }
    return 0;
}

static node_t *emit_val(low_t *L, int abs);
static int uns_k(int k) {
    if (k == K_CHAR || k == K_UCHAR) return K_UCHAR;
    if (k == K_SHORT || k == K_USHORT) return K_USHORT;
    if (k == K_INT || k == K_UINT) return K_UINT;
    if (k == K_LONG || k == K_ULONG) return K_ULONG;
    if (k == K_BOOL) return K_BOOL;
    return k;
}
static ty_t *same_width(ty_t *t, int k) {
    if (!t) return msl_tscal(k);
    if (t->k == K_VEC) return msl_tvec(k, t->n, 0);
    return msl_tscal(k);
}
static node_t *zext_to(low_t *L, node_t *a, int sbits, ty_t *dst) {
    if (!a) return 0;
    int wk = K_ULONG;
    ty_t *wide = a->t && a->t->k == K_VEC ? msl_tvec(wk, a->t->n, 0) : msl_tscal(wk);
    node_t *w = castn(L, a, wide);
    if (sbits < 64) {
        u64 mask = sbits <= 0 ? 0 : (1ull << sbits) - 1;
        node_t *m = a->t && a->t->k == K_VEC ? lit_i(L, (i64)mask, K_ULONG) : lit_i(L, (i64)mask, K_ULONG);
        w = bin(L, '&', w, m, wide);
    }
    if (ty_scal(dst) != K_ULONG || (dst->k == K_VEC) != (wide->k == K_VEC)) w = castn(L, w, dst);
    return w;
}
static node_t *sext_to(low_t *L, node_t *a, int sbits, ty_t *dst) {
    if (!a) return 0;
    if (sbits >= 64) return castn(L, a, dst);
    int sh = 64 - sbits;
    ty_t *wide = a->t && a->t->k == K_VEC ? msl_tvec(K_LONG, a->t->n, 0) : msl_tscal(K_LONG);
    node_t *w = castn(L, a, wide);
    w = bin(L, 'L', w, lit_i(L, sh, K_INT), wide);
    w = bin(L, 'R', w, lit_i(L, sh, K_INT), wide);
    if (ty_scal(dst) != K_LONG || (dst->k == K_VEC) != (wide->k == K_VEC)) w = castn(L, w, dst);
    return w;
}
static int is_fp(ty_t *t) { int k = t ? ty_scal(t) : 0; return k == K_FLOAT || k == K_HALF; }

static node_t *emit_binop(low_t *L, int opc, node_t *a, node_t *b, ty_t *t, int sti) {
    if (!a || !b || !t) { low_fail(L, "operando vazio"); return 0; }
    if (is_fp(t)) {
        if (opc == 6) {
            node_t *n = Nn(L, E_BI, t);
            if (!n) return 0;
            n->bi = B_FMOD;
            n->nv = 2;
            n->v = msl_alloc(L->lib, sizeof(node_t *) * 2);
            if (n->v) { n->v[0] = a; n->v[1] = b; }
            return n;
        }
        int sub = opc == 0 ? '+' : opc == 1 ? '-' : opc == 2 ? '*' : opc == 4 ? '/' : 0;
        if (!sub) { low_fail(L, "binop float desconhecido"); return 0; }
        return bin(L, sub, a, b, t);
    }
    int sub = 0, uns = 0;
    switch (opc) {
    case 0: sub = '+'; break;
    case 1: sub = '-'; break;
    case 2: sub = '*'; break;
    case 3: sub = '/'; uns = 1; break;
    case 4: sub = '/'; break;
    case 5: sub = '%'; uns = 1; break;
    case 6: sub = '%'; break;
    case 7: sub = 'L'; break;
    case 8: sub = 'R'; uns = 1; break;
    case 9: sub = 'R'; break;
    case 10: sub = '&'; break;
    case 11: sub = '|'; break;
    case 12: sub = '^'; break;
    default: low_fail(L, "binop inteiro desconhecido"); return 0;
    }
    ty_t *rt = uns ? same_width(t, uns_k(ty_scal(t))) : t;
    (void)sti;
    return bin(L, sub, a, b, rt);
}
static node_t *emit_cast(low_t *L, int opc, node_t *a, ty_t *dst, int src_ti) {
    if (!a || !dst) return 0;
    int sb = ty_bits(L->m, src_ti);
    switch (opc) {
    case 0: return castn(L, a, dst);
    case 1: return zext_to(L, a, sb, dst);
    case 2: return sext_to(L, a, sb, dst);
    case 3: return castn(L, a, same_width(dst, uns_k(ty_scal(dst))));
    case 4: return castn(L, a, dst);
    case 5: return castn(L, zext_to(L, a, sb, same_width(a->t ? a->t : msl_tscal(K_INT), K_ULONG)), dst);
    case 6: case 7: case 8: return castn(L, a, dst);
    case 9: {
        node_t *w = bitc(L, a, msl_tscal(K_ULONG));
        if (ty_scal(dst) != K_ULONG) w = castn(L, w, dst);
        return w;
    }
    case 10: {
        node_t *w = a;
        if (!a->t || ty_scal(a->t) != K_ULONG) w = zext_to(L, a, sb, msl_tscal(K_ULONG));
        return bitc(L, w, L->bytep);
    }
    case 11: case 12: return bitc(L, a, dst);
    default: low_fail(L, "cast desconhecido"); return 0;
    }
}
static node_t *emit_cmp(low_t *L, int pred, node_t *a, node_t *b, int sti) {
    if (pred == 0) return lit_i(L, 0, K_BOOL);
    if (pred == 15) return lit_i(L, 1, K_BOOL);
    int sub = 0, uns = 0;
    int vecn = (a && a->t && a->t->k == K_VEC) ? a->t->n : 0;
    if (pred >= 32) {
        switch (pred) {
        case 32: sub = 'e'; break;
        case 33: sub = 'n'; break;
        case 34: sub = '>'; uns = 1; break;
        case 35: sub = 'g'; uns = 1; break;
        case 36: sub = '<'; uns = 1; break;
        case 37: sub = 'l'; uns = 1; break;
        case 38: sub = '>'; break;
        case 39: sub = 'g'; break;
        case 40: sub = '<'; break;
        case 41: sub = 'l'; break;
        default: low_fail(L, "icmp desconhecido"); return 0;
        }
        if (uns && vecn) { low_fail(L, "icmp vetorial sem sinal não baixa"); return 0; }
        if (uns) {
            int bits = ty_bits(L->m, sti);
            if (bits >= 64) {
                node_t *hi = lit_i(L, (i64)(1ull << 63), K_LONG);
                a = bin(L, '^', a, hi, msl_tscal(K_LONG));
                b = bin(L, '^', b, hi, msl_tscal(K_LONG));
            } else {
                a = zext_to(L, a, bits, msl_tscal(K_LONG));
                b = zext_to(L, b, bits, msl_tscal(K_LONG));
            }
        }
    } else {
        switch (pred) {
        case 1: sub = 'e'; break;
        case 2: sub = '>'; break;
        case 3: sub = 'g'; break;
        case 4: sub = '<'; break;
        case 5: sub = 'l'; break;
        case 6: sub = 'n'; break;
        default: low_fail(L, "fcmp sem ordem não baixa"); return 0;
        }
    }
    return bin(L, sub, a, b, vecn ? msl_tvec(K_BOOL, vecn, 0) : msl_tscal(K_BOOL));
}

static double fp_bits(air_module_t *m, int ti, u64 bits) {
    int kind = (ti >= 0 && ti < m->ntypes) ? m->types[ti].kind : TYC_FLOAT;
    if (kind == TYC_HALF) return (double)msl_h2f((u16)bits);
    if (kind == TYC_DOUBLE) {
        double d;
        memcpy(&d, &bits, 8);
        return d;
    }
    float f;
    u32 u = (u32)bits;
    memcpy(&f, &u, 4);
    return (double)f;
}
static node_t *emit_agg(low_t *L, airval_t *v, ty_t *t) {
    int n = v->nelt;
    node_t *c = Nn(L, E_CTOR, t);
    if (!c) return 0;
    c->nv = n;
    c->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)(n ? n : 1));
    ty_t *e = t->k == K_VEC || t->k == K_ARRAY ? t->e : msl_tscal(K_INT);
    int ek = e ? ty_scal(e) : K_INT;
    for (int i = 0; i < n; i++) {
        if (v->ckind == CK_DATA) {
            if (ek == K_FLOAT || ek == K_HALF) c->v[i] = lit_f(L, fp_bits(L->m, v->ty, (u64)(u32)v->elts[i]), ek);
            else c->v[i] = lit_i(L, (i64)(i32)v->elts[i], ek == K_LONG ? K_LONG : ek);
        } else c->v[i] = emit_val(L, v->elts[i]);
    }
    return c;
}
static node_t *emit_const(low_t *L, airval_t *v) {
    ty_t *t = ty_of(L, v->ty >= 0 ? v->ty : -2);
    if (!t) return lit_i(L, 0, K_INT);
    if (v->ckind == CK_NULL || v->ckind == CK_UNDEF || v->ckind == CK_POISON || v->ckind == CK_INT) {
        i64 iv = v->ckind == CK_INT ? v->iv : 0;
        if (t->k == K_VEC || t->k == K_ARRAY) {
            node_t *c = Nn(L, E_CTOR, t);
            if (!c) return 0;
            int n = t->n > 0 ? t->n : 1;
            c->nv = n;
            c->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)n);
            for (int i = 0; i < n; i++) c->v[i] = is_fp(t->e) ? lit_f(L, (double)iv, ty_scal(t->e)) : lit_i(L, iv, ty_scal(t->e));
            return c;
        }
        if (is_fp(t)) return lit_f(L, (double)iv, ty_scal(t));
        if (IS_SCALAR(t)) return lit_i(L, iv, t->k);
        node_t *c = Nn(L, E_CTOR, t);
        if (!c) return 0;
        int n = t->k == K_VEC ? t->n : 1;
        c->nv = n;
        c->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)n);
        for (int i = 0; i < n; i++) c->v[i] = is_fp(t) ? lit_f(L, 0, ty_scal(t)) : lit_i(L, iv, ty_scal(t));
        return c;
    }
    if (v->ckind == CK_FP) return lit_f(L, fp_bits(L->m, v->ty, v->bits), is_fp(t) ? ty_scal(t) : K_FLOAT);
    if (v->ckind == CK_AGG || v->ckind == CK_DATA) return emit_agg(L, v, t);
    if (v->ckind == CK_CE) {
        if (v->ce_opc == 10 && v->nelt >= 3) {
            node_t *a = emit_val(L, v->elts[1]);
            node_t *b = emit_val(L, v->elts[2]);
            return emit_binop(L, v->elts[0], a, b, t, val_ti(L, v->elts[1]));
        }
        if (v->ce_opc == 11 && v->nelt >= 3)
            return emit_cast(L, v->elts[0], emit_val(L, v->elts[2]), t, v->elts[1]);
        low_fail(L, "constexpr ainda não baixado");
        return lit_i(L, 0, K_INT);
    }
    low_fail(L, "constante desconhecida");
    return lit_i(L, 0, K_INT);
}
static node_t *emit_val(low_t *L, int abs) {
    if (L->failed) return 0;
    if (++L->depth > 48) { low_fail(L, "valor recursivo"); L->depth--; return 0; }
    airval_t *v = val_at(L, abs);
    node_t *r = 0;
    if (!v) low_fail(L, "id fora da lista");
    else if (v->kind == AV_CONST) r = emit_const(L, v);
    else if (v->kind == AV_ARG || v->kind == AV_INST) {
        int loc = v->kind == AV_ARG ? v->aux : -1;
        if (v->kind == AV_INST) {
            int j = v->aux - L->afn->first_inst;
            loc = (j >= 0 && j < L->afn->ninsts) ? L->iloc[j] : -1;
        }
        if (loc < 0 || loc >= L->afn->nvals || !L->slot[loc]) low_fail(L, "valor sem slot");
        else r = varn(L, L->slot[loc]);
    } else if (v->kind == AV_MOD_FN) low_fail(L, "função usada como valor");
    else low_fail(L, "global sem inicializador");
    L->depth--;
    return r;
}

static node_t *ptr_add(low_t *L, node_t *ptr, node_t *bytes) {
    node_t *pb = bitc(L, ptr, msl_tscal(K_ULONG));
    node_t *bu = castn(L, bytes, msl_tscal(K_ULONG));
    node_t *sum = bin(L, '+', pb, bu, msl_tscal(K_ULONG));
    return bitc(L, sum, L->bytep);
}
static node_t *scale_idx(low_t *L, node_t *ix, int iti, int esz) {
    node_t *w = sext_to(L, ix, ty_bits(L->m, iti), msl_tscal(K_LONG));
    if (esz == 1) return w;
    return bin(L, '*', w, lit_i(L, esz, K_LONG), msl_tscal(K_LONG));
}
static node_t *emit_gep(low_t *L, airinst_t *in) {
    if (in->narg < 1) { low_fail(L, "gep sem base"); return 0; }
    node_t *ptr = emit_val(L, in->args[0]);
    int cur = in->aux;
    if (cur < 0) { low_fail(L, "gep sem tipo de elemento"); return ptr; }
    for (int i = 1; i < in->narg; i++) {
        node_t *ix = emit_val(L, in->args[i]);
        int iti = val_ti(L, in->args[i]);
        if (i == 1) {
            ty_t *et = ty_of(L, cur);
            int sz = et ? et->size : 0;
            if (!sz) { low_fail(L, "gep de tipo vazio"); return ptr; }
            ptr = ptr_add(L, ptr, scale_idx(L, ix, iti, sz));
            continue;
        }
        if (cur < 0 || cur >= L->m->ntypes) { low_fail(L, "gep fora do tipo"); return ptr; }
        airty_t *at = &L->m->types[cur];
        ty_t *t = ty_of(L, cur);
        if (at->kind == TYC_STRUCT_ANON || at->kind == TYC_STRUCT_NAMED) {
            i64 fi;
            if (!const_i64(L, in->args[i], &fi) || !t || fi < 0 || fi >= t->nf) { low_fail(L, "campo de gep inválido"); return ptr; }
            ptr = ptr_add(L, ptr, lit_i(L, t->f[fi].off, K_LONG));
            cur = at->params ? at->params[fi] : -1;
        } else if (at->kind == TYC_ARRAY || at->kind == TYC_VECTOR) {
            int es = 0;
            if (t && t->k == K_VEC) es = scal_size(t->e->k);
            else if (t && t->e) es = t->e->size;
            ptr = ptr_add(L, ptr, scale_idx(L, ix, iti, es ? es : 1));
            cur = at->elem;
        } else { low_fail(L, "gep não entra nesse tipo"); return ptr; }
    }
    return ptr;
}

static int shuffle_idx(low_t *L, airval_t *mask, int i, i64 *out) {
    if (!mask || i >= mask->nelt) return 0;
    if (mask->ckind == CK_DATA) { *out = (i64)(i32)mask->elts[i]; return 1; }
    if (mask->ckind == CK_AGG) return const_i64(L, mask->elts[i], out);
    return 0;
}
static node_t *emit_index(low_t *L, node_t *vec, node_t *ix, ty_t *et) {
    node_t *n = Nn(L, E_INDEX, et);
    if (!n) return 0;
    n->a = vec;
    n->b = ix;
    return n;
}
static node_t *emit_member(low_t *L, node_t *agg, int off, ty_t *ft) {
    node_t *n = Nn(L, E_MEMBER, ft);
    if (!n) return 0;
    n->a = agg;
    n->off = off;
    return n;
}

static int ai_builtin(int ai) {
    switch (ai) {
    case AI_FABS: return B_ABS;
    case AI_FMIN: return B_MIN;
    case AI_FMAX: return B_MAX;
    case AI_CLAMP: return B_CLAMP;
    case AI_FLOOR: return B_FLOOR;
    case AI_CEIL: return B_CEIL;
    case AI_FRACT: return B_FRACT;
    case AI_RINT: return B_RINT;
    case AI_TRUNC: return B_TRUNC;
    case AI_SQRT: return B_SQRT;
    case AI_RSQRT: return B_RSQRT;
    case AI_SIN: return B_SIN;
    case AI_COS: return B_COS;
    case AI_EXP2: return B_EXP2;
    case AI_LOG2: return B_LOG2;
    case AI_POW: return B_POW;
    case AI_FMA: return B_FMA;
    case AI_DOT: return B_DOT;
    case AI_NORMALIZE: return B_NORMALIZE;
    case AI_LENGTH: return B_LENGTH;
    case AI_CROSS: return B_CROSS;
    case AI_MIX: return B_MIX;
    case AI_STEP: return B_STEP;
    case AI_SMOOTHSTEP: return B_SMOOTHSTEP;
    case AI_SATURATE: return B_SATURATE;
    case AI_SIGN: return B_SIGN;
    case AI_COPYSIGN: return B_COPYSIGN;
    case AI_DFDX: return B_DFDX;
    case AI_DFDY: return B_DFDY;
    case AI_FWIDTH: return B_FWIDTH;
    case AI_DISCARD: return B_DISCARD;
    case AI_BARRIER: return B_BARRIER;
    default: return 0;
    }
}
static node_t *emit_bi(low_t *L, int bi, ty_t *rt, node_t **args, int na) {
    node_t *n = Nn(L, E_BI, rt ? rt : msl_tscal(K_VOID));
    if (!n) return 0;
    n->bi = bi;
    n->nv = na;
    if (na) {
        n->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)na);
        for (int i = 0; i < na; i++) n->v[i] = args[i];
    }
    if (bi == B_BARRIER) L->fn->barrier = 1;
    if (bi == B_DISCARD) L->fn->discard = 1;
    return n;
}
static void note_cal(low_t *L, int fi) {
    if (L->nc == L->capc) {
        L->capc = L->capc ? L->capc * 2 : 4;
        int *n = realloc(L->cals, sizeof(int) * (size_t)L->capc);
        if (!n) { low_fail(L, "sem memória"); return; }
        L->cals = n;
    }
    L->cals[L->nc++] = fi;
}
static node_t *emit_call(low_t *L, airinst_t *in, msl_fn_t **fns, int *stored) {
    if (in->narg < 1) { low_fail(L, "call sem alvo"); return 0; }
    const char *name = air_call_target(L->m, in, 0, 0);
    int ai = air_intrinsic(name);
    int na = in->narg - 1;
    if (na > 16) { low_fail(L, "call com argumentos demais"); return 0; }
    node_t *args[16];
    for (int i = 0; i < na; i++) args[i] = emit_val(L, in->args[i + 1]);
    ty_t *rt = 0;
    int produces = 0;
    if (in->ty >= 0 && in->ty < L->m->ntypes && L->m->types[in->ty].kind != TYC_VOID) {
        rt = ty_of(L, in->ty);
        produces = rt && rt->k != K_VOID;
    }
    if (!name) { low_fail(L, "call sem nome"); return 0; }
    if (ai == AI_UNKNOWN) {
        airval_t *cv = val_at(L, in->args[0]);
        if (!cv || cv->kind != AV_MOD_FN) { low_fail(L, "chamada indireta"); return 0; }
        int fi = cv->aux;
        if (fi < 0 || fi >= L->m->nfns || !fns[fi]) { low_fail(L, "call alvo fora do módulo"); return 0; }
        note_cal(L, fi);
        node_t *c = Nn(L, E_CALL, rt ? rt : msl_tscal(K_VOID));
        if (!c) return 0;
        c->fn = fns[fi];
        c->nv = na;
        if (na) {
            c->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)na);
            for (int i = 0; i < na; i++) c->v[i] = args[i];
        }
        if (!produces) { push_bb(L, in->bb, sexpr(L, c)); *stored = 1; return 0; }
        return c;
    }
    if (ai == AI_IGNORE) {
        if (produces) low_fail(L, "intrínseco ignorado devolve valor");
        *stored = 1;
        return 0;
    }
    if (ai == AI_CONVERT && name && strstr(name, "expect")) {
        *stored = 0;
        return na ? args[0] : lit_i(L, 0, K_INT);
    }
    if (ai == AI_CONVERT) {
        if (!na || !rt) { low_fail(L, "convert sem tipo"); return 0; }
        return castn(L, args[0], rt);
    }
    if (ai == AI_ATOMIC || ai == AI_PACK || ai == AI_UNPACK) {
        low_fail(L, ai == AI_ATOMIC ? "atômico air.atomic não baixa" : "pack/unpack não baixa");
        return 0;
    }
    if (ai == AI_SAMPLE || ai == AI_SAMPLE_CMP || ai == AI_READ || ai == AI_WRITE || ai == AI_GATHER ||
        ai == AI_GET_W || ai == AI_GET_H || ai == AI_GET_D) {
        if (na < 1) { low_fail(L, "textura sem argumento"); return 0; }
        int sub = ai == AI_SAMPLE ? T_SAMPLE : ai == AI_SAMPLE_CMP ? T_SAMPLE_CMP : ai == AI_READ ? T_READ :
                  ai == AI_WRITE ? T_WRITE : ai == AI_GATHER ? T_GATHER : ai == AI_GET_W ? T_GET_W : ai == AI_GET_H ? T_GET_H : T_GET_D;
        node_t *n = Nn(L, E_TEX, rt ? rt : msl_tscal(K_VOID));
        if (!n) return 0;
        n->a = args[0];
        n->sub = sub;
        int from = 1;
        n->nv = na - from;
        if (n->nv > 0) {
            n->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)n->nv);
            for (int i = 0; i < n->nv; i++) n->v[i] = args[from + i];
        }
        if (ai == AI_WRITE) { push_bb(L, in->bb, sexpr(L, n)); *stored = 1; return 0; }
        return n;
    }
    int b = ai_builtin(ai);
    if (!b) { low_fail(L, "intrínseco sem builtin"); return 0; }
    node_t *n = emit_bi(L, b, rt, args, na);
    if (!produces) { push_bb(L, in->bb, sexpr(L, n)); *stored = 1; return 0; }
    return n;
}

static node_t *edge_block(low_t *L, int from, int to) {
    node_t *tmpv[64];
    var_t *tmps[64];
    int np = 0;
    if (to < 0 || to >= L->nbb) {
        node_t *br = Nn(L, S_BREAK, msl_tscal(K_VOID));
        return br;
    }
    int base = L->afn->first_inst;
    for (int j = 0; j < L->afn->ninsts && np < 64; j++) {
        airinst_t *in = &L->m->insts[base + j];
        if (in->bb != to || in->code != 16 || !in->dec_ok) continue;
        int slot = -1;
        for (int k = 0; k < in->nlit; k++) if (in->lits[k] == from) { slot = k; break; }
        int loc = L->iloc[j];
        if (slot < 0 || loc < 0 || !L->slot[loc]) continue;
        var_t *tv = new_var(L, L->slot[loc]->t, 0, 0);
        tmps[np] = tv;
        tmpv[np] = asn(L, varn(L, tv), emit_val(L, in->args[slot]));
        np++;
        (void)tmps;
    }
    node_t **ss = np || 1 ? malloc(sizeof(node_t *) * (size_t)(np * 2 + 2)) : 0;
    int ns = 0;
    if (!ss) { low_fail(L, "sem memória"); return 0; }
    int p = 0;
    for (int j = 0; j < L->afn->ninsts && p < np; j++) {
        airinst_t *in = &L->m->insts[base + j];
        if (in->bb != to || in->code != 16 || !in->dec_ok) continue;
        int slot = -1;
        for (int k = 0; k < in->nlit; k++) if (in->lits[k] == from) { slot = k; break; }
        int loc = L->iloc[j];
        if (slot < 0 || loc < 0 || !L->slot[loc]) continue;
        ss[ns++] = sexpr(L, tmpv[p]);
        p++;
    }
    p = 0;
    for (int j = 0; j < L->afn->ninsts && p < np; j++) {
        airinst_t *in = &L->m->insts[base + j];
        if (in->bb != to || in->code != 16 || !in->dec_ok) continue;
        int slot = -1;
        for (int k = 0; k < in->nlit; k++) if (in->lits[k] == from) { slot = k; break; }
        int loc = L->iloc[j];
        if (slot < 0 || loc < 0 || !L->slot[loc]) continue;
        ss[ns++] = sexpr(L, asn(L, varn(L, L->slot[loc]), varn(L, tmps[p])));
        p++;
    }
    ss[ns++] = sexpr(L, asn(L, varn(L, L->vbb), lit_i(L, to, K_INT)));
    node_t *b = sblock(L, ss, ns);
    free(ss);
    return b;
}

static ty_t *resty(low_t *L, airinst_t *in) {
    if (in->code == 9 || in->code == 28) {
        if (in->ty >= 0 && in->ty < L->m->ntypes && L->m->types[in->ty].kind == TYC_VECTOR)
            return msl_tvec(K_BOOL, L->m->types[in->ty].n, 0);
        return msl_tscal(K_BOOL);
    }
    if (in->code == 19 || in->code == 43 || in->ty == -2) return L->bytep;
    if (in->code == 8 && in->narg >= 3) {
        int vt = val_ti(L, in->args[0]);
        int mt = val_ti(L, in->args[2]);
        ty_t *v = vt >= 0 ? ty_of(L, vt) : 0;
        ty_t *m = mt >= 0 ? ty_of(L, mt) : 0;
        if (v && m && m->k == K_VEC) return msl_tvec(ty_scal(v), m->n, 0);
    }
    if (in->ty >= 0) return ty_of(L, in->ty);
    if (in->narg >= 1) {
        int t = val_ti(L, in->args[0]);
        if (in->code == 6 && t >= 0 && t < L->m->ntypes && L->m->types[t].kind == TYC_VECTOR) t = L->m->types[t].elem;
        if (t == -2) return L->bytep;
        if (t >= 0) return ty_of(L, t);
    }
    return 0;
}

static void emit_inst(low_t *L, airinst_t *in, int j, msl_fn_t **fns) {
    if (L->failed) return;
    if (!in->dec_ok && in->code != 35 && in->code != 33) { low_fail(L, "instrução não decodificou"); return; }
    if (!air_opcode_lowerable(in->code) && in->code != 33) {
        char b[64];
        snprintf(b, sizeof b, "opcode %d sem emissão", in->code);
        low_fail(L, b);
        return;
    }
    int stored = 0;
    node_t *e = 0;
    int loc = L->iloc[j];
    switch (in->code) {
    case 33: case 35: return;
    case 10:
        if (in->narg >= 1) e = emit_val(L, in->args[0]);
        {
            node_t *r = Nn(L, S_RET, msl_tscal(K_VOID));
            if (r) r->a = e;
            push_bb(L, in->bb, r);
        }
        L->bb[in->bb].term = 1;
        return;
    case 11:
        if (in->nlit == 1) push_bb(L, in->bb, edge_block(L, in->bb, in->lits[0]));
        else if (in->nlit >= 2 && in->narg >= 1) {
            node_t *ifs = Nn(L, S_IF, msl_tscal(K_VOID));
            if (ifs) {
                ifs->a = emit_val(L, in->args[0]);
                ifs->b = edge_block(L, in->bb, in->lits[0]);
                ifs->c = edge_block(L, in->bb, in->lits[1]);
                push_bb(L, in->bb, ifs);
            }
        } else low_fail(L, "br malformado");
        L->bb[in->bb].term = 1;
        return;
    case 16: return;
    case 19: {
        if (!L->amem[j]) { low_fail(L, "alloca de tamanho não constante"); return; }
        e = Nn(L, E_ADDR, L->bytep);
        if (e) e->a = varn(L, L->amem[j]);
        break;
    }
    case 20: {
        ty_t *rt = resty(L, in);
        node_t *p = in->narg ? emit_val(L, in->args[0]) : 0;
        e = Nn(L, E_DEREF, rt);
        if (e) e->a = p;
        break;
    }
    case 44: case 24: {
        if (in->narg < 2) { low_fail(L, "store sem operandos"); return; }
        int vti = in->aux >= 0 ? in->aux : val_ti(L, in->args[1]);
        ty_t *vt = vti == -2 ? L->bytep : ty_of(L, vti);
        node_t *lhs = Nn(L, E_DEREF, vt);
        if (lhs) lhs->a = emit_val(L, in->args[0]);
        push_bb(L, in->bb, sexpr(L, asn(L, lhs, emit_val(L, in->args[1]))));
        return;
    }
    case 2:
        if (in->narg < 2) { low_fail(L, "binop curto"); return; }
        e = emit_binop(L, in->opc, emit_val(L, in->args[0]), emit_val(L, in->args[1]), resty(L, in), val_ti(L, in->args[0]));
        break;
    case 3:
        if (in->narg < 1) { low_fail(L, "cast curto"); return; }
        e = emit_cast(L, in->opc, emit_val(L, in->args[0]), resty(L, in), val_ti(L, in->args[0]));
        break;
    case 5: case 29: {
        if (in->narg < 3) { low_fail(L, "select curto"); return; }
        e = Nn(L, E_COND, resty(L, in));
        if (e) { e->a = emit_val(L, in->args[0]); e->b = emit_val(L, in->args[1]); e->c = emit_val(L, in->args[2]); }
        break;
    }
    case 9: case 28:
        if (in->narg < 2) { low_fail(L, "cmp curto"); return; }
        e = emit_cmp(L, in->opc, emit_val(L, in->args[0]), emit_val(L, in->args[1]), val_ti(L, in->args[0]));
        break;
    case 6: {
        if (in->narg < 2) { low_fail(L, "extractelement curto"); return; }
        ty_t *et = resty(L, in);
        e = emit_index(L, emit_val(L, in->args[0]), emit_val(L, in->args[1]), et);
        break;
    }
    case 7: {
        if (in->narg < 3 || loc < 0 || !L->slot[loc]) { low_fail(L, "insertelement curto"); return; }
        push_bb(L, in->bb, sexpr(L, asn(L, varn(L, L->slot[loc]), emit_val(L, in->args[0]))));
        ty_t *et = L->slot[loc]->t && L->slot[loc]->t->k == K_VEC ? L->slot[loc]->t->e : msl_tscal(K_FLOAT);
        node_t *ix = emit_index(L, varn(L, L->slot[loc]), emit_val(L, in->args[2]), et);
        push_bb(L, in->bb, sexpr(L, asn(L, ix, emit_val(L, in->args[1]))));
        return;
    }
    case 8: {
        if (in->narg < 3) { low_fail(L, "shufflevector curto"); return; }
        airval_t *mask = val_at(L, in->args[2]);
        ty_t *rt = resty(L, in);
        if (!mask || (mask->ckind != CK_AGG && mask->ckind != CK_DATA) || !rt) { low_fail(L, "máscara de shuffle não é constante"); return; }
        int n = rt->k == K_VEC ? rt->n : 1;
        int n1 = 0;
        int vt = val_ti(L, in->args[0]);
        if (vt >= 0 && vt < L->m->ntypes && L->m->types[vt].kind == TYC_VECTOR) n1 = L->m->types[vt].n;
        node_t *c = Nn(L, E_CTOR, rt->k == K_VEC ? rt : msl_tvec(ty_scal(rt), n, 0));
        if (!c) return;
        c->nv = n;
        c->v = msl_alloc(L->lib, sizeof(node_t *) * (u64)(n ? n : 1));
        ty_t *et = c->t->k == K_VEC ? c->t->e : c->t;
        for (int i = 0; i < n; i++) {
            i64 ix;
            if (!shuffle_idx(L, mask, i, &ix) || ix < 0) c->v[i] = is_fp(et) ? lit_f(L, 0, et->k) : lit_i(L, 0, et->k);
            else if (ix < n1) c->v[i] = emit_index(L, emit_val(L, in->args[0]), lit_i(L, ix, K_INT), et);
            else c->v[i] = emit_index(L, emit_val(L, in->args[1]), lit_i(L, ix - n1, K_INT), et);
        }
        e = c;
        break;
    }
    case 26: case 27: {
        if (in->narg < 1) { low_fail(L, "extractvalue curto"); return; }
        int ti = val_ti(L, in->args[0]);
        node_t *cur = in->code == 27 && loc >= 0 && L->slot[loc]
            ? varn(L, L->slot[loc]) : emit_val(L, in->args[0]);
        if (in->code == 27 && loc >= 0 && L->slot[loc])
            push_bb(L, in->bb, sexpr(L, asn(L, varn(L, L->slot[loc]), emit_val(L, in->args[0]))));
        ty_t *ct = ti >= 0 ? ty_of(L, ti) : 0;
        for (int k = 0; k < in->nlit && ct; k++) {
            int ix = in->lits[k];
            if (ct->k == K_STRUCT) {
                if (ix < 0 || ix >= ct->nf) { low_fail(L, "extractvalue fora do struct"); return; }
                cur = emit_member(L, cur, ct->f[ix].off, ct->f[ix].t);
                ct = ct->f[ix].t;
                if (ti >= 0 && ti < L->m->ntypes && L->m->types[ti].params) ti = L->m->types[ti].params[ix];
            } else if (ct->k == K_ARRAY || ct->k == K_VEC) {
                ty_t *et = ct->k == K_VEC ? ct->e : ct->e;
                cur = emit_index(L, cur, lit_i(L, ix, K_INT), et);
                ct = et;
            } else { low_fail(L, "extractvalue em escalar"); return; }
        }
        if (in->code == 27 && loc >= 0 && L->slot[loc]) {
            push_bb(L, in->bb, sexpr(L, asn(L, cur, in->narg >= 2 ? emit_val(L, in->args[1]) : lit_i(L, 0, K_INT))));
            return;
        }
        e = cur;
        break;
    }
    case 34:
        e = emit_call(L, in, fns, &stored);
        break;
    case 43:
        e = emit_gep(L, in);
        break;
    default:
        low_fail(L, "opcode sem corpo de emissão");
        return;
    }
    if (stored || L->failed) return;
    if (loc >= 0 && L->slot[loc] && e) push_bb(L, in->bb, sexpr(L, asn(L, varn(L, L->slot[loc]), e)));
    else if (e) push_bb(L, in->bb, sexpr(L, e));
}

static const char *md_str(air_module_t *m, int id) {
    if (id <= 0 || id > m->nmds) return 0;
    airmd_t *d = &m->mds[id - 1];
    if (d->kind != 0) return 0;
    return d->s;
}
static int parse_idx(const char *s) {
    if (!s || !*s) return 0;
    int n = 0, sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return n * sign;
}
static int attr_of(const char *s) {
    if (!s) return -1;
    if (!strcmp(s, "buffer")) return A_BUFFER;
    if (!strcmp(s, "texture")) return A_TEXTURE;
    if (!strcmp(s, "sampler")) return A_SAMPLER;
    if (!strcmp(s, "thread_position_in_grid")) return A_TPIG;
    if (!strcmp(s, "thread_position_in_threadgroup")) return A_TPITG;
    if (!strcmp(s, "threadgroup_position_in_grid")) return A_TGPIG;
    if (!strcmp(s, "threads_per_threadgroup")) return A_TPTG;
    if (!strcmp(s, "thread_index_in_threadgroup")) return A_TIITG;
    if (!strcmp(s, "stage_in")) return A_STAGE_IN;
    if (!strcmp(s, "threadgroup")) return A_THREADGROUP;
    return -1;
}
static void bind_meta(air_module_t *m, airfn_t *fn) {
    for (int i = 0; i < m->nnamed; i++) {
        const char *nn = m->named[i].name;
        if (strcmp(nn, "air.kernel") && strcmp(nn, "air.vertex") && strcmp(nn, "air.fragment")) continue;
        if (m->named[i].nops < 1) continue;
        int rec = m->named[i].ops[0];
        if (rec < 0 || rec >= m->nmds || m->mds[rec].kind != 1) continue;
        airmd_t *fr = &m->mds[rec];
        if (fr->nops < 1) continue;
        const char *nm = md_str(m, fr->ops[0]);
        if (!nm || !fn->name || strcmp(nm, fn->name)) continue;
        for (int a = 0; a < fn->nargs && a + 1 < fr->nops && a < 16; a++) {
            int id = fr->ops[a + 1];
            if (id <= 0 || id > m->nmds || m->mds[id - 1].kind != 1) continue;
            airmd_t *ar = &m->mds[id - 1];
            if (ar->nops < 1) continue;
            int at = attr_of(md_str(m, ar->ops[0]));
            if (at < 0) continue;
            fn->arg_attr[a] = at;
            fn->arg_set[a] = 1;
            if (ar->nops >= 2) fn->arg_idx[a] = parse_idx(md_str(m, ar->ops[1]));
        }
    }
}
static void bind_fallback(air_module_t *m, airfn_t *fn) {
    int nb = 0, ng = 0;
    if (fn->type < 0 || fn->type >= m->ntypes) return;
    airty_t *ft = &m->types[fn->type];
    for (int a = 0; a < fn->nargs && a < 16; a++) {
        if (fn->arg_set[a]) {
            if (fn->arg_attr[a] == A_BUFFER) { if (fn->arg_idx[a] >= nb) nb = fn->arg_idx[a] + 1; }
            continue;
        }
        int ti = ft->params ? ft->params[a] : -1;
        if (ti < 0 || ti >= m->ntypes) continue;
        airty_t *t = &m->types[ti];
        if (t->kind == TYC_POINTER || t->kind == TYC_OPAQUE_POINTER) {
            if (t->addrspace == 3) { fn->arg_attr[a] = A_THREADGROUP; fn->arg_idx[a] = ng++; }
            else { fn->arg_attr[a] = A_BUFFER; fn->arg_idx[a] = nb++; }
            fn->arg_set[a] = 1;
        }
    }
}

static int map_stage(int s) {
    if (s == 0) return MSL_VERTEX;
    if (s == 1) return MSL_FRAGMENT;
    if (s == 2) return MSL_KERNEL;
    return MSL_HELPER;
}
static char *aname(msl_lib_t *L, const char *s) {
    if (!s) s = "air";
    u64 n = strlen(s);
    char *d = msl_alloc(L, n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

typedef struct {
    msl_fn_t *f;
    int ok;
    int *cals;
    int nc;
    char err[160];
} built_t;

static void lower_body(msl_lib_t *lib, air_module_t *m, int fi, msl_fn_t **fns, built_t *me) {
    airfn_t *af = &m->fns[fi];
    low_t L;
    memset(&L, 0, sizeof L);
    L.lib = lib;
    L.m = m;
    L.afn = af;
    L.fn = fns[fi];
    L.fname = af->name ? af->name : "air";
    L.cache = calloc((size_t)(m->ntypes > 0 ? m->ntypes : 1), sizeof(ty_t *));
    L.bytep = make_ptr(&L, msl_tscal(K_UCHAR));
    L.slot = calloc((size_t)(af->nvals > 0 ? af->nvals : 1), sizeof(var_t *));
    L.amem = calloc((size_t)(af->ninsts > 0 ? af->ninsts : 1), sizeof(var_t *));
    L.iloc = malloc(sizeof(int) * (size_t)(af->ninsts > 0 ? af->ninsts : 1));
    if (!L.cache || !L.bytep || !L.slot || !L.iloc) { snprintf(me->err, sizeof me->err, "%s: sem memória", L.fname); goto done; }
    for (int j = 0; j < af->ninsts; j++) L.iloc[j] = -1;
    for (int i = 0; i < af->nvals; i++) {
        if (af->vals[i].kind == AV_INST) {
            int j = af->vals[i].aux - af->first_inst;
            if (j >= 0 && j < af->ninsts) L.iloc[j] = i;
        }
    }
    if (af->type >= 0 && af->type < m->ntypes && m->types[af->type].kind == TYC_FUNCTION) {
        int rti = m->types[af->type].ret;
        if (rti >= 0 && rti < m->ntypes && m->types[rti].kind != TYC_VOID) L.fn->ret = ty_of(&L, rti);
    }
    if (!L.fn->ret) L.fn->ret = msl_tscal(K_VOID);
    L.nbb = af->nbb > 0 ? af->nbb : 1;
    for (int j = 0; j < af->ninsts; j++) if (m->insts[af->first_inst + j].bb + 1 > L.nbb) L.nbb = m->insts[af->first_inst + j].bb + 1;
    if (L.nbb > 4096) { low_fail(&L, "blocos demais"); goto done; }
    L.bb = calloc((size_t)L.nbb, sizeof *L.bb);
    airty_t *ft = (af->type >= 0 && af->type < m->ntypes) ? &m->types[af->type] : 0;
    L.fn->np = af->nargs;
    L.fn->params = af->nargs ? msl_alloc(lib, sizeof(var_t *) * (u64)af->nargs) : 0;
    for (int a = 0; a < af->nargs; a++) {
        int ti = ft && ft->params ? ft->params[a] : -1;
        ty_t *pt = 0;
        if (a < 16 && af->arg_attr[a] == A_TEXTURE) pt = make_tex(&L, K_TEX);
        else if (a < 16 && af->arg_attr[a] == A_SAMPLER) pt = make_tex(&L, K_SAMP);
        else if (ti == -2) pt = L.bytep;
        else pt = ty_of(&L, ti);
        var_t *v = new_var(&L, pt, a < 16 ? af->arg_attr[a] : 0, a < 16 ? af->arg_idx[a] : 0);
        L.slot[a] = v;
        if (L.fn->params) L.fn->params[a] = v;
    }
    for (int j = 0; j < af->ninsts; j++) {
        int loc = L.iloc[j];
        if (loc < 0) continue;
        airinst_t *in = &m->insts[af->first_inst + j];
        if (in->code == 19) {
            i64 n = 1;
            ty_t *et = ty_of(&L, in->aux);
            if (et && const_i64(&L, in->args ? in->args[0] : -1, &n) && n > 1 && n < (1 << 20)) et = make_arr(&L, et, (int)n);
            if (et) L.amem[j] = new_var(&L, et, 0, 0);
            L.slot[loc] = new_var(&L, L.bytep, 0, 0);
            continue;
        }
        L.slot[loc] = new_var(&L, resty(&L, in), 0, 0);
    }
    L.vbb = new_var(&L, msl_tscal(K_INT), 0, 0);
    L.vstep = new_var(&L, msl_tscal(K_INT), 0, 0);
    for (int j = 0; j < af->ninsts && !L.failed; j++) {
        airinst_t *in = &m->insts[af->first_inst + j];
        if (in->bb < 0 || in->bb >= L.nbb) { low_fail(&L, "bloco fora"); break; }
        emit_inst(&L, in, j, fns);
    }
    if (!L.failed) {
        for (int b = 0; b < L.nbb; b++) if (!L.bb[b].term) {
            node_t *br = Nn(&L, S_BREAK, msl_tscal(K_VOID));
            push_bb(&L, b, br);
        }
        node_t **body = malloc(sizeof(node_t *) * (size_t)(L.nbb + 4));
        int nb = 0;
        if (!body) low_fail(&L, "sem memória");
        else {
            body[nb++] = sexpr(&L, asn(&L, varn(&L, L.vstep), bin(&L, '+', varn(&L, L.vstep), lit_i(&L, 1, K_INT), msl_tscal(K_INT))));
            node_t *chain = Nn(&L, S_BREAK, msl_tscal(K_VOID));
            for (int b = L.nbb - 1; b >= 0; b--) {
                node_t *ifs = Nn(&L, S_IF, msl_tscal(K_VOID));
                if (!ifs) break;
                ifs->a = bin(&L, 'e', varn(&L, L.vbb), lit_i(&L, b, K_INT), msl_tscal(K_BOOL));
                ifs->b = sblock(&L, L.bb[b].s, L.bb[b].n);
                ifs->c = chain;
                chain = ifs;
            }
            body[nb++] = chain;
            node_t *wh = Nn(&L, S_WHILE, msl_tscal(K_VOID));
            if (wh) {
                wh->a = bin(&L, '<', varn(&L, L.vstep), lit_i(&L, 1 << 20, K_INT), msl_tscal(K_BOOL));
                wh->b = sblock(&L, body, nb);
            }
            L.fn->body = wh;
            free(body);
        }
    }
    if (!L.failed && L.fn->ret && L.fn->ret->k != K_VOID) {
        int al = L.fn->ret->align ? L.fn->ret->align : 1;
        L.frame = align_to(L.frame, al);
        L.fn->retoff = L.frame;
        L.frame += L.fn->ret->size ? L.fn->ret->size : 1;
    }
    L.fn->frame = L.frame;
done:
    if (L.failed) snprintf(me->err, sizeof me->err, "%s", L.err[0] ? L.err : "falhou");
    else me->ok = 1;
    me->cals = L.cals;
    me->nc = L.nc;
    for (int b = 0; L.bb && b < L.nbb; b++) free(L.bb[b].s);
    free(L.bb);
    free(L.cache);
    free(L.slot);
    free(L.amem);
    free(L.iloc);
}

static air_module_t *open_mod(const void *data, u64 len, char *err, int errn) {
    const u8 *p = data;
    if (len >= 4 && p[0] == 'M' && p[1] == 'T' && p[2] == 'L' && p[3] == 'B') return mtlair_parse(data, len, err, errn);
    if (len >= 4 && p[0] == 'B' && p[1] == 'C' && p[2] == 0xC0 && p[3] == 0xDE) return airmod_parse(p + 4, len - 4, err, errn);
    if (err && errn) snprintf(err, errn, "nem MTLB nem bitcode LLVM");
    return 0;
}

static void lib_add(msl_lib_t *L, msl_fn_t *f) {
    if (L->nfn == L->capfn) {
        int c = L->capfn ? L->capfn * 2 : 8;
        msl_fn_t **n = realloc(L->fns, sizeof(msl_fn_t *) * (size_t)c);
        if (!n) return;
        L->fns = n;
        L->capfn = c;
    }
    L->fns[L->nfn++] = f;
}

/* into: biblioteca já existente para acrescentar, ou 0 para criar uma.
 * fn/attrs/idxs: amarração explícita (o teste). attrs nulo = só metadado e o fallback de ponteiro.
 * Devolve NULL só quando nada entrou e a lib era nova. Função que não baixa é omitida. */
msl_lib_t *mtlair_lower_binds(msl_lib_t *into, const void *data, u64 len, const char *fn,
                              const int *attrs, const int *idxs, int nargs, char *err, int errn) {
    char local[160];
    if (!err) { err = local; errn = sizeof local; }
    err[0] = 0;
    air_module_t *m = open_mod(data, len, err, errn);
    if (!m) return into && into->nfn ? into : 0;
    int created = 0;
    msl_lib_t *lib = into;
    if (!lib) { lib = calloc(1, sizeof *lib); created = 1; }
    if (!lib) { airmod_free(m); if (errn) snprintf(err, errn, "sem memória"); return 0; }
    built_t *b = calloc((size_t)(m->nfns > 0 ? m->nfns : 1), sizeof *b);
    msl_fn_t **fns = calloc((size_t)(m->nfns > 0 ? m->nfns : 1), sizeof *fns);
    if (!b || !fns) { free(b); free(fns); airmod_free(m); if (created) msl_lib_free(lib); if (errn) snprintf(err, errn, "sem memória"); return into; }
    for (int i = 0; i < m->nfns; i++) {
        msl_fn_t *f = msl_alloc(lib, sizeof *f);
        f->name = aname(lib, m->fns[i].name ? m->fns[i].name : "air");
        f->stage = map_stage(m->fns[i].stage);
        f->lib = lib;
        f->ret = msl_tscal(K_VOID);
        fns[i] = f;
        b[i].f = f;
        if (!m->fns[i].has_body) continue;
        bind_meta(m, &m->fns[i]);
        if (attrs && (!fn || (m->fns[i].name && !strcmp(m->fns[i].name, fn)))) {
            int n = nargs < m->fns[i].nargs ? nargs : m->fns[i].nargs;
            if (n > 16) n = 16;
            for (int a = 0; a < n; a++) {
                m->fns[i].arg_attr[a] = attrs[a];
                m->fns[i].arg_idx[a] = idxs ? idxs[a] : 0;
                m->fns[i].arg_set[a] = 1;
            }
        }
        bind_fallback(m, &m->fns[i]);
        lower_body(lib, m, i, fns, &b[i]);
    }
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < m->nfns; i++) if (b[i].ok) {
            for (int c = 0; c < b[i].nc; c++) {
                int k = b[i].cals[c];
                if (k < 0 || k >= m->nfns || !b[k].ok) {
                    b[i].ok = 0;
                    snprintf(b[i].err, sizeof b[i].err, "%s: chama função que não baixou", m->fns[i].name ? m->fns[i].name : "?");
                    changed = 1;
                    break;
                }
            }
        }
    }
    int nok = 0;
    char why[160] = {0};
    for (int i = 0; i < m->nfns; i++) {
        if (b[i].ok && m->fns[i].has_body) { b[i].f->defined = 1; lib_add(lib, b[i].f); nok++; }
        else if (m->fns[i].has_body && !why[0] && b[i].err[0]) snprintf(why, sizeof why, "%s", b[i].err);
        free(b[i].cals);
    }
    free(b);
    free(fns);
    airmod_free(m);
    if (!nok) {
        if (why[0] && errn) snprintf(err, errn, "%s", why);
        else if (errn) snprintf(err, errn, "nenhuma função do AIR baixou");
        if (created) { msl_lib_free(lib); return 0; }
        return into;
    }
    err[0] = 0;
    return lib;
}
msl_lib_t *mtlair_lower(const void *data, u64 len, char *err, int errn) {
    return mtlair_lower_binds(0, data, len, 0, 0, 0, 0, err, errn);
}

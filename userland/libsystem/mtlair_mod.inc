/*
 * mOS libSystem — parser do módulo de bitcode LLVM de um .metallib (incluído por mtlair.c, sobre mtlair_bc.inc).
 * Lê o MODULE_BLOCK: tabela de strings (STRTAB), tabela de tipos (TYPE_BLOCK), globais e funções (MODULE_CODE_*),
 * constantes (CONSTANTS_BLOCK), metadados (METADATA_BLOCK: strings em lote, nós, nós nomeados como air.vertex) e,
 * por função definida, o FUNCTION_BLOCK com as instruções em SSA decodificadas (sem ainda baixar para a árvore).
 * O objetivo deste arquivo é extrair a estrutura; a tradução para os nós do interpretador fica em mtlair_lower.inc.
 */

/* IDs de bloco do LLVM */
#define BLK_BLOCKINFO_ID 0
enum {
    BLK_MODULE = 8, BLK_PARAMATTR = 9, BLK_PARAMATTR_GROUP = 10, BLK_CONSTANTS = 11, BLK_FUNCTION = 12,
    BLK_IDENTIFICATION = 13, BLK_VALUE_SYMTAB = 14, BLK_METADATA = 15, BLK_METADATA_ATTACHMENT = 16,
    BLK_TYPE = 17, BLK_USELIST = 18, BLK_MODULE_STRTAB = 19, BLK_GLOBALVAL_SUMMARY = 20,
    BLK_OPERAND_BUNDLE_TAGS = 21, BLK_METADATA_KIND = 22, BLK_STRTAB = 23, BLK_FULL_LTO_GLOBALVAL_SUMMARY = 24,
    BLK_SYMTAB = 25, BLK_SYNC_SCOPE_NAMES = 26,
};
/* MODULE_CODE_* */
enum { MOD_VERSION = 1, MOD_TRIPLE = 2, MOD_DATALAYOUT = 3, MOD_GLOBALVAR = 7, MOD_FUNCTION = 8, MOD_ALIAS = 14,
       MOD_VSTOFFSET = 13, MOD_SOURCE_FILENAME = 16 };
/* TYPE_CODE_* */
enum { TYC_NUMENTRY = 1, TYC_VOID = 2, TYC_FLOAT = 3, TYC_DOUBLE = 4, TYC_LABEL = 5, TYC_OPAQUE = 6, TYC_INTEGER = 7,
       TYC_POINTER = 8, TYC_HALF = 10, TYC_ARRAY = 11, TYC_VECTOR = 12, TYC_METADATA = 16, TYC_STRUCT_ANON = 18,
       TYC_STRUCT_NAME = 19, TYC_STRUCT_NAMED = 20, TYC_FUNCTION = 21, TYC_TOKEN = 22, TYC_BFLOAT = 23,
       TYC_OPAQUE_POINTER = 25 };
/* METADATA_CODE_* (os que nos interessam) */
enum { MD_STRING_OLD = 1, MD_VALUE = 2, MD_NODE = 3, MD_NAME = 4, MD_KIND = 6, MD_NAMED_NODE = 10,
       MD_STRINGS = 35 };
/* VALUE_SYMTAB */
enum { VST_ENTRY = 1, VST_BBENTRY = 2, VST_FNENTRY = 3 };

#define AIR_MAXFUNC 256

/* ---- tipo do módulo (uma visão simples, suficiente para o lowering decidir) */
typedef struct airty {
    int kind;                /* TYC_* */
    int bits;                /* inteiro */
    int n;                   /* vetor/array: elementos; função: nº de params */
    int elem;                /* índice do tipo do elemento (vetor/array/ponteiro) */
    int ret;                 /* função: índice do retorno */
    int *params;             /* função/struct: índices dos tipos */
    int addrspace;           /* ponteiro */
    int packed;              /* struct empacotada */
} airty_t;

/* valor da lista SSA: módulo (global/função/constante de módulo) ou local da função (arg, constante, instrução) */
enum { AV_NONE = 0, AV_MOD_GLOB, AV_MOD_FN, AV_ARG, AV_CONST, AV_INST };
enum { CK_UNDEF = 0, CK_NULL, CK_INT, CK_FP, CK_AGG, CK_DATA, CK_POISON, CK_CE };
typedef struct airval {
    int kind, ty, aux;       /* aux: índice da função, do argumento ou da instrução */
    int ckind, ce_opc;
    i64 iv;
    u64 bits;
    int *elts;               /* agregados e constexpr: ids absolutos ou o registro cru */
    int nelt;
} airval_t;

typedef struct {
    char *name; u64 name_off, name_len;
    int type; int stage; int has_body; int first_inst, ninsts;
    int nmod_at;             /* tamanho da lista de módulo quando o corpo foi lido */
    int nargs, nbb;
    airval_t *vals;          /* valores locais: args + constantes + resultados */
    int nvals, capvals;
    int arg_attr[16], arg_idx[16], arg_set[16];
} airfn_t;

/* instrução do FUNCTION_BLOCK: operandos crus e, quando dec_ok, ids absolutos */
typedef struct {
    int code, nops;
    i64 *ops;
    int ty;                  /* tipo do resultado; -1 = void */
    int opc, aux;            /* binop/cast/predicado; gep: tipo do elemento */
    int bb;
    int narg, *args;         /* ids absolutos (CALL: args[0] = callee) */
    int nlit, *lits;         /* índices literais, destinos de BR */
    int dec_ok;
} airinst_t;

/* metadado indexado (0 = string, 1 = nó). O id no registro de nó é 1-based (0 = nulo). */
typedef struct { int kind; char *s; int *ops; int nops; } airmd_t;
typedef struct { char name[96]; int *ops; int nops; } airnamed_t;

typedef struct air_module {
    /* strtab global */
    const u8 *strtab;
    u64 strtab_len;
    /* tipos */
    airty_t *types;
    int ntypes, captypes;
    /* funções */
    airfn_t *fns;
    int nfns, capfns;
    /* metadados: strings e nós; nós nomeados air.vertex/air.fragment/air.kernel */
    char **mdstr;
    int nmdstr, capmdstr;
    airmd_t *mds;
    int nmds, capmds;
    airnamed_t *named;
    int nnamed, capnamed;
    /* lista de valores de módulo, na ordem do bitcode (globais, funções, constantes) */
    airval_t *mvals;
    int nmvals, capmvals;
    int relative;            /* MODULE_CODE_VERSION >= 1: ids relativos */
    /* instruções de todos os corpos, concatenadas (airfn_t aponta a faixa) */
    airinst_t *insts;
    int ninsts, capinsts;
    /* do contêiner */
    int nfuncs_container;
    char func_names[AIR_MAXFUNC][96];
    int func_stage[AIR_MAXFUNC];
    /* contagens para diagnóstico */
    int nglobals, nmdnodes, nnamedmd;
    int err;
    char errmsg[200];
} air_module_t;

static void *xrealloc(void *p, int *cap, int n, int elem) {
    if (n < *cap) return p;
    int nc = *cap ? *cap * 2 : 16;
    while (nc <= n) nc *= 2;
    void *np = realloc(p, (size_t)nc * elem);
    *cap = nc;
    return np;
}

static int modfail(air_module_t *m, const char *msg) { m->err = 1; snprintf(m->errmsg, sizeof m->errmsg, "%s", msg); return 0; }

static int ty_add(air_module_t *m, airty_t t) {
    m->types = xrealloc(m->types, &m->captypes, m->ntypes, sizeof(airty_t));
    if (!m->types) return -1;
    m->types[m->ntypes] = t;
    return m->ntypes++;
}

/* ---- STRTAB: strings indexadas por (offset, tamanho) */
static char *strtab_dup(air_module_t *m, u64 off, u64 len) {
    if (!m->strtab || off + len > m->strtab_len) return 0;
    char *s = malloc((size_t)len + 1);
    if (!s) return 0;
    memcpy(s, m->strtab + off, (size_t)len);
    s[len] = 0;
    return s;
}

/* ---- TYPE_BLOCK */
static void parse_types(air_module_t *m, bs_t *b, binfos_t *bi) {
    blk_t k;
    blk_enter(&k, b, BLK_TYPE, bi);
    rec_t r = {0};
    char pending_name[128] = {0};
    for (;;) {
        u32 sub;
        int e = blk_next(&k, &r, &sub, bi);
        if (e == EN_END || e == EN_ERR) break;
        if (e == EN_SUB) { blk_skip(b); continue; }
        airty_t t;
        memset(&t, 0, sizeof t);
        t.kind = r.code;
        switch (r.code) {
        case TYC_NUMENTRY: break;
        case TYC_VOID: case TYC_FLOAT: case TYC_DOUBLE: case TYC_HALF: case TYC_BFLOAT:
        case TYC_LABEL: case TYC_METADATA: case TYC_TOKEN: case TYC_OPAQUE:
            ty_add(m, t); break;
        case TYC_INTEGER: t.bits = r.n >= 1 ? (int)r.ops[0] : 0; ty_add(m, t); break;
        case TYC_POINTER: t.elem = r.n >= 1 ? (int)r.ops[0] : 0; t.addrspace = r.n >= 2 ? (int)r.ops[1] : 0; ty_add(m, t); break;
        case TYC_OPAQUE_POINTER: t.addrspace = r.n >= 1 ? (int)r.ops[0] : 0; ty_add(m, t); break;
        case TYC_ARRAY: t.n = r.n >= 1 ? (int)r.ops[0] : 0; t.elem = r.n >= 2 ? (int)r.ops[1] : 0; ty_add(m, t); break;
        case TYC_VECTOR: t.n = r.n >= 1 ? (int)r.ops[0] : 0; t.elem = r.n >= 2 ? (int)r.ops[1] : 0; ty_add(m, t); break;
        case TYC_FUNCTION: {
            /* [vararg, retty, paramty...] */
            t.ret = r.n >= 2 ? (int)r.ops[1] : 0;
            t.n = r.n > 2 ? r.n - 2 : 0;
            if (t.n) { t.params = calloc(t.n, sizeof(int)); for (int i = 0; i < t.n; i++) t.params[i] = (int)r.ops[2 + i]; }
            ty_add(m, t);
            break;
        }
        case TYC_STRUCT_NAME: {
            int j = 0;
            for (int i = 0; i < r.n && j < 127; i++) pending_name[j++] = (char)r.ops[i];
            pending_name[j] = 0;
            break;
        }
        case TYC_STRUCT_ANON: case TYC_STRUCT_NAMED: {
            int base = 1;   /* [ispacked, elt...] */
            t.packed = r.n >= 1 && r.ops[0] != 0;
            t.n = r.n > base ? r.n - base : 0;
            if (t.n) { t.params = calloc(t.n, sizeof(int)); for (int i = 0; i < t.n; i++) t.params[i] = (int)r.ops[base + i]; }
            ty_add(m, t);
            pending_name[0] = 0;
            break;
        }
        default: ty_add(m, t); break;   /* tipo desconhecido: guarda o slot para os índices baterem */
        }
    }
    free(r.ops);
    blk_free(&k);
}

/* ---- valores e constantes (o lowering resolve ids absolutos) */
static i64 signrot(u64 V) {
    if ((V & 1) == 0) return (i64)(V >> 1);
    if (V != 1) return -(i64)(V >> 1);
    return (i64)(1ULL << 63);
}
static airval_t *val_push(airval_t **v, int *n, int *cap) {
    if (*n == *cap) { *cap = *cap ? *cap * 2 : 16; *v = realloc(*v, sizeof(airval_t) * (size_t)*cap); }
    airval_t *a = &(*v)[(*n)++];
    memset(a, 0, sizeof *a);
    a->ty = -1;
    return a;
}
static int ty_find_int(air_module_t *m, int bits) {
    for (int i = 0; i < m->ntypes; i++) if (m->types[i].kind == TYC_INTEGER && m->types[i].bits == bits) return i;
    return -1;
}
/* lê um par valor/tipo no estilo do BitcodeReader (id relativo; tipo explícito só em referência futura) */
static int dec_vtp(const i64 *ops, int nops, int *slot, int next, int *abs, int *ty) {
    if (*slot >= nops) return 0;
    int rel = (int)ops[(*slot)++];
    int id = next - rel;
    if (id < next) { *abs = id; *ty = -1; return 1; }
    if (*slot >= nops) return 0;
    *ty = (int)ops[(*slot)++];
    *abs = id;
    return 1;
}
static int dec_pop(const i64 *ops, int nops, int *slot, int next, int *abs) {
    if (*slot >= nops) return 0;
    *abs = next - (int)ops[(*slot)++];
    return 1;
}
static int *dup_ints(const u64 *ops, int n) {
    int *e = n ? calloc((size_t)n, sizeof(int)) : 0;
    for (int i = 0; i < n; i++) e[i] = (int)ops[i];
    return e;
}
static void const_push(air_module_t *m, airval_t **vals, int *n, int *cap, int ty, int ckind, i64 iv, u64 bits, int *elts, int nelt, int opc) {
    airval_t *a = val_push(vals, n, cap);
    a->kind = AV_CONST;
    a->ty = ty;
    a->ckind = ckind;
    a->iv = iv;
    a->bits = bits;
    a->elts = elts;
    a->nelt = nelt;
    a->ce_opc = opc;
    (void)m;
}
/* CONSTANTS_BLOCK: cada valor entra na lista (SETTYPE não entra). Ids de agregado/constexpr são absolutos. */
static void parse_constants(air_module_t *m, bs_t *b, binfos_t *bi, airval_t **vals, int *n, int *cap) {
    blk_t k;
    blk_enter(&k, b, BLK_CONSTANTS, bi);
    rec_t r = {0};
    int cur = ty_find_int(m, 32);
    for (;;) {
        u32 sub;
        int e = blk_next(&k, &r, &sub, bi);
        if (e == EN_END || e == EN_ERR) break;
        if (e == EN_SUB) { blk_skip(b); continue; }
        if (r.code == 1) { if (r.n >= 1) cur = (int)r.ops[0]; continue; }          /* SETTYPE */
        if (r.code == 2) const_push(m, vals, n, cap, cur, CK_NULL, 0, 0, 0, 0, 0);
        else if (r.code == 3 || r.code == 26) const_push(m, vals, n, cap, cur, r.code == 26 ? CK_POISON : CK_UNDEF, 0, 0, 0, 0, 0);
        else if (r.code == 4) const_push(m, vals, n, cap, cur, CK_INT, r.n ? signrot((u64)r.ops[0]) : 0, 0, 0, 0, 0);
        else if (r.code == 5) const_push(m, vals, n, cap, cur, CK_INT, r.n ? signrot((u64)r.ops[0]) : 0, 0, 0, 0, 0);
        else if (r.code == 6) const_push(m, vals, n, cap, cur, CK_FP, 0, r.n ? (u64)r.ops[0] : 0, 0, 0, 0);
        else if (r.code == 7 || r.code == 22) const_push(m, vals, n, cap, cur, r.code == 7 ? CK_AGG : CK_DATA, 0, 0, dup_ints(r.ops, r.n), r.n, 0);
        else if (r.code == 10 || r.code == 11 || r.code == 12 || r.code == 20 || r.code == 25 || r.code == 13)
            const_push(m, vals, n, cap, cur, CK_CE, 0, 0, dup_ints(r.ops, r.n), r.n, r.code);
        else const_push(m, vals, n, cap, cur, CK_UNDEF, 0, 0, 0, 0, 0);
    }
    free(r.ops);
    blk_free(&k);
}
static int inst_is_void_code(int c) {
    switch (c) {
    case 10: case 11: case 12: case 15: case 24: case 31: case 33: case 35: case 36:
    case 42: case 44: case 45: case 48: case 49: case 55:
        return 1;
    default: return 0;
    }
}
/* decodifica os operandos no formato do LLVM 18 (ids relativos ao próximo valor). devolve 0 se o registro não fecha. */
static int decode_inst(air_module_t *m, airfn_t *fn, airinst_t *in, int next) {
    i64 *op = in->ops;
    int n = in->nops, slot = 0, abs = 0, ty = -1;
    in->ty = -1;
    in->dec_ok = 0;
    in->args = 0;
    in->lits = 0;
    (void)m;
    (void)fn;
    if (!m->relative && in->code != 1) {
        /* bitcode antigo (versão 0): os ids já são absolutos. Não aparece nos .metallib atuais. */
    }
    int rel = m->relative;
    (void)rel;
    switch (in->code) {
    case 33: case 35: in->dec_ok = 1; return 1; /* DEBUG_LOC: não produz valor */
    case 2: { /* BINOP: [val, val, opcode] */
        int a, b;
        if (!dec_vtp(op, n, &slot, next, &a, &ty) || !dec_pop(op, n, &slot, next, &b) || slot >= n) return 0;
        in->opc = (int)op[slot];
        in->narg = 2; in->args = calloc(2, sizeof(int)); in->args[0] = a; in->args[1] = b;
        in->aux = ty; /* -1: tipo vem do valor */
        in->ty = ty;
        in->dec_ok = 1; return 1;
    }
    case 3: { /* CAST: [val, destty, opc] */
        int a;
        if (!dec_vtp(op, n, &slot, next, &a, &ty) || slot + 1 >= n) return 0;
        in->ty = (int)op[slot];
        in->opc = (int)op[slot + 1];
        in->narg = 1; in->args = calloc(1, sizeof(int)); in->args[0] = a;
        in->dec_ok = 1; return 1;
    }
    case 5: case 29: { /* SELECT / VSELECT */
        int t, f, c;
        if (!dec_vtp(op, n, &slot, next, &t, &ty) || !dec_pop(op, n, &slot, next, &f)) return 0;
        if (in->code == 29) { int ct; if (!dec_vtp(op, n, &slot, next, &c, &ct)) return 0; }
        else { if (!dec_pop(op, n, &slot, next, &c)) return 0; }
        in->ty = ty; in->aux = ty;
        in->narg = 3; in->args = calloc(3, sizeof(int));
        in->args[0] = c; in->args[1] = t; in->args[2] = f;
        in->dec_ok = 1; return 1;
    }
    case 6: { /* EXTRACTELT */
        int v, i, it;
        if (!dec_vtp(op, n, &slot, next, &v, &ty) || !dec_vtp(op, n, &slot, next, &i, &it)) return 0;
        in->aux = ty;
        in->narg = 2; in->args = calloc(2, sizeof(int)); in->args[0] = v; in->args[1] = i;
        in->dec_ok = 1; return 1;
    }
    case 7: { /* INSERTELT: vetor, elemento, índice */
        int v, e, i, it;
        if (!dec_vtp(op, n, &slot, next, &v, &ty) || !dec_pop(op, n, &slot, next, &e) || !dec_vtp(op, n, &slot, next, &i, &it)) return 0;
        in->ty = ty; in->aux = ty;
        in->narg = 3; in->args = calloc(3, sizeof(int));
        in->args[0] = v; in->args[1] = e; in->args[2] = i;
        in->dec_ok = 1; return 1;
    }
    case 8: { /* SHUFFLEVEC */
        int a, b, mask, mt;
        if (!dec_vtp(op, n, &slot, next, &a, &ty)) return 0;
        if (!dec_vtp(op, n, &slot, next, &b, &mt)) return 0;
        if (!dec_vtp(op, n, &slot, next, &mask, &mt)) return 0;
        in->aux = ty;
        in->narg = 3; in->args = calloc(3, sizeof(int));
        in->args[0] = a; in->args[1] = b; in->args[2] = mask;
        in->dec_ok = 1; return 1;
    }
    case 9: case 28: { /* CMP / CMP2 */
        int a, b;
        if (!dec_vtp(op, n, &slot, next, &a, &ty) || !dec_pop(op, n, &slot, next, &b) || slot >= n) return 0;
        in->opc = (int)op[slot];
        in->aux = ty;
        in->narg = 2; in->args = calloc(2, sizeof(int)); in->args[0] = a; in->args[1] = b;
        in->dec_ok = 1; return 1;
    }
    case 10: { /* RET */
        if (n == 0) { in->dec_ok = 1; return 1; }
        int a;
        if (!dec_vtp(op, n, &slot, next, &a, &ty)) return 0;
        in->narg = 1; in->args = calloc(1, sizeof(int)); in->args[0] = a; in->aux = ty;
        in->dec_ok = 1; return 1;
    }
    case 11: { /* BR: [bb] ou [bb, bb, cond] */
        if (n == 1) { in->nlit = 1; in->lits = calloc(1, sizeof(int)); in->lits[0] = (int)op[0]; in->dec_ok = 1; return 1; }
        if (n != 3) return 0;
        in->nlit = 2; in->lits = calloc(2, sizeof(int)); in->lits[0] = (int)op[0]; in->lits[1] = (int)op[1];
        in->narg = 1; in->args = calloc(1, sizeof(int)); in->args[0] = next - (int)op[2];
        in->dec_ok = 1; return 1;
    }
    case 16: { /* PHI: [ty, val0, bb0, ...] valores com VBR com sinal */
        if (n < 1) return 0;
        in->ty = (int)op[0];
        int np = (n - 1) / 2;
        in->narg = np; in->args = calloc(np ? (size_t)np : 1, sizeof(int));
        in->nlit = np; in->lits = calloc(np ? (size_t)np : 1, sizeof(int));
        for (int i = 0; i < np; i++) {
            i64 rels = signrot((u64)op[1 + i * 2]);
            in->args[i] = next - (int)rels;
            in->lits[i] = (int)op[2 + i * 2];
        }
        in->dec_ok = 1; return 1;
    }
    case 19: { /* ALLOCA: [tipo, tipotam, id_absoluto_do_tamanho, flags] */
        if (n < 4) return 0;
        in->aux = (int)op[0];
        in->opc = (int)op[3];
        if (in->opc & (1 << 6)) { /* ExplicitType */ }
        else if (in->aux >= 0 && in->aux < m->ntypes && m->types[in->aux].kind == TYC_POINTER) in->aux = m->types[in->aux].elem;
        in->narg = 1; in->args = calloc(1, sizeof(int)); in->args[0] = (int)op[2]; /* absoluto */
        in->ty = -2; /* ponteiro */
        in->dec_ok = 1; return 1;
    }
    case 20: { /* LOAD: [ptr, (tipo)?, align, vol] */
        int p;
        if (!dec_vtp(op, n, &slot, next, &p, &ty)) return 0;
        if (slot + 3 == n) in->ty = (int)op[slot++];
        else in->ty = ty;
        in->narg = 1; in->args = calloc(1, sizeof(int)); in->args[0] = p;
        in->dec_ok = 1; return 1;
    }
    case 26: { /* EXTRACTVAL: [agg, índices...] */
        int a;
        if (!dec_vtp(op, n, &slot, next, &a, &ty)) return 0;
        in->aux = ty;
        in->narg = 1; in->args = calloc(1, sizeof(int)); in->args[0] = a;
        in->nlit = n - slot; in->lits = calloc(in->nlit ? (size_t)in->nlit : 1, sizeof(int));
        for (int i = 0; i < in->nlit; i++) in->lits[i] = (int)op[slot + i];
        in->dec_ok = 1; return 1;
    }
    case 27: { /* INSERTVAL */
        int a, v, vt;
        if (!dec_vtp(op, n, &slot, next, &a, &ty) || !dec_vtp(op, n, &slot, next, &v, &vt)) return 0;
        in->ty = ty; in->aux = ty;
        in->narg = 2; in->args = calloc(2, sizeof(int)); in->args[0] = a; in->args[1] = v;
        in->nlit = n - slot; in->lits = calloc(in->nlit ? (size_t)in->nlit : 1, sizeof(int));
        for (int i = 0; i < in->nlit; i++) in->lits[i] = (int)op[slot + i];
        in->dec_ok = 1; return 1;
    }
    case 34: { /* CALL: [attr, cc, (fmf)?, (fnty)?, callee, args...] */
        if (n < 3) return 0;
        slot = 2;
        u64 cc = (u64)op[1];
        if ((cc >> 17) & 1) slot++; /* CALL_FMF */
        int fnty = -1;
        if ((cc >> 15) & 1) { if (slot >= n) return 0; fnty = (int)op[slot++]; }
        int callee, ct;
        if (!dec_vtp(op, n, &slot, next, &callee, &ct)) return 0;
        int na = n - slot;
        if (na < 0 || na > 64) return 0;
        in->aux = fnty;
        in->narg = na + 1;
        in->args = calloc((size_t)in->narg, sizeof(int));
        in->args[0] = callee;
        for (int i = 0; i < na; i++) in->args[i + 1] = next - (int)op[slot + i];
        in->ty = -3; /* preenchido pelo tipo da função, se houver */
        if (fnty >= 0 && fnty < m->ntypes) in->ty = m->types[fnty].ret;
        in->dec_ok = 1; return 1;
    }
    case 43: case 4: case 30: { /* GEP */
        if (in->code == 43) {
            if (n < 3) return 0;
            in->opc = (int)op[0];
            in->aux = (int)op[1];
            slot = 2;
        } else slot = 0;
        int base, bt;
        if (!dec_vtp(op, n, &slot, next, &base, &bt)) return 0;
        int ni = 0, tmp[16], tt[16];
        while (slot < n && ni < 16) {
            if (!dec_vtp(op, n, &slot, next, &tmp[ni], &tt[ni])) return 0;
            ni++;
        }
        in->narg = ni + 1;
        in->args = calloc((size_t)in->narg, sizeof(int));
        in->args[0] = base;
        for (int i = 0; i < ni; i++) in->args[i + 1] = tmp[i];
        in->ty = -2;
        in->dec_ok = 1; return 1;
    }
    case 44: case 24: { /* STORE */
        int p, v, vt;
        if (!dec_vtp(op, n, &slot, next, &p, &ty)) return 0;
        if (in->code == 44) { if (!dec_vtp(op, n, &slot, next, &v, &vt)) return 0; }
        else { if (!dec_pop(op, n, &slot, next, &v)) return 0; vt = -1; }
        in->narg = 2; in->args = calloc(2, sizeof(int)); in->args[0] = p; in->args[1] = v;
        in->aux = vt;
        in->dec_ok = 1; return 1;
    }
    case 15: in->dec_ok = 1; return 1;
    default: return 0;
    }
}

static int lookup_ty(air_module_t *m, airfn_t *fn, int abs) {
    if (!fn || abs < 0) return -1;
    if (abs < fn->nmod_at) return abs < m->nmvals ? m->mvals[abs].ty : -1;
    int loc = abs - fn->nmod_at;
    if (loc < 0 || loc >= fn->nvals) return -1;
    return fn->vals[loc].ty;
}
static int find_vec_ty(air_module_t *m, int elem, int n) {
    for (int i = 0; i < m->ntypes; i++)
        if (m->types[i].kind == TYC_VECTOR && m->types[i].elem == elem && m->types[i].n == n) return i;
    return -1;
}
/* o registro do LLVM omite o tipo quando o operando já existe. preenche o tipo do resultado
 * a partir dos operandos que já estão na lista de valores. */
static void fill_result_ty(air_module_t *m, airfn_t *fn, airinst_t *in) {
    if (!in->dec_ok || in->ty >= 0 || !in->narg) return;
    switch (in->code) {
    case 2: case 7: case 27:
        in->ty = lookup_ty(m, fn, in->args[0]);
        break;
    case 5: case 29:
        if (in->narg > 1) in->ty = lookup_ty(m, fn, in->args[1]);
        break;
    case 6: {
        int vt = lookup_ty(m, fn, in->args[0]);
        if (vt >= 0 && vt < m->ntypes && m->types[vt].kind == TYC_VECTOR) in->ty = m->types[vt].elem;
        break;
    }
    case 8: {
        int vt = lookup_ty(m, fn, in->args[0]);
        int mt = in->narg > 2 ? lookup_ty(m, fn, in->args[2]) : -1;
        if (vt >= 0 && vt < m->ntypes && m->types[vt].kind == TYC_VECTOR) {
            int n = m->types[vt].n;
            if (mt >= 0 && mt < m->ntypes && m->types[mt].kind == TYC_VECTOR) n = m->types[mt].n;
            int found = find_vec_ty(m, m->types[vt].elem, n);
            in->ty = found >= 0 ? found : vt;
        }
        break;
    }
    case 26: {
        int t = lookup_ty(m, fn, in->args[0]);
        for (int i = 0; i < in->nlit && t >= 0 && t < m->ntypes; i++) {
            airty_t *at = &m->types[t];
            int ix = in->lits[i];
            if ((at->kind == TYC_STRUCT_ANON || at->kind == TYC_STRUCT_NAMED) && at->params && ix >= 0 && ix < at->n) t = at->params[ix];
            else if ((at->kind == TYC_ARRAY || at->kind == TYC_VECTOR) && ix >= 0) t = at->elem;
            else t = -1;
        }
        in->ty = t;
        break;
    }
    case 9: case 28: {
        int t = lookup_ty(m, fn, in->args[0]);
        int i1 = -1;
        for (int i = 0; i < m->ntypes; i++) if (m->types[i].kind == TYC_INTEGER && m->types[i].bits == 1) { i1 = i; break; }
        if (t >= 0 && t < m->ntypes && m->types[t].kind == TYC_VECTOR && i1 >= 0) {
            int found = find_vec_ty(m, i1, m->types[t].n);
            in->ty = found >= 0 ? found : i1;
        } else if (i1 >= 0) in->ty = i1;
        break;
    }
    default: break;
    }
}

/* ---- FUNCTION_BLOCK: instruções com ids absolutos e constantes do corpo */
static void parse_function_body(air_module_t *m, bs_t *b, binfos_t *bi, airfn_t *fn) {
    blk_t k;
    blk_enter(&k, b, BLK_FUNCTION, bi);
    rec_t r = {0};
    fn->first_inst = m->ninsts;
    fn->nmod_at = m->nmvals;
    /* argumentos entram na lista antes das constantes e das instruções */
    if (fn->type >= 0 && fn->type < m->ntypes && m->types[fn->type].kind == TYC_FUNCTION) {
        airty_t *ft = &m->types[fn->type];
        fn->nargs = ft->n;
        for (int i = 0; i < ft->n; i++) {
            airval_t *a = val_push(&fn->vals, &fn->nvals, &fn->capvals);
            a->kind = AV_ARG;
            a->aux = i;
            a->ty = ft->params ? ft->params[i] : -1;
        }
    }
    int bb = 0;
    for (;;) {
        u32 sub;
        int e = blk_next(&k, &r, &sub, bi);
        if (e == EN_END || e == EN_ERR) break;
        if (e == EN_SUB) {
            if (sub == BLK_CONSTANTS) parse_constants(m, b, bi, &fn->vals, &fn->nvals, &fn->capvals);
            else blk_skip(b);
            continue;
        }
        if (r.code == 1) { if (r.n >= 1) fn->nbb = (int)r.ops[0]; continue; }
        m->insts = xrealloc(m->insts, &m->capinsts, m->ninsts, sizeof(airinst_t));
        if (!m->insts) break;
        airinst_t *in = &m->insts[m->ninsts++];
        memset(in, 0, sizeof *in);
        in->code = r.code;
        in->bb = bb;
        in->ty = -1;
        in->nops = r.n;
        in->ops = r.n ? malloc(sizeof(i64) * (size_t)r.n) : 0;
        for (int i = 0; i < r.n; i++) in->ops[i] = (i64)r.ops[i];
        int next = fn->nmod_at + fn->nvals;
        decode_inst(m, fn, in, next);
        fill_result_ty(m, fn, in);
        int produces = !inst_is_void_code(in->code);
        if (in->code == 34 && in->dec_ok) {
            int rt = in->ty;
            if (rt >= 0 && rt < m->ntypes && m->types[rt].kind == TYC_VOID) produces = 0;
            if (rt < 0) produces = 0;
        }
        if (produces) {
            airval_t *a = val_push(&fn->vals, &fn->nvals, &fn->capvals);
            a->kind = AV_INST;
            a->aux = m->ninsts - 1;
            a->ty = in->ty;
        }
        if (in->code == 10 || in->code == 11 || in->code == 12 || in->code == 15) bb++;
    }
    fn->ninsts = m->ninsts - fn->first_inst;
    fn->has_body = 1;
    if (!fn->nbb) fn->nbb = bb > 0 ? bb : 1;
    free(r.ops);
    blk_free(&k);
}

static airmd_t *md_slot(air_module_t *m) {
    m->mds = xrealloc(m->mds, &m->capmds, m->nmds, sizeof(airmd_t));
    airmd_t *d = &m->mds[m->nmds++];
    memset(d, 0, sizeof *d);
    return d;
}
static void md_add_str(air_module_t *m, char *s) {
    airmd_t *d = md_slot(m);
    d->kind = 0;
    d->s = s;
    m->mdstr = xrealloc(m->mdstr, &m->capmdstr, m->nmdstr, sizeof(char *));
    if (m->mdstr) m->mdstr[m->nmdstr++] = s;
}
/* ---- METADATA_BLOCK: strings em lote e nós nomeados */
static void parse_metadata(air_module_t *m, bs_t *b, binfos_t *bi) {
    blk_t k;
    blk_enter(&k, b, BLK_METADATA, bi);
    rec_t r = {0};
    char named[256] = {0};
    for (;;) {
        u32 sub;
        int e = blk_next(&k, &r, &sub, bi);
        if (e == EN_END || e == EN_ERR) break;
        if (e == EN_SUB) { blk_skip(b); continue; }
        if (r.code == MD_STRINGS) {
            /* [numstrings, offset-para-os-dados, blob]; os tamanhos são VBR6 no começo do blob */
            if (r.n < 2 || !r.blob) continue;
            u32 nstr = (u32)r.ops[0];
            u64 doff = r.ops[1];
            bs_t sb = {r.blob, r.bloblen * 8, 0, 0};
            u64 dpos = doff * 8;
            for (u32 i = 0; i < nstr && !sb.err; i++) {
                u64 slen = bs_vbr(&sb, 6);
                /* a string fica em doff + acumulado; lê slen bytes a partir de dpos */
                char *s = malloc((size_t)slen + 1);
                if (!s) break;
                for (u64 j = 0; j < slen; j++) s[j] = (char)(dpos / 8 + j < r.bloblen ? r.blob[dpos / 8 + j] : 0);
                s[slen] = 0;
                dpos += slen * 8;
                md_add_str(m, s);
            }
        } else if (r.code == MD_STRING_OLD) {
            char *s = malloc((size_t)r.n + 1);
            if (!s) continue;
            for (int i = 0; i < r.n; i++) s[i] = (char)r.ops[i];
            s[r.n] = 0;
            md_add_str(m, s);
        } else if (r.code == MD_NAME) {
            int j = 0;
            for (int i = 0; i < r.n && j < 255; i++) named[j++] = (char)r.ops[i];
            named[j] = 0;
        } else if (r.code == MD_NAMED_NODE) {
            m->nnamedmd++;
            if (m->nnamed == m->capnamed) { m->capnamed = m->capnamed ? m->capnamed * 2 : 8; m->named = realloc(m->named, sizeof(airnamed_t) * (size_t)m->capnamed); }
            airnamed_t *nd = &m->named[m->nnamed++];
            memset(nd, 0, sizeof *nd);
            snprintf(nd->name, sizeof nd->name, "%s", named);
            nd->nops = r.n;
            nd->ops = r.n ? calloc((size_t)r.n, sizeof(int)) : 0;
            for (int i = 0; i < r.n; i++) nd->ops[i] = (int)r.ops[i]; /* índice 0-based do nó */
            named[0] = 0;
        } else if (r.code == MD_NODE || r.code == 5) { /* NODE e DISTINCT_NODE ocupam um índice */
            m->nmdnodes++;
            airmd_t *d = md_slot(m);
            d->kind = 1;
            d->nops = r.n;
            d->ops = r.n ? calloc((size_t)r.n, sizeof(int)) : 0;
            for (int i = 0; i < r.n; i++) d->ops[i] = (int)r.ops[i]; /* 1-based; 0 = nulo */
        } else if (r.code != MD_KIND) {
            /* outros registros (debug, location, value...) também ocupam um índice no LLVM */
            md_slot(m);
        }
    }
    free(r.ops);
    blk_free(&k);
}

/* ---- MODULE_BLOCK */
static int parse_module_block(air_module_t *m, bs_t *b, binfos_t *bi) {
    blk_t k;
    blk_enter(&k, b, BLK_MODULE, bi);
    rec_t r = {0};
    int fnidx = 0;
    for (;;) {
        u32 sub;
        int e = blk_next(&k, &r, &sub, bi);
        if (e == EN_END) break;
        if (e == EN_ERR) { blk_free(&k); return modfail(m, "módulo truncado"); }
        if (e == EN_SUB) {
            if (sub == BLK_BLOCKINFO_ID) read_blockinfo(b, bi);
            else if (sub == BLK_TYPE) parse_types(m, b, bi);
            else if (sub == BLK_METADATA) parse_metadata(m, b, bi);
            else if (sub == BLK_STRTAB) {
                blk_t sk;
                blk_enter(&sk, b, BLK_STRTAB, bi);
                rec_t sr = {0};
                for (;;) { u32 ss; int se = blk_next(&sk, &sr, &ss, bi); if (se == EN_END || se == EN_ERR) break; if (se == EN_SUB) { blk_skip(b); continue; }
                    if (sr.code == 1 && sr.blob) { m->strtab = sr.blob; m->strtab_len = sr.bloblen; } }
                free(sr.ops);
                blk_free(&sk);
            } else if (sub == BLK_CONSTANTS) parse_constants(m, b, bi, &m->mvals, &m->nmvals, &m->capmvals);
            else if (sub == BLK_FUNCTION) {
                /* casa com a próxima função definida (na ordem das declarações com corpo) */
                while (fnidx < m->nfns && !m->fns[fnidx].has_body && m->fns[fnidx].type == -2) fnidx++;
                airfn_t *fn = 0;
                for (int i = fnidx; i < m->nfns; i++) if (!m->fns[i].has_body && m->fns[i].type != -2) { fn = &m->fns[i]; fnidx = i + 1; break; }
                if (fn) parse_function_body(m, b, bi, fn);
                else blk_skip(b);
            } else blk_skip(b);
            continue;
        }
        /* registros do módulo */
        if (r.code == MOD_VERSION) {
            m->relative = r.n >= 1 && r.ops[0] >= 1;
        } else if (r.code == MOD_FUNCTION) {
            /* STRTAB-based: [strtab_off, strtab_size, type, callingconv, isproto, linkage, ...] */
            m->fns = xrealloc(m->fns, &m->capfns, m->nfns, sizeof(airfn_t));
            if (!m->fns) break;
            airfn_t *fn = &m->fns[m->nfns];
            memset(fn, 0, sizeof *fn);
            fn->stage = -1;
            fn->first_inst = -1;
            if (r.n >= 5) {
                fn->name_off = r.ops[0];           /* a STRTAB pode vir depois; resolve no fim */
                fn->name_len = r.ops[1];
                fn->name = strtab_dup(m, r.ops[0], r.ops[1]);   /* se a strtab já foi lida */
                int isproto = (int)r.ops[4];
                /* protótipo (sem corpo): type = -2 faz o casamento de corpos o pular */
                fn->type = isproto ? -2 : (int)r.ops[2];
            } else fn->type = -2;
            airval_t *mv = val_push(&m->mvals, &m->nmvals, &m->capmvals);
            mv->kind = AV_MOD_FN;
            mv->aux = m->nfns;
            mv->ty = fn->type;
            m->nfns++;
        } else if (r.code == MOD_GLOBALVAR || r.code == MOD_ALIAS) {
            if (r.code == MOD_GLOBALVAR) m->nglobals++;
            airval_t *mv = val_push(&m->mvals, &m->nmvals, &m->capmvals);
            mv->kind = AV_MOD_GLOB;
        }
    }
    free(r.ops);
    blk_free(&k);
    return 1;
}


void airmod_free(air_module_t *m);
static air_module_t *airmod_parse(const u8 *bc, u64 bclen, char *err, int errn) {
    air_module_t *m = calloc(1, sizeof *m);
    if (!m) { if (err) snprintf(err, errn, "sem memória"); return 0; }
    m->relative = 1;
    bs_t b = {bc, bclen * 8, 0, 0};
    binfos_t bi = {0, 0};
    /* o topo: ENTER_SUBBLOCK (abbrev width 2) para IDENTIFICATION e depois MODULE */
    int ok = 0;
    while (b.pos < b.nbits && !b.err) {
        u64 abbr = bs_read(&b, 2);
        if (abbr != 1) {   /* no topo só há ENTER_SUBBLOCK; outra coisa = fim/alinhamento */
            if (b.pos >= b.nbits - 32) break;
            continue;
        }
        u32 id = (u32)bs_vbr(&b, 8);
        if (id == BLK_MODULE) { ok = parse_module_block(m, &b, &bi); if (!ok) break; }
        else if (id == BLK_BLOCKINFO_ID) read_blockinfo(&b, &bi);
        else if (id == BLK_STRTAB) {   /* a STRTAB costuma ser um bloco de topo depois do módulo */
            blk_t sk;
            blk_enter(&sk, &b, BLK_STRTAB, &bi);
            rec_t sr = {0};
            for (;;) { u32 ss; int se = blk_next(&sk, &sr, &ss, &bi); if (se == EN_END || se == EN_ERR) break; if (se == EN_SUB) { blk_skip(&b); continue; }
                if (sr.code == 1 && sr.blob) { m->strtab = sr.blob; m->strtab_len = sr.bloblen; } }
            free(sr.ops);
            blk_free(&sk);
        } else blk_skip(&b);
    }
    for (int i = 0; i < bi.n; i++) { for (int j = 0; j < bi.b[i].ab.n; j++) free(bi.b[i].ab.a[j].ops); free(bi.b[i].ab.a); }
    free(bi.b);
    if (!ok && !m->err) modfail(m, "nenhum MODULE_BLOCK");
    if (m->err) { if (err) snprintf(err, errn, "%s", m->errmsg); airmod_free(m); return 0; }
    /* resolve os nomes que ficaram pendentes (STRTAB lida depois dos registros de função) */
    for (int i = 0; i < m->nfns; i++)
        if (!m->fns[i].name && m->fns[i].name_len) m->fns[i].name = strtab_dup(m, m->fns[i].name_off, m->fns[i].name_len);
    /* casa os estágios do contêiner com as funções pelo nome */
    for (int i = 0; i < m->nfuncs_container; i++)
        for (int j = 0; j < m->nfns; j++)
            if (m->fns[j].name && !strcmp(m->fns[j].name, m->func_names[i])) m->fns[j].stage = m->func_stage[i];
    return m;
}

static void val_list_free(airval_t *v, int n) {
    for (int i = 0; i < n; i++) free(v[i].elts);
    free(v);
}
void airmod_free(air_module_t *m) {
    if (!m) return;
    for (int i = 0; i < m->ntypes; i++) free(m->types[i].params);
    free(m->types);
    for (int i = 0; i < m->nfns; i++) {
        free(m->fns[i].name);
        val_list_free(m->fns[i].vals, m->fns[i].nvals);
    }
    free(m->fns);
    /* as strings de mdstr são as mesmas de mds[].s */
    free(m->mdstr);
    for (int i = 0; i < m->nmds; i++) { free(m->mds[i].s); free(m->mds[i].ops); }
    free(m->mds);
    for (int i = 0; i < m->nnamed; i++) free(m->named[i].ops);
    free(m->named);
    val_list_free(m->mvals, m->nmvals);
    for (int i = 0; i < m->ninsts; i++) { free(m->insts[i].ops); free(m->insts[i].args); free(m->insts[i].lits); }
    free(m->insts);
    free(m);
}

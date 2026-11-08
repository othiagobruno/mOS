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
} airty_t;

typedef struct { char *name; u64 name_off, name_len; int type; int stage; int has_body; int first_inst, ninsts; } airfn_t;

/* uma instrução decodificada do FUNCTION_BLOCK: código + operandos crus (ainda em IDs relativos/valores) */
typedef struct { int code; int nops; i64 *ops; int ty; } airinst_t;

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
            int base = r.code == TYC_STRUCT_NAMED ? 1 : 1;   /* [ispacked, elt...] */
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

/* ---- FUNCTION_BLOCK: só conta e guarda as instruções cruas (o lowering as traduz) */
static void parse_function_body(air_module_t *m, bs_t *b, binfos_t *bi, airfn_t *fn) {
    blk_t k;
    blk_enter(&k, b, BLK_FUNCTION, bi);
    rec_t r = {0};
    fn->first_inst = m->ninsts;
    for (;;) {
        u32 sub;
        int e = blk_next(&k, &r, &sub, bi);
        if (e == EN_END || e == EN_ERR) break;
        if (e == EN_SUB) { blk_skip(b); continue; }   /* CONSTANTS/VST/METADATA_ATTACH dentro do corpo: por ora pulados */
        /* DECLAREBLOCKS (1) não é instrução */
        if (r.code == 1) continue;
        m->insts = xrealloc(m->insts, &m->capinsts, m->ninsts, sizeof(airinst_t));
        if (!m->insts) break;
        airinst_t *in = &m->insts[m->ninsts++];
        in->code = r.code;
        in->nops = r.n;
        in->ops = r.n ? malloc(sizeof(i64) * (size_t)r.n) : 0;
        for (int i = 0; i < r.n; i++) in->ops[i] = (i64)r.ops[i];
        in->ty = 0;
    }
    fn->ninsts = m->ninsts - fn->first_inst;
    fn->has_body = 1;
    free(r.ops);
    blk_free(&k);
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
                m->mdstr = xrealloc(m->mdstr, &m->capmdstr, m->nmdstr, sizeof(char *));
                if (m->mdstr) m->mdstr[m->nmdstr++] = s; else free(s);
            }
        } else if (r.code == MD_NAME) {
            int j = 0;
            for (int i = 0; i < r.n && j < 255; i++) named[j++] = (char)r.ops[i];
            named[j] = 0;
        } else if (r.code == MD_NAMED_NODE) {
            m->nnamedmd++;
            named[0] = 0;
        } else if (r.code == MD_NODE) {
            m->nmdnodes++;
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
            } else if (sub == BLK_FUNCTION) {
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
        if (r.code == MOD_FUNCTION) {
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
            m->nfns++;
        } else if (r.code == MOD_GLOBALVAR) {
            m->nglobals++;
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

void airmod_free(air_module_t *m) {
    if (!m) return;
    for (int i = 0; i < m->ntypes; i++) free(m->types[i].params);
    free(m->types);
    for (int i = 0; i < m->nfns; i++) free(m->fns[i].name);
    free(m->fns);
    for (int i = 0; i < m->nmdstr; i++) free(m->mdstr[i]);
    free(m->mdstr);
    for (int i = 0; i < m->ninsts; i++) free(m->insts[i].ops);
    free(m->insts);
    free(m);
}

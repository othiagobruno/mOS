/*
 * mOS libSystem — lowering do AIR (bitcode LLVM de um .metallib) para a árvore do interpretador de MSL (mtlint.h),
 * incluído por mtlair.c depois de mtlair_mod.inc. Primeira parte: o modelo de valores SSA e a tradução de tipos, e um
 * passo de cobertura que diz, por função, quantas instruções e quais intrínsecos air.* o lowering já sabe tratar. A
 * emissão dos nós vem a seguir sobre esta base.
 *
 * Numeração de valores do LLVM num corpo: primeiro os valores de módulo (globais + funções), depois os argumentos da
 * função, depois os resultados das instruções, na ordem. Os operandos vêm em IDs RELATIVOS (padrão pós-LLVM 3.2):
 * valor = (nº do valor atual) - idRelativo. Este arquivo resolve isso e mapeia o tipo de cada valor.
 */

/* um valor SSA: de onde veio e seu tipo (índice na tabela de tipos do módulo) */
enum { V_MODULE = 0, V_ARG, V_INST, V_CONST };
typedef struct { int kind; int type; int inst; } airval_t;

/* o tipo LLVM traduzido para a visão do interpretador (escalar + nº de componentes), ou 0 se não mapeável */
typedef struct { int scal; int n; int ok; } airscal_t;

static airscal_t air_map_type(air_module_t *m, int ti) {
    airscal_t r = {0, 0, 0};
    if (ti < 0 || ti >= m->ntypes) return r;
    airty_t *t = &m->types[ti];
    switch (t->kind) {
    case TYC_FLOAT: r.scal = MSL_FLOAT; r.n = 1; r.ok = 1; break;
    case TYC_HALF: r.scal = MSL_HALF; r.n = 1; r.ok = 1; break;
    case TYC_INTEGER:
        r.n = 1; r.ok = 1;
        r.scal = t->bits <= 1 ? MSL_BOOL : t->bits <= 8 ? MSL_CHAR : t->bits <= 16 ? MSL_SHORT : t->bits <= 32 ? MSL_INT : MSL_LONG;
        break;
    case TYC_VECTOR: {
        airscal_t e = air_map_type(m, t->elem);
        if (e.ok) { r.scal = e.scal; r.n = t->n; r.ok = 1; }
        break;
    }
    case TYC_POINTER: case TYC_OPAQUE_POINTER: r.scal = MSL_UINT; r.n = 1; r.ok = 1; break;   /* ponteiro: tratado por acesso */
    default: break;
    }
    return r;
}

/* ---- intrínsecos air.* que as CALL usam: reconhecidos pelo nome mangado da função-alvo */
enum {
    AI_UNKNOWN = 0, AI_SAMPLE, AI_SAMPLE_CMP, AI_READ, AI_WRITE, AI_GATHER, AI_GET_W, AI_GET_H, AI_GET_D,
    AI_CONVERT, AI_FABS, AI_FMIN, AI_FMAX, AI_CLAMP, AI_FLOOR, AI_CEIL, AI_FRACT, AI_RINT, AI_TRUNC,
    AI_SQRT, AI_RSQRT, AI_SIN, AI_COS, AI_EXP2, AI_LOG2, AI_POW, AI_FMA, AI_DOT, AI_NORMALIZE, AI_LENGTH,
    AI_CROSS, AI_MIX, AI_STEP, AI_SMOOTHSTEP, AI_SATURATE, AI_SIGN, AI_DFDX, AI_DFDY, AI_FWIDTH, AI_DISCARD,
    AI_UNPACK, AI_PACK, AI_ATOMIC, AI_BARRIER, AI_IGNORE,
};
/* tabela nome->intrínseco: casa por substring do nome mangado do AIR (air.sample.*, _Z... metal::...) */
static int air_intrinsic(const char *name) {
    if (!name) return AI_UNKNOWN;
    struct { const char *s; int id; } tab[] = {
        {"air.sample_compare", AI_SAMPLE_CMP}, {"air.sample", AI_SAMPLE}, {"air.gather", AI_GATHER},
        {"air.read", AI_READ}, {"air.write", AI_WRITE},
        {"air.get_width", AI_GET_W}, {"air.get_height", AI_GET_H}, {"air.get_depth", AI_GET_D},
        {"air.convert", AI_CONVERT}, {"air.fabs", AI_FABS}, {"air.fmin", AI_FMIN}, {"air.fmax", AI_FMAX},
        {"air.clamp", AI_CLAMP}, {"air.floor", AI_FLOOR}, {"air.ceil", AI_CEIL}, {"air.fract", AI_FRACT},
        {"air.rint", AI_RINT}, {"air.trunc", AI_TRUNC}, {"air.sqrt", AI_SQRT}, {"air.rsqrt", AI_RSQRT},
        {"air.fast_sqrt", AI_SQRT}, {"air.fast_rsqrt", AI_RSQRT}, {"air.sin", AI_SIN}, {"air.cos", AI_COS},
        {"air.exp2", AI_EXP2}, {"air.log2", AI_LOG2}, {"air.pow", AI_POW}, {"air.fma", AI_FMA},
        {"air.dot", AI_DOT}, {"air.normalize", AI_NORMALIZE}, {"air.length", AI_LENGTH}, {"air.cross", AI_CROSS},
        {"air.mix", AI_MIX}, {"air.step", AI_STEP}, {"air.smoothstep", AI_SMOOTHSTEP}, {"air.saturate", AI_SATURATE},
        {"air.sign", AI_SIGN}, {"air.dfdx", AI_DFDX}, {"air.dfdy", AI_DFDY}, {"air.fwidth", AI_FWIDTH},
        {"air.discard_fragment", AI_DISCARD}, {"air.unpack", AI_UNPACK}, {"air.pack", AI_PACK},
        {"air.atomic", AI_ATOMIC}, {"air.wg.barrier", AI_BARRIER}, {"air.simdgroup", AI_IGNORE},
        {"llvm.", AI_IGNORE}, {"air.lifetime", AI_IGNORE}, {"air.compile", AI_IGNORE},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strstr(name, tab[i].s)) return tab[i].id;
    return AI_UNKNOWN;
}

/* instruções que o lowering sabe (ou vai saber) emitir */
static int air_opcode_lowerable(int code) {
    switch (code) {
    case 2: case 3: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 16: case 19:
    case 20: case 26: case 27: case 28: case 29: case 34: case 43: case 44: case 35: case 1:
        return 1;
    default: return 0;
    }
}

/* resultado de um passo de cobertura por módulo */
typedef struct {
    int ninsts, lowerable_insts;
    int ncalls, known_calls;
    int nfns_body, fns_full;        /* funções cujas instruções são todas tratáveis */
    char worst[128];                /* uma razão da função não-coberta */
} air_coverage_t;

/* nome da função alvo de uma CALL: o último operando é o callee (valor de módulo); resolve pelo índice de função */
static const char *air_call_target(air_module_t *m, airinst_t *in, int base_module_vals, int val_at) {
    /* CALL: [paramattrs, cc, fmf?, fnty, fnid, args...]; o fnid é relativo. Aproximação: procura entre os operandos
     * um que caia na faixa dos valores de módulo (funções) e usa o nome. */
    for (int k = 0; k < in->nops; k++) {
        i64 rel = in->ops[k];
        i64 v = val_at - rel;
        if (v >= 0 && v < m->nglobals + m->nfns && v >= m->nglobals) {
            int fi = (int)(v - m->nglobals);
            if (fi >= 0 && fi < m->nfns && m->fns[fi].name) return m->fns[fi].name;
        }
        if (rel >= 0 && rel < m->nfns && m->fns[rel].name) { /* alguns encodings usam o índice absoluto da função */ }
    }
    (void)base_module_vals;
    return 0;
}

static void air_coverage(air_module_t *m, air_coverage_t *cov) {
    memset(cov, 0, sizeof *cov);
    int module_vals = m->nglobals + m->nfns;
    for (int i = 0; i < m->nfns; i++) {
        airfn_t *fn = &m->fns[i];
        if (!fn->has_body) continue;
        cov->nfns_body++;
        int val = module_vals;   /* aproximação: sem contar os args, só para a resolução de callee relativa */
        int full = 1;
        for (int j = 0; j < fn->ninsts; j++) {
            airinst_t *in = &m->insts[fn->first_inst + j];
            cov->ninsts++;
            int low = air_opcode_lowerable(in->code);
            if (low) cov->lowerable_insts++;
            else { full = 0; if (!cov->worst[0]) snprintf(cov->worst, sizeof cov->worst, "%s: opcode %d", fn->name ? fn->name : "?", in->code); }
            if (in->code == 34) {   /* CALL */
                cov->ncalls++;
                const char *tgt = air_call_target(m, in, module_vals, val);
                int ai = air_intrinsic(tgt);
                if (ai != AI_UNKNOWN) cov->known_calls++;
                else { full = 0; if (!cov->worst[0]) snprintf(cov->worst, sizeof cov->worst, "%s: call %s", fn->name ? fn->name : "?", tgt ? tgt : "(alvo?)"); }
            }
            /* resultados avançam o número de valor (aproximação: toda instrução não-void conta como 1) */
            if (in->code != 10 && in->code != 11 && in->code != 44 && in->code != 35 && in->code != 1) val++;
        }
        if (full) cov->fns_full++;
    }
}

/* exposto para o metal.c decidir se tenta o AIR (o host chama air_coverage direto; aqui depende do contêiner) */
#ifndef MTLAIR_HOST
int mtlair_coverage(const void *data, u64 len, int *ninsts, int *lowerable, int *ncalls, int *known_calls,
                    int *fns_body, int *fns_full, char *worst, int worstn) {
    air_container_t c;
    if (!air_open(&c, data, len)) { air_close(&c); return -1; }
    char err[64];
    air_module_t *m = airmod_parse(c.bc, c.bclen, err, sizeof err);
    air_close(&c);
    if (!m) return -1;
    air_coverage_t cov;
    air_coverage(m, &cov);
    if (ninsts) *ninsts = cov.ninsts;
    if (lowerable) *lowerable = cov.lowerable_insts;
    if (ncalls) *ncalls = cov.ncalls;
    if (known_calls) *known_calls = cov.known_calls;
    if (fns_body) *fns_body = cov.nfns_body;
    if (fns_full) *fns_full = cov.fns_full;
    if (worst && worstn) snprintf(worst, worstn, "%s", cov.worst);
    airmod_free(m);
    return 0;
}
#endif

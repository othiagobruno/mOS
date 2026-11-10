/*
 * mOS libSystem — lowering do AIR (bitcode LLVM de um .metallib) para a árvore do interpretador de MSL (mtlint.h),
 * incluído por mtlair.c depois de mtlair_mod.inc.
 *
 * A lista de valores (ids absolutos) já vem do parser: módulo, depois argumentos, constantes do corpo e resultados.
 * Os 17 opcodes medidos nos .metallib (e SELECT) viram nós. CALL de air.* / llvm.* vira builtin; o que não tem
 * tradução faz a função ser omitida (não entra na biblioteca), com o motivo na mensagem se nenhuma entrou.
 *
 * Ponteiros opacos são ponteiros de byte. O GEP soma o deslocamento em bytes (não usa a soma escalada de ponteiro
 * do interpretador). O CFG vira um while com um inteiro de bloco, porque o interpretador só tem controle estruturado.
 * PHI é cópia paralela no fim de cada predecessor.
 */

/* ---- intrínsecos air.* que as CALL usam: reconhecidos pelo nome mangado da função-alvo */
enum {
    AI_UNKNOWN = 0, AI_SAMPLE, AI_SAMPLE_CMP, AI_READ, AI_WRITE, AI_GATHER, AI_GET_W, AI_GET_H, AI_GET_D,
    AI_CONVERT, AI_FABS, AI_FMIN, AI_FMAX, AI_CLAMP, AI_FLOOR, AI_CEIL, AI_FRACT, AI_RINT, AI_TRUNC,
    AI_SQRT, AI_RSQRT, AI_SIN, AI_COS, AI_EXP2, AI_LOG2, AI_POW, AI_FMA, AI_DOT, AI_NORMALIZE, AI_LENGTH,
    AI_CROSS, AI_MIX, AI_STEP, AI_SMOOTHSTEP, AI_SATURATE, AI_SIGN, AI_DFDX, AI_DFDY, AI_FWIDTH, AI_DISCARD,
    AI_UNPACK, AI_PACK, AI_ATOMIC, AI_BARRIER, AI_IGNORE, AI_COPYSIGN,
};
/* tabela nome->intrínseco: casa por substring do nome mangado do AIR (air.sample.*, _Z... metal::...) */
static int air_intrinsic(const char *name) {
    if (!name) return AI_UNKNOWN;
    /* o específico vem antes do prefixo. llvm.* desconhecido NÃO é ignorado: a função falha. */
    struct { const char *s; int id; } tab[] = {
        {"llvm.fmuladd", AI_FMA}, {"llvm.fma", AI_FMA}, {"llvm.fabs", AI_FABS},
        {"llvm.sqrt", AI_SQRT}, {"llvm.maxnum", AI_FMAX}, {"llvm.minnum", AI_FMIN},
        {"llvm.floor", AI_FLOOR}, {"llvm.ceil", AI_CEIL}, {"llvm.trunc", AI_TRUNC},
        {"llvm.rint", AI_RINT}, {"llvm.round", AI_RINT}, {"llvm.sin", AI_SIN}, {"llvm.cos", AI_COS},
        {"llvm.pow", AI_POW}, {"llvm.exp2", AI_EXP2}, {"llvm.log2", AI_LOG2}, {"llvm.copysign", AI_COPYSIGN},
        {"air.copysign", AI_COPYSIGN},
        {"llvm.lifetime", AI_IGNORE}, {"llvm.dbg", AI_IGNORE}, {"llvm.donothing", AI_IGNORE},
        {"llvm.invariant", AI_IGNORE}, {"llvm.assume", AI_IGNORE}, {"llvm.expect", AI_CONVERT},
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
        {"air.lifetime", AI_IGNORE}, {"air.compile", AI_IGNORE},
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

/* nome da função alvo de uma CALL já decodificada (args[0] = id absoluto do callee) */
static const char *air_call_target(air_module_t *m, airinst_t *in, int base_module_vals, int val_at) {
    (void)base_module_vals;
    (void)val_at;
    if (in->dec_ok && in->narg >= 1) {
        int id = in->args[0];
        if (id >= 0 && id < m->nmvals && m->mvals[id].kind == AV_MOD_FN) {
            int fi = m->mvals[id].aux;
            if (fi >= 0 && fi < m->nfns) return m->fns[fi].name;
        }
    }
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
#include "mtlair_emit.inc"
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

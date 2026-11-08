/*
 * mOS — ferramenta de host: lê um ou mais .metallib e imprime o que o leitor de AIR do mOS extrai (contêiner MTLB +
 * módulo de bitcode LLVM): funções e estágios, tipos, globais, strings de metadados e as instruções por corpo.
 * Valida o parser (userland/libsystem/mtlair.c) contra arquivos reais, sem a VM.
 *   zig cc -DMTLAIR_HOST -O2 tools/airdump.c -o build/airdump.exe
 *   build/airdump.exe caminho/para/shader.metallib
 */
#define MTLAIR_HOST 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* o mtlair.c inclui "mtlint.h" quando não é host; no host, só os tipos básicos */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;
#include "../userland/libsystem/mtlair_bc.inc"
#include "../userland/libsystem/mtlair_mod.inc"
#include "../userland/libsystem/mtlair_lower.inc"

/* copiado da parte de contêiner de mtlair.c (para não arrastar o resto do libSystem no host) */
typedef struct { char name[96]; int stage; uint64_t bc_off, pub_off, priv_off; } air_func_t;
typedef struct { const uint8_t *data; uint64_t len; air_func_t *funcs; int nfuncs; const uint8_t *bc; uint64_t bclen; int err; char errmsg[160]; } air_container_t;
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t rd64(const uint8_t *p) { return (uint64_t)rd32(p) | (uint64_t)rd32(p + 4) << 32; }
static int air_fail(air_container_t *c, const char *m) { c->err = 1; snprintf(c->errmsg, sizeof c->errmsg, "%s", m); return 0; }
static int air_parse_funclist(air_container_t *c, uint64_t off, uint64_t size) {
    if (off + 4 > c->len) return air_fail(c, "lista fora do arquivo");
    const uint8_t *p = c->data + off, *end = c->data + c->len;
    (void)size;
    uint32_t n = rd32(p); p += 4;
    if (n > 65536) return air_fail(c, "n absurdo");
    c->funcs = calloc(n ? n : 1, sizeof(air_func_t));
    for (uint32_t i = 0; i < n; i++) {
        if (p + 4 > end) return air_fail(c, "entrada truncada");
        uint32_t glen = rd32(p); const uint8_t *g = p + 4, *ge = p + glen;   /* o tamanho inclui o proprio campo */
        if (glen < 4 || ge > end) return air_fail(c, "grupo fora");
        air_func_t *f = &c->funcs[c->nfuncs]; f->stage = -1;
        while (g + 4 <= ge) {
            uint32_t tag = rd32(g);
            if (tag == 0x54444e45) break;
            if (g + 6 > ge) break;
            uint16_t tl = rd16(g + 4); const uint8_t *v = g + 6;
            if (v + tl > ge) break;
            if (tag == 0x454d414e) { uint32_t cp = tl < sizeof f->name ? tl : (uint32_t)sizeof f->name - 1; memcpy(f->name, v, cp); f->name[cp && !f->name[cp-1] ? cp-1 : cp] = 0; }
            else if (tag == 0x45505954) { if (tl >= 1) f->stage = v[0]; }
            else if (tag == 0x5446464f) { if (tl >= 24) { f->pub_off = rd64(v); f->priv_off = rd64(v+8); f->bc_off = rd64(v+16); } }
            g = v + tl;
        }
        c->nfuncs++; p += glen;
    }
    return 1;
}
static int air_locate_bitcode(air_container_t *c, uint64_t off, uint64_t size) {
    if (off + 4 > c->len) return air_fail(c, "bitcode fora");
    const uint8_t *p = c->data + off; uint64_t avail = c->len - off;
    if (size && size < avail) avail = size;
    if (avail >= 20 && rd32(p) == 0x0B17C0DE) { uint64_t bo = rd32(p+8), bs = rd32(p+12); if (bo+bs > avail) return air_fail(c, "wrapper ruim"); p += bo; avail -= bo; if (bs) avail = bs; }
    if (avail < 4 || !(p[0]=='B' && p[1]=='C' && p[2]==0xC0 && p[3]==0xDE)) return air_fail(c, "sem magia BC");
    c->bc = p + 4; c->bclen = avail - 4; return 1;
}
static int air_open(air_container_t *c, const uint8_t *d, uint64_t len) {
    memset(c, 0, sizeof *c); c->data = d; c->len = len;
    if (len < 0x58 || !(d[0]=='M'&&d[1]=='T'&&d[2]=='L'&&d[3]=='B')) return air_fail(c, "não é MTLB");
    uint64_t flo = rd64(d+0x18), fls = rd64(d+0x20), bco = rd64(d+0x48), bcs = rd64(d+0x50);
    if (!air_parse_funclist(c, flo, fls)) return 0;
    if (!air_locate_bitcode(c, bco, bcs)) return 0;
    return 1;
}
static void air_close(air_container_t *c) { free(c->funcs); }

static const char *stage_name(int s) { return s == 0 ? "vertex" : s == 1 ? "fragment" : s == 2 ? "kernel" : "?"; }

static int dump(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("%s: nao abriu\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n);
    if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return 1; }
    fclose(f);
    air_container_t c;
    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    if (!air_open(&c, d, n)) { printf("%s: ERRO contêiner: %s\n", base, c.errmsg); free(c.funcs); free(d); return 1; }
    printf("== %s (%ld bytes) ==\n", base, n);
    printf("  contêiner: %d função(ões); bitcode %llu bytes\n", c.nfuncs, (unsigned long long)c.bclen);
    for (int i = 0; i < c.nfuncs; i++) printf("    [%s] %s (bc@%llu)\n", stage_name(c.funcs[i].stage), c.funcs[i].name, (unsigned long long)c.funcs[i].bc_off);
    char err[256] = {0};
    air_module_t *m = airmod_parse(c.bc, c.bclen, err, sizeof err);
    if (!m) { printf("  módulo: ERRO: %s\n", err); free(c.funcs); free(d); return 1; }
    int bodies = 0, protos = 0, totinsts = 0;
    for (int i = 0; i < m->nfns; i++) { if (m->fns[i].has_body) bodies++; else protos++; totinsts += m->fns[i].ninsts; }
    printf("  módulo: %d tipos, %d funções (%d com corpo, %d protótipos), %d globais\n", m->ntypes, m->nfns, bodies, protos, m->nglobals);
    printf("          %d nós de metadados, %d nós nomeados, %d strings de metadados, %d instruções\n", m->nmdnodes, m->nnamedmd, m->nmdstr, m->ninsts);
    for (int i = 0; i < m->nfns; i++)
        if (m->fns[i].has_body) printf("    corpo: %s [%s] — %d instruções\n", m->fns[i].name ? m->fns[i].name : "?", stage_name(m->fns[i].stage), m->fns[i].ninsts);
    /* amostra de strings de metadados que ajudam a confirmar (air.*, nomes de tipo) */
    int shown = 0;
    for (int i = 0; i < m->nmdstr && shown < 6; i++)
        if (m->mdstr[i] && (strstr(m->mdstr[i], "air.") || strstr(m->mdstr[i], "metal"))) { printf("    md: \"%s\"\n", m->mdstr[i]); shown++; }
    airmod_free(m);
    free(c.funcs);
    free(d);
    return 0;
}

/* histograma dos códigos de instrução do FUNCTION_BLOCK, por todos os arquivos */
static int g_hist[64];
static const char *func_code_name(int c) {
    switch (c) {
    case 1: return "DECLAREBLOCKS"; case 2: return "BINOP"; case 3: return "CAST"; case 4: return "GEP_OLD";
    case 5: return "SELECT"; case 6: return "EXTRACTELT"; case 7: return "INSERTELT"; case 8: return "SHUFFLEVEC";
    case 9: return "CMP"; case 10: return "RET"; case 11: return "BR"; case 12: return "SWITCH"; case 13: return "INVOKE";
    case 15: return "UNREACHABLE"; case 16: return "PHI"; case 19: return "ALLOCA"; case 20: return "LOAD";
    case 23: return "VAARG"; case 24: return "STORE_OLD"; case 26: return "EXTRACTVAL"; case 27: return "INSERTVAL";
    case 28: return "CMP2"; case 29: return "VSELECT"; case 30: return "INBOUNDS_GEP_OLD"; case 31: return "INDIRECTBR";
    case 34: return "CALL"; case 35: return "DEBUG_LOC"; case 36: return "FENCE"; case 37: return "CMPXCHG_OLD";
    case 38: return "ATOMICRMW_OLD"; case 39: return "RESUME"; case 40: return "LANDINGPAD_OLD"; case 41: return "LOADATOMIC";
    case 42: return "STOREATOMIC_OLD"; case 43: return "GEP"; case 44: return "STORE"; case 45: return "STOREATOMIC";
    case 46: return "CMPXCHG"; case 47: return "LANDINGPAD"; case 48: return "CLEANUPRET"; case 49: return "CATCHRET";
    case 54: return "FREEZE"; case 56: return "CALLBR";
    default: return "?";
    }
}
static void hist_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n); if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return; } fclose(f);
    air_container_t c;
    if (!air_open(&c, d, n)) { free(c.funcs); free(d); return; }
    char err[256];
    air_module_t *m = airmod_parse(c.bc, c.bclen, err, sizeof err);
    if (m) { for (int i = 0; i < m->ninsts; i++) if (m->insts[i].code < 64) g_hist[m->insts[i].code]++; airmod_free(m); }
    free(c.funcs); free(d);
}

static void dump_insts(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n); if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return; } fclose(f);
    air_container_t c;
    if (!air_open(&c, d, n)) { free(c.funcs); free(d); return; }
    char err[256];
    air_module_t *m = airmod_parse(c.bc, c.bclen, err, sizeof err);
    if (!m) { printf("erro: %s\n", err); free(c.funcs); free(d); return; }
    for (int i = 0; i < m->nfns; i++) {
        if (!m->fns[i].has_body) continue;
        printf("função %s (%d instruções):\n", m->fns[i].name ? m->fns[i].name : "?", m->fns[i].ninsts);
        for (int j = 0; j < m->fns[i].ninsts; j++) {
            airinst_t *in = &m->insts[m->fns[i].first_inst + j];
            printf("  %2d %-14s ops:", j, func_code_name(in->code));
            for (int k = 0; k < in->nops && k < 10; k++) printf(" %lld", (long long)in->ops[k]);
            printf("\n");
        }
    }
    airmod_free(m); free(c.funcs); free(d);
}

int main(int argc, char **argv) {
    int hist = 0, start = 1;
    if (argc >= 2 && !strcmp(argv[1], "-h")) { hist = 1; start = 2; }
    if (argc >= 3 && !strcmp(argv[1], "-i")) { dump_insts(argv[2]); return 0; }
    if (argc <= start) { printf("uso: airdump [-h] <arquivo.metallib> ...\n"); return 2; }
    if (hist) {
        for (int i = start; i < argc; i++) hist_file(argv[i]);
        printf("histograma de opcodes (FUNCTION_BLOCK) em %d arquivos:\n", argc - start);
        for (int i = 0; i < 64; i++) if (g_hist[i]) printf("  %3d %-18s %d\n", i, func_code_name(i), g_hist[i]);
        return 0;
    }
    int bad = 0;
    for (int i = start; i < argc; i++) bad += dump(argv[i]);
    printf("\n%d de %d arquivos com erro\n", bad, argc - start);
    return bad ? 1 : 0;
}

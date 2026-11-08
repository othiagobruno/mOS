/*
 * mOS libSystem — leitor de .metallib (AIR): contêiner MTLB + módulo de bitcode LLVM. O objetivo é executar os
 * .metallib pré-compilados dos apps no Metal por software (metal.c, mtlexec.c), já que muitos trazem só
 * default.metallib (AIR), sem o fonte .metal.
 *
 * Duas etapas:
 *  1. contêiner MTLB: cabeçalho, lista de funções (nome, estágio, deslocamento do bitcode), blobs de metadados e o
 *     blob de bitcode (um módulo LLVM por biblioteca, com o wrapper 0x0B17C0DE);
 *  2. módulo de bitcode (mtlair_bc.inc + mtlair_mod.inc): tipos, globais, funções, constantes, metadados e os corpos
 *     de função em SSA, traduzidos para a árvore do interpretador (mtlair_lower.inc -> os nós de mtlint.h).
 *
 * O que não dá para traduzir (uma instrução, um intrínseco ou um tipo que o lowering não cobre) faz a função
 * devolver NULL com uma mensagem; o Metal então cai no erro de "AIR não suportado", como antes, mas agora só quando
 * é mesmo o caso. Portátil: compila no host em tools/airdump.c para validar contra .metallib reais.
 */
#include "mtlint.h"

#ifdef MTLAIR_HOST
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;
#else
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int snprintf(char *b, size_t n, const char *fmt, ...);
#endif

#include "mtlair_bc.inc"

/* ================================================================ contêiner MTLB
 * Cabeçalho (little endian):
 *   0x00 "MTLB"  0x04 u16 ver maior  0x06 u16 ver menor  0x08 u64 tipo/plataforma
 *   0x10 u64 tamanho do arquivo  0x18 u64 offset da lista de funções  0x20 u64 tamanho dela
 *   0x28 u64 offset metadados públicos  0x30 u64 tamanho  0x38 priv offset  0x40 priv tamanho
 *   0x48 u64 offset do bitcode  0x50 u64 tamanho do bitcode
 * Lista: u32 nFuncs; por função u32 tamanho-do-grupo e tags de 4 letras com u16 de tamanho:
 *   NAME <nome\0>  TYPE <u8 estágio: 0 vertex, 1 fragment, 2 kernel>  HASH <32>  OFFT <3×u64:
 *   public md, private md, bitcode>  VERS <...>  MDSZ <u64>  ... ENDT. */

typedef struct { char name[96]; int stage; u64 bc_off, pub_off, priv_off; } air_func_t;
typedef struct {
    const u8 *data;
    u64 len;
    air_func_t *funcs;
    int nfuncs;
    const u8 *bc;            /* início do bitstream (depois de "BC C0 DE") */
    u64 bclen;
    int err;
    char errmsg[160];
} air_container_t;

static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 rd64(const u8 *p) { return (u64)rd32(p) | (u64)rd32(p + 4) << 32; }

static int air_fail(air_container_t *c, const char *msg) { c->err = 1; snprintf(c->errmsg, sizeof c->errmsg, "%s", msg); return 0; }

static int air_parse_funclist(air_container_t *c, u64 off, u64 size) {
    if (off + 4 > c->len) return air_fail(c, "lista de funções fora do arquivo");
    const u8 *p = c->data + off;
    const u8 *end = c->data + c->len;   /* o tamanho em 0x20 nao delimita; o fim real eh a secao seguinte */
    (void)size;
    u32 n = rd32(p);
    p += 4;
    if (n > 65536) return air_fail(c, "número de funções absurdo");
    c->funcs = calloc(n ? n : 1, sizeof(air_func_t));
    if (!c->funcs) return air_fail(c, "sem memória");
    for (u32 i = 0; i < n; i++) {
        if (p + 4 > end) return air_fail(c, "entrada de função truncada");
        u32 glen = rd32(p);   /* o tamanho inclui o proprio campo de 4 bytes */
        const u8 *g = p + 4, *gend = p + glen;
        if (glen < 4 || gend > end) return air_fail(c, "grupo de tags fora do arquivo");
        air_func_t *f = &c->funcs[c->nfuncs];
        f->stage = -1;
        while (g + 4 <= gend) {
            u32 tag = rd32(g);
            if (tag == 0x54444e45) break;   /* "ENDT" */
            if (g + 6 > gend) break;
            u16 tl = rd16(g + 4);
            const u8 *v = g + 6;
            if (v + tl > gend) break;
            if (tag == 0x454d414e) {        /* "NAME" */
                u32 cp = tl < sizeof f->name ? tl : (u32)sizeof f->name - 1;
                memcpy(f->name, v, cp);
                f->name[cp && f->name[cp - 1] == 0 ? cp - 1 : cp] = 0;
            } else if (tag == 0x45505954) {  /* "TYPE" */
                if (tl >= 1) f->stage = v[0];
            } else if (tag == 0x5446464f) {  /* "OFFT" */
                if (tl >= 24) { f->pub_off = rd64(v); f->priv_off = rd64(v + 8); f->bc_off = rd64(v + 16); }
            }
            g = v + tl;
        }
        c->nfuncs++;
        p += glen;
    }
    return 1;
}

/* o blob de bitcode: pula o wrapper 0x0B17C0DE e os 4 bytes "BC C0 DE" */
static int air_locate_bitcode(air_container_t *c, u64 off, u64 size) {
    if (off + 4 > c->len) return air_fail(c, "bitcode fora do arquivo");
    const u8 *p = c->data + off;
    u64 avail = c->len - off;
    if (size && size < avail) avail = size;
    if (avail >= 20 && rd32(p) == 0x0B17C0DE) {   /* wrapper: magic, ver, off, size, cputype */
        u64 bcoff = rd32(p + 8), bcsize = rd32(p + 12);
        if (bcoff + bcsize > avail) return air_fail(c, "wrapper de bitcode inconsistente");
        p += bcoff;
        avail -= bcoff;
        if (bcsize) avail = bcsize;
    }
    if (avail < 4 || !(p[0] == 'B' && p[1] == 'C' && p[2] == 0xC0 && p[3] == 0xDE))
        return air_fail(c, "magia de bitcode LLVM ausente");
    c->bc = p + 4;
    c->bclen = avail - 4;
    return 1;
}

static int air_open(air_container_t *c, const u8 *data, u64 len) {
    memset(c, 0, sizeof *c);
    c->data = data;
    c->len = len;
    if (len < 0x58 || !(data[0] == 'M' && data[1] == 'T' && data[2] == 'L' && data[3] == 'B'))
        return air_fail(c, "não é um .metallib (MTLB)");
    u64 fsize = rd64(data + 0x10);
    if (fsize && fsize > len) return air_fail(c, "tamanho do arquivo no cabeçalho maior que o arquivo");
    u64 fl_off = rd64(data + 0x18), fl_size = rd64(data + 0x20);
    u64 bc_off = rd64(data + 0x48), bc_size = rd64(data + 0x50);
    if (!air_parse_funclist(c, fl_off, fl_size)) return 0;
    if (!air_locate_bitcode(c, bc_off, bc_size)) return 0;
    return 1;
}
static void air_close(air_container_t *c) { free(c->funcs); }

#include "mtlair_mod.inc"

/* ================================================================ interface pública (metal.c) e host */

/* conta as funções de entrada (vertex/fragment/kernel) e preenche nomes/estágios; só o contêiner, sem traduzir */
int mtlair_scan(const void *data, u64 len, void (*cb)(void *ud, const char *name, int stage), void *ud, char *err, int errn) {
    air_container_t c;
    if (!air_open(&c, data, len)) { if (err) snprintf(err, errn, "%s", c.errmsg); air_close(&c); return -1; }
    int n = c.nfuncs;
    for (int i = 0; i < c.nfuncs; i++) if (cb) cb(ud, c.funcs[i].name, c.funcs[i].stage);
    air_close(&c);
    return n;
}

/* parse completo do módulo: devolve a lista de módulo (tipos, funções, metadados) ou 0 com erro */
air_module_t *mtlair_parse(const void *data, u64 len, char *err, int errn) {
    air_container_t c;
    if (!air_open(&c, data, len)) { if (err) snprintf(err, errn, "%s", c.errmsg); air_close(&c); return 0; }
    air_module_t *m = airmod_parse(c.bc, c.bclen, err, errn);
    if (m) {
        m->nfuncs_container = c.nfuncs;
        for (int i = 0; i < c.nfuncs && i < AIR_MAXFUNC; i++) {
            snprintf(m->func_names[i], sizeof m->func_names[i], "%s", c.funcs[i].name);
            m->func_stage[i] = c.funcs[i].stage;
            /* casa o estágio do contêiner com a função do módulo pelo nome */
            for (int j = 0; j < m->nfns; j++)
                if (m->fns[j].name && !strcmp(m->fns[j].name, c.funcs[i].name)) m->fns[j].stage = c.funcs[i].stage;
        }
    }
    air_close(&c);
    return m;
}
void mtlair_free(air_module_t *m) { airmod_free(m); }

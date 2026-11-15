/*
 * mOS — fuzzing diferencial interpretador × JIT do tradutor arm64 no Linux, sem oráculo.
 * Gera lotes no formato de tools/a64oracle/fz.h, roda o tools/a64host nos dois motores e compara
 * fz_result byte a byte (tipo de saída, pc, palavra, info, hash do rascunho e o estado inteiro).
 * O que diverge sai em stdout como caso reproduzível (grupo, semente, palavras, primeira diferença).
 *
 *   cc -O2 -o build/a64fuzz_host tools/a64fuzz_host.c
 *   build/a64fuzz_host build/a64host [--casos N] [--k K] [--semente S] [--caso I] grupo...
 * Grupo = nome da tabela abaixo (as máscaras do tools/a64fuzz.c, sem sys.sysreg/sys.outros, que leem
 * contadores, nem br.exception/naoalocado) ou "todos".
 * Não substitui o a64fuzz do Windows: não há QEMU, então "os dois motores erram igual" não aparece aqui.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "a64oracle/fz.h"

typedef struct { const char *nome; uint32_t mask, val; } grupo_t;
static const grupo_t grupos[] = {
    { "dp.pcrel", 0x1F000000, 0x10000000 },
    { "dp.addsub_imm", 0x1F800000, 0x11000000 },
    { "dp.addsub_tags", 0x1F800000, 0x11800000 },
    { "dp.logic_imm", 0x1F800000, 0x12000000 },
    { "dp.movewide", 0x1F800000, 0x12800000 },
    { "dp.bitfield", 0x1F800000, 0x13000000 },
    { "dp.extract", 0x1F800000, 0x13800000 },
    { "br.cond", 0xFE000000, 0x54000000 },
    { "sys.hint", 0xFFFFF01F, 0xD503201F },
    { "sys.barrier", 0xFFFFF01F, 0xD503301F },
    { "sys.pstate", 0xFFF8F01F, 0xD500401F },
    { "sys.sysinstr", 0xFFD80000, 0xD5080000 },
    { "br.reg", 0xFE000000, 0xD6000000 },
    { "br.imm", 0x7C000000, 0x14000000 },
    { "br.cmp", 0x7E000000, 0x34000000 },
    { "br.test", 0x7E000000, 0x36000000 },
    { "simd.ls_mult", 0xBFBF0000, 0x0C000000 },
    { "simd.ls_mult_post", 0xBFA00000, 0x0C800000 },
    { "simd.ls_single", 0xBF9F0000, 0x0D000000 },
    { "simd.ls_single_post", 0xBF800000, 0x0D800000 },
    { "ls.excl", 0x3F000000, 0x08000000 },
    { "ls.rcpc_unscaled", 0x3F200C00, 0x19000000 },
    { "ls.literal", 0x3F000000, 0x18000000 },
    { "simd.ls_literal", 0x3F000000, 0x1C000000 },
    { "ls.pair", 0x3E000000, 0x28000000 },
    { "simd.ls_pair", 0x3E000000, 0x2C000000 },
    { "ls.imm9", 0x3F200000, 0x38000000 },
    { "simd.ls_imm9", 0x3F200000, 0x3C000000 },
    { "ls.atomic", 0x3F200C00, 0x38200000 },
    { "ls.regoff", 0x3F200C00, 0x38200800 },
    { "simd.ls_regoff", 0x3F200C00, 0x3C200800 },
    { "ls.pac", 0x3F200400, 0x38200400 },
    { "ls.uimm", 0x3F000000, 0x39000000 },
    { "simd.ls_uimm", 0x3F000000, 0x3D000000 },
    { "ls.outros", 0x0A000000, 0x08000000 },
    { "dp.2src", 0x5FE00000, 0x1AC00000 },
    { "dp.1src", 0x5FE00000, 0x5AC00000 },
    { "dp.logic_reg", 0x1F000000, 0x0A000000 },
    { "dp.addsub_shift", 0x1F200000, 0x0B000000 },
    { "dp.addsub_ext", 0x1F200000, 0x0B200000 },
    { "dp.adc", 0x1FE0FC00, 0x1A000000 },
    { "dp.rmif", 0x1FE07C00, 0x1A000400 },
    { "dp.setf", 0x1FE03C00, 0x1A000800 },
    { "dp.ccmp_reg", 0x1FE00800, 0x1A400000 },
    { "dp.ccmp_imm", 0x1FE00800, 0x1A400800 },
    { "dp.csel", 0x1FE00000, 0x1A800000 },
    { "dp.3src", 0x1F000000, 0x1B000000 },
    { "dp.outros", 0x0E000000, 0x0A000000 },
    { "fp.fixed", 0x5F200000, 0x1E000000 },
    { "fp.int", 0x5F20FC00, 0x1E200000 },
    { "fp.1src", 0x5F207C00, 0x1E204000 },
    { "fp.cmp", 0x5F203C00, 0x1E202000 },
    { "fp.imm", 0x5F201C00, 0x1E201000 },
    { "fp.ccmp", 0x5F200C00, 0x1E200400 },
    { "fp.2src", 0x5F200C00, 0x1E200800 },
    { "fp.csel", 0x5F200C00, 0x1E200C00 },
    { "fp.3src", 0x5F000000, 0x1F000000 },
    { "crypto.aes", 0xFF3E0C00, 0x4E280800 },
    { "crypto.sha3reg", 0xFF208C00, 0x5E000000 },
    { "crypto.sha2reg", 0xFF3E0C00, 0x5E280800 },
    { "crypto.sha512_3", 0xFFE0B000, 0xCE608000 },
    { "crypto.4reg", 0xFF808000, 0xCE000000 },
    { "crypto.xar", 0xFFE00000, 0xCE800000 },
    { "crypto.sha512_2", 0xFFFFF000, 0xCEC08000 },
    { "simds.copy", 0xDFE08400, 0x5E000400 },
    { "simds.3same_fp16", 0xDF60C400, 0x5E400400 },
    { "simds.2misc_fp16", 0xDF7E0C00, 0x5E780800 },
    { "simds.3same_extra", 0xDF208400, 0x5E008400 },
    { "simds.2misc", 0xDF3E0C00, 0x5E200800 },
    { "simds.pairwise", 0xDF3E0C00, 0x5E300800 },
    { "simds.3diff", 0xDF200C00, 0x5E200000 },
    { "simds.3same", 0xDF200400, 0x5E200400 },
    { "simds.shift", 0xDF800400, 0x5F000400 },
    { "simds.indexed", 0xDF000400, 0x5F000000 },
    { "simdv.tbl", 0xBF208C00, 0x0E000000 },
    { "simdv.perm", 0xBF208C00, 0x0E000800 },
    { "simdv.ext", 0xBF208400, 0x2E000000 },
    { "simdv.copy", 0x9FE08400, 0x0E000400 },
    { "simdv.3same_fp16", 0x9F60C400, 0x0E400400 },
    { "simdv.2misc_fp16", 0x9F7E0C00, 0x0E780800 },
    { "simdv.3same_extra", 0x9F208400, 0x0E008400 },
    { "simdv.2misc", 0x9F3E0C00, 0x0E200800 },
    { "simdv.across", 0x9F3E0C00, 0x0E300800 },
    { "simdv.3diff", 0x9F200C00, 0x0E200000 },
    { "simdv.3same", 0x9F200400, 0x0E200400 },
    { "simdv.modimm", 0x9FF80400, 0x0F000400 },
    { "simdv.shift", 0x9F800400, 0x0F000400 },
    { "simdv.indexed", 0x9F000400, 0x0F000000 },
    { "simd.outros", 0x0E000000, 0x0E000000 },
};
#define NGRUPOS (sizeof grupos / sizeof grupos[0])

static uint64_t rs;
static uint64_t rnd(void) { return fz_rng(&rs); }

static uint64_t especial(void) {
    static const uint64_t v[] = { 0, 1, 2, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000,
        0xFFFFFFFF, 0x100000000ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, ~0ull, 63, 64, 31, 32 };
    return v[rnd() % (sizeof v / sizeof v[0])];
}

static void estado(fz_state *s) {
    memset(s, 0, sizeof *s);
    for (int i = 0; i < 31; i++) {
        unsigned k = rnd() % 4;
        if (k == 0) s->x[i] = especial();
        else if (k == 1) s->x[i] = FZ_SCR_VA + 256 + (rnd() % (FZ_SCR_SZ - 512));   /* ponteiro no rascunho */
        else if (k == 2) s->x[i] = rnd() % 4096;                                       /* deslocamentos pequenos */
        else s->x[i] = rnd();
    }
    s->sp = (FZ_SCR_VA + 1024 + (rnd() % (FZ_SCR_SZ - 2048))) & ~15ull;
    s->nzcv = (uint32_t)(rnd() & 0xF) << 28;
    s->fpcr = 0;                                     /* RN: o caminho nativo do JIT */
    if (rnd() % 4 == 0) s->fpcr = (uint32_t)((rnd() % 4) << 22 | (rnd() % 2) << 24 | (rnd() % 2) << 25);
    s->tpidr = rnd();
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < 16; j += 8) { uint64_t v = rnd() % 3 ? rnd() : especial(); memcpy(&s->v[i][j], &v, 8); }
}

/* Puxa a amostra para o que executa: nos 1src/2src, opcodes alocados (S = 0); nos loads/stores, a base
 * (Rn, bits 9:5) aponta para o rascunho. Um quarto dos casos fica com a palavra crua (cobre o UDF). */
static uint32_t vicia(const grupo_t *g, uint32_t w, fz_state *st) {
    if (rnd() % 4 == 0) return w;
    if (!strcmp(g->nome, "dp.1src")) {
        w &= ~((1u << 29) | (0x1Fu << 16) | (0x3Fu << 10));
        if (rnd() % 2) w |= (1u << 16) | (uint32_t)(rnd() % 18) << 10;          /* PAC/AUT/XPAC */
        else w |= (uint32_t)(rnd() % 8) << 10;                                   /* RBIT REV16 REV32 REV CLZ CLS CTZ CNT */
    } else if (!strcmp(g->nome, "dp.2src")) {
        static const uint8_t op[] = { 2, 3, 8, 9, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };
        w &= ~((1u << 29) | (0x3Fu << 10));
        w |= (uint32_t)op[rnd() % sizeof op] << 10;
    }
    if (((w >> 25) & 5) == 4) {                                                  /* loads e stores (op0 = x1x0) */
        unsigned rn = (w >> 5) & 31;
        uint64_t p = FZ_SCR_VA + 512 + (rnd() % (FZ_SCR_SZ - 1024));
        if (rnd() % 2) p &= ~15ull;
        if (rn == 31) st->sp = p & ~15ull; else st->x[rn] = p;
    }
    return w;
}

static int roda(const char *host, int jit, const char *in, const char *out) {
    pid_t p = fork();
    if (p == 0) {
        if (jit) execl(host, host, "--jit", "--fuzz", in, out, (char *)0);
        else execl(host, host, "--fuzz", in, out, (char *)0);
        _exit(127);
    }
    int st = 0;
    waitpid(p, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

static const char *campo(const fz_result *a, const fz_result *b) {
    static char buf[64];
    if (a->kind != b->kind) return "tipo de saída";
    if (a->pc != b->pc) return "pc";
    if (a->info != b->info) return "info";
    if (a->scr_hash != b->scr_hash) return "rascunho";
    for (int i = 0; i < 31; i++) if (a->st.x[i] != b->st.x[i]) { snprintf(buf, sizeof buf, "x%d", i); return buf; }
    if (a->st.sp != b->st.sp) return "sp";
    if (a->st.nzcv != b->st.nzcv) return "nzcv";
    if (a->st.fpsr != b->st.fpsr) return "fpsr";
    if (a->st.fpcr != b->st.fpcr) return "fpcr";
    for (int i = 0; i < 32; i++) if (memcmp(a->st.v[i], b->st.v[i], 16)) { snprintf(buf, sizeof buf, "v%d", i); return buf; }
    return "outro";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: a64fuzz_host <a64host> [--casos N] [--k K] [--semente S] grupo...|todos\n"); return 2; }
    const char *host = argv[1];
    unsigned casos = 2000, K = 1;
    uint64_t semente = 20261115;
    long so = -1;                                    /* --caso N: gera a série inteira e roda só o caso N */
    const grupo_t *sel[NGRUPOS]; unsigned nsel = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--casos") && i + 1 < argc) casos = (unsigned)strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--k") && i + 1 < argc) K = (unsigned)strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--semente") && i + 1 < argc) semente = strtoull(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--caso") && i + 1 < argc) so = strtol(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "todos")) { nsel = 0; for (unsigned g = 0; g < NGRUPOS; g++) sel[nsel++] = &grupos[g]; }
        else {
            unsigned g;
            for (g = 0; g < NGRUPOS; g++) if (!strcmp(argv[i], grupos[g].nome)) break;
            if (g == NGRUPOS) { fprintf(stderr, "grupo desconhecido: %s\n", argv[i]); return 2; }
            if (nsel < NGRUPOS) sel[nsel++] = &grupos[g];
        }
    }
    if (!nsel || K < 1 || K > FZ_MAXINSN - 1) { fprintf(stderr, "a64fuzz_host: sem grupo ou K inválido\n"); return 2; }
    char dir[] = "/tmp/a64fuzz_host.XXXXXX";
    if (!mkdtemp(dir)) return 2;
    char fin[96], fi[96], fj[96];
    snprintf(fin, sizeof fin, "%s/lote.bin", dir);
    snprintf(fi, sizeof fi, "%s/interp.bin", dir);
    snprintf(fj, sizeof fj, "%s/jit.bin", dir);
    fz_case *cs = calloc(casos, sizeof *cs);
    fz_result *ri = calloc(casos, sizeof *ri), *rj = calloc(casos, sizeof *rj);
    if (!cs || !ri || !rj) return 2;
    unsigned total_div = 0;
    for (unsigned g = 0; g < nsel; g++) {
        rs = semente ^ (0x9E3779B97F4A7C15ull * (g + 1)) ^ (uint64_t)K << 56;
        if (!rs) rs = 1;
        for (unsigned c = 0; c < casos; c++) {
            estado(&cs[c].st);
            cs[c].scr_seed = rnd();
            cs[c].ninsn = K;
            for (unsigned k = 0; k < K; k++) {
                const grupo_t *gr = (k == 0) ? sel[g] : sel[rnd() % nsel];
                cs[c].insn[k] = vicia(gr, gr->val | ((uint32_t)rnd() & ~gr->mask), &cs[c].st);
            }
        }
        unsigned n = casos;
        if (so >= 0 && (unsigned long)so < casos) {
            cs[0] = cs[so];
            n = 1;
            printf("caso %ld:", so);
            for (unsigned k = 0; k < K; k++) printf(" %08x", cs[0].insn[k]);
            printf("\n");
            fflush(stdout);
        }
        fz_batch_hdr h = { .magic = FZ_BATCH_MAGIC, .ncases = n, .case_size = sizeof(fz_case),
                           .res_size = sizeof(fz_result), .budget = 10000 };
        FILE *f = fopen(fin, "wb");
        if (!f || fwrite(&h, sizeof h, 1, f) != 1 || fwrite(cs, sizeof *cs, n, f) != n) return 2;
        fclose(f);
        const char *salva = getenv("A64FUZZ_LOTE");        /* cópia do lote para reproduzir no a64host/gdb */
        if (salva) {
            FILE *o = fopen(salva, "wb");
            if (o) { fwrite(&h, sizeof h, 1, o); fwrite(cs, sizeof *cs, n, o); fclose(o); }
        }
        int ei = roda(host, 0, fin, fi), ej = roda(host, 1, fin, fj);
        FILE *a = fopen(fi, "rb"), *b = fopen(fj, "rb");
        if (ei || ej || !a || !b || fread(ri, sizeof *ri, n, a) != n || fread(rj, sizeof *rj, n, b) != n) {
            printf("ERRO %s: a64host saiu %d (interp) / %d (jit)\n", sel[g]->nome, ei, ej);
            return 1;
        }
        fclose(a); fclose(b);
        unsigned div = 0, kinds[8] = {0};
        for (unsigned c = 0; c < n; c++) {
            kinds[ri[c].kind & 7]++;
            if (!memcmp(&ri[c], &rj[c], sizeof ri[c])) continue;
            if (div++ < 5) {
                printf("  DIV %s caso %u:", sel[g]->nome, c);
                for (unsigned k = 0; k < K; k++) printf(" %08x", cs[c].insn[k]);
                printf("  campo=%s interp(kind=%u pc=%llx) jit(kind=%u pc=%llx)\n", campo(&ri[c], &rj[c]),
                       ri[c].kind, (unsigned long long)ri[c].pc, rj[c].kind, (unsigned long long)rj[c].pc);
            }
        }
        total_div += div;
        printf("%-18s K=%u casos=%u fim=%u udf=%u fault=%u outros=%u  JIT≠interp=%u\n", sel[g]->nome, K, n,
               kinds[FZK_END], kinds[FZK_UDF], kinds[FZK_FAULT], n - kinds[FZK_END] - kinds[FZK_UDF] - kinds[FZK_FAULT], div);
    }
    unlink(fin); unlink(fi); unlink(fj); rmdir(dir);
    printf("a64fuzz_host: %u divergência(s)\n", total_div);
    return total_div != 0;
}

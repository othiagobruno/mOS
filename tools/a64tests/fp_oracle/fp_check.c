/* mOS — oráculo FP: verificador (host x86_64). Roda cada vetor no interpretador (a64_exec_fp)
 * e compara com a saída do QEMU (x0..x3, v0..v3, FPSR, NZCV, UDF).
 *   fp_check <in.bin> <out-qemu.bin> [max-falhas-por-modelo]
 * Saída: placar por modelo e as primeiras falhas de cada um. Código de saída 0 = tudo bate. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fpvec.h"
#include "../../../userland/a64/a64.h"

typedef uint64_t u64;
typedef uint32_t u32;

/* serviços do hospedeiro que o motor pode pedir (não usados pelo grupo FP) */
a64_u64 a64_host_cntvct(void) { return 0; }
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; (void)addr; (void)len; }
void a64_host_yield(a64_cpu *c) { (void)c; }

static void *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb");
    if (!f) { perror(p); exit(1); }
    fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    void *b = malloc(*n + 1);
    if (fread(b, 1, *n, f) != *n) { perror(p); exit(1); }
    fclose(f);
    return b;
}

/* codificações que o QEMU -cpu max executa por ter recursos que o mOS NÃO anuncia (a64.h,
 * A64_FEATURES): para nós o correto é UDF. Devolve o nome do recurso ou NULL. */
static const char *recurso_extra(u32 w) {
    /* FEAT_FPRCVT: FCVTxx/SCVTF/UCVTF entre FP e registrador SIMD&FP, no grupo FP<->inteiro com
     * opcode 010..101 e rmode != 00, ou opcode 11x e rmode = 10 (não alocados até 2023) */
    if ((w & 0x5F20FC00u) == 0x1E200000u && !((w >> 29) & 1)) {
        unsigned rmode = (w >> 19) & 3, op = (w >> 16) & 7;
        if (op >= 2 && op <= 5 && rmode) return "FEAT_FPRCVT";
        if (op >= 6 && rmode == 2) return "FEAT_FPRCVT";       /* SCVTF/UCVTF/FCVTxx SIMD&FP */
    }
    return NULL;
}

/* executa a primitiva correspondente à instrução AdvSIMD do vetor (ver fpvec.h) */
static int run_prim(a64_cpu *c, const fpvec_in *in) {
    int es = (int)in->pad[1];
    a64_u64 m = es == 8 ? ~0ull : (1ull << (es * 8)) - 1, sb = 1ull << (es * 8 - 1);
    a64_u64 a = c->v[1].d[0] & m, b = c->v[2].d[0] & m, r;
    switch (in->pad[0]) {
    case P_MULX: r = a64_fp_mulx(c, a, b, es); break;
    case P_RECPS: r = a64_fp_recps(c, a, b, es); break;
    case P_RSQRTS: r = a64_fp_rsqrts(c, a, b, es); break;
    case P_ABD: r = a64_fp_abd(c, a, b, es); break;
    case P_CMEQ: r = a64_fp_cmp_eq(c, a, b, es) ? m : 0; break;
    case P_CMGE: r = a64_fp_cmp_ge(c, a, b, es) ? m : 0; break;
    case P_CMGT: r = a64_fp_cmp_gt(c, a, b, es) ? m : 0; break;
    case P_ACGE: r = a64_fp_cmp_ge(c, a & ~sb, b & ~sb, es) ? m : 0; break;
    case P_ACGT: r = a64_fp_cmp_gt(c, a & ~sb, b & ~sb, es) ? m : 0; break;
    case P_RECPE: r = a64_fp_recpe(c, a, es); break;
    case P_RSQRTE: r = a64_fp_rsqrte(c, a, es); break;
    case P_RECPX: r = a64_fp_recpx(c, a, es); break;
    case P_FCVTXN: r = a64_fp_convert(c, a, 8, 4, A64_RM_ODD); break;
    case P_FCVTZS16: r = a64_fp_to_int(c, a, 2, 0, 16, 0, A64_RM_ZERO); break;
    case P_FCVTZU16: r = a64_fp_to_int(c, a, 2, 0, 16, 1, A64_RM_ZERO); break;
    case P_FCVTNS16: r = a64_fp_to_int(c, a, 2, 0, 16, 0, A64_RM_TIEEVEN); break;
    case P_FCVTAU16: r = a64_fp_to_int(c, a, 2, 0, 16, 1, A64_RM_TIEAWAY); break;
    case P_SCVTF16: r = a64_int_to_fp(c, a, 16, 0, 0, 2, A64_RM_FPCR); break;
    case P_UCVTF16: r = a64_int_to_fp(c, a, 16, 1, 0, 2, A64_RM_FPCR); break;
    case P_FCVTZS_SD: r = a64_fp_to_int(c, a, es, 0, es * 8, 0, A64_RM_ZERO); break;
    case P_SCVTF_SD: r = a64_int_to_fp(c, a, es * 8, 0, 0, es, A64_RM_FPCR); break;
    case P_URECPE: case P_URSQRTE: {
        a64_u32 l0 = c->v[1].s[0], l1 = c->v[1].s[1];
        int u = in->pad[0] == P_URECPE;
        c->v[0].q = 0;
        c->v[0].s[0] = u ? a64_recpe_u32(l0) : a64_rsqrte_u32(l0);
        c->v[0].s[1] = u ? a64_recpe_u32(l1) : a64_rsqrte_u32(l1);
        return A64_NEXT;
    }
    default: return A64_EXIT_UDF;
    }
    c->v[0].q = 0;
    c->v[0].d[0] = r;
    return A64_NEXT;
}

#define MAXT 512
static char names[MAXT][48];
static u64 tot[MAXT], bad[MAXT], live[MAXT], extra[MAXT];
static u32 flg[MAXT];
static const char *flagstr(u32 f) {
    static char b[8];
    const char *l = "IZOUX..D";
    for (int i = 0; i < 8; i++) b[i] = (f >> i) & 1 ? l[i] : '-';
    b[5] = b[6] = 0;
    if (f & A64_FPSR_IDC) b[5] = 'D';
    else b[5] = '-';
    return b;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: fp_check in.bin out.bin [max]\n"); return 2; }
    int maxshow = argc > 3 ? atoi(argv[3]) : 3;
    size_t ni, no;
    unsigned char *bi = slurp(argv[1], &ni), *bo = slurp(argv[2], &no);
    fpvec_hdr *h = (fpvec_hdr *)bi;
    if (h->magic != FPVEC_MAGIC) { fprintf(stderr, "entrada inválida\n"); return 2; }
    u64 n = h->count;
    if (no != n * sizeof(fpvec_out)) { fprintf(stderr, "saída com tamanho %zu, esperado %llu\n", no, (unsigned long long)(n * sizeof(fpvec_out))); return 2; }
    char nb[600];
    snprintf(nb, sizeof nb, "%s.names", argv[1]);
    FILE *fn = fopen(nb, "r");
    int nt = 0;
    if (fn) { while (nt < MAXT && fgets(names[nt], 48, fn)) { names[nt][strcspn(names[nt], "\r\n")] = 0; nt++; } fclose(fn); }
    fpvec_in *vi = (fpvec_in *)(bi + sizeof *h);
    fpvec_out *vo = (fpvec_out *)bo;
    static a64_cpu c;
    u64 nbad = 0;
    for (u64 k = 0; k < n; k++) {
        fpvec_in *in = &vi[k];
        fpvec_out *q = &vo[k];
        memset(&c, 0, sizeof c);
        for (int i = 0; i < 4; i++) { c.x[i] = in->x[i]; c.v[i].d[0] = in->v[i][0]; c.v[i].d[1] = in->v[i][1]; }
        c.fpcr = in->fpcr; c.fpsr = 0; c.nzcv = in->nzcv;
        int r = in->pad[0] ? run_prim(&c, in) : a64_exec_fp(&c, in->insn);
        u32 t = in->tmpl < MAXT ? in->tmpl : MAXT - 1;
        tot[t]++;
        if (!q->exc) { live[t]++; flg[t] |= q->fpsr; }
        int ok;
        if (r == A64_EXIT_UDF && !q->exc && recurso_extra(in->insn)) { extra[t]++; continue; }
        if (r == A64_EXIT_UDF || q->exc) ok = (r == A64_EXIT_UDF) && q->exc;
        else {
            ok = r == A64_NEXT && c.fpsr == q->fpsr && c.nzcv == q->nzcv;
            for (int i = 0; i < 4; i++)
                ok = ok && c.x[i] == q->x[i] && c.v[i].d[0] == q->v[i][0] && c.v[i].d[1] == q->v[i][1];
        }
        if (ok) continue;
        nbad++;
        if (bad[t]++ < (u64)maxshow) {
            printf("FALHA %s #%llu insn=%08x fpcr=%08x nzcv_in=%x\n", t < (u32)nt ? names[t] : "?", (unsigned long long)k, in->insn, in->fpcr, in->nzcv >> 28);
            printf("  in : x1=%016llx v1=%016llx v2=%016llx v3=%016llx\n", (unsigned long long)in->x[1],
                   (unsigned long long)in->v[1][0], (unsigned long long)in->v[2][0], (unsigned long long)in->v[3][0]);
            printf("  qemu: exc=%u x0=%016llx v0=%016llx:%016llx fpsr=%08x nzcv=%x\n", q->exc, (unsigned long long)q->x[0],
                   (unsigned long long)q->v[0][1], (unsigned long long)q->v[0][0], q->fpsr, q->nzcv >> 28);
            printf("  nós : ret=%d x0=%016llx v0=%016llx:%016llx fpsr=%08x nzcv=%x\n", r, (unsigned long long)c.x[0],
                   (unsigned long long)c.v[0].d[1], (unsigned long long)c.v[0].d[0], c.fpsr, c.nzcv >> 28);
            for (int i = 1; i < 4; i++)
                if (c.x[i] != q->x[i] || c.v[i].d[0] != q->v[i][0] || c.v[i].d[1] != q->v[i][1])
                    printf("  reg %d difere: qemu x=%016llx v=%016llx:%016llx nós x=%016llx v=%016llx:%016llx\n", i,
                           (unsigned long long)q->x[i], (unsigned long long)q->v[i][1], (unsigned long long)q->v[i][0],
                           (unsigned long long)c.x[i], (unsigned long long)c.v[i].d[1], (unsigned long long)c.v[i].d[0]);
        }
    }
    printf("\n%-24s %10s %10s %10s %10s  %s\n", "modelo", "vetores", "executou", "extra", "falhas", "FPSR(IZOUXD)");
    for (int t = 0; t < MAXT; t++)
        if (tot[t]) printf("%-24s %10llu %10llu %10llu %10llu  %s%s\n", t < nt ? names[t] : "?", (unsigned long long)tot[t],
                           (unsigned long long)live[t], (unsigned long long)extra[t], (unsigned long long)bad[t], flagstr(flg[t]), bad[t] ? "  <<<" : "");
    printf("(extra = o QEMU executa por ter recurso não anunciado no mOS, ex. FEAT_FPRCVT; nós dar UDF é o correto)\n");
    printf("\nTOTAL %llu vetores, %llu falhas\n", (unsigned long long)n, (unsigned long long)nbad);
    return nbad ? 1 : 0;
}

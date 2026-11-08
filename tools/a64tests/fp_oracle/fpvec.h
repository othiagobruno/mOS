/* mOS — oráculo FP do tradutor arm64: formato dos vetores (gerador, runner do QEMU e verificador).
 *
 * Arquivo de entrada: cabeçalho de 128 bytes + count registros fpvec_in. O runner (runner.S,
 * bare-metal no qemu-system-aarch64 -M virt -cpu max) executa cada registro e grava count
 * registros fpvec_out no arquivo nomeado no cabeçalho (semihosting).
 * Cada registro de entrada começa com o próprio código: { insn, ret } — o runner faz blr nele,
 * então não há código automodificável. Registradores: x0..x3 e v0..v3 entram e saem; v4..v31
 * entram zerados. Uma exceção síncrona (instrução indefinida) marca exc = 1 e pula a instrução.
 */
#pragma once
#include <stdint.h>

#define FPVEC_MAGIC 0x31434556504Full   /* "OPVEC1" */

typedef struct fpvec_hdr {
    uint64_t count;
    uint64_t magic;
    char outname[112];
} fpvec_hdr;

typedef struct fpvec_in {
    uint32_t insn, ret;          /* ret = 0xd65f03c0 */
    uint32_t fpcr, nzcv;
    uint64_t x[4];
    uint64_t v[4][2];            /* [lo, hi] */
    uint32_t tmpl, pad[3];       /* tmpl: índice do modelo no gerador (relatório) */
} fpvec_in;

typedef struct fpvec_out {
    uint64_t x[4];
    uint64_t v[4][2];
    uint32_t fpsr, nzcv, exc, pad;
    uint32_t pad2[4];
} fpvec_out;

_Static_assert(sizeof(fpvec_hdr) == 128, "hdr");
_Static_assert(sizeof(fpvec_in) == 128, "in");
_Static_assert(sizeof(fpvec_out) == 128, "out");

/* fpvec_in.pad[0]: 0 = instrução do grupo FP escalar (o verificador roda a64_exec_fp);
 * senão é a primitiva a64_fp_* que o verificador chama direto para conferir a instrução do
 * AdvSIMD (forma escalar, ou vetorial 2S para URECPE/URSQRTE) executada no QEMU. */
enum {
    P_NONE, P_MULX, P_RECPS, P_RSQRTS, P_ABD, P_CMEQ, P_CMGE, P_CMGT, P_ACGE, P_ACGT,
    P_RECPE, P_RSQRTE, P_RECPX, P_FCVTXN, P_FCVTZS16, P_FCVTZU16, P_FCVTNS16, P_FCVTAU16,
    P_SCVTF16, P_UCVTF16, P_URECPE, P_URSQRTE, P_FCVTZS_SD, P_SCVTF_SD, P_NPRIM
};

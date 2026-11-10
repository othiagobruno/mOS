/*
 * mOS — fuzzing diferencial do tradutor arm64: interpretador × JIT × oráculo QEMU.
 *
 *   a64fuzz [--grupo G]... [--casos N] [--k K] [--semente S] [--jit] [--lote N]
 *           [--salvar] [--max-salvos M] [--relatorio arq.tsv] [--exemplos arq.txt] [--listar]
 *           [--so-gerar lote.bin]   (só grava o lote, para rodar à mão: a64test [--jit] --fuzz lote.bin res.bin)
 *
 * Para cada grupo de codificação (tabela GROUPS: máscara/valor tirados da árvore de decodificação
 * do ARM ARM, C4.1), gera palavras aleatórias dentro do grupo, um estado inicial aleatório
 * (valores inteiros e FP especiais, ponteiros para o rascunho) e roda o MESMO lote de casos em
 *   - build\a64test.exe --fuzz  (interpretador; com --jit também o JIT)
 *   - qemu-system-aarch64 + build\a64oracle\mon.elf (EL0 de verdade, muitos casos por boot)
 * Um caso = K instruções na página de código fixa (fz.h), seguidas de svc #0x4001. Compara o
 * estado final completo (x, sp, nzcv, fpcr, fpsr, DIT/SSBS, tpidr_el0, v0..v31, hash do rascunho,
 * tipo de saída e pc).
 *
 * O que "deveria executar": o -cpu max do QEMU tem recursos que NÃO anunciamos (SVE, SME, MTE,
 * MOPS, AFP, XS, SPECRES...). A validade de cada palavra vem do desmontador do LLVM
 * (llvm-objdump) em duas passadas: só com os recursos de A64_FEATURES e com +all.
 *   - desconhecida para nós mas conhecida com +all, "udf", ou forma genérica de sysreg/sys que
 *     com +all vira instrução com nome → tem que ser UDF (independente do QEMU);
 *   - senão quem decide é o QEMU: se ele dá UDF, esperamos UDF; se executa, esperamos o mesmo
 *     estado final.
 * Veredito por caso: bate | diverge | udf_indevido (QEMU executa, nós UDF) | deveria_udf |
 * udf_ok. Casos que divergem com K>1 são minimizados (instruções viram NOP enquanto a
 * divergência persistir) e, com --salvar, viram tools\a64tests\regress_<grupo>_<insn>.S
 * (reproduzíveis com tools\a64cmp.ps1).
 *
 * Instruções sem resultado determinístico no oráculo ficam de fora do gerador e são cobertas por
 * testes próprios: MRS CNTVCT/CNTPCT(SS)/CNTFRQ (tempo e frequência do QEMU), MRS DCZID_EL0
 * (o QEMU anuncia 512 B e DZP=1 por causa da emulação do DC ZVA), PACGA (algoritmo de PAC).
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "a64oracle/fz.h"

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;

/* ------------------------------------------------------------------------------------------ */
/* grupos de codificação                                                                       */

typedef struct { u32 mask, val; } tmpl;
typedef struct group {
    const char *name, *front;
    u32 mask, val;             /* classificação (primeira que casa, na ordem da tabela) */
    const tmpl *extra;         /* moldes adicionais para o gerador (metade dos casos) */
} group;

static const tmpl T_SYSREG[] = {
    { 0xFFFFFFE0, 0xD53B4200 }, { 0xFFFFFFE0, 0xD51B4200 },   /* MRS/MSR NZCV */
    { 0xFFFFFFE0, 0xD53B4400 }, { 0xFFFFFFE0, 0xD51B4400 },   /* FPCR */
    { 0xFFFFFFE0, 0xD53B4420 }, { 0xFFFFFFE0, 0xD51B4420 },   /* FPSR */
    { 0xFFFFFFE0, 0xD53B42A0 }, { 0xFFFFFFE0, 0xD51B42A0 },   /* DIT */
    { 0xFFFFFFE0, 0xD53B42C0 }, { 0xFFFFFFE0, 0xD51B42C0 },   /* SSBS */
    { 0xFFFFFFE0, 0xD53B42E0 }, { 0xFFFFFFE0, 0xD51B42E0 },   /* TCO (MTE: UDF) */
    { 0xFFFFFFE0, 0xD53BD040 }, { 0xFFFFFFE0, 0xD51BD040 },   /* TPIDR_EL0 */
    { 0xFFFFFFE0, 0xD53BD060 }, { 0xFFFFFFE0, 0xD51BD060 },   /* TPIDRRO_EL0 (MSR: UDF) */
    { 0xFFFFFFE0, 0xD53B0020 },                               /* CTR_EL0 */
    { 0xFFFFFFE0, 0xD5384200 }, { 0xFFFFFFE0, 0xD5380000 },   /* MRS de EL1 (UDF) */
    { 0, 0 } };
static const tmpl T_PSTATE[] = {
    { 0xFFFFF0FF, 0xD503405F }, { 0xFFFFF0FF, 0xD503403F }, { 0xFFFFF0FF, 0xD503409F },  /* DIT SSBS TCO */
    { 0xFFFFFFFF, 0xD500401F }, { 0xFFFFFFFF, 0xD500403F }, { 0xFFFFFFFF, 0xD500405F },  /* CFINV XAFLAG AXFLAG */
    { 0, 0 } };
static const tmpl T_SYSINSTR[] = {
    { 0xFFFFFFE0, 0xD50B7420 }, { 0xFFFFFFE0, 0xD50B7B20 }, { 0xFFFFFFE0, 0xD50B7E20 },  /* DC ZVA CVAU CIVAC */
    { 0xFFFFFFE0, 0xD50B7A20 }, { 0xFFFFFFE0, 0xD50B7C20 }, { 0xFFFFFFE0, 0xD50B7D20 },  /* DC CVAC CVAP CVADP */
    { 0xFFFFFFE0, 0xD50B7520 },                                                       /* IC IVAU */
    { 0, 0 } };
static const tmpl T_EXC[] = {
    { 0xFFE0001F, 0xD4000001 }, { 0xFFE0001F, 0xD4200000 }, { 0xFFE0001F, 0xD4400000 },  /* SVC BRK HLT */
    { 0, 0 } };
static const tmpl T_BRREG[] = {
    { 0xFFFFFC1F, 0xD61F0000 }, { 0xFFFFFC1F, 0xD63F0000 }, { 0xFFFFFC1F, 0xD65F0000 },  /* BR BLR RET */
    { 0xFFFFFFFF, 0xD65F0BFF }, { 0xFFFFFFFF, 0xD65F0FFF },                           /* RETAA RETAB */
    { 0xFFFFF800, 0xD71F0800 }, { 0xFFFFF800, 0xD73F0800 },                           /* BRAA/BLRAA */
    { 0xFFFFF81F, 0xD61F081F }, { 0xFFFFF81F, 0xD63F081F },                           /* BRAAZ BLRAAZ */
    { 0, 0 } };
static const tmpl T_HINT[] = {
    { 0xFFFFFFFF, 0xD50320FF }, { 0xFFFFFFFF, 0xD503233F }, { 0xFFFFFFFF, 0xD50323BF },  /* XPACLRI PACIASP AUTIASP */
    { 0xFFFFFFFF, 0xD503211F }, { 0xFFFFFFFF, 0xD503219F },                           /* PACIA1716 AUTIA1716 */
    { 0, 0 } };
static const tmpl T_1SRC[] = {
    { 0xFFFFE000, 0xDAC10000 }, { 0xFFFFE000, 0xDAC14000 }, { 0xFFFFFFE0, 0xDAC143E0 },  /* PAC* AUT* XPACI XPACD */
    { 0xFFFFF000, 0xDAC00000 }, { 0xFFFFF000, 0x5AC00000 }, { 0xFFFFF000, 0xDAC01000 },  /* RBIT REV CLZ CLS */
    { 0, 0 } };

/* ordem = prioridade na classificação (mais específico primeiro) */
static const group GROUPS[] = {
    /* ---- dados com imediato ---- */
    { "dp.pcrel",        "INT", 0x1F000000, 0x10000000, 0 },
    { "dp.addsub_imm",   "INT", 0x1F800000, 0x11000000, 0 },
    { "dp.addsub_tags",  "INT", 0x1F800000, 0x11800000, 0 },
    { "dp.logic_imm",    "INT", 0x1F800000, 0x12000000, 0 },
    { "dp.movewide",     "INT", 0x1F800000, 0x12800000, 0 },
    { "dp.bitfield",     "INT", 0x1F800000, 0x13000000, 0 },
    { "dp.extract",      "INT", 0x1F800000, 0x13800000, 0 },
    /* ---- desvios, exceções, sistema ---- */
    { "br.cond",         "INT", 0xFE000000, 0x54000000, 0 },
    { "br.exception",    "INT", 0xFF000000, 0xD4000000, T_EXC },
    { "sys.hint",        "INT", 0xFFFFF01F, 0xD503201F, T_HINT },
    { "sys.barrier",     "INT", 0xFFFFF01F, 0xD503301F, 0 },
    { "sys.pstate",      "INT", 0xFFF8F01F, 0xD500401F, T_PSTATE },
    { "sys.sysinstr",    "INT", 0xFFD80000, 0xD5080000, T_SYSINSTR },
    { "sys.sysreg",      "INT", 0xFFD00000, 0xD5100000, T_SYSREG },
    { "sys.outros",      "INT", 0xFFC00000, 0xD5000000, 0 },
    { "br.reg",          "INT", 0xFE000000, 0xD6000000, T_BRREG },
    { "br.imm",          "INT", 0x7C000000, 0x14000000, 0 },
    { "br.cmp",          "INT", 0x7E000000, 0x34000000, 0 },
    { "br.test",         "INT", 0x7E000000, 0x36000000, 0 },
    /* ---- loads/stores ---- */
    { "simd.ls_mult",    "SIMD", 0xBFBF0000, 0x0C000000, 0 },
    { "simd.ls_mult_post","SIMD",0xBFA00000, 0x0C800000, 0 },
    { "simd.ls_single",  "SIMD", 0xBF9F0000, 0x0D000000, 0 },
    { "simd.ls_single_post","SIMD",0xBF800000,0x0D800000, 0 },
    { "ls.excl",         "INT", 0x3F000000, 0x08000000, 0 },
    { "ls.rcpc_unscaled","INT", 0x3F200C00, 0x19000000, 0 },
    { "ls.literal",      "INT", 0x3F000000, 0x18000000, 0 },
    { "simd.ls_literal", "SIMD", 0x3F000000, 0x1C000000, 0 },
    { "ls.pair",         "INT", 0x3E000000, 0x28000000, 0 },
    { "simd.ls_pair",    "SIMD", 0x3E000000, 0x2C000000, 0 },
    { "ls.imm9",         "INT", 0x3F200000, 0x38000000, 0 },
    { "simd.ls_imm9",    "SIMD", 0x3F200000, 0x3C000000, 0 },
    { "ls.atomic",       "INT", 0x3F200C00, 0x38200000, 0 },
    { "ls.regoff",       "INT", 0x3F200C00, 0x38200800, 0 },
    { "simd.ls_regoff",  "SIMD", 0x3F200C00, 0x3C200800, 0 },
    { "ls.pac",          "INT", 0x3F200400, 0x38200400, 0 },
    { "ls.uimm",         "INT", 0x3F000000, 0x39000000, 0 },
    { "simd.ls_uimm",    "SIMD", 0x3F000000, 0x3D000000, 0 },
    { "ls.outros",       "INT", 0x0A000000, 0x08000000, 0 },
    /* ---- dados com registrador ---- */
    { "dp.2src",         "INT", 0x5FE00000, 0x1AC00000, 0 },
    { "dp.1src",         "INT", 0x5FE00000, 0x5AC00000, T_1SRC },
    { "dp.logic_reg",    "INT", 0x1F000000, 0x0A000000, 0 },
    { "dp.addsub_shift", "INT", 0x1F200000, 0x0B000000, 0 },
    { "dp.addsub_ext",   "INT", 0x1F200000, 0x0B200000, 0 },
    { "dp.adc",          "INT", 0x1FE0FC00, 0x1A000000, 0 },
    { "dp.rmif",         "INT", 0x1FE07C00, 0x1A000400, 0 },
    { "dp.setf",         "INT", 0x1FE03C00, 0x1A000800, 0 },
    { "dp.ccmp_reg",     "INT", 0x1FE00800, 0x1A400000, 0 },
    { "dp.ccmp_imm",     "INT", 0x1FE00800, 0x1A400800, 0 },
    { "dp.csel",         "INT", 0x1FE00000, 0x1A800000, 0 },
    { "dp.3src",         "INT", 0x1F000000, 0x1B000000, 0 },
    { "dp.outros",       "INT", 0x0E000000, 0x0A000000, 0 },
    /* ---- ponto flutuante escalar ---- */
    { "fp.fixed",        "FP",  0x5F200000, 0x1E000000, 0 },
    { "fp.int",          "FP",  0x5F20FC00, 0x1E200000, 0 },
    { "fp.1src",         "FP",  0x5F207C00, 0x1E204000, 0 },
    { "fp.cmp",          "FP",  0x5F203C00, 0x1E202000, 0 },
    { "fp.imm",          "FP",  0x5F201C00, 0x1E201000, 0 },
    { "fp.ccmp",         "FP",  0x5F200C00, 0x1E200400, 0 },
    { "fp.2src",         "FP",  0x5F200C00, 0x1E200800, 0 },
    { "fp.csel",         "FP",  0x5F200C00, 0x1E200C00, 0 },
    { "fp.3src",         "FP",  0x5F000000, 0x1F000000, 0 },
    /* ---- criptografia ---- */
    { "crypto.aes",      "SIMD", 0xFF3E0C00, 0x4E280800, 0 },
    { "crypto.sha3reg",  "SIMD", 0xFF208C00, 0x5E000000, 0 },
    { "crypto.sha2reg",  "SIMD", 0xFF3E0C00, 0x5E280800, 0 },
    { "crypto.sha512_3", "SIMD", 0xFFE0B000, 0xCE608000, 0 },
    { "crypto.4reg",     "SIMD", 0xFF808000, 0xCE000000, 0 },
    { "crypto.xar",      "SIMD", 0xFFE00000, 0xCE800000, 0 },
    { "crypto.sha512_2", "SIMD", 0xFFFFF000, 0xCEC08000, 0 },
    /* ---- AdvSIMD escalar ---- */
    { "simds.copy",      "SIMD", 0xDFE08400, 0x5E000400, 0 },
    { "simds.3same_fp16","SIMD", 0xDF60C400, 0x5E400400, 0 },
    { "simds.2misc_fp16","SIMD", 0xDF7E0C00, 0x5E780800, 0 },
    { "simds.3same_extra","SIMD",0xDF208400, 0x5E008400, 0 },
    { "simds.2misc",     "SIMD", 0xDF3E0C00, 0x5E200800, 0 },
    { "simds.pairwise",  "SIMD", 0xDF3E0C00, 0x5E300800, 0 },
    { "simds.3diff",     "SIMD", 0xDF200C00, 0x5E200000, 0 },
    { "simds.3same",     "SIMD", 0xDF200400, 0x5E200400, 0 },
    { "simds.shift",     "SIMD", 0xDF800400, 0x5F000400, 0 },
    { "simds.indexed",   "SIMD", 0xDF000400, 0x5F000000, 0 },
    /* ---- AdvSIMD vetorial ---- */
    { "simdv.tbl",       "SIMD", 0xBF208C00, 0x0E000000, 0 },
    { "simdv.perm",      "SIMD", 0xBF208C00, 0x0E000800, 0 },
    { "simdv.ext",       "SIMD", 0xBF208400, 0x2E000000, 0 },
    { "simdv.copy",      "SIMD", 0x9FE08400, 0x0E000400, 0 },
    { "simdv.3same_fp16","SIMD", 0x9F60C400, 0x0E400400, 0 },
    { "simdv.2misc_fp16","SIMD", 0x9F7E0C00, 0x0E780800, 0 },
    { "simdv.3same_extra","SIMD",0x9F208400, 0x0E008400, 0 },
    { "simdv.2misc",     "SIMD", 0x9F3E0C00, 0x0E200800, 0 },
    { "simdv.across",    "SIMD", 0x9F3E0C00, 0x0E300800, 0 },
    { "simdv.3diff",     "SIMD", 0x9F200C00, 0x0E200000, 0 },
    { "simdv.3same",     "SIMD", 0x9F200400, 0x0E200400, 0 },
    { "simdv.modimm",    "SIMD", 0x9FF80400, 0x0F000400, 0 },
    { "simdv.shift",     "SIMD", 0x9F800400, 0x0F000400, 0 },
    { "simdv.indexed",   "SIMD", 0x9F000400, 0x0F000000, 0 },
    { "simd.outros",     "SIMD", 0x0E000000, 0x0E000000, 0 },
    /* ---- não alocado / SVE / SME (op0 = 0000..0011): tudo UDF ---- */
    { "naoalocado",      "INT", 0x18000000, 0x00000000, 0 },
};
#define NGROUPS ((int)(sizeof GROUPS / sizeof GROUPS[0]))

static int classify(u32 w) {
    for (int i = 0; i < NGROUPS; i++) if ((w & GROUPS[i].mask) == GROUPS[i].val) return i;
    return NGROUPS - 1;
}

/* instruções sem resultado determinístico no oráculo (ver o comentário do topo) */
static int excluded(u32 w) {
    u32 m = w & 0xFFFFFFE0;
    if (m == 0xD53BE000 || m == 0xD53BE020 || m == 0xD53BE040 || m == 0xD53BE0A0 || m == 0xD53BE0C0) return 1;
    if (m == 0xD53B00E0) return 1;                           /* DCZID_EL0 */
    if ((w & 0xFFE0FC00) == 0x9AC03000) return 1;            /* PACGA */
    if ((w & 0xFFF00000) == 0xD5300000 && ((w >> 12) & 0xF) == 14) return 1;  /* MRS CNT* de EL0 (timers: tempo) */
    return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* aleatoriedade e estado inicial                                                              */

static u64 g_rng = 0x9E3779B97F4A7C15ull;
static u64 rnd(void) { return fz_rng(&g_rng); }
static u32 rndn(u32 n) { return (u32)(rnd() % n); }

static const u64 SPECIAL64[] = {
    0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000,
    0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull,
    0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFF80000000ull, 0xFFFFFFFF7FFFFFFFull,
    0x5555555555555555ull, 0xAAAAAAAAAAAAAAAAull, 0x0123456789ABCDEFull, 0x00000000FFFF0000ull,
};
static const u64 FP64[] = {
    0x0000000000000000ull, 0x8000000000000000ull, 0x3FF0000000000000ull, 0xBFF0000000000000ull,
    0x3FE0000000000000ull, 0x4000000000000000ull, 0x7FF0000000000000ull, 0xFFF0000000000000ull,
    0x7FF8000000000000ull, 0xFFF8000000000001ull, 0x7FF4000000000000ull, 0x7FF0000000000001ull,
    0x7FEFFFFFFFFFFFFFull, 0x0010000000000000ull, 0x000FFFFFFFFFFFFFull, 0x0000000000000001ull,
    0x8000000000000001ull, 0x4330000000000000ull, 0x4330000000000001ull, 0x43E0000000000000ull,
    0x43F0000000000000ull, 0xC3E0000000000000ull, 0x41E0000000000000ull, 0x41EFFFFFFFE00000ull,
    0x3FF8000000000000ull, 0x4004000000000000ull, 0xC00C000000000000ull, 0x3FDFFFFFFFFFFFFFull,
    0x7E37E43C8800759Cull, 0x0360000000000000ull,
};
static const u32 FP32[] = {
    0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x3F000000, 0x40000000, 0x7F800000, 0xFF800000,
    0x7FC00000, 0xFFC00001, 0x7FA00000, 0x7F800001, 0x7F7FFFFF, 0x00800000, 0x007FFFFF, 0x00000001,
    0x80000001, 0x4B000000, 0x4B000001, 0x5F000000, 0x5F800000, 0xDF000000, 0x4F000000, 0x4F7FFFFF,
    0x3FC00000, 0x40200000, 0xC0600000, 0x3EFFFFFF, 0x00400000, 0x0C800000,
};
static const uint16_t FP16[] = {
    0x0000, 0x8000, 0x3C00, 0xBC00, 0x3800, 0x4000, 0x7C00, 0xFC00, 0x7E00, 0xFE01, 0x7D00, 0x7C01,
    0x7BFF, 0x0400, 0x03FF, 0x0001, 0x8001, 0x6400, 0x6401, 0x3E00, 0x4100, 0xC300, 0x37FF, 0x0200,
};
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

static u64 rand_x(void) {
    switch (rndn(10)) {
    case 0: case 1: case 2: return SPECIAL64[rndn(NELEM(SPECIAL64))];
    case 3: return rnd();
    case 4: return (u32)rnd();
    case 5: return (u64)-(int64_t)rndn(300);
    case 6: return rndn(256);
    case 7: case 8: {                                    /* ponteiro para o rascunho */
        u64 off = 0x800 + rndn(0x3000);
        if (rndn(4)) off &= ~7ull;
        return FZ_SCR_VA + off;
    }
    default: return SPECIAL64[rndn(NELEM(SPECIAL64))] + rndn(3) - 1;
    }
}
static void rand_v(u8 *v) {
    u64 d[2];
    switch (rndn(6)) {
    case 0: d[0] = rnd(); d[1] = rnd(); break;
    case 1: d[0] = FP64[rndn(NELEM(FP64))]; d[1] = FP64[rndn(NELEM(FP64))]; break;
    case 2: for (int i = 0; i < 4; i++) ((u32 *)d)[i] = FP32[rndn(NELEM(FP32))]; break;
    case 3: for (int i = 0; i < 8; i++) ((uint16_t *)d)[i] = FP16[rndn(NELEM(FP16))]; break;
    case 4: d[0] = SPECIAL64[rndn(NELEM(SPECIAL64))]; d[1] = SPECIAL64[rndn(NELEM(SPECIAL64))]; break;
    default: for (int i = 0; i < 16; i++) ((u8 *)d)[i] = (u8)(rndn(4) ? rndn(256) : (rndn(2) ? 0x80 : 0x7F)); break;
    }
    memcpy(v, d, 16);
}
static void rand_state(fz_state *s) {
    memset(s, 0, sizeof *s);
    for (int i = 0; i < 31; i++) s->x[i] = rand_x();
    /* sp sempre alinhado: o QEMU não implementa a checagem de SCTLR_EL1.SA0 (sp desalinhado
     * como base de acesso → falha), então isso não é verificável no oráculo */
    u64 off = 0x1000 + rndn(0x2000);
    s->sp = FZ_SCR_VA + (off & ~15ull);
    s->nzcv = (u32)rndn(16) << 28;
    static const u32 fpcrs[] = { 0, 0, 0, 1u << 25, 1u << 24, 1u << 19, 1u << 26, 1u << 22, 2u << 22, 3u << 22,
                                 (1u << 25) | (1u << 24), (3u << 22) | (1u << 24) | (1u << 19) };
    s->fpcr = fpcrs[rndn(NELEM(fpcrs))];
    if (!rndn(4)) s->fpcr = (u32)rnd() & ((1u << 19) | (3u << 22) | (1u << 24) | (1u << 25) | (1u << 26));
    s->fpsr = rndn(3) ? 0 : ((u32)rnd() & (0x9Fu | (1u << 27)));
    s->pstate = (rndn(4) ? 0 : (1u << 24)) | (rndn(4) ? 0 : (1u << 12));
    s->tpidr = rndn(2) ? 0 : rand_x();
    for (int i = 0; i < 32; i++) rand_v(s->v[i]);
}

static u32 gen_word(int g) {
    const group *G = &GROUPS[g];
    for (int tries = 0; tries < 1000; tries++) {
        u32 w;
        if (G->extra && rndn(2)) {
            int n = 0; while (G->extra[n].mask) n++;
            const tmpl *t = &G->extra[rndn(n)];
            w = t->val | ((u32)rnd() & ~t->mask);
        } else w = G->val | ((u32)rnd() & ~G->mask);
        if (classify(w) == g && !excluded(w)) return w;
    }
    return G->val;
}

/* ------------------------------------------------------------------------------------------ */
/* processos e arquivos                                                                        */

static char g_root[MAX_PATH], g_build[MAX_PATH], g_tmp[MAX_PATH];
static char g_qemu[MAX_PATH], g_objdump[MAX_PATH], g_a64test[MAX_PATH], g_mon[MAX_PATH];
static int g_quiet;

static int run(const char *cmd, const char *outfile, DWORD timeout_ms) {
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    HANDLE hout = INVALID_HANDLE_VALUE;
    if (outfile) {
        SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
        hout = CreateFileA(outfile, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hout; si.hStdError = GetStdHandle(STD_ERROR_HANDLE); si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    char *c = _strdup(cmd);
    BOOL ok = CreateProcessA(NULL, c, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(c);
    if (hout != INVALID_HANDLE_VALUE) CloseHandle(hout);
    if (!ok) { fprintf(stderr, "a64fuzz: não executou: %s (%lu)\n", cmd, GetLastError()); return -1; }
    DWORD code = 0;
    if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 255); WaitForSingleObject(pi.hProcess, 5000);
        fprintf(stderr, "a64fuzz: tempo esgotado: %s\n", cmd);
        code = (DWORD)-2;
    } else GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)code;
}
static void *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    void *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; }
    fclose(f); ((char *)b)[l] = 0; if (n) *n = l; return b;
}
static int exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

/* ------------------------------------------------------------------------------------------ */
/* validade pelo LLVM                                                                          */

#define LLVM_OURS "+v8a,+lor,+fp-armv8,+neon,+fullfp16,+fp16fml,+aes,+sha2,+sha3,+crc,+lse,+lse2,+rdm,+dotprod," \
                  "+complxnum,+jsconv,+rcpc,+rcpc-immo,+flagm,+altnzcv,+fptoint,+sb,+ssbs,+ccpp,+ccdp,+dit," \
                  "+pauth,+bti,+bf16,+i8mm,+ecv"
typedef struct { char ours[64], all[64]; } disinfo;

static void write_elf(const char *path, const u32 *w, u32 n) {
    FILE *f = fopen(path, "wb");
    u8 eh[64]; memset(eh, 0, 64);
    memcpy(eh, "\x7f" "ELF\x02\x01\x01", 7);
    *(uint16_t *)(eh + 16) = 1; *(uint16_t *)(eh + 18) = 183; *(u32 *)(eh + 20) = 1;
    static const char shs[] = "\0.text\0.shstrtab\0";
    u64 textoff = 64, textsz = 4ull * n, shsoff = textoff + textsz, shoff = (shsoff + sizeof shs + 7) & ~7ull;
    *(u64 *)(eh + 40) = shoff; *(uint16_t *)(eh + 52) = 64; *(uint16_t *)(eh + 58) = 64;
    *(uint16_t *)(eh + 60) = 3; *(uint16_t *)(eh + 62) = 2;
    fwrite(eh, 1, 64, f); fwrite(w, 4, n, f); fwrite(shs, 1, sizeof shs, f);
    for (u64 p = shsoff + sizeof shs; p < shoff; p++) fputc(0, f);
    u8 sh[3][64]; memset(sh, 0, sizeof sh);
    *(u32 *)(sh[1] + 0) = 1; *(u32 *)(sh[1] + 4) = 1; *(u64 *)(sh[1] + 8) = 6;
    *(u64 *)(sh[1] + 24) = textoff; *(u64 *)(sh[1] + 32) = textsz; *(u64 *)(sh[1] + 48) = 4;
    *(u32 *)(sh[2] + 0) = 7; *(u32 *)(sh[2] + 4) = 3; *(u64 *)(sh[2] + 24) = shsoff; *(u64 *)(sh[2] + 32) = sizeof shs; *(u64 *)(sh[2] + 48) = 1;
    fwrite(sh, 1, sizeof sh, f);
    fclose(f);
}
static void llvm_pass(const u32 *w, u32 n, const char *mattr, disinfo *d, int all) {
    char elf[MAX_PATH], out[MAX_PATH], cmd[2048];
    snprintf(elf, sizeof elf, "%s\\dis.o", g_tmp); snprintf(out, sizeof out, "%s\\dis%d.txt", g_tmp, all);
    write_elf(elf, w, n);
    snprintf(cmd, sizeof cmd, "\"%s\" -d --mattr=%s \"%s\"", g_objdump, mattr, elf);
    run(cmd, out, 600000);
    char *t = read_file(out, NULL);
    for (u32 i = 0; i < n; i++) strcpy(all ? d[i].all : d[i].ours, "<unknown>");
    if (!t) return;
    for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) {
        char *p = l; while (*p == ' ') p++;
        char *e; unsigned long long off = strtoull(p, &e, 16);
        if (e == p || *e != ':') continue;
        p = e + 1; while (*p == ' ' || *p == '\t') p++;
        p += 8; while (*p == ' ' || *p == '\t') p++;   /* palavra crua */
        char *cr = strchr(p, '\r'); if (cr) *cr = 0;
        for (char *q = p; *q; q++) if (*q == '\t') *q = ' ';
        u32 idx = (u32)(off / 4);
        if (idx < n) { char *dst = all ? d[idx].all : d[idx].ours; strncpy(dst, p, 63); dst[63] = 0; }
    }
    free(t);
}
static int g_have_llvm;
static void llvm_classify(const u32 *w, u32 n, disinfo *d) {
    if (!g_have_llvm) { for (u32 i = 0; i < n; i++) { strcpy(d[i].ours, "?"); strcpy(d[i].all, "?"); } return; }
    llvm_pass(w, n, LLVM_OURS, d, 0);
    llvm_pass(w, n, "+all", d, 1);
}
static int generic_sys(const char *s) {
    return !strncmp(s, "msr S", 5) || (!strncmp(s, "mrs ", 4) && strstr(s, ", S")) || !strncmp(s, "sys ", 4) || !strncmp(s, "sysl ", 5);
}
/* 1 = tem que ser UDF independentemente do QEMU; 0 = o QEMU decide */
static int must_udf(const disinfo *d) {
    if (!strcmp(d->ours, "?")) return 0;
    if (!strncmp(d->ours, "udf", 3)) return 1;
    /* desconhecida para nós: UDF, mesmo se o LLVM +all também não conhece (FEAT_FPRCVT do QEMU 11, p. ex.) */
    if (!strcmp(d->ours, "<unknown>")) return 1;
    if (generic_sys(d->ours) && !generic_sys(d->all) && strcmp(d->all, "<unknown>")) return 1;
    return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* execução dos lotes                                                                          */

static int write_batch(const char *path, const fz_case *c, u32 n, const char *outpath) {
    fz_batch_hdr h; memset(&h, 0, sizeof h);
    h.magic = FZ_BATCH_MAGIC; h.ncases = n; h.case_size = sizeof(fz_case); h.res_size = sizeof(fz_result);
    h.budget = 100000;
    if (outpath) { strncpy(h.outpath, outpath, sizeof h.outpath - 1); for (char *p = h.outpath; *p; p++) if (*p == '\\') *p = '/'; }
    FILE *f = fopen(path, "wb"); if (!f) return 0;
    fwrite(&h, sizeof h, 1, f); fwrite(c, sizeof *c, n, f); fclose(f);
    return 1;
}
static fz_result *load_results(const char *p, u32 n) {
    size_t sz; fz_result *r = read_file(p, &sz);
    if (!r || sz != (size_t)n * sizeof(fz_result)) { fprintf(stderr, "a64fuzz: resultado inválido em %s\n", p); free(r); return NULL; }
    return r;
}
enum { ENG_INTERP, ENG_JIT, ENG_QEMU };
#define JIT_FAILED 0xFFFFu   /* fz_result.kind: o a64test --jit caiu ou travou neste pedaço do lote */
static fz_result *run_engine(int eng, const fz_case *c, u32 n, int tag) {
    char batch[MAX_PATH], res[MAX_PATH], cmd[4096];
    snprintf(batch, sizeof batch, "%s\\lote%d_%d.bin", g_tmp, tag, eng);
    snprintf(res, sizeof res, "%s\\res%d_%d.bin", g_tmp, tag, eng);
    DeleteFileA(res);
    write_batch(batch, c, n, eng == ENG_QEMU ? res : NULL);
    int rc;
    if (eng == ENG_QEMU) {
        snprintf(cmd, sizeof cmd, "\"%s\" -M virt -cpu max -m 2048 -display none -serial none -monitor none -nodefaults "
                 "-semihosting-config enable=on,target=native -kernel \"%s\" -device \"loader,file=%s,addr=0x44000000,force-raw=on\"",
                 g_qemu, g_mon, batch);
        rc = run(cmd, NULL, 3600000);
    } else {
        snprintf(cmd, sizeof cmd, "\"%s\" %s--fuzz \"%s\" \"%s\"", g_a64test, eng == ENG_JIT ? "--jit " : "", batch, res);
        /* o interpretador tem orçamento por caso; o JIT só o vigia de 300 ms, que não resolve uma
         * trava do próprio motor: tempo limite proporcional ao lote */
        rc = run(cmd, NULL, eng == ENG_JIT ? (tag == 3 ? 5000u : 30000u) + 20 * n : 3600000);
    }
    if (rc != 0) { fprintf(stderr, "a64fuzz: motor %d saiu com %d\n", eng, rc); return NULL; }
    return load_results(res, n);
}

/* ------------------------------------------------------------------------------------------ */
/* comparação                                                                                  */

#define FPCR_MASK ((1u << 19) | (3u << 22) | (1u << 24) | (1u << 25) | (1u << 26))
enum { D_KIND = 1, D_PC = 2, D_INFO = 4, D_X = 8, D_SP = 16, D_NZCV = 32, D_FPCR = 64, D_FPSR = 128,
       D_PST = 256, D_TPIDR = 512, D_V = 1024, D_SCR = 2048 };
static u32 g_warn_addr, g_warn_scr;
/* STXR/STLXR/STXP/STLXP com o monitor local aberto (caso de 1 instrução): o ARM deixa
 * IMPLEMENTATION DEFINED se o store que falha faz a checagem de tradução/alinhamento. Aceita
 * "falhou com abort" × "status 1 sem acesso" (o QEMU nunca sonda). */
static u32 g_warn_excl;
static int store_excl(u32 w) {
    return (w & 0x3F400000) == 0x08000000 && !(w & (1u << 23)) && (!(w & (1u << 21)) || (w & (1u << 31)));
}
static int excl_tolerated(const fz_result *a, const fz_result *b, u32 insn, u32 ninsn) {
    if (ninsn != 1 || !store_excl(insn)) return 0;
    const fz_result *ok = a->kind == FZK_END ? a : b, *bad = a->kind == FZK_END ? b : a;
    if (ok->kind != FZK_END || bad->kind != FZK_FAULT) return 0;
    unsigned rs = (insn >> 16) & 31;
    if (rs != 31 && (u32)ok->st.x[rs] != 1) return 0;
    if (ok->scr_hash != bad->scr_hash) return 0;
    g_warn_excl++;
    return 1;
}
/* O mesmo IMPLEMENTATION DEFINED, quando o STXR* não é a palavra do caso: um BR/RET caiu no
 * rascunho (dados) e a exclusiva que falha aborta aqui e não no QEMU. O QEMU segue e pode dar
 * UDF numa palavra seguinte. O resto deixa de ser comparável. */
static int excl_fail_tolerated(const fz_result *ours, const fz_result *q) {
    if (ours->kind != FZK_FAULT || !store_excl(ours->insn)) return 0;
    if (q->kind == FZK_FAULT && q->pc == ours->pc) return 0;
    g_warn_excl++;
    return 1;
}

/* DC CVAC/CVAU/CIVAC e IC IVAU num endereço sem mapeamento: no ARM podem dar falha de tradução
 * (o tradutor sonda a linha, como o hardware), mas o QEMU trata como NOP sem sondar. Aceita
 * "nós FAULT" × "QEMU segue" com o resto do estado igual. (DC CVAP/CVADP o QEMU sonda: estrito.) */
static u32 g_warn_dc;
static int dc_tolerated(const fz_result *ours, const fz_result *q, u32 insn, u32 ninsn) {
    u32 m = insn & 0xFFFFFFE0;
    if (ninsn != 1 || (m != 0xD50B7A20 && m != 0xD50B7B20 && m != 0xD50B7E20 && m != 0xD50B7520)) return 0;
    if (ours->kind != FZK_FAULT || q->kind != FZK_END) return 0;
    if (memcmp(ours->st.x, q->st.x, sizeof ours->st.x) || ours->scr_hash != q->scr_hash) return 0;
    g_warn_dc++;
    return 1;
}

/* sequências: se nós falhamos num DC/IC sem sonda no QEMU, num STXR* (ver acima) ou por SA0, e o QEMU
 * passou dali, o resto da sequência não é comparável — conta como tolerado */
static u32 g_warn_seq;
static int seq_tolerated(const fz_result *ours, const fz_result *q, const fz_case *k) {
    u64 base = FZ_CODE_VA + FZ_CASE_OFF;
    /* o QEMU usa -1 como exclusive_addr "vazio": STXR* com base 0xffff...ffff tenta o store e
     * falha, onde o ARM (e nós) só devolvem status 1 — impl. definido também */
    if (k->ninsn > 1 && q->kind == FZK_FAULT && q->info == ~0ull && q->pc >= base && q->pc < base + 4ull * k->ninsn &&
        !(q->pc & 3) && store_excl(k->insn[(q->pc - base) / 4]) && !(ours->kind == FZK_FAULT && ours->pc == q->pc)) {
        g_warn_seq++;
        return 1;
    }
    if (k->ninsn <= 1 || ours->kind != FZK_FAULT) return 0;
    if (ours->pc < base || ours->pc >= base + 4ull * k->ninsn || (ours->pc & 3)) return 0;
    u32 w = k->insn[(ours->pc - base) / 4], m = w & 0xFFFFFFE0;
    int dc = m == 0xD50B7A20 || m == 0xD50B7B20 || m == 0xD50B7E20 || m == 0xD50B7520;
    /* SA0: uma instrução anterior desalinhou o sp (pré/pós-índice); nós falhamos como o XNU
     * (exit_info = sp), o QEMU não implementa a checagem */
    int sa0 = (ours->st.sp & 15) && ours->info == ours->st.sp;
    if (!dc && !sa0 && !store_excl(w)) return 0;
    if (q->kind == FZK_FAULT && q->pc == ours->pc) return 0;
    g_warn_seq++;
    return 1;
}

/* MSR FPCR com valor aleatório: o QEMU (-cpu max tem FEAT_AFP/EBF16) guarda AH/FIZ/NEP/EBF, que
 * mudam a semântica de FP das instruções seguintes; nós não anunciamos isso (A64_FPCR_MASK). Se o
 * QEMU terminou com bits fora da máscara, o caso não é comparável. */
/* páginas que o Windows mapeia em todo processo (KUSER_SHARED_DATA e vizinhas, 0x7FFE0000..
 * 0x7FFEFFFF): o a64test não consegue reservá-las, então um acesso ali não falha no harness */
static u32 g_warn_win;
static int win_tolerated(const fz_result *ours, const fz_result *q) {
    if (q->kind != FZK_FAULT || q->info < 0x7FFE0000ull - 0x10000 || q->info >= 0x7FFF0000ull) return 0;
    if (ours->kind == FZK_FAULT && ours->pc == q->pc) return 0;
    g_warn_win++;
    return 1;
}
static u32 g_warn_afp;
static int afp_tolerated(const fz_result *q) {
    if (!(q->st.fpcr & ~((1u << 19) | (3u << 22) | (1u << 24) | (1u << 25) | (1u << 26)))) return 0;
    g_warn_afp++;
    return 1;
}

static u32 cmp_res(const fz_result *a, const fz_result *b) {
    u32 d = 0;
    if (a->kind != b->kind) return D_KIND;
    if (a->kind == FZK_BUDGET) return 0;
    if (a->pc != b->pc) d |= D_PC;
    if (a->kind == FZK_FAULT) { if (a->info != b->info && a->info != ~0ull && b->info != ~0ull) g_warn_addr++; }
    else if (a->info != b->info) d |= D_INFO;
    if (memcmp(a->st.x, b->st.x, sizeof a->st.x)) d |= D_X;
    if (a->st.sp != b->st.sp) d |= D_SP;
    if (a->st.nzcv != b->st.nzcv) d |= D_NZCV;
    if ((a->st.fpcr & FPCR_MASK) != (b->st.fpcr & FPCR_MASK)) d |= D_FPCR;
    if (a->st.fpsr != b->st.fpsr) d |= D_FPSR;
    if (a->st.pstate != b->st.pstate) d |= D_PST;
    if (a->st.tpidr != b->st.tpidr) d |= D_TPIDR;
    if (memcmp(a->st.v, b->st.v, sizeof a->st.v)) d |= D_V;
    if (a->scr_hash != b->scr_hash) { if (a->kind == FZK_FAULT) g_warn_scr++; else d |= D_SCR; }
    return d;
}
static const char *kind_name(u32 k) {
    static const char *n[] = { "END", "SVC", "BRK", "UDF", "FAULT", "BUDGET", "HLT", "OTHER" };
    return k < 8 ? n[k] : "?";
}
static void describe(FILE *f, const fz_result *a, const fz_result *b, const char *la, const char *lb) {
    u32 d = cmp_res(a, b);
    if (d & D_KIND) { fprintf(f, "    saída: %s=%s(pc=%llx info=%llx insn=%08x) %s=%s(pc=%llx info=%llx insn=%08x)\n", la, kind_name(a->kind),
                              (unsigned long long)a->pc, (unsigned long long)a->info, a->insn, lb, kind_name(b->kind),
                              (unsigned long long)b->pc, (unsigned long long)b->info, b->insn); return; }
    if (d & D_PC) fprintf(f, "    pc: %s=%llx %s=%llx\n", la, (unsigned long long)a->pc, lb, (unsigned long long)b->pc);
    if (d & D_INFO) fprintf(f, "    info: %s=%llx %s=%llx\n", la, (unsigned long long)a->info, lb, (unsigned long long)b->info);
    for (int i = 0; i < 31; i++) if (a->st.x[i] != b->st.x[i])
        fprintf(f, "    x%d: %s=%016llx %s=%016llx\n", i, la, (unsigned long long)a->st.x[i], lb, (unsigned long long)b->st.x[i]);
    if (d & D_SP) fprintf(f, "    sp: %s=%016llx %s=%016llx\n", la, (unsigned long long)a->st.sp, lb, (unsigned long long)b->st.sp);
    if (d & D_NZCV) fprintf(f, "    nzcv: %s=%08x %s=%08x\n", la, a->st.nzcv, lb, b->st.nzcv);
    if (d & D_FPCR) fprintf(f, "    fpcr: %s=%08x %s=%08x\n", la, a->st.fpcr, lb, b->st.fpcr);
    if (d & D_FPSR) fprintf(f, "    fpsr: %s=%08x %s=%08x\n", la, a->st.fpsr, lb, b->st.fpsr);
    if (d & D_PST) fprintf(f, "    pstate: %s=%08x %s=%08x\n", la, a->st.pstate, lb, b->st.pstate);
    if (d & D_TPIDR) fprintf(f, "    tpidr_el0: %s=%016llx %s=%016llx\n", la, (unsigned long long)a->st.tpidr, lb, (unsigned long long)b->st.tpidr);
    for (int i = 0; i < 32; i++) if (memcmp(a->st.v[i], b->st.v[i], 16)) {
        u64 x[2], y[2]; memcpy(x, a->st.v[i], 16); memcpy(y, b->st.v[i], 16);
        fprintf(f, "    v%d: %s=%016llx%016llx %s=%016llx%016llx\n", i, la, (unsigned long long)x[1], (unsigned long long)x[0],
                lb, (unsigned long long)y[1], (unsigned long long)y[0]);
    }
    if (d & D_SCR) fprintf(f, "    rascunho (hash): %s=%016llx %s=%016llx\n", la, (unsigned long long)a->scr_hash, lb, (unsigned long long)b->scr_hash);
}

/* ------------------------------------------------------------------------------------------ */
/* regressões                                                                                  */

static void save_regress(const fz_case *k, int g, const char *dis) {
    char name[256], path[MAX_PATH];
    char gn[64]; strcpy(gn, GROUPS[g].name); for (char *p = gn; *p; p++) if (*p == '.') *p = '_';
    u32 key = k->ninsn ? k->insn[0] : 0;
    for (u32 i = 1; i < k->ninsn; i++) key = key * 31 + k->insn[i];
    snprintf(name, sizeof name, "regress_%s_%08x", gn, key);
    snprintf(path, sizeof path, "%s\\tools\\a64tests\\%s.S", g_root, name);
    if (exists(path)) return;
    FILE *f = fopen(path, "w"); if (!f) return;
    fprintf(f, "/* %s: divergência achada pelo tools/a64fuzz (grupo %s, frente %s).\n", name, GROUPS[g].name, GROUPS[g].front);
    fprintf(f, " * instrução: %s\n * Reproduzir: tools/build-a64test.ps1 -Teste %s; tools/a64cmp.ps1 build/a64tests/%s.elf\n */\n", dis, name, name);
    fprintf(f, "    .text\n    .globl main\nmain:\n    ldr x0, =0x%llx\n    br x0\n    .ltorg\n\n",
            (unsigned long long)(FZ_CODE_VA + 0xF000));
    fprintf(f, "    .section .a64fzcode, \"awx\", @progbits\n");
    fprintf(f, "    .fill 0x%llx, 4, 0x%08x\n", (unsigned long long)(FZ_CASE_OFF / 4), FZ_TRAP_INSN);
    fprintf(f, "caso:\n");
    for (u32 i = 0; i < k->ninsn; i++) fprintf(f, "    .inst 0x%08x\n", k->insn[i]);
    fprintf(f, "    .inst 0x%08x    /* svc #0x4001: dump */\n    .inst 0x%08x    /* svc #0x4002: exit(0) */\n", FZ_END_INSN, FZ_EXIT0_INSN);
    fprintf(f, "    .fill 0x%llx, 4, 0x%08x\n", (unsigned long long)((0xF000 - FZ_CASE_OFF) / 4 - k->ninsn - 2), FZ_TRAP_INSN);
    fprintf(f, "carga:                          /* em 0x%llx: estado inicial do caso */\n", (unsigned long long)(FZ_CODE_VA + 0xF000));
    fprintf(f, "    adr x30, estado\n");
    for (int i = 0; i < 32; i += 2) fprintf(f, "    ldp q%d, q%d, [x30, #%d]\n", i, i + 1, 288 + 16 * i);
    fprintf(f, "    ldr w0, [x30, #260]\n    msr fpcr, x0\n    ldr w0, [x30, #264]\n    msr fpsr, x0\n");
    fprintf(f, "    ldr w0, [x30, #268]\n    tbz w0, #24, 1f\n    msr dit, #1\n1:  tbz w0, #12, 2f\n    msr ssbs, #1\n2:\n");
    fprintf(f, "    ldr x0, [x30, #272]\n    msr tpidr_el0, x0\n    ldr x0, [x30, #248]\n    mov sp, x0\n");
    fprintf(f, "    ldr w0, [x30, #256]\n    msr nzcv, x0\n");
    for (int i = 0; i < 30; i += 2) fprintf(f, "    ldp x%d, x%d, [x30, #%d]\n", i, i + 1, 8 * i);
    fprintf(f, "    ldr x30, [x30, #240]\n    b caso\n");
    fprintf(f, "    .balign 16\nestado:\n");
    const u64 *q = (const u64 *)&k->st;
    for (unsigned i = 0; i < sizeof(fz_state) / 8; i++) fprintf(f, "%s0x%016llx%s", i % 4 ? ", " : "    .quad ", (unsigned long long)q[i], i % 4 == 3 ? "\n" : "");
    fprintf(f, "\n    .section .a64fzscr, \"aw\", @progbits\n");
    u64 s = k->scr_seed | 1;
    for (unsigned i = 0; i < FZ_SCR_SZ / 8; i++) fprintf(f, "%s0x%016llx%s", i % 4 ? ", " : "    .quad ", (unsigned long long)fz_rng(&s), i % 4 == 3 ? "\n" : "");
    fclose(f);
    if (!g_quiet) printf("  salvo: tools\\a64tests\\%s.S\n", name);
}

/* ------------------------------------------------------------------------------------------ */

static void cleanup_tmp(void) {
    char pat[MAX_PATH], p[MAX_PATH]; WIN32_FIND_DATAA fd;
    snprintf(pat, sizeof pat, "%s\\*", g_tmp);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { snprintf(p, sizeof p, "%s\\%s", g_tmp, fd.cFileName); DeleteFileA(p); } } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(g_tmp);
}

/* ------------------------------------------------------------------------------------------ */
/* FMA do oráculo: o qemu-system-aarch64 do Windows (mingw) tem um caminho "hardfloat" que, com
 * FPSR.IXC já ligado, RN e entradas normais, calcula muladd de f32/f64 com o fma() da libm do
 * mingw — que arredonda DUAS vezes (produto em x87 de 80 bits). Fora disso usa o softfloat, que é
 * exato. Então, em casos de 1 instrução da família FMA:
 *   - o estado inicial tem FPSR.IXC = 0 → a operação escalar (e a lane 0) sempre sai certa;
 *   - FMLA/FMLS/FRECPS vetoriais que divergem são rechecados lane a lane: para a lane k, as lanes
 *     j < k de Vd/Vn (e Vm, fora do "por elemento") viram +0 → 0*x+0 é exato, IXC fica limpo e a
 *     lane k passa pelo softfloat;
 *   - o resto da família com lanes de 32/64 bits (FCMLA, FMLAL/FMLSL, BF16) que divergir fica como
 *     "suspeito_fma" (contado à parte, não como divergência). FRSQRTS não entra: o QEMU sempre usa o
 *     softfloat nele (float_muladd_halve_result). */
static u32 g_fma_fixed;
static void mnem(const char *d, char *m) { int i = 0; while (d[i] && d[i] != ' ' && i < 15) { m[i] = d[i]; i++; } m[i] = 0; }
static int fma_family(const char *d) {
    static const char *F[] = { "fmla", "fmls", "fmlal", "fmlal2", "fmlsl", "fmlsl2", "frecps", "fcmla", "bfmlalb", "bfmlalt",
                               "fmadd", "fmsub", "fnmadd", "fnmsub", "bfdot", "bfmmla", 0 };
    char m[16]; mnem(d, m);
    for (int k = 0; F[k]; k++) if (!strcmp(m, F[k])) return 1;
    return 0;
}
static int vec_lanes(const char *d, int *esize) {
    const char *p = strchr(d, ' ');
    if (!p || p[1] != 'v') return 0;
    p = strchr(p, '.'); if (!p) return 0;
    if (!strncmp(p, ".2d", 3)) { *esize = 8; return 2; }
    if (!strncmp(p, ".4s", 3)) { *esize = 4; return 4; }
    if (!strncmp(p, ".2s", 3)) { *esize = 4; return 2; }
    return 0;
}
static void fma_recheck(const fz_case *cs, u32 n, int K, const disinfo *dis, const fz_result *ri, fz_result *rq, u8 *susp) {
    if (K != 1) {   /* sequências: FMA vetorial só pode estar na 1ª posição; sem recheque */
        for (u32 i = 0; i < n; i++) {
            int es;
            if (fma_family(dis[i].ours) && vec_lanes(dis[i].ours, &es) && cmp_res(&ri[i], &rq[i])) susp[i] = 1;
        }
        return;
    }
    u32 cap = 0, nv = 0; fz_case *var = NULL; u32 *from = NULL, *lane = NULL;
    for (u32 i = 0; i < n; i++) {
        if (ri[i].kind == FZK_UDF || rq[i].kind == FZK_UDF || !fma_family(dis[i].ours) || !cmp_res(&ri[i], &rq[i])) continue;
        int es, L = vec_lanes(dis[i].ours, &es);
        if (!L) continue;                                  /* escalar ou f16: o QEMU já é confiável */
        char m[16]; mnem(dis[i].ours, m);
        u32 w = cs[i].insn[0], rd = w & 31, rn = (w >> 5) & 31, rm = (w >> 16) & 31;
        int byelem = strchr(dis[i].ours, '[') != NULL;
        int fcmla = !strcmp(m, "fcmla");
        int iso = !strcmp(m, "fmla") || !strcmp(m, "fmls") || !strcmp(m, "frecps") || (fcmla && !byelem);
        if (iso && byelem && (rm == rn || rm == rd)) iso = 0;
        /* FCMLA (vetor): o QEMU faz e0 = muladd(n[i+f], m[i+f], d[i]) e depois e1 =
         * muladd(n[i+f], m[i+1-f], d[i+1]), f = rot & 1. Para conferir e1, zera d[i] e m[i+f]
         * (e0 vira exato sem mexer nas entradas de e1); pares anteriores zerados por inteiro. */
        int flip = (int)((w >> 11) & 1);
        if (fcmla && iso && (rn == rd || rn == rm || rd == rm)) iso = 0;
        if (!iso) { susp[i] = 1; continue; }
        for (int k = 1; k < L; k++) {
            if (nv == cap) { cap = cap ? cap * 2 : 256; var = realloc(var, cap * sizeof *var); from = realloc(from, cap * 4); lane = realloc(lane, cap * 4); }
            fz_case v = cs[i];
            int upto = fcmla ? (k & ~1) : k;
            for (int j = 0; j < upto; j++) {
                memset(v.st.v[rn] + j * es, 0, es);
                memset(v.st.v[rd] + j * es, 0, es);
                if (!byelem) memset(v.st.v[rm] + j * es, 0, es);
            }
            if (fcmla && (k & 1)) {
                memset(v.st.v[rd] + (k - 1) * es, 0, es);
                memset(v.st.v[rm] + (k - 1 + flip) * es, 0, es);
            }
            var[nv] = v; from[nv] = i; lane[nv] = (u32)k; nv++;
        }
    }
    if (!nv) return;
    fz_result *r = run_engine(ENG_QEMU, var, nv, 2);
    if (!r) { for (u32 t = 0; t < nv; t++) susp[from[t]] = 1; free(var); free(from); free(lane); return; }
    for (u32 t = 0; t < nv; t++) {
        u32 i = from[t]; int es; vec_lanes(dis[i].ours, &es);
        u32 rd = cs[i].insn[0] & 31;
        if (r[t].kind != rq[i].kind) { susp[i] = 1; continue; }
        if (memcmp(rq[i].st.v[rd] + lane[t] * es, r[t].st.v[rd] + lane[t] * es, es)) g_fma_fixed++;
        memcpy(rq[i].st.v[rd] + lane[t] * es, r[t].st.v[rd] + lane[t] * es, es);   /* lane k corrigida */
    }
    free(r); free(var); free(from); free(lane);
}

typedef struct { u32 gen, must_udf, exp_exec, exp_udf, impl, match, diverge, udf_bad, should_udf, udf_ok, fma_susp,
                 jit_div, jit_match, saved; } gstat;

int main(int argc, char **argv) {
    int casos = 2000, K = 1, jit = 0, salvar = 0, max_salvos = 3, listar = 0, lote = 40000;
    const char *relatorio = NULL, *exemplos = NULL, *so_gerar = NULL;
    int sel[NGROUPS]; int nsel = 0;
    u64 seed = GetTickCount64();
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--grupo") && i + 1 < argc) {
            const char *g = argv[++i];
            for (int j = 0; j < NGROUPS; j++)
                if (!strcmp(g, "all") || !strcmp(GROUPS[j].name, g) || !_stricmp(GROUPS[j].front, g) ||
                    (strchr(g, '*') && !strncmp(GROUPS[j].name, g, strchr(g, '*') - g))) {
                    int dup = 0; for (int k = 0; k < nsel; k++) dup |= sel[k] == j;
                    if (!dup) sel[nsel++] = j;
                }
        }
        else if (!strcmp(a, "--casos") && i + 1 < argc) casos = atoi(argv[++i]);
        else if (!strcmp(a, "--k") && i + 1 < argc) K = atoi(argv[++i]);
        else if (!strcmp(a, "--semente") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(a, "--jit")) jit = 1;
        else if (!strcmp(a, "--salvar")) salvar = 1;
        else if (!strcmp(a, "--max-salvos") && i + 1 < argc) max_salvos = atoi(argv[++i]);
        else if (!strcmp(a, "--lote") && i + 1 < argc) lote = atoi(argv[++i]);
        else if (!strcmp(a, "--relatorio") && i + 1 < argc) relatorio = argv[++i];
        else if (!strcmp(a, "--exemplos") && i + 1 < argc) exemplos = argv[++i];
        else if (!strcmp(a, "--quieto")) g_quiet = 1;
        else if (!strcmp(a, "--listar")) listar = 1;
        else if (!strcmp(a, "--so-gerar") && i + 1 < argc) so_gerar = argv[++i];
        else { fprintf(stderr, "a64fuzz: opção desconhecida %s (ver o comentário do topo de tools/a64fuzz.c)\n", a); return 2; }
    }
    if (listar) { for (int j = 0; j < NGROUPS; j++) printf("%-22s %-5s %08x/%08x\n", GROUPS[j].name, GROUPS[j].front, GROUPS[j].mask, GROUPS[j].val); return 0; }
    if (!nsel) for (int j = 0; j < NGROUPS; j++) sel[nsel++] = j;
    if (K < 1) K = 1; if (K > FZ_MAXINSN - 1) K = FZ_MAXINSN - 1;
    g_rng = seed * 0x9E3779B97F4A7C15ull + 1;

    GetModuleFileNameA(NULL, g_build, sizeof g_build);
    *strrchr(g_build, '\\') = 0;                       /* ...\build */
    strcpy(g_root, g_build); *strrchr(g_root, '\\') = 0;
    /* um diretório por processo: várias sessões rodam o fuzzer ao mesmo tempo */
    snprintf(g_tmp, sizeof g_tmp, "%s\\a64fuzz", g_build); CreateDirectoryA(g_tmp, NULL);
    { size_t l = strlen(g_tmp); snprintf(g_tmp + l, sizeof g_tmp - l, "\\%lu", (unsigned long)GetCurrentProcessId()); CreateDirectoryA(g_tmp, NULL); }
    { const char *t = getenv("MOS_A64TEST"); if (t) snprintf(g_a64test, sizeof g_a64test, "%s", t); else snprintf(g_a64test, sizeof g_a64test, "%s\\a64test.exe", g_build); }   /* o placar usa uma cópia própria */
    snprintf(g_mon, sizeof g_mon, "%s\\a64oracle\\mon.elf", g_build);
    const char *q = getenv("MOS_QEMU_A64"); strcpy(g_qemu, q ? q : "C:\\Program Files\\qemu\\qemu-system-aarch64.exe");
    const char *o = getenv("MOS_LLVM_OBJDUMP"); strcpy(g_objdump, o ? o : "C:\\Program Files\\LLVM\\bin\\llvm-objdump.exe");
    g_have_llvm = exists(g_objdump);
    if (!exists(g_qemu) || !exists(g_mon) || !exists(g_a64test)) {
        fprintf(stderr, "a64fuzz: falta %s\n", !exists(g_qemu) ? g_qemu : !exists(g_mon) ? g_mon : g_a64test); return 2;
    }
    if (!g_have_llvm) fprintf(stderr, "a64fuzz: aviso: sem llvm-objdump (%s); validade só pelo QEMU (recursos a mais do -cpu max não viram UDF esperado)\n", g_objdump);
    if (!g_quiet) printf("a64fuzz: semente %llu, %d grupo(s) × %d casos, K=%d%s\n", (unsigned long long)seed, nsel, casos, K, jit ? ", com JIT" : "");

    u32 n = (u32)nsel * (u32)casos;
    fz_case *cs = calloc(n, sizeof *cs);
    int *cg = calloc(n, sizeof(int));
    disinfo *dis = calloc(n, sizeof *dis);
    /* candidatas: CAND× por caso; o LLVM separa as válidas e ~85% dos casos usam uma válida
     * (o resto fica aleatório dentro do grupo, para cobrir o que tem que dar UDF) */
    const u32 CAND = g_have_llvm ? 6 : 1;
    u32 nc = n * CAND;
    u32 *cw = malloc((size_t)nc * 4);
    for (u32 i = 0; i < nc; i++) cw[i] = gen_word(sel[i / ((u32)casos * CAND)]);
    disinfo *cd = calloc(nc, sizeof *cd);
    llvm_classify(cw, nc, cd);
    u32 *validpool = malloc((size_t)nc * 4), nvalid = 0;
    for (u32 i = 0; i < nc; i++)
        if (!g_have_llvm || (!must_udf(&cd[i]) && !fma_family(cd[i].ours))) validpool[nvalid++] = i;   /* sem FMA no meio das sequências (ver fma_family) */
    for (int k = 0; k < nsel; k++) {
        u32 b = (u32)k * (u32)casos * CAND, e = b + (u32)casos * CAND;
        u32 *vi = malloc((size_t)casos * CAND * 4), nv = 0;
        for (u32 i = b; i < e; i++) if (!g_have_llvm || !must_udf(&cd[i])) vi[nv++] = i;
        for (int j = 0; j < casos; j++) {
            u32 i = (u32)k * (u32)casos + (u32)j;
            u32 pick = (nv && (K > 1 || rndn(100) < 85)) ? vi[rndn(nv)] : b + rndn(e - b);
            if (K > 1 && !nv && nvalid) pick = validpool[rndn(nvalid)];   /* grupo só de UDF: sequência de válidas */
            rand_state(&cs[i].st);
            cs[i].scr_seed = rnd();
            cs[i].ninsn = (u32)K;
            cs[i].insn[0] = cw[pick];
            dis[i] = cd[pick];
            if (fma_family(dis[i].ours)) cs[i].st.fpsr &= ~0x10u;   /* ver fma_family */
            for (int q = 1; q < K; q++) cs[i].insn[q] = nvalid ? cw[validpool[rndn(nvalid)]] : gen_word(sel[rndn(nsel)]);
            cg[i] = sel[k];
        }
        free(vi);
    }
    free(cw); free(cd); free(validpool);
    if (so_gerar) {                          /* depuração: só grava o lote (a64test --fuzz lote saida) */
        int ok = write_batch(so_gerar, cs, n, NULL);
        cleanup_tmp();
        return ok ? 0 : 2;
    }

    fz_result *ri = calloc(n, sizeof *ri), *rj = jit ? calloc(n, sizeof *rj) : NULL, *rq = calloc(n, sizeof *rq);
    DWORD t0 = GetTickCount();
    for (u32 b = 0; b < n; b += (u32)lote) {
        u32 m = n - b < (u32)lote ? n - b : (u32)lote;
        fz_result *x = run_engine(ENG_INTERP, cs + b, m, 0); if (!x) return 3; memcpy(ri + b, x, m * sizeof *x); free(x);
        if (jit) {
            x = run_engine(ENG_JIT, cs + b, m, 0);
            if (x) { memcpy(rj + b, x, m * sizeof *x); free(x); }
            else {
                /* o JIT caiu ou travou: refaz em pedaços; o pedaço que falhar fica marcado */
                for (u32 p = 0; p < m; p += 64) {
                    u32 q = m - p < 64 ? m - p : 64;
                    fz_result *y = run_engine(ENG_JIT, cs + b + p, q, 3);
                    if (y) { memcpy(rj + b + p, y, q * sizeof *y); free(y); }
                    else for (u32 t = 0; t < q; t++) rj[b + p + t].kind = JIT_FAILED;
                }
            }
        }
        x = run_engine(ENG_QEMU, cs + b, m, 0); if (!x) return 3; memcpy(rq + b, x, m * sizeof *x); free(x);
    }
    DWORD t1 = GetTickCount();

    u8 *susp = calloc(n, 1);
    /* UDF nosso numa palavra que não é a do caso (dado do rascunho executado depois de um desvio):
     * se o LLVM com os nossos recursos não a conhece (MTE, HBC, CMPBR... que o -cpu max tem), o UDF
     * está certo. Vale também quando o QEMU executa essa palavra e só dá UDF mais adiante. */
    u8 *udf_alt = calloc(n, 1);
    {
        u32 na = 0, *idx = malloc((size_t)n * 4), *wa = malloc((size_t)n * 4);
        for (u32 i = 0; i < n; i++)
            if (ri[i].kind == FZK_UDF && ri[i].insn != cs[i].insn[0]) { idx[na] = i; wa[na++] = ri[i].insn; }
        if (na) {
            disinfo *da = calloc(na, sizeof *da);
            llvm_classify(wa, na, da);
            for (u32 t = 0; t < na; t++) if (must_udf(&da[t])) udf_alt[idx[t]] = 1;
            free(da);
        }
        free(idx); free(wa);
    }
    fma_recheck(cs, n, K, dis, ri, rq, susp);

    gstat st[NGROUPS]; memset(st, 0, sizeof st);
    FILE *ex = exemplos ? fopen(exemplos, "w") : NULL;
    u32 shown[NGROUPS]; memset(shown, 0, sizeof shown);
    fz_case *todo = calloc(n, sizeof *todo); int *todo_g = calloc(n, sizeof(int)); u32 ntodo = 0;
    for (u32 i = 0; i < n; i++) {
        int g = cg[i]; gstat *s = &st[g];
        s->gen++;
        int mu = (K == 1) && must_udf(&dis[i]);
        int exp_udf = mu || (K == 1 && rq[i].kind == FZK_UDF);   /* K>1: UDF do QEMU no meio da sequência compara como saída */
        if (mu) s->must_udf++;
        const char *verd;
        int same_udf = ri[i].kind == FZK_UDF && rq[i].kind == FZK_UDF && rq[i].pc == ri[i].pc;
        if (exp_udf && ri[i].kind == FZK_UDF) {
            s->exp_udf++;
            s->udf_ok++; verd = NULL;
        } else if (exp_udf && !mu && excl_fail_tolerated(&ri[i], &rq[i])) {
            /* o UDF do QEMU é consequência de ele ter passado do STXR*; o caso em si executou */
            s->exp_exec++; s->impl++; s->match++; verd = NULL;
        } else if (exp_udf) {
            s->exp_udf++;
            s->should_udf++; verd = "deveria_udf";
        } else {
            s->exp_exec++;
            if (ri[i].kind == FZK_UDF && udf_alt[i] && !same_udf) { s->impl++; s->match++; verd = NULL; }   /* executou o caso; o UDF da palavra não anunciada está certo */
            else if (ri[i].kind == FZK_UDF && rq[i].kind != FZK_UDF && !(K > 1 && seq_tolerated(&ri[i], &rq[i], &cs[i]))) { s->udf_bad++; verd = "udf_indevido"; }
            else {
                s->impl++;
                if (cmp_res(&ri[i], &rq[i]) && !excl_tolerated(&ri[i], &rq[i], cs[i].insn[0], cs[i].ninsn) && !dc_tolerated(&ri[i], &rq[i], cs[i].insn[0], cs[i].ninsn) && !seq_tolerated(&ri[i], &rq[i], &cs[i]) && !excl_fail_tolerated(&ri[i], &rq[i]) && !afp_tolerated(&rq[i]) && !win_tolerated(&ri[i], &rq[i])) {
                    if (susp[i]) { s->fma_susp++; verd = "suspeito_fma"; } else { s->diverge++; verd = "diverge"; }
                } else { s->match++; verd = NULL; }
            }
        }
        if (jit) {
            if (rj[i].kind == JIT_FAILED) { s->jit_div++; if (!verd) verd = "jit_travou"; }
            else if (cmp_res(&rj[i], &ri[i])) { s->jit_div++; if (!verd) verd = "jit_diverge"; } else s->jit_match++;
        }
        if (verd && ex && shown[g] < 20) {
            shown[g]++;
            fprintf(ex, "[%s] %s  %08x  %s  (llvm: %s | +all: %s)\n", GROUPS[g].name, verd, cs[i].insn[0], GROUPS[g].front, dis[i].ours, dis[i].all);
            for (u32 j = 1; j < cs[i].ninsn; j++) fprintf(ex, "    + %08x\n", cs[i].insn[j]);
            if (!strcmp(verd, "jit_travou")) fprintf(ex, "    a64test --jit caiu ou travou no pedaço de até 64 casos que contém este\n");
            else if (!strcmp(verd, "jit_diverge")) describe(ex, &rj[i], &ri[i], "jit", "interp");
            else describe(ex, &ri[i], &rq[i], "interp", "qemu");
        }
        if (verd && !strcmp(verd, "diverge") && salvar && st[g].saved < (u32)max_salvos) {
            todo[ntodo] = cs[i]; todo_g[ntodo] = g; ntodo++; st[g].saved++;
        }
    }

    /* minimização (K>1): troca instruções por NOP enquanto interp × qemu continuar divergindo */
    for (int round = 0; round < K && ntodo; round++) {
        u32 nv = 0; fz_case *var = calloc((size_t)ntodo * K, sizeof *var); u32 *vfrom = calloc((size_t)ntodo * K, 4); u32 *vpos = calloc((size_t)ntodo * K, 4);
        for (u32 t = 0; t < ntodo; t++)
            for (u32 j = 0; j < todo[t].ninsn; j++) {
                if (todo[t].ninsn == 1 || todo[t].insn[j] == 0xD503201F) continue;
                var[nv] = todo[t]; var[nv].insn[j] = 0xD503201F; vfrom[nv] = t; vpos[nv] = j; nv++;
            }
        if (!nv) { free(var); free(vfrom); free(vpos); break; }
        fz_result *a = run_engine(ENG_INTERP, var, nv, 1), *bq = run_engine(ENG_QEMU, var, nv, 1);
        int changed = 0;
        if (a && bq) {
            u32 *done = calloc(ntodo, 4);
            for (u32 v = 0; v < nv; v++) if (!done[vfrom[v]] && a[v].kind != FZK_UDF && bq[v].kind != FZK_UDF && cmp_res(&a[v], &bq[v])) {
                todo[vfrom[v]].insn[vpos[v]] = 0xD503201F; done[vfrom[v]] = 1; changed = 1;
            }
            free(done);
        }
        free(a); free(bq); free(var); free(vfrom); free(vpos);
        if (!changed) break;
    }
    for (u32 t = 0; t < ntodo; t++) {
        fz_case k = todo[t]; u32 m = 0;
        for (u32 j = 0; j < k.ninsn; j++) if (k.insn[j] != 0xD503201F || k.ninsn == 1) k.insn[m++] = k.insn[j];
        if (!m) { m = 1; k.insn[0] = todo[t].insn[0]; }
        k.ninsn = m;
        disinfo d1; llvm_classify(&k.insn[0], 1, &d1);
        save_regress(&k, classify(k.insn[0]) == todo_g[t] ? todo_g[t] : classify(k.insn[0]), d1.ours);
    }
    if (ex) fclose(ex);

    /* resumo */
    FILE *rep = relatorio ? fopen(relatorio, "w") : NULL;
    if (rep) fprintf(rep, "grupo\tfrente\tgerados\tesperado_exec\tesperado_udf\timplementadas\tbatem\tdivergem\tudf_indevido\tdeveria_udf\tjit_batem\tjit_divergem\tsuspeito_fma\n");
    if (!g_quiet) printf("%-22s %-5s %7s %7s %7s %7s %7s %7s %7s %7s %7s%s\n", "grupo", "frente", "gerados", "exec", "impl", "batem", "diverg", "udf_ind", "dev_udf", "udf_ok", "fma_sus", jit ? "  jit_div" : "");
    gstat tot; memset(&tot, 0, sizeof tot);
    for (int k = 0; k < nsel; k++) {
        int g = sel[k]; gstat *s = &st[g];
        if (!g_quiet) printf("%-22s %-5s %7u %7u %7u %7u %7u %7u %7u %7u %7u", GROUPS[g].name, GROUPS[g].front, s->gen, s->exp_exec, s->impl, s->match, s->diverge, s->udf_bad, s->should_udf, s->udf_ok, s->fma_susp);
        if (!g_quiet) { if (jit) printf("  %7u", s->jit_div); printf("\n"); }
        if (rep) fprintf(rep, "%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n", GROUPS[g].name, GROUPS[g].front, s->gen, s->exp_exec, s->exp_udf, s->impl, s->match, s->diverge, s->udf_bad, s->should_udf, jit ? s->jit_match : 0, jit ? s->jit_div : 0, s->fma_susp);
        tot.gen += s->gen; tot.exp_exec += s->exp_exec; tot.impl += s->impl; tot.match += s->match; tot.diverge += s->diverge;
        tot.udf_bad += s->udf_bad; tot.should_udf += s->should_udf; tot.udf_ok += s->udf_ok; tot.jit_div += s->jit_div; tot.fma_susp += s->fma_susp;
    }
    if (rep) fclose(rep);
    if (!g_quiet) {
        printf("%-22s %-5s %7u %7u %7u %7u %7u %7u %7u %7u %7u", "TOTAL", "", tot.gen, tot.exp_exec, tot.impl, tot.match, tot.diverge, tot.udf_bad, tot.should_udf, tot.udf_ok, tot.fma_susp);
        if (jit) printf("  %7u", tot.jit_div);
        printf("\n%u casos × %d instrução(ões) em %.1f s (avisos: %u endereço de falha, %u rascunho após falha, %u STXR impl. definido, %u DC/IC sondado só por nós, %u sequências cortadas em DC/STXR, %u com FPCR.AH/FIZ/NEP do QEMU, %u em página do Windows, %u lanes de FMA do QEMU corrigidas)\n",
               n, K, (t1 - t0) / 1000.0, g_warn_addr, g_warn_scr, g_warn_excl, g_warn_dc, g_warn_seq, g_warn_afp, g_warn_win, g_fma_fixed);
    }
    cleanup_tmp();
    return (tot.diverge || tot.should_udf || tot.jit_div) ? 1 : 0;
}

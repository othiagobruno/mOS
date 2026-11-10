/*
 * mOS — tradutor arm64 → x86_64 (A64, modo usuário EL0). Contrato comum entre as partes.
 * Arquitetura, metas e divisão do trabalho: docs/tradutor-arm64.md. Leia antes de mexer aqui.
 *
 * Este cabeçalho é C11 freestanding: compila no runtime do mOS (ELF x86_64 sem libc) e no
 * harness do Windows (tools/a64test.c). Nada aqui pode depender de libc além de <stdint.h>,
 * <stddef.h>, <stdbool.h> e dos builtins do clang.
 *
 * Mudanças neste arquivo: só acrescentar (campos novos no FIM das structs, funções novas).
 * Mudar o significado de algo existente exige combinar com todas as frentes (ver o doc).
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t  a64_u8;
typedef uint16_t a64_u16;
typedef uint32_t a64_u32;
typedef uint64_t a64_u64;
typedef int64_t  a64_i64;
typedef unsigned __int128 a64_u128;

/* ---------------------------------------------------------------------------------------------
 * Estado arquitetural de uma thread guest
 * ------------------------------------------------------------------------------------------- */

typedef union a64_vreg {
    a64_u8  b[16];
    a64_u16 h[8];
    a64_u32 s[4];
    a64_u64 d[2];
    float   f32[4];
    double  f64[2];
    a64_u128 q;
} __attribute__((aligned(16))) a64_vreg;

/* bits de nzcv (mesma posição do PSTATE/registrador NZCV) */
#define A64_N (1u << 31)
#define A64_Z (1u << 30)
#define A64_C (1u << 29)
#define A64_V (1u << 28)

/* FPCR */
#define A64_FPCR_FIZ   (1u << 0)    /* FEAT_AFP: não anunciado; sempre 0 */
#define A64_FPCR_AH    (1u << 1)    /* FEAT_AFP: não anunciado; sempre 0 */
#define A64_FPCR_NEP   (1u << 2)    /* FEAT_AFP: não anunciado; sempre 0 */
#define A64_FPCR_FZ16  (1u << 19)
#define A64_FPCR_RMODE_SHIFT 22     /* 0=RN 1=RP 2=RM 3=RZ */
#define A64_FPCR_FZ    (1u << 24)
#define A64_FPCR_DN    (1u << 25)
#define A64_FPCR_AHP   (1u << 26)
/* bits graváveis do FPCR no Apple Silicon. IDE/IXE/UFE/OFE/DZE/IOE são RES0 nesse hardware: o mOS
 * também os descarta (sem SIGFPE). Os apps usam os sticky flags do FPSR. Decisão em docs/sinais.md. */
#define A64_FPCR_MASK  (A64_FPCR_FZ16 | (3u << A64_FPCR_RMODE_SHIFT) | A64_FPCR_FZ | A64_FPCR_DN | A64_FPCR_AHP)

/* FPSR (exceções acumuladas + QC) */
#define A64_FPSR_IOC (1u << 0)
#define A64_FPSR_DZC (1u << 1)
#define A64_FPSR_OFC (1u << 2)
#define A64_FPSR_UFC (1u << 3)
#define A64_FPSR_IXC (1u << 4)
#define A64_FPSR_IDC (1u << 7)
#define A64_FPSR_QC  (1u << 27)
#define A64_FPSR_MASK (A64_FPSR_IOC | A64_FPSR_DZC | A64_FPSR_OFC | A64_FPSR_UFC | A64_FPSR_IXC | A64_FPSR_IDC | A64_FPSR_QC | 0xF0000000u)

/* bits extras do PSTATE visíveis em EL0 (campo pstate) */
#define A64_PS_DIT   (1u << 24)     /* FEAT_DIT (MSR DIT) — sem efeito na tradução, só guardado */
#define A64_PS_SSBS  (1u << 12)     /* FEAT_SSBS (MSR SSBS) — idem */
#define A64_PS_TCO   (1u << 25)     /* FEAT_MTE: não anunciado */
#define A64_PS_BTYPE_SHIFT 10       /* FEAT_BTI: BTYPE (2 bits); guardado, sem imposição */

typedef struct a64_cpu {
    /* x[0..30] = X0..X30; x[31] = SP. O registrador 31 é ZR ou SP conforme a instrução:
     * use a64_xr()/a64_xw() (ZR) e a64_xsp() (SP). */
    a64_u64 x[32];
    a64_u64 pc;                 /* endereço da instrução em execução (ver contrato de execução) */
    a64_u32 nzcv;               /* só os bits 31..28 */
    a64_u32 fpcr;
    a64_u32 fpsr;
    a64_u32 pstate;             /* A64_PS_* */
    a64_vreg v[32];

    a64_u64 tpidr_el0;          /* TPIDR_EL0: livre para o guest */
    a64_u64 tpidrro_el0;        /* TPIDRRO_EL0: no Darwin aponta para o TSD da thread (o runtime põe) */

    /* monitor exclusivo (LDXR/STXR, LDAXP/STLXP). A época fica no gate (a64_xmon.h);
     * estes campos espelham a reserva para depuração e para o caminho que ainda os lê. */
    a64_u64 excl_addr;
    a64_u64 excl_val[2];
    a64_u32 excl_size;          /* bytes acessados (1,2,4,8,16 ou 32 para pares de 128) */
    a64_u32 excl_valid;

    /* motivo da última saída do motor (A64_EXIT_*) e informação associada */
    a64_u32 exit_reason;
    a64_u32 exit_insn;          /* palavra da instrução que causou a saída */
    a64_u64 exit_info;          /* SVC/HVC/BRK/HLT: imm16; FAULT: endereço */

    void *host;                 /* privado do runtime/harness */
    void *jit;                  /* privado do JIT */
    a64_u64 reserved[16];       /* [8] geração de preempção, [9] slot+1, [10] época do LDXR, [11] gate;
                                 * [12..15] do JIT; [15] = pedido de parada (a64_request_stop) */
} a64_cpu;

/* offsets fixos (o JIT gera código com eles) */
_Static_assert(offsetof(a64_cpu, x) == 0, "layout");
_Static_assert(offsetof(a64_cpu, pc) == 256, "layout");
_Static_assert(offsetof(a64_cpu, nzcv) == 264, "layout");
_Static_assert(offsetof(a64_cpu, v) % 16 == 0, "layout");

static inline a64_u64 a64_xr(const a64_cpu *c, unsigned r) { return r == 31 ? 0 : c->x[r]; }
static inline a64_u32 a64_wr(const a64_cpu *c, unsigned r) { return r == 31 ? 0 : (a64_u32)c->x[r]; }
static inline void a64_xw(a64_cpu *c, unsigned r, a64_u64 v) { if (r != 31) c->x[r] = v; }
static inline void a64_ww(a64_cpu *c, unsigned r, a64_u32 v) { if (r != 31) c->x[r] = v; } /* zera 63:32 */
static inline a64_u64 *a64_xsp(a64_cpu *c, unsigned r) { return &c->x[r]; }           /* 31 = SP */

/* ---------------------------------------------------------------------------------------------
 * Contrato de execução (vale para o interpretador e para o JIT)
 *
 * - c->pc é o endereço da instrução corrente. Uma função de execução que não desvia devolve
 *   A64_NEXT e quem chamou soma 4. Uma que desvia grava o alvo em c->pc e devolve A64_BRANCH.
 * - Endereços do guest são endereços do host (mapeamento 1:1; o guest roda no mesmo espaço).
 *   Acessos de memória são feitos direto pelo ponteiro (helpers a64_mem.h). Uma falha de página
 *   vira exceção do host; o runtime precisa poder reiniciar a instrução. Por isso:
 *     NENHUMA instrução altera estado arquitetural antes do seu último acesso que possa falhar.
 *   Exceção permitida pela arquitetura: instruções com vários acessos (STP, ST1..ST4, LDP com
 *   escrita parcial nunca visível) podem ter feito parte das escritas na memória, mas registradores
 *   (incluindo writeback de base) só mudam depois de todos os acessos.
 * - Saídas: SVC devolve A64_EXIT_SVC com c->pc já na instrução SEGUINTE (como ELR_EL1).
 *   BRK/HLT/UDF/instrução não alocada devolvem o código com c->pc NA instrução.
 * - Instruções UNDEFINED, não alocadas, ou de recurso não anunciado → A64_EXIT_UDF. Nunca
 *   "ignorar" uma instrução desconhecida.
 * ------------------------------------------------------------------------------------------- */

enum {
    A64_NEXT = 0,          /* executou; pc += 4 */
    A64_BRANCH = 1,        /* executou; c->pc já é o próximo */
    A64_EXIT_SVC = 2,      /* syscall; exit_info = imm16; pc = próxima */
    A64_EXIT_BRK = 3,      /* BRK #imm → SIGTRAP; exit_info = imm16 */
    A64_EXIT_UDF = 4,      /* instrução indefinida → SIGILL */
    A64_EXIT_HLT = 5,      /* HLT #imm (sem depurador externo → SIGILL no runtime) */
    A64_EXIT_FAULT = 6,    /* falha de memória detectada pelo próprio motor; exit_info = endereço */
    A64_EXIT_ICACHE = 7,   /* IC IVAU/IC IALLU executado: exit_info = endereço; o laço do motor
                              invalida a tradução e continua (o interpretador chama o host e segue) */
    A64_EXIT_YIELD = 8,    /* WFE/WFI/YIELD: dica para o runtime ceder a CPU; pc = próxima */
    A64_EXIT_STOP = 9,     /* pedido externo (sinal pendente, a64_request_stop) */
    A64_EXIT_HVC = 10,     /* HVC/SMC em EL0 são UNDEFINED; não usar fora do harness */
};

/* ---------------------------------------------------------------------------------------------
 * Interpretador de referência (userland/a64/interp*.c) — semântica completa, é o "oráculo"
 * interno contra o qual o JIT é testado. Também é o fallback do JIT para qualquer instrução.
 * ------------------------------------------------------------------------------------------- */

/* executa UMA instrução em c->pc (lê a palavra da memória); atualiza pc conforme o retorno.
 * Devolve A64_NEXT/A64_BRANCH (pc já atualizado nos dois casos) ou A64_EXIT_*. */
int a64_interp_step(a64_cpu *c);
/* executa a instrução 'insn' como se estivesse em c->pc, sem atualizar pc (o chamador faz). */
int a64_interp_exec(a64_cpu *c, a64_u32 insn);
/* roda até uma saída ou até 'budget' instruções (0 = sem limite); devolve A64_EXIT_* ou A64_EXIT_STOP */
int a64_interp_run(a64_cpu *c, a64_u64 budget);

/* grupos de decodificação (ARM ARM C4.1, op0 = bits 28:25). Cada um em seu arquivo/frente. */
int a64_exec_dpimm(a64_cpu *c, a64_u32 insn);     /* 100x: dados com imediato          — interp_int.c */
int a64_exec_branch(a64_cpu *c, a64_u32 insn);    /* 101x: desvios, exceções, sistema  — interp_int.c */
int a64_exec_dpreg(a64_cpu *c, a64_u32 insn);     /* x101: dados com registrador       — interp_int.c */
int a64_exec_ldst(a64_cpu *c, a64_u32 insn);      /* x1x0: loads/stores inteiros       — interp_int.c
                                                     (repassa os com V=1 para a64_exec_ldst_simd) */
int a64_exec_ldst_simd(a64_cpu *c, a64_u32 insn); /* loads/stores de SIMD&FP (V=1)     — interp_simd.c */
int a64_exec_fp(a64_cpu *c, a64_u32 insn);        /* x111 com bit 28=1, bit 30=0, bit 24=0... ponto
                                                     flutuante escalar (C4.1.96 "Floating-point"),
                                                     conversões FP<->inteiro/fixo   — interp_fp.c */
int a64_exec_simd(a64_cpu *c, a64_u32 insn);      /* resto de x111: AdvSIMD vetorial e escalar,
                                                     criptografia                   — interp_simd.c */

/* ---------------------------------------------------------------------------------------------
 * Primitivas de ponto flutuante (interp_fp.c / a64_fp.c — frente FP). Semântica exata do ARM:
 * arredondamento do FPCR.RMode (ou 'rm' explícito), FZ/FZ16/DN, NaN padrão 0x7FC00000/
 * 0x7FF8000000000000/0x7E00, propagação de NaN do ARM (SNaN antes de QNaN, operando 1 antes
 * do 2), flags acumuladas no c->fpsr. 'es' = tamanho do elemento em bytes: 2 (half), 4, 8.
 * Valores entram e saem como bits (zero-extended). A frente SIMD usa estas para os lanes FP.
 * Variante *_fpcr: usa um FPCR explícito (instruções "standard FPSCR" do AdvSIMD não existem
 * em A64; todas usam c->fpcr — o parâmetro existe para FRECPE/FRSQRTE e testes).
 * ------------------------------------------------------------------------------------------- */

enum { A64_RM_TIEEVEN = 0, A64_RM_POSINF = 1, A64_RM_NEGINF = 2, A64_RM_ZERO = 3,
       A64_RM_TIEAWAY = 4, A64_RM_ODD = 5 /* FCVTXN */, A64_RM_FPCR = 8 /* usar FPCR.RMode */ };

a64_u64 a64_fp_add(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
a64_u64 a64_fp_sub(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
a64_u64 a64_fp_mul(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
a64_u64 a64_fp_mulx(a64_cpu *c, a64_u64 a, a64_u64 b, int es);   /* FMULX: 0*inf = ±2 */
a64_u64 a64_fp_div(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
a64_u64 a64_fp_sqrt(a64_cpu *c, a64_u64 a, int es);
/* fused: resultado = addend + m1*m2 com um arredondamento (FMADD/FMLA). Negações ficam a cargo
 * de quem chama (FMSUB/FNMADD etc. negam operandos ANTES, como o pseudocódigo do ARM). */
a64_u64 a64_fp_muladd(a64_cpu *c, a64_u64 addend, a64_u64 m1, a64_u64 m2, int es);
a64_u64 a64_fp_max(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
a64_u64 a64_fp_min(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
a64_u64 a64_fp_maxnum(a64_cpu *c, a64_u64 a, a64_u64 b, int es); /* FMAXNM */
a64_u64 a64_fp_minnum(a64_cpu *c, a64_u64 a, a64_u64 b, int es); /* FMINNM */
a64_u64 a64_fp_abd(a64_cpu *c, a64_u64 a, a64_u64 b, int es);    /* FABD = abs(a-b) */
/* comparações: FCMP/FCMPE → nzcv (bits 31..28); 'signal' = 1 sinaliza IOC também com QNaN */
a64_u32 a64_fp_compare(a64_cpu *c, a64_u64 a, a64_u64 b, int es, int signal);
/* comparações de máscara do AdvSIMD (FCMEQ/FCMGE/FCMGT/FACGE/FACGT): devolve 1 = verdadeiro */
int a64_fp_cmp_eq(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
int a64_fp_cmp_ge(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
int a64_fp_cmp_gt(a64_cpu *c, a64_u64 a, a64_u64 b, int es);
/* estimativas e passos */
a64_u64 a64_fp_recpe(a64_cpu *c, a64_u64 a, int es);
a64_u64 a64_fp_rsqrte(a64_cpu *c, a64_u64 a, int es);
a64_u64 a64_fp_recps(a64_cpu *c, a64_u64 a, a64_u64 b, int es);  /* 2 - a*b (fused) */
a64_u64 a64_fp_rsqrts(a64_cpu *c, a64_u64 a, a64_u64 b, int es); /* (3 - a*b)/2 (fused) */
a64_u64 a64_fp_recpx(a64_cpu *c, a64_u64 a, int es);
a64_u32 a64_recpe_u32(a64_u32 a);                                /* URECPE */
a64_u32 a64_rsqrte_u32(a64_u32 a);                               /* URSQRTE */
/* arredondamento para inteiro em FP (FRINT*): rm = A64_RM_*; exact = 1 sinaliza IXC (FRINTX) */
a64_u64 a64_fp_roundint(a64_cpu *c, a64_u64 a, int es, int rm, int exact);
/* FRINT32Z/X, FRINT64Z/X (FEAT_FRINTTS): intbits = 32 ou 64 */
a64_u64 a64_fp_roundint_n(a64_cpu *c, a64_u64 a, int es, int rm, int intbits);
/* conversões entre formatos FP (h↔s↔d), com AHP para half; rm para FCVTXN = A64_RM_ODD */
a64_u64 a64_fp_convert(a64_cpu *c, a64_u64 a, int from_es, int to_es, int rm);
/* FP → inteiro (saturante, como o ARM: NaN → 0, IOC). fbits = bits de fração (fixo); ibits = 32/64
 * (16 para as formas half/escalar do AdvSIMD); is_unsigned. Resultado zero-extended. */
a64_u64 a64_fp_to_int(a64_cpu *c, a64_u64 a, int es, int fbits, int ibits, int is_unsigned, int rm);
/* inteiro → FP. 'v' tem ibits significativos (sign-extend se !is_unsigned). */
a64_u64 a64_int_to_fp(a64_cpu *c, a64_u64 v, int ibits, int is_unsigned, int fbits, int es, int rm);
/* FJCVTZS (FEAT_JSCVT): devolve o resultado de 32 bits e grava Z em *z (1 = exato e sem overflow) */
a64_u32 a64_fp_jcvtzs(a64_cpu *c, a64_u64 d, int *z);
/* BFloat16 (FEAT_BF16): BFCVT (s→bf16) com arredondamento do FPCR */
a64_u16 a64_fp_to_bf16(a64_cpu *c, a64_u32 s);
/* constantes */
a64_u64 a64_fp_default_nan(int es);
a64_u64 a64_fp_expand_imm8(a64_u8 imm8, int es);                 /* VFPExpandImm (FMOV imm) */

/* ---------------------------------------------------------------------------------------------
 * Serviços que o hospedeiro (runtime do mOS ou harness do Windows) fornece
 * ------------------------------------------------------------------------------------------- */

a64_u64 a64_host_cntvct(void);                 /* CNTVCT_EL0 / CNTPCT_EL0, 24 MHz como no Apple Silicon */
#define A64_CNTFRQ 24000000u
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len); /* IC IVAU / sys_icache_invalidate */
void a64_host_yield(a64_cpu *c);               /* WFE/WFI/YIELD dentro do interpretador */

/* ---------------------------------------------------------------------------------------------
 * Motor (escolha entre interpretador e JIT) — userland/a64/engine.c
 * ------------------------------------------------------------------------------------------- */

enum { A64_ENGINE_INTERP = 0, A64_ENGINE_JIT = 1 };
/* roda a thread até uma saída que precise do runtime (SVC, BRK, UDF, HLT, FAULT, STOP).
 * IC IVAU e YIELD são tratados dentro. */
int a64_run(a64_cpu *c, int engine);
void a64_request_stop(a64_cpu *c);             /* assíncrono (sinal do host): faz a64_run sair com STOP */
void a64_invalidate(a64_u64 addr, a64_u64 len);/* descarta traduções do intervalo (todas as threads) */

/* JIT (userland/a64/jit*.c — frente JIT) */
int  a64_jit_run(a64_cpu *c);
void a64_jit_invalidate(a64_u64 addr, a64_u64 len);
/* Recuperação de estado numa falha do host dentro de código traduzido: dado o RIP do host e o
 * contexto (registradores do host em ordem rax..r15, rip, rflags, xmm0..15), reconstrói o estado
 * do guest em 'c' (pc = instrução que falhou, registradores consistentes com o contrato acima).
 * Devolve true se o RIP pertence ao cache do JIT. */
typedef struct a64_hostctx {
    a64_u64 gpr[16];             /* rax rcx rdx rbx rsp rbp rsi rdi r8..r15 */
    a64_u64 rip, rflags;
    a64_vreg xmm[16];
} a64_hostctx;
bool a64_jit_recover(a64_cpu *c, const a64_hostctx *h);

/* ---------------------------------------------------------------------------------------------
 * Recursos anunciados (hw.optional.arm.FEAT_* e hw.optional.* no sysctl do guest).
 * "100% completo" = TODA instrução A64 de EL0 destes recursos implementada e verificada
 * contra o oráculo (QEMU), e toda instrução fora deles → A64_EXIT_UDF.
 * A lista é a do Apple M1 + BF16/I8MM do M2. SVE/SME/MTE/AFP/WFxT/... não são anunciados.
 * ------------------------------------------------------------------------------------------- */
#define A64_FEATURES(X) \
    X(AdvSIMD) X(FP) X(FP16) X(FHM) X(AES) X(PMULL) X(SHA1) X(SHA256) X(SHA512) X(SHA3) \
    X(CRC32) X(LSE) X(LSE2) X(RDM) X(DotProd) X(FCMA) X(JSCVT) X(LRCPC) X(LRCPC2) \
    X(FlagM) X(FlagM2) X(FRINTTS) X(SB) X(SSBS) X(DPB) X(DPB2) X(DIT) X(PAuth) X(BTI) \
    X(BF16) X(I8MM) X(ECV) X(CSV2) X(CSV3)

/* ---------------------------------------------------------------------------------------------
 * JIT: serviços extras do hospedeiro e ganchos do runtime (frente JIT; detalhes no doc, seção JIT)
 *
 * Campos do a64_cpu usados pelo JIT: reserved[15] (32 bits baixos = pedido de parada, como no
 * interpretador; 32 bits altos = pedido interno do JIT), reserved[14] (NZCV em formato x86 enquanto
 * o JIT roda: c->nzcv só vale fora de a64_jit_run, dentro das chamadas ao interpretador e depois de
 * a64_jit_recover), reserved[13] (cache de saltos da thread), reserved[12] (reservado ao JIT), jit.
 * reserved[8..11] são do monitor exclusivo (a64_xmon.h), não do cache de registradores.
 *
 * No Windows (harness) o JIT usa VirtualAlloc/VirtualProtect e um VEH próprio; no runtime do mOS o
 * hospedeiro fornece as duas funções abaixo.
 * ------------------------------------------------------------------------------------------- */
/* memória leitura+escrita+execução (o JIT pede poucas regiões grandes no início) */
void *a64_host_jit_alloc(a64_u64 len);
/* página de 4 KiB do guest que tem código traduzido: protect=1 tira a escrita se a página era
 * gravável (devolve 1) ou devolve 0; protect=0 devolve a escrita; protect=2 só consulta (1 =
 * gravável agora). Um stub que devolve 0 desliga a rede de segurança (sobra IC IVAU). */
int a64_host_jit_wprotect(a64_u64 page, int protect);
/* handler de SIGSEGV do runtime: chamar primeiro. true = era escrita numa página que o JIT
 * protegeu; basta retomar o contexto (a escrita é refeita). */
bool a64_jit_write_fault(a64_u64 addr);
/* reescreve o contexto de falha do host para que, ao retomá-lo, a64_jit_run devolva 'code'
 * (A64_NEXT = recomeçar em c->pc). Usar depois de a64_jit_recover. false = não estava no JIT. */
bool a64_jit_unwind(a64_cpu *c, a64_hostctx *h, int code);
/* thread do guest terminou: libera o estado por thread do JIT */
void a64_jit_thread_exit(a64_cpu *c);
/* estatísticas: blocos traduzidos, esvaziamentos, bytes de código em uso */
void a64_jit_stats(a64_u64 *ntrans, a64_u64 *nflush, a64_u64 *code_bytes);
/* depois de a64_jit_recover, se o runtime abandonar a64_run por longjmp (em vez de
 * a64_jit_unwind): limpa o estado por thread do JIT */
void a64_jit_abort(a64_cpu *c);
/* filho de fork (só a thread que chamou fork sobrevive): zera as travas, solta o estado das
 * outras threads do JIT e aplica invalidações já pendentes. O cache copiado pelo COW permanece;
 * o kernel mantém PTE_JWP e entrega SIGBUS na primeira escrita de cada processo. */
void a64_jit_fork_child(a64_cpu *self);
/* sem trava: true se pode haver código traduzido em [addr, addr+len) (conservador). O runtime usa
 * para só chamar a64_invalidate depois de read()/pread() quando precisa. */
bool a64_jit_has_code(a64_u64 addr, a64_u64 len);
/* estatística das instruções que o JIT executa pelo interpretador: liga/desliga e lista as 'max'
 * classes mais frequentes (chave = palavra sem os campos de registrador 20:16 e 9:0) */
void a64_jit_fbstat(int on);
void a64_jit_fbstat_dump(void (*out)(a64_u32 key, a64_u64 count, void *arg), void *arg, int max);
/* Se [addr, addr+len) é uma das duas vistas de um vm_remap (as mesmas páginas físicas), devolve a
 * outra em [*lo, *hi). Sem apelido, 0. O motor tem uma definição fraca; o a64rt substitui. */
int a64_jit_alias_span(a64_u64 addr, a64_u64 len, a64_u64 *lo, a64_u64 *hi);

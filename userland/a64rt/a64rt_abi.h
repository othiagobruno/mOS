/*
 * mOS — tradutor arm64: ABI entre o kernel, o runtime a64rt.mos (x86_64) e o libSystem arm64 (guest).
 * Frente RUNTIME (docs/tradutor-arm64.md). C freestanding: incluído pelo kernel (macho.c), pelo runtime
 * (userland/a64rt) e pelo libSystem compilado para arm64 (frente LIBSYS). Só acrescentar.
 *
 * Processo arm64 no mOS:
 *   0x400000100000  libSystem arm64 (ELF AArch64 static-pie, R_AARCH64_RELATIVE; /Sistema/Biblioteca/a64/libSystem.mos)
 *   0x400001000000  imagens Mach-O arm64 (executável + dylibs do app), stubs, heap
 *   USER_STACK_TOP  pilha do guest (argv/envp/apple + bloco cx_start_t, igual ao x86)
 *   A64RT_BASE      a64rt.mos (ELF x86_64 static-pie; /Sistema/Biblioteca/a64/a64rt.mos) + pilha do host
 * A thread começa no runtime (x86_64) com rdi = a64rt_boot_t*; o runtime cria o a64_cpu e entra no
 * e_entry do libSystem arm64 (cx_start) com x0 = cx_start_t*, sp = 16n, lr = 0.
 */
#pragma once

#define A64RT_BASE        0x0000400041000000ULL   /* USER_STACK_TOP + 16 MiB: faixa livre até o mmap (4 GiB) */
#define A64RT_SPAN_MAX    0x0000000003000000ULL   /* runtime + pilha do host da thread principal (48 MiB) */
#define A64RT_DIR         "/Sistema/Biblioteca/a64"
#define A64RT_PATH        A64RT_DIR "/a64rt.mos"
#define A64RT_LIBSYSTEM   A64RT_DIR "/libSystem.mos"
#define A64RT_BOOT_MAGIC  0x544F4F4234364161ULL  /* "aA64BOOT" */

/* pilha do host de cada thread do runtime: alinhada ao próprio tamanho, a64rt_thread_t na base
 * (o tratador de sinal do host acha a thread com rsp & ~(A64RT_HSTACK-1)) */
#define A64RT_HSTACK      0x100000ULL

typedef struct {
    unsigned long long magic, version;      /* A64RT_BOOT_MAGIC, 1 */
    unsigned long long guest_entry;         /* e_entry do libSystem arm64 (cx_start) já relocado */
    unsigned long long guest_sp;            /* sp inicial do guest (16n) */
    unsigned long long guest_x0;            /* cx_start_t* (mesmo bloco do x86: userland/libsystem/sys.h) */
    unsigned long long ls_base, ls_size;    /* libSystem arm64 */
    unsigned long long rt_base, rt_size;    /* runtime (texto + dados) */
    unsigned long long hstack_lo;           /* pilha do host da thread principal (A64RT_HSTACK, alinhada) */
    unsigned long long flags;               /* A64RT_BOOT_* */
    unsigned long long exe_hdr;             /* cabeçalho Mach-O do executável */
} a64rt_boot_t;
#define A64RT_BOOT_FORCED 1                 /* o universal tinha x86_64, mas pediram arm64 (MOS_ARCH=arm64) */
#define A64RT_BOOT_DEBUG  2                 /* diagnóstico do runtime (MOS_A64_DEBUG no harness; A64RT_DIR/depurar no mOS) */
#define A64RT_BOOT_INTERP 4                 /* força o interpretador (MOS_A64_ENGINE=interp no harness; A64RT_DIR/interpretador) */
#define A64RT_BOOT_JIT    8                 /* força o JIT (MOS_A64_ENGINE=jit no harness) */
#define A64RT_BOOT_TRACE  16                /* registra cada syscall do guest (MOS_A64_DEBUG=2 no harness) */

/* ---- syscalls do guest -------------------------------------------------------------------------
 * svc #0x4d4f: x8 = número de kernel/include/syscall.h, x0..x5 = argumentos, x0 = resultado (-errno).
 * O runtime repassa 1:1 ao int 0x80 (mesmas structs: LP64 nos dois lados), exceto:
 *   SYS_THREAD_CREATE (entry, sp, arg)  nova thread guest: pc = entry, x0 = arg, sp = sp, lr = 0, TPIDRRO = 0
 *   SYS_SIG / SYS_SIGACTION / SYS_SIGRETURN  sinais no layout arm64 (abaixo); MASK/PENDING/TKILL/WAIT repassados
 *   SYS_THREADCTL MOS_THR_GETSTATE      devolve a64_darwin_thread_state_t (272 bytes) em vez do x86
 *   SYS_EXIT / SYS_THREAD_EXIT          repassados (o runtime solta a thread do host)
 * svc #0x80 (Darwin cru, x16 = número, x0..x5, erro = bit C e x0 = errno): exit 1, read 3, write 4,
 * getpid 20, kill 37, x16 = 0x80000000 (machdep) com x3 = 2 (thread_set_tsd_base) ou 3; o resto vira SIGSYS.
 */
#define A64RT_SVC_MOS     0x4d4f
#define A64RT_SVC_DARWIN  0x80

#define A64RT_SYS_SET_TSD       0x4D00  /* (base) -> 0      TPIDRRO_EL0 da thread atual (o TSD do pthread) */
#define A64RT_SYS_ICACHE_INVAL  0x4D01  /* (addr, len) -> 0 sys_icache_invalidate (IC IVAU também serve) */
#define A64RT_SYS_JIT_WP        0x4D02  /* (habilita) -> 0  pthread_jit_write_protect_np (memória do mOS é RWX) */
#define A64RT_SYS_INFO          0x4D03  /* (A64RT_INFO_*) -> valor */
#define A64RT_SYS_GET_TSD       0x4D04  /* () -> TPIDRRO_EL0 */
enum { A64RT_INFO_ENGINE = 1,           /* 0 interpretador, 1 JIT */
       A64RT_INFO_VERSION = 2 };

/* ---- sinais (Darwin arm64) ----------------------------------------------------------------------
 * MOS_SIGOP_ACTION com tramp != 0: o runtime chama tramp(handler, estilo, sinal, siginfo*, ucontext*, 0) em
 * x0..x5 (estilo 1 = UC_TRAD, 30 = UC_FLAVOR), sp = quadro (16n), lr = 0. O tramp chama o handler e volta por
 * MOS_SIGOP_RETURN(ucontext*). Sem tramp (SYS_SIGACTION antigo): handler(sinal, siginfo*, ucontext*) com lr =
 * restorer, que chama SYS_SIGRETURN com sp no quadro. siginfo = mos_siginfo_t (104 bytes, igual ao Darwin);
 * números de sinal: os do kernel (o libSystem já converte BUS/SYS/USR1... como no x86). Red zone de 128 bytes.
 * Quem muda __ss no handler retoma noutro ponto; __ns (v, fpsr, fpcr) também é restaurado. */
typedef struct { unsigned long long far; unsigned int esr, exception; } a64_darwin_exc_state_t;   /* 16 */
typedef struct {
    unsigned long long x[29], fp, lr, sp, pc;
    unsigned int cpsr, pad;                                                                        /* 272 */
} a64_darwin_thread_state_t;
typedef struct {
    unsigned long long v[32][2];
    unsigned int fpsr, fpcr;
    unsigned long long pad;                                                                        /* 528 */
} __attribute__((aligned(16))) a64_darwin_neon_state_t;
typedef struct {
    a64_darwin_exc_state_t es;
    a64_darwin_thread_state_t ss;
    a64_darwin_neon_state_t ns;
} __attribute__((aligned(16))) a64_darwin_mcontext_t;                                              /* 816 */
typedef struct {
    int uc_onstack; unsigned int uc_sigmask;
    unsigned long long ss_sp, ss_size; int ss_flags, ss_pad;
    unsigned long long uc_link, uc_mcsize, uc_mcontext;
} a64_darwin_ucontext_t;                                                                           /* 56 */
_Static_assert(sizeof(a64_darwin_thread_state_t) == 272, "arm_thread_state64");
_Static_assert(sizeof(a64_darwin_neon_state_t) == 528, "arm_neon_state64");
_Static_assert(sizeof(a64_darwin_mcontext_t) == 816, "mcontext64");
_Static_assert(sizeof(a64_darwin_ucontext_t) == 56, "ucontext");

/* ESR_EL1 entregue em __es.esr (classes do ARM ARM D17.2.37) */
#define A64_ESR_EC_UNKNOWN  0x00     /* UDF / não alocada → SIGILL */
#define A64_ESR_EC_SVC      0x15
#define A64_ESR_EC_IABT_LO  0x20     /* falta de instrução → SIGSEGV/SIGBUS */
#define A64_ESR_EC_PCALIGN  0x22
#define A64_ESR_EC_DABT_LO  0x24     /* falta de dados; ISS.WnR = bit 6 */
#define A64_ESR_EC_SPALIGN  0x26
#define A64_ESR_EC_BRK      0x3C     /* BRK #imm → SIGTRAP; ISS = imm16 */
#define A64_ESR_IL          (1u << 25)
#define A64_ESR_WNR         (1u << 6)
#define A64_ESR_DFSC_TRANS  0x07     /* falta de tradução, nível 3 */
#define A64_ESR_DFSC_PERM   0x0F     /* falta de permissão, nível 3 */

/* ---- sysctl coerente com A64_FEATURES (userland/a64/a64.h) para o libSystem arm64 ------------------
 * Uso no libSystem arm64: A64RT_SYSCTL_INTS(X) com X(nome, valor) e A64RT_SYSCTL_STRS(X). Os FEAT_* vêm
 * todos de A64_FEATURES: inclua a64.h e gere "hw.optional.arm.FEAT_<f>" = 1 com
 *   #define F(f) X(A64RT_FEAT_NAME(f), 1)
 *   A64_FEATURES(F)  */
#define A64RT_CPUTYPE      0x0100000C   /* CPU_TYPE_ARM64 */
#define A64RT_CPUSUBTYPE   2            /* CPU_SUBTYPE_ARM64E (o que o M1/M2 informam) */
#define A64RT_CPUFAMILY    0xDA33D83Du  /* CPUFAMILY_ARM_BLIZZARD_AVALANCHE (M2: o conjunto de A64_FEATURES) */
#define A64RT_FEAT_NAME(f) "hw.optional.arm.FEAT_" #f
#define A64RT_SYSCTL_INTS(X) \
    X("hw.cputype", A64RT_CPUTYPE) X("hw.cpusubtype", A64RT_CPUSUBTYPE) X("hw.cpufamily", (int)A64RT_CPUFAMILY) \
    X("hw.cpusubfamily", 2) X("hw.cputhreadtype", 0) X("hw.byteorder", 1234) \
    X("hw.optional.arm64", 1) X("hw.optional.x86_64", 0) X("hw.optional.sse2", 0) X("hw.optional.sse3", 0) \
    X("hw.optional.avx512f", 0) X("sysctl.proc_translated", 0) X("sysctl.proc_native", 1) \
    X("hw.optional.floatingpoint", 1) X("hw.optional.neon", 1) X("hw.optional.neon_hpfp", 1) X("hw.optional.neon_fp16", 1) \
    X("hw.optional.AdvSIMD", 1) X("hw.optional.AdvSIMD_HPFPCvt", 1) X("hw.optional.ucnormal_mem", 1) \
    X("hw.optional.armv8_1_atomics", 1) X("hw.optional.armv8_crc32", 1) X("hw.optional.armv8_2_fhm", 1) \
    X("hw.optional.armv8_2_sha512", 1) X("hw.optional.armv8_2_sha3", 1) X("hw.optional.armv8_3_compnum", 1) \
    X("hw.optional.armv8_gpi", 0) X("hw.optional.arm.FEAT_SME", 0) X("hw.optional.arm.FEAT_SME2", 0) \
    X("hw.optional.arm.FEAT_AFP", 0) X("hw.optional.arm.FEAT_WFxT", 0) X("hw.optional.arm.FEAT_MTE", 0) \
    X("hw.optional.arm.FEAT_RPRES", 0) X("hw.optional.arm.FEAT_EBF16", 0) X("hw.optional.arm.FEAT_SPECRES", 0) \
    X("hw.optional.arm.FEAT_PACIMP", 0) X("hw.optional.arm.FEAT_FPAC", 0) X("hw.optional.arm.FEAT_FPACCOMBINE", 0) \
    X("hw.optional.arm.FEAT_CSSC", 0) X("hw.optional.arm.FEAT_HBC", 0) X("hw.optional.arm.FEAT_LSE128", 0) \
    X("hw.optional.arm.FEAT_PAuth2", 0) X("hw.optional.arm.FEAT_EPAC", 0) \
    X("hw.optional.arm.AdvSIMD_HPFPCvt", 1) X("hw.optional.arm.SME_F32F32", 0) \
    X("hw.cachelinesize", 128) X("hw.l1icachesize", 131072) X("hw.l1dcachesize", 65536) X("hw.l2cachesize", 4194304) \
    X("hw.pagesize", 16384) X("hw.pagesize32", 16384) X("vm.pagesize", 16384) X("hw.tbfrequency", 24000000) \
    X("hw.optional.arm.caps", 0)
#define A64RT_SYSCTL_STRS(X) \
    X("hw.machine", "arm64") X("hw.model", "Mac14,2") X("machdep.cpu.brand_string", "Apple M2 (mOS)") \
    X("kern.version_arch", "ARM64")
/* uname(3) do processo arm64: machine = "arm64"; arch(1) e NXGetLocalArchInfo: "arm64" */
#define A64RT_UNAME_MACHINE "arm64"
/* hw.pagesize = 16 KiB como no Apple Silicon: o mmap do libSystem arm64 alinha a 16 KiB (o kernel aloca
 * em 4 KiB; o Go/Swift/V8 do arm64 contam com páginas de 16 KiB). */

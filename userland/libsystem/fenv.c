/*
 * mOS libSystem — <fenv.h> com os tipos e constantes da Apple.
 *
 * x86_64: fenv_t = {u16 __control (x87), u16 __status (x87), u32 __mxcsr, char __reserved[8]}, fexcept_t = u16.
 *   Exceções FE_INVALID 0x01, FE_DENORMALOPERAND 0x02, FE_DIVBYZERO 0x04, FE_OVERFLOW 0x08, FE_UNDERFLOW 0x10,
 *   FE_INEXACT 0x20 (os mesmos bits no status do x87 e no MXCSR); arredondamento nos bits 10-11 do x87 (0x000, 0x400,
 *   0x800, 0xC00) e 13-14 do MXCSR (o mesmo valor << 3). As duas unidades são lidas e gravadas juntas: o código SSE e
 *   o long double (x87) enxergam o mesmo ambiente.
 * arm64: fenv_t = {u64 __fpsr, u64 __fpcr}. Exceções nos bits do FPSR (IOC 0x01, DZC 0x02, OFC 0x04, UFC 0x08,
 *   IXC 0x10, IDC 0x80 = FE_FLUSHTOZERO); arredondamento no RMode do FPCR (bits 22-23); traps nos bits 8-12 e 15.
 */
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned short fexcept_t;

#if defined(__aarch64__)
typedef struct { u64 __fpsr, __fpcr; } fenv_t;
#define EXC_ALL 0x9F
#define RND_MASK 0x00C00000ull
#define TRAP_MASK 0x9F00ull

static u64 rd_fpsr(void) { u64 v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return v; }
static void wr_fpsr(u64 v) { __asm__ volatile("msr fpsr, %0" : : "r"(v)); }
static u64 rd_fpcr(void) { u64 v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
static void wr_fpcr(u64 v) { __asm__ volatile("msr fpcr, %0" : : "r"(v)); }

const fenv_t _FE_DFL_ENV = {0, 0};

int feclearexcept(int e) { wr_fpsr(rd_fpsr() & ~(u64)(e & EXC_ALL)); return 0; }
int feraiseexcept(int e) { wr_fpsr(rd_fpsr() | (u64)(e & EXC_ALL)); return 0; }
int fetestexcept(int e) { return (int)(rd_fpsr() & (u64)(e & EXC_ALL)); }
int fegetexceptflag(fexcept_t *f, int e) { *f = (fexcept_t)(rd_fpsr() & (u64)(e & EXC_ALL)); return 0; }
int fesetexceptflag(const fexcept_t *f, int e) {
    u64 m = (u64)(e & EXC_ALL);
    wr_fpsr((rd_fpsr() & ~m) | ((u64)*f & m));
    return 0;
}
int fegetround(void) { return (int)(rd_fpcr() & RND_MASK); }
int fesetround(int r) {
    if ((u64)r & ~RND_MASK) return -1;
    wr_fpcr((rd_fpcr() & ~RND_MASK) | (u64)r);
    return 0;
}
int fegetenv(fenv_t *env) { env->__fpsr = rd_fpsr(); env->__fpcr = rd_fpcr(); return 0; }
int fesetenv(const fenv_t *env) { wr_fpcr(env->__fpcr); wr_fpsr(env->__fpsr); return 0; }
int feholdexcept(fenv_t *env) {
    fegetenv(env);
    wr_fpsr(env->__fpsr & ~(u64)EXC_ALL);
    wr_fpcr(env->__fpcr & ~TRAP_MASK);
    return 0;
}
#else
typedef struct { u16 __control, __status; u32 __mxcsr; char __reserved[8]; } fenv_t;
#define EXC_ALL 0x3F
#define RND_MASK 0x0C00

/* imagem de 28 bytes do fnstenv/fldenv em modo de 32 bits: controle, status e tag nos três primeiros dwords */
typedef struct { u32 control, status, tag, ip, cs, dp, ds; } x87env_t;

static u32 rd_mxcsr(void) { u32 v; __asm__ volatile("stmxcsr %0" : "=m"(v)); return v; }
static void wr_mxcsr(u32 v) { __asm__ volatile("ldmxcsr %0" : : "m"(v)); }
static u16 rd_x87_status(void) { u16 v; __asm__ volatile("fnstsw %0" : "=m"(v)); return v; }
static u16 rd_x87_control(void) { u16 v; __asm__ volatile("fnstcw %0" : "=m"(v)); return v; }
static void wr_x87_control(u16 v) { __asm__ volatile("fldcw %0" : : "m"(v)); }
/* troca os bits de exceção do status do x87 sem gerar trap (fnstenv mascara tudo; o fldenv devolve o controle) */
static void x87_set_status(u16 mask, u16 bits) {
    x87env_t e;
    __asm__ volatile("fnstenv %0" : "=m"(e));
    e.status = (e.status & ~(u32)mask) | (bits & mask);
    __asm__ volatile("fldenv %0" : : "m"(e));
}

const fenv_t _FE_DFL_ENV = {0x037F, 0x0000, 0x1F80, {0}};
/* o mesmo, com DAZ e FTZ (desnormais tratados como zero no SSE) */
const fenv_t _FE_DFL_DISABLE_SSE_DENORMS_ENV = {0x037F, 0x0000, 0x9FC0, {0}};

int feclearexcept(int e) {
    u16 m = (u16)(e & EXC_ALL);
    if (m == EXC_ALL) __asm__ volatile("fnclex");
    else x87_set_status(m, 0);
    wr_mxcsr(rd_mxcsr() & ~(u32)m);
    return 0;
}
int fetestexcept(int e) { return (int)((rd_x87_status() | rd_mxcsr()) & (u32)(e & EXC_ALL)); }
int fegetexceptflag(fexcept_t *f, int e) { *f = (fexcept_t)fetestexcept(e); return 0; }
int fesetexceptflag(const fexcept_t *f, int e) {
    u16 m = (u16)(e & EXC_ALL);
    x87_set_status(m, *f);
    wr_mxcsr((rd_mxcsr() & ~(u32)m) | (*f & m));
    return 0;
}
/* como na libm da Apple e do FreeBSD: grava as flags no x87 e o fwait entrega a exceção se ela estiver desmascarada */
int feraiseexcept(int e) {
    fexcept_t f = (fexcept_t)(e & EXC_ALL);
    x87_set_status(f, f);
    __asm__ volatile("fwait");
    return 0;
}
int fegetround(void) { return rd_x87_control() & RND_MASK; }
int fesetround(int r) {
    if (r & ~RND_MASK) return -1;
    wr_x87_control((u16)((rd_x87_control() & ~RND_MASK) | r));
    wr_mxcsr((rd_mxcsr() & ~(u32)(RND_MASK << 3)) | ((u32)r << 3));
    return 0;
}
int fegetenv(fenv_t *env) {
    x87env_t e;
    __asm__ volatile("fnstenv %0" : "=m"(e));
    __asm__ volatile("fldenv %0" : : "m"(e));     /* o fnstenv mascarou as exceções do x87: devolve o controle */
    env->__control = (u16)e.control;
    env->__status = (u16)e.status;
    env->__mxcsr = rd_mxcsr();
    for (int i = 0; i < 8; i++) env->__reserved[i] = 0;
    return 0;
}
int fesetenv(const fenv_t *env) {
    x87env_t e;
    __asm__ volatile("fnstenv %0" : "=m"(e));
    e.control = (e.control & 0xFFFF0000u) | env->__control;
    e.status = (e.status & 0xFFFF0000u) | env->__status;
    __asm__ volatile("fldenv %0" : : "m"(e));
    wr_mxcsr(env->__mxcsr);
    return 0;
}
int feholdexcept(fenv_t *env) {
    fegetenv(env);
    fenv_t h = *env;
    h.__control |= EXC_ALL;                          /* máscara de todas no x87 */
    h.__status &= (u16)~EXC_ALL;
    h.__mxcsr = (h.__mxcsr | (EXC_ALL << 7)) & ~(u32)EXC_ALL;
    fesetenv(&h);
    return 0;
}
#endif

int feupdateenv(const fenv_t *env) {
    int pend = fetestexcept(EXC_ALL);
    fesetenv(env);
    feraiseexcept(pend);
    return 0;
}

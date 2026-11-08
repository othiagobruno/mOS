/*
 * mOS — a64rt: runtime do tradutor arm64 → x86_64 (frente RUNTIME, docs/tradutor-arm64.md).
 * ELF x86_64 freestanding mapeado pelo kernel (kernel/compat/macho.c) em processos Mach-O arm64; roda o
 * motor (userland/a64: interpretador/JIT), faz a ponte das syscalls do guest para o int 0x80 do mOS e
 * cuida de threads, sinais (ucontext arm64 do Darwin) e invalidação de código.
 */
#pragma once
#include "../a64/a64.h"
#include "a64rt_abi.h"
#include "../../kernel/include/syscall.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

long rt_sys(long n, long a, long b, long c, long d, long e, long f);
#define RS0(n)             rt_sys((n), 0, 0, 0, 0, 0, 0)
#define RS1(n, a)          rt_sys((n), (long)(a), 0, 0, 0, 0, 0)
#define RS2(n, a, b)       rt_sys((n), (long)(a), (long)(b), 0, 0, 0, 0)
#define RS3(n, a, b, c)    rt_sys((n), (long)(a), (long)(b), (long)(c), 0, 0, 0)
#define RS4(n, a, b, c, d) rt_sys((n), (long)(a), (long)(b), (long)(c), (long)(d), 0, 0)

typedef struct { u64 rbx, rbp, r12, r13, r14, r15, rsp, rip; } rt_jmp_t;
int rt_setjmp(rt_jmp_t *j);

#define RT_THREAD_MAGIC 0x4441455254343641ULL   /* "A64TREAD" */
#define RT_MAX_THREADS  4096

typedef struct rt_thread {
    u64 magic;
    struct rt_thread *self;
    a64_cpu cpu;
    rt_jmp_t jb;                    /* volta do tratador de falta para o laço */
    volatile int in_guest;          /* dentro de a64_run */
    volatile int in_svc;            /* atendendo uma syscall do guest (cpu consistente) */
    volatile int state;             /* 0 livre, 1 viva */
    int pad0;
    long tid;
    /* sinais do guest pendentes nesta thread (numeração do kernel) */
    volatile u32 pend;
    u32 pend_oldmask[MOS_NSIG];     /* máscara antes da entrega (vai para uc_sigmask) */
    mos_siginfo_t pinfo[MOS_NSIG];
    u32 pesr[MOS_NSIG];             /* ESR para __es (faltas síncronas) */
    /* falta síncrona que abortou a64_run */
    int fault_sig, fault_code;
    u64 fault_addr;
    u32 fault_esr, fault_kind;
    volatile int fault_pending;     /* falta no JIT: a64_jit_run devolveu STOP por a64_jit_unwind */
    /* sigaltstack do guest */
    u64 ss_sp, ss_size;
    u64 hstack_lo;                  /* base da pilha do host (= este bloco) */
    long syscall_nr;                /* syscall em andamento (SA_RESTART) */
    int sig_depth;
    volatile u64 freed_ms;          /* SYS_TIME de quando a thread saiu (reuso só depois de uma carência) */
} rt_thread_t;

static inline rt_thread_t *rt_self(void) {
    u64 sp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
    return (rt_thread_t *)(sp & ~(A64RT_HSTACK - 1));
}

/* rt.c */
extern int rt_engine, rt_debug;
extern u64 rt_text_lo, rt_text_hi;
void rt_run_thread(rt_thread_t *T) __attribute__((noreturn));
rt_thread_t *rt_thread_find(long tid);
void rt_log(const char *fmt, ...);
void rt_die(const char *fmt, ...) __attribute__((noreturn));
void *rt_alloc(u64 len);
void rt_free(void *p, u64 len);

/* sig.c */
void rt_sig_init(void);
long rt_sig_syscall(rt_thread_t *T, long op, long a, long b, long c);
long rt_sigaction_old(rt_thread_t *T, long sig, long handler, long restorer);
void rt_sig_return_old(rt_thread_t *T);
void rt_sig_raise_sync(rt_thread_t *T, int sig, int code, u64 addr, u32 esr);  /* UDF/BRK/FAULT/SIGSYS */
void rt_sig_deliver_pending(rt_thread_t *T, int interrupted_restartable);
bool rt_sig_restart_ok(int sig);
void rt_host_signal(int sig, mos_siginfo_t *si, mos_ucontext_t *uc);
void rt_sig_fork_child(void);
void rt_fbstat_final(const char *quando);

/* libc mínima */
void *memset(void *d, int c, unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);
void *memmove(void *d, const void *s, unsigned long n);
int memcmp(const void *a, const void *b, unsigned long n);
unsigned long strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);

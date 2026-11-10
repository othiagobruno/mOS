/* Host: pilha de 3 frames via unw_step, get/set de callee-saved, XMM, unw_resume
 * e o trampolim de sinal do mOS (magic mOSSIGNL + mcontext). Liga cxx_unwind.c. */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UNW_ESUCCESS = 0, UNW_EBADREG = -6542, UNW_ENOINFO = -6549 };
enum { UNW_REG_IP = -1, UNW_REG_SP = -2, UNW_X86_64_RBX = 3, UNW_X86_64_R12 = 12, UNW_X86_64_XMM0 = 17 };

struct unw_context_t { uint64_t data[128]; };
struct unw_cursor_t { uint64_t data[140]; };
_Static_assert(sizeof(struct unw_context_t) == 1024, "contexto Apple");
_Static_assert(sizeof(struct unw_cursor_t) == 1120, "cursor Apple");

struct unw_proc_info_t {
    uint64_t start_ip, end_ip, lsda, handler, gp, flags;
    uint32_t format, unwind_info_size;
    uint64_t unwind_info, extra;
};

extern int unw_init_local(struct unw_cursor_t *, struct unw_context_t *);
extern int unw_step(struct unw_cursor_t *);
extern int unw_get_reg(struct unw_cursor_t *, int, uint64_t *);
extern int unw_set_reg(struct unw_cursor_t *, int, uint64_t);
extern int unw_get_fpreg(struct unw_cursor_t *, int, double *);
extern int unw_set_fpreg(struct unw_cursor_t *, int, double);
extern int unw_resume(struct unw_cursor_t *);
extern int unw_get_proc_info(struct unw_cursor_t *, struct unw_proc_info_t *);
extern int unw_get_proc_name(struct unw_cursor_t *, char *, size_t, uint64_t *);
extern int unw_is_signal_frame(struct unw_cursor_t *);
extern int __unw_getcontext(struct unw_context_t *);

static void fail(const char *m) {
    fprintf(stderr, "unw_hosttest: %s\n", m);
    abort();
}

static uint64_t reg(struct unw_cursor_t *c, int n) {
    uint64_t v = 0;
    if (unw_get_reg(c, n, &v) != UNW_ESUCCESS) fail("get_reg");
    return v;
}

static uint64_t g_sp_top, g_sp_mid, g_sp_leaf;
static void *g_ip_top, *g_ip_mid, *g_ip_leaf;
static int g_resumed;
static uint64_t g_r12_after;
static uint64_t g_resume_rsp;

__attribute__((noinline)) void leaf(void) {
    struct unw_context_t uc;
    struct unw_cursor_t cur;
    void *ip;
    uint64_t sp, r12, xmm_bits;
    __asm__ volatile(
        "movabs $0xA11CE5A7E5, %%r12\n\t"
        "movabs $0x3FF0000000000000, %%rax\n\t"
        "movq %%rax, %%xmm0\n\t"
        "movq %%rsp, %%r11\n\t"
        "andq $-16, %%rsp\n\t"
        "movq %%rsp, %1\n\t"
        "call __unw_getcontext\n\t"
        "0: movq %%r11, %%rsp\n\t"
        "leaq 0b(%%rip), %0\n\t"
        : "=&r"(ip), "=&r"(sp)
        : "D"(&uc)
        : "rax", "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11", "r12", "xmm0", "memory");
    g_ip_leaf = ip;
    g_sp_leaf = sp;
    g_ip_mid = __builtin_return_address(0);
    g_ip_top = __builtin_return_address(1);
    /* O SP restaurado é o CFA de quem chamou o getcontext/o call: %rbp+16. */
    g_sp_mid = (uint64_t)__builtin_frame_address(0) + 16;
    g_sp_top = (uint64_t)__builtin_frame_address(1) + 16;
    if (unw_init_local(&cur, &uc) != UNW_ESUCCESS) fail("init");
    if (reg(&cur, UNW_REG_IP) != (uint64_t)ip) fail("ip do leaf");
    if (reg(&cur, UNW_REG_SP) != sp) fail("sp do leaf");
    if (reg(&cur, UNW_X86_64_R12) != 0xA11CE5A7E5ULL) fail("r12 capturado");
    if (unw_set_reg(&cur, UNW_X86_64_R12, 0xC0FFEE) != UNW_ESUCCESS) fail("set r12");
    if (reg(&cur, UNW_X86_64_R12) != 0xC0FFEE) fail("r12 depois do set");
    if (unw_get_reg(&cur, 99, &r12) != UNW_EBADREG) fail("registrador invalido");
    double xm = 0;
    if (unw_get_fpreg(&cur, UNW_X86_64_XMM0, &xm) != UNW_ESUCCESS) fail("xmm0");
    memcpy(&xmm_bits, &xm, 8);
    if (xmm_bits != 0x3FF0000000000000ULL) fail("bits do xmm0");
    if (unw_is_signal_frame(&cur) != 0) fail("leaf nao e sinal");
    struct unw_proc_info_t pi;
    if (unw_get_proc_info(&cur, &pi) != UNW_ESUCCESS) fail("proc_info");
    if (pi.start_ip == 0 || (uint64_t)ip < pi.start_ip || (pi.end_ip && (uint64_t)ip >= pi.end_ip))
        fail("faixa do leaf");
    char name[64];
    uint64_t off = 0;
    if (unw_get_proc_name(&cur, name, sizeof name, &off) != UNW_ESUCCESS) fail("proc_name");
    if (!strstr(name, "leaf")) fail(name);
    if (unw_step(&cur) <= 0) fail("step mid");
    if (reg(&cur, UNW_REG_IP) != (uint64_t)g_ip_mid) {
        fprintf(stderr, "mid ip %llx want %p\n", (unsigned long long)reg(&cur, UNW_REG_IP), g_ip_mid);
        fail("ip do mid");
    }
    if (reg(&cur, UNW_REG_SP) != g_sp_mid) {
        fprintf(stderr, "mid sp %llx want %llx rbp0 %p rbp1 %p\n",
            (unsigned long long)reg(&cur, UNW_REG_SP), (unsigned long long)g_sp_mid,
            __builtin_frame_address(0), __builtin_frame_address(1));
        fail("sp do mid");
    }
    if (unw_step(&cur) <= 0) fail("step top");
    if (reg(&cur, UNW_REG_IP) != (uint64_t)g_ip_top) {
        fprintf(stderr, "top ip %llx want %p\n", (unsigned long long)reg(&cur, UNW_REG_IP), g_ip_top);
        fail("ip do top");
    }
    if (reg(&cur, UNW_REG_SP) != g_sp_top) {
        fprintf(stderr, "top sp %llx want %llx rbp0 %p rbp1 %p rbp2 %p\n",
            (unsigned long long)reg(&cur, UNW_REG_SP), (unsigned long long)g_sp_top,
            __builtin_frame_address(0), __builtin_frame_address(1), __builtin_frame_address(2));
        fail("sp do top");
    }
    int n = 2;
    int st;
    while ((st = unw_step(&cur)) > 0 && n < 32) n++;
    if (st < 0) fail("step erro");
    if (n < 3) fail("menos de 3 frames");
    (void)r12;
}

__attribute__((noinline)) void mid(void) { leaf(); }
__attribute__((noinline)) void top(void) { mid(); }

__attribute__((noinline)) static void resume_from(void) {
    struct unw_context_t uc;
    struct unw_cursor_t cur;
    uint64_t r12 = 0;
    __asm__ volatile(
        "movq %%rsp, %1\n\t"
        "andq $-16, %%rsp\n\t"
        "call __unw_getcontext\n\t"
        "0: movq %1, %%rsp\n\t"
        "movq %%r12, %0\n\t"
        : "=&r"(r12), "+m"(g_resume_rsp)
        : "D"(&uc)
        : "rax", "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11", "memory");
    if (g_resumed) {
        g_r12_after = r12;
        return;
    }
    g_resumed = 1;
    if (unw_init_local(&cur, &uc) != UNW_ESUCCESS) fail("init resume");
    if (unw_set_reg(&cur, UNW_X86_64_R12, 0xA5A5A5A5ULL) != UNW_ESUCCESS) fail("set r12 resume");
    unw_resume(&cur);
    fail("resume voltou");
}

#define SIGFRAME_MAGIC 0x6D4F535349474E4CULL
struct sigframe {
    uint64_t magic, uctx;
    uint8_t info[104];
    uint8_t uc[56];
    uint8_t mc_head[16];
    uint64_t ss[21];
    uint8_t fpu_pad[8]; /* fpu_reserved[2] do mcontext, antes do FXSAVE */
    uint8_t fx[512];
};
_Static_assert(sizeof(struct sigframe) == 16 + 104 + 56 + 16 + 21 * 8 + 8 + 512, "quadro");

static uint8_t pile[16384] __attribute__((aligned(16)));
static jmp_buf jb;
static void *g_intr_ip;
static int g_saw_sig, g_sig_ok;
static uint64_t g_sig_ip, g_sig_bx, g_sig_xm;

__attribute__((noinline)) void interrupted_body(void) {
    __asm__ volatile("nop\n\t1: leaq 1b(%%rip), %0" : "=r"(g_intr_ip));
}

__attribute__((noinline)) static void sig_handler(int s, void *si, void *ucx) {
    (void)s; (void)si; (void)ucx;
    struct unw_context_t uc;
    struct unw_cursor_t cur;
    __unw_getcontext(&uc);
    if (unw_init_local(&cur, &uc) != UNW_ESUCCESS) fail("init sinal");
    int guard = 0, saw = 0;
    while (guard++ < 16) {
        if (unw_is_signal_frame(&cur) > 0) { saw = 1; break; }
        if (unw_step(&cur) <= 0) break;
    }
    g_saw_sig = saw;
    if (saw) {
        if (unw_step(&cur) <= 0) fail("step apos sinal");
        g_sig_ip = reg(&cur, UNW_REG_IP);
        g_sig_bx = reg(&cur, UNW_X86_64_RBX);
        g_sig_ok = g_sig_ip == (uint64_t)g_intr_ip && g_sig_bx == 0xB0B0B0B0ULL;
        double xm = 0;
        if (unw_get_fpreg(&cur, UNW_X86_64_XMM0, &xm) != UNW_ESUCCESS) fail("xmm do sinal");
        memcpy(&g_sig_xm, &xm, 8);
        if (g_sig_xm != 0x4008000000000000ULL) g_sig_ok = 0;
    }
    longjmp(jb, 1);
}

void cx_sigtramp(void *h, int style, int sig, void *si, void *uc) {
    (void)style;
    ((void (*)(int, void *, void *))h)(sig, si, uc);
}

static void test_signal(void) {
    interrupted_body();
    uint8_t *top = pile + sizeof pile;
    struct sigframe *sf = (struct sigframe *)((uintptr_t)(top - sizeof *sf) & ~(uintptr_t)15);
    memset(sf, 0, sizeof *sf);
    sf->magic = SIGFRAME_MAGIC;
    sf->ss[1] = 0xB0B0B0B0ULL;
    sf->ss[16] = (uint64_t)g_intr_ip;
    sf->ss[7] = (uint64_t)pile + 64;
    sf->ss[6] = 0;
    uint64_t one = 0x4008000000000000ULL;
    memcpy(sf->fx + 160, &one, 8);
    if (setjmp(jb) == 0) {
        void *fa = sf;
        __asm__ volatile(
            "leaq -8(%0), %%rsp\n\t"
            "movq $0, (%%rsp)\n\t"
            "movq %1, %%rdi\n\t"
            "xorl %%esi, %%esi\n\t"
            "xorl %%edx, %%edx\n\t"
            "xorl %%ecx, %%ecx\n\t"
            "movq %2, %%r8\n\t"
            "jmp cx_sigtramp\n\t"
            :
            : "r"(fa), "r"(sig_handler), "r"(sf)
            : "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory");
        fail("trampolim retornou");
    }
    if (!g_saw_sig) fail("nao viu o trampolim");
    if (!g_sig_ok) {
        fprintf(stderr, "sinal ip %llx want %p bx %llx xm %llx saw %d\n",
            (unsigned long long)g_sig_ip, g_intr_ip, (unsigned long long)g_sig_bx,
            (unsigned long long)g_sig_xm, g_saw_sig);
        fail("contexto do sinal");
    }
}

int main(void) {
    top();
    resume_from();
    if (!g_resumed || g_r12_after != 0xA5A5A5A5ULL) fail("resume");
    test_signal();
    printf("unw_hosttest: 3 frames, r12, xmm, resume, sinal\n");
    return 0;
}

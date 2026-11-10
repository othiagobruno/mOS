/*
 * mOS — o que cada syscall grava no guest (frente tradução). Fonte: os uvm_copy_out do kernel
 * e os memcpy do runtime que respondem a syscall no lugar do kernel (SYS_SIG, GETSTATE).
 * Um intervalo a mais só descarta tradução; um a menos deixa código velho executável.
 */
#include "writes.h"
#include "a64rt_abi.h"
#include "../../kernel/include/syscall.h"

_Static_assert(sizeof(mos_stat_t) == 24, "stat");
_Static_assert(sizeof(mos_procinfo_t) == 64, "procinfo");
_Static_assert(sizeof(mos_sigaction_t) == 24, "sigaction");
_Static_assert(sizeof(mos_sigstack_t) == 24, "sigstack");
_Static_assert(sizeof(mos_siginfo_t) == 104, "siginfo");
_Static_assert(sizeof(a64_darwin_thread_state_t) == 272, "thread state arm64");

/* MACH_RCV_MSG, o mesmo bit de userland/libsystem/mach.c */
#define A64RT_MACH_RCV 0x2u
/* direção IOC_OUT do macOS: o tamanho do ioctl está nos bits 16..28 */
#define A64RT_IOC_OUT 0x40000000u

static void push(a64rt_span *o, int *n, int max, a64rt_u64 addr, a64rt_u64 len) {
    if (!o || !addr || !len || *n >= max) return;
    o[*n].addr = addr;
    o[*n].len = len;
    (*n)++;
}

static void net_spans(a64rt_u64 op, a64rt_u64 a, a64rt_u64 b, a64rt_i64 ret, a64rt_peek_fn peek, void *ctx,
                      a64rt_span *o, int *n, int max) {
    switch (op) {
    case MOS_NET_RECV:
    case MOS_NET_ICMP_RECV:
    case MOS_NET_TLS_DRAIN:
    case MOS_NET_TLS_RECV:
    case MOS_NET_TLS_ERROR:
    case MOS_NET_TLS_FREE:
        if (ret > 0) push(o, n, max, b, (a64rt_u64)ret);          /* buf no 2º argumento do kernel = x2 */
        break;
    case MOS_NET_TLS_ROOTS:
        if (a && ret > 0) push(o, n, max, a, (a64rt_u64)ret);      /* PEM em x1 */
        break;
    case MOS_NET_RESOLVE:
        if (ret == 1 && b) push(o, n, max, b, 4);
        break;
    case MOS_NET_RESOLVE6:
        if (ret == 1 && b) push(o, n, max, b, 16);
        break;
    case MOS_NET_INFO:
        if (a && ret >= 0) push(o, n, max, a, 16);
        break;
    case MOS_NET_INFO6:
        if (a && ret >= 0) push(o, n, max, a, sizeof(mos_net6_info_t));
        break;
    case MOS_NET_ADDRS:
        if (b && ret == 0) push(o, n, max, b, sizeof(mos_net_addrs_t));
        break;
    case MOS_NET_UDP_ADDRS:
        if (b && ret == 0) push(o, n, max, b, sizeof(mos_net_addrs_t));
        break;
    case MOS_NET_UDP_RECVFROM:
        /* ret >= 0: a struct em x2 foi reescrita; o payload está em d.buf, com `ret` bytes */
        if (ret >= 0 && b) {
            push(o, n, max, b, sizeof(mos_net_dgram_t));
            if (ret > 0 && peek) {
                mos_net_dgram_t d;
                if (peek(ctx, b, &d, sizeof d) && d.buf) push(o, n, max, d.buf, (a64rt_u64)ret);
            }
        }
        break;
    default:
        break;
    }
}

static void sig_spans(a64rt_u64 op, a64rt_u64 a, a64rt_u64 b, a64rt_u64 c, a64rt_i64 ret,
                      a64rt_span *o, int *n, int max) {
    switch (op) {
    case MOS_SIGOP_ACTION:                                         /* runtime: antiga em x3 */
        if (ret == 0 && c) push(o, n, max, c, sizeof(mos_sigaction_t));
        break;
    case MOS_SIGOP_MASK:                                           /* kernel: máscara antiga em x3 */
        if (ret == 0 && c) push(o, n, max, c, 4);
        break;
    case MOS_SIGOP_PENDING:                                        /* runtime: 4 bytes em x1 */
        if (ret == 0 && a) push(o, n, max, a, 4);
        break;
    case MOS_SIGOP_ALTSTACK:                                       /* runtime escreve a antiga antes de validar a nova */
        if (b) push(o, n, max, b, sizeof(mos_sigstack_t));
        break;
    case MOS_SIGOP_WAIT:                                           /* kernel: siginfo em x2 */
        if (ret > 0 && b) push(o, n, max, b, sizeof(mos_siginfo_t));
        break;
    default:
        break;
    }
}

static void tty_spans(a64rt_u64 op, a64rt_u64 a, a64rt_u64 b, a64rt_u64 c, a64rt_i64 ret,
                      a64rt_span *o, int *n, int max) {
    (void)a;
    switch (op) {
    case MOS_TTY_NAME:                                             /* (fd, buf, cap) -> tamanho sem o 0 */
        if (ret >= 0 && b) push(o, n, max, b, (a64rt_u64)ret + 1);
        break;
    case MOS_TTY_PROCINFO:
        if (ret == 0 && b) push(o, n, max, b, 16);
        break;
    case MOS_TTY_IOCTL:
        if (ret == 0 && c && (b & 0xC0000000u) == A64RT_IOC_OUT) {
            a64rt_u64 sz = (b >> 16) & 0x1FFFu;                    /* TIOCGETA, TIOCGWINSZ, FIONREAD, ... */
            if (sz) push(o, n, max, c, sz);
        }
        break;
    default:
        break;
    }
}

int a64rt_guest_writes(long nr, a64rt_u64 x0, a64rt_u64 x1, a64rt_u64 x2, a64rt_u64 x3, a64rt_i64 ret,
                       a64rt_peek_fn peek, void *ctx, a64rt_span *o, int max) {
    int n = 0;
    if (max <= 0) return 0;
    switch (nr) {
    case SYS_READ:
    case SYS_PREAD:
    case SYS_READFILE:                                             /* buf em x1, bytes = retorno */
        if (ret > 0) push(o, &n, max, x1, (a64rt_u64)ret);
        break;
    case SYS_FSTAT:
    case SYS_STAT:
        if (ret == 0 && x1) push(o, &n, max, x1, sizeof(mos_stat_t));
        break;
    case SYS_READDIR:                                              /* nome em x2, no máximo cap (x3) */
        if (ret == 1 && x2 && x3) push(o, &n, max, x2, x3);
        break;
    case SYS_PIPE:
        if (ret == 0 && x0) push(o, &n, max, x0, 8);
        break;
    case SYS_GETCWD:                                               /* grava n bytes e devolve n-1 */
        if (ret >= 0 && x0) push(o, &n, max, x0, (a64rt_u64)ret + 1);
        break;
    case SYS_FDPATH:
        if (ret >= 0 && x1) push(o, &n, max, x1, (a64rt_u64)ret + 1);
        break;
    case SYS_WAITPID:
        if (ret > 0 && x1) push(o, &n, max, x1, 4);
        break;
    case SYS_WAITANY:
        if (ret > 0 && x0) push(o, &n, max, x0, 4);
        break;
    case SYS_PROCLIST:
        if (ret == 1 && x1) push(o, &n, max, x1, sizeof(mos_procinfo_t));
        break;
    case SYS_IMGATTACH:                                            /* caminhos em x1; o kernel cabe em 1024 */
        if (ret > 0 && x1 && x2) {
            a64rt_u64 cap = x2 < 1024 ? x2 : 1024;
            push(o, &n, max, x1, cap);
        }
        break;
    case SYS_NET:
        net_spans(x0, x1, x2, ret, peek, ctx, o, &n, max);
        break;
    case SYS_SIG:
        sig_spans(x0, x1, x2, x3, ret, o, &n, max);
        break;
    case SYS_TTY:
        tty_spans(x0, x1, x2, x3, ret, o, &n, max);
        break;
    case SYS_DYLD:                                                 /* (op, a, b, c) = (x0, x1, x2, x3) */
        if (x0 == MOS_DYLD_IMAGE && ret != 0 && x2 && x3) push(o, &n, max, x2, x3);
        else if (x0 == MOS_DYLD_ERROR && x1 && x2 && ret >= 0) push(o, &n, max, x1, x2);
        else if (x0 == MOS_DYLD_OPEN && x2 && ret > -4096) push(o, &n, max, x2, sizeof(mos_dyld_open_t));
        break;
    case SYS_LS:                                                   /* out[] mora dentro do pedido */
        if (x1 && ret != -MOS_EFAULT) push(o, &n, max, x1, sizeof(mos_ls_req_t));
        break;
    case SYS_MACH_PORT:                                            /* o vetor a[8] volta sempre que a cópia entrou */
        if (x1) push(o, &n, max, x1, 64);
        break;
    case SYS_MACH_BOOTSTRAP:
        if (x2) push(o, &n, max, x2, 4);
        break;
    case SYS_MACH_VM:
        if (x1) push(o, &n, max, x1, 64);                          /* o kernel devolve o vetor */
        if (ret == 0 && x0 == MACH_VM_PROTECT && peek) {
            a64rt_u64 a[2];
            if (peek(ctx, x1, a, sizeof a) && a[0] && a[1]) push(o, &n, max, a[0], a[1]);
        }
        if (ret == 0 && x0 == MACH_VM_MAP && peek) {
            a64rt_u64 a[2];
            if (peek(ctx, x1, a, sizeof a) && a[0] && a[1]) push(o, &n, max, a[0] & ~0xFFFull, (a[1] + 0xFFF) & ~0xFFFull);
        }
        break;
    case SYS_MACH_MSG:
        if (ret == 0 && x0 && peek) {
            mos_mach_msg_args_t m;
            if (peek(ctx, x0, &m, sizeof m) && (m.option & A64RT_MACH_RCV) && m.msg && m.rcv_size)
                push(o, &n, max, m.msg, m.rcv_size);
        }
        break;
    case SYS_THREADCTL:
        if (x0 == MOS_THR_GETSTATE && ret == 0 && x2) push(o, &n, max, x2, sizeof(a64_darwin_thread_state_t));
        break;
    default:
        break;
    }
    return n;
}

int a64rt_hstack_plan(a64rt_u64 raw, a64rt_u64 stack, a64rt_hstack *o) {
    if (!o || !raw || !stack || (stack & (stack - 1))) return 0;
    a64rt_u64 use = (raw + stack - 1) & ~(stack - 1);
    if (use < raw || use + stack > raw + 2 * stack) return 0;
    o->use = use;
    o->free_base = raw;
    o->free_len = 2 * stack;
    return 1;
}

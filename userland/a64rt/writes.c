/*
 * mOS — o que cada syscall grava no guest (frente tradução). Fonte: os uvm_copy_out do kernel
 * e os memcpy do runtime que respondem a syscall no lugar do kernel (SYS_SIG, GETSTATE).
 * Um intervalo a mais só descarta tradução; um a menos deixa código velho executável.
 * Toda escrita do host em memória do guest passa por a64rt_invalidate_range (ou por
 * a64rt_invalidate_spans, que chama esse ponto). Se a lista não cabe, devolve -1 e quem
 * chama invalida toda tradução.
 */
#include "writes.h"
#include "a64.h"
#include "a64rt_abi.h"
#include "../../kernel/include/syscall.h"

typedef unsigned int u32;
typedef int i32;
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long long usize;
#include "../../kernel/include/compat_gui.h"

_Static_assert(sizeof(mos_stat_t) == 24, "stat");
_Static_assert(sizeof(mos_procinfo_t) == 64, "procinfo");
_Static_assert(sizeof(mos_sigaction_t) == 24, "sigaction");
_Static_assert(sizeof(mos_sigstack_t) == 24, "sigstack");
_Static_assert(sizeof(mos_siginfo_t) == 104, "siginfo");
_Static_assert(sizeof(a64_darwin_thread_state_t) == 272, "thread state arm64");
_Static_assert(sizeof(gui_event_t) >= 48, "gui_event cobre um bloco curto");

/* MACH_RCV_MSG, o mesmo bit de userland/libsystem/mach.c */
#define A64RT_MACH_RCV 0x2u
/* direção IOC_OUT do macOS: o tamanho do ioctl está nos bits 16..28 */
#define A64RT_IOC_OUT 0x40000000u
/* mach_msg.h do XNU, os mesmos números de kernel/proc/machipc.c */
#define A64RT_MSGH_COMPLEX 0x80000000u
#define A64RT_D_PORT 0u
#define A64RT_D_OOL 1u
#define A64RT_D_OOL_PORTS 2u
#define A64RT_D_OOL_VOL 3u
#define A64RT_MAX_ITEMS 64u
#define A64RT_MAX_OOL (256ull << 20)   /* o kernel recusa um OOL maior e não mapeia */
#define A64RT_MAX_OOL_PORTS 4096u
#define A64RT_H_BITS 0u
#define A64RT_H_NDESC 24u
#define A64RT_H_BODY 28u

typedef struct {
    a64rt_span *o;
    int n, max, overflow;
} spans;

static void push(spans *s, a64rt_u64 addr, a64rt_u64 len) {
    if (!addr || !len || s->overflow) return;
    if (!s->o || s->n >= s->max) { s->overflow = 1; return; }
    s->o[s->n].addr = addr;
    s->o[s->n].len = len;
    s->n++;
}

/* o kernel mapeia páginas inteiras (ool_copyin zera o resto da página) */
static a64rt_u64 paginas(a64rt_u64 n) {
    if (!n || n > ~(a64rt_u64)0 - 0xFFFull) return 0;
    return (n + 0xFFFull) & ~(a64rt_u64)0xFFF;
}

static a64rt_u64 rd32b(const unsigned char *b) {
    return (a64rt_u64)b[0] | ((a64rt_u64)b[1] << 8) | ((a64rt_u64)b[2] << 16) | ((a64rt_u64)b[3] << 24);
}

static a64rt_u64 rd64b(const unsigned char *b) {
    return rd32b(b) | (rd32b(b + 4) << 32);
}

static void net_spans(a64rt_u64 op, a64rt_u64 a, a64rt_u64 b, a64rt_i64 ret, a64rt_peek_fn peek, void *ctx, spans *s) {
    switch (op) {
    case MOS_NET_RECV:
    case MOS_NET_ICMP_RECV:
    case MOS_NET_TLS_DRAIN:
    case MOS_NET_TLS_RECV:
    case MOS_NET_TLS_ERROR:
    case MOS_NET_TLS_FREE:
        if (ret > 0) push(s, b, (a64rt_u64)ret);                   /* buf no 2º argumento do kernel = x2 */
        break;
    case MOS_NET_TLS_ROOTS:
        if (a && ret > 0) push(s, a, (a64rt_u64)ret);              /* PEM em x1 */
        break;
    case MOS_NET_RESOLVE:
        if (ret == 1 && b) push(s, b, 4);
        break;
    case MOS_NET_RESOLVE6:
        if (ret == 1 && b) push(s, b, 16);
        break;
    case MOS_NET_INFO:
        if (a && ret >= 0) push(s, a, 16);
        break;
    case MOS_NET_INFO6:
        if (a && ret >= 0) push(s, a, sizeof(mos_net6_info_t));
        break;
    case MOS_NET_ADDRS:
        if (b && ret == 0) push(s, b, sizeof(mos_net_addrs_t));
        break;
    case MOS_NET_UDP_ADDRS:
        if (b && ret == 0) push(s, b, sizeof(mos_net_addrs_t));
        break;
    case MOS_NET_UDP_RECVFROM:
        /* ret >= 0: a struct em x2 foi reescrita; o payload está em d.buf, com `ret` bytes */
        if (ret >= 0 && b) {
            push(s, b, sizeof(mos_net_dgram_t));
            if (ret > 0 && peek) {
                mos_net_dgram_t d;
                if (peek(ctx, b, &d, sizeof d) && d.buf) push(s, d.buf, (a64rt_u64)ret);
            }
        }
        break;
    default:
        break;
    }
}

static void sig_spans(a64rt_u64 op, a64rt_u64 a, a64rt_u64 b, a64rt_u64 c, a64rt_i64 ret, spans *s) {
    switch (op) {
    case MOS_SIGOP_ACTION:                                         /* runtime: antiga em x3 */
        if (ret == 0 && c) push(s, c, sizeof(mos_sigaction_t));
        break;
    case MOS_SIGOP_MASK:                                           /* kernel: máscara antiga em x3 */
        if (ret == 0 && c) push(s, c, 4);
        break;
    case MOS_SIGOP_PENDING:                                        /* runtime: 4 bytes em x1 */
        if (ret == 0 && a) push(s, a, 4);
        break;
    case MOS_SIGOP_ALTSTACK:                                       /* runtime escreve a antiga antes de validar a nova */
        if (b) push(s, b, sizeof(mos_sigstack_t));
        break;
    case MOS_SIGOP_WAIT:                                           /* kernel: siginfo em x2 */
        if (ret > 0 && b) push(s, b, sizeof(mos_siginfo_t));
        break;
    default:
        break;
    }
}

static void tty_spans(a64rt_u64 op, a64rt_u64 a, a64rt_u64 b, a64rt_u64 c, a64rt_i64 ret, spans *s) {
    (void)a;
    switch (op) {
    case MOS_TTY_NAME:                                             /* (fd, buf, cap) -> tamanho sem o 0 */
        if (ret >= 0 && b) push(s, b, (a64rt_u64)ret + 1);
        break;
    case MOS_TTY_PROCINFO:
        if (ret == 0 && b) push(s, b, 16);
        break;
    case MOS_TTY_IOCTL:
        if (ret == 0 && c && (b & 0xC0000000u) == A64RT_IOC_OUT) {
            a64rt_u64 sz = (b >> 16) & 0x1FFFu;                    /* TIOCGETA, TIOCGWINSZ, FIONREAD, ... */
            if (sz) push(s, c, sz);
        }
        break;
    default:
        break;
    }
}

/* GUI_DEVICE: gamepad, captura de tela, áudio, lista de janelas e BLE. O ponteiro de saída não é
 * sempre o mesmo registrador (compat_gui.h). */
static void device_spans(a64rt_u64 op, a64rt_u64 x1, a64rt_u64 x2, a64rt_u64 x3, a64rt_i64 ret, spans *s) {
    switch (op) {
    case DEV_PAD_GET:                                              /* (índice, gui_gamepad_t *) -> 1 */
        if (ret == 1 && x2) push(s, x2, sizeof(gui_gamepad_t));
        break;
    case DEV_AUDIO_STATE:                                          /* (stream, gui_audio_state_t *) -> 0 */
        if (ret == 0 && x2) push(s, x2, sizeof(gui_audio_state_t));
        break;
    case DEV_AUDIO_INFO:                                           /* (gui_audio_state_t *) -> 0 ou 1; os dois gravaram */
        if (ret >= 0 && x1) push(s, x1, sizeof(gui_audio_state_t));
        break;
    case DEV_SCREEN_CAPTURE:                                       /* (xy, wh, buf) -> w | h << 32; buf 0 só pergunta */
        if (ret > 0 && x3) {
            a64rt_u64 w = (a64rt_u64)ret & 0xFFFFFFFFull;
            a64rt_u64 h = ((a64rt_u64)ret >> 32) & 0xFFFFFFFFull;
            if (w && h && h <= (~(a64rt_u64)0 / 4) / w) push(s, x3, w * h * 4);
        }
        break;
    case DEV_WINDOW_LIST:                                          /* (índice, gui_cgwindow_t *) -> 1; ponteiro 0 não grava */
        if (ret == 1 && x2) push(s, x2, sizeof(gui_cgwindow_t));
        break;
    case DEV_BT_DEVICE:                                            /* (índice, gui_bt_device_t *) -> 1 */
        if (ret == 1 && x2) push(s, x2, sizeof(gui_bt_device_t));
        break;
    case DEV_BT_ATT_RECV:                                          /* (buf, cap) -> bytes */
        if (ret > 0 && x1) push(s, x1, (a64rt_u64)ret);
        break;
    case DEV_BT_LINK:                                              /* (gui_bt_link_t *) -> 0 */
        if (ret == 0 && x1) push(s, x1, sizeof(gui_bt_link_t));
        break;
    default:
        break;
    }
}

static void font_spans(a64rt_u64 op, a64rt_u64 x3, a64rt_u64 x4, a64rt_i64 ret, a64rt_peek_fn peek, void *ctx, spans *s) {
    if (op == GF_TABLE) {
        /* (peso, tag, buf, cap) -> tamanho cheio; grava min(tamanho, cap), com cap limitado a 8 MiB */
        if (ret > 0 && x3 && x4) {
            a64rt_u64 cap = x4 > (8ull << 20) ? (8ull << 20) : x4;
            a64rt_u64 n = (a64rt_u64)ret < cap ? (a64rt_u64)ret : cap;
            if (n) push(s, x3, n);
        }
        return;
    }
    if (op == GF_METRICS) {
        if (ret == 0 && x3) push(s, x3, sizeof(gui_font_metrics_t));
        return;
    }
    if (op == GF_RASTER && ret >= 0 && x3) {
        push(s, x3, sizeof(gui_glyph_req_t));                      /* a struct volta com w/h/left/top */
        if (ret > 0 && peek) {
            gui_glyph_req_t req;
            if (peek(ctx, x3, &req, sizeof req) && req.buf) {
                a64rt_u64 n = (a64rt_u64)ret;
                if (req.cap && n > req.cap) n = req.cap;
                if (n) push(s, req.buf, n);
            }
        }
    }
}

static void gui_spans(long nr, a64rt_u64 x0, a64rt_u64 x1, a64rt_u64 x2, a64rt_u64 x3, a64rt_u64 x4,
                      a64rt_i64 ret, a64rt_peek_fn peek, void *ctx, spans *s) {
    switch (nr) {
    case GUI_POLL:                                                 /* (gui_event_t *) -> 1 gravou, 0 não havia evento */
        if (ret == 1 && x0) push(s, x0, sizeof(gui_event_t));
        break;
    case GUI_MEASURE:                                              /* (texto, bytes, gui_metrics_t *, ...) -> 0 */
        if (ret == 0 && x2) push(s, x2, sizeof(gui_metrics_t));
        break;
    case GUI_WIN_INFO:                                             /* (id, gui_wininfo_t *) -> 0 */
        if (ret == 0 && x1) push(s, x1, sizeof(gui_wininfo_t));
        break;
    case GUI_SCREEN:                                               /* (gui_wininfo_t *) -> 0 */
        if (ret == 0 && x0) push(s, x0, sizeof(gui_wininfo_t));
        break;
    case GUI_DECODE_IMAGE:                                         /* (dados, bytes, out, cap) -> need; só grava se cap >= need */
        if (ret > 0 && x2 && x3 >= (a64rt_u64)ret) push(s, x2, (a64rt_u64)ret);
        break;
    case GUI_FONT:
        font_spans(x0, x3, x4, ret, peek, ctx, s);
        break;
    case GUI_DEVICE:
        device_spans(x0, x1, x2, x3, ret, s);
        break;
    case GUI_DRAG:                                                 /* drag_syscall / ime_syscall não estão neste repositório */
    case GUI_IME:
        break;
    default:
        break;
    }
}

static void web_spans(a64rt_u64 op, a64rt_u64 x2, a64rt_u64 x3, a64rt_i64 ret, a64rt_peek_fn peek, void *ctx, spans *s) {
    switch (op) {
    case MOS_WEB_PAINT:                                            /* (id, buf, cap) -> bytes */
        if (ret > 0 && x2) push(s, x2, (a64rt_u64)ret);
        break;
    case MOS_WEB_INFO:                                             /* (id, mos_web_info_t *) -> 0 */
        if (ret == 0 && x2) push(s, x2, sizeof(mos_web_info_t));
        break;
    case MOS_WEB_EVAL:                                             /* (id, mos_web_eval_t *) -> tamanho; o texto vai para E.out se coube */
        if (ret >= 0 && x2) {
            push(s, x2, sizeof(mos_web_eval_t));
            if (peek) {
                mos_web_eval_t e;
                if (peek(ctx, x2, &e, sizeof e) && e.out && e.len && (a64rt_u64)e.len <= e.cap)
                    push(s, e.out, e.len);
            }
        }
        break;
    case MOS_WEB_TAKEMSG:
        /* 1 = struct e corpo; -EINVAL = a struct voltou com o tamanho, o corpo ficou na fila */
        if ((ret == 1 || ret == -MOS_EINVAL) && x2) {
            push(s, x2, sizeof(mos_web_msg_t));
            if (ret == 1 && peek) {
                mos_web_msg_t m;
                if (peek(ctx, x2, &m, sizeof m) && m.body && m.len) push(s, m.body, m.len);
            }
        }
        break;
    case MOS_WEB_TEXT:                                             /* (id, buf, cap) -> tamanho cheio; grava min(tamanho, cap) */
        if (ret > 0 && x2 && x3) {
            a64rt_u64 n = (a64rt_u64)ret < x3 ? (a64rt_u64)ret : x3;
            if (n) push(s, x2, n);
        }
        break;
    case MOS_WEB_RESULT:                                           /* (id, buf, cap) -> bytes gravados, ou -EINVAL se não coube */
        if (ret > 0 && x2) push(s, x2, (a64rt_u64)ret);
        break;
    case MOS_WEB_HISTORY:                                          /* (id, índice, mos_web_hist_t *) -> 1 */
        if (ret == 1 && x3) push(s, x3, sizeof(mos_web_hist_t));
        break;
    default:
        break;
    }
}

/* descritores da mensagem já recebida: o endereço OOL é o VA novo que o kernel acabou de preencher */
static void ool_spans(a64rt_u64 msg, a64rt_u64 rcv, a64rt_peek_fn peek, void *ctx, spans *s) {
    unsigned char b[16];
    if (!peek || rcv < 4) return;
    if (!peek(ctx, msg, b, 4)) { s->overflow = 1; return; }
    if ((rd32b(b) & A64RT_MSGH_COMPLEX) == 0 || rcv < A64RT_H_BODY) return;
    if (!peek(ctx, msg + A64RT_H_NDESC, b, 4)) { s->overflow = 1; return; }
    a64rt_u64 nd = rd32b(b);
    if (nd > A64RT_MAX_ITEMS) { s->overflow = 1; return; }
    a64rt_u64 off = A64RT_H_BODY;
    for (a64rt_u64 i = 0; i < nd; i++) {
        if (off + 12 > rcv) break;
        a64rt_u64 nread = off + 16 <= rcv ? 16 : 12;
        if (!peek(ctx, msg + off, b, nread)) { s->overflow = 1; return; }
        unsigned dtype = b[11];
        if (dtype == A64RT_D_PORT) { off += 12; continue; }
        if (nread < 16) break;
        if (dtype == A64RT_D_OOL || dtype == A64RT_D_OOL_VOL) {
            a64rt_u64 addr = rd64b(b), sz = rd32b(b + 12);
            a64rt_u64 len = sz && sz <= A64RT_MAX_OOL ? paginas(sz) : 0;
            if (sz && sz <= A64RT_MAX_OOL && !len) s->overflow = 1;
            else if (len) push(s, addr, len);
        } else if (dtype == A64RT_D_OOL_PORTS) {
            a64rt_u64 addr = rd64b(b), cnt = rd32b(b + 12);
            if (cnt && cnt <= A64RT_MAX_OOL_PORTS) {
                a64rt_u64 len = paginas(cnt * 4);
                if (!len) s->overflow = 1;
                else push(s, addr, len);
            }
        }
        off += 16;
    }
}

int a64rt_guest_writes(long nr, a64rt_u64 x0, a64rt_u64 x1, a64rt_u64 x2, a64rt_u64 x3, a64rt_u64 x4,
                       a64rt_i64 ret, a64rt_peek_fn peek, void *ctx, a64rt_span *o, int max) {
    spans s = {o, 0, max, 0};
    if (max <= 0) return 0;
    switch (nr) {
    case SYS_READ:
    case SYS_PREAD:
    case SYS_READFILE:                                             /* buf em x1, bytes = retorno */
        if (ret > 0) push(&s, x1, (a64rt_u64)ret);
        break;
    case SYS_FSTAT:
    case SYS_STAT:
        if (ret == 0 && x1) push(&s, x1, sizeof(mos_stat_t));
        break;
    case SYS_READDIR:                                              /* nome em x2, no máximo cap (x3) */
        if (ret == 1 && x2 && x3) push(&s, x2, x3);
        break;
    case SYS_PIPE:
        if (ret == 0 && x0) push(&s, x0, 8);
        break;
    case SYS_GETCWD:                                               /* grava n bytes e devolve n-1 */
        if (ret >= 0 && x0) push(&s, x0, (a64rt_u64)ret + 1);
        break;
    case SYS_FDPATH:
        if (ret >= 0 && x1) push(&s, x1, (a64rt_u64)ret + 1);
        break;
    case SYS_WAITPID:
        if (ret > 0 && x1) push(&s, x1, 4);
        break;
    case SYS_WAITANY:
        if (ret > 0 && x0) push(&s, x0, 4);
        break;
    case SYS_PROCLIST:
        if (ret == 1 && x1) push(&s, x1, sizeof(mos_procinfo_t));
        break;
    case SYS_IMGATTACH:                                            /* caminhos em x1; o kernel cabe em 1024 */
        if (ret > 0 && x1 && x2) {
            a64rt_u64 cap = x2 < 1024 ? x2 : 1024;
            push(&s, x1, cap);
        }
        break;
    case SYS_NET:
        net_spans(x0, x1, x2, ret, peek, ctx, &s);
        break;
    case SYS_SIG:
        sig_spans(x0, x1, x2, x3, ret, &s);
        break;
    case SYS_TTY:
        tty_spans(x0, x1, x2, x3, ret, &s);
        break;
    case SYS_DYLD:                                                 /* (op, a, b, c) = (x0, x1, x2, x3) */
        if (x0 == MOS_DYLD_IMAGE && ret != 0 && x2 && x3) push(&s, x2, x3);
        else if (x0 == MOS_DYLD_ERROR && x1 && x2 && ret >= 0) push(&s, x1, x2);
        else if (x0 == MOS_DYLD_OPEN && x2 && ret > -4096) push(&s, x2, sizeof(mos_dyld_open_t));
        break;
    case SYS_LS:                                                   /* out[] mora dentro do pedido */
        if (x1 && ret != -MOS_EFAULT) push(&s, x1, sizeof(mos_ls_req_t));
        break;
    case SYS_MACH_PORT:                                            /* o vetor a[8] volta sempre que a cópia entrou */
        if (x1) push(&s, x1, 64);
        break;
    case SYS_MACH_BOOTSTRAP:
        if (x2) push(&s, x2, 4);
        break;
    case SYS_MACH_VM:
        if (x1) push(&s, x1, 64);                                  /* o kernel devolve o vetor */
        if (ret == 0 && x0 == MACH_VM_PROTECT && peek) {
            a64rt_u64 a[2];
            if (peek(ctx, x1, a, sizeof a) && a[0] && a[1]) push(&s, a[0], a[1]);
        }
        if (ret == 0 && x0 == MACH_VM_MAP && peek) {
            a64rt_u64 a[2];
            if (peek(ctx, x1, a, sizeof a) && a[0] && a[1]) push(&s, a[0] & ~0xFFFull, (a[1] + 0xFFF) & ~0xFFFull);
        }
        break;
    case SYS_MACH_MSG:
        if (ret == 0 && x0 && peek) {
            mos_mach_msg_args_t m;
            if (peek(ctx, x0, &m, sizeof m) && (m.option & A64RT_MACH_RCV) && m.msg && m.rcv_size) {
                push(&s, m.msg, m.rcv_size);
                ool_spans(m.msg, m.rcv_size, peek, ctx, &s);
            }
        }
        break;
    case SYS_THREADCTL:
        if (x0 == MOS_THR_GETSTATE && ret == 0 && x2) push(&s, x2, sizeof(a64_darwin_thread_state_t));
        break;
    case SYS_WEB:
        web_spans(x0, x2, x3, ret, peek, ctx, &s);
        break;
    default:
        if (nr >= GUI_WIN_CREATE && nr <= GUI_LAST)
            gui_spans(nr, x0, x1, x2, x3, x4, ret, peek, ctx, &s);
        break;
    }
    if (s.overflow) return -1;
    return s.n;
}

void a64rt_invalidate_range(a64rt_u64 addr, a64rt_u64 len) {
    if (!addr || !len) return;
#ifdef A64_HAVE_JIT
    /* buffer sem tradução (o caso comum) não toma a trava do JIT */
    if (!a64_jit_has_code(addr, len)) return;
#endif
    a64_invalidate(addr, len);
}

void a64rt_invalidate_spans(const a64rt_span *sp, int n) {
    if (n < 0) {
        a64_invalidate(0, ~(a64rt_u64)0);                          /* a lista estourou: não dá para saber o que ficou de fora */
        return;
    }
    if (!sp) return;
    for (int i = 0; i < n; i++) a64rt_invalidate_range(sp[i].addr, sp[i].len);
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

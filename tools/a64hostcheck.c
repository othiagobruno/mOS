/*
 * Harness Linux do que esta frente mudou: intervalos que uma syscall grava no guest,
 * plano da pilha do host, recuperação de falta no JIT e invalidação seguida de reexecução.
 * Não é o a64test do Windows nem o QEMU. Compilar com clang, ver o comentário no fim do relatório.
 */
#define _GNU_SOURCE
#include "a64.h"
#include "writes.h"
#include "syscall.h"

#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <ucontext.h>

typedef unsigned int u32;
typedef int i32;
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long long usize;
#include "compat_gui.h"

static int g_falhas;

static void ok(int cond, const char *msg) {
    if (cond) return;
    fprintf(stderr, "FALHA: %s\n", msg);
    g_falhas++;
}

/* ---- hospedeiro do JIT (Linux). wprotect devolve 0: a rede de SMC fica desligada de propósito,
 * para o teste de invalidação distinguir "não invalidou" de "a proteção refez a tradução". */
void *a64_host_jit_alloc(a64_u64 len) {
    void *p = mmap(NULL, (size_t)len, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
int a64_host_jit_wprotect(a64_u64 page, int protect) {
    (void)page;
    (void)protect;
    return 0;
}
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) {
    (void)c;
    a64_invalidate(addr, len);
}
void a64_host_yield(a64_cpu *c) { (void)c; }
a64_u64 a64_host_cntvct(void) { return 0; }

static int peek_mem(void *ctx, a64rt_u64 addr, void *dst, a64rt_u64 n) {
    (void)ctx;
    if (!addr || !dst || !n) return 0;
    memcpy(dst, (const void *)(uintptr_t)addr, (size_t)n);
    return 1;
}

static int nspans(long nr, a64rt_u64 x0, a64rt_u64 x1, a64rt_u64 x2, a64rt_u64 x3, a64rt_i64 ret,
                  a64rt_span *o, int max) {
    return a64rt_guest_writes(nr, x0, x1, x2, x3, 0, ret, peek_mem, NULL, o, max);
}

static int tem(const a64rt_span *o, int n, a64rt_u64 addr, a64rt_u64 len) {
    for (int i = 0; i < n; i++)
        if (o[i].addr == addr && o[i].len == len) return 1;
    return 0;
}

static void teste_intervalos(void) {
    a64rt_span o[4];
    int n;
    const a64rt_u64 buf = 0x5000;

    n = nspans(SYS_READ, 1, buf, 32, 0, 5, o, 4);
    ok(n == 1 && tem(o, n, buf, 5), "read com bytes grava o buffer");
    n = nspans(SYS_READ, 1, buf, 32, 0, 0, o, 4);
    ok(n == 0, "read com retorno 0 não grava");
    n = nspans(SYS_READ, 1, buf, 32, 0, -1, o, 4);
    ok(n == 0, "read com erro não grava");
    n = nspans(SYS_PREAD, 1, buf, 8, 0, 8, o, 4);
    ok(n == 1 && tem(o, n, buf, 8), "pread grava o buffer");
    n = nspans(SYS_WRITE, 1, buf, 8, 0, 8, o, 4);
    ok(n == 0, "write não grava no guest");

    n = nspans(SYS_STAT, 0, buf, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(mos_stat_t)), "stat grava mos_stat_t");
    n = nspans(SYS_PIPE, buf, 0, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, 8), "pipe grava o par de fds em x0");
    n = nspans(SYS_GETCWD, buf, 64, 0, 0, 3, o, 4);
    ok(n == 1 && tem(o, n, buf, 4), "getcwd grava o NUL (retorno + 1)");

    n = nspans(SYS_NET, MOS_NET_RECV, 0, buf, 0, 4, o, 4);
    ok(n == 1 && tem(o, n, buf, 4), "recv grava em x2");
    n = nspans(SYS_NET, MOS_NET_RECV, 0, buf, 0, 0, o, 4);
    ok(n == 0, "recv sem bytes não grava");

    mos_net_dgram_t dg;
    memset(&dg, 0, sizeof dg);
    static unsigned char payload[16];
    dg.buf = (a64rt_u64)(uintptr_t)payload;
    dg.len = 8;
    n = nspans(SYS_NET, MOS_NET_UDP_RECVFROM, 0, (a64rt_u64)(uintptr_t)&dg, 0, 8, o, 4);
    ok(n == 2 && tem(o, n, (a64rt_u64)(uintptr_t)&dg, sizeof dg) && tem(o, n, dg.buf, 8),
       "udp recvfrom grava a struct e o payload");

    n = nspans(SYS_DYLD, MOS_DYLD_ERROR, buf, 64, 0, 10, o, 4);
    ok(n == 1 && tem(o, n, buf, 64) && !tem(o, n, MOS_DYLD_ERROR, 64),
       "dyld error grava x1, não o número da operação");
    n = nspans(SYS_DYLD, MOS_DYLD_IMAGE, 0, buf, 80, 0x1000, o, 4);
    ok(n == 1 && tem(o, n, buf, 80), "dyld image grava o caminho em x2");

    n = nspans(SYS_SIG, MOS_SIGOP_ALTSTACK, 0, buf, 0, -MOS_EINVAL, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(mos_sigstack_t)), "altstack grava a pilha antiga mesmo com erro");
    n = nspans(SYS_SIG, MOS_SIGOP_ACTION, 0, 0, buf, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(mos_sigaction_t)), "sigaction grava a antiga em x3");

    n = nspans(SYS_TTY, MOS_TTY_IOCTL, 1, 0x40487413u, buf, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, 72), "TIOCGETA tem 72 bytes");

    unsigned char corpo[128];
    memset(corpo, 0, sizeof corpo);
    mos_mach_msg_args_t msg;
    memset(&msg, 0, sizeof msg);
    msg.msg = (a64rt_u64)(uintptr_t)corpo;
    msg.option = 2;
    msg.rcv_size = 128;
    n = nspans(SYS_MACH_MSG, (a64rt_u64)(uintptr_t)&msg, 0, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, msg.msg, 128), "mach_msg com receive grava o buffer");
    msg.option = 0;
    n = nspans(SYS_MACH_MSG, (a64rt_u64)(uintptr_t)&msg, 0, 0, 0, 0, o, 4);
    ok(n == 0, "mach_msg sem receive não grava o buffer");

    a64rt_u64 vec[8] = {0x1234, 0x1001};
    n = nspans(SYS_MACH_VM, MACH_VM_MAP, (a64rt_u64)(uintptr_t)vec, 0, 0, 0, o, 4);
    ok(n == 2 && tem(o, n, (a64rt_u64)(uintptr_t)vec, 64) && tem(o, n, 0x1000, 0x2000),
       "mach_vm_map devolve o vetor e a faixa alinhada");

    n = nspans(SYS_THREADCTL, MOS_THR_GETSTATE, 1, buf, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, 272), "getstate grava 272 bytes");

    n = nspans(SYS_IMGATTACH, 0, buf, 0, 0, 10, o, 4);
    ok(n == 0, "imgattach sem capacidade não grava");
    n = nspans(SYS_IMGATTACH, 0, buf, 2000, 0, 10, o, 4);
    ok(n == 1 && tem(o, n, buf, 1024), "imgattach cabe em 1024");

    n = nspans(SYS_LS, 0, buf, 0, 0, -MOS_EFAULT, o, 4);
    ok(n == 0, "launchd com EFAULT não grava");
    n = nspans(SYS_LS, 0, buf, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(mos_ls_req_t)), "launchd grava o pedido inteiro");

    n = nspans(SYS_MACH_VM, MACH_VM_MAP, (a64rt_u64)(uintptr_t)vec, 0, 0, 0, o, 1);
    ok(n == -1, "faixa que não cabe pede invalidação total");

    n = nspans(GUI_POLL, buf, 0, 0, 0, 1, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_event_t)), "GUI_POLL grava o evento em x0");
    n = nspans(GUI_POLL, buf, 0, 0, 0, 0, o, 4);
    ok(n == 0, "GUI_POLL sem evento não grava");
    n = nspans(GUI_MEASURE, 0, 0, buf, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_metrics_t)), "GUI_MEASURE grava as métricas em x2");
    n = nspans(GUI_WIN_INFO, 1, buf, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_wininfo_t)), "GUI_WIN_INFO grava em x1");
    n = nspans(GUI_SCREEN, buf, 0, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_wininfo_t)), "GUI_SCREEN grava em x0");
    n = nspans(GUI_DECODE_IMAGE, 0, 0, buf, 100, 40, o, 4);
    ok(n == 1 && tem(o, n, buf, 40), "decode grava quando a capacidade cabe");
    n = nspans(GUI_DECODE_IMAGE, 0, 0, buf, 10, 40, o, 4);
    ok(n == 0, "decode sem capacidade não grava");

    n = nspans(GUI_DEVICE, DEV_PAD_GET, 0, buf, 0, 1, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_gamepad_t)), "gamepad grava o estado em x2");
    n = nspans(GUI_DEVICE, DEV_PAD_GET, 0, 0, 0, 1, o, 4);
    ok(n == 0, "gamepad sem ponteiro não grava");
    n = nspans(GUI_DEVICE, DEV_AUDIO_STATE, 1, buf, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_audio_state_t)), "áudio grava o estado em x2");
    n = nspans(GUI_DEVICE, DEV_AUDIO_INFO, buf, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_audio_state_t)), "info de áudio grava em x1 mesmo com retorno 0");
    n = nspans(GUI_DEVICE, DEV_SCREEN_CAPTURE, 0, 0, buf, (a64rt_i64)(2ull | (3ull << 32)), o, 4);
    ok(n == 1 && tem(o, n, buf, 24), "captura de tela grava w*h*4");
    n = nspans(GUI_DEVICE, DEV_SCREEN_CAPTURE, 0, 0, 0, (a64rt_i64)(2ull | (3ull << 32)), o, 4);
    ok(n == 0, "captura sem buffer não grava");
    n = nspans(GUI_DEVICE, DEV_WINDOW_LIST, 0, buf, 0, 1, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_cgwindow_t)), "lista de janelas grava em x2");
    n = nspans(GUI_DEVICE, DEV_BT_DEVICE, 0, buf, 0, 1, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_bt_device_t)), "BLE grava o dispositivo em x2");
    n = nspans(GUI_DEVICE, DEV_BT_ATT_RECV, buf, 32, 0, 5, o, 4);
    ok(n == 1 && tem(o, n, buf, 5), "ATT grava os bytes recebidos em x1");
    n = nspans(GUI_DEVICE, DEV_BT_LINK, buf, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_bt_link_t)), "link BLE grava em x1");

    n = a64rt_guest_writes(GUI_FONT, GF_TABLE, 0, 0, buf, 10, 40, peek_mem, NULL, o, 4);
    ok(n == 1 && tem(o, n, buf, 10), "tabela de fonte grava min(tamanho, capacidade de x4)");
    n = a64rt_guest_writes(GUI_FONT, GF_TABLE, 0, 0, buf, 100, 40, peek_mem, NULL, o, 4);
    ok(n == 1 && tem(o, n, buf, 40), "tabela de fonte grava o tamanho quando a capacidade sobra");
    n = a64rt_guest_writes(GUI_FONT, GF_TABLE, 0, 0, 0, 100, 40, peek_mem, NULL, o, 4);
    ok(n == 0, "tabela de fonte sem buffer não grava");
    n = nspans(GUI_FONT, GF_METRICS, 0, 0, buf, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(gui_font_metrics_t)), "métricas da fonte gravam a struct");

    static unsigned char alfa[32];
    gui_glyph_req_t gl;
    memset(&gl, 0, sizeof gl);
    gl.buf = (u64)(uintptr_t)alfa;
    gl.cap = 32;
    n = nspans(GUI_FONT, GF_RASTER, 0, 0, (a64rt_u64)(uintptr_t)&gl, 16, o, 4);
    ok(n == 2 && tem(o, n, (a64rt_u64)(uintptr_t)&gl, sizeof gl) && tem(o, n, gl.buf, 16),
       "raster grava a struct e o bitmap");
    gl.cap = 8;
    n = nspans(GUI_FONT, GF_RASTER, 0, 0, (a64rt_u64)(uintptr_t)&gl, 16, o, 4);
    ok(n == 2 && tem(o, n, gl.buf, 8), "raster corta o bitmap na capacidade");

    n = nspans(SYS_WEB, MOS_WEB_PAINT, 1, buf, 64, 16, o, 4);
    ok(n == 1 && tem(o, n, buf, 16), "web paint grava os bytes em x2");
    n = nspans(SYS_WEB, MOS_WEB_INFO, 1, buf, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(mos_web_info_t)), "web info grava a struct");
    n = nspans(SYS_WEB, MOS_WEB_TEXT, 1, buf, 4, 10, o, 4);
    ok(n == 1 && tem(o, n, buf, 4), "web text grava min(tamanho, capacidade)");
    n = nspans(SYS_WEB, MOS_WEB_RESULT, 1, buf, 8, 6, o, 4);
    ok(n == 1 && tem(o, n, buf, 6), "web result grava o resultado");
    n = nspans(SYS_WEB, MOS_WEB_RESULT, 1, buf, 8, -MOS_EINVAL, o, 4);
    ok(n == 0, "web result que não coube não grava");
    n = nspans(SYS_WEB, MOS_WEB_HISTORY, 1, 0, buf, 1, o, 4);
    ok(n == 1 && tem(o, n, buf, sizeof(mos_web_hist_t)), "web history grava em x3");

    static unsigned char evout[8];
    mos_web_eval_t ev;
    memset(&ev, 0, sizeof ev);
    ev.out = (unsigned long long)(uintptr_t)evout;
    ev.len = 4;
    ev.cap = 8;
    n = nspans(SYS_WEB, MOS_WEB_EVAL, 1, (a64rt_u64)(uintptr_t)&ev, 0, 4, o, 4);
    ok(n == 2 && tem(o, n, (a64rt_u64)(uintptr_t)&ev, sizeof ev) && tem(o, n, ev.out, 4),
       "web eval grava a struct e o texto que coube");
    ev.len = 20;
    n = nspans(SYS_WEB, MOS_WEB_EVAL, 1, (a64rt_u64)(uintptr_t)&ev, 0, 20, o, 4);
    ok(n == 1 && tem(o, n, (a64rt_u64)(uintptr_t)&ev, sizeof ev) && !tem(o, n, ev.out, 20),
       "web eval sem espaço não grava o texto");

    static unsigned char corpo_msg[16];
    mos_web_msg_t wm;
    memset(&wm, 0, sizeof wm);
    wm.body = (unsigned long long)(uintptr_t)corpo_msg;
    wm.len = 6;
    wm.cap = 16;
    n = nspans(SYS_WEB, MOS_WEB_TAKEMSG, 1, (a64rt_u64)(uintptr_t)&wm, 0, 1, o, 4);
    ok(n == 2 && tem(o, n, (a64rt_u64)(uintptr_t)&wm, sizeof wm) && tem(o, n, wm.body, 6),
       "web takemsg grava a struct e o corpo");
    n = nspans(SYS_WEB, MOS_WEB_TAKEMSG, 1, (a64rt_u64)(uintptr_t)&wm, 0, -MOS_EINVAL, o, 4);
    ok(n == 1 && tem(o, n, (a64rt_u64)(uintptr_t)&wm, sizeof wm) && !tem(o, n, wm.body, 6),
       "web takemsg que não cabe grava só a struct");

    /* descritor de porta (12 bytes) e depois um OOL: o passo não pode ser 16 para os dois */
    unsigned char oolmsg[64];
    memset(oolmsg, 0, sizeof oolmsg);
    oolmsg[3] = 0x80;                                          /* MSGH_COMPLEX */
    oolmsg[24] = 2;                                            /* dois descritores */
    oolmsg[28 + 11] = 0;                                       /* porta */
    const a64rt_u64 ool_at = 0xABC000;
    memcpy(oolmsg + 40, &ool_at, 8);
    oolmsg[40 + 11] = 1;                                       /* OOL */
    uint32_t osz = 4;
    memcpy(oolmsg + 40 + 12, &osz, 4);
    memset(&msg, 0, sizeof msg);
    msg.msg = (a64rt_u64)(uintptr_t)oolmsg;
    msg.option = 2;
    msg.rcv_size = sizeof oolmsg;
    n = nspans(SYS_MACH_MSG, (a64rt_u64)(uintptr_t)&msg, 0, 0, 0, 0, o, 4);
    ok(n == 2 && tem(o, n, msg.msg, sizeof oolmsg) && tem(o, n, ool_at, 4096),
       "OOL depois de uma porta invalida a página mapeada");

    oolmsg[24] = 65;                                           /* o kernel não entrega mais que 64 descritores */
    n = nspans(SYS_MACH_MSG, (a64rt_u64)(uintptr_t)&msg, 0, 0, 0, 0, o, 4);
    ok(n == -1, "descritores demais pedem invalidação total");
}

static void teste_pilha(void) {
    a64rt_hstack p;
    const a64rt_u64 stack = 0x100000;
    ok(!a64rt_hstack_plan(0, stack, &p), "mmap nulo não vira pilha");
    ok(!a64rt_hstack_plan(0x1000, 0x3000, &p), "tamanho que não é potência de dois é recusado");
    ok(a64rt_hstack_plan(0x200000, stack, &p), "base já alinhada é aceita");
    ok(p.use == 0x200000 && p.free_base == 0x200000 && p.free_len == 2 * stack,
       "base alinhada usa o começo e solta o bloco inteiro");
    ok(a64rt_hstack_plan(0x1000, stack, &p), "base de 4 KiB é aceita");
    ok(p.use == 0x100000, "o uso sobe para o alinhamento de 1 MiB");
    ok(p.free_base == 0x1000 && p.free_len == 2 * stack, "a liberação é a faixa crua, não o meio");
    ok(p.use >= p.free_base && p.use + stack <= p.free_base + p.free_len, "a pilha usada cabe na faixa liberável");
}

/* ---- JIT: falta e invalidação --------------------------------------------------------------- */
static __thread sigjmp_buf g_salto;
static __thread volatile a64_cpu *g_cpu_falha;
static __thread volatile sig_atomic_t g_espera_falha;
static __thread volatile sig_atomic_t g_recuperou;

static void preencher_host(a64_hostctx *h, ucontext_t *uc) {
    static const int reg[16] = {
        REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
        REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15
    };
    memset(h, 0, sizeof *h);
    for (int i = 0; i < 16; i++) h->gpr[i] = (a64_u64)uc->uc_mcontext.gregs[reg[i]];
    h->rip = (a64_u64)uc->uc_mcontext.gregs[REG_RIP];
    h->rflags = (a64_u64)uc->uc_mcontext.gregs[REG_EFL];
}

static void no_segv(int sig, siginfo_t *si, void *ctx) {
    static __thread int profundidade;
    a64_cpu *cpu = (a64_cpu *)g_cpu_falha;
    if (profundidade++ > 0 || !g_espera_falha || !cpu) {
        static const char m[] = "sinal fora do teste de falta\n";
        if (write(2, m, sizeof m - 1) < 0) { }
        (void)sig;
        (void)si;
        _exit(97);
    }
    a64_hostctx h;
    preencher_host(&h, (ucontext_t *)ctx);
    g_recuperou = a64_jit_recover(cpu, &h) ? 1 : 0;
    profundidade = 0;
    siglongjmp(g_salto, 1);
}

static void *pagina(int prot, size_t n) {
    void *p = mmap(NULL, n, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return p;
}

static int rodar(a64_cpu *c, uint32_t *codigo) {
    c->pc = (a64_u64)(uintptr_t)codigo;
    return a64_run(c, A64_ENGINE_JIT);
}

static void teste_invalidar(void) {
    uint32_t *codigo = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    codigo[0] = 0x52800060u; /* movz w0, #3 */
    codigo[1] = 0xD4200000u; /* brk #0 */
    a64_cpu *c = calloc(1, sizeof *c);
    if (!c) exit(1);
    int r = rodar(c, codigo);
    ok(r == A64_EXIT_BRK && c->x[0] == 3, "primeira execução devolve 3");
    ok(a64_jit_has_code((a64_u64)(uintptr_t)codigo, 4), "o bloco ficou no cache");
    codigo[0] = 0x528000E0u; /* movz w0, #7, sem invalidar */
    memset(c, 0, sizeof *c);
    r = rodar(c, codigo);
    ok(r == A64_EXIT_BRK && c->x[0] == 3, "sem invalidar, o bloco velho continua");
    a64_invalidate((a64_u64)(uintptr_t)codigo, 4);
    memset(c, 0, sizeof *c);
    r = rodar(c, codigo);
    ok(r == A64_EXIT_BRK && c->x[0] == 7, "depois de invalidar, vale o valor novo");

    /* o intervalo de um read sobre esse código é o mesmo que o runtime pediria */
    a64rt_span sp[2];
    int n = nspans(SYS_READ, 0, (a64rt_u64)(uintptr_t)codigo, 4, 0, 4, sp, 2);
    ok(n == 1 && sp[0].addr == (a64rt_u64)(uintptr_t)codigo && sp[0].len == 4, "read sobre o código cobre o bloco");
    free(c);
    a64_invalidate((a64_u64)(uintptr_t)codigo, 4096);
    munmap(codigo, 4096);
}

static int falhar_em(a64_cpu *c, uint32_t *codigo) {
    g_cpu_falha = c;
    g_espera_falha = 1;
    g_recuperou = 0;
    if (sigsetjmp(g_salto, 1) == 0) {
        rodar(c, codigo);
        g_espera_falha = 0;
        return 0; /* não faltou */
    }
    g_espera_falha = 0;
    a64_jit_abort(c);
    return 1;
}

static void teste_falta(void) {
    uint32_t *codigo = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    void *nada = pagina(PROT_NONE, 4096);
    codigo[0] = 0x91001C21u; /* add x1, x1, #7 */
    codigo[1] = 0xF9400002u; /* ldr x2, [x0] */
    codigo[2] = 0xD4200000u;
    a64_cpu *c = calloc(1, sizeof *c);
    if (!c) exit(1);
    c->x[0] = (a64_u64)(uintptr_t)nada;
    c->x[1] = 10;
    c->x[2] = 123;
    c->x[5] = 99;
    int faltou = falhar_em(c, codigo);
    ok(faltou && g_recuperou, "a falta no load recupera o bloco");
    ok(c->x[1] == 17, "x1 fica com o add que já completou");
    ok(c->pc == (a64_u64)(uintptr_t)codigo + 4, "o pc volta na instrução que faltou");
    ok(c->x[2] == 123, "o destino do load que faltou não muda");
    ok(c->x[5] == 99, "registrador fora da instrução permanece");

    a64_invalidate((a64_u64)(uintptr_t)codigo, 4096);
    codigo[0] = 0xF100143Fu; /* cmp x1, #5  (subs xzr, x1, #5) */
    codigo[1] = 0xF9400002u;
    codigo[2] = 0xD4200000u;
    memset(c, 0, sizeof *c);
    c->x[0] = (a64_u64)(uintptr_t)nada;
    c->x[1] = 5;
    c->x[2] = 123;
    faltou = falhar_em(c, codigo);
    ok(faltou && g_recuperou, "a falta depois do cmp recupera");
    ok((c->nzcv & A64_Z) != 0, "o Z do cmp sobrevive à falta");

    a64_hostctx fora;
    memset(&fora, 0, sizeof fora);
    fora.rip = (a64_u64)(uintptr_t)&teste_falta;
    c->x[0] = 1;
    ok(!a64_jit_recover(c, &fora) && c->x[0] == 1, "rip fora do JIT não recupera e não mexe nos registradores");

    free(c);
    a64_invalidate((a64_u64)(uintptr_t)codigo, 4096);
    munmap(codigo, 4096);
    munmap(nada, 4096);
}

struct giro {
    volatile uint64_t publicado;
    volatile uint32_t parar;
    int saiu;
    a64_u64 x3;
};

static void *giro_thread(void *arg) {
    struct giro *g = arg;
    uint32_t *codigo = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    codigo[0] = 0x91000463u; /* add x3, x3, #1 */
    codigo[1] = 0xF90000A3u; /* str x3, [x5] */
    codigo[2] = 0xB9400044u; /* ldr w4, [x2] */
    codigo[3] = 0x34FFFFA4u; /* cbz w4, add (imm19 = -3) */
    codigo[4] = 0xD4200000u;
    a64_cpu *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->x[2] = (a64_u64)(uintptr_t)&g->parar;
    c->x[5] = (a64_u64)(uintptr_t)&g->publicado;
    g->saiu = rodar(c, codigo);
    g->x3 = c->x[3];
    free(c);
    a64_invalidate((a64_u64)(uintptr_t)codigo, 4096);
    munmap(codigo, 4096);
    return NULL;
}

static void teste_concorrente(void) {
    struct giro g;
    memset(&g, 0, sizeof g);
    pthread_t th;
    ok(pthread_create(&th, NULL, giro_thread, &g) == 0, "a thread do laço sobe");
    struct timespec t0, agora;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (g.publicado < 1000) {
        clock_gettime(CLOCK_MONOTONIC, &agora);
        if (agora.tv_sec - t0.tv_sec > 3) break;
    }
    ok(g.publicado >= 1000, "o laço no JIT publica o contador");

    uint32_t *codigo = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    void *nada = pagina(PROT_NONE, 4096);
    codigo[0] = 0x91001C21u; /* add x1, x1, #7 */
    codigo[1] = 0xF9400002u; /* ldr x2, [x0] */
    codigo[2] = 0xD4200000u;
    a64_cpu *c = calloc(1, sizeof *c);
    if (!c) exit(1);
    c->x[0] = (a64_u64)(uintptr_t)nada;
    c->x[1] = 40;
    c->x[2] = 7;
    int faltou = falhar_em(c, codigo);
    ok(faltou && g_recuperou && c->x[1] == 47, "a falta da outra thread recupera o próprio x1");

    g.parar = 1;
    void *st = NULL;
    struct timespec limite;
    clock_gettime(CLOCK_REALTIME, &limite);
    limite.tv_sec += 3;
    int jr = pthread_timedjoin_np(th, &st, &limite);
    ok(jr == 0 && g.saiu == A64_EXIT_BRK && g.x3 >= 1000, "o laço termina com o próprio contador");
    free(c);
    a64_invalidate((a64_u64)(uintptr_t)codigo, 4096);
    munmap(codigo, 4096);
    munmap(nada, 4096);
}

/* reexecuta movz/brk. Sem invalidar, o bloco velho fica; a faixa devolvida pelo pedido faz valer o novo. */
static void reexecuta(uint32_t *codigo, const a64rt_span *sp, int nsp, const char *quem) {
    a64_cpu *c = calloc(1, sizeof *c);
    if (!c) exit(1);
    int r = rodar(c, codigo);
    ok(r == A64_EXIT_BRK && c->x[0] == 3, quem);
    codigo[0] = 0x528000E0u; /* movz w0, #7 */
    memset(c, 0, sizeof *c);
    r = rodar(c, codigo);
    char antigo[96];
    snprintf(antigo, sizeof antigo, "%s: sem invalidar, o bloco velho continua", quem);
    ok(r == A64_EXIT_BRK && c->x[0] == 3, antigo);
    int cobriu = 0;
    for (int i = 0; i < nsp; i++) {
        if (sp[i].addr == (a64rt_u64)(uintptr_t)codigo) {
            a64rt_invalidate_range(sp[i].addr, sp[i].len);
            cobriu = 1;
        }
    }
    ok(cobriu, quem);
    memset(c, 0, sizeof *c);
    r = rodar(c, codigo);
    char novo[96];
    snprintf(novo, sizeof novo, "%s: a reexecução vê o valor novo", quem);
    ok(r == A64_EXIT_BRK && c->x[0] == 7, novo);
    free(c);
}

static void teste_escrita_host(void) {
    uint32_t *gui = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    gui[0] = 0x52800060u;
    gui[1] = 0xD4200000u;
    a64rt_span sp[4];
    int n = nspans(GUI_POLL, (a64rt_u64)(uintptr_t)gui, 0, 0, 0, 1, sp, 4);
    ok(n == 1 && sp[0].len == sizeof(gui_event_t), "o pedido GUI cobre o código traduzido");
    reexecuta(gui, sp, n, "escrita GUI");
    a64_invalidate((a64_u64)(uintptr_t)gui, 4096);
    munmap(gui, 4096);

    uint32_t *ool = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    ool[0] = 0x52800060u;
    ool[1] = 0xD4200000u;
    unsigned char corpo[64];
    memset(corpo, 0, sizeof corpo);
    corpo[3] = 0x80;
    corpo[24] = 1;
    a64rt_u64 dest = (a64rt_u64)(uintptr_t)ool;
    memcpy(corpo + 28, &dest, 8);
    corpo[28 + 11] = 1;
    uint32_t sz = 4;
    memcpy(corpo + 28 + 12, &sz, 4);
    mos_mach_msg_args_t msg;
    memset(&msg, 0, sizeof msg);
    msg.msg = (a64rt_u64)(uintptr_t)corpo;
    msg.option = 2;
    msg.rcv_size = sizeof corpo;
    n = nspans(SYS_MACH_MSG, (a64rt_u64)(uintptr_t)&msg, 0, 0, 0, 0, sp, 4);
    ok(n == 2 && tem(sp, n, dest, 4096), "o descritor OOL cobre a página do código");
    reexecuta(ool, sp, n, "escrita OOL");
    a64_invalidate((a64_u64)(uintptr_t)ool, 4096);
    munmap(ool, 4096);
}

struct laco {
    uint32_t *codigo;
    volatile uint32_t visto;
    volatile uint32_t parar;
    volatile uint64_t passos;
    int saiu;
};

static void *laco_thread(void *arg) {
    struct laco *g = arg;
    uint32_t *codigo = g->codigo;
    a64_cpu *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->x[1] = (a64_u64)(uintptr_t)&g->visto;
    c->x[3] = (a64_u64)(uintptr_t)&g->parar;
    c->x[5] = (a64_u64)(uintptr_t)&g->passos;
    g->saiu = rodar(c, codigo);
    free(c);
    return NULL;
}

static int esperou(volatile uint64_t *p, uint64_t alvo, int segundos) {
    struct timespec t0, agora;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (*p < alvo) {
        clock_gettime(CLOCK_MONOTONIC, &agora);
        if (agora.tv_sec - t0.tv_sec > segundos) return 0;
    }
    return 1;
}

static void teste_laco_safepoint(void) {
    struct laco g;
    memset(&g, 0, sizeof g);
    g.codigo = pagina(PROT_READ | PROT_WRITE | PROT_EXEC, 4096);
    /* prefixo fora do laço: o traço começa aqui, e o cbz volta para o movz, não para o início.
     * Um safepoint só em alvo <= pc0 não vê este desvio. */
    g.codigo[0] = 0x52800026u; /* movz w6, #1 (não é x5: x5 é o ponteiro do contador) */
    g.codigo[1] = 0x52800060u; /* movz w0, #3 */
    g.codigo[2] = 0xB9000020u; /* str w0, [x1] */
    g.codigo[3] = 0x91000484u; /* add x4, x4, #1 */
    g.codigo[4] = 0xF90000A4u; /* str x4, [x5] */
    g.codigo[5] = 0xB9400062u; /* ldr w2, [x3] */
    g.codigo[6] = 0x34FFFF62u; /* cbz w2, movz w0 (imm19 = -5) */
    g.codigo[7] = 0xD4200000u;

    pthread_t th;
    ok(pthread_create(&th, NULL, laco_thread, &g) == 0, "a thread do safepoint sobe");
    ok(esperou(&g.passos, 100000, 3) && g.visto == 3, "o laço traduzido publica o imediato 3");

    g.codigo[1] = 0x528000E0u; /* movz w0, #7, ainda sem invalidar */
    uint64_t marca = g.passos;
    ok(esperou(&g.passos, marca + 10000, 2) && g.visto == 3,
       "dentro do bloco, o imediato novo ainda não vale");

    /* o mesmo caminho do runtime: o pedido GUI (ou o OOL) produz a faixa, e um só ponto invalida */
    a64rt_span sp[4];
    int n = nspans(GUI_POLL, (a64rt_u64)(uintptr_t)g.codigo, 0, 0, 0, 1, sp, 4);
    ok(n == 1 && sp[0].addr == (a64rt_u64)(uintptr_t)g.codigo, "a faixa do GUI cobre o laço");
    a64rt_invalidate_spans(sp, n);
    struct timespec t0, agora;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (g.visto != 7) {
        clock_gettime(CLOCK_MONOTONIC, &agora);
        if (agora.tv_sec - t0.tv_sec > 2) break;
    }
    ok(g.visto == 7, "no próximo desvio para trás o laço vê o valor novo");

    g.parar = 1;
    struct timespec limite;
    clock_gettime(CLOCK_REALTIME, &limite);
    limite.tv_sec += 3;
    int jr = pthread_timedjoin_np(th, NULL, &limite);
    ok(jr == 0 && g.saiu == A64_EXIT_BRK, "o laço sai no brk depois do safepoint");
    a64_invalidate((a64_u64)(uintptr_t)g.codigo, 4096);
    munmap(g.codigo, 4096);
}

int main(void) {
    teste_intervalos();
    teste_pilha();

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = no_segv;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) != 0 || sigaction(SIGBUS, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }
    teste_invalidar();
    teste_escrita_host();
    teste_laco_safepoint();
    teste_falta();
    teste_concorrente();

    if (g_falhas) {
        fprintf(stderr, "%d falha(s)\n", g_falhas);
        return 1;
    }
    printf("a64hostcheck: ok\n");
    return 0;
}

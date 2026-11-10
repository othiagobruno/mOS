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
    return a64rt_guest_writes(nr, x0, x1, x2, x3, ret, peek_mem, NULL, o, max);
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

    mos_mach_msg_args_t msg;
    memset(&msg, 0, sizeof msg);
    msg.msg = 0x8000;
    msg.option = 2;
    msg.rcv_size = 128;
    n = nspans(SYS_MACH_MSG, (a64rt_u64)(uintptr_t)&msg, 0, 0, 0, 0, o, 4);
    ok(n == 1 && tem(o, n, 0x8000, 128), "mach_msg com receive grava o buffer");
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
    teste_falta();
    teste_concorrente();

    if (g_falhas) {
        fprintf(stderr, "%d falha(s)\n", g_falhas);
        return 1;
    }
    printf("a64hostcheck: ok\n");
    return 0;
}

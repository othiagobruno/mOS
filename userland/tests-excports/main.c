/* mOS — portas de exceção Mach, como o crashpad de todo Electron as usa (kernel/proc/machipc.c, machipc_raise).
 *
 * 1. thread_set_exception_ports(EXC_MASK_BAD_ACCESS): a falta vai como mach_exception_raise (2405) a uma thread do
 *    mesmo processo, que mapeia a página e responde KERN_SUCCESS; a instrução é refeita e lê o valor.
 * 2. a mesma falta com resposta KERN_FAILURE: segue para o tratador de SIGSEGV.
 * 3. EXC_CRASH entre processos: o filho registra a porta de exceção da task e cai. O pai recebe a mensagem com as
 *    portas da thread e da task, confere o pid, lê a memória do filho (mach_vm_read), lista as threads
 *    (task_threads), lê o estado da thread que caiu (thread_get_state) e responde; o filho então morre por SIGSEGV.
 * Saída: uma linha "ok" por checagem e "excports: N/M ok"; código de saída = falhas.
 */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <mach/mach.h>
/* mach/mach_vm.h e servers/bootstrap.h não vêm no SDK do zig */
extern kern_return_t mach_vm_read(vm_map_t, mach_vm_address_t, mach_vm_size_t, vm_offset_t *, mach_msg_type_number_t *);
extern kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
extern kern_return_t mach_vm_region(vm_map_t, mach_vm_address_t *, mach_vm_size_t *, vm_region_flavor_t, vm_region_info_t,
                                    mach_msg_type_number_t *, mach_port_t *);
extern mach_port_t bootstrap_port;
extern kern_return_t bootstrap_check_in(mach_port_t bp, const char *name, mach_port_t *sp);
extern kern_return_t bootstrap_look_up(mach_port_t bp, const char *name, mach_port_t *sp);

static int fails, total;
#define CHECK(c, ...) do { total++; if (c) printf("  ok  " __VA_ARGS__); else { fails++; printf("  FALHOU  " __VA_ARGS__); } printf("\n"); } while (0)

/* mach_exception_raise (mach_exc.defs, pack(4)): portas da thread e da task, NDR, exceção e dois códigos de 64 bits */
#pragma pack(4)
typedef struct {
    mach_msg_header_t h;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t thread, task;
    NDR_record_t ndr;
    exception_type_t exc;
    mach_msg_type_number_t ncode;
    int64_t code[2];
    char pad[512];
} exc_req_t;
typedef struct {
    mach_msg_header_t h;
    NDR_record_t ndr;
    kern_return_t ret;
} exc_rep_t;
#pragma pack()

static kern_return_t reply(const exc_req_t *q, kern_return_t ret) {
    exc_rep_t r;
    memset(&r, 0, sizeof r);
    r.h.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(q->h.msgh_bits), 0);
    r.h.msgh_remote_port = q->h.msgh_remote_port;
    r.h.msgh_size = sizeof r;
    r.h.msgh_id = q->h.msgh_id + 100;
    r.ndr = NDR_record;
    r.ret = ret;
    return mach_msg(&r.h, MACH_SEND_MSG, sizeof r, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

static mach_port_t new_port(void) {
    mach_port_t p = MACH_PORT_NULL;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p);
    mach_port_insert_right(mach_task_self(), p, p, MACH_MSG_TYPE_MAKE_SEND);
    return p;
}

/* ---- 1 e 2: tratador na outra thread do mesmo processo */
static mach_port_t g_port;
static volatile char *g_hole;
static kern_return_t g_answer;
static struct { int got, exc, id, again; int64_t addr; uint64_t rip; int pidok; } g_seen;

static void *handler(void *arg) {
    (void)arg;
    /* atende até 3 mensagens: se a página não ficou boa, a mesma falta volta e é recusada (sem travar o teste) */
    for (int k = 0; k < 3; k++) {
        exc_req_t q;
        memset(&q, 0, sizeof q);
        kern_return_t kr = mach_msg(&q.h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof q, g_port, k ? 2000 : 10000, MACH_PORT_NULL);
        if (kr != KERN_SUCCESS) return 0;
        if (!g_seen.got) {
            g_seen.got = 1; g_seen.id = q.h.msgh_id; g_seen.exc = q.exc; g_seen.addr = q.code[1];
            int pid = 0;
            g_seen.pidok = pid_for_task(q.task.name, &pid) == KERN_SUCCESS && pid == getpid();
            x86_thread_state64_t st;
            mach_msg_type_number_t cnt = x86_THREAD_STATE64_COUNT;
            if (thread_get_state(q.thread.name, x86_THREAD_STATE64, (thread_state_t)&st, &cnt) == KERN_SUCCESS) g_seen.rip = st.__rip;
            if (g_answer == KERN_SUCCESS && mprotect((void *)g_hole, 4096, PROT_READ | PROT_WRITE) == 0)
                *(volatile int *)g_hole = 42;      /* a instrução que faltou é refeita e lê o 42 */
            reply(&q, g_answer);
        } else {
            g_seen.again++;
            reply(&q, KERN_FAILURE);
        }
        if (g_answer != KERN_SUCCESS) return 0;
    }
    return 0;
}

static sigjmp_buf g_jb;
static void on_segv(int sig) { (void)sig; siglongjmp(g_jb, 1); }

static void teste_mesmo_processo(void) {
    g_port = new_port();
    /* reserva sem páginas (PROT_NONE): ler ali falta; o tratador libera com mprotect. Uma página para cada caso. */
    char *res = mmap(0, 65536, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    g_hole = res + 16384;
    kern_return_t kr = thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, g_port,
                                                  (exception_behavior_t)(EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES), THREAD_STATE_NONE);
    CHECK(kr == KERN_SUCCESS, "thread_set_exception_ports(EXC_MASK_BAD_ACCESS) = %d", kr);
    exception_mask_t masks[4]; mach_msg_type_number_t n = 4; mach_port_t ports[4]; exception_behavior_t bs[4]; thread_state_flavor_t fs[4];
    kr = thread_get_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, masks, &n, ports, bs, fs);
    CHECK(kr == KERN_SUCCESS && n == 1 && masks[0] == EXC_MASK_BAD_ACCESS && ports[0] != MACH_PORT_NULL &&
          (unsigned)bs[0] == (EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES), "thread_get_exception_ports devolve a porta (n %u)", n);

    /* 1: o tratador resolve */
    pthread_t th;
    g_answer = KERN_SUCCESS;
    memset(&g_seen, 0, sizeof g_seen);
    pthread_create(&th, 0, handler, 0);
    usleep(100000);
    int v = *(volatile int *)g_hole;               /* falta, vai ao tratador, que mapeia e responde */
    pthread_join(th, 0);
    CHECK(g_seen.got && g_seen.id == 2405 && g_seen.exc == EXC_BAD_ACCESS, "mach_exception_raise chegou (id %d, exc %d)", g_seen.id, g_seen.exc);
    CHECK(g_seen.addr == (int64_t)(uintptr_t)g_hole, "code[1] é o endereço da falta (%llx)", (long long)g_seen.addr);
    CHECK(g_seen.pidok, "a porta da task na mensagem é desta task (pid_for_task)");
    CHECK(g_seen.rip != 0, "thread_get_state da thread que faltou (rip %llx)", (unsigned long long)g_seen.rip);
    CHECK(v == 42 && !g_seen.again, "KERN_SUCCESS: a instrução foi refeita e leu o que o tratador escreveu (%d, repetições %d)", v, g_seen.again);

    /* 2: o tratador recusa: o SIGSEGV vem depois (outra página da reserva) */
    g_hole = res + 40960;
    g_answer = KERN_FAILURE;
    memset(&g_seen, 0, sizeof g_seen);
    signal(SIGSEGV, on_segv);
    signal(SIGBUS, on_segv);
    pthread_create(&th, 0, handler, 0);
    usleep(100000);
    int caiu = 0;
    if (!sigsetjmp(g_jb, 1)) (void)*(volatile int *)g_hole;
    else caiu = 1;
    pthread_join(th, 0);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    CHECK(g_seen.got && caiu, "KERN_FAILURE: a falta segue para o tratador de sinal (mensagem %d, sinal %d)", g_seen.got, caiu);
    thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
}

/* ---- 3: EXC_CRASH de outro processo */
static volatile uint64_t g_magic;

static void teste_crash(void) {
    mach_port_t svc = MACH_PORT_NULL;
    kern_return_t kr = bootstrap_check_in(bootstrap_port, "mos.teste.excports", &svc);
    CHECK(kr == KERN_SUCCESS && svc, "bootstrap_check_in da porta de exceção (%d)", kr);
    if (kr != KERN_SUCCESS) return;
    pid_t pid = fork();
    if (pid == 0) {
        mach_port_t p = MACH_PORT_NULL;
        if (bootstrap_look_up(bootstrap_port, "mos.teste.excports", &p) != KERN_SUCCESS) _exit(3);
        if (task_set_exception_ports(mach_task_self(), EXC_MASK_CRASH, p,
                                     (exception_behavior_t)(EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES), THREAD_STATE_NONE) != KERN_SUCCESS) _exit(4);
        g_magic = 0x6D4F53637261736ULL;            /* o pai lê isto pela porta da task */
        *(volatile int *)16 = 1;                   /* sem tratador de SIGSEGV: morre, e antes vem o EXC_CRASH */
        _exit(5);
    }
    exc_req_t q;
    memset(&q, 0, sizeof q);
    kr = mach_msg(&q.h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof q, svc, 15000, MACH_PORT_NULL);
    CHECK(kr == KERN_SUCCESS && q.h.msgh_id == 2405 && q.exc == EXC_CRASH, "EXC_CRASH do filho (kr 0x%x, id %d, exc %d)", kr, q.h.msgh_id, q.exc);
    if (kr != KERN_SUCCESS) { kill(pid, SIGKILL); waitpid(pid, 0, 0); return; }
    CHECK(((q.code[0] >> 24) & 0xFF) == SIGSEGV, "code[0] traz o SIGSEGV (0x%llx)", (long long)q.code[0]);
    int tp = 0;
    CHECK(pid_for_task(q.task.name, &tp) == KERN_SUCCESS && tp == pid, "pid_for_task da porta recebida = filho (%d / %d)", tp, pid);
    vm_offset_t data = 0;
    mach_msg_type_number_t dn = 0;
    kr = mach_vm_read(q.task.name, (mach_vm_address_t)(uintptr_t)&g_magic, 8, &data, &dn);
    uint64_t got = kr == KERN_SUCCESS && dn == 8 ? *(uint64_t *)data : 0;
    CHECK(got == 0x6D4F53637261736ULL, "mach_vm_read na memória do filho (kr %d, 0x%llx)", kr, (unsigned long long)got);
    uint64_t mine = 0;
    mach_vm_size_t outsz = 0;
    kr = mach_vm_read_overwrite(q.task.name, (mach_vm_address_t)(uintptr_t)&g_magic, 8, (mach_vm_address_t)(uintptr_t)&mine, &outsz);
    CHECK(kr == KERN_SUCCESS && mine == 0x6D4F53637261736ULL, "mach_vm_read_overwrite também (kr %d)", kr);
    mach_vm_address_t ra = (mach_vm_address_t)(uintptr_t)&g_magic;
    mach_vm_size_t rs = 0;
    vm_region_basic_info_data_64_t bi;
    mach_msg_type_number_t bc = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj = MACH_PORT_NULL;
    kr = mach_vm_region(q.task.name, &ra, &rs, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&bi, &bc, &obj);
    CHECK(kr == KERN_SUCCESS && rs > 0 && ra <= (mach_vm_address_t)(uintptr_t)&g_magic, "mach_vm_region na outra task (kr %d)", kr);
    thread_act_array_t th = 0;
    mach_msg_type_number_t tn = 0;
    kr = task_threads(q.task.name, &th, &tn);
    CHECK(kr == KERN_SUCCESS && tn >= 1, "task_threads do filho (kr %d, %u threads)", kr, tn);
    x86_thread_state64_t st;
    mach_msg_type_number_t sc = x86_THREAD_STATE64_COUNT;
    kr = thread_get_state(q.thread.name, x86_THREAD_STATE64, (thread_state_t)&st, &sc);
    CHECK(kr == KERN_SUCCESS && st.__rip != 0, "thread_get_state da thread que caiu (kr %d, rip %llx)", kr, (unsigned long long)st.__rip);
    reply(&q, KERN_SUCCESS);
    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    CHECK(w == pid && WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV, "depois da resposta o filho morre por SIGSEGV (status 0x%x)", status);
}

int main(void) {
    printf("excports: pid %d\n", getpid());
    teste_mesmo_processo();
    teste_crash();
    printf("excports: %d/%d ok\n", total - fails, total);
    return fails;
}

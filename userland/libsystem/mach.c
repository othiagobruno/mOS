/*
 * mOS libSystem — base Mach/Darwin para apps grandes (porte do VS Code/Electron, docs/porte-vscode.md).
 *
 * O mOS não tem portas Mach. Aqui elas são nomes (inteiros) que identificam a task, as threads e o
 * host, o suficiente para o código que só compara, guarda ou devolve esses nomes (o caso do Chromium
 * ao iniciar). Operações que exigiriam IPC de verdade devolvem erro em vez de fingir sucesso.
 */
#include "sys.h"
#include "libc.h"
#if defined(__aarch64__)
#include "a64page.h"
#endif

typedef u32 mach_port_t;
typedef int kern_return_t;

#define KERN_SUCCESS          0
#define KERN_INVALID_ARGUMENT 4
#define KERN_FAILURE          5
#define KERN_NOT_SUPPORTED    46

#define TASK_PORT   0x103u
#define HOST_PORT   0x203u
#define THREAD_BASE 0x1000u        /* porta da thread = THREAD_BASE + tid do kernel */

/* ---- portas Mach entre processos (kernel/proc/machipc.c, syscalls 80..95)
 * Agora as portas são de verdade: o kernel guarda os direitos e as filas, e mach_msg/bootstrap entre
 * processos passam por ele. Os nomes "especiais" (task 0x103, host 0x203, thread 0x1000+tid, semáforos
 * 0x20000+, entradas de memória 0x30000+) continuam resolvidos aqui; o kernel os trata como objetos
 * sem receptor quando aparecem numa mensagem. */
static long mach_port(u32 op, u64 *a) { return cx_syscall(SYS_MACH_PORT, (long)op, (long)a, 0, 0, 0); }

/* tamanho de página: são VARIÁVEIS no macOS (o Chromium faz CHECK(vm_page_size == 4096 || 16384)) */
#if defined(__aarch64__)   /* processo arm64: 16 KiB (A64_GUEST_PAGE). O x86_64 abaixo continua em 4 KiB. */
u64 vm_page_size = A64_GUEST_PAGE, vm_page_mask = A64_GUEST_PAGE - 1;
int vm_page_shift = A64_GUEST_PAGE_SHIFT;
u64 vm_kernel_page_size = A64_GUEST_PAGE, vm_kernel_page_mask = A64_GUEST_PAGE - 1;
int vm_kernel_page_shift = A64_GUEST_PAGE_SHIFT;
#else
u64 vm_page_size = 4096, vm_page_mask = 4095;
int vm_page_shift = 12;
u64 vm_kernel_page_size = 4096, vm_kernel_page_mask = 4095;
int vm_kernel_page_shift = 12;
#endif

/* a variável mach_task_self_ (lida pela macro mach_task_self() do SDK) fica em libc.c, com TASK_PORT */
mach_port_t mach_task_self(void) { return TASK_PORT; }
mach_port_t mach_host_self(void) { return HOST_PORT; }
mach_port_t mach_thread_self(void) { return THREAD_BASE + (u32)SYS0(SYS_GETTID); }

/* a porta de qualquer pthread_t: o bloco no formato do macOS guarda o tid do kernel (cx_pthread_tid, extra.c); o
 * pthread_t que o pthread_create devolve também. thread_suspend/thread_get_state usam essa porta (vscode_libc.c) */
long cx_pthread_tid(void *t);
mach_port_t pthread_mach_thread_np(void *t) { return THREAD_BASE + (u32)cx_pthread_tid(t); }

/* ---- direitos de porta: delegados ao kernel (machipc.c) */
kern_return_t mach_port_allocate(mach_port_t task, int right, mach_port_t *name) {
    (void)task;
    if (!name) return KERN_INVALID_ARGUMENT;
    u64 a[8] = {(u64)right, 0, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_ALLOCATE, a);
    if (r == KERN_SUCCESS) *name = (u32)a[2];
    return (kern_return_t)r;
}
kern_return_t mach_port_allocate_name(mach_port_t task, int right, mach_port_t name) {
    (void)task;
    u64 a[8] = {(u64)right, name, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_ALLOCATE, a);
}
kern_return_t mach_port_deallocate(mach_port_t task, mach_port_t name) {
    (void)task;
    u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_DEALLOCATE, a);
}
kern_return_t mach_port_mod_refs(mach_port_t task, mach_port_t name, int right, int delta) {
    (void)task;
    u64 a[8] = {name, (u64)right, (u64)(long)delta, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_MOD_REFS, a);
}
kern_return_t mach_port_insert_right(mach_port_t task, mach_port_t name, mach_port_t poly, int type) {
    (void)task;
    u64 a[8] = {name, poly, (u64)type, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_INSERT_RIGHT, a);
}
kern_return_t mach_port_extract_right(mach_port_t task, mach_port_t name, int disp, mach_port_t *out, u32 *out_type) {
    (void)task;
    u64 a[8] = {name, (u64)disp, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_EXTRACT_RIGHT, a);
    if (r == KERN_SUCCESS) { if (out) *out = (u32)a[2]; if (out_type) *out_type = (u32)a[3]; }
    return (kern_return_t)r;
}
kern_return_t mach_port_get_refs(mach_port_t task, mach_port_t name, int right, u32 *refs) {
    (void)task;
    u64 a[8] = {name, (u64)right, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_GET_REFS, a);
    if (r == KERN_SUCCESS && refs) *refs = (u32)a[2];
    return (kern_return_t)r;
}
kern_return_t mach_port_type(mach_port_t task, mach_port_t name, u32 *ptype) {
    (void)task;
    u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_TYPE, a);
    if (r == KERN_SUCCESS && ptype) *ptype = (u32)a[1];
    return (kern_return_t)r;
}
kern_return_t mach_port_insert_member(mach_port_t task, mach_port_t name, mach_port_t pset) {
    (void)task;
    u64 a[8] = {name, pset, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_INSERT_MEMBER, a);
}
kern_return_t mach_port_move_member(mach_port_t task, mach_port_t name, mach_port_t pset) {
    (void)task;
    u64 a[8] = {name, pset, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_MOVE_MEMBER, a);
}
kern_return_t mach_port_extract_member(mach_port_t task, mach_port_t name, mach_port_t pset) {
    (void)task;
    u64 a[8] = {name, pset, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_EXTRACT_MEMBER, a);
}
/* mach_port_request_notification(task, name, variante, sync, notify, notifyPoly, *previous) */
kern_return_t mach_port_request_notification(mach_port_t task, mach_port_t name, int variant, u32 sync,
                                             mach_port_t notify, int notifyPoly, mach_port_t *previous) {
    (void)task;
    u64 a[8] = {name, (u64)variant, sync, notify, (u64)notifyPoly, 0, 0, 0};
    long r = mach_port(MACH_OP_NOTIFY, a);
    if (previous) *previous = (u32)a[5];
    return (kern_return_t)r;
}
kern_return_t mach_port_set_attributes(mach_port_t task, mach_port_t name, int flavor, void *info, u32 count) {
    (void)task;
    u64 val = (flavor == 1 && info && count) ? *(u32 *)info : 0;   /* MACH_PORT_LIMITS_INFO: qlimit */
    u64 a[8] = {name, (u64)flavor, val, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_SET_ATTR, a);
}
kern_return_t mach_port_set_context(mach_port_t task, mach_port_t name, u64 context) {
    (void)task;
    u64 a[8] = {name, context, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_SET_CONTEXT, a);
}
kern_return_t mach_port_get_context(mach_port_t task, mach_port_t name, u64 *context) {
    (void)task;
    u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_GET_CONTEXT, a);
    if (context) *context = a[1];
    return (kern_return_t)r;
}
/* ---- memória: o mOS ainda não guarda um mapa de regiões; responde com a página que contém o
 * endereço, leitura/escrita (é o que o Chromium consulta antes de mexer na zona do malloc) */
#define KERN_INVALID_ADDRESS 1
#define VM_REGION_BASIC_INFO_64 9
#pragma pack(push, 4)
typedef struct {
    int protection, max_protection;
    u32 inheritance, shared, reserved;
    u64 offset;
    int behavior;
    u16 user_wired_count;
} vm_region_basic_info_64_t;
#pragma pack(pop)
/* registra no kernel a proteção pedida para [addr, addr+len) (mprotect em extra.c e mach_vm_protect chamam).
 * VM_PROT_* e PROT_* têm os mesmos bits (READ 1, WRITE 2, EXEC 4). Só registra: a PTE não muda. */
void cx_mach_record_prot(u64 addr, u64 len, int prot) {
    u64 q[8] = {addr, len, (u64)(prot & 7), 0, 0, 0, 0, 0};
    cx_syscall(SYS_MACH_VM, (long)MACH_VM_PROTECT, (long)q, 0, 0, 0);
}
kern_return_t mach_vm_region(mach_port_t task, u64 *addr, u64 *size, int flavor, void *info, u32 *count, mach_port_t *obj) {
    (void)task;
    if (obj) *obj = 0;
    if (!addr || !size || *addr < 0x400000000000ULL || *addr >= 0x440000000000ULL) return KERN_INVALID_ADDRESS;
    /* a proteção vem do kernel (machipc.c, MACH_VM_REGION): initprot dos segmentos Mach-O e o que o
     * mprotect/mach_vm_protect pediram. Sem registro: a página, RWX (o NX está desligado no mOS). */
    u64 q[8] = {*addr, 0, 0, 7, 7, 0, 0, 0};
    if (cx_syscall(SYS_MACH_VM, (long)MACH_VM_REGION, (long)q, 0, 0, 0) != KERN_SUCCESS) return KERN_INVALID_ADDRESS;
    *addr = q[1];
    *size = q[2];
#if defined(__aarch64__)
    a64_region_guest(addr, size, (unsigned)q[3]);
#endif
    if (flavor == VM_REGION_BASIC_INFO_64 && info && count && *count >= sizeof(vm_region_basic_info_64_t) / 4) {
        vm_region_basic_info_64_t *b = info;
        b->protection = (int)q[3];
        b->max_protection = (int)q[4];
        b->inheritance = 1;          /* VM_INHERIT_COPY */
        b->shared = b->reserved = 0;
        b->offset = 0;
        b->behavior = 0;
        b->user_wired_count = 0;
        *count = sizeof(vm_region_basic_info_64_t) / 4;
    }
    return KERN_SUCCESS;
}
int mprotect(void *a, size_t len, int prot);
kern_return_t mach_vm_protect(mach_port_t task, u64 addr, u64 size, int set_max, int prot) {
    (void)task;
    if (!size) return KERN_SUCCESS;
#if defined(__aarch64__)
    a64_span_t sp = a64_span(addr, size);
    if (sp.err) return sp.err == MOS_ENOMEM ? KERN_FAILURE : KERN_INVALID_ADDRESS;
    addr = sp.addr;
    size = sp.len;
#endif
    if (!set_max) cx_mach_record_prot(addr, size, prot);  /* o mprotect abaixo também registra; aqui cobre prot = 0 */
    /* o mesmo que mprotect: uma reserva com acesso passa a ter páginas sob demanda.
     * prot == 0 no x86_64 não desmapeia. No arm64, mprotect(PROT_NONE) tira as páginas
     * da página guest (guarda); set_max não mexe no mapeamento. */
    if (prot && mprotect((void *)addr, (size_t)size, prot) != 0) return KERN_INVALID_ADDRESS;
#if defined(__aarch64__)
    if (!set_max && !prot && mprotect((void *)addr, (size_t)size, 0) != 0) return KERN_INVALID_ADDRESS;
#endif
    return KERN_SUCCESS;
}
kern_return_t vm_protect(mach_port_t task, u64 addr, u64 size, int set_max, int prot) { return mach_vm_protect(task, addr, size, set_max, prot); }
kern_return_t vm_region_64(mach_port_t task, u64 *addr, u64 *size, int flavor, void *info, u32 *count, mach_port_t *obj) {
    return mach_vm_region(task, addr, size, flavor, info, count, obj);
}

/* ---- SecTask: não há AMFI. O objeto existe para o Chromium perguntar, e os entitlements
 * de JIT, biblioteca do bundle e rede vêm concedidos (kCFBooleanTrue, +1). O resto é NULL. */
extern void *kCFBooleanTrue;
const char *cx_cstr(void *o);
void *cx_str(const char *s);
void *objc_retain(void *o);
void *SecTaskCreateFromSelf(void *allocator) { (void)allocator; return cx_str("mOS-sectask"); }
void *SecTaskCopyValueForEntitlement(void *task, void *entitlement, void **error) {
    (void)task;
    if (error) *error = 0;
    const char *k = entitlement ? cx_cstr(entitlement) : 0;
    if (!k) return 0;
    if (!strncmp(k, "com.apple.security.cs.", 22) ||
        !strcmp(k, "com.apple.security.get-task-allow") ||
        !strcmp(k, "com.apple.security.network.client") ||
        !strcmp(k, "com.apple.security.network.server"))
        return objc_retain(kCFBooleanTrue);
    return 0;
}
kern_return_t task_for_pid(mach_port_t target, int pid, mach_port_t *task) {
    (void)target;
    if (!task) return KERN_INVALID_ARGUMENT;
    if (pid == (int)SYS0(SYS_GETPID)) { *task = TASK_PORT; return KERN_SUCCESS; }
    *task = 0;
    return KERN_FAILURE;
}
kern_return_t pid_for_task(mach_port_t task, int *pid) {
    if (!pid) return KERN_INVALID_ARGUMENT;
    if (task == TASK_PORT) { *pid = (int)SYS0(SYS_GETPID); return KERN_SUCCESS; }
    *pid = 0;
    return KERN_FAILURE;
}
kern_return_t host_get_special_port(mach_port_t host, int node, int which, mach_port_t *port) {
    (void)host; (void)node; (void)which;
    if (port) *port = 0;
    return KERN_FAILURE;             /* sem host_priv */
}

/* ---- task_info / thread_info / host_info (sabores que o Chromium lê na partida).
 * Contagem em natural_t (4 bytes). Buffer menor que o sabor: KERN_INVALID_ARGUMENT. */
typedef struct { int seconds, microseconds; } cx_time_value_t;
typedef struct {
    int suspend_count; int pad;
    u64 virtual_size, resident_size;
    cx_time_value_t user_time, system_time;
    int policy;
} cx_task_basic64_t;
typedef struct {
    u64 virtual_size, resident_size, resident_size_max;
    cx_time_value_t user_time, system_time;
    int policy, suspend_count;
} cx_mach_task_basic_t;
typedef struct { u64 addr, size; int format; } cx_task_dyld_t;
typedef struct {
    cx_time_value_t user_time, system_time;
    int cpu_usage, policy, run_state, flags, suspend_count, sleep_time;
} cx_thread_basic_t;
typedef struct { u64 thread_id, thread_handle, dispatch_qaddr; } cx_thread_id_t;
typedef struct {
    int max_cpus, avail_cpus, memory_size, cpu_type, cpu_subtype, cpu_threadtype;
    int physical_cpu, physical_cpu_max, logical_cpu, logical_cpu_max;
    u64 max_mem;
} cx_host_basic_t;

static int info_fit(void *info, u32 *count, u32 bytes) {
    if (!info || !count || *count < bytes / 4 || bytes > 1024) return 0;
    memset(info, 0, bytes);
    *count = bytes / 4;
    return 1;
}
kern_return_t task_info(mach_port_t task, int flavor, void *info, u32 *count) {
    if (task && task != TASK_PORT) return KERN_INVALID_ARGUMENT;
    if (flavor == 5 && info_fit(info, count, sizeof(cx_task_basic64_t))) {          /* TASK_BASIC_INFO_64 */
        cx_task_basic64_t *b = info;
        b->virtual_size = b->resident_size = 64ull << 20;
        b->policy = 1;
        return KERN_SUCCESS;
    }
    if (flavor == 20 && info_fit(info, count, sizeof(cx_mach_task_basic_t))) {      /* MACH_TASK_BASIC_INFO */
        cx_mach_task_basic_t *b = info;
        b->virtual_size = b->resident_size = b->resident_size_max = 64ull << 20;
        b->policy = 1;
        return KERN_SUCCESS;
    }
    if (flavor == 17 && info_fit(info, count, sizeof(cx_task_dyld_t))) {            /* TASK_DYLD_INFO */
        ((cx_task_dyld_t *)info)->format = 1;                                       /* 64 bits; sem all_image_info */
        return KERN_SUCCESS;
    }
    if (flavor == 15 && info_fit(info, count, 32)) {                                /* TASK_AUDIT_TOKEN */
        u32 *v = info;
        v[0] = v[1] = v[3] = 501; v[2] = v[4] = 20; v[5] = (u32)SYS0(SYS_GETPID); v[7] = 1;
        return KERN_SUCCESS;
    }
    if (flavor == 13 && info_fit(info, count, 8)) { ((u32 *)info)[0] = 501; ((u32 *)info)[1] = 20; return KERN_SUCCESS; }
    if ((flavor == 22 || flavor == 23) && info && count && *count >= 4) {          /* TASK_VM_INFO */
        u32 n = *count > 128 ? 128 : *count;
        memset(info, 0, n * 4);
        *count = n;
        if (n >= 2) ((u64 *)info)[0] = 64ull << 20;                                 /* virtual_size */
        if (n >= 4) ((u32 *)info)[3] = (u32)vm_page_size;                                      /* page_size */
        if (n >= 6) ((u64 *)info)[2] = 32ull << 20;                                 /* resident_size */
        if (n >= 38) *(u64 *)((u8 *)info + 144) = 32ull << 20;                      /* phys_footprint */
        return KERN_SUCCESS;
    }
    return KERN_INVALID_ARGUMENT;
}
kern_return_t thread_info(mach_port_t thread, int flavor, void *info, u32 *count) {
    u32 tid = thread >= THREAD_BASE ? thread - THREAD_BASE : (u32)SYS0(SYS_GETTID);
    if (flavor == 3 && info_fit(info, count, sizeof(cx_thread_basic_t))) {          /* THREAD_BASIC_INFO */
        ((cx_thread_basic_t *)info)->run_state = 1;                                 /* TH_STATE_RUNNING */
        ((cx_thread_basic_t *)info)->policy = 1;
        return KERN_SUCCESS;
    }
    if (flavor == 4 && info_fit(info, count, sizeof(cx_thread_id_t))) {             /* THREAD_IDENTIFIER_INFO */
        ((cx_thread_id_t *)info)->thread_id = tid;
        ((cx_thread_id_t *)info)->thread_handle = tid;
        return KERN_SUCCESS;
    }
    return KERN_INVALID_ARGUMENT;
}
kern_return_t host_info(mach_port_t host, int flavor, void *info, u32 *count) {
    (void)host;
    if (flavor == 1 && info_fit(info, count, sizeof(cx_host_basic_t))) {            /* HOST_BASIC_INFO */
        cx_host_basic_t *h = info;
        long c = SYS0(SYS_CPUS);
        int n = c > 0 ? (int)c : 1;
        h->max_cpus = h->avail_cpus = h->physical_cpu = h->physical_cpu_max = h->logical_cpu = h->logical_cpu_max = n;
        h->memory_size = 0x80000000;
        h->cpu_type = 0x01000007;                                                   /* CPU_TYPE_X86_64 */
        h->cpu_subtype = 3;
        h->max_mem = 2ull << 30;
        return KERN_SUCCESS;
    }
    return KERN_INVALID_ARGUMENT;
}
/* host_page_size: a página que o processo vê (4 KiB no x86_64, 16 KiB no arm64) */
kern_return_t host_page_size(mach_port_t host, u64 *size) {
    (void)host;
    if (!size) return KERN_INVALID_ARGUMENT;
    *size = vm_page_size;
    return KERN_SUCCESS;
}
kern_return_t host_kernel_version(mach_port_t host, char *ver) {
    (void)host;
    if (!ver) return KERN_INVALID_ARGUMENT;
    const char *s = "Darwin Kernel Version 23.0.0: mOS Nucleo; RELEASE_X86_64";
    u32 i = 0;
    for (; s[i] && i < 511; i++) ver[i] = s[i];
    ver[i] = 0;
    return KERN_SUCCESS;
}

/* ---- semáforos Mach (contador + espera cooperativa; o mOS tem threads preemptivas).
 * semaphore_t é um nome de porta de 32 bits: SEM_BASE + índice numa tabela. */
#define SEM_BASE 0x20000u
#define MAX_SEMS 1024
typedef struct { volatile int count; int used; volatile int waiters; } cx_sem_t;
/* futex do kernel (extra.c): quem espera dorme no contador em vez de girar com SYS_YIELD */
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
static cx_sem_t sems[MAX_SEMS];
static cx_sem_t *sem_of(mach_port_t name) {
    u32 i = name - SEM_BASE;
    return name >= SEM_BASE && i < MAX_SEMS && sems[i].used ? &sems[i] : 0;
}
kern_return_t semaphore_create(mach_port_t task, mach_port_t *sem, int policy, int value) {
    (void)task; (void)policy;
    for (u32 i = 0; i < MAX_SEMS; i++)
        if (!__sync_lock_test_and_set(&sems[i].used, 1)) { sems[i].count = value; sems[i].waiters = 0; *sem = SEM_BASE + i; return KERN_SUCCESS; }
    return KERN_FAILURE;
}
kern_return_t semaphore_destroy(mach_port_t task, mach_port_t sem) { (void)task; cx_sem_t *s = sem_of(sem); if (s) s->used = 0; return KERN_SUCCESS; }
kern_return_t semaphore_signal(mach_port_t sem) {
    cx_sem_t *s = sem_of(sem);
    if (!s) return KERN_INVALID_ARGUMENT;
    __sync_fetch_and_add(&s->count, 1);
    if (s->waiters) cx_futex_wake(&s->count, 1);
    return KERN_SUCCESS;
}
/* como no XNU: acorda quem está esperando agora e não deixa crédito para quem vier depois */
kern_return_t semaphore_signal_all(mach_port_t sem) {
    cx_sem_t *s = sem_of(sem);
    if (!s) return KERN_INVALID_ARGUMENT;
    int w = s->waiters;
    if (w > 0) { __sync_fetch_and_add(&s->count, w); cx_futex_wake(&s->count, 0x7FFFFFFF); }
    return KERN_SUCCESS;
}
static int sem_try(cx_sem_t *s) {
    int c = s->count;
    return c > 0 && __sync_bool_compare_and_swap(&s->count, c, c - 1);
}
/* espera até conseguir uma unidade; ms < 0 = sem prazo. 0 = conseguiu, 49 = KERN_OPERATION_TIMED_OUT */
static kern_return_t sem_wait_ms(cx_sem_t *s, long ms) {
    u64 end = ms < 0 ? 0 : (u64)SYS0(SYS_TIME) + (u64)ms;
    for (;;) {
        if (sem_try(s)) return KERN_SUCCESS;
        long left = -1;
        if (ms >= 0) {
            u64 now = (u64)SYS0(SYS_TIME);
            if (now >= end) return 49;
            left = (long)(end - now);
        }
        __sync_fetch_and_add(&s->waiters, 1);
        int c = s->count;
        if (c <= 0) cx_futex_wait(&s->count, c, left);
        __sync_fetch_and_sub(&s->waiters, 1);
    }
}
kern_return_t semaphore_wait(mach_port_t sem) {
    cx_sem_t *s = sem_of(sem);
    if (!s) return KERN_INVALID_ARGUMENT;
    return sem_wait_ms(s, -1);
}
/* mach_timespec_t {unsigned sec; int nsec} passado por valor (cabe em um registrador) */
kern_return_t semaphore_timedwait(mach_port_t sem, u64 ts) {
    cx_sem_t *s = sem_of(sem);
    if (!s) return KERN_INVALID_ARGUMENT;
    u64 ms = (ts & 0xFFFFFFFFu) * 1000ull + (u64)((ts >> 32) & 0xFFFFFFFFu) / 1000000ull;
    return sem_wait_ms(s, (long)ms);
}

/* ---- pthreads que faltavam (o mutex do mOS não tem protocolo; o rwlock fica em extra.c, estado zerado aqui) */
int pthread_mutexattr_setprotocol(void *a, int p) { (void)a; (void)p; return 0; }
int pthread_mutexattr_setpolicy_np(void *a, int p) { (void)a; (void)p; return 0; }
int pthread_rwlock_init(void *l, const void *a) { (void)a; for (int i = 0; i < 8; i++) ((u64 *)l)[i] = 0; return 0; }
int pthread_rwlock_destroy(void *l) { (void)l; return 0; }

const char *mach_error_string(kern_return_t e) {
    switch (e) {
    case 0: return "(os/kern) successful";
    case 1: return "(os/kern) invalid address";
    case 4: return "(os/kern) invalid argument";
    case 5: return "(os/kern) failure";
    case 46: return "(os/kern) not supported";
    case 49: return "(os/kern) operation timed out";
    default: return "(os/kern) erro desconhecido no mOS";
    }
}
const char *mach_error_type(kern_return_t e) { (void)e; return "(os/kern)"; }
void mach_error(const char *s, kern_return_t e) { cx_fdprintf(2, "%s %s (0x%x)\n", s ? s : "", mach_error_string(e), e); }

/* ---- MIG: porta de resposta por thread (o cliente MIG manda o pedido com ela e espera a resposta nela;
 * o rendezvous do Chromium usa). Fica num slot fixo da TSD (extra.c: cx_tsd). */
#define TSD_MIG_REPLY 102
void **cx_tsd(void);
mach_port_t mig_get_reply_port(void) {
    void **t = cx_tsd();
    mach_port_t p = (mach_port_t)(u64)t[TSD_MIG_REPLY];
    if (!p && mach_port_allocate(TASK_PORT, 1, &p) == KERN_SUCCESS) t[TSD_MIG_REPLY] = (void *)(u64)p;   /* 1 = RECEIVE */
    return p;
}
void mig_dealloc_reply_port(mach_port_t p) {
    void **t = cx_tsd();
    if ((mach_port_t)(u64)t[TSD_MIG_REPLY] == p) t[TSD_MIG_REPLY] = 0;
    mach_port_mod_refs(TASK_PORT, p, 1, -1);
}
void mig_put_reply_port(mach_port_t p) { (void)p; }

/* ---- audit token (trailer de auditoria do kernel: val[5] = pid; ver kernel/proc/machipc.c) */
typedef struct { u32 val[8]; } audit_token_t;
int audit_token_to_pid(audit_token_t t) { return (int)t.val[5]; }
int audit_token_to_pidversion(audit_token_t t) { return (int)t.val[7]; }
u32 audit_token_to_euid(audit_token_t t) { (void)t; return 501; }
u32 audit_token_to_ruid(audit_token_t t) { (void)t; return 501; }
u32 audit_token_to_egid(audit_token_t t) { (void)t; return 20; }
u32 audit_token_to_rgid(audit_token_t t) { (void)t; return 20; }
u32 audit_token_to_auid(audit_token_t t) { (void)t; return 501; }
u32 audit_token_to_asid(audit_token_t t) { (void)t; return 0; }

/* task_policy_set/get: o escalonador do mOS não tem políticas por tarefa; aceitar é o comportamento útil */
kern_return_t task_policy_set(mach_port_t task, int flavor, void *info, u32 count) { (void)task; (void)flavor; (void)info; (void)count; return KERN_SUCCESS; }
kern_return_t task_policy_get(mach_port_t task, int flavor, void *info, u32 *count, int *get_default) {
    (void)task; (void)flavor; (void)info; (void)count;
    if (get_default) *get_default = 1;
    return KERN_SUCCESS;
}

/* mach_port_construct(task, options, context, *name): cria uma porta de recepção com limite/contexto.
 * mach_port_options_t: flags(u32), mpl_qlimit(u32), ... Flags usados: MPO_QLIMIT (0x02: o qlimit só vale
 * com ele) e MPO_INSERT_SEND_RIGHT (0x10: cria também o direito de envio — o WaitableEvent do Chromium
 * depende disso, porque depois manda mensagens para a própria porta com COPY_SEND). */
#define MPO_QLIMIT            0x02
#define MPO_INSERT_SEND_RIGHT 0x10
#define MACH_MSG_TYPE_MAKE_SEND_ 20
kern_return_t mach_port_construct(mach_port_t task, void *options, u64 context, mach_port_t *name) {
    u32 flags = options ? ((u32 *)options)[0] : 0;
    u32 qlimit = options && (flags & MPO_QLIMIT) ? ((u32 *)options)[1] : 0;
    u64 a[8] = {0, qlimit, context, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_CONSTRUCT, a);
    if (r != KERN_SUCCESS) return (kern_return_t)r;
    mach_port_t n = (u32)a[3];
    if (flags & MPO_INSERT_SEND_RIGHT) {
        kern_return_t k = mach_port_insert_right(task, n, n, MACH_MSG_TYPE_MAKE_SEND_);
        if (k != KERN_SUCCESS) return k;
    }
    if (name) *name = n;
    return KERN_SUCCESS;
}

/* ---- memória compartilhada (base::SharedMemory do Chromium): uma "entrada de memória" é uma porta
 * (KP_MEM no kernel) que carrega as páginas. Enviada num mach_msg para outro processo, os dois mapeiam
 * a mesma memória. Delegada ao kernel (SYS_MACH_VM) para funcionar entre processos. */
void *mmap(void *addr, size_t len, int prot, int flags, int fd, long off);
static long mach_vm_syscall(u32 op, u64 *a) { return cx_syscall(SYS_MACH_VM, (long)op, (long)a, 0, 0, 0); }
kern_return_t mach_make_memory_entry_64(mach_port_t task, u64 *size, u64 offset, int perm, mach_port_t *handle, mach_port_t parent) {
    (void)task; (void)perm;
    if (!size || !handle) return KERN_INVALID_ARGUMENT;
    /* com MAP_MEM_NAMED_CREATE (0x20000) o kernel cria um objeto novo e zerado (offset ignorado);
     * sem ele, offset é o endereço de memória já mapeada a compartilhar (pai = MACH_PORT_NULL) */
    u64 a[8] = {*size, offset, (u64)perm, parent, 0, 0, 0, 0};
    long r = mach_vm_syscall(MACH_VM_MAKE_ENTRY, a);
    if (r == KERN_SUCCESS) { *handle = (u32)a[4]; *size = a[5]; }
    /* DIAGNÓSTICO TEMPORÁRIO (porte VS Code, vm_map do Chromium): remover depois */
    static int dbg_me = 200;
    if (dbg_me > 0) { dbg_me--; cx_fdprintf(2, "mach[diag]: make_memory_entry perm=0x%x size=0x%llx offset=0x%llx pai=0x%x -> kr=0x%x nome=0x%x tam=0x%llx\n", perm, a[0], offset, parent, (u32)r, (u32)a[4], a[5]); }
    return (kern_return_t)r;
}
kern_return_t mach_make_memory_entry(mach_port_t task, u64 *size, u64 offset, int perm, mach_port_t *handle, mach_port_t parent) {
    return mach_make_memory_entry_64(task, size, offset, perm, handle, parent);
}
kern_return_t mach_vm_map(mach_port_t task, u64 *addr, u64 size, u64 mask, int flags, mach_port_t object, u64 offset,
                          int copy, int cur, int max, unsigned inherit) {
    (void)task; (void)mask; (void)copy; (void)cur; (void)max; (void)inherit;
    if (!addr) return KERN_INVALID_ARGUMENT;
    if (object) {                                              /* entrada de memória: mapeia no kernel */
        u64 a[8] = {*addr, size, (u64)flags, object, offset, (u64)cur, (u64)max, 0};   /* max: proteção máxima pedida */
        long r = mach_vm_syscall(MACH_VM_MAP, a);
        if (r != KERN_SUCCESS && r != 17 /* esperado: escrita numa entrada só-leitura */) cx_fdprintf(2, "mach[diag]: vm_map FALHOU objeto=0x%x size=0x%llx flags=0x%x offset=0x%llx cur=%d -> kr=0x%x\n", object, size, flags, offset, cur, (u32)r);  /* DIAGNÓSTICO TEMPORÁRIO */
        if (r == KERN_SUCCESS) *addr = a[0];
        return (kern_return_t)r;
    }
    /* sem objeto: anônima. Sem VM_FLAGS_ANYWHERE o endereço pedido vai com MAP_FIXED.
     * Esse caminho substitui a faixa (mmap); KERN_NO_SPACE de faixa ocupada sem
     * VM_FLAGS_OVERWRITE vale para o caminho com objeto, no kernel. */
    int mf = 0x1002;
    if (!(flags & 1) && *addr) mf |= 0x10;
    void *p = mmap((flags & 1) ? 0 : (void *)*addr, size, 3, mf, -1, 0);
    if (p == (void *)-1) return 3;                             /* KERN_NO_SPACE */
    *addr = (u64)p;
    return KERN_SUCCESS;
}
kern_return_t vm_map(mach_port_t task, u64 *addr, u64 size, u64 mask, int flags, mach_port_t object, u64 offset,
                     int copy, int cur, int max, unsigned inherit) {
    return mach_vm_map(task, addr, size, mask, flags, object, offset, copy, cur, max, inherit);
}
kern_return_t mach_vm_allocate(mach_port_t task, u64 *addr, u64 size, int flags) {
    return mach_vm_map(task, addr, size, 0, flags | 1, 0, 0, 0, 3, 7, 1);
}
kern_return_t vm_allocate(mach_port_t task, u64 *addr, u64 size, int flags) { return mach_vm_allocate(task, addr, size, flags); }
int munmap(void *addr, size_t len);
kern_return_t mach_vm_deallocate(mach_port_t task, u64 addr, u64 size) {
    (void)task;
    if (!size) return KERN_SUCCESS;
    u64 base, end;
#if defined(__aarch64__)
    unsigned long long base_g, end_g;
    a64_dealloc_span(addr, size, &base_g, &end_g);
    base = base_g;
    end = end_g;
#else
    base = addr & ~0xFFFULL;
    end = (addr + size + 0xFFF) & ~0xFFFULL;
#endif
    if (end < base) return KERN_INVALID_ARGUMENT;
    /* o V8 e o PartitionAlloc soltam regiões com isto. Na arena de mmap o intervalo sai de
     * verdade, senão o próximo mapeamento no mesmo endereço lia lixo. Fora dela (a imagem
     * Mach-O) o kernel recusa a chamada inteira e nada é desmapeado; devolver erro faz o
     * PartitionAlloc abortar o renderer. */
    munmap((void *)base, (size_t)(end - base));
    return KERN_SUCCESS;
}
kern_return_t vm_deallocate(mach_port_t task, u64 addr, u64 size) { return mach_vm_deallocate(task, addr, size); }

/* ---- IOKit mínimo: o mOS não tem registro de dispositivos nem notificações de energia.
 * MACH_PORT_NULL (0) é a resposta documentada de "não foi possível". */
mach_port_t IORegisterForSystemPower(void *refcon, void **port_ref, void *callback, mach_port_t *notifier) {
    (void)refcon; (void)callback;
    if (port_ref) *port_ref = 0;
    if (notifier) *notifier = 0;
    return 0;
}
kern_return_t IODeregisterForSystemPower(mach_port_t *notifier) { (void)notifier; return KERN_SUCCESS; }
kern_return_t IOAllowPowerChange(mach_port_t root, long id) { (void)root; (void)id; return KERN_SUCCESS; }
void *IONotificationPortCreate(mach_port_t master) { (void)master; return 0; }
void IONotificationPortDestroy(void *p) { (void)p; }
void *IONotificationPortGetRunLoopSource(void *p) { (void)p; return 0; }
mach_port_t IOServiceGetMatchingService(mach_port_t master, void *matching) { (void)master; (void)matching; return 0; }
kern_return_t IOServiceGetMatchingServices(mach_port_t master, void *matching, mach_port_t *it) {
    (void)master; (void)matching;
    if (it) *it = 0;
    return KERN_FAILURE;
}
void *IOServiceMatching(const char *name) { (void)name; return 0; }
kern_return_t IOObjectRelease(mach_port_t o) { (void)o; return KERN_SUCCESS; }
kern_return_t IOObjectRetain(mach_port_t o) { (void)o; return KERN_SUCCESS; }
mach_port_t IOIteratorNext(mach_port_t it) { (void)it; return 0; }
void *IORegistryEntryCreateCFProperty(mach_port_t e, void *key, void *alloc, u32 opts) { (void)e; (void)key; (void)alloc; (void)opts; return 0; }
mach_port_t kIOMasterPortDefault = 0, kIOMainPortDefault = 0;

/* SIP (System Integrity Protection): o mOS não tem; configuração "nada ativo" */
int csr_get_active_config(u32 *config) { if (config) *config = 0; return 0; }
int csr_check(u32 mask) { (void)mask; return 0; }
/* __mac_syscall (MAC framework / sandbox): não existe no mOS */
int __mac_syscall(const char *policy, int call, void *arg) {
    (void)policy; (void)call; (void)arg;
    extern int cx_errno;
    cx_errno = 78;                                     /* ENOSYS */
    return -1;
}

/* ---- mach_msg: delegado ao kernel (machipc.c, SYS_MACH_MSG). O kernel copia a mensagem, troca os
 * nomes por portas (header, descritores de porta/memória/vetores de portas), põe na fila do destino e,
 * na recepção, insere os direitos e mapeia a memória OOL no espaço de quem recebe. Funciona dentro do
 * mesmo processo e entre processos (o rendezvous do VS Code). */
typedef struct { u32 bits, size; mach_port_t remote, local; u32 voucher; int id; } mach_msg_header_t;
#define MACH_SEND_MSG     0x1
#define MACH_RCV_MSG      0x2
#define MACH_RCV_LARGE    0x4
#define MACH_SEND_TIMEOUT 0x10
#define MACH_RCV_TIMEOUT  0x100
#define MACH_RCV_TIMED_OUT    0x10004003
#define MACH_RCV_TOO_LARGE    0x10004004
#define MACH_SEND_INVALID_DEST 0x10000003
#define MACH_SEND_INTERRUPT    0x40
#define MACH_RCV_INTERRUPT     0x400
#define MACH_SEND_INTERRUPTED  0x10000007
#define MACH_RCV_INTERRUPTED   0x10004005
static u64 mono_ms(void) {
    extern u64 clock_gettime_nsec_np(int clk);
    return clock_gettime_nsec_np(6) / 1000000;              /* CLOCK_MONOTONIC (macOS) */
}
kern_return_t mach_msg(mach_msg_header_t *msg, int option, u32 send_size, u32 rcv_size, mach_port_t rcv_name, u32 timeout, mach_port_t notify) {
    mos_mach_msg_args_t a = {(u64)msg, (u32)option, send_size, rcv_size, rcv_name, timeout, notify};
    u64 t0 = (option & MACH_RCV_TIMEOUT) ? mono_ms() : 0;
    kern_return_t r = (kern_return_t)cx_syscall(SYS_MACH_MSG, (long)&a, 0, 0, 0, 0);
    /* -4: a espera foi interrompida por um sinal (ex.: SIGCHLD de um helper que saiu). O XNU devolve
     * MACH_*_INTERRUPTED só a quem pediu MACH_*_INTERRUPT; aos outros, retoma a recepção sozinho. O envio
     * (se havia) já foi feito antes de bloquear, então só a recepção recomeça, com o prazo que sobrou.
     * O WaitableEvent do Chromium tem CHECK(kr == MACH_RCV_TIMED_OUT) e derrubava o navegador com -4. */
    while (r == -4) {
        int rcv = (option & MACH_RCV_MSG) != 0;
        if (option & (rcv ? MACH_RCV_INTERRUPT : MACH_SEND_INTERRUPT)) return rcv ? MACH_RCV_INTERRUPTED : MACH_SEND_INTERRUPTED;
        if (!rcv) { a.option = (u32)option; }
        else {
            a.option = (u32)option & ~(u32)MACH_SEND_MSG;
            a.send_size = 0;
            if (option & MACH_RCV_TIMEOUT) {
                u64 gone = mono_ms() - t0;
                if (gone >= timeout) return MACH_RCV_TIMED_OUT;
                a.timeout = timeout - (u32)gone;
            }
        }
        r = (kern_return_t)cx_syscall(SYS_MACH_MSG, (long)&a, 0, 0, 0, 0);
    }
    /* diagnóstico (ca-log): erro de recepção que não é prazo/interrupção. O ChannelMac do Mojo para de vigiar o canal
     * quando o mach_msg falha, e as mensagens seguintes ficam na fila para sempre */
    if (r && (option & 2) && r != 0x10004003 && r != 0x10004005) {
        extern int cx_ca_diag(void);
        extern int cx_fdprintf(int fd, const char *fmt, ...);
        static int n;
        if (cx_ca_diag() && ++n <= 30)
            cx_fdprintf(2, "[ca] mach_msg: recepção na porta 0x%x falhou: 0x%x (opção 0x%x, tamanho %u, thread %ld)\n",
                        rcv_name, (unsigned)r, (unsigned)option, rcv_size, SYS0(SYS_GETTID));
    }
    return r;
}
/* mach_port_peek: há mensagem? devolve tamanho/seqno/id da próxima sem consumir */
kern_return_t mach_port_peek(mach_port_t task, mach_port_t name, u32 type, u32 *seq, u32 *size, void *ctx, void *trailer, u32 *tsize) {
    (void)task; (void)type; (void)ctx; (void)trailer; (void)tsize;
    u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_PEEK, a);
    if (r == KERN_SUCCESS) { if (size) *size = (u32)a[1]; if (seq) *seq = (u32)a[2]; }
    return (kern_return_t)r;
}

/* para o kqueue (EVFILT_MACHPORT): há mensagem esperando nesta porta (ou no conjunto)? */
int cx_mach_pending(mach_port_t port) {
    u64 a[8] = {port, 0, 0, 0, 0, 0, 0, 0};
    if (mach_port(MACH_OP_PENDING, a) != KERN_SUCCESS) return 0;
    return (int)a[1];
}
kern_return_t mach_msg_overwrite(mach_msg_header_t *msg, int option, u32 send_size, u32 rcv_size, mach_port_t rcv_name,
                                 u32 timeout, mach_port_t notify, mach_msg_header_t *rcv_msg, u32 rcv_limit) {
    (void)rcv_msg; (void)rcv_limit;
    return mach_msg(msg, option, send_size, rcv_size, rcv_name, timeout, notify);
}
void mach_msg_destroy(mach_msg_header_t *msg) {
    if (!msg) return;
    /* só direitos MOVE são do receptor (MAKE/COPY continuam com quem enviou). Soltá-los dispara
     * MACH_NOTIFY_SEND_ONCE no kernel quando o direito é send-once — é o que desbloqueia o cliente
     * do rendezvous se o servidor recusa o pedido. */
    u32 bits = msg->bits;
    u32 remote = bits & 0x1f, local = (bits >> 8) & 0x1f;
    if ((remote == 16 || remote == 17 || remote == 18) && msg->remote) mach_port_deallocate(0, msg->remote);
    if ((local == 16 || local == 17 || local == 18) && msg->local) mach_port_deallocate(0, msg->local);
    if (!(bits & 0x80000000u)) return;
    u8 *b = (u8 *)msg;
    u32 nd = *(u32 *)(b + 24);
    u32 off = 28;
    for (u32 i = 0; i < nd && i < 64 && off + 12 <= msg->size; i++) {
        u8 type = b[off + 11];
        if (type == 0) {
            u32 name = *(u32 *)(b + off);
            u8 disp = b[off + 10];
            if ((disp == 16 || disp == 17 || disp == 18) && name) mach_port_deallocate(0, name);
            off += 12;
        } else off += 16;
    }
}
kern_return_t mach_msg_send(mach_msg_header_t *msg) { return msg ? mach_msg(msg, MACH_SEND_MSG, msg->size, 0, 0, 0, 0) : KERN_INVALID_ARGUMENT; }
kern_return_t mach_msg_receive(mach_msg_header_t *msg) { return msg ? mach_msg(msg, MACH_RCV_MSG, 0, msg->size, msg->local, 0, 0) : KERN_INVALID_ARGUMENT; }

/* ---- notify(3): em svcclient.c, sobre o notifyd (userland/notifyd.c) */

/* IOKit energia: sem informação (kIOReturnUnsupported) */
int IOPMCopyCPUPowerStatus(void **status) { if (status) *status = 0; return (int)0xe00002c7; }
/* pthread_atfork fica em libc.c (junto do fork, para soltar a trava do heap no filho) */

kern_return_t mach_port_destruct(mach_port_t task, mach_port_t name, int delta, u64 guard) {
    (void)task; (void)guard;
    u64 a[8] = {name, (u64)(long)delta, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)mach_port(MACH_OP_DESTRUCT, a);
}

/* ---- bootstrap (o "launchd" do mOS) e portas especiais da task.
 * Movido de darwin.c (sessão DMG) para cá: agora o registro de serviços é global, no kernel
 * (SYS_MACH_BOOTSTRAP), então um processo registra um serviço (check_in) e outro o encontra
 * (look_up) e troca mensagens com ele. É o que o MachPortRendezvous do VS Code/Chromium usa. */
u32 bootstrap_port = 0x303;        /* nome fixo do direito de envio do bootstrap em todo processo */
static long mach_bootstrap(u32 op, const char *name, u32 *port) {
    return cx_syscall(SYS_MACH_BOOTSTRAP, (long)op, (long)name, (long)port, 0, 0);
}
int bootstrap_check_in(u32 bp, const char *name, u32 *port) {
    (void)bp;
    return (int)mach_bootstrap(MACH_BS_CHECK_IN, name, port);
}
int bootstrap_look_up(u32 bp, const char *name, u32 *port) {
    (void)bp;
    return (int)mach_bootstrap(MACH_BS_LOOK_UP, name, port);
}
int bootstrap_look_up2(u32 bp, const char *name, u32 *port, int pid, u64 flags) { (void)pid; (void)flags; return bootstrap_look_up(bp, name, port); }
int bootstrap_register(u32 bp, const char *name, u32 port) {
    (void)bp;
    u32 p = port;
    return (int)mach_bootstrap(MACH_BS_REGISTER, name, &p);
}
int bootstrap_register2(u32 bp, const char *name, u32 port, u64 flags) { (void)flags; return bootstrap_register(bp, name, port); }
const char *bootstrap_strerror(int r) { return r == 1102 ? "serviço desconhecido" : r ? "erro do bootstrap" : "ok"; }
int task_get_special_port(u32 task, int which, u32 *port) {
    (void)task;
    u64 a[8] = {(u64)which, 0, 0, 0, 0, 0, 0, 0};
    long r = mach_port(MACH_OP_GET_SPECIAL, a);
    if (port) *port = (u32)a[1];
    return (int)r;
}
int task_set_special_port(u32 task, int which, u32 port) {
    (void)task;
    u64 a[8] = {(u64)which, port, 0, 0, 0, 0, 0, 0};
    return (int)mach_port(MACH_OP_SET_SPECIAL, a);
}

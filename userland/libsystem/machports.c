/*
 * mOS libSystem — rotinas MIG de cliente de portas que faltavam (mach_port_names, mach_port_space_info,
 * mach_port_space_basic_info, mach_port_get_srights, mach_port_get_set_status, mach_port_kobject,
 * mach_port_kernel_object, mach_port_allocate_full/qos, mach_port_dnrequest_info, mach_port_rename).
 * O espaço de nomes vem do kernel (MACH_OP_NAMES, kernel/proc/machipc.c). Só o próprio espaço: a porta de task de
 * outro processo (PR #390) devolve KERN_INVALID_ARGUMENT aqui, como um macOS sem task_for_pid de leitura.
 * docs/darwin-bsd-mach.md, fase F3. Listas saem em memória do mach_vm_allocate, como o MIG entrega.
 */
#include "sys.h"

typedef u32 mach_port_t;
typedef int kern_return_t;
#define KERN_SUCCESS 0
#define KERN_INVALID_ADDRESS 1
#define KERN_INVALID_ARGUMENT 4
#define KERN_FAILURE 5
#define KERN_RESOURCE_SHORTAGE 6
#define KERN_INVALID_NAME 15
#define KERN_INVALID_RIGHT 17
#define KERN_NOT_SUPPORTED 46
#define TASK_PORT 0x103u
#define HOST_PORT 0x203u
#define THREAD_BASE 0x1000u
#define T_SEND 0x10000u
#define T_RECEIVE 0x20000u
#define T_PSET 0x80000u

kern_return_t mach_vm_allocate(mach_port_t task, u64 *addr, u64 size, int flags);
kern_return_t mach_vm_deallocate(mach_port_t task, u64 addr, u64 size);
kern_return_t mach_port_allocate(mach_port_t task, int right, mach_port_t *name);
kern_return_t mach_port_allocate_name(mach_port_t task, int right, mach_port_t name);
static long mport(u32 op, u64 *a) { return cx_syscall(SYS_MACH_PORT, (long)op, (long)a, 0, 0, 0); }
static int own_task(mach_port_t t) { return t == 0 || t == TASK_PORT; }

/* lê o espaço inteiro numa área do mach_vm_allocate; *n = quantidade */
static kern_return_t space(mos_mach_name_t **out, u32 *n) {
    for (int tries = 0; tries < 4; tries++) {
        u64 a[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        long r = mport(MACH_OP_NAMES, a);
        if (r != KERN_SUCCESS) return (kern_return_t)r;
        u64 cap = a[2] + 16, buf = 0, bytes = cap * sizeof(mos_mach_name_t);
        if (mach_vm_allocate(TASK_PORT, &buf, bytes ? bytes : 4096, 1) != KERN_SUCCESS) return KERN_RESOURCE_SHORTAGE;
        u64 b[8] = {buf, cap, 0, 0, 0, 0, 0, 0};
        r = mport(MACH_OP_NAMES, b);
        if (r == KERN_SUCCESS && b[2] <= cap) { *out = (mos_mach_name_t *)(usize)buf; *n = (u32)b[2]; return KERN_SUCCESS; }
        mach_vm_deallocate(TASK_PORT, buf, bytes ? bytes : 4096);
        if (r != KERN_SUCCESS) return (kern_return_t)r;
    }
    return KERN_RESOURCE_SHORTAGE;                 /* o espaço não parou de crescer */
}

kern_return_t mach_port_names(mach_port_t task, mach_port_t **names, u32 *ncnt, u32 **types, u32 *tcnt) {
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    if (!names || !ncnt || !types || !tcnt) return KERN_INVALID_ADDRESS;
    mos_mach_name_t *sp;
    u32 n;
    kern_return_t kr = space(&sp, &n);
    if (kr) return kr;
    u64 na = 0, ta = 0, bytes = (n ? n : 1) * 4ull;
    if (mach_vm_allocate(TASK_PORT, &na, bytes, 1) || mach_vm_allocate(TASK_PORT, &ta, bytes, 1)) return KERN_RESOURCE_SHORTAGE;
    for (u32 i = 0; i < n; i++) { ((u32 *)(usize)na)[i] = sp[i].name; ((u32 *)(usize)ta)[i] = sp[i].type; }
    mach_vm_deallocate(TASK_PORT, (u64)(usize)sp, (n + 16) * sizeof *sp);
    *names = (mach_port_t *)(usize)na; *ncnt = n;
    *types = (u32 *)(usize)ta; *tcnt = n;
    return KERN_SUCCESS;
}

/* ipc_info_space_t (6 natural_t) e ipc_info_name_t (7 natural_t); a árvore (splay) não existe mais no XNU: vazia */
kern_return_t mach_port_space_info(mach_port_t task, u32 *info, u32 **table, u32 *tcnt, void **tree, u32 *trcnt) {
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    if (!info || !table || !tcnt) return KERN_INVALID_ADDRESS;
    mos_mach_name_t *sp;
    u32 n;
    kern_return_t kr = space(&sp, &n);
    if (kr) return kr;
    u64 ta = 0;
    if (mach_vm_allocate(TASK_PORT, &ta, (n ? n : 1) * 28ull, 1)) return KERN_RESOURCE_SHORTAGE;
    u32 *t = (u32 *)(usize)ta;
    for (u32 i = 0; i < n; i++) {
        u32 *e = t + i * 7;
        e[0] = sp[i].name; e[1] = 0; e[2] = sp[i].type;
        e[3] = sp[i].srefs ? sp[i].srefs : sp[i].drefs;          /* iin_urefs */
        e[4] = sp[i].name;                                          /* iin_object: identidade estável do nome */
        e[5] = 0; e[6] = 0;
    }
    mach_vm_deallocate(TASK_PORT, (u64)(usize)sp, (n + 16) * sizeof *sp);
    info[0] = 0xffffff00u; info[1] = n; info[2] = n; info[3] = 0; info[4] = 0; info[5] = 0;
    *table = t; *tcnt = n;
    if (tree) *tree = 0;
    if (trcnt) *trcnt = 0;
    return KERN_SUCCESS;
}
kern_return_t mach_port_space_basic_info(mach_port_t task, u32 *info) {   /* ipc_info_space_basic_t: 6 natural_t */
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    if (!info) return KERN_INVALID_ADDRESS;
    u64 a[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    long r = mport(MACH_OP_NAMES, a);
    if (r) return (kern_return_t)r;
    info[0] = 0xffffff00u; info[1] = (u32)a[2]; info[2] = (u32)a[2]; info[3] = (u32)a[2]; info[4] = info[5] = 0;
    return KERN_SUCCESS;
}

kern_return_t mach_port_get_srights(mach_port_t task, mach_port_t name, u32 *srights) {
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    if (!srights) return KERN_INVALID_ADDRESS;
    u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    long r = mport(MACH_OP_TYPE, a);
    if (r) return (kern_return_t)r;
    if (!(a[1] & T_RECEIVE)) return KERN_INVALID_RIGHT;   /* só quem tem o receive sabe quantos envios existem */
    u64 s[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    r = mport(MACH_OP_STATUS, s);
    if (r) return (kern_return_t)r;
    *srights = (u32)s[4];
    return KERN_SUCCESS;
}

/* membros de um conjunto: os receives cujo MACH_OP_STATUS aponta para ele */
kern_return_t mach_port_get_set_status(mach_port_t task, mach_port_t pset, mach_port_t **members, u32 *cnt) {
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    if (!members || !cnt) return KERN_INVALID_ADDRESS;
    u64 t[8] = {pset, 0, 0, 0, 0, 0, 0, 0};
    long r = mport(MACH_OP_TYPE, t);
    if (r) return (kern_return_t)r;
    if (!(t[1] & T_PSET)) return KERN_INVALID_RIGHT;
    mos_mach_name_t *sp;
    u32 n;
    kern_return_t kr = space(&sp, &n);
    if (kr) return kr;
    u64 out = 0;
    if (mach_vm_allocate(TASK_PORT, &out, (n ? n : 1) * 4ull, 1)) return KERN_RESOURCE_SHORTAGE;
    u32 k = 0;
    for (u32 i = 0; i < n; i++) {
        if (!(sp[i].type & T_RECEIVE)) continue;
        u64 s[8] = {sp[i].name, 0, 0, 0, 0, 0, 0, 0};
        if (mport(MACH_OP_STATUS, s) == KERN_SUCCESS && (u32)s[7] == pset) ((u32 *)(usize)out)[k++] = sp[i].name;
    }
    mach_vm_deallocate(TASK_PORT, (u64)(usize)sp, (n + 16) * sizeof *sp);
    *members = (mach_port_t *)(usize)out; *cnt = k;
    return KERN_SUCCESS;
}

/* tipo de objeto do kernel (IKOT_*): task 2, thread 1, host 3; porta comum 0 */
kern_return_t mach_port_kobject(mach_port_t task, mach_port_t name, u32 *kotype, u64 *addr) {
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    if (!kotype) return KERN_INVALID_ADDRESS;
    u32 k = 0;
    if (name == TASK_PORT) k = 2;
    else if (name == HOST_PORT) k = 3;
    else {
        /* o kernel primeiro: nomes de porta comuns também caem na faixa 0x1000+ das threads */
        u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
        long r = mport(MACH_OP_TYPE, a);
        if (r == KERN_SUCCESS) { if (!(a[1] & T_SEND)) return KERN_INVALID_RIGHT; }
        else if (name >= THREAD_BASE && name < THREAD_BASE + 0x10000u) {
            mos_dw_thread_t th[256];   /* porta de thread do libSystem (0x1000 + tid): vale se a thread existe */
            long nt = cx_syscall(SYS_DARWIN, MOS_DW_THREADS, cx_syscall(SYS_GETPID, 0, 0, 0, 0, 0), (long)th, 256, 0);
            int hit = 0;
            for (long i = 0; i < nt && i < 256; i++) if (th[i].tid == name - THREAD_BASE) hit = 1;
            if (!hit) return KERN_INVALID_NAME;
            k = 1;
        } else return (kern_return_t)r;
    }
    *kotype = k;
    if (addr) *addr = 0;                          /* o XNU também esconde o endereço fora de DEVELOPMENT */
    return KERN_SUCCESS;
}
kern_return_t mach_port_kernel_object(mach_port_t task, mach_port_t name, u32 *type, u32 *addr) {
    u64 a64 = 0;
    kern_return_t r = mach_port_kobject(task, name, type, &a64);
    if (addr) *addr = (u32)a64;
    return r;
}
kern_return_t mach_port_kobject_description(mach_port_t task, mach_port_t name, u32 *kotype, u64 *addr, char *desc) {
    kern_return_t r = mach_port_kobject(task, name, kotype, addr);
    if (desc) desc[0] = 0;
    return r;
}

/* mach_port_qos_t {name:1, prealloc:1, pad; len}: name = usar *name pedido */
kern_return_t mach_port_allocate_full(mach_port_t task, int right, mach_port_t subsystem, const u32 *qos, mach_port_t *name) {
    (void)subsystem;
    if (!name) return KERN_INVALID_ADDRESS;
    if (qos && (qos[0] & 1)) return mach_port_allocate_name(task, right, *name);
    return mach_port_allocate(task, right, name);
}
kern_return_t mach_port_allocate_qos(mach_port_t task, int right, const u32 *qos, mach_port_t *name) {
    if (!name) return KERN_INVALID_ADDRESS;
    if (qos && (qos[0] & 1)) return KERN_INVALID_ARGUMENT;   /* como no XNU: allocate_qos não aceita nome pedido */
    return mach_port_allocate(task, right, name);
}
/* pedidos de nome morto: o kernel guarda, sem contador exposto */
kern_return_t mach_port_dnrequest_info(mach_port_t task, mach_port_t name, u32 *total, u32 *used) {
    if (!own_task(task)) return KERN_INVALID_ARGUMENT;
    u64 a[8] = {name, 0, 0, 0, 0, 0, 0, 0};
    long r = mport(MACH_OP_TYPE, a);
    if (r) return (kern_return_t)r;
    if (total) *total = 0;
    if (used) *used = 0;
    return KERN_SUCCESS;
}
kern_return_t mach_port_rename(mach_port_t task, mach_port_t old, mach_port_t nw) { (void)task; (void)old; (void)nw; return KERN_NOT_SUPPORTED; }   /* removida do XNU */

/*
 * mOS — SYS_DARWIN: os dados do sistema e dos processos que o libSystem usa para sysctl, proc_info, host_info e
 * task_info (docs/darwin-bsd-mach.md, fase F1). Tudo aqui é leitura do estado real do kernel; nada é inventado.
 *
 * Syscalls rodam com interrupções desligadas (proc.h): a lista de processos e a de threads não mudam durante a chamada.
 */
#include "proc.h"
#include "syscall.h"
#include "vmcomp.h"

#ifndef MOS_HOST_TEST
#define DW_STATIC static
#else
#define DW_STATIC
#endif

/* kern.hostname: nasce "mOS"; sethostname(3) do root troca (como no XNU, não persiste entre boots) */
static char dw_hostname[256] = "mOS";
static u64 dw_boot_unix_us;

/* mesmo cálculo do unix_now de process.c (dias desde 1970 pelo algoritmo de Howard Hinnant) */
DW_STATIC i64 dw_unix_from(const datetime_t *dt) {
    int y = dt->year - (dt->month <= 2), era = y / 400, yoe = y - era * 400;
    int doy = (153 * (dt->month + (dt->month > 2 ? -3 : 9)) + 2) / 5 + dt->day - 1;
    i64 days = (i64)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return days * 86400 + dt->hour * 3600 + dt->minute * 60 + dt->second;
}

/* hora do boot: lida uma vez (agora - uptime) e guardada, para kern.boottime não andar quando o relógio é acertado */
static u64 boot_us(void) {
    if (!dw_boot_unix_us) {
        datetime_t dt;
        rtc_now(&dt);
        i64 now_us = dw_unix_from(&dt) * 1000000;
        i64 up = (i64)timer_ticks() * 1000;
        dw_boot_unix_us = now_us > up ? (u64)(now_us - up) : 1;
    }
    return dw_boot_unix_us;
}

static int thread_state(const thread_t *t) {
    if (t->state == TH_RUNNING || t->state == TH_READY) return MOS_DW_ST_RUN;
    return MOS_DW_ST_SLEEP;
}

static bool proc_live(const process_t *q) { return q && !q->exited; }

static int nthreads_of(const process_t *q, u64 *cpu, int *runnable) {
    int n = 0;
    for (thread_t *t = sched_threads(); t; t = t->next) {
        if (t->proc != q || t->state == TH_DEAD) continue;
        n++;
        if (cpu) *cpu += t->cpu_ms;
        if (runnable && (t->state == TH_RUNNING || t->state == TH_READY)) (*runnable)++;
    }
    return n;
}

static i64 dw_sysinfo(process_t *p, u64 uva) {
    mos_dw_sysinfo_t si;
    memset(&si, 0, sizeof si);
    si.mem_total = pmm_total_pages() * 4096;
    si.mem_free = pmm_free_pages() * 4096;
    si.boot_unix_us = boot_us();
    si.uptime_ms = timer_ticks();
    swap_stats_t ss;
    memset(&ss, 0, sizeof ss);
    swap_stats(&ss);
    /* a troca do mOS é comprimida em RAM e cresce sob demanda: "usado" = bytes comprimidos guardados; o total é o
     * usado arredondado para 64 MiB acima, como o vm.swapusage do macOS (arquivos de troca que crescem em blocos) */
    si.swap_used = ss.zone_bytes;
    si.swap_total = (ss.zone_bytes + (64ull << 20) - 1) & ~((64ull << 20) - 1);
    si.pageins = ss.nin;
    si.pageouts = ss.nout;
    si.ncpu = (u32)smp_cpu_count();
    int run = 0;
    for (process_t *q = proc_list(); q; q = q->next) {
        if (!proc_live(q)) continue;
        si.nprocs++;
        si.nthreads += (u32)nthreads_of(q, 0, &run);
    }
    si.nrunnable = (u32)run;
    return uvm_copy_out(p, uva, &si, sizeof si) ? 0 : -MOS_EFAULT;
}

static i64 dw_hostname_get(process_t *p, u64 buf, u64 cap) {
    usize n = strlen(dw_hostname);
    if (buf && cap) {
        usize k = n + 1 <= cap ? n + 1 : cap;
        if (!uvm_copy_out(p, buf, dw_hostname, k)) return -MOS_EFAULT;
    }
    return (i64)n;
}

static i64 dw_hostname_set(process_t *p, u64 name, u64 len) {
    if (p->euid != 0) return -MOS_EPERM;
    if (len > 255) return -MOS_EINVAL;
    char tmp[256];
    if (len && !uvm_copy_in(p, tmp, name, len)) return -MOS_EFAULT;
    tmp[len] = 0;
    for (usize i = 0; i < len; i++) if (!tmp[i]) { len = i; break; }   /* o XNU guarda até o primeiro 0 */
    memcpy(dw_hostname, tmp, len + 1);
    return 0;
}

static i64 dw_pids(process_t *p, u64 buf, u64 cap) {
    /* em ordem crescente de pid, como o proc_listpids do XNU devolve na prática (a lista do kernel é a mais nova antes) */
    int n = 0, last = -1;
    for (;;) {
        int best = 0x7fffffff;
        for (process_t *q = proc_list(); q; q = q->next)
            if (proc_live(q) && q->pid > last && q->pid < best) best = q->pid;
        if (best == 0x7fffffff) break;
        if (buf && (u64)n < cap && !uvm_copy_out(p, buf + (u64)n * 4, &best, 4)) return -MOS_EFAULT;
        n++;
        last = best;
    }
    return n;
}

static i64 dw_proc(process_t *p, int pid, u64 uva) {
    process_t *q = proc_find(pid);
    if (!proc_live(q)) return -MOS_ESRCH;
    mos_dw_proc_t d;
    memset(&d, 0, sizeof d);
    d.pid = q->pid; d.ppid = q->ppid; d.pgid = q->pgid; d.sid = q->sid;
    d.uid = q->uid; d.euid = q->euid; d.suid = q->suid;
    d.gid = q->gid; d.egid = q->egid; d.sgid = q->sgid;
    d.stopped = q->stopped;
    int run = 0;
    d.nthreads = nthreads_of(q, &d.cpu_ms, &run);
    d.state = q->stopped ? MOS_DW_ST_STOP : run ? MOS_DW_ST_RUN : d.nthreads ? MOS_DW_ST_SLEEP : MOS_DW_ST_ZOMBIE;
    d.flags = (q->vfork_of ? 1 : 0) | (q->ctty ? 2 : 0);
    d.start_unix_us = boot_us() + q->start_ms * 1000;
    d.mem_bytes = q->mem_pages * 4096;
    u64 heap = q->brk > q->brk_start ? q->brk - q->brk_start : 0;
    u64 maps = q->mmap_next > USER_MMAP_BASE ? q->mmap_next - USER_MMAP_BASE : 0;
    d.vsize = heap + maps + USER_STACK_SIZE;
    memcpy(d.name, q->name, sizeof d.name);
    d.name[sizeof d.name - 1] = 0;
    memcpy(d.path, q->path, sizeof d.path);
    d.path[sizeof d.path - 1] = 0;
    return uvm_copy_out(p, uva, &d, sizeof d) ? 0 : -MOS_EFAULT;
}

/* KERN_PROCARGS2: int argc, executável, zeros até alinhar a 8 (o XNU alinha ao ponteiro), argv\0..., envp\0...
 * As strings vêm da pilha do alvo (o setproctitle que reescreve argv aparece aqui, como no macOS). Só o próprio
 * usuário ou o root leem (o XNU dá EPERM a outros uids; o ps usa KERN_PROC para estes). */
static i64 dw_args(process_t *p, int pid, u64 buf, u64 cap) {
    process_t *q = proc_find(pid);
    if (!proc_live(q)) return -MOS_ESRCH;
    if (p->euid != 0 && p->euid != q->uid) return -MOS_EPERM;
    usize pl = strlen(q->path);
    usize head = 4 + pl + 1;
    head = (head + 7) & ~(usize)7;
    u64 body = q->args_va ? q->args_len : 0;
    u64 total = head + body;
    if (!buf) return (i64)total;
    if (cap < total) return -MOS_ENOMEM;
    u8 *k = kmalloc(total ? total : 1);
    if (!k) return -MOS_ENOMEM;
    memset(k, 0, head);
    int argc = q->args_va ? q->args_argc : 0;
    memcpy(k, &argc, 4);
    memcpy(k + 4, q->path, pl);
    i64 r = (i64)total;
    if (body && !uvm_copy_in(q, k + head, q->args_va, body)) { r = head; argc = 0; memcpy(k, &argc, 4); }   /* pilha já trocada */
    if (!uvm_copy_out(p, buf, k, (usize)r)) r = -MOS_EFAULT;
    kfree(k);
    return r;
}

static i64 dw_threads(process_t *p, int pid, u64 buf, u64 cap) {
    process_t *q = proc_find(pid);
    if (!proc_live(q)) return -MOS_ESRCH;
    i64 n = 0;
    for (thread_t *t = sched_threads(); t; t = t->next) {
        if (t->proc != q || t->state == TH_DEAD) continue;
        if (buf && (u64)n < cap) {
            mos_dw_thread_t d;
            memset(&d, 0, sizeof d);
            d.tid = (u64)t->id;
            d.cpu_ms = t->cpu_ms;
            d.state = thread_state(t);
            d.suspend = t->suspend;
            d.qos = t->qos;
            memcpy(d.name, t->name, sizeof d.name);
            d.name[sizeof d.name - 1] = 0;
            if (!uvm_copy_out(p, buf + (u64)n * sizeof d, &d, sizeof d)) return -MOS_EFAULT;
        }
        n++;
    }
    return n;
}

static i64 dw_fds(process_t *p, int pid, u64 buf, u64 cap) {
    process_t *q = proc_find(pid);
    if (!proc_live(q)) return -MOS_ESRCH;
    if (p->euid != 0 && p->euid != q->uid) return -MOS_EPERM;
    i64 n = 0;
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        struct fdent *e = &q->fds[i];
        if (!e->used) continue;
        if (buf && (u64)n < cap) {
            mos_dw_fd_t d;
            memset(&d, 0, sizeof d);
            d.fd = i;
            d.kind = e->kind == FD_FILE ? MOS_DW_FD_FILE : (e->kind == FD_PIPE_R || e->kind == FD_PIPE_W) ? MOS_DW_FD_PIPE :
                     (e->kind == FD_PTY_M || e->kind == FD_PTY_S) ? MOS_DW_FD_PTY : MOS_DW_FD_SOCK;
            d.flags = (unsigned)e->flags;
            d.off = e->off;
            d.cloexec = (e->flags & MOS_O_CLOEXEC) != 0;
            memcpy(d.path, e->path, sizeof d.path);
            d.path[sizeof d.path - 1] = 0;
            if (!uvm_copy_out(p, buf + (u64)n * sizeof d, &d, sizeof d)) return -MOS_EFAULT;
        }
        n++;
    }
    return n;
}

static i64 dw_cwd(process_t *p, int pid, u64 buf, u64 cap) {
    process_t *q = proc_find(pid);
    if (!proc_live(q)) return -MOS_ESRCH;
    if (p->euid != 0 && p->euid != q->uid) return -MOS_EPERM;
    char c[sizeof q->cwd + 1];
    memcpy(c, q->cwd, sizeof q->cwd);
    c[sizeof q->cwd] = 0;
    if (!c[0]) { c[0] = '/'; c[1] = 0; }
    usize n = strlen(c);
    if (buf && cap && !uvm_copy_out(p, buf, c, n + 1 <= cap ? n + 1 : cap)) return -MOS_EFAULT;
    return (i64)n;
}

/* NOTE_EXITSTATUS do kevent: o status do filho zumbi, sem colher (quem colhe é o waitpid do dono) */
static i64 dw_exitstatus(process_t *p, int pid) {
    for (process_t *q = proc_list(); q; q = q->next) {
        if (q->pid != pid || q->ppid != p->pid || !q->exited) continue;
        int code = q->exit_code;
        return code < 0 && code >= -31 ? -code : (code & 0xFF) << 8;   /* posix_status de process.c */
    }
    return -MOS_ESRCH;
}

i64 darwin_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    switch (op) {
    case MOS_DW_SYSINFO:     return dw_sysinfo(p, a);
    case MOS_DW_HOSTNAME:    return dw_hostname_get(p, a, b);
    case MOS_DW_SETHOSTNAME: return dw_hostname_set(p, a, b);
    case MOS_DW_PIDS:        return dw_pids(p, a, b);
    case MOS_DW_PROC:        return dw_proc(p, (int)a, b);
    case MOS_DW_ARGS:        return dw_args(p, (int)a, b, c);
    case MOS_DW_THREADS:     return dw_threads(p, (int)a, b, c);
    case MOS_DW_FDS:         return dw_fds(p, (int)a, b, c);
    case MOS_DW_CWD:         return dw_cwd(p, (int)a, b, c);
    case MOS_DW_EXITSTATUS:  return dw_exitstatus(p, (int)a);
    default:                 return -MOS_EINVAL;
    }
}

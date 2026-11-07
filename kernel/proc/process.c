/*
 * mOS — processos de usuário: espaço de endereçamento próprio (ring 3), syscalls via int 0x80,
 * saída para um console compartilhado e encerramento isolado em caso de falha.
 *
 * Espaço de endereçamento: cópia da PML4 do kernel (o mapa de identidade continua lá, mas sem o
 * bit U, então o ring 3 não o alcança) + a região de usuário em PML4[128] com páginas de 4 KiB U|RW.
 *
 * ABI das syscalls (int 0x80): número em rax, argumentos em rdi, rsi, rdx; resultado em rax.
 * Ver userland/mos.h.
 */
#include "proc.h"
#include "vfs.h"
#include "syscall.h"
#include "account.h"

#define PTE_P  1ULL
#define PTE_RW 2ULL
#define PTE_U  4ULL
#define PTE_ADDR 0x000FFFFFFFFFF000ULL
#define CONSOLE_SIZE (16u << 10)
#define MAX_ZOMBIES 16

extern thread_t *uthread_create(process_t *p, u64 entry, u64 user_rsp, u64 arg);

static process_t *procs;
static int next_pid = 1;
static u64 *kernel_pml4;
static char console[CONSOLE_SIZE];
static usize con_head, con_tail;

/* ------------------------------------------------------------ console dos programas */
static void console_put(const char *s, usize n) {
    for (usize i = 0; i < n; i++) {
        console[con_head] = s[i];
        con_head = (con_head + 1) % CONSOLE_SIZE;
        if (con_head == con_tail) con_tail = (con_tail + 1) % CONSOLE_SIZE;   /* descarta o mais antigo */
    }
}

usize proc_console_read(char *buf, usize max) {
    u64 f = irq_save();
    usize n = 0;
    while (con_tail != con_head && n < max) { buf[n++] = console[con_tail]; con_tail = (con_tail + 1) % CONSOLE_SIZE; }
    irq_restore(f);
    return n;
}

/* ------------------------------------------------------------ sessões (tty) */
static void tty_out(tty_t *t, const char *s, usize n) {
    for (usize i = 0; i < n; i++) {
        t->out[t->oh] = s[i];
        t->oh = (t->oh + 1) % sizeof t->out;
        if (t->oh == t->ot) t->ot = (t->ot + 1) % sizeof t->out;
    }
}

static void out_put(process_t *p, const char *s, usize n) {
    /* diagnóstico: com /Sistema/Biblioteca/stderr-serial, a saída dos programas também vai para o log serial */
    static int to_serial = -1;
    if (to_serial < 0) to_serial = vfs_lookup("/Sistema/Biblioteca/stderr-serial") != NULL;
    if (to_serial) kprintf("%.*s", (int)n, s);
    if (p && p->tty) tty_out(p->tty, s, n);
    else console_put(s, n);
}

/* entrega a entrada pendente a uma leitura bloqueada (rax = bytes; 0 = EOF) */
static void tty_feed_reader(tty_t *t) {
    thread_t *r = t->reader;
    if (!r || r->state != TH_WAITING) { t->reader = NULL; return; }
    if (t->ih == t->it && !t->eof) return;
    char tmp[512];
    usize n = 0;
    while (t->it != t->ih && n < MIN(sizeof tmp, r->read_len)) {
        tmp[n++] = t->in[t->it];
        t->it = (t->it + 1) % sizeof t->in;
        if (tmp[n - 1] == '\n') break;                    /* entrega linha a linha, como um terminal */
    }
    bool ok = !n || uvm_copy_out(r->proc, r->read_buf, tmp, n);
    ((int_frame_t *)r->frame)->rax = ok ? (u64)n : (u64)-MOS_EFAULT;
    r->state = TH_READY;
    t->reader = NULL;
}

static process_t *find(int pid);

int proc_spawn_tty(const char *path, const char *arg) {
    tty_t *t = kcalloc(1, sizeof *t);
    if (!t) return -1;
    extern int proc_spawn_in(const char *path, const char *arg, tty_t *tty,
                             char **xargv, int xargc, char **xenv, int xenvc,
                             mos_spawn_fa_t *acts, int nacts, unsigned spawn_flags);
    int pid = proc_spawn_in(path, arg, t, NULL, 0, NULL, 0, NULL, 0, 0);
    if (pid < 0) kfree(t);
    return pid;
}

usize proc_tty_read(int pid, char *buf, usize max) {
    u64 fl = irq_save();
    process_t *p = find(pid);
    usize n = 0;
    if (p && p->tty) {
        tty_t *t = p->tty;
        while (t->ot != t->oh && n < max) { buf[n++] = t->out[t->ot]; t->ot = (t->ot + 1) % sizeof t->out; }
    }
    irq_restore(fl);
    return n;
}

bool proc_tty_write(int pid, const char *data, usize len) {
    u64 fl = irq_save();
    process_t *p = find(pid);
    bool ok = p && p->tty && !p->tty->eof;
    if (ok) {
        tty_t *t = p->tty;
        for (usize i = 0; i < len; i++) {
            u32 nx = (t->ih + 1) % sizeof t->in;
            if (nx == t->it) { ok = false; break; }           /* entrada cheia */
            t->in[t->ih] = data[i];
            t->ih = nx;
        }
        tty_feed_reader(t);
    }
    irq_restore(fl);
    return ok;
}

static void finish(process_t *p, int code);
bool proc_signal(int pid, int sig);

int proc_tty_interrupt(int pid) {
    u64 fl = irq_save();
    process_t *lead = find(pid);
    int n = 0;
    if (lead && lead->tty) {
        for (process_t *q = procs; q; q = q->next)
            if (q != lead && q->tty == lead->tty && !q->exited) { proc_signal(q->pid, MOS_SIGINT); n++; }
        tty_out(lead->tty, "^C\n", 3);
    }
    irq_restore(fl);
    return n;
}

/* como proc_tty_interrupt, mas só age (e só ecoa ^C) quando há um processo em primeiro plano no terminal */
int proc_tty_interrupt_fg(int pid) {
    u64 fl = irq_save();
    process_t *lead = find(pid);
    bool any = false;
    if (lead && lead->tty)
        for (process_t *q = procs; q; q = q->next)
            if (q != lead && q->tty == lead->tty && !q->exited) { any = true; break; }
    irq_restore(fl);
    return any ? proc_tty_interrupt(pid) : 0;
}

void proc_tty_eof(int pid) {
    u64 fl = irq_save();
    process_t *p = find(pid);
    if (p && p->tty) { p->tty->eof = true; tty_feed_reader(p->tty); }
    irq_restore(fl);
}

/* ------------------------------------------------------------ memória de usuário */
static u64 *table(process_t *p, u64 *t, int i, bool create) {
    if (!(t[i] & PTE_P)) {
        if (!create) return NULL;
        u64 pg = pmm_alloc(1);
        if (!pg) return NULL;
        memset((void *)pg, 0, 4096);
        t[i] = pg | PTE_P | PTE_RW | PTE_U;
        p->mem_pages++;
    }
    return (u64 *)(t[i] & PTE_ADDR);
}

static u64 *walk(process_t *p, u64 va, bool create) {
    if (va < USER_BASE || va >= USER_END) return NULL;
    u64 *pdpt = table(p, p->pml4, (va >> 39) & 511, create);
    u64 *pd = pdpt ? table(p, pdpt, (va >> 30) & 511, create) : NULL;
    u64 *pt = pd ? table(p, pd, (va >> 21) & 511, create) : NULL;
    return pt ? &pt[(va >> 12) & 511] : NULL;
}

u64 *uvm_pte(process_t *p, u64 va) {
    u64 *e = walk(p, va, false);
    return e && (*e & PTE_P) ? e : NULL;
}

bool uvm_map(process_t *p, u64 va, u64 len) {
    u64 end = va + len;
    if (end < va || va < USER_BASE || end > USER_END) return false;
    for (u64 a = va & ~0xFFFULL; a < end; a += 4096) {
        u64 *e = walk(p, a, true);
        if (!e) return false;
        if (*e & PTE_P) continue;
        u64 pg = pmm_alloc(1);
        if (!pg) return false;
        memset((void *)pg, 0, 4096);
        *e = pg | PTE_P | PTE_RW | PTE_U;
        p->mem_pages++;
    }
    return true;
}

/* ------------------------------------------------------------ cópia na escrita (fork)
 * Páginas compartilhadas entre pai e filho ficam somente leitura com o bit COW (bit 9, livre para o
 * SO). A primeira escrita (falta de página) copia a página se ela ainda tiver mais de um dono.
 * pref[] conta os donos de cada página física (0 = um dono só). */
#define PTE_COW (1ULL << 9)
static u16 *pref;
static u64 pref_n;

static bool pref_ready(void) {
    if (pref) return true;
    u64 top = 0;
    /* só RAM que o pmm pode entregar (livre, do kernel, dos serviços de boot): regiões reservadas do
     * firmware lá no alto (o QEMU anuncia algumas acima de 1 TiB) faziam esta tabela pedir 512 MiB */
    for (u32 i = 0; i < g_boot->region_count; i++) {
        u32 ty = g_boot->regions[i].type;
        u64 e = g_boot->regions[i].base + g_boot->regions[i].pages * 4096;
        if (e > top && (ty == MEM_FREE || ty == MEM_KERNEL || ty == MEM_ACPI || ty == MEM_BOOTSVC)) top = e;   /* as mesmas do pmm (mm/memory.c) */
    }
    pref_n = top / 4096;
    pref = kcalloc(pref_n, sizeof *pref);
    return pref != NULL;
}

static u16 page_refs(u64 phys) { u64 pn = phys >> 12; return pref && pn < pref_n && pref[pn] ? pref[pn] : 1; }

/* larga uma referência à página; libera quando era a última */
static void page_put(u64 phys) {
    u64 pn = phys >> 12;
    if (pref && pn < pref_n && pref[pn] > 1) { pref[pn]--; return; }
    if (pref && pn < pref_n) pref[pn] = 0;
    pmm_free(phys, 1);
}

static bool cow_break(process_t *p, u64 *e, u64 va) {
    u64 phys = *e & PTE_ADDR;
    if (page_refs(phys) > 1) {
        u64 np = pmm_alloc(1);
        if (!np) return false;
        memcpy((void *)np, (void *)phys, 4096);
        pref[phys >> 12]--;
        *e = np | PTE_P | PTE_RW | PTE_U;
    } else *e = (*e | PTE_RW) & ~PTE_COW;
    if ((u64)p->pml4 == read_cr3()) __asm__ volatile("invlpg (%0)" :: "r"(va & ~0xFFFULL) : "memory");
    return true;
}

/* memória Mach compartilhada (machipc.c): a mesma página física em vários espaços, contada em pref[]
 * como as do COW; cada mapeamento e cada objeto de memória é um dono */
void uvm_page_get(u64 phys) {
    if (!pref_ready()) return;
    u64 pn = phys >> 12;
    if (pn < pref_n) pref[pn] = (u16)(page_refs(phys) + 1);
}
void uvm_page_put(u64 phys) { page_put(phys); }
bool uvm_map_phys(process_t *p, u64 va, u64 phys, bool rw) {
    if (!pref_ready()) return false;
    u64 *e = walk(p, va & ~0xFFFULL, true);
    if (!e) return false;
    if (*e & PTE_P) { page_put(*e & PTE_ADDR); p->mem_pages--; }
    uvm_page_get(phys);
    *e = (phys & PTE_ADDR) | PTE_P | PTE_U | (rw ? PTE_RW : 0);
    p->mem_pages++;
    if ((u64)p->pml4 == read_cr3()) __asm__ volatile("invlpg (%0)" :: "r"(va & ~0xFFFULL) : "memory");
    return true;
}

/* imagens Mach-O compartilhadas entre processos (macho.c, icache): as mesmas páginas físicas em cópia-na-escrita, como
 * no fork. uvm_share_cow põe a página de va em COW e dá mais uma referência a ela (a do cache); uvm_map_cow mapeia em
 * va, como COW, uma página do cache. */
bool uvm_share_cow(process_t *p, u64 va, u64 *phys) {
    u64 *e = uvm_pte(p, va);
    if (!e || !pref_ready()) return false;
    u64 ph = *e & PTE_ADDR;
    uvm_page_get(ph);
    if (*e & PTE_RW) *e = (*e & ~PTE_RW) | PTE_COW;
    if ((u64)p->pml4 == read_cr3()) __asm__ volatile("invlpg (%0)" :: "r"(va & ~0xFFFULL) : "memory");
    *phys = ph;
    return true;
}
bool uvm_map_cow(process_t *p, u64 va, u64 phys) {
    if (!pref_ready()) return false;
    u64 *e = walk(p, va & ~0xFFFULL, true);
    if (!e) return false;
    if (*e & PTE_P) { page_put(*e & PTE_ADDR); p->mem_pages--; }
    uvm_page_get(phys);
    *e = (phys & PTE_ADDR) | PTE_P | PTE_U | PTE_COW;
    p->mem_pages++;
    if ((u64)p->pml4 == read_cr3()) __asm__ volatile("invlpg (%0)" :: "r"(va & ~0xFFFULL) : "memory");
    return true;
}
u16 uvm_page_refs(u64 phys) { return page_refs(phys); }
/* página COW (fork, imagem compartilhada) vira privada antes de ir, com escrita, para outro processo
 * (mach_make_memory_entry de uma faixa existente): senão o outro escreveria na cópia de todos */
bool uvm_make_private(process_t *p, u64 va) {
    u64 *e = uvm_pte(p, va);
    return e && (!(*e & PTE_COW) || cow_break(p, e, va));
}

bool lazy_fault(process_t *p, u64 va);   /* kernel/mm/lazy.c: memória sob demanda ainda não tocada */
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len) {
    const u8 *s = src;
    while (len) {
        u64 *e = uvm_pte(p, va);
        if (!e && lazy_fault(p, va)) e = uvm_pte(p, va);
        if (!e) return false;
        if ((*e & PTE_COW) && !cow_break(p, e, va)) return false;   /* o kernel também respeita o COW */
        usize k = MIN(len, 4096 - (va & 0xFFF));
        memcpy((u8 *)((*e & PTE_ADDR) + (va & 0xFFF)), s, k);
        va += k; s += k; len -= k;
    }
    return true;
}

bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len) {
    u8 *d = dst;
    while (len) {
        u64 *e = uvm_pte(p, va);
        if (!e && lazy_fault(p, va)) e = uvm_pte(p, va);
        if (!e) return false;
        usize k = MIN(len, 4096 - (va & 0xFFF));
        memcpy(d, (u8 *)((*e & PTE_ADDR) + (va & 0xFFF)), k);
        va += k; d += k; len -= k;
    }
    return true;
}

static bool copy_str_in(process_t *p, char *dst, u64 va, usize cap) {
    for (usize i = 0; i < cap; i++) {
        if (!uvm_copy_in(p, &dst[i], va + i, 1)) return false;
        if (!dst[i]) return true;
    }
    dst[cap - 1] = 0;
    return true;
}

/* caminho do usuário, relativo à pasta atual do processo se não começar com '/' (256 bytes) */
static bool copy_path(process_t *p, char *dst, u64 va) {
    char tmp[256];
    if (!copy_str_in(p, tmp, va, sizeof tmp)) return false;
    if (tmp[0] == '/') memcpy(dst, tmp, sizeof tmp);
    else snprintf(dst, 256, "%s/%s", strcmp(p->cwd, "/") ? p->cwd : "", tmp);
    return true;
}

static void free_level(u64 *t, int level) {
    for (int i = 0; i < 512; i++) {
        if (!(t[i] & PTE_P)) continue;
        u64 *child = (u64 *)(t[i] & PTE_ADDR);
        if (level > 1) { free_level(child, level - 1); pmm_free((u64)child, 1); }
        else page_put((u64)child);                  /* página de dados: pode estar compartilhada (COW) */
    }
}

/* duplica a árvore de usuário de src em dst: tabelas novas, páginas compartilhadas como COW */
static bool cow_copy_level(process_t *dst, u64 *s, u64 *d, int level) {
    for (int i = 0; i < 512; i++) {
        if (!(s[i] & PTE_P)) continue;
        if (level == 1) {
            if (s[i] & PTE_RW) s[i] = (s[i] & ~PTE_RW) | PTE_COW;
            u64 pn = (s[i] & PTE_ADDR) >> 12;
            if (pn < pref_n) pref[pn] = (u16)(page_refs(s[i] & PTE_ADDR) + 1);
            d[i] = s[i];
            dst->mem_pages++;
            continue;
        }
        u64 nt = pmm_alloc(1);
        if (!nt) return false;
        memset((void *)nt, 0, 4096);
        d[i] = nt | PTE_P | PTE_RW | PTE_U;
        dst->mem_pages++;
        if (!cow_copy_level(dst, (u64 *)(s[i] & PTE_ADDR), (u64 *)nt, level - 1)) return false;
    }
    return true;
}

/* kernel/compat/macho.c: grafo de imagens do dlopen (process_t.dyld) */
__attribute__((weak)) void macho_dyld_free(void *d) { (void)d; }
__attribute__((weak)) void *macho_dyld_clone(void *d) { (void)d; return NULL; }
__attribute__((weak)) i64 macho_dyld_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    (void)p; (void)op; (void)a; (void)b; (void)c;
    return -MOS_EINVAL;
}
__attribute__((weak)) void macho_icache_trim(void) {}   /* imagens compartilhadas que ninguém mais usa */

static void free_address_space(process_t *p) {
    if (p->dyld) { macho_dyld_free(p->dyld); p->dyld = NULL; }
    if (!p->pml4) return;
    for (int k = 0; k < USER_PML4_SLOTS; k++) {
        int i = USER_PML4_FIRST + k;
        if (!(p->pml4[i] & PTE_P)) continue;
        u64 *pdpt = (u64 *)(p->pml4[i] & PTE_ADDR);
        free_level(pdpt, 3);
        pmm_free((u64)pdpt, 1);
        p->pml4[i] = 0;
    }
    pmm_free((u64)p->pml4, 1);
    p->pml4 = NULL;
    macho_icache_trim();
}

/* ------------------------------------------------------------ ciclo de vida */
static process_t *find(int pid) {
    for (process_t *p = procs; p; p = p->next) if (p->pid == pid) return p;
    return NULL;
}

static void unlink_proc(process_t *dead) {
    for (process_t **pp = &procs; *pp; pp = &(*pp)->next)
        if (*pp == dead) { *pp = dead->next; break; }
    if (dead->tty && --dead->tty->refs == 0) kfree(dead->tty);   /* nenhum processo usa mais a sessão */
    kfree(dead);
}

bool proc_alive(int pid) { process_t *p = find(pid); return p && !p->exited; }
int proc_exit_code(int pid) { process_t *p = find(pid); return p && p->exited ? p->exit_code : -1000; }

/* processos encerrados ficam como "zumbis" até alguém chamar wait; limita quantos sobram */
/* zumbi que alguém ainda vai recolher: o pai está vivo e não recebeu o código (ppid > 0). Descartar esse
 * faz o waitpid do pai falhar com ECHILD (o tar perdia o gzip filho assim). Esses só saem acima de um teto alto. */
static bool zombie_awaited(process_t *z) {
    if (z->ppid <= 0) return false;
    process_t *par = find(z->ppid);
    return par && !par->exited;
}
static void trim_zombies(void) {
    int n = 0, awaited = 0;
    for (process_t *p = procs; p; p = p->next)
        if (p->exited && !p->main) { if (zombie_awaited(p)) awaited++; else n++; }
    while (n-- > MAX_ZOMBIES)
        for (process_t *p = procs; p; p = p->next)
            if (p->exited && !p->main && !zombie_awaited(p)) { unlink_proc(p); break; }
    while (awaited-- > MAX_ZOMBIES * 16)                       /* pai que nunca chama wait: não cresce sem limite */
        for (process_t *p = procs; p; p = p->next)
            if (p->exited && !p->main && zombie_awaited(p)) { unlink_proc(p); break; }
}

/* uma thread de usuário foi recolhida pelo escalonador; a memória sai com a última do processo */
void proc_thread_exited(thread_t *t) {
    process_t *p = t->proc;
    if (!p) return;
    if (p->main == t) p->main = NULL;
    if (--p->nthreads > 0) return;
    free_address_space(p);
    p->main = NULL;
    trim_zombies();
}

__attribute__((weak)) void compat_gui_proc_exit(process_t *p) { (void)p; }    /* kernel/compat: fecha janelas */
__attribute__((weak)) i64 compat_gui_syscall(process_t *p, u64 n, u64 a, u64 b, u64 c, u64 d, u64 e) {
    (void)p; (void)n; (void)a; (void)b; (void)c; (void)d; (void)e;
    return -MOS_EINVAL;
}

void proc_reap(process_t *p) { (void)p; }

static void fd_close(process_t *p, int fd);

/* código de saída do mOS -> status do wait(2): sinal em 0x7F, saída normal em 0xFF00.
 * finish() recebe -sinal quando o processo morre por sinal (proc_kill usa -9). */
static int posix_status(int code) {
    if (code < 0 && code >= -31) return -code;
    return (code & 0xFF) << 8;
}

/* marca o fim do processo e acorda quem espera por ele */
/* threads de p que esperam num pipe: o pipe pode sobreviver (outro processo tem a outra ponta) e a
 * thread vai ser liberada pelo escalonador; o ponteiro em rwait/wwait viraria memória solta (kfree duplo) */
/* diagnóstico do heap (kheap_check, mm/memory.c): a primeira operação depois da qual o heap está corrompido */
static void heap_probe(const char *what, process_t *p) {
    extern bool kheap_check(const char *where);
    char w[96];
    snprintf(w, sizeof w, "%s %s (pid %d)", what, p->name, p->pid);
    kheap_check(w);
}

static void drop_pipe_waiters(process_t *p, thread_t *keep) {
    for (process_t *q = procs; q; q = q->next)
        for (int i = 0; i < PROC_MAX_FDS; i++) {
            pipe_t *pp = q->fds[i].used ? q->fds[i].pipe : NULL;
            if (!pp) continue;
            if (pp->rwait && pp->rwait->proc == p && pp->rwait != keep) pp->rwait = NULL;
            if (pp->wwait && pp->wwait->proc == p && pp->wwait != keep) pp->wwait = NULL;
        }
}

static void finish(process_t *p, int code) {
    if (p->exited) return;
    p->exited = true;
    p->exit_code = code;
    drop_pipe_waiters(p, NULL);
    for (thread_t *t = sched_threads(); t; t = t->next)   /* todas as threads do processo */
        if (t->proc == p) {
            if (p->tty && p->tty->reader == t) p->tty->reader = NULL;
            t->state = TH_DEAD;
        }
    for (int i = 0; i < PROC_MAX_FDS; i++) fd_close(p, i);      /* fecha arquivos e pipes (acorda quem espera) */
    compat_gui_proc_exit(p);
    machipc_exit(p);                                  /* portas Mach: receptores morrem, envios viram nomes mortos */
    { extern void net_user_exit(process_t *p); net_user_exit(p); }   /* conexões TCP do processo (kernel/net/usersock.c) */
    { extern void lazy_exit(process_t *p); lazy_exit(p); }           /* faixas de memória sob demanda (kernel/mm/lazy.c) */
    for (process_t *c = procs; c; c = c->next) if (c->ppid == p->pid) c->ppid = 0;   /* órfãos */
    bool reported = false;
    for (thread_t *t = sched_threads(); t; t = t->next) {
        if (t->state != TH_WAITING) continue;
        if (t->wait_posix && !reported && t->proc && t->proc->pid == p->ppid &&
            (t->wait_pid == p->pid || t->wait_pid == -1)) {
            int st = posix_status(code);
            if (t->wait_status) uvm_copy_out(t->proc, t->wait_status, &st, sizeof st);
            ((int_frame_t *)t->frame)->rax = (u64)p->pid;
            t->wait_posix = false;
            t->state = TH_READY;
            reported = true;
        } else if (t->wait_posix) {
            continue;
        } else if (t->wait_pid == p->pid) {
            ((int_frame_t *)t->frame)->rax = (u64)(i64)code;
            t->state = TH_READY;
            reported = true;
        } else if (t->wait_pid == -1 && !reported && t->proc && t->proc->pid == p->ppid) {
            if (t->wait_status) uvm_copy_out(t->proc, t->wait_status, &code, sizeof code);
            ((int_frame_t *)t->frame)->rax = (u64)p->pid;
            t->state = TH_READY;
            reported = true;
        }
    }
    p->ppid = reported ? -1 : p->ppid;   /* -1: já informado a quem esperava (não volta no waitany) */
}

static u32 le32_(const u8 *b) { return (u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24; }

/* definida de verdade em kernel/compat/ (carregador Mach-O); sem ele, Mach-O é recusado */
__attribute__((weak)) bool macho_load(process_t *p, const u8 *img, usize len, const char *arg, u64 *entry,
                                      u64 *user_rsp, u64 *rdi, char *err, usize errlen) {
    (void)p; (void)img; (void)len; (void)arg; (void)entry; (void)user_rsp; (void)rdi;
    snprintf(err, errlen, "executável Mach-O não suportado (carregador ausente)");
    return false;
}

int proc_spawn_in(const char *path, const char *arg, tty_t *tty,
                  char **xargv, int xargc, char **xenv, int xenvc,
                  mos_spawn_fa_t *acts, int nacts, unsigned spawn_flags);

static void spawn_fail(process_t *p) {
    if (p->tty) p->tty->refs--;      /* quem criou a sessão (proc_spawn_tty) a libera */
    kfree(p);
}

/* o filho herda a sessão e a pasta atual de quem o criou */
int proc_spawn(const char *path, const char *arg) {
    thread_t *cur = thread_current();
    return proc_spawn_in(path, arg, cur && cur->proc ? cur->proc->tty : NULL, NULL, 0, NULL, 0, NULL, 0, 0);
}

static void fd_close(process_t *p, int fd);
static void fd_copy(process_t *dst, int dfd, process_t *src, int sfd);

/* fds do posix_spawn: sem CLOEXEC_DEFAULT o filho herda os do pai; as ações rodam em seguida.
 * dup2 lê o fd no filho se uma ação anterior o criou, senão no pai. */
static void spawn_apply_fds(process_t *child, process_t *parent, mos_spawn_fa_t *acts, int n, unsigned flags) {
    if (!child) return;
    if (parent && !(flags & MOS_SPAWN_CLOEXEC_DEFAULT))
        for (int i = 0; i < PROC_MAX_FDS; i++) if (parent->fds[i].used && !(parent->fds[i].flags & MOS_O_CLOEXEC)) fd_copy(child, i, parent, i);   /* posix_spawn = exec: FD_CLOEXEC não passa */
    for (int i = 0; i < n; i++) {
        int k = acts[i].kind, a = acts[i].a, b = acts[i].b;
        if (k == MOS_SPAWN_FA_CLOSE) {
            if (a >= 0 && a < PROC_MAX_FDS) fd_close(child, a);
        } else if (k == MOS_SPAWN_FA_DUP2 && a >= 0 && a < PROC_MAX_FDS && b >= 0 && b < PROC_MAX_FDS) {
            process_t *src = (child->fds[a].used) ? child : parent;
            if (src && src->fds[a].used) fd_copy(child, b, src, a);
        } else if (k == MOS_SPAWN_FA_OPEN && a >= 0 && a < PROC_MAX_FDS && acts[i].path[0]) {
            if (child->fds[a].used) fd_close(child, a);
            memset(&child->fds[a], 0, sizeof child->fds[a]);
            child->fds[a].used = true;
            child->fds[a].kind = FD_FILE;
            child->fds[a].flags = acts[i].flags;
            strncpy(child->fds[a].path, acts[i].path, sizeof child->fds[a].path - 1);
        } else if (k == MOS_SPAWN_FA_CHDIR && acts[i].path[0]) {
            strncpy(child->cwd, acts[i].path, sizeof child->cwd - 1);
        }
    }
}

int proc_spawn_in(const char *path, const char *arg, tty_t *tty,
                  char **xargv, int xargc, char **xenv, int xenvc,
                  mos_spawn_fa_t *acts, int nacts, unsigned spawn_flags) {
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir) {
        char alt[160];                                  /* nome curto: procura em /Sistema/Programas */
        snprintf(alt, sizeof alt, "/Sistema/Programas/%s", path);
        if (path[0] != '/') v = vfs_lookup(alt);
        if (!v || v->dir) { kprintf("proc: %s: programa não encontrado\n", path); return -1; }
    }
    if (!v->data || !v->size) { kprintf("proc: %s: arquivo vazio\n", path); return -1; }
    process_t *p = kcalloc(1, sizeof *p);
    if (!p) return -1;
    p->pid = next_pid++;
    thread_t *cur = thread_current();
    p->ppid = cur && cur->proc ? cur->proc->pid : 0;
    strncpy(p->cwd, cur && cur->proc ? cur->proc->cwd : account_home(), sizeof p->cwd - 1);
    p->tty = tty;
    if (tty) tty->refs++;
    const char *base = strrchr(path, '/');
    strncpy(p->name, base ? base + 1 : path, sizeof p->name - 1);
    strncpy(p->path, path, sizeof p->path - 1);
    p->start_ms = timer_ticks();
    p->pml4 = (u64 *)pmm_alloc(1);
    if (!p->pml4) { spawn_fail(p); return -1; }
    memcpy(p->pml4, kernel_pml4, 4096);
    for (int k = 0; k < USER_PML4_SLOTS; k++) p->pml4[USER_PML4_FIRST + k] = 0;
    p->mem_pages = 1;

    char err[128];
    u64 entry = 0;
    u32 magic = v->size >= 4 ? le32_(v->data) : 0;
    if (magic == 0xFEEDFACFu || magic == 0xBEBAFECAu || magic == 0xCAFEBABEu) {
        /* Mach-O (kernel/compat/, sessão de compatibilidade): monta pilha e argumentos sozinho */
        u64 usp = 0, rdi = 0;
        p->spawn_argv = xargv;
        p->spawn_argc = xargc;
        p->spawn_env = xenv;
        p->spawn_envc = xenvc;
        if (!macho_load(p, v->data, v->size, arg, &entry, &usp, &rdi, err, sizeof err)) {
            p->spawn_argv = NULL; p->spawn_argc = 0; p->spawn_env = NULL; p->spawn_envc = 0;
            kprintf("proc: %s: %s\n", path, err);
            free_address_space(p);
            spawn_fail(p);
            return -1;
        }
        spawn_apply_fds(p, cur ? cur->proc : NULL, acts, nacts, spawn_flags);
        u64 fl = irq_save();
        p->next = procs;
        procs = p;
        machipc_spawn(cur ? cur->proc : NULL, p);        /* porta de bootstrap herdada (Mach) */
        p->main = uthread_create(p, entry, usp, rdi);
        p->nthreads = p->main ? 1 : 0;
        irq_restore(fl);
        if (!p->main) {
            p->spawn_argv = NULL; p->spawn_argc = 0; p->spawn_env = NULL; p->spawn_envc = 0;
            finish(p, -1); return -1;
        }
        p->spawn_argv = NULL; p->spawn_argc = 0; p->spawn_env = NULL; p->spawn_envc = 0;
        kprintf("proc: iniciado %s (Mach-O, pid %d, %llu KiB)\n", p->name, p->pid, p->mem_pages * 4);
        if (xargc || xenvc) kprintf("proc: %s pid %d recebeu %d argumentos e %d variáveis de ambiente\n", p->name, p->pid, xargc, xenvc);
        return p->pid;
    }
    if (!elf_load(p, v->data, v->size, &entry, err, sizeof err)) {
        kprintf("proc: %s: %s\n", path, err);
        free_address_space(p);
        spawn_fail(p);
        return -1;
    }
    /* pilha + argumento (string copiada para o topo) */
    u64 lo = USER_STACK_TOP - USER_STACK_SIZE;
    if (!uvm_map(p, lo, USER_STACK_SIZE)) { free_address_space(p); spawn_fail(p); return -1; }
    u64 sp = USER_STACK_TOP, argp = 0;
    if (arg) {
        usize n = strlen(arg) + 1;
        if (n > 4096) n = 4096;
        sp -= (n + 15) & ~15ULL;
        uvm_copy_out(p, sp, arg, n - 1);
        u8 z = 0;
        uvm_copy_out(p, sp + n - 1, &z, 1);
        argp = sp;
    }
    sp = (sp & ~15ULL) - 8;                             /* como depois de um call */
    u64 zero = 0;
    uvm_copy_out(p, sp, &zero, 8);

    spawn_apply_fds(p, cur ? cur->proc : NULL, acts, nacts, spawn_flags);
    u64 fl = irq_save();
    p->next = procs;
    procs = p;
    machipc_spawn(cur ? cur->proc : NULL, p);
    p->main = uthread_create(p, entry, sp, argp);
    p->nthreads = p->main ? 1 : 0;
    irq_restore(fl);
    if (!p->main) { finish(p, -1); return -1; }
    kprintf("proc: iniciado %s (pid %d, %llu KiB)\n", p->name, p->pid, p->mem_pages * 4);
    return p->pid;
}

bool proc_kill(int pid) {
    u64 fl = irq_save();
    process_t *p = find(pid);
    bool ok = p && !p->exited;
    if (ok) { kprintf("proc: %s (pid %d) encerrado à força\n", p->name, pid); finish(p, -9); }
    irq_restore(fl);
    return ok;
}

int proc_count(void) {
    int n = 0;
    for (process_t *p = procs; p; p = p->next) if (!p->exited) n++;
    return n;
}

bool proc_info(int i, int *pid, char *name, usize ncap, int *state, u64 *mem_kb, u64 *cpu_ms) {
    for (process_t *p = procs; p; p = p->next) {
        if (p->exited || i-- > 0) continue;
        if (pid) *pid = p->pid;
        if (name) { strncpy(name, p->name, ncap - 1); name[ncap - 1] = 0; }
        if (state) *state = p->main ? p->main->state : TH_DEAD;
        if (mem_kb) *mem_kb = p->mem_pages * 4;
        if (cpu_ms) *cpu_ms = p->main ? p->main->cpu_ms : 0;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ exceções e syscalls */
static const char *fault_name(u64 v) {
    switch (v) {
    case 0: return "divisão por zero";
    case 6: return "instrução inválida";
    case 13: return "proteção geral";
    case 14: return "falta de página";
    default: return "exceção";
    }
}

u64 proc_fault(int_frame_t *f, u64 cr2) {
    thread_t *t = thread_current();
    process_t *p = t->proc;
    if (!p) return (u64)f;
    /* escrita numa página compartilhada depois do fork: copia e repete a instrução */
    if (f->vector == 14 && (f->error & 7) == 7) {
        u64 *e = uvm_pte(p, cr2);
        if (e && (*e & PTE_COW) && cow_break(p, e, cr2)) return (u64)f;
    }
    /* página ausente numa faixa de memória sob demanda (mmap anônimo, kernel/mm/lazy.c): página zerada e repete */
    if (f->vector == 14 && !(f->error & 1)) {
        extern bool lazy_fault(process_t *p, u64 va);
        if (lazy_fault(p, cr2)) return (u64)f;
    }
    /* tracepoint de diagnóstico (proc/tracepoint.c): registra, devolve o byte original e repete a instrução */
    { extern bool tp_hit(process_t *p, int_frame_t *f); if (tp_hit(p, f)) return (u64)f; }
    /* o programa tem tratador para esta falha? entrega o sinal */
    int sig = f->vector == 14 || f->vector == 13 ? MOS_SIGSEGV : f->vector == 0 ? MOS_SIGFPE : f->vector == 6 ? MOS_SIGILL : 0;
    if (sig && p->sig_handler[sig] > MOS_SIG_IGN && !(p->sig_pending & (1u << sig))) {
        /* CHECK do Chromium é ud2. O crashpad instala tratador e sai em silêncio; sem esta linha
         * o renderer do VS Code desaparece do dump sem "processo encerrado". */
        if (f->vector == 6)
            kprintf("proc: %s (pid %d): instrução inválida em 0x%llx (entregue ao tratador)\n", p->name, p->pid, f->rip);
        p->sig_pending |= 1u << sig;
        return proc_signal_check(f);
    }
    char msg[160];
    if (f->vector == 14) snprintf(msg, sizeof msg, "%s (pid %d): %s em 0x%llx, endereço 0x%llx — processo encerrado\n",
                                  p->name, p->pid, fault_name(f->vector), f->rip, cr2);
    else snprintf(msg, sizeof msg, "%s (pid %d): %s (vetor %llu) em 0x%llx — processo encerrado\n",
                  p->name, p->pid, fault_name(f->vector), (unsigned long long)f->vector, f->rip);
    kprintf("proc: %s", msg);
    /* diagnóstico: thread e registradores principais (no log serial) */
    thread_t *ft = thread_current();
    kprintf("proc:   thread %d, rsp 0x%llx, rax 0x%llx, rbx 0x%llx, rdi 0x%llx, rsi 0x%llx\n",
            ft ? ft->id : -1, f->rsp, f->rax, f->rbx, f->rdi, f->rsi);
    {   /* endereços de código na pilha do usuário (prováveis endereços de retorno).
         * Uma palavra por vez: o topo da pilha pode estar na última página mapeada. */
        char line[200];
        usize o = 0;
        int shown = 0, strs = 0;
        u64 spw = f->rsp & ~7ULL;
        for (int i = 0; i < 96 && (shown < 16 || strs < 8); i++) {
            u64 v = 0;
            if (!uvm_copy_in(p, &v, spw + (u64)i * 8, 8)) break;
            if (shown < 16 && v >= USER_BASE + 0x100000ULL && v < USER_BASE + 0x0E000000ULL) {
                o += (usize)snprintf(line + o, sizeof line - o, " %llx", v);
                shown++;
                if (o > 150) { kprintf("proc:   pilha:%s\n", line); o = 0; }
            }
            if (strs < 8 && v >= USER_BASE && v < USER_END) {
                char s[49];
                if (!uvm_copy_in(p, s, v, 48)) continue;
                s[48] = 0;
                int k = 0;
                while (k < 48 && s[k] >= 0x20 && s[k] < 0x7F) k++;
                if (k < 4 || (k < 48 && s[k] != 0)) continue;
                kprintf("proc:   texto [rsp+%d] %llx: \"%s\"\n", i * 8, v, s);
                strs++;
            }
        }
        if (o) kprintf("proc:   pilha:%s\n", line);
    }
    console_put(msg, strlen(msg));
    finish(p, -(int)(f->vector == 14 ? 11 : 4 + f->vector));
    return sched_switch(f);
}

/* diagnóstico: com /Sistema/Biblioteca/despejar-threads (e stderr-serial), a cada 30 s o log serial recebe,
 * para cada thread de usuário, o estado, o rip/rsp salvos e os endereços de código na pilha — para achar
 * onde um app (ex.: o VS Code) ficou parado sem falhar. Disparado de SYS_SLEEP (contexto de syscall). */
thread_t *sched_threads(void);
static void dump_threads_maybe(int_frame_t *cur) {
    static int on = -1;
    static u64 last;
    if (on < 0) on = vfs_lookup("/Sistema/Biblioteca/despejar-threads") != NULL;
    if (!on || timer_ticks() - last < 30000) return;
    last = timer_ticks();
    static const char *st[] = {"pronta", "rodando", "dormindo", "esperando", "morta"};
    kprintf("despejo: threads em %llu ms\n", (unsigned long long)last);
    for (thread_t *t = sched_threads(); t; t = t->next) {
        if (!t->proc || t->state == TH_DEAD || (!t->frame && t != thread_current())) continue;
        int_frame_t *f = t == thread_current() ? cur : (int_frame_t *)t->frame;   /* a atual: o frame desta syscall */
        kprintf("despejo: %s pid %d thread %d \"%s\" %s (espera %d) rip %llx rsp %llx cpu %llu ms\n", t->proc->name, t->proc->pid, t->id,
                t->name, t->state <= TH_DEAD ? st[t->state] : "?", t->wait_pid, f->rip, f->rsp, (unsigned long long)t->cpu_ms);
        /* palavra por palavra até o fim da pilha mapeada: uma thread rasa (logo abaixo do topo) não cabia em
         * um bloco de 1280 bytes e saía sem nenhuma linha de pilha */
        u64 words[160];
        int nw = 0;
        while (nw < 160 && uvm_copy_in(t->proc, &words[nw], (f->rsp & ~7ULL) + (u64)nw * 8, 8)) nw++;
        if (!nw) continue;
        for (int k = nw; k < 160; k++) words[k] = 0;
        char line[200];
        usize o = 0;
        int shown = 0;
        for (int i = 0; i < 160 && shown < 20; i++) {
            u64 v = words[i];
            /* libSystem + imagens do spawn, e as do dlopen (ex.: Google Chrome Framework em USER_BASE + 4 GiB) */
            bool code = (v >= USER_BASE + 0x100000ULL && v < USER_BASE + 0x0E000000ULL) ||
                        (v >= USER_BASE + 0x100000000ULL && v < USER_BASE + 0x120000000ULL);
            if (!code) continue;
            o += (usize)snprintf(line + o, sizeof line - o, " %llx", v);
            shown++;
            if (o > 150) { kprintf("despejo:   pilha:%s\n", line); o = 0; }
        }
        if (o) kprintf("despejo:   pilha:%s\n", line);
    }
    /* filas Mach com mensagens que ninguém recebeu ainda (machipc.c) */
    { extern void machipc_dump_queues(process_t *p); for (process_t *q = procs; q; q = q->next) machipc_dump_queues(q); }
}

/* ------------------------------------------------------------ sinais
 * Entrega na volta para o ring 3: grava na pilha do usuário o frame interrompido e o estado FXSAVE,
 * empilha o endereço do "restorer" (que chama SYS_SIGRETURN) e desvia para o tratador(sig). */
typedef struct { int_frame_t frame; u8 fx[512]; u64 magic; } sigframe_t;
#define SIGFRAME_MAGIC 0x6D4F535349474E4CULL   /* "mOSSIGNL" */

static bool default_ignored(int sig) { return sig == MOS_SIGCHLD; }

u64 proc_signal_check(int_frame_t *f) {
    thread_t *t = thread_current();
    process_t *p = t ? t->proc : NULL;
    if (!p || p->exited || !p->sig_pending || (f->cs & 3) != 3) return (u64)f;
    int sig = 0;
    while (!(p->sig_pending & (1u << sig))) sig++;
    p->sig_pending &= ~(1u << sig);
    u64 h = p->sig_handler[sig];
    if (h == MOS_SIG_IGN || (h == MOS_SIG_DFL && default_ignored(sig))) return proc_signal_check(f);
    if (h == MOS_SIG_DFL || !p->sig_restorer) {                     /* ação padrão: encerra */
        char m[96];
        snprintf(m, sizeof m, "%s (pid %d) encerrado pelo sinal %d\n", p->name, p->pid, sig);
        if (sig != MOS_SIGINT) kprintf("proc: %s", m);
        finish(p, -sig);
        return sched_switch(f);
    }
    sigframe_t *sf = kmalloc(sizeof *sf);
    if (!sf) { finish(p, -sig); return sched_switch(f); }
    sf->frame = *f;
    memcpy(sf->fx, (void *)(((u64)f - 512) & ~15ULL), 512);
    sf->magic = SIGFRAME_MAGIC;
    u64 sp = ((f->rsp - 128 - sizeof *sf) & ~15ULL);                 /* respeita a red zone */
    u64 ret = sp - 8;
    bool ok = uvm_copy_out(p, sp, sf, sizeof *sf) && uvm_copy_out(p, ret, &p->sig_restorer, 8);
    kfree(sf);
    if (!ok) { finish(p, -MOS_SIGSEGV); return sched_switch(f); }   /* pilha estourada */
    f->rsp = ret;                                                    /* rsp = 16n+8, como depois de um call */
    f->rip = h;
    f->rdi = (u64)sig;
    return (u64)f;
}

static u64 sig_return(int_frame_t *f) {
    thread_t *t = thread_current();
    process_t *p = t->proc;
    sigframe_t *sf = kmalloc(sizeof *sf);
    if (!sf || !uvm_copy_in(p, sf, f->rsp, sizeof *sf) || sf->magic != SIGFRAME_MAGIC) {
        kfree(sf);
        finish(p, -MOS_SIGSEGV);
        return sched_switch(f);
    }
    int_frame_t saved = sf->frame;
    saved.cs = 0x1B; saved.ss = 0x23;                                /* nunca volta para outro anel */
    saved.rflags = (saved.rflags & 0xCD5) | 0x202;                   /* só flags aritméticas + IF */
    *f = saved;
    memcpy((void *)(((u64)f - 512) & ~15ULL), sf->fx, 512);
    kfree(sf);
    return proc_signal_check(f);
}

bool proc_signal(int pid, int sig) {
    if (sig == 0) return proc_alive(pid);                /* kill(pid, 0): só pergunta se o processo existe */
    if (sig < 0 || sig >= MOS_NSIG) return false;
    u64 fl = irq_save();
    process_t *p = find(pid);
    bool ok = p && !p->exited;
    if (ok) {
        if (sig == MOS_SIGKILL) finish(p, -MOS_SIGKILL);
        else {
            p->sig_pending |= 1u << sig;
            /* acorda quem estiver dormindo/esperando para receber o sinal (a syscall devolve -EINTR) */
            for (thread_t *t = sched_threads(); t; t = t->next)
                if (t->proc == p && (t->state == TH_SLEEPING || t->state == TH_WAITING)) {
                    if (p->tty && p->tty->reader == t) p->tty->reader = NULL;
                    ((int_frame_t *)t->frame)->rax = (u64)-4;
                    t->state = TH_READY;
                    break;
                }
        }
    }
    irq_restore(fl);
    return ok;
}

/* ------------------------------------------------------------ arquivos por descritor */
static i64 unix_now(void) {
    datetime_t dt;
    rtc_now(&dt);
    int y = dt.year - (dt.month <= 2), era = y / 400, yoe = y - era * 400;
    int doy = (153 * (dt.month + (dt.month > 2 ? -3 : 9)) + 2) / 5 + dt.day - 1;
    i64 days = (i64)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

static int fd_get(process_t *p, u64 fd) { return fd < PROC_MAX_FDS && p->fds[fd].used ? (int)fd : -1; }

/* ------------------------------------------------------------ pipes */
#define EAGAIN_ 11
static i64 pipe_try_read(pipe_t *pp, process_t *p, u64 buf, u64 len) {
    if (!pp->count) return pp->writers ? -EAGAIN_ : 0;          /* vazio: espera, ou EOF sem escritores */
    char tmp[512];
    u64 done = 0;
    while (done < len && pp->count) {
        u32 k = 0;
        while (k < sizeof tmp && done + k < len && pp->count) { tmp[k++] = pp->buf[pp->tail]; pp->tail = (pp->tail + 1) % sizeof pp->buf; pp->count--; }
        if (!uvm_copy_out(p, buf + done, tmp, k)) return done ? (i64)done : -MOS_EFAULT;
        done += k;
    }
    return (i64)done;
}

static i64 pipe_try_write(pipe_t *pp, process_t *p, u64 buf, u64 len) {
    if (!pp->readers) return -MOS_EPIPE;
    if (pp->count == sizeof pp->buf) return -EAGAIN_;
    char tmp[512];
    u64 done = 0;
    while (done < len && pp->count < sizeof pp->buf) {
        u32 k = (u32)MIN(MIN(len - done, (u64)sizeof tmp), (u64)(sizeof pp->buf - pp->count));
        if (!uvm_copy_in(p, tmp, buf + done, k)) return done ? (i64)done : -MOS_EFAULT;
        for (u32 i = 0; i < k; i++) { pp->buf[pp->head] = tmp[i]; pp->head = (pp->head + 1) % sizeof pp->buf; }
        pp->count += k;
        done += k;
    }
    return (i64)done;
}

/* tenta concluir as operações bloqueadas no pipe (chamado após cada mudança) */
static void pipe_service(pipe_t *pp) {
    for (int round = 0; round < 2; round++) {
        thread_t *r = pp->rwait;
        if (r && r->state == TH_WAITING) {
            i64 n = pipe_try_read(pp, r->proc, r->read_buf, r->read_len);
            if (n != -EAGAIN_) { ((int_frame_t *)r->frame)->rax = (u64)n; r->state = TH_READY; pp->rwait = NULL; }
        } else pp->rwait = NULL;
        thread_t *w = pp->wwait;
        if (w && w->state == TH_WAITING) {
            i64 n = pipe_try_write(pp, w->proc, w->read_buf, w->read_len);
            if (n != -EAGAIN_) { ((int_frame_t *)w->frame)->rax = (u64)n; w->state = TH_READY; pp->wwait = NULL; }
        } else pp->wwait = NULL;
    }
}

static void fd_close(process_t *p, int fd) {
    if (!p->fds[fd].used) return;
    pipe_t *pp = p->fds[fd].pipe;
    if (pp) {
        if (p->fds[fd].kind == FD_PIPE_R) pp->readers--; else pp->writers--;
        pipe_service(pp);
        if (pp->readers <= 0 && pp->writers <= 0) kfree(pp);
    }
    p->fds[fd].used = false;
    p->fds[fd].pipe = NULL;
}

static void fd_copy(process_t *dst, int dfd, process_t *src, int sfd) {
    /* a referência nova entra antes de fechar o destino: se os dois são do mesmo pipe e o destino era a
     * última referência, fechar primeiro liberaria o pipe que a cópia ainda vai usar (kfree duplo depois) */
    __typeof__(src->fds[0]) e = src->fds[sfd];
    if (e.pipe) { if (e.kind == FD_PIPE_R) e.pipe->readers++; else e.pipe->writers++; }
    if (dst->fds[dfd].used) fd_close(dst, dfd);
    dst->fds[dfd] = e;
}

/* fileports (machipc.c, portas KP_FILE): a porta guarda uma cópia da entrada do descritor (caminho, modo,
 * posição, ponta de pipe com a sua referência); fileport_makefd instala outra cópia no processo que recebeu.
 * A posição não é compartilhada entre as cópias (cada uma anda sozinha), o que basta para o Mojo: o Chromium
 * lê esses arquivos com pread/mmap ou do começo. */
typedef __typeof__(((process_t *)0)->fds[0]) fdent_t;
void *proc_fd_snapshot(process_t *p, int fd) {
    if (fd < 0 || fd >= PROC_MAX_FDS || !p->fds[fd].used) return NULL;
    fdent_t *s = kmalloc(sizeof *s);
    if (!s) return NULL;
    *s = p->fds[fd];
    s->flags &= ~MOS_O_CLOEXEC;
    if (s->pipe) { if (s->kind == FD_PIPE_R) s->pipe->readers++; else s->pipe->writers++; }
    return s;
}
int proc_fd_from_snapshot(process_t *p, const void *snap) {
    const fdent_t *s = snap;
    for (int fd = 3; fd < PROC_MAX_FDS; fd++) {
        if (p->fds[fd].used) continue;
        p->fds[fd] = *s;
        p->fds[fd].used = true;
        if (s->pipe) { if (s->kind == FD_PIPE_R) s->pipe->readers++; else s->pipe->writers++; }
        return fd;
    }
    return -MOS_EMFILE;
}
void proc_fd_snapshot_free(void *snap) {
    fdent_t *s = snap;
    if (!s) return;
    pipe_t *pp = s->pipe;
    if (pp) {
        if (s->kind == FD_PIPE_R) pp->readers--; else pp->writers--;
        pipe_service(pp);
        if (pp->readers <= 0 && pp->writers <= 0) kfree(pp);
    }
    kfree(s);
}

/* operação de pipe que pode bloquear: devolve o frame a restaurar */
static u64 pipe_op(int_frame_t *f, thread_t *t, pipe_t *pp, bool rd, u64 buf, u64 len) {
    i64 n = rd ? pipe_try_read(pp, t->proc, buf, len) : pipe_try_write(pp, t->proc, buf, len);
    if (n != -EAGAIN_) {
        pipe_service(pp);
        f->rax = (u64)n;
        return (u64)f;
    }
    if (rd ? pp->rwait != NULL : pp->wwait != NULL) { f->rax = (u64)-MOS_EINVAL; return (u64)f; }   /* um por vez */
    t->frame = (u64)f;
    t->read_buf = buf;
    t->read_len = len;
    t->wait_pid = rd ? -3 : -4;
    t->state = TH_WAITING;
    if (rd) pp->rwait = t; else pp->wwait = t;
    return sched_switch(f);
}

/* grava len bytes na posição off (o VFS guarda arquivos inteiros: reconstrói o conteúdo) */
static i64 file_pwrite(const char *path, u64 off, const u8 *data, u64 len) {
    vnode_t *v = vfs_lookup(path);
    if (v && v->dir) return -MOS_EISDIR;
    u64 old = v ? v->size : 0, nsz = MAX(old, off + len);
    if (nsz > (256ull << 20)) return -MOS_ENOSPC;
    u8 *buf = kcalloc(1, nsz ? nsz : 1);
    if (!buf) return -MOS_ENOMEM;
    if (v && old && v->data) memcpy(buf, v->data, old);
    memcpy(buf + off, data, len);
    vnode_t *w = vfs_write_file(path, buf, nsz);
    kfree(buf);
    return w ? (i64)len : -MOS_ENOENT;
}

static void fill_stat(vnode_t *v, mos_stat_t *st) {
    st->size = v->dir ? 0 : v->size;
    st->is_dir = v->dir;
    /* vnode->mtime: ms desde o boot (0 = veio no disco de boot); sem data real, vale a hora do boot.
     * Com 0 (1970) o gzip e o tar reclamam de "timestamp out of range". */
    i64 boot = unix_now() - (i64)(timer_ticks() / 1000);
    st->mtime = boot + (i64)(v->mtime / 1000);
}

/* /dev/null: não existe no VFS; open aceita, write descarta, read dá fim de arquivo (o git exige) */
static bool is_devnull(const char *path) { return !strcmp(path, "/dev/null"); }

static i64 sys_write(process_t *p, int fd, u64 buf, u64 len) {
    if (fd_get(p, fd) >= 0) {
        if (!(p->fds[fd].flags & (MOS_O_WRONLY | MOS_O_RDWR))) return -MOS_EBADF;
        if (is_devnull(p->fds[fd].path)) return (i64)len;
        u8 *tmp = len ? kmalloc(len) : NULL;
        if (len && !tmp) return -MOS_ENOMEM;
        if (len && !uvm_copy_in(p, tmp, buf, len)) { kfree(tmp); return -MOS_EFAULT; }
        if (p->fds[fd].flags & MOS_O_APPEND) { vnode_t *v = vfs_lookup(p->fds[fd].path); p->fds[fd].off = v ? v->size : 0; }
        i64 r = file_pwrite(p->fds[fd].path, p->fds[fd].off, tmp, len);
        if (r > 0) p->fds[fd].off += (u64)r;
        kfree(tmp);
        return r;
    }
    if (fd != 1 && fd != 2) return -MOS_EBADF;
    char tmp[256];
    u64 done = 0;
    while (done < len) {
        usize k = (usize)MIN(len - done, sizeof tmp - 1);
        if (!uvm_copy_in(p, tmp, buf + done, k)) return done ? (i64)done : -1;
        tmp[k] = 0;
        out_put(p, tmp, k);
        if (p->tty) { done += k; continue; }             /* sessão de terminal: não polui a serial */
        kprintf("%s", tmp);
        done += k;
    }
    return (i64)done;
}

static void free_vec(char **v, int n) {
    if (!v) return;
    for (int i = 0; i < n; i++) kfree(v[i]);
    kfree(v);
}

/* copia um vetor NULL-terminado de strings (argv ou envp) do processo pai.
 * Cada string cabe em 64 KiB; o conjunto, em 384 KiB. O Electron passa bem mais
 * que os 256 bytes do SYS_SPAWN antigo. */
static char **copy_user_vec(process_t *parent, u64 uvec, int *out_n) {
    enum { MAXN = 384, STR = 65536, BUDGET = 384 << 10 };
    char **v = kcalloc(MAXN, sizeof(char *));
    if (!v) return NULL;
    int n = 0;
    usize used = 0;
    for (;;) {
        u64 ptr = 0;
        if (!uvm_copy_in(parent, &ptr, uvec + (u64)n * 8, 8)) { free_vec(v, n); return NULL; }
        if (!ptr) break;
        if (n >= MAXN - 1) { kprintf("proc: spawn: argumentos demais\n"); free_vec(v, n); return NULL; }
        char *s = kmalloc(STR);
        if (!s) { free_vec(v, n); return NULL; }
        if (!copy_str_in(parent, s, ptr, STR) || strlen(s) == STR - 1) {
            kprintf("proc: spawn: argumento longo demais\n");
            kfree(s); free_vec(v, n); return NULL;
        }
        usize len = strlen(s);
        if (used + len + 1 > BUDGET) { kprintf("proc: spawn: linha de comando grande demais\n"); kfree(s); free_vec(v, n); return NULL; }
        used += len + 1;
        v[n++] = s;
    }
    *out_n = n;
    return v;
}

/* ------------------------------------------------------------ execve */
#define LINK_MAGIC "#mOS-link:"          /* links simbólicos do libSystem (userland/libsystem/sockets.c) */

/* segue os links do libSystem: o alvo relativo vale a partir da pasta do link */
static vnode_t *exec_follow(char *path, usize cap) {
    for (int hop = 0; hop < 8; hop++) {
        vnode_t *v = vfs_lookup(path);
        usize ml = sizeof LINK_MAGIC - 1;
        if (!v || v->dir || v->size <= ml || memcmp(v->data, LINK_MAGIC, ml)) return v;
        char tgt[256];
        usize n = v->size - ml;
        if (n >= sizeof tgt) return NULL;
        memcpy(tgt, v->data + ml, n);
        tgt[n] = 0;
        while (n && (tgt[n - 1] == '\n' || tgt[n - 1] == '\r')) tgt[--n] = 0;
        if (tgt[0] == '/') { strncpy(path, tgt, cap - 1); path[cap - 1] = 0; }
        else {
            char dir[256];
            strncpy(dir, path, sizeof dir - 1); dir[sizeof dir - 1] = 0;
            char *sl = strrchr(dir, '/');
            if (sl) *sl = 0;
            snprintf(path, cap, "%s/%s", dir, tgt);
        }
    }
    return NULL;                                   /* ELOOP */
}

/* acrescenta argumentos na frente do argv (shebang): devolve o vetor novo, libera o antigo */
static char **vec_prepend(char **v, int *n, const char *x0, const char *x1, const char *x2) {
    const char *xs[3] = {x0, x1, x2};
    int k = 0;
    for (int i = 0; i < 3; i++) if (xs[i]) k++;
    int skip = *n > 0 ? 1 : 0;                     /* o argv[0] antigo dá lugar ao caminho do script */
    char **nv = kcalloc((usize)(*n - skip + k + 1), sizeof(char *));
    if (!nv) return NULL;
    int o = 0;
    for (int i = 0; i < 3; i++) if (xs[i]) {
        usize l = strlen(xs[i]) + 1;
        nv[o] = kmalloc(l);
        if (!nv[o]) { free_vec(nv, o); return NULL; }
        memcpy(nv[o++], xs[i], l);
    }
    for (int i = skip; i < *n; i++) nv[o++] = v[i];
    if (skip && v) kfree(v[0]);
    kfree(v);
    *n = o;
    return nv;
}

/* resolve o programa: links, nome curto em /Sistema/Programas e "#!interpretador [arg]".
 * Pode trocar path e argv. Devolve o vnode da imagem binária final. */
static vnode_t *exec_resolve(char *path, usize cap, char ***argv, int *argc) {
    for (int depth = 0; depth < 4; depth++) {
        vnode_t *v = exec_follow(path, cap);
        if ((!v || v->dir) && path[0] != '/') {
            char alt[256];
            snprintf(alt, sizeof alt, "/Sistema/Programas/%s", path);
            strncpy(path, alt, cap - 1);
            v = exec_follow(path, cap);
        }
        if (!v || v->dir || !v->data || v->size < 4) return NULL;
        if (v->data[0] != '#' || v->data[1] != '!') return v;
        char line[256];
        usize i = 2, o = 0;
        while (i < v->size && (v->data[i] == ' ' || v->data[i] == '\t')) i++;
        while (i < v->size && v->data[i] != '\n' && o < sizeof line - 1) line[o++] = (char)v->data[i++];
        while (o && (line[o - 1] == '\r' || line[o - 1] == ' ' || line[o - 1] == '\t')) o--;
        line[o] = 0;
        char *interp = line, *opt = NULL;
        for (char *s = line; *s; s++) if (*s == ' ' || *s == '\t') {
            *s++ = 0;
            while (*s == ' ' || *s == '\t') s++;
            if (*s) opt = s;
            break;
        }
        if (!interp[0]) return NULL;
        char script[256];
        strncpy(script, path, sizeof script - 1); script[sizeof script - 1] = 0;
        char **nv = vec_prepend(*argv, argc, interp, opt, script);
        if (!nv) return NULL;
        *argv = nv;
        strncpy(path, interp, cap - 1); path[cap - 1] = 0;
    }
    return NULL;
}

/* troca a imagem do processo atual. A imagem nova é montada num processo provisório; só depois
 * de carregada com sucesso o espaço de endereçamento antigo é descartado (falha = execve volta erro). */
static i64 proc_exec(int_frame_t *f, thread_t *t, process_t *p, char *path, char **argv, int argc, char **env, int envc) {
    vnode_t *v = exec_resolve(path, 256, &argv, &argc);
    if (!v) { free_vec(argv, argc); free_vec(env, envc); return -MOS_ENOENT; }
    process_t *np = kcalloc(1, sizeof *np);
    if (!np) { free_vec(argv, argc); free_vec(env, envc); return -MOS_ENOMEM; }
    np->pid = p->pid;
    np->ppid = p->ppid;
    memcpy(np->cwd, p->cwd, sizeof np->cwd);
    const char *base = strrchr(path, '/');
    strncpy(np->name, base ? base + 1 : path, sizeof np->name - 1);
    strncpy(np->path, path, sizeof np->path - 1);
    np->pml4 = (u64 *)pmm_alloc(1);
    if (!np->pml4) { kfree(np); free_vec(argv, argc); free_vec(env, envc); return -MOS_ENOMEM; }
    memcpy(np->pml4, kernel_pml4, 4096);
    for (int k = 0; k < USER_PML4_SLOTS; k++) np->pml4[USER_PML4_FIRST + k] = 0;
    np->mem_pages = 1;

    char err[128];
    u64 entry = 0, usp = 0, rdi = 0;
    bool ok;
    u32 magic = le32_(v->data);
    if (magic == 0xFEEDFACFu || magic == 0xBEBAFECAu || magic == 0xCAFEBABEu) {
        np->spawn_argv = argv; np->spawn_argc = argc;
        np->spawn_env = env;   np->spawn_envc = envc;
        ok = macho_load(np, v->data, v->size, NULL, &entry, &usp, &rdi, err, sizeof err);
        np->spawn_argv = NULL; np->spawn_env = NULL; np->spawn_argc = np->spawn_envc = 0;
    } else {
        /* programa nativo (ELF): recebe os argumentos numa string só, como no SYS_SPAWN */
        ok = elf_load(np, v->data, v->size, &entry, err, sizeof err);
        u64 lo = USER_STACK_TOP - USER_STACK_SIZE;
        if (ok && !uvm_map(np, lo, USER_STACK_SIZE)) { ok = false; snprintf(err, sizeof err, "sem memória para a pilha"); }
        if (ok) {
            char arg[1024]; usize o = 0;
            for (int i = 1; i < argc && o < sizeof arg - 1; i++)
                o += (usize)snprintf(arg + o, sizeof arg - o, "%s%s", i > 1 ? " " : "", argv[i]);
            arg[o < sizeof arg ? o : sizeof arg - 1] = 0;
            u64 sp = USER_STACK_TOP;
            rdi = 0;
            if (argc > 1) {
                usize n = strlen(arg) + 1;
                sp -= (n + 15) & ~15ULL;
                uvm_copy_out(np, sp, arg, n);
                rdi = sp;
            }
            sp = (sp & ~15ULL) - 8;
            u64 zero = 0;
            uvm_copy_out(np, sp, &zero, 8);
            usp = sp;
        }
    }
    free_vec(argv, argc);
    free_vec(env, envc);
    if (!ok) {
        kprintf("proc: execve %s: %s\n", path, err);
        free_address_space(np);
        kfree(np);
        return -MOS_ENOENT;
    }

    /* ponto sem volta: as outras threads morrem, a memória antiga sai */
    u64 fl = irq_save();
    for (thread_t *o = sched_threads(); o; o = o->next)
        if (o->proc == p && o != t) {
            if (p->tty && p->tty->reader == o) p->tty->reader = NULL;
            o->state = TH_DEAD;
        }
    p->main = t;
    drop_pipe_waiters(p, t);
    compat_gui_proc_exit(p);
    { extern void lazy_exit(process_t *p); lazy_exit(p); }
    /* troca o espaço de endereçamento com np (que está no heap) e libera o antigo por ele. Nada de process_t
     * na pilha: com PROC_MAX_FDS = 256 a struct passa de 64 KiB e um "process_t old = {0}" aqui zerava a
     * memória abaixo da pilha do kernel (blocos do heap com magic 0, kfree inválido e pânico). */
    u64 *old_pml4 = p->pml4;
    void *old_dyld = p->dyld;
    p->pml4 = np->pml4;
    p->dyld = np->dyld;
    p->brk = np->brk; p->brk_start = np->brk_start;
    p->mmap_next = np->mmap_next;
    p->mem_pages = np->mem_pages;
    memcpy(p->name, np->name, sizeof p->name);
    memcpy(p->path, np->path, sizeof p->path);
    __asm__ volatile("mov %0, %%cr3" :: "r"((u64)p->pml4) : "memory");
    np->pml4 = old_pml4;
    np->dyld = old_dyld;
    free_address_space(np);
    kfree(np);
    for (int i = 0; i < PROC_MAX_FDS; i++)                       /* O_CLOEXEC fecha; o resto passa para a imagem nova */
        if (p->fds[i].used && (p->fds[i].flags & MOS_O_CLOEXEC)) fd_close(p, i);
    for (int s = 0; s < 32; s++)                       /* tratadores somem; "ignorar" continua */
        if (p->sig_handler[s] > MOS_SIG_IGN) p->sig_handler[s] = 0;
    p->sig_restorer = 0;
    p->sig_pending = 0;
    t->gs_base = 0;
    __asm__ volatile("wrmsr" :: "c"(0xC0000101u), "a"(0u), "d"(0u));   /* GS de usuário: o dyld novo monta o TLS */
    irq_restore(fl);

    u64 cs = f->cs, ss = f->ss;
    memset(f, 0, sizeof *f);
    f->cs = cs; f->ss = ss;
    f->rflags = 0x202;
    f->rip = entry;
    f->rsp = usp;
    f->rdi = rdi;
    u8 *fx = (u8 *)(((u64)f - 512) & ~15ULL);          /* FPU/SSE limpos, como numa thread nova */
    memset(fx, 0, 512);
    fx[0] = 0x7F; fx[1] = 0x03;
    fx[24] = 0x80; fx[25] = 0x1F;
    kprintf("proc: execve pid %d -> %s (%llu KiB)\n", p->pid, p->name, p->mem_pages * 4);
    return 0;
}

u64 proc_syscall(int_frame_t *f) {
    thread_t *t = thread_current();
    process_t *p = t->proc;
    if (!p || (f->cs & 3) != 3) { f->rax = (u64)-MOS_EINVAL; return (u64)f; }
    u64 a = f->rdi, b = f->rsi, c = f->rdx;
    char path[256];
    i64 r = -MOS_EINVAL;
    switch (f->rax) {
    case SYS_EXIT:
        /* diagnóstico (despejar-threads): quem encerra um helper do VS Code, com código e pilha */
        if (!strncmp(p->name, "Code Helper", 11) && vfs_lookup("/Sistema/Biblioteca/despejar-threads")) {
            kprintf("proc: %s (pid %d) chamou exit(%d), thread %d\n", p->name, p->pid, (int)a, t ? t->id : -1);
            u64 w[64];
            if (uvm_copy_in(p, w, f->rsp & ~7ULL, sizeof w)) {
                char line[200]; usize o = 0; int shown = 0;
                for (int i = 0; i < 64 && shown < 14; i++)
                    if (w[i] >= USER_BASE + 0x100000ULL && w[i] < USER_BASE + 0x0E000000ULL) { o += (usize)snprintf(line + o, sizeof line - o, " %llx", w[i]); shown++; }
                if (o) kprintf("proc:   pilha:%s\n", line);
            }
        }
        finish(p, (int)a);
        return sched_switch(f);
    case SYS_WRITE:
        if (fd_get(p, a) >= 0 && p->fds[a].pipe) {
            if (p->fds[a].kind != FD_PIPE_W) { r = -MOS_EBADF; break; }
            return pipe_op(f, t, p->fds[a].pipe, false, b, c);
        }
        r = sys_write(p, (int)a, b, c);               /* arquivo (inclusive stdio redirecionado) ou console */
        break;
    case SYS_GETPID: r = p->pid; break;
    case SYS_SLEEP:
        dump_threads_maybe(f);
        t->wake_at = timer_ticks() + a;
        t->state = TH_SLEEPING;
        f->rax = 0;
        return sched_switch(f);
    case SYS_YIELD:
        f->rax = 0;
        t->state = TH_READY;
        return sched_switch(f);

    /* ---- futex: espera numa palavra de 32 bits do processo. O libSystem monta pthread_mutex/cond e os semáforos
     * em cima disto; antes eles giravam com SYS_YIELD e cada thread ociosa do Chromium (ThreadPool, V8, libuv)
     * ficava sempre pronta, roubando a CPU de quem tinha trabalho. Conferir o valor e bloquear é atômico aqui:
     * syscalls rodam com interrupções desligadas num núcleo só, então nenhum FUTEX_WAKE cai no meio. */
    case SYS_FUTEX_WAIT: {
        dump_threads_maybe(f);       /* o despejo periódico também sai daqui: um app todo parado em futex não chama SYS_SLEEP */
        u32 cur;
        if ((a & 3) || !uvm_copy_in(p, &cur, a, 4)) { r = -MOS_EFAULT; break; }
        if (cur != (u32)b) { r = -MOS_EAGAIN; break; }
        if (!c) { r = -MOS_ETIMEDOUT; break; }
        t->futex_va = a;
        t->wake_at = c == ~0ULL ? 0 : timer_ticks() + c;    /* o prazo é do escalonador (wake_sleepers) */
        t->wait_pid = -8;
        t->state = TH_WAITING;
        f->rax = 0;                                          /* acordada por FUTEX_WAKE */
        return sched_switch(f);
    }
    case SYS_FUTEX_WAKE: {
        i64 n = 0;
        for (thread_t *w = sched_threads(); w && n < (i64)(b ? b : 1); w = w->next) {
            if (w->state != TH_WAITING || w->wait_pid != -8 || w->proc != p || w->futex_va != a) continue;
            ((int_frame_t *)w->frame)->rax = 0;
            w->futex_va = 0;
            w->wake_at = 0;
            w->state = TH_READY;
            n++;
        }
        r = n;
        break;
    }
    case SYS_TIME: r = (i64)timer_ticks(); break;
    case SYS_BRK:
        if (a == 0) r = (i64)p->brk;
        else if (a >= p->brk_start && a < USER_STACK_TOP - USER_STACK_SIZE - 0x100000 && uvm_map(p, p->brk, a > p->brk ? a - p->brk : 0)) {
            p->brk = a;
            r = (i64)a;
        } else r = -MOS_ENOMEM;
        break;
    case SYS_READFILE: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *v = vfs_lookup(path);
        if (!v) { r = -MOS_ENOENT; break; }
        if (v->dir) { r = -MOS_EISDIR; break; }
        u64 n = MIN((u64)v->size, c);
        r = !n || (v->data && uvm_copy_out(p, b, v->data, n)) ? (i64)n : -MOS_EFAULT;
        break;
    }
    case SYS_FILESIZE: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *v = vfs_lookup(path);
        r = !v ? -MOS_ENOENT : v->dir ? -MOS_EISDIR : (i64)v->size;
        break;
    }
    case SYS_WRITEFILE: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        if (c > (64u << 20)) { r = -MOS_ENOSPC; break; }
        u8 *tmp = c ? kmalloc(c) : NULL;
        if (c && !tmp) { r = -MOS_ENOMEM; break; }
        if (!c || uvm_copy_in(p, tmp, b, c)) r = vfs_write_file(path, tmp ? (void *)tmp : (void *)"", c) ? 0 : -MOS_ENOENT;
        else r = -MOS_EFAULT;
        kfree(tmp);
        break;
    }
    case SYS_SPAWN: {
        char arg[256];
        /* nome sem '/' procura em /Sistema/Programas; com '/' é relativo à pasta atual */
        if (!copy_str_in(p, path, a, sizeof path) || (b && !copy_str_in(p, arg, b, sizeof arg))) { r = -MOS_EFAULT; break; }
        if (strchr(path, '/') && path[0] != '/') { char rel[256]; memcpy(rel, path, sizeof rel); snprintf(path, sizeof path, "%s/%s", p->cwd, rel); }
        r = proc_spawn(path, b ? arg : NULL);
        if (r < 0) r = -MOS_ENOENT;
        break;
    }
    case SYS_SPAWNVEC: {
        mos_spawn_vec_t v;
        if (!copy_str_in(p, path, a, sizeof path) || !uvm_copy_in(p, &v, b, sizeof v)) { r = -MOS_EFAULT; break; }
        if (strchr(path, '/') && path[0] != '/') { char rel[256]; memcpy(rel, path, sizeof rel); snprintf(path, sizeof path, "%s/%s", p->cwd, rel); }
        char **argv = NULL; int argc = 0;
        char **env = NULL; int envc = 0;
        mos_spawn_fa_t acts[MOS_SPAWN_FA_MAX];
        int nacts = 0;
        if (v.argv && !(argv = copy_user_vec(p, v.argv, &argc))) { r = -MOS_EFAULT; break; }
        if (v.envp && !(env = copy_user_vec(p, v.envp, &envc))) { free_vec(argv, argc); r = -MOS_EFAULT; break; }
        if (v.nacts && v.acts) {
            nacts = v.nacts > MOS_SPAWN_FA_MAX ? MOS_SPAWN_FA_MAX : (int)v.nacts;
            if (!uvm_copy_in(p, acts, v.acts, sizeof(mos_spawn_fa_t) * (usize)nacts)) {
                free_vec(argv, argc); free_vec(env, envc); r = -MOS_EFAULT; break;
            }
        }
        if (!exec_resolve(path, sizeof path, &argv, &argc)) { free_vec(argv, argc); free_vec(env, envc); r = -MOS_ENOENT; break; }
        r = proc_spawn_in(path, NULL, p->tty, argv, argc, env, envc, nacts ? acts : NULL, nacts, v.flags);
        free_vec(argv, argc);
        free_vec(env, envc);
        if (r < 0) r = -MOS_ENOENT;
        break;
    }
    case SYS_EXECVE: {
        mos_spawn_vec_t v;
        if (!copy_str_in(p, path, a, sizeof path) || !uvm_copy_in(p, &v, b, sizeof v)) { r = -MOS_EFAULT; break; }
        if (path[0] != '/' && strchr(path, '/')) { char rel[256]; memcpy(rel, path, sizeof rel); snprintf(path, sizeof path, "%s/%s", p->cwd, rel); }
        char **argv = NULL; int argc = 0;
        char **env = NULL; int envc = 0;
        if (v.argv && !(argv = copy_user_vec(p, v.argv, &argc))) { r = -MOS_EFAULT; break; }
        if (v.envp && !(env = copy_user_vec(p, v.envp, &envc))) { free_vec(argv, argc); r = -MOS_EFAULT; break; }
        r = proc_exec(f, t, p, path, argv, argc, env, envc);   /* libera argv e env */
        if (r == 0) return (u64)f;                             /* frame já aponta para a imagem nova */
        break;
    }
    case SYS_WAITPID: {
        int want = (int)(i64)a;
        bool any = false;
        r = 0;
        for (process_t *q = procs; q; q = q->next) {
            if (q->ppid != p->pid || (want > 0 && q->pid != want)) continue;
            any = true;
            if (q->exited) {
                int st = posix_status(q->exit_code);
                if (b) uvm_copy_out(p, b, &st, sizeof st);
                q->ppid = -1;                              /* recolhido: trim_zombies libera depois */
                r = q->pid;
                break;
            }
        }
        if (r > 0) break;
        if (!any) { r = -MOS_ECHILD; break; }
        if (c & MOS_WNOHANG) { r = 0; break; }
        t->wait_pid = want > 0 ? want : -1;
        t->wait_status = b;
        t->wait_posix = true;
        t->state = TH_WAITING;
        return sched_switch(f);
    }
    case SYS_WAIT: {
        process_t *q = find((int)a);
        if (!q || q == p) { r = -MOS_ESRCH; break; }
        if (q->exited) {
            r = q->exit_code;
            if (!q->main) unlink_proc(q);
            break;
        }
        t->wait_pid = q->pid;
        t->state = TH_WAITING;
        return sched_switch(f);        /* finish() põe o código de saída em rax e acorda */
    }
    case SYS_CPUS: r = smp_cpu_count(); break;
    case SYS_KILL: r = proc_kill((int)a) ? 0 : -MOS_ESRCH; break;

    /* ---- arquivos por descritor */
    case SYS_OPEN: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        int fd = -1;
        for (int i = 3; i < PROC_MAX_FDS; i++) if (!p->fds[i].used) { fd = i; break; }
        /* diagnóstico (com despejar-threads): open que falha nos helpers do VS Code vai para o log serial */
        static int open_diag = -1;
        if (open_diag < 0) open_diag = vfs_lookup("/Sistema/Biblioteca/despejar-threads") != NULL;
        bool diag = open_diag && !strncmp(p->name, "Code Helper", 11);
        /* caches em disco do Chromium (Cache_Data, GPUCache, Code Cache): "Unable to create cache" sem outra pista */
        bool cdiag = open_diag && (strstr(path, "Cache") || strstr(path, "cache") ||
                                   (!strncmp(p->name, "Code", 4) && !p->name[4] && strstr(path, ".asar")));   /* AsarURLLoader */
        /* EMFILE vai sempre para o log (raro e sério: o processo principal do VS Code abre dezenas de arquivos) */
        if (fd < 0) { kprintf("open[diag]: %s (pid %d): EMFILE %s\n", p->name, p->pid, path); r = -MOS_EMFILE; break; }
        vnode_t *v = is_devnull(path) ? NULL : vfs_lookup(path);
        bool wr = b & (MOS_O_WRONLY | MOS_O_RDWR);
        if (is_devnull(path)) { /* sem vnode: read/write tratam pelo caminho */ }
        else if (!v) {
            if (!(b & MOS_O_CREAT)) {
                if (diag || cdiag || (open_diag && strstr(path, "workbench"))) kprintf("open[diag]: %s (pid %d): ENOENT %s\n", p->name, p->pid, path);
                r = -MOS_ENOENT;
                break;
            }
            if (!vfs_write_file(path, "", 0)) {
                if (cdiag) kprintf("open[diag]: %s (pid %d): ENOENT ao criar %s (flags %llx)\n", p->name, p->pid, path, b);
                r = -MOS_ENOENT;
                break;
            }
        } else if (v->dir && wr) { if (cdiag) kprintf("open[diag]: %s (pid %d): EISDIR %s\n", p->name, p->pid, path); r = -MOS_EISDIR; break; }
        else if (wr && (b & MOS_O_TRUNC)) vfs_write_file(path, "", 0);
        if (cdiag) kprintf("open[diag]: %s (pid %d): ok %s flags %llx%s\n", p->name, p->pid, path, b, v && v->dir ? " (pasta)" : v ? "" : " (criado)");
        else if (diag && strstr(p->name, "Renderer")) kprintf("open[diag]: %s (pid %d): ok %s%s\n", p->name, p->pid, path, v && v->dir ? " (pasta)" : "");
        /* a página do workbench: quem a abre (e se abre) diz em que ponto a navegação da janela para */
        else if (open_diag && strstr(path, "workbench")) kprintf("open[diag]: %s (pid %d): ok %s\n", p->name, p->pid, path);
        p->fds[fd].used = true;
        p->fds[fd].kind = FD_FILE;
        p->fds[fd].pipe = NULL;
        p->fds[fd].flags = (int)b;
        p->fds[fd].off = 0;
        strncpy(p->fds[fd].path, path, sizeof p->fds[fd].path - 1);
        r = fd;
        break;
    }
    case SYS_CLOSE:
        if (fd_get(p, a) < 0) { r = -MOS_EBADF; break; }
        fd_close(p, (int)a);
        r = 0;
        break;
    case SYS_PIPE: {
        int rfd = -1, wfd = -1;
        for (int i = 3; i < PROC_MAX_FDS; i++) if (!p->fds[i].used) { if (rfd < 0) rfd = i; else { wfd = i; break; } }
        if (wfd < 0) { r = -MOS_EMFILE; break; }
        pipe_t *pp = kcalloc(1, sizeof *pp);
        if (!pp) { r = -MOS_ENOMEM; break; }
        pp->readers = pp->writers = 1;
        int both[2] = {rfd, wfd};
        if (!uvm_copy_out(p, a, both, sizeof both)) { kfree(pp); r = -MOS_EFAULT; break; }
        memset(&p->fds[rfd], 0, sizeof p->fds[rfd]);
        memset(&p->fds[wfd], 0, sizeof p->fds[wfd]);
        p->fds[rfd].used = p->fds[wfd].used = true;
        p->fds[rfd].kind = FD_PIPE_R; p->fds[wfd].kind = FD_PIPE_W;
        p->fds[rfd].pipe = p->fds[wfd].pipe = pp;
        r = 0;
        break;
    }
    case SYS_DUP2:
        if (fd_get(p, a) < 0 || b >= PROC_MAX_FDS) { r = -MOS_EBADF; break; }
        if (a != b) { fd_copy(p, (int)b, p, (int)a); p->fds[b].flags &= ~MOS_O_CLOEXEC; }   /* POSIX: a cópia nasce sem FD_CLOEXEC */
        r = (i64)b;
        break;
    case SYS_FDFLAGS:                                       /* fcntl F_GETFD/F_SETFD: o git espera EOF no pipe de aviso do exec */
        if (fd_get(p, a) < 0) { r = -MOS_EBADF; break; }
        if (b) { if (c) p->fds[a].flags |= MOS_O_CLOEXEC; else p->fds[a].flags &= ~MOS_O_CLOEXEC; }
        r = (p->fds[a].flags & MOS_O_CLOEXEC) ? 1 : 0;
        break;
    case SYS_SPAWNIO: {
        char arg[256];
        int io[3];
        if (!copy_str_in(p, path, a, sizeof path) || (b && !copy_str_in(p, arg, b, sizeof arg)) ||
            !uvm_copy_in(p, io, c, sizeof io)) { r = -MOS_EFAULT; break; }
        if (strchr(path, '/') && path[0] != '/') { char rel[256]; memcpy(rel, path, sizeof rel); snprintf(path, sizeof path, "%s/%s", p->cwd, rel); }
        /* O filho recebe só 0..2 (io[i], ou o fd i do pai quando io[i] < 0), aplicados antes de a thread existir:
         * herdar os demais fds deixaria cópias das pontas de pipe no filho (o `wc` de "seq | wc" segurava a ponta
         * de escrita e nunca via EOF), e redirecionar depois do spawn corria com o filho já rodando em outro núcleo. */
        mos_spawn_fa_t acts[3];
        int nacts = 0;
        for (int i = 0; i < 3; i++) {
            int src = io[i] >= 0 ? io[i] : i;
            if (src >= PROC_MAX_FDS || fd_get(p, (u64)src) < 0) continue;
            memset(&acts[nacts], 0, sizeof acts[nacts]);
            acts[nacts].kind = MOS_SPAWN_FA_DUP2;
            acts[nacts].a = src;
            acts[nacts].b = i;
            nacts++;
        }
        extern int proc_spawn_in(const char *path, const char *arg, tty_t *tty,
                                 char **xargv, int xargc, char **xenv, int xenvc,
                                 mos_spawn_fa_t *acts, int nacts, unsigned spawn_flags);
        r = proc_spawn_in(path, b ? arg : NULL, p->tty, NULL, 0, NULL, 0, acts, nacts, MOS_SPAWN_CLOEXEC_DEFAULT);
        if (r < 0) { r = -MOS_ENOENT; break; }
        break;
    }
    case SYS_READ: {
        if (fd_get(p, a) >= 0 && p->fds[a].pipe) {
            if (p->fds[a].kind != FD_PIPE_R) { r = -MOS_EBADF; break; }
            return pipe_op(f, t, p->fds[a].pipe, true, b, c);
        }
        if (a == 0 && fd_get(p, 0) < 0) {                   /* stdin: entrada da sessão */
            tty_t *tt = p->tty;
            if (!tt) { r = 0; break; }                      /* sem terminal: EOF */
            if (tt->reader && tt->reader != t) { r = -MOS_EINVAL; break; }
            t->frame = (u64)f;                              /* frame atual: o rax da resposta vai nele */
            t->read_buf = b;
            t->read_len = c;
            t->wait_pid = -2;
            t->state = TH_WAITING;
            tt->reader = t;
            tty_feed_reader(tt);                            /* já havia entrada? entrega agora */
            if (t->state == TH_READY) { t->state = TH_RUNNING; return (u64)f; }
            return sched_switch(f);
        }
        if (fd_get(p, a) < 0 || (p->fds[a].flags & MOS_O_WRONLY)) { r = -MOS_EBADF; break; }
        if (is_devnull(p->fds[a].path)) { r = 0; break; }   /* /dev/null: sempre fim de arquivo */
        vnode_t *v = vfs_lookup(p->fds[a].path);
        if (!v) { r = -MOS_ENOENT; break; }
        if (v->dir) { r = -MOS_EISDIR; break; }
        u64 off = p->fds[a].off, n = off < v->size ? MIN(c, v->size - off) : 0;
        if (n && (!v->data || !uvm_copy_out(p, b, v->data + off, n))) { r = -MOS_EFAULT; break; }
        p->fds[a].off += n;
        r = (i64)n;
        break;
    }
    /* pread/pwrite: deslocamento explícito, posição do fd intocada. No libSystem eram lseek+read+lseek, o que quebrava
     * com threads lendo o mesmo fd (o pool do libuv lendo o node_modules.asar enquanto o AsarURLLoader lia também) */
    case SYS_PREAD: {
        if (fd_get(p, a) < 0 || p->fds[a].pipe) { r = fd_get(p, a) < 0 ? -MOS_EBADF : -MOS_ESPIPE; break; }
        if (p->fds[a].flags & MOS_O_WRONLY) { r = -MOS_EBADF; break; }
        if (is_devnull(p->fds[a].path)) { r = 0; break; }
        vnode_t *v = vfs_lookup(p->fds[a].path);
        if (!v) { r = -MOS_ENOENT; break; }
        if (v->dir) { r = -MOS_EISDIR; break; }
        u64 off = f->r10, n = off < v->size ? MIN(c, v->size - off) : 0;
        if (n && (!v->data || !uvm_copy_out(p, b, v->data + off, n))) { r = -MOS_EFAULT; break; }
        r = (i64)n;
        break;
    }
    case SYS_PWRITE: {
        if (fd_get(p, a) < 0 || p->fds[a].pipe) { r = fd_get(p, a) < 0 ? -MOS_EBADF : -MOS_ESPIPE; break; }
        if (!(p->fds[a].flags & (MOS_O_WRONLY | MOS_O_RDWR))) { r = -MOS_EBADF; break; }
        if (is_devnull(p->fds[a].path)) { r = (i64)c; break; }
        u8 *tmp = c ? kmalloc(c) : NULL;
        if (c && !tmp) { r = -MOS_ENOMEM; break; }
        if (c && !uvm_copy_in(p, tmp, b, c)) { kfree(tmp); r = -MOS_EFAULT; break; }
        r = file_pwrite(p->fds[a].path, f->r10, tmp, c);
        kfree(tmp);
        break;
    }
    case SYS_LSEEK: {
        if (fd_get(p, a) < 0) { r = -MOS_EBADF; break; }
        vnode_t *v = vfs_lookup(p->fds[a].path);
        i64 base = c == MOS_SEEK_SET ? 0 : c == MOS_SEEK_CUR ? (i64)p->fds[a].off : c == MOS_SEEK_END ? (i64)(v ? v->size : 0) : -1;
        i64 pos = base + (i64)b;
        if (base < 0 || pos < 0) { r = -MOS_EINVAL; break; }
        p->fds[a].off = (u64)pos;
        r = pos;
        break;
    }
    case SYS_FSTAT:
    case SYS_STAT: {
        vnode_t *v;
        /* pipe: is_dir = 2 (ponta de leitura) ou 3 (escrita); size = bytes para ler (ou espaço livre);
         * mtime = 1 se a outra ponta já fechou. É o que kqueue/poll do libSystem usam para saber se há dados. */
        if (f->rax == SYS_FSTAT && fd_get(p, a) >= 0 && p->fds[a].pipe) {
            pipe_t *pp = p->fds[a].pipe;
            bool rd = p->fds[a].kind == FD_PIPE_R;
            mos_stat_t st = {0};
            st.is_dir = rd ? 2 : 3;
            st.size = rd ? pp->count : sizeof pp->buf - pp->count;
            st.mtime = rd ? (pp->writers == 0) : (pp->readers == 0);
            r = uvm_copy_out(p, b, &st, sizeof st) ? 0 : -MOS_EFAULT;
            break;
        }
        const char *sp;
        if (f->rax == SYS_FSTAT) { if (fd_get(p, a) < 0) { r = -MOS_EBADF; break; } sp = p->fds[a].path; }
        else { if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; } sp = path; }
        if (is_devnull(sp)) {                               /* is_dir 4 = dispositivo de caracteres (libSystem) */
            mos_stat_t st = {0, 4, unix_now()};
            r = uvm_copy_out(p, b, &st, sizeof st) ? 0 : -MOS_EFAULT;
            break;
        }
        v = vfs_lookup(sp);
        if (!v) { r = -MOS_ENOENT; break; }
        mos_stat_t st;
        fill_stat(v, &st);
        r = uvm_copy_out(p, b, &st, sizeof st) ? 0 : -MOS_EFAULT;
        break;
    }
    case SYS_READDIR: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *d = vfs_lookup(path);
        if (!d) { r = -MOS_ENOENT; break; }
        if (!d->dir) { r = -MOS_EINVAL; break; }
        vnode_t *e = d->child;
        for (u64 i = 0; e && i < b; i++) e = e->next;
        if (!e) { r = 0; break; }
        usize n = MIN(strlen(e->name) + 1, (usize)f->r10);
        if (!n) { r = -MOS_EINVAL; break; }
        char *nm = kmalloc(n);
        if (!nm) { r = -MOS_ENOMEM; break; }
        memcpy(nm, e->name, n - 1);
        nm[n - 1] = 0;
        r = uvm_copy_out(p, c, nm, n) ? 1 : -MOS_EFAULT;
        kfree(nm);
        break;
    }
    case SYS_MKDIR:
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        r = vfs_mkdir(path) ? 0 : -MOS_ENOENT;
        if (r && vfs_lookup("/Sistema/Biblioteca/despejar-threads") && (strstr(path, "Cache") || strstr(path, "cache")))
            kprintf("mkdir[diag]: %s (pid %d): falhou %s%s\n", p->name, p->pid, path, vfs_lookup(path) ? " (já existe)" : "");
        break;
    case SYS_UNLINK: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *v = vfs_lookup(path);
        r = !v ? -MOS_ENOENT : vfs_delete(v) ? 0 : -MOS_EINVAL;
        break;
    }
    case SYS_RENAME: {
        char to[256];
        if (!copy_path(p, path, a) || !copy_path(p, to, b)) { r = -MOS_EFAULT; break; }
        vnode_t *v = vfs_lookup(path);
        char *slash = strrchr(to, '/');
        if (!v) { r = -MOS_ENOENT; break; }
        if (!slash || !slash[1]) { r = -MOS_EINVAL; break; }
        *slash = 0;
        vnode_t *np = vfs_lookup(to[0] ? to : "/");
        if (!np || !np->dir) { r = -MOS_ENOENT; break; }
        vnode_t *old = vfs_child(np, slash + 1);
        if (old == v) { r = 0; break; }
        if (old) {                                          /* POSIX: o destino é substituído (o git grava X.lock e renomeia) */
            if (old->dir && !v->dir) { r = -MOS_EISDIR; break; }
            if (!old->dir && v->dir) { r = -MOS_ENOTDIR; break; }
            if (old->dir && old->child) { r = -MOS_ENOTEMPTY; break; }
            if (!vfs_delete(old)) { r = -MOS_EINVAL; break; }
        }
        r = vfs_move(v, np, slash + 1) ? 0 : -MOS_EINVAL;
        break;
    }

    /* ---- memória e tempo */
    case SYS_MMAP: {
        /* diagnóstico do porte do VS Code: toda recusa de memória aparece no log serial */
#define MMAP_NOMEM_LOG() kprintf("mmap ENOMEM: pid %d len 0x%llx b %llu c 0x%llx next 0x%llx\n", p->pid, \
        (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)p->mmap_next)
        /* b = 0: memória anônima zerada; b = 1: só reserva endereços, sem páginas (mmap PROT_NONE: o
         * PartitionAlloc do Chromium reserva gigabytes assim); b = 2: aloca as páginas de [c, c + a)
         * dentro da área já reservada (mprotect para leitura/escrita), mantendo as que já existem */
        u64 len = (a + 0xFFF) & ~0xFFFULL;
        if (!p->mmap_next) p->mmap_next = USER_MMAP_BASE;
        /* b = 3: memória sob demanda (kernel/mm/lazy.c), como o mmap anônimo do macOS. c = 0: faixa nova (só
         * endereços; cada página vira RAM ao ser tocada); c != 0: [c, c + a) de uma reserva passa a ser sob
         * demanda (mprotect / MAP_FIXED), mantendo as páginas que já existem */
        if (b == 3) {
            extern bool lazy_add(process_t *p, u64 va, u64 len);
            if (c) {
                u64 at = c & ~0xFFFULL, end = (c + a + 0xFFF) & ~0xFFFULL;
                if (!a || at < USER_MMAP_BASE || end > p->mmap_next || end < at) { r = -MOS_EINVAL; break; }
                r = lazy_add(p, at, end - at) ? 0 : -MOS_ENOMEM;
                if (r) MMAP_NOMEM_LOG();
                break;
            }
            /* faixas grandes ficam alinhadas à maior potência de 2 que cabe no tamanho (até 4 GiB): o alocador do
             * JavaScriptCore (Bun) pede N bytes e exige o endereço alinhado a N (RELEASE_ASSERT) */
            u64 al = 4096;
            if (len >= (2ull << 20)) { al = 1ull << (63 - __builtin_clzll(len)); if (al > (4ull << 30)) al = 4ull << 30; }
            u64 at = (p->mmap_next + al - 1) & ~(al - 1);
            if (!len || len > (64ull << 30) || at + len > USER_END) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            if (!lazy_add(p, at, len)) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            p->mmap_next = at;
            p->mmap_next += len + 4096;
            r = (i64)at;
            break;
        }
        if (b == 2) {
            u64 at = c & ~0xFFFULL, end = (c + a + 0xFFF) & ~0xFFFULL;
            if (!a || at < USER_MMAP_BASE || end > p->mmap_next || end < at) { r = -MOS_EINVAL; break; }
            r = uvm_map(p, at, end - at) ? 0 : -MOS_ENOMEM;
            if (r) MMAP_NOMEM_LOG();
            break;
        }
        u64 max = b == 1 ? (1792ull << 30) : (1ull << 32);
        if (!len || len > max || p->mmap_next + len > USER_END) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
        u64 at = p->mmap_next;
        if (b != 1 && !uvm_map(p, at, len)) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
        p->mmap_next += len + 4096;                         /* página de guarda entre regiões */
        r = (i64)at;
        break;
    }
    case SYS_MUNMAP: {
        if (a < USER_MMAP_BASE || a & 0xFFF || a + b > USER_END) { r = -MOS_EINVAL; break; }
        { extern void lazy_remove(process_t *p, u64 va, u64 len); lazy_remove(p, a, b); }
        for (u64 va = a; va < a + b; ) {
            u64 *e = walk(p, va, false);
            /* sem tabela de páginas: os 2 MiB inteiros estão vazios (as reservas do V8 têm terabytes) */
            if (!e) { va = (va + 0x200000) & ~0x1FFFFFULL; continue; }
            if (*e & PTE_P) {
                page_put(*e & 0x000FFFFFFFFFF000ULL);
                p->mem_pages--;
                __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
            }
            *e = 0;
            va += 4096;
        }
        /* liberar o topo devolve os endereços: o V8 e o PartitionAlloc reservam, aparam e soltam regiões
         * de gigabytes em laço, e sem isso o espaço de usuário se esgotava em poucos segundos */
        {
            u64 end = (a + b + 0xFFF) & ~0xFFFULL;
            if (b && a < p->mmap_next && end + 4096 >= p->mmap_next) p->mmap_next = a;
        }
        r = 0;
        break;
    }
    case SYS_UNIXTIME: r = unix_now(); break;

    /* ---- threads do mesmo processo */
    case SYS_THREAD_CREATE: {
        if (a < USER_BASE || a >= USER_END || b < USER_BASE || b > USER_END) { r = -MOS_EINVAL; break; }
        thread_t *nt = uthread_create(p, a, b, c);
        if (!nt) { r = -MOS_ENOMEM; break; }
        p->nthreads++;
        r = nt->id;
        break;
    }
    case SYS_THREAD_EXIT:
        if (t == p->main) { finish(p, (int)a); return sched_switch(f); }
        t->exit_code = (int)a;
        for (int i = 15; i > 0; i--) p->texit[i] = p->texit[i - 1];
        p->texit[0].tid = t->id; p->texit[0].code = (int)a;
        for (thread_t *w = sched_threads(); w; w = w->next)
            if (w->state == TH_WAITING && w->proc == p && w->join_tid == t->id) {
                ((int_frame_t *)w->frame)->rax = (u64)(i64)(int)a;
                w->state = TH_READY;
                w->join_tid = 0;
            }
        t->state = TH_DEAD;
        return sched_switch(f);
    case SYS_THREAD_JOIN: {
        thread_t *j = NULL;
        for (thread_t *w = sched_threads(); w; w = w->next) if (w->id == (int)a && w->proc == p) j = w;
        if (j == t) { r = -MOS_EINVAL; break; }
        if (!j || j->state == TH_DEAD) {
            r = -MOS_ESRCH;
            for (int i = 0; i < 16; i++) if (p->texit[i].tid == (int)a) { r = p->texit[i].code; break; }
            break;
        }
        t->join_tid = (int)a;
        t->wait_pid = -5;
        t->state = TH_WAITING;
        return sched_switch(f);
    }
    case SYS_GETTID: r = t->id; break;

    /* ---- fork: filho com cópia sob demanda da memória */
    case SYS_FORK: {
        if (!pref_ready()) { r = -MOS_ENOMEM; break; }
        process_t *q = kcalloc(1, sizeof *q);
        u64 *pml4 = q ? (u64 *)pmm_alloc(1) : NULL;
        if (!pml4) { kfree(q); r = -MOS_ENOMEM; break; }
        memcpy(pml4, kernel_pml4, 4096);
        for (int k = 0; k < USER_PML4_SLOTS; k++) pml4[USER_PML4_FIRST + k] = 0;
        q->pml4 = pml4;
        q->mem_pages = 1;
        bool fork_ok = true;
        for (int k = 0; k < USER_PML4_SLOTS && fork_ok; k++) {
            int ui = USER_PML4_FIRST + k;
            if (!(p->pml4[ui] & PTE_P)) continue;
            u64 nt = pmm_alloc(1);
            if (!nt) { fork_ok = false; break; }
            memset((void *)nt, 0, 4096);
            pml4[ui] = nt | PTE_P | PTE_RW | PTE_U;
            fork_ok = cow_copy_level(q, (u64 *)(p->pml4[ui] & PTE_ADDR), (u64 *)nt, 3);
        }
        if (!fork_ok) { free_address_space(q); kfree(q); r = -MOS_ENOMEM; break; }
        __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");   /* o pai agora é só leitura */
        q->pid = next_pid++;
        q->ppid = p->pid;
        memcpy(q->name, p->name, sizeof q->name);
        memcpy(q->path, p->path, sizeof q->path);
        memcpy(q->cwd, p->cwd, sizeof q->cwd);
        q->brk = p->brk; q->brk_start = p->brk_start; q->mmap_next = p->mmap_next;
        q->dyld = macho_dyld_clone(p->dyld);            /* imagens do dlopen: mesma memória, cópia do grafo */
        memcpy(q->sig_handler, p->sig_handler, sizeof q->sig_handler);
        q->sig_restorer = p->sig_restorer;
        q->start_ms = timer_ticks();
        q->tty = p->tty;
        if (q->tty) q->tty->refs++;
        machipc_spawn(p, q);                            /* como no macOS: o filho só herda as portas especiais */
        { extern void lazy_fork(process_t *from, process_t *to); lazy_fork(p, q); }   /* faixas sob demanda (mm/lazy.c) */
        for (int i = 0; i < PROC_MAX_FDS; i++) if (p->fds[i].used) fd_copy(q, i, p, i);
        f->rax = 0;                                     /* o filho vê 0 */
        thread_t *ct = uthread_clone(q, f);
        if (!ct) { for (int i = 0; i < PROC_MAX_FDS; i++) fd_close(q, i); free_address_space(q); if (q->tty) q->tty->refs--; kfree(q); r = -MOS_ENOMEM; break; }
        q->main = ct;
        q->nthreads = 1;
        q->next = procs;
        procs = q;
        kprintf("proc: fork de %s (pid %d -> %d, %llu KiB compartilhados)\n", p->name, p->pid, q->pid, q->mem_pages * 4);
        r = q->pid;                                     /* o pai vê o pid do filho */
        break;
    }

    /* ---- sinais */
    case SYS_SIGACTION:
        if (a == 0 || a >= MOS_NSIG || a == MOS_SIGKILL) { r = -MOS_EINVAL; break; }
        if (b > MOS_SIG_IGN && (b < USER_BASE || b >= USER_END)) { r = -MOS_EFAULT; break; }
        r = (i64)p->sig_handler[a];
        p->sig_handler[a] = b;
        if (c) p->sig_restorer = c;
        break;
    case SYS_SIGRETURN: return sig_return(f);
    case SYS_SIGKILL: r = proc_signal((int)a, (int)b) ? 0 : -MOS_ESRCH; break;
    case SYS_DYLD: r = macho_dyld_syscall(p, a, b, c, f->r10); break;   /* dlopen/dlsym (kernel/compat/macho.c) */
    case SYS_NET: { extern i64 net_user_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c); r = net_user_syscall(p, a, b, c, f->r10); break; }   /* TCP/DNS (kernel/net/usersock.c) */

    /* ---- processos e pasta atual */
    case SYS_WAITANY: {
        bool any = false;
        for (process_t *q = procs; q; q = q->next) {
            if (q->ppid != p->pid) continue;
            any = true;
            if (q->exited) {                                /* já terminou: devolve na hora */
                int code = q->exit_code;
                if (a) uvm_copy_out(p, a, &code, sizeof code);
                q->ppid = -1;
                r = q->pid;
                break;
            }
        }
        if (r > 0) break;
        if (!any) { r = -MOS_ESRCH; break; }
        t->wait_pid = -1;
        t->wait_status = a;
        t->state = TH_WAITING;
        return sched_switch(f);
    }
    case SYS_CHDIR: {
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *d = vfs_lookup(path);
        if (!d) { r = -MOS_ENOENT; break; }
        if (!d->dir) { r = -MOS_EINVAL; break; }
        vfs_path(d, p->cwd, sizeof p->cwd);                 /* normaliza "..", "." */
        r = 0;
        break;
    }
    case SYS_GETCWD: {
        usize n = strlen(p->cwd) + 1;
        if (n > b) { r = -MOS_EINVAL; break; }
        r = uvm_copy_out(p, a, p->cwd, n) ? (i64)n - 1 : -MOS_EFAULT;
        break;
    }
    case SYS_FDPATH: {                                      /* fcntl(F_GETPATH) e fchdir no libSystem */
        if (fd_get(p, a) < 0 || p->fds[a].kind != FD_FILE) { r = -MOS_EBADF; break; }
        usize n = strlen(p->fds[a].path) + 1;
        if (n > c) { r = -MOS_EINVAL; break; }
        r = uvm_copy_out(p, b, p->fds[a].path, n) ? (i64)n - 1 : -MOS_EFAULT;
        break;
    }
    case SYS_PROCLIST: {
        process_t *q = procs;
        for (u64 i = 0; q; q = q->next) if (!q->exited && i++ == a) break;
        if (!q) { r = 0; break; }
        mos_procinfo_t pi = {0};
        pi.pid = q->pid;
        pi.ppid = q->ppid;
        pi.state = q->main ? q->main->state : TH_DEAD;
        pi.mem_kb = q->mem_pages * 4;
        pi.cpu_ms = q->main ? q->main->cpu_ms : 0;
        memcpy(pi.name, q->name, sizeof pi.name);
        r = uvm_copy_out(p, b, &pi, sizeof pi) ? 1 : -MOS_EFAULT;
        break;
    }
    default:
        if (f->rax >= SYS_MACH_FIRST && f->rax <= SYS_MACH_LAST) return machipc_syscall(f);   /* pode bloquear */
        if (f->rax >= SYS_GUI_FIRST && f->rax <= SYS_GUI_LAST) r = compat_gui_syscall(p, f->rax, a, b, c, f->r10, f->r8);
        else r = -MOS_EINVAL;
        break;
    }
    f->rax = (u64)r;
    return proc_signal_check(f);                      /* sinais pendentes saem na volta ao ring 3 */
}

__attribute__((weak)) int smp_cpu_count(void) { return 1; }

/* ------------------------------------------------------------ autoteste do shell
 * Uma thread do kernel "digita" no sh por uma sessão, como o Terminal faria, e confere a saída. */
static int sh_test_pid;

static void sh_test(void *arg) {
    (void)arg;
    static const char cmds[] =
        "echo ola do shell\n"
        "cd /Sistema/Programas\n"
        "pwd\n"
        "ls\n"
        "filho via-shell\n"
        "echo gravado pelo sh > /tmp/sh.txt\n"
        "cat /tmp/sh.txt\n"
        "ps\n"
        "naoexiste\n"
        "seq 5000 | wc\n"
        "spin 0\n";                                     /* nunca termina: Ctrl+C abaixo */
    static const char after[] = "echo depois do ctrl-c\nexit 7\n";
    /* Orientado a eventos, não a tempos fixos: sob QEMU sem aceleração o boot é pesado (compositor, bench,
     * DHCP) e o sh pode levar bem mais que alguns segundos para chegar a cada etapa. */
    char *out = kcalloc(1, 8192);
    if (!out) return;
    usize n = 0;
    u64 t0 = timer_ticks();
#define SH_PUMP() do { n += proc_tty_read(sh_test_pid, out + n, 8191 - n); out[n] = 0; } while (0)
    while (timer_ticks() - t0 < 20000 && !strstr(out, "$ ")) { SH_PUMP(); hlt(); }          /* prompt */
    proc_tty_write(sh_test_pid, cmds, sizeof cmds - 1);
    while (timer_ticks() - t0 < 60000 && !strstr(out, "23893 bytes")) { SH_PUMP(); hlt(); } /* seq | wc terminou */
    int killed = 0;
    for (u64 last = 0; timer_ticks() - t0 < 60000 && !killed; hlt()) {   /* Ctrl+C assim que o spin estiver vivo */
        SH_PUMP();
        if (timer_ticks() - last >= 250) { last = timer_ticks(); killed = proc_tty_interrupt_fg(sh_test_pid); }
    }
    kprintf("teste-sh: Ctrl+C encerrou %d processo(s)\n", killed);
    proc_tty_write(sh_test_pid, after, sizeof after - 1);
    while (timer_ticks() - t0 < 90000) {
        SH_PUMP();
        if (!proc_alive(sh_test_pid)) { SH_PUMP(); break; }
        hlt();
    }
#undef SH_PUMP
    kprintf("---- saída da sessão do sh ----\n%s\n-------------------------------\n", out);
    static const char *want[] = {"ola do shell", "/Sistema/Programas $", "teste", "filho: pid", "via-shell",
                                 "gravado pelo sh", "ESTADO", "msh", "comando não encontrado",
                                 "5000 linhas, 5000 palavras, 23893 bytes", "^C", "[spin terminou com código -2]",
                                 "depois do ctrl-c"};
    int ok = 0;
    for (u32 i = 0; i < ARRAY_LEN(want); i++) {
        if (strstr(out, want[i])) ok++;
        else kprintf("teste-sh: faltou \"%s\"\n", want[i]);
    }
    int code = proc_exit_code(sh_test_pid);
    kprintf("teste-sh: %s (%d/%u trechos, código de saída %d)\n",
            ok == (int)ARRAY_LEN(want) && code == 7 ? "OK" : "FALHOU", ok, (u32)ARRAY_LEN(want), code);
    kfree(out);
}

/* ------------------------------------------------------------ inicialização */
void proc_init(void) {
    sched_init();
    kernel_pml4 = (u64 *)read_cr3();
    vnode_t *t = vfs_lookup("/Sistema/Programas/teste");
    int n = 0;
    vnode_t *dir = vfs_lookup("/Sistema/Programas");
    if (dir) for (vnode_t *c = dir->child; c; c = c->next) n++;
    kprintf("proc: %d programa(s) em /Sistema/Programas\n", n);
    /* init (papel do launchd): sobe os serviços de /Sistema/Biblioteca/LaunchDaemons */
    if (vfs_lookup("/Sistema/Programas/init")) proc_spawn("/Sistema/Programas/init", NULL);
    /* autoteste: só quando a imagem foi gerada com os testes (build.ps1 -Tests) */
    if (t && vfs_lookup("/Sistema/Programas/.autoteste")) {
        extern void account_selftest(void);
        account_selftest();
        proc_spawn("/Sistema/Programas/teste", "autoteste");
        sh_test_pid = proc_spawn_tty("/Sistema/Programas/sh", NULL);
        if (sh_test_pid > 0) kthread_create("teste-sh", sh_test, NULL);
    }
}

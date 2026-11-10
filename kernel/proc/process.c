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
#include "smp_perf.h"
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
bool proc_signal_info(int pid, int sig, int code, int spid, int status);

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

static void vm_lock_mask(process_t *p, unsigned m) {
    int me = this_cpu(), i;
    for (i = 0; i < 16; i++) if (m & (1u << i)) {
        if (p->vmreg[i].depth && p->vmreg[i].owner == me) { p->vmreg[i].depth++; continue; }
        p->vmreg[i].fl = spin_lock_irqsave((spinlock_t *)&p->vmreg[i].lk);
        p->vmreg[i].owner = me;
        p->vmreg[i].depth = 1;
    }
}
static void vm_unlock_mask(process_t *p, unsigned m) {
    int i;
    for (i = 15; i >= 0; i--) if (m & (1u << i)) {
        if (--p->vmreg[i].depth) continue;
        p->vmreg[i].owner = 0;
        spin_unlock_irqrestore((spinlock_t *)&p->vmreg[i].lk, p->vmreg[i].fl);
    }
}
static void vm_lock_range(process_t *p, u64 a, u64 len) { if (p && len) vm_lock_mask(p, smp_stripes(a, len)); }
static void vm_unlock_range(process_t *p, u64 a, u64 len) { if (p && len) vm_unlock_mask(p, smp_stripes(a, len)); }

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
    bool ok = true;
    if (end < va || va < USER_BASE || end > USER_END) return false;
    vm_lock_range(p, va, len);
    for (u64 a = va & ~0xFFFULL; a < end; a += 4096) {
        u64 *e = walk(p, a, true);
        if (!e) { ok = false; break; }
        if (*e & PTE_P) continue;
        u64 pg = pmm_alloc(1);
        if (!pg) { ok = false; break; }
        memset((void *)pg, 0, 4096);
        *e = pg | PTE_P | PTE_RW | PTE_U;
        p->mem_pages++;
    }
    vm_unlock_range(p, va, len);
    return ok;
}

/* ------------------------------------------------------------ cópia na escrita (fork)
 * Páginas compartilhadas entre pai e filho ficam somente leitura com o bit COW (bit 9, livre para o
 * SO). A primeira escrita (falta de página) copia a página se ela ainda tiver mais de um dono.
 * pref[] conta os donos de cada página física (0 = um dono só). */
#define PTE_COW (1ULL << 9)
#define PTE_JWP (1ULL << 10)   /* JIT arm64: página de código traduzido, sem escrita (uvm_jit_wprotect) */
#define PTE_SHARED (1ULL << 11) /* MAP_SHARED: a mesma página física nos dois lados, sem COW */
#define PTE_NX (1ULL << 63)     /* só vale com EFER.NXE (mm/memory.c); o mapa do kernel não usa este bit */
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

/* As contagens são atômicas (CAS): com threads de usuário em vários núcleos, dois processos podem largar ou
 * copiar a mesma página COW ao mesmo tempo. 0 e 1 querem dizer "um dono só". */
static u16 page_refs(u64 phys) {
    u64 pn = phys >> 12;
    u16 v = pref && pn < pref_n ? __atomic_load_n(&pref[pn], __ATOMIC_ACQUIRE) : 0;
    return v ? v : 1;
}

/* mais um dono para a página (fork, imagem compartilhada, memória Mach) */
static void page_ref_inc(u64 phys) {
    u64 pn = phys >> 12;
    if (!pref || pn >= pref_n) return;
    u16 v = __atomic_load_n(&pref[pn], __ATOMIC_RELAXED), nv;
    do {
        nv = (u16)((v ? v : 1) + 1);
        if (!nv) return;                                /* saturou (65535 donos): a página fica para sempre */
    } while (!__atomic_compare_exchange_n(&pref[pn], &v, nv, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
}

/* larga uma referência à página; libera quando era a última */
static void page_put(u64 phys) {
    u64 pn = phys >> 12;
    if (pref && pn < pref_n) {
        u16 v = __atomic_load_n(&pref[pn], __ATOMIC_RELAXED);
        for (;;) {
            if (v == 0xFFFF) return;                    /* saturada: nunca é liberada */
            u16 nv = v > 1 ? (u16)(v - 1) : 0;
            if (__atomic_compare_exchange_n(&pref[pn], &v, nv, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
                if (v > 1) return;                      /* ainda tem outro dono */
                break;
            }
        }
    }
    pmm_free(phys, 1);
}

/* TLB: a CPU atual (se estiver neste espaço) e as outras CPUs com o mesmo CR3. smp_tlb_shootdown fica em
 * kernel/proc/sched.c (sessão SMP); esta definição fraca vale enquanto só o BSP roda threads de usuário. */
__attribute__((weak)) void smp_tlb_shootdown(u64 cr3) { (void)cr3; }
static void vm_flush(process_t *p, u64 va) {
    u64 cur = read_cr3() & ~0xFFFULL & ~(1ULL << 63);
    if (cur == ((u64)p->pml4 & ~0xFFFULL)) __asm__ volatile("invlpg (%0)" :: "r"(va & ~0xFFFULL) : "memory");
    smp_tlb_shootdown((u64)p->pml4);
}

static bool cow_break(process_t *p, u64 *e, u64 va) {
    bool ok = false;
    u64 phys, nx, np;
    vm_lock_range(p, va, 4096);
    phys = *e & PTE_ADDR;
    nx = *e & PTE_NX;
    if (page_refs(phys) > 1) {
        np = pmm_alloc(1);
        if (np) {
            memcpy((void *)np, (void *)phys, 4096);
            *e = np | PTE_P | PTE_RW | PTE_U | nx;
            vm_flush(p, va);
            page_put(phys);
            ok = true;
        }
    } else {
        *e = (*e | PTE_RW) & ~PTE_COW;
        vm_flush(p, va);
        ok = true;
    }
    vm_unlock_range(p, va, 4096);
    return ok;
}

/* fork de página PTE_JWP: a cópia fica privada, ainda sem escrita e com JWP. A falta segue para o
 * runtime (SIGBUS), que invalida só as traduções deste processo. O outro lado permanece protegido. */
static bool cow_split_jwp(process_t *p, u64 *e, u64 va) {
    u64 phys = *e & PTE_ADDR;
    u64 nx = *e & PTE_NX;
    if (page_refs(phys) > 1) {
        u64 np = pmm_alloc(1);
        if (!np) return false;
        memcpy((void *)np, (void *)phys, 4096);
        *e = np | PTE_P | PTE_U | PTE_JWP | nx;
        vm_flush(p, va);
        page_put(phys);
        return true;
    }
    *e = (*e | PTE_JWP) & ~(PTE_RW | PTE_COW);
    vm_flush(p, va);
    return true;
}

/* memória Mach compartilhada (machipc.c): a mesma página física em vários espaços, contada em pref[]
 * como as do COW; cada mapeamento e cada objeto de memória é um dono */
void uvm_page_get(u64 phys) {
    if (!pref_ready()) return;
    page_ref_inc(phys);
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
    vm_flush(p, va);
    return true;
}

/* Solta [a, a+len) sem devolver o endereço ao cursor (mmap_next). mmap MAP_FIXED e
 * mach_vm_map com VM_FLAGS_OVERWRITE substituem a faixa: o MAP_SHARED grava de volta
 * antes (lazy_sync), a faixa lazy é furada e as PTEs saem. A ordem é a do munmap,
 * para o writeback ainda enxergar a página. */
void mm_release_span(process_t *p, u64 a, u64 len) {
    extern void lazy_sync(process_t *p, u64 va, u64 len, int only_last);
    extern void lazy_remove(process_t *p, u64 va, u64 len);
    if (!p || !len || a + len < a) return;
    lazy_sync(p, a, len, 0);
    lazy_remove(p, a, len);
    u64 gone[64];
    int ng = 0;
    for (u64 va = a; va < a + len; ) {
        u64 *e = walk(p, va, false);
        if (!e) { va = (va + 0x200000) & ~0x1FFFFFULL; continue; }
        if (*e & PTE_P) {
            gone[ng++] = *e & 0x000FFFFFFFFFF000ULL;
            p->mem_pages--;
            __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
        }
        *e = 0;
        va += 4096;
        if (ng == 64 || (ng && va >= a + len)) {
            smp_tlb_shootdown((u64)p->pml4);
            for (int k = 0; k < ng; k++) page_put(gone[k]);
            ng = 0;
        }
    }
    if (ng) { smp_tlb_shootdown((u64)p->pml4); for (int k = 0; k < ng; k++) page_put(gone[k]); }
}

/* 1 se alguma página de [a, a+len) tem PTE presente ou faixa lazy. */
int mm_span_taken(process_t *p, u64 a, u64 len) {
    extern bool lazy_overlaps(process_t *p, u64 va, u64 len);
    if (!p || !len || a + len < a) return 0;
    if (lazy_overlaps(p, a, len)) return 1;
    for (u64 va = a; va < a + len; ) {
        u64 *e = walk(p, va, false);
        if (!e) { va = (va + 0x200000) & ~0x1FFFFFULL; continue; }
        if (*e & PTE_P) return 1;
        va += 4096;
    }
    return 0;
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
    vm_flush(p, va);
    *phys = ph;
    return true;
}
bool uvm_map_cow(process_t *p, u64 va, u64 phys) {
    bool ok = false;
    if (!pref_ready()) return false;
    vm_lock_range(p, va, 4096);
    u64 *e = walk(p, va & ~0xFFFULL, true);
    if (e) {
        if (*e & PTE_P) { page_put(*e & PTE_ADDR); p->mem_pages--; }
        uvm_page_get(phys);
        *e = (phys & PTE_ADDR) | PTE_P | PTE_U | PTE_COW;
        p->mem_pages++;
        vm_flush(p, va);
        ok = true;
    }
    vm_unlock_range(p, va, 4096);
    return ok;
}
/* página nova (um dono só, já preenchida) em va, se lá ainda não houver página: false = já havia (outra thread
 * resolveu a mesma falta) ou faltou memória para as tabelas; o chamador então devolve a página. kernel/mm/lazy.c */
bool uvm_install(process_t *p, u64 va, u64 phys) {
    bool ok = false;
    vm_lock_range(p, va, 4096);
    u64 *e = walk(p, va & ~0xFFFULL, true);
    if (e && !(*e & PTE_P)) {
        *e = (phys & PTE_ADDR) | PTE_P | PTE_RW | PTE_U;
        p->mem_pages++;
        vm_flush(p, va);
        ok = true;
    }
    vm_unlock_range(p, va, 4096);
    return ok;
}
u16 uvm_page_refs(u64 phys) { return page_refs(phys); }

/* MAP_SHARED: a página do cache ganha mais um dono e entra com PTE_SHARED (o fork não a transforma em COW).
 * prot: bit 1 leitura, bit 2 escrita, bit 4 execução. Sem execução, bit 63 (NX). No x86 a página presente
 * continua legível: não há bit de "só escrita" nem de PROT_NONE. */
bool uvm_map_shared(process_t *p, u64 va, u64 phys, u8 prot) {
    bool ok = false;
    if (!pref_ready()) return false;
    vm_lock_range(p, va, 4096);
    u64 *e = walk(p, va & ~0xFFFULL, true);
    if (!e) { vm_unlock_range(p, va, 4096); return false; }
    if (*e & PTE_P) { vm_unlock_range(p, va, 4096); return true; }
    page_ref_inc(phys);
    u64 bits = PTE_P | PTE_U | PTE_SHARED;
    if (prot & 2) bits |= PTE_RW;
    if (!(prot & 4)) bits |= PTE_NX;
    *e = (phys & PTE_ADDR) | bits;
    p->mem_pages++;
    vm_flush(p, va);
    ok = true;
    vm_unlock_range(p, va, 4096);
    return ok;
}

/* 64 KiB NX compartilhados (A64RT_JIT_SHARE): cache de blocos do JIT entre processos. */
#define JIT_SHARE_VA 0x0000400044000000ULL
#define JIT_SHARE_NP 16
#define JIT_SHARE_MAGIC 0x4A49545348415245ULL
static u64 jit_share_phys;
static void jitshare_map(process_t *p) {
    int i;
    if (!p || !p->pml4 || !pref_ready()) return;
    if (!jit_share_phys) {
        u64 ph = pmm_alloc(JIT_SHARE_NP);
        if (!ph) return;
        memset((void *)ph, 0, JIT_SHARE_NP * 4096);
        *(u64 *)ph = JIT_SHARE_MAGIC;
        jit_share_phys = ph;
    }
    for (i = 0; i < JIT_SHARE_NP; i++) {
        u64 va = JIT_SHARE_VA + (u64)i * 4096, old, phys = jit_share_phys + (u64)i * 4096;
        u64 *e = walk(p, va, true);
        if (!e) return;
        if ((*e & PTE_P) && (*e & PTE_SHARED) && (*e & PTE_ADDR) == phys) continue;
        if (*e & PTE_P) {
            old = *e & PTE_ADDR;
            *e = 0;
            if (p->mem_pages) p->mem_pages--;
            if (old != phys) page_put(old);
            vm_flush(p, va);
        }
        uvm_map_shared(p, va, phys, 3);
    }
}

/* mprotect de verdade. Página COW não ganha RW (a escrita ainda copia). Página JWP sem escrita fica JWP;
 * mprotect com escrita desfaz o JWP. O bit de execução liga ou tira o NX. Página ausente fica para a falta. */
bool uvm_protect(process_t *p, u64 va, u64 len, u8 prot) {
    u64 end = va + len;
    bool ok = true;
    if (end < va) return false;
    vm_lock_range(p, va, len);
    for (u64 a = va & ~0xFFFULL; a < end; ) {
        u64 *e = walk(p, a, false);
        if (!e) { a = (a + 0x200000) & ~0x1FFFFFULL; continue; }
        if ((*e & PTE_P) && (*e & PTE_U)) {
            if (*e & PTE_JWP) {
                if (prot & 2) {
                    if ((*e & PTE_COW) && !cow_break(p, e, a)) { ok = false; break; }
                    *e = (*e | PTE_RW) & ~PTE_JWP;
                }
            } else if (!(*e & PTE_COW)) {
                if (prot & 2) *e |= PTE_RW;
                else *e &= ~PTE_RW;
            }
            if (prot & 4) *e &= ~PTE_NX;
            else *e |= PTE_NX;
            vm_flush(p, a);
        }
        a += 4096;
    }
    vm_unlock_range(p, va, len);
    return ok;
}

/* Rede de segurança do JIT do tradutor arm64 (userland/a64rt, MACH_VM_JIT_WPROTECT): página com código traduzido
 * perde a escrita na PTE e ganha o bit PTE_JWP (10, livre para o SO); a escrita do programa vira falta de proteção →
 * SIGBUS no tratador do runtime, que devolve a escrita (op 0), invalida as traduções e repete a instrução. Páginas
 * COW ficam de fora de op 1 (devolver a escrita a uma página compartilhada a escreveria para todos). op 0 numa
 * página COW primeiro a torna privada. No fork a página segue com JWP e COW (cow_copy_level); a escrita quebra
 * a cópia sem devolver a escrita e o SIGBUS avisa o runtime daquele processo. */
int uvm_jit_wprotect(process_t *p, u64 va, int op) {
    va &= ~0xFFFULL;
    u64 *e = uvm_pte(p, va);
    if (!e || !(*e & PTE_U)) return 0;
    if (op == 2) return (*e & PTE_RW) && !(*e & PTE_COW) ? 1 : 0;
    if (op == 1) {
        if (!(*e & PTE_RW) || (*e & PTE_COW)) return 0;
        *e = (*e & ~PTE_RW) | PTE_JWP;
        vm_flush(p, va);
        return 1;
    }
    if (*e & PTE_JWP) {
        if ((*e & PTE_COW) && !cow_break(p, e, va)) return 0;
        *e = (*e | PTE_RW) & ~PTE_JWP;
        vm_flush(p, va);
    }
    return 1;
}

/* MACH_VM_REMAP sem cópia: o mesmo físico em outro endereço virtual, os dois no mesmo processo.
 * A vista nova nasce gravável. Se a origem era COW, vira privada antes: senão a escrita no apelido
 * copiaria só uma das PTEs e as duas deixariam de ver os mesmos bytes. O fork (T4) ainda pode
 * separar o par, porque cada PTE vira COW por conta própria. */
bool lazy_fault(process_t *p, u64 va);
static void alias_drop(process_t *p, u64 va, u64 n) {
    for (u64 a = va; a < va + n; a += 4096) {
        u64 *e = walk(p, a, false);
        if (!e || !(*e & PTE_P)) continue;
        u64 pg = *e & PTE_ADDR;
        *e = 0;
        if (p->mem_pages) p->mem_pages--;
        uvm_page_put(pg);
        vm_flush(p, a);
    }
}
int uvm_alias_map(process_t *p, u64 *dst_io, u64 src, u64 len) {
    if (!p || !dst_io || !len || (len & 0xFFF) || (src & 0xFFF) || len > (1ull << 30)) return -MOS_EINVAL;
    if (src < USER_BASE || src + len > USER_END || src + len < src) return -MOS_EINVAL;
    u64 dst = *dst_io;
    if (dst & 0xFFF) return -MOS_EINVAL;
    if (!dst) {
        if (!p->mmap_next) p->mmap_next = USER_MMAP_BASE;
        u64 at = p->mmap_next;
        int found = 0;
        for (int t = 0; t < 64; t++) {
            if (at < USER_MMAP_BASE || at + len > USER_END || at + len < at) return -MOS_ENOMEM;
            int free = 1;
            for (u64 a = at; a < at + len; a += 4096) if (uvm_pte(p, a)) { free = 0; break; }
            if (free) { dst = at; found = 1; break; }
            at += len + 4096;
        }
        if (!found) return -MOS_ENOMEM;
    } else if (dst < USER_BASE || dst + len > USER_END || dst + len < dst) {
        return -MOS_EINVAL;
    } else {
        for (u64 a = dst; a < dst + len; a += 4096) if (uvm_pte(p, a)) return -MOS_EINVAL;
    }
    if (!(dst + len <= src || src + len <= dst)) return -MOS_EINVAL;
    u64 done = 0;
    for (u64 i = 0; i < len; i += 4096) {
        u64 sva = src + i;
        u64 *e = uvm_pte(p, sva);
        if (!e && lazy_fault(p, sva)) e = uvm_pte(p, sva);
        if (!e) { alias_drop(p, dst, done); return -MOS_ENOMEM; }
        if ((*e & PTE_COW) && !cow_break(p, e, sva)) { alias_drop(p, dst, done); return -MOS_ENOMEM; }
        e = uvm_pte(p, sva);
        if (!e || !uvm_map_phys(p, dst + i, *e & PTE_ADDR, true)) {
            alias_drop(p, dst, done);
            return -MOS_ENOMEM;
        }
        done += 4096;
    }
    if (!*dst_io && dst + len + 4096 > p->mmap_next) p->mmap_next = dst + len + 4096;
    *dst_io = dst;
    return 0;
}
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
            if (s[i] & PTE_SHARED) {
                /* MAP_SHARED: os dois lados escrevem na mesma página. Não vira COW. */
            } else if (s[i] & PTE_JWP) s[i] = (s[i] & ~PTE_RW) | PTE_COW;   /* código traduzido: segue sem escrita e com JWP */
            else if (s[i] & PTE_RW) s[i] = (s[i] & ~PTE_RW) | PTE_COW;
            page_ref_inc(s[i] & PTE_ADDR);
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
/* para proc/pty.c: sessões, grupos e sinais de terminal percorrem os processos */
process_t *proc_list(void) { return procs; }
process_t *proc_find(int pid) { return find(pid); }

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
            for (int k = 0; k < 2; k++) {                  /* .pipe2: a ponta de escrita de um socketpair */
                pipe_t *pp = !q->fds[i].used ? NULL : k ? q->fds[i].pipe2 : q->fds[i].pipe;
                if (!pp) continue;
                if (pp->rwait && pp->rwait->proc == p && pp->rwait != keep) pp->rwait = NULL;
                if (pp->wwait && pp->wwait->proc == p && pp->wwait != keep) pp->wwait = NULL;
            }
        }
}

static void finish(process_t *p, int code) {
    if (p->exited) return;
    p->exited = true;
    p->exit_code = code;
    p->stopped = 0;
    p->stop_unreported = 0;
    p->cont_unreported = 0;
    drop_pipe_waiters(p, NULL);
    for (thread_t *t = sched_threads(); t; t = t->next)   /* todas as threads do processo */
        if (t->proc == p) {
            if (p->tty && p->tty->reader == t) p->tty->reader = NULL;
            t->state = TH_DEAD;
        }
    { extern void smp_kick_proc(process_t *p); smp_kick_proc(p); }   /* as que rodam em outro núcleo saem do ring 3 já */
    pty_proc_exit(p);                                 /* líder de sessão: SIGHUP ao grupo em frente, a sessão perde o terminal */
    for (int i = 0; i < PROC_MAX_FDS; i++) fd_close(p, i);      /* fecha arquivos, pipes e terminais (acorda quem espera) */
    compat_gui_proc_exit(p);
    machipc_exit(p);                                  /* portas Mach: receptores morrem, envios viram nomes mortos */
    { extern void net_user_exit(process_t *p); net_user_exit(p); }   /* conexões TCP do processo (kernel/net/usersock.c) */
    { extern void webview_user_exit(process_t *p); webview_user_exit(p); }   /* vistas do WKWebView (kernel/web/webview.c) */
    { extern void audio_stream_proc_exit(int pid); audio_stream_proc_exit(p->pid); }   /* streams de áudio (kernel/dev/sound.c) */
    { extern void lazy_exit(process_t *p); lazy_exit(p); }           /* faixas de memória sob demanda (kernel/mm/lazy.c) */
    for (process_t *c = procs; c; c = c->next) if (c->ppid == p->pid) c->ppid = 0;   /* órfãos */
    int parent = p->ppid;
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
    /* SIGCHLD ao pai que tem tratador para ele: o libuv (Node/Electron) só colhe filhos assim, com o tratador
     * acordando o loop, que chama waitpid(WNOHANG). Sem isto nenhum child_process do VS Code (shell de login,
     * git, ripgrep da busca) dava 'exit'. Sem tratador (ação padrão: ignorar) o sinal some em sig_post sem acordar
     * ninguém com EINTR, salvo se o pai estiver num sigwait por ele. siginfo: CLD_EXITED/CLD_KILLED, pid e status. */
    if (parent > 0) {
        bool killed = code < 0 && code >= -31;
        proc_signal_info(parent, MOS_SIGCHLD, killed ? MOS_CLD_KILLED : MOS_CLD_EXITED, p->pid, killed ? -code : code & 0xFF);
    }
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
            /* como o dup2: a cópia nasce sem FD_CLOEXEC (addinherit_np é dup2(fd, fd)). O libuv marca CLOEXEC nas pontas
             * dos pipes de stdio; copiada, a marca fechava o stdout no execve do filho (o "bash -ilc 'Code -p ...'" que o
             * VS Code usa para ler o ambiente do shell escrevia o JSON no console e o resultado vinha vazio) */
            if (src && src->fds[a].used) { fd_copy(child, b, src, a); child->fds[b].flags &= ~MOS_O_CLOEXEC; }
        } else if (k == MOS_SPAWN_FA_OPEN && a >= 0 && a < PROC_MAX_FDS && acts[i].path[0]) {
            if (child->fds[a].used) fd_close(child, a);
            if (pty_open_path(child, a, acts[i].path, acts[i].flags) <= 0) continue;   /* /dev/ptmx, /dev/ttysNNN, /dev/tty */
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
    pty_proc_init(p, cur ? cur->proc : NULL, spawn_flags);   /* sessão e grupo (POSIX_SPAWN_SETSID do node-pty) */
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
    jitshare_map(p);

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
        /* processos do Chromium/Electron: o tipo vem em --type= (e --utility-sub-type=), não no nome do helper */
        for (int i = 0; xargv && i < xargc; i++)
            if (xargv[i] && (!strncmp(xargv[i], "--type=", 7) || !strncmp(xargv[i], "--utility-sub-type=", 19)))
                kprintf("proc: %s pid %d %s\n", p->name, p->pid, xargv[i]);
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

static bool sig_fault(int_frame_t *f, thread_t *t, process_t *p, u64 cr2);
static u64 proc_fault_(int_frame_t *f, u64 cr2);
/* a falta pode matar o processo e trocar de thread: a que volta também passa pela entrega de sinais */
u64 proc_fault(int_frame_t *f, u64 cr2) {
    u64 r = proc_fault_(f, cr2);
    return ((int_frame_t *)r)->cs & 3 ? proc_signal_check((int_frame_t *)r) : r;
}
static u64 proc_fault_(int_frame_t *f, u64 cr2) {
    thread_t *t = thread_current();
    process_t *p = t->proc;
    if (!p) return (u64)f;
    /* escrita numa página compartilhada depois do fork: copia e repete a instrução. Página de código
     * traduzido (JWP) fica privada ainda sem escrita; o SIGBUS abaixo avisa o runtime deste processo. */
    if (f->vector == 14 && (f->error & 7) == 7) {
        u64 *e = uvm_pte(p, cr2);
        if (e && (*e & PTE_COW)) {
            if (*e & PTE_JWP) cow_split_jwp(p, e, cr2);
            else if (cow_break(p, e, cr2)) return (u64)f;
        }
    }
    /* página ausente numa faixa de memória sob demanda (mmap anônimo, kernel/mm/lazy.c): página zerada e repete */
    if (f->vector == 14 && !(f->error & 1)) {
        extern bool lazy_fault(process_t *p, u64 va);
        if (lazy_fault(p, cr2)) return (u64)f;
    }
    /* falta espúria (SMP): outro núcleo mapeou a página ou quebrou o COW entre a falta e esta trava, ou a TLB deste
     * núcleo ainda tinha a entrada velha. Se a página agora permite o acesso, invalida a entrada local e repete.
     * Sem isto o marcador concorrente do V8 (lock cmpxchg no bitmap de marcação) matava o Claude.app.
     * Busca de instrução com NX não repete: senão a falta de uma página não executável não acaba nunca. */
    if (f->vector == 14 && cr2 >= USER_BASE && cr2 < USER_END) {
        u64 *e = uvm_pte(p, cr2);
        bool write = (f->error & 2) != 0;
        bool fetch = (f->error & 0x10) != 0;
        bool can = e && (*e & PTE_P) && (*e & PTE_U) && (!write || ((*e & PTE_RW) && !(*e & PTE_COW)));
        if (can && fetch && (*e & PTE_NX)) can = false;
        if (can) {
            __asm__ volatile("invlpg (%0)" :: "r"(cr2) : "memory");
            return (u64)f;
        }
    }
    /* tracepoint de diagnóstico (proc/tracepoint.c): registra, devolve o byte original e repete a instrução */
    { extern bool tp_hit(process_t *p, int_frame_t *f); if (tp_hit(p, f)) return (u64)f; }
    /* o programa tem tratador para esta falha? vira sinal da thread (SIGSEGV/SIGBUS/SIGFPE/SIGILL/SIGTRAP) */
    if (sig_fault(f, t, p, cr2)) return proc_signal_check(f);
    char msg[160];
    if (f->vector == 14) snprintf(msg, sizeof msg, "%s (pid %d): %s em 0x%llx, endereço 0x%llx (erro %llx) — processo encerrado\n",
                                  p->name, p->pid, fault_name(f->vector), f->rip, cr2, (unsigned long long)f->error);
    else snprintf(msg, sizeof msg, "%s (pid %d): %s (vetor %llu) em 0x%llx — processo encerrado\n",
                  p->name, p->pid, fault_name(f->vector), (unsigned long long)f->vector, f->rip);
    kprintf("proc: %s", msg);
    /* diagnóstico: thread e registradores principais (no log serial) */
    thread_t *ft = thread_current();
    kprintf("proc:   thread %d, rsp 0x%llx, rax 0x%llx, rbx 0x%llx, rdi 0x%llx, rsi 0x%llx\n",
            ft ? ft->id : -1, f->rsp, f->rax, f->rbx, f->rdi, f->rsi);
    if (f->vector == 14) { u64 *pe = uvm_pte(p, cr2); kprintf("proc:   pte de 0x%llx: 0x%llx\n", cr2, pe ? *pe : 0ULL); }
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
        /* a cadeia de rbp ({rbp salvo, retorno}): em código com frame pointer (Chromium, libSystem) é a pilha de
         * chamadas de verdade, sem os falsos positivos da varredura acima */
        u64 fp = f->rbp;
        o = 0;
        for (int k = 0; k < 20 && fp; k++) {
            u64 fr[2];
            if ((fp & 7) || !uvm_copy_in(p, fr, fp, sizeof fr)) break;
            o += (usize)snprintf(line + o, sizeof line - o, " %llx", (unsigned long long)fr[1]);
            if (o > 150) { kprintf("proc:   chamadas:%s\n", line); o = 0; }
            if (fr[0] <= fp) break;
            fp = fr[0];
        }
        if (o) kprintf("proc:   chamadas:%s\n", line);
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
    kprintf("despejo: threads em %llu ms (memória livre %llu de %llu MiB)\n", (unsigned long long)last,
            (unsigned long long)(pmm_free_pages() / 256), (unsigned long long)(pmm_total_pages() / 256));
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
        /* cadeia de quadros (rbp -> [rbp+8]): o Chromium mantém o ponteiro de quadro, então isto é a pilha de chamadas
         * de verdade (a varredura acima também pega endereços velhos que sobraram na pilha) */
        {
            char fl[200];
            usize fo = 0;
            u64 bp = f->rbp;
            for (int d = 0; d < 12 && bp && !(bp & 7); d++) {
                u64 pair[2];
                if (!uvm_copy_in(t->proc, pair, bp, 16) || !pair[1]) break;
                fo += (usize)snprintf(fl + fo, sizeof fl - fo, " %llx", pair[1]);
                if (fo > 150) { kprintf("despejo:   quadros:%s\n", fl); fo = 0; }
                if (pair[0] <= bp) break;                   /* a cadeia sobe na pilha */
                bp = pair[0];
            }
            if (fo) kprintf("despejo:   quadros:%s\n", fl);
        }
    }
    /* filas Mach com mensagens que ninguém recebeu ainda (machipc.c) */
    { extern void machipc_dump_queues(process_t *p); for (process_t *q = procs; q; q = q->next) machipc_dump_queues(q); }
}

/* ------------------------------------------------------------ sinais
 * Modelo (detalhes em syscall.h, SYS_SIG): ações por processo (tratador, sa_mask, sa_flags, trampolim), máscara e
 * pendentes por thread, pendentes de kill() no processo. A entrega é sempre na volta ao ring 3 (fim de syscall,
 * de interrupção ou de falta): proc_signal_check escolhe um sinal não bloqueado, monta o quadro na pilha do
 * usuário (siginfo + ucontext + mcontext no layout do Darwin) e desvia para o trampolim/tratador.
 * Quem está bloqueado numa espera interrompível (sleep, pipe, terminal, wait, futex, sigsuspend, sigwait) é
 * acordado com -EINTR; se nenhum tratador acabar rodando nela (o sinal foi para outra thread, ou virou
 * "ignorar"), ou se a ação tem SA_RESTART e a syscall é refazível, ela é refeita (rip volta 2 bytes, para o int
 * 0x80/syscall, e rax volta a ser o número). Esperas Mach e o join de thread não são interrompidas: o sinal fica
 * pendente para a próxima thread que voltar ao ring 3. */
#define SIG_BIT(s) (1u << (s))
#define SIG_UNBLOCKABLE (SIG_BIT(MOS_SIGKILL) | SIG_BIT(MOS_SIGSTOP))
#define STOP_BITS (SIG_BIT(MOS_SIGSTOP) | SIG_BIT(MOS_SIGTSTP) | SIG_BIT(MOS_SIGTTIN) | SIG_BIT(MOS_SIGTTOU))
#define SIGFRAME_MAGIC 0x6D4F535349474E4CULL   /* "mOSSIGNL" */
#define SIG_MINSTKSZ 32768                     /* MINSIGSTKSZ do macOS */
#define WAIT_SIGSUSPEND -10
#define WAIT_SIGWAIT    -11

/* quadro na pilha do usuário (endereço 16-alinhado; o endereço de retorno fica logo abaixo) */
typedef struct {
    u64 magic;
    u64 uctx;                   /* endereço de .uc: o restorer antigo acha o quadro pelo rsp */
    mos_siginfo_t info;
    mos_ucontext_t uc;
    mos_mcontext_t mc;
    u64 pad;
    /* com XSAVE (AVX ligado, arch/cpu.c): os bytes 512..831 da área de FPU, o cabeçalho XSAVE e as metades altas dos YMM.
     * A imagem FXSAVE do mc só leva os 128 bits de baixo; sem isto o tratador devolvia ao código interrompido os YMM dele.
     * Fica depois do mcontext, que mantém o layout do Darwin. */
    u8 xext[320];
} usigframe_t;
extern u8 g_use_xsave;
_Static_assert(sizeof(mos_siginfo_t) == 104 && sizeof(mos_ucontext_t) == 56 && sizeof(mos_mcontext_t) == 712,
               "layout do Darwin x86-64");
_Static_assert(sizeof(usigframe_t) % 16 == 0, "quadro alinhado");

typedef struct { int code, pid, status; u64 addr; } siginfo_k;

/* ação padrão "ignorar" (macOS: CHLD, URG, CONT, WINCH, INFO, IO). CONT continua o processo antes deste
 * teste. STOP/TSTP/TTIN/TTOU não entram: a ação padrão para o processo (proc_stop), não o encerra. */
static bool default_ignored(int sig) {
    switch (sig) {
    case MOS_SIGCHLD: case MOS_SIGURG: case MOS_SIGCONT: case MOS_SIGWINCH: case MOS_SIGINFO: case MOS_SIGIO:
        return true;
    }
    return false;
}
static bool is_stop_sig(int sig) {
    return sig == MOS_SIGSTOP || sig == MOS_SIGTSTP || sig == MOS_SIGTTIN || sig == MOS_SIGTTOU;
}
static bool sig_ignored(process_t *p, int sig) {
    u64 h = p->sig_handler[sig];
    return h == MOS_SIG_IGN || (h == MOS_SIG_DFL && default_ignored(sig));
}
static u8 *fx_area(int_frame_t *f) { return FPU_AREA_OF(f); }   /* os 512 primeiros: a imagem FXSAVE (proc.h) */

/* a espera desta thread pode ser interrompida por um sinal? */
static bool sig_interruptible(thread_t *t) {
    if (t->state == TH_SLEEPING) return true;      /* SYS_SLEEP, leitura de terminal com VTIME, sigwait com prazo */
    if (t->state != TH_WAITING) return false;
    int w = t->wait_pid;
    return w > 0 || w == -1 || w == -2 || w == -3 || w == -4 || w == -8 || w == -9 || w == WAIT_SIGSUSPEND || w == WAIT_SIGWAIT;
}

/* tira t da espera com -EINTR, desfazendo as referências que o pipe/terminal/tty guardam dela */
/* diagnóstico: com /Sistema/Biblioteca/despejar-threads, as primeiras entregas e interrupções vão para a serial */
static bool sig_diag(void) {
    static int on = -1, n;
    if (on < 0) on = vfs_lookup("/Sistema/Biblioteca/despejar-threads") != NULL;
    return on && n++ < 400;
}
static void sig_interrupt(thread_t *t) {
    process_t *p = t->proc;
    if (sig_diag()) kprintf("sinal: %s pid %d thread %d acordada com EINTR (espera %d, syscall %llu)\n", p->name, p->pid, t->id,
                            t->state == TH_SLEEPING ? -100 : t->wait_pid, (unsigned long long)t->sys_nr);
    if (p->tty && p->tty->reader == t) p->tty->reader = NULL;
    if (t->wait_pid == -3 || t->wait_pid == -4)
        for (int i = 0; i < PROC_MAX_FDS; i++) {
            for (int k = 0; k < 2; k++) {
                pipe_t *pp = !p->fds[i].used ? NULL : k ? p->fds[i].pipe2 : p->fds[i].pipe;
                if (pp && pp->rwait == t) pp->rwait = NULL;
                if (pp && pp->wwait == t) pp->wwait = NULL;
            }
        }
    t->pty_wait = NULL;
    t->futex_va = 0;
    t->wake_at = 0;
    t->wait_posix = false;
    t->wait_pid = 0;
    t->sig_wait_set = 0;
    ((int_frame_t *)t->frame)->rax = (u64)-MOS_EINTR;
    t->sig_intr = true;
    t->state = TH_READY;
}

/* sigwait em andamento que aceita sig: entrega direto (devolve true) */
static bool sig_to_waiter(process_t *p, thread_t *only, int sig, const siginfo_k *k) {
    for (thread_t *t = sched_threads(); t; t = t->next) {
        if (t->proc != p || (only && t != only) || t->wait_pid != WAIT_SIGWAIT || !(t->sig_wait_set & SIG_BIT(sig))) continue;
        if (t->state != TH_WAITING && t->state != TH_SLEEPING) continue;
        if (t->wait_status) {
            mos_siginfo_t si;
            memset(&si, 0, sizeof si);
            si.si_signo = sig; si.si_code = k->code; si.si_pid = k->pid; si.si_status = k->status; si.si_addr = k->addr; si.si_uid = 501;
            uvm_copy_out(p, t->wait_status, &si, sizeof si);
        }
        ((int_frame_t *)t->frame)->rax = (u64)sig;
        t->wait_pid = 0; t->sig_wait_set = 0; t->wake_at = 0; t->wait_status = 0;
        t->state = TH_READY;
        return true;
    }
    return false;
}

static void sig_kill_default(process_t *p, int sig) {
    char m[96];
    snprintf(m, sizeof m, "%s (pid %d) encerrado pelo sinal %d\n", p->name, p->pid, sig);
    if (sig != MOS_SIGINT) kprintf("proc: %s", m);
    finish(p, -sig);
}

static void sig_clear_pending(process_t *p, u32 bits) {
    p->sig_pending &= ~bits;
    for (thread_t *w = sched_threads(); w; w = w->next)
        if (w->proc == p) w->sig_pending &= ~bits;
}

/* status de parada/continuação no formato do Darwin: (sinal << 8) | 0x7f. O número é o do kernel;
 * o libSystem traduz o byte alto (STOP 20 → 17) para WSTOPSIG/WIFCONTINUED. */
static int stop_status(int sig) { return (sig << 8) | 0x7F; }

/* acorda no máximo um waitpid do pai que pediu este evento. Antes do SIGCHLD, para o tratador do pai
 * não consumir o status com waitpid(WNOHANG) e deixar o waitpid bloqueado sem resposta. */
static void report_child_event(process_t *p, int st, bool cont) {
    if (p->ppid <= 0) return;
    for (thread_t *w = sched_threads(); w; w = w->next) {
        if (!w->wait_posix || w->state != TH_WAITING || !w->proc || w->proc->pid != p->ppid) continue;
        if (w->wait_pid != p->pid && w->wait_pid != -1) continue;
        if (cont ? !(w->wait_opt & MOS_WCONTINUED) : !(w->wait_opt & MOS_WUNTRACED)) continue;
        if (w->wait_status) uvm_copy_out(w->proc, w->wait_status, &st, sizeof st);
        ((int_frame_t *)w->frame)->rax = (u64)p->pid;
        w->wait_posix = false;
        w->wait_pid = 0;
        w->state = TH_READY;
        if (cont) p->cont_unreported = 0;
        else p->stop_unreported = 0;
        return;
    }
}

static bool sig_post(process_t *p, thread_t *th, int sig, const siginfo_k *k);
static void notify_parent_cld(process_t *p, int code, int status) {
    if (p->ppid <= 0) return;
    process_t *par = find(p->ppid);
    if (!par || par->exited) return;
    if (par->sig_flags[MOS_SIGCHLD] & MOS_SA_NOCLDSTOP) return;
    siginfo_k ck = { code, p->pid, status, 0 };
    sig_post(par, NULL, MOS_SIGCHLD, &ck);
}

/* para todas as threads (nativas e as do tradutor ARM64: são threads deste processo). Já parado: o POSIX
 * descarta o sinal novo e não manda outro SIGCHLD. */
static void proc_stop(process_t *p, int sig) {
    if (!p || p->exited || p->stopped) return;
    p->stopped = sig;
    p->cont_unreported = 0;
    p->stop_unreported = 1;
    report_child_event(p, stop_status(sig), false);
    notify_parent_cld(p, MOS_CLD_STOPPED, sig);
    { extern void smp_kick_proc(process_t *p); smp_kick_proc(p); }
}

static void proc_continue(process_t *p) {
    if (!p || !p->stopped) return;
    p->stopped = 0;
    p->stop_unreported = 0;
    p->cont_unreported = 1;
    report_child_event(p, stop_status(MOS_SIGCONT), true);
    notify_parent_cld(p, MOS_CLD_CONTINUED, MOS_SIGCONT);
}

/* todas as threads vivas bloqueiam sig? (SIGSTOP não conta: é unblockable) */
static bool sig_blocked_everywhere(process_t *p, thread_t *only, int sig) {
    if (SIG_BIT(sig) & SIG_UNBLOCKABLE) return false;
    bool any = false;
    for (thread_t *w = sched_threads(); w; w = w->next) {
        if (w->proc != p || w->state == TH_DEAD) continue;
        if (only && w != only) continue;
        any = true;
        if (!(w->sig_mask & SIG_BIT(sig))) return false;
    }
    return any;
}

/* sinal para o processo (kill) ou para uma thread (pthread_kill, faltas) */
static bool sig_post(process_t *p, thread_t *th, int sig, const siginfo_k *k) {
    if (!p || p->exited) return false;
    if (sig == MOS_SIGKILL) { finish(p, -MOS_SIGKILL); return true; }
    if (sig == MOS_SIGCONT) {
        sig_clear_pending(p, STOP_BITS);             /* POSIX: CONT descarta paradas pendentes */
        if (p->stopped) proc_continue(p);
    } else if (is_stop_sig(sig))
        sig_clear_pending(p, SIG_BIT(MOS_SIGCONT));
    if (sig_to_waiter(p, th, sig, k)) return true;      /* sigwait pega mesmo bloqueado (é o uso normal) */
    if (is_stop_sig(sig)) {
        bool caught = sig != MOS_SIGSTOP && p->sig_handler[sig] > MOS_SIG_IGN;
        bool held = sig_blocked_everywhere(p, th, sig);
        if (!caught) {
            if (sig != MOS_SIGSTOP && sig_ignored(p, sig)) return true;   /* TSTP/TTIN/TTOU ignorados não param */
            if (!held) { proc_stop(p, sig); return true; }                /* ação padrão: para já, sem EINTR */
        }
        /* bloqueado, ou com tratador: fica pendente e sai quando desbloquear / na volta ao ring 3 */
    } else if (sig_ignored(p, sig)) return true;        /* some aqui, sem acordar ninguém com EINTR (SIGWINCH...) */
    if (th) { th->sig_pending |= SIG_BIT(sig); th->sig_info[sig].code = k->code; th->sig_info[sig].pid = k->pid;
              th->sig_info[sig].status = k->status; th->sig_info[sig].addr = k->addr; }
    else    { p->sig_pending |= SIG_BIT(sig); p->sig_info[sig].code = k->code; p->sig_info[sig].pid = k->pid;
              p->sig_info[sig].status = k->status; p->sig_info[sig].addr = k->addr; }
    /* uma thread que não o bloqueia e está parada numa espera interrompível volta com EINTR para recebê-lo.
     * Se nenhuma está (todas rodando, ou em mach_msg), a primeira que voltar ao ring 3 o entrega. */
    thread_t *cur = thread_current();
    for (thread_t *t = sched_threads(); t; t = t->next) {
        if (t->proc != p || (th && t != th) || (t->sig_mask & SIG_BIT(sig))) continue;
        if (t == cur && t->state == TH_RUNNING) return true;   /* sai na volta desta syscall */
        if (sig_interruptible(t)) { sig_interrupt(t); return true; }
    }
    return true;
}

bool proc_signal_info(int pid, int sig, int code, int spid, int status) {
    if (sig == 0) return proc_alive(pid);                /* kill(pid, 0): só pergunta se o processo existe */
    if (sig < 0 || sig >= MOS_NSIG) return false;
    u64 fl = irq_save();
    process_t *p = find(pid);
    siginfo_k k = { code, spid, status, 0 };
    if (p && sig_diag()) kprintf("sinal: %d para %s pid %d (de pid %d, %s)\n", sig, p->name, pid, spid,
                                 sig_ignored(p, sig) ? "ignorado" : p->sig_handler[sig] > MOS_SIG_IGN ? "tratador" : "padrão");
    bool ok = p && !p->exited && sig_post(p, NULL, sig, &k);
    irq_restore(fl);
    return ok;
}

bool proc_signal(int pid, int sig) {
    thread_t *c = thread_current();
    return proc_signal_info(pid, sig, MOS_SI_USER, c && c->proc ? c->proc->pid : 0, 0);
}

/* pty.c, com a trava já segurada. Não gera o sinal se ele for ignorado ou se esta thread o bloqueia. */
int proc_job_signal(process_t *p, thread_t *th, int sig) {
    if (!p || p->exited || !is_stop_sig(sig)) return 0;
    if (sig != MOS_SIGSTOP && (p->sig_handler[sig] == MOS_SIG_IGN || (th && (th->sig_mask & SIG_BIT(sig)))))
        return 0;
    siginfo_k k = { MOS_SI_KERNEL, p ? p->pid : 0, 0, 0 };
    sig_post(p, NULL, sig, &k);
    return p->stopped ? 1 : 2;
}

/* syscalls que o SA_RESTART refaz (as demais bloqueantes devolvem EINTR, como no macOS: sleep, poll, sigsuspend) */
static bool sys_restartable(u64 nr) {
    return nr == SYS_READ || nr == SYS_WRITE || nr == SYS_WAITPID || nr == SYS_WAIT || nr == SYS_WAITANY || nr == SYS_FUTEX_WAIT;
}
static void sys_restart(int_frame_t *f, thread_t *t) { f->rip -= 2; f->rax = t->sys_nr; }   /* int 0x80 e syscall: 2 bytes */

static u64 sig_deliver(int_frame_t *f, thread_t *t, process_t *p, int sig, const siginfo_k *k) {
    u32 fl = p->sig_flags[sig];
    u64 h = p->sig_handler[sig], tramp = p->sig_tramp[sig];
    if (sig_diag()) kprintf("sinal: %d entregue a %s pid %d thread %d (rip 0x%llx, flags 0x%x, interrompida %d, syscall %llu)\n", sig,
                            p->name, p->pid, t->id, f->rip, fl, t->sig_intr, (unsigned long long)t->sys_nr);
    if (t->sig_intr) {                                   /* syscall interrompida: EINTR, ou refeita com SA_RESTART */
        t->sig_intr = false;
        if ((fl & MOS_SA_RESTART) && sys_restartable(t->sys_nr)) sys_restart(f, t);
    }
    u32 oldmask = t->sig_suspended ? t->sig_saved_mask : t->sig_mask;   /* sigsuspend: a volta restaura a máscara antiga */
    t->sig_suspended = false;
    bool onalt = t->ss_size && f->rsp > t->ss_sp && f->rsp <= t->ss_sp + t->ss_size;
    bool usealt = (fl & MOS_SA_ONSTACK) && t->ss_size && !onalt;
    u64 top = usealt ? t->ss_sp + t->ss_size : f->rsp - 128;            /* fora da red zone */
    u64 fa = (top - sizeof(usigframe_t)) & ~15ULL, ret = fa - 8;
    usigframe_t *sf = kcalloc(1, sizeof *sf);
    if (!sf) { finish(p, -sig); return sched_switch(f); }
    sf->magic = SIGFRAME_MAGIC;
    sf->uctx = fa + __builtin_offsetof(usigframe_t, uc);
    sf->info.si_signo = sig; sf->info.si_code = k->code; sf->info.si_pid = k->pid; sf->info.si_uid = 501;
    sf->info.si_status = k->status; sf->info.si_addr = k->addr;
    sf->uc.uc_onstack = onalt || usealt;
    sf->uc.uc_sigmask = oldmask;
    sf->uc.uc_stack.sp = t->ss_sp; sf->uc.uc_stack.size = t->ss_size;
    sf->uc.uc_stack.flags = !t->ss_size ? MOS_SS_DISABLE : (onalt || usealt) ? MOS_SS_ONSTACK : 0;
    sf->uc.uc_mcsize = sizeof(mos_mcontext_t);
    sf->uc.uc_mcontext = fa + __builtin_offsetof(usigframe_t, mc);
    mos_mcontext_t *mc = &sf->mc;
    if (k->addr || t->sig_trapno) { mc->trapno = t->sig_trapno; mc->err = t->sig_err; mc->faultvaddr = k->addr; }
    t->sig_trapno = 0; t->sig_err = 0;
    mos_thread_state_t *s = &mc->ss;
    s->rax = f->rax; s->rbx = f->rbx; s->rcx = f->rcx; s->rdx = f->rdx; s->rdi = f->rdi; s->rsi = f->rsi;
    s->rbp = f->rbp; s->rsp = f->rsp; s->r8 = f->r8; s->r9 = f->r9; s->r10 = f->r10; s->r11 = f->r11;
    s->r12 = f->r12; s->r13 = f->r13; s->r14 = f->r14; s->r15 = f->r15; s->rip = f->rip; s->rflags = f->rflags;
    s->cs = f->cs;
    memcpy(mc->fx, fx_area(f), 512);
    if (g_use_xsave) memcpy(sf->xext, fx_area(f) + 512, sizeof sf->xext);
    u64 retv = tramp ? 0 : p->sig_restorer;
    bool ok = uvm_copy_out(p, fa, sf, sizeof *sf) && uvm_copy_out(p, ret, &retv, 8);
    kfree(sf);
    if (!ok) {                                           /* pilha (ou sigaltstack) estourada: como no macOS, morre */
        kprintf("proc: %s (pid %d): sem pilha para entregar o sinal %d (rsp 0x%llx)\n", p->name, p->pid, sig, f->rsp);
        finish(p, -MOS_SIGSEGV);
        return sched_switch(f);
    }
    t->sig_mask = (t->sig_mask | p->sig_amask[sig] | ((fl & MOS_SA_NODEFER) ? 0 : SIG_BIT(sig))) & ~SIG_UNBLOCKABLE;
    if ((fl & MOS_SA_RESETHAND) && sig != MOS_SIGILL && sig != MOS_SIGTRAP) {
        p->sig_handler[sig] = MOS_SIG_DFL;
        p->sig_flags[sig] &= ~MOS_SA_SIGINFO;
    }
    f->rsp = ret;                                        /* rsp = 16n+8, como depois de um call */
    if (tramp) {                                         /* _sigtramp(handler, estilo, sinal, siginfo*, ucontext*) */
        f->rip = tramp;
        f->rdi = h;
        f->rsi = (fl & MOS_SA_SIGINFO) ? 30 : 1;         /* UC_FLAVOR : UC_TRAD */
        f->rdx = (u64)sig;
        f->rcx = fa + __builtin_offsetof(usigframe_t, info);
        f->r8 = fa + __builtin_offsetof(usigframe_t, uc);
    } else {                                             /* handler(sinal, siginfo*, ucontext*) e volta pelo restorer */
        f->rip = h;
        f->rdi = (u64)sig;
        f->rsi = fa + __builtin_offsetof(usigframe_t, info);
        f->rdx = fa + __builtin_offsetof(usigframe_t, uc);
    }
    f->rflags = (f->rflags & ~0x500ULL) | 0x200;         /* DF e TF limpos (ABI), IF ligado */
    return (u64)f;
}

u64 proc_signal_check(int_frame_t *f) {
    thread_t *t = thread_current();
    process_t *p = t ? t->proc : NULL;
    if (!p || (f->cs & 3) != 3) return (u64)f;
    if (p->exited) return t->state == TH_DEAD ? sched_switch(f) : (u64)f;   /* morto agora (kill -9 em si mesmo): não volta */
    if (p->stopped) {                                    /* SIGSTOP em si mesmo, ou o kick de outro núcleo */
        if (t->state == TH_RUNNING) t->state = TH_READY;
        return sched_switch(f);
    }
    for (;;) {
        u32 ready = (t->sig_pending | p->sig_pending) & ~(t->sig_mask & ~SIG_UNBLOCKABLE);
        if (!ready) break;
        u32 own = t->sig_pending & ready;                /* os da thread (faltas) primeiro */
        int sig = __builtin_ctz(own ? own : ready);
        siginfo_k k;
        if (t->sig_pending & SIG_BIT(sig)) {
            t->sig_pending &= ~SIG_BIT(sig);
            k.code = t->sig_info[sig].code; k.pid = t->sig_info[sig].pid; k.status = t->sig_info[sig].status; k.addr = t->sig_info[sig].addr;
        } else {
            p->sig_pending &= ~SIG_BIT(sig);
            k.code = p->sig_info[sig].code; k.pid = p->sig_info[sig].pid; k.status = p->sig_info[sig].status; k.addr = p->sig_info[sig].addr;
        }
        if (sig_ignored(p, sig)) continue;
        if (is_stop_sig(sig)) {
            /* desbloqueou uma parada que estava pendente. Com tratador, entrega; senão para (não mata). */
            if (sig != MOS_SIGSTOP && p->sig_handler[sig] > MOS_SIG_IGN && (p->sig_tramp[sig] || p->sig_restorer))
                return sig_deliver(f, t, p, sig, &k);
            proc_stop(p, sig);
            if (t->state == TH_RUNNING) t->state = TH_READY;
            return sched_switch(f);
        }
        u64 h = p->sig_handler[sig];
        if (h == MOS_SIG_DFL || (!p->sig_tramp[sig] && !p->sig_restorer)) {   /* ação padrão: encerra */
            sig_kill_default(p, sig);
            return sched_switch(f);
        }
        return sig_deliver(f, t, p, sig, &k);
    }
    /* acordada por um sinal que não chegou a rodar tratador nesta thread: refaz a espera, sem EINTR à toa */
    if (t->sig_suspended) { t->sig_mask = t->sig_saved_mask; t->sig_suspended = false; }
    if (t->sig_intr) { t->sig_intr = false; sys_restart(f, t); }
    return (u64)f;
}

/* volta do tratador: o quadro em fa (validado pelo magic), registradores do mcontext para onde uc_mcontext aponta */
static u64 sig_restore(int_frame_t *f, u64 fa) {
    thread_t *t = thread_current();
    process_t *p = t->proc;
    usigframe_t *sf = kmalloc(sizeof *sf);
    bool ok = sf && uvm_copy_in(p, sf, fa, sizeof *sf) && sf->magic == SIGFRAME_MAGIC;
    if (ok && sf->uc.uc_mcontext != fa + __builtin_offsetof(usigframe_t, mc))   /* o programa trocou o mcontext */
        ok = uvm_copy_in(p, &sf->mc, sf->uc.uc_mcontext, sizeof sf->mc);
    const mos_thread_state_t *s = ok ? &sf->mc.ss : NULL;
    if (ok && (s->rip >= 0x0000800000000000ULL || s->rsp >= 0x0000800000000000ULL)) ok = false;   /* iretq não canônico: #GP no kernel */
    if (!ok) {
        kprintf("proc: %s (pid %d): sigreturn com quadro inválido em 0x%llx\n", p->name, p->pid, fa);
        kfree(sf);
        finish(p, -MOS_SIGSEGV);
        return sched_switch(f);
    }
    f->rax = s->rax; f->rbx = s->rbx; f->rcx = s->rcx; f->rdx = s->rdx; f->rdi = s->rdi; f->rsi = s->rsi;
    f->rbp = s->rbp; f->rsp = s->rsp; f->r8 = s->r8; f->r9 = s->r9; f->r10 = s->r10; f->r11 = s->r11;
    f->r12 = s->r12; f->r13 = s->r13; f->r14 = s->r14; f->r15 = s->r15; f->rip = s->rip;
    f->cs = 0x1B; f->ss = 0x23;                                      /* nunca volta para outro anel */
    f->rflags = (s->rflags & 0xDD5) | 0x202;                         /* flags aritméticas, DF e TF + IF */
    u8 *fx = fx_area(f);
    memcpy(fx, sf->mc.fx, 512);
    if (g_use_xsave) {
        /* o cabeçalho vem da pilha do usuário: só os bits x87|SSE|AVX no XSTATE_BV e o resto zerado, senão o XRSTOR do
         * isr_common dá #GP no kernel */
        u64 bv;
        memcpy(&bv, sf->xext, 8);
        bv &= 7;
        memset(sf->xext, 0, 64);
        memcpy(sf->xext, &bv, 8);
        memcpy(fx + 512, sf->xext, sizeof sf->xext);
    }
    u32 mxcsr, mmask;
    memcpy(&mxcsr, fx + 24, 4); memcpy(&mmask, fx + 28, 4);
    mxcsr &= mmask ? mmask : 0xFFBF;                                 /* bit reservado ligado: fxrstor daria #GP */
    memcpy(fx + 24, &mxcsr, 4);
    t->sig_mask = sf->uc.uc_sigmask & ~SIG_UNBLOCKABLE;
    t->sig_intr = false;
    kfree(sf);
    return proc_signal_check(f);
}
static u64 sig_return(int_frame_t *f) { return sig_restore(f, f->rsp); }   /* SYS_SIGRETURN: o restorer, com rsp no quadro */

/* SYS_SIG */
static u64 sig_syscall(int_frame_t *f, thread_t *t, process_t *p) {
    u64 op = f->rdi, a = f->rsi, b = f->rdx, c = f->r10;
    i64 r = 0;
    switch (op) {
    case MOS_SIGOP_ACTION: {
        int sig = (int)a;
        if (sig <= 0 || sig >= MOS_NSIG || (b && (sig == MOS_SIGKILL || sig == MOS_SIGSTOP))) { r = -MOS_EINVAL; break; }
        mos_sigaction_t o = { p->sig_handler[sig], p->sig_tramp[sig], p->sig_amask[sig], p->sig_flags[sig] }, n;
        if (b && !uvm_copy_in(p, &n, b, sizeof n)) { r = -MOS_EFAULT; break; }
        if (c && !uvm_copy_out(p, c, &o, sizeof o)) { r = -MOS_EFAULT; break; }
        if (!b) break;
        if (n.handler > MOS_SIG_IGN && (n.handler < USER_BASE || n.handler >= USER_END)) { r = -MOS_EINVAL; break; }
        p->sig_handler[sig] = n.handler;
        p->sig_tramp[sig] = n.handler > MOS_SIG_IGN ? n.tramp : 0;
        p->sig_amask[sig] = n.mask & ~SIG_UNBLOCKABLE;
        p->sig_flags[sig] = n.flags;
        if (sig_ignored(p, sig)) {                       /* POSIX: pendentes de um sinal que passa a ser ignorado somem */
            p->sig_pending &= ~SIG_BIT(sig);
            for (thread_t *w = sched_threads(); w; w = w->next) if (w->proc == p) w->sig_pending &= ~SIG_BIT(sig);
        }
        break;
    }
    case MOS_SIGOP_MASK: {
        u32 set = 0, old = t->sig_mask;
        if (b && !uvm_copy_in(p, &set, b, 4)) { r = -MOS_EFAULT; break; }
        if (b) {
            if (a == MOS_SIG_BLOCK) t->sig_mask |= set;
            else if (a == MOS_SIG_UNBLOCK) t->sig_mask &= ~set;
            else if (a == MOS_SIG_SETMASK) t->sig_mask = set;
            else { r = -MOS_EINVAL; break; }
            t->sig_mask &= ~SIG_UNBLOCKABLE;
        }
        if (c && !uvm_copy_out(p, c, &old, 4)) r = -MOS_EFAULT;
        break;                                           /* o que ficou desbloqueado sai na volta (proc_signal_check) */
    }
    case MOS_SIGOP_PENDING: {
        u32 s = t->sig_pending | p->sig_pending;
        if (!uvm_copy_out(p, a, &s, 4)) r = -MOS_EFAULT;
        break;
    }
    case MOS_SIGOP_SUSPEND:
        t->sig_saved_mask = t->sig_mask;
        t->sig_suspended = true;
        t->sig_mask = (u32)a & ~SIG_UNBLOCKABLE;
        f->rax = (u64)-MOS_EINTR;
        if ((t->sig_pending | p->sig_pending) & ~t->sig_mask) return proc_signal_check(f);   /* já há um: entrega agora */
        t->wait_pid = WAIT_SIGSUSPEND;
        t->state = TH_WAITING;
        return sched_switch(f);
    case MOS_SIGOP_ALTSTACK: {
        bool onalt = t->ss_size && f->rsp > t->ss_sp && f->rsp <= t->ss_sp + t->ss_size;
        mos_sigstack_t o = { t->ss_sp, t->ss_size, !t->ss_size ? MOS_SS_DISABLE : onalt ? MOS_SS_ONSTACK : 0, 0 }, n;
        if (a && !uvm_copy_in(p, &n, a, sizeof n)) { r = -MOS_EFAULT; break; }
        if (b && !uvm_copy_out(p, b, &o, sizeof o)) { r = -MOS_EFAULT; break; }
        if (!a) break;
        if (onalt) { r = -MOS_EPERM; break; }
        if (n.flags & MOS_SS_DISABLE) { t->ss_sp = 0; t->ss_size = 0; break; }
        if (n.flags & ~MOS_SS_DISABLE) { r = -MOS_EINVAL; break; }
        if (n.size < SIG_MINSTKSZ) { r = -MOS_ENOMEM; break; }
        if (n.sp < USER_BASE || n.sp + n.size > USER_END) { r = -MOS_EINVAL; break; }
        t->ss_sp = n.sp; t->ss_size = n.size;
        break;
    }
    case MOS_SIGOP_RETURN: return sig_restore(f, a - __builtin_offsetof(usigframe_t, uc));
    case MOS_SIGOP_TKILL: {
        int sig = (int)b;
        thread_t *j = NULL;
        for (thread_t *w = sched_threads(); w; w = w->next) if (w->id == (int)a && w->proc == p && w->state != TH_DEAD) j = w;
        if (!j) { r = -MOS_ESRCH; break; }
        if (sig < 0 || sig >= MOS_NSIG) { r = -MOS_EINVAL; break; }
        if (!sig) break;
        siginfo_k k = { MOS_SI_USER, p->pid, 0, 0 };
        sig_post(p, j, sig, &k);
        break;
    }
    case MOS_SIGOP_WAIT: {
        u32 set = (u32)a & ~SIG_UNBLOCKABLE;
        u32 hit = (t->sig_pending | p->sig_pending) & set;
        if (hit) {
            int sig = __builtin_ctz(hit);
            bool own = t->sig_pending & SIG_BIT(sig);
            mos_siginfo_t si;
            memset(&si, 0, sizeof si);
            si.si_signo = sig; si.si_uid = 501;
            si.si_code = own ? t->sig_info[sig].code : p->sig_info[sig].code;
            si.si_pid = own ? t->sig_info[sig].pid : p->sig_info[sig].pid;
            si.si_status = own ? t->sig_info[sig].status : p->sig_info[sig].status;
            si.si_addr = own ? t->sig_info[sig].addr : p->sig_info[sig].addr;
            if (own) t->sig_pending &= ~SIG_BIT(sig); else p->sig_pending &= ~SIG_BIT(sig);
            if (b) uvm_copy_out(p, b, &si, sizeof si);
            r = sig;
            break;
        }
        if (!c) { r = -MOS_EAGAIN; break; }
        t->sig_wait_set = set;
        t->wait_status = b;
        t->wait_pid = WAIT_SIGWAIT;
        f->rax = (u64)-MOS_EAGAIN;                       /* prazo vencido (TH_SLEEPING acorda sem mexer no rax) */
        if (c == ~0ULL) { t->wake_at = 0; t->state = TH_WAITING; }
        else { t->wake_at = timer_ticks() + c; t->state = TH_SLEEPING; }
        return sched_switch(f);
    }
    default: r = -MOS_EINVAL;
    }
    f->rax = (u64)r;
    return proc_signal_check(f);
}

/* falta em ring 3 que o programa trata: vira sinal da thread (devolve false se deve matar o processo) */
static bool sig_fault(int_frame_t *f, thread_t *t, process_t *p, u64 cr2) {
    int sig = 0, code = 0;
    u64 addr = cr2;
    switch (f->vector) {
    case 14:   /* como o macOS: ausente = SIGSEGV, proteção = SIGBUS. Fora da região de usuário a página pode estar
                * presente (mapa de identidade do kernel, sem o bit U), mas para o programa ela não existe: SIGSEGV */
        if ((f->error & 1) && cr2 >= USER_BASE && cr2 < USER_END) { sig = MOS_SIGBUS; code = MOS_BUS_ADRERR; } else { sig = MOS_SIGSEGV; code = MOS_SEGV_MAPERR; }
        /* diagnóstico: uma falta na área de mmap abaixo do topo é memória que já foi do processo (munmap, MAP_FIXED
         * PROT_NONE) ou ponteiro velho; o tratador do programa (Bun, crashpad) às vezes não diz o endereço */
        if (cr2 >= USER_MMAP_BASE && cr2 < p->mmap_next) {
            static int avisos;
            if (avisos < 10) {
                avisos++;
                kprintf("proc: %s (pid %d): falta em 0x%llx (rip 0x%llx, erro 0x%llx) na área de mmap, abaixo do topo 0x%llx\n",
                        p->name, p->pid, (unsigned long long)cr2, (unsigned long long)f->rip, (unsigned long long)f->error,
                        (unsigned long long)p->mmap_next);
                /* /Sistema/Biblioteca/despejar-falta: os registradores e 128 bytes em cada um que aponta para memória do
                 * processo (a estrutura da página do mimalloc, o bloco anterior da lista...), para ver de onde veio o ponteiro */
                static int despejar = -1;
                if (despejar < 0) despejar = vfs_lookup("/Sistema/Biblioteca/despejar-falta") != NULL;
                if (despejar && avisos <= 2) {
                    const char *nome[15] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8", "r9", "r10", "r11", "r12",
                                            "r13", "r14", "r15"};
                    u64 val[15] = {f->rax, f->rbx, f->rcx, f->rdx, f->rsi, f->rdi, f->rbp, f->r8, f->r9, f->r10, f->r11,
                                   f->r12, f->r13, f->r14, f->r15};
                    kprintf("falta: rsp 0x%llx\n", (unsigned long long)f->rsp);
                    for (int i = 0; i < 15; i++) {
                        kprintf("falta: %s = 0x%llx\n", nome[i], (unsigned long long)val[i]);
                        u64 a = val[i] & ~7ULL, q[16];
                        if (a < USER_BASE || a >= USER_END || !uvm_pte(p, a) || !uvm_copy_in(p, q, a, sizeof q)) continue;
                        for (int k = 0; k < 16; k += 4)
                            kprintf("falta:   [%s+%02x] %016llx %016llx %016llx %016llx\n", nome[i], k * 8, (unsigned long long)q[k],
                                    (unsigned long long)q[k + 1], (unsigned long long)q[k + 2], (unsigned long long)q[k + 3]);
                    }
                }
            }
        }
        break;
    case 13:
        /* int3 (e int n) em ring 3 chega como #GP com erro = (vetor << 3) | 2, porque as portas têm DPL 0 */
        if (f->error == ((3 << 3) | 2)) { sig = MOS_SIGTRAP; code = MOS_TRAP_BRKPT; addr = f->rip; f->rip += 1; break; }
        sig = MOS_SIGSEGV; code = 0; addr = 0;           /* EXC_I386_GPFLT: endereço não canônico, instrução privilegiada */
        break;
    case 0: sig = MOS_SIGFPE; code = MOS_FPE_INTDIV; addr = f->rip; break;
    case 16: case 19: sig = MOS_SIGFPE; code = MOS_FPE_FLTINV; addr = f->rip; break;
    case 6: sig = MOS_SIGILL; code = MOS_ILL_ILLOPC; addr = f->rip; break;
    case 17: sig = MOS_SIGBUS; code = MOS_BUS_ADRALN; break;
    case 1: case 3: sig = MOS_SIGTRAP; code = MOS_TRAP_BRKPT; addr = f->rip; break;
    default: return false;
    }
    /* bloqueado, ignorado ou sem tratador: o macOS mata (repetir a instrução daria a mesma falta para sempre) */
    if (p->sig_handler[sig] <= MOS_SIG_IGN || (t->sig_mask & SIG_BIT(sig))) {
        if (f->vector == 13 && sig == MOS_SIGTRAP) f->rip -= 1;
        return false;
    }
    /* CHECK do Chromium é ud2. O crashpad instala tratador e sai em silêncio; sem esta linha
     * o renderer do VS Code desaparece do dump sem "processo encerrado". */
    if (f->vector == 6) {
        /* os bytes da instrução e o estado do AVX (CR4.OSXSAVE, XCR0): c4/c5 é um prefixo VEX */
        u8 ib[6] = {0};
        bool got = uvm_copy_in(p, ib, f->rip, sizeof ib);
        u64 cr4, xcr0 = 0;
        __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
        if (cr4 & (1ULL << 18)) { u32 lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); xcr0 = (u64)hi << 32 | lo; }
        kprintf("proc: %s (pid %d): instrução inválida em 0x%llx (entregue ao tratador)%s%02x %02x %02x %02x %02x %02x; OSXSAVE %d, XCR0 0x%llx\n",
                p->name, p->pid, f->rip, got ? ": " : " (sem bytes) ", ib[0], ib[1], ib[2], ib[3], ib[4], ib[5],
                (int)((cr4 >> 18) & 1), (unsigned long long)xcr0);
    }
    t->sig_pending |= SIG_BIT(sig);
    t->sig_info[sig].code = code; t->sig_info[sig].pid = 0; t->sig_info[sig].status = 0; t->sig_info[sig].addr = addr;
    t->sig_trapno = (u16)f->vector; t->sig_err = (u32)f->error;
    return true;
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

/* referências de um descritor de pipe: FD_PIPE_R lê do .pipe, FD_PIPE_W escreve nele; FD_PIPE_RW (socketpair) lê
 * do .pipe e escreve no .pipe2. Soltar a última referência das duas pontas libera o pipe. */
static void pipe_end_drop(pipe_t *pp, bool reader) {
    if (!pp) return;
    if (reader) pp->readers--; else pp->writers--;
    pipe_service(pp);
    if (pp->readers <= 0 && pp->writers <= 0) kfree(pp);
}
static void pipe_refs(pipe_t *pp, pipe_t *pp2, u8 kind, int d) {
    if (pp) { if (kind == FD_PIPE_R || kind == FD_PIPE_RW) pp->readers += d; else pp->writers += d; }
    if (pp2) pp2->writers += d;
}
static void pipe_refs_drop(pipe_t *pp, pipe_t *pp2, u8 kind) {
    pipe_end_drop(pp, kind == FD_PIPE_R || kind == FD_PIPE_RW);
    pipe_end_drop(pp2, false);
}

static void fd_close(process_t *p, int fd) {
    if (!p->fds[fd].used) return;
    pipe_t *pp = p->fds[fd].pipe, *pp2 = p->fds[fd].pipe2;
    struct pty *ty = p->fds[fd].pty;
    bool tm = p->fds[fd].kind == FD_PTY_M;
    if (pp || pp2) pipe_refs_drop(pp, pp2, p->fds[fd].kind);
    p->fds[fd].used = false;
    p->fds[fd].pipe = NULL;
    p->fds[fd].pipe2 = NULL;
    p->fds[fd].pty = NULL;
    if (ty) pty_fd_ref(ty, tm, -1);                   /* depois de soltar o fd: o par pode ser liberado aqui */
}

static void fd_copy(process_t *dst, int dfd, process_t *src, int sfd) {
    /* a referência nova entra antes de fechar o destino: se os dois são do mesmo pipe e o destino era a
     * última referência, fechar primeiro liberaria o pipe que a cópia ainda vai usar (kfree duplo depois) */
    __typeof__(src->fds[0]) e = src->fds[sfd];
    pipe_refs(e.pipe, e.pipe2, e.kind, +1);
    if (e.pty) pty_fd_ref(e.pty, e.kind == FD_PTY_M, +1);
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
    pipe_refs(s->pipe, s->pipe2, s->kind, +1);
    if (s->pty) pty_fd_ref(s->pty, s->kind == FD_PTY_M, +1);
    return s;
}
int proc_fd_from_snapshot(process_t *p, const void *snap) {
    const fdent_t *s = snap;
    for (int fd = 3; fd < PROC_MAX_FDS; fd++) {
        if (p->fds[fd].used) continue;
        p->fds[fd] = *s;
        p->fds[fd].used = true;
        pipe_refs(s->pipe, s->pipe2, s->kind, +1);
        if (s->pty) pty_fd_ref(s->pty, s->kind == FD_PTY_M, +1);
        return fd;
    }
    return -MOS_EMFILE;
}
void proc_fd_snapshot_free(void *snap) {
    fdent_t *s = snap;
    if (!s) return;
    if (s->pipe || s->pipe2) pipe_refs_drop(s->pipe, s->pipe2, s->kind);
    if (s->pty) pty_fd_ref(s->pty, s->kind == FD_PTY_M, -1);
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

/* Escrita em arquivo no lugar, no buffer do próprio vnode. Antes cada write/pwrite montava um buffer do tamanho do arquivo
 * inteiro (kcalloc), copiava o arquivo, aplicava a escrita e regravava tudo com vfs_write_file (mais uma cópia, e o cap
 * dobrava): o SQLite do perfil do Chrome e o LevelDB do Claude gravam uma página por vez, e cada uma pedia blocos
 * contíguos de vários MB ("kmalloc: sem memória para 14893184 bytes" com a VM cheia e fragmentada) e copiava o arquivo
 * duas vezes. Agora só se aloca quando o arquivo cresce além do buffer (geométrico, como o vfs_append) ou quando o buffer
 * não pode ser escrito no lugar. */

/* Espaço para nsz bytes (mais o 0 final) num buffer do vnode gravável no lugar; guarda os primeiros MIN(size, nsz) bytes
 * (quem chama acerta o size). Troca de buffer quando não cabe ou quando ele não é só do VFS: cap 0 = dados do pendrive;
 * kmalloc_pages = páginas que o macho.c (__TEXT) e o lazy.c (mmap) mapeiam direto nos processos, onde nada se escreve no
 * lugar (o vfs_append faz o mesmo). Folga de até 16 MiB por crescimento; encolher muito devolve a memória. */
static bool file_room(vnode_t *v, u64 nsz) {
    bool lent = v->cap && kmalloc_is_pages(v->data);
    bool shrink = v->cap > (1u << 20) && nsz + 1 < v->cap / 4;     /* journal do SQLite zerado, log reescrito... */
    if (v->cap && !lent && !shrink && nsz + 1 <= v->cap) return true;
    u64 nc = nsz + 256;
    if (v->cap && !lent && !shrink) nc = MAX(nc, MIN((u64)v->cap * 2, nsz + (16ull << 20)));
    u8 *nd = kmalloc(nc);
    if (!nd) return false;
    u64 keep = MIN((u64)v->size, nsz);
    if (keep && v->data) memcpy(nd, v->data, keep);
    else if (keep) memset(nd, 0, keep);
    nd[keep] = 0;
    if (v->cap) kfree(v->data);
    v->data = nd;
    v->cap = nc;
    return true;
}

/* rename: os fds de arquivo guardam o caminho em texto. Como no POSIX (o fd segue o arquivo), quem tinha aberto o
 * caminho renomeado, ou algo dentro da pasta renomeada, passa a apontar para o nome novo. O Zig escreve a saída em
 * cache/tmp/<n>/, renomeia a pasta para cache/o/<hash>/ e ainda faz ftruncate no arquivo aberto: dava ENOENT. */
static void rename_fix_fds(const char *from, const char *to) {
    usize fl = strlen(from), tl = strlen(to);
    u64 irq = irq_save();
    for (process_t *q = procs; q; q = q->next) {
        if (q->exited) continue;
        for (int i = 0; i < PROC_MAX_FDS; i++) {
            if (!q->fds[i].used || q->fds[i].kind != FD_FILE || q->fds[i].pipe || q->fds[i].pty) continue;
            char *fp = q->fds[i].path;
            if (strncmp(fp, from, fl) || (fp[fl] && fp[fl] != '/')) continue;
            usize rest = strlen(fp + fl);
            if (tl + rest >= sizeof q->fds[i].path) continue;   /* não cabe: fica com o nome antigo */
            memmove(fp + tl, fp + fl, rest + 1);
            memcpy(fp, to, tl);
        }
    }
    irq_restore(irq);
}

/* ftruncate: o SQLite (journal TRUNCATE, encolher o banco) precisa; sem isso o commit do perfil do Chrome falhava */
static i64 file_truncate(const char *path, u64 nsz) {
    vnode_t *v = vfs_lookup(path);
    if (!v) return -MOS_ENOENT;
    if (v->dir) return -MOS_EISDIR;
    if (nsz == v->size) return 0;
    if (nsz > (256ull << 20)) return -MOS_ENOSPC;
    if (vfs_is_read_only(v)) return -MOS_ENOENT;
    u64 old = v->size;
    if (!file_room(v, nsz)) return -MOS_ENOMEM;
    if (nsz > old) memset(v->data + old, 0, nsz - old);
    v->size = nsz;
    v->data[nsz] = 0;
    v->mtime = timer_ticks();
    persist_mark(v);
    return 0;
}

/* grava len bytes do processo (ubuf) na posição off, direto no buffer do arquivo (sem cópia intermediária) */
static i64 file_write_at(process_t *p, const char *path, u64 off, u64 ubuf, u64 len) {
    vnode_t *v = vfs_lookup(path);
    if (v && v->dir) return -MOS_EISDIR;
    if (!v && !(v = vfs_create(path))) return -MOS_ENOENT;     /* arquivo novo: cria as pastas que faltam, como antes */
    if (vfs_is_read_only(v)) return -MOS_ENOENT;
    u64 old = v->size, end = off + len;
    if (end < off) return -MOS_EINVAL;
    u64 nsz = MAX(old, end);
    if (nsz > (256ull << 20)) return -MOS_ENOSPC;
    if (!file_room(v, nsz)) return -MOS_ENOMEM;
    if (off > old) memset(v->data + old, 0, off - old);        /* buraco depois do fim lê como zeros */
    if (len && !uvm_copy_in(p, v->data + off, ubuf, len)) { v->data[v->size] = 0; return -MOS_EFAULT; }
    v->size = nsz;
    v->data[nsz] = 0;
    v->mtime = timer_ticks();
    persist_mark(v);
    return (i64)len;
}

/* o mesmo que file_write_at, com a origem já no kernel (página física do MAP_SHARED). Não cria arquivo. */
i64 file_store_at(const char *path, u64 off, const void *src, u64 len) {
    if (!path || (!src && len)) return -MOS_EINVAL;
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir) return -MOS_ENOENT;
    if (vfs_is_read_only(v)) return -MOS_ENOENT;
    u64 end = off + len;
    if (end < off) return -MOS_EINVAL;
    u64 old = v->size, nsz = MAX(old, end);
    if (nsz > (256ull << 20)) return -MOS_ENOSPC;
    if (!file_room(v, nsz)) return -MOS_ENOMEM;
    if (off > old) memset(v->data + old, 0, off - old);
    if (len) memcpy(v->data + off, src, len);
    v->size = nsz;
    v->data[nsz] = 0;
    v->mtime = timer_ticks();
    persist_mark(v);
    return (i64)len;
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

/* diagnóstico: com /Sistema/Biblioteca/rastrear-munmap na imagem, o SYS_MMAP e o SYS_MUNMAP registram cada faixa no log
 * serial ("mm: ..."), para casar uma "falta em" na área de mmap com quem criou e quem tirou a faixa */
static bool rastrear_mm(void) {
    static int on = -1;
    if (on < 0) on = vfs_lookup("/Sistema/Biblioteca/rastrear-munmap") != NULL;
    return on > 0;
}

static i64 sys_write(process_t *p, int fd, u64 buf, u64 len) {
    if (fd_get(p, fd) >= 0) {
        if (!(p->fds[fd].flags & (MOS_O_WRONLY | MOS_O_RDWR))) return -MOS_EBADF;
        if (is_devnull(p->fds[fd].path)) return (i64)len;
        if (p->fds[fd].flags & MOS_O_APPEND) { vnode_t *v = vfs_lookup(p->fds[fd].path); p->fds[fd].off = v ? v->size : 0; }
        i64 r = file_write_at(p, p->fds[fd].path, p->fds[fd].off, buf, len);
        if (r > 0) p->fds[fd].off += (u64)r;
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
 * Cada string cabe em 64 KiB; o conjunto, em 384 KiB; até 4096 entradas. O Electron passa bem mais
 * que os 256 bytes do SYS_SPAWN antigo, e o `zig cc` chama `zig clang -cc1` com mais de 384 argumentos.
 * Cada string é lida num rascunho de 64 KiB e guardada no tamanho exato (4096 × 64 KiB não caberia). */
static char **copy_user_vec(process_t *parent, u64 uvec, int *out_n) {
    enum { MAXN = 4096, STR = 65536, BUDGET = 384 << 10 };
    char **v = kcalloc(MAXN, sizeof(char *));
    char *tmp = kmalloc(STR);
    if (!v || !tmp) { kfree(v); kfree(tmp); return NULL; }
    int n = 0;
    usize used = 0;
    for (;;) {
        u64 ptr = 0;
        if (!uvm_copy_in(parent, &ptr, uvec + (u64)n * 8, 8)) { free_vec(v, n); v = NULL; break; }
        if (!ptr) break;
        if (n >= MAXN - 1) { kprintf("proc: spawn: argumentos demais\n"); free_vec(v, n); v = NULL; break; }
        if (!copy_str_in(parent, tmp, ptr, STR) || strlen(tmp) == STR - 1) {
            kprintf("proc: spawn: argumento longo demais\n");
            free_vec(v, n); v = NULL; break;
        }
        usize len = strlen(tmp);
        if (used + len + 1 > BUDGET) { kprintf("proc: spawn: linha de comando grande demais\n"); free_vec(v, n); v = NULL; break; }
        char *s = kmalloc(len + 1);
        if (!s) { free_vec(v, n); v = NULL; break; }
        memcpy(s, tmp, len + 1);
        used += len + 1;
        v[n++] = s;
    }
    kfree(tmp);
    if (v) *out_n = n;
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
    jitshare_map(np);

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
    { extern void smp_kick_proc(process_t *p); smp_kick_proc(p); }   /* idem nos outros núcleos (proc/sched.c) */
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
    for (int s = 0; s < 32; s++) { p->sig_tramp[s] = 0; p->sig_flags[s] = 0; p->sig_amask[s] = 0; }
    t->ss_sp = 0; t->ss_size = 0;                      /* a máscara da thread continua (POSIX) */
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
    u8 *fx = FPU_AREA_OF(f);                           /* FPU/SSE/AVX limpos, como numa thread nova */
    memset(fx, 0, FPU_AREA);
    fx[0] = 0x7F; fx[1] = 0x03;
    fx[24] = 0x80; fx[25] = 0x1F;
    kprintf("proc: execve pid %d -> %s (%llu KiB)\n", p->pid, p->name, p->mem_pages * 4);
    return 0;
}

static u64 proc_syscall_(int_frame_t *f);
/* a syscall pode bloquear e trocar de thread: a thread que volta ao ring 3 (talvez acordada por um sinal, com
 * rax = -EINTR) passa pela entrega de sinais, que também decide se a syscall interrompida é refeita */
/* diagnóstico (marcador /Sistema/Biblioteca/rastrear-arquivos-zig): syscalls de escrita em arquivo dos processos "zig",
 * com o caminho do fd e o resultado. Foi assim que se achou onde o `zig cc -c` perdia o conteúdo do objeto. */
static void trace_zig_fs(process_t *p, u64 nr, u64 a, u64 b, u64 c, i64 res) {
    if (nr == SYS_WRITE || nr == SYS_PWRITE || nr == SYS_FTRUNCATE || nr == SYS_CLOSE || nr == SYS_LSEEK) {
        if (nr == SYS_WRITE && a <= 2) return;
        const char *fp = a < PROC_MAX_FDS && p->fds[a].used ? p->fds[a].path : "?";
        kprintf("zigfs %d: sys %llu fd %llu (%s) b=%llx c=%llx -> %lld\n", p->pid, nr, a, fp, b, c, res);
    } else if (nr == SYS_OPEN || nr == SYS_RENAME) {
        char s[160] = "?";
        copy_str_in(p, s, a, sizeof s);
        if (nr == SYS_OPEN && !(b & (MOS_O_WRONLY | MOS_O_RDWR | MOS_O_CREAT))) return;   /* leituras de headers: muitas */
        kprintf("zigfs %d: sys %llu %s b=%llx -> %lld\n", p->pid, nr, s, b, res);
    }
}
u64 proc_syscall(int_frame_t *f) {
    static int trace_zig = -1;
    if (trace_zig < 0) trace_zig = vfs_lookup("/Sistema/Biblioteca/rastrear-arquivos-zig") != NULL;
    process_t *tp = trace_zig && thread_current() ? thread_current()->proc : NULL;
    bool tr = tp && !strcmp(tp->name, "zig") && (f->cs & 3) == 3;
    u64 nr = f->rax, a = f->rdi, b = f->rsi, c = f->rdx;
    u64 r = proc_syscall_(f);
    if (tr) trace_zig_fs(tp, nr, a, b, c, (i64)f->rax);
    return ((int_frame_t *)r)->cs & 3 ? proc_signal_check((int_frame_t *)r) : r;
}
static u64 proc_syscall_(int_frame_t *f) {
    thread_t *t = thread_current();
    process_t *p = t->proc;
    if (!p || (f->cs & 3) != 3) { f->rax = (u64)-MOS_EINVAL; return (u64)f; }
    t->sys_nr = f->rax;          /* SA_RESTART refaz a syscall com este número */
    t->sig_intr = false;
    t->pty_wait = NULL;          /* uma espera num terminal (pty.c) nunca passa da syscall em que começou */
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
        if (fd_get(p, a) >= 0 && p->fds[a].pty) return pty_rw(f, t, p, (int)a, false, b, c);   /* terminal (pty.c) */
        if (fd_get(p, a) >= 0 && p->fds[a].kind == FD_PIPE_RW) return pipe_op(f, t, p->fds[a].pipe2, false, b, c);   /* socketpair */
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
        int opt = (int)c;
        bool any = false;
        process_t *hit = NULL;
        int st = 0, kind = 0;                          /* 1 saída, 2 parada, 3 continuação */
        r = 0;
        for (process_t *q = procs; q; q = q->next) {
            if (q->ppid != p->pid || (want > 0 && q->pid != want)) continue;
            any = true;
            if (q->exited) { hit = q; kind = 1; st = posix_status(q->exit_code); break; }
        }
        if (!hit && (opt & MOS_WUNTRACED))
            for (process_t *q = procs; q; q = q->next) {
                if (q->ppid != p->pid || (want > 0 && q->pid != want) || !q->stopped || !q->stop_unreported) continue;
                hit = q; kind = 2; st = stop_status(q->stopped); break;
            }
        if (!hit && (opt & MOS_WCONTINUED))
            for (process_t *q = procs; q; q = q->next) {
                if (q->ppid != p->pid || (want > 0 && q->pid != want) || q->exited || q->stopped || !q->cont_unreported) continue;
                hit = q; kind = 3; st = stop_status(MOS_SIGCONT); break;
            }
        if (hit) {
            if (b) uvm_copy_out(p, b, &st, sizeof st);
            if (kind == 1) hit->ppid = -1;             /* recolhido: trim_zombies libera depois */
            else if (kind == 2) hit->stop_unreported = 0;
            else hit->cont_unreported = 0;
            r = hit->pid;
            break;
        }
        if (!any) { r = -MOS_ECHILD; break; }
        if (opt & MOS_WNOHANG) { r = 0; break; }
        t->wait_pid = want > 0 ? want : -1;
        t->wait_status = b;
        t->wait_posix = true;
        t->wait_opt = opt;
        t->state = TH_WAITING;
        return sched_switch(f);
    }
    case SYS_WAIT: {
        process_t *q = find((int)a);
        if (!q || q == p) { r = -MOS_ESRCH; break; }
        if (q->exited) {
            r = q->exit_code;
            if (!q->main) unlink_proc(q);
            else q->ppid = -1;              /* já informado (SMP: a thread morta ainda não foi recolhida); não volta no waitpid(-1) */
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
        {   /* /dev/fd/N e /dev/std{in,out,err}: um dup do descritor (o bash usa /dev/fd/63 em "< <(comando)") */
            int src = -1;
            if (!strncmp(path, "/dev/fd/", 8) && path[8] >= '0' && path[8] <= '9') { src = 0; for (const char *q = path + 8; *q >= '0' && *q <= '9'; q++) src = src * 10 + (*q - '0'); }
            else if (!strcmp(path, "/dev/stdin")) src = 0;
            else if (!strcmp(path, "/dev/stdout")) src = 1;
            else if (!strcmp(path, "/dev/stderr")) src = 2;
            if (src >= 0) {
                if (src >= PROC_MAX_FDS || !p->fds[src].used) { r = -MOS_ENOENT; break; }   /* 0-2 do console: sem entrada */
                if (fd < 0) { r = -MOS_EMFILE; break; }
                fd_copy(p, fd, p, src);
                p->fds[fd].flags &= ~MOS_O_CLOEXEC;
                r = fd;
                break;
            }
        }
        /* diagnóstico (com despejar-threads): open que falha nos helpers do VS Code vai para o log serial */
        static int open_diag = -1;
        if (open_diag < 0) open_diag = vfs_lookup("/Sistema/Biblioteca/despejar-threads") != NULL;
        bool diag = open_diag && !strncmp(p->name, "Code Helper", 11);
        /* caches em disco do Chromium (Cache_Data, GPUCache, Code Cache): "Unable to create cache" sem outra pista */
        bool cdiag = open_diag && (strstr(path, "Cache") || strstr(path, "cache") ||
                                   (!strncmp(p->name, "Code", 4) && !p->name[4] && strstr(path, ".asar")));   /* AsarURLLoader */
        /* EMFILE vai sempre para o log (raro e sério: o processo principal do VS Code abre dezenas de arquivos) */
        if (fd < 0) { kprintf("open[diag]: %s (pid %d): EMFILE %s\n", p->name, p->pid, path); r = -MOS_EMFILE; break; }
        {   /* terminais (pty.c): /dev/ptmx cria um par, /dev/ttysNNN abre o escravo, /dev/tty o terminal de controle */
            int pr = pty_open_path(p, fd, path, (int)b);
            if (pr <= 0) { r = pr < 0 ? pr : fd; break; }
        }
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
        p->fds[fd].pipe2 = NULL;
        p->fds[fd].pty = NULL;
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
        if (b == MOS_PIPE_SOCKPAIR) {
            /* socketpair (b = MOS_PIPE_SOCKPAIR): dois pipes cruzados, e cada ponta lê de um e escreve no outro. O libuv do Node e do
             * Electron liga o stdin/stdout dos filhos assim; com um pipe só (o de antes), o pai escrevia na ponta de
             * leitura e o filho via EOF no stdin (o "codex app-server" do Codex desktop saía na hora com código 0) */
            pipe_t *p2 = kcalloc(1, sizeof *p2);
            if (!p2) { kfree(pp); r = -MOS_ENOMEM; break; }
            p2->readers = p2->writers = 1;
            if (!uvm_copy_out(p, a, both, sizeof both)) { kfree(pp); kfree(p2); r = -MOS_EFAULT; break; }
            memset(&p->fds[rfd], 0, sizeof p->fds[rfd]);
            memset(&p->fds[wfd], 0, sizeof p->fds[wfd]);
            p->fds[rfd].used = p->fds[wfd].used = true;
            p->fds[rfd].kind = p->fds[wfd].kind = FD_PIPE_RW;
            p->fds[rfd].pipe = pp; p->fds[rfd].pipe2 = p2;   /* ponta 0: lê pp, escreve p2 */
            p->fds[wfd].pipe = p2; p->fds[wfd].pipe2 = pp;   /* ponta 1: lê p2, escreve pp */
            r = 0;
            break;
        }
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
        if (fd_get(p, a) >= 0 && b == MOS_DUP_ANY) {        /* dup()/F_DUPFD: o número sai daqui, junto com a cópia */
            if (c >= PROC_MAX_FDS) { r = -MOS_EINVAL; break; }
            int nfd = -1;
            for (int i = c > 3 ? (int)c : 3; i < PROC_MAX_FDS; i++) if (!p->fds[i].used) { nfd = i; break; }
            if (nfd < 0) { r = -MOS_EMFILE; break; }
            fd_copy(p, nfd, p, (int)a);
            p->fds[nfd].flags &= ~MOS_O_CLOEXEC;
            r = nfd;
            break;
        }
        if (fd_get(p, a) < 0 || b >= PROC_MAX_FDS) { r = -MOS_EBADF; break; }
        if (a != b) { fd_copy(p, (int)b, p, (int)a); p->fds[b].flags &= ~MOS_O_CLOEXEC; }   /* POSIX: a cópia nasce sem FD_CLOEXEC */
        r = (i64)b;
        break;
    case SYS_IMGATTACH: {                                   /* hdiutil attach (casks .dmg do Homebrew): monta pelo leitor do kernel */
        extern int vfs_mount_file(vnode_t *f);
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *img = vfs_lookup(path);
        if (!img || img->dir) { r = -MOS_ENOENT; break; }
        vnode_t *vols = vfs_lookup("/Volumes");
        vnode_t *had[64];
        int nhad = 0;
        for (vnode_t *ch = vols && vols->dir ? vols->child : NULL; ch && nhad < 64; ch = ch->next) had[nhad++] = ch;
        int n = vfs_mount_file(img);
        if (n <= 0) { r = -MOS_EINVAL; break; }             /* não é imagem conhecida, ou já montada */
        vols = vfs_lookup("/Volumes");
        char out[1024];
        usize o = 0;
        for (vnode_t *ch = vols ? vols->child : NULL; ch; ch = ch->next) {
            bool old = false;
            for (int i = 0; i < nhad; i++) if (had[i] == ch) old = true;
            if (old || o + strlen(ch->name) + 11 >= sizeof out) continue;
            o += (usize)snprintf(out + o, sizeof out - o, "/Volumes/%s\n", ch->name);
        }
        if (c && o) { usize k = o < c - 1 ? o : c - 1; if (!uvm_copy_out(p, b, out, k)) { r = -MOS_EFAULT; break; } u8 z = 0; uvm_copy_out(p, b + k, &z, 1); }
        r = n;
        break;
    }
    case SYS_IMGDETACH: {                                   /* hdiutil detach */
        if (!copy_path(p, path, a)) { r = -MOS_EFAULT; break; }
        vnode_t *vol = vfs_lookup(path);
        if (!vol) { r = -MOS_ENOENT; break; }
        if (!vfs_is_volume(vol)) { r = -MOS_EINVAL; break; }
        r = vfs_eject(vol) ? 0 : -MOS_EINVAL;
        break;
    }
    case SYS_FDFLAGS:                                       /* fcntl F_GETFD/F_SETFD: o git espera EOF no pipe de aviso do exec */
        if (fd_get(p, a) < 0) { r = -MOS_EBADF; break; }
        /* b = 2: só consulta o modo de acesso da abertura (O_RDONLY/O_WRONLY/O_RDWR), para o F_GETFL de fds que o
         * libSystem não abriu (fileport, herdados): o Chrome exige O_RDONLY nos arquivos que passa a outro processo */
        if (b == 2) { r = p->fds[a].flags & 3; break; }
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
        if (fd_get(p, a) >= 0 && p->fds[a].pty) return pty_rw(f, t, p, (int)a, true, b, c);    /* terminal (pty.c) */
        if (fd_get(p, a) >= 0 && p->fds[a].pipe) {
            if (p->fds[a].kind != FD_PIPE_R && p->fds[a].kind != FD_PIPE_RW) { r = -MOS_EBADF; break; }
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
        if (fd_get(p, a) < 0 || p->fds[a].pipe || p->fds[a].pty) { r = fd_get(p, a) < 0 ? -MOS_EBADF : -MOS_ESPIPE; break; }
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
        if (fd_get(p, a) < 0 || p->fds[a].pipe || p->fds[a].pty) { r = fd_get(p, a) < 0 ? -MOS_EBADF : -MOS_ESPIPE; break; }
        if (!(p->fds[a].flags & (MOS_O_WRONLY | MOS_O_RDWR))) { r = -MOS_EBADF; break; }
        if (is_devnull(p->fds[a].path)) { r = (i64)c; break; }
        r = file_write_at(p, p->fds[a].path, f->r10, b, c);
        break;
    }
    case SYS_FTRUNCATE: {
        if (fd_get(p, a) < 0 || p->fds[a].pipe || p->fds[a].pty) { r = fd_get(p, a) < 0 ? -MOS_EBADF : -MOS_EINVAL; break; }
        if (!(p->fds[a].flags & (MOS_O_WRONLY | MOS_O_RDWR))) { r = -MOS_EBADF; break; }
        if (is_devnull(p->fds[a].path)) { r = 0; break; }
        r = file_truncate(p->fds[a].path, b);
        if (r < 0) {
            char dir[256]; strncpy(dir, p->fds[a].path, sizeof dir - 1); dir[sizeof dir - 1] = 0;
            char *sl = strrchr(dir, '/'); if (sl) *sl = 0;
            kprintf("ftruncate: %s (pid %d): %s -> %llu bytes: erro %lld (pasta %s)\n", p->name, p->pid, p->fds[a].path, b, r,
                    vfs_lookup(dir[0] ? dir : "/") ? "existe" : "sumiu");
        }
        break;
    }
    case SYS_LSEEK: {
        if (fd_get(p, a) < 0) { r = -MOS_EBADF; break; }
        if (p->fds[a].pty) { r = -MOS_ESPIPE; break; }      /* terminal: sem posição */
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
        if (f->rax == SYS_FSTAT && fd_get(p, a) >= 0 && p->fds[a].pty) { r = pty_fstat(p, (int)a, b); break; }   /* is_dir 5 */
        if (f->rax == SYS_FSTAT && fd_get(p, a) >= 0 && p->fds[a].pipe) {
            pipe_t *pp = p->fds[a].pipe;
            bool rd = p->fds[a].kind == FD_PIPE_R || p->fds[a].kind == FD_PIPE_RW;   /* socketpair: como a ponta de leitura */
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
        if (f->rax == SYS_STAT && pty_stat_path(sp)) {      /* /dev/ptmx, /dev/tty, /dev/ttysNNN: terminal */
            mos_stat_t st = {0, 5, unix_now()};
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
        char to[256], to_full[256];
        if (!copy_path(p, path, a) || !copy_path(p, to, b)) { r = -MOS_EFAULT; break; }
        memcpy(to_full, to, sizeof to_full);
        vnode_t *v = vfs_lookup(path);
        char *slash = strrchr(to, '/');
        if (!v) { r = -MOS_ENOENT; break; }
        if (!slash || !slash[1]) { r = -MOS_EINVAL; break; }
        *slash = 0;
        vnode_t *np = vfs_lookup_dir(to[0] ? to : "/");   /* a pasta de destino pode ser um link (opt/<f> -> Cellar) */
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
        if (r == 0) rename_fix_fds(path, to_full);
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
            if (rastrear_mm()) kprintf("mm: %s pid %d tid %d mmap 0x%llx + 0x%llx\n", p->name, p->pid, t->id, (unsigned long long)at, (unsigned long long)len);
            break;
        }
        /* b = 4: arquivo sob demanda (kernel/mm/lazy.c). c = fd, r10 = deslocamento no arquivo (múltiplo de 4096).
         * Cada página é lida do arquivo na primeira falta e fica privada do processo (MAP_PRIVATE). Pipes, terminais
         * e /dev/null devolvem EINVAL: o libSystem cai para a cópia com read() */
        if (b == 4) {
            extern bool lazy_add_file(process_t *p, u64 va, u64 len, const char *path, u64 foff);
            u64 foff = f->r10;
            if (fd_get(p, c) < 0) { r = -MOS_EBADF; break; }
            if (p->fds[c].pipe || p->fds[c].pty || is_devnull(p->fds[c].path)) { r = -MOS_EINVAL; break; }
            if (p->fds[c].flags & MOS_O_WRONLY) { r = -13; break; }       /* EACCES */
            if (!len || len > (64ull << 30) || (foff & 0xFFF)) { r = -MOS_EINVAL; break; }
            vnode_t *v = vfs_lookup(p->fds[c].path);
            if (!v) { r = -MOS_ENOENT; break; }
            if (v->dir) { r = -MOS_EINVAL; break; }
            u64 at = p->mmap_next;
            if (at + len > USER_END) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            if (!lazy_add_file(p, at, len, p->fds[c].path, foff)) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            p->mmap_next += len + 4096;                     /* página de guarda entre regiões */
            r = (i64)at;
            if (rastrear_mm()) kprintf("mm: %s pid %d tid %d mmap 0x%llx + 0x%llx arquivo %s @0x%llx\n", p->name, p->pid, t->id,
                                       (unsigned long long)at, (unsigned long long)len, p->fds[c].path, (unsigned long long)foff);
            break;
        }
        /* b = 5: arquivo MAP_SHARED. c = fd, r10 = deslocamento, r8 = prot (1 leitura, 2 escrita, 4 execução).
         * b = 6: MAP_SHARED|MAP_ANON. A página física é a mesma para quem mapeou o objeto (kernel/mm/lazy.c).
         * Pipe, terminal e /dev/null devolvem EINVAL para o libSystem cair na cópia. */
        if (b == 5 || b == 6) {
            extern bool lazy_add_shared_file(process_t *p, u64 va, u64 len, const char *path, u64 foff, u8 prot);
            extern bool lazy_add_shared_anon(process_t *p, u64 va, u64 len, u8 prot);
            u8 prot = (u8)(f->r8 & 7);
            if (!prot) prot = 1;
            if (!len || len > (64ull << 30)) { r = -MOS_EINVAL; break; }
            u64 at = p->mmap_next;
            if (at + len > USER_END) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            if (b == 5) {
                u64 foff = f->r10;
                if (fd_get(p, c) < 0) { r = -MOS_EBADF; break; }
                if (p->fds[c].pipe || p->fds[c].pty || is_devnull(p->fds[c].path)) { r = -MOS_EINVAL; break; }
                if (p->fds[c].flags & MOS_O_WRONLY) { r = -MOS_EACCES; break; }
                if ((prot & 2) && !(p->fds[c].flags & (MOS_O_WRONLY | MOS_O_RDWR))) { r = -MOS_EACCES; break; }
                if (foff & 0xFFF) { r = -MOS_EINVAL; break; }
                vnode_t *v = vfs_lookup(p->fds[c].path);
                if (!v) { r = -MOS_ENOENT; break; }
                if (v->dir) { r = -MOS_EINVAL; break; }
                if (!lazy_add_shared_file(p, at, len, p->fds[c].path, foff, prot)) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            } else if (!lazy_add_shared_anon(p, at, len, prot)) { r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break; }
            p->mmap_next += len + 4096;
            r = (i64)at;
            break;
        }
        /* b = 8: MAP_FIXED de arquivo privado. b = 9: MAP_FIXED de arquivo compartilhado.
         * c = endereço, r10 = deslocamento, r8 = fd | (prot << 32). Substitui o que houver
         * em [c, c+len), como o mmap MAP_FIXED do Darwin (grava o MAP_SHARED antes).
         * Não recua mmap_next. Se a faixa passa do cursor, o cursor anda para depois dela
         * e da página de guarda, para o alocador sequencial não cair em cima. */
        if (b == 8 || b == 9) {
            extern bool lazy_add_file(process_t *p, u64 va, u64 len, const char *path, u64 foff);
            extern bool lazy_add_shared_file(process_t *p, u64 va, u64 len, const char *path, u64 foff, u8 prot);
            u64 at = c, foff = f->r10, end = at + len;
            int fd = (int)(u32)f->r8;
            u8 prot = (u8)((f->r8 >> 32) & 7);
            if (!prot) prot = 1;
            if (!len || len > (64ull << 30) || (at & 0xFFF) || (foff & 0xFFF) ||
                at < USER_BASE || end > USER_END || end < at) { r = -MOS_EINVAL; break; }
            if (fd_get(p, (u64)fd) < 0) { r = -MOS_EBADF; break; }
            if (p->fds[fd].pipe || p->fds[fd].pty || is_devnull(p->fds[fd].path)) { r = -MOS_EINVAL; break; }
            if (p->fds[fd].flags & MOS_O_WRONLY) { r = -MOS_EACCES; break; }
            if (b == 9 && (prot & 2) && !(p->fds[fd].flags & (MOS_O_WRONLY | MOS_O_RDWR))) { r = -MOS_EACCES; break; }
            vnode_t *v = vfs_lookup(p->fds[fd].path);
            if (!v) { r = -MOS_ENOENT; break; }
            if (v->dir) { r = -MOS_EINVAL; break; }
            mm_release_span(p, at, len);
            if (!(b == 8 ? lazy_add_file(p, at, len, p->fds[fd].path, foff)
                         : lazy_add_shared_file(p, at, len, p->fds[fd].path, foff, prot))) {
                r = -MOS_ENOMEM; MMAP_NOMEM_LOG(); break;
            }
            if (at >= USER_MMAP_BASE && end + 4096 > p->mmap_next) p->mmap_next = end + 4096;
            r = (i64)at;
            if (rastrear_mm()) kprintf("mm: %s pid %d tid %d mmap fixo 0x%llx + 0x%llx arquivo %s @0x%llx\n", p->name, p->pid, t->id,
                                       (unsigned long long)at, (unsigned long long)len, p->fds[fd].path, (unsigned long long)foff);
            break;
        }
        /* b = 7: prot (r8) nas PTEs de [c, c+len) e na faixa que a falta consulta. Sem o bit 4 a página fica NX.
         * Página que ainda não existe só fica registrada; a falta aplica o prot. PROT_NONE (0) não desmapeia. */
        if (b == 7) {
            extern bool lazy_protect(process_t *p, u64 va, u64 len, u8 prot);
            u64 at = c & ~0xFFFULL, end = (c + a + 0xFFF) & ~0xFFFULL;
            if (!a) { r = 0; break; }
            if (at < USER_BASE || end > USER_END || end < at) { r = -MOS_EINVAL; break; }
            r = lazy_protect(p, at, end - at, (u8)(f->r8 & 7)) ? 0 : -MOS_ENOMEM;
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
        if (rastrear_mm()) kprintf("mm: %s pid %d tid %d mmap 0x%llx + 0x%llx%s\n", p->name, p->pid, t->id, (unsigned long long)at,
                                   (unsigned long long)len, b == 1 ? " (só reserva)" : "");
        break;
    }
    case SYS_MUNMAP: {
        if (a < USER_MMAP_BASE || a & 0xFFF || a + b > USER_END) { r = -MOS_EINVAL; break; }
        /* diagnóstico (rastrear_mm): cada faixa que sai, para casar com uma "falta em" depois */
        if (rastrear_mm() && c != MOS_MUNMAP_DISCARD)
            kprintf("mm: %s pid %d tid %d munmap 0x%llx + 0x%llx%s (topo 0x%llx)\n", p->name, p->pid, t->id,
                    (unsigned long long)a, (unsigned long long)b, c == MOS_MUNMAP_KEEP ? " [MAP_FIXED]" : "",
                    (unsigned long long)p->mmap_next);
        /* MOS_MUNMAP_DISCARD (madvise MADV_DONTNEED/FREE/FREE_REUSABLE do libSystem): o conteúdo não importa mais.
         * Saem só as páginas privadas de faixas anônimas sob demanda; a faixa e a reserva ficam, e a página volta
         * zerada no próximo toque (lazy_fault), como o VM_SYNC_KILLPAGES do XNU. Compartilhadas (COW, memória Mach) e
         * de arquivo ficam. O mimalloc 3 do Bun conta com os zeros depois do purge: sem isto o vetor de índices de uma
         * PropertyTable do JavaScriptCore vinha sujo e o rehash procurava um vazio para sempre (claude -p). */
        /* MOS_MUNMAP_SYNC: msync. Grava o MAP_SHARED de arquivo no vnode e não desmapeia. */
        if (c == MOS_MUNMAP_SYNC) {
            extern void lazy_sync(process_t *p, u64 va, u64 len, int only_last);
            lazy_sync(p, a, b, 0);
            r = 0;
            break;
        }
        if (c == MOS_MUNMAP_DISCARD) {
            extern bool lazy_is_anon(process_t *p, u64 va);
            u64 gone[64];
            int ng = 0;
            for (u64 va = a; va < a + b; ) {
                u64 *e = walk(p, va, false);
                if (!e) { va = (va + 0x200000) & ~0x1FFFFFULL; continue; }
                u64 ph = *e & PTE_ADDR;
                if ((*e & PTE_P) && !(*e & PTE_COW) && page_refs(ph) <= 1 && lazy_is_anon(p, va)) {
                    *e = 0;
                    p->mem_pages--;
                    gone[ng++] = ph;
                    __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
                }
                va += 4096;
                if (ng == 64 || (ng && va >= a + b)) {
                    smp_tlb_shootdown((u64)p->pml4);
                    for (int k = 0; k < ng; k++) page_put(gone[k]);
                    ng = 0;
                }
            }
            r = 0;
            break;
        }
        { extern void lazy_sync(process_t *p, u64 va, u64 len, int only_last); lazy_sync(p, a, b, 0); }
        { extern void lazy_remove(process_t *p, u64 va, u64 len); lazy_remove(p, a, b); }
        /* as páginas só voltam ao pmm depois que nenhuma CPU as enxerga mais: em lotes de 64, um
         * smp_tlb_shootdown por lote (com threads do processo em outros núcleos) */
        u64 gone[64];
        int ng = 0;
        for (u64 va = a; va < a + b; ) {
            u64 *e = walk(p, va, false);
            /* sem tabela de páginas: os 2 MiB inteiros estão vazios (as reservas do V8 têm terabytes) */
            if (!e) { va = (va + 0x200000) & ~0x1FFFFFULL; continue; }
            if (*e & PTE_P) {
                gone[ng++] = *e & 0x000FFFFFFFFFF000ULL;
                p->mem_pages--;
                __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
            }
            *e = 0;
            va += 4096;
            if (ng == 64 || (ng && va >= a + b)) {
                smp_tlb_shootdown((u64)p->pml4);
                for (int k = 0; k < ng; k++) page_put(gone[k]);
                ng = 0;
            }
        }
        if (ng) { smp_tlb_shootdown((u64)p->pml4); for (int k = 0; k < ng; k++) page_put(gone[k]); }
        /* liberar o topo devolve os endereços: o V8 e o PartitionAlloc reservam, aparam e soltam regiões
         * de gigabytes em laço, e sem isso o espaço de usuário se esgotava em poucos segundos */
        {
            u64 end = (a + b + 0xFFF) & ~0xFFFULL;
            if (b && a < p->mmap_next && end + 4096 >= p->mmap_next) {
                /* MOS_MUNMAP_KEEP (mmap MAP_FIXED do libSystem): o trecho ainda é de quem chamou; o topo fica */
                if (c == MOS_MUNMAP_KEEP) {
                    static int avisos;
                    if (avisos < 20) {
                        avisos++;
                        kprintf("mmap: MAP_FIXED no topo (pid %d, 0x%llx + 0x%llx): a reserva continua do processo\n", p->pid,
                                (unsigned long long)a, (unsigned long long)b);
                    }
                } else p->mmap_next = a;
            }
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
        nt->sig_mask = t->sig_mask;                     /* pthread_create: a thread nova herda a máscara */
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
        jitshare_map(q);
        __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");   /* o pai agora é só leitura */
        smp_tlb_shootdown((u64)p->pml4);                /* também nas outras CPUs com threads do pai */
        q->pid = next_pid++;
        q->ppid = p->pid;
        memcpy(q->name, p->name, sizeof q->name);
        memcpy(q->path, p->path, sizeof q->path);
        memcpy(q->cwd, p->cwd, sizeof q->cwd);
        q->brk = p->brk; q->brk_start = p->brk_start; q->mmap_next = p->mmap_next;
        q->dyld = macho_dyld_clone(p->dyld);            /* imagens do dlopen: mesma memória, cópia do grafo */
        memcpy(q->sig_handler, p->sig_handler, sizeof q->sig_handler);
        q->sig_restorer = p->sig_restorer;
        memcpy(q->sig_tramp, p->sig_tramp, sizeof q->sig_tramp);
        memcpy(q->sig_flags, p->sig_flags, sizeof q->sig_flags);
        memcpy(q->sig_amask, p->sig_amask, sizeof q->sig_amask);
        q->start_ms = timer_ticks();
        q->tty = p->tty;
        if (q->tty) q->tty->refs++;
        q->pgid = p->pgid; q->sid = p->sid; q->ctty = p->ctty;   /* mesmo grupo, sessão e terminal de controle */
        machipc_spawn(p, q);                            /* como no macOS: o filho só herda as portas especiais */
        { extern void lazy_fork(process_t *from, process_t *to); lazy_fork(p, q); }   /* faixas sob demanda (mm/lazy.c) */
        for (int i = 0; i < PROC_MAX_FDS; i++) if (p->fds[i].used) fd_copy(q, i, p, i);
        f->rax = 0;                                     /* o filho vê 0 */
        thread_t *ct = uthread_clone(q, f);
        if (ct) { ct->sig_mask = t->sig_mask; ct->ss_sp = t->ss_sp; ct->ss_size = t->ss_size; }   /* máscara e sigaltstack passam ao filho */
        if (!ct) { for (int i = 0; i < PROC_MAX_FDS; i++) fd_close(q, i); free_address_space(q); if (q->tty) q->tty->refs--; kfree(q); r = -MOS_ENOMEM; break; }
        {   /* o filho herda o GS de usuário da thread que chamou fork (TLS: pthread_getspecific, thread_local);
             * com 0, o primeiro acesso a TLS no filho falhava (o ruby caía logo depois do fork) */
            u32 lo, hi;
            __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000101u));
            ct->gs_base = (u64)hi << 32 | lo;
        }
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
        if (a == 0 || a >= MOS_NSIG || a == MOS_SIGKILL || a == MOS_SIGSTOP) { r = -MOS_EINVAL; break; }
        if (b > MOS_SIG_IGN && (b < USER_BASE || b >= USER_END)) { r = -MOS_EFAULT; break; }
        r = (i64)p->sig_handler[a];
        p->sig_handler[a] = b;
        p->sig_tramp[a] = 0; p->sig_flags[a] = 0; p->sig_amask[a] = 0;   /* interface antiga (mos.h): chamada direta */
        if (c) p->sig_restorer = c;
        break;
    case SYS_SIGRETURN: return sig_return(f);
    case SYS_SIG: return sig_syscall(f, t, p);           /* ações, máscaras, sigsuspend, sigaltstack, sigwait */
    case SYS_WEB: { extern i64 web_user_syscall(process_t *p, u64 op, u64 id, u64 a, u64 b); r = web_user_syscall(p, a, b, c, f->r10); break; }   /* WKWebView (kernel/web/webview.c) */
    case SYS_THREADCTL: {   /* thread_suspend/resume/get_state do Mach (MOS_THR_*, syscall.h): o coletor do JavaScriptCore */
        extern void sched_kick_thread(thread_t *t);
        extern bool sched_thread_stopped(const thread_t *t);
        thread_t *q = NULL;
        if (a == MOS_THR_ROFILE) {
            extern int lazy_ro_file_at(process_t *p, u64 va, u64 *id, u64 *off);
            u64 id = 0, off = 0, pair[2];
            if (!lazy_ro_file_at(p, b, &id, &off)) { r = 0; break; }
            pair[0] = id; pair[1] = off;
            r = c && uvm_copy_out(p, c, pair, sizeof pair) ? 1 : -MOS_EFAULT;
            break;
        }
        for (thread_t *w = sched_threads(); w; w = w->next)
            if (w->proc == p && w->id == (int)b && w->state != TH_DEAD) { q = w; break; }
        if (!q && b == 0 && (a == MOS_THR_SETQOS || a == MOS_THR_AFFINITY)) q = t;
        if (!q) { r = -MOS_ESRCH; break; }
        {   /* diagnóstico: as primeiras chamadas (quem suspende quem), para ver no log que o coletor do JSC usa isto */
            static int vistos;
            if (a == MOS_THR_SUSPEND && vistos < 10) {
                vistos++;
                kprintf("threadctl: %s pid %d: thread %d suspende a %d (rodando: %s)\n", p->name, p->pid, t->id, q->id,
                        q->state == TH_RUNNING ? "sim" : "não");
            }
        }
        if (a == MOS_THR_SUSPEND) {
            if (q->suspend < 0xFFFF) q->suspend++;
            if (q == t) { f->rax = 0; return sched_switch(f); }   /* a própria thread: dorme até alguém dar resume */
            sched_kick_thread(q);                     /* rodando em outro núcleo: o IPI a tira de lá */
            r = 0;
        } else if (a == MOS_THR_RESUME) {
            if (!q->suspend) { r = -MOS_EINVAL; break; }
            q->suspend--;
            r = 0;
        } else if (a == MOS_THR_STOPPED) {
            r = sched_thread_stopped(q) ? 1 : 0;
            if (!r && q->suspend) sched_kick_thread(q);
        } else if (a == MOS_THR_GETSTATE) {
            if (q == t || !sched_thread_stopped(q) || !q->frame) { r = -MOS_EBUSY; break; }
            const int_frame_t *g = (const int_frame_t *)q->frame;
            u64 regs[21] = { g->rax, g->rbx, g->rcx, g->rdx, g->rdi, g->rsi, g->rbp, g->rsp, g->r8, g->r9, g->r10, g->r11,
                             g->r12, g->r13, g->r14, g->r15, g->rip, g->rflags, g->cs, 0, 0 };
            r = uvm_copy_out(p, c, regs, sizeof regs) ? 0 : -MOS_EFAULT;
        } else if (a == MOS_THR_SETQOS) {
            sched_set_qos(q, (unsigned)c);
            r = 0;
        } else if (a == MOS_THR_AFFINITY) {
            sched_set_affinity(q, c);
            r = 0;
        } else r = -MOS_EINVAL;
        break;
    }
    case SYS_SIGKILL: r = proc_signal((int)a, (int)b) ? 0 : -MOS_ESRCH; break;
    case SYS_TTY:                                         /* terminais, sessões e grupos (proc/pty.c) */
        r = pty_syscall(p, a, b, c, f->r10);
        if (p->exited) return sched_switch(f);            /* killpg do próprio grupo com SIGKILL */
        break;
    case SYS_DYLD: r = macho_dyld_syscall(p, a, b, c, f->r10); break;   /* dlopen/dlsym (kernel/compat/macho.c) */
    case SYS_LS: { extern i64 ls_syscall(process_t *p, u64 op, u64 ureq); r = ls_syscall(p, a, b); break; }   /* Launch Services (kernel/compat/launchsvc.c) */
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
    case SYS_DESKTOP: {
        extern i64 deskbridge_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d);
        r = deskbridge_syscall(p, a, b, c, f->r10);
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
        if (vfs_lookup("/Sistema/Programas/smpteste")) proc_spawn("/Sistema/Programas/smpteste", NULL);   /* SMP: carga paralela */
    }
}

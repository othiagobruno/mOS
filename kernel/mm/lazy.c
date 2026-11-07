/* mOS — memória anônima sob demanda (demand-zero) para os processos.
 *
 * No macOS, mmap(MAP_ANON, PROT_READ|PROT_WRITE) só reserva endereços: a página vira RAM na primeira vez que é
 * tocada. O JavaScriptCore (Bun do Claude Code) pede assim regiões de 1–4 GB e só usa uma fração; o SYS_MMAP do
 * mOS alocava tudo na hora (b = 0/2) e essas chamadas falhavam. Agora o SYS_MMAP b = 3 registra a faixa aqui, e a
 * falta de página (proc_fault) ou uma cópia do kernel para/da memória do processo (uvm_copy_out/in) dentro de
 * uma faixa registrada mapeia uma página zerada no lugar.
 *
 * As faixas são guardadas por pid numa tabela única (protegida por lazy_lock); o fork copia as do pai (o filho
 * herda as páginas já tocadas por COW e as não tocadas continuam sob demanda), e a saída do processo as apaga.
 * munmap tira a faixa (lazy_remove): um vm_deallocate seguido de mapeamento no mesmo endereço não pode
 * voltar a receber página zerada por cima da memória nova. */
#include "kernel.h"
#include "proc.h"

bool uvm_map(process_t *p, u64 va, u64 len);
u64 *uvm_pte(process_t *p, u64 va);

typedef struct { int pid; u64 start, end; } lazy_t;
static lazy_t *lz;
static u32 nlz, caplz;
static spinlock_t lazy_lock;

static bool grow(void) {
    u32 nc = caplz ? caplz * 2 : 64;
    lazy_t *n = kmalloc(nc * sizeof *n);
    if (!n) return false;
    if (lz) { memcpy(n, lz, nlz * sizeof *n); kfree(lz); }
    lz = n; caplz = nc;
    return true;
}

/* registra [va, va + len) do processo p como sob demanda; junta com uma faixa vizinha do mesmo processo */
bool lazy_add(process_t *p, u64 va, u64 len) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return false;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < nlz; i++)
        if (lz[i].pid == p->pid && s <= lz[i].end && e >= lz[i].start) {
            if (s < lz[i].start) lz[i].start = s;
            if (e > lz[i].end) lz[i].end = e;
            spin_unlock_irqrestore(&lazy_lock, fl);
            return true;
        }
    bool ok = nlz < caplz || grow();
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e};
    spin_unlock_irqrestore(&lazy_lock, fl);
    return ok;
}

static bool lazy_has(process_t *p, u64 va) {
    u64 fl = spin_lock_irqsave(&lazy_lock);
    bool r = false;
    for (u32 i = 0; i < nlz && !r; i++) r = lz[i].pid == p->pid && va >= lz[i].start && va < lz[i].end;
    spin_unlock_irqrestore(&lazy_lock, fl);
    return r;
}

/* a página de va falta: se estiver numa faixa sob demanda de p, mapeia uma página zerada (true = repetir) */
bool lazy_fault(process_t *p, u64 va) {
    if (!p || !lazy_has(p, va)) return false;
    if (uvm_pte(p, va)) return true;                       /* outra thread do processo já mapeou */
    return uvm_map(p, va & ~0xFFFULL, 4096);
}

void lazy_fork(process_t *from, process_t *to) {
    u64 fl = spin_lock_irqsave(&lazy_lock);
    u32 n = nlz;
    for (u32 i = 0; i < n; i++) {
        if (lz[i].pid != from->pid) continue;
        if (nlz == caplz && !grow()) break;
        lz[nlz++] = (lazy_t){to->pid, lz[i].start, lz[i].end};
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
}

/* recorta [va, va+len) das faixas sob demanda deste processo */
void lazy_remove(process_t *p, u64 va, u64 len) {
    if (!p || !len) return;
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < nlz;) {
        if (lz[i].pid != p->pid || e <= lz[i].start || s >= lz[i].end) { i++; continue; }
        u64 a = lz[i].start, b = lz[i].end;
        if (s <= a && e >= b) { lz[i] = lz[--nlz]; continue; }
        if (s <= a) { lz[i].start = e; i++; continue; }
        if (e >= b) { lz[i].end = s; i++; continue; }
        lz[i].end = s;
        if (nlz < caplz || grow()) lz[nlz++] = (lazy_t){p->pid, e, b};
        i++;
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
}

void lazy_exit(process_t *p) {
    u64 fl = spin_lock_irqsave(&lazy_lock);
    u32 o = 0;
    for (u32 i = 0; i < nlz; i++) if (lz[i].pid != p->pid) lz[o++] = lz[i];
    nlz = o;
    spin_unlock_irqrestore(&lazy_lock, fl);
}

/* mOS — memória sob demanda dos processos: anônima (demand-zero) e de arquivo.
 *
 * No macOS, mmap(MAP_ANON, PROT_READ|PROT_WRITE) só reserva endereços: a página vira RAM na primeira vez que é
 * tocada. O JavaScriptCore (Bun do Claude Code) pede assim regiões de 1–4 GB e só usa uma fração; o SYS_MMAP do
 * mOS alocava tudo na hora (b = 0/2) e essas chamadas falhavam. Agora o SYS_MMAP b = 3 registra a faixa aqui, e a
 * falta de página (proc_fault) ou uma cópia do kernel para/da memória do processo (uvm_copy_out/in) dentro de
 * uma faixa registrada mapeia uma página zerada no lugar.
 *
 * mmap de arquivo (SYS_MMAP b = 4, MAP_PRIVATE ou só leitura): a faixa guarda o caminho do arquivo e o
 * deslocamento; cada página é lida do arquivo na primeira falta (o resto depois do fim do arquivo fica zerado).
 * O Chromium/Electron mapeia icudtl.dat, os .pak e o snapshot do V8 (dezenas de MB) e lê só partes; antes o
 * libSystem copiava o arquivo inteiro na hora. A página lida é privada do processo (escrever nela não muda o
 * arquivo, como MAP_PRIVATE). Como no macOS, uma página ainda não tocada mostra o conteúdo atual do arquivo.
 *
 * As faixas são guardadas por pid numa tabela única (protegida por lazy_lock); o fork copia as do pai (o filho
 * herda as páginas já tocadas por COW e as não tocadas continuam sob demanda), e a saída do processo as apaga.
 * munmap tira a faixa (lazy_remove): um vm_deallocate seguido de mapeamento no mesmo endereço não pode
 * voltar a receber página zerada por cima da memória nova. */
#include "kernel.h"
#include "proc.h"
#include "vfs.h"

bool uvm_map(process_t *p, u64 va, u64 len);
u64 *uvm_pte(process_t *p, u64 va);
bool uvm_install(process_t *p, u64 va, u64 phys);

/* path = NULL: anônima (zeros); senão a página va vem do arquivo, no deslocamento foff + (va - start) */
typedef struct { int pid; u64 start, end; char *path; u64 foff; } lazy_t;
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

static char *dup_path(const char *s) {
    if (!s) return NULL;
    usize n = strlen(s) + 1;
    char *d = kmalloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* registra [va, va + len) do processo p como sob demanda; junta com uma faixa anônima vizinha do mesmo processo */
bool lazy_add(process_t *p, u64 va, u64 len) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return false;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < nlz; i++)
        if (lz[i].pid == p->pid && !lz[i].path && s <= lz[i].end && e >= lz[i].start) {
            if (s < lz[i].start) lz[i].start = s;
            if (e > lz[i].end) lz[i].end = e;
            spin_unlock_irqrestore(&lazy_lock, fl);
            return true;
        }
    bool ok = nlz < caplz || grow();
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e, NULL, 0};
    spin_unlock_irqrestore(&lazy_lock, fl);
    return ok;
}

/* [va, va + len) vem do arquivo path a partir de foff (múltiplo de 4096) */
bool lazy_add_file(process_t *p, u64 va, u64 len, const char *path, u64 foff) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s || !path) return false;
    char *pc = dup_path(path);
    if (!pc) return false;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    bool ok = nlz < caplz || grow();
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e, pc, foff};
    spin_unlock_irqrestore(&lazy_lock, fl);
    if (!ok) kfree(pc);
    return ok;
}

/* 0 = fora das faixas; 1 = anônima; 2 = de arquivo (caminho em path, deslocamento da página em *off).
 * Faixas de arquivo têm prioridade: o mprotect do libSystem registra uma faixa anônima por cima (b = 3) e
 * isso não pode trocar o conteúdo do arquivo por zeros. */
static int lazy_find(process_t *p, u64 va, char *path, usize cap, u64 *off) {
    int r = 0;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < nlz; i++) {
        if (lz[i].pid != p->pid || va < lz[i].start || va >= lz[i].end) continue;
        if (!lz[i].path) { r = 1; continue; }
        usize n = strlen(lz[i].path);
        if (n >= cap) n = cap - 1;
        memcpy(path, lz[i].path, n);
        path[n] = 0;
        *off = lz[i].foff + ((va & ~0xFFFULL) - lz[i].start);
        r = 2;
        break;
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
    return r;
}

/* a página de va falta: se estiver numa faixa sob demanda de p, mapeia a página (true = repetir) */
bool lazy_fault(process_t *p, u64 va) {
    if (!p) return false;
    char path[384];
    u64 off = 0;
    int kind = lazy_find(p, va, path, sizeof path, &off);
    if (!kind) return false;
    if (uvm_pte(p, va)) return true;                       /* outra thread do processo já mapeou */
    if (kind == 1) return uvm_map(p, va & ~0xFFFULL, 4096);
    u64 pg = pmm_alloc(1);
    if (!pg) return false;
    memset((void *)pg, 0, 4096);
    vnode_t *v = vfs_lookup(path);
    if (v && !v->dir && v->data && off < v->size) {
        u64 n = v->size - off;
        memcpy((void *)pg, v->data + off, n < 4096 ? n : 4096);
    }
    /* arquivo apagado ou página depois do fim: fica zerada (o macOS daria SIGBUS; zeros são mais tolerantes) */
    if (uvm_install(p, va, pg)) return true;
    pmm_free(pg, 1);
    return uvm_pte(p, va) != NULL;
}

void lazy_fork(process_t *from, process_t *to) {
    u64 fl = spin_lock_irqsave(&lazy_lock);
    u32 n = nlz;
    for (u32 i = 0; i < n; i++) {
        if (lz[i].pid != from->pid) continue;
        if (nlz == caplz && !grow()) break;
        char *pc = NULL;
        if (lz[i].path && !(pc = dup_path(lz[i].path))) break;
        lz[nlz++] = (lazy_t){to->pid, lz[i].start, lz[i].end, pc, lz[i].foff};
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
        if (s <= a && e >= b) { if (lz[i].path) kfree(lz[i].path); lz[i] = lz[--nlz]; continue; }
        if (s <= a) { lz[i].start = e; lz[i].foff += e - a; i++; continue; }
        if (e >= b) { lz[i].end = s; i++; continue; }
        /* buraco no meio: a ponta de cima vira outra faixa */
        lz[i].end = s;
        if (nlz < caplz || grow()) {
            char *pc = lz[i].path ? dup_path(lz[i].path) : NULL;
            if (!lz[i].path || pc) lz[nlz++] = (lazy_t){p->pid, e, b, pc, lz[i].foff + (e - a)};
        }
        i++;
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
}

void lazy_exit(process_t *p) {
    u64 fl = spin_lock_irqsave(&lazy_lock);
    u32 o = 0;
    for (u32 i = 0; i < nlz; i++) {
        if (lz[i].pid != p->pid) lz[o++] = lz[i];
        else if (lz[i].path) kfree(lz[i].path);
    }
    nlz = o;
    spin_unlock_irqrestore(&lazy_lock, fl);
}

/* mOS — memória sob demanda dos processos: anônima (demand-zero), de arquivo e compartilhada.
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
 * MAP_SHARED (b = 5 arquivo, b = 6 anônimo): a página física é a mesma em todo mundo que mapeou o objeto.
 * O arquivo não usa o buffer do vnode (vfs_append e file_room recusam escrita no lugar numa página de
 * kmalloc_pages). A primeira falta copia o arquivo para uma página do cache; as outras faltas mapeiam essa
 * página. msync, munmap e a saída do último mapeador gravam a página de volta (file_store_at).
 *
 * Proteção (b = 7): uma faixa à parte, por cima das de conteúdo. A falta aplica o prot depois de mapear, para
 * um mprotect pequeno não marcar NX uma região enorme que o lazy_add fundiu. PROT_NONE não entra aqui: no
 * guest arm64 quem desmapeia a página de 16 KiB é o libSystem (PR #11, a64_span).
 *
 * As faixas são guardadas por pid numa tabela única (protegida por lazy_lock); o fork copia as do pai (o filho
 * herda as páginas já tocadas por COW — ou compartilhadas, se forem MAP_SHARED — e as não tocadas continuam sob
 * demanda), e a saída do processo as apaga. munmap tira a faixa (lazy_remove): um vm_deallocate seguido de
 * mapeamento no mesmo endereço não pode voltar a receber página zerada por cima da memória nova. */
#include "kernel.h"
#include "proc.h"
#include "vfs.h"

bool uvm_map(process_t *p, u64 va, u64 len);
u64 *uvm_pte(process_t *p, u64 va);
bool uvm_install(process_t *p, u64 va, u64 phys);
bool uvm_map_cow(process_t *p, u64 va, u64 phys);
bool uvm_map_shared(process_t *p, u64 va, u64 phys, u8 prot);
bool uvm_protect(process_t *p, u64 va, u64 len, u8 prot);
i64 file_store_at(const char *path, u64 off, const void *src, u64 len);

#define LAZY_SHARED 1u

/* path = NULL: anônima (zeros); senão a página va vem do arquivo, no deslocamento foff + (va - start).
 * LAZY_SHARED: a página física é do cache (arquivo ou anônimo), não uma cópia privada. shid identifica o
 * objeto anônimo; foff é o deslocamento nele, e um recorte no início soma em foff (a página não troca de
 * número). prot 0 = a falta deixa a página RWX, como o mmap antigo. */
typedef struct {
    int pid;
    u64 start, end;
    char *path;
    u64 foff;
    u32 flags;
    u32 shid;
    u8 prot;
} lazy_t;
static lazy_t *lz;
static u32 nlz, caplz;
static spinlock_t lazy_lock;
static u32 next_shid = 1;

/* proteção pedida pelo mprotect / mmap, por cima do conteúdo. A última faixa que cobre a página vence. */
typedef struct { int pid; u64 start, end; u8 prot; } pov_t;
static pov_t *pv;
static u32 npv, cappv;

/* cache de MAP_SHARED. A página nasce com pref 0 (um dono: o cache). Cada PTE soma um dono com
 * page_ref_inc. O cache larga a sua referência quando nenhuma faixa ainda cobre o deslocamento. */
typedef struct { char *path; u64 off; u64 phys; } fcache_t;
typedef struct { u32 shid; u64 off; u64 phys; } scache_t;
static fcache_t *fc;
static scache_t *sc;
static u32 nfc, capfc, nsc, capsc;

static bool grow(void) {
    u32 nc = caplz ? caplz * 2 : 64;
    lazy_t *n = kmalloc(nc * sizeof *n);
    if (!n) return false;
    if (lz) { memcpy(n, lz, nlz * sizeof *n); kfree(lz); }
    lz = n; caplz = nc;
    return true;
}

static bool grow_pv(void) {
    u32 nc = cappv ? cappv * 2 : 64;
    pov_t *n = kmalloc(nc * sizeof *n);
    if (!n) return false;
    if (pv) { memcpy(n, pv, npv * sizeof *n); kfree(pv); }
    pv = n; cappv = nc;
    return true;
}

static char *dup_path(const char *s) {
    if (!s) return NULL;
    usize n = strlen(s) + 1;
    char *d = kmalloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static bool file_off_in(const lazy_t *z, u64 off) {
    return off >= z->foff && off < z->foff + (z->end - z->start);
}

/* registra [va, va + len) do processo p como sob demanda; junta com uma faixa anônima privada vizinha */
bool lazy_add(process_t *p, u64 va, u64 len) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return false;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < nlz; i++)
        if (lz[i].pid == p->pid && !lz[i].path && !(lz[i].flags & LAZY_SHARED) && s <= lz[i].end && e >= lz[i].start) {
            if (s < lz[i].start) lz[i].start = s;
            if (e > lz[i].end) lz[i].end = e;
            spin_unlock_irqrestore(&lazy_lock, fl);
            return true;
        }
    bool ok = nlz < caplz || grow();
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e, NULL, 0, 0, 0, 0};
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
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e, pc, foff, 0, 0, 0};
    spin_unlock_irqrestore(&lazy_lock, fl);
    if (!ok) kfree(pc);
    return ok;
}

bool lazy_add_shared_file(process_t *p, u64 va, u64 len, const char *path, u64 foff, u8 prot) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s || !path) return false;
    char *pc = dup_path(path);
    if (!pc) return false;
    if (!prot) prot = 1;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    bool ok = nlz < caplz || grow();
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e, pc, foff, LAZY_SHARED, 0, prot};
    spin_unlock_irqrestore(&lazy_lock, fl);
    if (!ok) kfree(pc);
    return ok;
}

bool lazy_add_shared_anon(process_t *p, u64 va, u64 len, u8 prot) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return false;
    if (!prot) prot = 1;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    bool ok = nlz < caplz || grow();
    u32 id = next_shid++;
    if (!id) id = next_shid++;
    if (ok) lz[nlz++] = (lazy_t){p->pid, s, e, NULL, 0, LAZY_SHARED, id, prot};
    spin_unlock_irqrestore(&lazy_lock, fl);
    return ok;
}

/* 0 = fora; 1 = anônima privada; 2 = arquivo privado; 3 = arquivo compartilhado; 4 = anônima compartilhada.
 * Compartilhada vence arquivo privado, que vence anônima: o mprotect registra uma faixa anônima por cima
 * (b = 3) e isso não pode trocar o arquivo nem o MAP_SHARED por zeros. */
typedef struct { int kind; char path[384]; u64 off; u32 shid; u8 prot; } lazy_hit_t;

static bool lazy_hit(process_t *p, u64 va, lazy_hit_t *h) {
    int best = 0;
    memset(h, 0, sizeof *h);
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < nlz; i++) {
        if (lz[i].pid != p->pid || va < lz[i].start || va >= lz[i].end) continue;
        int k = (lz[i].flags & LAZY_SHARED) ? (lz[i].path ? 3 : 4) : (lz[i].path ? 2 : 1);
        if (k < best) continue;
        best = k;
        h->kind = k;
        h->off = lz[i].foff + ((va & ~0xFFFULL) - lz[i].start);
        h->shid = lz[i].shid;
        h->prot = lz[i].prot;
        h->path[0] = 0;
        if (lz[i].path) {
            usize n = strlen(lz[i].path);
            if (n >= sizeof h->path) n = sizeof h->path - 1;
            memcpy(h->path, lz[i].path, n);
            h->path[n] = 0;
        }
        if (k >= 3) break;
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
    return best != 0;
}

/* va está numa faixa anônima privada (e em nenhuma de arquivo ou compartilhada)? O madvise
 * (MOS_MUNMAP_DISCARD) só descarta essas: a compartilhada não pode voltar zerada para o outro processo. */
bool lazy_is_anon(process_t *p, u64 va) {
    lazy_hit_t h;
    return p && lazy_hit(p, va, &h) && h.kind == 1;
}

static bool file_cached(const char *path, u64 off, u64 *phys) {
    for (u32 i = 0; i < nfc; i++)
        if (fc[i].off == off && !strcmp(fc[i].path, path)) { *phys = fc[i].phys; return true; }
    return false;
}

static bool anon_cached(u32 shid, u64 off, u64 *phys) {
    for (u32 i = 0; i < nsc; i++)
        if (sc[i].shid == shid && sc[i].off == off) { *phys = sc[i].phys; return true; }
    return false;
}

static u64 file_page(const char *path, u64 off) {
    u64 phys = 0;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    if (file_cached(path, off, &phys)) { spin_unlock_irqrestore(&lazy_lock, fl); return phys; }
    spin_unlock_irqrestore(&lazy_lock, fl);
    phys = pmm_alloc(1);
    if (!phys) return 0;
    memset((void *)phys, 0, 4096);
    vnode_t *v = vfs_lookup(path);
    if (v && !v->dir && v->data && off < v->size) {
        u64 n = v->size - off;
        memcpy((void *)phys, v->data + off, n < 4096 ? n : 4096);
    }
    char *pc = dup_path(path);
    if (!pc) { pmm_free(phys, 1); return 0; }
    fl = spin_lock_irqsave(&lazy_lock);
    u64 had = 0;
    if (file_cached(path, off, &had)) {
        spin_unlock_irqrestore(&lazy_lock, fl);
        kfree(pc);
        pmm_free(phys, 1);
        return had;
    }
    if (nfc == capfc) {
        u32 nc = capfc ? capfc * 2 : 32;
        fcache_t *n = kmalloc(nc * sizeof *n);
        if (!n) { spin_unlock_irqrestore(&lazy_lock, fl); kfree(pc); pmm_free(phys, 1); return 0; }
        if (fc) { memcpy(n, fc, nfc * sizeof *n); kfree(fc); }
        fc = n; capfc = nc;
    }
    fc[nfc++] = (fcache_t){pc, off, phys};
    spin_unlock_irqrestore(&lazy_lock, fl);
    return phys;
}

static u64 anon_page(u32 shid, u64 off) {
    u64 phys = 0;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    if (anon_cached(shid, off, &phys)) { spin_unlock_irqrestore(&lazy_lock, fl); return phys; }
    spin_unlock_irqrestore(&lazy_lock, fl);
    phys = pmm_alloc(1);
    if (!phys) return 0;
    memset((void *)phys, 0, 4096);
    fl = spin_lock_irqsave(&lazy_lock);
    u64 had = 0;
    if (anon_cached(shid, off, &had)) {
        spin_unlock_irqrestore(&lazy_lock, fl);
        pmm_free(phys, 1);
        return had;
    }
    if (nsc == capsc) {
        u32 nc = capsc ? capsc * 2 : 32;
        scache_t *n = kmalloc(nc * sizeof *n);
        if (!n) { spin_unlock_irqrestore(&lazy_lock, fl); pmm_free(phys, 1); return 0; }
        if (sc) { memcpy(n, sc, nsc * sizeof *n); kfree(sc); }
        sc = n; capsc = nc;
    }
    sc[nsc++] = (scache_t){shid, off, phys};
    spin_unlock_irqrestore(&lazy_lock, fl);
    return phys;
}

static bool overlay_prot(process_t *p, u64 va, u8 *prot) {
    bool hit = false;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    for (u32 i = 0; i < npv; i++)
        if (pv[i].pid == p->pid && va >= pv[i].start && va < pv[i].end) { *prot = pv[i].prot; hit = true; }
    spin_unlock_irqrestore(&lazy_lock, fl);
    return hit;
}

/* a página de va falta: se estiver numa faixa sob demanda de p, mapeia a página (true = repetir) */
bool lazy_fault(process_t *p, u64 va) {
    if (!p) return false;
    lazy_hit_t h;
    if (!lazy_hit(p, va, &h)) return false;
    if (uvm_pte(p, va)) return true;
    u64 page = va & ~0xFFFULL;
    bool ok = false;
    if (h.kind == 1) ok = uvm_map(p, page, 4096);
    else if (h.kind == 2) {
        vnode_t *v = vfs_lookup(h.path);
        if (v && !v->dir && v->cap && !(h.off & 0xFFF) && h.off + 4096 <= v->size && kmalloc_is_pages(v->data) &&
            uvm_map_cow(p, page, (u64)v->data + h.off))
            ok = true;
        else {
            u64 pg = pmm_alloc(1);
            if (!pg) return false;
            memset((void *)pg, 0, 4096);
            if (v && !v->dir && v->data && h.off < v->size) {
                u64 n = v->size - h.off;
                memcpy((void *)pg, v->data + h.off, n < 4096 ? n : 4096);
            }
            if (uvm_install(p, va, pg)) ok = true;
            else { pmm_free(pg, 1); ok = uvm_pte(p, va) != NULL; }
        }
    } else if (h.kind == 3 || h.kind == 4) {
        u64 phys = h.kind == 3 ? file_page(h.path, h.off) : anon_page(h.shid, h.off);
        if (!phys) return false;
        u8 prot = h.prot ? h.prot : 7;
        ok = uvm_map_shared(p, page, phys, prot) || uvm_pte(p, va) != NULL;
    }
    if (!ok) return false;
    u8 op = 0;
    if (overlay_prot(p, page, &op)) uvm_protect(p, page, 4096, op);
    return true;
}

static void copy_ranges(process_t *from, process_t *to) {
    u32 n = nlz;
    for (u32 i = 0; i < n; i++) {
        if (lz[i].pid != from->pid) continue;
        if (nlz == caplz && !grow()) break;
        char *pc = NULL;
        if (lz[i].path && !(pc = dup_path(lz[i].path))) break;
        lz[nlz++] = (lazy_t){to->pid, lz[i].start, lz[i].end, pc, lz[i].foff, lz[i].flags, lz[i].shid, lz[i].prot};
    }
    n = npv;
    for (u32 i = 0; i < n; i++) {
        if (pv[i].pid != from->pid) continue;
        if (npv == cappv && !grow_pv()) break;
        pv[npv++] = (pov_t){to->pid, pv[i].start, pv[i].end, pv[i].prot};
    }
}

void lazy_fork(process_t *from, process_t *to) {
    u64 fl = spin_lock_irqsave(&lazy_lock);
    copy_ranges(from, to);
    spin_unlock_irqrestore(&lazy_lock, fl);
}

/* recorta [s, e) de uma tabela de faixas {pid, start, end}. O miolo vira outra entrada. */
static void punch_lazy(process_t *p, u64 s, u64 e) {
    for (u32 i = 0; i < nlz;) {
        if (lz[i].pid != p->pid || e <= lz[i].start || s >= lz[i].end) { i++; continue; }
        u64 a = lz[i].start, b = lz[i].end;
        if (s <= a && e >= b) { if (lz[i].path) kfree(lz[i].path); lz[i] = lz[--nlz]; continue; }
        if (s <= a) { lz[i].start = e; lz[i].foff += e - a; i++; continue; }
        if (e >= b) { lz[i].end = s; i++; continue; }
        lz[i].end = s;
        if (nlz < caplz || grow()) {
            char *pc = lz[i].path ? dup_path(lz[i].path) : NULL;
            if (!lz[i].path || pc)
                lz[nlz++] = (lazy_t){p->pid, e, b, pc, lz[i].foff + (e - a), lz[i].flags, lz[i].shid, lz[i].prot};
        }
        i++;
    }
}

static void punch_prot(process_t *p, u64 s, u64 e) {
    for (u32 i = 0; i < npv;) {
        if (pv[i].pid != p->pid || e <= pv[i].start || s >= pv[i].end) { i++; continue; }
        u64 a = pv[i].start, b = pv[i].end;
        u8 prot = pv[i].prot;
        if (s <= a && e >= b) { pv[i] = pv[--npv]; continue; }
        if (s <= a) { pv[i].start = e; i++; continue; }
        if (e >= b) { pv[i].end = s; i++; continue; }
        pv[i].end = s;
        if (npv < cappv || grow_pv()) pv[npv++] = (pov_t){p->pid, e, b, prot};
        i++;
    }
}

static bool file_elsewhere(int pid, const char *path, u64 off) {
    for (u32 i = 0; i < nlz; i++) {
        if (lz[i].pid == pid || !(lz[i].flags & LAZY_SHARED) || !lz[i].path) continue;
        if (strcmp(lz[i].path, path)) continue;
        if (file_off_in(&lz[i], off)) return true;
    }
    return false;
}

static bool file_covered(const char *path, u64 off) {
    for (u32 i = 0; i < nlz; i++)
        if ((lz[i].flags & LAZY_SHARED) && lz[i].path && !strcmp(lz[i].path, path) && file_off_in(&lz[i], off))
            return true;
    return false;
}

static bool anon_covered(u32 shid, u64 off) {
    for (u32 i = 0; i < nlz; i++)
        if ((lz[i].flags & LAZY_SHARED) && !lz[i].path && lz[i].shid == shid && file_off_in(&lz[i], off))
            return true;
    return false;
}

/* larga a referência do cache das páginas que nenhuma faixa ainda cobre */
static void cache_reap(void) {
    for (u32 i = 0; i < nfc;) {
        if (file_covered(fc[i].path, fc[i].off)) { i++; continue; }
        u64 phys = fc[i].phys;
        kfree(fc[i].path);
        fc[i] = fc[--nfc];
        uvm_page_put(phys);
    }
    for (u32 i = 0; i < nsc;) {
        if (anon_covered(sc[i].shid, sc[i].off)) { i++; continue; }
        u64 phys = sc[i].phys;
        sc[i] = sc[--nsc];
        uvm_page_put(phys);
    }
}

/* grava no arquivo as páginas residentes de MAP_SHARED gravável que cruzam [va, va+len).
 * only_last: só as que nenhum outro processo ainda mapeia (saída do último). munmap e msync gravam já. */
void lazy_sync(process_t *p, u64 va, u64 len, int only_last) {
    if (!p || !len) return;
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return;
    typedef struct { char *path; u64 lo, hi, foff, base; } sy_t;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    u32 n = 0;
    for (u32 i = 0; i < nlz; i++)
        if (lz[i].pid == p->pid && (lz[i].flags & LAZY_SHARED) && lz[i].path && e > lz[i].start && s < lz[i].end) n++;
    sy_t *sy = n ? kmalloc(n * sizeof *sy) : NULL;
    u32 ns = 0;
    if (sy) {
        for (u32 i = 0; i < nlz && ns < n; i++) {
            if (lz[i].pid != p->pid || !(lz[i].flags & LAZY_SHARED) || !lz[i].path) continue;
            u64 lo = s > lz[i].start ? s : lz[i].start;
            u64 hi = e < lz[i].end ? e : lz[i].end;
            if (lo >= hi) continue;
            char *pc = dup_path(lz[i].path);
            if (!pc) continue;
            sy[ns++] = (sy_t){pc, lo, hi, lz[i].foff, lz[i].start};
        }
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
    for (u32 i = 0; i < ns; i++) {
        for (u64 a = sy[i].lo; a < sy[i].hi; a += 4096) {
            u64 off = sy[i].foff + (a - sy[i].base);
            if (only_last) {
                fl = spin_lock_irqsave(&lazy_lock);
                bool skip = file_elsewhere(p->pid, sy[i].path, off);
                spin_unlock_irqrestore(&lazy_lock, fl);
                if (skip) continue;
            }
            u64 *pte = uvm_pte(p, a);
            if (!pte) continue;
            u64 ent = *pte;
            if (!(ent & (1ULL << 11)) || !(ent & 2)) continue;   /* PTE_SHARED e PTE_RW, sem incluir process.c */
            file_store_at(sy[i].path, off, (void *)(ent & 0x000FFFFFFFFFF000ULL), 4096);
        }
        kfree(sy[i].path);
    }
    if (sy) kfree(sy);
}

/* recorta [va, va+len) das faixas sob demanda deste processo */
void lazy_remove(process_t *p, u64 va, u64 len) {
    if (!p || !len) return;
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (e <= s) return;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    punch_lazy(p, s, e);
    punch_prot(p, s, e);
    cache_reap();
    spin_unlock_irqrestore(&lazy_lock, fl);
}

bool lazy_protect(process_t *p, u64 va, u64 len, u8 prot) {
    u64 s = va & ~0xFFFULL, e = (va + len + 0xFFF) & ~0xFFFULL;
    if (!p || e <= s) return true;
    u64 fl = spin_lock_irqsave(&lazy_lock);
    punch_prot(p, s, e);
    bool ok = npv < cappv || grow_pv();
    if (ok) pv[npv++] = (pov_t){p->pid, s, e, prot};
    for (u32 i = 0; i < nlz; i++) {
        if (lz[i].pid != p->pid || !(lz[i].flags & LAZY_SHARED)) continue;
        if (s <= lz[i].start && e >= lz[i].end) lz[i].prot = prot;
    }
    spin_unlock_irqrestore(&lazy_lock, fl);
    if (!ok) return false;
    return uvm_protect(p, s, e - s, prot);
}

void lazy_exit(process_t *p) {
    lazy_sync(p, USER_BASE, USER_END - USER_BASE, 1);
    u64 fl = spin_lock_irqsave(&lazy_lock);
    u32 o = 0;
    for (u32 i = 0; i < nlz; i++) {
        if (lz[i].pid != p->pid) lz[o++] = lz[i];
        else if (lz[i].path) kfree(lz[i].path);
    }
    nlz = o;
    o = 0;
    for (u32 i = 0; i < npv; i++) if (pv[i].pid != p->pid) pv[o++] = pv[i];
    npv = o;
    cache_reap();
    spin_unlock_irqrestore(&lazy_lock, fl);
}

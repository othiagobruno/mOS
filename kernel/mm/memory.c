/* mOS — gerência de memória: alocador físico (bitmap), paginação e heap */
#include "kernel.h"
#include "commpage.h"

/* =========================================================== PMM */
static u64 *bitmap;     /* 1 = ocupado */
static u64  max_pages, free_count, total_count, hint;

static inline void bm_set(u64 p)   { bitmap[p >> 6] |= 1ULL << (p & 63); }
static inline void bm_clear(u64 p) { bitmap[p >> 6] &= ~(1ULL << (p & 63)); }
static inline bool bm_test(u64 p)  { return bitmap[p >> 6] >> (p & 63) & 1; }

void pmm_init(void) {
    u64 top = 0;
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type == MEM_FREE || r->type == MEM_KERNEL || r->type == MEM_ACPI || r->type == MEM_BOOTSVC) {
            u64 end = r->base + r->pages * 4096;
            if (end > top) top = end;
        }
    }
    max_pages = top / 4096;
    u64 bm_bytes = ALIGN_UP((max_pages + 63) / 64 * 8, 4096);
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type == MEM_FREE && r->base >= 0x100000 && r->pages * 4096 >= bm_bytes) {
            bitmap = (u64 *)r->base;
            break;
        }
    }
    if (!bitmap) panic("PMM: sem espaço para o bitmap");
    memset(bitmap, 0xFF, bm_bytes);
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type != MEM_FREE) continue;
        for (u64 p = r->base / 4096; p < r->base / 4096 + r->pages; p++) {
            if (p < 256) continue; /* deixa o 1º MiB em paz */
            bm_clear(p);
            free_count++;
        }
    }
    total_count = free_count;
    for (u32 i = 0; i < g_boot->region_count; i++)
        if (g_boot->regions[i].type == MEM_KERNEL) total_count += g_boot->regions[i].pages;
    for (u64 p = (u64)bitmap / 4096; p < ((u64)bitmap + bm_bytes) / 4096; p++) {
        if (!bm_test(p)) { bm_set(p); free_count--; }
    }
    hint = 256;
    kprintf("PMM: %llu MiB livres de %llu MiB, topo 0x%llx\n", free_count / 256, total_count / 256, top);
}

static spinlock_t pmm_lock;

/* marca [p, p+n) de uma vez: palavras inteiras do bitmap quando possível */
static void bm_fill(u64 p, u64 n, bool set) {
    u64 e = p + n;
    while (p < e && (p & 63)) { if (set) bm_set(p); else bm_clear(p); p++; }
    while (p + 64 <= e) { bitmap[p >> 6] = set ? ~0ULL : 0; p += 64; }
    while (p < e) { if (set) bm_set(p); else bm_clear(p); p++; }
}

/* varre o bitmap uma palavra (64 páginas) por vez: pula palavras cheias, engole palavras vazias
 * e, dentro de uma palavra mista, salta direto para o próximo bit livre/ocupado */
static u64 pmm_find(u64 from, u64 pages) {
    u64 run = 0, start = 0;
    for (u64 p = from; p < max_pages;) {
        u64 w = bitmap[p >> 6] >> (p & 63);                    /* bits de p até o fim da palavra */
        u64 left = 64 - (p & 63);
        if (p + left > max_pages) left = max_pages - p;
        if (w & 1) {                                          /* ocupado: pula a sequência de 1s */
            u64 ones = ~w ? (u64)__builtin_ctzll(~w) : 64;
            if (ones > left) ones = left;
            run = 0;
            p += ones;
            continue;
        }
        u64 zeros = w ? (u64)__builtin_ctzll(w) : 64;          /* livres até o próximo ocupado */
        if (zeros > left) zeros = left;
        if (!run) start = p;
        run += zeros;
        if (run >= pages) return start;
        p += zeros;
    }
    return ~0ULL;
}

u64 pmm_alloc(u64 pages) {
    if (!pages) return 0;
    if (!swap_busy() && swap_floor() && pmm_free_pages() < swap_floor()) mem_pressure();
    u64 f = spin_lock_irqsave(&pmm_lock);
    u64 s = pmm_find(hint, pages);
    if (s == ~0ULL && hint > 256) s = pmm_find(256, pages);
    if (s == ~0ULL) { spin_unlock_irqrestore(&pmm_lock, f); return 0; }
    bm_fill(s, pages, true);
    free_count -= pages;
    hint = s + pages;
    spin_unlock_irqrestore(&pmm_lock, f);
    return s * 4096;
}

void pmm_free(u64 addr, u64 pages) {
    u64 f = spin_lock_irqsave(&pmm_lock);
    for (u64 p = addr / 4096; p < addr / 4096 + pages; p++) {
        if (bm_test(p)) { bm_clear(p); free_count++; }
    }
    if (addr / 4096 < hint) hint = addr / 4096;
    spin_unlock_irqrestore(&pmm_lock, f);
}

/* depois que nossas tabelas de página estão ativas, a memória do firmware vira RAM livre */
void pmm_reclaim_boot(void) {
    u64 n = 0;
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type != MEM_BOOTSVC) continue;
        for (u64 p = r->base / 4096; p < r->base / 4096 + r->pages; p++) {
            if (p < 256 || p >= max_pages) continue;
            if (bm_test(p)) { bm_clear(p); free_count++; total_count++; n++; }
        }
        r->type = MEM_FREE;
    }
    kprintf("PMM: %llu MiB recuperados do firmware\n", n / 256);
}

u64 pmm_total_pages(void) { return total_count; }
/* [addr, addr+len) é RAM do mapa identidade (virtual = físico)? Drivers usam para DMA direto no buffer
 * de quem chamou, sem o bounce; endereços de usuário (0x4000_0000_0000+) nunca passam. */
bool pmm_is_ram(u64 addr, u64 len) { return addr >= 0x100000 && addr + len >= addr && addr + len <= max_pages * 4096; }
u64 pmm_free_pages(void)  { return free_count; }

/* =========================================================== Paginação */
static u64 *pml4;
#define PG_P   1ULL
#define PG_RW  2ULL
#define PG_PWT 8ULL
#define PG_PCD 16ULL
#define PG_PS  0x80ULL

static u64 *next_table(u64 *t, int i) {
    if (!(t[i] & PG_P)) {
        u64 p = pmm_alloc(1);
        if (!p) panic("paging: sem memória para tabelas");
        memset((void *)p, 0, 4096);
        t[i] = p | PG_P | PG_RW;
    }
    return (u64 *)(t[i] & 0x000FFFFFFFFFF000ULL);
}

static void map_2m(u64 a, u64 cache) {
    u64 *pdpt = next_table(pml4, (a >> 39) & 511);
    u64 *pd = next_table(pdpt, (a >> 30) & 511);
    pd[(a >> 21) & 511] = (a & ~0x1FFFFFULL) | PG_P | PG_RW | PG_PS | cache;
}

static bool is_ram(u64 a) {
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type == MEM_FREE || r->type == MEM_KERNEL || r->type == MEM_ACPI || r->type == MEM_RESERVED || r->type == MEM_BOOTSVC)
            if (a + 0x200000 > r->base && a < r->base + r->pages * 4096) return true;
    }
    return false;
}

/* mapeia MMIO de dispositivos (pode estar acima de 4 GiB) como não-cacheável */
void vmm_map_mmio(u64 phys, u64 size) {
    for (u64 a = ALIGN_DOWN(phys, 0x200000); a < phys + size; a += 0x200000) map_2m(a, PG_PCD | PG_PWT);
    __asm__ volatile("mov %%cr3,%%rax; mov %%rax,%%cr3" ::: "rax", "memory");
}

void paging_init(void) {
    pml4 = (u64 *)pmm_alloc(1);
    memset(pml4, 0, 4096);
    u64 top = 4ULL << 30;
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type == MEM_FREE || r->type == MEM_KERNEL || r->type == MEM_ACPI || r->type == MEM_BOOTSVC) {
            u64 e = r->base + r->pages * 4096;
            if (e > top) top = e;
        }
    }
    top = ALIGN_UP(top, 0x200000);
    u64 fb0 = (u64)g_boot->fb.fb, fb1 = fb0 + ALIGN_UP(g_boot->fb.fb_size ? g_boot->fb.fb_size : (u64)g_boot->fb.pitch * g_boot->fb.height * 4, 0x200000);
    for (u64 a = 0; a < top; a += 0x200000) {
        u64 cache = 0;
        if (a >= ALIGN_DOWN(fb0, 0x200000) && a < fb1) cache = PG_PWT; /* PAT1 = WC */
        else if (!is_ram(a)) cache = PG_PCD | PG_PWT;                  /* UC */
        map_2m(a, cache);
    }
    for (u64 a = ALIGN_DOWN(fb0, 0x200000); a < fb1; a += 0x200000) map_2m(a, PG_PWT);
    /* PAT: PA0=WB PA1=WC PA2=UC- PA3=UC */
    u64 pat = 0x0007010600070106ULL;
    __asm__ volatile("wrmsr" :: "c"(0x277), "a"((u32)pat), "d"((u32)(pat >> 32)));
    __asm__ volatile("wbinvd; mov %0,%%cr3" :: "r"((u64)pml4) : "memory");
    /* NX antes do smp_init: os APs copiam EFER.NXE do BSP (arch/smp.c) e este mapa de identidade não tem o bit 63,
     * então o kernel e o trampolim continuam executáveis. Página de usuário só fica NX quando o prot não pede execução. */
    u32 maxe = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "=a"(maxe), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000u));
    if (maxe >= 0x80000001u) {
        u32 eax = 0;
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001u));
        if (edx & (1u << 20)) {
            u32 lo = 0, hi = 0;
            __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080u));
            __asm__ volatile("wrmsr" :: "c"(0xC0000080u), "a"(lo | (1u << 11)), "d"(hi));
            kprintf("Paging: EFER.NXE ligado; o mapa de identidade segue sem o bit 63\n");
        }
    }
    /* Commpage Darwin arm64 em 0xFFFFFC000 (64 GiB − 16 KiB). Não está no mapa de 4 GiB:
     * uma folha nova, usuário, só leitura, NX. Os níveis de cima ganham U; o resto não. */
    {
        const u64 va = 0xFFFFFC000ULL;
        u64 *pdpt = next_table(pml4, (int)((va >> 39) & 511));
        u64 *pd = next_table(pdpt, (int)((va >> 30) & 511));
        int pdi = (int)((va >> 21) & 511);
        u64 pde = pd[pdi];
        if (!(pde & PG_PS) || !is_ram(va & ~0x1FFFFFULL)) {
            int hole = !(pde & PG_PS);
            u64 *pt = (u64 *)pmm_alloc(1);
            u64 page = pt ? pmm_alloc(1) : 0;
            if (pt && page) {
                memset(pt, 0, 4096);
                if (!hole) {
                    u64 cache = pde & (PG_PWT | PG_PCD);
                    u64 base = pde & ~0x1FFFFFULL;
                    for (int i = 0; i < 512; i++)
                        pt[i] = (base + (u64)i * 4096) | PG_P | PG_RW | cache;
                }
                commpage_fill((void *)page);
                pt[(va >> 12) & 511] = page | PG_P | (1ULL << 2) | (1ULL << 63);
                pd[pdi] = (u64)pt | PG_P | PG_RW | (1ULL << 2);
                pml4[(va >> 39) & 511] |= 1ULL << 2;
                pdpt[(va >> 30) & 511] |= 1ULL << 2;
                __asm__ volatile("mov %0,%%cr3" :: "r"((u64)pml4) : "memory");
                kprintf("commpage: Darwin arm64 em 0x%llx\n", va);
            }
        }
    }
    kprintf("Paging: identidade até 0x%llx, framebuffer em WC\n", top);
}

/* =========================================================== Heap
 *
 * Três caminhos, do mais comum para o mais raro:
 *   - pequenos (bloco <= SMALL_MAX): listas LIFO por classe de 32 bytes, O(1) para alocar e liberar;
 *   - médios: first-fit na lista livre ordenada por endereço, com fusão de vizinhos;
 *   - grandes (>= BIG_MIN): páginas direto do PMM e devolvidas a ele no kfree, então superfícies e
 *     arquivos grandes não fragmentam o heap e a RAM volta para o sistema quando são liberados.
 * Tudo sob uma trava de giro (os APs de par_for podem, em último caso, alocar). */
typedef struct blk { usize size; usize magic; struct blk *next; usize pad; } blk_t;
#define MAGIC_FREE  0xF4EEB10Cull
#define MAGIC_USED  0xA110CA7Eull
#define MAGIC_BIG   0xB16B10C5ull
#define MAGIC_CACHE 0xCAC4EB1Cull
#define ARENA_BYTES (16ULL << 20)
#define SMALL_MAX   4096u
#define NCLASS      (SMALL_MAX / 32)
#define CACHE_MAX   (8u << 20)              /* teto de bytes guardados nas classes; o excesso volta à lista geral */
#define BIG_MIN     (256u << 10)

static blk_t *free_list;
static blk_t *class_list[NCLASS];
static usize used_bytes, cached_bytes;
static spinlock_t heap_lock;

static void add_free(blk_t *b);

/* devolve todos os blocos das classes à lista geral (funde vizinhos); antes de crescer o heap */
static bool flush_classes(void) {
    if (!cached_bytes) return false;
    for (u32 c = 0; c < NCLASS; c++) {
        blk_t *b = class_list[c];
        class_list[c] = NULL;
        while (b) { blk_t *nx = b->next; add_free(b); b = nx; }
    }
    cached_bytes = 0;
    return true;
}

static void add_free(blk_t *b) {
    b->magic = MAGIC_FREE;
    blk_t **pp = &free_list, *prev = NULL;
    while (*pp && *pp < b) { prev = *pp; pp = &(*pp)->next; }
    b->next = *pp;
    *pp = b;
    if (b->next && (u8 *)b + b->size == (u8 *)b->next) { b->size += b->next->size; b->next = b->next->next; }
    if (prev && (u8 *)prev + prev->size == (u8 *)b) { prev->size += b->size; prev->next = b->next; }
}

static bool grow(usize need) {
    usize bytes = ALIGN_UP(MAX(need + 4096, ARENA_BYTES), 4096);
    u64 p = pmm_alloc(bytes / 4096);
    if (!p) return false;
    blk_t *b = (blk_t *)p;
    b->size = bytes;
    add_free(b);
    return true;
}

void heap_init(void) { grow(ARENA_BYTES * 4); }

void *kmalloc(usize n) {
    if (!n) n = 1;
    usize need = ALIGN_UP(n + sizeof(blk_t), 32);
    if (need >= BIG_MIN) {
        usize pages = ALIGN_UP(need, 4096) / 4096;
        u64 p = pmm_alloc(pages);
        if (!p) { kprintf("kmalloc: sem memória para %llu bytes\n", (u64)n); return NULL; }
        blk_t *b = (blk_t *)p;
        b->size = pages * 4096;
        b->magic = MAGIC_BIG;
        b->next = NULL;
        u64 f = spin_lock_irqsave(&heap_lock);
        used_bytes += b->size;
        spin_unlock_irqrestore(&heap_lock, f);
        return b + 1;
    }
    u64 f = spin_lock_irqsave(&heap_lock);
    if (need <= SMALL_MAX) {
        u32 c = (u32)(need / 32 - 1);
        blk_t *b = class_list[c];
        if (b) {
            class_list[c] = b->next;
            cached_bytes -= b->size;
            b->magic = MAGIC_USED;
            b->next = NULL;
            used_bytes += b->size;
            spin_unlock_irqrestore(&heap_lock, f);
            return b + 1;
        }
    }
    for (int attempt = 0; attempt < 2; attempt++) {
        blk_t **pp = &free_list;
        while (*pp) {
            blk_t *b = *pp;
            if (b->size >= need) {
                if (b->size - need >= 128) {
                    blk_t *rest = (blk_t *)((u8 *)b + need);
                    rest->size = b->size - need;
                    rest->magic = MAGIC_FREE;
                    rest->next = b->next;
                    *pp = rest;
                    b->size = need;
                } else {
                    *pp = b->next;
                }
                b->magic = MAGIC_USED;
                b->next = NULL;
                used_bytes += b->size;
                spin_unlock_irqrestore(&heap_lock, f);
                return b + 1;
            }
            pp = &b->next;
        }
        if (attempt == 0 && flush_classes()) { attempt = -1; continue; }   /* tenta de novo sem crescer */
        if (!grow(need)) break;
    }
    spin_unlock_irqrestore(&heap_lock, f);
    kprintf("kmalloc: sem memória para %llu bytes\n", (u64)n);
    return NULL;
}

/* diagnóstico: confere as listas livres e as classes (magic e encadeamento). Devolve false e diz onde, na
 * primeira inconsistência; quem chama informa o contexto (ex.: "depois do execve de X"). */
bool kheap_check(const char *where) {
    static bool reported;
    if (reported) return false;
    u64 f = spin_lock_irqsave(&heap_lock);
    const char *bad = NULL;
    const blk_t *at = NULL;
    int n = 0;
    for (const blk_t *b = free_list; b && !bad; b = b->next, n++) {
        if (b->magic != MAGIC_FREE) { bad = "lista livre: magic"; at = b; }
        else if (b->next && b->next <= b) { bad = "lista livre: ordem"; at = b; }
        else if (n > 1000000) { bad = "lista livre: laço"; at = b; }
    }
    for (u32 c = 0; c < NCLASS && !bad; c++) {
        n = 0;
        for (const blk_t *b = class_list[c]; b && !bad; b = b->next, n++)
            if (b->magic != MAGIC_CACHE && b->magic != MAGIC_USED && b->magic != MAGIC_FREE) { bad = "classe: magic"; at = b; }
            else if (n > 1000000) { bad = "classe: laço"; at = b; }
    }
    spin_unlock_irqrestore(&heap_lock, f);
    if (bad) {
        reported = true;
        kprintf("kheap_check: %s em %p (magic %llx tamanho %llu) — %s\n", bad, at, (u64)at->magic, (u64)at->size, where);
    }
    return !bad;
}

/* Bloco grande com os dados alinhados à página: o cabeçalho fica no fim de uma página só dele, logo antes dos dados, e o
 * kfree devolve cada página de dados com uvm_page_put (process.c). Assim o carregador Mach-O pode mapear as páginas de um
 * arquivo direto nos processos (o __TEXT dos frameworks, macho.c): uma página ainda mapeada (contada em pref[]) só volta
 * ao PMM quando o último dono a larga, mesmo que o VFS libere o buffer antes. */
#define MAGIC_PAGES 0x9A6E5B1Cull
void *kmalloc_pages(usize n) {
    if (!n) n = 1;
    usize pages = ALIGN_UP(n, 4096) / 4096 + 1;
    u64 p = pmm_alloc(pages);
    if (!p) { kprintf("kmalloc: sem memória para %llu bytes (alinhados à página)\n", (u64)n); return NULL; }
    blk_t *b = (blk_t *)(p + 4096) - 1;
    b->size = pages * 4096;
    b->magic = MAGIC_PAGES;
    b->next = NULL;
    u64 f = spin_lock_irqsave(&heap_lock);
    used_bytes += b->size;
    spin_unlock_irqrestore(&heap_lock, f);
    return (void *)(p + 4096);
}
/* p é o começo de um bloco de kmalloc_pages? (os do kmalloc nunca começam numa página: o cabeçalho vem antes) */
bool kmalloc_is_pages(const void *p) {
    if (!p || ((u64)p & 0xFFF) || (u64)p < 0x2000) return false;
    return ((const blk_t *)p - 1)->magic == MAGIC_PAGES;
}

/* diagnóstico: o bloco ainda está alocado? (pega kfree duplo antes de acontecer) */
bool kheap_live(const void *p) {
    if (!p) return true;
    const blk_t *b = (const blk_t *)p - 1;
    return b->magic == MAGIC_USED || b->magic == MAGIC_BIG || b->magic == MAGIC_PAGES;
}

void kfree(void *p) {
    if (!p) return;
    blk_t *b = (blk_t *)p - 1;
    if (b->magic == MAGIC_PAGES) {
        u64 f = spin_lock_irqsave(&heap_lock);
        used_bytes -= b->size;
        spin_unlock_irqrestore(&heap_lock, f);
        b->magic = 0;
        u64 base = (u64)p - 4096, pages = b->size / 4096;
        extern void uvm_page_put(u64 phys);
        for (u64 i = 1; i < pages; i++) uvm_page_put(base + i * 4096);   /* dados: pode haver outro dono (um processo) */
        pmm_free(base, 1);                                               /* a página do cabeçalho */
        return;
    }
    if (b->magic == MAGIC_BIG) {
        u64 f = spin_lock_irqsave(&heap_lock);
        used_bytes -= b->size;
        spin_unlock_irqrestore(&heap_lock, f);
        b->magic = 0;
        pmm_free((u64)b, b->size / 4096);
        return;
    }
    if (b->magic != MAGIC_USED) {   /* chamador como base+RVA: tools/kernel-map.ps1 -Addr traduz */
        extern char __ImageBase;
        kprintf("kfree: ponteiro inválido %p magic %llx (chamado de base+0x%llx)\n", p, (u64)b->magic,
                (u64)__builtin_return_address(0) - (u64)&__ImageBase);
        return;
    }
    u64 f = spin_lock_irqsave(&heap_lock);
    used_bytes -= b->size;
    if (b->size <= SMALL_MAX) {
        u32 c = (u32)(b->size / 32 - 1);
        if (cached_bytes + b->size <= CACHE_MAX) {
            b->magic = MAGIC_CACHE;
            b->next = class_list[c];
            class_list[c] = b;
            cached_bytes += b->size;
            spin_unlock_irqrestore(&heap_lock, f);
            return;
        }
    }
    add_free(b);
    spin_unlock_irqrestore(&heap_lock, f);
}

void *kcalloc(usize n, usize sz) {
    void *p = kmalloc(n * sz);
    if (p) memset(p, 0, n * sz);
    return p;
}

void *krealloc(void *p, usize n) {
    if (!p) return kmalloc(n);
    blk_t *b = (blk_t *)p - 1;
    usize have = b->magic == MAGIC_PAGES ? b->size - 4096 : b->size - sizeof(blk_t);
    if (have >= n) return p;
    void *q = kmalloc(n);
    if (q) { memcpy(q, p, have); kfree(p); }
    return q;
}

usize heap_used(void) { return used_bytes; }

void *dma_alloc(usize bytes, u64 *phys) {
    u64 p = pmm_alloc(ALIGN_UP(bytes, 4096) / 4096);
    if (!p) return NULL;
    memset((void *)p, 0, ALIGN_UP(bytes, 4096));
    if (phys) *phys = p;
    return (void *)p;
}

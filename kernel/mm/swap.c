/* mOS — swap comprimido das páginas anônimas privadas.
 *
 * A página que sai (vm_pageout em proc/process.c) é comprimida (mm/vmcomp.c) e guardada na zona, que
 * cresce página a página do PMM até 1/4 da RAM. Página toda zero ocupa só o slot. Página que não
 * comprime abaixo de VMC_MAX fica na RAM (o swap em disco para essas está em docs/nucleo-plano.md).
 *
 * PTE com P = 0 e o bit 52 ligado é de swap (vmcomp.h): não é demand-zero (entrada 0) nem MAP_SHARED
 * (esse continua no cache do lazy.c). Slot nos bits 12..35; RW, JWP e NX da PTE de origem voltam com a
 * página. COW: só sai página com um dono. O fork copia a PTE de swap e soma uma referência no slot; cada
 * lado que falta recebe uma cópia privada, e o slot só se reaproveita quando a conta chega a zero.
 *
 * Uma trava de giro protege slots, zona e o buffer de compressão: pode ser chamado de qualquer núcleo.
 */
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#include "vmcomp.h"
typedef struct { volatile u32 v; } spinlock_t;
static inline u64 spin_lock_irqsave(spinlock_t *l) { (void)l; return 0; }
static inline void spin_unlock_irqrestore(spinlock_t *l, u64 f) { (void)l; (void)f; }
u64 pmm_total_pages(void);
u64 pmm_free_pages(void);
#else
#include "kernel.h"
#include "vmcomp.h"
#endif

typedef struct { void *obj; u16 len; u16 ref; u32 next; } slot_t;
#define CHUNK_SLOTS 4096u
#define NCHUNK      ((1u << SWAP_SLOTBITS) / CHUNK_SLOTS)
#define NO_SLOT     0xFFFFFFFFu

static slot_t *chunk[NCHUNK];
static u32 nchunks, free_head = NO_SLOT;
static u64 used_slots, zero_slots, stored, nout, nin, rejected;
static u64 floor_pages;
static int swapping, ready;
static spinlock_t swap_lock;
static u8 cbuf[VMC_MAX];

static inline slot_t *slot_at(u32 s) { return &chunk[s / CHUNK_SLOTS][s % CHUNK_SLOTS]; }
static inline u32 slot_of(u64 pte) { return (u32)((pte >> 12) & ((1u << SWAP_SLOTBITS) - 1)); }

bool swap_is(u64 pte) { return !(pte & 1) && (pte & SWAP_MARK); }

void swap_init(void) {
    u64 tot = pmm_total_pages();
    floor_pages = tot / 16;
    if (floor_pages < 256) floor_pages = 256;
    vmc_zone_limit(tot / 4);
    ready = 1;
    kprintf("swap: compressor na RAM, zona até %llu MiB, pressão abaixo de %llu páginas livres\n",
            (unsigned long long)(tot / 4 / 256), (unsigned long long)floor_pages);
}

static bool grow_slots(void) {
    if (nchunks >= NCHUNK) return false;
    slot_t *c = kcalloc(CHUNK_SLOTS, sizeof *c);
    if (!c) return false;
    u32 base = nchunks * CHUNK_SLOTS;
    for (u32 i = 0; i < CHUNK_SLOTS; i++) c[i].next = i + 1 < CHUNK_SLOTS ? base + i + 1 : free_head;
    chunk[nchunks++] = c;
    free_head = base;
    return true;
}

static bool valid(u32 s) { return s / CHUNK_SLOTS < nchunks && slot_at(s)->ref; }

static void slot_free(u32 s) {
    slot_t *sl = slot_at(s);
    if (sl->obj) { vmc_zfree(sl->obj); stored--; }
    else zero_slots--;
    sl->obj = NULL;
    sl->len = 0;
    sl->ref = 0;
    sl->next = free_head;
    free_head = s;
    used_slots--;
}

void swap_hold(u64 pte) {
    u32 s = slot_of(pte);
    u64 f = spin_lock_irqsave(&swap_lock);
    if (valid(s) && slot_at(s)->ref != 0xFFFF) slot_at(s)->ref++;   /* 0xFFFF: saturado, fica para sempre */
    spin_unlock_irqrestore(&swap_lock, f);
}

void swap_release(u64 pte) {
    u32 s = slot_of(pte);
    u64 f = spin_lock_irqsave(&swap_lock);
    if (valid(s)) {
        slot_t *sl = slot_at(s);
        if (sl->ref != 0xFFFF && --sl->ref == 0) slot_free(s);
    }
    spin_unlock_irqrestore(&swap_lock, f);
}

static bool page_zero(const u64 *p) {
    for (u32 i = 0; i < 512; i++) if (p[i]) return false;
    return true;
}

/* Guarda a página e devolve a PTE de swap (com os bits SWAP_KEEP de `pte`). Quem chama libera o frame.
 * 0 = não comprime o bastante, zona cheia ou sem slot. */
u64 swap_store(u64 phys, u64 pte) {
    if (!ready) return 0;
    u64 f = spin_lock_irqsave(&swap_lock);
    if (free_head == NO_SLOT && !grow_slots()) { spin_unlock_irqrestore(&swap_lock, f); return 0; }
    void *obj = NULL;
    u32 len = 0;
    if (!page_zero((const u64 *)phys)) {
        len = vmc_compress((const void *)phys, cbuf, sizeof cbuf);
        if (!len || !(obj = vmc_zalloc(len))) { rejected++; spin_unlock_irqrestore(&swap_lock, f); return 0; }
        memcpy(obj, cbuf, len);
    }
    u32 s = free_head;
    slot_t *sl = slot_at(s);
    free_head = sl->next;
    sl->obj = obj;
    sl->len = (u16)len;
    sl->ref = 1;
    sl->next = NO_SLOT;
    used_slots++;
    if (obj) stored++;
    else zero_slots++;
    nout++;
    spin_unlock_irqrestore(&swap_lock, f);
    return SWAP_MARK | ((u64)s << 12) | (pte & SWAP_KEEP);
}

/* Traz o slot de volta para `page` (já alocada) e solta uma referência. false = slot inválido ou dado
 * corrompido: quem chama não pode mapear a página. */
bool swap_load(u64 pte, u64 page) {
    u32 s = slot_of(pte);
    bool ok = false;
    u64 f = spin_lock_irqsave(&swap_lock);
    if (valid(s)) {
        slot_t *sl = slot_at(s);
        if (!sl->obj) { memset((void *)page, 0, 4096); ok = true; }
        else ok = vmc_decompress(sl->obj, sl->len, (void *)page);
        if (ok) {
            nin++;
            if (sl->ref != 0xFFFF && --sl->ref == 0) slot_free(s);
        }
    }
    spin_unlock_irqrestore(&swap_lock, f);
    if (!ok) kprintf("swap: slot %u inválido ou corrompido na volta\n", s);
    return ok;
}

void swap_stats(swap_stats_t *st) {
    u64 f = spin_lock_irqsave(&swap_lock);
    st->slots_used = used_slots;
    st->zero_pages = zero_slots;
    st->stored_pages = stored;
    st->zone_pages = vmc_zone_pages();
    st->zone_bytes = vmc_zone_bytes();
    st->nout = nout;
    st->nin = nin;
    st->rejected = rejected;
    spin_unlock_irqrestore(&swap_lock, f);
}

u64 swap_floor(void) { return floor_pages; }
int swap_busy(void) { return swapping; }
void swap_set_busy(int v) { swapping = v; }

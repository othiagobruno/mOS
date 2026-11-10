/* Partição de swap das páginas anônimas privadas.
 *
 * Não usa disco. O storage.c recusa escrita em AHCI/NVMe/USB sem uma faixa concedida, e esses
 * discos são os "físicos" do kernel mesmo quando o QEMU os emula. A partição é um bloco contíguo
 * tirado do PMM na subida: deixa de ser página livre e vira o lugar para onde a vítima vai.
 * A memória virtual anônima passa a poder ser maior que os frames livres.
 *
 * PTE com P=0 e o bit 52 ligado não é demand-zero (entrada 0) nem MAP_SHARED (esse continua no
 * cache do lazy.c). O slot fica nos bits 12..31. Bit 63 (NX) e bit 10 (JWP) voltam com a página.
 * COW: só sai quem tem um dono. O fork copia a PTE de swap e soma uma referência no slot; cada
 * lado que falta recebe uma cópia privada, e o slot só se reaproveita quando a conta chega a zero.
 */
#include "kernel.h"
#include "proc.h"

#define SWAP_MARK (1ULL << 52)
#define SWAP_MAX  4096u

static u8 *swap_mem;
static u32 nslots;
static u16 slot_ref[SWAP_MAX];
static u64 floor_pages;
static int swapping;
static u32 nout, nin;

bool swap_is(u64 pte) { return !(pte & 1) && (pte & SWAP_MARK); }

static u32 swap_slot(u64 pte) { return (u32)((pte >> 12) & 0xFFFF); }

void swap_init(void) {
    u64 tot = pmm_total_pages();
    u64 n = tot / 16;
    if (n > SWAP_MAX) n = SWAP_MAX;
    if (n < 128 || pmm_free_pages() < n + 2048) {
        kprintf("swap: partição não reservada (livre %llu páginas)\n", pmm_free_pages());
        return;
    }
    u64 phys = pmm_alloc(n);
    if (!phys) return;
    swap_mem = (u8 *)phys;
    nslots = (u32)n;
    floor_pages = tot / 16;
    if (floor_pages < 256) floor_pages = 256;
    kprintf("swap: partição de %u páginas (%u MiB) reservada no PMM, pressão abaixo de %llu páginas livres\n",
            nslots, nslots / 256, floor_pages);
}

void swap_hold(u64 pte) {
    u32 s = swap_slot(pte);
    if (swap_mem && s < nslots && slot_ref[s]) slot_ref[s]++;
}

void swap_release(u64 pte) {
    u32 s = swap_slot(pte);
    if (!swap_mem || s >= nslots || !slot_ref[s]) return;
    if (--slot_ref[s] == 0) memset(swap_mem + (u64)s * 4096, 0, 4096);
}

static int slot_new(void) {
    for (u32 i = 0; i < nslots; i++) if (!slot_ref[i]) { slot_ref[i] = 1; return (int)i; }
    return -1;
}

/* Grava a página e devolve a PTE de swap. Quem chama libera o frame. 0 = não deu. */
u64 swap_store(u64 phys, int nx, int jwp) {
    int s = slot_new();
    if (s < 0) return 0;
    memcpy(swap_mem + (u64)s * 4096, (void *)phys, 4096);
    nout++;
    return SWAP_MARK | ((u64)s << 12) | (nx ? (1ULL << 63) : 0) | (jwp ? (1ULL << 10) : 0);
}

/* Traz o slot de volta para `page` (já alocada) e zera a referência se era a última. */
void swap_load(u64 pte, u64 page) {
    u32 s = swap_slot(pte);
    if (swap_mem && s < nslots) memcpy((void *)page, swap_mem + (u64)s * 4096, 4096);
    nin++;
    swap_release(pte);
}

u64 swap_floor(void) { return floor_pages; }
int swap_busy(void) { return swapping; }
void swap_set_busy(int v) { swapping = v; }
u32 swap_nout(void) { return nout; }
u32 swap_nin(void) { return nin; }

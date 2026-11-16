/* mOS — compressor de memória (mm/vmcomp.c) e swap comprimido (mm/swap.c) */
#pragma once

#define VMC_WORDS 1024u      /* palavras de 32 bits numa página de 4 KiB */
#define VMC_MAX   1984u      /* maior saída guardada; acima disso a página fica na RAM */

/* comprime a página (4 KiB) em out; 0 = não coube em cap */
u32  vmc_compress(const void *page, u8 *out, u32 cap);
/* false = fluxo corrompido ou com bytes sobrando */
bool vmc_decompress(const u8 *in, u32 len, void *page);

void *vmc_zalloc(u32 len);   /* objeto da zona, NULL = sem memória, acima do teto ou len inválido */
void  vmc_zfree(void *obj);
void  vmc_zone_limit(u64 pages);
u64   vmc_zone_pages(void);
u64   vmc_zone_bytes(void);

/* PTE de swap: P = 0, bit 52, slot nos bits 12..35. Guarda RW (1), JWP (10) e NX (63) da PTE de origem. */
#define SWAP_MARK     (1ULL << 52)
#define SWAP_SLOTBITS 24u
#define SWAP_KEEP     ((1ULL << 1) | (1ULL << 10) | (1ULL << 63))

typedef struct {
    u64 slots_used, zero_pages, zone_pages, zone_bytes, stored_pages;
    u64 nout, nin, rejected;
} swap_stats_t;
void swap_stats(swap_stats_t *s);

/* Host: compressor de memória (kernel/mm/vmcomp.c) e swap comprimido (kernel/mm/swap.c).
 * gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/vmcomp_host.c kernel/mm/vmcomp.c \
 *     kernel/mm/swap.c -o /tmp/vmcomp_host && /tmp/vmcomp_host */
#include "host_shim.h"
#include "vmcomp.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
static long zone_live;
static u64 total_pages = 1u << 18;   /* 1 GiB: zona até 256 MiB */
u64 pmm_total_pages(void) { return total_pages; }
u64 pmm_free_pages(void) { return total_pages; }
u64 vmc_host_page_alloc(void) { void *p = aligned_alloc(4096, 4096); if (p) zone_live++; return (u64)(uintptr_t)p; }
void vmc_host_page_free(u64 p) { zone_live--; free((void *)(uintptr_t)p); }

void swap_init(void);
bool swap_is(u64 pte);
void swap_hold(u64 pte);
void swap_release(u64 pte);
u64 swap_store(u64 phys, u64 pte);
bool swap_load(u64 pte, u64 page);

static int falhas;
static void check(int c, const char *m) { if (!c) { printf("FALHA: %s\n", m); falhas++; } }

static u64 rng = 88172645463325252ULL;
static u64 xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

/* padrões típicos de página anônima */
static void fill(u32 *w, int kind) {
    switch (kind) {
    case 0: memset(w, 0, 4096); break;                                       /* zero */
    case 1: for (int i = 0; i < 1024; i++) w[i] = i % 7 == 0 ? 0 : (u32)(i * 3); break;   /* inteiros pequenos */
    case 2: {                                                                /* ponteiros de 64 bits do heap */
        u64 *q = (u64 *)w;
        for (int i = 0; i < 512; i++) q[i] = 0x00007f3a12000000ULL + (xr() & 0xFFFF0) ;
        break;
    }
    case 3: for (int i = 0; i < 1024; i++) w[i] = (u32)xr(); break;           /* aleatório: não comprime */
    case 4: {                                                                /* texto */
        const char *t = "o mOS comprime paginas anonimas antes de faltar memoria. ";
        size_t n = strlen(t);
        for (int i = 0; i < 4096; i++) ((char *)w)[i] = t[i % n];
        break;
    }
    case 5: for (int i = 0; i < 1024; i++) w[i] = xr() % 5 ? 0 : (u32)xr(); break;   /* esparso */
    case 6: for (int i = 0; i < 1024; i++) w[i] = 0x3F800000u + (u32)(xr() & 0x3FF); break;   /* floats próximos */
    default: memset(w, 0xFF, 4096); for (int i = 0; i < 64; i++) w[xr() % 1024] = (u32)xr(); break;
    }
}

static void test_codec(void) {
    u32 in[1024], back[1024];
    u8 out[4096 + 64];
    const char *nome[] = {"zero", "inteiros", "ponteiros", "aleatório", "texto", "esparso", "floats", "cheio"};
    for (int k = 0; k < 8; k++) {
        fill(in, k);
        u32 n = vmc_compress(in, out, sizeof out);
        if (k == 3) { check(n == 0, "aleatório não cabe em 4160 bytes"); fill(in, 2); n = vmc_compress(in, out, sizeof out); }
        check(n > 0, "compress cabe em 4160 bytes");
        memset(back, 0xA5, sizeof back);
        check(vmc_decompress(out, n, back) && !memcmp(in, back, 4096), "ida e volta iguais");
        printf("  %-10s %4u bytes (%.1f%%)\n", nome[k], n, n * 100.0 / 4096);
        if (k == 0 || k == 4 || k == 5 || k == 7) check(n <= 1365, "padrão comum comprime para menos de 1/3");
        if (k == 0) check(n <= 4, "página zero: uma corrida só");
        if (n > 1) {
            check(!vmc_decompress(out, n - 1, back), "fluxo cortado é recusado");
            u8 extra[4200];
            memcpy(extra, out, n);
            extra[n] = 0;
            check(!vmc_decompress(extra, n + 1, back), "byte sobrando é recusado");
        }
    }
    /* aleatório em massa: ida e volta sempre igual, qualquer mistura */
    for (int r = 0; r < 20000; r++) {
        for (int i = 0; i < 1024; i++) {
            u64 x = xr();
            int m = (int)(x & 7);
            in[i] = m < 3 ? 0 : m < 5 ? (i > 40 ? in[i - 1 - (x >> 40) % 40] : 1) : m < 6 ? (u32)(x >> 32) & 0xFFFFFC00u : (u32)(x >> 20);
        }
        u32 n = vmc_compress(in, out, sizeof out);
        if (!n || !vmc_decompress(out, n, back) || memcmp(in, back, 4096)) { check(0, "mistura aleatória"); break; }
    }
}

static void test_zone(void) {
    enum { N = 30000 };
    static void *obj[N];
    static u32 len[N];
    vmc_zone_limit(~0ULL);
    for (int i = 0; i < N; i++) {
        len[i] = 1 + (u32)(xr() % VMC_MAX);
        obj[i] = vmc_zalloc(len[i]);
        if (!obj[i]) { check(0, "zalloc"); return; }
        memset(obj[i], (u8)i, len[i]);
    }
    check(vmc_zalloc(0) == NULL && vmc_zalloc(VMC_MAX + 1) == NULL, "tamanho inválido recusado");
    for (int i = 0; i < N; i += 2) { vmc_zfree(obj[i]); obj[i] = NULL; }
    for (int i = 0; i < N; i += 2) { obj[i] = vmc_zalloc(len[i]); memset(obj[i], (u8)i, len[i]); }
    int bad = 0;
    for (int i = 0; i < N; i++) {
        u8 *b = obj[i];
        for (u32 j = 0; j < len[i]; j++) if (b[j] != (u8)i) { bad++; break; }
    }
    check(!bad, "objetos não se sobrepõem");
    u64 useful = 0;
    for (int i = 0; i < N; i++) useful += len[i];
    printf("  zona: %llu páginas para %llu bytes úteis (%.0f%% de aproveitamento)\n", (unsigned long long)vmc_zone_pages(),
           (unsigned long long)useful, useful * 100.0 / (vmc_zone_pages() * 4096.0));
    for (int i = 0; i < N; i++) vmc_zfree(obj[i]);
    check(vmc_zone_pages() == 0 && vmc_zone_bytes() == 0 && zone_live == 0, "zona vazia devolve todas as páginas");
    /* teto */
    vmc_zone_limit(2);
    void *a = vmc_zalloc(1984), *b = vmc_zalloc(1984), *c = vmc_zalloc(1984), *d = vmc_zalloc(1984), *e = vmc_zalloc(1984);
    check(a && b && c && d && !e, "teto da zona: 2 páginas, 2 objetos de 1984 cada");
    vmc_zfree(a); vmc_zfree(b); vmc_zfree(c); vmc_zfree(d);
    check(zone_live == 0, "teto: páginas devolvidas");
    vmc_zone_limit(~0ULL);
}

static void test_swap(void) {
    enum { P = 4000 };
    static u32 pages[P][1024], back[1024];
    static u64 pte[P];
    swap_init();
    int kinds[P];
    int stored = 0, rejected = 0, wrong = 0;
    for (int i = 0; i < P; i++) {
        kinds[i] = (int)(xr() % 8);
        fill(pages[i], kinds[i]);
        u64 bits = (i & 1 ? 2ULL : 0) | (i & 2 ? 1ULL << 10 : 0) | (i & 4 ? 1ULL << 63 : 0);
        pte[i] = swap_store((u64)(uintptr_t)pages[i], bits | 0xABC000ULL | 1);
        if (!pte[i]) { rejected++; if (kinds[i] != 2 && kinds[i] != 3 && kinds[i] != 6) wrong++; continue; }
        stored++;
        check(swap_is(pte[i]) && !(pte[i] & 1), "PTE de swap: ausente e com a marca");
        check((pte[i] & SWAP_KEEP) == bits, "RW, JWP e NX guardados, o resto não");
    }
    check(rejected > 0 && !wrong, "só ficam de fora as que passam de VMC_MAX (aleatória, ponteiros espalhados, floats)");
    swap_stats_t st;
    swap_stats(&st);
    printf("  swap: %d páginas guardadas (%llu zero, %llu comprimidas em %llu páginas da zona), %d recusadas\n", stored,
           (unsigned long long)st.zero_pages, (unsigned long long)st.stored_pages, (unsigned long long)st.zone_pages, rejected);
    check(st.zone_pages * 2 < (u64)stored, "a zona usa menos da metade das páginas guardadas");
    /* fork: mais um dono em metade */
    for (int i = 0; i < P; i += 2) if (pte[i]) swap_hold(pte[i]);
    int bad = 0;
    for (int i = 0; i < P; i++) {
        if (!pte[i]) continue;
        memset(back, 0x5A, sizeof back);
        if (!swap_load(pte[i], (u64)(uintptr_t)back) || memcmp(back, pages[i], 4096)) bad++;
    }
    check(!bad, "toda página volta igual");
    for (int i = 0; i < P; i += 2) {
        if (!pte[i]) continue;
        if (!swap_load(pte[i], (u64)(uintptr_t)back) || memcmp(back, pages[i], 4096)) bad++;   /* o outro dono */
    }
    check(!bad, "o segundo dono também lê");
    swap_stats(&st);
    check(st.slots_used == 0 && st.zone_pages == 0 && zone_live == 0, "slots e zona vazios no fim");
    check(!swap_load(pte[0] ? pte[0] : pte[2], (u64)(uintptr_t)back), "slot já solto é recusado");
    /* release sem load (munmap) */
    fill(pages[0], 4);
    u64 t = swap_store((u64)(uintptr_t)pages[0], 0);
    swap_hold(t);
    swap_release(t);
    swap_release(t);
    swap_stats(&st);
    check(st.slots_used == 0 && zone_live == 0, "munmap dos dois donos solta o slot");
    /* desempenho */
    fill(pages[0], 2);
    clock_t c0 = clock();
    u8 out[4200];
    u32 n = 0;
    for (int r = 0; r < 20000; r++) n = vmc_compress(pages[0], out, sizeof out);
    clock_t c1 = clock();
    for (int r = 0; r < 20000; r++) vmc_decompress(out, n, back);
    clock_t c2 = clock();
    double tc = (double)(c1 - c0) / CLOCKS_PER_SEC, td = (double)(c2 - c1) / CLOCKS_PER_SEC;
    printf("  ritmo (ponteiros): %.0f MiB/s comprimindo, %.0f MiB/s voltando\n", 20000 * 4096.0 / 1048576 / tc,
           20000 * 4096.0 / 1048576 / td);
}

int main(void) {
    printf("vmcomp_host: codificador\n");
    test_codec();
    printf("vmcomp_host: zona\n");
    test_zone();
    printf("vmcomp_host: swap\n");
    test_swap();
    if (falhas) { printf("vmcomp_host: %d falhas\n", falhas); return 1; }
    printf("vmcomp_host: ok\n");
    return 0;
}

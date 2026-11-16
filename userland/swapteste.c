/* swapteste — pressão de memória de verdade sobre o compressor (kernel/mm/swap.c, vmcomp.c, process.c).
 *
 * Aloca memória anônima sob demanda (SYS_MMAP b = 3) acima do que está livre, escreve um padrão diferente em cada
 * página, lê tudo de volta e confere. Com a RAM esgotada, o vm_pageout tem de comprimir páginas e a falta de página
 * tem de trazê-las de volta iguais. No fim, um fork confere que o filho enxerga as páginas trocadas do pai (slot com
 * dois donos) e que o pai continua com os próprios dados depois de o filho escrever (cópia privada).
 *
 * Saída na serial: "swapteste: TODOS OS TESTES OK" ou "swapteste: N FALHA(S)". Roda pelo userland/teste.c quando
 * o binário está na imagem. Sem swap (pouca RAM na subida) o teste diz "pulado" e sai 0.
 */
#include "mos.h"
typedef unsigned int u32;

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

static void put_u(u64 v) {
    char b[24];
    int n = 0;
    if (!v) b[n++] = '0';
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    char o[24];
    for (int i = 0; i < n; i++) o[i] = b[n - 1 - i];
    o[n] = 0;
    print(o);
}

/* padrão por página: metade zeros, metade inteiros pequenos que dependem da página e da posição (comprime ~4:1) */
static inline u32 pat(u64 pg, u32 w) { return (w & 1) ? 0 : (u32)(pg * 1031u + w * 7u) & 0x3FFFFu; }

static int stats(mos_vmstats_t *st) { return (int)sys3(SYS_VMCTL, MOS_VM_STATS, (u64)st, 0); }

static int run(void) {
    mos_vmstats_t a, b;
    if (stats(&a) != 0) { print("swapteste: SYS_VMCTL indisponível\n"); return 1; }
    print("swapteste: RAM ");
    put_u(a.total_pages / 256);
    print(" MiB, livre ");
    put_u(a.free_pages / 256);
    print(" MiB\n");
    /* o necessário para passar do livre em 1/8 da RAM; teto em 1,5 GiB para a VM não levar horas sob TCG */
    u64 pages = a.free_pages + a.total_pages / 8;
    if (pages > (1536ull << 20) / 4096) pages = (1536ull << 20) / 4096;
    if (a.free_pages > pages) { print("swapteste: pulado (RAM livre demais para pressionar)\n"); return 0; }
    u64 len = pages * 4096;
    u32 *m = (u32 *)sys3(SYS_MMAP, len, 3, 0);
    CHECK((i64)(u64)m > 0, "mmap sob demanda acima da RAM livre");
    if ((i64)(u64)m <= 0) return 1;

    for (u64 pg = 0; pg < pages; pg++) {
        u32 *p = m + pg * 1024;
        for (u32 w = 0; w < 1024; w++) p[w] = pat(pg, w);
    }
    stats(&b);
    print("swapteste: depois de escrever: ");
    put_u(b.pageouts - a.pageouts);
    print(" páginas foram para o compressor, zona com ");
    put_u(b.compressor_pages);
    print(" páginas\n");
    CHECK(b.pageouts > a.pageouts, "o vm_pageout comprimiu páginas sob pressão");

    u64 bad = 0;
    for (u64 pg = 0; pg < pages; pg++) {
        u32 *p = m + pg * 1024;
        for (u32 w = 0; w < 1024; w++) if (p[w] != pat(pg, w)) { bad++; break; }
    }
    stats(&b);
    CHECK(bad == 0, "todas as páginas voltaram iguais");
    CHECK(b.pageins > a.pageins, "a falta de página trouxe páginas do compressor");
    if (bad) { print("swapteste: páginas erradas: "); put_u(bad); print("\n"); }

    /* fork: o filho lê as do pai (inclusive trocadas) e escreve nas dele; o pai não vê a escrita */
    int pid = (int)sys3(SYS_FORK, 0, 0, 0);
    if (pid == 0) {
        u64 e = 0;
        for (u64 pg = 0; pg < pages; pg += 7) {
            u32 *p = m + pg * 1024;
            if (p[2] != pat(pg, 2) || p[1022] != pat(pg, 1022)) e++;
            p[0] = 0xDEADBEEF;
        }
        exit(e ? 3 : 0);
    }
    CHECK(pid > 0, "fork");
    int st = pid > 0 ? (int)sys3(SYS_WAIT, (u64)pid, 0, 0) : -1;
    CHECK(st == 0, "o filho leu as páginas do pai, trocadas ou não");
    bad = 0;
    for (u64 pg = 0; pg < pages; pg += 7) if (m[pg * 1024] != pat(pg, 0)) bad++;
    CHECK(bad == 0, "a escrita do filho ficou no filho (cópia privada da página trocada)");

    sys3(SYS_MUNMAP, (u64)m, len, 0);
    stats(&b);
    CHECK(b.compressed_pages <= a.compressed_pages + 64, "munmap soltou os slots do compressor");

    if (fails) { print("swapteste: %d FALHA(S)\n", (i64)fails); return 1; }
    print("swapteste: TODOS OS TESTES OK\n");
    return 0;
}

void _start(const char *arg) {
    (void)arg;
    exit(run());
}

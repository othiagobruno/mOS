/* mOS — teste de host da tabela de páginas do AMD-Vi (kernel/dev/iommu_pt.c)
 *   gcc -O1 -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include tools/iommu_host.c kernel/dev/iommu_pt.c -o /tmp/iommu
 * As faixas imitam o mapa UEFI do PC (14 GB, buracos de MMIO abaixo de 4 GiB). */
#include "iommu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails;
static void ok(int c, const char *m) { if (!c) { fails++; printf("FALHOU: %s\n", m); } }
/* "físico" de mentira: índice de página num vetor */
static u8 *pages[4096]; static int np;
static void *alloc(u64 *phys) { pages[np] = aligned_alloc(4096, 4096); memset(pages[np], 0, 4096); *phys = (u64)(np + 1) << 12; return pages[np++]; }
static void *p2v(u64 p) { return pages[(p >> 12) - 1]; }
static u8 seen[65536];
static void mark(u32 id, void *c) { (void)c; if (id < 65536) seen[id] = 1; }
static void test_ivrs(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); fails++; return; }
    static u8 t[65536]; size_t n = fread(t, 1, sizeof t, f); fclose(f); (void)n;
    iommu_ivhd_t h;
    ok(iommu_ivrs_parse(t, &h, mark, NULL), "IVRS real");
    printf("IVRS: IVHD 0x%02x, IOMMU %04x, base 0x%llx, EFR 0x%llx\n", h.type, h.devid, (unsigned long long)h.base, (unsigned long long)h.efr);
    ok(h.type == 0x40 && h.base == 0xFEB80000ull && h.devid == 0x0002, "IVHD 0x40 do Raven em 0xFEB80000");
    ok(h.efr == 0x004F77EF22294ADAull && (h.efr & (1ull << 6)), "EFR com IASup");
    ok(!seen[0x0000] && !seen[0x0002] && seen[0x0008] && seen[0x0300] && seen[0x0306] && seen[0x0400] && seen[0xFFFF] && seen[0x00A0] && seen[0x00A4],
       "requisitantes: 00:01.0 em diante, GPU 03:00.x, SATA 04:00.0, IOAPIC/HPET 00A0, apelido 00A4");
}

int main(int argc, char **argv) {
    if (argc > 1) test_ivrs(argv[1]);
    iommu_range_t r[] = {
        { 0x0, 0x9F000 },                 /* RAM baixa */
        { 0x100000, 0xDD000000 - 0x100000 },
        { 0xDE000000, 0x2000000 },        /* reservado (UMA da GPU) */
        { 0x100000000ull, 0x2A0000000ull },/* RAM acima de 4 GiB (~10,5 GiB) */
    };
    u64 used = 0, root = iommu_build_identity(r, 4, alloc, &used);
    ok(root != 0, "raiz");
    u64 pa;
    ok(iommu_walk(root, 0x1000, &pa, p2v) && pa == 0x1000, "RAM baixa identidade");
    ok(iommu_walk(root, 0x12345678, &pa, p2v) && pa == 0x12345678, "RAM abaixo de 4 GiB");
    ok(iommu_walk(root, 0xDE100000, &pa, p2v) && pa == 0xDE100000, "reservado da GPU mapeado");
    ok(!iommu_walk(root, 0xFEB80000, &pa, p2v), "MMIO do IOMMU bloqueado");
    ok(!iommu_walk(root, 0xFCC00000, &pa, p2v), "BAR de dispositivo bloqueado");
    ok(!iommu_walk(root, 0xFEE00000, &pa, p2v), "faixa MSI fora das páginas");
    ok(iommu_walk(root, 0x3A0000000ull - 8, &pa, p2v) && pa == 0x3A0000000ull - 8, "último byte de RAM alta");
    ok(!iommu_walk(root, 0x3A0000000ull, &pa, p2v), "depois do fim da RAM");
    ok(!iommu_walk(root, 600ull << 30, &pa, p2v), "além de 512 GiB");
    ok(used == 1 + 15, "1 raiz + 15 tabelas de nível 2 (0..14 GiB)");
    u64 d[4]; iommu_dte(d, root, 1);
    ok((d[0] & 3) == 3 && ((d[0] >> 9) & 7) == 3 && (d[0] & IOPTE_ADDR) == root && (d[0] >> 61) == 3 && d[1] == 1 && d[2] == 0, "DTE");
    printf(fails ? "iommu: %d falhas\n" : "iommu: ok\n", fails);
    return fails != 0;
}

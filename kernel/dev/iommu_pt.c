/* mOS — tabela de páginas identidade e DTE do AMD-Vi (ver iommu.h) */
#include "iommu.h"
#ifdef MOS_HOST_TEST
#include <string.h>
#else
#include "kernel.h"
#endif

#define GB (1ull << 30)
#define MB2 (1ull << 21)

u64 iommu_build_identity(const iommu_range_t *r, int n, iommu_alloc_fn alloc, u64 *tables_used) {
    u64 rootp;
    u64 *root = alloc(&rootp);
    if (!root) return 0;
    u64 used = 1;
    static u64 *l2[512]; static u64 l2p[512];   /* só no boot: fora da pilha */
    memset(l2, 0, sizeof l2);
    for (int i = 0; i < n; i++) {
        if (!r[i].len) continue;
        u64 a = r[i].base & ~(MB2 - 1), e = (r[i].base + r[i].len + MB2 - 1) & ~(MB2 - 1);
        if (e > 512 * GB) e = 512 * GB;
        for (; a < e; a += MB2) {
            u32 gi = (u32)(a / GB), mi = (u32)((a % GB) / MB2);
            if (!l2[gi]) {
                l2[gi] = alloc(&l2p[gi]);
                if (!l2[gi]) return 0;
                used++;
                root[gi] = (l2p[gi] & IOPTE_ADDR) | IOPTE_NL(2) | IOPTE_PR | IOPTE_IR | IOPTE_IW;
            }
            l2[gi][mi] = (a & IOPTE_ADDR) | IOPTE_NL(0) | IOPTE_PR | IOPTE_IR | IOPTE_IW;   /* página de 2 MiB */
        }
    }
    if (tables_used) *tables_used = used;
    return rootp;
}

bool iommu_walk(u64 root, u64 iova, u64 *pa, void *(*p2v)(u64)) {
    if (iova >= 512 * GB) return false;
    u64 *t = p2v(root);
    u64 e = t[iova / GB];
    if (!(e & IOPTE_PR) || !(e & IOPTE_IR) || !(e & IOPTE_IW)) return false;
    u32 nl = (u32)((e >> 9) & 7);
    if (nl == 0) { *pa = (e & IOPTE_ADDR & ~(GB - 1)) | (iova & (GB - 1)); return true; }
    if (nl != 2) return false;
    t = p2v(e & IOPTE_ADDR);
    e = t[(iova % GB) / MB2];
    if (!(e & IOPTE_PR) || !(e & IOPTE_IR) || !(e & IOPTE_IW)) return false;
    nl = (u32)((e >> 9) & 7);
    if (nl != 0) return false;          /* esta tabela só usa páginas de 2 MiB */
    *pa = (e & IOPTE_ADDR & ~(MB2 - 1)) | (iova & (MB2 - 1));
    return true;
}

void iommu_dte(u64 dte[4], u64 root, u16 domain) {
    dte[0] = 1ull                        /* V */
           | 1ull << 1                   /* TV: traduz */
           | 3ull << 9                   /* Mode 3 */
           | (root & IOPTE_ADDR)
           | IOPTE_IR | IOPTE_IW;
    dte[1] = domain;
    dte[2] = 0;                          /* IV = 0 */
    dte[3] = 0;
}

static void entries(const u8 *p, const u8 *end, iommu_ivhd_t *h, void (*setdev)(u32, void *), void *ctx) {
    u32 start = 0; bool range = false, aliasing = false; u16 alias = 0;
#define SET(d) do { setdev((d), ctx); h->ndev++; } while (0)
    while (p < end) {
        u8 t = p[0];
        u16 id = (u16)(p[1] | p[2] << 8);
        u32 len;
        if (t < 0x40) len = 4;
        else if (t < 0x80) len = 8;
        else if (t == 0xF0 && p + 22 <= end) len = 22u + p[21];   /* ACPI HID: UID de tamanho variável */
        else break;
        switch (t) {
        case 0x01: for (u32 d = 0; d <= 0xFFFF; d++) SET(d); break;
        case 0x02: case 0x46: SET(id); break;
        case 0x42: SET(id); SET((u16)(p[5] | p[6] << 8)); break;
        case 0x03: case 0x43: case 0x47:
            start = id; range = true;
            aliasing = t == 0x43; if (aliasing) alias = (u16)(p[5] | p[6] << 8);
            break;
        case 0x04:
            if (range) { for (u32 d = start; d <= id; d++) SET(d); if (aliasing) SET(alias); }
            range = false; aliasing = false;
            break;
        case 0x48: SET((u16)(p[5] | p[6] << 8)); break;            /* IOAPIC/HPET: o requisitante real */
        case 0xF0: SET(id); break;
        }
        p += len;
    }
#undef SET
}

bool iommu_ivrs_parse(const u8 *t, iommu_ivhd_t *h, void (*setdev)(u32, void *), void *ctx) {
    memset(h, 0, sizeof *h);
    if (memcmp(t, "IVRS", 4)) return false;
    u32 len = (u32)t[4] | (u32)t[5] << 8 | (u32)t[6] << 16 | (u32)t[7] << 24;
    const u8 *end = t + len, *best = NULL;
    u8 bt = 0;
    for (const u8 *q = t + 48; q + 4 <= end;) {
        u16 l = (u16)(q[2] | q[3] << 8);
        if (l < 4 || q + l > end) break;
        if ((q[0] == 0x10 || q[0] == 0x11 || q[0] == 0x40) && q[0] >= bt) { best = q; bt = q[0]; }
        q += l;
    }
    if (!best) return false;
    u16 bl = (u16)(best[2] | best[3] << 8);
    h->type = bt;
    h->devid = (u16)(best[4] | best[5] << 8);
    h->cap = (u16)(best[6] | best[7] << 8);
    memcpy(&h->base, best + 8, 8);
    if (bt != 0x10) memcpy(&h->efr, best + 24, 8);
    entries(best + (bt == 0x10 ? 24 : 40), best + bl, h, setdev, ctx);
    return h->base != 0;
}

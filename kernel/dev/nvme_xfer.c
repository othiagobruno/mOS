#include "nvme_xfer.h"

int nvme_prp_build(u64 addr, u32 bytes, u64 *list, int list_max, nvme_prp_t *out) {
    if (!out || !bytes || (addr & 3)) return -1;
    u64 first = addr & ~(u64)(NVME_PAGE - 1);
    u64 last = (addr + (u64)bytes - 1) & ~(u64)(NVME_PAGE - 1);
    int npages = (int)((last - first) / NVME_PAGE) + 1;
    out->prp1 = addr;
    out->prp2 = 0;
    out->list_entries = 0;
    if (npages <= 1) return 0;
    if (npages == 2) {
        out->prp2 = first + NVME_PAGE;
        return 0;
    }
    int n = npages - 1;
    if (!list || n > list_max) return -1;
    for (int i = 0; i < n; i++) list[i] = first + (u64)(i + 1) * NVME_PAGE;
    out->list_entries = n;
    return 0;
}

int nvme_sgl_build(u64 addr, u32 bytes, nvme_sgl_t *out) {
    if (!out || !bytes || (addr & 3) || (bytes & 3)) return -1;
    out->addr = addr;
    out->len = bytes;
    out->id = 0; /* SGL Data Block */
    return 0;
}

int nvme_q_for_cpu(int cpu, int nq) {
    if (nq <= 1) return 0;
    if (cpu < 0) cpu = 0;
    return cpu % nq;
}

int nvme_intr_pick(int has_msix, int msix_ok, int has_msi, int msi_ok) {
    if (has_msix && msix_ok) return NVME_INTR_MSIX;
    if (has_msi && msi_ok) return NVME_INTR_MSI;
    return NVME_INTR_POLL;
}

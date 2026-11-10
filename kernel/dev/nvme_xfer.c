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

int nvme_msi_compose(u64 lapic_base, u32 apic_id, int x2apic, u32 vector,
                     u32 *addr_lo, u32 *addr_hi, u32 *data) {
    if (!addr_lo || !addr_hi || !data || !lapic_base || !vector || vector > 254) return -1;
    u64 base = lapic_base & ~0xFFFull;
    if (!base) return -1;
    *addr_lo = (u32)base | ((apic_id & 0xFFu) << 12);
    *addr_hi = x2apic ? (apic_id >> 8) : (u32)(base >> 32);
    *data = vector & 0xFFu;
    return 0;
}

int nvme_ioq_count(int smp_done, int ncpus, int granted, int maxq) {
    if (!smp_done || ncpus < 1 || granted < 1 || maxq < 1) return 0;
    int n = ncpus;
    if (n > granted) n = granted;
    if (n > maxq) n = maxq;
    return n;
}

void nvme_flights_init(nvme_flights_t *f, int max) {
    if (!f) return;
    for (int i = 0; i < NVME_FLIGHT_MAX; i++) {
        f->slot[i].used = 0;
        f->slot[i].done = 0;
        f->slot[i].cid = 0;
        f->slot[i].qid = 0;
        f->slot[i].status = 0;
        f->slot[i].dw0 = 0;
    }
    if (max < 1) max = 1;
    if (max > NVME_FLIGHT_MAX) max = NVME_FLIGHT_MAX;
    f->max = max;
    f->next_cid = 1;
}

int nvme_flight_claim(nvme_flights_t *f, u16 qid) {
    if (!f) return -1;
    for (int i = 0; i < f->max; i++) {
        if (f->slot[i].used) continue;
        u16 cid = f->next_cid++;
        if (!f->next_cid) f->next_cid = 1;
        f->slot[i].used = 1;
        f->slot[i].done = 0;
        f->slot[i].cid = cid;
        f->slot[i].qid = qid;
        f->slot[i].status = 0;
        f->slot[i].dw0 = 0;
        return i;
    }
    return -1;
}

int nvme_flight_complete(nvme_flights_t *f, u16 qid, u16 cid, u16 status, u32 dw0) {
    if (!f) return -1;
    for (int i = 0; i < f->max; i++) {
        if (!f->slot[i].used || f->slot[i].done) continue;
        if (f->slot[i].cid != cid || f->slot[i].qid != qid) continue;
        f->slot[i].done = 1;
        f->slot[i].status = status;
        f->slot[i].dw0 = dw0;
        return i;
    }
    return -1;
}

int nvme_flight_outstanding(const nvme_flights_t *f) {
    if (!f) return 0;
    int n = 0;
    for (int i = 0; i < f->max; i++) if (f->slot[i].used && !f->slot[i].done) n++;
    return n;
}

void nvme_flight_release(nvme_flights_t *f, int idx) {
    if (!f || idx < 0 || idx >= f->max) return;
    f->slot[idx].used = 0;
    f->slot[idx].done = 0;
}

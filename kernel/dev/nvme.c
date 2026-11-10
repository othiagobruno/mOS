/*
 * mOS — driver NVMe: fila de administração + uma fila de E/S por núcleo (até NVME_MAX_IOQ),
 * IDENTIFY, READ/WRITE/FLUSH. DMA por PRP; SGL Data Block quando o controlador anuncia SGLS
 * e o buffer não começa em página. MSI-X, senão MSI, senão polling da fase do CQE.
 * Cada namespace ativo vira um blkdev_t gravável registrado como "nvme<c>n<ns>".
 */
#include "storage.h"
#include "nvme_xfer.h"

/* ---- registradores do controlador */
#define R_CAP   0x00
#define R_VS    0x08
#define R_INTMS 0x0C
#define R_CC    0x14
#define R_CSTS  0x1C
#define R_AQA   0x24
#define R_ASQ   0x28
#define R_ACQ   0x30
#define CC_EN   (1u << 0)
#define CSTS_RDY (1u << 0)
#define CSTS_CFS (1u << 1)

/* ---- opcodes */
#define ADM_CREATE_SQ 0x01
#define ADM_CREATE_CQ 0x05
#define ADM_IDENTIFY  0x06
#define IO_FLUSH      0x00
#define IO_WRITE      0x01
#define IO_READ       0x02
#define ADM_SET_FEAT  0x09
#define FEAT_NQUEUES  0x07

#define QDEPTH       32
#define NVME_MAX_IOQ 8
#define MSI_VECTOR   0xE0
#define NVME_WIN     8
#define BOUNCE_BYTES (128u << 10)
#define ADMIN_TIMEOUT_MS 5000
#define IO_TIMEOUT_MS    10000
#define MAX_CTRL 4
#define MAX_NS   4

typedef struct { u32 cdw[16]; } sqe_t;
typedef struct { u32 dw0, dw1; u16 sqhd, sqid; u16 cid, status; } cqe_t;

typedef struct {
    sqe_t *sq; cqe_t *cq;
    u64 sq_phys, cq_phys;
    u16 tail, head, qid, depth;
    u8  phase;
} queue_t;

typedef struct nvme_ctrl nvme_ctrl_t;

typedef struct {
    nvme_ctrl_t *c;
    u32 nsid;
    u64 sectors;
    u32 ss;                     /* bytes por bloco lógico */
    char desc[96];
    blkdev_t dev;
} nvme_ns_t;

struct nvme_ctrl {
    int idx;
    volatile u8 *regs;
    u32 dstrd;                  /* passo dos doorbells em bytes */
    u64 timeout_ms;
    queue_t admin;
    queue_t io[NVME_MAX_IOQ];
    int nio;
    int intr;                   /* NVME_INTR_* */
    volatile u32 irq_pending;
    bool sgl;
    u16 cid;
    nvme_flights_t flight;
    int granted;                /* filas de E/S que o controlador concedeu */
    u32 qd;
    u8  *buf;   u64 buf_phys;   /* bounce de DMA */
    u64 *prpl;  u64 prpl_phys;  /* lista de PRP do bounce (página única) */
    u64 *dprpl; u64 dprpl_phys; /* lista de PRP para DMA direto no buffer de quem chamou */
    u32 max_xfer;               /* bytes por comando (MDTS e bounce) */
    u32 dmax;                   /* bytes por comando no DMA direto (MDTS, até 1 MiB) */
    char model[41], serial[21], fw[9];
    nvme_ns_t ns[MAX_NS];
    int nns;
    pci_dev_t *pci;
};

static nvme_ctrl_t *ctrls[MAX_CTRL];
static int nctrls;

static inline u32 rr(nvme_ctrl_t *c, u32 off) { return mmio_r32(c->regs, off); }
static inline void rw(nvme_ctrl_t *c, u32 off, u32 v) { mmio_w32(c->regs, off, v); }
static inline u64 rr64(nvme_ctrl_t *c, u32 off) { return (u64)rr(c, off) | (u64)rr(c, off + 4) << 32; }
static inline void rw64(nvme_ctrl_t *c, u32 off, u64 v) { rw(c, off, (u32)v); rw(c, off + 4, (u32)(v >> 32)); }

static void ring_sq(nvme_ctrl_t *c, queue_t *q) { rw(c, 0x1000 + (2 * q->qid) * c->dstrd, q->tail); }
static void ring_cq(nvme_ctrl_t *c, queue_t *q) { rw(c, 0x1000 + (2 * q->qid + 1) * c->dstrd, q->head); }

static bool alloc_queue(queue_t *q, u16 qid, u16 depth) {
    q->sq = dma_alloc(depth * sizeof(sqe_t), &q->sq_phys);
    q->cq = dma_alloc(depth * sizeof(cqe_t), &q->cq_phys);
    q->tail = q->head = 0;
    q->phase = 1;
    q->qid = qid;
    q->depth = depth;
    return q->sq && q->cq && depth >= 2;
}

static queue_t *ioq(nvme_ctrl_t *c) {
    int i = nvme_q_for_cpu(this_cpu(), c->nio);
    return &c->io[i];
}

static int irqs_enabled(void) {
    u64 f;
    __asm__ volatile("pushfq; popq %0" : "=r"(f));
    return (f & 0x200) != 0;
}

static void reap_q(nvme_ctrl_t *c, queue_t *q) {
    for (;;) {
        volatile cqe_t *e = &q->cq[q->head];
        if ((e->status & 1) != q->phase) break;
        u16 st = (u16)(e->status >> 1);
        u16 cid = e->cid;
        u32 dw0 = e->dw0;
        q->head = (u16)((q->head + 1) % q->depth);
        if (!q->head) q->phase ^= 1;
        ring_cq(c, q);
        nvme_flight_complete(&c->flight, q->qid, cid, st, dw0);
    }
}

static void reap_io(nvme_ctrl_t *c) {
    u64 f = irq_save();
    for (int i = 0; i < c->nio; i++) reap_q(c, &c->io[i]);
    irq_restore(f);
}

static void nvme_on_irq(void) {
    for (int i = 0; i < nctrls; i++) {
        if (!ctrls[i]) continue;
        ctrls[i]->irq_pending = 1;
        reap_io(ctrls[i]);
    }
}

static int post_io(nvme_ctrl_t *c, queue_t *q, sqe_t *cmd) {
    if (!q || ((q->tail + 1) % q->depth) == q->head) return -1;
    int s = nvme_flight_claim(&c->flight, q->qid);
    if (s < 0) return -1;
    u16 cid = c->flight.slot[s].cid;
    cmd->cdw[0] = (cmd->cdw[0] & 0xFFFF) | ((u32)cid << 16);
    q->sq[q->tail] = *cmd;
    q->tail = (u16)((q->tail + 1) % q->depth);
    return s;
}

/* Espera os slots. Com MSI/MSI-X, dorme até a interrupção colher os CQE; senão lê a fase. */
static int wait_slots(nvme_ctrl_t *c, int *slots, int n) {
    u64 end = timer_ticks() + IO_TIMEOUT_MS;
    for (;;) {
        reap_io(c);
        int left = 0, bad = 0;
        for (int i = 0; i < n; i++) {
            nvme_flight_slot_t *s = &c->flight.slot[slots[i]];
            if (!s->done) left++;
            else if (s->status) bad = 1;
        }
        if (!left) {
            for (int i = 0; i < n; i++) nvme_flight_release(&c->flight, slots[i]);
            return bad ? -1 : 0;
        }
        if (rr(c, R_CSTS) & CSTS_CFS) { kprintf("nvme%d: falha fatal do controlador\n", c->idx); return -1; }
        if (timer_ticks() > end) {
            reap_io(c);
            kprintf("nvme%d: %d comando(s) expiraram\n", c->idx, left);
            return -1;
        }
        if (c->intr) {
            int on = irqs_enabled();
            if (!on) sti();
            if (!c->irq_pending) hlt_raw();
            c->irq_pending = 0;
            if (!on) cli();
        } else __asm__ volatile("pause");
    }
}

/* envia um comando e espera a conclusão; devolve o campo de status (0 = sucesso) ou -1 em timeout */
static int submit(nvme_ctrl_t *c, queue_t *q, sqe_t *cmd, u64 timeout_ms, u32 *dw0) {
    u16 cid = ++c->cid;
    cmd->cdw[0] = (cmd->cdw[0] & 0xFFFF) | (u32)cid << 16;
    q->sq[q->tail] = *cmd;
    q->tail = (u16)((q->tail + 1) % q->depth);
    __asm__ volatile("mfence" ::: "memory");
    ring_sq(c, q);
    u64 end = timer_ticks() + timeout_ms;
    for (;;) {
        volatile cqe_t *e = &q->cq[q->head];
        if ((e->status & 1) == q->phase) {
            u16 st = e->status >> 1, ecid = e->cid;
            if (dw0) *dw0 = e->dw0;
            q->head = (u16)((q->head + 1) % q->depth);
            if (!q->head) q->phase ^= 1;
            ring_cq(c, q);
            if (ecid != cid) continue;           /* conclusão atrasada de um comando anterior */
            return st;
        }
        if (rr(c, R_CSTS) & CSTS_CFS) { kprintf("nvme%d: falha fatal do controlador\n", c->idx); return -1; }
        if (timer_ticks() > end) {
            /* a MSI pode ter se perdido: uma última leitura da fase ainda completa o comando */
            if ((e->status & 1) == q->phase) continue;
            kprintf("nvme%d: comando %02x expirou\n", c->idx, cmd->cdw[0] & 0xFF);
            return -1;
        }
        if (c->intr) c->irq_pending = 0;
        __asm__ volatile("pause");
    }
}

/* PRP1/PRP2. Com mais de duas páginas, `list` recebe as páginas seguintes e PRP2 fica `list_phys`. */
static bool set_prp(sqe_t *cmd, u64 phys, u32 bytes, u64 *list, u64 list_phys) {
    nvme_prp_t m;
    if (nvme_prp_build(phys, bytes, list, 512, &m)) return false;
    if (m.list_entries) m.prp2 = list_phys;
    cmd->cdw[6] = (u32)m.prp1;
    cmd->cdw[7] = (u32)(m.prp1 >> 32);
    cmd->cdw[8] = (u32)m.prp2;
    cmd->cdw[9] = (u32)(m.prp2 >> 32);
    return true;
}

static bool set_sgl(sqe_t *cmd, u64 phys, u32 bytes) {
    nvme_sgl_t s;
    if (nvme_sgl_build(phys, bytes, &s)) return false;
    cmd->cdw[0] = (cmd->cdw[0] & ~(3u << 14)) | (1u << 14); /* PSDT = SGL */
    cmd->cdw[6] = (u32)s.addr;
    cmd->cdw[7] = (u32)(s.addr >> 32);
    cmd->cdw[8] = s.len;
    cmd->cdw[9] = s.id << 24;
    return true;
}

static bool identify(nvme_ctrl_t *c, u32 cns, u32 nsid) {
    sqe_t cmd = {0};
    cmd.cdw[0] = ADM_IDENTIFY;
    cmd.cdw[1] = nsid;
    cmd.cdw[10] = cns;
    memset(c->buf, 0, 4096);
    if (!set_prp(&cmd, c->buf_phys, 4096, c->prpl, c->prpl_phys)) return false;
    return submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL) == 0;
}

/* ---- E/S direto no buffer de quem chamou (memória identidade = física, contígua): sem bounce e sem
 * memcpy, comandos de até dmax bytes. PRP1 = endereço (alinhado a 4 bytes); demais páginas em PRP2
 * ou numa lista de PRP. */
/* Vários comandos na mesma fila, um toque de campainha, conclusão pela interrupção (ou pela fase). */
static bool rw_window(nvme_ns_t *ns, u64 lba, u64 n, u8 *data, bool write, bool direct) {
    nvme_ctrl_t *c = ns->c;
    if (!c->nio) return false;
    queue_t *q = ioq(c);
    /* 8 KiB cabe em PRP1+PRP2: vários comandos não compartilham a lista de PRP. */
    u32 chunk = 8192;
    u32 cap = direct ? c->dmax : c->max_xfer;
    if (chunk > cap) chunk = cap;
    if (!direct && chunk > BOUNCE_BYTES / NVME_WIN) chunk = BOUNCE_BYTES / NVME_WIN;
    chunk = (chunk / ns->ss) * ns->ss;
    if (!chunk) return false;
    u32 maxb = chunk / ns->ss;
    while (n) {
        int slots[NVME_WIN];
        u32 ks[NVME_WIN];
        u8 *src[NVME_WIN];
        int m = 0;
        u64 lb = lba;
        u8 *p = data;
        u64 left = n;
        while (left && m < NVME_WIN) {
            u32 k = (u32)MIN(left, (u64)maxb);
            u32 bytes = k * ns->ss;
            u64 addr = direct ? (u64)p : c->buf_phys + (u64)m * (BOUNCE_BYTES / NVME_WIN);
            u64 first = addr & ~4095ULL, last = (addr + bytes - 1) & ~4095ULL;
            int spans = (int)((last - first) / 4096) + 1;
            if (spans > 2 && m) break;
            u64 *list = c->dprpl;
            u64 list_phys = c->dprpl_phys;
            if (!direct) {
                if (write) memcpy(c->buf + (u64)m * (BOUNCE_BYTES / NVME_WIN), p, bytes);
                list = c->prpl;
                list_phys = c->prpl_phys;
            }
            sqe_t cmd = {0};
            cmd.cdw[0] = write ? IO_WRITE : IO_READ;
            cmd.cdw[1] = ns->nsid;
            if (direct && c->sgl && (addr & 4095)) {
                if (!set_sgl(&cmd, addr, bytes)) break;
            } else if (!set_prp(&cmd, addr, bytes, list, list_phys)) break;
            cmd.cdw[10] = (u32)lb;
            cmd.cdw[11] = (u32)(lb >> 32);
            cmd.cdw[12] = k - 1;
            int s = post_io(c, q, &cmd);
            if (s < 0) break;
            slots[m] = s;
            ks[m] = k;
            src[m] = p;
            m++;
            lb += k;
            left -= k;
            p += bytes;
        }
        if (!m) return false;
        __asm__ volatile("mfence" ::: "memory");
        ring_sq(c, q);
        if (wait_slots(c, slots, m)) {
            kprintf("nvme%d: %s lba %llu falhou\n", c->idx, write ? "escrita" : "leitura", lba);
            return false;
        }
        if (!write && !direct) {
            for (int i = 0; i < m; i++)
                memcpy(src[i], c->buf + (u64)i * (BOUNCE_BYTES / NVME_WIN), ks[i] * ns->ss);
        }
        u64 step = 0;
        for (int i = 0; i < m; i++) step += ks[i];
        lba += step;
        n -= step;
        data += step * ns->ss;
    }
    return true;
}

static bool rw_direct(nvme_ns_t *ns, u64 lba, u64 n, u8 *data, bool write) {
    return rw_window(ns, lba, n, data, write, true);
}

/* ---- E/S por blocos (até max_xfer por comando) */
static bool rw_blocks(nvme_ns_t *ns, u64 lba, u64 n, u8 *data, bool write) {
    nvme_ctrl_t *c = ns->c;
    /* contenção de escrita: único ponto onde este driver emite NVMe Write (rw_direct só é chamado daqui) */
    if (write && !storage_write_ok(&ns->dev, lba * ns->ss, n * ns->ss)) return false;
    if (c->dprpl && !((u64)data & 3) && pmm_is_ram((u64)data, n * ns->ss) && n * ns->ss >= 4096)
        return rw_direct(ns, lba, n, data, write);
    return rw_window(ns, lba, n, data, write, false);
}

/* ---- blkdev (deslocamentos em bytes, qualquer alinhamento) */
static bool ns_read(blkdev_t *bd, u64 off, void *out, u64 len) {
    nvme_ns_t *ns = bd->ctx;
    u8 *o = out, *tmp = NULL;
    while (len) {
        u64 lba = off / ns->ss, in = off % ns->ss;
        if (in || len < ns->ss) {
            if (!tmp && !(tmp = kmalloc(ns->ss))) return false;
            if (!rw_blocks(ns, lba, 1, tmp, false)) { kfree(tmp); return false; }
            u64 k = MIN(len, ns->ss - in);
            memcpy(o, tmp + in, k);
            o += k; off += k; len -= k;
        } else {
            u64 n = len / ns->ss;
            if (!rw_blocks(ns, lba, n, o, false)) { kfree(tmp); return false; }
            o += n * ns->ss; off += n * ns->ss; len -= n * ns->ss;
        }
    }
    kfree(tmp);
    return true;
}

static bool ns_write(blkdev_t *bd, u64 off, const void *src, u64 len) {
    nvme_ns_t *ns = bd->ctx;
    const u8 *s = src;
    u8 *tmp = NULL;
    while (len) {
        u64 lba = off / ns->ss, in = off % ns->ss;
        if (in || len < ns->ss) {                /* ler-modificar-gravar */
            if (!tmp && !(tmp = kmalloc(ns->ss))) return false;
            if (!rw_blocks(ns, lba, 1, tmp, false)) { kfree(tmp); return false; }
            u64 k = MIN(len, ns->ss - in);
            memcpy(tmp + in, s, k);
            if (!rw_blocks(ns, lba, 1, tmp, true)) { kfree(tmp); return false; }
            s += k; off += k; len -= k;
        } else {
            u64 n = len / ns->ss;
            if (!rw_blocks(ns, lba, n, (u8 *)s, true)) { kfree(tmp); return false; }
            s += n * ns->ss; off += n * ns->ss; len -= n * ns->ss;
        }
    }
    kfree(tmp);
    return true;
}

static bool ns_flush(blkdev_t *bd) {
    nvme_ns_t *ns = bd->ctx;
    sqe_t cmd = {0};
    cmd.cdw[0] = IO_FLUSH;
    cmd.cdw[1] = ns->nsid;
    return submit(ns->c, ioq(ns->c), &cmd, IO_TIMEOUT_MS, NULL) == 0;
}

static void copy_str(char *dst, const u8 *src, int n) {
    memcpy(dst, src, n);
    dst[n] = 0;
    for (int i = n - 1; i >= 0 && (dst[i] == ' ' || !dst[i]); i--) dst[i] = 0;
}

static bool wait_ready(nvme_ctrl_t *c, bool ready) {
    u64 end = timer_ticks() + c->timeout_ms;
    while (((rr(c, R_CSTS) & CSTS_RDY) != 0) != ready) {
        if (rr(c, R_CSTS) & CSTS_CFS && ready) return false;
        if (timer_ticks() > end) return false;
        __asm__ volatile("pause");
    }
    return true;
}

static void add_namespace(nvme_ctrl_t *c, u32 nsid) {
    if (c->nns >= MAX_NS || !identify(c, 0, nsid)) return;
    const u8 *id = c->buf;
    u64 nsze = le64(id);
    u8 flbas = id[26] & 0x0F;
    u32 lbaf = le32(id + 128 + 4 * flbas);
    u32 lbads = (lbaf >> 16) & 0xFF, ms = lbaf & 0xFFFF;
    if (!nsze || lbads < 9 || lbads > 16) return;
    if (ms && !(id[26] & 0x10)) { kprintf("nvme%d: ns %u usa metadados separados, ignorado\n", c->idx, nsid); return; }
    nvme_ns_t *ns = &c->ns[c->nns++];
    ns->c = c;
    ns->nsid = nsid;
    ns->sectors = nsze;
    ns->ss = 1u << lbads;
    u64 bytes = nsze * ns->ss;
    snprintf(ns->desc, sizeof ns->desc, "%s (%llu MB)", c->model[0] ? c->model : "SSD NVMe", bytes >> 20);
    ns->dev.size = bytes;
    ns->dev.read = ns_read;
    ns->dev.write = ns_write;
    ns->dev.flush = ns_flush;
    ns->dev.ctx = ns;
    char name[16];
    snprintf(name, sizeof name, "nvme%dn%u", c->idx, nsid);
    kprintf("nvme%d: namespace %u, %llu blocos de %u bytes\n", c->idx, nsid, nsze, ns->ss);
    ns->dev.lbs = ns->ss;
    storage_register(&ns->dev, name, ns->desc);
    {
        diskphys_t ph;
        memset(&ph, 0, sizeof ph);
        ph.kind = PHYS_NVME;
        ph.nsid = nsid;
        if (c->pci) pci_chain(c->pci, &ph);
        storage_set_phys(&ns->dev, &ph);
    }
}

/* Caminha as capabilities PCI. Devolve o offset do id pedido. */
static bool pci_find_cap(pci_dev_t *d, u8 want, u8 *at) {
    if (!((pci_read(d, 0x04) >> 16) & (1u << 4))) return false;
    u8 off = (u8)(pci_read(d, 0x34) & 0xFC);
    for (int i = 0; i < 48 && off >= 0x40; i++) {
        u32 w = pci_read(d, off);
        u8 id = (u8)(w & 0xFF);
        u8 next = (u8)((w >> 8) & 0xFC);
        if (id == want) { *at = off; return true; }
        if (!next || next == off) return false;
        off = next;
    }
    return false;
}

/* Programa uma mensagem. Se a capability não existe ou a tabela não cabe, o chamador fica em polling. */
static bool arm_msix(pci_dev_t *pd, u32 addr, u32 addr_hi, u32 data) {
    u8 off;
    if (!pci_find_cap(pd, 0x11, &off)) return false;
    u32 toff = pci_read(pd, (u8)(off + 4));
    u8 bir = (u8)(toff & 7);
    if (bir > 5) return false;
    u64 bar = pci_bar_addr(pd, bir);
    if (!bar) return false;
    u64 tbl = bar + (toff & ~7u);
    vmm_map_mmio(tbl, 16);
    volatile u32 *e = (volatile u32 *)tbl;
    e[0] = addr;
    e[1] = addr_hi;
    e[2] = data;
    e[3] = 0;                       /* desmascara o vetor 0 */
    u32 w = pci_read(pd, off);
    u32 mc = (w >> 16) & 0x3FFFu;   /* liga MSI-X, tira o function mask */
    mc |= 1u << 15;
    pci_write(pd, off, (w & 0xFFFF) | (mc << 16));
    u32 cmd = pci_read(pd, 0x04);
    pci_write(pd, 0x04, cmd | (1u << 10)); /* INTx desligado */
    return true;
}

static bool arm_msi(pci_dev_t *pd, u32 addr, u32 addr_hi, u32 data) {
    u8 off;
    if (!pci_find_cap(pd, 0x05, &off)) return false;
    u32 w = pci_read(pd, off);
    u16 mc = (u16)(w >> 16);
    bool is64 = (mc & (1u << 7)) != 0;
    pci_write(pd, (u8)(off + 4), addr);
    if (is64) {
        pci_write(pd, (u8)(off + 8), addr_hi);
        pci_write(pd, (u8)(off + 12), data);
    } else {
        pci_write(pd, (u8)(off + 8), data);
    }
    mc = (u16)((mc & ~0x70u) | 1u); /* uma mensagem, MSI enable; bits 6:4 ficam */
    mc = (u16)((mc & ~(7u << 1)) | 1u);
    pci_write(pd, off, (w & 0xFFFF) | ((u32)mc << 16));
    u32 cmd = pci_read(pd, 0x04);
    pci_write(pd, 0x04, cmd | (1u << 10));
    return true;
}

static bool setup(pci_dev_t *pd) {
    u64 base = pci_bar_addr(pd, 0);
    if (!base) return false;
    nvme_ctrl_t *c = kcalloc(1, sizeof *c);
    if (!c) return false;
    c->idx = nctrls;
    c->pci = pd;
    pci_enable_bus_master(pd);
    vmm_map_mmio(base, 0x4000);
    c->regs = (volatile u8 *)base;
    u64 cap = rr64(c, R_CAP);
    u32 mqes = (u32)(cap & 0xFFFF) + 1, mpsmin = (u32)((cap >> 48) & 0xF);
    c->dstrd = 4u << ((cap >> 32) & 0xF);
    c->timeout_ms = MAX(500, ((cap >> 24) & 0xFF) * 500);
    u32 vs = rr(c, R_VS);
    if (!(cap & (1ull << 37)) || mpsmin > 0 || mqes < 2) {
        kprintf("nvme: controlador %04x:%04x não suporta o conjunto NVM com páginas de 4 KiB\n", pd->vendor, pd->device);
        kfree(c);
        return false;
    }
    /* reinicia o controlador (o firmware pode tê-lo deixado ligado) */
    rw(c, R_CC, rr(c, R_CC) & ~CC_EN);
    if (!wait_ready(c, false)) { kprintf("nvme%d: não desligou\n", c->idx); kfree(c); return false; }
    u32 qd = MIN(QDEPTH, mqes);
    if (qd < 2) { kfree(c); return false; }
    c->buf = dma_alloc(BOUNCE_BYTES, &c->buf_phys);
    c->prpl = dma_alloc(4096, &c->prpl_phys);
    if (!c->buf || !c->prpl || !alloc_queue(&c->admin, 0, (u16)qd)) { kfree(c); return false; }
    for (u32 i = 1; i < BOUNCE_BYTES / 4096; i++) c->prpl[i - 1] = c->buf_phys + (u64)i * 4096;
    c->dprpl = dma_alloc(4096, &c->dprpl_phys);      /* opcional: sem ela, tudo passa pelo bounce */
    rw(c, R_AQA, (qd - 1) << 16 | (qd - 1));
    rw64(c, R_ASQ, c->admin.sq_phys);
    rw64(c, R_ACQ, c->admin.cq_phys);
    rw(c, R_INTMS, 0xFFFFFFFFu);
    rw(c, R_CC, CC_EN | (6u << 16) | (4u << 20));   /* NVM, páginas de 4 KiB, SQE 64 B, CQE 16 B */
    if (!wait_ready(c, true)) { kprintf("nvme%d: não ficou pronto (CSTS %08x)\n", c->idx, rr(c, R_CSTS)); kfree(c); return false; }

    if (!identify(c, 1, 0)) { kprintf("nvme%d: IDENTIFY do controlador falhou\n", c->idx); kfree(c); return false; }
    copy_str(c->serial, c->buf + 4, 20);
    copy_str(c->model, c->buf + 24, 40);
    copy_str(c->fw, c->buf + 64, 8);
    u8 mdts = c->buf[77];
    c->max_xfer = BOUNCE_BYTES;
    if (mdts && mdts < 20 && (4096u << mdts) < c->max_xfer) c->max_xfer = 4096u << mdts;
    c->dmax = 1u << 20;                               /* <= 257 entradas: a lista nunca enche a página */
    if (mdts && mdts < 20 && (4096u << mdts) < c->dmax) c->dmax = 4096u << mdts;
    u32 nn = le32(c->buf + 516);
    c->sgl = (le32(c->buf + 536) & 3u) != 0;

    /* A MSI usa o LAPIC, que o smp_init ainda não mapeou. Até lá, uma fila do BSP em polling. */
    c->intr = NVME_INTR_POLL;

    /* quantas filas de E/S o controlador concede (uma por núcleo, teto NVME_MAX_IOQ) */
    u32 want = NVME_MAX_IOQ;
    sqe_t cmd = {0};
    u32 dw0 = 0;
    cmd.cdw[0] = ADM_SET_FEAT;
    cmd.cdw[10] = FEAT_NQUEUES;
    cmd.cdw[11] = ((want - 1) << 16) | (want - 1);
    if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, &dw0) == 0) {
        u32 nsq = (dw0 & 0xFFFF) + 1, ncq = ((dw0 >> 16) & 0xFFFF) + 1;
        if (nsq < want) want = nsq;
        if (ncq < want) want = ncq;
    } else want = 1;
    if (want < 1) want = 1;
    if (want > NVME_MAX_IOQ) want = NVME_MAX_IOQ;
    c->granted = (int)want;

    u32 ien = 0;                                  /* polling até o nvme_smp_ready */
    want = 1;                                     /* o conjunto por CPU nasce depois do SMP */
    for (u32 q = 0; q < want; q++) {
        if (!alloc_queue(&c->io[q], (u16)(q + 1), (u16)qd)) { kprintf("nvme%d: sem memória para a fila %u\n", c->idx, q + 1); kfree(c); return false; }
        memset(&cmd, 0, sizeof cmd);
        cmd.cdw[0] = ADM_CREATE_CQ;
        cmd.cdw[6] = (u32)c->io[q].cq_phys; cmd.cdw[7] = (u32)(c->io[q].cq_phys >> 32);
        cmd.cdw[10] = (qd - 1) << 16 | (q + 1);
        cmd.cdw[11] = ien | 1;                    /* fisicamente contígua, vetor 0 */
        if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL)) { kprintf("nvme%d: criar CQ %u falhou\n", c->idx, q + 1); kfree(c); return false; }
        memset(&cmd, 0, sizeof cmd);
        cmd.cdw[0] = ADM_CREATE_SQ;
        cmd.cdw[6] = (u32)c->io[q].sq_phys; cmd.cdw[7] = (u32)(c->io[q].sq_phys >> 32);
        cmd.cdw[10] = (qd - 1) << 16 | (q + 1);
        cmd.cdw[11] = ((q + 1) << 16) | 1;        /* CQ irmã, contígua */
        if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL)) { kprintf("nvme%d: criar SQ %u falhou\n", c->idx, q + 1); kfree(c); return false; }
    }
    c->nio = (int)want;
    c->qd = qd;
    nvme_flights_init(&c->flight, (int)qd > 1 ? (int)qd - 1 : 1);

    kprintf("nvme%d: %s, série %s, fw %s, NVMe %u.%u, %u namespace(s), %d fila(s) de E/S, %s, até %u KiB por comando\n",
            c->idx, c->model, c->serial, c->fw, vs >> 16, (vs >> 8) & 0xFF, nn, c->nio,
            c->intr == NVME_INTR_MSIX ? "MSI-X" : c->intr == NVME_INTR_MSI ? "MSI" : "polling", c->max_xfer >> 10);
    ctrls[nctrls++] = c;

    /* namespaces ativos (CNS 2); controladores 1.0 não têm a lista: tenta 1..nn */
    u32 list[MAX_NS] = {0};
    int nl = 0;
    if (vs >= 0x10100 && identify(c, 2, 0))
        for (int i = 0; i < 1024 && nl < MAX_NS; i++) { u32 id = le32(c->buf + i * 4); if (!id) break; list[nl++] = id; }
    else
        for (u32 i = 1; i <= nn && nl < MAX_NS; i++) list[nl++] = i;
    for (int i = 0; i < nl; i++) add_namespace(c, list[i]);
    return true;
}

void nvme_init(void) {
    for (int nth = 0; nctrls < MAX_CTRL; nth++) {
        pci_dev_t *pd = pci_find_class(1, 8, nth);
        if (!pd) break;
        if (pd->progif == 2) setup(pd);
    }
}

static bool delete_ioq(nvme_ctrl_t *c, u16 qid) {
    sqe_t cmd = {0};
    cmd.cdw[0] = 0x00;                 /* Delete I/O Submission Queue */
    cmd.cdw[10] = qid;
    if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL)) return false;
    memset(&cmd, 0, sizeof cmd);
    cmd.cdw[0] = 0x04;                 /* Delete I/O Completion Queue */
    cmd.cdw[10] = qid;
    return submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL) == 0;
}

static bool create_ioq(nvme_ctrl_t *c, u32 q, u32 ien) {
    u32 qd = c->qd;
    if (!qd) return false;
    if (!c->io[q].sq) {
        if (!alloc_queue(&c->io[q], (u16)(q + 1), (u16)qd)) return false;
    } else {
        c->io[q].head = c->io[q].tail = 0;
        c->io[q].phase = 1;
        c->io[q].qid = (u16)(q + 1);
    }
    sqe_t cmd = {0};
    cmd.cdw[0] = ADM_CREATE_CQ;
    cmd.cdw[6] = (u32)c->io[q].cq_phys;
    cmd.cdw[7] = (u32)(c->io[q].cq_phys >> 32);
    cmd.cdw[10] = (qd - 1) << 16 | (q + 1);
    cmd.cdw[11] = ien | 1;
    if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL)) return false;
    memset(&cmd, 0, sizeof cmd);
    cmd.cdw[0] = ADM_CREATE_SQ;
    cmd.cdw[6] = (u32)c->io[q].sq_phys;
    cmd.cdw[7] = (u32)(c->io[q].sq_phys >> 32);
    cmd.cdw[10] = (qd - 1) << 16 | (q + 1);
    cmd.cdw[11] = ((q + 1) << 16) | 1;
    return submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL) == 0;
}

void nvme_smp_ready(void) {
    for (int i = 0; i < nctrls; i++) {
        nvme_ctrl_t *c = ctrls[i];
        if (!c) continue;
        int n = nvme_ioq_count(1, smp_cpu_count(), c->granted > 0 ? c->granted : 1, NVME_MAX_IOQ);
        if (n < 1) n = 1;
        u32 lo = 0, hi = 0, data = 0;
        int msg = nvme_msi_compose(smp_lapic_base(), smp_apic_id(0), smp_x2apic(), MSI_VECTOR, &lo, &hi, &data);
        int mode = NVME_INTR_POLL;
        if (!msg && c->pci) {
            u8 off = 0;
            if (pci_find_cap(c->pci, 0x11, &off) && arm_msix(c->pci, lo, hi, data)) mode = NVME_INTR_MSIX;
            else if (pci_find_cap(c->pci, 0x05, &off) && arm_msi(c->pci, lo, hi, data)) mode = NVME_INTR_MSI;
        }
        if (mode != NVME_INTR_POLL && !nvme_flight_outstanding(&c->flight)) {
            int old = c->nio;
            for (int q = 0; q < old; q++) delete_ioq(c, (u16)(q + 1));
            c->nio = 0;
            int made = 0;
            for (int q = 0; q < n && q < NVME_MAX_IOQ; q++) {
                if (!create_ioq(c, (u32)q, 1u << 1)) break;
                made++;
            }
            if (made) {
                c->nio = made;
                c->intr = mode;
                msi_set_handler(nvme_on_irq);
            } else if (!create_ioq(c, 0, 0)) {
                kprintf("nvme%d: fila de E/S perdida ao recriar\n", c->idx);
            } else c->nio = 1;
        }
        kprintf("nvme%d: %d fila(s) de E/S após o SMP, %s, mensagem %08x:%08x\n",
                c->idx, c->nio,
                c->intr == NVME_INTR_MSIX ? "MSI-X" : c->intr == NVME_INTR_MSI ? "MSI" : "polling",
                hi, lo);
    }
}

int nvme_disk_count(void) {
    int n = 0;
    for (int i = 0; i < nctrls; i++) n += ctrls[i]->nns;
    return n;
}

/*
 * mOS — driver NVMe por polling: fila de administração + um par de filas de E/S por controlador,
 * IDENTIFY de controlador e namespaces, READ/WRITE/FLUSH com PRP sobre um bounce buffer contíguo.
 * Interrupções ficam desligadas (IEN = 0 nas filas, INTMS mascarado). Cada namespace ativo vira
 * um blkdev_t gravável registrado como "nvme<c>n<ns>".
 */
#include "storage.h"

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

#define QDEPTH       32            /* entradas por fila (só usamos uma por vez) */
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
    u16 tail, head, qid;
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
    queue_t admin, io;
    u16 cid;
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

static bool alloc_queue(queue_t *q, u16 qid) {
    q->sq = dma_alloc(QDEPTH * sizeof(sqe_t), &q->sq_phys);
    q->cq = dma_alloc(QDEPTH * sizeof(cqe_t), &q->cq_phys);
    q->tail = q->head = 0;
    q->phase = 1;
    q->qid = qid;
    return q->sq && q->cq;
}

/* envia um comando e espera a conclusão; devolve o campo de status (0 = sucesso) ou -1 em timeout */
static int submit(nvme_ctrl_t *c, queue_t *q, sqe_t *cmd, u64 timeout_ms, u32 *dw0) {
    u16 cid = ++c->cid;
    cmd->cdw[0] = (cmd->cdw[0] & 0xFFFF) | (u32)cid << 16;
    q->sq[q->tail] = *cmd;
    q->tail = (q->tail + 1) % QDEPTH;
    __asm__ volatile("mfence" ::: "memory");
    ring_sq(c, q);
    u64 end = timer_ticks() + timeout_ms;
    for (;;) {
        volatile cqe_t *e = &q->cq[q->head];
        if ((e->status & 1) == q->phase) {
            u16 st = e->status >> 1, ecid = e->cid;
            if (dw0) *dw0 = e->dw0;
            q->head = (q->head + 1) % QDEPTH;
            if (!q->head) q->phase ^= 1;
            ring_cq(c, q);
            if (ecid != cid) continue;           /* conclusão atrasada de um comando anterior */
            return st;
        }
        if (rr(c, R_CSTS) & CSTS_CFS) { kprintf("nvme%d: falha fatal do controlador\n", c->idx); return -1; }
        if (timer_ticks() > end) { kprintf("nvme%d: comando %02x expirou\n", c->idx, cmd->cdw[0] & 0xFF); return -1; }
        __asm__ volatile("pause");
    }
}

/* PRP1/PRP2 para bytes a partir do início do bounce (alinhado a página) */
static void set_prp(nvme_ctrl_t *c, sqe_t *cmd, u32 bytes) {
    cmd->cdw[6] = (u32)c->buf_phys;
    cmd->cdw[7] = (u32)(c->buf_phys >> 32);
    u64 prp2 = 0;
    if (bytes > 8192) prp2 = c->prpl_phys;
    else if (bytes > 4096) prp2 = c->buf_phys + 4096;
    cmd->cdw[8] = (u32)prp2;
    cmd->cdw[9] = (u32)(prp2 >> 32);
}

static bool identify(nvme_ctrl_t *c, u32 cns, u32 nsid) {
    sqe_t cmd = {0};
    cmd.cdw[0] = ADM_IDENTIFY;
    cmd.cdw[1] = nsid;
    cmd.cdw[10] = cns;
    memset(c->buf, 0, 4096);
    set_prp(c, &cmd, 4096);
    return submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL) == 0;
}

/* ---- E/S direto no buffer de quem chamou (memória identidade = física, contígua): sem bounce e sem
 * memcpy, comandos de até dmax bytes. PRP1 = endereço (alinhado a 4 bytes); demais páginas em PRP2
 * ou numa lista de PRP. */
static bool rw_direct(nvme_ns_t *ns, u64 lba, u64 n, u8 *data, bool write) {
    nvme_ctrl_t *c = ns->c;
    u32 maxb = c->dmax / ns->ss;
    while (n) {
        u32 k = (u32)MIN(n, (u64)maxb);
        u64 addr = (u64)data, bytes = (u64)k * ns->ss;
        u64 first = addr & ~4095ULL, npages = ((addr & 4095) + bytes + 4095) / 4096;
        sqe_t cmd = {0};
        cmd.cdw[0] = write ? IO_WRITE : IO_READ;
        cmd.cdw[1] = ns->nsid;
        cmd.cdw[6] = (u32)addr; cmd.cdw[7] = (u32)(addr >> 32);
        u64 prp2 = 0;
        if (npages == 2) prp2 = first + 4096;
        else if (npages > 2) {
            for (u64 i = 1; i < npages; i++) c->dprpl[i - 1] = first + i * 4096;
            prp2 = c->dprpl_phys;
        }
        cmd.cdw[8] = (u32)prp2; cmd.cdw[9] = (u32)(prp2 >> 32);
        cmd.cdw[10] = (u32)lba;
        cmd.cdw[11] = (u32)(lba >> 32);
        cmd.cdw[12] = k - 1;
        int st = submit(c, &c->io, &cmd, IO_TIMEOUT_MS, NULL);
        if (st) { kprintf("nvme%d: %s lba %llu falhou (status %x)\n", c->idx, write ? "escrita" : "leitura", lba, st); return false; }
        lba += k; n -= k; data += bytes;
    }
    return true;
}

/* ---- E/S por blocos (até max_xfer por comando) */
static bool rw_blocks(nvme_ns_t *ns, u64 lba, u64 n, u8 *data, bool write) {
    nvme_ctrl_t *c = ns->c;
    /* contenção de escrita: único ponto onde este driver emite NVMe Write (rw_direct só é chamado daqui) */
    if (write && !storage_write_ok(&ns->dev, lba * ns->ss, n * ns->ss)) return false;
    if (c->dprpl && !((u64)data & 3) && pmm_is_ram((u64)data, n * ns->ss) && n * ns->ss >= 4096)
        return rw_direct(ns, lba, n, data, write);
    u32 maxb = c->max_xfer / ns->ss;
    while (n) {
        u32 k = (u32)MIN(n, (u64)maxb);
        u32 bytes = k * ns->ss;
        if (write) memcpy(c->buf, data, bytes);
        sqe_t cmd = {0};
        cmd.cdw[0] = write ? IO_WRITE : IO_READ;
        cmd.cdw[1] = ns->nsid;
        set_prp(c, &cmd, bytes);
        cmd.cdw[10] = (u32)lba;
        cmd.cdw[11] = (u32)(lba >> 32);
        cmd.cdw[12] = k - 1;
        int st = submit(c, &c->io, &cmd, IO_TIMEOUT_MS, NULL);
        if (st) { kprintf("nvme%d: %s lba %llu falhou (status %x)\n", c->idx, write ? "escrita" : "leitura", lba, st); return false; }
        if (!write) memcpy(data, c->buf, bytes);
        lba += k; n -= k; data += bytes;
    }
    return true;
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
    return submit(ns->c, &ns->c->io, &cmd, IO_TIMEOUT_MS, NULL) == 0;
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
    c->buf = dma_alloc(BOUNCE_BYTES, &c->buf_phys);
    c->prpl = dma_alloc(4096, &c->prpl_phys);
    if (!c->buf || !c->prpl || !alloc_queue(&c->admin, 0) || !alloc_queue(&c->io, 1)) { kfree(c); return false; }
    for (u32 i = 1; i < BOUNCE_BYTES / 4096; i++) c->prpl[i - 1] = c->buf_phys + (u64)i * 4096;
    c->dprpl = dma_alloc(4096, &c->dprpl_phys);      /* opcional: sem ela, tudo passa pelo bounce */
    u32 qd = MIN(QDEPTH, mqes);
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

    /* fila de E/S 1 (CQ antes da SQ), sem interrupção */
    sqe_t cmd = {0};
    cmd.cdw[0] = ADM_CREATE_CQ;
    cmd.cdw[6] = (u32)c->io.cq_phys; cmd.cdw[7] = (u32)(c->io.cq_phys >> 32);
    cmd.cdw[10] = (qd - 1) << 16 | 1;
    cmd.cdw[11] = 1;                                  /* fisicamente contígua, IEN = 0 */
    if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL)) { kprintf("nvme%d: criar CQ falhou\n", c->idx); kfree(c); return false; }
    memset(&cmd, 0, sizeof cmd);
    cmd.cdw[0] = ADM_CREATE_SQ;
    cmd.cdw[6] = (u32)c->io.sq_phys; cmd.cdw[7] = (u32)(c->io.sq_phys >> 32);
    cmd.cdw[10] = (qd - 1) << 16 | 1;
    cmd.cdw[11] = 1u << 16 | 1;                       /* CQ 1, contígua */
    if (submit(c, &c->admin, &cmd, ADMIN_TIMEOUT_MS, NULL)) { kprintf("nvme%d: criar SQ falhou\n", c->idx); kfree(c); return false; }

    kprintf("nvme%d: %s, série %s, fw %s, NVMe %u.%u, %u namespace(s), até %u KiB por comando\n", c->idx,
            c->model, c->serial, c->fw, vs >> 16, (vs >> 8) & 0xFF, nn, c->max_xfer >> 10);
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

int nvme_disk_count(void) {
    int n = 0;
    for (int i = 0; i < nctrls; i++) n += ctrls[i]->nns;
    return n;
}

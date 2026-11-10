/* Teste de host: fila, PRP/SGL e NCQ contra um dispositivo só em RAM. Não toca disco físico. */
#include "nvme_xfer.h"
#include "ahci_ncq.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;

static void expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "falhou: %s\n", msg);
        fails++;
    }
}

enum { RAM = 65536, DISK = 32768, SS = 512 };

typedef struct { u32 cdw[16]; } sqe_t;
typedef struct { u32 dw0, dw1; u16 sqhd, sqid, cid, status; } cqe_t;

typedef struct {
    sqe_t sq[8];
    cqe_t cq[8];
    u16 head, tail, phase, depth;
    u16 cid_seen;
    u8 ram[RAM];
    u8 disk[DISK];
} nvdev_t;

static int copy_span(nvdev_t *d, u64 addr, u32 bytes, u32 disk_off, int to_disk) {
    if (addr + bytes > RAM || disk_off + bytes > DISK) return -1;
    if (to_disk) memcpy(d->disk + disk_off, d->ram + addr, bytes);
    else memcpy(d->ram + addr, d->disk + disk_off, bytes);
    return 0;
}

static int dma_prp(nvdev_t *d, u64 prp1, u64 prp2, u32 bytes, u32 disk_off, int to_disk) {
    u64 first = prp1 & ~(u64)(NVME_PAGE - 1);
    u64 last = (prp1 + bytes - 1) & ~(u64)(NVME_PAGE - 1);
    int npages = (int)((last - first) / NVME_PAGE) + 1;
    u32 left = bytes, off = disk_off;
    u64 addr = prp1;
    for (int i = 0; i < npages; i++) {
        u32 room = (u32)(NVME_PAGE - (addr & (NVME_PAGE - 1)));
        u32 take = left < room ? left : room;
        if (i == 0) addr = prp1;
        else if (npages == 2) addr = prp2;
        else {
            if (prp2 + (u64)(i - 1) * 8 + 8 > RAM) return -1;
            memcpy(&addr, d->ram + prp2 + (u64)(i - 1) * 8, 8);
        }
        if (copy_span(d, addr, take, off, to_disk)) return -1;
        off += take;
        left -= take;
        addr = (addr & ~(u64)(NVME_PAGE - 1)) + NVME_PAGE;
    }
    return left ? -1 : 0;
}

/* Consome a SQ. READ/WRITE opcode 2/1, 512 B por bloco. PSDT 0 = PRP, 1 = SGL. */
static int nv_run(nvdev_t *d) {
    int n = 0;
    while (d->head != d->tail) {
        sqe_t *c = &d->sq[d->head];
        u8 opc = (u8)(c->cdw[0] & 0xFF);
        int psdt = (c->cdw[0] >> 14) & 3;
        u32 lba = c->cdw[10];
        u32 bytes = ((c->cdw[12] & 0xFFFF) + 1) * SS;
        int wr = opc == 1;
        int rc;
        if (psdt == 1) {
            u64 addr = (u64)c->cdw[6] | ((u64)c->cdw[7] << 32);
            u32 len = c->cdw[8];
            u32 id = c->cdw[9] >> 24;
            if (id != 0 || len < bytes) return -1;
            rc = copy_span(d, addr, bytes, lba * SS, wr);
        } else if (psdt == 0) {
            u64 prp1 = (u64)c->cdw[6] | ((u64)c->cdw[7] << 32);
            u64 prp2 = (u64)c->cdw[8] | ((u64)c->cdw[9] << 32);
            rc = dma_prp(d, prp1, prp2, bytes, lba * SS, wr);
        } else return -1;
        cqe_t *e = &d->cq[d->head];
        e->cid = (u16)(c->cdw[0] >> 16);
        e->status = (u16)((rc ? (1u << 1) : 0) | d->phase);
        d->cid_seen = e->cid;
        d->head = (u16)((d->head + 1) % d->depth);
        if (!d->head) d->phase ^= 1;
        n++;
    }
    return n;
}

static void post(nvdev_t *d, sqe_t cmd) {
    d->sq[d->tail] = cmd;
    d->tail = (u16)((d->tail + 1) % d->depth);
}

static void test_prp_sgl_queue(void) {
    u64 list[8];
    nvme_prp_t m;
    expect(nvme_prp_build(100, 100, list, 8, &m) == 0 && m.prp1 == 100 && m.prp2 == 0 && m.list_entries == 0, "prp 1 pagina");
    expect(nvme_prp_build(4096, 8192, list, 8, &m) == 0 && m.prp2 == 8192 && m.list_entries == 0, "prp 2 paginas");
    expect(nvme_prp_build(0, 12288, list, 8, &m) == 0 && m.list_entries == 2 && list[0] == 4096 && list[1] == 8192, "prp lista");
    expect(nvme_prp_build(1, 16, list, 8, &m) != 0, "prp desalinhado");

    nvme_sgl_t s;
    expect(nvme_sgl_build(64, 1024, &s) == 0 && s.addr == 64 && s.len == 1024 && s.id == 0, "sgl");
    expect(nvme_sgl_build(64, 2, &s) != 0, "sgl tamanho");

    expect(nvme_q_for_cpu(0, 4) == 0, "fila cpu0");
    expect(nvme_q_for_cpu(1, 4) == 1, "fila cpu1");
    expect(nvme_q_for_cpu(5, 4) == 1, "fila cpu5");
    expect(nvme_q_for_cpu(3, 1) == 0, "uma fila");
    expect(nvme_intr_pick(1, 1, 1, 1) == NVME_INTR_MSIX, "prefere msix");
    expect(nvme_intr_pick(1, 0, 1, 1) == NVME_INTR_MSI, "cai para msi");
    expect(nvme_intr_pick(1, 0, 1, 0) == NVME_INTR_POLL, "cai para polling");

    nvdev_t dev;
    memset(&dev, 0, sizeof dev);
    dev.depth = 4;
    dev.phase = 1;
    for (int i = 0; i < 16; i++) dev.disk[i] = (u8)(0xA0 + i);

    sqe_t cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.cdw[0] = 2 | (7u << 16); /* read, cid 7 */
    nvme_prp_build(128, SS, list, 8, &m);
    cmd.cdw[6] = (u32)m.prp1;
    cmd.cdw[7] = (u32)(m.prp1 >> 32);
    cmd.cdw[10] = 0;
    cmd.cdw[12] = 0;
    post(&dev, cmd);
    expect(nv_run(&dev) == 1, "um cqe");
    expect(dev.cq[0].cid == 7 && (dev.cq[0].status & 1) == 1, "fase e cid");
    expect(memcmp(dev.ram + 128, dev.disk, SS) == 0, "leitura prp");

    memset(&cmd, 0, sizeof cmd);
    cmd.cdw[0] = 1 | (1u << 14) | (9u << 16); /* write SGL, cid 9 */
    nvme_sgl_build(2048, SS, &s);
    for (int i = 0; i < SS; i++) dev.ram[2048 + i] = (u8)i;
    cmd.cdw[6] = (u32)s.addr;
    cmd.cdw[7] = (u32)(s.addr >> 32);
    cmd.cdw[8] = s.len;
    cmd.cdw[9] = s.id << 24;
    cmd.cdw[10] = 2;
    cmd.cdw[12] = 0;
    post(&dev, cmd);
    expect(nv_run(&dev) == 1, "cqe sgl");
    expect(memcmp(dev.disk + 2 * SS, dev.ram + 2048, SS) == 0, "escrita sgl");

    /* três páginas via lista de PRP: o dispositivo lê a lista na RAM simulada */
    memset(&cmd, 0, sizeof cmd);
    cmd.cdw[0] = 2 | (3u << 16);
    u64 lst[4];
    nvme_prp_build(0, NVME_PAGE * 3, lst, 4, &m);
    expect(m.list_entries == 2 && lst[0] == 4096 && lst[1] == 8192, "lista de 3 paginas");
    memcpy(dev.ram + 32768, lst, (size_t)m.list_entries * 8);
    for (u32 i = 0; i < NVME_PAGE * 3; i++) dev.disk[i] = (u8)(i * 3);
    cmd.cdw[6] = 0;
    cmd.cdw[8] = 32768;
    cmd.cdw[10] = 0;
    cmd.cdw[12] = (NVME_PAGE * 3) / SS - 1;
    post(&dev, cmd);
    expect(nv_run(&dev) == 1, "cqe prp lista");
    expect(memcmp(dev.ram, dev.disk, NVME_PAGE * 3) == 0, "leitura 3 paginas");
}

static void test_ncq(void) {
    ahci_tags_t t;
    ahci_tags_init(&t, 32);
    int got[32];
    for (int i = 0; i < 32; i++) {
        got[i] = ahci_tag_get(&t);
        expect(got[i] == i, "tag em ordem");
    }
    expect(ahci_tag_get(&t) < 0, "33a tag recusada");
    ahci_tag_put(&t, 4);
    expect(ahci_tag_get(&t) == 4, "tag 4 de novo");
    ahci_tag_put(&t, 4);
    for (int i = 0; i < 32; i++) if (i != 4) ahci_tag_put(&t, i);

    u8 fis[20];
    ahci_fpdma_fis(fis, 1, 0x1122334455ull, 8, 5);
    expect(fis[0] == 0x27 && fis[1] == 0x80 && fis[2] == ATA_WRITE_FPDMA, "fis tipo");
    expect(fis[3] == 8 && fis[11] == 0 && fis[12] == (5 << 3), "contagem e tag");
    expect(fis[4] == 0x55 && fis[9] == 0x11 && fis[10] == 0, "lba");

    u8 ctl[16384];
    u8 disk[8192];
    u8 ram[8192];
    u32 ci = 0, sact = 0;
    memset(ctl, 0, sizeof ctl);
    memset(disk, 0, sizeof disk);
    memset(ram, 0x5A, sizeof ram);
    for (int i = 0; i < 512; i++) ram[100 + i] = (u8)(i ^ 0x3C);

    int tags[2] = {0, 3};
    for (int k = 0; k < 2; k++) {
        int tag = tags[k];
        u8 *ct = ctl + 0x1000 + tag * 0x100;
        ahci_fpdma_fis(ct, 1, (u64)k, 1, (u8)tag);
        u32 *prd = (u32 *)(ct + 0x80);
        prd[0] = 100;
        prd[3] = 511;
        ci |= 1u << tag;
        sact |= 1u << tag;
    }
    expect((ci & sact) == ci && __builtin_popcount(ci) == 2, "dois comandos ativos");

    for (int tag = 0; tag < 32; tag++) {
        if (!(ci & (1u << tag))) continue;
        u8 *ct = ctl + 0x1000 + tag * 0x100;
        expect(ct[2] == ATA_WRITE_FPDMA, "fpdma write");
        expect((ct[12] >> 3) == tag, "tag no fis");
        u16 count = (u16)(ct[3] | (ct[11] << 8));
        u64 lba = (u64)ct[4] | ((u64)ct[5] << 8) | ((u64)ct[6] << 16) |
                  ((u64)ct[8] << 24) | ((u64)ct[9] << 32) | ((u64)ct[10] << 40);
        u32 dba = *(u32 *)(ct + 0x80);
        u32 bytes = (u32)count * 512;
        memcpy(disk + lba * 512, ram + dba, bytes);
        ci &= ~(1u << tag);
        sact &= ~(1u << tag);
    }
    expect(ci == 0 && sact == 0, "tags liberadas");
    expect(memcmp(disk, ram + 100, 512) == 0, "setor 0");
    expect(memcmp(disk + 512, ram + 100, 512) == 0, "setor 1");

    /* leitura FPDMA do setor 0 para outro buffer */
    u8 *ct = ctl + 0x1000;
    memset(ct, 0, 256);
    ahci_fpdma_fis(ct, 0, 0, 1, 0);
    *(u32 *)(ct + 0x80) = 2000;
    ci = 1;
    memcpy(ram + 2000, disk, 512);
    expect(ct[2] == ATA_READ_FPDMA && memcmp(ram + 2000, ram + 100, 512) == 0, "leitura fpdma");
    (void)ci;
}

int main(void) {
    test_prp_sgl_queue();
    test_ncq();
    if (fails) {
        fprintf(stderr, "%d falha(s)\n", fails);
        return 1;
    }
    printf("disktest: ok\n");
    return 0;
}

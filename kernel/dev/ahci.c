/*
 * mOS — driver AHCI (SATA) por polling: IDENTIFY, READ/WRITE DMA EXT (LBA48), FLUSH CACHE EXT.
 * Cada disco SATA vira um blkdev_t gravável e é sondado pelo dfs (GPT/APM/MBR -> HFS+/APFS).
 *
 * Sequência de inicialização e recuperação de porta adaptadas do RavynAHCIPort do ravynOS
 * (Kernel/Extensions/RavynAHCIPort, Copyright (C) 2026 Zoe Knox, licença MIT):
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 *   and associated documentation files (the "Software"), to deal in the Software without restriction,
 *   including without limitation the rights to use, copy, modify, merge, publish, distribute,
 *   sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions: The above copyright notice and this
 *   permission notice shall be included in all copies or substantial portions of the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */
#include "storage.h"

/* ---- registradores do HBA */
#define HBA_CAP   0x00
#define HBA_GHC   0x04
#define HBA_IS    0x08
#define HBA_PI    0x0C
#define HBA_VS    0x10
#define HBA_CAP2  0x24
#define HBA_BOHC  0x28
#define CAP_S64A  (1u << 31)
#define CAP_SSS   (1u << 27)
#define GHC_AE    (1u << 31)
#define GHC_IE    (1u << 1)
#define CAP2_BOH  (1u << 0)
#define BOHC_BOS  (1u << 0)
#define BOHC_OOS  (1u << 1)
#define BOHC_BB   (1u << 4)

/* ---- registradores de porta (0x100 + 0x80 * n) */
#define P_CLB   0x00
#define P_CLBU  0x04
#define P_FB    0x08
#define P_FBU   0x0C
#define P_IS    0x10
#define P_IE    0x14
#define P_CMD   0x18
#define P_TFD   0x20
#define P_SIG   0x24
#define P_SSTS  0x28
#define P_SCTL  0x2C
#define P_SERR  0x30
#define P_CI    0x38
#define CMD_ST  (1u << 0)
#define CMD_SUD (1u << 1)
#define CMD_POD (1u << 2)
#define CMD_FRE (1u << 4)
#define CMD_FR  (1u << 14)
#define CMD_CR  (1u << 15)
#define CMD_CPD (1u << 20)
#define IS_TFES (1u << 30)
#define TFD_ERR 0x01
#define TFD_DRQ 0x08
#define TFD_BSY 0x80
#define SIG_SATA 0x00000101u

/* ---- comandos ATA */
#define ATA_IDENTIFY      0xEC
#define ATA_READ_DMA_EXT  0x25
#define ATA_WRITE_DMA_EXT 0x35
#define ATA_FLUSH_EXT     0xEA

#define CMD_TIMEOUT_MS 5000
#define BOUNCE_BYTES   (128u << 10)

/* layout da página de controle de cada porta */
#define OFF_CLB 0x000   /* 32 cabeçalhos x 32 bytes (usamos só o slot 0) */
#define OFF_FIS 0x400   /* FIS recebidos, 256 bytes */
#define OFF_CT  0x800   /* tabela de comando: CFIS em 0x00, PRDT em 0x80 */

typedef struct {
    u8  flags, flags2;  /* CFL (dwords do CFIS) | W=bit6 */
    u16 prdtl;
    volatile u32 prdbc;
    u32 ctba, ctbau;
    u32 rsv[4];
} cmd_hdr_t;

typedef struct { u32 dba, dbau, rsv, dbc; } prd_t;

typedef struct {
    int  port;
    u8  *ctl;  u64 ctl_phys;     /* página de controle */
    u8  *buf;  u64 buf_phys;     /* bounce de DMA */
    u64  sectors;
    u32  ss;                     /* bytes por setor lógico */
    bool lba48;
    char model[41], serial[21], fw[9];
    char desc[96];
    blkdev_t dev;
} ahci_disk_t;

static volatile u8 *abar;
static u32 hba_cap;
static ahci_disk_t *disks[8];
static int ndisks;
static char status[96] = "Nenhum controlador AHCI";

static inline u32 hr(u32 off) { return mmio_r32(abar, off); }
static inline void hw(u32 off, u32 v) { mmio_w32(abar, off, v); }
static inline u32 pr(int p, u32 off) { return mmio_r32(abar, 0x100 + 0x80 * p + off); }
static inline void pw(int p, u32 off, u32 v) { mmio_w32(abar, 0x100 + 0x80 * p + off, v); }

static bool wait_clear(int p, u32 reg, u32 mask, u32 ms) {
    u64 end = timer_ticks() + ms;
    while (pr(p, reg) & mask)
        if (timer_ticks() > end) return false;
        else __asm__ volatile("pause");
    return true;
}

static bool stop_engine(int p) {
    pw(p, P_CMD, pr(p, P_CMD) & ~CMD_ST);
    if (!wait_clear(p, P_CMD, CMD_CR, 500)) return false;
    pw(p, P_CMD, pr(p, P_CMD) & ~CMD_FRE);
    return wait_clear(p, P_CMD, CMD_FR, 500);
}

static void start_engine(int p) {
    wait_clear(p, P_CMD, CMD_CR, 500);
    pw(p, P_CMD, pr(p, P_CMD) | CMD_FRE);
    pw(p, P_CMD, pr(p, P_CMD) | CMD_ST);
}

/* COMRESET: DET=1 por >1 ms, depois DET=0 e espera o link voltar */
static bool reset_port(int p) {
    u32 sctl = pr(p, P_SCTL) & ~0xFu;
    pw(p, P_SCTL, sctl | 1);
    sleep_ms(5);
    pw(p, P_SCTL, sctl);
    u64 end = timer_ticks() + 1000;
    while ((pr(p, P_SSTS) & 0xF) != 3)
        if (timer_ticks() > end) return false;
    pw(p, P_SERR, 0xFFFFFFFFu);
    return true;
}

/* um comando travado deixa o slot 0 ocupado para sempre: reinicia a porta */
static void recover_port(int p) {
    stop_engine(p);
    if (!reset_port(p)) kprintf("ahci: porta %d não voltou após reset\n", p);
    pw(p, P_SERR, 0xFFFFFFFFu);
    pw(p, P_IS, 0xFFFFFFFFu);
    start_engine(p);
}

/* executa um comando no slot 0 com DMA em `phys` (bounce ou o próprio buffer; bytes <= 4 MiB, par) */
static bool issue_at(ahci_disk_t *d, u8 cmd, u64 lba, u16 count, u64 phys, u32 bytes, bool write) {
    int p = d->port;
    if (!wait_clear(p, P_TFD, TFD_BSY | TFD_DRQ, 1000)) { recover_port(p); return false; }
    if (pr(p, P_CI) & 1) { recover_port(p); return false; }

    cmd_hdr_t *h = (cmd_hdr_t *)(d->ctl + OFF_CLB);
    u8 *ct = d->ctl + OFF_CT;
    memset(h, 0, sizeof(*h));
    memset(ct, 0, 0x100);
    h->flags = 5 | (write ? 0x40 : 0);           /* CFIS de 5 dwords */
    h->prdtl = bytes ? 1 : 0;
    h->ctba = (u32)(d->ctl_phys + OFF_CT);
    h->ctbau = (u32)((d->ctl_phys + OFF_CT) >> 32);
    if (bytes) {
        prd_t *prd = (prd_t *)(ct + 0x80);
        prd->dba = (u32)phys;
        prd->dbau = (u32)(phys >> 32);
        prd->dbc = bytes - 1;
    }
    u8 *f = ct;                                  /* FIS Register H2D */
    f[0] = 0x27;
    f[1] = 0x80;                                 /* C: é um comando */
    f[2] = cmd;
    f[7] = 0x40;                                 /* modo LBA */
    f[4] = (u8)lba;        f[5] = (u8)(lba >> 8);  f[6] = (u8)(lba >> 16);
    f[8] = (u8)(lba >> 24); f[9] = (u8)(lba >> 32); f[10] = (u8)(lba >> 40);
    f[12] = (u8)count;     f[13] = (u8)(count >> 8);

    pw(p, P_IS, 0xFFFFFFFFu);
    pw(p, P_SERR, 0xFFFFFFFFu);
    __asm__ volatile("mfence" ::: "memory");
    pw(p, P_CI, 1);

    u64 end = timer_ticks() + CMD_TIMEOUT_MS;
    for (;;) {
        u32 is = pr(p, P_IS);
        if (is & IS_TFES) {
            kprintf("ahci: comando %02x falhou (porta %d, lba %llu, TFD %08x)\n", cmd, p, lba, pr(p, P_TFD));
            recover_port(p);
            return false;
        }
        if (!(pr(p, P_CI) & 1)) break;
        if (timer_ticks() > end) {
            kprintf("ahci: comando %02x expirou (porta %d, lba %llu)\n", cmd, p, lba);
            recover_port(p);
            return false;
        }
        __asm__ volatile("pause");
    }
    __asm__ volatile("mfence" ::: "memory");
    return !(pr(p, P_TFD) & TFD_ERR);
}

static bool issue(ahci_disk_t *d, u8 cmd, u64 lba, u16 count, u32 bytes, bool write) {
    return issue_at(d, cmd, lba, count, d->buf_phys, bytes, write);
}

/* ---- E/S por setores */
#define DIRECT_MAX (4u << 20)                        /* teto de um PRD; LBA48 conta até 65536 setores */

static bool rw_sectors(ahci_disk_t *d, u64 lba, u64 n, u8 *data, bool write) {
    /* DMA direto no buffer de quem chamou (identidade = físico): sem bounce e sem memcpy, comandos de
     * até 4 MiB. Exige endereço par e, sem S64A, abaixo de 4 GiB. */
    u64 total = n * d->ss;
    if (d->lba48 && total >= 8192 && !((u64)data & 1) && pmm_is_ram((u64)data, total) &&
        ((hba_cap & CAP_S64A) || (u64)data + total <= (1ull << 32))) {
        u32 maxd = MIN(DIRECT_MAX / d->ss, 65536u);
        while (n) {
            u32 c = (u32)MIN(n, (u64)maxd);
            u32 bytes = c * d->ss;
            if (!issue_at(d, write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT, lba, (u16)(c == 65536 ? 0 : c), (u64)data, bytes, write)) return false;
            lba += c; n -= c; data += bytes;
        }
        return true;
    }
    u32 max = BOUNCE_BYTES / d->ss;
    while (n) {
        u32 c = (u32)MIN(n, (u64)max);
        u32 bytes = c * d->ss;
        if (write) memcpy(d->buf, data, bytes);
        if (!issue(d, write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT, lba, (u16)(c == 65536 ? 0 : c), bytes, write)) return false;
        if (!write) memcpy(data, d->buf, bytes);
        lba += c; n -= c; data += bytes;
    }
    return true;
}

/* ---- interface blkdev (deslocamentos em bytes, qualquer alinhamento) */
static bool disk_read(blkdev_t *bd, u64 off, void *out, u64 len) {
    ahci_disk_t *d = bd->ctx;
    u8 *o = out;
    u8 *tmp = NULL;
    while (len) {
        u64 lba = off / d->ss, in = off % d->ss;
        if (in || len < d->ss) {                 /* pedaço desalinhado: lê o setor inteiro */
            if (!tmp && !(tmp = kmalloc(d->ss))) return false;
            if (!rw_sectors(d, lba, 1, tmp, false)) { kfree(tmp); return false; }
            u64 k = MIN(len, d->ss - in);
            memcpy(o, tmp + in, k);
            o += k; off += k; len -= k;
        } else {
            u64 n = len / d->ss;
            if (!rw_sectors(d, lba, n, o, false)) { kfree(tmp); return false; }
            o += n * d->ss; off += n * d->ss; len -= n * d->ss;
        }
    }
    kfree(tmp);
    return true;
}

static bool disk_write(blkdev_t *bd, u64 off, const void *src, u64 len) {
    ahci_disk_t *d = bd->ctx;
    const u8 *s = src;
    u8 *tmp = NULL;
    while (len) {
        u64 lba = off / d->ss, in = off % d->ss;
        if (in || len < d->ss) {                 /* ler-modificar-gravar */
            if (!tmp && !(tmp = kmalloc(d->ss))) return false;
            if (!rw_sectors(d, lba, 1, tmp, false)) { kfree(tmp); return false; }
            u64 k = MIN(len, d->ss - in);
            memcpy(tmp + in, s, k);
            if (!rw_sectors(d, lba, 1, tmp, true)) { kfree(tmp); return false; }
            s += k; off += k; len -= k;
        } else {
            u64 n = len / d->ss;
            if (!rw_sectors(d, lba, n, (u8 *)s, true)) { kfree(tmp); return false; }
            s += n * d->ss; off += n * d->ss; len -= n * d->ss;
        }
    }
    kfree(tmp);
    return true;
}

static bool disk_flush(blkdev_t *bd) {
    ahci_disk_t *d = bd->ctx;
    return issue(d, ATA_FLUSH_EXT, 0, 0, 0, false);
}

/* ---- IDENTIFY */
static void ata_string(char *dst, const u16 *w, int words) {
    int o = 0;
    for (int i = 0; i < words; i++) { dst[o++] = (char)(w[i] >> 8); dst[o++] = (char)w[i]; }
    while (o && dst[o - 1] == ' ') o--;
    dst[o] = 0;
    int s = 0;
    while (dst[s] == ' ') s++;
    if (s) memmove(dst, dst + s, o - s + 1);
}

static bool identify(ahci_disk_t *d) {
    if (!issue(d, ATA_IDENTIFY, 0, 0, 512, false)) return false;
    const u16 *id = (const u16 *)d->buf;
    ata_string(d->serial, id + 10, 10);
    ata_string(d->fw, id + 23, 4);
    ata_string(d->model, id + 27, 20);
    d->lba48 = id[83] & (1u << 10);
    d->sectors = d->lba48 ? (u64)id[100] | (u64)id[101] << 16 | (u64)id[102] << 32 | (u64)id[103] << 48
                          : (u64)id[60] | (u64)id[61] << 16;
    d->ss = 512;
    if ((id[106] & 0xC000) == 0x4000 && (id[106] & (1u << 12))) {
        u32 words = (u32)id[117] | (u32)id[118] << 16;
        if (words >= 256 && words <= 8192) d->ss = words * 2;
    }
    return true;
}

static bool setup_port(int p) {
    ahci_disk_t *d = kcalloc(1, sizeof(*d));
    if (!d) return false;
    d->port = p;
    d->ctl = dma_alloc(4096, &d->ctl_phys);
    d->buf = dma_alloc(BOUNCE_BYTES, &d->buf_phys);
    if (!d->ctl || !d->buf) goto fail;
    if (!(hba_cap & CAP_S64A) && ((d->ctl_phys | d->buf_phys) + BOUNCE_BYTES) >> 32) {
        kprintf("ahci: porta %d: HBA sem endereçamento de 64 bits e memória DMA acima de 4 GiB\n", p);
        goto fail;
    }
    /* liga o dispositivo (spin-up escalonado / alimentação de porta) */
    u32 cmd = pr(p, P_CMD);
    if (hba_cap & CAP_SSS) cmd |= CMD_SUD;
    if (cmd & CMD_CPD) cmd |= CMD_POD;
    pw(p, P_CMD, cmd);
    if (hba_cap & CAP_SSS) sleep_ms(10);

    if (!stop_engine(p)) { kprintf("ahci: porta %d: motor não parou\n", p); goto fail; }
    pw(p, P_CLB, (u32)(d->ctl_phys + OFF_CLB));
    pw(p, P_CLBU, (u32)((d->ctl_phys + OFF_CLB) >> 32));
    pw(p, P_FB, (u32)(d->ctl_phys + OFF_FIS));
    pw(p, P_FBU, (u32)((d->ctl_phys + OFF_FIS) >> 32));
    pw(p, P_IE, 0);
    pw(p, P_IS, 0xFFFFFFFFu);
    pw(p, P_SERR, 0xFFFFFFFFu);
    if (!wait_clear(p, P_TFD, TFD_BSY | TFD_DRQ, 1000)) {
        if (!reset_port(p) || !wait_clear(p, P_TFD, TFD_BSY | TFD_DRQ, 2000)) {
            kprintf("ahci: porta %d: dispositivo ocupado (TFD %08x)\n", p, pr(p, P_TFD));
            goto fail;
        }
    }
    start_engine(p);
    if (!identify(d)) { kprintf("ahci: porta %d: IDENTIFY falhou\n", p); goto fail; }
    if (!d->lba48) { kprintf("ahci: porta %d: %s sem LBA48, ignorado\n", p, d->model); goto fail; }

    u64 bytes = d->sectors * d->ss;
    snprintf(d->desc, sizeof d->desc, "%s (%llu MB)", d->model[0] ? d->model : "Disco SATA", bytes >> 20);
    d->dev.size = bytes;
    d->dev.read = disk_read;
    d->dev.write = disk_write;
    d->dev.flush = disk_flush;
    d->dev.ctx = d;
    disks[ndisks++] = d;
    kprintf("ahci: porta %d: %s, série %s, fw %s, %llu setores de %u bytes\n",
            p, d->model, d->serial, d->fw, d->sectors, d->ss);
    char name[16];
    snprintf(name, sizeof name, "sata%d", p);
    storage_register(&d->dev, name, d->desc);
    return true;
fail:
    if (d->ctl) pmm_free(d->ctl_phys, 1);
    if (d->buf) pmm_free(d->buf_phys, BOUNCE_BYTES / 4096);
    kfree(d);
    return false;
}

/* firmware ainda pode ser dono do HBA (BIOS/OS handoff) */
static void take_ownership(void) {
    if (!(hr(HBA_CAP2) & CAP2_BOH)) return;
    hw(HBA_BOHC, hr(HBA_BOHC) | BOHC_OOS);
    u64 end = timer_ticks() + 50;
    while ((hr(HBA_BOHC) & BOHC_BOS) && timer_ticks() < end) __asm__ volatile("pause");
    if (hr(HBA_BOHC) & BOHC_BB) sleep_ms(2000);   /* firmware pediu mais tempo */
}

static bool selftest(ahci_disk_t *d);

void ahci_init(void) {
    for (int nth = 0;; nth++) {
        pci_dev_t *pd = pci_find_class(1, 6, nth);
        if (!pd) break;
        if (pd->progif != 1) continue;
        u64 base = pci_bar_addr(pd, 5);
        if (!base) { kprintf("ahci: %02x:%02x.%x sem ABAR\n", pd->bus, pd->dev, pd->fn); continue; }
        pci_enable_bus_master(pd);
        vmm_map_mmio(base, 0x2000);
        abar = (volatile u8 *)base;
        take_ownership();
        hw(HBA_GHC, hr(HBA_GHC) | GHC_AE);
        hw(HBA_GHC, hr(HBA_GHC) & ~GHC_IE);       /* só polling por enquanto */
        hba_cap = hr(HBA_CAP);
        u32 pi = hr(HBA_PI), vs = hr(HBA_VS);
        kprintf("ahci: controlador %04x:%04x AHCI %u.%u, %u portas, ABAR 0x%llx\n", pd->vendor, pd->device,
                vs >> 16, (vs >> 8) & 0xFF, (hba_cap & 0x1F) + 1, base);
        for (int p = 0; p < 32 && ndisks < (int)ARRAY_LEN(disks); p++) {
            if (!(pi & (1u << p))) continue;
            u32 ssts = pr(p, P_SSTS), sig = pr(p, P_SIG);
            if ((ssts & 0xF) != 3) continue;      /* nada conectado */
            if (sig != SIG_SATA) { kprintf("ahci: porta %d: assinatura %08x (não é disco), ignorada\n", p, sig); continue; }
            setup_port(p);
        }
        break; /* um controlador por enquanto (abar é global) */
    }
    if (abar) snprintf(status, sizeof status, ndisks == 1 ? "%d disco SATA" : "%d discos SATA", ndisks);
    for (int i = 0; i < ndisks; i++) selftest(disks[i]);
}

int ahci_disk_count(void) { return ndisks; }
const char *ahci_disk_desc(int i) { return i >= 0 && i < ndisks ? disks[i]->desc : ""; }
blkdev_t *ahci_disk(int i) { return i >= 0 && i < ndisks ? &disks[i]->dev : NULL; }
const char *ahci_status(void) { return status; }

/* Autoteste de escrita: só roda em discos cujo setor 0 começa com "mOS-AHCI-TEST" (imagem de teste
 * criada por tools\qemu-test.ps1 -AhciTest). Nunca toca em discos comuns. */
static bool selftest(ahci_disk_t *d) {
    static const char magic[] = "mOS-AHCI-TEST";
    u8 *s0 = kmalloc(d->ss);
    if (!s0) return false;
    bool ok = disk_read(&d->dev, 0, s0, d->ss) && !memcmp(s0, magic, sizeof magic - 1);
    kfree(s0);
    if (!ok) return false;
    const u64 n = 300 << 10;                     /* > bounce: força várias transferências */
    u8 *a = kmalloc(n), *b = kmalloc(n);
    bool pass = a && b;
    for (u64 i = 0; pass && i < n; i++) a[i] = (u8)(i * 7 + (i >> 9));
    pass = pass && disk_write(&d->dev, 4096 + 3, a, n) && disk_flush(&d->dev); /* desalinhado de propósito */
    if (pass) memset(b, 0, n);
    pass = pass && disk_read(&d->dev, 4096 + 3, b, n) && !memcmp(a, b, n);
    /* alinhado: setores inteiros num buffer par, pelo caminho de DMA direto (sem bounce) */
    if (pass) memset(b, 0, n);
    pass = pass && disk_write(&d->dev, 1 << 20, a, n) && disk_flush(&d->dev) &&
           disk_read(&d->dev, 1 << 20, b, n) && !memcmp(a, b, n) &&
           disk_read(&d->dev, 4096 + 3, b, 1000) && !memcmp(a, b, 1000);   /* e o trecho desalinhado continua lá */
    kprintf("ahci: autoteste de escrita na porta %d: %s\n", d->port, pass ? "OK" : "FALHOU");
    kfree(a); kfree(b);
    return pass;
}

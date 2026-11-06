/*
 * mOS — armazenamento em massa USB: Bulk-Only Transport (BBB) + comandos SCSI
 * (INQUIRY, TEST UNIT READY, REQUEST SENSE, READ CAPACITY 10/16, READ/WRITE 10/16).
 * API pública em usb.h (usb_msc_*).
 */
#include "usb.h"
#include "fs.h"

#define MAX_MSC  8
#define XFER_MAX 0x10000            /* 64 KiB por comando */

typedef struct {
    bool used, ready;
    usb_dev_t *d;
    u8   iface, ep_in, ep_out, lun;
    u32  tag, gen;
    u32  bsize;
    u64  blocks;
    bool wp;                        /* protegido contra escrita (MODE SENSE) */
    u8   last_key;                  /* sense key do último comando que falhou */
    blkdev_t dev;
    u8  *buf;                       /* DMA, XFER_MAX */
    u8  *cbw;                       /* DMA: CBW em +0, CSW em +64 */
    char name[48];
} msc_t;

static msc_t mscs[MAX_MSC];

static void reset_recovery(msc_t *m) {
    usb_control(m->d, 0x21, 0xFF, 0, m->iface, 0, NULL);      /* Bulk-Only Mass Storage Reset */
    usb_clear_halt(m->d, m->ep_in);
    usb_clear_halt(m->d, m->ep_out);
}

/* executa um comando; dados em m->buf.  Retorna bytes transferidos, ou <0; *csw_status = status SCSI */
static int bot(msc_t *m, const u8 *cdb, u8 cdblen, u32 len, bool in, u32 timeout, int *csw_status) {
    if (!m->d) return USB_NODEV;
    u8 *c = m->cbw;
    memset(c, 0, 31);
    u32 tag = ++m->tag;
    c[0] = 'U'; c[1] = 'S'; c[2] = 'B'; c[3] = 'C';
    memcpy(c + 4, &tag, 4);
    memcpy(c + 8, &len, 4);
    c[12] = in ? 0x80 : 0;
    c[13] = m->lun;
    c[14] = cdblen;
    memcpy(c + 15, cdb, cdblen);
    int r = usb_bulk(m->d, m->ep_out, c, 31, 3000);
    if (r != 31) { reset_recovery(m); return r < 0 ? r : USB_ERR; }
    int got = 0;
    if (len) {
        r = usb_bulk(m->d, in ? m->ep_in : m->ep_out, m->buf, len, timeout);
        if (r == USB_STALL) usb_clear_halt(m->d, in ? m->ep_in : m->ep_out);
        else if (r < 0) { reset_recovery(m); return r; }
        else got = r;
    }
    u8 *s = m->cbw + 64;
    r = usb_bulk(m->d, m->ep_in, s, 13, 3000);
    if (r == USB_STALL) { usb_clear_halt(m->d, m->ep_in); r = usb_bulk(m->d, m->ep_in, s, 13, 3000); }
    if (r != 13) { reset_recovery(m); return r < 0 ? r : USB_ERR; }
    u32 sig, stag;
    memcpy(&sig, s, 4); memcpy(&stag, s + 4, 4);
    if (sig != 0x53425355 || stag != tag || s[12] == 2) { reset_recovery(m); return USB_ERR; }
    if (csw_status) *csw_status = s[12];
    return s[12] == 0 ? got : USB_ERR;
}

static int sense(msc_t *m, u8 *key, u8 *asc) {
    u8 cdb[6] = {0x03, 0, 0, 0, 18, 0};
    int st = 0;
    int r = bot(m, cdb, 6, 18, true, 2000, &st);
    if (r < 0 && st == 0) return r;
    *key = m->buf[2] & 0xF; *asc = m->buf[12];
    return USB_OK;
}

static int scsi(msc_t *m, const u8 *cdb, u8 cdblen, u32 len, bool in, u32 timeout) {
    int r = USB_ERR;
    for (int t = 0; t < 3; t++) {
        int st = 0;
        r = bot(m, cdb, cdblen, len, in, timeout, &st);
        if (r >= 0) return r;
        if (!m->d) return USB_NODEV;
        if (st == 1) {
            u8 k = 0, a = 0;
            sense(m, &k, &a);
            m->last_key = k;
            if (t == 2 || k != 2) kprintf("USB MSC: comando %02x falhou: sense %x/%02x\n", cdb[0], k, a);
            if (k == 5 || k == 7) return USB_ERR;   /* ILLEGAL REQUEST / DATA PROTECT: não adianta repetir */
        }
        sleep_ms(50);
    }
    return r;
}

static void put32be(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static u32 get32be(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static int rw(msc_t *m, u64 lba, u32 n, bool write) {
    u8 cdb[16];
    memset(cdb, 0, sizeof cdb);
    if (lba + n > 0xFFFFFFFFULL) {
        cdb[0] = write ? 0x8A : 0x88;
        put32be(cdb + 2, (u32)(lba >> 32)); put32be(cdb + 6, (u32)lba); put32be(cdb + 10, n);
        return scsi(m, cdb, 16, n * m->bsize, !write, 10000);
    }
    cdb[0] = write ? 0x2A : 0x28;
    put32be(cdb + 2, (u32)lba);
    cdb[7] = n >> 8; cdb[8] = n;
    return scsi(m, cdb, 10, n * m->bsize, !write, 10000);
}

static msc_t *get(int dev) {
    for (int i = 0; i < MAX_MSC; i++)
        if (mscs[i].used && mscs[i].ready && dev-- == 0) return &mscs[i];
    return NULL;
}

int usb_msc_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_MSC; i++) if (mscs[i].used && mscs[i].ready) n++;
    return n;
}
u32 usb_msc_block_size(int dev) { msc_t *m = get(dev); return m ? m->bsize : 0; }
u64 usb_msc_blocks(int dev) { msc_t *m = get(dev); return m ? m->blocks : 0; }
const char *usb_msc_name(int dev) { msc_t *m = get(dev); return m ? m->name : ""; }

static int xfer(msc_t *m, u64 lba, u32 count, u8 *p, bool write) {
    if (!m || !m->ready) return USB_NODEV;
    if (lba + count > m->blocks) return USB_ERR;
    u32 per = XFER_MAX / m->bsize;
    while (count) {
        u32 n = MIN(count, per);
        if (write) memcpy(m->buf, p, (usize)n * m->bsize);
        int r = rw(m, lba, n, write);
        if (r < 0) return r;
        if ((u32)r < n * m->bsize) return USB_ERR;
        if (!write) memcpy(p, m->buf, (usize)n * m->bsize);
        p += (usize)n * m->bsize; lba += n; count -= n;
    }
    return USB_OK;
}
int usb_msc_read(int dev, u64 lba, u32 count, void *buf) { return xfer(get(dev), lba, count, buf, false); }
int usb_msc_write(int dev, u64 lba, u32 count, const void *buf) { return xfer(get(dev), lba, count, (u8 *)buf, true); }

/* ------------------------------------------------------------------ interface blkdev
 * Deslocamentos em bytes, qualquer alinhamento (mesmo modelo do ahci.c).  O blkdev_t fica
 * dentro do msc_t, que é estático: depois da remoção, read/write/flush falham com segurança. */
static bool disk_read(blkdev_t *bd, u64 off, void *out, u64 len) {
    msc_t *m = bd->ctx;
    if (!m->ready) return false;
    u8 *o = out;
    u8 *tmp = NULL;
    u32 ss = m->bsize;
    while (len) {
        u64 lba = off / ss, in = off % ss;
        if (in || len < ss) {                    /* pedaço desalinhado: lê o setor inteiro */
            if (!tmp && !(tmp = kmalloc(ss))) return false;
            if (xfer(m, lba, 1, tmp, false) != USB_OK) { kfree(tmp); return false; }
            u64 k = MIN(len, ss - in);
            memcpy(o, tmp + in, k);
            o += k; off += k; len -= k;
        } else {
            u64 n = MIN(len / ss, (u64)0xFFFF0000u / ss);
            if (xfer(m, lba, (u32)n, o, false) != USB_OK) { kfree(tmp); return false; }
            o += n * ss; off += n * ss; len -= n * ss;
        }
    }
    kfree(tmp);
    return true;
}

static bool disk_write(blkdev_t *bd, u64 off, const void *src, u64 len) {
    msc_t *m = bd->ctx;
    if (!m->ready || m->wp) return false;
    const u8 *s = src;
    u8 *tmp = NULL;
    u32 ss = m->bsize;
    while (len) {
        u64 lba = off / ss, in = off % ss;
        if (in || len < ss) {                    /* ler-modificar-gravar */
            if (!tmp && !(tmp = kmalloc(ss))) return false;
            if (xfer(m, lba, 1, tmp, false) != USB_OK) { kfree(tmp); return false; }
            u64 k = MIN(len, ss - in);
            memcpy(tmp + in, s, k);
            if (xfer(m, lba, 1, tmp, true) != USB_OK) { kfree(tmp); return false; }
            s += k; off += k; len -= k;
        } else {
            u64 n = MIN(len / ss, (u64)0xFFFF0000u / ss);
            if (xfer(m, lba, (u32)n, (u8 *)s, true) != USB_OK) { kfree(tmp); return false; }
            s += n * ss; off += n * ss; len -= n * ss;
        }
    }
    kfree(tmp);
    return true;
}

/* SYNCHRONIZE CACHE(10); unidades sem cache de escrita respondem ILLEGAL REQUEST = ok */
static bool disk_flush(blkdev_t *bd) {
    msc_t *m = bd->ctx;
    if (!m->ready) return false;
    u8 cdb[10] = {0x35};
    m->last_key = 0;
    int r = scsi(m, cdb, 10, 0, false, 20000);
    return r >= 0 || (r == USB_ERR && m->last_key == 5);
}

blkdev_t *usb_msc_disk(int i) { msc_t *m = get(i); return m ? &m->dev : NULL; }
/* ------------------------------------------------------------------ driver */
static bool msc_probe(usb_dev_t *d, usb_iface_t *it) {
    if (it->cls != 8) return false;
    if (it->proto != 0x50 || (it->sub != 6 && it->sub != 2 && it->sub != 5)) {
        kprintf("USB MSC: subclasse %02x / protocolo %02x não suportados (só SCSI Bulk-Only)\n", it->sub, it->proto);
        usb_set_kind(d, USB_KIND_STORAGE);
        return false;
    }
    int ii, io;
    if (!usb_find_ep(it, USB_EP_BULK, true, &ii) || !usb_find_ep(it, USB_EP_BULK, false, &io)) return false;
    msc_t *m = NULL;
    /* prefere slots nunca usados: um blkdev_t antigo (unidade removida) não deve apontar para outra unidade */
    for (int i = 0; i < MAX_MSC && !m; i++) if (!mscs[i].used && !mscs[i].buf) m = &mscs[i];
    for (int i = 0; i < MAX_MSC && !m; i++) if (!mscs[i].used) m = &mscs[i];
    if (!m) return false;
    u8 *buf = m->buf, *cbw = m->cbw;      /* reaproveita buffers DMA de uma unidade removida */
    u32 gen = m->gen;
    memset(m, 0, sizeof *m);
    m->buf = buf; m->cbw = cbw; m->gen = gen + 1;
    if (!m->buf) { u64 ph; m->buf = dma_alloc(XFER_MAX, &ph); }
    if (!m->cbw) { u64 ph; m->cbw = dma_alloc(4096, &ph); }
    if (!m->buf || !m->cbw) return false;
    m->used = true; m->d = d; m->iface = it->num;
    m->ep_in = it->ep[ii].bEndpointAddress;
    m->ep_out = it->ep[io].bEndpointAddress;
    it->priv = m;
    usb_add_ep(d, it, ii);
    usb_add_ep(d, it, io);
    return true;
}

static void selftest(msc_t *m);

static void msc_start(usb_dev_t *d, usb_iface_t *it) {
    msc_t *m = it->priv;
    if (!m) return;
    usb_set_kind(d, USB_KIND_STORAGE);
    u8 maxlun = 0;
    if (usb_control(d, 0xA1, 0xFE, 0, it->num, 1, &maxlun) < 1) maxlun = 0;   /* GET MAX LUN (pode dar STALL) */
    m->lun = 0;

    u8 inq[6] = {0x12, 0, 0, 0, 36, 0};
    int r = scsi(m, inq, 6, 36, true, 3000);
    if (r >= 8) {
        char v[9], p[17];
        memcpy(v, m->buf + 8, 8); v[8] = 0;
        memcpy(p, m->buf + 16, 16); p[16] = 0;
        for (int i = 7; i >= 0 && v[i] == ' '; i--) v[i] = 0;
        for (int i = 15; i >= 0 && p[i] == ' '; i--) p[i] = 0;
        snprintf(m->name, sizeof m->name, "%s %s", v, p);
        kprintf("USB MSC: INQUIRY: \"%s\" tipo %02x%s, LUNs %u\n", m->name, m->buf[0] & 0x1F, (m->buf[1] & 0x80) ? " removível" : "", maxlun + 1);
    } else kprintf("USB MSC: INQUIRY falhou (%d)\n", r);

    /* espera a mídia ficar pronta (até ~5 s) */
    u8 tur[6] = {0};
    for (int t = 0; t < 25; t++) {
        int st = 0;
        if (bot(m, tur, 6, 0, false, 2000, &st) >= 0) break;
        if (!m->d) return;
        u8 k = 0, a = 0;
        if (st == 1) sense(m, &k, &a);
        if (st == 1 && k == 2 && a == 0x3A) { kprintf("USB MSC: sem mídia\n"); return; }
        sleep_ms(200);
    }

    u8 rc[10] = {0x25};
    r = scsi(m, rc, 10, 8, true, 3000);
    if (r < 8) { kprintf("USB MSC: READ CAPACITY(10) falhou (%d)\n", r); return; }
    u32 last = get32be(m->buf);
    m->bsize = get32be(m->buf + 4);
    m->blocks = (u64)last + 1;
    if (last == 0xFFFFFFFF) {
        u8 rc16[16] = {0x9E, 0x10};
        rc16[13] = 32;
        if (scsi(m, rc16, 16, 32, true, 3000) >= 12) {
            m->blocks = ((u64)get32be(m->buf) << 32 | get32be(m->buf + 4)) + 1;
            m->bsize = get32be(m->buf + 8);
        }
    }
    if (m->bsize < 512 || m->bsize > 4096 || (m->bsize & (m->bsize - 1))) {
        kprintf("USB MSC: tamanho de bloco %u não suportado\n", m->bsize);
        return;
    }
    kprintf("USB MSC: READ CAPACITY: %llu blocos de %u bytes (%llu MiB)\n", m->blocks, m->bsize, m->blocks * m->bsize >> 20);
    m->ready = true;
    r = rw(m, 0, 1, false);
    if (r >= 512) {
        u8 *b = m->buf;
        kprintf("USB MSC: READ(10) LBA 0 ok — assinatura %02X%02X%s\n", b[510], b[511],
                (b[510] == 0x55 && b[511] == 0xAA) ? " (MBR/setor de boot válido)" : "");
    } else kprintf("USB MSC: READ(10) LBA 0 falhou (%d)\n", r);

    /* MODE SENSE(6), todas as páginas: bit 7 do byte 2 = protegido contra escrita */
    u8 ms[6] = {0x1A, 0, 0x3F, 0, 192, 0};
    r = scsi(m, ms, 6, 192, true, 3000);
    if (r >= 4) m->wp = (m->buf[2] & 0x80) != 0;
    m->dev.size = m->blocks * m->bsize;
    m->dev.read = disk_read;
    m->dev.write = m->wp ? NULL : disk_write;
    m->dev.flush = disk_flush;
    m->dev.release = NULL;              /* estático: nada a liberar */
    m->dev.ctx = m;
    kprintf("USB MSC: disco de bloco pronto (%s)\n", m->wp ? "somente leitura: protegido contra escrita" : "leitura e escrita");
    /* autoteste só no disco virtual do QEMU e só se ele não tiver GPT (nunca num disco de boot/sistema) */
    if (d->desc.idVendor == 0x46F4) {
        bool has_gpt = rw(m, 1, 1, false) >= 512 && !memcmp(m->buf, "EFI PART", 8);
        if (!has_gpt) selftest(m);
        else kprintf("USB MSC: autoteste pulado (disco com GPT)\n");
    }
    /* publica o pendrive no registro genérico de discos (storage.c da camada de persistência), se existir */
    extern void storage_register(blkdev_t *dev, const char *name, const char *desc) __attribute__((weak));
    if (storage_register) {
        static char names[MAX_MSC][16], descs[MAX_MSC][96];
        int slot = (int)(m - mscs);
        snprintf(names[slot], 16, "usb%d", slot);
        snprintf(descs[slot], 96, "%s (%llu MB)", m->name, m->blocks * m->bsize / 1000000);
        storage_register(&m->dev, names[slot], descs[slot]);
    }
}

/* autoteste não destrutivo (só QEMU): grava setor alinhado + trecho desalinhado no fim do disco,
 * relê, compara e restaura o conteúdo original */
static void selftest(msc_t *m) {
    if (m->wp || m->blocks < 16) return;
    u32 ss = m->bsize;
    u64 base = (m->blocks - 8) * ss;
    u32 span = 4 * ss;
    u8 *save = kmalloc(span), *a = kmalloc(span), *b = kmalloc(span);
    bool ok = save && a && b && disk_read(&m->dev, base, save, span);
    if (ok) {
        for (u32 i = 0; i < span; i++) a[i] = (u8)(i * 7 + 0x5A);
        /* 1) um setor inteiro alinhado */
        bool s1 = disk_write(&m->dev, base, a, ss) && disk_flush(&m->dev) && disk_read(&m->dev, base, b, ss) && !memcmp(a, b, ss);
        /* 2) trecho desalinhado (ler-modificar-gravar nas pontas) */
        u32 off = 3, n = 2 * ss + 100;
        bool s2 = disk_write(&m->dev, base + ss + off, a + 11, n) && disk_flush(&m->dev) &&
                  disk_read(&m->dev, base + ss + off, b, n) && !memcmp(a + 11, b, n);
        /* vizinhos do trecho desalinhado devem estar intactos */
        bool s3 = disk_read(&m->dev, base + ss, b, off) && !memcmp(b, save + ss, off);
        ok = s1 && s2 && s3;
        bool restored = disk_write(&m->dev, base, save, span) && disk_flush(&m->dev);
        kprintf("USB MSC: autoteste de escrita (LBA %llu): setor alinhado %s, desalinhado %s, vizinhos %s, restaurado %s\n",
                base / ss, s1 ? "ok" : "FALHOU", s2 ? "ok" : "FALHOU", s3 ? "ok" : "FALHOU", restored ? "sim" : "NÃO");
    } else kprintf("USB MSC: autoteste de escrita não executado\n");
    kfree(save); kfree(a); kfree(b);
}

static void msc_detach(usb_dev_t *d, usb_iface_t *it) {
    msc_t *m = it->priv;
    if (!m) return;
    m->ready = false;
    m->d = NULL;
    m->used = false;       /* os buffers DMA ficam para a próxima unidade; o blkdev_t falha com segurança */
    it->priv = NULL;
}

const usb_driver_t usb_msc_driver = { "msc", msc_probe, msc_start, msc_detach, NULL };

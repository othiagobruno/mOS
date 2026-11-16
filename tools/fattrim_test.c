/*
 * mOS — TRIM do FAT (kernel/fs/fat.c): clusters soltos só são descartados depois que a FAT nova foi gravada, e só
 * os que continuam livres.
 *   gcc -O1 -g -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include -Itools tools/fattrim_test.c \
 *       kernel/fs/fat.c kernel/fs/blkdev.c kernel/fs/dfs.c kernel/fs/inflate.c kernel/fs/lzfse.c kernel/fs/bzip2.c kernel/fs/xz.c kernel/fs/dmg.c kernel/fs/apfs.c kernel/fs/hfsplus.c kernel/fs/hfsplus_wr.c -o /tmp/fattrim && /tmp/fattrim
 */
#include "fs.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) { *dt = (datetime_t){2026, 11, 16, 12, 0, 0, 0}; }
u32 rand32(void) { return 7; }
void dfs_publish_new(dmount_t *before) { (void)before; }
bool fat_format(blkdev_t *dev, const char *label);
extern bool (*fat_discard_hook)(blkdev_t *d, u64 off, u64 len);

#define SZ (64ull << 20)
static u8 *img;
static int fat_writes_since_discard, writes_total;
static u64 fat_lo = 0, fat_hi = 0;            /* faixa da FAT (bytes) */
static struct { u64 off, len; int fat_written_before; } disc[256];
static int ndisc;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; memcpy(buf, img + off, len); return true; }
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) {
    (void)d;
    memcpy(img + off, buf, len);
    writes_total++;
    if (off < fat_hi && off + len > fat_lo) fat_writes_since_discard++;
    return true;
}
static int bad_discard;          /* cluster descartado que a FAT gravada ainda dá como em uso */
static u64 data_lo; static u32 clus_bytes;
static bool disc_cb(blkdev_t *d, u64 off, u64 len) {
    (void)d;
    for (u64 o = off; o < off + len; o += clus_bytes) {
        u32 c = (u32)((o - data_lo) / clus_bytes) + 2;
        if (le32(img + fat_lo + 4ull * c) & 0x0FFFFFFF) bad_discard++;
    }
    if (ndisc < 256) { disc[ndisc].off = off; disc[ndisc].len = len; disc[ndisc].fat_written_before = fat_writes_since_discard; ndisc++; }
    fat_writes_since_discard = 0;
    memset(img + off, 0, len);                 /* o SSD devolve zeros num bloco descartado */
    return true;
}

/* sem o page cache do kernel (fs/pcache.c): leitura e escrita diretas no dispositivo */
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) { return d->read(d, off, buf, len); }
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) { return d->write && d->write(d, off, buf, len); }
bool blk_flush(blkdev_t *d) { return !d->flush || d->flush(d); }
void pcache_drop(blkdev_t *d) { (void)d; }

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)

int main(void) {
    img = calloc(1, SZ);
    blkdev_t dev = {.size = SZ, .read = rd, .write = wr};
    CHECK(fat_format(&dev, "TRIM"));
    fatfs_t *fs = fat_mount(&dev);
    CHECK(fs != NULL);
    if (!fs) return 1;
    fat_enable_rw(fs);
    /* FAT32: setores reservados no boot (bytes 14-15), FAT de bytes 36-39 setores */
    u32 bps = img[11] | img[12] << 8, res = img[14] | img[15] << 8, fsz = le32(img + 36), nf = img[16];
    fat_lo = (u64)res * bps; fat_hi = fat_lo + (u64)nf * fsz * bps;
    data_lo = fat_hi; clus_bytes = bps * img[13];
    fat_discard_hook = disc_cb;
    static u8 big[1 << 20];
    for (u32 i = 0; i < sizeof big; i++) big[i] = (u8)(i * 13 + 1);
    CHECK(fat_write_file(fs, "/a.bin", big, sizeof big));
    CHECK(fat_write_file(fs, "/b.bin", big, 300000));
    CHECK(fat_sync(fs));
    int d0 = ndisc;
    CHECK(fat_remove(fs, "/a.bin"));
    /* fat_remove já faz o sync: o descarte pode vir aqui (depois da FAT gravada; a invariante confere) */
    CHECK(fat_sync(fs));
    CHECK(ndisc > d0);
    u64 tot = 0;
    for (int i = d0; i < ndisc; i++) tot += disc[i].len;
    CHECK(tot >= sizeof big && tot < sizeof big + 64 * 1024);   /* cobre o arquivo, nada além do último cluster */
    CHECK(disc[d0].fat_written_before > 0);       /* a FAT com os livres foi gravada antes do descarte */
    u64 n = 0;
    u8 *b = fat_read_file(fs, "/b.bin", &n);
    CHECK(b && n == 300000 && !memcmp(b, big, 300000));   /* o vizinho intacto */
    kfree(b);
    /* soltar e reaproveitar antes do sync: o reaproveitado não pode ser descartado */
    int before = ndisc;
    CHECK(fat_write_file(fs, "/c.bin", big, sizeof big));
    CHECK(fat_sync(fs));
    CHECK(fat_write_file(fs, "/c.bin", big + 7, 500000));  /* substitui: solta a cadeia velha, aloca nova */
    CHECK(fat_write_file(fs, "/d.bin", big, 400000));      /* pode cair nos clusters recém-soltos */
    CHECK(fat_sync(fs));
    CHECK(ndisc > before);
    b = fat_read_file(fs, "/d.bin", &n);
    CHECK(b && n == 400000 && !memcmp(b, big, 400000));    /* nada do que está em uso foi zerado */
    kfree(b);
    b = fat_read_file(fs, "/c.bin", &n);
    CHECK(b && n == 500000 && !memcmp(b, big + 7, 500000));
    kfree(b);
    CHECK(bad_discard == 0);                     /* nunca um cluster em uso na FAT do disco */
    fat_unmount(fs);
    free(img);
    printf("fattrim: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

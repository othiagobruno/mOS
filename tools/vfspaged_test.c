/*
 * mOS — VFS sob demanda no host: disco FAT32 (como o MOS-APPS) montado pelo dfs e publicado em /Volumes pelo VFS
 * reais (kernel/lib/vfs*.c, kernel/fs/vfs_bridge.c, dfs.c, fat.c). Arquivo grande (fragmentado de propósito) fica
 * paginado: o vfs_lookup não o carrega, vfs_read_at lê só o trecho pedido, igual ao arquivo de origem; o pequeno
 * continua inteiro na RAM; vfs_ensure, cópia, append e exchange preservam o conteúdo.
 *   sh tools/vfspaged_test.sh
 */
#include "vfs_host_kernel.h"
#include "vfs.h"
#include "fs.h"
#include "syscall.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
static u64 fake_ms = 1000;
u64 timer_ticks(void) { return fake_ms; }
void rtc_now(datetime_t *dt) { time_t t = time(NULL); struct tm *m = gmtime(&t); dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday; dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday; }
uint32_t rand32(void) { return 4; }
static boot_info_t boot0;
boot_info_t *g_boot = &boot0;
static u64 dev_reads, dev_bytes;
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) { dev_reads++; dev_bytes += len; return d->read(d, off, buf, len); }
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) { return d->write && d->write(d, off, buf, len); }
bool blk_flush(blkdev_t *d) { return !d->flush || d->flush(d); }
void pcache_drop(blkdev_t *d) { (void)d; }
bool persist_load(void) { return false; }
void persist_enable(void) {}
bool persist_available(void) { return false; }
void persist_meta_mark(void) {}
void persist_mark(vnode_t *n) { vol_mark(n); }
void persist_delete(vnode_t *n) { vol_delete(n); }
void persist_move(const char *o, vnode_t *n) { vol_move(o, n); }
bool storage_grant_write(blkdev_t *d, u64 off, u64 len) { (void)d; (void)off; (void)len; return true; }
void storage_revoke_write(blkdev_t *d, u64 off, u64 len) { (void)d; (void)off; (void)len; }
#include "origin.h"
int storage_disk_index(blkdev_t *r) { (void)r; return -1; }
const char *storage_disk_name(int i) { (void)i; return "teste"; }
static origin_t org0;
const origin_t *origin_get(void) { return &org0; }
void vfs_init(void);

static u8 *img;
static u64 img_size;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; memcpy(buf, img + off, len); return true; }
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) { (void)d; memcpy(img + off, buf, len); return true; }
static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)
static dmount_t *mnt(void) { for (dmount_t *m = dfs_mounts(); m; m = m->next) if (!strcmp(m->ops->name, "FAT")) return m; return NULL; }
static u8 pat(u64 i) { return (u8)(i * 2654435761u >> 13 ^ i >> 17); }

#define BIG (24ull << 20 | 777)
#define LOGN (18u << 20)            /* também > VFS_PAGED_MIN */   /* > VFS_PAGED_MIN, não múltiplo do cluster */

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    fseeko(f, 0, SEEK_END); img_size = (u64)ftello(f); fseeko(f, 0, SEEK_SET);
    img = malloc(img_size);
    if (fread(img, 1, img_size, f) != img_size) return 2;
    fclose(f);
    vfs_init();
    dfs_init();
    blkdev_t *d = calloc(1, sizeof *d);
    d->size = img_size; d->read = rd; d->write = wr;
    dfs_mount_disk(d, "apps");
    dmount_t *m = mnt();
    CHECK(m && m->writable);
    if (!m) return 1;
    /* buracos na FAT: o arquivo grande ocupa clusters espalhados */
    u8 *small = malloc(65536);
    char p[300];
    for (int i = 0; i < 24; i++) {
        memset(small, 'a' + i % 26, 65536);
        snprintf(p, sizeof p, "/f%02d", i);
        CHECK(m->ops->write_file(m, p, small, 65536));
    }
    for (int i = 0; i < 24; i += 2) { snprintf(p, sizeof p, "/f%02d", i); CHECK(m->ops->remove(m, p)); }
    u8 *big = malloc(BIG);
    for (u64 i = 0; i < BIG; i++) big[i] = pat(i);
    CHECK(m->ops->mkdir(m, "/Aplicativos"));
    CHECK(m->ops->write_file(m, "/Aplicativos/app.asar", big, BIG));
    CHECK(m->ops->write_file(m, "/Aplicativos/Info.plist", "<plist/>", 8));
    CHECK(m->ops->sync(m));
    /* remonta (caches frios) e publica em /Volumes */
    dfs_unmount(m);
    blkdev_t *d2 = calloc(1, sizeof *d2);
    d2->size = img_size; d2->read = rd; d2->write = wr;
    dfs_mount_disk(d2, "apps");
    dfs_publish_new(NULL);
    m = mnt();
    char bp[300], sp[300];
    snprintf(bp, sizeof bp, "/Volumes/%s/Aplicativos/app.asar", m->root->name);
    snprintf(sp, sizeof sp, "/Volumes/%s/Aplicativos/Info.plist", m->root->name);

    u64 r0 = dev_bytes;
    vnode_t *v = vfs_lookup(bp);
    CHECK(v && !v->dir && v->size == BIG);
    CHECK(v && !v->data && vfs_paged(v));               /* o lookup não carregou */
    CHECK(dev_bytes - r0 < (1u << 20));                  /* nem leu o arquivo do disco */
    vnode_t *s = vfs_lookup(sp);
    CHECK(s && s->data && s->size == 8 && !memcmp(s->data, "<plist/>", 8) && !vfs_paged(s));
    if (!v) return 1;

    /* trechos: começo, meio, atravessando clusters, o fim, além do fim */
    u8 buf[70000];
    u64 offs[] = {0, 1, 4095, 65536 * 5 + 17, BIG / 2, BIG - 70000, BIG - 1, 12345678};
    for (unsigned i = 0; i < ARRAY_LEN(offs); i++) {
        u64 o = offs[i], want = MIN(sizeof buf, BIG - o);
        i64 k = vfs_read_at(v, o, buf, sizeof buf);
        CHECK(k == (i64)want && !memcmp(buf, big + o, want));
    }
    CHECK(vfs_read_at(v, BIG, buf, 10) == 0);
    CHECK(vfs_read_at(v, BIG + 5000, buf, 10) == 0);
    /* leitura sequencial do arquivo inteiro em pedaços de 4 KiB (o que o read() de um app faz); rápida = a cadeia
     * da FAT não é refeita do início a cada pedaço */
    clock_t t0 = clock();
    u64 off = 0;
    bool same = true;
    while (off < BIG) {
        i64 k = vfs_read_at(v, off, buf, 4096);
        if (k <= 0 || memcmp(buf, big + off, (u64)k)) { same = false; break; }
        off += (u64)k;
    }
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    CHECK(same && off == BIG);
    CHECK(secs < 3.0);
    printf("vfspaged: leitura sequencial de %llu MiB em 4 KiB: %.2f s\n", (unsigned long long)(BIG >> 20), secs);
    /* de trás para frente (mmap tocando páginas fora de ordem) */
    same = true;
    for (u64 o = BIG & ~4095ull; ; o -= 4096 * 37) {
        i64 k = vfs_read_at(v, o, buf, 4096);
        if (k <= 0 || memcmp(buf, big + o, (u64)k)) { same = false; break; }
        if (o < 4096 * 37) break;
    }
    CHECK(same);
    CHECK(v->data == NULL);                              /* nada disso carregou o arquivo */

    /* cópia para fora do volume, aos pedaços */
    vfs_mkdir("/tmp");
    CHECK(vfs_copy_tree(bp, "/tmp/copia.asar"));
    vnode_t *c = vfs_lookup("/tmp/copia.asar");
    CHECK(c && c->size == BIG && c->data && !memcmp(c->data, big, BIG));
    CHECK(vfs_clone(v, "/tmp/clone.asar", 0, 501, 20) == 0);
    c = vfs_lookup("/tmp/clone.asar");
    CHECK(c && c->size == BIG && c->data && !memcmp(c->data, big, BIG));

    /* vfs_ensure: quem precisa de tudo na RAM continua tendo */
    CHECK(vfs_ensure(v) && v->data && v->size == BIG && !memcmp(v->data, big, BIG));
    CHECK(vfs_read_at(v, 100, buf, 100) == 100 && !memcmp(buf, big + 100, 100));

    /* append num arquivo paginado do volume gravável: o conteúdo antigo fica e vai para o disco */
    vfs_mkdir("/tmp/x");
    snprintf(p, sizeof p, "/Volumes/%s/Aplicativos/log.bin", m->root->name);
    CHECK(m->ops->write_file(m, "/Aplicativos/log.bin", big, LOGN));
    {   /* ejeta pelo caminho normal e monta de novo, para o VFS ver o arquivo novo */
        char vp0[300];
        snprintf(vp0, sizeof vp0, "/Volumes/%s", m->root->name);
        CHECK(vfs_eject(vfs_lookup(vp0)));
    }
    blkdev_t *d3 = calloc(1, sizeof *d3);
    d3->size = img_size; d3->read = rd; d3->write = wr;
    dfs_mount_disk(d3, "apps");
    dfs_publish_new(NULL);
    m = mnt();
    snprintf(p, sizeof p, "/Volumes/%s/Aplicativos/log.bin", m->root->name);
    vnode_t *lg = vfs_lookup(p);
    CHECK(lg && vfs_paged(lg) && lg->size == (LOGN));
    CHECK(lg && vfs_append(lg, "FIM", 3));
    CHECK(lg && lg->size == (LOGN) + 3 && !memcmp(lg->data, big, LOGN) && !memcmp(lg->data + (LOGN), "FIM", 3));
    char vp[300];
    snprintf(vp, sizeof vp, "/Volumes/%s", m->root->name);
    CHECK(vol_flush(vfs_lookup(vp)));
    CHECK(vfs_eject(vfs_lookup(vp)));                   /* remonta: confere o que está no disco, não o cache */
    blkdev_t *d4 = calloc(1, sizeof *d4);
    d4->size = img_size; d4->read = rd; d4->write = wr;
    dfs_mount_disk(d4, "apps");
    m = mnt();
    snprintf(p, sizeof p, "/Volumes/%s/Aplicativos/log.bin", m->root->name);
    dnode_t *dn = dfs_lookup(p);
    u8 tail[3];
    CHECK(dn && dn->size == (LOGN) + 3 && dfs_read(dn, LOGN, tail, 3) == 3 && !memcmp(tail, "FIM", 3));
    u8 *head = malloc(1u << 20);
    CHECK(dn && dfs_read(dn, 0, head, 1u << 20) == (1 << 20) && !memcmp(head, big, 1u << 20));
    free(head);

    if (m->ops->sync) m->ops->sync(m);
    f = fopen(argv[1], "wb"); fwrite(img, 1, img_size, f); fclose(f);
    free(big); free(small);
    printf("vfspaged: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

/* lê um .dmg com o leitor do kernel (kernel/fs/dmg.c) e monta os volumes (dfs + HFS+/FAT reais): nome e nº de itens */
#include "fs.h"
long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) { *dt = (datetime_t){2026, 11, 16, 12, 0, 0, 0}; }
u32 rand32(void) { return 1; }
void dfs_publish_new(dmount_t *b) { (void)b; }
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) { return d->read(d, off, buf, len); }
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) { return d->write && d->write(d, off, buf, len); }
bool blk_flush(blkdev_t *d) { return !d->flush || d->flush(d); }
void pcache_drop(blkdev_t *d) { (void)d; }
int main(int c, char **v) {
    FILE *f = fopen(v[1], "rb"); fseeko(f, 0, SEEK_END); u64 n = (u64)ftello(f); fseeko(f, 0, SEEK_SET);
    u8 *b = malloc(n); if (fread(b, 1, n, f) != n) return 2; fclose(f);
    dfs_init();
    int k = dfs_mount_memory(b, n, v[1]);
    for (dmount_t *m = dfs_mounts(); m; m = m->next) printf("%s|%s|%s\n", m->ops->name, m->volname, m->format);
    return k > 0 ? 0 : 1;
}

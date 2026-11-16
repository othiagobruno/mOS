/*
 * mOS — xattr de ponta a ponta num volume HFS+ de /Volumes: VFS (kernel/lib/vfs*.c) + ponte (kernel/fs/vfs_bridge.c)
 * + dfs + driver HFS+, todos reais. setxattr pelo VFS chega ao disco; um vnode novo lê do disco; FAT recusa.
 *   sh tools/volxattr_test.sh
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
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) { return d->read(d, off, buf, len); }
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) { return d->write && d->write(d, off, buf, len); }
bool blk_flush(blkdev_t *d) { return !d->flush || d->flush(d); }
void pcache_drop(blkdev_t *d) { (void)d; }
bool persist_load(void) { return false; }
void persist_enable(void) {}
bool persist_available(void) { return false; }
void persist_meta_mark(void) {}
/* o persist.c real chama a ponte (vol_mark & cia.); aqui o mesmo encaminhamento */
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
static dmount_t *mnt(const char *ops) { for (dmount_t *m = dfs_mounts(); m; m = m->next) if (!strcmp(m->ops->name, ops)) return m; return NULL; }

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
    dfs_mount_disk(d, "teste");
    { dmount_t *h0 = mnt("HFS+"); if (h0) h0->ops->setxattr(h0, "/seed.txt", "user.do.disco", "D", 1); }
    dfs_publish_new(NULL);
    dmount_t *hm = mnt("HFS+"), *fm = mnt("FAT");
    CHECK(hm && hm->writable && fm);
    char hv[300], fv[300];
    snprintf(hv, sizeof hv, "/Volumes/%s/seed.txt", hm->root->name);
    snprintf(fv, sizeof fv, "/Volumes/%s/seed.txt", fm->root->name);
    vnode_t *h = vfs_lookup(hv), *fa = vfs_lookup(fv);
    CHECK(h && fa);
    if (!h || !fa) return 1;
    CHECK(h->uid == 501 && h->gid == 20 && (h->mode & 07777) == 0644);   /* dono 99 do volume = o usuário */
    CHECK(h->btime_ns > 0 && h->mtime_ns > 0);
    CHECK(vfs_xattr_set(h, "com.apple.quarantine", "0083;1;x;", 9, 0, 0) == 0);
    CHECK(vfs_xattr_set(fa, "com.apple.quarantine", "x", 1, 0, 0) == -MOS_ENOTSUP);   /* FAT não tem atributos */
    /* o driver tem o atributo no disco */
    dnode_t *dn = dfs_lookup(hv);
    char b[64];
    CHECK(dn && hm->ops->getxattr(hm, dn, "com.apple.quarantine", b, sizeof b) == 9 && !memcmp(b, "0083;1;x;", 9));
    /* atributo que já estava no disco antes de o volume aparecer no VFS (gravado pelo driver antes do publish) */
    CHECK(vfs_xattr_get(h, "user.do.disco", b, sizeof b, 0) == 1 && b[0] == 'D');
    i64 ln = vfs_xattr_list(h, NULL, 0);
    CHECK(ln == (i64)strlen("com.apple.quarantine") + 1 + (i64)strlen("user.do.disco") + 1);
    /* arquivo novo criado pelo VFS (ainda pendente) + xattr: o arquivo vai ao disco antes */
    char pp[300];
    snprintf(pp, sizeof pp, "/Volumes/%s/novo.txt", hm->root->name);
    vnode_t *nv = vfs_write_file(pp, "conteudo", 8);
    CHECK(nv && vfs_xattr_set(nv, "user.x", "1", 1, 0, 0) == 0);
    dnode_t *ndn = NULL;
    for (dnode_t *c = dfs_first_child(hm->root); c; c = c->next) if (!strcmp(c->name, "novo.txt")) ndn = c;
    (void)ndn;
    /* remove pelo VFS some do disco */
    CHECK(vfs_xattr_remove(h, "com.apple.quarantine") == 0);
    dn = dfs_lookup(hv);
    CHECK(dn && hm->ops->getxattr(hm, dn, "com.apple.quarantine", b, sizeof b) == -1);
    /* metadados pelo VFS chegam ao disco */
    mos_fsmeta_t rq = {0};
    rq.a = 0x8000;                                       /* UF_HIDDEN */
    CHECK(vfs_fsmeta(h, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 501, 20) == 0);
    rq = (mos_fsmeta_t){0};
    rq.a = VT_MTIME; rq.t[1] = 1300000000ll * 1000000000ll;
    CHECK(vfs_fsmeta(h, MOS_FSM_SETTIMES, &rq, NULL, NULL, 501, 20) == 0);
    h->mode = 0x10000u | 0640;
    CHECK(vol_meta_write(h, VFS_META_MODE) == 0);       /* o que o SYS_CHMOD faz */
    /* link do mOS gravado no volume vira link simbólico de verdade */
    char lp[300];
    snprintf(lp, sizeof lp, "/Volumes/%s/atalho", hm->root->name);
    CHECK(vfs_write_file(lp, "#mOS-link:seed.txt", 18) != NULL);
    char vp[300];
    snprintf(vp, sizeof vp, "/Volumes/%s", hm->root->name);
    CHECK(vol_flush(vfs_lookup(vp)));
    /* relê do disco por uma montagem nova do dfs */
    while (dfs_mounts()) { dmount_t *x = dfs_mounts(); if (x->ops->sync) x->ops->sync(x); dfs_unmount(x); }
    blkdev_t *d2 = calloc(1, sizeof *d2);
    d2->size = img_size; d2->read = rd; d2->write = wr;
    dfs_mount_disk(d2, "teste2");
    hm = mnt("HFS+");
    char dp[300];
    snprintf(dp, sizeof dp, "/Volumes/%s/seed.txt", hm->root->name);
    dnode_t *sd = dfs_lookup(dp);
    CHECK(sd && sd->mode == 0640 && (sd->flags & 0x8000) && sd->mtime == 1300000000);
    snprintf(dp, sizeof dp, "/Volumes/%s/atalho", hm->root->name);
    dnode_t *at = dfs_lookup(dp);
    CHECK(at && at->type == DN_LINK);
    hm->ops->sync(hm);
    f = fopen(argv[1], "wb"); fwrite(img, 1, img_size, f); fclose(f);
    printf("volxattr: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

/*
 * mOS — .dmg gravável de ponta a ponta no host: VFS (kernel/lib/vfs*.c) + ponte (kernel/fs/vfs_bridge.c) + dfs +
 * leitor/escritor de UDIF (kernel/fs/dmg.c) + HFS+/FAT reais. A imagem é um arquivo do VFS; o volume dela é montado
 * gravável, recebe arquivos e pastas, e o .dmg de origem passa a ter os bytes novos (CRCs corrigidos). UDZO monta
 * só leitura; -readonly também.
 *   sh tools/dmgrw_test.sh
 * argv: <UDRW HFS+> <UDZO> <cru FAT32 (.cdr)> <saída UDRW> <saída cru>
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
static int marked;
void persist_mark(vnode_t *n) { marked++; vol_mark(n); }
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

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)

static u8 *load(const char *p, u64 *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseeko(f, 0, SEEK_END); *n = (u64)ftello(f); fseeko(f, 0, SEEK_SET);
    u8 *b = malloc(*n);
    if (fread(b, 1, *n, f) != *n) { fclose(f); free(b); return NULL; }
    fclose(f);
    return b;
}
static dmount_t *mount_from(const char *src) { for (dmount_t *m = dfs_mounts(); m; m = m->next) if (!strcmp(m->source, src)) return m; return NULL; }
static vnode_t *vol_of(dmount_t *m) { char p[300]; snprintf(p, sizeof p, "/Volumes/%s", m->root->name); return vfs_lookup(p); }

/* lê um arquivo do volume pelo VFS */
static bool same(const char *path, const void *want, u64 n) {
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || !vfs_ensure(v)) return false;
    return v->size == n && !memcmp(v->data, want, n);
}

/* o volume de uma imagem gravável: arquivos, pasta, arquivo grande (vários chunks de 1 MiB), sobrescrita e remoção */
static void exercise(const char *img, const char *outp, const char *fsname, u8 *big, u64 bign) {
    vnode_t *f = vfs_lookup(img);
    u64 orig_n = f->size;
    u8 *orig = malloc(orig_n);
    memcpy(orig, f->data, orig_n);
    CHECK(vfs_mount_file_ex(f, false) > 0);
    dmount_t *m = mount_from(img);
    CHECK(m && m->writable && !strcmp(m->ops->name, fsname));
    if (!m) return;
    vnode_t *vol = vol_of(m);
    CHECK(vol != NULL);
    char p[400], q[400];
    snprintf(p, sizeof p, "/Volumes/%s/olá.txt", m->root->name);
    CHECK(vfs_write_file(p, "olá, imagem", 12) != NULL);
    snprintf(p, sizeof p, "/Volumes/%s/Pasta", m->root->name);
    CHECK(vfs_mkdir(p) != NULL);
    snprintf(p, sizeof p, "/Volumes/%s/Pasta/grande.bin", m->root->name);
    CHECK(vfs_write_file(p, big, bign) != NULL);
    snprintf(p, sizeof p, "/Volumes/%s/apagar.txt", m->root->name);
    CHECK(vfs_write_file(p, "x", 1) != NULL);
    marked = 0;
    CHECK(vol_flush(vol));
    CHECK(marked >= 1);                                  /* o .dmg de origem foi marcado como alterado */
    f = vfs_lookup(img);
    CHECK(f && f->size == orig_n && memcmp(f->data, orig, orig_n) != 0);
    /* segunda rodada: sobrescreve e apaga, depois ejeta (o eject grava o pendente) */
    snprintf(p, sizeof p, "/Volumes/%s/olá.txt", m->root->name);
    CHECK(vfs_write_file(p, "segunda versão", 15) != NULL);   /* vfs_create zera o existente */
    snprintf(q, sizeof q, "/Volumes/%s/apagar.txt", m->root->name);
    CHECK(vfs_delete(vfs_lookup(q)));
    CHECK(vfs_eject(vol));
    CHECK(!mount_from(img));
    f = vfs_lookup(img);
    FILE *w = fopen(outp, "wb");
    fwrite(f->data, 1, f->size, w);
    fclose(w);
    /* remonta só leitura e confere pelo VFS */
    CHECK(vfs_mount_file_ex(f, true) > 0);
    m = mount_from(img);
    CHECK(m && !m->writable);
    if (!m) return;
    snprintf(p, sizeof p, "/Volumes/%s/olá.txt", m->root->name);
    CHECK(same(p, "segunda versão", 15));
    snprintf(p, sizeof p, "/Volumes/%s/Pasta/grande.bin", m->root->name);
    CHECK(same(p, big, bign));
    snprintf(p, sizeof p, "/Volumes/%s/apagar.txt", m->root->name);
    CHECK(vfs_lookup(p) == NULL);
    CHECK(vfs_eject(vol_of(m)));
    free(orig);
}

int main(int argc, char **argv) {
    if (argc < 6) return 2;
    vfs_init();
    dfs_init();
    vfs_mkdir("/Users");
    u64 n;
    u8 *b = load(argv[1], &n);
    CHECK(b && vfs_write_file("/Users/grava.dmg", b, n));
    free(b);
    b = load(argv[2], &n);
    CHECK(b && vfs_write_file("/Users/comprimida.dmg", b, n));
    free(b);
    b = load(argv[3], &n);
    CHECK(b && vfs_write_file("/Users/crua.img", b, n));
    free(b);

    u64 bign = 3u << 20 | 123;
    u8 *big = malloc(bign);
    for (u64 i = 0; i < bign; i++) big[i] = (u8)(i * 131 + (i >> 9));
    exercise("/Users/grava.dmg", argv[4], "HFS+", big, bign);
    exercise("/Users/crua.img", argv[5], "FAT", big, 70000);

    /* UDZO: o formato só pode ser lido, então monta só leitura mesmo pedindo gravação */
    vnode_t *z = vfs_lookup("/Users/comprimida.dmg");
    CHECK(vfs_mount_file_ex(z, false) > 0);
    dmount_t *m = mount_from("/Users/comprimida.dmg");
    CHECK(m && !m->writable);
    if (m) CHECK(vfs_eject(vol_of(m)));
    /* -readonly numa UDRW */
    vnode_t *g = vfs_lookup("/Users/grava.dmg");
    u64 gn = g->size;
    u8 *gs = malloc(gn);
    memcpy(gs, g->data, gn);
    CHECK(vfs_mount_file_ex(g, true) > 0);
    m = mount_from("/Users/grava.dmg");
    CHECK(m && !m->writable);
    char p[300];
    if (m) {
        snprintf(p, sizeof p, "/Volumes/%s/nao.txt", m->root->name);
        vfs_write_file(p, "n", 1);                       /* fica só na RAM: o volume é só leitura */
        vol_flush(vol_of(m));
        CHECK(vfs_eject(vol_of(m)));
    }
    g = vfs_lookup("/Users/grava.dmg");
    CHECK(g->size == gn && !memcmp(g->data, gs, gn));
    /* já montada: a segunda montagem não duplica */
    CHECK(vfs_mount_file_ex(g, false) > 0);
    CHECK(vfs_mount_file_ex(g, false) == 0);
    m = mount_from("/Users/grava.dmg");
    if (m) CHECK(vfs_eject(vol_of(m)));
    free(gs);
    free(big);
    printf("dmgrw: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

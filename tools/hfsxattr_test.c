/*
 * mOS — atributos estendidos no HFS+ (kernel/fs/hfsplus.c e hfsplus_wr.c, drivers reais): inline, grande (fork),
 * nome UTF-8, FinderInfo e ResourceFork no catálogo, pasta e raiz, remover, remontar e reler, criar a árvore de
 * atributos num volume que não tem, e o volimg check (verificador independente) no fim.
 *   sh tools/hfsxattr_test.sh
 */
#include "fs.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) { time_t t = time(NULL); struct tm *m = gmtime(&t); dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday; dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday; }
u32 rand32(void) { return (u32)rand(); }
void dfs_publish_new(dmount_t *before) { (void)before; }
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) { return d->read(d, off, buf, len); }
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) { return d->write && d->write(d, off, buf, len); }
bool blk_flush(blkdev_t *d) { return !d->flush || d->flush(d); }
void pcache_drop(blkdev_t *d) { (void)d; }

static u8 *img;
static u64 img_size;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; memcpy(buf, img + off, len); return true; }
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) { (void)d; memcpy(img + off, buf, len); return true; }
static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)

static dmount_t *hfs(void) { for (dmount_t *m = dfs_mounts(); m; m = m->next) if (!strcmp(m->ops->name, "HFS+")) return m; return NULL; }
static dnode_t *look(dmount_t *m, const char *p) { char f[600]; snprintf(f, sizeof f, "/Volumes/%s%s", m->root->name, p); return dfs_lookup(f); }
static void remount(void) {
    while (dfs_mounts()) dfs_unmount(dfs_mounts());
    blkdev_t *d = calloc(1, sizeof *d);
    d->size = img_size; d->read = rd; d->write = wr;
    dfs_mount_disk(d, "teste");
}
static i64 get(dmount_t *m, const char *path, const char *name, void *b, u64 cap) { dnode_t *n = look(m, path); return n ? m->ops->getxattr(m, n, name, b, cap) : -9; }
static int listed(dmount_t *m, const char *path, const char *name) {
    dnode_t *n = look(m, path);
    char buf[4096];
    i64 k = n ? m->ops->listxattr(m, n, buf, sizeof buf) : -1;
    for (i64 o = 0; o < k; o += (i64)strlen(buf + o) + 1) if (!strcmp(buf + o, name)) return 1;
    return 0;
}

static void load(const char *p) { FILE *f = fopen(p, "rb"); fseeko(f, 0, SEEK_END); img_size = (u64)ftello(f); fseeko(f, 0, SEEK_SET); img = malloc(img_size); if (fread(img, 1, img_size, f) != img_size) exit(2); fclose(f); }
static void save(const char *p) { FILE *f = fopen(p, "wb"); fwrite(img, 1, img_size, f); fclose(f); }

static void run(int drop_attr_tree) {
    remount();
    dmount_t *m = hfs();
    CHECK(m && m->writable);
    if (!m || !m->writable) return;
    if (drop_attr_tree) CHECK(m->ops->listxattr(m, look(m, "/seed.txt"), NULL, 0) == 0);
    /* pequeno (inline), nome com acento, quarentena */
    const char *q = "0083;6553f100;Navegador;";
    CHECK(m->ops->setxattr(m, "/seed.txt", "com.apple.quarantine", q, strlen(q)));
    CHECK(m->ops->setxattr(m, "/seed.txt", "user.ação", "valor", 5));
    char b[64];
    CHECK(get(m, "/seed.txt", "com.apple.quarantine", b, sizeof b) == (i64)strlen(q) && !memcmp(b, q, strlen(q)));
    CHECK(get(m, "/seed.txt", "user.ação", b, sizeof b) == 5 && !memcmp(b, "valor", 5));
    CHECK(get(m, "/seed.txt", "user.ação", b, 2) == -2);
    CHECK(get(m, "/seed.txt", "nao.existe", b, sizeof b) == -1);
    /* substituir */
    CHECK(m->ops->setxattr(m, "/seed.txt", "user.ação", "outro valor", 11));
    CHECK(get(m, "/seed.txt", "user.ação", b, sizeof b) == 11 && !memcmp(b, "outro valor", 11));
    /* grande: 300 KiB em fork próprio */
    u64 bigl = 300 * 1024;
    u8 *big = malloc(bigl);
    for (u64 i = 0; i < bigl; i++) big[i] = (u8)(i * 31 + 7);
    CHECK(m->ops->setxattr(m, "/seed.txt", "com.exemplo.grande", big, bigl));
    u8 *rb = malloc(bigl);
    CHECK(get(m, "/seed.txt", "com.exemplo.grande", rb, bigl) == (i64)bigl && !memcmp(rb, big, bigl));
    /* pasta e raiz */
    CHECK(m->ops->mkdir(m, "/Pasta"));
    CHECK(m->ops->setxattr(m, "/Pasta", "user.pasta", "p", 1));
    CHECK(m->ops->setxattr(m, "/", "user.raiz", "r", 1));
    /* FinderInfo e ResourceFork (campos do catálogo) */
    u8 fi[32] = {'T', 'E', 'X', 'T', 'm', 'O', 'S', '!', 0x40, 0};
    CHECK(m->ops->setxattr(m, "/seed.txt", "com.apple.FinderInfo", fi, 32));
    CHECK(!m->ops->setxattr(m, "/seed.txt", "com.apple.FinderInfo", fi, 31));
    CHECK(m->ops->setxattr(m, "/seed.txt", "com.apple.ResourceFork", "RECURSO", 7));
    CHECK(m->ops->sync(m));
    /* remonta e relê tudo do disco */
    remount();
    m = hfs();
    CHECK(get(m, "/seed.txt", "com.apple.quarantine", b, sizeof b) == (i64)strlen(q));
    CHECK(get(m, "/seed.txt", "user.ação", b, sizeof b) == 11);
    memset(rb, 0, bigl);
    CHECK(get(m, "/seed.txt", "com.exemplo.grande", rb, bigl) == (i64)bigl && !memcmp(rb, big, bigl));
    CHECK(get(m, "/Pasta", "user.pasta", b, sizeof b) == 1 && b[0] == 'p');
    CHECK(get(m, "/seed.txt", "com.apple.FinderInfo", b, 32) == 32 && !memcmp(b, fi, 32));
    CHECK(get(m, "/seed.txt", "com.apple.ResourceFork", b, sizeof b) == 7 && !memcmp(b, "RECURSO", 7));
    CHECK(listed(m, "/seed.txt", "com.apple.quarantine") && listed(m, "/seed.txt", "user.ação") && listed(m, "/seed.txt", "com.exemplo.grande"));
    CHECK(listed(m, "/seed.txt", "com.apple.FinderInfo") && listed(m, "/seed.txt", "com.apple.ResourceFork"));
    CHECK(!listed(m, "/Pasta", "user.ação"));
    /* o conteúdo do arquivo não mudou */
    dnode_t *sd = look(m, "/seed.txt");
    u64 sl = 0;
    u8 *sb = sd ? dfs_read_all(sd, 1 << 20, &sl) : NULL;
    CHECK(sb && sl == 18 && !memcmp(sb, "semente do volimg\n", 18));
    kfree(sb);
    /* remover (o grande solta os blocos), FinderInfo e ResourceFork */
    CHECK(m->ops->removexattr(m, "/seed.txt", "com.exemplo.grande"));
    CHECK(m->ops->removexattr(m, "/seed.txt", "user.ação"));
    CHECK(m->ops->removexattr(m, "/seed.txt", "com.apple.FinderInfo"));
    CHECK(m->ops->removexattr(m, "/seed.txt", "com.apple.ResourceFork"));
    CHECK(m->ops->removexattr(m, "/", "user.raiz"));
    CHECK(m->ops->sync(m));
    remount();
    m = hfs();
    CHECK(get(m, "/seed.txt", "com.exemplo.grande", NULL, 0) == -1 && get(m, "/seed.txt", "user.ação", NULL, 0) == -1);
    CHECK(get(m, "/seed.txt", "com.apple.FinderInfo", NULL, 0) == -1 && get(m, "/seed.txt", "com.apple.ResourceFork", NULL, 0) == -1);
    CHECK(get(m, "/seed.txt", "com.apple.quarantine", NULL, 0) == (i64)strlen(q));
    /* apagar o arquivo apaga os atributos */
    CHECK(m->ops->remove(m, "/Pasta"));
    CHECK(m->ops->sync(m));
    free(big); free(rb);
    while (dfs_mounts()) dfs_unmount(dfs_mounts());
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: hfsxattr_test <imagem> <imagem-sem-attr>\n"); return 2; }
    dfs_init();
    load(argv[1]);
    run(0);
    save(argv[1]);
    free(img);
    load(argv[2]);
    run(1);
    save(argv[2]);
    free(img);
    printf("hfsxattr: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

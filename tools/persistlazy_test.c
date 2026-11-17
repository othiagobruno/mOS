/*
 * mOS — VFS sob demanda no volume do sistema (fs/persist.c real + kernel/lib/vfs*.c + fs/fat.c): um FAT32 com \mOS\
 * vira o disco do sistema. O boot cria os vnodes sem conteúdo (só tamanho e cluster); /Aplicativos e arquivos grandes
 * são lidos quando usados; os pequenos carregados vão para uma LRU que devolve memória; mudar, renomear, mover para
 * /tmp e apagar preservam o conteúdo e o que vai ao disco.
 *   sh tools/persistlazy_test.sh
 */
#include "vfs_host_kernel.h"
#include "vfs.h"
#include "fs.h"
#include "syscall.h"
#include "storage.h"
#include "origin.h"
bool persist_writable(void);
#include <time.h>

long long g_live_allocs, g_live_bytes;
static u64 fake_ms = 1000;
u64 timer_ticks(void) { return fake_ms; }
void rtc_now(datetime_t *dt) { time_t t = time(NULL); struct tm *m = gmtime(&t); dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday; dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday; }
uint32_t rand32(void) { return 4; }
static boot_info_t boot0;
boot_info_t *g_boot = &boot0;
static u64 dev_bytes;
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) { dev_bytes += len; return d->read(d, off, buf, len); }
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) { return d->write && d->write(d, off, buf, len); }
bool blk_flush(blkdev_t *d) { return !d->flush || d->flush(d); }
void pcache_drop(blkdev_t *d) { (void)d; }
void pcache_tick(void) {}
bool storage_grant_write(blkdev_t *d, u64 off, u64 len) { (void)d; (void)off; (void)len; return true; }
void storage_revoke_write(blkdev_t *d, u64 off, u64 len) { (void)d; (void)off; (void)len; }
int storage_disk_index(blkdev_t *r) { (void)r; return -1; }
const char *storage_disk_name(int i) { (void)i; return "sata0"; }
static blkdev_t *g_disk;
blkdev_t *storage_disk(int i) { return i == 0 ? g_disk : NULL; }
static origin_t org0;
const origin_t *origin_get(void) { return &org0; }
u64 pmm_free_pages(void) { return (1ull << 30) / 4096; }
void acpi_reboot(void) {}
bool update_boot_pending(fatfs_t *fs) { (void)fs; return false; }
bool update_boot_apply(fatfs_t *fs) { (void)fs; return false; }
void update_poll(void) {}
char klog_buf[16];
usize klog_len;
void vfs_init(void);

static u8 *img;
static u64 img_size;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; memcpy(buf, img + off, len); return true; }
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) { (void)d; memcpy(img + off, buf, len); return true; }
static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)
static u8 pat(u64 i, u32 seed) { return (u8)((i + seed) * 2654435761u >> 11 ^ i >> 15); }
static u8 *mk(u64 n, u32 seed) { u8 *b = malloc(n); for (u64 i = 0; i < n; i++) b[i] = pat(i, seed); return b; }

/* filho direto sem passar pelo vfs_lookup (que carregaria o conteúdo) */
static vnode_t *peek(const char *dir, const char *name) {
    vnode_t *d = vfs_lookup_dir(dir);
    for (vnode_t *c = d ? d->child : NULL; c; c = c->next) if (!strcmp(c->name, name)) return c;
    return NULL;
}

#define BIG  (20ull << 20 | 333)
#define MID  (2ull << 20)
#define NMID 40

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    fseeko(f, 0, SEEK_END); img_size = (u64)ftello(f); fseeko(f, 0, SEEK_SET);
    img = malloc(img_size);
    if (fread(img, 1, img_size, f) != img_size) return 2;
    fclose(f);
    g_disk = calloc(1, sizeof *g_disk);
    g_disk->size = img_size; g_disk->read = rd; g_disk->write = wr;
    /* partição 1 da GPT */
    u64 first = *(u64 *)(img + 1024 + 32), last = *(u64 *)(img + 1024 + 40);
    org0.status = ORIGIN_TRUSTED; org0.disk = 0; org0.part_off = first * 512; org0.part_size = (last - first + 1) * 512;
    /* monta à parte e põe o \mOS\ de teste */
    blkdev_t *p = blk_slice(g_disk, org0.part_off, org0.part_size);
    fatfs_t *fs = fat_mount(p);
    CHECK(fs != NULL);
    if (!fs) return 1;
    fat_enable_rw(fs);
    u8 *big = mk(BIG, 1), *mid[NMID];
    CHECK(fat_mkdir(fs, "/mOS") && fat_mkdir(fs, "/mOS/Sistema") && fat_mkdir(fs, "/mOS/Sistema/Biblioteca") &&
          fat_mkdir(fs, "/mOS/Aplicativos") && fat_mkdir(fs, "/mOS/Aplicativos/X.app") && fat_mkdir(fs, "/mOS/Users"));
    CHECK(fat_write_file(fs, "/mOS/Sistema/Biblioteca/estado-sistema", "INSTALADO", 9));
    CHECK(fat_write_file(fs, "/mOS/Sistema/config.txt", "cor=azul", 8));
    CHECK(fat_write_file(fs, "/mOS/Sistema/vazio", "", 0));
    CHECK(fat_write_file(fs, "/mOS/Aplicativos/X.app/big.bin", big, BIG));
    CHECK(fat_write_file(fs, "/mOS/Aplicativos/X.app/main.js", "console.log(1)", 14));
    for (int i = 0; i < NMID; i++) {
        char q[96];
        snprintf(q, sizeof q, "/mOS/Aplicativos/X.app/m%02d.bin", i);
        mid[i] = mk(MID, 100 + i);
        CHECK(fat_write_file(fs, q, mid[i], MID));
    }
    CHECK(fat_flush(fs));
    fat_unmount(fs);
    blk_release(p);

    /* boot: o vfs_init chama persist_load e persist_enable */
    dev_bytes = 0;
    vfs_init();
    CHECK(persist_available() && persist_writable());
    CHECK(dev_bytes < (2u << 20));                       /* 100 MB no disco, quase nada lido no boot */
    printf("persistlazy: boot leu %llu KiB do disco\n", (unsigned long long)(dev_bytes >> 10));

    vnode_t *cfg = peek("/Sistema", "config.txt"), *bg = peek("/Aplicativos/X.app", "big.bin"), *js = peek("/Aplicativos/X.app", "main.js");
    CHECK(cfg && cfg->data && cfg->size == 8 && !memcmp(cfg->data, "cor=azul", 8));   /* fora de /Aplicativos: no boot */
    CHECK(bg && !bg->data && bg->size == BIG && bg->dclus && vfs_paged(bg));
    CHECK(js && !js->data && js->size == 14 && js->dclus);                             /* app: só quando usado */
    vnode_t *vz = peek("/Sistema", "vazio");
    CHECK(vz && vz->size == 0 && vz->data);

    /* lookup traz o pequeno; o grande fica paginado e lê por trechos */
    CHECK(vfs_lookup("/Aplicativos/X.app/main.js") == js && js->data && !memcmp(js->data, "console.log(1)", 14));
    CHECK(vfs_lookup("/Aplicativos/X.app/big.bin") == bg && !bg->data);
    u8 buf[100000];
    u64 offs[] = {0, 4095, 1u << 20, BIG / 3, BIG - 100000, BIG - 1};
    for (unsigned i = 0; i < ARRAY_LEN(offs); i++) {
        u64 want = MIN(sizeof buf, BIG - offs[i]);
        CHECK(vfs_read_at(bg, offs[i], buf, sizeof buf) == (i64)want && !memcmp(buf, big + offs[i], want));
    }
    CHECK(vfs_read_at(bg, BIG, buf, 1) == 0);

    /* LRU: 80 MB de arquivos lidos, a RAM fica no orçamento e o conteúdo continua certo */
    for (int i = 0; i < NMID; i++) {
        char q[96];
        snprintf(q, sizeof q, "/Aplicativos/X.app/m%02d.bin", i);
        vnode_t *v = vfs_lookup(q);
        CHECK(v && v->data && v->size == MID && !memcmp(v->data, mid[i], MID));
    }
    u64 resident = 0;
    int nres = 0;
    for (int i = 0; i < NMID; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "m%02d.bin", i);
        vnode_t *v = peek("/Aplicativos/X.app", nm);
        if (v && v->data) { resident += v->size; nres++; }
    }
    CHECK(resident <= (48ull << 20) + MID && nres >= 8 && nres < NMID);
    printf("persistlazy: %d de %d arquivos de 2 MiB ainda na RAM (%llu MiB)\n", nres, NMID, (unsigned long long)(resident >> 20));
    vnode_t *m0 = peek("/Aplicativos/X.app", "m00.bin");
    CHECK(m0 && !m0->data && m0->dclus);                 /* o mais antigo voltou para o disco */
    CHECK(vfs_lookup("/Aplicativos/X.app/m00.bin") == m0 && m0->data && !memcmp(m0->data, mid[0], MID));   /* e volta */
    /* vfs_ensure: o grande inteiro (exec/dyld) */
    CHECK(vfs_ensure(bg) && bg->data && !memcmp(bg->data, big, BIG));

    /* mudança: o conteúdo antigo fica, sai do disco-como-fonte e é gravado */
    vnode_t *m5 = vfs_lookup("/Aplicativos/X.app/m05.bin");
    CHECK(m5 && vfs_append(m5, "FIM", 3));
    CHECK(m5->dclus == 0 && m5->dirty && m5->size == MID + 3 && !memcmp(m5->data, mid[5], MID) && !memcmp(m5->data + MID, "FIM", 3));
    persist_sync();
    CHECK(!m5->dirty);
    /* rename dentro da área gravada: o vnode continua lendo do mesmo cluster */
    vnode_t *m7 = peek("/Aplicativos/X.app", "m07.bin");
    CHECK(m7 && vfs_move(m7, vfs_lookup_dir("/Users"), "m7-novo.bin"));
    vnode_t *n7 = peek("/Users", "m7-novo.bin");
    CHECK(n7 == m7 && n7->dclus);
    CHECK(vfs_read_at(n7, MID - 5000, buf, 5000) == 5000 && !memcmp(buf, mid[7] + MID - 5000, 5000));
    /* para /tmp (fora do disco): vem para a RAM antes de o disco apagar */
    vfs_mkdir("/tmp");
    vnode_t *m9 = peek("/Aplicativos/X.app", "m09.bin");
    CHECK(m9 && vfs_move(m9, vfs_lookup_dir("/tmp"), "m9.bin"));
    CHECK(m9->dclus == 0 && m9->data && m9->size == MID && !memcmp(m9->data, mid[9], MID));
    /* apagar */
    vnode_t *m11 = vfs_lookup("/Aplicativos/X.app/m11.bin");
    CHECK(m11 && vfs_delete(m11));
    /* cópia de um paginado */
    CHECK(vfs_copy_tree("/Aplicativos/X.app/m12.bin", "/Users/copia.bin"));

    /* o disco: confere com uma montagem nova */
    persist_sync();
    p = blk_slice(g_disk, org0.part_off, org0.part_size);
    fs = fat_mount(p);
    u64 n = 0;
    u8 *x = fat_read_file(fs, "/mOS/Aplicativos/X.app/m05.bin", &n);
    CHECK(x && n == MID + 3 && !memcmp(x, mid[5], MID) && !memcmp(x + MID, "FIM", 3));
    free(x);
    x = fat_read_file(fs, "/mOS/Users/m7-novo.bin", &n);
    CHECK(x && n == MID && !memcmp(x, mid[7], MID));
    free(x);
    bool dir;
    CHECK(!fat_stat(fs, "/mOS/Aplicativos/X.app/m07.bin", &dir, NULL));
    CHECK(!fat_stat(fs, "/mOS/Aplicativos/X.app/m09.bin", &dir, NULL));
    CHECK(!fat_stat(fs, "/mOS/Aplicativos/X.app/m11.bin", &dir, NULL));
    x = fat_read_file(fs, "/mOS/Users/copia.bin", &n);
    CHECK(x && n == MID && !memcmp(x, mid[12], MID));
    free(x);
    x = fat_read_file(fs, "/mOS/Aplicativos/X.app/big.bin", &n);
    CHECK(x && n == BIG && !memcmp(x, big, BIG));
    free(x);
    fat_unmount(fs);
    blk_release(p);
    f = fopen(argv[1], "wb"); fwrite(img, 1, img_size, f); fclose(f);
    printf("persistlazy: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}

/*
 * mOS — lê arquivos de uma imagem GPT/FAT32 do mkimg no host, com o driver do kernel (kernel/fs/fat.c).
 * Serve para tirar da imagem o que a VM gravou (rodar com -Persist: o run.ps1 sem ele usa -snapshot):
 * um NetLog do Chromium, um log, um despejo.
 *   fatcat <imagem.img> <caminho> [saida]     extrai (sem saida: stdout)
 *   fatcat <imagem.img> --ls <pasta>          lista
 * Caminhos no volume: o "/" da VM é "/mOS" aqui (ex.: /mOS/Users/Visitante/n.json).
 * zig cc -target x86_64-windows-gnu -O2 -DMOS_HOST_TEST -Ikernel/include -Itools tools/fatcat.c kernel/fs/fat.c -o build/fatcat.exe
 */
#include "fs.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) {
    time_t t = time(NULL);
    struct tm *m = localtime(&t);
    dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday;
    dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday;
}
u32 rand32(void) { return (u32)rand() << 16 ^ (u32)rand(); }
dmount_t *dfs_attach(const dfs_ops_t *o, blkdev_t *d, const char *v, const char *s, const char *f, void *p, u64 r) { return NULL; }
dnode_t *dfs_new_child(dnode_t *dir, const char *name, u8 type, u64 size) { return NULL; }

static FILE *img;
static u64 part_off;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) {
    (void)d;
    return !_fseeki64(img, (long long)(part_off + off), SEEK_SET) && fread(buf, 1, len, img) == len;
}
static void ls_cb(void *ctx, const char *name, bool dir, u64 size) { (void)ctx; printf("%s%s\t%llu\n", name, dir ? "/" : "", (unsigned long long)size); }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: fatcat <imagem.img> <caminho> [saida] | fatcat <imagem.img> --ls <pasta>\n"); return 2; }
    img = fopen(argv[1], "rb");
    if (!img) { perror(argv[1]); return 2; }
    u8 hdr[1024];
    if (fread(hdr, 1, sizeof hdr, img) != sizeof hdr || memcmp(hdr + 512, "EFI PART", 8)) { fprintf(stderr, "sem GPT\n"); return 2; }
    u8 e[128];
    if (_fseeki64(img, (long long)(le64(hdr + 512 + 72) * 512), SEEK_SET) || fread(e, 1, sizeof e, img) != sizeof e) return 2;
    part_off = le64(e + 32) * 512;
    blkdev_t dev = {.size = (le64(e + 40) - le64(e + 32) + 1) * 512, .read = rd};   /* sem write: somente leitura */
    fatfs_t *fs = fat_mount(&dev);
    if (!fs) { fprintf(stderr, "não é FAT32\n"); return 1; }
    if (!strcmp(argv[2], "--ls")) return fat_list(fs, argc > 3 ? argv[3] : "/", ls_cb, NULL) < 0;
    u64 len = 0;
    u8 *d = fat_read_file(fs, argv[2], &len);
    if (!d) { fprintf(stderr, "%s: não encontrado\n", argv[2]); return 1; }
    FILE *o = argc > 3 ? fopen(argv[3], "wb") : stdout;
    if (!o) { perror(argv[3]); return 2; }
    fwrite(d, 1, len, o);
    if (o != stdout) { fclose(o); fprintf(stderr, "%s: %llu bytes\n", argv[3], (unsigned long long)len); }
    return 0;
}

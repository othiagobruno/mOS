/* Teste de host do kernel/fs/bootlog.c (log de boot em \EFI\mOS\boot.log, docs/instalar-no-pc.md).
 * Disco falso num arquivo; o boot.log do volume em 2 faixas (fragmentado de propósito) dentro de uma partição em 1 MiB.
 * Cada execução é um boot: ./t <disco> <bytes de texto a mais>. Confere que toda escrita cai dentro das faixas do
 * arquivo, em setores inteiros; o resto (N, slot, marca de corte) é conferido pelo tools/ler-boot-log.py no .sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "fs.h"
#include "storage.h"

#define DISK (8u << 20)
#define PART (1u << 20)
static FILE *img;
static u64 E_OFF[2] = {256 << 10, 4u << 20}, E_LEN[2] = {3u << 20, 1u << 20};   /* relativos à partição */
static u64 allow_off[8], allow_len[8];
static int nallow, bad;
static u64 writes;

static bool d_read(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; return !fseek(img, (long)off, SEEK_SET) && fread(buf, 1, len, img) == len; }
static bool d_write(blkdev_t *d, u64 off, const void *buf, u64 len) {
    (void)d;
    int ok = 0;
    for (int i = 0; i < nallow; i++) if (off >= allow_off[i] && off + len <= allow_off[i] + allow_len[i]) ok = 1;
    if (!ok || (off | len) & 511) { fprintf(stderr, "ESCRITA FORA DO LOG: off %llu len %llu\n", (unsigned long long)off, (unsigned long long)len); bad = 1; return false; }
    writes += len;
    return !fseek(img, (long)off, SEEK_SET) && fwrite(buf, 1, len, img) == len;
}
static bool d_flush(blkdev_t *d) { (void)d; return fflush(img) == 0; }
static blkdev_t disk = {.size = DISK, .read = d_read, .write = d_write, .flush = d_flush};

/* o que o bootlog.c usa do kernel */
char klog_buf[64] = "mOS: iniciando o Nucleo...\n";
usize klog_len = 27;
static u64 now;
u64 timer_ticks(void) { return now; }
blkdev_t *storage_disk(int i) { return i == 0 ? &disk : NULL; }
bool storage_allow_log(blkdev_t *d, u64 off, u64 len) { (void)d; allow_off[nallow] = off; allow_len[nallow] = len; nallow++; return true; }
int fat_extents(fatfs_t *fs, const char *path, u64 *off, u64 *len, int max, u64 *size) {
    (void)fs; (void)max;
    if (strcmp(path, "/EFI/mOS/boot.log")) return -1;
    for (int i = 0; i < 2; i++) { off[i] = E_OFF[i]; len[i] = E_LEN[i]; }
    *size = 4u << 20;
    return 2;
}
const char *update_current_version(void) { return "0.1-teste"; }
void *kmalloc_pages(usize n) { return malloc(n); }

void bootlog_start(fatfs_t *fs, int disk_index, u64 part_off);
void bootlog_putc(char c);
void bootlog_tick(void);
void bootlog_panic(void);

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    img = fopen(argv[1], "r+b");
    if (!img) { img = fopen(argv[1], "w+b"); if (!img) return 2; fseek(img, DISK - 1, SEEK_SET); fputc(0, img); }
    long extra = atol(argv[2]);
    bootlog_start((fatfs_t *)1, 0, PART);
    if (nallow != 2) { fprintf(stderr, "faixas liberadas: %d\n", nallow); return 1; }
    const char *line = "linha de teste do console 0123456789\n";
    for (long k = 0; k < extra; k++) bootlog_putc(line[k % 37]);
    now += 2000; bootlog_tick();                  /* ~1 s depois: grava */
    const char *fim = "fim do boot\n";
    for (const char *p = fim; *p; p++) bootlog_putc(*p);
    bootlog_panic();                              /* o que faltou vai no pânico */
    fclose(img);
    printf("bytes gravados: %llu\n", (unsigned long long)writes);
    return bad;
}
